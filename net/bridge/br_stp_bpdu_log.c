/*
 * Copyright (c) 2000 Lennert Buytenhek
 *
 * This software may be distributed either under the terms of the
 * BSD-style license that accompanies tcpdump or the GNU General
 * Public License
 *
 * Format and print IEEE 802.1d spanning tree protocol packets.
 * Contributed by Lennert Buytenhek <buytenh@gnu.org>
 */

#include <linux/skbuff.h>
#include <linux/if_vlan.h>

#include "br_private.h"
#include "br_private_stp.h"

#if !IS_ENABLED(CONFIG_VLAN_8021Q)
#error Kernel must contain support of VLAN 8021Q
#endif /* CONFIG_VLAN_8021Q */

#define STP_PRINT_BUF_SIZE		64

#define RSTP_EXTRACT_PORT_ROLE(x)	(((x) & 0x0C) >> 2)
/* STP timers are expressed in multiples of 1/256th second */
#define STP_TIME_BASE			256
#define STP_BPDU_MSTP_MIN_LEN		102

struct stp_bpdu {
	u8 protocol_id[2];
	u8 protocol_version;
	u8 bpdu_type;
	u8 flags;
	u8 root_id[8];
	u8 root_path_cost[4];
	u8 bridge_id[8];
	u8 port_id[2];
	u8 message_age[2];
	u8 max_age[2];
	u8 hello_time[2];
	u8 forward_delay[2];
	u8 v1_length;
};

#define STP_PROTO_REGULAR		0x00
#define STP_PROTO_RAPID			0x02
#define STP_PROTO_MSTP			0x03

struct tok {
	int v;			/* value */
	const char *s;		/* string */
};

static const struct tok stp_proto_values[] = {
	{ STP_PROTO_REGULAR, "802.1d" },
	{ STP_PROTO_RAPID, "802.1w" },
	{ STP_PROTO_MSTP, "802.1s" },
	{ 0, NULL}
};

#define STP_BPDU_TYPE_CONFIG		0x00
#define STP_BPDU_TYPE_RSTP		0x02
#define STP_BPDU_TYPE_TOPO_CHANGE	0x80

static const struct tok stp_bpdu_flag_values[] = {
	{ 0x01, "Topology change" },
	{ 0x02, "Proposal" },
	{ 0x10, "Learn" },
	{ 0x20, "Forward" },
	{ 0x40, "Agreement" },
	{ 0x80, "Topology change ACK" },
	{ 0, NULL}
};

static const struct tok stp_bpdu_type_values[] = {
	{ STP_BPDU_TYPE_CONFIG, "Config" },
	{ STP_BPDU_TYPE_RSTP, "Rapid STP" },
	{ STP_BPDU_TYPE_TOPO_CHANGE, "Topology Change" },
	{ 0, NULL}
};

static const struct tok rstp_obj_port_role_values[] = {
	{ 0x00, "Unknown" },
	{ 0x01, "Alternate" },
	{ 0x02, "Root" },
	{ 0x03, "Designated" },
	{ 0, NULL}
};

