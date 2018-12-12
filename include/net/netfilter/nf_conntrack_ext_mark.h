#ifndef __NF_CONNTRACK_EXT_MARK_H__
#define __NF_CONNTRACK_EXT_MARK_H__

#define NF_CT_DEFAULT_MARK 0

#include <net/netfilter/nf_conntrack_extend.h>

struct nf_conntrack_ext_mark {
	u16	mark;
};

static inline u16 nf_ct_mark(const struct nf_conn *ct)
{
	struct nf_conntrack_ext_mark *nf_mark;
	nf_mark = nf_ct_ext_find(ct, NF_CT_EXT_MARK);
	if (nf_mark)
		return nf_mark->mark;
	return NF_CT_DEFAULT_MARK;
}

#endif
