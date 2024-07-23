/*
 *	Spanning tree protocol; BPDU handling
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netfilter_bridge.h>
#include <linux/etherdevice.h>
#include <linux/llc.h>
#include <linux/slab.h>
#include <linux/pkt_sched.h>
#include <linux/inetdevice.h>
#include <net/net_namespace.h>
#include <net/llc.h>
#include <net/llc_pdu.h>
#include <net/stp.h>
#include <net/gre.h>
#include <asm/unaligned.h>
#include <net/addrconf.h>

#include "br_private.h"
#include "br_private_stp.h"
#include "br_nf_hook.h"

#define STP_HZ		256

#define LLC_RESERVE sizeof(struct llc_pdu_un)

static int br_send_bpdu_finish(struct net *net, struct sock *sk,
			       struct sk_buff *skb)
{
	return dev_queue_xmit(skb);
}

struct stp_encap {
	uint32_t tag;
};

#define ENCAP_TAG	0x4e444d53 /* ASCII 'NDMS' */
#define ETH_P_STP	0x8181

#define ENCAP_RESERVE (sizeof(struct stp_encap) +\
		       sizeof(struct gre_base_hdr) +\
		       sizeof(struct ipv6hdr) +\
		       VLAN_ETH_HLEN)

static void br_send_bpdu_encap_dest(struct net_device* dev,
				    struct ipv6hdr* ip6)
{
	struct inet6_ifaddr *ifa;
	struct inet6_dev *idev6 = __in6_dev_get(dev);

	if (unlikely(!idev6))
		return;

	list_for_each_entry_rcu(ifa, &idev6->addr_list, if_list) {
		if (ifa->flags & (IFA_F_TENTATIVE |
				  IFA_F_DEPRECATED))
			continue;

		if (!(ipv6_addr_type(&ifa->addr) & IPV6_ADDR_LINKLOCAL))
			continue;

		memcpy(ip6->saddr.s6_addr, ifa->addr.s6_addr, 16);
		return;
	}

	list_for_each_entry(ifa, &idev6->addr_list, if_list) {
		if (ifa->flags & (IFA_F_TENTATIVE |
				  IFA_F_DEPRECATED))
			continue;

		memcpy(ip6->saddr.s6_addr + 8, ifa->addr.s6_addr + 8, 8);
		return;
	}
}

static void br_send_bpdu_encap(struct net_bridge_port *p,
			       const unsigned char *data, int length)
{
	struct sk_buff *skb = dev_alloc_skb(length + ENCAP_RESERVE);
	struct stp_encap se;
	struct gre_base_hdr greh;
	struct ipv6hdr ip6;

	if (!skb)
		return;

	skb->dev = p->dev;
	skb->priority = TC_PRIO_CONTROL;

	skb_reserve(skb, ENCAP_RESERVE);
	memcpy(__skb_put(skb, length), data, length);

	if (p->br->stp_log == BR_LOG_STP_ENABLE)
		br_stp_bdpu_send_print_encap(p->br, p->dev, skb);

	se.tag = ENCAP_TAG;

	memcpy(__skb_push(skb, sizeof(se)), &se, sizeof(se));

	memset(&greh, 0, sizeof(greh));
	greh.flags = htons(GREPROTO_NDM);
	greh.protocol = htons(ETH_P_STP);

	memcpy(__skb_push(skb, sizeof(greh)), &greh, sizeof(greh));
	skb_reset_transport_header(skb);

	memset(&ip6, 0, sizeof(ip6));
	ip6.version = 6;
	ip6.nexthdr = IPPROTO_GRE;

	ip6.saddr = in6addr_linklocal_allnodes;
	ip6.daddr = in6addr_linklocal_allnodes;

	rcu_read_lock();

	br_send_bpdu_encap_dest(p->br->dev, &ip6);

	rcu_read_unlock();

	ip6.hop_limit = 1;
	ip6.payload_len = htons(length + sizeof(se) + sizeof(greh));

	memcpy(__skb_push(skb, sizeof(ip6)), &ip6, sizeof(ip6));
	skb_reset_network_header(skb);

	if (is_vlan_dev(p->dev) && vlan_dev_vlan_id(p->dev) > 1) {
		struct vlan_ethhdr eth;

		memset(&eth, 0, sizeof(eth));
		memcpy(&eth.h_source, p->dev->dev_addr, sizeof(eth.h_source));
		ipv6_eth_mc_map(&ip6.daddr, eth.h_dest);
		eth.h_vlan_proto = vlan_dev_vlan_proto(p->dev);
		eth.h_vlan_TCI = htons(vlan_dev_vlan_id(p->dev));
		eth.h_vlan_encapsulated_proto = htons(ETH_P_IPV6);

		memcpy(__skb_push(skb, sizeof(eth)), &eth, sizeof(eth));
		skb_reset_mac_header(skb);

		skb->protocol = htons(ETH_P_8021Q);

	} else {
		struct ethhdr eth;

		memset(&eth, 0, sizeof(eth));
		memcpy(&eth.h_source, p->dev->dev_addr, sizeof(eth.h_source));
		ipv6_eth_mc_map(&ip6.daddr, eth.h_dest);
		eth.h_proto = htons(ETH_P_IPV6);

		memcpy(__skb_push(skb, sizeof(eth)), &eth, sizeof(eth));
		skb_reset_mac_header(skb);

		skb->protocol = htons(ETH_P_IPV6);
	}

	BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT,
		dev_net(p->dev), NULL, skb, NULL, skb->dev,
		br_send_bpdu_finish);
}

