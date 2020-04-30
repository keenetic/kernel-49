/*
 *  Filename:       nf_conntrack_proto_esp.c
 *  Author:         Pavan Kumar
 *  Creation Date:  05/27/04
 *  Description:
 *  Implements the ESP ALG connectiontracking.
 *  Migrated to kernel 2.6.21.5 on April 16, 2008 by Dan-Han Tsai.
 *
 * Based on
 * ip_conntrack_proto_gre.c - Version 3.0
 *
 * Connection tracking protocol helper module for GRE.
 *
 * GRE is a generic encapsulation protocol, which is generally not very
 * suited for NAT, as it has no protocol-specific part as port numbers.
 *
 * It has an optional key field, which may help us distinguishing two
 * connections between the same two hosts.
 *
 * GRE is defined in RFC 1701 and RFC 1702, as well as RFC 2784
 *
 * PPTP is built on top of a modified version of GRE, and has a mandatory
 * field called "CallID", which serves us for the same purpose as the key
 * field in plain GRE.
 *
 * Documentation about PPTP can be found in RFC 2637
 *
 * (C) 2000-2005 by Harald Welte <laforge@gnumonks.org>
 *
 * Development of this code funded by Astaro AG (http://www.astaro.com/)
 *
 * (C) 2006-2012 Patrick McHardy <kaber@trash.net>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/dst.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_proto_esp.h>

#define PROCFS_DIR						"driver/nf_conntrack_proto_esp"
#define PROC_ON_INTERFACE_PRIVATE		"on_interface_private"
#define PROC_ON_INTERFACE_DESTROYED		"on_interface_destroyed"
#define SECLEVEL_PUBLIC					"public"
#define SECLEVEL_PROTECTED				"protected"
#define SECLEVEL_PRIVATE				"private"
#define SL_PRIVATE						1
#define SL_PROTECTED					2
#define SL_PUBLIC						3

#define BUFFER_LEN						32

struct esp_iface {
	int ifindex;
	uint32_t seclevel;

	struct list_head list;
};

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *on_iface_private;
static struct proc_dir_entry *on_iface_destroyed;
static struct esp_iface esp_ifaces;
static rwlock_t esp_ifaces_lock;

static ssize_t write_proc_on_iface_private(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[BUFFER_LEN];
	char *sep, *sep2;
	long ifindex;
	int err = 0;
	int found = 0;
	struct esp_iface *tmp;

	if (count >= BUFFER_LEN)
		count = BUFFER_LEN - 1;

	memset(buffer, 0, BUFFER_LEN);

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	sep = buffer;
	strsep(&sep, " ");

	if (!sep || !(*sep))
		return -EFAULT;

	err = kstrtol(buffer, 10, &ifindex);

	if (err)
		return err;

	sep2 = sep;
	strsep(&sep2, "\n");

	if (strcmp(sep, SECLEVEL_PUBLIC) &&
		strcmp(sep, SECLEVEL_PROTECTED) &&
		strcmp(sep, SECLEVEL_PRIVATE)) {
		return -EFAULT;
	}

	write_lock_bh(&esp_ifaces_lock);

	list_for_each_entry(tmp, &(esp_ifaces.list), list) {
		if (tmp->ifindex == ifindex) {
			if (!strcmp(sep, SECLEVEL_PUBLIC))
				tmp->seclevel = SL_PUBLIC;
			else if (!strcmp(sep, SECLEVEL_PRIVATE))
				tmp->seclevel = SL_PRIVATE;
			else if (!strcmp(sep, SECLEVEL_PROTECTED))
				tmp->seclevel = SL_PROTECTED;
			found = 1;
		}
	}

	if (!found) {
		tmp = kmalloc(sizeof(struct esp_iface), GFP_ATOMIC);

		if (tmp == NULL)
			return -EFAULT;

		memset(tmp, 0, sizeof(struct esp_iface));

		tmp->ifindex = ifindex;
		if (!strcmp(sep, SECLEVEL_PUBLIC))
			tmp->seclevel = SL_PUBLIC;
		else if (!strcmp(sep, SECLEVEL_PRIVATE))
			tmp->seclevel = SL_PRIVATE;
		else if (!strcmp(sep, SECLEVEL_PROTECTED))
			tmp->seclevel = SL_PROTECTED;
		else
			pr_err("invalid seclevel: %s\n", sep);

		list_add(&(tmp->list), &(esp_ifaces.list));
	}

	write_unlock_bh(&esp_ifaces_lock);

	return count;
}

static ssize_t write_proc_on_iface_destroyed(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[BUFFER_LEN];
	long ifindex;
	int err = 0;
	struct list_head *pos, *q;
	struct esp_iface *tmp;

	if (count >= BUFFER_LEN)
		count = BUFFER_LEN - 1;

	memset(buffer, 0, BUFFER_LEN);

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	err = kstrtol(buffer, 10, &ifindex);

	if (err)
		return err;

	write_lock_bh(&esp_ifaces_lock);

	list_for_each_safe(pos, q, &(esp_ifaces.list)) {
		tmp = list_entry(pos, struct esp_iface, list);

		if (tmp->ifindex == ifindex) {
			list_del(pos);
			kfree(tmp);
		}
	}

	write_unlock_bh(&esp_ifaces_lock);

	return count;
}

static const struct file_operations fops_iface_private = {
	.owner		= THIS_MODULE,
	.write		= write_proc_on_iface_private,
	.llseek		= noop_llseek,
};

static const struct file_operations fops_iface_destroyed = {
	.owner		= THIS_MODULE,
	.write		= write_proc_on_iface_destroyed,
	.llseek		= noop_llseek,
};

static void esp_cleanup_procfs(void)
{
	if (on_iface_private) {
		remove_proc_entry(PROC_ON_INTERFACE_PRIVATE, proc_dir);
		on_iface_private = NULL;
	}

	if (on_iface_destroyed) {
		remove_proc_entry(PROC_ON_INTERFACE_DESTROYED, proc_dir);
		on_iface_destroyed = NULL;
	}

	if (proc_dir) {
		remove_proc_entry(PROCFS_DIR, NULL);
		proc_dir = NULL;
	}
}

enum espp_conntrack {
	ESP_CT_UNREPLIED,
	ESP_CT_REPLIED,
	ESP_CT_MAX
};

static unsigned int esp_timeouts[ESP_CT_MAX] = {
	[ESP_CT_UNREPLIED]	= 30*HZ,
	[ESP_CT_REPLIED]	= 180*HZ,
};

static int proto_esp_net_id __read_mostly;
struct netns_proto_esp {
	struct nf_proto_net	nf;
	rwlock_t		keymap_lock;
	struct list_head	keymap_list;
	unsigned int		esp_timeouts[ESP_CT_MAX];
};

static inline struct netns_proto_esp *esp_pernet(struct net *net)
{
	return net_generic(net, proto_esp_net_id);
}

#define MAX_PORTS			48
#define ESP_REF_TMOUT		(esp_timeouts[ESP_CT_UNREPLIED])
#define ESP_CONN_TMOUT		(esp_timeouts[ESP_CT_REPLIED])
#define ESP_TMOUT_COUNT		(ESP_CONN_TMOUT/ESP_REF_TMOUT)
#define TEMP_SPI_START		1500

struct esp_table_t {
	__be32 l_spi;
	__be32 r_spi;
	__be32 l_ip;
	__be32 r_ip;
	u32 timeout;
	u16 tspi;
	struct timer_list refresh_timer;
	int pkt_rcvd;
	int ntimeouts;
	int inuse;
};

static struct esp_table_t esp_table[MAX_PORTS];

static void esp_free_entry(int index)
{
	if (esp_table[index].inuse) {
		del_timer(&esp_table[index].refresh_timer);
		memset(&esp_table[index], 0, sizeof(struct esp_table_t));
	}
}

static void esp_refresh_ct(unsigned long data)
{
	struct esp_table_t *esp_entry = NULL;

	if (data >= MAX_PORTS)
		return;

	esp_entry = &esp_table[data];
	if (esp_entry == NULL)
		return;

	if (esp_entry->pkt_rcvd) {
		esp_entry->pkt_rcvd  = 0;
		esp_entry->ntimeouts = 0;
	} else {
		esp_entry->ntimeouts++;

		if (esp_entry->ntimeouts >= ESP_TMOUT_COUNT) {
			esp_free_entry(data);

			return;
		}
	}

	esp_entry->refresh_timer.expires = jiffies + ESP_REF_TMOUT;
	esp_entry->refresh_timer.function = esp_refresh_ct;
	esp_entry->refresh_timer.data = data;
	add_timer(&esp_entry->refresh_timer);
}

static struct esp_table_t *alloc_esp_entry(void)
{
	int idx = 0;

	for (; idx < MAX_PORTS; idx++) {
		if (esp_table[idx].inuse)
			continue;

		memset(&esp_table[idx], 0, sizeof(struct esp_table_t));
		esp_table[idx].tspi = idx + TEMP_SPI_START;
		esp_table[idx].inuse = 1;

		init_timer(&esp_table[idx].refresh_timer);
		esp_table[idx].refresh_timer.data = idx;
		esp_table[idx].refresh_timer.expires = jiffies + ESP_REF_TMOUT;
		esp_table[idx].refresh_timer.function = esp_refresh_ct;
		add_timer(&esp_table[idx].refresh_timer);

		return &esp_table[idx];
	}

	return NULL;
}

static struct esp_table_t *search_esp_entry_by_ip(
		const struct sk_buff *skb,
		const struct nf_conntrack_tuple *tuple,
		const __be32 spi)
{
	int idx = 0;
	__be32 srcip = tuple->src.u3.ip;
	__be32 dstip = tuple->dst.u3.ip;
	struct esp_table_t *esp_entry = esp_table;
	struct esp_iface *tmp;

	for (; idx < MAX_PORTS; idx++, esp_entry++) {
		if (srcip == esp_entry->l_ip) {
			read_lock_bh(&esp_ifaces_lock);

			list_for_each_entry(tmp, &(esp_ifaces.list), list) {
				if (skb->dev != NULL && skb->dev->ifindex == tmp->ifindex) {
					esp_entry->l_spi = spi;

					if (dstip != esp_entry->r_ip) {
						esp_entry->r_ip = dstip;
						esp_entry->r_spi = 0;
					}

					read_unlock_bh(&esp_ifaces_lock);

					return esp_entry;
				}
			}

			read_unlock_bh(&esp_ifaces_lock);
		}

		if (srcip == esp_entry->r_ip) {
			if (esp_entry->r_spi == 0) {
				esp_entry->r_spi = spi;

				return esp_entry;
			}
		}
	}

	return NULL;
}

static struct esp_table_t *search_esp_entry_by_spi(
		const __be32 spi, const __be32 srcip)
{
	int idx = 0;
	struct esp_table_t *esp_entry = esp_table;

	for (; idx < MAX_PORTS; idx++, esp_entry++) {
		if ((spi == esp_entry->l_spi) || (spi == esp_entry->r_spi)) {
			if ((spi == esp_entry->l_spi) && (srcip == esp_entry->r_ip))
				esp_entry->r_spi = spi;

			return esp_entry;
		}
	}

	return NULL;
}

static void nf_ct_esp_keymap_flush(struct net *net)
{
	struct netns_proto_esp *net_esp = esp_pernet(net);
	struct nf_ct_esp_keymap *km, *tmp;

	write_lock_bh(&net_esp->keymap_lock);
	list_for_each_entry_safe(km, tmp, &net_esp->keymap_list, list) {
		list_del(&km->list);
		kfree(km);
	}
	write_unlock_bh(&net_esp->keymap_lock);
}

/* PUBLIC CONNTRACK PROTO HELPER FUNCTIONS */

