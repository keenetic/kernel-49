#ifndef _WIFI_HOSTADP_H_
#define _WIFI_HOSTADP_H_

#include <linux/types.h>

struct sk_buff;
struct net_device;
struct napi_struct;

struct sk_buff *wifi_hostadp_skb_rx(u32 ring_id, u32 *vnd, u8 *resch);
int wifi_hostadp_skb_tx(u32 ring_id, struct sk_buff *skb, u8 *vnd, u32 vnd_len);
int wifi_hostadp_reg_napi_rx(u32 ring_id, struct napi_struct *ns);
int wifi_hostadp_set_napi_dev(u32 ring_id, struct net_device *nd);
int wifi_hostadp_enable_int_rx(u32 ring_id);

#endif /* _WIFI_HOSTADP_H_ */
