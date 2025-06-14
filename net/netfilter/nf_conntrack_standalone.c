/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 * (C) 2005-2012 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <linux/netdevice.h>
#include <linux/security.h>
#include <linux/inet.h>
#include <net/net_namespace.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_timestamp.h>
#include <net/netfilter/nf_conntrack_ext_mark.h>
#include <linux/rculist_nulls.h>

#if IS_ENABLED(CONFIG_FAST_NAT)
#include <net/fast_nat.h>
#endif

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
#include <linux/ntc_shaper_hooks.h>
#endif

#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
#include <net/netfilter/nf_conntrack_rtcache.h>
#endif

#include <net/netfilter/nf_ntce.h>
#include <net/netfilter/nf_nsc.h>

MODULE_LICENSE("GPL");

#ifdef CONFIG_NF_CONNTRACK_PROCFS
void
print_tuple(struct seq_file *s, const struct nf_conntrack_tuple *tuple,
            const struct nf_conntrack_l3proto *l3proto,
            const struct nf_conntrack_l4proto *l4proto)
{
	l3proto->print_tuple(s, tuple);
	l4proto->print_tuple(s, tuple);
}
EXPORT_SYMBOL_GPL(print_tuple);

struct ct_iter_state {
	struct seq_net_private p;
	struct hlist_nulls_head *hash;
	unsigned int htable_size;
	unsigned int bucket;
	u_int64_t time_now;
};

static struct hlist_nulls_node *ct_get_first(struct seq_file *seq)
{
	struct ct_iter_state *st = seq->private;
	struct hlist_nulls_node *n;

	for (st->bucket = 0;
	     st->bucket < st->htable_size;
	     st->bucket++) {
		n = rcu_dereference(
			hlist_nulls_first_rcu(&st->hash[st->bucket]));
		if (!is_a_nulls(n))
			return n;
	}
	return NULL;
}

static struct hlist_nulls_node *ct_get_next(struct seq_file *seq,
				      struct hlist_nulls_node *head)
{
	struct ct_iter_state *st = seq->private;

	head = rcu_dereference(hlist_nulls_next_rcu(head));
	while (is_a_nulls(head)) {
		if (likely(get_nulls_value(head) == st->bucket)) {
			if (++st->bucket >= st->htable_size)
				return NULL;
		}
		head = rcu_dereference(
			hlist_nulls_first_rcu(&st->hash[st->bucket]));
	}
	return head;
}

static struct hlist_nulls_node *ct_get_idx(struct seq_file *seq, loff_t pos)
{
	struct hlist_nulls_node *head = ct_get_first(seq);

	if (head)
		while (pos && (head = ct_get_next(seq, head)))
			pos--;
	return pos ? NULL : head;
}

static void *ct_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	struct ct_iter_state *st = seq->private;

	st->time_now = ktime_get_real_ns();
	rcu_read_lock();

	nf_conntrack_get_ht(&st->hash, &st->htable_size);
	return ct_get_idx(seq, *pos);
}

static void *ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	return ct_get_next(s, v);
}

static void ct_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

#ifdef CONFIG_NF_CONNTRACK_SECMARK
static void ct_show_secctx(struct seq_file *s, const struct nf_conn *ct)
{
	int ret;
	u32 len;
	char *secctx;

	ret = security_secid_to_secctx(ct->secmark, &secctx, &len);
	if (ret)
		return;

	seq_printf(s, "secctx=%s ", secctx);

	security_release_secctx(secctx, len);
}
#else
static inline void ct_show_secctx(struct seq_file *s, const struct nf_conn *ct)
{
}
#endif

#ifdef CONFIG_NF_CONNTRACK_ZONES
static void ct_show_zone(struct seq_file *s, const struct nf_conn *ct,
			 int dir)
{
	const struct nf_conntrack_zone *zone = nf_ct_zone(ct);

	if (zone->dir != dir)
		return;
	switch (zone->dir) {
	case NF_CT_DEFAULT_ZONE_DIR:
		seq_printf(s, "zone=%u ", zone->id);
		break;
	case NF_CT_ZONE_DIR_ORIG:
		seq_printf(s, "zone-orig=%u ", zone->id);
		break;
	case NF_CT_ZONE_DIR_REPL:
		seq_printf(s, "zone-reply=%u ", zone->id);
		break;
	default:
		break;
	}
}
#else
static inline void ct_show_zone(struct seq_file *s, const struct nf_conn *ct,
				int dir)
{
}
#endif