/* invert esp part of tuple */
static bool esp_invert_tuple(struct nf_conntrack_tuple *tuple,
			     const struct nf_conntrack_tuple *orig)
{
	tuple->dst.u.esp.spi = orig->src.u.esp.spi;
	tuple->src.u.esp.spi = orig->dst.u.esp.spi;
	return true;
}

/* esp hdr info to tuple */
static bool esp_pkt_to_tuple(const struct sk_buff *skb, unsigned int dataoff,
			     struct net *net, struct nf_conntrack_tuple *tuple)
{
	const struct esp_hdr_ct *esphdr;
	struct esp_hdr_ct _esphdr;
	struct esp_table_t *esp_entry = NULL;
	struct esp_iface *tmp;
	bool permitted = false;

	esphdr = skb_header_pointer(skb, dataoff, sizeof(_esphdr), &_esphdr);
	if (!esphdr) {
		/* try to behave like "nf_conntrack_proto_generic" */
		tuple->src.u.all = 0;
		tuple->dst.u.all = 0;
		return true;
	}

/* check if esphdr has a new SPI:
 *   if no, update tuple with correct tspi and increment pkt count;
 *   if yes, check if we have seen the source IP:
 *             if yes, do the tspi and pkt count update
 *             if no, create a new entry
 */

