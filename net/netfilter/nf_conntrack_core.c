/* Connection state tracking for netfilter.  This is separated from,
   but required by, the NAT layer; it can also be used by an iptables
   extension. */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2006 Netfilter Core Team <coreteam@netfilter.org>
 * (C) 2003,2004 USAGI/WIDE Project <http://www.linux-ipv6.org>
 * (C) 2005-2012 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/jhash.h>
#include <linux/siphash.h>
#include <linux/err.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/rculist_nulls.h>
#include <linux/nf_conntrack_hooks.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_seqadj.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_timestamp.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/nf_conntrack_labels.h>
#include <net/netfilter/nf_conntrack_synproxy.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netns/hash.h>

#include <net/netfilter/nf_nsc.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/ip.h>
#include <net/fast_vpn.h>
#endif

#if IS_ENABLED(CONFIG_FAST_NAT) || IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
#include <net/fast_nat.h>
#endif

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
#include <linux/ntc_shaper_hooks.h>
#include <../net/bridge/br_private.h>
#include <../net/bridge/ubridge_private.h>
#endif

#include <net/netfilter/nf_ntce.h>

#define NF_CONNTRACK_VERSION	"0.5.0"

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
#ifndef CONFIG_NDM_SECURITY_LEVEL
#error "NDM security level support required"
#endif

static void
__nf_ct_ext_ndm_skip(struct sk_buff *skb, struct nf_conn *ct)
{
	struct nf_ct_ext_ntc_label *lbl = nf_ct_ext_find_ntc(ct);

	memset(lbl, 0, sizeof(*lbl));
	clear_bit(IPS_NDM_FROM_WAN_BIT, &ct->status);
	clear_bit(IPS_NDM_FILLED_BIT, &ct->status);

	set_bit(IPS_NDM_SKIPPED_BIT, &ct->status);

	/* skip an output handler */
	xt_ndmmark_kernel_set_ndm_skip(skb);
}

static unsigned short
__nf_ct_ext_ndm_security_level(struct net_device *dev, s32 *ifindex)
{
	unsigned short sl = NDM_SECURITY_LEVEL_NONE;

	rcu_read_lock_bh();

	if (is_ubridge_port(dev)) {
		struct net_device *dev_sl = ubr_get_by_slave_rcu_bh(dev);

		if (dev_sl)
			dev = dev_sl;
	}

	if (br_port_exists(dev)) {
		const struct net_bridge_port *p = br_port_get_rcu_bh(dev);

		if (p && p->br && p->br->dev)
			dev = p->br->dev;
	}

	/* an index and a security level of an active L3 interface */
	*ifindex = (s32)dev->ifindex;
	sl = dev->ndm_security_level;

	rcu_read_unlock_bh();

	return sl;
}

static void
__nf_ct_ext_ndm_fill(struct net *net, struct sk_buff *skb,
		     struct net_device *dev, struct nf_conn *ct,
		     const bool input)
{
	struct nf_ct_ext_ntc_label *lbl;
	bool is_vpn_server = false;
	s32 ifindex = -1;
	const unsigned short sl = __nf_ct_ext_ndm_security_level(dev, &ifindex);

	if (ifindex <= 0 || sl == NDM_SECURITY_LEVEL_NONE) {
		if (sl == NDM_SECURITY_LEVEL_NONE &&
		    input &&
		    (!strncmp(dev->name, "vpn", 3) ||
		     !strncmp(dev->name, "sstp", 4) ||
		     !strncmp(dev->name, "l2tp", 4)))
			is_vpn_server = true;

		if (!is_vpn_server) {
			__nf_ct_ext_ndm_skip(skb, ct);

			return;
		}
	}

	lbl = nf_ct_ext_find_ntc(ct);

	if (sl == NDM_SECURITY_LEVEL_PUBLIC || is_vpn_server) {
		const s32 wan_iface = READ_ONCE(lbl->wan_iface);

		if (!is_vpn_server &&
		     input &&
		    !test_bit(IPS_NDM_FROM_PUBLIC_BIT, &ct->status)) {
			set_bit(IPS_NDM_FROM_PUBLIC_BIT, &ct->status);
			atomic_inc(&net->ct.public_count);
		}

		if (wan_iface > 0) {
			/* WAN <-> WAN connection */
			if (wan_iface != ifindex)
				__nf_ct_ext_ndm_skip(skb, ct);

			return;
		}

		WRITE_ONCE(lbl->wan_iface, ifindex);

		if (input) {
			/* an input interface is WAN */
			set_bit(IPS_NDM_FROM_WAN_BIT, &ct->status);
			xt_ndmmark_kernel_set_wan(skb); /* from WAN */
		}
	} else {
		const s32 lan_iface = READ_ONCE(lbl->lan_iface);
		struct ethhdr *hdr;

		if (lan_iface > 0) {
			/* LAN <-> LAN connection */
			if (lan_iface != ifindex)
				__nf_ct_ext_ndm_skip(skb, ct);

			return;
		}

		WRITE_ONCE(lbl->lan_iface, ifindex);

		/* an input interface is LAN */
		if (input)
			lbl->flags |= NF_CT_EXT_NTC_FROM_LAN;

		if (skb->dev->type != ARPHRD_ETHER ||
		    !skb_mac_header_was_set(skb) ||
		    skb_mac_header_len(skb) < ETH_HLEN) {
			/* LAN without L2 */
			__nf_ct_ext_ndm_skip(skb, ct);
			return;
		}

		hdr = eth_hdr(skb);
		memcpy(lbl->mac, input ? hdr->h_source : hdr->h_dest,
		       sizeof(lbl->mac));
	}

	if (nf_ct_ext_ntc_filled(lbl))
		set_bit(IPS_NDM_FILLED_BIT, &ct->status);
}

static inline void
nf_ct_ext_ndm_input(u8 pf, struct net *net, struct sk_buff *skb,
		    struct nf_conn *ct)
{
	struct net_device *dev;

	if (unlikely(pf != PF_INET && pf != PF_INET6)) {
		__nf_ct_ext_ndm_skip(skb, ct);
		return;
	}

	if (unlikely(skb_sec_path(skb))) {
		__nf_ct_ext_ndm_skip(skb, ct);
		return;
	}

	dev = dev_get_by_index(net, skb->skb_iif);
	if (unlikely(!dev)) {
		__nf_ct_ext_ndm_skip(skb, ct);
		return;
	}

	__nf_ct_ext_ndm_fill(net, skb, dev, ct, true);
	dev_put(dev);
}

void nf_ct_ext_ndm_output(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;

	if (!skb->dev)
		return;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return;

	if (test_bit(IPS_NDM_FILLED_BIT, &ct->status) ||
	    test_bit(IPS_NDM_SKIPPED_BIT, &ct->status) ||
	    nf_ct_is_untracked(ct))
		return;

	__nf_ct_ext_ndm_fill(read_pnet(&ct->ct_net), skb, skb->dev, ct, false);
}
EXPORT_SYMBOL(nf_ct_ext_ndm_output);

static inline void
nf_ct_ext_ndm_mark_skb(struct sk_buff *skb, struct nf_conn *ct,
		       const enum ip_conntrack_info ctinfo)
{
	if (test_bit(IPS_NDM_FILLED_BIT, &ct->status)) {
		/* A connection origin is a WAN interface
		 * and a packet going to original direction
		 * or
		 * the connection origin is a LAN interface
		 * and the packet going to reply direction.
		 */
		if (test_bit(IPS_NDM_FROM_WAN_BIT, &ct->status) ==
		    (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL))
			xt_ndmmark_kernel_set_wan(skb); /* from WAN */

		xt_ndmmark_kernel_set_ndm_skip(skb);
		return;
	}

	if (test_bit(IPS_NDM_SKIPPED_BIT, &ct->status))
		xt_ndmmark_kernel_set_ndm_skip(skb);
}

static unsigned short
nf_ct_ext_ndm_sl(struct net *net, struct sk_buff *skb)
{
	unsigned short sl = NDM_SECURITY_LEVEL_NONE;
	struct net_device *dev = dev_get_by_index(net, skb->skb_iif);
	s32 ifindex = -1;

	if (unlikely(!dev))
		return sl;

	sl = __nf_ct_ext_ndm_security_level(dev, &ifindex);
	dev_put(dev);

	return sl;
}

#else
static inline void
nf_ct_ext_ndm_input(u8 pf, struct net *net, struct sk_buff *skb,
		    struct nf_conn *ct) {}

static inline void
nf_ct_ext_ndm_output(struct sk_buff *skb) {}

static inline void
nf_ct_ext_ndm_mark_skb(struct sk_buff *skb, struct nf_conn *ct,
		       const enum ip_conntrack_info ctinfo) {}
#endif

int (*nfnetlink_parse_nat_setup_hook)(struct nf_conn *ct,
				      enum nf_nat_manip_type manip,
				      const struct nlattr *attr) __read_mostly;
EXPORT_SYMBOL_GPL(nfnetlink_parse_nat_setup_hook);

__cacheline_aligned_in_smp spinlock_t nf_conntrack_locks[CONNTRACK_LOCKS];
EXPORT_SYMBOL_GPL(nf_conntrack_locks);

__cacheline_aligned_in_smp DEFINE_SPINLOCK(nf_conntrack_expect_lock);
EXPORT_SYMBOL_GPL(nf_conntrack_expect_lock);

struct hlist_nulls_head *nf_conntrack_hash __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_hash);

#if IS_ENABLED(CONFIG_FAST_NAT)
int nf_fastnat_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastnat_control);

#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
int nf_fastroute_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastroute_control);
#endif

#if IS_ENABLED(CONFIG_PPTP)
int nf_fastpath_pptp_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastpath_pptp_control);
#endif

#if IS_ENABLED(CONFIG_PPPOL2TP)
int nf_fastpath_l2tp_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastpath_l2tp_control);
#endif

#ifdef CONFIG_XFRM
int nf_fastpath_esp_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastpath_esp_control);

int nf_fastnat_xfrm_control __read_mostly;
EXPORT_SYMBOL_GPL(nf_fastnat_xfrm_control);
#endif
#endif /* CONFIG_FAST_NAT_V2 */
#endif /* CONFIG_FAST_NAT */

struct conntrack_gc_work {
	struct delayed_work	dwork;
	u32			last_bucket;
	bool			exiting;
	long			next_gc_run;
};

static __read_mostly struct kmem_cache *nf_conntrack_cachep;
static __read_mostly spinlock_t nf_conntrack_locks_all_lock;
static __read_mostly DEFINE_SPINLOCK(nf_conntrack_locks_all_lock);
static __read_mostly bool nf_conntrack_locks_all;

#ifdef CONFIG_NAT_CONE
unsigned int nf_conntrack_nat_mode __read_mostly = NAT_MODE_LINUX;
int nf_conntrack_nat_cone_ifindex __read_mostly = -1;
#endif

/* every gc cycle scans at most 1/GC_MAX_BUCKETS_DIV part of table */
#define GC_MAX_BUCKETS_DIV	128u
/* upper bound of full table scan */
#define GC_MAX_SCAN_JIFFIES	(16u * HZ)
/* desired ratio of entries found to be expired */
#define GC_EVICT_RATIO	50u

static struct conntrack_gc_work conntrack_gc_work;

void nf_conntrack_lock(spinlock_t *lock) __acquires(lock)
{
	/* 1) Acquire the lock */
	spin_lock(lock);

	/* 2) read nf_conntrack_locks_all, with ACQUIRE semantics
	 * It pairs with the smp_store_release() in nf_conntrack_all_unlock()
	 */
	if (likely(smp_load_acquire(&nf_conntrack_locks_all) == false))
		return;

	/* fast path failed, unlock */
	spin_unlock(lock);

	/* Slow path 1) get global lock */
	spin_lock(&nf_conntrack_locks_all_lock);

	/* Slow path 2) get the lock we want */
	spin_lock(lock);

	/* Slow path 3) release the global lock */
	spin_unlock(&nf_conntrack_locks_all_lock);
}
EXPORT_SYMBOL_GPL(nf_conntrack_lock);

static void nf_conntrack_double_unlock(unsigned int h1, unsigned int h2)
{
	h1 %= CONNTRACK_LOCKS;
	h2 %= CONNTRACK_LOCKS;
	spin_unlock(&nf_conntrack_locks[h1]);
	if (h1 != h2)
		spin_unlock(&nf_conntrack_locks[h2]);
}