#ifdef CONFIG_NF_CONNTRACK_TIMESTAMP
static void ct_show_delta_time(struct seq_file *s, const struct nf_conn *ct)
{
	struct ct_iter_state *st = s->private;
	struct nf_conn_tstamp *tstamp;
	s64 delta_time;

	tstamp = nf_conn_tstamp_find(ct);
	if (tstamp) {
		delta_time = st->time_now - tstamp->start;
		if (delta_time > 0)
			delta_time = div_s64(delta_time, NSEC_PER_SEC);
		else
			delta_time = 0;

		seq_printf(s, "delta-time=%llu ",
			   (unsigned long long)delta_time);
	}
	return;
}
#else
static inline void
ct_show_delta_time(struct seq_file *s, const struct nf_conn *ct)
{
}
#endif

#ifdef CONFIG_NF_CONNTRACK_CUSTOM
static void ct_show_ndm_ifaces(struct seq_file *s, const struct nf_conn *ct)
{
	int ctr = 0;
	struct nf_ct_ext_ntc_label *lbl = nf_ct_ext_find_ntc(ct);

	if (lbl) {
		if (lbl->wan_iface > 0) {
			seq_printf(s, "ifw=%d ", lbl->wan_iface);
			ctr++;
		}
		if (lbl->lan_iface > 0) {
			seq_printf(s, "ifl=%d ", lbl->lan_iface);
			ctr++;
		}

		if (nf_ct_ext_ntc_mac_isset(lbl))
			seq_printf(s, "mac=%pM ", &lbl->mac);
		else
			seq_printf(s, "nomac ");

		if (nf_ct_ext_ntc_from_lan(lbl))
			seq_printf(s, "slan ");
		else
			seq_printf(s, "swan ");
	}

	if (ctr == 0)
		seq_printf(s, "no_if ");
}
#else
static inline void
ct_show_ndm_ifaces(struct seq_file *s, const struct nf_conn *ct)
{
}
#endif

#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
static void ct_show_rtcache(struct seq_file *s, const struct nf_conn *ct)
{
	nf_conn_rtcache_dump(s, ct);
}
#else
static inline void ct_show_rtcache(struct seq_file *s, const struct nf_conn *ct)
{
}
#endif

