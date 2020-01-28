#ifndef _BR_NF_HOOK_H
#define _BR_NF_HOOK_H

#include <linux/netdevice.h>

#if !IS_ENABLED(CONFIG_BRIDGE_NETFILTER)
static inline int ebtables_run_hooks(void)
{
#if IS_ENABLED(CONFIG_BRIDGE_NF_EBTABLES)
	extern int brnf_call_ebtables;

	return brnf_call_ebtables;
#else
	return 0;
#endif
}
#endif

static inline int
BR_HOOK(uint8_t pf, unsigned int hook, struct net *net, struct sock *sk,
	struct sk_buff *skb, struct net_device *in, struct net_device *out,
	int (*okfn)(struct net *, struct sock *, struct sk_buff *))
{
#if !IS_ENABLED(CONFIG_BRIDGE_NETFILTER)
	if (!ebtables_run_hooks())
		return okfn(net, sk, skb);
#endif
	return NF_HOOK(pf, hook, net, sk, skb, in, out, okfn);
}

#endif
