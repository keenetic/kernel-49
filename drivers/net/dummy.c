/* dummy.c: a dummy net driver

	The purpose of this driver is to provide a device to point a
	route through, but not to actually transmit packets.

	Why?  If you have a machine whose only connection is an occasional
	PPP/SLIP/PLIP link, you can only connect to your own hostname
	when the link is up.  Otherwise you have to use localhost.
	This isn't very consistent.

	One solution is to set up a dummy link using PPP/SLIP/PLIP,
	but this seems (to me) too much overhead for too little gain.
	This driver provides a small alternative. Thus you can do

	[when not running slip]
		ifconfig dummy slip.addr.ess.here up
	[to go to slip]
		ifconfig dummy down
		dip whatever

	This was written by looking at Donald Becker's skeleton driver
	and the loopback driver.  I then threw away anything that didn't
	apply!	Thanks to Alan Cox for the key clue on what to do with
	misguided packets.

			Nick Holloway, 27th May 1994
	[I tweaked this explanation a little but that's all]
			Alan Cox, 30th May 1994
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>
#include <linux/u64_stats_sync.h>

#include <linux/ntc_shaper_hooks.h>

#define DRV_NAME	"dummy"
#define DRV_VERSION	"1.0"

static int numdummies = 1;
static int pass_ntc = 0;

/* fake multicast ability */
static void set_multicast_list(struct net_device *dev)
{
}

struct pcpu_dstats {
	u64			tx_packets;
	u64			tx_bytes;
	struct u64_stats_sync	syncp;
};

static struct rtnl_link_stats64 *dummy_get_stats64(struct net_device *dev,
						   struct rtnl_link_stats64 *stats)
{
	int i;

	for_each_possible_cpu(i) {
		const struct pcpu_dstats *dstats;
		u64 tbytes, tpackets;
		unsigned int start;

		dstats = per_cpu_ptr(dev->dstats, i);
		do {
			start = u64_stats_fetch_begin_irq(&dstats->syncp);
			tbytes = dstats->tx_bytes;
			tpackets = dstats->tx_packets;
		} while (u64_stats_fetch_retry_irq(&dstats->syncp, start));
		stats->tx_bytes += tbytes;
		stats->tx_packets += tpackets;
	}
	return stats;
}

static inline void
dummy_ntc__(ntc_shaper_hook_fn *fn,
	    struct ntc_shaper_fwd_t *fwd,
	    struct sk_buff *skb,
	    const size_t req_len)
{
	const size_t len = skb_tail_pointer(skb) - skb_network_header(skb);

	if (req_len > len) {
		if (skb_tailroom(skb) < req_len) {
			struct sk_buff *skb2 = skb_copy_expand(skb, 0,
							       req_len,
							       GFP_ATOMIC);

			dev_kfree_skb(skb);

			if (skb2 == NULL)
				return;

			skb = skb2;
		}

		skb_put(skb, req_len);
	}

	if (fn(skb, fwd) != NF_STOLEN)
		dev_kfree_skb(skb);
}

static inline void
dummy_ntc_ip4(ntc_shaper_hook_fn *fn,
	      struct ntc_shaper_fwd_t *fwd,
	      struct sk_buff *skb)
{
	struct iphdr *iph = (struct iphdr *)skb_network_header(skb);

	if (iph->version != 4 ||
	    (iph->protocol != IPPROTO_TCP &&
	     iph->protocol != IPPROTO_UDP)) {
		dev_kfree_skb(skb);

		return;
	}

	fwd->is_ipv4 = true;

	dummy_ntc__(fn, fwd, skb, ntohs(iph->tot_len));
}

static inline void
dummy_ntc_ip6(ntc_shaper_hook_fn *fn,
	      struct ntc_shaper_fwd_t *fwd,
	      struct sk_buff *skb)
{
	struct ipv6hdr *ip6 = (struct ipv6hdr *)skb_network_header(skb);

	if (ip6->version != 6 ||
	    (ip6->nexthdr != IPPROTO_TCP &&
	     ip6->nexthdr != IPPROTO_UDP)) {
		dev_kfree_skb(skb);

		return;
	}

	fwd->is_ipv4 = false;

	dummy_ntc__(fn, fwd, skb, ntohs(ip6->payload_len));
}

static inline void
dummy_ntc_(ntc_shaper_hook_fn *fn, struct sk_buff *skb)
{
	struct ntc_shaper_fwd_t fwd = {
		.okfn		= NULL,
		.net		= NULL,
		.sk		= NULL,
		.is_ipv4	= true,
		.is_swnat	= false
	};
	const __be16 proto = eth_hdr(skb)->h_proto;

	if (proto == htons(ETH_P_IP)) {
		dummy_ntc_ip4(fn, &fwd, skb);

		return;
	}

	if (proto == htons(ETH_P_IPV6)) {
		dummy_ntc_ip6(fn, &fwd, skb);

		return;
	}

	dev_kfree_skb(skb);
}