/* return true if we need to recompute hashes (in case hash table was resized) */
static bool nf_conntrack_double_lock(struct net *net, unsigned int h1,
				     unsigned int h2, unsigned int sequence)
{
	h1 %= CONNTRACK_LOCKS;
	h2 %= CONNTRACK_LOCKS;
	if (h1 <= h2) {
		nf_conntrack_lock(&nf_conntrack_locks[h1]);
		if (h1 != h2)
			spin_lock_nested(&nf_conntrack_locks[h2],
					 SINGLE_DEPTH_NESTING);
	} else {
		nf_conntrack_lock(&nf_conntrack_locks[h2]);
		spin_lock_nested(&nf_conntrack_locks[h1],
				 SINGLE_DEPTH_NESTING);
	}
	if (read_seqcount_retry(&nf_conntrack_generation, sequence)) {
		nf_conntrack_double_unlock(h1, h2);
		return true;
	}
	return false;
}

static void nf_conntrack_all_lock(void)
{
	int i;

	spin_lock(&nf_conntrack_locks_all_lock);

	nf_conntrack_locks_all = true;

	for (i = 0; i < CONNTRACK_LOCKS; i++) {
		spin_lock(&nf_conntrack_locks[i]);

		/* This spin_unlock provides the "release" to ensure that
		 * nf_conntrack_locks_all==true is visible to everyone that
		 * acquired spin_lock(&nf_conntrack_locks[]).
		 */
		spin_unlock(&nf_conntrack_locks[i]);
	}
}

static void nf_conntrack_all_unlock(void)
{
	/* All prior stores must be complete before we clear
	 * 'nf_conntrack_locks_all'. Otherwise nf_conntrack_lock()
	 * might observe the false value but not the entire
	 * critical section.
	 * It pairs with the smp_load_acquire() in nf_conntrack_lock()
	 */
	smp_store_release(&nf_conntrack_locks_all, false);
	spin_unlock(&nf_conntrack_locks_all_lock);
}

unsigned int nf_conntrack_htable_size __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_htable_size);

unsigned int nf_conntrack_max __read_mostly;
EXPORT_SYMBOL(nf_conntrack_max);
seqcount_t nf_conntrack_generation __read_mostly;

unsigned int nf_conntrack_public_max __read_mostly;

atomic_t nf_conntrack_public_locked __read_mostly;
unsigned long nf_conntrack_public_locked_until __read_mostly;
unsigned int nf_conntrack_public_lockout_time __read_mostly;
spinlock_t nf_conntrack_public_lock;

int nf_conntrack_public_status(void)
{
	int res = 0;

	if (unlikely(atomic_read(&nf_conntrack_public_locked))) {
		res = jiffies_to_msecs(
			nf_conntrack_public_locked_until - jiffies);
	}

	return res;
}

DEFINE_PER_CPU(struct nf_conn, nf_conntrack_untracked);
EXPORT_PER_CPU_SYMBOL(nf_conntrack_untracked);

static unsigned int nf_conntrack_hash_rnd __read_mostly;

static u32 hash_conntrack_raw(const struct nf_conntrack_tuple *tuple,
			      const struct net *net)
{
	unsigned int n;
	u32 seed;

	get_random_once(&nf_conntrack_hash_rnd, sizeof(nf_conntrack_hash_rnd));
#ifdef CONFIG_NET_NS
	seed = nf_conntrack_hash_rnd ^ net_hash_mix(net);
#else
	seed = nf_conntrack_hash_rnd;
#endif

#ifdef CONFIG_NAT_CONE
	if (tuple->dst.protonum == IPPROTO_UDP &&
	    nf_conntrack_nat_mode != NAT_MODE_LINUX) {
		if (nf_conntrack_nat_mode == NAT_MODE_RCONE) {
			u32 a, b;

			/* src ip & l3 proto */
			a = jhash2(tuple->src.u3.all,
				   sizeof(tuple->src.u3.all) / sizeof(u32),
				   tuple->src.l3num);

			/* dst ip & dst port & dst proto */
			b = jhash2(tuple->dst.u3.all,
				   sizeof(tuple->dst.u3.all) / sizeof(u32),
				   ((__force __u16)tuple->dst.u.all << 16) |
				   tuple->dst.protonum);

			return jhash_2words(a, b, seed);
		}

		/* dst ip & dst port & dst proto */
		return jhash2(tuple->dst.u3.all,
			      sizeof(tuple->dst.u3.all) / sizeof(u32),
			      seed ^ (((__force __u16)tuple->dst.u.all << 16) |
				      tuple->dst.protonum));
	}
#endif

	/* The direction must be ignored, so we hash everything up to the
	 * destination ports (which is a multiple of 4) and treat the last
	 * three bytes manually.
	 */
	n = (sizeof(tuple->src) + sizeof(tuple->dst.u3)) / sizeof(u32);
	return jhash2((u32 *)tuple, n, seed ^
		      (((__force __u16)tuple->dst.u.all << 16) |
		      tuple->dst.protonum));
}

static inline u32 scale_hash(u32 hash)
{
	return reciprocal_scale(hash, nf_conntrack_htable_size);
}

static inline u32 __hash_conntrack(const struct net *net,
			    const struct nf_conntrack_tuple *tuple,
			    unsigned int size)
{
	return reciprocal_scale(hash_conntrack_raw(tuple, net), size);
}

static inline u32 hash_conntrack(const struct net *net,
			  const struct nf_conntrack_tuple *tuple)
{
	return scale_hash(hash_conntrack_raw(tuple, net));
}

bool
nf_ct_get_tuple(const struct sk_buff *skb,
		unsigned int nhoff,
		unsigned int dataoff,
		u_int16_t l3num,
		u_int8_t protonum,
		struct net *net,
		struct nf_conntrack_tuple *tuple,
		const struct nf_conntrack_l3proto *l3proto,
		const struct nf_conntrack_l4proto *l4proto)
{
	memset(tuple, 0, sizeof(*tuple));

	tuple->src.l3num = l3num;
	if (l3proto->pkt_to_tuple(skb, nhoff, tuple) == 0)
		return false;

	tuple->dst.protonum = protonum;
	tuple->dst.dir = IP_CT_DIR_ORIGINAL;

	return l4proto->pkt_to_tuple(skb, dataoff, net, tuple);
}
EXPORT_SYMBOL_GPL(nf_ct_get_tuple);

bool nf_ct_get_tuplepr(const struct sk_buff *skb, unsigned int nhoff,
		       u_int16_t l3num,
		       struct net *net, struct nf_conntrack_tuple *tuple)
{
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;
	unsigned int protoff;
	u_int8_t protonum;
	int ret;

	rcu_read_lock();

	l3proto = __nf_ct_l3proto_find(l3num);
	ret = l3proto->get_l4proto(skb, nhoff, &protoff, &protonum);
	if (ret != NF_ACCEPT) {
		rcu_read_unlock();
		return false;
	}

	l4proto = __nf_ct_l4proto_find(l3num, protonum);

	ret = nf_ct_get_tuple(skb, nhoff, protoff, l3num, protonum, net, tuple,
			      l3proto, l4proto);

	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(nf_ct_get_tuplepr);

bool
nf_ct_invert_tuple(struct nf_conntrack_tuple *inverse,
		   const struct nf_conntrack_tuple *orig,
		   const struct nf_conntrack_l3proto *l3proto,
		   const struct nf_conntrack_l4proto *l4proto)
{
	memset(inverse, 0, sizeof(*inverse));

	inverse->src.l3num = orig->src.l3num;
	if (l3proto->invert_tuple(inverse, orig) == 0)
		return false;

	inverse->dst.dir = !orig->dst.dir;

	inverse->dst.protonum = orig->dst.protonum;
	return l4proto->invert_tuple(inverse, orig);
}
EXPORT_SYMBOL_GPL(nf_ct_invert_tuple);

/* Generate a almost-unique pseudo-id for a given conntrack.
 *
 * intentionally doesn't re-use any of the seeds used for hash
 * table location, we assume id gets exposed to userspace.
 *
 * Following nf_conn items do not change throughout lifetime
 * of the nf_conn:
 *
 * 1. nf_conn address
 * 2. nf_conn->master address (normally NULL)
 * 3. the associated net namespace
 * 4. the original direction tuple
 */
u32 nf_ct_get_id(const struct nf_conn *ct)
{
	static __read_mostly siphash_key_t ct_id_seed;
	unsigned long a, b, c, d;

	net_get_random_once(&ct_id_seed, sizeof(ct_id_seed));

	a = (unsigned long)ct;
	b = (unsigned long)ct->master;
	c = (unsigned long)nf_ct_net(ct);
	d = (unsigned long)siphash(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
				   sizeof(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple),
				   &ct_id_seed);
#ifdef CONFIG_64BIT
	return siphash_4u64((u64)a, (u64)b, (u64)c, (u64)d, &ct_id_seed);
#else
	return siphash_4u32((u32)a, (u32)b, (u32)c, (u32)d, &ct_id_seed);
#endif
}
EXPORT_SYMBOL_GPL(nf_ct_get_id);

static void
clean_from_lists(struct nf_conn *ct)
{
	pr_debug("clean_from_lists(%p)\n", ct);
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_REPLY].hnnode);

	/* Destroy all pending expectations */
	nf_ct_remove_expectations(ct);
}

/* must be called with local_bh_disable */
static void nf_ct_add_to_dying_list(struct nf_conn *ct)
{
	struct ct_pcpu *pcpu;

	/* add this conntrack to the (per cpu) dying list */
	ct->cpu = smp_processor_id();
	pcpu = per_cpu_ptr(nf_ct_net(ct)->ct.pcpu_lists, ct->cpu);

	spin_lock(&pcpu->lock);
	hlist_nulls_add_head(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
			     &pcpu->dying);
	spin_unlock(&pcpu->lock);
}

/* must be called with local_bh_disable */
static void nf_ct_add_to_unconfirmed_list(struct nf_conn *ct)
{
	struct ct_pcpu *pcpu;

	/* add this conntrack to the (per cpu) unconfirmed list */
	ct->cpu = smp_processor_id();
	pcpu = per_cpu_ptr(nf_ct_net(ct)->ct.pcpu_lists, ct->cpu);

	spin_lock(&pcpu->lock);
	hlist_nulls_add_head(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
			     &pcpu->unconfirmed);
	spin_unlock(&pcpu->lock);
}

/* must be called with local_bh_disable */
static void nf_ct_del_from_dying_or_unconfirmed_list(struct nf_conn *ct)
{
	struct ct_pcpu *pcpu;

	/* We overload first tuple to link into unconfirmed or dying list.*/
	pcpu = per_cpu_ptr(nf_ct_net(ct)->ct.pcpu_lists, ct->cpu);

	spin_lock(&pcpu->lock);
	BUG_ON(hlist_nulls_unhashed(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode));
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);
	spin_unlock(&pcpu->lock);
}

/* Released via destroy_conntrack() */
struct nf_conn *nf_ct_tmpl_alloc(struct net *net,
				 const struct nf_conntrack_zone *zone,
				 gfp_t flags)
{
	struct nf_conn *tmpl;

	tmpl = kzalloc(sizeof(*tmpl), flags);
	if (tmpl == NULL)
		return NULL;

	tmpl->status = IPS_TEMPLATE;
	write_pnet(&tmpl->ct_net, net);
	nf_ct_zone_add(tmpl, zone);
	atomic_set(&tmpl->ct_general.use, 0);

	return tmpl;
}
EXPORT_SYMBOL_GPL(nf_ct_tmpl_alloc);

void nf_ct_tmpl_free(struct nf_conn *tmpl)
{
	nf_ct_ext_destroy(tmpl);
	nf_ct_ext_free(tmpl);
	kfree(tmpl);
}
EXPORT_SYMBOL_GPL(nf_ct_tmpl_free);

static void
destroy_conntrack(struct nf_conntrack *nfct)
{
	struct nf_conn *ct = (struct nf_conn *)nfct;
	struct nf_conntrack_l4proto *l4proto;

	pr_debug("destroy_conntrack(%p)\n", ct);
	NF_CT_ASSERT(atomic_read(&nfct->use) == 0);

	if (unlikely(nf_ct_is_template(ct))) {
		nf_ct_tmpl_free(ct);
		return;
	}
	rcu_read_lock();
	l4proto = __nf_ct_l4proto_find(nf_ct_l3num(ct), nf_ct_protonum(ct));
	if (l4proto->destroy)
		l4proto->destroy(ct);

	rcu_read_unlock();

	local_bh_disable();
	/* Expectations will have been removed in clean_from_lists,
	 * except TFTP can create an expectation on the first packet,
	 * before connection is in the list, so we need to clean here,
	 * too.
	 */
	nf_ct_remove_expectations(ct);

	nf_ct_del_from_dying_or_unconfirmed_list(ct);

	local_bh_enable();

	if (ct->master)
		nf_ct_put(ct->master);

	pr_debug("destroy_conntrack: returning ct=%p to slab\n", ct);
	nf_conntrack_free(ct);
}

