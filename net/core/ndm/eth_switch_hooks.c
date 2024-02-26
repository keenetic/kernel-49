#include <linux/module.h>
#include <linux/eth_switch_hooks.h>

DEFINE_RWLOCK(eth_switch_lock);
EXPORT_SYMBOL(eth_switch_lock);

#define ETH_SWITCH_DEFINE_HOOK_RCU(name)				\
eth_switch_##name##_fn __rcu *eth_switch_##name##_hook;			\
EXPORT_SYMBOL(eth_switch_##name##_hook);

#define ETH_SWITCH_DEFINE_HOOK(name)					\
eth_switch_##name##_fn *eth_switch_##name##_hook;			\
EXPORT_SYMBOL(eth_switch_##name##_hook);

ETH_SWITCH_DEFINE_HOOK_RCU(map_mc_mac)
ETH_SWITCH_DEFINE_HOOK_RCU(unmap_mc_mac)
ETH_SWITCH_DEFINE_HOOK_RCU(mark_mr_mac)
ETH_SWITCH_DEFINE_HOOK_RCU(set_wan_port)

ETH_SWITCH_DEFINE_HOOK(mt7530_reg_write_bh)
ETH_SWITCH_DEFINE_HOOK(rtl83xx_reg_write_bh)

#define ETH_SWITCH_DEFINE_OPS(name)					\
struct eth_switch_##name##_ops *__eth_switch_##name##_ops;		\
EXPORT_SYMBOL(__eth_switch_##name##_ops);

ETH_SWITCH_DEFINE_OPS(mt7531)
ETH_SWITCH_DEFINE_OPS(rtl8211)
ETH_SWITCH_DEFINE_OPS(cl45)
