#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/string.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/inet.h>
#include <asm/errno.h>
#include <linux/glob.h>

#include <uapi/linux/netfilter/xt_tls.h>

#define NAME_BUFFER 128
/* #define XT_TLS_DEBUG */

/*
 * Searches through skb->data and looks for a
 * client or server handshake. A client
 * handshake is preferred as the SNI
 * field tells us what domain the client
 * wants to connect to.
 */
static int get_tls_hostname(const struct sk_buff *skb, int thoff, bool tls_only,
			    bool dtls, char *dest, bool *is_tls, bool *has_esni)
{
	size_t data_len;
	int l7offset;
	uint16_t tls_header_len;
	uint8_t handshake_protocol;
	uint8_t u8data;

	if (!dtls) {
		struct tcphdr _tcph;
		const struct tcphdr *th;

		th = skb_header_pointer(skb, thoff, sizeof(_tcph), &_tcph);
		if (th == NULL)
			return -ENOMEM;

		/* check malformed tcp */
		if (th->doff * 4 < sizeof(_tcph))
			return -ENOMEM;

		l7offset = thoff + th->doff * 4;
	} else {
		struct udphdr _udph;
		const struct udphdr *uh;

		uh = skb_header_pointer(skb, thoff, sizeof(_udph), &_udph);
		if (uh == NULL)
			return -ENOMEM;

		l7offset = thoff + sizeof(struct udphdr);
	}

	/* Now at D/TLS header */
	if (unlikely(l7offset > skb->len))
		return -ENOMEM;

	/* Calculate packet data length */
	data_len = skb->len - l7offset;

	/* Minimal size of TLS/DTLS header */
	if (data_len < 15)
		return -EPROTO;

	if (skb_copy_bits(
			skb,
			l7offset,
			&u8data,
			sizeof(u8data)))
		return -EFAULT;

	/* If this isn't an TLS handshake, abort */
	if (u8data != 0x16)
		return -EPROTO;

	if (skb_copy_bits(
			skb,
			l7offset + (dtls ? 11 : 3),
			&tls_header_len,
			sizeof(tls_header_len)))
		return -EFAULT;

	tls_header_len = ntohs(tls_header_len);
	tls_header_len += (dtls ? 13 : 5);

	if (skb_copy_bits(
			skb,
			l7offset + (dtls ? 13 : 5),
			&handshake_protocol,
			sizeof(handshake_protocol)))
		return -EFAULT;

	/* Even if we don't have all the data, try matching anyway */
	if (tls_header_len > data_len)
		tls_header_len = data_len;

	if (tls_header_len > 4) {
		/* Check only client hellos for now */
		if (handshake_protocol == 0x01) {
			u_int offset;
			u_int base_offset = (dtls ? (43 + 14 + 8) : 43);
			u_int extension_offset = 2;
			uint16_t cipher_len;
			uint16_t extensions_len;
			uint8_t compression_len, session_id_len;

			if (base_offset + 2 > data_len) {
#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] Data length is to small (%d)\n",
					(int)data_len);
#endif
				return -EPROTO;
			}

			/* Get the length of the session ID */
			if (skb_copy_bits(
					skb,
					l7offset + base_offset,
					&session_id_len,
					sizeof(session_id_len)))
				return -EFAULT;

#ifdef XT_TLS_DEBUG
			pr_info("[xt_tls] Session ID length: %d\n",
				session_id_len);
#endif
			if ((session_id_len + base_offset + 2) > tls_header_len) {
#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] TLS header length is smaller "
					"than session_id_len + base_offset +2 (%d > %d)\n",
					(session_id_len + base_offset + 2), tls_header_len);
#endif
				return -EPROTO;
			}

			/* Get the length of the ciphers */
			if (skb_copy_bits(
					skb,
					l7offset + base_offset + session_id_len + 1,
					&cipher_len,
					sizeof(cipher_len)))
				return -EFAULT;

			cipher_len = ntohs(cipher_len);
			offset = base_offset + session_id_len + cipher_len + 2;
#ifdef XT_TLS_DEBUG
			pr_info("[xt_tls] Cipher len: %d\n", cipher_len);
			pr_info("[xt_tls] Offset (1): %d\n", offset);
#endif
			if (offset > tls_header_len) {
#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] TLS header length is smaller "
					"than offset (%d > %d)\n", offset, tls_header_len);
#endif
				return -EPROTO;
			}

			/* Get the length of the compression types */
			if (skb_copy_bits(
					skb,
					l7offset + offset + 1,
					&compression_len,
					sizeof(compression_len)))
				return -EFAULT;

			offset += compression_len + 2;
