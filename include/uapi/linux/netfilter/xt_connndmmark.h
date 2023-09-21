#ifndef _XT_CONNNDMMARK_H
#define _XT_CONNNDMMARK_H

#include <linux/types.h>

/* Must be in-sync with ndm/Netfilter/Typedefs.h */

enum xt_connndmmark_list {
	XT_CONNNDMMARK_FWD           = 0x80,
	XT_CONNNDMMARK_DNS           = 0x40,
	XT_CONNNDMMARK_TLS           = 0x20,
	XT_CONNNDMMARK_CS_MASK       = 0x1C,
	XT_CONNNDMMARK_CS_SHIFT      = 0x02,
	XT_CONNNDMMARK_IPSEC_IN      = 0x01,
	XT_CONNNDMMARK_IPSEC_OUT     = 0x02,
	XT_CONNNDMMARK_IPSEC_MASK    = 0x03,
	XT_CONNNDMMARK_KEEP_CT       = 0x100,
	XT_CONNNDMMARK_VPN_SRV       = 0x200,
};

enum {
	XT_CONNNDMMARK_SET = 0,
	XT_CONNNDMMARK_RESTORE
};

struct xt_connndmmark_tginfo {
	__u32 ctmark, ctmask, nfmask;
	__u32 mode;
};

struct xt_connndmmark_mtinfo {
	__u32 mark, mask;
	__u32 invert;
};

static inline bool xt_connndmmark_is_keep_ct(struct nf_conn *ct)
{
	return (ct->ndm_mark & XT_CONNNDMMARK_KEEP_CT) ==
		XT_CONNNDMMARK_KEEP_CT;
}

static inline bool xt_connndmmark_is_vpn_srv(struct nf_conn *ct)
{
	return (ct->ndm_mark & XT_CONNNDMMARK_VPN_SRV) ==
		XT_CONNNDMMARK_VPN_SRV;
}


#endif /*_XT_CONNNDMMARK_H*/