static char *
bittok2str(const struct tok *lp, const char *fmt, int v,
	   char *buf, size_t buflen)
{
	int blen = 0;
	int rotbit; /* this is the bit we rotate through all bitpositions */
	int tokval;

	while (lp->s != NULL && lp != NULL) {
		tokval = lp->v;   /* load our first value */
		rotbit = 1;

		while (rotbit != 0) {
			/*
			 * lets AND the rotating bit with our token value
			 * and see if we have got a match
			 */
			if (tokval == (v & rotbit)) {
				/* ok we have found something */
				blen += snprintf(buf + blen, buflen - blen,
						 "%s, ", lp->s);
				break;
			}

			/* no match - lets shift and try again */
			rotbit = rotbit << 1;
		}

		lp++;
	}

	if (blen != 0) {
		/* did we find anything
		 * yep, set the the trailing zero 2 bytes before
		 * to eliminate the last comma & whitespace
		 */
		buf[blen - 2] = '\0';

		return buf;
	}

	/* bummer - lets print the "unknown" message
	 * as advised in the fmt string if we got one
	 */

	if (fmt == NULL)
		fmt = "#%d";

	(void)snprintf(buf, buflen, fmt, v);

	return buf;
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
static const char *
tok2strbuf(const struct tok *lp, const char *fmt,
	   int v, char *buf, size_t bufsize)
{
	if (lp != NULL) {
		while (lp->s != NULL) {
			if (lp->v == v)
				return lp->s;
			++lp;
		}
	}

	if (fmt == NULL)
		fmt = "#%d";

	(void)snprintf(buf, bufsize, fmt, v);

	return (const char *)buf;
}

static char *
stp_print_id(const u8 *p, char *buf, const size_t buflen)
{
	snprintf(buf, buflen,
		 "%.2x%.2x.%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

	return buf;
}

static void
stp_print_config_bpdu(const struct stp_bpdu *bpdu, size_t length,
		      char *buf, size_t buflen)
{
	pr_cont(", Flags [%s]",
		bittok2str(stp_bpdu_flag_values,
			   "none", bpdu->flags,
			   buf, buflen));

	pr_cont(", bridge-id %s.%04x, length %zu\n",
		stp_print_id((const u8 *)&bpdu->bridge_id, buf, buflen),
		get_unaligned_be16(&bpdu->port_id),
		length);

	pr_info(" +-> message-age %u ms, max-age %u ms"
		", hello-time %u ms, forwarding-delay %u ms\n",
		1000u * get_unaligned_be16(&bpdu->message_age) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->max_age) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->hello_time) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->forward_delay) / STP_TIME_BASE);

	pr_info(" +-> root-id %s, root-pathcost %u",
		stp_print_id((const u8 *)&bpdu->root_id, buf, buflen),
		get_unaligned_be32(&bpdu->root_path_cost));

	/* Port role is only valid for 802.1w */
	if (bpdu->protocol_version == STP_PROTO_RAPID) {
		pr_cont(", port-role %s",
			tok2strbuf(rstp_obj_port_role_values, "Unknown",
				   RSTP_EXTRACT_PORT_ROLE(bpdu->flags),
				   buf, buflen));
	}
}

/*
 * MSTP packet format
 * Ref. IEEE 802.1Q 2003 Ed. Section 14
 *
 * MSTP BPDU
 *
 * 2 -  bytes Protocol Id
 * 1 -  byte  Protocol Ver.
 * 1 -  byte  BPDU tye
 * 1 -  byte  Flags
 * 8 -  bytes CIST Root Identifier
 * 4 -  bytes CIST External Path Cost
 * 8 -  bytes CIST Regional Root Identifier
 * 2 -  bytes CIST Port Identifier
 * 2 -  bytes Message Age
 * 2 -  bytes Max age
 * 2 -  bytes Hello Time
 * 2 -  bytes Forward delay
 * 1 -  byte  Version 1 length. Must be 0
 * 2 -  bytes Version 3 length
 * 1 -  byte  Config Identifier
 * 32 - bytes Config Name
 * 2 -  bytes Revision level
 * 16 - bytes Config Digest [MD5]
 * 4 -  bytes CIST Internal Root Path Cost
 * 8 -  bytes CIST Bridge Identifier
 * 1 -  byte  CIST Remaining Hops
 * 16 - bytes MSTI information [Max 64 MSTI, each 16 bytes]
 *
 * MSTI Payload
 *
 * 1 - byte  MSTI flag
 * 8 - bytes MSTI Regional Root Identifier
 * 4 - bytes MSTI Regional Path Cost
 * 1 - byte  MSTI Bridge Priority
 * 1 - byte  MSTI Port Priority
 * 1 - byte  MSTI Remaining Hops
 */

#define MST_BPDU_MSTI_LENGTH			16
#define MST_BPDU_CONFIG_INFO_LENGTH		64

/* Offsets of fields from the begginning for the packet */
#define MST_BPDU_VER3_LEN_OFFSET		36
#define MST_BPDU_CONFIG_NAME_OFFSET		39
#define MST_BPDU_CONFIG_DIGEST_OFFSET		73
#define MST_BPDU_CIST_INT_PATH_COST_OFFSET	89
#define MST_BPDU_CIST_BRIDGE_ID_OFFSET		93
#define MST_BPDU_CIST_REMAIN_HOPS_OFFSET	101
#define MST_BPDU_MSTI_OFFSET			102
/* Offsets within  an MSTI */
#define MST_BPDU_MSTI_ROOT_PRIO_OFFSET		1
#define MST_BPDU_MSTI_ROOT_PATH_COST_OFFSET	9
#define MST_BPDU_MSTI_BRIDGE_PRIO_OFFSET	13
#define MST_BPDU_MSTI_PORT_PRIO_OFFSET		14
#define MST_BPDU_MSTI_REMAIN_HOPS_OFFSET	15

