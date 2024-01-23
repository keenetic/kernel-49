#ifdef CONFIG_NTCE_MODULE
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/vmalloc.h>

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

#include <net/netfilter/nf_nsc.h>
#include <net/netfilter/nf_ntce.h>

enum nf_ct_ext_id nf_ct_ext_id_ntce __read_mostly;
EXPORT_SYMBOL(nf_ct_ext_id_ntce);

#define NF_NTCE_IFACES_HASH_NUM		1024	/* must be 2^X */
#define NF_NTCE_IFACES_COUNT		64	/* max 255 */

#define NF_NTCE_QOS_MAP_SIZE		50

struct nf_ntce_if_ent {
	int iface;
};

struct nf_ntce_qos_map_ent {
	u32 group;
	u8 sc;
};

static struct net_device *nf_ntce_dev __read_mostly;

static uint8_t nf_ntce_if_hash[NF_NTCE_IFACES_HASH_NUM] __read_mostly;
static struct nf_ntce_if_ent nf_ntce_ifaces[NF_NTCE_IFACES_COUNT] __read_mostly;
static atomic_t nf_ntce_enabled __read_mostly = ATOMIC_INIT(0);
static DEFINE_SEQLOCK(nf_ntce_if_seqlock);
static struct nf_ntce_qos_map_ent nf_ntce_qos_map[NF_NTCE_QOS_MAP_SIZE] __read_mostly;
static size_t nf_ntce_qos_map_size __read_mostly;
static DEFINE_RWLOCK(nf_ntce_qos_map_lock);

bool nf_ntce_is_enabled(void)
{
	return !!atomic_read(&nf_ntce_enabled);
}

static inline void nf_ntce_set_if_hash(int ifindex, int id)
{
	nf_ntce_if_hash[ifindex & (NF_NTCE_IFACES_HASH_NUM - 1)] = (uint8_t)id;
}

bool nf_ntce_if_pass(const int ifidx)
{
	int seq = 0; /* reader-only, even */
	bool pass;
	uint8_t hash_index;

	if (ifidx == 0)
		return 0;

	do {
		pass = false;

		read_seqbegin_or_lock(&nf_ntce_if_seqlock, &seq);

		hash_index = nf_ntce_if_hash[ifidx & (NF_NTCE_IFACES_HASH_NUM - 1)];
		if (nf_ntce_ifaces[hash_index].iface == ifidx)
			pass = true;

	} while (need_seqretry(&nf_ntce_if_seqlock, seq));

	done_seqretry(&nf_ntce_if_seqlock, seq);

	return pass;
}
EXPORT_SYMBOL(nf_ntce_if_pass);

static int nf_ntce_if_seq_show(struct seq_file *s, void *v)
{
	size_t i;

	read_seqlock_excl_bh(&nf_ntce_if_seqlock);

	for (i = 0; i < NF_NTCE_IFACES_COUNT; i++) {
		if (nf_ntce_ifaces[i].iface == 0)
			continue;

		seq_printf(s, "%zu: %d\n", i, nf_ntce_ifaces[i].iface);
	}

	read_sequnlock_excl_bh(&nf_ntce_if_seqlock);

	return 0;
}

