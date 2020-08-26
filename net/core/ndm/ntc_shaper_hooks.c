#include <linux/module.h>
#include <linux/ntc_shaper_hooks.h>

DEFINE_RWLOCK(ntc_shaper_lock);
EXPORT_SYMBOL(ntc_shaper_lock);

ntc_shaper_hook_fn *ntc_shaper_ingress_hook = NULL;
EXPORT_SYMBOL(ntc_shaper_ingress_hook);

ntc_shaper_hook_fn *ntc_shaper_egress_hook = NULL;
EXPORT_SYMBOL(ntc_shaper_egress_hook);

ntc_shaper_bound_hook_fn *ntc_shaper_check_bound_hook = NULL;
EXPORT_SYMBOL(ntc_shaper_check_bound_hook);

#ifdef CONFIG_NTCE_MODULE
unsigned int (*ntce_pass_pkt_func)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ntce_pass_pkt_func);

void (*ntce_enq_pkt_hook_func)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ntce_enq_pkt_hook_func);
#endif

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
enum nf_ct_ext_id nf_ct_ext_id_ntc = 0;
EXPORT_SYMBOL(nf_ct_ext_id_ntc);
#endif