static inline void
dummy_ntc(struct sk_buff *skb)
{
	ntc_shaper_hook_fn *fn = ntc_shaper_test_hook_get();

	if (fn == NULL) {
		ntc_shaper_test_hook_put();
		dev_kfree_skb(skb);

		return;
	}

	dummy_ntc_(fn, skb);

	ntc_shaper_test_hook_put();
}

static netdev_tx_t dummy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct pcpu_dstats *dstats = this_cpu_ptr(dev->dstats);

	u64_stats_update_begin(&dstats->syncp);
	dstats->tx_packets++;
	dstats->tx_bytes += skb->len;
	u64_stats_update_end(&dstats->syncp);

	if (pass_ntc)
		dummy_ntc(skb);
	else
		dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static int dummy_dev_init(struct net_device *dev)
{
	dev->dstats = netdev_alloc_pcpu_stats(struct pcpu_dstats);
	if (!dev->dstats)
		return -ENOMEM;

	return 0;
}

static void dummy_dev_uninit(struct net_device *dev)
{
	free_percpu(dev->dstats);
}

static int dummy_change_carrier(struct net_device *dev, bool new_carrier)
{
	if (new_carrier)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	return 0;
}

static const struct net_device_ops dummy_netdev_ops = {
	.ndo_init		= dummy_dev_init,
	.ndo_uninit		= dummy_dev_uninit,
	.ndo_start_xmit		= dummy_xmit,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_get_stats64	= dummy_get_stats64,
	.ndo_change_carrier	= dummy_change_carrier,
};

static void dummy_get_drvinfo(struct net_device *dev,
			      struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
}

static const struct ethtool_ops dummy_ethtool_ops = {
	.get_drvinfo            = dummy_get_drvinfo,
};

static void dummy_setup(struct net_device *dev)
{
	ether_setup(dev);

	/* Initialize the device structure. */
	dev->netdev_ops = &dummy_netdev_ops;
	dev->ethtool_ops = &dummy_ethtool_ops;
	dev->destructor = free_netdev;

	/* Fill in device structure with ethernet-generic values. */
	dev->flags |= IFF_NOARP;
	dev->flags &= ~IFF_MULTICAST;
	dev->priv_flags |= IFF_LIVE_ADDR_CHANGE | IFF_NO_QUEUE;
	dev->features	|= NETIF_F_SG | NETIF_F_FRAGLIST;
	dev->features	|= NETIF_F_ALL_TSO;
	dev->features	|= NETIF_F_HW_CSUM | NETIF_F_HIGHDMA | NETIF_F_LLTX;
	dev->features	|= NETIF_F_GSO_ENCAP_ALL;
	dev->hw_features |= dev->features;
	dev->hw_enc_features |= dev->features;
	eth_hw_addr_random(dev);
}

static int dummy_validate(struct nlattr *tb[], struct nlattr *data[])
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}

static struct rtnl_link_ops dummy_link_ops __read_mostly = {
	.kind		= DRV_NAME,
	.setup		= dummy_setup,
	.validate	= dummy_validate,
};

/* Number of dummy devices to be set up by this module. */
module_param(numdummies, int, 0);
MODULE_PARM_DESC(numdummies, "Number of dummy pseudo devices");
module_param(pass_ntc, int, 0);
MODULE_PARM_DESC(pass_ntc, "Enable NTC test target bypass");

static int __init dummy_init_one(void)
{
	struct net_device *dev_dummy;
	int err;

	dev_dummy = alloc_netdev(0, "dummy%d", NET_NAME_UNKNOWN, dummy_setup);
	if (!dev_dummy)
		return -ENOMEM;

	dev_dummy->rtnl_link_ops = &dummy_link_ops;
	err = register_netdevice(dev_dummy);
	if (err < 0)
		goto err;
	return 0;

err:
	free_netdev(dev_dummy);
	return err;
}

static int __init dummy_init_module(void)
{
	int i, err = 0;

	rtnl_lock();
	err = __rtnl_link_register(&dummy_link_ops);
	if (err < 0)
		goto out;

	for (i = 0; i < numdummies && !err; i++) {
		err = dummy_init_one();
		cond_resched();
	}
	if (err < 0)
		__rtnl_link_unregister(&dummy_link_ops);

out:
	rtnl_unlock();

	return err;
}

static void __exit dummy_cleanup_module(void)
{
	rtnl_link_unregister(&dummy_link_ops);
}

module_init(dummy_init_module);
module_exit(dummy_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);
MODULE_VERSION(DRV_VERSION);