static void br_send_bpdu(struct net_bridge_port *p,
			 const unsigned char *data, int length)
{
	struct sk_buff *skb;

	if (p->stp_choke == BR_PORT_STP_CHOKE)
		return;

	if (p->br->stp_encap == BR_STP_ENCAP) {
		br_send_bpdu_encap(p, data, length);
		return;
	}

	skb = dev_alloc_skb(length+LLC_RESERVE);
	if (!skb)
		return;

	skb->dev = p->dev;
	skb->protocol = htons(ETH_P_802_2);
	skb->priority = TC_PRIO_CONTROL;

	skb_reserve(skb, LLC_RESERVE);
	memcpy(__skb_put(skb, length), data, length);

	if (p->br->stp_log == BR_LOG_STP_ENABLE)
		br_stp_bdpu_send_print(p->br, p->dev, skb);

	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, LLC_SAP_BSPAN,
			    LLC_SAP_BSPAN, LLC_PDU_CMD);
	llc_pdu_init_as_ui_cmd(skb);

	llc_mac_hdr_init(skb, p->dev->dev_addr, p->br->group_addr);

	skb_reset_mac_header(skb);

	BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT,
		dev_net(p->dev), NULL, skb, NULL, skb->dev,
		br_send_bpdu_finish);
}

static inline void br_set_ticks(unsigned char *dest, int j)
{
	unsigned long ticks = (STP_HZ * j)/ HZ;

	put_unaligned_be16(ticks, dest);
}

static inline int br_get_ticks(const unsigned char *src)
{
	unsigned long ticks = get_unaligned_be16(src);

	return DIV_ROUND_UP(ticks * HZ, STP_HZ);
}

