#define DRV_NAME		"ubridge"
#define DRV_VERSION		"1.2"
#define DRV_DESCRIPTION		"Tiny bridge driver"
#define DRV_AUTHOR		"Alexander Papenko <ap@ndmsystems.com>"

/**
 * UBR_UC_SYNC - allow sync unicast list for slave device.
 * Note: usbnet devices usually not implements unicast list.
 **/
/* #define UBR_UC_SYNC */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_bridge.h>
#include <linux/netfilter_bridge.h>
#include <net/ip.h>
#include <net/arp.h>
#include <../net/8021q/vlan.h>

#if IS_ENABLED(CONFIG_IPV6)
#include <linux/ipv6.h>
#include <net/if_inet6.h>
#endif

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/fast_vpn.h>
#endif

#include "ubridge_private.h"

#define MAC_FORCED	0

#define COMMON_FEATURES (NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA | \
			 NETIF_F_GSO_MASK | NETIF_F_HW_CSUM)

static LIST_HEAD(ubr_list);

static inline struct ubr_private *ubr_priv_get_rcu(const struct net_device *dev)
{
	return rcu_dereference(dev->rx_handler_data);
}

static inline bool is_netdev_rawip(struct net_device *netdev)
{
	return (netdev->type == ARPHRD_NONE);
}

static rx_handler_result_t ubr_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct ubr_private *ubr = ubr_priv_get_rcu(skb->dev);
	struct pcpu_sw_netstats *ustats;

	if (ubr == NULL)
		return RX_HANDLER_PASS;

#ifdef DEBUG
	pr_info("%s: packet %s -> %s\n",
		__func__, ubr->slave_dev->name, ubr->dev->name);
#endif

	ustats = this_cpu_ptr(ubr->stats);

	if (likely(1
#if IS_ENABLED(CONFIG_FAST_NAT)
	    && !SWNAT_KA_CHECK_MARK(skb)
#endif
#if IS_ENABLED(CONFIG_RA_HW_NAT)
	    && !FOE_SKB_IS_KEEPALIVE(skb)
#endif
#if IS_ENABLED(CONFIG_USB_NET_KPDSL)
	    && skb->protocol != htons(ETH_P_EBM)
#endif
		)) {
		u64_stats_update_begin(&ustats->syncp);
		ustats->rx_packets++;
		ustats->rx_bytes += skb->len;
		u64_stats_update_end(&ustats->syncp);
	}

	skb->dev = ubr->dev;
	skb->pkt_type = PACKET_HOST;

	skb_dst_drop(skb);

	if (is_netdev_rawip(ubr->slave_dev)) {
		struct iphdr *iph;
		struct ethhdr *eth;

		if (unlikely((skb_headroom(skb) < ETH_HLEN)
			|| skb_shared(skb)
			|| skb_cloned(skb))) {
			struct sk_buff *skb2;

			skb2 = skb_realloc_headroom(skb, ETH_HLEN);

			consume_skb(skb);
			if (!skb2) {
				ubr->dev->stats.rx_dropped++;

				return RX_HANDLER_CONSUMED;
			}

			skb = skb2;
		}

		iph = (struct iphdr *)skb->data;

		skb_push(skb, ETH_HLEN);

		skb_reset_mac_header(skb);
		skb_reset_network_header(skb);

		eth = (struct ethhdr *)skb->data;

		ether_addr_copy(eth->h_dest, ubr->dev->dev_addr);

		if (iph->version == 6) {
#if IS_ENABLED(CONFIG_IPV6)
			struct ipv6hdr *ip6h = (struct ipv6hdr *)iph;

			if (skb->len < ETH_HLEN + sizeof(struct ipv6hdr)) {
				kfree_skb(skb);
				ubr->dev->stats.rx_errors++;

				return RX_HANDLER_CONSUMED;
			}

			if (ipv6_addr_is_multicast(&ip6h->daddr))
				ipv6_eth_mc_map(&ip6h->daddr, eth->h_dest);

			eth->h_proto = htons(ETH_P_IPV6);
#else
			kfree_skb(skb);
			ubr->dev->stats.rx_errors++;

			return RX_HANDLER_CONSUMED;
#endif
		} else if (iph->version == 4) {
			if (ipv4_is_lbcast(iph->daddr))
				eth_broadcast_addr(eth->h_dest);
			else if (ipv4_is_multicast(iph->daddr))
				ip_eth_mc_map(iph->daddr, eth->h_dest);

			eth->h_proto = htons(ETH_P_IP);
		} else {
			/* Something weird... */

			consume_skb(skb);
			ubr->dev->stats.rx_dropped++;

			return RX_HANDLER_CONSUMED;
		}

		ether_addr_copy(eth->h_source, ubr->dev->dev_addr);

		skb->protocol = eth_type_trans(skb, ubr->dev);
	}

	netif_receive_skb(skb);

	return RX_HANDLER_CONSUMED;
}

