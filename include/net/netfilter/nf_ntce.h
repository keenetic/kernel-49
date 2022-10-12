#ifndef _LINUX_NF_NTCE_H
#define _LINUX_NF_NTCE_H

struct sk_buff;
struct nf_conn;
struct net;

#ifdef CONFIG_NTCE_MODULE
#include <linux/version.h>
#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/fast_nat.h>
#endif

#include <net/ip.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/xt_ndmmark.h>
#include <net/netfilter/nf_conntrack_acct.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#define NF_NTCE_IPCB_BYPASS			BIT(14)
#define NF_NTCE_IPCB_ENQUEUE			BIT(15)

#define NF_NTCE_IP6CB_BYPASS		1024
#define NF_NTCE_IP6CB_ENQUEUE		2048

#define NF_NTCE_HARD_PACKET_LIMIT		750

struct seq_file;

bool nf_ntce_if_pass(const int ifidx);
void nf_ntce_enq_packet(const struct nf_conn *ct, struct sk_buff *skb);
bool nf_ntce_is_enabled(void);

int nf_ntce_init_proc(struct net *net);
void nf_ntce_fini_proc(struct net *net);
void nf_ntce_ct_show_labels(struct seq_file *s, const struct nf_conn *ct);
int nf_ntce_ctnetlink_dump(struct sk_buff *skb, const struct nf_conn *ct);
size_t nf_ntce_ctnetlink_size(const struct nf_conn *ct);
void nf_ntce_update_sc_ct(struct nf_conn *ct);

static inline void nf_ntce_rst_bypass(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		IPCB(skb)->flags &= ~(NF_NTCE_IPCB_BYPASS);
	else
	if (pf == PF_INET6)
		IP6CB(skb)->flags &= ~(NF_NTCE_IP6CB_BYPASS);
}

static inline void nf_ntce_set_bypass(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		IPCB(skb)->flags |= NF_NTCE_IPCB_BYPASS;
	else
	if (pf == PF_INET6)
		IP6CB(skb)->flags |= NF_NTCE_IP6CB_BYPASS;
}

static inline bool nf_ntce_is_bypass(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		return !!(IPCB(skb)->flags & NF_NTCE_IPCB_BYPASS);

	if (pf == PF_INET6)
		return !!(IP6CB(skb)->flags & NF_NTCE_IP6CB_BYPASS);

	return true;
}

static inline void nf_ntce_rst_enqueue(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		IPCB(skb)->flags &= ~(NF_NTCE_IPCB_ENQUEUE);
	else
	if (pf == PF_INET6)
		IP6CB(skb)->flags &= ~(NF_NTCE_IP6CB_ENQUEUE);
}

static inline void nf_ntce_set_enqueue(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		IPCB(skb)->flags |= NF_NTCE_IPCB_ENQUEUE;
	else
	if (pf == PF_INET6)
		IP6CB(skb)->flags |= NF_NTCE_IP6CB_ENQUEUE;
}

static inline bool nf_ntce_is_enqueue(const u8 pf, struct sk_buff *skb)
{
	if (pf == PF_INET)
		return !!(IPCB(skb)->flags & NF_NTCE_IPCB_ENQUEUE);

	if (pf == PF_INET6)
		return !!(IP6CB(skb)->flags & NF_NTCE_IP6CB_ENQUEUE);

	return false;
}

static inline int nf_ntce_check_limit(const u8 pf,
				      struct sk_buff *skb,
				      const unsigned long long packets)
{
	if (packets > NF_NTCE_HARD_PACKET_LIMIT) {
		nf_ntce_set_bypass(pf, skb);

		return 1;
	}

	return 0;
}

static inline bool nf_ntce_has_helper(struct nf_conn *ct)
{
	struct nf_conn_help *help = nfct_help(ct);

	return help && help->helper;
}

static inline size_t nf_ntce_fastnat_limit(const size_t limit,
					   const bool is_enabled)
{
	return is_enabled ? NF_NTCE_HARD_PACKET_LIMIT : limit;
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
		pr_err_ratelimited("unable to allocate NTCE ct label");
}

