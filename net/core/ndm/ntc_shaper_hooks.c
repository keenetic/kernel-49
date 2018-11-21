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