int ubr_update_stats(struct net_device *dev,
		     unsigned long rxbytes,
		     unsigned long rxpackets,
		     unsigned long txbytes,
		     unsigned long txpackets)
{
	struct ubr_private *ubr = netdev_priv(dev);
	struct pcpu_sw_netstats *ustats;

	if (!is_ubridge(dev) || ubr == NULL)
		return -EINVAL;

	ustats = this_cpu_ptr(ubr->stats);

	u64_stats_update_begin(&ustats->syncp);
	ustats->rx_packets += rxpackets;
	ustats->rx_bytes += rxbytes;
	ustats->tx_packets += txpackets;
	ustats->tx_bytes += txbytes;
	u64_stats_update_end(&ustats->syncp);

	return 0;
}
EXPORT_SYMBOL(ubr_update_stats);

static int ubr_init(struct net_device *dev)
{
	struct ubr_private *ubr = netdev_priv(dev);

	ubr->stats = netdev_alloc_pcpu_stats(struct pcpu_sw_netstats);
	if (!ubr->stats)
		return -ENOMEM;

	return 0;
}

static int ubr_open(struct net_device *dev)
{
	netdev_update_features(dev);
	netif_start_queue(dev);
	return 0;
}

static int ubr_stop(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

static void ubr_send_arp_reply(struct sk_buff *skb,
			    struct ubr_private *ubr)
{
	const struct ethhdr *eth_src = (struct ethhdr *)eth_hdr(skb);
	const struct arphdr *arph_src =
		(struct arphdr *)((u8 *)eth_src + ETH_HLEN);
	struct sk_buff *skb2;
	u8 *p_src;
	unsigned int data_len;
	__be32 source_ip, target_ip;

	data_len = ETH_HLEN + arp_hdr_len(ubr->dev);
	if (skb->len < data_len) {
		ubr->dev->stats.tx_errors++;
		return;
	}

	p_src = (u8 *)(arph_src + 1);
	source_ip = *((__be32 *)(p_src + ETH_ALEN));
	target_ip = *((__be32 *)(p_src + 2 * ETH_ALEN + sizeof(u32)));

	if (arph_src->ar_op != htons(ARPOP_REQUEST) ||
	    ipv4_is_multicast(target_ip) ||
	    target_ip == 0)
		return;

	skb2 = arp_create(ARPOP_REPLY, ETH_P_ARP,
			  source_ip,	/* Set target IP as source IP address */
			  ubr->dev,
			  target_ip,	/* Set sender IP as target IP address */
			  eth_src->h_source,
			  NULL,		/* Set sender MAC as interface MAC */
			  p_src);	/* Set target MAC as source MAC */

	if (skb2 == NULL)
		return;

	skb_reset_mac_header(skb2);
	skb_pull_inline(skb2, ETH_HLEN);

	netif_receive_skb(skb2);
}

static netdev_tx_t ubr_xmit(struct sk_buff *skb,
			    struct net_device *dev)
{
	struct ubr_private *ubr = netdev_priv(dev);
	struct net_device *slave_dev = ubr->slave_dev;
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct pcpu_sw_netstats *ustats;

	if (!slave_dev) {
		dev_kfree_skb(skb);
		return -ENOTCONN;
	}

	if (is_netdev_rawip(slave_dev)) {
		unsigned int maclen = 0;

		switch (ntohs(eth->h_proto)) {
		case ETH_P_IP:
		case ETH_P_IPV6:
			maclen = ETH_HLEN;
			break;
		case ETH_P_ARP:
			skb_reset_mac_header(skb);
			ubr_send_arp_reply(skb, ubr);
			consume_skb(skb);
			return NET_XMIT_SUCCESS;
		default:
			break;
		}

		if (maclen == 0) {
			/* Unsupported protocols, silently drop */
			consume_skb(skb);
			dev->stats.tx_dropped++;

			return NET_XMIT_SUCCESS;
		}

		if (!(dev->flags & IFF_PROMISC) &&
		    !ether_addr_equal(eth->h_dest, dev->dev_addr) &&
		    !is_multicast_ether_addr(eth->h_dest) &&
		    !is_broadcast_ether_addr(eth->h_dest)) {
			/* Packet is not for us, silently drop */
			consume_skb(skb);
			dev->stats.tx_dropped++;

			return NET_XMIT_SUCCESS;
		}

		skb_pull_inline(skb, maclen);
		skb_reset_mac_header(skb);
		skb_reset_network_header(skb);
	}

	ustats = this_cpu_ptr(ubr->stats);

	if (likely(1
#if IS_ENABLED(CONFIG_FAST_NAT)
	    && !SWNAT_KA_CHECK_MARK(skb)
#endif
#if IS_ENABLED(CONFIG_RA_HW_NAT)
	    && !FOE_SKB_IS_KEEPALIVE(skb)
#endif
#if IS_ENABLED(CONFIG_USB_NET_KPDSL)
	    && eth->h_proto != htons(ETH_P_EBM)
#endif
		)) {
		u64_stats_update_begin(&ustats->syncp);
		ustats->tx_packets++;
		ustats->tx_bytes += skb->len;
		u64_stats_update_end(&ustats->syncp);
	}

	skb->dev = slave_dev;
	return dev_queue_xmit(skb);
}

static struct rtnl_link_stats64 *
ubr_get_stats64(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	struct ubr_private *ubr = netdev_priv(dev);
	struct pcpu_sw_netstats tmp, sum = { 0 };
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		unsigned int start;
		const struct pcpu_sw_netstats *ustats;

		ustats = per_cpu_ptr(ubr->stats, cpu);

		do {
			start = u64_stats_fetch_begin_irq(&ustats->syncp);
			tmp.tx_bytes   = ustats->tx_bytes;
			tmp.tx_packets = ustats->tx_packets;
			tmp.rx_bytes   = ustats->rx_bytes;
			tmp.rx_packets = ustats->rx_packets;
		} while (u64_stats_fetch_retry_irq(&ustats->syncp, start));

		sum.tx_bytes   += tmp.tx_bytes;
		sum.tx_packets += tmp.tx_packets;
		sum.rx_bytes   += tmp.rx_bytes;
		sum.rx_packets += tmp.rx_packets;
	}

	stats->tx_bytes   = sum.tx_bytes;
	stats->tx_packets = sum.tx_packets;
	stats->rx_bytes   = sum.rx_bytes;
	stats->rx_packets = sum.rx_packets;

	stats->tx_errors  = dev->stats.tx_errors;
	stats->tx_dropped = dev->stats.tx_dropped;
	stats->rx_errors  = dev->stats.rx_errors;
	stats->rx_dropped = dev->stats.rx_dropped;

	return stats;
}

