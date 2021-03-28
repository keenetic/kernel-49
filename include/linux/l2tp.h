/*
 * L2TP-over-IP socket for L2TPv3.
 *
 * Author: James Chapman <jchapman@katalix.com>
 */
#ifndef _LINUX_L2TP_H_
#define _LINUX_L2TP_H_

#include <linux/in.h>
#include <linux/in6.h>
#include <uapi/linux/l2tp.h>

/* tune skb pad for CPU with L1_CACHE_BYTES >= 128 (e.g. MT7622) */
#if L1_CACHE_BYTES > 64
#define L2TP_SKB_PAD	(NET_SKB_PAD >> 1)
#else
#define L2TP_SKB_PAD	NET_SKB_PAD_ORIG
#endif

#endif
