/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2005 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <linux/netfilter/x_tables.h>
#include <uapi/linux/netfilter/xt_connskip.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rusty Russell <rusty@rustcorp.com.au>");
MODULE_DESCRIPTION("ip[6]_tables connection tracking skip first match module");

static bool
connskip_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_connskip_info *sinfo = par->matchinfo;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	struct nf_conn_counter *counters;
	struct nf_conn_acct *acct;
	u64 pkts;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct)
		return false;

	acct = nf_conn_acct_find(ct);

	if (!acct)
		return false;

	counters = acct->counter;

	pkts = atomic64_read(&counters[IP_CT_DIR_ORIGINAL].packets);

	return pkts < (u64)sinfo->skipcount;
}

static int connskip_mt_check(const struct xt_mtchk_param *par)
{
	int ret;

	ret = nf_ct_l3proto_try_module_get(par->family);
	if (ret < 0)
		pr_info("cannot load conntrack support for proto=%u\n",
			par->family);
	return ret;
}

static void connskip_mt_destroy(const struct xt_mtdtor_param *par)
{
	nf_ct_l3proto_module_put(par->family);
}

static struct xt_match connskip_mt_reg __read_mostly = {
	.name		= "connskip",
	.family		= NFPROTO_UNSPEC,
	.checkentry	= connskip_mt_check,
	.match		= connskip_mt,
	.destroy	= connskip_mt_destroy,
	.matchsize	= sizeof(struct xt_connskip_info),
	.me		= THIS_MODULE,
};

static int __init connskip_mt_init(void)
{
	return xt_register_match(&connskip_mt_reg);
}

static void __exit connskip_mt_exit(void)
{
	xt_unregister_match(&connskip_mt_reg);
}

module_init(connskip_mt_init);
module_exit(connskip_mt_exit);
