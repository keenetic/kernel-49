#ifndef _RA_NAT_WANTED
#define _RA_NAT_WANTED

#include <linux/ip.h>
#include <linux/ipv6.h>

struct PdmaRxDescInfo4 {
	uint16_t MAGIC_TAG;
#ifdef __BIG_ENDIAN
	uint32_t IF:8;
	uint32_t ALG:1;
	uint32_t SPORT:4;
	uint32_t CRSN:5;
	uint32_t FOE_Entry:14;
#else
	uint32_t FOE_Entry:14;
	uint32_t CRSN:5;
	uint32_t SPORT:4;
	uint32_t ALG:1;
	uint32_t IF:8;
#endif
}  __packed;

/*
 * DEFINITIONS AND MACROS
 */

/*
 *    2bytes         4bytes
 * +-----------+-------------------+
 * | Magic Tag | RX/TX Desc info4  |
 * +-----------+-------------------+
 * |<------FOE Flow Info---------->|
 */

#define FOE_INFO_LEN			6
#define FOE_MAGIC_EXTIF			0x7274
#define FOE_MAGIC_PCI			FOE_MAGIC_EXTIF
#define FOE_MAGIC_WLAN			FOE_MAGIC_EXTIF
#define FOE_MAGIC_GE			0x7275
#define FOE_MAGIC_PPE			0x7276
#ifdef __BIG_ENDIAN
#define FOE_MAGIC_PPE_DWORD		0x7672ff3fUL	/* HNAT_V2: FOE_Entry=0x3fff */
#else
#define FOE_MAGIC_PPE_DWORD		0x3fff7276UL	/* HNAT_V2: FOE_Entry=0x3fff */
#endif

/* choose one of them to keep HNAT related information in somewhere. */
//#define HNAT_USE_HEADROOM
//#define HNAT_USE_TAILROOM
#define HNAT_USE_SKB_CB

#if defined(HNAT_USE_HEADROOM)

#define IS_SPACE_AVAILABLED(skb)	((skb_headroom(skb) >= FOE_INFO_LEN) ? 1 : 0)
#define FOE_INFO_START_ADDR(skb)	(skb->head)

#elif defined(HNAT_USE_TAILROOM)

#define IS_SPACE_AVAILABLED(skb)	((skb_tailroom(skb) >= FOE_INFO_LEN) ? 1 : 0)
#define FOE_INFO_START_ADDR(skb)	(skb->end - FOE_INFO_LEN)

#elif defined(HNAT_USE_SKB_CB)

//change the position of skb_CB if necessary
#define CB_OFFSET			40
#define IS_SPACE_AVAILABLED(skb)	1
#define FOE_INFO_START_ADDR(skb)	(skb->cb + CB_OFFSET)

#endif

#define FOE_MAGIC_TAG(skb)	((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->MAGIC_TAG
#define FOE_ENTRY_NUM(skb)	((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->FOE_Entry
#define FOE_ALG(skb)		((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->ALG
#define FOE_ENTRY_VALID(skb)   (((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->FOE_Entry != 0x3fff)
#define FOE_AI(skb)		((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->CRSN
#define FOE_SP(skb)		((struct PdmaRxDescInfo4 *)FOE_INFO_START_ADDR(skb))->SPORT

#ifndef UN_HIT
#define UN_HIT 0x0D
#endif
#ifndef HIT_BIND_KEEPALIVE_DUP_OLD_HDR
#define HIT_BIND_KEEPALIVE_DUP_OLD_HDR 0x15
#endif
#ifndef NO_FLOW_IS_ASSIGNED
#define NO_FLOW_IS_ASSIGNED 0x07
#endif

#define IS_MAGIC_TAG_VALID(skb) \
	((FOE_MAGIC_TAG(skb) == FOE_MAGIC_GE))

#define IS_DPORT_PPE_VALID(skb) \
	(*((uint32_t *)(FOE_INFO_START_ADDR(skb))) == FOE_MAGIC_PPE_DWORD)

/* mark flow need skipped from PPE */
#define FOE_ALG_SKIP(skb) \
	if (IS_SPACE_AVAILABLED(skb) && !FOE_ALG(skb) && IS_MAGIC_TAG_VALID(skb)) FOE_ALG(skb) = 1

#define FOE_ALG_MARK(skb) \
	FOE_ALG_SKIP(skb)

#define FOE_SKB_IS_KEEPALIVE(skb) \
	(FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR)

/* reset AI for local output flow */
#define FOE_AI_UNHIT(skb) \
	if (IS_SPACE_AVAILABLED(skb)) FOE_AI(skb) = UN_HIT

