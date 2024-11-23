#ifndef _WIFI_MAIL_H_
#define _WIFI_MAIL_H_

#include <linux/types.h>
#include <soc/airoha/npu_mbox_api.h>
#include <soc/airoha/wifi_mail_type.h>

struct wifi_mail_data *
wifi_mail_alloc(u32 intf_id, dma_addr_t *pa, gfp_t gfp_mask);

int wifi_mail_send(struct wifi_mail_data *md, dma_addr_t pa, bool do_free);
void wifi_mail_free(struct wifi_mail_data *md, dma_addr_t pa);

static inline int
WIFI_MAIL_API_SET_WAIT_PKT_BUF_ADDR(u32 intf_id, u32 pkt_buf_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	if (!pkt_buf_addr)
		return NPU_MAILBOX_ERROR;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_PKT_BUF_ADDR;
	md->wifi_mail_private.pkt_buf_addr = pkt_buf_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_TX_PKT_BUF_ADDR(u32 intf_id, u32 pkt_buf_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	if (!pkt_buf_addr)
		return NPU_MAILBOX_ERROR;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_TX_PKT_BUF_ADDR;
	md->wifi_mail_private.pkt_buf_addr = pkt_buf_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_INODE_TXRX_REG_ADDR(u32 intf_id, u32 dir, u32 in_counter_addr,
					   u32 out_status_addr, u32 out_counter_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_INODE_TXRX_REG_ADDR;
	md->wifi_mail_private.txrx_addr.dir = dir;
	md->wifi_mail_private.txrx_addr.in_counter_addr = in_counter_addr;
	md->wifi_mail_private.txrx_addr.out_status_addr = out_status_addr;
	md->wifi_mail_private.txrx_addr.out_counter_addr = out_counter_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_PCIE_ADDR(u32 intf_id, u32 pcie_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_ADDR;
	md->wifi_mail_private.pcie_addr = pcie_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_TX_RING_PCIE_ADDR(u32 intf_id, u32 pcie_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_TX_RING_PCIE_ADDR;
	md->wifi_mail_private.pcie_addr = pcie_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_PCIE_PORT_TYPE(u32 intf_id, u32 pcie_port_type)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_PORT_TYPE;
	md->wifi_mail_private.pcie_port_type = pcie_port_type;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_PCIE_STATE(u32 intf_id)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_PCIE_STATE;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_TX_DESC_BASE(u32 intf_id, u32 base_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_TX_DESC_HW_BASE;
	md->wifi_mail_private.phy_addr = base_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_TX_BUF_SPACE_BASE(u32 intf_id, u32 base_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_TX_BUF_SPACE_HW_BASE;
	md->wifi_mail_private.phy_addr = base_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_RX_RING_FOR_TXDONE_HW_BASE(u32 intf_id, u32 base_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_RX_RING_FOR_TXDONE_HW_BASE;
	md->wifi_mail_private.phy_addr = base_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_DESC(u32 intf_id, u32 desc)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_DESC;
	md->wifi_mail_private.desc = desc;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_NPU_INIT_DONE(u32 intf_id, u32 npu_init_done)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_NPU_INIT_DONE;
	md->wifi_mail_private.npu_init_done = npu_init_done;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_TRAN_TO_CPU(u32 intf_id, u32 tran2cpu)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_TRAN_TO_CPU;
	md->wifi_mail_private.tran2cpu = tran2cpu;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_IS_FORCE_TO_CPU(u32 intf_id, u32 force2cpu)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_IS_FORCE_TO_CPU;
	md->wifi_mail_private.force2cpu_flag = force2cpu;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_NPU_BAND0_ONCPU(u32 intf_id, u32 band0_oncpu)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_NPU_BAND0_ONCPU;
	md->wifi_mail_private.band0_oncpu_flag = band0_oncpu;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_BAR_INFO(u32 intf_id, u32 wcid, u32 tid, u32 sn)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;
	u32 bar_info;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	bar_info = ((sn & 0xfff) << 11) | ((wcid & 0xff) << 3) | (tid & 0x7);

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_BAR_INFO;
	md->wifi_mail_private.bar_info = bar_info;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_BA_WIN_SIZE(u32 intf_id, u32 wcid, u32 tid, u32 bawsize, u32 sn)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;
	u32 bawin_size;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	bawin_size = ((sn & 0xfff) << 20) | ((bawsize & 0x1ff) << 11) |
		     ((wcid & 0xff) << 3) | (tid & 0x7);

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_BA_WIN_SIZE;
	md->wifi_mail_private.bawin_size = bawin_size;

	return wifi_mail_send(md, pa, true);
}

int WIFI_MAIL_API_SET_WAIT_DRAM_BA_NODE_ADDR(u32 intf_id, u32 banode_addr)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_DRAM_BA_NODE_ADDR;
	md->wifi_mail_private.dram_banode_addr = banode_addr;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_NOBA_FLAG(u32 intf_id, u32 noba_flag)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_IS_TEST_NOBA;
	md->wifi_mail_private.test_noba_flag = noba_flag;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_FAST_FLAG(u32 intf_id, u32 fast_flag)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_FAST_FLAG;
	md->wifi_mail_private.fast_flag = fast_flag;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_FLUSHONE_TIMEOUT(u32 intf_id, u32 timeout)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_FLUSHONE_TIMEOUT;
	md->wifi_mail_private.flushone_timeout = timeout;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_FLUSHALL_TIMEOUT(u32 intf_id, u32 timeout)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_FLUSHALL_TIMEOUT;
	md->wifi_mail_private.flushall_timeout = timeout;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_ADD_DEL_STA(u32 intf_id, u32 wcid)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_ATOMIC);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_DEL_STA;
	md->wifi_mail_private.wcid = wcid;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_SET_WAIT_DRIVER_MODEL(u32 intf_id, u32 driver_model)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = SET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_SET_WAIT_DRIVER_MODEL;
	md->wifi_mail_private.driver_model = driver_model;

	return wifi_mail_send(md, pa, true);
}

static inline int
WIFI_MAIL_API_GET_WAIT_DBG_COUNTER(u32 intf_id, struct dbg_counter *dc)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;
	int res;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = GET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_GET_WAIT_DBG_COUNTER;

	res = wifi_mail_send(md, pa, false);
	if (res != NPU_MAILBOX_SUCCESS)
		goto mail_free;

	dc->reg_count = md->wifi_mail_private.dbg_count.reg_count;

mail_free:
	wifi_mail_free(md, pa);

	return res;
}

static inline int
WIFI_MAIL_API_GET_WAIT_NPU_INFO(u32 intf_id, struct npu_info *ni)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;
	int res;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = GET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_GET_WAIT_NPU_INFO;

	res = wifi_mail_send(md, pa, false);
	if (res != NPU_MAILBOX_SUCCESS)
		goto mail_free;

	ni->info = md->wifi_mail_private.ni.info;

mail_free:
	wifi_mail_free(md, pa);

	return res;
}

static inline int
WIFI_MAIL_API_GET_WAIT_RXDESC_BASE(u32 intf_id, struct npu_info *ni)
{
	struct wifi_mail_data *md;
	dma_addr_t pa;
	int res;

	md = wifi_mail_alloc(intf_id, &pa, GFP_KERNEL);
	if (!md)
		return NPU_MAILBOX_ERROR;

	md->func_type = GET_WAIT;
	md->func_id = WIFI_MAIL_FUNCTION_GET_WAIT_RXDESC_BASE;

	res = wifi_mail_send(md, pa, false);
	if (res != NPU_MAILBOX_SUCCESS)
		goto mail_free;

	ni->info = md->wifi_mail_private.ni.info;

mail_free:
	wifi_mail_free(md, pa);

	return res;
}

#endif /* _WIFI_MAIL_H_ */
