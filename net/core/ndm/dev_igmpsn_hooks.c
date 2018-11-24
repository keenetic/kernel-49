#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>

struct new_mc_streams;

int (*igmpsn_hook)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(igmpsn_hook);

void (*update_mc_streams)(struct new_mc_streams *streams_list) = NULL;
EXPORT_SYMBOL(update_mc_streams);