static void nf_ct_delete_from_lists(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);
	unsigned int hash, reply_hash;
	unsigned int sequence;

	nf_ct_helper_destroy(ct);

	local_bh_disable();
	do {
		sequence = read_seqcount_begin(&nf_conntrack_generation);
		hash = hash_conntrack(net,
				      &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
		reply_hash = hash_conntrack(net,
					   &ct->tuplehash[IP_CT_DIR_REPLY].tuple);
	} while (nf_conntrack_double_lock(net, hash, reply_hash, sequence));

	clean_from_lists(ct);
	nf_conntrack_double_unlock(hash, reply_hash);

	nf_ct_add_to_dying_list(ct);

	local_bh_enable();
}

bool nf_ct_delete(struct nf_conn *ct, u32 portid, int report)
{
	struct nf_conn_tstamp *tstamp;

	if (test_and_set_bit(IPS_DYING_BIT, &ct->status))
		return false;

	tstamp = nf_conn_tstamp_find(ct);
	if (tstamp) {
		s32 timeout = ct->timeout - nfct_time_stamp;

		tstamp->stop = ktime_get_real_ns();
		if (timeout < 0)
			tstamp->stop -= jiffies_to_nsecs(-timeout);
	}

	if (nf_conntrack_event_report(IPCT_DESTROY, ct,
				    portid, report) < 0) {
		/* destroy event was not delivered. nf_ct_put will
		 * be done by event cache worker on redelivery.
		 */
		nf_ct_delete_from_lists(ct);
		nf_conntrack_ecache_delayed_work(nf_ct_net(ct));
		return false;
	}

	nf_conntrack_ecache_work(nf_ct_net(ct));
	nf_ct_delete_from_lists(ct);
	nf_ct_put(ct);
	return true;
}
EXPORT_SYMBOL_GPL(nf_ct_delete);

static inline bool
nf_ct_key_equal(struct nf_conntrack_tuple_hash *h,
		const struct nf_conntrack_tuple *tuple,
		const struct nf_conntrack_zone *zone,
		const struct net *net)
{
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(h);

#ifdef CONFIG_NET_NS
	if (!net_eq(net, nf_ct_net(ct)))
		return false;
#endif

#ifdef CONFIG_NF_CONNTRACK_ZONES
	if (!nf_ct_zone_equal(ct, zone, NF_CT_DIRECTION(h)))
		return false;
#endif

	/* A conntrack can be recreated with the equal tuple,
	 * so we need to check that the conntrack is confirmed
	 */
	return nf_ct_tuple_equal(tuple, &h->tuple) &&
	       nf_ct_is_confirmed(ct);
}

/* caller must hold rcu readlock and none of the nf_conntrack_locks */
static void nf_ct_gc_expired(struct nf_conn *ct)
{
	if (!atomic_inc_not_zero(&ct->ct_general.use))
		return;

	if (nf_ct_should_gc(ct))
		nf_ct_kill(ct);

	nf_ct_put(ct);
}

/*
 * Warning :
 * - Caller must take a reference on returned object
 *   and recheck nf_ct_tuple_equal(tuple, &h->tuple)
 */
static struct nf_conntrack_tuple_hash *
____nf_conntrack_find(struct net *net, const struct nf_conntrack_zone *zone,
		      const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_head *ct_hash;
	struct hlist_nulls_node *n;
	unsigned int bucket, hsize;

begin:
	nf_conntrack_get_ht(&ct_hash, &hsize);
	bucket = reciprocal_scale(hash, hsize);

	hlist_nulls_for_each_entry_rcu(h, n, &ct_hash[bucket], hnnode) {
		struct nf_conn *ct;

		ct = nf_ct_tuplehash_to_ctrack(h);
		if (nf_ct_is_expired(ct)) {
			nf_ct_gc_expired(ct);
			continue;
		}

		if (nf_ct_is_dying(ct))
			continue;

		if (nf_ct_key_equal(h, tuple, zone, net))
			return h;
	}
	/*
	 * if the nulls value we got at the end of this lookup is
	 * not the expected one, we must restart lookup.
	 * We probably met an item that was moved to another chain.
	 */
	if (get_nulls_value(n) != bucket) {
		NF_CT_STAT_INC_ATOMIC(net, search_restart);
		goto begin;
	}

	return NULL;
}

#ifdef CONFIG_NAT_CONE
static inline bool
nf_cone_ct_key_equal(struct nf_conntrack_tuple_hash *h,
		     const struct nf_conntrack_tuple *tuple,
		     const struct net *net)
{
#ifdef CONFIG_NET_NS
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(h);

	if (!net_eq(net, nf_ct_net(ct)))
		return false;
#endif

	if (nf_conntrack_nat_mode == NAT_MODE_RCONE) {
		/* dst ip & dst port & dst proto & src ip  */
		return (__nf_ct_tuple_dst_equal(tuple, &h->tuple) &&
			tuple->src.l3num == h->tuple.src.l3num &&
			nf_inet_addr_cmp(&tuple->src.u3, &h->tuple.src.u3));
	}

	/* dst ip & dst port & dst proto */
	return __nf_ct_tuple_dst_equal(tuple, &h->tuple);
}

static struct nf_conntrack_tuple_hash *
__nf_cone_conntrack_find(struct net *net,
			 const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_head *ct_hash;
	struct hlist_nulls_node *n;
	unsigned int bucket, hsize;

begin:
	nf_conntrack_get_ht(&ct_hash, &hsize);
	bucket = reciprocal_scale(hash, hsize);

	hlist_nulls_for_each_entry_rcu(h, n, &ct_hash[bucket], hnnode) {
		struct nf_conn *ct;

		ct = nf_ct_tuplehash_to_ctrack(h);
		if (nf_ct_is_expired(ct)) {
			nf_ct_gc_expired(ct);
			continue;
		}

		if (nf_ct_is_dying(ct))
			continue;

		if (nf_cone_ct_key_equal(h, tuple, net))
			return h;
	}
	/*
	 * if the nulls value we got at the end of this lookup is
	 * not the expected one, we must restart lookup.
	 * We probably met an item that was moved to another chain.
	 */
	if (get_nulls_value(n) != bucket) {
		NF_CT_STAT_INC_ATOMIC(net, search_restart);
		goto begin;
	}

	return NULL;
}

/* Find a connection corresponding to a tuple. */
static struct nf_conntrack_tuple_hash *
__nf_cone_conntrack_find_get(struct net *net,
			     const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	rcu_read_lock();
begin:
	h = __nf_cone_conntrack_find(net, tuple, hash);
	if (h) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (unlikely(nf_ct_is_dying(ct) ||
			     !atomic_inc_not_zero(&ct->ct_general.use)))
			h = NULL;
		else {
			if (unlikely(!nf_cone_ct_key_equal(h, tuple, net))) {
				nf_ct_put(ct);
				goto begin;
			}
		}
	}
	rcu_read_unlock();

	return h;
}
#endif

/* Find a connection corresponding to a tuple. */
static struct nf_conntrack_tuple_hash *
__nf_conntrack_find_get(struct net *net, const struct nf_conntrack_zone *zone,
			const struct nf_conntrack_tuple *tuple, u32 hash)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	rcu_read_lock();
begin:
	h = ____nf_conntrack_find(net, zone, tuple, hash);
	if (h) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (unlikely(nf_ct_is_dying(ct) ||
			     !atomic_inc_not_zero(&ct->ct_general.use)))
			h = NULL;
		else {
			if (unlikely(!nf_ct_key_equal(h, tuple, zone, net))) {
				nf_ct_put(ct);
				goto begin;
			}
		}
	}
	rcu_read_unlock();

	return h;
}

struct nf_conntrack_tuple_hash *
nf_conntrack_find_get(struct net *net, const struct nf_conntrack_zone *zone,
		      const struct nf_conntrack_tuple *tuple)
{
	return __nf_conntrack_find_get(net, zone, tuple,
				       hash_conntrack_raw(tuple, net));
}
EXPORT_SYMBOL_GPL(nf_conntrack_find_get);

static void __nf_conntrack_hash_insert(struct nf_conn *ct,
				       unsigned int hash,
				       unsigned int reply_hash)
{
	hlist_nulls_add_head_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
			   &nf_conntrack_hash[hash]);
	hlist_nulls_add_head_rcu(&ct->tuplehash[IP_CT_DIR_REPLY].hnnode,
			   &nf_conntrack_hash[reply_hash]);
}

int
nf_conntrack_hash_check_insert(struct nf_conn *ct)
{
	const struct nf_conntrack_zone *zone;
	struct net *net = nf_ct_net(ct);
	unsigned int hash, reply_hash;
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	unsigned int sequence;

	zone = nf_ct_zone(ct);

	local_bh_disable();
	do {
		sequence = read_seqcount_begin(&nf_conntrack_generation);
		hash = hash_conntrack(net,
				      &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
		reply_hash = hash_conntrack(net,
					   &ct->tuplehash[IP_CT_DIR_REPLY].tuple);
	} while (nf_conntrack_double_lock(net, hash, reply_hash, sequence));

	/* See if there's one in the list already, including reverse */
	hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[hash], hnnode)
		if (nf_ct_key_equal(h, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
				    zone, net))
			goto out;

	hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[reply_hash], hnnode)
		if (nf_ct_key_equal(h, &ct->tuplehash[IP_CT_DIR_REPLY].tuple,
				    zone, net))
			goto out;

	smp_wmb();
	/* The caller holds a reference to this object */
	atomic_set(&ct->ct_general.use, 2);
	__nf_conntrack_hash_insert(ct, hash, reply_hash);
	nf_conntrack_double_unlock(hash, reply_hash);
	NF_CT_STAT_INC(net, insert);
	local_bh_enable();
	return 0;

out:
	nf_conntrack_double_unlock(hash, reply_hash);
	NF_CT_STAT_INC(net, insert_failed);
	local_bh_enable();
	return -EEXIST;
}
EXPORT_SYMBOL_GPL(nf_conntrack_hash_check_insert);

static inline void nf_ct_acct_update(struct nf_conn *ct,
				     enum ip_conntrack_info ctinfo,
				     unsigned int len)
{
	nf_ct_acct_add_packet_len(ct, ctinfo, len);
}

static void nf_ct_acct_merge(struct nf_conn *ct, enum ip_conntrack_info ctinfo,
			     const struct nf_conn *loser_ct)
{
	struct nf_conn_acct *acct;

	acct = nf_conn_acct_find(loser_ct);
	if (acct) {
		struct nf_conn_counter *counter = acct->counter;
		unsigned int bytes;

		/* u32 should be fine since we must have seen one packet. */
		bytes = atomic64_read(&counter[CTINFO2DIR(ctinfo)].bytes);
		nf_ct_acct_update(ct, ctinfo, bytes);
	}
}

/* Resolve race on insertion if this protocol allows this. */
static int nf_ct_resolve_clash(struct net *net, struct sk_buff *skb,
			       enum ip_conntrack_info ctinfo,
			       struct nf_conntrack_tuple_hash *h)
{
	/* This is the conntrack entry already in hashes that won race. */
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(h);
	struct nf_conntrack_l4proto *l4proto;

	l4proto = __nf_ct_l4proto_find(nf_ct_l3num(ct), nf_ct_protonum(ct));
	if (l4proto->allow_clash &&
	    ((ct->status & IPS_NAT_DONE_MASK) == 0) &&
	    !nf_ct_is_dying(ct) &&
	    atomic_inc_not_zero(&ct->ct_general.use)) {
		nf_ct_acct_merge(ct, ctinfo, (struct nf_conn *)skb->nfct);
		nf_conntrack_put(skb->nfct);
		/* Assign conntrack already in hashes to this skbuff. Don't
		 * modify skb->nfctinfo to ensure consistent stateful filtering.
		 */
		skb->nfct = &ct->ct_general;
		return NF_ACCEPT;
	}
	NF_CT_STAT_INC(net, drop);
	return NF_DROP;
}