void ubr_reset_stats(struct net_device *dev)
{
	int cpu;
	struct ubr_private *ubr = netdev_priv(dev);

	if (!is_ubridge(dev) || ubr == NULL)
		return;

	local_bh_disable();

	for_each_possible_cpu(cpu) {
		struct pcpu_sw_netstats *ustats = per_cpu_ptr(ubr->stats, cpu);

		u64_stats_update_begin(&ustats->syncp);
		ustats->rx_bytes = 0;
		ustats->rx_packets = 0;
		ustats->tx_bytes = 0;
		ustats->tx_packets = 0;
		u64_stats_update_end(&ustats->syncp);
	}

	dev->stats.tx_errors = 0;
	dev->stats.tx_dropped = 0;
	dev->stats.rx_errors = 0;
	dev->stats.rx_dropped = 0;

	local_bh_enable();
}
EXPORT_SYMBOL(ubr_reset_stats);

static int
ubr_set_mac_addr(struct net_device *master_dev, struct sockaddr *addr)
{
	struct sockaddr old_addr;
	struct vlan_info *vlan_info;

	ether_addr_copy(old_addr.sa_data, master_dev->dev_addr);
	ether_addr_copy(master_dev->dev_addr, addr->sa_data);

	/* Update all VLAN sub-devices' MAC address */
	vlan_info = rtnl_dereference(master_dev->vlan_info);
	if (vlan_info) {
		struct vlan_group *grp = &vlan_info->grp;
		struct net_device *vlan_dev;
		int i;

		vlan_group_for_each_dev(grp, i, vlan_dev) {
			struct sockaddr vaddr;

			/* Do not modify manually changed vlan MAC */
			if (!ether_addr_equal(old_addr.sa_data,
					      vlan_dev->dev_addr))
				continue;

			ether_addr_copy(vaddr.sa_data, addr->sa_data);
			vaddr.sa_family = vlan_dev->type;

			dev_set_mac_address(vlan_dev, &vaddr);
		}
	}

	call_netdevice_notifiers(NETDEV_CHANGEADDR, master_dev);

	return 0;
}

