#ifndef __INCLUDE_LINUX_ETH_STATS_HOOKS_H
#define __INCLUDE_LINUX_ETH_STATS_HOOKS_H

#include <linux/types.h>
#include <linux/spinlock.h>

extern rwlock_t eth_stats_rw_lock;

static inline void eth_stats_read_lock(void)
{
	read_lock(&eth_stats_rw_lock);
}

static inline void eth_stats_read_unlock(void)
{
	read_unlock(&eth_stats_rw_lock);
}

static inline void eth_stats_read_lock_bh(void)
{
	read_lock_bh(&eth_stats_rw_lock);
}

static inline void eth_stats_read_unlock_bh(void)
{
	read_unlock_bh(&eth_stats_rw_lock);
}

static inline void eth_stats_write_lock_bh(void)
{
	write_lock_bh(&eth_stats_rw_lock);
}

static inline void eth_stats_write_unlock_bh(void)
{
	write_unlock_bh(&eth_stats_rw_lock);
}

#endif /* __INCLUDE_LINUX_ETH_STATS_HOOKS_H */

