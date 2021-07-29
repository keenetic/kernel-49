#include <linux/module.h>
#include <linux/eth_stats_hooks.h>

DEFINE_RWLOCK(eth_stats_rw_lock);
EXPORT_SYMBOL(eth_stats_rw_lock);