static int ubr_set_mac_addr_force(struct net_device *dev, void *p)
{
	struct ubr_private *ubr = netdev_priv(dev);
	struct net_device *slave_dev = ubr->slave_dev;
	int ret;

	ret = ubr_set_mac_addr(dev, p);
	if (ret)
		return ret;

	set_bit(MAC_FORCED, &ubr->flags);

	if (slave_dev && !is_netdev_rawip(slave_dev)) {
		if (!ether_addr_equal(dev->dev_addr, slave_dev->dev_addr))
			dev_set_promiscuity(slave_dev, 1);
		else
			dev_set_promiscuity(slave_dev, -1);
	}

	return 0;
}

static void ubr_change_rx_flags(struct net_device *dev, int change)
{
	struct ubr_private *master_info = netdev_priv(dev);
	struct net_device *slave_dev = master_info->slave_dev;

	if (!slave_dev)
		return;

	if (is_netdev_rawip(slave_dev))
		return;

	if (change & IFF_ALLMULTI)
		dev_set_allmulti(slave_dev, (dev->flags & IFF_ALLMULTI) ?
				 1 : -1);

	if (change & IFF_PROMISC)
		dev_set_promiscuity(slave_dev, (dev->flags & IFF_PROMISC) ?
				    1 : -1);
}

static void ubr_set_rx_mode(struct net_device *dev)
{
	struct ubr_private *master_info = netdev_priv(dev);
	struct net_device *slave_dev = master_info->slave_dev;

	if (!slave_dev)
		return;

#ifdef UBR_UC_SYNC
	dev_uc_sync(slave_dev, dev);
#endif
	dev_mc_sync(slave_dev, dev);
}

static unsigned int get_max_headroom(const struct ubr_private *ubr)
{
	struct net_device *slave_dev = ubr->slave_dev;
	unsigned int max_headroom = 0;

	if (slave_dev) {
		unsigned int dev_headroom = netdev_get_fwd_headroom(slave_dev);

		if (dev_headroom > max_headroom)
			max_headroom = dev_headroom;
	}

	return max_headroom;
}

static void update_headroom(const struct ubr_private *ubr, int new_hr)
{
	struct net_device *slave_dev = ubr->slave_dev;

	if (slave_dev)
		netdev_set_rx_headroom(slave_dev, new_hr);

	ubr->dev->needed_headroom = new_hr;
}

/* MTU of the bridge pseudo-device: ETH_DATA_LEN or the minimum of the slave */
static int ubr_min_mtu(const struct ubr_private *ubr)
{
	int mtu = ETH_DATA_LEN;
	const struct net_device *slave_dev = ubr->slave_dev;

	if (slave_dev && slave_dev->mtu < mtu)
		mtu = slave_dev->mtu;

	return mtu;
}

static void ubr_set_gso_limits(const struct ubr_private *ubr)
{
	unsigned int gso_max_size = GSO_MAX_SIZE;
	u16 gso_max_segs = GSO_MAX_SEGS;
	const struct net_device *slave_dev = ubr->slave_dev;

	if (slave_dev) {
		gso_max_size = min(gso_max_size, slave_dev->gso_max_size);
		gso_max_segs = min(gso_max_segs, slave_dev->gso_max_segs);
	}

	ubr->dev->gso_max_size = gso_max_size;
	ubr->dev->gso_max_segs = gso_max_segs;
}

static void
ubr_ethtool_proxy_get_drvinfo(struct net_device *netdev,
			      struct ethtool_drvinfo *info)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));

	if (slave_dev == NULL)
		return;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_drvinfo)
		return;

	ops->get_drvinfo(slave_dev, info);
}

