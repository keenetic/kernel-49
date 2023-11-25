/*
 *	Handle incoming frames
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

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/neighbour.h>
#include <net/arp.h>
#include <linux/export.h>
#include <linux/rculist.h>
#include "br_private.h"
#include "br_nf_hook.h"

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/fast_vpn.h>
#endif

#if !defined(ETH_P_LLDP)
#define ETH_P_LLDP					0x88cc
#endif /* ETH_P_LLDP */

struct br_lldpdu_hdr {
	u8 tlv_type;
	u8 tlv_length;
	u8 sub_type;
	u8 mac[ETH_ALEN];
} __attribute__ ((__packed__));

static inline bool
br_is_own_lldpdu(struct net_bridge *br, struct sk_buff *skb)
{
	struct br_lldpdu_hdr *lldpdu, _lldpdu;

	if (skb->protocol != ntohs(ETH_P_LLDP))
		return false;

	if (skb->len <= sizeof(*lldpdu))
		return false;

	lldpdu = skb_header_pointer(skb, 0, sizeof(*lldpdu), &_lldpdu);

	if (lldpdu == NULL)
		return false;

	/* Chassis */
	if (lldpdu->tlv_type != 2)
		return false;

	/* Payload is a MAC */
	if (lldpdu->sub_type != 4)
		return false;

	return ether_addr_equal(br->dev->dev_addr, lldpdu->mac);
}

#if IS_ENABLED(CONFIG_BRIDGE_EBT_BROUTE)
/* Hook for brouter */
br_should_route_hook_t __rcu *br_should_route_hook __read_mostly;
EXPORT_SYMBOL(br_should_route_hook);
#endif

#if IS_ENABLED(CONFIG_BRIDGE_NF_EBTABLES)
int brnf_call_ebtables __read_mostly;
EXPORT_SYMBOL_GPL(brnf_call_ebtables);
#endif

static int
br_netif_receive_skb(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	br_drop_fake_rtable(skb);
	return netif_receive_skb(skb);
}

static int br_pass_frame_up(struct sk_buff *skb)
{
	struct net_device *indev, *brdev = BR_INPUT_SKB_CB(skb)->brdev;
	struct net_bridge *br = netdev_priv(brdev);
#ifdef CONFIG_BRIDGE_VLAN_FILTERING
	struct net_bridge_vlan_group *vg;
#endif

	if (likely(1
#if IS_ENABLED(CONFIG_RA_HW_NAT)
	    && !FOE_SKB_IS_KEEPALIVE(skb)
#endif
#if IS_ENABLED(CONFIG_FAST_NAT)
	    && !SWNAT_KA_CHECK_MARK(skb)
#endif
	    )) {
		struct pcpu_sw_netstats *brstats = this_cpu_ptr(br->stats);

		u64_stats_update_begin(&brstats->syncp);
		brstats->rx_packets++;
		brstats->rx_bytes += skb->len;
		u64_stats_update_end(&brstats->syncp);
	}

#ifdef CONFIG_BRIDGE_VLAN_FILTERING
	vg = br_vlan_group_rcu(br);
	/* Bridge is just like any other port.  Make sure the
	 * packet is allowed except in promisc modue when someone
	 * may be running packet capture.
	 */
	if (!(brdev->flags & IFF_PROMISC) &&
	    !br_allowed_egress(vg, skb)) {
		kfree_skb(skb);
		return NET_RX_DROP;
	}
#endif

	indev = skb->dev;
	skb->dev = brdev;
#ifdef CONFIG_BRIDGE_VLAN_FILTERING
	skb = br_handle_vlan(br, vg, skb);
	if (!skb)
		return NET_RX_DROP;
#endif
	/* update the multicast stats if the packet is IGMP/MLD */
	br_multicast_count(br, NULL, skb, br_multicast_igmp_type(skb),
			   BR_MCAST_DIR_TX);

	return BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN,
		       dev_net(indev), NULL, skb, indev, NULL,
		       br_netif_receive_skb);
}