/* called under bridge lock */
void br_send_config_bpdu(struct net_bridge_port *p, struct br_config_bpdu *bpdu)
{
	unsigned char buf[35];

	if (p->br->stp_enabled != BR_KERNEL_STP)
		return;

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = BPDU_TYPE_CONFIG;
	buf[4] = (bpdu->topology_change ? 0x01 : 0) |
		(bpdu->topology_change_ack ? 0x80 : 0);
	buf[5] = bpdu->root.prio[0];
	buf[6] = bpdu->root.prio[1];
	buf[7] = bpdu->root.addr[0];
	buf[8] = bpdu->root.addr[1];
	buf[9] = bpdu->root.addr[2];
	buf[10] = bpdu->root.addr[3];
	buf[11] = bpdu->root.addr[4];
	buf[12] = bpdu->root.addr[5];
	buf[13] = (bpdu->root_path_cost >> 24) & 0xFF;
	buf[14] = (bpdu->root_path_cost >> 16) & 0xFF;
	buf[15] = (bpdu->root_path_cost >> 8) & 0xFF;
	buf[16] = bpdu->root_path_cost & 0xFF;
	buf[17] = bpdu->bridge_id.prio[0];
	buf[18] = bpdu->bridge_id.prio[1];
	buf[19] = bpdu->bridge_id.addr[0];
	buf[20] = bpdu->bridge_id.addr[1];
	buf[21] = bpdu->bridge_id.addr[2];
	buf[22] = bpdu->bridge_id.addr[3];
	buf[23] = bpdu->bridge_id.addr[4];
	buf[24] = bpdu->bridge_id.addr[5];
	buf[25] = (bpdu->port_id >> 8) & 0xFF;
	buf[26] = bpdu->port_id & 0xFF;

	br_set_ticks(buf+27, bpdu->message_age);
	br_set_ticks(buf+29, bpdu->max_age);
	br_set_ticks(buf+31, bpdu->hello_time);
	br_set_ticks(buf+33, bpdu->forward_delay);

	br_send_bpdu(p, buf, 35);
}

/* called under bridge lock */
void br_send_tcn_bpdu(struct net_bridge_port *p)
{
	unsigned char buf[4];

	if (p->br->stp_enabled != BR_KERNEL_STP)
		return;

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = BPDU_TYPE_TCN;
	br_send_bpdu(p, buf, 4);
}

/*
 * Called from llc.
 *
 * NO locks, but rcu_read_lock
 */
static void br_stp_rcv_(const struct stp_proto *proto, struct sk_buff *skb,
			struct net_device *dev, const bool encap)
{
	struct net_bridge_port *p;
	struct net_bridge *br;
	const unsigned char *buf;

	if (!pskb_may_pull(skb, 4))
		goto err;

	/* compare of protocol id and version */
	buf = skb->data;
	if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0)
		goto err;

	p = br_port_get_check_rcu(dev);
	if (!p)
		goto err;

	br = p->br;
	spin_lock(&br->lock);

	if (br->stp_enabled != BR_KERNEL_STP)
		goto out;

	if (!(br->dev->flags & IFF_UP))
		goto out;

	if (p->state == BR_STATE_DISABLED)
		goto out;

	if (!encap && !ether_addr_equal(eth_hdr(skb)->h_dest, br->group_addr))
		goto out;

	if (p->flags & BR_BPDU_GUARD) {
		br_notice(br, "BPDU received on blocked port %u(%s)\n",
			  (unsigned int) p->port_no, p->dev->name);
		br_stp_disable_port(p);
		goto out;
	}

	if (p->stp_choke == BR_PORT_STP_CHOKE)
		goto out;

	if (br->stp_log == BR_LOG_STP_ENABLE) {
		if (encap)
			br_stp_bdpu_recv_print_encap(br, dev, skb);
		else
			br_stp_bdpu_recv_print(br, dev, skb);
	}

	buf = skb_pull(skb, 3);

	if (buf[0] == BPDU_TYPE_CONFIG) {
		struct br_config_bpdu bpdu;

		if (!pskb_may_pull(skb, 32))
			goto out;

		buf = skb->data;
		bpdu.topology_change = (buf[1] & 0x01) ? 1 : 0;
		bpdu.topology_change_ack = (buf[1] & 0x80) ? 1 : 0;

		bpdu.root.prio[0] = buf[2];
		bpdu.root.prio[1] = buf[3];
		bpdu.root.addr[0] = buf[4];
		bpdu.root.addr[1] = buf[5];
		bpdu.root.addr[2] = buf[6];
		bpdu.root.addr[3] = buf[7];
		bpdu.root.addr[4] = buf[8];
		bpdu.root.addr[5] = buf[9];
		bpdu.root_path_cost =
			(buf[10] << 24) |
			(buf[11] << 16) |
			(buf[12] << 8) |
			buf[13];
		bpdu.bridge_id.prio[0] = buf[14];
		bpdu.bridge_id.prio[1] = buf[15];
		bpdu.bridge_id.addr[0] = buf[16];
		bpdu.bridge_id.addr[1] = buf[17];
		bpdu.bridge_id.addr[2] = buf[18];
		bpdu.bridge_id.addr[3] = buf[19];
		bpdu.bridge_id.addr[4] = buf[20];
		bpdu.bridge_id.addr[5] = buf[21];
		bpdu.port_id = (buf[22] << 8) | buf[23];

		bpdu.message_age = br_get_ticks(buf+24);
		bpdu.max_age = br_get_ticks(buf+26);
		bpdu.hello_time = br_get_ticks(buf+28);
		bpdu.forward_delay = br_get_ticks(buf+30);

		if (bpdu.message_age > bpdu.max_age) {
			if (net_ratelimit())
				br_notice(p->br,
					  "port %u config from %pM"
					  " (message_age %ul > max_age %ul)\n",
					  p->port_no,
					  eth_hdr(skb)->h_source,
					  bpdu.message_age, bpdu.max_age);
			goto out;
		}

		br_received_config_bpdu(p, &bpdu, eth_hdr(skb)->h_source);
	} else if (buf[0] == BPDU_TYPE_TCN) {
		br_received_tcn_bpdu(p);
	}
 out:
	spin_unlock(&br->lock);
 err:
	kfree_skb(skb);
}

