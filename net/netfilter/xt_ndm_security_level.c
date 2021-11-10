/* Kernel module to match NDM security level. */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter/xt_ndm_security_level.h>
#include <linux/netfilter/x_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NDM Systems");
MODULE_DESCRIPTION("Xtables: NDM security level match");

static bool ndm_sl_in_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_ndm_security_level *info = par->matchinfo;
	bool ret;

	if (par->in == NULL)
		return false;

	ret = info->ndm_security_level == par->in->ndm_security_level;
	ret ^= info->invert;

	return ret;
}

static bool ndm_sl_out_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_ndm_security_level *info = par->matchinfo;
	bool ret;

	if (par->out == NULL)
		return false;

	ret = info->ndm_security_level == par->out->ndm_security_level;
	ret ^= info->invert;

	return ret;
}

static struct xt_match ndm_sl_mt_reg[] __read_mostly = {
	{
		.name      = "ndmslin",
		.revision  = 0,
		.family    = NFPROTO_UNSPEC,
		.match     = ndm_sl_in_mt,
		.matchsize = sizeof(struct xt_ndm_security_level),
		.hooks     = (1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_IN) |
					(1 << NF_INET_FORWARD),
		.me        = THIS_MODULE,
	},
	{
		.name      = "ndmslout",
		.revision  = 0,
		.family    = NFPROTO_UNSPEC,
		.match     = ndm_sl_out_mt,
		.matchsize = sizeof(struct xt_ndm_security_level),
		.hooks     = (1 << NF_INET_POST_ROUTING) | (1 << NF_INET_LOCAL_OUT) |
					(1 << NF_INET_FORWARD),
		.me        = THIS_MODULE,
	}
};

static int __init ndm_sl_mt_init(void)
{
	return xt_register_matches(ndm_sl_mt_reg, ARRAY_SIZE(ndm_sl_mt_reg));
}

static void __exit ndm_sl_mt_exit(void)
{
	xt_unregister_matches(ndm_sl_mt_reg, ARRAY_SIZE(ndm_sl_mt_reg));
}

module_init(ndm_sl_mt_init);
module_exit(ndm_sl_mt_exit);