	esp_entry = search_esp_entry_by_spi(esphdr->spi, tuple->src.u3.ip);
	if (esp_entry == NULL) {
		esp_entry = search_esp_entry_by_ip(skb, tuple, esphdr->spi);
		if (esp_entry == NULL) {
			read_lock_bh(&esp_ifaces_lock);

			list_for_each_entry(tmp, &(esp_ifaces.list), list) {
				if (skb->dev != NULL && skb->dev->ifindex == tmp->ifindex) {
					permitted = true;
					break;
				}
			}

			read_unlock_bh(&esp_ifaces_lock);

			if (!permitted)
				return false;

			esp_entry = alloc_esp_entry();
			if (esp_entry == NULL) {
				pr_info_ratelimited("all esp entries are in use\n");
				return false;
			}

			esp_entry->l_spi = esphdr->spi;
			esp_entry->l_ip = tuple->src.u3.ip;
			esp_entry->r_ip = tuple->dst.u3.ip;
		}
	}

	tuple->dst.u.esp.spi = tuple->src.u.esp.spi = esp_entry->tspi;
	esp_entry->pkt_rcvd++;

	return true;
}

/* print esp part of tuple */
static void esp_print_tuple(struct seq_file *s,
			    const struct nf_conntrack_tuple *tuple)
{
	seq_printf(s, "srcspi16=0x%x dstspi16=0x%x ",
		   ntohs(tuple->src.u.esp.spi),
		   ntohs(tuple->dst.u.esp.spi));
}