/* Confirm a connection given skb; places it in hash table */
int
__nf_conntrack_confirm(struct sk_buff *skb)
{
	const struct nf_conntrack_zone *zone;
	unsigned int hash, reply_hash;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct nf_conn_help *help;
	struct nf_conn_tstamp *tstamp;
	struct hlist_nulls_node *n;
	enum ip_conntrack_info ctinfo;
	struct net *net;
	unsigned int sequence;
	int ret = NF_DROP;

	ct = nf_ct_get(skb, &ctinfo);
	net = nf_ct_net(ct);

	/* ipt_REJECT uses nf_conntrack_attach to attach related
	   ICMP/TCP RST packets in other direction.  Actual packet
	   which created connection will be IP_CT_NEW or for an
	   expected connection, IP_CT_RELATED. */
	if (CTINFO2DIR(ctinfo) != IP_CT_DIR_ORIGINAL)
		return NF_ACCEPT;

	zone = nf_ct_zone(ct);
	local_bh_disable();

	do {
		sequence = read_seqcount_begin(&nf_conntrack_generation);
		/* reuse the hash saved before */
		hash = *(unsigned long *)&ct->tuplehash[IP_CT_DIR_REPLY].hnnode.pprev;
		hash = scale_hash(hash);
		reply_hash = hash_conntrack(net,
					   &ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	} while (nf_conntrack_double_lock(net, hash, reply_hash, sequence));

	/* We're not in hash table, and we refuse to set up related
	 * connections for unconfirmed conns.  But packet copies and
	 * REJECT will give spurious warnings here.
	 */
	/* NF_CT_ASSERT(atomic_read(&ct->ct_general.use) == 1); */

	/* No external references means no one else could have
	 * confirmed us.
	 */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));
	pr_debug("Confirming conntrack %p\n", ct);
	/* We have to check the DYING flag after unlink to prevent
	 * a race against nf_ct_get_next_corpse() possibly called from
	 * user context, else we insert an already 'dead' hash, blocking
	 * further use of that particular connection -JM.
	 */
	nf_ct_del_from_dying_or_unconfirmed_list(ct);

	if (unlikely(nf_ct_is_dying(ct))) {
		nf_ct_add_to_dying_list(ct);
		goto dying;
	}

	/* See if there's one in the list already, including reverse:
	   NAT could have grabbed it without realizing, since we're
	   not in the hash.  If there is, we lost race. */
	hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[hash], hnnode)
		if (nf_ct_key_equal(h, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
				    zone, net))
			goto out;

	hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[reply_hash], hnnode)
		if (nf_ct_key_equal(h, &ct->tuplehash[IP_CT_DIR_REPLY].tuple,
				    zone, net))
			goto out;

	/* Timer relative to confirmation time, not original
	   setting time, otherwise we'd get timer wrap in
	   weird delay cases. */
	ct->timeout += nfct_time_stamp;
	atomic_inc(&ct->ct_general.use);
	ct->status |= IPS_CONFIRMED;

	/* set conntrack timestamp, if enabled. */
	tstamp = nf_conn_tstamp_find(ct);
	if (tstamp) {
		if (skb->tstamp.tv64 == 0)
			__net_timestamp(skb);

		tstamp->start = ktime_to_ns(skb->tstamp);
	}
	/* Since the lookup is lockless, hash insertion must be done after
	 * starting the timer and setting the CONFIRMED bit. The RCU barriers
	 * guarantee that no other CPU can find the conntrack before the above
	 * stores are visible.
	 */
	__nf_conntrack_hash_insert(ct, hash, reply_hash);
	nf_conntrack_double_unlock(hash, reply_hash);
	local_bh_enable();

	help = nfct_help(ct);
	if (help && help->helper)
		nf_conntrack_event_cache(IPCT_HELPER, ct);

	nf_conntrack_event_cache(master_ct(ct) ?
				 IPCT_RELATED : IPCT_NEW, ct);

	if (skb->dev && skb->dev->type != ARPHRD_ETHER)
		nf_ct_ext_ndm_output(skb);

	return NF_ACCEPT;

out:
	nf_ct_add_to_dying_list(ct);
	ret = nf_ct_resolve_clash(net, skb, ctinfo, h);
dying:
	nf_conntrack_double_unlock(hash, reply_hash);
	NF_CT_STAT_INC(net, insert_failed);
	local_bh_enable();
	return ret;
}
EXPORT_SYMBOL_GPL(__nf_conntrack_confirm);

/* Returns true if a connection correspondings to the tuple (required
   for NAT). */