/* return 0 on success, 1 in case of error */
static int ct_seq_show(struct seq_file *s, void *v)
{
	struct nf_conntrack_tuple_hash *hash = v;
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(hash);
	const struct nf_conntrack_l3proto *l3proto;
	const struct nf_conntrack_l4proto *l4proto;
	struct net *net = seq_file_net(s);
	int ret = 0;

	NF_CT_ASSERT(ct);
	if (unlikely(!atomic_inc_not_zero(&ct->ct_general.use)))
		return 0;

	if (nf_ct_should_gc(ct)) {
		nf_ct_kill(ct);
		goto release;
	}

	/* we only want to print DIR_ORIGINAL */
	if (NF_CT_DIRECTION(hash))
		goto release;

	if (!net_eq(nf_ct_net(ct), net))
		goto release;

	l3proto = __nf_ct_l3proto_find(nf_ct_l3num(ct));
	NF_CT_ASSERT(l3proto);
	l4proto = __nf_ct_l4proto_find(nf_ct_l3num(ct), nf_ct_protonum(ct));
	NF_CT_ASSERT(l4proto);

	ret = -ENOSPC;
	seq_printf(s, "%-8s %u %-8s %u %ld ",
		   l3proto->name, nf_ct_l3num(ct),
		   l4proto->name, nf_ct_protonum(ct),
		   nf_ct_expires(ct)  / HZ);

	if (l4proto->print_conntrack)
		l4proto->print_conntrack(s, ct);

	print_tuple(s, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
		    l3proto, l4proto);

	ct_show_zone(s, ct, NF_CT_ZONE_DIR_ORIG);

	if (seq_has_overflowed(s))
		goto release;

	if (seq_print_acct(s, ct, IP_CT_DIR_ORIGINAL))
		goto release;

	if (!(test_bit(IPS_SEEN_REPLY_BIT, &ct->status)))
		seq_printf(s, "[UNREPLIED] ");

	print_tuple(s, &ct->tuplehash[IP_CT_DIR_REPLY].tuple,
		    l3proto, l4proto);

	ct_show_zone(s, ct, NF_CT_ZONE_DIR_REPL);

	if (seq_print_acct(s, ct, IP_CT_DIR_REPLY))
		goto release;

	if (test_bit(IPS_ASSURED_BIT, &ct->status))
		seq_printf(s, "[ASSURED] ");

#if IS_ENABLED(CONFIG_FAST_NAT)
	if (l3proto->l3proto == PF_INET && !ct->fast_ext)
		seq_printf(s, "[FASTNAT] ");
#endif

	ct_show_rtcache(s, ct);

	if (seq_has_overflowed(s))
		goto release;

#if defined(CONFIG_NF_CONNTRACK_MARK)
	seq_printf(s, "mark=%u ", ct->mark);
	seq_printf(s, "nmark=%u sc=%u ",
		   ct->ndm_mark, nf_nsc_ctmark_to_sc(ct->ndm_mark));
#endif

	ct_show_ndm_ifaces(s, ct);
	nf_ntce_ct_show_labels(s, ct);

	ct_show_secctx(s, ct);
	ct_show_zone(s, ct, NF_CT_DEFAULT_ZONE_DIR);
	ct_show_delta_time(s, ct);

	seq_printf(s, "use=%u\n", atomic_read(&ct->ct_general.use));

	if (seq_has_overflowed(s))
		goto release;

	ret = 0;
release:
	nf_ct_put(ct);
	return ret;
}

static const struct seq_operations ct_seq_ops = {
	.start = ct_seq_start,
	.next  = ct_seq_next,
	.stop  = ct_seq_stop,
	.show  = ct_seq_show
};

static int ct_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ct_seq_ops,
			sizeof(struct ct_iter_state));
}

struct kill_request {
	u16 family;
	union nf_inet_addr addr;
};

static int kill_matching(struct nf_conn *i, void *data)
{
	struct kill_request *kr = data;
	struct nf_conntrack_tuple *t1 = &i->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	struct nf_conntrack_tuple *t2 = &i->tuplehash[IP_CT_DIR_REPLY].tuple;

	if (!kr->family)
		return 1;

	if (t1->src.l3num != kr->family)
		return 0;

	return (nf_inet_addr_cmp(&kr->addr, &t1->src.u3) ||
	        nf_inet_addr_cmp(&kr->addr, &t1->dst.u3) ||
	        nf_inet_addr_cmp(&kr->addr, &t2->src.u3) ||
	        nf_inet_addr_cmp(&kr->addr, &t2->dst.u3));
}

static ssize_t ct_file_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct net *net = seq_file_net(seq);
	struct kill_request kr = { };
	char req[INET6_ADDRSTRLEN] = { };

	if (count == 0)
		return 0;

	if (count >= INET6_ADDRSTRLEN)
		count = INET6_ADDRSTRLEN - 1;

	if (copy_from_user(req, buf, count))
		return -EFAULT;

	if (strnchr(req, count, ':')) {
		kr.family = AF_INET6;
		if (!in6_pton(req, count, (void *)&kr.addr, '\n', NULL))
			return -EINVAL;
	} else if (strnchr(req, count, '.')) {
		kr.family = AF_INET;
		if (!in4_pton(req, count, (void *)&kr.addr, '\n', NULL))
			return -EINVAL;
	}

	nf_ct_iterate_cleanup(net, kill_matching, &kr, 0, 0);

	return count;
}

static const struct file_operations ct_file_ops = {
	.owner   = THIS_MODULE,
	.open    = ct_open,
	.read    = seq_read,
	.write	 = ct_file_write,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};

static void *ct_cpu_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct net *net = seq_file_net(seq);
	int cpu;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	for (cpu = *pos-1; cpu < nr_cpu_ids; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*pos = cpu + 1;
		return per_cpu_ptr(net->ct.stat, cpu);
	}

	return NULL;
}

