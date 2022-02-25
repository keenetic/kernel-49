#ifndef _LINUX_NF_NSC_H
#define _LINUX_NF_NSC_H

#include <linux/if_vlan.h>
#include <linux/netfilter/xt_dscp.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter/xt_ndmmark.h>
#include <linux/netfilter/xt_connndmmark.h>

#define NF_NSC_COUNT	8

struct sk_buff;

extern const u8 nf_nsc_sc2dscp[NF_NSC_COUNT];

static inline u8 ndm_sc_to_dscp(const u8 sc)
{
	return nf_nsc_sc2dscp[sc];
}

extern const u8 nf_nsc_sc2pcp[NF_NSC_COUNT];

static inline u8 nf_nsc_sc_to_pcp(const u8 sc)
{
	return nf_nsc_sc2pcp[sc];
}

int nf_nsc_update_dscp(struct nf_conn *ct, struct sk_buff *skb);
void nf_nsc_update_sc_ct(struct nf_conn *ct, const u8 sc);

static inline u8 nf_nsc_ctmark_to_sc(const u8 mark)
{
	return (mark & XT_CONNNDMMARK_CS_MASK) >> XT_CONNNDMMARK_CS_SHIFT;
}

static inline u8 nf_nsc_ctmark_to_dscp(const u8 mark)
{
	return ndm_sc_to_dscp(nf_nsc_ctmark_to_sc(mark));
}

static inline bool nf_nsc_valid_sc(const u8 sc)
{
	return sc >= 0 && sc <= 6;
}

#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
void nf_nsc_update_ndm_mark(struct sk_buff *skb);

static inline void nf_nsc_sc_to_ndm_mark(struct sk_buff *skb, const u8 sc)
{
	skb->ndm_mark_kernel =
		(skb->ndm_mark_kernel & ~XT_NDMMARK_KERNEL_CS_MASK) ^
		(sc << XT_NDMMARK_KERNEL_CS_SHIFT);
}

static inline u8 nf_nsc_sc_from_ndm_mark(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_CS_MASK) >>
		XT_NDMMARK_KERNEL_CS_SHIFT;
}

static inline void nf_nsc_update_ndm_mark_ct(struct nf_conn *ct,
					     struct sk_buff *skb)
{
	nf_nsc_sc_to_ndm_mark(skb, nf_nsc_ctmark_to_sc(ct->ndm_mark));
}

#else
static inline void nf_nsc_update_ndm_mark_ct(struct nf_conn *ct,
					     struct sk_buff *skb)
{
}

static inline void nf_nsc_update_ndm_mark(struct sk_buff *skb)
{
}

static inline void nf_nsc_sc_to_ndm_mark(struct sk_buff *skb, const u8 sc)
{
}

static inline u8 nf_nsc_sc_from_ndm_mark(struct sk_buff *skb)
{
	return 0;
}

#endif /* IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK) */

static inline u16 nf_nsc_pcp_from_skb(struct sk_buff *skb)
{
	return ((u16)nf_nsc_sc_to_pcp(nf_nsc_sc_from_ndm_mark(skb))) <<
		VLAN_PRIO_SHIFT;
}

static inline u8 nf_nsc_dscp_from_skb(struct sk_buff *skb)
{
	return ndm_sc_to_dscp(nf_nsc_sc_from_ndm_mark(skb)) << XT_DSCP_SHIFT;
}

#define NF_NSC_PCP_COUNT	8

extern const u32 nf_nsc_pcp2qid[NF_NSC_PCP_COUNT];

static inline u32 nf_nsc_pcp_to_qid(const u32 vlan_tci)
{
	const u8 pcp =
		((vlan_tci & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT) &
		(NF_NSC_PCP_COUNT - 1);

	return nf_nsc_pcp2qid[pcp];
}

extern const u32 nf_nsc_sc2qid[NF_NSC_COUNT];

static inline u32 nf_nsc_qid_from_skb(struct sk_buff *skb)
{
	const u8 sc = nf_nsc_sc_from_ndm_mark(skb);

	return nf_nsc_sc2qid[sc];
}

#endif /* _LINUX_NF_NSC_H */