static int
ubr_ethtool_proxy_get_settings(struct net_device *netdev,
			       struct ethtool_cmd *cmd)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_settings)
		return -EOPNOTSUPP;

	return ops->get_settings(slave_dev, cmd);
}

static int
ubr_ethtool_proxy_set_settings(struct net_device *netdev,
			       struct ethtool_cmd *cmd)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->set_settings)
		return -EOPNOTSUPP;

	return ops->set_settings(slave_dev, cmd);
}

static void
ubr_ethtool_proxy_get_wol(struct net_device *netdev,
			  struct ethtool_wolinfo *wol)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_wol)
		return;

	ops->get_wol(slave_dev, wol);
}

static int
ubr_ethtool_proxy_set_wol(struct net_device *netdev,
			  struct ethtool_wolinfo *wol)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->set_wol)
		return -EOPNOTSUPP;

	return ops->set_wol(slave_dev, wol);
}

static void
ubr_ethtool_proxy_get_ethtool_stats(struct net_device *netdev,
				    struct ethtool_stats *stats, u64 *data)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_ethtool_stats)
		return;

	ops->get_ethtool_stats(slave_dev, stats, data);
}

static int
ubr_ethtool_proxy_get_eee(struct net_device *netdev, struct ethtool_eee *edata)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_eee)
		return -EOPNOTSUPP;

	return ops->get_eee(slave_dev, edata);
}

static int
ubr_ethtool_proxy_set_eee(struct net_device *netdev, struct ethtool_eee *edata)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->set_eee)
		return -EOPNOTSUPP;

	return ops->set_eee(slave_dev, edata);
}

static int
ubr_ethtool_proxy_get_link_ksettings(struct net_device *netdev,
				     struct ethtool_link_ksettings *cmd)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_link_ksettings)
		return -EOPNOTSUPP;

	return ops->get_link_ksettings(slave_dev, cmd);
}

static int
ubr_ethtool_proxy_set_link_ksettings(struct net_device *netdev,
				     const struct ethtool_link_ksettings *cmd)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->set_link_ksettings)
		return -EOPNOTSUPP;

	return ops->set_link_ksettings(slave_dev, cmd);
}

static void
ubr_ethtool_proxy_get_pauseparam(struct net_device *netdev,
				 struct ethtool_pauseparam *pause)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return;

	ops = slave_dev->ethtool_ops;
	if (!ops->get_pauseparam)
		return;

	ops->get_pauseparam(slave_dev, pause);
}

static int
ubr_ethtool_proxy_set_pauseparam(struct net_device *netdev,
				 struct ethtool_pauseparam *pause)
{
	struct ubr_private *ubr = netdev_priv(netdev);
	struct net_device *slave_dev = ubr->slave_dev;
	const struct ethtool_ops *ops;

	if (slave_dev == NULL)
		return 0;

	ops = slave_dev->ethtool_ops;
	if (!ops->set_pauseparam)
		return -EOPNOTSUPP;

	return ops->set_pauseparam(slave_dev, pause);
}

static void
ubr_install_ethtool_hooks(struct net_device *master_dev)
{
	struct ethtool_ops *mops =
		(struct ethtool_ops *)master_dev->ethtool_ops;

	mops->get_drvinfo = ubr_ethtool_proxy_get_drvinfo;
	mops->get_settings = ubr_ethtool_proxy_get_settings;
	mops->set_settings = ubr_ethtool_proxy_set_settings;
	mops->get_wol = ubr_ethtool_proxy_get_wol;
	mops->set_wol = ubr_ethtool_proxy_set_wol;
	mops->get_ethtool_stats = ubr_ethtool_proxy_get_ethtool_stats;
	mops->get_eee = ubr_ethtool_proxy_get_eee;
	mops->set_eee = ubr_ethtool_proxy_set_eee;
	mops->get_link_ksettings = ubr_ethtool_proxy_get_link_ksettings;
	mops->set_link_ksettings = ubr_ethtool_proxy_set_link_ksettings;
	mops->get_pauseparam = ubr_ethtool_proxy_get_pauseparam;
	mops->set_pauseparam = ubr_ethtool_proxy_set_pauseparam;
}

