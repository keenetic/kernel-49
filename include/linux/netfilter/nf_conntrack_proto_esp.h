#ifndef _CONNTRACK_PROTO_ESP_H
#define _CONNTRACK_PROTO_ESP_H
#include <asm/byteorder.h>

struct esp_hdr_ct {
	__be32 spi;
};

struct nf_ct_esp {
	unsigned int stream_timeout;
	unsigned int timeout;
};

#ifdef __KERNEL__
#include <net/netfilter/nf_conntrack_tuple.h>

struct nf_conn;

/* structure for original <-> reply keymap */
struct nf_ct_esp_keymap {
	struct list_head list;
	struct nf_conntrack_tuple tuple;
};

void nf_nat_need_esp(void);

#endif /* __KERNEL__ */
#endif /* _CONNTRACK_PROTO_ESP_H */