static ssize_t nf_ntce_if_seq_write(struct file *file,
				   const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	char buf[sizeof("-2147000000") + 2];
	int i, j, v;
	long ifidx = 0;
	int ret;

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	ret = kstrtol(buf, 10, &ifidx);
	if (ret != 0)
		return ret;

	write_seqlock_bh(&nf_ntce_if_seqlock);

	for (i = 0; i < ARRAY_SIZE(nf_ntce_ifaces); i++) {
		struct nf_ntce_if_ent *e = &nf_ntce_ifaces[i];

		if (ifidx == 0) {
			e->iface = 0;
			continue;
		}

		if (ifidx > 0) {
			if (e->iface == ifidx) {
				break;
			}
		} else if (e->iface == -ifidx) {
			e->iface = 0;
			nf_ntce_set_if_hash(0, i);
			break;
		}
	}

	if (ifidx > 0 && i == NF_NTCE_IFACES_COUNT) {
		for (i = 0; i < ARRAY_SIZE(nf_ntce_ifaces); i++) {
			struct nf_ntce_if_ent *e = &nf_ntce_ifaces[i];

			if (e->iface == 0) {
				e->iface = ifidx;
				nf_ntce_set_if_hash(ifidx, i);
				break;
			}
		}
	}

	if (ifidx == 0)
		memset(nf_ntce_if_hash, 0, sizeof(nf_ntce_if_hash));

	v = 0;

	for (j = 0; j < NF_NTCE_IFACES_COUNT; j++) {
		if (nf_ntce_ifaces[j].iface == 0)
			continue;

		v++;
	}

	atomic_set(&nf_ntce_enabled, v);

	write_sequnlock_bh(&nf_ntce_if_seqlock);

	if (ifidx != 0 && i == NF_NTCE_IFACES_COUNT) {
		pr_err("unable to find free slot\n");

		return -E2BIG;
	}

	return count;
}

static void *nf_ntce_if_seq_start(struct seq_file *p, loff_t *pos)
{
	return NULL + (*pos == 0);
}

static void *nf_ntce_if_seq_next(struct seq_file *p, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void nf_ntce_if_seq_stop(struct seq_file *p, void *v)
{
}

static const struct seq_operations nf_ntce_if_seq_ops = {
	.start	= nf_ntce_if_seq_start,
	.next	= nf_ntce_if_seq_next,
	.stop	= nf_ntce_if_seq_stop,
	.show	= nf_ntce_if_seq_show,
};

static int nf_ntce_if_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nf_ntce_if_seq_ops);
}

static const struct file_operations nf_ntce_if_fops = {
	.open		= nf_ntce_if_seq_open,
	.write		= nf_ntce_if_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int nf_ntce_if_queue_seq_show(struct seq_file *s, void *v)
{
	rcu_read_lock_bh();

	if (nf_ntce_dev != NULL)
		seq_puts(s, nf_ntce_dev->name);

	rcu_read_unlock_bh();

	return 0;
}

static ssize_t nf_ntce_if_queue_seq_write(struct file *file,
					  const char __user *buffer,
					  size_t count, loff_t *ppos)
{
	struct net_device *dev = NULL;
	char buf[IFNAMSIZ + 1];
	int ret = count;

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	rcu_read_lock_bh();

	if (nf_ntce_dev != NULL)
		dev_put(nf_ntce_dev);

	if (buf[0] != '\0') {
		dev = dev_get_by_name_rcu(&init_net, buf);
		if (dev)
			dev_hold(dev);
		else
			ret = -ENOENT;
	}

	nf_ntce_dev = dev;

	rcu_read_unlock_bh();

	if (ret < 0)
		pr_err("no target interface \"%s\" found\n", buf);

	return ret;
}

static void *nf_ntce_if_queue_seq_start(struct seq_file *p, loff_t *pos)
{
	return NULL + (*pos == 0);
}

static void *nf_ntce_if_queue_seq_next(struct seq_file *p, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void nf_ntce_if_queue_seq_stop(struct seq_file *p, void *v)
{
}

static const struct seq_operations nf_ntce_if_queue_seq_ops = {
	.start	= nf_ntce_if_queue_seq_start,
	.next	= nf_ntce_if_queue_seq_next,
	.stop	= nf_ntce_if_queue_seq_stop,
	.show	= nf_ntce_if_queue_seq_show,
};

static int nf_ntce_if_queue_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nf_ntce_if_queue_seq_ops);
}

