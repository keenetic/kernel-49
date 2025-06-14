/*
 * Connection tracking protocol helper module for SCTP.
 *
 * Copyright (c) 2004 Kiran Kumar Immidi <immidi_kiran@yahoo.com>
 * Copyright (c) 2004-2012 Patrick McHardy <kaber@trash.net>
 *
 * SCTP is defined in RFC 2960. References to various sections in this code
 * are to this RFC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/sctp.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_ecache.h>

/* FIXME: Examine ipfilter's timeouts and conntrack transitions more
   closely.  They're more complex. --RR

   And so for me for SCTP :D -Kiran */

static const char *const sctp_conntrack_names[] = {
	"NONE",
	"CLOSED",
	"COOKIE_WAIT",
	"COOKIE_ECHOED",
	"ESTABLISHED",
	"SHUTDOWN_SENT",
	"SHUTDOWN_RECD",
	"SHUTDOWN_ACK_SENT",
	"HEARTBEAT_SENT",
	"HEARTBEAT_ACKED",
};

#define SECS  * HZ
#define MINS  * 60 SECS
#define HOURS * 60 MINS
#define DAYS  * 24 HOURS

static unsigned int sctp_timeouts[SCTP_CONNTRACK_MAX] __read_mostly = {
	[SCTP_CONNTRACK_CLOSED]			= 10 SECS,
	[SCTP_CONNTRACK_COOKIE_WAIT]		= 3 SECS,
	[SCTP_CONNTRACK_COOKIE_ECHOED]		= 3 SECS,
	[SCTP_CONNTRACK_ESTABLISHED]		= 5 DAYS,
	[SCTP_CONNTRACK_SHUTDOWN_SENT]		= 3 SECS,
	[SCTP_CONNTRACK_SHUTDOWN_RECD]		= 3 SECS,
	[SCTP_CONNTRACK_SHUTDOWN_ACK_SENT]	= 3 SECS,
	[SCTP_CONNTRACK_HEARTBEAT_SENT]		= 30 SECS,
	[SCTP_CONNTRACK_HEARTBEAT_ACKED]	= 210 SECS,
};

#define sNO SCTP_CONNTRACK_NONE
#define	sCL SCTP_CONNTRACK_CLOSED
#define	sCW SCTP_CONNTRACK_COOKIE_WAIT
#define	sCE SCTP_CONNTRACK_COOKIE_ECHOED
#define	sES SCTP_CONNTRACK_ESTABLISHED
#define	sSS SCTP_CONNTRACK_SHUTDOWN_SENT
#define	sSR SCTP_CONNTRACK_SHUTDOWN_RECD
#define	sSA SCTP_CONNTRACK_SHUTDOWN_ACK_SENT
#define	sHS SCTP_CONNTRACK_HEARTBEAT_SENT
#define	sHA SCTP_CONNTRACK_HEARTBEAT_ACKED
#define	sIV SCTP_CONNTRACK_MAX

/*
	These are the descriptions of the states:

NOTE: These state names are tantalizingly similar to the states of an
SCTP endpoint. But the interpretation of the states is a little different,
considering that these are the states of the connection and not of an end
point. Please note the subtleties. -Kiran

NONE              - Nothing so far.
COOKIE WAIT       - We have seen an INIT chunk in the original direction, or also
		    an INIT_ACK chunk in the reply direction.
COOKIE ECHOED     - We have seen a COOKIE_ECHO chunk in the original direction.
ESTABLISHED       - We have seen a COOKIE_ACK in the reply direction.
SHUTDOWN_SENT     - We have seen a SHUTDOWN chunk in the original direction.
SHUTDOWN_RECD     - We have seen a SHUTDOWN chunk in the reply directoin.
SHUTDOWN_ACK_SENT - We have seen a SHUTDOWN_ACK chunk in the direction opposite
		    to that of the SHUTDOWN chunk.
CLOSED            - We have seen a SHUTDOWN_COMPLETE chunk in the direction of
		    the SHUTDOWN chunk. Connection is closed.
HEARTBEAT_SENT    - We have seen a HEARTBEAT in a new flow.
HEARTBEAT_ACKED   - We have seen a HEARTBEAT-ACK in the direction opposite to
		    that of the HEARTBEAT chunk. Secondary connection is
		    established.
*/

/* TODO
 - I have assumed that the first INIT is in the original direction.
 This messes things when an INIT comes in the reply direction in CLOSED
 state.
 - Check the error type in the reply dir before transitioning from
cookie echoed to closed.
 - Sec 5.2.4 of RFC 2960
 - Full Multi Homing support.
*/