static void
stp_print_mstp_bpdu(const struct stp_bpdu *bpdu, size_t length,
		    char *buf, size_t buflen)
{
	const u8 *ptr = (const u8 *)bpdu;
	u16 v3len;
	u16 len;
	u16 msti;
	u16 offset;

	pr_cont(", CIST Flags [%s]",
		bittok2str(stp_bpdu_flag_values, "none", bpdu->flags,
			   buf, buflen));

	pr_cont(", CIST bridge-id %s.%04x, length %u\n",
		stp_print_id(ptr + MST_BPDU_CIST_BRIDGE_ID_OFFSET, buf, buflen),
		get_unaligned_be16(&bpdu->port_id), length);

	pr_info(" +-> message-age %u ms, max-age %u ms"
		", hello-time %u ms, forwarding-delay %u ms\n",
		1000u * get_unaligned_be16(&bpdu->message_age) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->max_age) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->hello_time) / STP_TIME_BASE,
		1000u * get_unaligned_be16(&bpdu->forward_delay) / STP_TIME_BASE);

	pr_info(" +-> CIST root-id %s, ext-pathcost %u int-pathcost %u",
		stp_print_id((const u8 *)&bpdu->root_id, buf, buflen),
		get_unaligned_be32(&bpdu->root_path_cost),
		get_unaligned_be32(ptr + MST_BPDU_CIST_INT_PATH_COST_OFFSET));

	pr_cont(", port-role %s\n",
		tok2strbuf(rstp_obj_port_role_values, "Unknown",
			   RSTP_EXTRACT_PORT_ROLE(bpdu->flags),
			   buf, buflen));

	pr_info(" +-> CIST regional-root-id %s",
		stp_print_id((const u8 *)&bpdu->bridge_id, buf, buflen));

	pr_cont(" +-> MSTP Configuration Name %s, revision %u, digest %08x%08x%08x%08x\n",
		ptr + MST_BPDU_CONFIG_NAME_OFFSET,
		get_unaligned_be16(ptr + MST_BPDU_CONFIG_NAME_OFFSET + 32),
		get_unaligned_be32(ptr + MST_BPDU_CONFIG_DIGEST_OFFSET),
		get_unaligned_be32(ptr + MST_BPDU_CONFIG_DIGEST_OFFSET + 4),
		get_unaligned_be32(ptr + MST_BPDU_CONFIG_DIGEST_OFFSET + 8),
		get_unaligned_be32(ptr + MST_BPDU_CONFIG_DIGEST_OFFSET + 12));

	pr_info(" +-> CIST remaining-hops %d",
		ptr[MST_BPDU_CIST_REMAIN_HOPS_OFFSET]);

	/* Dump all MSTI's */
	v3len = get_unaligned_be16(ptr + MST_BPDU_VER3_LEN_OFFSET);

	if (v3len > MST_BPDU_CONFIG_INFO_LENGTH) {
		len = v3len - MST_BPDU_CONFIG_INFO_LENGTH;
		offset = MST_BPDU_MSTI_OFFSET;

		while (len >= MST_BPDU_MSTI_LENGTH) {
			msti = get_unaligned_be16(ptr + offset +
				MST_BPDU_MSTI_ROOT_PRIO_OFFSET);
			msti = msti & 0x0FFF;

			pr_cont("\n +-+-> MSTI %d, Flags [%s], port-role %s",
				msti,
				bittok2str(stp_bpdu_flag_values, "none",
					   ptr[offset],
					   buf, buflen),
				tok2strbuf(rstp_obj_port_role_values,
					   "Unknown",
					   RSTP_EXTRACT_PORT_ROLE(ptr[offset]),
					   buf, buflen));

			pr_cont("\n +-+-> MSTI regional-root-id %s, pathcost %u",
				stp_print_id(ptr + offset +
					     MST_BPDU_MSTI_ROOT_PRIO_OFFSET,
					     buf, buflen),
				get_unaligned_be32(ptr + offset +
						   MST_BPDU_MSTI_ROOT_PATH_COST_OFFSET));

			pr_cont("\n +-+-> MSTI bridge-prio %d, port-prio %d, hops %d",
				ptr[offset + MST_BPDU_MSTI_BRIDGE_PRIO_OFFSET] >> 4,
				ptr[offset + MST_BPDU_MSTI_PORT_PRIO_OFFSET] >> 4,
				ptr[offset + MST_BPDU_MSTI_REMAIN_HOPS_OFFSET]);

			len -= MST_BPDU_MSTI_LENGTH;
			offset += MST_BPDU_MSTI_LENGTH;
		}
	}
}

