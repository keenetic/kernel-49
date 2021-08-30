#ifndef __INCLUDE_LINUX_ETH_SWITCH_HOOKS_H
#define __INCLUDE_LINUX_ETH_SWITCH_HOOKS_H

#include <linux/types.h>
#include <linux/spinlock.h>

#define ETH_SWITCH_HOOKS_MARK_MR_MAC

struct net_device;

typedef bool eth_switch_iface_fn(const struct net_device *const dev);

typedef int eth_switch_map_mc_mac_fn(const struct net_device *const dev,
				     const u8 *const uc_mac,
				     const u8 *const mc_mac);

typedef int eth_switch_unmap_mc_mac_fn(const struct net_device *const dev,
				       const u8 *const uc_mac,
				       const u8 *const mc_mac);

typedef int eth_switch_mark_mr_mac_fn(const struct net_device *const dev,
				      const u8 *const mr_mac,
				      bool gquery);

typedef int eth_switch_set_wan_port_fn(const unsigned char port);

typedef int eth_switch_mt7530_reg_write_bh_fn(const u32 addr,
					      const u32 data);

typedef int eth_switch_rtl83xx_reg_write_bh_fn(const u32 addr,
					       const u32 data);

typedef int eth_switch_rtl8211_reg_write_bh_fn(const u16 page,
					       const u16 addr,
					       const u16 data);

extern rwlock_t eth_switch_lock;

#define ETH_SWITCH_DECLARE_HOOK(name)					\
extern eth_switch_##name##_fn *eth_switch_##name##_hook;		\
									\
static inline eth_switch_##name##_fn *					\
eth_switch_##name##_hook_get(void)					\
{									\
	read_lock_bh(&eth_switch_lock);					\
									\
	return eth_switch_##name##_hook;				\
}									\
									\
static inline void							\
eth_switch_##name##_hook_put(void)					\
{									\
	read_unlock_bh(&eth_switch_lock);				\
}									\
									\
static inline void							\
eth_switch_##name##_hook_set(eth_switch_##name##_fn *name##_hook)	\
{									\
	write_lock_bh(&eth_switch_lock);				\
	eth_switch_##name##_hook = name##_hook;				\
	write_unlock_bh(&eth_switch_lock);				\
}

ETH_SWITCH_DECLARE_HOOK(iface)
ETH_SWITCH_DECLARE_HOOK(map_mc_mac)
ETH_SWITCH_DECLARE_HOOK(unmap_mc_mac)
ETH_SWITCH_DECLARE_HOOK(mark_mr_mac)
ETH_SWITCH_DECLARE_HOOK(set_wan_port)

ETH_SWITCH_DECLARE_HOOK(mt7530_reg_write_bh);
ETH_SWITCH_DECLARE_HOOK(rtl83xx_reg_write_bh);
ETH_SWITCH_DECLARE_HOOK(rtl8211_reg_write_bh);

#define ETH_SWITCH_DECLARE_OPS(name)					\
extern struct eth_switch_##name##_ops *__eth_switch_##name##_ops;	\
									\
static inline bool							\
eth_switch_##name##_ops_get_bh(struct eth_switch_##name##_ops **ops)	\
{									\
	read_lock_bh(&eth_switch_lock);					\
	*ops = __eth_switch_##name##_ops;				\
									\
	if (*ops == NULL) {						\
		read_unlock_bh(&eth_switch_lock);			\
		return false;						\
	}								\
									\
	return true;							\
}									\
									\
static inline void							\
eth_switch_##name##_ops_put_bh(struct eth_switch_##name##_ops **ops)	\
{									\
	*ops = NULL;							\
	read_unlock_bh(&eth_switch_lock);				\
}									\
									\
static inline void							\
eth_switch_##name##_ops_set_bh(struct eth_switch_##name##_ops *ops)	\
{									\
	write_lock_bh(&eth_switch_lock);				\
	__eth_switch_##name##_ops = ops;				\
	write_unlock_bh(&eth_switch_lock);				\
}

struct eth_switch_mt7531_ops {
	int (*r32_bh)(const u32 addr, u32 *data);
	int (*w32_bh)(const u32 addr, const u32 data);
};

ETH_SWITCH_DECLARE_OPS(mt7531);

struct eth_switch_rtl8221_ops {
	int (*r32_bh)(const u32 addr, u32 *data);
	int (*w32_bh)(const u32 addr, const u32 data);
};

ETH_SWITCH_DECLARE_OPS(rtl8221);

#endif /* __INCLUDE_LINUX_ETH_SWITCH_HOOKS_H */

