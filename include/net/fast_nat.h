#ifndef __FAST_NAT_H_
#define __FAST_NAT_H_

#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <linux/ntc_shaper_hooks.h>

#define FAST_NAT_BIND_PKT_DIR_BOTH	5
#define FAST_NAT_BIND_PKT_DIR_HALF	200

extern int nf_fastnat_control;

#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
extern int nf_fastroute_control;

extern int (*nf_fastroute_rtcache_in)(u_int8_t pf,
				      struct sk_buff *skb,
				      int ifindex);
#endif

#if IS_ENABLED(CONFIG_PPTP)
extern int nf_fastpath_pptp_control;

extern int (*nf_fastpath_pptp_in)(struct sk_buff *skb,
				  unsigned int dataoff,
				  u16 call_id);
#endif

#if IS_ENABLED(CONFIG_PPPOL2TP)
extern int nf_fastpath_l2tp_control;

extern int (*nf_fastpath_l2tp_in)(struct sk_buff *skb,
				  unsigned int dataoff);
#endif

#ifdef CONFIG_XFRM
extern int nf_fastnat_xfrm_control;
extern int nf_fastpath_esp_control;

int nf_fastpath_esp4_in(struct net *net, struct sk_buff *skb,
			unsigned int dataoff, uint8_t protonum);
#endif
#endif /* CONFIG_FAST_NAT_V2 */

/* ip_output.c */
int ip_finish_output(struct net *net, struct sock *sk, struct sk_buff *skb);

/* nf_fast_nat.c */
int fast_nat_do_bind(struct nf_conn *ct,
		     struct sk_buff *skb,
		     const struct nf_conntrack_l3proto *l3proto,
		     const struct nf_conntrack_l4proto *l4proto,
		     enum ip_conntrack_info ctinfo);

int fast_nat_path(struct net *net, struct sock *sk, struct sk_buff *skb);

/*
 * nf_conntrack helpers
 */
static inline bool
is_nf_connection_ready_nat(struct nf_conn *ct)
{
	if ((ct->status & IPS_NAT_DONE_MASK) == IPS_NAT_DONE_MASK)
		return true;

	return false;
}

static inline bool
is_nf_connection_has_nat(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *t1, *t2;

	t1 = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	t2 = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	return (!(t1->dst.u3.ip == t2->src.u3.ip &&
		  t1->src.u3.ip == t2->dst.u3.ip &&
		  t1->dst.u.all == t2->src.u.all &&
		  t1->src.u.all == t2->dst.u.all));
}

static inline bool
is_nf_connection_has_no_nat(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *t1, *t2;

	t1 = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	t2 = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	return (t1->dst.u3.ip != t1->src.u3.ip &&
		  t1->dst.u3.ip == t2->src.u3.ip &&
		  t1->src.u3.ip == t2->dst.u3.ip &&
		  t1->dst.u.all == t2->src.u.all &&
		  t1->src.u.all == t2->dst.u.all);
}

static inline bool
is_nf_connection_ipv4_tcpudp(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *t1, *t2;

	t1 = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	t2 = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	return
		(t1->src.l3num == PF_INET) && (t2->src.l3num == PF_INET) &&
		(t1->dst.protonum == IPPROTO_UDP || t1->dst.protonum == IPPROTO_TCP) &&
		(t2->dst.protonum == IPPROTO_UDP || t2->dst.protonum == IPPROTO_TCP);
}

static inline bool
is_nf_connection_ipv4_tcpudpgreicmp(struct nf_conn *ct)
{
	struct nf_conntrack_tuple *t1, *t2;

	t1 = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	t2 = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	return
		(t1->src.l3num == PF_INET) && (t2->src.l3num == PF_INET) &&
		(t1->dst.protonum == IPPROTO_UDP ||
		 t1->dst.protonum == IPPROTO_TCP ||
		 t1->dst.protonum == IPPROTO_GRE ||
		 t1->dst.protonum == IPPROTO_ICMP ||
		 t1->dst.protonum == IPPROTO_ICMPV6) &&
		(t2->dst.protonum == IPPROTO_UDP ||
		 t2->dst.protonum == IPPROTO_TCP ||
		 t2->dst.protonum == IPPROTO_GRE ||
		 t2->dst.protonum == IPPROTO_ICMP ||
		 t2->dst.protonum == IPPROTO_ICMPV6);
}

static inline int
fast_nat_ntc_ingress(struct net *net, struct sk_buff *skb, __be32 saddr)
{
	ntc_shaper_hook_fn *ntc_ingress;
	int ret = NF_FAST_NAT;

	ntc_ingress = ntc_shaper_ingress_hook_get();

	if (ntc_ingress) {
		const struct ntc_shaper_fwd_t fwd = {
			.okfn		= fast_nat_path,
			.net		= net,
			.sk		= skb->sk,
			.is_ipv4	= true,
			.is_swnat	= false
		};
		unsigned int ntc_retval = ntc_ingress(skb, &fwd);

		if (ntc_retval == NF_DROP) {
			/* Shaper tell us to drop it */
			ret = NF_DROP;
		} else if (ntc_retval == NF_STOLEN) {
			/* Shaper queued packet and will handle it's destiny */
			ret = NF_STOLEN;
		}
	}

	ntc_shaper_ingress_hook_put();

	return ret;
}

#endif /*__FAST_NAT_H_ */