void br_stp_rcv(const struct stp_proto *proto, struct sk_buff *skb,
		struct net_device *dev)
{
	struct net_bridge_port *p;
	struct net_bridge *br;

	if (skb->dev == NULL)
		return;

	p = br_port_get_check_rcu(skb->dev);
	if (!p)
		return;

	br = p->br;
	spin_lock(&br->lock);

	if (br->stp_encap != BR_STP_ENCAP) {
		spin_unlock(&br->lock);

		return;
	}

	spin_unlock(&br->lock);

	br_stp_rcv_(proto, skb, dev, false);
}

static int br_stp_encap_gre_rcv_(struct sk_buff *skb)
{
	struct stp_encap *se;
	struct gre_base_hdr *greh;

	if (!pskb_may_pull(skb, sizeof(*greh)))
		goto drop;

	greh = (struct gre_base_hdr *)skb->data;

	if (greh->flags != htons(GREPROTO_NDM) ||
	    greh->protocol != htons(ETH_P_STP))
		goto drop;

	skb_pull(skb, sizeof(*greh));

	if (!pskb_may_pull(skb, sizeof(*se)))
		goto drop;

	se = (struct stp_encap *)skb->data;

	if (se->tag != ENCAP_TAG)
		goto drop;

	skb_pull(skb, sizeof(*se));

	br_stp_rcv_(NULL, skb, skb->dev, true);

	return 0;

drop:
	kfree_skb(skb);
	return 0;
}

static bool br_stp_is_encap(struct net_bridge *br)
{
	spin_lock(&br->lock);

	if (br->stp_encap != BR_STP_ENCAP) {
		spin_unlock(&br->lock);

		return false;
	}

	spin_unlock(&br->lock);

	return true;
}

int br_stp_encap_gre_rcv(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;

	if (dev == NULL)
		goto drop;

	if (!(dev->priv_flags & IFF_EBRIDGE)) {
		struct net_bridge_port *p;

		if (rcu_access_pointer(dev->rx_handler) != br_handle_frame)
			goto drop;

		p = br_port_get_rcu(dev);

		if (p == NULL || !br_stp_is_encap(p->br))
			goto drop;

	} else if (!br_stp_is_encap(netdev_priv(dev)))
		goto drop;

	return br_stp_encap_gre_rcv_(skb);

drop:
	kfree_skb(skb);
	return 0;
}
