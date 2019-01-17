#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/netfilter.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/inet.h>

#include <net/netfilter/nf_fp_smb.h>

#define HOSTS_COUNT		8
#define PHOST_AT(idx_)		((__be32 *)&entries[idx_].nf_fp_smb_host)
#define PMASK_AT(idx_)		((__be32 *)&entries[idx_].nf_fp_smb_mask)
#define PSUBN_AT(idx_)		((__be32 *)&entries[idx_].nf_fp_smb_subn)
#define HOST_AT(idx_)		(*(PHOST_AT(idx_)))
#define MASK_AT(idx_)		(*(PMASK_AT(idx_)))
#define SUBN_AT(idx_)		(*(PSUBN_AT(idx_)))

struct nf_fp_smb_entry {
	__be32	nf_fp_smb_host;
	__be32	nf_fp_smb_mask;
	__be32	nf_fp_smb_subn;
};

static u32 nf_fp_smb_on __read_mostly;
static struct nf_fp_smb_entry entries[HOSTS_COUNT] __read_mostly;

int nf_fp_smb_hook_in(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	const struct tcphdr *tcph;
	struct tcphdr _hdr;
	u32 found = 0;
	size_t i;

	if (!nf_fp_smb_on)
		return 0;

	if (unlikely(iph->protocol != IPPROTO_TCP))
		return 0;

	for (i = 0; i < HOSTS_COUNT; ++i) {
		if (iph->daddr == HOST_AT(i)) {
			if ((iph->saddr & MASK_AT(i)) != SUBN_AT(i))
				return 0;

			found = 1;

			break;
		}

		if (HOST_AT(i) == 0)
			break;
	}

	if (likely(!found))
		return 0;

	tcph = skb_header_pointer(skb, (iph->ihl << 2), sizeof(_hdr), &_hdr);
	if (!tcph)
		return 0;

	if (tcph->dest == htons(445) ||
	    tcph->dest == htons(139)) {
		skb->nf_fp_cache = 1;
		return 1;
	}

	return 0;
}

int nf_fp_smb_hook_out(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	const struct tcphdr *tcph;
	struct tcphdr _hdr;
	u32 found = 0;
	size_t i;

	if (!nf_fp_smb_on)
		return 0;

	if (unlikely(iph->protocol != IPPROTO_TCP))
		return 0;

	for (i = 0; i < HOSTS_COUNT; ++i) {
		if (iph->saddr == HOST_AT(i)) {
			if ((iph->daddr & MASK_AT(i)) != SUBN_AT(i))
				return 0;

			found = 1;

			break;
		}

		if (HOST_AT(i) == 0)
			break;
	}

	if (likely(!found))
		return 0;

	tcph = skb_header_pointer(skb, (iph->ihl << 2), sizeof(_hdr), &_hdr);
	if (!tcph)
		return 0;

	if (tcph->source == htons(445) ||
	    tcph->source == htons(139)) {
		skb->nf_fp_cache = 1;
		return 1;
	}

	return 0;
}

#ifdef CONFIG_PROC_FS

static int nf_fp_smb_seq_show(struct seq_file *s, void *v)
{
	size_t i;

	seq_printf(s, "%lu\n", (unsigned long)nf_fp_smb_on);

	for (i = 0; i < HOSTS_COUNT; ++i)
		seq_printf(s, "%pI4/%pI4\n", PHOST_AT(i), PMASK_AT(i));

	return 0;
}

static ssize_t nf_fp_smb_seq_write(struct file *file,
				   const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	char buf[INET_ADDRSTRLEN*2+2];
	u32 in[8], smb_ip = 0, mask = 0;
	size_t i;
	long conv = 0;

	if (count > (sizeof(buf) - 1))
		count = (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	if (sscanf(buf, "%d.%d.%d.%d/%d.%d.%d.%d", &in[0], &in[1], &in[2], &in[3],
			&in[4], &in[5], &in[6], &in[7]) == 8) {
		for (i = 0; i < 8; i++) {
			if (in[i] > 255)
				return count;
		}
		smb_ip = (in[0] << 24) | (in[1] << 16) | (in[2] << 8) | in[3];
		mask = (in[4] << 24) | (in[5] << 16) | (in[6] << 8) | in[7];
	} else if (kstrtol(buf, 10, &conv) != 0) {
		return count;
	}

	if (conv != 0 && smb_ip == 0) {
		nf_fp_smb_on = 0;

		memset(entries, 0, sizeof(entries));

		pr_info("Disable SMB fastpath\n");

		return count;
	}

	for (i = 0; i < HOSTS_COUNT; ++i) {
		if (HOST_AT(i) == 0 || HOST_AT(i) == cpu_to_be32(smb_ip)) {
			HOST_AT(i) = cpu_to_be32(smb_ip);
			MASK_AT(i) = cpu_to_be32(mask);
			SUBN_AT(i) = cpu_to_be32(smb_ip & mask);

			nf_fp_smb_on = 1;

			pr_info("Enable SMB fastpath for %pI4/%pI4\n",
				PHOST_AT(i), PMASK_AT(i));

			break;
		}
	}

	return count;
}

static void *seq_start(struct seq_file *p, loff_t *pos)
{
	return NULL + (*pos == 0);
}

static void *seq_next(struct seq_file *p, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void seq_stop(struct seq_file *p, void *v)
{
}

static const struct seq_operations nf_fp_smb_seq_ops = {
	.start	= seq_start,
	.next	= seq_next,
	.stop	= seq_stop,
	.show	= nf_fp_smb_seq_show,
};

static int nf_fp_smb_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nf_fp_smb_seq_ops);
}

static const struct file_operations nf_fp_smb_fops = {
	.open		= nf_fp_smb_seq_open,
	.write		= nf_fp_smb_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#endif

static int __net_init netfilter_fp_smb_net_init(struct net *net)
{
	nf_fp_smb_on = 0;

	memset(entries, 0, sizeof(entries));

#ifdef CONFIG_PROC_FS
	if (!proc_create("nf_fp_smb", 0664,
			net->nf.proc_netfilter, &nf_fp_smb_fops))
		return -ENOMEM;
#endif

	return 0;
}

static void __net_exit netfilter_fp_smb_net_exit(struct net *net)
{
	remove_proc_entry("nf_fp_smb", net->nf.proc_netfilter);
}

static struct pernet_operations netfilter_fp_smb_net_ops = {
	.init = netfilter_fp_smb_net_init,
	.exit = netfilter_fp_smb_net_exit,
};

static int __init netfilter_fp_smb_init(void)
{
	return register_pernet_subsys(&netfilter_fp_smb_net_ops);
}

static void __exit netfilter_fp_smb_exit(void)
{
	unregister_pernet_subsys(&netfilter_fp_smb_net_ops);
}

module_init(netfilter_fp_smb_init);
module_exit(netfilter_fp_smb_exit);