static int
ubr_attach_if(struct net_device *master_dev, struct net_device *slave_dev)
{
	struct ubr_private *ubr = netdev_priv(master_dev);
	unsigned int br_hr, dev_hr;
	int err = -ENODEV;
	bool mac_differ = false;
	bool is_rawip = false;

	if (ubr->slave_dev != NULL)
		return -EBUSY;

	if (!slave_dev)
		goto out;

	/* Don't allow bridging loopback devices, or DSA-enabled
	 * master network devices since the bridge layer rx_handler prevents
	 * the DSA fake ethertype handler to be invoked, so we do not strip off
	 * the DSA switch tag protocol header and the bridge layer just return
	 * RX_HANDLER_CONSUMED, stopping RX processing for these frames.
	 */
	if ((slave_dev->flags & IFF_LOOPBACK) ||
	    netdev_uses_dsa(slave_dev))
		return -EINVAL;

	/* No bridging of bridges */
	if (slave_dev->netdev_ops->ndo_start_xmit == ubr_xmit)
		return -ELOOP;

	/* Device has master upper dev */
	if (netdev_master_upper_dev_get(slave_dev))
		return -EBUSY;

	/* No bridging devices that dislike that (e.g. wireless) */
	if (slave_dev->priv_flags & IFF_DONT_BRIDGE)
		return -EOPNOTSUPP;

	if (is_netdev_rawip(slave_dev))
		is_rawip = true;

	if (!is_rawip) {
		if (!test_bit(MAC_FORCED, &ubr->flags)) {
			struct sockaddr addr;

			ether_addr_copy(addr.sa_data, slave_dev->dev_addr);

			if (ubr_set_mac_addr(master_dev, &addr))
				pr_err("ubr_atto_master error setting MAC\n");
		}

		mac_differ = !ether_addr_equal(master_dev->dev_addr,
					       slave_dev->dev_addr);
	} else {
		master_dev->flags |= slave_dev->flags & IFF_POINTOPOINT;
	}

	call_netdevice_notifiers(NETDEV_JOIN, slave_dev);

	ubr->slave_dev = slave_dev;

	err = netdev_rx_handler_register(slave_dev, ubr_handle_frame, ubr);
	if (err)
		goto err1;

	slave_dev->priv_flags |= IFF_UBRIDGE_PORT;

	err = netdev_master_upper_dev_link(slave_dev, master_dev, NULL, NULL);
	if (err)
		goto err2;

	dev_disable_lro(slave_dev);

	if (!is_rawip) {
		if ((master_dev->flags & IFF_PROMISC) || mac_differ)
			dev_set_promiscuity(slave_dev, 1);

		if (master_dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(slave_dev, 1);
	}

	netdev_update_features(master_dev);

	br_hr = master_dev->needed_headroom;
	dev_hr = netdev_get_fwd_headroom(slave_dev);
	if (br_hr < dev_hr)
		update_headroom(ubr, dev_hr);
	else
		netdev_set_rx_headroom(slave_dev, br_hr);

	dev_set_mtu(master_dev, ubr_min_mtu(ubr));
	ubr_set_gso_limits(ubr);
	netif_carrier_on(master_dev);

	return 0;

err2:
	slave_dev->priv_flags &= ~IFF_UBRIDGE_PORT;
	netdev_rx_handler_unregister(slave_dev);

err1:
	ubr->slave_dev = NULL;

out:
	return err;
}

static void ubr_unreg_if(struct ubr_private *ubr)
{
	struct net_device *slave_dev = ubr->slave_dev;
	struct net_device *master_dev = ubr->dev;

	if (!slave_dev)
		return;

	netif_carrier_off(master_dev);

	netdev_upper_dev_unlink(slave_dev, master_dev);
	netdev_rx_handler_unregister(slave_dev);

	ubr->slave_dev = NULL;

	if (netdev_get_fwd_headroom(slave_dev) == master_dev->needed_headroom)
		update_headroom(ubr, get_max_headroom(ubr));
	dev_set_mtu(master_dev, ubr_min_mtu(ubr));
	ubr_set_gso_limits(ubr);

	master_dev->flags &= ~IFF_POINTOPOINT;

	netdev_update_features(master_dev);
}

static int
ubr_detach_if(struct net_device *master_dev, struct net_device *slave_dev)
{
	struct ubr_private *ubr = netdev_priv(master_dev);
	int err = -EINVAL;

	if (!slave_dev)
		goto out;

	if (ubr->slave_dev != slave_dev)
		goto out;

	ubr_unreg_if(ubr);

	netdev_reset_rx_headroom(slave_dev);

	if (!is_netdev_rawip(slave_dev)) {
		if (master_dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(slave_dev, -1);

		if (master_dev->flags & IFF_PROMISC)
			dev_set_promiscuity(slave_dev, -1);
	}

	slave_dev->priv_flags &= ~IFF_UBRIDGE_PORT;

	err = 0;

out:
	return err;
}

/* called with RTNL */
static int ubr_add_del_if(struct net_device *dev, int ifindex, int is_add)
{
	struct net *net = dev_net(dev);
	struct net_device *slave_dev;
	int ret;

	if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
		return -EPERM;

	slave_dev = __dev_get_by_index(net, ifindex);
	if (slave_dev == NULL)
		return -EINVAL;

	if (is_add)
		ret = ubr_attach_if(dev, slave_dev);
	else
		ret = ubr_detach_if(dev, slave_dev);

	return ret;
}

static int ubr_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	switch (cmd) {
	case SIOCBRADDIF:
	case SIOCBRDELIF:
		return ubr_add_del_if(dev, rq->ifr_ifindex, cmd == SIOCBRADDIF);
	}

	return -EOPNOTSUPP;
}

static const struct net_device_ops ubr_netdev_ops = {
	.ndo_init		 = ubr_init,
	.ndo_open		 = ubr_open,
	.ndo_stop		 = ubr_stop,
	.ndo_start_xmit		 = ubr_xmit,
	.ndo_get_stats64	 = ubr_get_stats64,
	.ndo_do_ioctl		 = ubr_dev_ioctl,
	.ndo_change_rx_flags	 = ubr_change_rx_flags,
	.ndo_set_rx_mode	 = ubr_set_rx_mode,
	.ndo_set_mac_address	 = ubr_set_mac_addr_force,
	.ndo_add_slave		 = ubr_attach_if,
	.ndo_del_slave		 = ubr_detach_if,
};

static void ubr_dev_free(struct net_device *dev)
{
	struct ubr_private *ubr = netdev_priv(dev);

	free_percpu(ubr->stats);
	free_netdev(dev);
}

/* RTNL locked */
static int ubr_deregister(struct net_device *dev)
{
	struct ubr_private *ubr = netdev_priv(dev);

	dev_close(dev);

	if (ubr) {
		if (!list_empty(&ubr->list))
			list_del(&ubr->list);
		if (ubr->slave_dev) {
			netdev_upper_dev_unlink(ubr->slave_dev, dev);
			netdev_rx_handler_unregister(ubr->slave_dev);
		}
	}

	unregister_netdevice(dev);

	return 0;
}

static int ubr_free_master(struct net *net, const char *name)
{
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();
	dev = __dev_get_by_name(net, name);
	if (dev == NULL)
		ret =  -ENXIO; /* Could not find device */
	else if (dev->flags & IFF_UP)
		/* Not shutdown yet. */
		ret = -EBUSY;
	else
		ret = ubr_deregister(dev);
	rtnl_unlock();

	return ret;
}

static struct device_type ubr_type = {
	.name	= "ubridge",
};

static void ubr_dev_setup(struct net_device *dev)
{
	struct ubr_private *ubr = netdev_priv(dev);

	eth_hw_addr_random(dev);
	ether_setup(dev);

	dev->netdev_ops = &ubr_netdev_ops;
	dev->destructor = ubr_dev_free;
	SET_NETDEV_DEVTYPE(dev, &ubr_type);
	dev->priv_flags		|= IFF_UBRIDGE | IFF_NO_QUEUE;

	dev->features = COMMON_FEATURES | NETIF_F_LLTX | NETIF_F_NETNS_LOCAL |
			NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_STAG_TX;
	dev->hw_features = COMMON_FEATURES | NETIF_F_HW_VLAN_CTAG_TX |
			   NETIF_F_HW_VLAN_STAG_TX;
	dev->vlan_features = COMMON_FEATURES;

	ubr->dev = dev;

	INIT_LIST_HEAD(&ubr->list);
}

static int ubr_alloc_master(struct net *net, const char *name)
{
	struct net_device *dev;
	struct ubr_private *ubr;
	int err = 0;

	dev = alloc_netdev(sizeof(struct ubr_private), name, NET_NAME_UNKNOWN,
			   ubr_dev_setup);
	if (!dev)
		return -ENOMEM;

	dev_net_set(dev, net);

	err = register_netdev(dev);
	if (err) {
		free_netdev(dev);
		goto out;
	}

	netif_carrier_off(dev);

	ubr = netdev_priv(dev);

	rtnl_lock();
	list_add(&ubr->list, &ubr_list);
	rtnl_unlock();

	dev->ethtool_ops = &ubr->ethtool_ops;
	ubr_install_ethtool_hooks(dev);

out:
	return err;
}

#define SHOW_BUF_MAX_LEN	4096

static long ubr_show(char *buf, long len)
{
	long written = 0;
	struct ubr_private *ubr_item;

	if (len == 0 || len > SHOW_BUF_MAX_LEN)
		len = SHOW_BUF_MAX_LEN;

	list_for_each_entry(ubr_item, &ubr_list, list) {
		written += snprintf(buf + written, len - written,
				"%-16s %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\t",
				ubr_item->dev->name,
				ubr_item->dev->dev_addr[0],
				ubr_item->dev->dev_addr[1],
				ubr_item->dev->dev_addr[2],
				ubr_item->dev->dev_addr[3],
				ubr_item->dev->dev_addr[4],
				ubr_item->dev->dev_addr[5]);
		if (written >= len - 2)
			break;

		if (ubr_item->slave_dev == NULL)
			written += sprintf(buf + written, "-\n");
		else
			written += snprintf(buf + written, len - written,
					   "%s\n", ubr_item->slave_dev->name);
		if (written >= len - 1)
			break;
	}

	return written;
}
int
ubr_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *uarg)
{
	char buf[IFNAMSIZ];

	switch (cmd) {
	case SIOCUBRADDBR:
	case SIOCUBRDELBR:
		if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, uarg, IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ - 1] = 0;
		if (cmd == SIOCUBRADDBR)
			return ubr_alloc_master(net, buf);

		return ubr_free_master(net, buf);
	case SIOCUBRSHOW:
		{
			char *buf_;
			long res;
			struct {
				long len;
				char *buf;
			} args;

			if (copy_from_user(&args, uarg, sizeof(args)))
				return -EFAULT;
			buf_ = kzalloc(SHOW_BUF_MAX_LEN, GFP_KERNEL);
			if (buf_ == NULL)
				return -ENOMEM;
			res = ubr_show(buf_, args.len);
			if (copy_to_user(args.buf, buf_, res) ||
			    copy_to_user(uarg, &res, sizeof(long))) {
				kfree(buf_);
				return -EFAULT;
			}
			kfree(buf_);
			return 0;
		}
	}
	return -EOPNOTSUPP;
}