/*
 * Print 802.1d / 802.1w / 802.1q (mstp) packets.
 */
static void
br_stp_bdpu_print(struct net_bridge *br, struct net_device *dev,
		  struct sk_buff *skb, const bool send)
{
	char buf[STP_PRINT_BUF_SIZE];
	const struct stp_bpdu *bpdu;
	u16 mstp_len;

	if (send)
		pr_info("[%s -> %s] Send BDPU from %pM",
			br->dev->name, dev->name,
			dev->dev_addr);
	else
		pr_info("[%s <- %s] Recv BDPU from %pM",
			br->dev->name, dev->name,
			&eth_hdr(skb)->h_source);

	if (is_vlan_dev(dev))
		pr_cont(", VLAN %u\n", vlan_dev_vlan_id(dev));
	else
		pr_cont("\n");

	/* Minimum STP Frame size. */
	if (skb->len < 4)
		goto trunc;

	bpdu = (struct stp_bpdu *)skb->data;

	if (get_unaligned_be16(&bpdu->protocol_id)) {
		pr_info(" +-> unknown STP version, length %u", skb->len);

		goto flush;
	}

	pr_info(" +-> STP %s",
		tok2strbuf(stp_proto_values,
			   "Unknown STP protocol (0x%02x)",
			   bpdu->protocol_version,
			   buf, sizeof(buf)));

	switch (bpdu->protocol_version) {
	case STP_PROTO_REGULAR:
	case STP_PROTO_RAPID:
	case STP_PROTO_MSTP:
		break;
	default:
		goto cleanup;
	}

	pr_cont(", %s",
		tok2strbuf(stp_bpdu_type_values,
			   "Unknown BPDU Type (0x%02x)",
			   bpdu->bpdu_type,
			   buf, sizeof(buf)));

	switch (bpdu->bpdu_type) {
	case STP_BPDU_TYPE_CONFIG:
		if (skb->len < sizeof(struct stp_bpdu) - 1)
			goto trunc;

		stp_print_config_bpdu(bpdu, skb->len, buf, sizeof(buf));
		break;

	case STP_BPDU_TYPE_RSTP:
		if (bpdu->protocol_version == STP_PROTO_RAPID) {
			if (skb->len < sizeof(struct stp_bpdu))
				goto trunc;

			stp_print_config_bpdu(bpdu, skb->len,
					      buf, sizeof(buf));

		} else
		if (bpdu->protocol_version == STP_PROTO_MSTP) {
			if (skb->len < STP_BPDU_MSTP_MIN_LEN)
				goto trunc;

			if (bpdu->v1_length != 0)
				/* FIX ME: Emit a message here ? */
				goto trunc;

			/* Validate v3 length */
			mstp_len = get_unaligned_be16(skb->data +
						      MST_BPDU_VER3_LEN_OFFSET);
			mstp_len += 2;  /* length encoding itself is 2 bytes */
			if (skb->len < (sizeof(struct stp_bpdu) + mstp_len))
				goto trunc;

			stp_print_mstp_bpdu(bpdu, skb->len,
					    buf, sizeof(buf));
		}
		break;

	case STP_BPDU_TYPE_TOPO_CHANGE:
		/* always empty message - just break out */
		break;

	default:
		break;
	}

flush:
	pr_cont("\n");

	return;

trunc:
	pr_cont("\n +-> TRUNCATED\n");

cleanup:
	return;
}

void
br_stp_bdpu_recv_print(struct net_bridge *br,
		       struct net_device *dev, struct sk_buff *skb)
{
	br_stp_bdpu_print(br, dev, skb, false);
}

void
br_stp_bdpu_send_print(struct net_bridge *br,
		       struct net_device *dev, struct sk_buff *skb)
{
	br_stp_bdpu_print(br, dev, skb, true);
}
