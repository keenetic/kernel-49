#ifndef __FAST_VPN_H_
#define __FAST_VPN_H_

#include <linux/list.h>

struct iphdr;
struct ipv6hdr;
struct nf_conn;
enum ip_conntrack_info;

#define FAST_VPN_ACTION_SETUP		1
#define FAST_VPN_ACTION_RELEASE		0

#define FAST_VPN_RES_OK			1
#define FAST_VPN_RES_SKIPPED		0

/* SWNAT section */

#define SWNAT_CB_OFFSET			47

#define SWNAT_FNAT_MARK			0x01
#define SWNAT_PPP_MARK			0x02
#define SWNAT_NAT46_MARK		0x03
#define SWNAT_RTCACHE_MARK		0x04
#define SWNAT_ENCAP46_MARK		0x05

#define SWNAT_RESET_MARKS(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = 0; \
} while (0);

/* FNAT mark */

#define SWNAT_FNAT_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_FNAT_MARK; \
} while (0);

#define SWNAT_FNAT_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_FNAT_MARK)

/* End of FNAT mark */

/* PPP mark */

#define SWNAT_PPP_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_PPP_MARK; \
} while (0);

#define SWNAT_PPP_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_PPP_MARK)

/* End of PPP mark */

/* NAT46 mark */

#define SWNAT_NAT46_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_NAT46_MARK; \
} while (0);

#define SWNAT_NAT46_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_NAT46_MARK)

/* End of NAT46 mark */

/* RTCACHE mark */

#define SWNAT_RTCACHE_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_RTCACHE_MARK; \
} while (0);

#define SWNAT_RTCACHE_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_RTCACHE_MARK)

/* End of RTCACHE mark */

/* ENCAP46 mark */

#define SWNAT_ENCAP46_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_ENCAP46_MARK; \
} while (0);

#define SWNAT_ENCAP46_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_ENCAP46_MARK)

/* End of RTCACHE mark */

/* KA mark */

#define SWNAT_KA_SET_MARK(skb_) \
do { \
	(skb_)->swnat_ka_mark = 1; \
} while (0);

#define SWNAT_KA_CHECK_MARK(skb_) \
	((skb_)->swnat_ka_mark != 0)

/* End of KA mark */

/* prebind hooks */
extern int (*go_swnat)(struct sk_buff *skb);

extern void (*prebind_from_eth)(struct sk_buff *skb);

extern void (*prebind_from_fastnat)(struct sk_buff *skb,
				    __be32 orig_saddr,
				    __be16 orig_sport,
				    struct nf_conn *ct,
				    enum ip_conntrack_info ctinfo);

extern void (*prebind_from_l2tptx)(struct sk_buff *skb,
				   struct sock *sock,
				   __be16 l2w_tid,
				   __be16 l2w_sid,
				   __be16 w2l_tid,
				   __be16 w2l_sid,
				   __be32 saddr,
				   __be32 daddr,
				   __be16 sport,
				   __be16 dport);

extern void (*prebind_from_pptptx)(struct sk_buff *skb,
				   struct iphdr *iph_int,
				   struct sock *sock,
				   __be32 saddr,
				   __be32 daddr);

extern void (*prebind_from_pppoetx)(struct sk_buff *skb,
				    struct sock *sock,
				    __be16 sid);

extern void (*prebind_from_nat46tx)(struct sk_buff *skb,
				    struct iphdr *ip4,
				    struct ipv6hdr *ip6);

extern void (*prebind_from_encap46tx)(struct sk_buff *skb,
				      const struct iphdr *ip4,
				      const struct ipv6hdr *ip6);

extern void (*prebind_from_rtcache)(struct sk_buff *skb,
				    struct nf_conn *ct,
				    enum ip_conntrack_info ctinfo);

extern void (*prebind_from_ct_mark)(struct nf_conn *ct);
/* End of prebind hooks */

/* List of new MC streams */

struct new_mc_streams {
	u32 group_addr;
	struct net_device * out_dev;
	u32 handled;

	struct list_head list;
};

static inline bool __attribute__((always_inline))
swnat_rx(struct sk_buff *skb)
{
	typeof(go_swnat) swnat;

	rcu_read_lock();
	swnat = rcu_dereference(go_swnat);
	if (likely(swnat != NULL)) {
		const bool res = !!swnat(skb);
		rcu_read_unlock();

		return res;
	}

	rcu_read_unlock();

	return false;
}

static inline void __attribute__((always_inline))
swnat_tx(struct sk_buff *skb)
{
	typeof(prebind_from_eth) swnat_prebind;

	if (likely(!SWNAT_PPP_CHECK_MARK(skb) &&
		   !SWNAT_FNAT_CHECK_MARK(skb) &&
		   !SWNAT_NAT46_CHECK_MARK(skb) &&
		   !SWNAT_ENCAP46_CHECK_MARK(skb) &&
		   !SWNAT_RTCACHE_CHECK_MARK(skb)))
		return;

	rcu_read_lock();

	swnat_prebind = rcu_dereference(prebind_from_eth);
	if (likely(swnat_prebind != NULL))
		swnat_prebind(skb);

	rcu_read_unlock();
}

static inline bool __attribute__((always_inline))
swnat_tx_consume(struct sk_buff *skb)
{
#if defined(SWNAT_KA_CHECK_MARK)
	if (unlikely(SWNAT_KA_CHECK_MARK(skb))) {
		consume_skb(skb);

		return true;
	}
#endif

	swnat_tx(skb);

	return false;
}

#endif /*__FAST_VPN_H_ */
