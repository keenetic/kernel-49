#ifndef __INCLUDE_LINUX_NF_CONNTRACK_HOOKS_H
#define __INCLUDE_LINUX_NF_CONNTRACK_HOOKS_H

struct nf_conn;

extern void (*nacct_conntrack_free)(struct nf_conn *ct);

extern void nf_ct_interate(int (*iter)(struct nf_conn *i, void *data), void *data);

#endif /* __INCLUDE_LINUX_NF_CONNTRACK_HOOKS_H */
