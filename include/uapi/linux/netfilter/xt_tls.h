#ifndef _XT_TLS_MATCH_H
#define _XT_TLS_MATCH_H

#define XT_TLS_OP_HOST				0x01
#define XT_TLS_OP_BLOCK_ESNI		0x02
#define XT_TLS_OP_FRAG_MATCH		0x04

/* match info */
struct xt_tls_mt_info {
	__u8 invert;
	char tls_host[127];
};

#endif /* _XT_TLS_MATCH_H */