static const struct file_operations nf_ntce_if_queue_fops = {
	.open		= nf_ntce_if_queue_seq_open,
	.write		= nf_ntce_if_queue_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int nf_ntce_group_cmp(const void *lhs, const void *rhs)
{
	const struct nf_ntce_qos_map_ent *l = lhs;
	const struct nf_ntce_qos_map_ent *r = rhs;

	return
		(l->group < r->group) ? -1 :
		(l->group > r->group) ?  1 : 0;
}

static int nf_ntce_qos_map_seq_show(struct seq_file *s, void *v)
{
	size_t i;

	read_lock(&nf_ntce_qos_map_lock);

	for (i = 0; i < nf_ntce_qos_map_size; ++i) {
		seq_printf(s, "%s%u,%u", i > 0 ? "," : "",
			   (unsigned int)nf_ntce_qos_map[i].group,
			   (unsigned int)nf_ntce_qos_map[i].sc);
	}

	read_unlock(&nf_ntce_qos_map_lock);

	return 0;
}

static ssize_t nf_ntce_qos_map_seq_write(struct file *file,
					 const char __user *buffer,
					 size_t count, loff_t *ppos)
{
	char *buf = NULL;
	int *opts = NULL;
	int ret = count;
	int i;
	size_t num;
	char *end;

	if (count > (PAGE_SIZE - 1))
		return -E2BIG;

	buf = vmalloc(PAGE_SIZE);

	if (buf == NULL)
		return -ENOMEM;

	opts = vmalloc((2 * NF_NTCE_QOS_MAP_SIZE + 1) * sizeof(int));

	if (opts == NULL) {
		vfree(buf);
		return -ENOMEM;
	}

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto out_free;
	}

	buf[count] = '\0';

	end = get_options(buf, 2 * NF_NTCE_QOS_MAP_SIZE, opts);

	if (*end != '\0' && *end != '\r' && *end != '\n') {
		pr_err("invalid options string: '%s'\n", buf);
		ret = -EINVAL;
		goto out_free;
	}

	if (opts[0] % 2 != 0) {
		pr_err("invalid options count: %d\n", opts[0]);
		ret = -EINVAL;
		goto out_free;
	}

	num = opts[0] / 2;

	if (num > NF_NTCE_QOS_MAP_SIZE) {
		ret = -E2BIG;
		goto out_free;
	}

	write_lock_bh(&nf_ntce_qos_map_lock);

	for (i = 1; i < opts[0] + 1; i += 2) {
		const int idx = i / 2;
		const int sc = opts[i + 1];

		if (!nf_nsc_valid_sc(sc)) {
			pr_err("invalid service class value: %d\n", sc);
			ret = -ERANGE;
			goto out_free_locked;
		}

		nf_ntce_qos_map[idx].group = opts[i];
		nf_ntce_qos_map[idx].sc = sc;
	}

	sort(nf_ntce_qos_map, num, sizeof(struct nf_ntce_qos_map_ent),
	     &nf_ntce_group_cmp, NULL);
	nf_ntce_qos_map_size = num;

out_free_locked:
	write_unlock_bh(&nf_ntce_qos_map_lock);
out_free:
	vfree(opts);
	vfree(buf);

	return ret;
}

static void *nf_ntce_qos_map_seq_start(struct seq_file *p, loff_t *pos)
{
	return NULL + (*pos == 0);
}

static void *nf_ntce_qos_map_seq_next(struct seq_file *p, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void nf_ntce_qos_map_seq_stop(struct seq_file *p, void *v)
{
}

static const struct seq_operations nf_ntce_qos_map_seq_ops = {
	.start	= nf_ntce_qos_map_seq_start,
	.next	= nf_ntce_qos_map_seq_next,
	.stop	= nf_ntce_qos_map_seq_stop,
	.show	= nf_ntce_qos_map_seq_show,
};

static int nf_ntce_qos_map_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nf_ntce_qos_map_seq_ops);
}

