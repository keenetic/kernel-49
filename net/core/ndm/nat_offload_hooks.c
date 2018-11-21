#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
int (*ra_sw_nat_hook_rx)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ra_sw_nat_hook_rx);

int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_no) = NULL;
EXPORT_SYMBOL(ra_sw_nat_hook_tx);

int (*ppe_dev_has_accel_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_has_accel_hook);

void (*ppe_dev_register_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_register_hook);

void (*ppe_dev_unregister_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_unregister_hook);

void (*ppe_enable_hook)(int do_ppe_enable) = NULL;
EXPORT_SYMBOL(ppe_enable_hook);
#endif
