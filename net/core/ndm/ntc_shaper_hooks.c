#include <linux/cache.h>
#include <linux/module.h>
#include <linux/ntc_shaper_hooks.h>

DEFINE_RWLOCK(ntc_shaper_lock);
EXPORT_SYMBOL(ntc_shaper_lock);

ntc_shaper_hook_fn *ntc_shaper_ingress_hook;
EXPORT_SYMBOL(ntc_shaper_ingress_hook);

ntc_shaper_hook_fn *ntc_shaper_egress_hook;
EXPORT_SYMBOL(ntc_shaper_egress_hook);

ntc_shaper_bound_hook_fn *ntc_shaper_check_bound_hook;
EXPORT_SYMBOL(ntc_shaper_check_bound_hook);

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
__read_mostly enum nf_ct_ext_id nf_ct_ext_id_ntc;
EXPORT_SYMBOL(nf_ct_ext_id_ntc);
#endif
