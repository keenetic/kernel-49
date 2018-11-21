#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ppp_channel.h>

#if IS_MODULE(CONFIG_PPP)
void (*ppp_stat_add_tx_hook)(struct ppp_channel *pch, u32 add_pkts,
			     u32 add_bytes) = NULL;
EXPORT_SYMBOL(ppp_stat_add_tx_hook);

void (*ppp_stat_add_rx_hook)(struct ppp_channel *pch, u32 add_pkts,
			     u32 add_bytes) = NULL;
EXPORT_SYMBOL(ppp_stat_add_rx_hook);

void (*ppp_stat_reset_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppp_stat_reset_hook);

#if IS_ENABLED(CONFIG_RA_HW_NAT)
void (*ppp_stats_update_hook)(struct net_device *dev,
			      u32 rx_bytes, u32 rx_packets,
			      u32 tx_bytes, u32 tx_packets) = NULL;
EXPORT_SYMBOL(ppp_stats_update_hook);
#endif

#endif