/* fast clear FoE Info (magic_tag,entry_num) */
#define DO_FAST_CLEAR_FOE(skb) \
	(*((uint32_t *)(FOE_INFO_START_ADDR(skb))) = 0U)

/* full clear FoE Info */
#define DO_FULL_CLEAR_FOE(skb) \
	(memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN))

/* fast fill FoE desc field */
#define DO_FILL_FOE_DESC(skb,desc) \
	(*((uint32_t *)(FOE_INFO_START_ADDR(skb) + 2)) = (uint32_t)(desc))

/* fast fill FoE desc to DPORT PPE (magic_tag,entry_num) */
#define DO_FILL_FOE_DPORT_PPE(skb) \
	(*((uint32_t *)(FOE_INFO_START_ADDR(skb))) = FOE_MAGIC_PPE_DWORD)

//////////////////////////////////////////////////////////////////////

/* gmac_id */
#define GMAC_ID_MAGIC_EXTIF		0
#define GMAC_ID_GDM1			1
#define GMAC_ID_GDM2			2
#define GMAC_ID_WDMA			3

#if defined(CONFIG_ECONET_EN7512) || \
    defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527)

/* gmac_type */
#define GMAC_TYPE_ETH			0
#define GMAC_TYPE_ATM			1
#define GMAC_TYPE_PTM			2

/* gmac_info fields */
struct gmac_info {
	union {
		struct {
			/* assume BE for all en75xx */
			uint32_t wi_rxid:	1;	/* QDMA RX ring */
			uint32_t atm_pppoa:	1;	/* ATM PPPoA incap */
			uint32_t atm_ipoa:	1;	/* ATM IPoA incap */
			uint32_t atm_mux_vc:	1;	/* ATM VC mux mode */
			uint32_t stag:		12;	/* ATM CLP/UU/XOA to PPE stag */
			uint32_t hwfq:		1;	/* send via QDMA HWFQ */
			uint32_t queue_id:	3;	/* QDMA QoS queue (0..7) */
			uint32_t channel_id:	8;	/* QDMA virtual channel */
			uint32_t gmac_type:	2;	/* 0: ETH, 1: ATM, 2: PTM */
			uint32_t gmac_id:	2;	/* 0: external (WiFi), 1: GDM1, 2: GDM2 */
		} bits;
		uint32_t word;
	};
} __packed;


#elif defined(CONFIG_ECONET_EN7528)

/* gmac_info fields */
struct gmac_info {
	union {
		struct {
			/* assume LE for en7528 */
			uint32_t gmac_id:	2;	/* 0: external (WiFi), 1: GDM1, 2: GDM2, 3: WDMA */
			uint32_t channel_id:	8;	/* QDMA virtual channel */
			uint32_t queue_id:	3;	/* QDMA QoS queue (0..7) */
			uint32_t hwfq:		1;	/* send via QDMA HWFQ */
			uint32_t is_wan:	1;	/* assume upstream path */
			uint32_t resv:		1;
			uint32_t wdmaid:	1;	/* WDMA0/1 */
			uint32_t wi_bssid:	6;	/* BSSID */
			uint32_t wi_wcid:	8;	/* WCID */
			uint32_t wi_rxid:	1;	/* WDMA/QDMA RX ring */
		} bits;
		uint32_t word;
	};
} __packed;

#else

/* gmac_info fields */
struct gmac_info {
	union {
		struct {
			/* assume LE for all mt762x */
			uint32_t gmac_id:	2;	/* 0: external (WiFi), 1: GDM1, 2: GDM2, 3: WDMA */
			uint32_t queue_id:	6;	/* QDMA QoS queue (0..63) */
			uint32_t hwfq:		1;	/* send via QDMA HWFQ */
			uint32_t is_wan:	1;	/* assume upstream path */
			uint32_t wdmaid:	1;	/* WDMA0/1 */
			uint32_t wi_bssid:	6;	/* BSSID */
			uint32_t wi_wcid:	8;	/* WCID */
			uint32_t wi_rxid:	2;	/* WDMA RX ring */
			uint32_t resv:		5;
		} bits;
		uint32_t word;
	};
} __packed;

#endif

extern int (*ra_sw_nat_hook_rx)(struct sk_buff *skb);
extern int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_info);
extern int (*ppe_dev_has_accel_hook)(struct net_device *dev);
extern void (*ppe_dev_register_hook)(struct net_device *dev);
extern void (*ppe_dev_unregister_hook)(struct net_device *dev);
extern void (*ppe_enable_hook)(int do_ppe_enable);

//////////////////////////////////////////////////////////////////////

#endif
