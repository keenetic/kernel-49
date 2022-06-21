/* route cache for netfilter.
 *
 * (C) 2014 Red Hat GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/export.h>
#include <linux/module.h>

#include <net/dst.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_rtcache.h>

#if IS_ENABLED(CONFIG_NF_CONNTRACK_IPV6)
#include <net/ip6_fib.h>
#endif

#ifdef CONFIG_FAST_NAT_V2
#include <net/fast_nat.h>
#include <net/fast_vpn.h>
#endif

static void __nf_conn_rtcache_destroy(struct nf_conn_rtcache *rtc,
				      enum ip_conntrack_dir dir)
{
	struct dst_entry *dst = rtc->cached_dst[dir].dst;

	dst_release(dst);
}

static void nf_conn_rtcache_destroy(struct nf_conn *ct)
{
	struct nf_conn_rtcache *rtc = nf_ct_rtcache_find(ct);

	if (!rtc)
		return;

	__nf_conn_rtcache_destroy(rtc, IP_CT_DIR_ORIGINAL);
	__nf_conn_rtcache_destroy(rtc, IP_CT_DIR_REPLY);
}

static void nf_ct_rtcache_ext_add(struct nf_conn *ct)
{
	struct nf_conn_rtcache *rtc;

	rtc = nf_ct_ext_add(ct, NF_CT_EXT_RTCACHE, GFP_ATOMIC);
	if (rtc) {
		rtc->cached_dst[IP_CT_DIR_ORIGINAL].iif = -1;
		rtc->cached_dst[IP_CT_DIR_ORIGINAL].dst = NULL;
		rtc->cached_dst[IP_CT_DIR_REPLY].iif = -1;
		rtc->cached_dst[IP_CT_DIR_REPLY].dst = NULL;
	}
}

static struct nf_conn_rtcache *nf_ct_rtcache_find_usable(const struct nf_conn *ct)
{
	if (nf_ct_is_untracked(ct))
		return NULL;
	return nf_ct_rtcache_find(ct);
}

static struct dst_entry *
nf_conn_rtcache_dst_get(const struct nf_conn_rtcache *rtc,
			enum ip_conntrack_dir dir)
{
	return rtc->cached_dst[dir].dst;
}

static u32 nf_rtcache_get_cookie(u_int8_t pf, const struct dst_entry *dst)
{
#if IS_ENABLED(CONFIG_NF_CONNTRACK_IPV6)
	if (pf == NFPROTO_IPV6) {
		return rt6_get_cookie((const struct rt6_info *)dst);
	}
#endif
	return 0;
}

static void nf_conn_rtcache_dst_set(int pf,
				    struct nf_conn_rtcache *rtc,
				    struct dst_entry *dst,
				    enum ip_conntrack_dir dir, int iif)
{
	if (dst->dev->ifindex == iif)
		return;

	if (rtc->cached_dst[dir].iif != iif)
		rtc->cached_dst[dir].iif = iif;

	if (rtc->cached_dst[dir].dst != dst) {
		struct dst_entry *old;

		dst_hold(dst);

		old = xchg(&rtc->cached_dst[dir].dst, dst);
		dst_release(old);

#if IS_ENABLED(CONFIG_NF_CONNTRACK_IPV6)
		if (pf == NFPROTO_IPV6)
			rtc->cached_dst[dir].cookie =
				nf_rtcache_get_cookie(pf, dst);
#endif
	}
}

static void nf_conn_rtcache_dst_obsolete(struct nf_conn_rtcache *rtc,
					 enum ip_conntrack_dir dir)
{
	struct dst_entry *old;

	pr_debug("Invalidate iif %d for dir %d on cache %p\n",
		 rtc->cached_dst[dir].iif, dir, rtc);

	old = xchg(&rtc->cached_dst[dir].dst, NULL);
	dst_release(old);
	rtc->cached_dst[dir].iif = -1;
}

void nf_conn_rtcache_dump(struct seq_file *s, const struct nf_conn *ct)
{
	struct nf_conn_rtcache *rtc;
	int orig_if, repl_if;
	struct dst_entry *orig_dst = NULL, *repl_dst = NULL;

	rtc = nf_ct_rtcache_find_usable(ct);
	if (!rtc)
		return;

	orig_if = nf_conn_rtcache_iif_get(rtc, IP_CT_DIR_ORIGINAL);
	repl_if = nf_conn_rtcache_iif_get(rtc, IP_CT_DIR_REPLY);

	orig_dst = nf_conn_rtcache_dst_get(rtc, IP_CT_DIR_ORIGINAL);
	repl_dst = nf_conn_rtcache_dst_get(rtc, IP_CT_DIR_REPLY);
	if (!orig_dst && !repl_dst)
		return;

	if (orig_dst)
		orig_dst = dst_check(orig_dst,
				     nf_rtcache_get_cookie(nf_ct_l3num(ct),
							   orig_dst));
	if (repl_dst)
		repl_dst = dst_check(repl_dst,
				     nf_rtcache_get_cookie(nf_ct_l3num(ct),
							   repl_dst));

	if (!orig_dst && !repl_dst)
		return;
	
	seq_printf(s, "[RTCACHE ");

	if (orig_dst)
		seq_printf(s, "o%d", orig_if);

	if (repl_dst)
		seq_printf(s, "%sr%d",
			   (orig_dst != NULL ? "/" : ""),
			   repl_if);

	seq_printf(s, "] ");
}

#ifdef CONFIG_FAST_NAT_V2
#ifdef CONFIG_NF_CONNTRACK_CUSTOM
#ifdef CONFIG_NDM_SECURITY_LEVEL
static inline void
swnat_rtcache_prebind__(struct sk_buff *skb,
			struct nf_conn *ct,
			enum ip_conntrack_info ctinfo)
{
	typeof(prebind_from_rtcache) swnat_prebind;

	rcu_read_lock();

	swnat_prebind = rcu_dereference(prebind_from_rtcache);
	if (swnat_prebind) {
		swnat_prebind(skb, ct, ctinfo);
		SWNAT_RTCACHE_SET_MARK(skb);
	}

	rcu_read_unlock();
}

static inline void
swnat_rtcache_prebind_(struct sk_buff *skb,
		       struct nf_conn *ct,
		       enum ip_conntrack_info ctinfo,
		       struct nf_conn_rtcache *rtc,
		       int iif)
{
	struct nf_ct_ext_ntc_label *lbl = nf_ct_ext_find_ntc(ct);
	int orig_if, repl_if;
	struct dst_entry *orig_dst = NULL, *repl_dst = NULL;

	if (unlikely(lbl == NULL || !nf_ct_ext_ntc_filled(lbl)))
		return;

	orig_if = nf_conn_rtcache_iif_get(rtc, IP_CT_DIR_ORIGINAL);
	repl_if = nf_conn_rtcache_iif_get(rtc, IP_CT_DIR_REPLY);

	orig_dst = nf_conn_rtcache_dst_get(rtc, IP_CT_DIR_ORIGINAL);
	repl_dst = nf_conn_rtcache_dst_get(rtc, IP_CT_DIR_REPLY);

	if (unlikely(!orig_dst || !repl_dst))
		return;

	orig_dst = dst_check(orig_dst,
			     nf_rtcache_get_cookie(nf_ct_l3num(ct),
						   orig_dst));
	repl_dst = dst_check(repl_dst,
			     nf_rtcache_get_cookie(nf_ct_l3num(ct),
						   repl_dst));

	if (unlikely(!orig_dst || !repl_dst))
		return;

	if (unlikely(orig_dst->dev->ifindex != repl_if ||
		     repl_dst->dev->ifindex != orig_if ||
		     orig_if == repl_if))
		return;

	if (!((orig_if == lbl->lan_iface && repl_if == lbl->wan_iface) ||
	      (orig_if == lbl->wan_iface && repl_if == lbl->lan_iface)))
		return;

	if (iif != lbl->lan_iface)
		return;

	swnat_rtcache_prebind__(skb, ct, ctinfo);
}

static inline void
swnat_rtcache_prebind(struct sk_buff *skb,
		      struct nf_conn *ct,
		      enum ip_conntrack_info ctinfo,
		      struct nf_conn_rtcache *rtc,
		      int iif)
{
	struct ipv6hdr *ip6 = ipv6_hdr(skb);

	if (ip6->nexthdr != IPPROTO_TCP && ip6->nexthdr != IPPROTO_UDP)
		return;

	if (ip6->nexthdr == IPPROTO_TCP &&
	    (ctinfo != IP_CT_ESTABLISHED &&
	     ctinfo != IP_CT_ESTABLISHED_REPLY))
		return;

	if (ip6->nexthdr == IPPROTO_UDP &&
	    !test_bit(IPS_ASSURED_BIT, &ct->status))
		return;

	if (unlikely(skb_sec_path(skb)))
		return;

	swnat_rtcache_prebind_(skb, ct, ctinfo, rtc, iif);
}

#else
#err NDM Security level is required
#endif
#else
#err Conntrack extensions are required
#endif
#endif

static int do_rtcache_in(u_int8_t pf, struct sk_buff *skb, int ifindex)
{
	struct nf_conn_rtcache *rtc;
	enum ip_conntrack_info ctinfo;
	enum ip_conntrack_dir dir;
	struct dst_entry *dst;
	struct nf_conn *ct;
	int iif;
	u32 cookie;

	if (skb->sk || skb_dst(skb))
		return 0;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return 0;

	rtc = nf_ct_rtcache_find_usable(ct);
	if (!rtc)
		return 0;

	/* if iif changes, don't use cache and let ip stack
	 * do route lookup.
	 *
	 * If rp_filter is enabled it might toss skb, so
	 * we don't want to avoid these checks.
	 */
	dir = CTINFO2DIR(ctinfo);
	iif = nf_conn_rtcache_iif_get(rtc, dir);
	if (ifindex != iif) {
		pr_debug("ct %p, iif %d, cached iif %d, skip cached entry\n",
			 ct, iif, ifindex);
		return 0;
	}
	dst = nf_conn_rtcache_dst_get(rtc, dir);
	if (dst == NULL)
		return 0;

	cookie = nf_rtcache_get_cookie(pf, dst);

	dst = dst_check(dst, cookie);
	pr_debug("obtained dst %p for skb %p, cookie %d\n", dst, skb, cookie);
	if (likely(dst)) {
		skb_dst_set_noref(skb, dst);

#ifdef CONFIG_FAST_NAT_V2
		if (pf == PF_INET6)
			swnat_rtcache_prebind(skb, ct, ctinfo, rtc, ifindex);
#endif

		return 1;
	} else
		nf_conn_rtcache_dst_obsolete(rtc, dir);

	return 0;
}

