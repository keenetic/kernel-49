#ifdef CONFIG_NTCE_MODULE
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <net/netfilter/nf_conntrack_acct.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#include <../ndm/hw_nat/ra_nat.h>
#endif

#include <net/netfilter/nf_ntce.h>

enum nf_ct_ext_id nf_ct_ext_id_ntce __read_mostly;
EXPORT_SYMBOL(nf_ct_ext_id_ntce);

#define NF_NTCE_IFACES_HASH_NUM		1024	/* must be 2^X */
#define NF_NTCE_IFACES_COUNT		64	/* max 255 */

struct nf_ntce_if_ent {
	int iface;
};

static struct net_device *nf_ntce_dev __read_mostly;

static uint8_t nf_ntce_if_hash[NF_NTCE_IFACES_HASH_NUM] __read_mostly;
static struct nf_ntce_if_ent nf_ntce_ifaces[NF_NTCE_IFACES_COUNT] __read_mostly;
static DEFINE_SEQLOCK(nf_ntce_if_seqlock);

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
	int i;
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
			if (e->iface == 0 || e->iface == ifidx) {
				e->iface = ifidx;
				nf_ntce_set_if_hash(ifidx, i);
				break;
			}
		} else if (e->iface == -ifidx) {
			e->iface = 0;
			nf_ntce_set_if_hash(0, i);
			break;
		}
	}

	if (ifidx == 0)
		memset(nf_ntce_if_hash, 0, sizeof(nf_ntce_if_hash));

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
		pr_err("unable to register ntce extend\n");
		goto exit_init;
	}

	nf_ct_ext_id_ntce = ntce_extend.id;

	if (!proc_create("nf_ntce_if", 0664, net->proc_net, &nf_ntce_if_fops))
		goto exit_init_proc_if;

	if (!proc_create("nf_ntce_if_queue", 0664, net->proc_net,
			 &nf_ntce_if_queue_fops))
		goto exit_init_proc_queue;

	if (register_netdevice_notifier(&nf_ntce_nb))
		goto exit_init_notify;

	return 0;

exit_init_notify:
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
	    nla_put_u8(skb, CTA_NTCE_FLAGS, lbl->flags))
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

static inline void nf_ntce_enq_packet(struct sk_buff *skb)
{
	struct net_device *dev;
	struct sk_buff *nskb;

	rcu_read_lock();

	dev = nf_ntce_dev;
	if (dev)
		dev_hold(dev);

	rcu_read_unlock();

	if (dev == NULL)
		return;

	nskb = skb_clone(skb, GFP_ATOMIC | __GFP_NOWARN);
	if (nskb == NULL)
		goto exit_put;

#if IS_ENABLED(CONFIG_RA_HW_NAT)
	if (unlikely(!FOE_SKB_IS_KEEPALIVE(skb)))
		FOE_ALG_MARK(skb);
#endif

	if (nskb->mac_len > 0)
		skb_push(nskb, nskb->mac_len);

	nskb->dev = dev;
	dev_queue_xmit(nskb);

exit_put:
	dev_put(dev);
}

int nf_ntce_enq_pkt(struct nf_conn *ct, struct sk_buff *skb)
{
	const struct nf_conn_counter *counters;
	const struct nf_ct_ext_ntce_label *lbl = nf_ct_ext_find_ntce(ct);
	u64 pkts;

	if (unlikely(lbl == NULL)) {
		pr_err_ratelimited("unable to find ntce label\n");

		return 0;
	}

	if (likely(nf_ct_ext_ntce_fastpath(lbl)))
		return 0;

	counters = (struct nf_conn_counter *)nf_conn_acct_find(ct);
	if (unlikely(counters == NULL)) {
		pr_err_ratelimited("unable to find accoutings\n");

		return 0;
	}

	pkts = atomic64_read(&counters[IP_CT_DIR_ORIGINAL].packets) +
		atomic64_read(&counters[IP_CT_DIR_REPLY].packets);

	if (pkts > NF_NTCE_HARD_PACKET_LIMIT)
		return 0;

	nf_ntce_enq_packet(skb);

	return 1;
}

#endif