static void *ct_cpu_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct net *net = seq_file_net(seq);
	int cpu;

	for (cpu = *pos; cpu < nr_cpu_ids; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*pos = cpu + 1;
		return per_cpu_ptr(net->ct.stat, cpu);
	}

	return NULL;
}

static void ct_cpu_seq_stop(struct seq_file *seq, void *v)
{
}

static int ct_cpu_seq_show(struct seq_file *seq, void *v)
{
	struct net *net = seq_file_net(seq);
	unsigned int nr_conntracks = atomic_read(&net->ct.count);
	const struct ip_conntrack_stat *st = v;

	if (v == SEQ_START_TOKEN) {
		seq_printf(seq, "entries  searched found new invalid ignore delete delete_list insert insert_failed drop early_drop icmp_error  expect_new expect_create expect_delete search_restart\n");
		return 0;
	}

	seq_printf(seq, "%08x  %08x %08x %08x %08x %08x %08x %08x "
			"%08x %08x %08x %08x %08x  %08x %08x %08x %08x\n",
		   nr_conntracks,
		   0,
		   st->found,
		   0,
		   st->invalid,
		   st->ignore,
		   0,
		   0,
		   st->insert,
		   st->insert_failed,
		   st->drop,
		   st->early_drop,
		   st->error,

		   st->expect_new,
		   st->expect_create,
		   st->expect_delete,
		   st->search_restart
		);
	return 0;
}

static const struct seq_operations ct_cpu_seq_ops = {
	.start	= ct_cpu_seq_start,
	.next	= ct_cpu_seq_next,
	.stop	= ct_cpu_seq_stop,
	.show	= ct_cpu_seq_show,
};

static int ct_cpu_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ct_cpu_seq_ops,
			    sizeof(struct seq_net_private));
}

static const struct file_operations ct_cpu_seq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = ct_cpu_seq_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_net,
};

static int nf_conntrack_standalone_init_proc(struct net *net)
{
	struct proc_dir_entry *pde;
	kuid_t root_uid;
	kgid_t root_gid;

	pde = proc_create("nf_conntrack", 0660, net->proc_net, &ct_file_ops);
	if (!pde)
		goto out_nf_conntrack;

	root_uid = make_kuid(net->user_ns, 0);
	root_gid = make_kgid(net->user_ns, 0);
	if (uid_valid(root_uid) && gid_valid(root_gid))
		proc_set_user(pde, root_uid, root_gid);

	pde = proc_create("nf_conntrack", S_IRUGO, net->proc_net_stat,
			  &ct_cpu_seq_fops);
	if (!pde)
		goto out_stat_nf_conntrack;

	if (nf_ntce_init_proc(net)) {
		remove_proc_entry("nf_conntrack", net->proc_net_stat);
		goto out_stat_nf_conntrack;
	}

	return 0;

out_stat_nf_conntrack:
	remove_proc_entry("nf_conntrack", net->proc_net);
out_nf_conntrack:
	return -ENOMEM;
}

static void nf_conntrack_standalone_fini_proc(struct net *net)
{
	nf_ntce_fini_proc(net);
	remove_proc_entry("nf_conntrack", net->proc_net_stat);
	remove_proc_entry("nf_conntrack", net->proc_net);
}
#else
static int nf_conntrack_standalone_init_proc(struct net *net)
{
	return 0;
}

static void nf_conntrack_standalone_fini_proc(struct net *net)
{
}
#endif /* CONFIG_NF_CONNTRACK_PROCFS */

/* Sysctl support */

#ifdef CONFIG_SYSCTL

static int nf_conntrack_public_lockout_status = 0;

static int nf_conntrack_public_lockout_status_(struct ctl_table *table,
					       int write,
					       void __user *buffer,
					       size_t *lenp, loff_t *ppos)
{
	nf_conntrack_public_lockout_status = nf_conntrack_public_status();

	return proc_dointvec(table, write, buffer, lenp, ppos);
}

static int nf_conntrack_sweep = 0;

