/*
 * Fastpath module for NAT speedup.
 *
 * Based on bcm_nat by BCM LTD and additional GPLv2 code blocks.
 *
 *      This program is free software, you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * 2016, Ilya Ponetaev <i.ponetaev@ndmsystems.com> ported to Linux 3.x
 * 2019, Andrey Zolotarev <a.zolotarev@ndmsystems.com> ported to Linux 4.x
 * 2019, Ilya Ponetayev <i.ponetaev@ndmsystems.com> XFRM ESP fastpath
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
#include <linux/netfilter/xt_ndmmark.h>

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
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	if (iph->ttl <= 1) {
		icmp_send(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
		kfree_skb(skb);
		return -EPERM;
	}

	ct = nf_ct_get(skb, &ctinfo);

	if (likely(ct)) {
		if (unlikely(ct->fast_out_hoplimit != 0 &&
			     ct->fast_out_hoplimit ==
				skb_dst(skb)->dev->out_hoplimit_ip4))
		{
			const u8 new_ttl = ct->fast_out_hoplimit;

			csum_replace2(&iph->check, htons(iph->ttl << 8),
				      htons(new_ttl << 8));
			iph->ttl = new_ttl;

#if IS_ENABLED(CONFIG_RA_HW_NAT)
			FOE_ALG_MARK(skb);
#endif

		} else {
			ip_decrease_ttl(iph);
		}
	}

	xt_ndmmark_set_fwd(skb);

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
		struct net_device *dev = skb->dev;
#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
		typeof(nf_fastroute_rtcache_in) do_rtcache_in;

		do_rtcache_in = rcu_dereference(nf_fastroute_rtcache_in);
		if (!do_rtcache_in || !do_rtcache_in(PF_INET, skb, dev->ifindex))
#endif
#endif
		{
			const struct iphdr *iph = ip_hdr(skb);

			if (ip_route_input(skb, iph->daddr, iph->saddr,
					   iph->tos, dev)) {
				kfree_skb(skb);
				return -EPERM;
			}
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
		if (likely(ct != NULL)) {
			unsigned int len = skb->len - skb_network_offset(skb);

			nf_ct_acct_add_packet_len(ct, ctinfo, len);
		}
	}

	shaper_egress = ntc_shaper_egress_hook_get();
	if (shaper_egress) {
		const struct ntc_shaper_fwd_t fwd = {
			.okfn		= fast_nat_path_egress,
			.net		= net,
			.sk		= sk,
			.is_ipv4	= true,
			.is_swnat	= false
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

#ifdef CONFIG_FAST_NAT_V2
#ifdef CONFIG_XFRM
static int
__xfrm4_in(struct net *net, struct sk_buff *skb, int esphoff, int encap_type)
{
	u8 _esph[sizeof(__be32)];
	const struct ip_esp_hdr *esph;

	esph = skb_header_pointer(skb, esphoff, sizeof(_esph), _esph);
	if (esph) {
		const struct iphdr *iph = ip_hdr(skb);
		struct xfrm_state *x;

		x = xfrm_state_lookup(net, skb->mark,
				      (const xfrm_address_t *)&iph->daddr,
				      esph->spi, IPPROTO_ESP, AF_INET);
		if (x) {
			skb->pkt_type = PACKET_HOST;
			__skb_pull(skb, esphoff);
			skb_reset_transport_header(skb);
			xfrm4_rcv_spi_encap(skb, IPPROTO_ESP, encap_type);

			return NF_STOLEN;
		}
	}

	return NF_ACCEPT;
}

int nf_fastpath_esp4_in(struct net *net, struct sk_buff *skb,
			unsigned int dataoff, uint8_t protonum)
{
	unsigned int min_len;

	if (protonum == IPPROTO_UDP) {
		u8 _l4hdr[sizeof(struct udphdr) + 3 * sizeof(__be32)];

		min_len = sizeof(struct iphdr) + sizeof(struct udphdr);

		if (skb->len > min_len) {
			unsigned int esp_len = skb->len - min_len;
			int hlen = sizeof(struct udphdr);
			const struct udphdr *uh;

			hlen += min_t(int, esp_len, 3 * sizeof(__be32));
			uh = skb_header_pointer(skb, dataoff, hlen, _l4hdr);

			/* perform input bypass only on NAT-T UDP/4500 */
			if (uh && uh->dest == htons(4500)) {
				u8 *udpdata = (u8 *)uh + sizeof(struct udphdr);
				__be32 *udpdata32 = (__be32 *)udpdata;
				int esphoff = dataoff + sizeof(struct udphdr);

				/* check NAT-T keepalive */
				if (unlikely(esp_len == 1 && udpdata[0] == 0xff))
					return NF_DROP;

				/* rfc 3948 p 2.1 */
				min_len = sizeof(struct ip_esp_hdr);
				if (esp_len > min_len && udpdata32[0] != 0)
					return __xfrm4_in(net, skb, esphoff,
							  UDP_ENCAP_ESPINUDP);

				/* draft-ietf-ipsec-udp-encaps-00, p 2.1 */
				min_len += 2 * sizeof(__be32);
				if (esp_len > min_len && udpdata32[2] != 0 &&
				    udpdata32[0] == 0 && udpdata32[1] == 0)
					return __xfrm4_in(net, skb,
						esphoff + 2 * sizeof(__be32),
						UDP_ENCAP_ESPINUDP_NON_IKE);
			}
		}
	} else if (protonum == IPPROTO_ESP) {
		min_len = sizeof(struct iphdr) + sizeof(struct ip_esp_hdr);

		if (skb->len > min_len)
			return __xfrm4_in(net, skb, dataoff, 0);
	}

	return NF_ACCEPT;
}
#endif /* CONFIG_XFRM */
#endif /* CONFIG_FAST_NAT_V2 */

static int __init fast_nat_init(void)
{
	nf_fastnat_control = 1;
#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	nf_fastroute_control = 1;
#endif
#if IS_ENABLED(CONFIG_PPTP)
	nf_fastpath_pptp_control = 1;
#endif
#if IS_ENABLED(CONFIG_PPPOL2TP)
	nf_fastpath_l2tp_control = 1;
#endif
#ifdef CONFIG_XFRM
	nf_fastpath_esp_control = 1;
	nf_fastnat_xfrm_control = 1;
#endif
#endif /* CONFIG_FAST_NAT_V2 */
	printk(KERN_INFO "Fast NAT loaded\n");
	return 0;
}

static void __exit fast_nat_fini(void)
{
	nf_fastnat_control = 0;
#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	nf_fastroute_control = 0;
#endif
#if IS_ENABLED(CONFIG_PPTP)
	nf_fastpath_pptp_control = 0;
#endif
#if IS_ENABLED(CONFIG_PPPOL2TP)
	nf_fastpath_l2tp_control = 0;
#endif
#ifdef CONFIG_XFRM
	nf_fastpath_esp_control = 0;
	nf_fastnat_xfrm_control = 0;
#endif
#endif /* CONFIG_FAST_NAT_V2 */
	printk(KERN_INFO "Fast NAT unloaded\n");
}

module_init(fast_nat_init);
module_exit(fast_nat_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Fastpath module for NAT offload");
