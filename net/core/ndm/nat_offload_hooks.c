#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/fast_vpn.h>

#if IS_ENABLED(CONFIG_FAST_NAT)
/* fastvpn */
int (*go_swnat)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(go_swnat);

void (*prebind_from_eth)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(prebind_from_eth);

void (*prebind_from_fastnat)(struct swnat_fastnat_t *fastnat) = NULL;
EXPORT_SYMBOL(prebind_from_fastnat);

void (*prebind_from_l2tptx)(struct swnat_l2tp_t *l2tp) = NULL;
EXPORT_SYMBOL(prebind_from_l2tptx);

void (*prebind_from_pptptx)(struct swnat_pptp_t *pptp) = NULL;
EXPORT_SYMBOL(prebind_from_pptptx);

void (*prebind_from_pppoetx)(struct swnat_pppoe_t *pppoe) = NULL;
EXPORT_SYMBOL(prebind_from_pppoetx);

void (*prebind_from_nat46tx)(struct swnat_nat46_t *nat46) = NULL;
EXPORT_SYMBOL(prebind_from_nat46tx);

void (*prebind_from_encap46tx)(struct swnat_encap46_t *encap46) = NULL;
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

void (*prebind_from_rtcache)(struct swnat_rtcache_t *rtcache) = NULL;
EXPORT_SYMBOL(prebind_from_rtcache);
#endif
#endif /* CONFIG_FAST_NAT_V2 */
#endif /* CONFIG_FAST_NAT */

#if IS_ENABLED(CONFIG_RA_HW_NAT)
int (*ra_sw_nat_hook_rx)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ra_sw_nat_hook_rx);

int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, const struct gmac_info *gi) = NULL;
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