int
nf_conntrack_tuple_taken(const struct nf_conntrack_tuple *tuple,
			 const struct nf_conn *ignored_conntrack)
{
	struct net *net = nf_ct_net(ignored_conntrack);
	const struct nf_conntrack_zone *zone;
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_head *ct_hash;
	unsigned int hash, hsize;
	struct hlist_nulls_node *n;
	struct nf_conn *ct;

	zone = nf_ct_zone(ignored_conntrack);

	rcu_read_lock();
 begin:
	nf_conntrack_get_ht(&ct_hash, &hsize);
	hash = __hash_conntrack(net, tuple, hsize);

	hlist_nulls_for_each_entry_rcu(h, n, &ct_hash[hash], hnnode) {
		ct = nf_ct_tuplehash_to_ctrack(h);

		if (ct == ignored_conntrack)
			continue;

		if (nf_ct_is_expired(ct)) {
			nf_ct_gc_expired(ct);
			continue;
		}

		if (nf_ct_key_equal(h, tuple, zone, net)) {
			/* Tuple is taken already, so caller will need to find
			 * a new source port to use.
			 *
			 * Only exception:
			 * If the *original tuples* are identical, then both
			 * conntracks refer to the same flow.
			 * This is a rare situation, it can occur e.g. when
			 * more than one UDP packet is sent from same socket
			 * in different threads.
			 *
			 * Let nf_ct_resolve_clash() deal with this later.
			 */
			if (nf_ct_tuple_equal(&ignored_conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
					      &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple) &&
					      nf_ct_zone_equal(ct, zone, IP_CT_DIR_ORIGINAL))
				continue;

			NF_CT_STAT_INC_ATOMIC(net, found);
			rcu_read_unlock();
			return 1;
		}
	}

	if (get_nulls_value(n) != hash) {
		NF_CT_STAT_INC_ATOMIC(net, search_restart);
		goto begin;
	}

	rcu_read_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(nf_conntrack_tuple_taken);

#define NF_CT_EVICTION_RANGE	8

/* There's a small race here where we may free a just-assured
   connection.  Too bad: we're in trouble anyway. */
static unsigned int early_drop_list(struct net *net,
				    struct hlist_nulls_head *head)
{
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	unsigned int drops = 0;
	struct nf_conn *tmp;

	hlist_nulls_for_each_entry_rcu(h, n, head, hnnode) {
		tmp = nf_ct_tuplehash_to_ctrack(h);

		if (nf_ct_is_expired(tmp)) {
			nf_ct_gc_expired(tmp);
			continue;
		}

		if (test_bit(IPS_ASSURED_BIT, &tmp->status) ||
		    !net_eq(nf_ct_net(tmp), net) ||
		    nf_ct_is_dying(tmp))
			continue;

		if (!atomic_inc_not_zero(&tmp->ct_general.use))
			continue;

		/* kill only if still in same netns -- might have moved due to
		 * SLAB_DESTROY_BY_RCU rules.
		 *
		 * We steal the timer reference.  If that fails timer has
		 * already fired or someone else deleted it. Just drop ref
		 * and move to next entry.
		 */
		if (net_eq(nf_ct_net(tmp), net) &&
		    nf_ct_is_confirmed(tmp) &&
		    nf_ct_delete(tmp, 0, 0))
			drops++;

		nf_ct_put(tmp);
	}

	return drops;
}

static noinline int early_drop(struct net *net, unsigned int hash)
{
	unsigned int i, bucket;

	for (i = 0; i < NF_CT_EVICTION_RANGE; i++) {
		struct hlist_nulls_head *ct_hash;
		unsigned int hsize, drops;

		rcu_read_lock();
		nf_conntrack_get_ht(&ct_hash, &hsize);
		if (!i)
			bucket = reciprocal_scale(hash, hsize);
		else
			bucket = (bucket + 1) % hsize;

		drops = early_drop_list(net, &ct_hash[bucket]);
		rcu_read_unlock();

		if (drops) {
			NF_CT_STAT_ADD_ATOMIC(net, early_drop, drops);
			return true;
		}
	}

	return false;
}

static void gc_worker(struct work_struct *work)
{
	unsigned int min_interval = max(HZ / GC_MAX_BUCKETS_DIV, 1u);
	unsigned int i, goal, buckets = 0, expired_count = 0;
	struct conntrack_gc_work *gc_work;
	unsigned int ratio, scanned = 0;
	unsigned long next_run;

	gc_work = container_of(work, struct conntrack_gc_work, dwork.work);

	goal = nf_conntrack_htable_size / GC_MAX_BUCKETS_DIV;
	i = gc_work->last_bucket;

	do {
		struct nf_conntrack_tuple_hash *h;
		struct hlist_nulls_head *ct_hash;
		struct hlist_nulls_node *n;
		unsigned int hashsz;
		struct nf_conn *tmp;

		i++;
		rcu_read_lock();

		nf_conntrack_get_ht(&ct_hash, &hashsz);
		if (i >= hashsz)
			i = 0;

		hlist_nulls_for_each_entry_rcu(h, n, &ct_hash[i], hnnode) {
			tmp = nf_ct_tuplehash_to_ctrack(h);

			scanned++;
			if (nf_ct_is_expired(tmp)) {
				nf_ct_gc_expired(tmp);
				expired_count++;
				continue;
			}
		}

		/* could check get_nulls_value() here and restart if ct
		 * was moved to another chain.  But given gc is best-effort
		 * we will just continue with next hash slot.
		 */
		rcu_read_unlock();
		cond_resched_rcu_qs();
	} while (++buckets < goal);

	if (unlikely(atomic_read(&nf_conntrack_public_locked))) {
		bool unlocked = false;

		spin_lock_bh(&nf_conntrack_public_lock);

		if (unlikely(time_after(
		    jiffies, nf_conntrack_public_locked_until))) {
			atomic_set(&nf_conntrack_public_locked, 0);
			unlocked = true;
		}

		spin_unlock_bh(&nf_conntrack_public_lock);

		if (unlikely(unlocked)) {
			net_warn_ratelimited(
				"public connections unlocked\n");
		}
	}

	if (gc_work->exiting)
		return;

	/*
	 * Eviction will normally happen from the packet path, and not
	 * from this gc worker.
	 *
	 * This worker is only here to reap expired entries when system went
	 * idle after a busy period.
	 *
	 * The heuristics below are supposed to balance conflicting goals:
	 *
	 * 1. Minimize time until we notice a stale entry
	 * 2. Maximize scan intervals to not waste cycles
	 *
	 * Normally, expire ratio will be close to 0.
	 *
	 * As soon as a sizeable fraction of the entries have expired
	 * increase scan frequency.
	 */
	ratio = scanned ? expired_count * 100 / scanned : 0;
	if (ratio > GC_EVICT_RATIO) {
		gc_work->next_gc_run = min_interval;
	} else {
		unsigned int max = GC_MAX_SCAN_JIFFIES / GC_MAX_BUCKETS_DIV;

		BUILD_BUG_ON((GC_MAX_SCAN_JIFFIES / GC_MAX_BUCKETS_DIV) == 0);

		gc_work->next_gc_run += min_interval;
		if (gc_work->next_gc_run > max)
			gc_work->next_gc_run = max;
	}

	next_run = gc_work->next_gc_run;
	gc_work->last_bucket = i;
	queue_delayed_work(system_power_efficient_wq, &gc_work->dwork, next_run);
}

static void conntrack_gc_work_init(struct conntrack_gc_work *gc_work)
{
	INIT_DELAYED_WORK(&gc_work->dwork, gc_worker);
	gc_work->next_gc_run = HZ;
	gc_work->exiting = false;
}

static struct nf_conn *
__nf_conntrack_alloc(struct net *net,
		     struct sk_buff *skb,
		     const struct nf_conntrack_zone *zone,
		     const struct nf_conntrack_tuple *orig,
		     const struct nf_conntrack_tuple *repl,
		     gfp_t gfp, u32 hash)
{
	struct nf_conn *ct;

	/* We don't want any race condition at early drop stage */
	atomic_inc(&net->ct.count);

	if (nf_conntrack_max &&
	    unlikely(atomic_read(&net->ct.count) > nf_conntrack_max)) {
		if (!early_drop(net, hash)) {
			atomic_dec(&net->ct.count);
			net_warn_ratelimited("nf_conntrack: table full, dropping packet\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	if (nf_conntrack_public_max &&
	    skb != NULL &&
	    nf_ct_ext_ndm_sl(net, skb) == NDM_SECURITY_LEVEL_PUBLIC) {
		bool locked = false;
		bool was_locked = false;

		spin_lock(&nf_conntrack_public_lock);

		was_locked = !!atomic_read(&nf_conntrack_public_locked);

		if (unlikely(!was_locked &&
		     atomic_read(&net->ct.public_count) >=
			nf_conntrack_public_max)) {
			nf_conntrack_public_locked_until =
				jiffies +
				msecs_to_jiffies(nf_conntrack_public_lockout_time);
			atomic_inc(&nf_conntrack_public_locked);
			locked = true;
		}

		spin_unlock(&nf_conntrack_public_lock);

		if (unlikely(!was_locked && locked)) {
			net_warn_ratelimited(
				"lockout threshold reached (%u), public connections locked for %u s\n",
				atomic_read(&net->ct.public_count),
				nf_conntrack_public_lockout_time / 1000);
		}

		if (unlikely(was_locked || locked)) {
			atomic_dec(&net->ct.count);
			return ERR_PTR(-E2BIG);
		}
	}

	/*
	 * Do not use kmem_cache_zalloc(), as this cache uses
	 * SLAB_DESTROY_BY_RCU.
	 */
	ct = kmem_cache_alloc(nf_conntrack_cachep, gfp);
	if (ct == NULL)
		goto out;

	spin_lock_init(&ct->lock);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple = *orig;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode.pprev = NULL;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple = *repl;
	/* save hash for reusing when confirming */
	*(unsigned long *)(&ct->tuplehash[IP_CT_DIR_REPLY].hnnode.pprev) = hash;
	ct->status = 0;
	write_pnet(&ct->ct_net, net);
	memset(&ct->__nfct_init_offset, 0,
	       offsetof(struct nf_conn, proto) -
	       offsetof(struct nf_conn, __nfct_init_offset));

	nf_ct_zone_add(ct, zone);

	/* Because we use RCU lookups, we set ct_general.use to zero before
	 * this is inserted in any list.
	 */
	atomic_set(&ct->ct_general.use, 0);
	return ct;
out:
	atomic_dec(&net->ct.count);
	return ERR_PTR(-ENOMEM);
}

struct nf_conn *nf_conntrack_alloc(struct net *net,
				   const struct nf_conntrack_zone *zone,
				   const struct nf_conntrack_tuple *orig,
				   const struct nf_conntrack_tuple *repl,
				   gfp_t gfp)
{
	return __nf_conntrack_alloc(net, NULL, zone, orig, repl, gfp, 0);
}
EXPORT_SYMBOL_GPL(nf_conntrack_alloc);

void nf_conntrack_free(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);
	typeof(nacct_conntrack_free) pfunc;
	const int ct_public = test_bit(IPS_NDM_FROM_PUBLIC_BIT, &ct->status);

	rcu_read_lock();
	pfunc = rcu_dereference(nacct_conntrack_free);
	if (pfunc)
		pfunc(ct);
	rcu_read_unlock();

	/* A freed object has refcnt == 0, that's
	 * the golden rule for SLAB_DESTROY_BY_RCU
	 */
	NF_CT_ASSERT(atomic_read(&ct->ct_general.use) == 0);

	nf_ct_ext_destroy(ct);
	nf_ct_ext_free(ct);
	kmem_cache_free(nf_conntrack_cachep, ct);
	smp_mb__before_atomic();
	atomic_dec(&net->ct.count);

	if (ct_public)
		atomic_dec(&net->ct.public_count);
}
EXPORT_SYMBOL_GPL(nf_conntrack_free);


/* Allocate a new conntrack: we return -ENOMEM if classification
   failed due to stress.  Otherwise it really is unclassifiable. */
static struct nf_conntrack_tuple_hash *
init_conntrack(struct net *net, struct nf_conn *tmpl,
	       const struct nf_conntrack_tuple *tuple,
	       struct nf_conntrack_l3proto *l3proto,
	       struct nf_conntrack_l4proto *l4proto,
	       struct sk_buff *skb,
	       unsigned int dataoff, u32 hash)
{
	struct nf_conn *ct;
	struct nf_conn_help *help;
	struct nf_conntrack_tuple repl_tuple;
	struct nf_conntrack_ecache *ecache;
	struct nf_conntrack_expect *exp = NULL;
	const struct nf_conntrack_zone *zone;
	struct nf_conn_timeout *timeout_ext;
	struct nf_conntrack_zone tmp;
	unsigned int *timeouts;

	if (!nf_ct_invert_tuple(&repl_tuple, tuple, l3proto, l4proto)) {
		pr_debug("Can't invert tuple.\n");
		return NULL;
	}

	zone = nf_ct_zone_tmpl(tmpl, skb, &tmp);
	ct = __nf_conntrack_alloc(net, skb, zone, tuple, &repl_tuple, GFP_ATOMIC,
				  hash);
	if (IS_ERR(ct))
		return (struct nf_conntrack_tuple_hash *)ct;

	if (!nf_ct_add_synproxy(ct, tmpl)) {
		nf_conntrack_free(ct);
		return ERR_PTR(-ENOMEM);
	}

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
	if (__nf_ct_ext_add_length(ct, nf_ct_ext_id_ntc,
				   sizeof(struct nf_ct_ext_ntc_label),
				   GFP_ATOMIC) == NULL) {
		nf_conntrack_free(ct);
		return ERR_PTR(-ENOMEM);
	}
#endif

	nf_ct_ntce_append(NF_INET_PRE_ROUTING, ct);
#ifdef CONFIG_NTCE_MODULE
	ct->created = jiffies;
#endif

	timeout_ext = tmpl ? nf_ct_timeout_find(tmpl) : NULL;
	if (timeout_ext) {
		timeouts = nf_ct_timeout_data(timeout_ext);
		if (unlikely(!timeouts))
			timeouts = l4proto->get_timeouts(net);
	} else {
		timeouts = l4proto->get_timeouts(net);
	}

	if (!l4proto->new(ct, skb, dataoff, timeouts)) {
		nf_conntrack_free(ct);
		pr_debug("can't track with proto module\n");
		return NULL;
	}

	if (timeout_ext)
		nf_ct_timeout_ext_add(ct, rcu_dereference(timeout_ext->timeout),
				      GFP_ATOMIC);

	nf_ct_acct_ext_add(ct, GFP_ATOMIC);
	nf_ct_tstamp_ext_add(ct, GFP_ATOMIC);
	nf_ct_labels_ext_add(ct);

	ecache = tmpl ? nf_ct_ecache_find(tmpl) : NULL;
	nf_ct_ecache_ext_add(ct, ecache ? ecache->ctmask : 0,
				 ecache ? ecache->expmask : 0,
			     GFP_ATOMIC);

	local_bh_disable();
	if (net->ct.expect_count) {
		spin_lock(&nf_conntrack_expect_lock);
		exp = nf_ct_find_expectation(net, zone, tuple);
		if (exp) {
			pr_debug("expectation arrives ct=%p exp=%p\n",
				 ct, exp);
			/* Welcome, Mr. Bond.  We've been expecting you... */
			__set_bit(IPS_EXPECTED_BIT, &ct->status);
			/* exp->master safe, refcnt bumped in nf_ct_find_expectation */
			ct->master = exp->master;
			if (exp->helper) {
				help = nf_ct_helper_ext_add(ct, exp->helper,
							    GFP_ATOMIC);
				if (help)
					rcu_assign_pointer(help->helper, exp->helper);
			}

#ifdef CONFIG_NF_CONNTRACK_MARK
			ct->mark = exp->master->mark;
			ct->ndm_mark = exp->master->ndm_mark;
#endif
#ifdef CONFIG_NF_CONNTRACK_SECMARK
			ct->secmark = exp->master->secmark;
#endif
			NF_CT_STAT_INC(net, expect_new);
		}
		spin_unlock(&nf_conntrack_expect_lock);
	}
	if (!exp)
		__nf_ct_try_assign_helper(ct, tmpl, GFP_ATOMIC);

	/* Now it is inserted into the unconfirmed list, bump refcount */
	nf_conntrack_get(&ct->ct_general);
	nf_ct_add_to_unconfirmed_list(ct);

	local_bh_enable();

	if (exp) {
		if (exp->expectfn)
			exp->expectfn(ct, exp);
		nf_ct_expect_put(exp);
	}

	nf_ct_ext_ndm_input(l3proto->l3proto, net, skb, ct);

	return &ct->tuplehash[IP_CT_DIR_ORIGINAL];
}

/* On success, returns conntrack ptr, sets skb->nfct and ctinfo */
static inline struct nf_conn *
resolve_normal_ct(struct net *net, struct nf_conn *tmpl,
		  struct sk_buff *skb,
		  unsigned int dataoff,
		  u_int16_t l3num,
		  u_int8_t protonum,
		  struct nf_conntrack_l3proto *l3proto,
		  struct nf_conntrack_l4proto *l4proto,
		  int *set_reply,
		  enum ip_conntrack_info *ctinfo)
{
	const struct nf_conntrack_zone *zone;
	struct nf_conntrack_tuple tuple;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conntrack_zone tmp;
	struct nf_conn *ct;
	u32 hash;

	if (!nf_ct_get_tuple(skb, skb_network_offset(skb),
			     dataoff, l3num, protonum, net, &tuple, l3proto,
			     l4proto)) {
		pr_debug("Can't get tuple\n");
		return NULL;
	}

	/* look for tuple match */
	zone = nf_ct_zone_tmpl(tmpl, skb, &tmp);
	hash = hash_conntrack_raw(&tuple, net);

#ifdef CONFIG_NAT_CONE
	/*
	 * Based on NAT treatments of UDP in RFC3489:
	 *
	 * 1)Full Cone: A full cone NAT is one where all requests from the
	 *   same internal IP address and port are mapped to the same external
	 *   IP address and port.  Furthermore, any external host can send a
	 *   packet to the internal host, by sending a packet to the mapped
	 *   external address.
	 *
	 * 2)Restricted Cone: A restricted cone NAT is one where all requests
	 *   from the same internal IP address and port are mapped to the same
	 *   external IP address and port.  Unlike a full cone NAT, an external
	 *   host (with IP address X) can send a packet to the internal host
	 *   only if the internal host had previously sent a packet to IP
	 *   address X.
	 *
	 * 3)Port Restricted Cone: A port restricted cone NAT is like a
	 *   restricted cone NAT, but the restriction includes port numbers.
	 *   Specifically, an external host can send a packet, with source IP
	 *   address X and source port P, to the internal host only if the
	 *   internal host had previously sent a packet to IP address X and
	 *   port P.
	 *
	 * 4)Symmetric: A symmetric NAT is one where all requests from the
	 *   same internal IP address and port, to a specific destination IP
	 *   address and port, are mapped to the same external IP address and
	 *   port.  If the same host sends a packet with the same source
	 *   address and port, but to a different destination, a different
	 *   mapping is used.  Furthermore, only the external host that
	 *   receives a packet can send a UDP packet back to the internal host.
	 *
	 *
	 * Original Linux NAT type is hybrid 'port restricted cone' and
	 * 'symmetric'. XBOX certificate recommands NAT type is 'fully cone'
	 * or 'restricted cone', so I patch the linux kernel to support
	 * this feature
	 * Tradition scenario from LAN->WAN:
	 *
	 *        (LAN)     (WAN)
	 * Client------>AP---------> Server
	 * -------------> (I)
	 *              -------------->(II)
	 *              <--------------(III)
	 * <------------- (IV)
	 *
	 *
	 * (CASE I/II/IV) Compared Tuple=src_ip/port & dst_ip/port & proto
	 * (CASE III)  Compared Tuple:
	 *             Fully cone=dst_ip/port & proto
	 *             Restricted Cone=dst_ip/port & proto & src_ip
	 *
	 */
	if (protonum == IPPROTO_UDP && nf_conntrack_nat_mode != NAT_MODE_LINUX &&
	    skb->dev && skb->dev->ifindex == nf_conntrack_nat_cone_ifindex) {
		/* CASE III To Cone NAT */
		h = __nf_cone_conntrack_find_get(net, &tuple, hash);
	} else
#endif
	{
		/* CASE I.II.IV To Linux NAT */
		h = __nf_conntrack_find_get(net, zone, &tuple, hash);
	}

	if (!h) {
#if IS_ENABLED(CONFIG_FAST_NAT)
		if (unlikely(SWNAT_KA_CHECK_MARK(skb)))
			return NULL;
#endif
#if IS_ENABLED(CONFIG_RA_HW_NAT)
		if (unlikely(FOE_SKB_IS_KEEPALIVE(skb)))
			return NULL;
#endif
		h = init_conntrack(net, tmpl, &tuple, l3proto, l4proto,
				   skb, dataoff, hash);
		if (!h)
			return NULL;
		if (IS_ERR(h))
			return (void *)h;
	}
	ct = nf_ct_tuplehash_to_ctrack(h);

	/* It exists; we have (non-exclusive) reference. */
	if (NF_CT_DIRECTION(h) == IP_CT_DIR_REPLY) {
		*ctinfo = IP_CT_ESTABLISHED_REPLY;
		/* Please set reply bit if this packet OK */
		*set_reply = 1;
	} else {
		/* Once we've had two way comms, always ESTABLISHED. */
		if (test_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
			pr_debug("normal packet for %p\n", ct);
			*ctinfo = IP_CT_ESTABLISHED;
		} else if (test_bit(IPS_EXPECTED_BIT, &ct->status)) {
			pr_debug("related packet for %p\n", ct);
			*ctinfo = IP_CT_RELATED;
		} else {
			pr_debug("new packet for %p\n", ct);
			*ctinfo = IP_CT_NEW;
		}
		*set_reply = 0;
	}
	skb->nfct = &ct->ct_general;
	skb->nfctinfo = *ctinfo;
	return ct;
}

unsigned int
nf_conntrack_in(struct net *net, u_int8_t pf, unsigned int hooknum,
		struct sk_buff *skb)
{
	struct nf_conn *ct, *tmpl = NULL;
	enum ip_conntrack_info ctinfo;
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;
	struct nf_conn_help *help;
	unsigned int *timeouts;
	unsigned int dataoff;
	u_int8_t protonum;
	int set_reply = 0;
	int ret;

	if (skb->nfct) {
		/* Previously seen (loopback or untracked)?  Ignore. */
		tmpl = (struct nf_conn *)skb->nfct;
		if (!nf_ct_is_template(tmpl)) {
			NF_CT_STAT_INC_ATOMIC(net, ignore);
			return NF_ACCEPT;
		}
		skb->nfct = NULL;
	}

	/* rcu_read_lock()ed by nf_hook_thresh */
	l3proto = __nf_ct_l3proto_find(pf);
	ret = l3proto->get_l4proto(skb, skb_network_offset(skb),
				   &dataoff, &protonum);
	if (ret <= 0) {
		pr_debug("not prepared to track yet or error occurred\n");
		NF_CT_STAT_INC_ATOMIC(net, error);
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		ret = -ret;
		goto out;
	}

	l4proto = __nf_ct_l4proto_find(pf, protonum);

	/* It may be an special packet, error, unclean...
	 * inverse of the return code tells to the netfilter
	 * core what to do with the packet. */
	if (l4proto->error != NULL) {
		ret = l4proto->error(net, tmpl, skb, dataoff, &ctinfo,
				     pf, hooknum);
		if (ret <= 0) {
			NF_CT_STAT_INC_ATOMIC(net, error);
			NF_CT_STAT_INC_ATOMIC(net, invalid);
			ret = -ret;
			goto out;
		}
		/* ICMP[v6] protocol trackers may assign one conntrack. */
		if (skb->nfct)
			goto out;
	}

	ct = resolve_normal_ct(net, tmpl, skb, dataoff, pf, protonum,
			       l3proto, l4proto, &set_reply, &ctinfo);
	if (!ct) {
		/* Not valid part of a connection */
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		ret = NF_ACCEPT;
		goto out;
	}

	if (IS_ERR(ct)) {
		/* Too stressed to deal. */
		NF_CT_STAT_INC_ATOMIC(net, drop);
		ret = NF_DROP;
		goto out;
	}

	NF_CT_ASSERT(skb->nfct);

	nf_ntce_rst_bypass(pf, skb);
	nf_ntce_rst_enqueue(pf, skb);

#if IS_ENABLED(CONFIG_FAST_NAT) || IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	if (!ct->fast_bind_reached) {
		struct nf_conn_acct *acct = nf_conn_acct_find(ct);
		struct nf_ct_ext_ntce_label *ntce = nf_ct_ext_find_ntce(ct);

		if (likely(acct != NULL)) {
			struct nf_conn_counter *counter = acct->counter;
			uint64_t pkt_o, pkt_r;
			const bool ntce_fp =
				(ntce != NULL && nf_ct_ext_ntce_fastpath(ntce));
			const bool ntce_enabled = !ntce_fp && nf_ntce_is_enabled();
			const size_t limit_half = nf_ntce_fastnat_limit(
				FAST_NAT_BIND_PKT_DIR_HALF, ntce_enabled);
			const size_t limit_both = nf_ntce_fastnat_limit(
				FAST_NAT_BIND_PKT_DIR_BOTH, ntce_enabled);

			pkt_r = atomic64_read(&counter[IP_CT_DIR_REPLY].packets);
			pkt_o = atomic64_read(&counter[IP_CT_DIR_ORIGINAL].packets);

			if (pkt_o > limit_both &&
			    pkt_r > limit_both)
				ct->fast_bind_reached = 1;
			else if (protonum == IPPROTO_UDP &&
				 (pkt_o > limit_half ||
				  pkt_r > limit_half))
				ct->fast_bind_reached = 1;

			nf_ntce_check_limit(ct, pf, skb, pkt_r + pkt_o);
		}
	}
#endif

	/* Decide what timeout policy we want to apply to this flow. */
	timeouts = nf_ct_timeout_lookup(net, ct, l4proto);

	ret = l4proto->packet(ct, skb, dataoff, ctinfo, pf, hooknum, timeouts);
	if (ret <= 0) {
		/* Invalid: inverse of the return code tells
		 * the netfilter core what to do */
		pr_debug("nf_conntrack_in: Can't track with proto module\n");
		nf_conntrack_put(skb->nfct);
		skb->nfct = NULL;
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		if (ret == -NF_DROP)
			NF_CT_STAT_INC_ATOMIC(net, drop);
		ret = -ret;
		goto out;
	}

	help = nfct_help(ct);
	if (help && help->helper) {
#if IS_ENABLED(CONFIG_RA_HW_NAT)
		FOE_ALG_MARK(skb);
#endif
#if IS_ENABLED(CONFIG_FAST_NAT)
		ct->fast_ext = 1;
#endif
	} else if (skb_sec_path(skb)) {
#if IS_ENABLED(CONFIG_RA_HW_NAT)
		FOE_ALG_MARK(skb);
#endif
#if IS_ENABLED(CONFIG_FAST_NAT)
#ifdef CONFIG_FAST_NAT_V2
#ifdef CONFIG_XFRM
		if (!nf_fastnat_xfrm_control)
#endif /* CONFIG_XFRM */
#endif /* CONFIG_FAST_NAT_V2 */
			ct->fast_ext = 1;
#endif
	}

	nf_ct_ext_ndm_mark_skb(skb, ct, ctinfo);

#if IS_ENABLED(CONFIG_FAST_NAT)
	/* check IPv4 condition */
	if (pf != PF_INET)
		goto fast_nat_exit;

	/* check fastpath condition */
	if (!(hooknum == NF_INET_PRE_ROUTING &&
	      (ctinfo == IP_CT_ESTABLISHED || ctinfo == IP_CT_ESTABLISHED_REPLY) &&
	      !SWNAT_KA_CHECK_MARK(skb)))
		goto fast_nat_exit;

	/* check fastnat condition */
	if ((protonum == IPPROTO_UDP || protonum == IPPROTO_TCP) &&
	   !ct->fast_ext &&
	    ct->fast_bind_reached &&
	    nf_fastnat_control &&
	    is_nf_connection_ready_nat(ct) &&
	    is_nf_connection_has_nat(ct)) {
		unsigned int ntce_skip_swnat = 1;
		u8 _l4hdr[4];
		__be32 orig_src, new_src;
		__be16 orig_port = 0;
#ifdef CONFIG_NF_CONNTRACK_MARK
		u32 oldmark = skb->mark;
#endif
		orig_src = ip_hdr(skb)->saddr;

		if (protonum == IPPROTO_TCP) {
			const struct tcphdr *tcph;

			/* actually only need first 4 bytes to get ports. */
			tcph = skb_header_pointer(skb, dataoff, sizeof(_l4hdr), _l4hdr);
			if (tcph)
				orig_port = tcph->source;
		} else {
			const struct udphdr *udph;

			/* actually only need first 4 bytes to get ports. */
			udph = skb_header_pointer(skb, dataoff, sizeof(_l4hdr), _l4hdr);
			if (udph)
				orig_port = udph->source;
		}

#ifdef CONFIG_NF_CONNTRACK_MARK
		if (ct->mark != 0)
			skb->mark = ct->mark;
#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
		if (xt_connndmmark_is_keep_ct(ct))
			xt_ndmmark_set_keep_ct(skb);
#endif
#endif

		if (unlikely(nf_nsc_update_dscp(ct, skb))) {
			ret = NF_DROP;
			goto out;
		}

		ntce_skip_swnat = nf_ntce_enqueue_in(pf, hooknum, ct, skb);

		ret = fast_nat_do_bind(ct, skb, l3proto, l4proto, ctinfo);

		new_src = ip_hdr(skb)->saddr;

		/* Get rid of junky binds, do swnat only when src IP changed */
		if (orig_src != new_src && !ntce_skip_swnat) {
			typeof(prebind_from_fastnat) swnat_prebind =
				rcu_dereference(prebind_from_fastnat);

			/* rcu_read_lock()ed by nf_hook_thresh */
			if (swnat_prebind) {
				struct swnat_fastnat_t fastnat =
				{
					.skb = skb,
					.orig_saddr = orig_src,
					.orig_sport = orig_port,
					.ct = ct,
					.ctinfo = ctinfo
				};

				swnat_prebind(&fastnat);

				/* Set mark for further binds */
				SWNAT_FNAT_SET_MARK(skb);
			}
		}

		if (ret == NF_FAST_NAT && orig_src != new_src)
			ret = fast_nat_ntc_ingress(net, skb, orig_src);
#ifdef CONFIG_NF_CONNTRACK_MARK
		else
			skb->mark = oldmark;
#endif
		goto fast_nat_exit;
	}

#ifdef CONFIG_FAST_NAT_V2
#if defined(CONFIG_XFRM) || \
    IS_ENABLED(CONFIG_PPTP) || \
    IS_ENABLED(CONFIG_PPPOL2TP) || \
    IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	if (!is_nf_connection_has_no_nat(ct))
		goto fast_nat_exit;
#endif

#if IS_ENABLED(CONFIG_PPTP)
	/* check PPTP GRE fastpath condition */
	if (protonum == IPPROTO_GRE &&
	    nf_fastpath_pptp_control) {
		const struct pptp_gre_header *pgh;
		u8 _l4hdr[2 * sizeof(u32)];

		pgh = skb_header_pointer(skb, dataoff, sizeof(_l4hdr), _l4hdr);

		if (pgh && pgh->gre_hd.protocol == GRE_PROTO_PPP &&
		    !GRE_IS_CSUM(pgh->gre_hd.flags) &&
		    !GRE_IS_ROUTING(pgh->gre_hd.flags) &&
		     GRE_IS_KEY(pgh->gre_hd.flags) &&
		    !(pgh->gre_hd.flags & GRE_FLAGS)) {
			unsigned int len = skb->len;
			typeof(nf_fastpath_pptp_in) pptp_in;

			pptp_in = rcu_dereference(nf_fastpath_pptp_in);
			if (pptp_in &&
			    pptp_in(skb, dataoff, ntohs(pgh->call_id))) {
				nf_ct_acct_add_packet_len(ct, ctinfo, len);
				ret = NF_STOLEN;
			}
		}

		goto fast_nat_exit_no_ntce;
	}
#endif /* CONFIG_PPTP */

#ifdef CONFIG_XFRM
	/* check IPsec ESP fastpath condition */
	if ((protonum == IPPROTO_UDP || protonum == IPPROTO_ESP) &&
	    nf_fastpath_esp_control) {
		unsigned int len = skb->len;
		int rv = nf_fastpath_esp4_in(net, skb, dataoff, protonum);

		if (rv == NF_ACCEPT) {
			/* for non NAT-T UDP we must pass next */
			if (protonum == IPPROTO_ESP)
				goto fast_nat_exit_no_ntce;
		} else {
			ret = rv;
			if (rv == NF_STOLEN)
				nf_ct_acct_add_packet_len(ct, ctinfo, len);
			goto fast_nat_exit_no_ntce;
		}
	}
#endif /* CONFIG_XFRM */

#if IS_ENABLED(CONFIG_PPPOL2TP)
	/* check L2TP fastpath condition */
	if (protonum == IPPROTO_UDP &&
	    nf_fastpath_l2tp_control) {
		unsigned int len = skb->len;
		typeof(nf_fastpath_l2tp_in) l2tp_in;

		l2tp_in = rcu_dereference(nf_fastpath_l2tp_in);
		if (l2tp_in && l2tp_in(skb, dataoff)) {
			nf_ct_acct_add_packet_len(ct, ctinfo, len);
			ret = NF_STOLEN;
			goto fast_nat_exit_no_ntce;
		}
	}
#endif /* CONFIG_PPPOL2TP */

#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	/* check pure fastroute condition */
	if ((protonum == IPPROTO_UDP || protonum == IPPROTO_TCP) &&
	    skb->dev &&
	   !ct->fast_ext &&
	    ct->fast_bind_reached &&
	    nf_fastroute_control) {
		int ifindex = skb->dev->ifindex;
		typeof(nf_fastroute_rtcache_in) do_rtcache_in;

		do_rtcache_in = rcu_dereference(nf_fastroute_rtcache_in);

		if (do_rtcache_in && do_rtcache_in(pf, skb, ifindex)) {
#ifdef CONFIG_NF_CONNTRACK_MARK
			if (ct->mark != 0)
				skb->mark = ct->mark;
#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
			if (ct->ndm_mark != 0)
				skb->ndm_mark = ct->ndm_mark;
#endif
#endif
			nf_ntce_enqueue_in(pf, hooknum, ct, skb);

			/* Change skb owner to output device */
			skb->dev = skb_dst(skb)->dev;

			/* Simple bypass to exit, no real NAT */
			ret = fast_nat_ntc_ingress(net, skb, ip_hdr(skb)->saddr);
		}
	}
#endif /* CONFIG_NF_CONNTRACK_RTCACHE */
#endif /* CONFIG_FAST_NAT_V2 */

fast_nat_exit:

	nf_ntce_enqueue_in(pf, hooknum, ct, skb);

fast_nat_exit_no_ntce:

#endif

	if (set_reply && !test_and_set_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
#if IS_ENABLED(CONFIG_FAST_NAT)
		if (hooknum == NF_INET_LOCAL_OUT)
			ct->fast_ext = 1;
#endif
		nf_conntrack_event_cache(IPCT_REPLY, ct);
	}
out:
	if (tmpl) {
		/* Special case: we have to repeat this hook, assign the
		 * template again to this packet. We assume that this packet
		 * has no conntrack assigned. This is used by nf_ct_tcp. */
		if (ret == NF_REPEAT)
			skb->nfct = (struct nf_conntrack *)tmpl;
		else
			nf_ct_put(tmpl);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(nf_conntrack_in);

bool nf_ct_invert_tuplepr(struct nf_conntrack_tuple *inverse,
			  const struct nf_conntrack_tuple *orig)
{
	bool ret;

	rcu_read_lock();
	ret = nf_ct_invert_tuple(inverse, orig,
				 __nf_ct_l3proto_find(orig->src.l3num),
				 __nf_ct_l4proto_find(orig->src.l3num,
						      orig->dst.protonum));
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(nf_ct_invert_tuplepr);

/* Alter reply tuple (maybe alter helper).  This is for NAT, and is
   implicitly racy: see __nf_conntrack_confirm */
void nf_conntrack_alter_reply(struct nf_conn *ct,
			      const struct nf_conntrack_tuple *newreply)
{
	struct nf_conn_help *help = nfct_help(ct);

	/* Should be unconfirmed, so not in hash table yet */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));

	pr_debug("Altering reply tuple of %p to ", ct);
	nf_ct_dump_tuple(newreply);

	ct->tuplehash[IP_CT_DIR_REPLY].tuple = *newreply;
	if (ct->master || (help && !hlist_empty(&help->expectations)))
		return;

	rcu_read_lock();
	__nf_ct_try_assign_helper(ct, NULL, GFP_ATOMIC);
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(nf_conntrack_alter_reply);

/* Refresh conntrack for this many jiffies and do accounting if do_acct is 1 */
void __nf_ct_refresh_acct(struct nf_conn *ct,
			  enum ip_conntrack_info ctinfo,
			  const struct sk_buff *skb,
			  unsigned long extra_jiffies,
			  int do_acct)
{
	NF_CT_ASSERT(skb);

	/* Only update if this is not a fixed timeout */
	if (test_bit(IPS_FIXED_TIMEOUT_BIT, &ct->status))
		goto acct;

	/* If not in hash table, timer will not be active yet */
	if (nf_ct_is_confirmed(ct))
		extra_jiffies += nfct_time_stamp;

	ct->timeout = extra_jiffies;
acct:
#if IS_ENABLED(CONFIG_FAST_NAT)
	if (unlikely(SWNAT_KA_CHECK_MARK(skb)))
		return;
#endif
#if IS_ENABLED(CONFIG_RA_HW_NAT)
	if (unlikely(FOE_SKB_IS_KEEPALIVE(skb)))
		return;
#endif
	if (do_acct)
		nf_ct_acct_update(ct, ctinfo, skb->len);
}
EXPORT_SYMBOL_GPL(__nf_ct_refresh_acct);

bool nf_ct_kill_acct(struct nf_conn *ct,
		     enum ip_conntrack_info ctinfo,
		     const struct sk_buff *skb)
{
	return nf_ct_delete(ct, 0, 0);
}
EXPORT_SYMBOL_GPL(nf_ct_kill_acct);

#if IS_ENABLED(CONFIG_NF_CT_NETLINK)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <linux/mutex.h>

/* Generic function for tcp/udp/sctp/dccp and alike. This needs to be
 * in ip_conntrack_core, since we don't want the protocols to autoload
 * or depend on ctnetlink */
int nf_ct_port_tuple_to_nlattr(struct sk_buff *skb,
			       const struct nf_conntrack_tuple *tuple)
{
	if (nla_put_be16(skb, CTA_PROTO_SRC_PORT, tuple->src.u.tcp.port) ||
	    nla_put_be16(skb, CTA_PROTO_DST_PORT, tuple->dst.u.tcp.port))
		goto nla_put_failure;
	return 0;

nla_put_failure:
	return -1;
}
EXPORT_SYMBOL_GPL(nf_ct_port_tuple_to_nlattr);

const struct nla_policy nf_ct_port_nla_policy[CTA_PROTO_MAX+1] = {
	[CTA_PROTO_SRC_PORT]  = { .type = NLA_U16 },
	[CTA_PROTO_DST_PORT]  = { .type = NLA_U16 },
};
EXPORT_SYMBOL_GPL(nf_ct_port_nla_policy);

int nf_ct_port_nlattr_to_tuple(struct nlattr *tb[],
			       struct nf_conntrack_tuple *t)
{
	if (!tb[CTA_PROTO_SRC_PORT] || !tb[CTA_PROTO_DST_PORT])
		return -EINVAL;

	t->src.u.tcp.port = nla_get_be16(tb[CTA_PROTO_SRC_PORT]);
	t->dst.u.tcp.port = nla_get_be16(tb[CTA_PROTO_DST_PORT]);

	return 0;
}
EXPORT_SYMBOL_GPL(nf_ct_port_nlattr_to_tuple);

int nf_ct_port_nlattr_tuple_size(void)
{
	return nla_policy_len(nf_ct_port_nla_policy, CTA_PROTO_MAX + 1);
}
EXPORT_SYMBOL_GPL(nf_ct_port_nlattr_tuple_size);
#endif

/* Used by ipt_REJECT and ip6t_REJECT. */
static void nf_conntrack_attach(struct sk_buff *nskb, const struct sk_buff *skb)
{
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	/* This ICMP is in reverse direction to the packet which caused it */
	ct = nf_ct_get(skb, &ctinfo);
	if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL)
		ctinfo = IP_CT_RELATED_REPLY;
	else
		ctinfo = IP_CT_RELATED;

	/* Attach to new skbuff, and increment count */
	nskb->nfct = &ct->ct_general;
	nskb->nfctinfo = ctinfo;
	nf_conntrack_get(nskb->nfct);
}

/* Bring out ya dead! */
static struct nf_conn *
get_next_corpse(struct net *net, int (*iter)(struct nf_conn *i, void *data),
		void *data, unsigned int *bucket)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct hlist_nulls_node *n;
	spinlock_t *lockp;

	for (; *bucket < nf_conntrack_htable_size; (*bucket)++) {
		lockp = &nf_conntrack_locks[*bucket % CONNTRACK_LOCKS];
		local_bh_disable();
		nf_conntrack_lock(lockp);
		if (*bucket < nf_conntrack_htable_size) {
			hlist_nulls_for_each_entry(h, n, &nf_conntrack_hash[*bucket], hnnode) {
				if (NF_CT_DIRECTION(h) != IP_CT_DIR_ORIGINAL)
					continue;
				ct = nf_ct_tuplehash_to_ctrack(h);
				if (net_eq(nf_ct_net(ct), net) &&
				    iter(ct, data))
					goto found;
			}
		}
		spin_unlock(lockp);
		local_bh_enable();
		cond_resched();
	}

	return NULL;
found:
	atomic_inc(&ct->ct_general.use);
	spin_unlock(lockp);
	local_bh_enable();
	return ct;
}

static void
__nf_ct_unconfirmed_destroy(struct net *net)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct nf_conntrack_tuple_hash *h;
		struct hlist_nulls_node *n;
		struct ct_pcpu *pcpu;

		pcpu = per_cpu_ptr(net->ct.pcpu_lists, cpu);

		spin_lock_bh(&pcpu->lock);
		hlist_nulls_for_each_entry(h, n, &pcpu->unconfirmed, hnnode) {
			struct nf_conn *ct;

			ct = nf_ct_tuplehash_to_ctrack(h);

			/* we cannot call iter() on unconfirmed list, the
			 * owning cpu can reallocate ct->ext at any time.
			 */
			set_bit(IPS_DYING_BIT, &ct->status);
		}
		spin_unlock_bh(&pcpu->lock);
		cond_resched();
	}
}