#ifdef XT_TLS_DEBUG
			pr_info("[xt_tls] Compression length: %d\n",
				compression_len);
			pr_info("[xt_tls] Offset (2): %d\n", offset);
#endif
			if (offset > tls_header_len) {
#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] TLS header length is smaller "
					"than offset w/compression (%d > %d)\n",
					offset, tls_header_len);
#endif
				return -EPROTO;
			}

			/* Get the length of all the extensions */
			if (skb_copy_bits(
					skb,
					l7offset + offset,
					&extensions_len,
					sizeof(extensions_len)))
				return -EFAULT;

			extensions_len = ntohs(extensions_len);
#ifdef XT_TLS_DEBUG
			pr_info("[xt_tls] Extensions length: %d\n",
				extensions_len);
#endif

			if ((extensions_len + offset) > tls_header_len) {
#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] TLS header length is smaller "
					"than offset w/extensions (%d > %d)\n",
					(extensions_len + offset), tls_header_len);
#endif
				return -EPROTO;
			}

#ifdef XT_TLS_DEBUG
			pr_info("[xt_tls] Match TLS packet\n");
#endif

			*is_tls = true;

			if (tls_only)
				return 0;

			/* Loop through all the extensions to find the SNI extension */
			while (extension_offset < extensions_len) {
				uint16_t extension_id, extension_len;

				if (skb_copy_bits(
						skb,
						l7offset + offset + extension_offset,
						&extension_id,
						sizeof(extension_id)))
					return -EFAULT;

				extension_offset += 2;

				if (skb_copy_bits(
						skb,
						l7offset + offset + extension_offset,
						&extension_len,
						sizeof(extension_len)))
					return -EFAULT;

				extension_offset += 2;

				extension_id = ntohs(extension_id);
				extension_len = ntohs(extension_len);

#ifdef XT_TLS_DEBUG
				pr_info("[xt_tls] Extension ID: %d\n",
					extension_id);
				pr_info("[xt_tls] Extension length: %d\n",
					extension_len);
#endif

				if (extension_id == 0 && dest[0] == '\0') {
					uint16_t name_length;
#ifdef XT_TLS_DEBUG
					uint8_t name_type;
#endif

					/* We don't need the server name list length,
					 * so skip that
					 */
					extension_offset += 2;

#ifdef XT_TLS_DEBUG
					/* We don't really need name_type at the moment
					 * as there's only one type in the RFC-spec.
					 * However I'm leaving it in here for
					 * debugging purposes.
					 */

					if (skb_copy_bits(
							skb,
							l7offset + offset + extension_offset,
							&name_type,
							sizeof(name_type)))
						return -EFAULT;
#endif

					extension_offset += 1;

					if (skb_copy_bits(
							skb,
							l7offset + offset + extension_offset,
							&name_length,
							sizeof(name_length)))
						return -EFAULT;

					name_length = ntohs(name_length);
					extension_offset += 2;

#ifdef XT_TLS_DEBUG
					pr_info("[xt_tls] Name type: %d\n",
						name_type);
					pr_info("[xt_tls] Name length: %d\n",
						name_length);
#endif
					if (name_length > (NAME_BUFFER - 1)) {
#ifdef XT_TLS_DEBUG
						pr_info("XXX %d %d\n", name_length, NAME_BUFFER - 1);
#endif
						return -EPROTO;
					}

					if (skb_copy_bits(
							skb,
							l7offset + offset + extension_offset,
							dest,
							name_length))
						return -EFAULT;

					/*  Make sure the string is always null-terminated */
					dest[name_length] = '\0';
					extension_len = name_length;
				} else
				if (extension_id == htons(0xffceU)) {
					*has_esni = true;
#ifdef XT_TLS_DEBUG
					pr_info("[xt_tls] Found ESNI extension\n");
#endif
				}

				extension_offset += extension_len;
			}
		}
	}

	return *is_tls ? 0 : -EPROTO;
}

