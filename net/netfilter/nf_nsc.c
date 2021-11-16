#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter.h>
#include <net/dsfield.h>
#include <linux/netfilter/xt_DSCP.h>
#include <linux/netfilter/xt_connndmmark.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#include <net/netfilter/nf_nsc.h>

static inline u8 ndm_sc_to_dscp(const u8 sc)
{
	/* Must be in-sync with kernel and ndm
	 */

	static const u8 SC_TO_DSCP_[] =
	{
		0x00, /* 0 -> DF */
		0x2e, /* 1 -> EF */
		0x22, /* 2 -> AF41 */
		0x1a, /* 3 -> AF31 */
		0x12, /* 4 -> AF21 */
		0x0e, /* 5 -> AF13 */
		0x08, /* 6 -> CS1 */
	};

	return SC_TO_DSCP_[sc];
}

static inline int nf_nsc_ctmark_to_sc_(const u8 mark)
{
	return (mark & XT_CONNNDMMARK_CS_MASK) >> XT_CONNNDMMARK_CS_SHIFT;
}

int nf_nsc_ctmark_to_sc(const u8 mark)
{
	return nf_nsc_ctmark_to_sc_(mark);
}

int nf_nsc_update_dscp(struct nf_conn *ct, struct sk_buff *skb)
{
	u8 ctdscp =  0;

#ifdef CONFIG_NF_CONNTRACK_MARK
	ctdscp = ndm_sc_to_dscp(nf_nsc_ctmark_to_sc_(ct->ndm_mark));

#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
	skb->ndm_mark = ct->ndm_mark & ~XT_CONNNDMMARK_CS_MASK;
#endif
#endif

	if (ctdscp > 0) {
		const u8 skbdscp =
			(ipv4_get_dsfield(ip_hdr(skb)) >> XT_DSCP_SHIFT);

		if (skbdscp == ctdscp)
			return 0;

		if (!skb_make_writable(skb, sizeof(struct iphdr)))
			return -1;

#if IS_ENABLED(CONFIG_RA_HW_NAT)
			FOE_ALG_MARK(skb);
#endif

		ipv4_change_dsfield(ip_hdr(skb),
				    (__force __u8)(~XT_DSCP_MASK),
				    (__force __u8)(ctdscp << XT_DSCP_SHIFT));
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