/* SCTP conntrack state transitions */
static const u8 sctp_conntracks[2][11][SCTP_CONNTRACK_MAX] = {
	{
/*	ORIGINAL	*/
/*                  sNO, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHS, sHA */
/* init         */ {sCW, sCW, sCW, sCE, sES, sCL, sCL, sSA, sCW, sHA},
/* init_ack     */ {sCL, sCL, sCW, sCE, sES, sSS, sSR, sSA, sCL, sHA},
/* abort        */ {sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sCL},
/* shutdown     */ {sCL, sCL, sCW, sCE, sSS, sSS, sSR, sSA, sCL, sSS},
/* shutdown_ack */ {sSA, sCL, sCW, sCE, sES, sSA, sSA, sSA, sSA, sHA},
/* error        */ {sCL, sCL, sCW, sCE, sES, sSS, sSR, sSA, sCL, sHA},/* Can't have Stale cookie*/
/* cookie_echo  */ {sCL, sCL, sCE, sCE, sES, sSS, sSR, sSA, sCL, sHA},/* 5.2.4 - Big TODO */
/* cookie_ack   */ {sCL, sCL, sCW, sCE, sES, sSS, sSR, sSA, sCL, sHA},/* Can't come in orig dir */
/* shutdown_comp*/ {sCL, sCL, sCW, sCE, sES, sSS, sSR, sCL, sCL, sHA},
/* heartbeat    */ {sHS, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHS, sHA},
/* heartbeat_ack*/ {sCL, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHS, sHA}
	},
	{
/*	REPLY	*/
/*                  sNO, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHS, sHA */
/* init         */ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sSA, sIV, sHA},/* INIT in sCL Big TODO */
/* init_ack     */ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sSA, sIV, sHA},
/* abort        */ {sIV, sCL, sCL, sCL, sCL, sCL, sCL, sCL, sIV, sCL},
/* shutdown     */ {sIV, sCL, sCW, sCE, sSR, sSS, sSR, sSA, sIV, sSR},
/* shutdown_ack */ {sIV, sCL, sCW, sCE, sES, sSA, sSA, sSA, sIV, sHA},
/* error        */ {sIV, sCL, sCW, sCL, sES, sSS, sSR, sSA, sIV, sHA},
/* cookie_echo  */ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sSA, sIV, sHA},/* Can't come in reply dir */
/* cookie_ack   */ {sIV, sCL, sCW, sES, sES, sSS, sSR, sSA, sIV, sHA},
/* shutdown_comp*/ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sCL, sIV, sHA},
/* heartbeat    */ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHS, sHA},
/* heartbeat_ack*/ {sIV, sCL, sCW, sCE, sES, sSS, sSR, sSA, sHA, sHA}
	}
};

static int sctp_net_id	__read_mostly;
struct sctp_net {
	struct nf_proto_net pn;
	unsigned int timeouts[SCTP_CONNTRACK_MAX];
};

static inline struct sctp_net *sctp_pernet(struct net *net)
{
	return net_generic(net, sctp_net_id);
}

static bool sctp_pkt_to_tuple(const struct sk_buff *skb, unsigned int dataoff,
			      struct net *net, struct nf_conntrack_tuple *tuple)
{
	const struct sctphdr *hp;
	struct sctphdr _hdr;

	/* Actually only need first 4 bytes to get ports. */
	hp = skb_header_pointer(skb, dataoff, 4, &_hdr);
	if (hp == NULL)
		return false;

	tuple->src.u.sctp.port = hp->source;
	tuple->dst.u.sctp.port = hp->dest;
	return true;
}

static bool sctp_invert_tuple(struct nf_conntrack_tuple *tuple,
			      const struct nf_conntrack_tuple *orig)
{
	tuple->src.u.sctp.port = orig->dst.u.sctp.port;
	tuple->dst.u.sctp.port = orig->src.u.sctp.port;
	return true;
}

/* Print out the per-protocol part of the tuple. */
static void sctp_print_tuple(struct seq_file *s,
			     const struct nf_conntrack_tuple *tuple)
{
	seq_printf(s, "sport=%hu dport=%hu ",
		   ntohs(tuple->src.u.sctp.port),
		   ntohs(tuple->dst.u.sctp.port));
}

/* Print out the private part of the conntrack. */
static void sctp_print_conntrack(struct seq_file *s, struct nf_conn *ct)
{
	seq_printf(s, "%s ", sctp_conntrack_names[ct->proto.sctp.state]);
}

