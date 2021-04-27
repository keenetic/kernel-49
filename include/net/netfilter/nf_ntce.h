#ifndef _LINUX_NF_NTCE_H
#define _LINUX_NF_NTCE_H

#ifdef CONFIG_NTCE_MODULE
#include <linux/version.h>
#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/fast_nat.h>
#endif

#include <net/ip.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/xt_ndmmark.h>

#define NF_NTCE_IPCB_BYPASS			BIT(14)

#define NF_NTCE_HARD_PACKET_LIMIT		500

struct sk_buff;
struct nf_conn;
struct net;
struct seq_file;

bool nf_ntce_if_pass(const int ifidx);
int nf_ntce_enq_pkt(struct nf_conn *ct, struct sk_buff *skb);

int nf_ntce_init_proc(struct net *net);
void nf_ntce_fini_proc(struct net *net);
void nf_ntce_ct_show_labels(struct seq_file *s, const struct nf_conn *ct);
int nf_ntce_ctnetlink_dump(struct sk_buff *skb, const struct nf_conn *ct);
size_t nf_ntce_ctnetlink_size(const struct nf_conn *ct);

static inline void nf_ntce_rst_bypass(struct sk_buff *skb)
{
	IPCB(skb)->flags &= ~(NF_NTCE_IPCB_BYPASS);
}

static inline void nf_ntce_set_bypass(struct sk_buff *skb)
{
	IPCB(skb)->flags |= NF_NTCE_IPCB_BYPASS;
}

static inline bool nf_ntce_is_bypass(struct sk_buff *skb)
{
	return !!(IPCB(skb)->flags & NF_NTCE_IPCB_BYPASS);
}

static inline void nf_ntce_check_limit(struct sk_buff *skb,
				       const unsigned long long packets)
{
	if (packets > NF_NTCE_HARD_PACKET_LIMIT)
		nf_ntce_set_bypass(skb);
}

static inline bool nf_ntce_has_helper(struct nf_conn *ct)
{
	struct nf_conn_help *help = nfct_help(ct);

	return help && help->helper;
}

static inline int nf_ntce_enqueue(struct nf_conn *ct,
				  struct sk_buff *skb,
				  const int ifindex)
{
	if (skb->pkt_type != PACKET_HOST)
		return 0;

#if IS_ENABLED(CONFIG_FAST_NAT)
	if (ct->fast_ext && !nf_ntce_has_helper(ct))
		return 0;

	if (!is_nf_connection_ipv4_tcpudp(ct))
		return 0;
#endif

	if (!nf_ntce_if_pass(ifindex))
		return 0;

	return nf_ntce_enq_pkt(ct, skb);
}

static inline void nf_ntce_enqueue_out_(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);

	if (ct == NULL)
		return;

	nf_ntce_enqueue(ct, skb, skb->dev->ifindex);
}

static inline void nf_ntce_enqueue_out(struct sk_buff *skb)
{
	if (!xt_ndmmark_is_fwd(skb))
		return;

	nf_ntce_enqueue_out_(skb);
}

static inline int nf_ntce_enqueue_in(int hooknum,
				     struct nf_conn *ct, struct sk_buff *skb)
{
	if (nf_ntce_is_bypass(skb))
		return 0;

	nf_ntce_set_bypass(skb);

	if (hooknum != NF_INET_PRE_ROUTING)
		return 0;

	return nf_ntce_enqueue(ct, skb, skb->skb_iif);
}

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>

#define NTCE_NF_F_FASTPATH				0x1

/* Must be no more than 128 bits long */
struct nf_ct_ext_ntce_label {
	u32 app;
	u32 group;

	union {
		u32 attributes;
		u8 attr[4];
	} attrs;

	u8 os;
	u8 flags;
};

extern enum nf_ct_ext_id nf_ct_ext_id_ntce;

static inline void *nf_ct_ext_add_ntce_(struct nf_conn *ct)
{
	if (unlikely(nf_ct_ext_id_ntce == 0))
		return NULL;

	return __nf_ct_ext_add_length(ct, nf_ct_ext_id_ntce,
		sizeof(struct nf_ct_ext_ntce_label), GFP_ATOMIC);
}

static inline struct nf_ct_ext_ntce_label *nf_ct_ext_find_ntce(
					  const struct nf_conn *ct)
{
	return (struct nf_ct_ext_ntce_label *)
		__nf_ct_ext_find(ct, nf_ct_ext_id_ntce);
}

static inline struct nf_ct_ext_ntce_label *nf_ct_ext_add_ntce(struct nf_conn *ct)
{
	struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_add_ntce_(ct);

	if (unlikely(lbl == NULL))
		return NULL;

	memset(lbl, 0, sizeof(*lbl));

	return lbl;
}

static inline void nf_ct_ext_ntce_set_fastpath(struct nf_ct_ext_ntce_label *lbl)
{
	lbl->flags |= NTCE_NF_F_FASTPATH;
}

static inline bool
nf_ct_ext_ntce_fastpath(const struct nf_ct_ext_ntce_label *lbl)
{
	return !!(lbl->flags & NTCE_NF_F_FASTPATH);
}

static inline void
nf_ct_ntce_append(int hooknum, struct nf_conn *ct)
{
	struct nf_ct_ext_ntce_label *ntce_ct_lbl;

	if (unlikely(nf_ct_ext_id_ntce == 0 || hooknum != NF_INET_PRE_ROUTING))
		return;

	ntce_ct_lbl = nf_ct_ext_find_ntce(ct);

	if (ntce_ct_lbl == NULL)
		ntce_ct_lbl = nf_ct_ext_add_ntce(ct);

	if (unlikely(ntce_ct_lbl == NULL))
		pr_err_ratelimited("unable to allocate ntce ct label");
}

#else

static inline void
nf_ct_ntce_append(int hooknum, struct nf_conn *ct)
{
}

static inline void nf_ntce_rst_bypass(struct sk_buff *skb)
{
}

static inline void nf_ntce_set_bypass(struct sk_buff *skb)
{
}

static inline bool nf_ntce_is_bypass(struct sk_buff *skb)
{
	return true;
}

static inline void nf_ntce_check_limit(struct sk_buff *skb,
				       const unsigned long long packets)
{
}

static inline bool nf_ntce_if_pass(const int idx)
{
	return 0;
}

static inline int nf_ntce_enq_pkt(struct nf_conn *ct, struct sk_buff *skb)
{
	return 0;
}

static inline void nf_ntce_enqueue_out(struct sk_buff *skb)
{
}

static inline int nf_ntce_enqueue_in(int h, struct nf_conn *ct, struct sk_buff *skb)
{
	return 0;
}

static inline int nf_ntce_init_proc(struct net *net)
{
	return 0;
}

static inline void nf_ntce_fini_proc(struct net *net)
{
}

static inline void
nf_ntce_ct_show_labels(struct seq_file *f, const struct nf_conn *ct)
{
}

static inline int
nf_ntce_ctnetlink_dump(struct sk_buff *skb, const struct nf_conn *ct)
{
	return 0;
}

static inline size_t nf_ntce_ctnetlink_size(const struct nf_conn *ct)
{
	return 0;
}

#endif

#endif
