#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <soc/airoha/npu.h>
#include <soc/airoha/npu_mbox_api.h>
#include <soc/airoha/wifi_mail_type.h>

struct wifi_mail_data *
wifi_mail_alloc(u32 intf_id, dma_addr_t *pa, gfp_t gfp_mask)
{
	struct device *dev = get_npu_dev();
	struct wifi_mail_data *md;
	size_t in_size = sizeof(struct wifi_mail_data);

	if (!dev)
		return NULL;

	md = dma_alloc_coherent(dev, in_size, pa, gfp_mask | __GFP_NOWARN);
	if (!md)
		return NULL;

	memset(md, 0, in_size);

	md->interface_id = intf_id;

	return md;
}
EXPORT_SYMBOL(wifi_mail_alloc);

int wifi_mail_send(struct wifi_mail_data *md, dma_addr_t pa, bool do_free)
{
	struct device *dev = get_npu_dev();
	struct npuMboxInfo mi;
	u32 timeout = 1000 * 10;
	int ret;

	if (md->func_id == WIFI_MAIL_FUNCTION_SET_WAIT_INODE_TXRX_REG_ADDR)
		timeout = 6000 * 10;

	mi.core_id = CORE0;
	mi.func_id = MFUNC_WIFI;
	mi.va = (unsigned long)md;
	mi.pa = (u32)pa;
	mi.len = (u16)sizeof(*md);
	mi.flags_word = 0;
	mi.flags.isBlockingMode = 1;
	mi.blockTimeout = timeout;
	mi.cb = NULL;

	ret = host_notify_npuMbox(&mi);

	if (do_free)
		dma_free_coherent(dev, sizeof(*md), (void *)md, pa);

	return ret;
}
EXPORT_SYMBOL(wifi_mail_send);

void wifi_mail_free(struct wifi_mail_data *md, dma_addr_t pa)
{
	struct device *dev = get_npu_dev();

	dma_free_coherent(dev, sizeof(*md), (void *)md, pa);
}
EXPORT_SYMBOL(wifi_mail_free);