static inline int nf_ntce_enqueue__(const u8 pf, struct nf_conn *ct,
				    struct sk_buff *skb,
				    const bool mark)
{
	const struct nf_conn_counter *counters;
	const struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_find_ntce(ct);
	u64 pkts;

	if (unlikely(lbl == NULL)) {
		pr_err_ratelimited("unable to find NTCE label\n");

		return 0;
	}

	if (likely(nf_ct_ext_ntce_fastpath(lbl))) {
		nf_ntce_update_sc_ct(ct);

		return 0;
	}

	counters = (struct nf_conn_counter *)nf_conn_acct_find(ct);
	if (unlikely(counters == NULL)) {
		pr_err_ratelimited("unable to find accoutings\n");

		return 0;
	}

	pkts = atomic64_read(&counters[IP_CT_DIR_ORIGINAL].packets) +
		atomic64_read(&counters[IP_CT_DIR_REPLY].packets);

	if (pkts > NF_NTCE_HARD_PACKET_LIMIT) {
		nf_ntce_update_sc_ct(ct);

		return 0;
	}

#if IS_ENABLED(CONFIG_RA_HW_NAT)
	if (unlikely(!FOE_SKB_IS_KEEPALIVE(skb)))
		FOE_ALG_MARK(skb);
#endif

	if (mark)
		nf_ntce_set_enqueue(pf, skb);
	else
		nf_ntce_enq_packet(ct, skb);

	return 1;
}

static inline int nf_ntce_enqueue_(const u8 pf,
				   struct nf_conn *ct,
				   struct sk_buff *skb,
				   const int ifindex,
				   const bool mark)
{
	/* Fast skip for miltucast / broadcast */
	if (skb->pkt_type != PACKET_HOST)
		return 0;

#if IS_ENABLED(CONFIG_FAST_NAT)
	if (pf == PF_INET)
	{
		if (ct->fast_ext && !nf_ntce_has_helper(ct))
			return 0;

		if (!is_nf_connection_ipv4_tcpudp(ct))
			return 0;
	}
#endif

	if (!nf_ntce_if_pass(ifindex)) 
		return 0;

	return nf_ntce_enqueue__(pf, ct, skb, mark);
}

static inline void nf_ntce_enqueue_out_(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);

	if (ct == NULL)
		return;

	nf_ntce_enqueue_(nf_ct_l3num(ct), ct, skb, skb->dev->ifindex, false);
}

static inline void nf_ntce_enqueue_out(struct sk_buff *skb)
{
	if (skb->sk)
		return;

	if (!xt_ndmmark_is_fwd(skb))
		return;

	nf_ntce_enqueue_out_(skb);
}

static inline int nf_ntce_enqueue_in(const u8 pf, int hooknum,
				     struct nf_conn *ct, struct sk_buff *skb)
{
	if (hooknum != NF_INET_PRE_ROUTING)
		return 0;

	if (nf_ntce_is_bypass(pf, skb))
		return 0;

	nf_ntce_set_bypass(pf, skb);

	return nf_ntce_enqueue_(pf, ct, skb, skb->skb_iif, true);
}

static inline void nf_ntce_enqueue_fwd(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);

	if (ct == NULL)
		return;

	if (likely(!nf_ntce_is_enqueue(nf_ct_l3num(ct), skb)))
		return;

	return nf_ntce_enq_packet(ct, skb);
}

#else

static inline size_t nf_ntce_fastnat_limit(const size_t limit,
					   const bool is_enabled)
{
	return limit;
}

static inline bool nf_ntce_is_enabled(void)
{
	return false;
}

static inline void
nf_ct_ntce_append(int hooknum, struct nf_conn *ct)
{
}

static inline void nf_ntce_rst_bypass(const u8 pf, struct sk_buff *skb)
{
}

static inline void nf_ntce_set_bypass(const u8 pf, struct sk_buff *skb)
{
}

static inline bool nf_ntce_is_bypass(const u8 pf, struct sk_buff *skb)
{
	return true;
}

static inline void nf_ntce_rst_enqueue(const u8 pf, struct sk_buff *skb)
{
}

static inline void nf_ntce_update_sc_ct(struct nf_conn *ct)
{
}

static inline int nf_ntce_check_limit(const u8 pf, 
				      struct sk_buff *skb,
				      const unsigned long long packets)
{
	return 1;
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

static inline int nf_ntce_enqueue_in(int h,
				     struct nf_conn *ct, struct sk_buff *skb)
{
	return 0;
}

static inline void nf_ntce_enqueue_fwd(struct sk_buff *skb)
{
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