static inline bool
tls_mt_(const struct sk_buff *skb, struct xt_action_param *par, bool dtls)
{
	char parsed_host[NAME_BUFFER];
	const struct xt_tls_mt_info *info = par->matchinfo;
	const bool invert = ((info->invert & XT_TLS_OP_HOST) != 0);
	const bool block_esni = ((info->invert & XT_TLS_OP_BLOCK_ESNI) != 0);
	const bool tls_only = (info->tls_host[0] == '\0');
	int result;
	bool match;
	bool is_tls = false;
	bool has_esni = false;

	/* This is a fragment, no TCP/UDP header is available */
	if (par->fragoff != 0)
		return false;

	parsed_host[0] = '\0';

#ifdef XT_TLS_DEBUG
	pr_info("\n[xt_tls] New enter\n");
#endif

	result = get_tls_hostname(skb, par->thoff, tls_only, dtls, parsed_host,
				  &is_tls, &has_esni);
	if (result == -ENOMEM)
		par->hotdrop = true;

	if (result != 0)
		return false;

	if (!is_tls)
		return false;

	if (tls_only) {
#ifdef XT_TLS_DEBUG
		pr_info("[xt_tls] Match any host\n");
#endif
		return true;
	}

	if (has_esni && block_esni) {
#ifdef XT_TLS_DEBUG
		pr_info("[xt_tls] ESNI is found and blocked\n");
#endif
		return true;
	}

	match = glob_match(info->tls_host, parsed_host);

#ifdef XT_TLS_DEBUG
	pr_info("[xt_tls] Parsed domain: %s\n", parsed_host);
	pr_info("[xt_tls] Domain matches: %s, invert: %s\n",
		match ? "true" : "false", invert ? "true" : "false");
#endif

	if (invert)
		match = !match;

	return match;
}

static bool tls_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	return tls_mt_(skb, par, false);
}

static bool dtls_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	return tls_mt_(skb, par, true);
}

static int tls_mt_check(const struct xt_mtchk_param *par)
{
	__u16 proto;

	if (par->family == NFPROTO_IPV4)
		proto = ((const struct ipt_ip *) par->entryinfo)->proto;
	else if (par->family == NFPROTO_IPV6)
		proto = ((const struct ip6t_ip6 *) par->entryinfo)->proto;
	else
		return -EINVAL;

	if (proto != IPPROTO_TCP) {
		pr_info("Can be used only in combination with "
			"-p tcp\n");
		return -EINVAL;
	}

	return 0;
}

static int dtls_mt_check(const struct xt_mtchk_param *par)
{
	__u16 proto;

	if (par->family == NFPROTO_IPV4)
		proto = ((const struct ipt_ip *) par->entryinfo)->proto;
	else if (par->family == NFPROTO_IPV6)
		proto = ((const struct ip6t_ip6 *) par->entryinfo)->proto;
	else
		return -EINVAL;

	if (proto != IPPROTO_UDP) {
		pr_info("Can be used only in combination with "
			"-p udp\n");
		return -EINVAL;
	}

	return 0;
}

static struct xt_match tls_mt_regs[] __read_mostly = {
	{
		.name       = "tls",
		.revision   = 0,
		.family     = NFPROTO_IPV4,
		.checkentry = tls_mt_check,
		.match      = tls_mt,
		.matchsize  = sizeof(struct xt_tls_mt_info),
		.me         = THIS_MODULE,
	},
	{
		.name       = "dtls",
		.revision   = 0,
		.family     = NFPROTO_IPV4,
		.checkentry = dtls_mt_check,
		.match      = dtls_mt,
		.matchsize  = sizeof(struct xt_tls_mt_info),
		.me         = THIS_MODULE,
	},
#if IS_ENABLED(CONFIG_IP6_NF_IPTABLES)
	{
		.name       = "tls",
		.revision   = 0,
		.family     = NFPROTO_IPV6,
		.checkentry = tls_mt_check,
		.match      = tls_mt,
		.matchsize  = sizeof(struct xt_tls_mt_info),
		.me         = THIS_MODULE,
	},
	{
		.name       = "dtls",
		.revision   = 0,
		.family     = NFPROTO_IPV6,
		.checkentry = dtls_mt_check,
		.match      = dtls_mt,
		.matchsize  = sizeof(struct xt_tls_mt_info),
		.me         = THIS_MODULE,
	},
#endif
};

static int __init tls_mt_init(void)
{
	return xt_register_matches(tls_mt_regs, ARRAY_SIZE(tls_mt_regs));
}

static void __exit tls_mt_exit(void)
{
	xt_unregister_matches(tls_mt_regs, ARRAY_SIZE(tls_mt_regs));
}

module_init(tls_mt_init);
module_exit(tls_mt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nils Andreas Svee <nils@stokkdalen.no>");
MODULE_DESCRIPTION("Xtables: TLS/DTLS (SNI) matching");
MODULE_ALIAS("ipt_tls");