#define for_each_sctp_chunk(skb, sch, _sch, offset, dataoff, count)	\
for ((offset) = (dataoff) + sizeof(sctp_sctphdr_t), (count) = 0;	\
	(offset) < (skb)->len &&					\
	((sch) = skb_header_pointer((skb), (offset), sizeof(_sch), &(_sch)));	\
	(offset) += (ntohs((sch)->length) + 3) & ~3, (count)++)

/* Some validity checks to make sure the chunks are fine */
static int do_basic_checks(struct nf_conn *ct,
			   const struct sk_buff *skb,
			   unsigned int dataoff,
			   unsigned long *map)
{
	u_int32_t offset, count;
	sctp_chunkhdr_t _sch, *sch;
	int flag;

	flag = 0;

	for_each_sctp_chunk (skb, sch, _sch, offset, dataoff, count) {
		pr_debug("Chunk Num: %d  Type: %d\n", count, sch->type);

		if (sch->type == SCTP_CID_INIT ||
		    sch->type == SCTP_CID_INIT_ACK ||
		    sch->type == SCTP_CID_SHUTDOWN_COMPLETE)
			flag = 1;

		/*
		 * Cookie Ack/Echo chunks not the first OR
		 * Init / Init Ack / Shutdown compl chunks not the only chunks
		 * OR zero-length.
		 */
		if (((sch->type == SCTP_CID_COOKIE_ACK ||
		      sch->type == SCTP_CID_COOKIE_ECHO ||
		      flag) &&
		     count != 0) || !sch->length) {
			pr_debug("Basic checks failed\n");
			return 1;
		}

		if (map)
			set_bit(sch->type, map);
	}

	pr_debug("Basic checks passed\n");
	return count == 0;
}

static int sctp_new_state(enum ip_conntrack_dir dir,
			  enum sctp_conntrack cur_state,
			  int chunk_type)
{
	int i;

	pr_debug("Chunk type: %d\n", chunk_type);

	switch (chunk_type) {
	case SCTP_CID_INIT:
		pr_debug("SCTP_CID_INIT\n");
		i = 0;
		break;
	case SCTP_CID_INIT_ACK:
		pr_debug("SCTP_CID_INIT_ACK\n");
		i = 1;
		break;
	case SCTP_CID_ABORT:
		pr_debug("SCTP_CID_ABORT\n");
		i = 2;
		break;
	case SCTP_CID_SHUTDOWN:
		pr_debug("SCTP_CID_SHUTDOWN\n");
		i = 3;
		break;
	case SCTP_CID_SHUTDOWN_ACK:
		pr_debug("SCTP_CID_SHUTDOWN_ACK\n");
		i = 4;
		break;
	case SCTP_CID_ERROR:
		pr_debug("SCTP_CID_ERROR\n");
		i = 5;
		break;
	case SCTP_CID_COOKIE_ECHO:
		pr_debug("SCTP_CID_COOKIE_ECHO\n");
		i = 6;
		break;
	case SCTP_CID_COOKIE_ACK:
		pr_debug("SCTP_CID_COOKIE_ACK\n");
		i = 7;
		break;
	case SCTP_CID_SHUTDOWN_COMPLETE:
		pr_debug("SCTP_CID_SHUTDOWN_COMPLETE\n");
		i = 8;
		break;
	case SCTP_CID_HEARTBEAT:
		pr_debug("SCTP_CID_HEARTBEAT");
		i = 9;
		break;
	case SCTP_CID_HEARTBEAT_ACK:
		pr_debug("SCTP_CID_HEARTBEAT_ACK");
		i = 10;
		break;
	default:
		/* Other chunks like DATA or SACK do not change the state */
		pr_debug("Unknown chunk type, Will stay in %s\n",
			 sctp_conntrack_names[cur_state]);
		return cur_state;
	}

	pr_debug("dir: %d   cur_state: %s  chunk_type: %d  new_state: %s\n",
		 dir, sctp_conntrack_names[cur_state], chunk_type,
		 sctp_conntrack_names[sctp_conntracks[dir][i][cur_state]]);

	return sctp_conntracks[dir][i][cur_state];
}

static unsigned int *sctp_get_timeouts(struct net *net)
{
	return sctp_pernet(net)->timeouts;
}

