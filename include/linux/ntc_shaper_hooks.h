#ifndef _LINUX_NTC_SHAPER_HOOKS_H
#define _LINUX_NTC_SHAPER_HOOKS_H

#include <linux/version.h>
#include <linux/spinlock.h>

struct net;
struct sock;
struct sk_buff;
struct nf_conn;

struct swnat_ntc_privdata_t {
	u8			 origin;
	u8			 set_ce;
};

struct ntc_shaper_fwd_t {
	uint32_t	 saddr_ext;
	uint32_t	 daddr_ext;
	int		 (*okfn_nf)(struct net *,
				    struct sock *,
				    struct sk_buff *);
	int		 (*okfn_custom)(struct sk_buff *, void *);
	void		*data;
	struct net	*net;
	struct sock	*sk;
};

typedef bool
ntc_shaper_bound_hook_fn(
	uint32_t ipaddr,
	const uint8_t *const mac,
	const struct nf_conn *const ct);

extern ntc_shaper_bound_hook_fn *ntc_shaper_check_bound_hook;

typedef unsigned int
ntc_shaper_hook_fn(struct sk_buff *skb,
		   const struct ntc_shaper_fwd_t *const sfwd);

extern rwlock_t ntc_shaper_lock;
extern ntc_shaper_hook_fn *ntc_shaper_ingress_hook;
extern ntc_shaper_hook_fn *ntc_shaper_egress_hook;

static inline ntc_shaper_hook_fn *
ntc_shaper_ingress_hook_get(void)
{
	read_lock_bh(&ntc_shaper_lock);

	return ntc_shaper_ingress_hook;
}

static inline void
ntc_shaper_ingress_hook_put(void)
{
	read_unlock_bh(&ntc_shaper_lock);
}

static inline ntc_shaper_hook_fn *
ntc_shaper_egress_hook_get(void)
{
	read_lock_bh(&ntc_shaper_lock);

	return ntc_shaper_egress_hook;
}

static inline void
ntc_shaper_egress_hook_put(void)
{
	read_unlock_bh(&ntc_shaper_lock);
}

static inline void
ntc_shaper_hooks_set(ntc_shaper_hook_fn *ingress_hook,
		     ntc_shaper_hook_fn *egress_hook)
{
	write_lock_bh(&ntc_shaper_lock);
	ntc_shaper_ingress_hook = ingress_hook;
	ntc_shaper_egress_hook = egress_hook;
	write_unlock_bh(&ntc_shaper_lock);
}

#ifdef CONFIG_NF_CONNTRACK_CUSTOM

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>

/* Must be no more than 128 bits long */
struct nf_ct_ext_ntc_label {
	int32_t wan_iface;
	int32_t lan_iface;
	char mac[ETH_ALEN];
	uint8_t flags;
};

#define NF_CT_EXT_NTC_MAC_SET			0x1
#define NF_CT_EXT_NTC_FROM_LAN			0x2

_Static_assert(sizeof(struct nf_ct_ext_ntc_label) <= 16,
	       "invalid struct nf_ct_ext_ntc_label size");

extern enum nf_ct_ext_id nf_ct_ext_id_ntc;

static inline void *nf_ct_ext_add_ntc_(struct nf_conn *ct)
{
	if (unlikely(nf_ct_ext_id_ntc == 0))
		return NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
	return nf_ct_ext_add(ct, nf_ct_ext_id_ntc, GFP_ATOMIC);
#else
	return __nf_ct_ext_add_length(ct, nf_ct_ext_id_ntc,
		sizeof(struct nf_ct_ext_ntc_label), GFP_ATOMIC);
#endif
}

static inline struct nf_ct_ext_ntc_label *nf_ct_ext_find_ntc(
					  const struct nf_conn *ct)
{
	return (struct nf_ct_ext_ntc_label *)
		__nf_ct_ext_find(ct, nf_ct_ext_id_ntc);
}

static inline struct nf_ct_ext_ntc_label *nf_ct_ext_add_ntc(struct nf_conn *ct)
{
	struct nf_ct_ext_ntc_label *lbl = nf_ct_ext_add_ntc_(ct);

	if (unlikely(lbl == NULL))
		return NULL;

	lbl->wan_iface = 0;
	lbl->lan_iface = 0;
	lbl->flags = 0;
	memset(&lbl->mac, 0, sizeof(lbl->mac));

	return lbl;
}

static inline bool nf_ct_ext_ntc_filled(struct nf_ct_ext_ntc_label *lbl)
{
	return lbl != NULL && lbl->wan_iface > 0 && lbl->lan_iface > 0;
}

static inline bool nf_ct_ext_ntc_mac_isset(struct nf_ct_ext_ntc_label *lbl)
{
	return lbl != NULL && (lbl->flags & NF_CT_EXT_NTC_MAC_SET);
}

static inline bool nf_ct_ext_ntc_from_lan(struct nf_ct_ext_ntc_label *lbl)
{
	return lbl != NULL && (lbl->flags & NF_CT_EXT_NTC_FROM_LAN);
}

#endif

#endif
