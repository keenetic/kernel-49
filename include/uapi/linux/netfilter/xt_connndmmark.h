#ifndef _XT_CONNNDMMARK_H
#define _XT_CONNNDMMARK_H

#include <linux/types.h>

/* Must be in-sync with ndm/Netfilter/Typedefs.h */

enum xt_connndmmark_list {
	XT_CONNNDMMARK_DNAT          = 0x80,
	XT_CONNNDMMARK_DNS           = 0x40,
	XT_CONNNDMMARK_TLS           = 0x20,
	XT_CONNNDMMARK_CS_MASK       = 0x1C,
	XT_CONNNDMMARK_CS_SHIFT      = 0x02,
	XT_CONNNDMMARK_IPSEC_IN      = 0x01,
	XT_CONNNDMMARK_IPSEC_OUT     = 0x02,
	XT_CONNNDMMARK_IPSEC_MASK    = 0x03,
};

enum {
	XT_CONNNDMMARK_SET = 0,
	XT_CONNNDMMARK_RESTORE
};

struct xt_connndmmark_tginfo {
	__u8 ctmark, ctmask, nfmask;
	__u8 mode;
};

struct xt_connndmmark_mtinfo {
	__u8 mark, mask;
	__u8 invert;
};

#endif /*_XT_CONNNDMMARK_H*/