/* print private data for conntrack */
static void esp_print_conntrack(struct seq_file *s, struct nf_conn *ct)
{
	seq_printf(s, "timeout=%u, stream_timeout=%u ",
		   (ct->proto.esp.timeout / HZ),
		   (ct->proto.esp.stream_timeout / HZ));
}

static unsigned int *esp_get_timeouts(struct net *net)
{
	return esp_pernet(net)->esp_timeouts;
}

/* Returns verdict for packet, and may modify conntrack */
static int esp_packet(struct nf_conn *ct,
		      const struct sk_buff *skb,
		      unsigned int dataoff,
		      enum ip_conntrack_info ctinfo,
		      u_int8_t pf,
		      unsigned int hooknum,
		      unsigned int *timeouts)
{
	/* If we've seen traffic both ways, this is a ESP connection.
	 * Extend timeout.
	 */
	if (ct->status & IPS_SEEN_REPLY) {
		nf_ct_refresh_acct(ct, ctinfo, skb,
				   ct->proto.esp.stream_timeout);
		/* Also, more likely to be important, and not a probe. */
		if (!test_and_set_bit(IPS_ASSURED_BIT, &ct->status))
			nf_conntrack_event_cache(IPCT_ASSURED, ct);
	} else
		nf_ct_refresh_acct(ct, ctinfo, skb,
				   ct->proto.esp.timeout);

	return NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static bool esp_new(struct nf_conn *ct, const struct sk_buff *skb,
		    unsigned int dataoff, unsigned int *timeouts)
{
	pr_debug(": ");
	nf_ct_dump_tuple(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);

	ct->proto.esp.stream_timeout = timeouts[ESP_CT_REPLIED];
	ct->proto.esp.timeout = timeouts[ESP_CT_UNREPLIED];

	return true;
}

#if IS_ENABLED(CONFIG_NF_CT_NETLINK_TIMEOUT)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_cttimeout.h>

static int esp_timeout_nlattr_to_obj(struct nlattr *tb[],
				     struct net *net, void *data)
{
	unsigned int *timeouts = data;
	struct netns_proto_esp *net_esp = esp_pernet(net);

	/* set default timeouts for GRE. */
	timeouts[GRE_CT_UNREPLIED] = net_esp->esp_timeouts[GRE_CT_UNREPLIED];
	timeouts[GRE_CT_REPLIED] = net_esp->esp_timeouts[GRE_CT_REPLIED];

	if (tb[CTA_TIMEOUT_GRE_UNREPLIED]) {
		timeouts[GRE_CT_UNREPLIED] =
			ntohl(nla_get_be32(tb[CTA_TIMEOUT_GRE_UNREPLIED])) * HZ;
	}
	if (tb[CTA_TIMEOUT_GRE_REPLIED]) {
		timeouts[GRE_CT_REPLIED] =
			ntohl(nla_get_be32(tb[CTA_TIMEOUT_GRE_REPLIED])) * HZ;
	}
	return 0;
}

static int
esp_timeout_obj_to_nlattr(struct sk_buff *skb, const void *data)
{
	const unsigned int *timeouts = data;

	if (nla_put_be32(skb, CTA_TIMEOUT_GRE_UNREPLIED,
			 htonl(timeouts[GRE_CT_UNREPLIED] / HZ)) ||
	    nla_put_be32(skb, CTA_TIMEOUT_GRE_REPLIED,
			 htonl(timeouts[GRE_CT_REPLIED] / HZ)))
		goto nla_put_failure;
	return 0;

nla_put_failure:
	return -ENOSPC;
}

static const struct nla_policy
esp_timeout_nla_policy[CTA_TIMEOUT_GRE_MAX+1] = {
	[CTA_TIMEOUT_GRE_UNREPLIED]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_GRE_REPLIED]	= { .type = NLA_U32 },
};
#endif /* CONFIG_NF_CT_NETLINK_TIMEOUT */