void nf_ct_iterate_cleanup(struct net *net,
			   int (*iter)(struct nf_conn *i, void *data),
			   void *data, u32 portid, int report)
{
	struct nf_conn *ct;
	unsigned int bucket = 0;

	might_sleep();

	if (atomic_read(&net->ct.count) == 0)
		return;

	__nf_ct_unconfirmed_destroy(net);

	synchronize_net();

	while ((ct = get_next_corpse(net, iter, data, &bucket)) != NULL) {
		/* Time to push up daises... */

		nf_ct_delete(ct, portid, report);
		nf_ct_put(ct);
		cond_resched();
	}
}
EXPORT_SYMBOL_GPL(nf_ct_iterate_cleanup);

static int kill_all(struct nf_conn *i, void *data)
{
	return 1;
}

static int sweep_user(struct nf_conn *ct, void *data)
{
	int res = 0;
	const struct nf_conntrack_tuple *tuple;

	rcu_read_lock();

	tuple = nf_ct_tuple(ct, IP_CT_DIR_ORIGINAL);

	if (likely(tuple)) {
		struct nf_conntrack_l4proto *l4proto;

		l4proto = __nf_ct_l4proto_find(tuple->src.l3num,
					       tuple->dst.protonum);

		if (l4proto && l4proto->sweep_user_ok)
			res = l4proto->sweep_user_ok(ct);
	}

	rcu_read_unlock();

	return res;
}

