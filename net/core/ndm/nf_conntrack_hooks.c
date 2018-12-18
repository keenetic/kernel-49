#include <linux/nf_conntrack_hooks.h>
#include <net/netfilter/nf_conntrack.h>

void (*nacct_conntrack_free)(struct nf_conn *ct) = NULL;
EXPORT_SYMBOL(nacct_conntrack_free);

void nf_ct_interate(int (*iter)(struct nf_conn *i, void *data), void *data)
{
	nf_ct_iterate_cleanup(&init_net, iter, data, 0, 0);
}
EXPORT_SYMBOL(nf_ct_interate);
