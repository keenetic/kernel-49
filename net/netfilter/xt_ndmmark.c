/*
 *	xt_ndmmark - Netfilter module to match NFMARK value
 *
 *	(C) 1999-2001 Marc Boucher <marc@mbsi.ca>
 *	Copyright © CC Computer Consultants GmbH, 2007 - 2008
 *	Jan Engelhardt <jengelh@medozas.de>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter/xt_ndmmark.h>
#include <linux/netfilter/x_tables.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marc Boucher <marc@mbsi.ca>");
MODULE_DESCRIPTION("Xtables: packet NDM mark operations");

static unsigned int
ndmmark_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct xt_ndmmark_tginfo *info = par->targinfo;

#if IS_ENABLED(CONFIG_RA_HW_NAT)
	FOE_ALG_MARK(skb);
#endif

	skb->ndm_mark = (skb->ndm_mark & ~info->mask) ^ info->mark;

	return XT_CONTINUE;
}

static bool
ndmmark_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_ndmmark_mtinfo *info = par->matchinfo;

	return ((skb->ndm_mark & info->mask) == info->mark) ^ info->invert;
}

static struct xt_target ndmmark_tg_reg __read_mostly = {
	.name           = "NDMMARK",
	.revision       = 0,
	.family         = NFPROTO_UNSPEC,
	.target         = ndmmark_tg,
	.targetsize     = sizeof(struct xt_ndmmark_tginfo),
	.me             = THIS_MODULE,
};

static struct xt_match ndmmark_mt_reg __read_mostly = {
	.name           = "ndmmark",
	.revision       = 0,
	.family         = NFPROTO_UNSPEC,
	.match          = ndmmark_mt,
	.matchsize      = sizeof(struct xt_ndmmark_mtinfo),
	.me             = THIS_MODULE,
};

static int __init ndmmark_mt_init(void)
{
	int ret;

	ret = xt_register_target(&ndmmark_tg_reg);
	if (ret < 0)
		return ret;
	ret = xt_register_match(&ndmmark_mt_reg);
	if (ret < 0) {
		xt_unregister_target(&ndmmark_tg_reg);
		return ret;
	}
	return 0;
}

static void __exit ndmmark_mt_exit(void)
{
	xt_unregister_match(&ndmmark_mt_reg);
	xt_unregister_target(&ndmmark_tg_reg);
}

module_init(ndmmark_mt_init);
module_exit(ndmmark_mt_exit);