void nf_conntrack_sweep_user(const int count)
{
	struct net *net = &init_net;

	if (atomic_read(&net->ct.count) < count)
		return;

	nf_ct_iterate_cleanup(net, sweep_user, NULL, 0, 0);
}

void nf_ct_free_hashtable(void *hash, unsigned int size)
{
	if (is_vmalloc_addr(hash))
		vfree(hash);
	else
		free_pages((unsigned long)hash,
			   get_order(sizeof(struct hlist_head) * size));
}
EXPORT_SYMBOL_GPL(nf_ct_free_hashtable);

static int untrack_refs(void)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu) {
		struct nf_conn *ct = &per_cpu(nf_conntrack_untracked, cpu);

		cnt += atomic_read(&ct->ct_general.use) - 1;
	}
	return cnt;
}

void nf_conntrack_cleanup_start(void)
{
	conntrack_gc_work.exiting = true;
	RCU_INIT_POINTER(ip_ct_attach, NULL);
}

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
static struct nf_ct_ext_type ntc_extend = {
	.len	= sizeof(struct nf_ct_ext_ntc_label),
	.align	= __alignof__(int),
};
#endif

void nf_conntrack_cleanup_end(void)
{
#ifdef CONFIG_NF_CONNTRACK_CUSTOM
	nf_ct_extend_unregister(&ntc_extend);
#endif

	RCU_INIT_POINTER(nf_ct_destroy, NULL);
	while (untrack_refs() > 0)
		schedule();

	cancel_delayed_work_sync(&conntrack_gc_work.dwork);
	nf_ct_free_hashtable(nf_conntrack_hash, nf_conntrack_htable_size);

	nf_conntrack_proto_fini();
	nf_conntrack_seqadj_fini();
	nf_conntrack_labels_fini();
	nf_conntrack_helper_fini();
	nf_conntrack_timeout_fini();
	nf_conntrack_ecache_fini();
	nf_conntrack_tstamp_fini();
	nf_conntrack_acct_fini();
	nf_conntrack_expect_fini();

	kmem_cache_destroy(nf_conntrack_cachep);
}

/*
 * Mishearing the voices in his head, our hero wonders how he's
 * supposed to kill the mall.
 */
void nf_conntrack_cleanup_net(struct net *net)
{
	LIST_HEAD(single);

	list_add(&net->exit_list, &single);
	nf_conntrack_cleanup_net_list(&single);
}

void nf_conntrack_cleanup_net_list(struct list_head *net_exit_list)
{
	int busy;
	struct net *net;

	/*
	 * This makes sure all current packets have passed through
	 *  netfilter framework.  Roll on, two-stage module
	 *  delete...
	 */
	synchronize_net();
i_see_dead_people:
	busy = 0;
	list_for_each_entry(net, net_exit_list, exit_list) {
		nf_ct_iterate_cleanup(net, kill_all, NULL, 0, 0);
		if (atomic_read(&net->ct.count) != 0)
			busy = 1;
	}
	if (busy) {
		schedule();
		goto i_see_dead_people;
	}

	list_for_each_entry(net, net_exit_list, exit_list) {
		nf_conntrack_proto_pernet_fini(net);
		nf_conntrack_helper_pernet_fini(net);
		nf_conntrack_ecache_pernet_fini(net);
		nf_conntrack_tstamp_pernet_fini(net);
		nf_conntrack_acct_pernet_fini(net);
		nf_conntrack_expect_pernet_fini(net);
		free_percpu(net->ct.stat);
		free_percpu(net->ct.pcpu_lists);
	}
}