static int esp_init_net(struct net *net, u_int16_t proto)
{
	struct netns_proto_esp *net_esp = esp_pernet(net);
	int i;

	rwlock_init(&net_esp->keymap_lock);
	INIT_LIST_HEAD(&net_esp->keymap_list);
	for (i = 0; i < ESP_CT_MAX; i++)
		net_esp->esp_timeouts[i] = esp_timeouts[i];

	return 0;
}

/* protocol helper struct */
static struct nf_conntrack_l4proto nf_conntrack_l4proto_esp4 __read_mostly = {
	.l3proto	 = AF_INET,
	.l4proto	 = IPPROTO_ESP,
	.name		 = "esp",
	.pkt_to_tuple	 = esp_pkt_to_tuple,
	.invert_tuple	 = esp_invert_tuple,
	.print_tuple	 = esp_print_tuple,
	.print_conntrack = esp_print_conntrack,
	.get_timeouts    = esp_get_timeouts,
	.packet		 = esp_packet,
	.new		 = esp_new,
	.me			 = THIS_MODULE,
#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
	.tuple_to_nlattr = nf_ct_port_tuple_to_nlattr,
	.nlattr_tuple_size = nf_ct_port_nlattr_tuple_size,
	.nlattr_to_tuple = nf_ct_port_nlattr_to_tuple,
	.nla_policy	 = nf_ct_port_nla_policy,
#endif
#if IS_ENABLED(CONFIG_NF_CT_NETLINK_TIMEOUT)
	.ctnl_timeout    = {
		.nlattr_to_obj	= esp_timeout_nlattr_to_obj,
		.obj_to_nlattr	= esp_timeout_obj_to_nlattr,
		.nlattr_max	= CTA_TIMEOUT_GRE_MAX,
		.obj_size	= sizeof(unsigned int) * GRE_CT_MAX,
		.nla_policy	= esp_timeout_nla_policy,
	},
#endif /* CONFIG_NF_CT_NETLINK_TIMEOUT */
	.net_id		= &proto_esp_net_id,
	.init_net	= esp_init_net,
};

