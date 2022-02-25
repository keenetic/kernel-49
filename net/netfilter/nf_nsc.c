#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter.h>
#include <net/dsfield.h>
#include <linux/netfilter/xt_DSCP.h>
#include <linux/netfilter/xt_connndmmark.h>

#include <net/netfilter/nf_nsc.h>

const __u8 nf_nsc_sc2dscp[NF_NSC_COUNT] =
{
	0x00, /* 0 -> DF */
	0x2e, /* 1 -> EF */
	0x22, /* 2 -> AF41 */
	0x1a, /* 3 -> AF31 */
	0x12, /* 4 -> AF21 */
	0x0e, /* 5 -> AF13 */
	0x01, /* 6 -> LE */
	0x00
};
EXPORT_SYMBOL(nf_nsc_sc2dscp);

const __u8 nf_nsc_sc2pcp[NF_NSC_COUNT] =
{
	0, /* 0 -> 0 */
	6, /* 1 -> EF */
	5, /* 2 -> AF41 */
	4, /* 3 -> AF31 */
	3, /* 4 -> AF21 */
	2, /* 5 -> AF13 */
	1, /* 6 -> LE */
	0
};
EXPORT_SYMBOL(nf_nsc_sc2pcp);

const __u32 nf_nsc_pcp2qid[NF_NSC_PCP_COUNT] =
{
	1, /* 0 -> 1 */
	0, /* 1 -> 0 */
	2, /* 2 -> 2 */
	3, /* 3 -> 3 */
	4, /* 4 -> 4 */
	5, /* 5 -> 5 */
	6, /* 6 -> 6 */
	7  /* 7 -> 7 */
};
EXPORT_SYMBOL(nf_nsc_pcp2qid);

const __u32 nf_nsc_sc2qid[NF_NSC_COUNT] =
{
	1, /* 0 -> 1 */
	6, /* 1 -> 6 */
	5, /* 2 -> 5 */
	4, /* 3 -> 4 */
	3, /* 4 -> 3 */
	2, /* 5 -> 2 */
	0, /* 6 -> 0 */
	1  /* 7 -> 1 */
};
EXPORT_SYMBOL(nf_nsc_sc2qid);

#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
void nf_nsc_update_ndm_mark(struct sk_buff *skb)
{
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct == NULL || nf_ct_is_untracked(ct))
		return;

	nf_nsc_update_ndm_mark_ct(ct, skb);
}
#endif

int nf_nsc_update_dscp(struct nf_conn *ct, struct sk_buff *skb)
{
	u8 ctdscp = 0;

#ifdef CONFIG_NF_CONNTRACK_MARK
	ctdscp = ndm_sc_to_dscp(nf_nsc_ctmark_to_sc(ct->ndm_mark));

	nf_nsc_sc_to_ndm_mark(skb, nf_nsc_ctmark_to_sc(ct->ndm_mark));
#endif

	if (ctdscp > 0) {
		const u8 skbdscp =
			(ipv4_get_dsfield(ip_hdr(skb)) >> XT_DSCP_SHIFT);

		if (skbdscp == ctdscp)
			return 0;

		if (!skb_make_writable(skb, sizeof(struct iphdr)))
			return -1;

		ipv4_change_dsfield(ip_hdr(skb),
				    (__force __u8)(~XT_DSCP_MASK),
				    (__force __u8)(ctdscp << XT_DSCP_SHIFT));
		skb->priority = rt_tos2priority(ip_hdr(skb)->tos);
	}

	return 0;
}

void nf_nsc_update_sc_ct(struct nf_conn *ct, const u8 sc)
{
#ifdef CONFIG_NF_CONNTRACK_MARK
	ct->ndm_mark = (ct->ndm_mark & ~XT_CONNNDMMARK_CS_MASK) ^
		       (sc << XT_CONNNDMMARK_CS_SHIFT);
#endif
}