void *nf_ct_alloc_hashtable(unsigned int *sizep, int nulls)
{
	struct hlist_nulls_head *hash;
	unsigned int nr_slots, i;
	size_t sz;

	if (*sizep > (UINT_MAX / sizeof(struct hlist_nulls_head)))
		return NULL;

	BUILD_BUG_ON(sizeof(struct hlist_nulls_head) != sizeof(struct hlist_head));
	nr_slots = *sizep = roundup(*sizep, PAGE_SIZE / sizeof(struct hlist_nulls_head));

	if (nr_slots > (UINT_MAX / sizeof(struct hlist_nulls_head)))
		return NULL;

	sz = nr_slots * sizeof(struct hlist_nulls_head);
	hash = (void *)__get_free_pages(GFP_KERNEL | __GFP_NOWARN | __GFP_ZERO,
					get_order(sz));
	if (!hash)
		hash = vzalloc(sz);

	if (hash && nulls)
		for (i = 0; i < nr_slots; i++)
			INIT_HLIST_NULLS_HEAD(&hash[i], i);

	return hash;
}
EXPORT_SYMBOL_GPL(nf_ct_alloc_hashtable);

int nf_conntrack_hash_resize(unsigned int hashsize)
{
	int i, bucket;
	unsigned int old_size;
	struct hlist_nulls_head *hash, *old_hash;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	if (!hashsize)
		return -EINVAL;

	hash = nf_ct_alloc_hashtable(&hashsize, 1);
	if (!hash)
		return -ENOMEM;

	old_size = nf_conntrack_htable_size;
	if (old_size == hashsize) {
		nf_ct_free_hashtable(hash, hashsize);
		return 0;
	}

	local_bh_disable();
	nf_conntrack_all_lock();
	write_seqcount_begin(&nf_conntrack_generation);

	/* Lookups in the old hash might happen in parallel, which means we
	 * might get false negatives during connection lookup. New connections
	 * created because of a false negative won't make it into the hash
	 * though since that required taking the locks.
	 */

	for (i = 0; i < nf_conntrack_htable_size; i++) {
		while (!hlist_nulls_empty(&nf_conntrack_hash[i])) {
			h = hlist_nulls_entry(nf_conntrack_hash[i].first,
					      struct nf_conntrack_tuple_hash, hnnode);
			ct = nf_ct_tuplehash_to_ctrack(h);
			hlist_nulls_del_rcu(&h->hnnode);
			bucket = __hash_conntrack(nf_ct_net(ct),
						  &h->tuple, hashsize);
			hlist_nulls_add_head_rcu(&h->hnnode, &hash[bucket]);
		}
	}
	old_size = nf_conntrack_htable_size;
	old_hash = nf_conntrack_hash;

	nf_conntrack_hash = hash;
	nf_conntrack_htable_size = hashsize;

	write_seqcount_end(&nf_conntrack_generation);
	nf_conntrack_all_unlock();
	local_bh_enable();

	synchronize_net();
	nf_ct_free_hashtable(old_hash, old_size);
	return 0;
}

int nf_conntrack_set_hashsize(const char *val, const struct kernel_param *kp)
{
	unsigned int hashsize;
	int rc;

	if (current->nsproxy->net_ns != &init_net)
		return -EOPNOTSUPP;

	/* On boot, we can set this without any fancy locking. */
	if (!nf_conntrack_hash)
		return param_set_uint(val, kp);

	rc = kstrtouint(val, 0, &hashsize);
	if (rc)
		return rc;

	return nf_conntrack_hash_resize(hashsize);
}
EXPORT_SYMBOL_GPL(nf_conntrack_set_hashsize);

module_param_call(hashsize, nf_conntrack_set_hashsize, param_get_uint,
		  &nf_conntrack_htable_size, 0600);

void nf_ct_untracked_status_or(unsigned long bits)
{
	int cpu;

	for_each_possible_cpu(cpu)
		per_cpu(nf_conntrack_untracked, cpu).status |= bits;
}
EXPORT_SYMBOL_GPL(nf_ct_untracked_status_or);

int nf_conntrack_init_start(void)
{
	int max_factor = 8;
	int ret = -ENOMEM;
	int i, cpu;

	seqcount_init(&nf_conntrack_generation);

	for (i = 0; i < CONNTRACK_LOCKS; i++)
		spin_lock_init(&nf_conntrack_locks[i]);

	spin_lock_init(&nf_conntrack_public_lock);

#ifdef CONFIG_X86_64
	if (!nf_conntrack_htable_size) {
		/* Idea from tcp.c: use 1/16384 of memory.
		 * On i386: 32MB machine has 512 buckets.
		 * >= 1GB machines have 16384 buckets.
		 * >= 4GB machines have 65536 buckets.
		 */
		nf_conntrack_htable_size
			= (((totalram_pages << PAGE_SHIFT) / 16384)
			   / sizeof(struct hlist_head));
		if (totalram_pages > (4 * (1024 * 1024 * 1024 / PAGE_SIZE)))
			nf_conntrack_htable_size = 65536;
		else if (totalram_pages > (1024 * 1024 * 1024 / PAGE_SIZE))
			nf_conntrack_htable_size = 16384;
		if (nf_conntrack_htable_size < 32)
			nf_conntrack_htable_size = 32;

		/* Use a max. factor of four by default to get the same max as
		 * with the old struct list_heads. When a table size is given
		 * we use the old value of 8 to avoid reducing the max.
		 * entries. */
		max_factor = 4;
	}
#else
	/* MIPS/ARM routers with 32..512 MB RAM */
	if (!nf_conntrack_htable_size) {
		unsigned int nfct_htable_size;

		nfct_htable_size = (((totalram_pages << PAGE_SHIFT) / 2048) /
				    sizeof(struct hlist_head));

		nfct_htable_size = roundup(nfct_htable_size, 1024);

		if (nfct_htable_size < 4096)
			nfct_htable_size = 4096;
		else if (nf_conntrack_htable_size > 32768)
			nfct_htable_size = 32768;

		if (nfct_htable_size >= 20000)
			max_factor = 2;
		else if (nfct_htable_size >= 10000)
			max_factor = 3;
		else
			max_factor = 4;

		nf_conntrack_htable_size = nfct_htable_size;
	}
#endif

	nf_conntrack_hash = nf_ct_alloc_hashtable(&nf_conntrack_htable_size, 1);
	if (!nf_conntrack_hash)
		return -ENOMEM;

	nf_conntrack_max = max_factor * nf_conntrack_htable_size;
	nf_conntrack_public_max = 0;

	nf_conntrack_cachep = kmem_cache_create("nf_conntrack",
						sizeof(struct nf_conn), 0,
						SLAB_DESTROY_BY_RCU | SLAB_HWCACHE_ALIGN, NULL);
	if (!nf_conntrack_cachep)
		goto err_cachep;

	printk(KERN_INFO "nf_conntrack version %s (%u buckets, %d max)\n",
	       NF_CONNTRACK_VERSION, nf_conntrack_htable_size,
	       nf_conntrack_max);

	ret = nf_conntrack_expect_init();
	if (ret < 0)
		goto err_expect;

	ret = nf_conntrack_acct_init();
	if (ret < 0)
		goto err_acct;

	ret = nf_conntrack_tstamp_init();
	if (ret < 0)
		goto err_tstamp;

	ret = nf_conntrack_ecache_init();
	if (ret < 0)
		goto err_ecache;

	ret = nf_conntrack_timeout_init();
	if (ret < 0)
		goto err_timeout;

	ret = nf_conntrack_helper_init();
	if (ret < 0)
		goto err_helper;

	ret = nf_conntrack_labels_init();
	if (ret < 0)
		goto err_labels;

	ret = nf_conntrack_seqadj_init();
	if (ret < 0)
		goto err_seqadj;

	ret = nf_conntrack_proto_init();
	if (ret < 0)
		goto err_proto;

	/* Set up fake conntrack: to never be deleted, not in any hashes */
	for_each_possible_cpu(cpu) {
		struct nf_conn *ct = &per_cpu(nf_conntrack_untracked, cpu);
		write_pnet(&ct->ct_net, &init_net);
		atomic_set(&ct->ct_general.use, 1);
	}
	/*  - and look it like as a confirmed connection */
	nf_ct_untracked_status_or(IPS_CONFIRMED | IPS_UNTRACKED);

	conntrack_gc_work_init(&conntrack_gc_work);
	queue_delayed_work(system_power_efficient_wq, &conntrack_gc_work.dwork, HZ);

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
	/* "NTC\0" in hex */
	ret = nf_ct_extend_custom_register(&ntc_extend, 0x4e544300);
	if (ret < 0) {
		pr_err("unable to register a NTC extend\n");
		goto err_custom;
	}

	nf_ct_ext_id_ntc = ntc_extend.id;
#endif

	return 0;

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
err_custom:

	RCU_INIT_POINTER(nf_ct_destroy, NULL);
	while (untrack_refs() > 0)
		schedule();

	cancel_delayed_work_sync(&conntrack_gc_work.dwork);
	nf_ct_free_hashtable(nf_conntrack_hash, nf_conntrack_htable_size);

	nf_conntrack_proto_fini();
#endif
err_proto:
	nf_conntrack_seqadj_fini();
err_seqadj:
	nf_conntrack_labels_fini();
err_labels:
	nf_conntrack_helper_fini();
err_helper:
	nf_conntrack_timeout_fini();
err_timeout:
	nf_conntrack_ecache_fini();
err_ecache:
	nf_conntrack_tstamp_fini();
err_tstamp:
	nf_conntrack_acct_fini();
err_acct:
	nf_conntrack_expect_fini();
err_expect:
	kmem_cache_destroy(nf_conntrack_cachep);
err_cachep:
	nf_ct_free_hashtable(nf_conntrack_hash, nf_conntrack_htable_size);
	return ret;
}

void nf_conntrack_init_end(void)
{
	/* For use by REJECT target */
	RCU_INIT_POINTER(ip_ct_attach, nf_conntrack_attach);
	RCU_INIT_POINTER(nf_ct_destroy, destroy_conntrack);
}

/*
 * We need to use special "null" values, not used in hash table
 */
#define UNCONFIRMED_NULLS_VAL	((1<<30)+0)
#define DYING_NULLS_VAL		((1<<30)+1)
#define TEMPLATE_NULLS_VAL	((1<<30)+2)

int nf_conntrack_init_net(struct net *net)
{
	int ret = -ENOMEM;
	int cpu;

	atomic_set(&net->ct.count, 0);
	atomic_set(&net->ct.public_count, 0);

	net->ct.pcpu_lists = alloc_percpu(struct ct_pcpu);
	if (!net->ct.pcpu_lists)
		goto err_stat;

	for_each_possible_cpu(cpu) {
		struct ct_pcpu *pcpu = per_cpu_ptr(net->ct.pcpu_lists, cpu);

		spin_lock_init(&pcpu->lock);
		INIT_HLIST_NULLS_HEAD(&pcpu->unconfirmed, UNCONFIRMED_NULLS_VAL);
		INIT_HLIST_NULLS_HEAD(&pcpu->dying, DYING_NULLS_VAL);
	}

	net->ct.stat = alloc_percpu(struct ip_conntrack_stat);
	if (!net->ct.stat)
		goto err_pcpu_lists;

	ret = nf_conntrack_expect_pernet_init(net);
	if (ret < 0)
		goto err_expect;
	ret = nf_conntrack_acct_pernet_init(net);
	if (ret < 0)
		goto err_acct;
	ret = nf_conntrack_tstamp_pernet_init(net);
	if (ret < 0)
		goto err_tstamp;
	ret = nf_conntrack_ecache_pernet_init(net);
	if (ret < 0)
		goto err_ecache;
	ret = nf_conntrack_helper_pernet_init(net);
	if (ret < 0)
		goto err_helper;
	ret = nf_conntrack_proto_pernet_init(net);
	if (ret < 0)
		goto err_proto;

	return 0;

err_proto:
	nf_conntrack_helper_pernet_fini(net);
err_helper:
	nf_conntrack_ecache_pernet_fini(net);
err_ecache:
	nf_conntrack_tstamp_pernet_fini(net);
err_tstamp:
	nf_conntrack_acct_pernet_fini(net);
err_acct:
	nf_conntrack_expect_pernet_fini(net);
err_expect:
	free_percpu(net->ct.stat);
err_pcpu_lists:
	free_percpu(net->ct.pcpu_lists);
err_stat:
	return ret;
}
