/* This is a module which is used to disable fastnat/hwnat offloading for ct
 */
#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter/x_tables.h>

#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/netfilter/nf_conntrack.h>
#include <linux/netfilter/nf_conntrack_common.h>
#endif

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

MODULE_DESCRIPTION("Xtables: disable flow offloading");
MODULE_LICENSE("GPL");

static unsigned int
ppe_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
#if IS_ENABLED(CONFIG_FAST_NAT)
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);

	if (ct != NULL)
		ct->fast_ext = 1;
#endif

#if IS_ENABLED(CONFIG_RA_HW_NAT)
	FOE_ALG_SKIP(skb);
#endif

	return XT_CONTINUE;
}

static struct xt_target ppe_tg_reg __read_mostly = {
	.name		= "PPE",
	.revision	= 0,
	.family		= NFPROTO_UNSPEC,
	.table		= "mangle",
	.target		= ppe_tg,
	.me		= THIS_MODULE,
};

static int __init ppe_tg_init(void)
{
	return xt_register_target(&ppe_tg_reg);
}

static void __exit ppe_tg_exit(void)
{
	xt_unregister_target(&ppe_tg_reg);
}

module_init(ppe_tg_init);
module_exit(ppe_tg_exit);