static inline unsigned int nf_rtcache_in(u_int8_t pf,
					 struct sk_buff *skb,
					 const struct nf_hook_state *state)
{
	do_rtcache_in(pf, skb, state->in->ifindex);

	return NF_ACCEPT;
}

static unsigned int nf_rtcache_forward(u_int8_t pf,
				       struct sk_buff *skb,
				       const struct nf_hook_state *state)
{
	struct nf_conn_rtcache *rtc;
	enum ip_conntrack_info ctinfo;
	enum ip_conntrack_dir dir;
	struct nf_conn *ct;
	struct dst_entry *dst = skb_dst(skb);
	int iif;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return NF_ACCEPT;

	if (dst && dst_xfrm(dst))
		return NF_ACCEPT;

	if (!nf_ct_is_confirmed(ct)) {
		if (WARN_ON(nf_ct_rtcache_find(ct)))
			return NF_ACCEPT;
		nf_ct_rtcache_ext_add(ct);
		return NF_ACCEPT;
	}

	rtc = nf_ct_rtcache_find_usable(ct);
	if (!rtc)
		return NF_ACCEPT;

	dir = CTINFO2DIR(ctinfo);
	iif = nf_conn_rtcache_iif_get(rtc, dir);
	pr_debug("ct %p, skb %p, dir %d, iif %d, cached iif %d\n",
		 ct, skb, dir, iif, state->in->ifindex);
	if (likely(state->in->ifindex == iif))
		return NF_ACCEPT;

	nf_conn_rtcache_dst_set(pf, rtc, skb_dst(skb), dir, state->in->ifindex);
	return NF_ACCEPT;
}