static void br_do_proxy_arp(struct sk_buff *skb, struct net_bridge *br,
			    u16 vid, struct net_bridge_port *p)
{
	struct net_device *dev = br->dev;
	struct neighbour *n;
	struct arphdr *parp;
	u8 *arpptr, *sha;
	__be32 sip, tip;

	BR_INPUT_SKB_CB(skb)->proxyarp_replied = false;

	if ((dev->flags & IFF_NOARP) ||
	    !pskb_may_pull(skb, arp_hdr_len(dev)))
		return;

	parp = arp_hdr(skb);

	if (parp->ar_pro != htons(ETH_P_IP) ||
	    parp->ar_op != htons(ARPOP_REQUEST) ||
	    parp->ar_hln != dev->addr_len ||
	    parp->ar_pln != 4)
		return;

	arpptr = (u8 *)parp + sizeof(struct arphdr);
	sha = arpptr;
	arpptr += dev->addr_len;	/* sha */
	memcpy(&sip, arpptr, sizeof(sip));
	arpptr += sizeof(sip);
	arpptr += dev->addr_len;	/* tha */
	memcpy(&tip, arpptr, sizeof(tip));

	if (ipv4_is_loopback(tip) ||
	    ipv4_is_multicast(tip))
		return;

	n = neigh_lookup(&arp_tbl, &tip, dev);
	if (n) {
		struct net_bridge_fdb_entry *f;

		if (!(n->nud_state & NUD_VALID)) {
			neigh_release(n);
			return;
		}

		f = __br_fdb_get(br, n->ha, vid);
		if (f && ((p->flags & BR_PROXYARP) ||
			  (f->dst && (f->dst->flags & BR_PROXYARP_WIFI)))) {
			arp_send(ARPOP_REPLY, ETH_P_ARP, sip, skb->dev, tip,
				 sha, n->ha, sha);
			BR_INPUT_SKB_CB(skb)->proxyarp_replied = true;
		}

		neigh_release(n);
	}
}

/* note: already called with rcu_read_lock */
int br_handle_frame_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	enum br_pkt_type pkt_type = BR_PKT_UNICAST;
	struct net_bridge_fdb_entry *dst = NULL;
	struct net_bridge_mdb_entry *mdst;
	bool local_rcv, mcast_hit = false;
	struct net_bridge *br;
	u16 vid = 0;

	if (!p || p->state == BR_STATE_DISABLED)
		goto drop;

	if (skb_vlan_tag_present(skb) || skb->protocol == htons(ETH_P_8021Q))
		if (!(p->flags & BR_ADMIT_TAGGED))
			goto drop;

	if (!br_allowed_ingress(p->br, nbp_vlan_group_rcu(p), skb, &vid))
		goto out;

	nbp_switchdev_frame_mark(p, skb);

	/* insert into forwarding database after filtering to avoid spoofing */
	br = p->br;
	if (p->flags & BR_LEARNING)
		br_fdb_update(br, p, eth_hdr(skb)->h_source, vid, false);

	local_rcv = !!(br->dev->flags & IFF_PROMISC);
	if (is_multicast_ether_addr(dest)) {
		/* by definition the broadcast is also a multicast address */
		if (is_broadcast_ether_addr(dest)) {
			pkt_type = BR_PKT_BROADCAST;
			local_rcv = true;
		} else {
			pkt_type = BR_PKT_MULTICAST;
			if (br_multicast_rcv(br, p, skb, vid))
				goto drop;
		}
	}

	if (p->state == BR_STATE_LEARNING)
		goto drop;

	BR_INPUT_SKB_CB(skb)->brdev = br->dev;

	if (IS_ENABLED(CONFIG_INET) && skb->protocol == htons(ETH_P_ARP))
		br_do_proxy_arp(skb, br, vid, p);

	if (p->flags & BR_ISOLATE_MODE)
		return br_pass_frame_up(skb);

	switch (pkt_type) {
	case BR_PKT_MULTICAST:
#ifdef CONFIG_BRIDGE_IGMP_SNOOPING
		/* pass IGMP/MLD (or all mcast) to igmpsn */
		if (br->multicast_disabled || br_multicast_igmp_type(skb))
#endif
		{
			extern int (*igmpsn_hook)(struct sk_buff *skb);
			typeof(igmpsn_hook) igmpsn;

			igmpsn = rcu_dereference(igmpsn_hook);
			if (igmpsn)
				igmpsn(skb);
		}

		mdst = br_mdb_get(br, skb, vid);
		if ((mdst || BR_INPUT_SKB_CB_MROUTERS_ONLY(skb)) &&
		    br_multicast_querier_exists(br, eth_hdr(skb))) {
			if ((mdst && mdst->mglist) ||
			    br_multicast_is_router(br)) {
				local_rcv = true;
				br->dev->stats.multicast++;
			}
			mcast_hit = true;
		} else {
			local_rcv = true;
			br->dev->stats.multicast++;
		}
		break;
	case BR_PKT_UNICAST:
		dst = __br_fdb_get(br, dest, vid);
	default:
		break;
	}

	if (dst) {
		if (dst->is_local)
			return br_pass_frame_up(skb);

		dst->used = jiffies;
		br_forward(dst->dst, skb, local_rcv, false);
	} else {
		if (!mcast_hit)
			br_flood(br, skb, pkt_type, local_rcv, false);
		else
			br_multicast_flood(mdst, skb, local_rcv, false);
	}

	if (local_rcv)
		return br_pass_frame_up(skb);

