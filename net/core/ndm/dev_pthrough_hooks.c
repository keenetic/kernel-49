#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>

int (*pppoe_pthrough)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(pppoe_pthrough);

int (*ipv6_pthrough)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ipv6_pthrough);