static unsigned int nf_rtcache_in4(void *priv,
				  struct sk_buff *skb,
				  const struct nf_hook_state *state)
{
	return nf_rtcache_in(NFPROTO_IPV4, skb, state);
}

static unsigned int nf_rtcache_forward4(void *priv,
				       struct sk_buff *skb,
				       const struct nf_hook_state *state)
{
	return nf_rtcache_forward(NFPROTO_IPV4, skb, state);
}

#if IS_ENABLED(CONFIG_NF_CONNTRACK_IPV6)
static unsigned int nf_rtcache_in6(void *priv,
				  struct sk_buff *skb,
				  const struct nf_hook_state *state)
{
	return nf_rtcache_in(NFPROTO_IPV6, skb, state);
}

static unsigned int nf_rtcache_forward6(void *priv,
				       struct sk_buff *skb,
				       const struct nf_hook_state *state)
{
	return nf_rtcache_forward(NFPROTO_IPV6, skb, state);
}
#endif

static int nf_rtcache_dst_remove(struct nf_conn *ct, void *data)
{
	struct nf_conn_rtcache *rtc = nf_ct_rtcache_find(ct);
	struct net_device *dev = data;

	if (!rtc)
		return 0;

	if (dev->ifindex == rtc->cached_dst[IP_CT_DIR_ORIGINAL].iif ||
	    dev->ifindex == rtc->cached_dst[IP_CT_DIR_REPLY].iif) {
		nf_conn_rtcache_dst_obsolete(rtc, IP_CT_DIR_ORIGINAL);
		nf_conn_rtcache_dst_obsolete(rtc, IP_CT_DIR_REPLY);
	}

	return 0;
}