out:
	return 0;
drop:
	kfree_skb(skb);
	goto out;
}
EXPORT_SYMBOL_GPL(br_handle_frame_finish);

static inline void __br_handle_local_finish(struct sk_buff *skb)
{
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	u16 vid = 0;

	/* check if vlan is allowed, to avoid spoofing */
	if (p->flags & BR_LEARNING && br_should_learn(p, skb, &vid))
		br_fdb_update(p->br, p, eth_hdr(skb)->h_source, vid, false);
}

/* note: already called with rcu_read_lock */
static int br_handle_local_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	__br_handle_local_finish(skb);

	/* return 1 to signal the okfn() was called so it's ok to use the skb */
	return 1;
}

static void br_disable_ports(struct net_bridge *br, struct net_bridge_port *p,
			     const char *reason)
{
	struct net_bridge_port *port;

	if (p->port_type == BR_PORT_TYPE_WIFI_STATION) {
		p->loop_detect = BR_PORT_LOOP_DETECTED;
		br_warn(br, "loop by %s detected, disable port %u(%s)\n",
			reason, (unsigned int) p->port_no, p->dev->name);
		br->wsta_shutdown_time =
			get_jiffies_64() +
			nsecs_to_jiffies64(3ULL * NSEC_PER_SEC);
	} else {
		list_for_each_entry_rcu(port, &br->port_list, list) {
			if (port == p)
				continue;

			if (port->loop_detect != BR_PORT_LOOP_LISTEN)
				continue;

			if (port->port_type == BR_PORT_TYPE_NORMAL)
				continue;

			port->loop_detect = BR_PORT_LOOP_DETECTED;
			br_warn(br, "loop by %s detected, disable port %u(%s)\n",
				reason, (unsigned int) port->port_no,
				port->dev->name);
		}
	}
}

static bool br_check_ports_macs(struct net_bridge *br,
				struct net_bridge_port *p,
				struct sk_buff *skb)
{
	struct net_bridge_port *port;

	list_for_each_entry_rcu(port, &br->port_list, list) {
		if (port == p)
			continue;

		if (port->port_type != BR_PORT_TYPE_WIFI_STATION)
			continue;

		if (port->loop_detect == BR_PORT_NO_LOOP)
			continue;

		if (ether_addr_equal(port->dev->dev_addr,
				     eth_hdr(skb)->h_source))
			return true;
	}

	return false;
}

/*
 * Return NULL if skb is handled
 * note: already called with rcu_read_lock
 */
rx_handler_result_t br_handle_frame(struct sk_buff **pskb)
{
	struct net_bridge_port *p;
	struct sk_buff *skb = *pskb;
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge *br;
#if IS_ENABLED(CONFIG_BRIDGE_EBT_BROUTE)
	br_should_route_hook_t *rhook;
#endif

	if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
		return RX_HANDLER_PASS;

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source))
		goto drop;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return RX_HANDLER_CONSUMED;

	p = br_port_get_rcu(skb->dev);
	br = p->br;