static int proto_esp_net_init(struct net *net)
{
	int ret = 0;

	ret = nf_ct_l4proto_pernet_register(net, &nf_conntrack_l4proto_esp4);
	if (ret < 0)
		pr_err("nf_conntrack_esp4: pernet registration failed.\n");

	return ret;
}

static void proto_esp_net_exit(struct net *net)
{
	nf_ct_l4proto_pernet_unregister(net, &nf_conntrack_l4proto_esp4);
	nf_ct_esp_keymap_flush(net);
}

static struct pernet_operations proto_esp_net_ops = {
	.init = proto_esp_net_init,
	.exit = proto_esp_net_exit,
	.id   = &proto_esp_net_id,
	.size = sizeof(struct netns_proto_esp),
};

static int __init nf_ct_proto_esp_init(void)
{
	int ret;

	rwlock_init(&esp_ifaces_lock);
	INIT_LIST_HEAD(&esp_ifaces.list);

	ret = register_pernet_subsys(&proto_esp_net_ops);
	if (ret < 0)
		goto out_pernet;

	ret = nf_ct_l4proto_register(&nf_conntrack_l4proto_esp4);
	if (ret < 0)
		goto out_esp4;

	proc_dir = proc_mkdir(PROCFS_DIR, NULL);
	if (proc_dir == NULL) {
		ret = -EACCES;
		goto out_l4proto;
	}

	on_iface_private = proc_create(PROC_ON_INTERFACE_PRIVATE, S_IWUGO,
				       proc_dir, &fops_iface_private);
	on_iface_destroyed = proc_create(PROC_ON_INTERFACE_DESTROYED, S_IWUGO,
					proc_dir, &fops_iface_destroyed);
	if (!on_iface_private || !on_iface_destroyed) {
		ret = -EACCES;
		goto out_procdir;
	}

	return 0;

out_procdir:
	esp_cleanup_procfs();
out_l4proto:
	nf_ct_l4proto_unregister(&nf_conntrack_l4proto_esp4);
out_esp4:
	unregister_pernet_subsys(&proto_esp_net_ops);
out_pernet:
	return ret;
}

static void __exit nf_ct_proto_esp_fini(void)
{
	struct list_head *pos, *q;
	struct esp_iface *tmp;
	int idx = 0;

	nf_ct_l4proto_unregister(&nf_conntrack_l4proto_esp4);
	unregister_pernet_subsys(&proto_esp_net_ops);

	synchronize_net();

	esp_cleanup_procfs();

	for (; idx < MAX_PORTS; idx++) {
		if (esp_table[idx].inuse)
			del_timer_sync(&esp_table[idx].refresh_timer);
	}

	list_for_each_safe(pos, q, &(esp_ifaces.list)) {
		tmp = list_entry(pos, struct esp_iface, list);

		list_del(pos);
		kfree(tmp);
	}
}

module_init(nf_ct_proto_esp_init);
module_exit(nf_ct_proto_esp_fini);

MODULE_LICENSE("GPL");