/* Returns verdict for packet, or -NF_ACCEPT for invalid. */
static int sctp_packet(struct nf_conn *ct,
		       const struct sk_buff *skb,
		       unsigned int dataoff,
		       enum ip_conntrack_info ctinfo,
		       u_int8_t pf,
		       unsigned int hooknum,
		       unsigned int *timeouts)
{
	enum sctp_conntrack new_state, old_state;
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	const struct sctphdr *sh;
	struct sctphdr _sctph;
	const struct sctp_chunkhdr *sch;
	struct sctp_chunkhdr _sch;
	u_int32_t offset, count;
	unsigned long map[256 / sizeof(unsigned long)] = { 0 };

	sh = skb_header_pointer(skb, dataoff, sizeof(_sctph), &_sctph);
	if (sh == NULL)
		goto out;

	if (do_basic_checks(ct, skb, dataoff, map) != 0)
		goto out;

	/* Check the verification tag (Sec 8.5) */
	if (!test_bit(SCTP_CID_INIT, map) &&
	    !test_bit(SCTP_CID_SHUTDOWN_COMPLETE, map) &&
	    !test_bit(SCTP_CID_COOKIE_ECHO, map) &&
	    !test_bit(SCTP_CID_ABORT, map) &&
	    !test_bit(SCTP_CID_SHUTDOWN_ACK, map) &&
	    !test_bit(SCTP_CID_HEARTBEAT, map) &&
	    !test_bit(SCTP_CID_HEARTBEAT_ACK, map) &&
	    sh->vtag != ct->proto.sctp.vtag[dir]) {
		pr_debug("Verification tag check failed\n");
		goto out;
	}

	old_state = new_state = SCTP_CONNTRACK_NONE;
	spin_lock_bh(&ct->lock);
	for_each_sctp_chunk (skb, sch, _sch, offset, dataoff, count) {
		/* Special cases of Verification tag check (Sec 8.5.1) */
		if (sch->type == SCTP_CID_INIT) {
			/* Sec 8.5.1 (A) */
			if (sh->vtag != 0)
				goto out_unlock;
		} else if (sch->type == SCTP_CID_ABORT) {
			/* Sec 8.5.1 (B) */
			if (sh->vtag != ct->proto.sctp.vtag[dir] &&
			    sh->vtag != ct->proto.sctp.vtag[!dir])
				goto out_unlock;
		} else if (sch->type == SCTP_CID_SHUTDOWN_COMPLETE) {
			/* Sec 8.5.1 (C) */
			if (sh->vtag != ct->proto.sctp.vtag[dir] &&
			    sh->vtag != ct->proto.sctp.vtag[!dir] &&
			    sch->flags & SCTP_CHUNK_FLAG_T)
				goto out_unlock;
		} else if (sch->type == SCTP_CID_COOKIE_ECHO) {
			/* Sec 8.5.1 (D) */
			if (sh->vtag != ct->proto.sctp.vtag[dir])
				goto out_unlock;
		} else if (sch->type == SCTP_CID_HEARTBEAT ||
			   sch->type == SCTP_CID_HEARTBEAT_ACK) {
			if (ct->proto.sctp.vtag[dir] == 0) {
				pr_debug("Setting vtag %x for dir %d\n",
					 sh->vtag, dir);
				ct->proto.sctp.vtag[dir] = sh->vtag;
			} else if (sh->vtag != ct->proto.sctp.vtag[dir]) {
				pr_debug("Verification tag check failed\n");
				goto out_unlock;
			}
		}

		old_state = ct->proto.sctp.state;
		new_state = sctp_new_state(dir, old_state, sch->type);

		/* Invalid */
		if (new_state == SCTP_CONNTRACK_MAX) {
			pr_debug("nf_conntrack_sctp: Invalid dir=%i ctype=%u "
				 "conntrack=%u\n",
				 dir, sch->type, old_state);
			goto out_unlock;
		}

		/* If it is an INIT or an INIT ACK note down the vtag */
		if (sch->type == SCTP_CID_INIT ||
		    sch->type == SCTP_CID_INIT_ACK) {
			sctp_inithdr_t _inithdr, *ih;

			ih = skb_header_pointer(skb, offset + sizeof(sctp_chunkhdr_t),
						sizeof(_inithdr), &_inithdr);
			if (ih == NULL)
				goto out_unlock;
			pr_debug("Setting vtag %x for dir %d\n",
				 ih->init_tag, !dir);
			ct->proto.sctp.vtag[!dir] = ih->init_tag;
		}

		ct->proto.sctp.state = new_state;
		if (old_state != new_state)
			nf_conntrack_event_cache(IPCT_PROTOINFO, ct);
	}
	spin_unlock_bh(&ct->lock);

	nf_ct_refresh_acct(ct, ctinfo, skb, timeouts[new_state]);

	if (old_state == SCTP_CONNTRACK_COOKIE_ECHOED &&
	    dir == IP_CT_DIR_REPLY &&
	    new_state == SCTP_CONNTRACK_ESTABLISHED) {
		pr_debug("Setting assured bit\n");
		set_bit(IPS_ASSURED_BIT, &ct->status);
		nf_conntrack_event_cache(IPCT_ASSURED, ct);
	}

	return NF_ACCEPT;

out_unlock:
	spin_unlock_bh(&ct->lock);
out:
	return -NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static bool sctp_new(struct nf_conn *ct, const struct sk_buff *skb,
		     unsigned int dataoff, unsigned int *timeouts)
{
	enum sctp_conntrack new_state;
	const struct sctphdr *sh;
	struct sctphdr _sctph;
	const struct sctp_chunkhdr *sch;
	struct sctp_chunkhdr _sch;
	u_int32_t offset, count;
	unsigned long map[256 / sizeof(unsigned long)] = { 0 };

	sh = skb_header_pointer(skb, dataoff, sizeof(_sctph), &_sctph);
	if (sh == NULL)
		return false;

	if (do_basic_checks(ct, skb, dataoff, map) != 0)
		return false;

	/* If an OOTB packet has any of these chunks discard (Sec 8.4) */
	if (test_bit(SCTP_CID_ABORT, map) ||
	    test_bit(SCTP_CID_SHUTDOWN_COMPLETE, map) ||
	    test_bit(SCTP_CID_COOKIE_ACK, map))
		return false;

	memset(&ct->proto.sctp, 0, sizeof(ct->proto.sctp));
	new_state = SCTP_CONNTRACK_MAX;
	for_each_sctp_chunk (skb, sch, _sch, offset, dataoff, count) {
		/* Don't need lock here: this conntrack not in circulation yet */
		new_state = sctp_new_state(IP_CT_DIR_ORIGINAL,
					   SCTP_CONNTRACK_NONE, sch->type);

		/* Invalid: delete conntrack */
		if (new_state == SCTP_CONNTRACK_NONE ||
		    new_state == SCTP_CONNTRACK_MAX) {
			pr_debug("nf_conntrack_sctp: invalid new deleting.\n");
			return false;
		}

		/* Copy the vtag into the state info */
		if (sch->type == SCTP_CID_INIT) {
			if (sh->vtag == 0) {
				sctp_inithdr_t _inithdr, *ih;

				ih = skb_header_pointer(skb, offset + sizeof(sctp_chunkhdr_t),
							sizeof(_inithdr), &_inithdr);
				if (ih == NULL)
					return false;

				pr_debug("Setting vtag %x for new conn\n",
					 ih->init_tag);

				ct->proto.sctp.vtag[IP_CT_DIR_REPLY] =
								ih->init_tag;
			} else {
				/* Sec 8.5.1 (A) */
				return false;
			}
		} else if (sch->type == SCTP_CID_HEARTBEAT) {
			pr_debug("Setting vtag %x for secondary conntrack\n",
				 sh->vtag);
			ct->proto.sctp.vtag[IP_CT_DIR_ORIGINAL] = sh->vtag;
		}
		/* If it is a shutdown ack OOTB packet, we expect a return
		   shutdown complete, otherwise an ABORT Sec 8.4 (5) and (8) */
		else {
			pr_debug("Setting vtag %x for new conn OOTB\n",
				 sh->vtag);
			ct->proto.sctp.vtag[IP_CT_DIR_REPLY] = sh->vtag;
		}

		ct->proto.sctp.state = new_state;
	}

	return true;
}

#if IS_ENABLED(CONFIG_NF_CT_NETLINK)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

static int sctp_to_nlattr(struct sk_buff *skb, struct nlattr *nla,
			  struct nf_conn *ct)
{
	struct nlattr *nest_parms;

	spin_lock_bh(&ct->lock);
	nest_parms = nla_nest_start(skb, CTA_PROTOINFO_SCTP | NLA_F_NESTED);
	if (!nest_parms)
		goto nla_put_failure;

	if (nla_put_u8(skb, CTA_PROTOINFO_SCTP_STATE, ct->proto.sctp.state) ||
	    nla_put_be32(skb, CTA_PROTOINFO_SCTP_VTAG_ORIGINAL,
			 ct->proto.sctp.vtag[IP_CT_DIR_ORIGINAL]) ||
	    nla_put_be32(skb, CTA_PROTOINFO_SCTP_VTAG_REPLY,
			 ct->proto.sctp.vtag[IP_CT_DIR_REPLY]))
		goto nla_put_failure;

	spin_unlock_bh(&ct->lock);

	nla_nest_end(skb, nest_parms);

	return 0;

nla_put_failure:
	spin_unlock_bh(&ct->lock);
	return -1;
}

static const struct nla_policy sctp_nla_policy[CTA_PROTOINFO_SCTP_MAX+1] = {
	[CTA_PROTOINFO_SCTP_STATE]	    = { .type = NLA_U8 },
	[CTA_PROTOINFO_SCTP_VTAG_ORIGINAL]  = { .type = NLA_U32 },
	[CTA_PROTOINFO_SCTP_VTAG_REPLY]     = { .type = NLA_U32 },
};

static int nlattr_to_sctp(struct nlattr *cda[], struct nf_conn *ct)
{
	struct nlattr *attr = cda[CTA_PROTOINFO_SCTP];
	struct nlattr *tb[CTA_PROTOINFO_SCTP_MAX+1];
	int err;

	/* updates may not contain the internal protocol info, skip parsing */
	if (!attr)
		return 0;

	err = nla_parse_nested(tb,
			       CTA_PROTOINFO_SCTP_MAX,
			       attr,
			       sctp_nla_policy);
	if (err < 0)
		return err;

	if (!tb[CTA_PROTOINFO_SCTP_STATE] ||
	    !tb[CTA_PROTOINFO_SCTP_VTAG_ORIGINAL] ||
	    !tb[CTA_PROTOINFO_SCTP_VTAG_REPLY])
		return -EINVAL;

	spin_lock_bh(&ct->lock);
	ct->proto.sctp.state = nla_get_u8(tb[CTA_PROTOINFO_SCTP_STATE]);
	ct->proto.sctp.vtag[IP_CT_DIR_ORIGINAL] =
		nla_get_be32(tb[CTA_PROTOINFO_SCTP_VTAG_ORIGINAL]);
	ct->proto.sctp.vtag[IP_CT_DIR_REPLY] =
		nla_get_be32(tb[CTA_PROTOINFO_SCTP_VTAG_REPLY]);
	spin_unlock_bh(&ct->lock);

	return 0;
}

static int sctp_nlattr_size(void)
{
	return nla_total_size(0)	/* CTA_PROTOINFO_SCTP */
		+ nla_policy_len(sctp_nla_policy, CTA_PROTOINFO_SCTP_MAX + 1);
}
#endif

#if IS_ENABLED(CONFIG_NF_CT_NETLINK_TIMEOUT)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_cttimeout.h>

static int sctp_timeout_nlattr_to_obj(struct nlattr *tb[],
				      struct net *net, void *data)
{
	unsigned int *timeouts = data;
	struct sctp_net *sn = sctp_pernet(net);
	int i;

	/* set default SCTP timeouts. */
	for (i=0; i<SCTP_CONNTRACK_MAX; i++)
		timeouts[i] = sn->timeouts[i];

	/* there's a 1:1 mapping between attributes and protocol states. */
	for (i=CTA_TIMEOUT_SCTP_UNSPEC+1; i<CTA_TIMEOUT_SCTP_MAX+1; i++) {
		if (tb[i]) {
			timeouts[i] = ntohl(nla_get_be32(tb[i])) * HZ;
		}
	}
	return 0;
}

static int
sctp_timeout_obj_to_nlattr(struct sk_buff *skb, const void *data)
{
        const unsigned int *timeouts = data;
	int i;

	for (i=CTA_TIMEOUT_SCTP_UNSPEC+1; i<CTA_TIMEOUT_SCTP_MAX+1; i++) {
	        if (nla_put_be32(skb, i, htonl(timeouts[i] / HZ)))
			goto nla_put_failure;
	}
        return 0;

nla_put_failure:
        return -ENOSPC;
}

static const struct nla_policy
sctp_timeout_nla_policy[CTA_TIMEOUT_SCTP_MAX+1] = {
	[CTA_TIMEOUT_SCTP_CLOSED]		= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_COOKIE_WAIT]		= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_COOKIE_ECHOED]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_ESTABLISHED]		= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_SHUTDOWN_SENT]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_SHUTDOWN_RECD]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_SHUTDOWN_ACK_SENT]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_HEARTBEAT_SENT]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_SCTP_HEARTBEAT_ACKED]	= { .type = NLA_U32 },
};
#endif /* CONFIG_NF_CT_NETLINK_TIMEOUT */


