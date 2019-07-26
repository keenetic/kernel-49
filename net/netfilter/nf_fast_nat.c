/*
 * Packet matching code.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2009-2002 Netfilter core team <coreteam@netfilter.org>
 *
 * 19 Jan 2002 Harald Welte <laforge@gnumonks.org>
 * 	- increase module usage count as soon as we have rules inside
 * 	  a table
 */
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/dst_metadata.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_l3proto.h>
#include <net/netfilter/nf_nat_l4proto.h>
#include <linux/netfilter/nf_conntrack_common.h>

#ifdef CONFIG_XFRM
#include <net/xfrm.h>
#endif

#include <linux/ntc_shaper_hooks.h>
#include <net/fast_nat.h>
#include <net/fast_vpn.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

static int
fast_nat_path_egress(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	int ret;

	if (iph->ttl <= 1) {
		icmp_send(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
		kfree_skb(skb);
		return -EPERM;
	}

	ip_decrease_ttl(iph);

	/* Direct send packets to output */
	ret = ip_finish_output(net, sk, skb);

	/* Don't return 1 */
	if (ret == 1)
		ret = 0;

	return ret;
}

/* Returns 1 if okfn() needs to be executed by the caller,
 * -EPERM for NF_DROP, 0 otherwise. */
int fast_nat_path(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	ntc_shaper_hook_fn *shaper_egress;
	unsigned int ntc_retval = NF_ACCEPT;
	int retval = 0;

	if (!skb_valid_dst(skb)) {
		const struct iphdr *iph = ip_hdr(skb);
		struct net_device *dev = skb->dev;

		if (ip_route_input(skb, iph->daddr, iph->saddr, iph->tos, dev)) {
			kfree_skb(skb);
			return -EPERM;
		}

		/*  Change skb owner to output device */
		skb->dev = skb_dst(skb)->dev;
	}

	if (likely(!SWNAT_KA_CHECK_MARK(skb)
#if IS_ENABLED(CONFIG_RA_HW_NAT)
		&& !FOE_SKB_IS_KEEPALIVE(skb)
#endif
	    )) {
		struct nf_conn *ct;
		enum ip_conntrack_info ctinfo;

		ct = nf_ct_get(skb, &ctinfo);
		nf_ct_acct_add_packet(ct, ctinfo, skb);
	}

	shaper_egress = ntc_shaper_egress_hook_get();
	if (shaper_egress) {
		const struct ntc_shaper_fwd_t fwd = {
			.saddr_ext	= 0,
			.daddr_ext	= 0,
			.okfn_nf	= fast_nat_path_egress,
			.okfn_custom	= NULL,
			.data		= NULL,
			.net		= net,
			.sk		= sk
		};

		ntc_retval = shaper_egress(skb, &fwd);
	}
	ntc_shaper_egress_hook_put();

	switch (ntc_retval) {
	case NF_ACCEPT:
		retval = fast_nat_path_egress(net, sk, skb);
		break;
	case NF_STOLEN:
		retval = 0;
		break;
	default:
		kfree_skb(skb);
		retval = -EPERM;
		break;
	}

	return retval;
}
EXPORT_SYMBOL(fast_nat_path);

int fast_nat_do_bind(struct nf_conn *ct,
		     struct sk_buff *skb,
		     const struct nf_conntrack_l3proto *l3proto,
		     const struct nf_conntrack_l4proto *l4proto,
		     enum ip_conntrack_info ctinfo)
{
	static int hn[2] = {NF_INET_PRE_ROUTING, NF_INET_POST_ROUTING};
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	int iphdroff = skb_network_offset(skb);
	unsigned int i = 0;

	do {
		const struct nf_nat_l3proto *nat_l3proto;
		const struct nf_nat_l4proto *nat_l4proto;
		enum nf_nat_manip_type mtype = HOOK2MANIP(hn[i]);
		unsigned long statusbit;

		if (mtype == NF_NAT_MANIP_SRC)
			statusbit = IPS_SRC_NAT;
		else
			statusbit = IPS_DST_NAT;

		/* Invert if this is reply dir. */
		if (dir == IP_CT_DIR_REPLY)
			statusbit ^= IPS_NAT_MASK;

		if (ct->status & statusbit) {
			struct nf_conntrack_tuple target;

			if (mtype == NF_NAT_MANIP_SRC && !skb_valid_dst(skb)) {
				const struct iphdr *iph = ip_hdr(skb);
				struct net_device *dev = skb->dev;

				if (ip_route_input(skb, iph->daddr, iph->saddr,
						   iph->tos, dev))
					return NF_DROP;

				/* Change skb owner to output device */
				skb->dev = skb_dst(skb)->dev;
			}

			/* We are aiming to look like inverse of other direction. */
			nf_ct_invert_tuple(&target, &ct->tuplehash[!dir].tuple,
						l3proto, l4proto);

			nat_l3proto = __nf_nat_l3proto_find(target.src.l3num);
			nat_l4proto = __nf_nat_l4proto_find(target.src.l3num,
							    target.dst.protonum);
			if (!nat_l3proto->manip_pkt(skb, iphdroff, nat_l4proto,
						    &target, mtype))
				return NF_DROP;
		}
		i++;
	} while (i < 2);

	return NF_FAST_NAT;
}

static inline int __attribute__((hot, always_inline))
fast_nat_esp_recv_final(struct sk_buff *skb, int esphoff, int encap_type)
{
	unsigned char _esph[sizeof(__be32)];
	const struct ip_esp_hdr *esph = skb_header_pointer(
			skb, esphoff,
			sizeof(__be32), _esph);
	const struct iphdr *iph = ip_hdr(skb);
	const struct xfrm_state *x = xfrm_state_lookup(
			dev_net(skb->dev), skb->mark,
			(xfrm_address_t *)&iph->daddr, esph->spi,
			IPPROTO_ESP, AF_INET);

	if (x) {
		skb->pkt_type = PACKET_HOST;
		skb_pull(skb, esphoff);
		skb_reset_transport_header(skb);
		xfrm4_rcv_spi_encap(skb, IPPROTO_ESP, encap_type);

		return NF_STOLEN;
	}

	return NF_ACCEPT;
}

int fast_nat_esp_recv(struct sk_buff *skb, int dataoff, uint8_t protonum)
{
#ifdef CONFIG_XFRM
	unsigned char _l4hdr[sizeof(struct udphdr) + 3 * sizeof(__be32)];

	if (protonum == IPPROTO_UDP && skb->len >
			(sizeof(struct udphdr) + sizeof(struct iphdr))) {
		const int len = skb->len - sizeof(struct udphdr) - sizeof(struct iphdr);
		const struct udphdr *udph = skb_header_pointer(
			skb, dataoff,
			sizeof(struct udphdr) + min(len, (int)(3 * sizeof(__be32))),
			_l4hdr);

		/* perform input bypass only on NAT-T UDP/4500 */

		if (udph && udph->dest == htons(4500)) {
			__u8 *udpdata = (__u8 *)udph + sizeof(struct udphdr);
			__be32 *udpdata32 = (__be32 *)udpdata;

			if (unlikely(len == 1 && udpdata[0] == 0xff))
				return NF_DROP; /* NAT-T keepalive */

			/* rfc 3948 p 2.1 */
			if (len > sizeof(struct ip_esp_hdr) && udpdata32[0] != 0)
				return fast_nat_esp_recv_final(skb,
					dataoff + sizeof(struct udphdr),
					UDP_ENCAP_ESPINUDP);

			/* draft-ietf-ipsec-udp-encaps-00, p 2.1, dirty old crap */
			if (len > (sizeof(struct ip_esp_hdr) + 2 * sizeof(__be32)) &&
				udpdata32[0] == 0 && udpdata32[1] == 0 &&
				udpdata32[2] != 0)
					return fast_nat_esp_recv_final(skb,
						dataoff + sizeof(struct udphdr) + 2 * sizeof(__be32),
						UDP_ENCAP_ESPINUDP_NON_IKE);
		}
	} else
	if (protonum == IPPROTO_ESP && skb->len >
			(sizeof(struct ip_esp_hdr) + sizeof(struct iphdr)))
		return fast_nat_esp_recv_final(skb, dataoff, 0);

#endif /* CONFIG_XFRM */

	return NF_ACCEPT;
}

static int __init fast_nat_init(void)
{
	nf_fastnat_control = 1;
	nf_fastnat_xfrm_control = 0;
	nf_fastnat_esp_bypass_control = 1;
	nf_fastnat_pptp_bypass_control = 1;
	printk(KERN_INFO "Fast NAT loaded\n");
	return 0;
}

static void __exit fast_nat_fini(void)
{
	nf_fastnat_control = 0;
	nf_fastnat_xfrm_control = 0;
	nf_fastnat_esp_bypass_control = 0;
	nf_fastnat_pptp_bypass_control = 0;
	printk(KERN_INFO "Fast NAT unloaded\n");
}

module_init(fast_nat_init);
module_exit(fast_nat_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("http://www.ndmsystems.com");
