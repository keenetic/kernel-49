#ifndef _XT_NDMMARK_H
#define _XT_NDMMARK_H

#include <linux/types.h>

/* Must be in-sync with ndm/Netfilter/Typedefs.h */

enum xt_ndmmark_list {
	XT_NDMMARK_EMPTY            = 0x00,
	XT_NDMMARK_IPSEC_INPUT      = 0x01,
	XT_NDMMARK_IPSEC_OUTPUT     = 0x02,
	XT_NDMMARK_IPSEC_MASK       = 0x03,
	XT_NDMMARK_DISCOVERY_DROP   = 0x03,
	XT_NDMMARK_DNAT             = 0x80, // with mask
	XT_NDMMARK_PPTP_VPN         = 0x40, // with mask
	XT_NDMMARK_SSTP_VPN         = 0x20, // with mask
	XT_NDMMARK_L2TP_IPSEC_VPN   = 0x10, // with mask
	XT_NDMMARK_CHILLI           = 0x08, // with mask
	XT_NDMMARK_FWD              = 0x04, // with mask
	XT_NDMMARK_KEEP_CT          = 0x100, // with mask
	XT_NDMMARK_ALL              = 0xff  // all-wide mask
};

enum xt_ndmmark_kernel_list {
	XT_NDMMARK_KERNEL_NTC       = 0x01,
	XT_NDMMARK_KERNEL_CS_MASK   = 0x1C,
	XT_NDMMARK_KERNEL_CS_SHIFT  = 0x02,
	XT_NDMMARK_KERNEL_WAN       = 0x20,
	XT_NDMMARK_KERNEL_USBMAC    = 0x40,
	XT_NDMMARK_KERNEL_SET_CE    = 0x80,
	XT_NDMMARK_KERNEL_HAS_IF    = 0x100,
	XT_NDMMARK_KERNEL_HAS_MAC   = 0x200,
};

struct xt_ndmmark_tginfo {
	__u32 mark, mask;
};

struct xt_ndmmark_mtinfo {
	__u32 mark, mask;
	__u32 invert;
};

#ifdef CONFIG_NETFILTER_XT_NDMMARK
static inline bool xt_ndmmark_is_dnat(struct sk_buff *skb)
{
	return (skb->ndm_mark & XT_NDMMARK_DNAT) == XT_NDMMARK_DNAT;
}
static inline bool xt_ndmmark_is_fwd(struct sk_buff *skb)
{
	return (skb->ndm_mark & XT_NDMMARK_FWD) == XT_NDMMARK_FWD;
}
static inline void xt_ndmmark_set_fwd(struct sk_buff *skb)
{
	skb->ndm_mark |= XT_NDMMARK_FWD;
}
static inline bool xt_ndmmark_is_ipsec(struct sk_buff *skb)
{
	return (skb->ndm_mark & XT_NDMMARK_IPSEC_MASK);
}
static inline void xt_ndmmark_set_keep_ct(struct sk_buff *skb)
{
	skb->ndm_mark |= XT_NDMMARK_KEEP_CT;
}
static inline bool xt_ndmmark_is_keep_ct(struct sk_buff *skb)
{
	return (skb->ndm_mark & XT_NDMMARK_KEEP_CT) == XT_NDMMARK_KEEP_CT;
}
static inline void xt_ndmmark_rst_keep_ct(struct sk_buff *skb)
{
	skb->ndm_mark &= ~XT_NDMMARK_KEEP_CT;
}
#else
static inline bool xt_ndmmark_is_dnat(struct sk_buff *skb)
{
	return false;
}
static inline bool xt_ndmmark_is_fwd(struct sk_buff *skb)
{
	return false;
}
static inline void xt_ndmmark_set_fwd(struct sk_buff *skb)
{
}
static inline bool xt_ndmmark_is_ipsec(struct sk_buff *skb)
{
	return false;
}
#endif

static inline void xt_ndmmark_kernel_set_wan(struct sk_buff *skb)
{
	skb->ndm_mark_kernel |= XT_NDMMARK_KERNEL_WAN;
}

static inline bool xt_ndmmark_kernel_is_wan(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_WAN) ==
		XT_NDMMARK_KERNEL_WAN;
}

static inline void xt_ndmmark_kernel_set_usbmac(struct sk_buff *skb)
{
	skb->ndm_mark_kernel |= XT_NDMMARK_KERNEL_USBMAC;
}

static inline void xt_ndmmark_kernel_rst_usbmac(struct sk_buff *skb)
{
	skb->ndm_mark_kernel &= ~XT_NDMMARK_KERNEL_USBMAC;
}

static inline bool xt_ndmmark_is_usbmac(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_USBMAC) ==
		XT_NDMMARK_KERNEL_USBMAC;
}

static inline void xt_ndmmark_kernel_set_ce(struct sk_buff *skb)
{
	skb->ndm_mark_kernel |= XT_NDMMARK_KERNEL_SET_CE;
}

static inline void xt_ndmmark_kernel_rst_ce(struct sk_buff *skb)
{
	skb->ndm_mark_kernel &= ~XT_NDMMARK_KERNEL_SET_CE;
}

static inline bool xt_ndmmark_is_set_ce(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_SET_CE) ==
		XT_NDMMARK_KERNEL_SET_CE;
}

static inline void xt_ndmmark_kernel_set_has_if(struct sk_buff *skb)
{
	skb->ndm_mark_kernel |= XT_NDMMARK_KERNEL_HAS_IF;
}

static inline bool xt_ndmmark_has_if(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_HAS_IF) ==
		XT_NDMMARK_KERNEL_HAS_IF;
}

static inline void xt_ndmmark_kernel_set_has_mac(struct sk_buff *skb)
{
	skb->ndm_mark_kernel |= XT_NDMMARK_KERNEL_HAS_MAC;
}

static inline bool xt_ndmmark_has_mac(struct sk_buff *skb)
{
	return (skb->ndm_mark_kernel & XT_NDMMARK_KERNEL_HAS_MAC) ==
		XT_NDMMARK_KERNEL_HAS_MAC;
}

#endif /* _XT_NDMMARK_H */