#if IS_ENABLED(CONFIG_USB_NET_KPDSL)
	if (skb->protocol == htons(ETH_P_EBM)) {
		struct net_device *brdev = br->dev;
		const u8 state = p->state;

		/* accept packets from microbridge interfaces only */
		if (!(p->dev->priv_flags & IFF_UBRIDGE))
			goto drop;

		/* update the FDB despite on a port state */
		p->state = BR_STATE_FORWARDING;
		br_fdb_update(br, p, eth_hdr(skb)->h_source, 0, false);
		p->state = state;

		if (ether_addr_equal(brdev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		skb->dev = brdev;
		BR_INPUT_SKB_CB(skb)->brdev = brdev;

		nbp_switchdev_frame_mark(p, skb);
		br_netif_receive_skb(NULL, NULL, skb);

		return RX_HANDLER_CONSUMED;
	}
#endif

	if (unlikely(br->stp_enabled == BR_NO_STP)) {
		if (unlikely(p->loop_detect == BR_PORT_LOOP_DETECTED))
			goto drop;

		if (p->loop_detect == BR_PORT_LOOP_LISTEN) {
			if (p->port_type == BR_PORT_TYPE_NORMAL &&
			    br_check_ports_macs(br, p, skb)) {
				br_disable_ports(br, p, "packet");
				goto drop;
			}

			if (((ether_addr_equal(p->dev->dev_addr,
				eth_hdr(skb)->h_source) ||
			      ether_addr_equal(br->dev->dev_addr,
				eth_hdr(skb)->h_source)))) {
				br_disable_ports(br, p, "packet");
				goto drop;
			}

			if (unlikely(br_queue_overloaded(&p->queue) &&
			    time_is_after_jiffies64(br->wsta_shutdown_time))) {
				br_disable_ports(br, p, "queue");
				goto drop;
			}

			if (unlikely(br_is_own_lldpdu(br, skb))) {
				br_disable_ports(br, p, "lldpdu");
				goto drop;
			}
		}

		if (unlikely(skb->pkt_type == PACKET_BROADCAST &&
			     p->broadcast_limit ==
				BR_PORT_BCAST_LIMIT_ENABLED)) {
			switch (br_enqueue(&p->queue, skb)) {
			case NF_ACCEPT:
				break;
			case NF_STOLEN:
				return RX_HANDLER_CONSUMED;
			case NF_DROP:
			default:
				goto drop;
			}
		}
	}

	if (unlikely(is_link_local_ether_addr(dest))) {
		u16 fwd_mask = br->group_fwd_mask_required;

		/*
		 * See IEEE 802.1D Table 7-10 Reserved addresses
		 *
		 * Assignment		 		Value
		 * Bridge Group Address		01-80-C2-00-00-00
		 * (MAC Control) 802.3		01-80-C2-00-00-01
		 * (Link Aggregation) 802.3	01-80-C2-00-00-02
		 * 802.1X PAE address		01-80-C2-00-00-03
		 *
		 * 802.1AB LLDP 		01-80-C2-00-00-0E
		 *
		 * Others reserved for future standardization
		 */
		switch (dest[5]) {
		case 0x00:	/* Bridge Group Address */
			/* If STP is turned off,
			   then must forward to keep loop detection */
			if (br->stp_enabled == BR_NO_STP ||
			    fwd_mask & (1u << dest[5]))
				goto forward;
			*pskb = skb;
			__br_handle_local_finish(skb);
			return RX_HANDLER_PASS;

		case 0x01:	/* IEEE MAC (Pause) */
			goto drop;

		case 0x0E:	/* 802.1AB LLDP */
			fwd_mask |= br->group_fwd_mask;
			if (fwd_mask & (1u << dest[5]))
				goto forward;
			*pskb = skb;
			__br_handle_local_finish(skb);
			return RX_HANDLER_PASS;

		default:
			/* Allow selective forwarding for most other protocols */
			fwd_mask |= br->group_fwd_mask;
			if (fwd_mask & (1u << dest[5]))
				goto forward;
		}

		/* The else clause should be hit when nf_hook():
		 *   - returns < 0 (drop/error)
		 *   - returns = 0 (stolen/nf_queue)
		 * Thus return 1 from the okfn() to signal the skb is ok to pass
		 */
		if (BR_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_IN,
			    dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			    br_handle_local_finish) == 1) {
			return RX_HANDLER_PASS;
		} else {
			return RX_HANDLER_CONSUMED;
		}
	}

forward:
	switch (p->state) {
	case BR_STATE_FORWARDING:
#if IS_ENABLED(CONFIG_BRIDGE_EBT_BROUTE)
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook) {
			if ((*rhook)(skb)) {
				*pskb = skb;
				return RX_HANDLER_PASS;
			}
			dest = eth_hdr(skb)->h_dest;
		}
#endif
		/* fall through */
	case BR_STATE_LEARNING:
		if (ether_addr_equal(br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		BR_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING,
			dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
drop:
		kfree_skb(skb);
	}
	return RX_HANDLER_CONSUMED;
}

void br_handle_broadcast_frame_finish(struct sk_buff *skb)
{
	struct net_bridge_port *p = br_port_get_rcu(skb->dev);
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge *br = p->br;
#if IS_ENABLED(CONFIG_BRIDGE_EBT_BROUTE)
	br_should_route_hook_t *rhook;
#endif

	switch (p->state) {
	case BR_STATE_FORWARDING:
#if IS_ENABLED(CONFIG_BRIDGE_EBT_BROUTE)
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook) {
			if ((*rhook)(skb)) {
				kfree_skb(skb);
				return;
			}
			dest = eth_hdr(skb)->h_dest;
		}
#endif
		/* fall through */
	case BR_STATE_LEARNING:
		if (ether_addr_equal(br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		BR_HOOK(NFPROTO_BRIDGE, NF_BR_PRE_ROUTING,
			dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
		kfree_skb(skb);
	}
}
