#ifndef _LINUX_NTC_SHAPER_HOOKS_H
#define _LINUX_NTC_SHAPER_HOOKS_H

#include <linux/version.h>
#include <linux/spinlock.h>

struct net;
struct sock;
struct sk_buff;
struct nf_conn;

struct ntc_shaper_fwd_t {
	int		 (*okfn)(struct net *,
				 struct sock *,
				 struct sk_buff *);
	struct net	*net;
	struct sock	*sk;

	bool		is_ipv4;
	bool		is_swnat;
};

typedef bool
ntc_shaper_bound_hook_fn(const struct nf_conn *const ct);

extern ntc_shaper_bound_hook_fn *ntc_shaper_check_bound_hook;

typedef unsigned int
ntc_shaper_hook_fn(struct sk_buff *skb,
		   const struct ntc_shaper_fwd_t *const sfwd);

extern rwlock_t ntc_shaper_lock;
extern ntc_shaper_hook_fn *ntc_shaper_ingress_hook;
extern ntc_shaper_hook_fn *ntc_shaper_egress_hook;
extern ntc_shaper_hook_fn *ntc_shaper_test_hook;

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

static inline ntc_shaper_hook_fn *
ntc_shaper_test_hook_get(void)
{
	read_lock_bh(&ntc_shaper_lock);

	return ntc_shaper_test_hook;
}

static inline void
ntc_shaper_test_hook_put(void)
{
	read_unlock_bh(&ntc_shaper_lock);
}

static inline void
ntc_shaper_hooks_set(ntc_shaper_hook_fn *ingress_hook,
		     ntc_shaper_hook_fn *egress_hook,
		     ntc_shaper_hook_fn *test_hook)
{
	write_lock_bh(&ntc_shaper_lock);
	ntc_shaper_ingress_hook = ingress_hook;
	ntc_shaper_egress_hook = egress_hook;
	ntc_shaper_test_hook = test_hook;
	write_unlock_bh(&ntc_shaper_lock);
}

#ifdef CONFIG_NF_CONNTRACK_CUSTOM

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>

/* Must be no more than 128 bits long */
struct nf_ct_ext_ntc_label {
	s32 wan_iface;
	s32 lan_iface;
	u8  mac[ETH_ALEN];
	u8  flags;
};

#define NF_CT_EXT_NTC_FROM_LAN			(0x1)

_Static_assert(sizeof(struct nf_ct_ext_ntc_label) <= 16,
	       "invalid struct nf_ct_ext_ntc_label size");

extern enum nf_ct_ext_id nf_ct_ext_id_ntc;

static inline struct nf_ct_ext_ntc_label *
nf_ct_ext_find_ntc(const struct nf_conn *ct)
{
	return __nf_ct_ext_find(ct, nf_ct_ext_id_ntc);
}

static inline bool
nf_ct_ext_ntc_filled(const struct nf_ct_ext_ntc_label *lbl)
{
	return lbl->wan_iface > 0 && lbl->lan_iface > 0;
}

static inline bool
nf_ct_ext_ntc_mac_isset(const struct nf_ct_ext_ntc_label *lbl)
{
	return lbl->lan_iface > 0;
}

static inline bool
nf_ct_ext_ntc_from_lan(const struct nf_ct_ext_ntc_label *lbl)
{
	return !!(lbl->flags & NF_CT_EXT_NTC_FROM_LAN);
}

#endif
#endif