static int nf_conntrack_sweep_(struct ctl_table *table,
			       int write,
			       void __user *buffer,
			       size_t *lenp, loff_t *ppos)
{
	const int res = proc_dointvec(table, write, buffer, lenp, ppos);

	nf_conntrack_sweep_user(nf_conntrack_sweep);

	return res;
}

/* Log invalid packets of a given protocol */
static int log_invalid_proto_min __read_mostly;
static int log_invalid_proto_max __read_mostly = 255;

/* size the user *wants to set */
static unsigned int nf_conntrack_htable_size_user __read_mostly;

static int
nf_conntrack_hash_sysctl(struct ctl_table *table, int write,
			 void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	/* module_param hashsize could have changed value */
	nf_conntrack_htable_size_user = nf_conntrack_htable_size;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret < 0 || !write)
		return ret;

	/* update ret, we might not be able to satisfy request */
	ret = nf_conntrack_hash_resize(nf_conntrack_htable_size_user);

	/* update it to the actual value used by conntrack */
	nf_conntrack_htable_size_user = nf_conntrack_htable_size;
	return ret;
}

static struct ctl_table_header *nf_ct_netfilter_header;

static struct ctl_table nf_ct_sysctl_table[] = {
	{
		.procname	= "nf_conntrack_max",
		.data		= &nf_conntrack_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_count",
		.data		= &init_net.ct.count,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname       = "nf_conntrack_buckets",
		.data           = &nf_conntrack_htable_size_user,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = nf_conntrack_hash_sysctl,
	},
	{
		.procname	= "nf_conntrack_checksum",
		.data		= &init_net.ct.sysctl_checksum,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_log_invalid",
		.data		= &init_net.ct.sysctl_log_invalid,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &log_invalid_proto_min,
		.extra2		= &log_invalid_proto_max,
	},
	{
		.procname	= "nf_conntrack_expect_max",
		.data		= &nf_ct_expect_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#if IS_ENABLED(CONFIG_FAST_NAT)
	{
		.procname	= "nf_conntrack_fastnat",
		.data		= &nf_fastnat_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#ifdef CONFIG_FAST_NAT_V2
#if IS_ENABLED(CONFIG_NF_CONNTRACK_RTCACHE)
	{
		.procname	= "nf_conntrack_fastroute",
		.data		= &nf_fastroute_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
#if IS_ENABLED(CONFIG_PPTP)
	{
		.procname	= "nf_conntrack_fastpath_pptp",
		.data		= &nf_fastpath_pptp_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
#if IS_ENABLED(CONFIG_PPPOL2TP)
	{
		.procname	= "nf_conntrack_fastpath_l2tp",
		.data		= &nf_fastpath_l2tp_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
#ifdef CONFIG_XFRM
	{
		.procname	= "nf_conntrack_fastpath_esp",
		.data		= &nf_fastpath_esp_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_fastnat_xfrm",
		.data		= &nf_fastnat_xfrm_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
#endif /* CONFIG_FAST_NAT_V2 */
#endif /* CONFIG_FAST_NAT */
#ifdef CONFIG_NAT_CONE
	{
		.procname	= "nf_conntrack_nat_mode",
		.data		= &nf_conntrack_nat_mode,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_nat_cone_ifindex",
		.data		= &nf_conntrack_nat_cone_ifindex,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
	{
		.procname	= "nf_conntrack_public_count",
		.data		= &init_net.ct.public_count,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_public_max",
		.data		= &nf_conntrack_public_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_public_lockout_time",
		.data		= &nf_conntrack_public_lockout_time,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_conntrack_public_lockout_status",
		.data		= &nf_conntrack_public_lockout_status,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= nf_conntrack_public_lockout_status_,
	},
	{
		.procname	= "nf_conntrack_sweep",
		.data		= &nf_conntrack_sweep,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= nf_conntrack_sweep_,
	},
	{ }
};

static struct ctl_table nf_ct_netfilter_table[] = {
	{
		.procname	= "nf_conntrack_max",
		.data		= &nf_conntrack_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

static int nf_conntrack_standalone_init_sysctl(struct net *net)
{
	struct ctl_table *table;

	table = kmemdup(nf_ct_sysctl_table, sizeof(nf_ct_sysctl_table),
			GFP_KERNEL);
	if (!table)
		goto out_kmemdup;

	table[1].data = &net->ct.count;
	table[3].data = &net->ct.sysctl_checksum;
	table[4].data = &net->ct.sysctl_log_invalid;

	/* Don't export sysctls to unprivileged users */
	if (net->user_ns != &init_user_ns)
		table[0].procname = NULL;

	if (!net_eq(&init_net, net)) {
		table[0].mode = 0444;
		table[2].mode = 0444;
		table[5].mode = 0444;
	}

	net->ct.sysctl_header = register_net_sysctl(net, "net/netfilter", table);
	if (!net->ct.sysctl_header)
		goto out_unregister_netfilter;

	return 0;

out_unregister_netfilter:
	kfree(table);
out_kmemdup:
	return -ENOMEM;
}

static void nf_conntrack_standalone_fini_sysctl(struct net *net)
{
	struct ctl_table *table;

	table = net->ct.sysctl_header->ctl_table_arg;
	unregister_net_sysctl_table(net->ct.sysctl_header);
	kfree(table);
}
#else
static int nf_conntrack_standalone_init_sysctl(struct net *net)
{
	return 0;
}

static void nf_conntrack_standalone_fini_sysctl(struct net *net)
{
}
#endif /* CONFIG_SYSCTL */

static int nf_conntrack_pernet_init(struct net *net)
{
	int ret;

	ret = nf_conntrack_init_net(net);
	if (ret < 0)
		goto out_init;

	ret = nf_conntrack_standalone_init_proc(net);
	if (ret < 0)
		goto out_proc;

	net->ct.sysctl_checksum = 1;
	net->ct.sysctl_log_invalid = 0;
	ret = nf_conntrack_standalone_init_sysctl(net);
	if (ret < 0)
		goto out_sysctl;

	return 0;

out_sysctl:
	nf_conntrack_standalone_fini_proc(net);
out_proc:
	nf_conntrack_cleanup_net(net);
out_init:
	return ret;
}

static void nf_conntrack_pernet_exit(struct list_head *net_exit_list)
{
	struct net *net;

	list_for_each_entry(net, net_exit_list, exit_list) {
		nf_conntrack_standalone_fini_sysctl(net);
		nf_conntrack_standalone_fini_proc(net);
	}
	nf_conntrack_cleanup_net_list(net_exit_list);
}

static struct pernet_operations nf_conntrack_net_ops = {
	.init		= nf_conntrack_pernet_init,
	.exit_batch	= nf_conntrack_pernet_exit,
};

static int __init nf_conntrack_standalone_init(void)
{
	int ret = nf_conntrack_init_start();
	if (ret < 0)
		goto out_start;

#ifdef CONFIG_SYSCTL
	nf_ct_netfilter_header =
		register_net_sysctl(&init_net, "net", nf_ct_netfilter_table);
	if (!nf_ct_netfilter_header) {
		pr_err("nf_conntrack: can't register to sysctl.\n");
		ret = -ENOMEM;
		goto out_sysctl;
	}

	nf_conntrack_htable_size_user = nf_conntrack_htable_size;
#endif

	ret = register_pernet_subsys(&nf_conntrack_net_ops);
	if (ret < 0)
		goto out_pernet;

	nf_conntrack_init_end();
	return 0;

out_pernet:
#ifdef CONFIG_SYSCTL
	unregister_net_sysctl_table(nf_ct_netfilter_header);
out_sysctl:
#endif
	nf_conntrack_cleanup_end();
out_start:
	return ret;
}

static void __exit nf_conntrack_standalone_fini(void)
{
	nf_conntrack_cleanup_start();
	unregister_pernet_subsys(&nf_conntrack_net_ops);
#ifdef CONFIG_SYSCTL
	unregister_net_sysctl_table(nf_ct_netfilter_header);
#endif
	nf_conntrack_cleanup_end();
}

module_init(nf_conntrack_standalone_init);
module_exit(nf_conntrack_standalone_fini);

/* Some modules need us, but don't depend directly on any symbol.
   They should call this. */
void need_conntrack(void)
{
}
EXPORT_SYMBOL_GPL(need_conntrack);
