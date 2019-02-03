#include <linux/module.h>
#include <linux/ntc_shaper_hooks.h>

DEFINE_RWLOCK(ntc_shaper_lock);
EXPORT_SYMBOL(ntc_shaper_lock);

ntc_shaper_hook_fn *ntc_shaper_ingress_hook = NULL;
EXPORT_SYMBOL(ntc_shaper_ingress_hook);

ntc_shaper_hook_fn *ntc_shaper_egress_hook = NULL;
EXPORT_SYMBOL(ntc_shaper_egress_hook);

int (*ntc_shaper_check_ip_and_mac)(uint32_t ipaddr, uint8_t * mac) = NULL;
EXPORT_SYMBOL(ntc_shaper_check_ip_and_mac);

#ifdef CONFIG_NTCE_MODULE
unsigned int (*ntce_pass_pkt_func)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ntce_pass_pkt_func);

void (*ntce_enq_pkt_hook_func)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ntce_enq_pkt_hook_func);
#endif