static const struct file_operations nf_ntce_qos_map_fops = {
	.open		= nf_ntce_qos_map_seq_open,
	.write		= nf_ntce_qos_map_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int nf_ntce_event(struct notifier_block *this,
			 unsigned long event, void *ptr)
{
	struct net_device *dev;

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;

	dev = netdev_notifier_info_to_dev(ptr);

	rcu_read_lock_bh();

	if (nf_ntce_dev == dev) {
		nf_ntce_dev = NULL;
		dev_put(dev);
	}

	rcu_read_unlock_bh();

	return NOTIFY_DONE;
}

static struct notifier_block nf_ntce_nb = {
	.notifier_call = nf_ntce_event,
};

static struct nf_ct_ext_type ntce_extend = {
	.len	= sizeof(struct nf_ct_ext_ntce_label),
	.align	= __alignof__(int),
};

int nf_ntce_init_proc(struct net *net)
{
	/* "ENTC" in hex */
	if (nf_ct_extend_custom_register(&ntce_extend, 0x454e5443) < 0) {
		pr_err("unable to register NTCE extend\n");
		goto exit_init;
	}

	nf_ct_ext_id_ntce = ntce_extend.id;

	if (!proc_create("nf_ntce_if", 0664, net->proc_net, &nf_ntce_if_fops))
		goto exit_init_proc_if;

	if (!proc_create("nf_ntce_if_queue", 0664, net->proc_net,
			 &nf_ntce_if_queue_fops))
		goto exit_init_proc_queue;

	if (!proc_create("nf_ntce_qos_map", 0644, net->proc_net,
			 &nf_ntce_qos_map_fops))
		goto exit_init_proc_qos_map;

	if (register_netdevice_notifier(&nf_ntce_nb))
		goto exit_init_notify;

	return 0;

exit_init_notify:
	remove_proc_entry("nf_ntce_qos_map", net->proc_net);
exit_init_proc_qos_map:
	remove_proc_entry("nf_ntce_if_queue", net->proc_net);
exit_init_proc_queue:
	remove_proc_entry("nf_ntce_if", net->proc_net);
exit_init_proc_if:
	nf_ct_extend_unregister(&ntce_extend);

exit_init:
	return -1;
}

void nf_ntce_fini_proc(struct net *net)
{
	unregister_netdevice_notifier(&nf_ntce_nb);

	remove_proc_entry("nf_ntce_if", net->proc_net);
	remove_proc_entry("nf_ntce_if_queue", net->proc_net);
	remove_proc_entry("nf_ntce_qos_map", net->proc_net);

	nf_ct_extend_unregister(&ntce_extend);
}

void nf_ntce_ct_show_labels(struct seq_file *s, const struct nf_conn *ct)
{
	int ctr = 0;
	struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_find_ntce(ct);

	if (lbl) {
		if (lbl->app > 0) {
			seq_printf(s, "app=%u ", lbl->app);
			ctr++;
		}

		if (lbl->group > 0) {
			seq_printf(s, "grp=%u ", lbl->group);
			ctr++;
		}

		if (lbl->attrs.attributes > 0) {
			size_t i = 0;

			seq_puts(s, "attrs=");

			for (i = 0; i < ARRAY_SIZE(lbl->attrs.attr); ++i) {
				const uint32_t attr = lbl->attrs.attr[i];

				if (attr == 0)
					break;

				seq_printf(s, "%s%u",
					   (i == 0) ? "" : ",", attr);
			}

			seq_puts(s, " ");

			ctr++;
		}

		if (lbl->os > 0) {
			seq_printf(s, "os=%u ", lbl->os);
			ctr++;
		}

		if (nf_ct_ext_ntce_fastpath(lbl)) {
			seq_puts(s, "fp ");
			ctr++;
		}

		if (nf_ct_ext_ntce_control(lbl)) {
			seq_puts(s, "ctrl ");
			ctr++;
		}
	}

	if (ctr == 0)
		seq_puts(s, "unclass ");
}

int nf_ntce_ctnetlink_dump(struct sk_buff *skb, const struct nf_conn *ct)
{
	struct nlattr *nest_lbl;
	struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_find_ntce(ct);

	if (lbl == NULL)
		return 0;

	nest_lbl = nla_nest_start(skb, CTA_NTCE | NLA_F_NESTED);

	if (nest_lbl == NULL ||
	    nla_put_u32(skb, CTA_NTCE_APP, lbl->app) ||
	    nla_put_u32(skb, CTA_NTCE_GROUP, lbl->group) ||
	    nla_put_u32(skb, CTA_NTCE_ATTRIBUTES, lbl->attrs.attributes) ||
	    nla_put_u8(skb, CTA_NTCE_OS, lbl->os) ||
	    nla_put_u8(skb, CTA_NTCE_FLAGS, lbl->flags) ||
	    nla_put_u8(skb, CTA_NTCE_SC, nf_nsc_ctmark_to_sc(ct->ndm_mark)))
		return -1;

	nla_nest_end(skb, nest_lbl);
	return 0;
}

size_t nf_ntce_ctnetlink_size(const struct nf_conn *ct)
{
	return nla_total_size(0)
	       + 3 * nla_total_size(sizeof(uint32_t))
	       + 2 * nla_total_size(sizeof(uint8_t));
}

static int nf_ntce_match_group(const void *key, const void *elt)
{
	const u32 *group = (const u32 *)key;
	const struct nf_ntce_qos_map_ent *e = elt;

	return
		(*group < e->group) ? -1 :
		(*group > e->group) ?  1 : 0;
}

static inline int nf_ntce_find_sc(struct nf_conn *ct,
				  const struct nf_ct_ext_ntce_label *lbl)
{
	struct nf_ntce_qos_map_ent *e;

	e = bsearch(&lbl->group, nf_ntce_qos_map,
		    nf_ntce_qos_map_size, sizeof(*nf_ntce_qos_map),
		    nf_ntce_match_group);

	if (e == NULL)
		return -1;

	return e->sc;
}

static void nf_ntce_update_sc_ct_(struct nf_conn *ct,
				  const struct nf_ct_ext_ntce_label *lbl)
{
	int sc;

	if (lbl->group == 0)
		return;

	read_lock(&nf_ntce_qos_map_lock);

	sc = nf_ntce_find_sc(ct, lbl);

	read_unlock(&nf_ntce_qos_map_lock);

	if (sc >= 0)
		nf_nsc_update_sc_ct(ct, sc);
}

void nf_ntce_update_sc_ct(struct nf_conn *ct)
{
	const struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_find_ntce(ct);

	if (unlikely(lbl == NULL)) {
		pr_err_ratelimited("unable to find NTCE label\n");
		return;
	}

	nf_ntce_update_sc_ct_(ct, lbl);
}
EXPORT_SYMBOL(nf_ntce_update_sc_ct);

struct mac_trailer {
	u8 l3proto;
	u8 flags;
	u8 mac[ETH_ALEN];
};

static void nf_ntce_enq_packet_(struct sk_buff *skb, const u8 l3, const u8 *mac)
{
	struct net_device *dev;
	struct sk_buff *nskb;
	struct mac_trailer *mt;

	rcu_read_lock();

	dev = nf_ntce_dev;
	if (dev)
		dev_hold(dev);

	rcu_read_unlock();

	if (dev == NULL)
		return;

	nskb = skb_copy_expand(skb, 0, sizeof(*mt), GFP_ATOMIC | __GFP_NOWARN);
	if (nskb == NULL)
		goto exit_put;

	if (nskb->mac_len > 0)
		skb_push(nskb, nskb->mac_len);

	mt = (struct mac_trailer *)skb_put(nskb, sizeof(*mt));

	memcpy(mt->mac, mac, sizeof(mt->mac));
	mt->l3proto = l3;

	nskb->dev = dev;
	dev_queue_xmit(nskb);

exit_put:
	dev_put(dev);
}

void nf_ntce_enq_packet(const struct nf_conn *ct, struct sk_buff *skb)
{
	struct nf_ct_ext_ntc_label *lbl = nf_ct_ext_find_ntc(ct);
	const u8 pf = nf_ct_l3num(ct);

	if (unlikely(pf != PF_INET && pf != PF_INET6))
		return;

	if (unlikely(!nf_ct_ext_ntc_mac_isset(lbl)))
		return;

	return nf_ntce_enq_packet_(skb, pf, lbl->mac);
}

#endif
