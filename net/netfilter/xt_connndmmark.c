/*
 *	xt_connndmmark - Netfilter module to operate on connection marks
 *
 *	Copyright (C) 2002,2004 MARA Systems AB <http://www.marasystems.com>
 *	by Henrik Nordstrom <hno@marasystems.com>
 *	Copyright © CC Computer Consultants GmbH, 2007 - 2008
 *	Jan Engelhardt <jengelh@medozas.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_connndmmark.h>

MODULE_AUTHOR("Henrik Nordstrom <hno@marasystems.com>");
MODULE_DESCRIPTION("Xtables: connection NDM mark operations");
MODULE_LICENSE("GPL");

static unsigned int
connndmmark_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct xt_connndmmark_tginfo *info = par->targinfo;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	u_int8_t newmark;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct == NULL || nf_ct_is_untracked(ct))
		return XT_CONTINUE;

	switch (info->mode) {
	case XT_CONNNDMMARK_SET:
		newmark = (ct->ndm_mark & ~info->ctmask) ^ info->ctmark;
		if (ct->ndm_mark != newmark) {
			ct->ndm_mark = newmark;
			nf_conntrack_event_cache(IPCT_NDMMARK, ct);
		}
		break;
	case XT_CONNNDMMARK_RESTORE:
#if IS_ENABLED(CONFIG_NETFILTER_XT_NDMMARK)
		newmark = (skb->ndm_mark & ~info->nfmask) ^
		          (ct->ndm_mark & info->ctmask);
		skb->ndm_mark = newmark;
#endif
		break;
	}

	return XT_CONTINUE;
}

static int connndmmark_tg_check(const struct xt_tgchk_param *par)
{
	int ret;

	ret = nf_ct_l3proto_try_module_get(par->family);
	if (ret < 0)
		pr_info("cannot load conntrack support for proto=%u\n",
			par->family);
	return ret;
}

static void connndmmark_tg_destroy(const struct xt_tgdtor_param *par)
{
	nf_ct_l3proto_module_put(par->family);
}

static bool
connndmmark_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_connndmmark_mtinfo *info = par->matchinfo;
	enum ip_conntrack_info ctinfo;
	const struct nf_conn *ct;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct == NULL || nf_ct_is_untracked(ct))
		return false;

	return ((ct->ndm_mark & info->mask) == info->mark) ^ info->invert;
}

static int connndmmark_mt_check(const struct xt_mtchk_param *par)
{
	int ret;

	ret = nf_ct_l3proto_try_module_get(par->family);
	if (ret < 0)
		pr_info("cannot load conntrack support for proto=%u\n",
			par->family);
	return ret;
}

static void connndmmark_mt_destroy(const struct xt_mtdtor_param *par)
{
	nf_ct_l3proto_module_put(par->family);
}

static struct xt_target connndmmark_tg_reg __read_mostly = {
	.name           = "CONNNDMMARK",
	.revision       = 0,
	.family         = NFPROTO_UNSPEC,
	.checkentry     = connndmmark_tg_check,
	.target         = connndmmark_tg,
	.targetsize     = sizeof(struct xt_connndmmark_tginfo),
	.destroy        = connndmmark_tg_destroy,
	.me             = THIS_MODULE,
};

static struct xt_match connndmmark_mt_reg __read_mostly = {
	.name           = "connndmmark",
	.revision       = 0,
	.family         = NFPROTO_UNSPEC,
	.checkentry     = connndmmark_mt_check,
	.match          = connndmmark_mt,
	.matchsize      = sizeof(struct xt_connndmmark_mtinfo),
	.destroy        = connndmmark_mt_destroy,
	.me             = THIS_MODULE,
};

static int __init connndmmark_mt_init(void)
{
	int ret;

	ret = xt_register_target(&connndmmark_tg_reg);
	if (ret < 0)
		return ret;
	ret = xt_register_match(&connndmmark_mt_reg);
	if (ret < 0) {
		xt_unregister_target(&connndmmark_tg_reg);
		return ret;
	}
	return 0;
}

static void __exit connndmmark_mt_exit(void)
{
	xt_unregister_match(&connndmmark_mt_reg);
	xt_unregister_target(&connndmmark_tg_reg);
}

module_init(connndmmark_mt_init);
module_exit(connndmmark_mt_exit);