static int nf_rtcache_netdev_event(struct notifier_block *this,
				   unsigned long event, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	struct net *net = dev_net(dev);

	if (event == NETDEV_DOWN)
		nf_ct_iterate_cleanup(net, nf_rtcache_dst_remove, dev, 0, 0);

	return NOTIFY_DONE;
}

static struct notifier_block nf_rtcache_notifier = {
	.notifier_call = nf_rtcache_netdev_event,
};

static struct nf_hook_ops rtcache_ops[] = {
	{
		.hook		= nf_rtcache_in4,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
	{
		.hook           = nf_rtcache_forward4,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_FORWARD,
		.priority       = NF_IP_PRI_LAST,
	},
#if IS_ENABLED(CONFIG_NF_CONNTRACK_IPV6)
	{
		.hook		= nf_rtcache_in6,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
	{
		.hook           = nf_rtcache_forward6,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_FORWARD,
		.priority       = NF_IP_PRI_LAST,
	},
#endif
};

static struct nf_ct_ext_type rtcache_extend __read_mostly = {
	.len	= sizeof(struct nf_conn_rtcache),
	.align	= __alignof__(struct nf_conn_rtcache),
	.id	= NF_CT_EXT_RTCACHE,
	.destroy = nf_conn_rtcache_destroy,
};

static int __init nf_conntrack_rtcache_init(void)
{
	int ret = nf_ct_extend_register(&rtcache_extend);

	if (ret < 0) {
		pr_err("nf_conntrack_rtcache: Unable to register extension\n");
		return ret;
	}

	ret = nf_register_hooks(rtcache_ops, ARRAY_SIZE(rtcache_ops));
	if (ret < 0) {
		nf_ct_extend_unregister(&rtcache_extend);
		return ret;
	}

	ret = register_netdevice_notifier(&nf_rtcache_notifier);
	if (ret) {
		nf_unregister_hooks(rtcache_ops, ARRAY_SIZE(rtcache_ops));
		nf_ct_extend_unregister(&rtcache_extend);
		return ret;
	}

#ifdef CONFIG_FAST_NAT_V2
	rcu_assign_pointer(nf_fastroute_rtcache_in, do_rtcache_in);
#endif

	return ret;
}

static int nf_rtcache_ext_remove(struct nf_conn *ct, void *data)
{
	struct nf_conn_rtcache *rtc = nf_ct_rtcache_find(ct);

	return rtc != NULL;
}

static bool __exit nf_conntrack_rtcache_wait_for_dying(struct net *net)
{
	bool wait = false;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct nf_conntrack_tuple_hash *h;
		struct hlist_nulls_node *n;
		struct nf_conn *ct;
		struct ct_pcpu *pcpu = per_cpu_ptr(net->ct.pcpu_lists, cpu);

		rcu_read_lock();
		spin_lock_bh(&pcpu->lock);

		hlist_nulls_for_each_entry(h, n, &pcpu->dying, hnnode) {
			ct = nf_ct_tuplehash_to_ctrack(h);
			if (nf_ct_rtcache_find(ct) != NULL) {
				wait = true;
				break;
			}
		}
		spin_unlock_bh(&pcpu->lock);
		rcu_read_unlock();
	}

	return wait;
}

static void __exit nf_conntrack_rtcache_fini(void)
{
	struct net *net;
	int count = 0;

#ifdef CONFIG_FAST_NAT_V2
	RCU_INIT_POINTER(nf_fastroute_rtcache_in, NULL);
#endif

	/* remove hooks so no new connections get rtcache extension */
	nf_unregister_hooks(rtcache_ops, ARRAY_SIZE(rtcache_ops));

	synchronize_net();

	unregister_netdevice_notifier(&nf_rtcache_notifier);

	rtnl_lock();

	/* zap all conntracks with rtcache extension */
	for_each_net(net)
		nf_ct_iterate_cleanup(net, nf_rtcache_ext_remove, NULL, 0, 0);

	for_each_net(net) {
		/* .. and make sure they're gone from dying list, too */
		while (nf_conntrack_rtcache_wait_for_dying(net)) {
			msleep(200);
			WARN_ONCE(++count > 25, "Waiting for all rtcache conntracks to go away\n");
		}
	}

	rtnl_unlock();
	synchronize_net();
	nf_ct_extend_unregister(&rtcache_extend);
}
module_init(nf_conntrack_rtcache_init);
module_exit(nf_conntrack_rtcache_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Westphal <fw@strlen.de>");
MODULE_DESCRIPTION("Conntrack route cache extension");