static int ubr_dev_event(struct notifier_block *unused,
			 unsigned long event,
			 void *ptr)
{
	struct net_device *pdev = netdev_notifier_info_to_dev(ptr);
	struct ubr_private *ubr;

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;

	rcu_read_lock();
	if (rcu_access_pointer(pdev->rx_handler) != ubr_handle_frame) {
		rcu_read_unlock();
		return NOTIFY_DONE;
	}

	ubr = ubr_priv_get_rcu(pdev);
	if (ubr)
		ubr_unreg_if(ubr);

	rcu_read_unlock();
	return NOTIFY_DONE;
}

static struct notifier_block ubr_device_notifier = {
	.notifier_call  = ubr_dev_event,
};

static int __init ubridge_init(void)
{
	pr_info("ubridge: %s, %s\n", DRV_DESCRIPTION, DRV_VERSION);

	ubrioctl_set(ubr_ioctl_deviceless_stub);
	if (register_netdevice_notifier(&ubr_device_notifier))
		pr_err("%s: Error registering notifier\n", __func__);

	return 0;
}

static void __exit ubridge_exit(void)
{
	struct ubr_private *ubr, *tmp;

	unregister_netdevice_notifier(&ubr_device_notifier);
	ubrioctl_set(NULL);
	rtnl_lock();
	list_for_each_entry_safe(ubr, tmp, &ubr_list, list)
		ubr_deregister(ubr->dev);
	rtnl_unlock();

	pr_info("ubridge: driver unloaded\n");
}

module_init(ubridge_init);
module_exit(ubridge_exit);

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_LICENSE("GPL");
