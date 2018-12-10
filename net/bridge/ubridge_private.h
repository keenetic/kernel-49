#ifndef _BR_UBRIDGE_PRIVATE_H
#define _BR_UBRIDGE_PRIVATE_H

#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>

struct ubr_private {
	struct net_device		*slave_dev;
	struct pcpu_sw_netstats		__percpu *stats;
	struct list_head		list;
	struct net_device		*dev;
	unsigned long			flags;
};

#define is_ubridge_port(dev)	(dev->priv_flags & IFF_UBRIDGE_PORT)
#define is_ubridge(dev)			(dev->priv_flags & IFF_UBRIDGE)

int ubr_update_stats(struct net_device *dev, unsigned long rxbytes,
	unsigned long rxpackets, unsigned long txbytes, unsigned long txpackets);

static inline struct net_device *ubr_get_by_slave_rcu(
	struct net_device *slave_dev)
{
	struct ubr_private *ubr = NULL;

	if (!slave_dev || !is_ubridge_port(slave_dev))
		return NULL;

	ubr = rcu_dereference(slave_dev->rx_handler_data);

	if (!ubr || !is_ubridge(ubr->dev))
		return NULL;

	return ubr->dev;
}

#endif