#ifdef CONFIG_SYSCTL
static struct ctl_table sctp_sysctl_table[] = {
	{
		.procname	= "nf_conntrack_sctp_timeout_closed",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_cookie_wait",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_cookie_echoed",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_established",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_shutdown_sent",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_shutdown_recd",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_shutdown_ack_sent",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_heartbeat_sent",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "nf_conntrack_sctp_timeout_heartbeat_acked",
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{ }
};
#endif

static int sctp_kmemdup_sysctl_table(struct nf_proto_net *pn,
				     struct sctp_net *sn)
{
#ifdef CONFIG_SYSCTL
	if (pn->ctl_table)
		return 0;

	pn->ctl_table = kmemdup(sctp_sysctl_table,
				sizeof(sctp_sysctl_table),
				GFP_KERNEL);
	if (!pn->ctl_table)
		return -ENOMEM;

	pn->ctl_table[0].data = &sn->timeouts[SCTP_CONNTRACK_CLOSED];
	pn->ctl_table[1].data = &sn->timeouts[SCTP_CONNTRACK_COOKIE_WAIT];
	pn->ctl_table[2].data = &sn->timeouts[SCTP_CONNTRACK_COOKIE_ECHOED];
	pn->ctl_table[3].data = &sn->timeouts[SCTP_CONNTRACK_ESTABLISHED];
	pn->ctl_table[4].data = &sn->timeouts[SCTP_CONNTRACK_SHUTDOWN_SENT];
	pn->ctl_table[5].data = &sn->timeouts[SCTP_CONNTRACK_SHUTDOWN_RECD];
	pn->ctl_table[6].data = &sn->timeouts[SCTP_CONNTRACK_SHUTDOWN_ACK_SENT];
	pn->ctl_table[7].data = &sn->timeouts[SCTP_CONNTRACK_HEARTBEAT_SENT];
	pn->ctl_table[8].data = &sn->timeouts[SCTP_CONNTRACK_HEARTBEAT_ACKED];
#endif
	return 0;
}

static int sctp_init_net(struct net *net, u_int16_t proto)
{
	struct sctp_net *sn = sctp_pernet(net);
	struct nf_proto_net *pn = &sn->pn;

	if (!pn->users) {
		int i;

		for (i = 0; i < SCTP_CONNTRACK_MAX; i++)
			sn->timeouts[i] = sctp_timeouts[i];
	}

	return sctp_kmemdup_sysctl_table(pn, sn);
}

static struct nf_conntrack_l4proto nf_conntrack_l4proto_sctp4 __read_mostly = {
	.l3proto		= PF_INET,
	.l4proto 		= IPPROTO_SCTP,
	.name 			= "sctp",
	.pkt_to_tuple 		= sctp_pkt_to_tuple,
	.invert_tuple 		= sctp_invert_tuple,
	.print_tuple 		= sctp_print_tuple,
	.print_conntrack	= sctp_print_conntrack,
	.packet 		= sctp_packet,
	.get_timeouts		= sctp_get_timeouts,
	.new 			= sctp_new,
	.me 			= THIS_MODULE,
#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
	.to_nlattr		= sctp_to_nlattr,
	.nlattr_size		= sctp_nlattr_size,
	.from_nlattr		= nlattr_to_sctp,
	.tuple_to_nlattr	= nf_ct_port_tuple_to_nlattr,
	.nlattr_tuple_size	= nf_ct_port_nlattr_tuple_size,
	.nlattr_to_tuple	= nf_ct_port_nlattr_to_tuple,
	.nla_policy		= nf_ct_port_nla_policy,
#endif
#if IS_ENABLED(CONFIG_NF_CT_NETLINK_TIMEOUT)
	.ctnl_timeout		= {
		.nlattr_to_obj	= sctp_timeout_nlattr_to_obj,
		.obj_to_nlattr	= sctp_timeout_obj_to_nlattr,
		.nlattr_max	= CTA_TIMEOUT_SCTP_MAX,
		.obj_size	= sizeof(unsigned int) * SCTP_CONNTRACK_MAX,
		.nla_policy	= sctp_timeout_nla_policy,
	},
#endif /* CONFIG_NF_CT_NETLINK_TIMEOUT */
	.net_id			= &sctp_net_id,
	.init_net		= sctp_init_net,
};

static struct nf_conntrack_l4proto nf_conntrack_l4proto_sctp6 __read_mostly = {
	.l3proto		= PF_INET6,
	.l4proto 		= IPPROTO_SCTP,
	.name 			= "sctp",
	.pkt_to_tuple 		= sctp_pkt_to_tuple,
	.invert_tuple 		= sctp_invert_tuple,
	.print_tuple 		= sctp_print_tuple,
	.print_conntrack	= sctp_print_conntrack,
	.packet 		= sctp_packet,
	.get_timeouts		= sctp_get_timeouts,
	.new 			= sctp_new,
	.me 			= THIS_MODULE,
#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
	.to_nlattr		= sctp_to_nlattr,
	.nlattr_size		= sctp_nlattr_size,
	.from_nlattr		= nlattr_to_sctp,
	.tuple_to_nlattr	= nf_ct_port_tuple_to_nlattr,
	.nlattr_tuple_size	= nf_ct_port_nlattr_tuple_size,
	.nlattr_to_tuple	= nf_ct_port_nlattr_to_tuple,
	.nla_policy		= nf_ct_port_nla_policy,
#if IS_ENABLED(CONFIG_NF_CT_NETLINK_TIMEOUT)
	.ctnl_timeout		= {
		.nlattr_to_obj	= sctp_timeout_nlattr_to_obj,
		.obj_to_nlattr	= sctp_timeout_obj_to_nlattr,
		.nlattr_max	= CTA_TIMEOUT_SCTP_MAX,
		.obj_size	= sizeof(unsigned int) * SCTP_CONNTRACK_MAX,
		.nla_policy	= sctp_timeout_nla_policy,
	},
#endif /* CONFIG_NF_CT_NETLINK_TIMEOUT */
#endif
	.net_id			= &sctp_net_id,
	.init_net		= sctp_init_net,
};

static int sctp_net_init(struct net *net)
{
	int ret = 0;

	ret = nf_ct_l4proto_pernet_register(net, &nf_conntrack_l4proto_sctp4);
	if (ret < 0) {
		pr_err("nf_conntrack_sctp4: pernet registration failed.\n");
		goto out;
	}
	ret = nf_ct_l4proto_pernet_register(net, &nf_conntrack_l4proto_sctp6);
	if (ret < 0) {
		pr_err("nf_conntrack_sctp6: pernet registration failed.\n");
		goto cleanup_sctp4;
	}
	return 0;

cleanup_sctp4:
	nf_ct_l4proto_pernet_unregister(net, &nf_conntrack_l4proto_sctp4);
out:
	return ret;
}

static void sctp_net_exit(struct net *net)
{
	nf_ct_l4proto_pernet_unregister(net, &nf_conntrack_l4proto_sctp6);
	nf_ct_l4proto_pernet_unregister(net, &nf_conntrack_l4proto_sctp4);
}

static struct pernet_operations sctp_net_ops = {
	.init = sctp_net_init,
	.exit = sctp_net_exit,
	.id   = &sctp_net_id,
	.size = sizeof(struct sctp_net),
};

static int __init nf_conntrack_proto_sctp_init(void)
{
	int ret;

	ret = register_pernet_subsys(&sctp_net_ops);
	if (ret < 0)
		goto out_pernet;

	ret = nf_ct_l4proto_register(&nf_conntrack_l4proto_sctp4);
	if (ret < 0)
		goto out_sctp4;

	ret = nf_ct_l4proto_register(&nf_conntrack_l4proto_sctp6);
	if (ret < 0)
		goto out_sctp6;

	return 0;
out_sctp6:
	nf_ct_l4proto_unregister(&nf_conntrack_l4proto_sctp4);
out_sctp4:
	unregister_pernet_subsys(&sctp_net_ops);
out_pernet:
	return ret;
}

static void __exit nf_conntrack_proto_sctp_fini(void)
{
	nf_ct_l4proto_unregister(&nf_conntrack_l4proto_sctp6);
	nf_ct_l4proto_unregister(&nf_conntrack_l4proto_sctp4);
	unregister_pernet_subsys(&sctp_net_ops);
}

module_init(nf_conntrack_proto_sctp_init);
module_exit(nf_conntrack_proto_sctp_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kiran Kumar Immidi");
MODULE_DESCRIPTION("Netfilter connection tracking protocol helper for SCTP");
MODULE_ALIAS("ip_conntrack_proto_sctp");
