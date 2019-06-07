/*
 * Xtables module to match the process control group.
 *
 * Might be used to implement individual "per-application" firewall
 * policies in contrast to global policies based on control groups.
 * Matching is based upon processes tagged to net_cls' classid marker.
 *
 * (C) 2013 Daniel Borkmann <dborkman@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_cgroup.h>
#include <net/sock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Borkmann <dborkman@redhat.com>");
MODULE_DESCRIPTION("Xtables: process control group matching");
MODULE_ALIAS("ipt_cgroup");
MODULE_ALIAS("ip6t_cgroup");

struct cgroup_mt_id {
	uint32_t id;
	atomic64_t bytes_recv;
	atomic64_t bytes_sent;

	struct list_head list;
};

static struct cgroup_mt_id cgroup_ids;
static rwlock_t cgroup_ids_lock;

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *state;

static int cgroup_mt_check_v0(const struct xt_mtchk_param *par)
{
	struct xt_cgroup_info_v0 *info = par->matchinfo;

	if (info->invert & ~1)
		return -EINVAL;

	return 0;
}

static int cgroup_mt_check_v1(const struct xt_mtchk_param *par)
{
	struct xt_cgroup_info_v1 *info = par->matchinfo;
	struct cgroup *cgrp;

	if ((info->invert_path & ~1) || (info->invert_classid & ~1))
		return -EINVAL;

	if (!info->has_path && !info->has_classid) {
		pr_info("xt_cgroup: no path or classid specified\n");
		return -EINVAL;
	}

	if (info->has_path && info->has_classid) {
		pr_info("xt_cgroup: both path and classid specified\n");
		return -EINVAL;
	}

	info->priv = NULL;
	if (info->has_path) {
		cgrp = cgroup_get_from_path(info->path);
		if (IS_ERR(cgrp)) {
			pr_info("xt_cgroup: invalid path, errno=%ld\n",
				PTR_ERR(cgrp));
			return -EINVAL;
		}
		info->priv = cgrp;
	}

	return 0;
}

static bool
cgroup_mt_v0(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_cgroup_info_v0 *info = par->matchinfo;

	if (skb->sk == NULL || !sk_fullsock(skb->sk))
		return false;

	return (info->id == sock_cgroup_classid(&skb->sk->sk_cgrp_data)) ^
		info->invert;
}

static bool cgroup_mt_v1(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_cgroup_info_v1 *info = par->matchinfo;
	struct sock_cgroup_data *skcd = &skb->sk->sk_cgrp_data;
	struct cgroup *ancestor = info->priv;

	if (!skb->sk || !sk_fullsock(skb->sk))
		return false;

	if (ancestor)
		return cgroup_is_descendant(sock_cgroup_ptr(skcd), ancestor) ^
			info->invert_path;
	else
		return (info->classid == sock_cgroup_classid(skcd)) ^
			info->invert_classid;
}

static void cgroup_mt_destroy_v1(const struct xt_mtdtor_param *par)
{
	struct xt_cgroup_info_v1 *info = par->matchinfo;

	if (info->priv)
		cgroup_put(info->priv);
}

static bool
cgroup_mt_v5(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_cgroup_info_v0 *info = par->matchinfo;
	struct cgroup_mt_id *tmp = NULL;
	int matched = 0;
	int found = 0;

	if (skb->sk == NULL || !sk_fullsock(skb->sk))
		return false;

	matched = ((info->id == sock_cgroup_classid(&skb->sk->sk_cgrp_data)) ^
		info->invert);

	if (!matched)
		return false;

	write_lock_bh(&cgroup_ids_lock);

	list_for_each_entry(tmp, &(cgroup_ids.list), list) {
		if (tmp->id == info->id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		tmp = kmalloc(sizeof(struct cgroup_mt_id), GFP_ATOMIC);

		if (tmp == NULL)
			return false;

		memset(tmp, 0, sizeof(struct cgroup_mt_id));
		tmp->id = info->id;
		list_add(&(tmp->list), &(cgroup_ids.list));
	}

	if (par->hooknum == NF_INET_LOCAL_OUT)
		atomic64_add(skb->len, &tmp->bytes_sent);
	else if (par->hooknum == NF_INET_LOCAL_IN)
		atomic64_add(skb->len, &tmp->bytes_recv);

	write_unlock_bh(&cgroup_ids_lock);

	return true;
}

static int cgroup_state(struct seq_file *fd, void *_v)
{
	struct cgroup_mt_id *tmp;

	read_lock_bh(&cgroup_ids_lock);

	list_for_each_entry(tmp, &(cgroup_ids.list), list) {
		long long bytes_recv = atomic64_xchg(&tmp->bytes_recv, 0);
		long long bytes_sent = atomic64_xchg(&tmp->bytes_sent, 0);

		seq_printf(fd, "%u %lld %lld\n",
			tmp->id,
			bytes_recv,
			bytes_sent);
	}

	read_unlock_bh(&cgroup_ids_lock);

	return 0;
}

static int cgroup_proc_open_state(struct inode *inode, struct file *file)
{
	return single_open(file, cgroup_state, NULL);
}

static const struct file_operations fops_state = {
	.owner		= THIS_MODULE,
	.open		= cgroup_proc_open_state,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static struct xt_match cgroup_mt_reg[] __read_mostly = {
	{
		.name		= "cgroup",
		.revision	= 0,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= cgroup_mt_check_v0,
		.match		= cgroup_mt_v0,
		.matchsize	= sizeof(struct xt_cgroup_info_v0),
		.me		= THIS_MODULE,
		.hooks		= (1 << NF_INET_LOCAL_OUT) |
				  (1 << NF_INET_POST_ROUTING) |
				  (1 << NF_INET_LOCAL_IN),
	},
	{
		.name		= "cgroup",
		.revision	= 1,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= cgroup_mt_check_v1,
		.match		= cgroup_mt_v1,
		.matchsize	= sizeof(struct xt_cgroup_info_v1),
		.destroy	= cgroup_mt_destroy_v1,
		.me		= THIS_MODULE,
		.hooks		= (1 << NF_INET_LOCAL_OUT) |
				  (1 << NF_INET_POST_ROUTING) |
				  (1 << NF_INET_LOCAL_IN),
	},
	{
		.name		= "cgroup",
		.revision	= 5,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= cgroup_mt_check_v0,
		.match		= cgroup_mt_v5,
		.matchsize	= sizeof(struct xt_cgroup_info_v0),
		.me		= THIS_MODULE,
		.hooks		= (1 << NF_INET_LOCAL_OUT) |
				  (1 << NF_INET_LOCAL_IN),
	},
};

static int __init cgroup_mt_init(void)
{
	rwlock_init(&cgroup_ids_lock);

	INIT_LIST_HEAD(&cgroup_ids.list);

	proc_dir = proc_mkdir("xt_cgroup", NULL);

	if (!proc_dir)
		return 0;

	state = proc_create("stats", S_IRUGO, proc_dir, &fops_state);

	if (!state) {
		remove_proc_entry("xt_cgroup", NULL);
		proc_dir = NULL;

		return 0;
	}

	return xt_register_matches(cgroup_mt_reg, ARRAY_SIZE(cgroup_mt_reg));
}

static void __exit cgroup_mt_exit(void)
{
	struct list_head *pos, *q;
	struct cgroup_mt_id *tmp;

	xt_unregister_matches(cgroup_mt_reg, ARRAY_SIZE(cgroup_mt_reg));

	write_lock_bh(&cgroup_ids_lock);

	list_for_each_safe(pos, q, &(cgroup_ids.list)) {
		tmp = list_entry(pos, struct cgroup_mt_id, list);

		list_del(pos);
		kfree(tmp);
	}

	write_unlock_bh(&cgroup_ids_lock);

	if (state) {
		remove_proc_entry("stats", proc_dir);
		state = NULL;
	}

	if (proc_dir) {
		remove_proc_entry("xt_cgroup", NULL);
		proc_dir = NULL;
	}
}

module_init(cgroup_mt_init);
module_exit(cgroup_mt_exit);
