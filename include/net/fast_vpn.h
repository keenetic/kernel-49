#ifndef __FAST_VPN_H_
#define __FAST_VPN_H_

#include <linux/list.h>

struct iphdr;
struct nf_conn;
enum ip_conntrack_info;

#define FAST_VPN_ACTION_SETUP		1
#define FAST_VPN_ACTION_RELEASE		0

#define FAST_VPN_RES_OK			1
#define FAST_VPN_RES_SKIPPED	0

/* SWNAT section */

#define SWNAT_ORIGIN_RAETH		0x10
#define SWNAT_ORIGIN_RT2860		0x20
#define SWNAT_ORIGIN_USB_MAC	0x30

#define SWNAT_CB_OFFSET		47

#define SWNAT_FNAT_MARK		0x01
#define SWNAT_PPP_MARK		0x02

#define SWNAT_RESET_MARKS(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = 0; \
} while (0);

/* FNAT mark */

#define SWNAT_FNAT_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_FNAT_MARK; \
} while (0);

#define SWNAT_FNAT_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_FNAT_MARK)

/* End of FNAT mark */

/* PPP mark */

#define SWNAT_PPP_SET_MARK(skb_) \
do { \
	(skb_)->cb[SWNAT_CB_OFFSET] = SWNAT_PPP_MARK; \
} while (0);

#define SWNAT_PPP_CHECK_MARK(skb_) \
	((skb_)->cb[SWNAT_CB_OFFSET] == SWNAT_PPP_MARK)

/* End of PPP mark */

/* KA mark */

#define SWNAT_KA_SET_MARK(skb_) \
do { \
	(skb_)->swnat_ka_mark = 1; \
} while (0);

#define SWNAT_KA_CHECK_MARK(skb_) \
	((skb_)->swnat_ka_mark != 0)

/* End of KA mark */

/* prebind hooks */
extern int (*go_swnat)(struct sk_buff *skb, u8 origin);

extern void (*prebind_from_raeth)(struct sk_buff *skb);

extern void (*prebind_from_usb_mac)(struct sk_buff * skb);

extern void (*prebind_from_fastnat)(struct sk_buff *skb,
				    __be32 orig_saddr,
				    __be16 orig_sport,
				    struct nf_conn *ct,
				    enum ip_conntrack_info ctinfo);

extern void (*prebind_from_l2tptx)(struct sk_buff *skb,
				   struct sock *sock,
				   __be16 l2w_tid,
				   __be16 l2w_sid,
				   __be16 w2l_tid,
				   __be16 w2l_sid,
				   __be32 saddr,
				   __be32 daddr,
				   __be16 sport,
				   __be16 dport);

extern void (*prebind_from_pptptx)(struct sk_buff *skb,
				   struct iphdr *iph_int,
				   struct sock *sock,
				   __be32 saddr,
				   __be32 daddr);

extern void (*prebind_from_pppoetx)(struct sk_buff *skb,
				    struct sock *sock,
				    __be16 sid);

extern void (*prebind_from_ct_mark)(struct nf_conn *ct);
/* End of prebind hooks */

/* List of new MC streams */

struct new_mc_streams {
	u32 group_addr;
	struct net_device * out_dev;
	u32 handled;

	struct list_head list;
};


#endif /*__FAST_VPN_H_ */
