#ifndef _LINUX_NF_NSC_H
#define _LINUX_NF_NSC_H

struct nf_conn;
struct sk_buff;

int nf_nsc_update_dscp(struct nf_conn *ct, struct sk_buff *skb);
void nf_nsc_update_sc_ct(struct nf_conn *ct, const u8 sc);
int nf_nsc_ctmark_to_sc(const u8 mark);

static inline bool nf_nsc_valid_sc(const int sc)
{
	return sc >= 0 && sc <= 6;
}

#endif /* _LINUX_NF_NSC_H */
