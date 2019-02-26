#ifndef __FAST_NAT_H_
#define __FAST_NAT_H_

#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>

#define FAST_NAT_BIND_PKT_DIR_BOTH	5
#define FAST_NAT_BIND_PKT_DIR_HALF	200

extern int nf_fastnat_control;

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

#endif /*__FAST_NAT_H_ */
