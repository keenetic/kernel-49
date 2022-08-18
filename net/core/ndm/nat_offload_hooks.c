#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>

#if IS_ENABLED(CONFIG_FAST_NAT)
/* fastvpn */
int (*go_swnat)(struct sk_buff *skb, u8 origin) = NULL;
EXPORT_SYMBOL(go_swnat);

void (*prebind_from_raeth)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(prebind_from_raeth);

void (*prebind_from_usb_mac)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(prebind_from_usb_mac);

void (*prebind_from_fastnat)(struct sk_buff *skb,
			     __be32 orig_saddr,
			     __be16 orig_sport,
			     struct nf_conn *ct,
			     enum ip_conntrack_info ctinfo) = NULL;
EXPORT_SYMBOL(prebind_from_fastnat);

void (*prebind_from_l2tptx)(struct sk_buff *skb,
			    struct sock *sock,
			    __be16 l2w_tid,
			    __be16 l2w_sid,
			    __be16 w2l_tid,
			    __be16 w2l_sid,
			    __be32 saddr,
			    __be32 daddr,
			    __be16 sport,
			    __be16 dport) = NULL;
EXPORT_SYMBOL(prebind_from_l2tptx);

void (*prebind_from_pptptx)(struct sk_buff *skb,
			    struct iphdr *iph_int,
			    struct sock *sock,
			    __be32 saddr,
			    __be32 daddr) = NULL;
EXPORT_SYMBOL(prebind_from_pptptx);

void (*prebind_from_pppoetx)(struct sk_buff *skb,
			     struct sock *sock,
			     __be16 sid) = NULL;
EXPORT_SYMBOL(prebind_from_pppoetx);

void (*prebind_from_nat46tx)(struct sk_buff *skb,
			     struct iphdr *ip4,
			     struct ipv6hdr *ip6) = NULL;
EXPORT_SYMBOL(prebind_from_nat46tx);

void (*prebind_from_encap46tx)(struct sk_buff *skb,
			       const struct iphdr *ip4,
			       const struct ipv6hdr *ip6) = NULL;
EXPORT_SYMBOL(prebind_from_encap46tx);

void (*prebind_from_ct_mark)(struct nf_conn *ct);
EXPORT_SYMBOL(prebind_from_ct_mark);

#ifdef CONFIG_FAST_NAT_V2
int (*nf_fastpath_pptp_in)(struct sk_buff *skb,
			   unsigned int dataoff,
			   u16 call_id) = NULL;
EXPORT_SYMBOL(nf_fastpath_pptp_in);

int (*nf_fastpath_l2tp_in)(struct sk_buff *skb,
			   unsigned int dataoff) = NULL;
EXPORT_SYMBOL(nf_fastpath_l2tp_in);

#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
int (*nf_fastroute_rtcache_in)(u_int8_t pf,
			       struct sk_buff *skb,
			       int ifindex) = NULL;
EXPORT_SYMBOL(nf_fastroute_rtcache_in);

int (*prebind_from_rtcache)(struct sk_buff *skb,
			    struct nf_conn *ct,
			    enum ip_conntrack_info ctinfo) = NULL;
EXPORT_SYMBOL(prebind_from_rtcache);
#endif
#endif /* CONFIG_FAST_NAT_V2 */
#endif /* CONFIG_FAST_NAT */

#if IS_ENABLED(CONFIG_RA_HW_NAT)
int (*ra_sw_nat_hook_rx)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ra_sw_nat_hook_rx);

int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_no) = NULL;
EXPORT_SYMBOL(ra_sw_nat_hook_tx);

int (*ppe_del_entry_by_addr_hook)(const char *addr) = NULL;
EXPORT_SYMBOL(ppe_del_entry_by_addr_hook);

int (*ppe_dev_has_accel_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_has_accel_hook);

void (*ppe_dev_register_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_register_hook);

void (*ppe_dev_unregister_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_unregister_hook);
#endif
