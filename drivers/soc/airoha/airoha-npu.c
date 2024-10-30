#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <soc/airoha/npu_mbox_api.h>

extern u32 airoha_get_l2c_sram(void);

#if defined(CONFIG_MACH_AN7583)
#define NPU_V2_P
#define NPU_WIFI_TX
#define NPU_MAX_CORE_NUM	6	/* NPU_V2_P */
#define HWF_DSCP_SRAM_QDMA1	16384
#define HWF_DSCP_SRAM_QDMA2	16384
#elif defined(CONFIG_MACH_AN7581)
#define NPU_WIFI_TX
#define NPU_MAX_CORE_NUM	8	/* NPU_V2 */
#define HWF_DSCP_SRAM_QDMA1	16384
#define HWF_DSCP_SRAM_QDMA2	20480
#elif defined(CONFIG_MACH_AN7552)
#define NPU_V2_S
#define NPU_MAX_CORE_NUM	2	/* NPU_V2_S */
#define HWF_DSCP_SRAM_QDMA1	16384
#define HWF_DSCP_SRAM_QDMA2	16384
#else
#error Airoha SoC not defined
#endif

#if IS_ENABLED(CONFIG_MT7916_AP)
#define NPU_PKT_BUF_SIZE	0x1600000
#else
#define NPU_PKT_BUF_SIZE	0x2c00000
#endif
#define NPU_BA_NODE_SIZE	0x0200000
#define NPU_TX_PKT_BUF_SIZE	0x3a00000

#define NPU_DMA_MASK		0xbfffffff

#define NPU_MAX_INT_NUM		6
#define NPU_INT_M2H		6
#define NPU_INT_WDG		7

#define NPU_DEBUG_BASE		0x305000
#define NPU_CLUSTER_BASE	0x306000
#define CR_PC_BASE_ADDR		NPU_DEBUG_BASE

#define CR_CORE_BOOT_TRIGGER	(NPU_CLUSTER_BASE + 0x000)
#define CR_CORE_BOOT_CONFIG	(NPU_CLUSTER_BASE + 0x004)
#define CR_CORE_BOOT_BASE	(NPU_CLUSTER_BASE + 0x020)

#define NPU_MBOX_BASE		0x30c000
#define CR_MBOX_INTR_STATUS	(NPU_MBOX_BASE + 0x000)
#define CR_MBOX_INT_MASK0	(NPU_MBOX_BASE + 0x004)
#define CR_MBOX_INT_MASK8	(NPU_MBOX_BASE + 0x024)
#define CR_MBQ0_CTRL0		(NPU_MBOX_BASE + 0x030)
#define CR_MBQ0_CTRL1		(NPU_MBOX_BASE + 0x034)
#define CR_MBQ0_CTRL2		(NPU_MBOX_BASE + 0x038)
#define CR_MBQ0_CTRL3		(NPU_MBOX_BASE + 0x03C)
#define CR_MBQ8_CTRL0		(NPU_MBOX_BASE + 0x0b0)
#define CR_MBQ8_CTRL1		(NPU_MBOX_BASE + 0x0b4)
#define CR_MBQ8_CTRL2		(NPU_MBOX_BASE + 0x0b8)
#define CR_MBQ8_CTRL3		(NPU_MBOX_BASE + 0x0bC)
#define CR_NPU_MIB0		(NPU_MBOX_BASE + 0x140)
#define CR_NPU_MIB8		(NPU_MBOX_BASE + 0x160)
#define CR_NPU_MIB9		(NPU_MBOX_BASE + 0x164)
#define CR_NPU_MIB10		(NPU_MBOX_BASE + 0x168)
#define CR_NPU_MIB11		(NPU_MBOX_BASE + 0x16C)
#define CR_NPU_MIB12		(NPU_MBOX_BASE + 0x170)
#define CR_NPU_MIB13		(NPU_MBOX_BASE + 0x174)
#define CR_NPU_MIB14		(NPU_MBOX_BASE + 0x178)
#define CR_NPU_MIB15		(NPU_MBOX_BASE + 0x17C)
#define CR_NPU_MIB16		(NPU_MBOX_BASE + 0x180)
#define CR_NPU_MIB19		(NPU_MBOX_BASE + 0x18C)
#define CR_NPU_MIB20		(NPU_MBOX_BASE + 0x190)
#define CR_NPU_MIB21		(NPU_MBOX_BASE + 0x194)

#define NPU_HOSTADAPTER_BASE	0x30d000

#define NPU_UART_BASE		0x310000
#define NPU_TIME0_BASE		0x310100
#define NPU_TIME1_BASE		0x310200
#define NPU_SCU_BASE		0x311000

#define NPU_MBOX2HOST_BIT	(1U << MBOX2HOST_IDX)

struct airoha_npu_mbox {
	unsigned long va[MAX_MBOX_FUNC];
	u32 pa[MAX_MBOX_FUNC];
	h_mbox_cb_t mbox8_cb[MAX_MBOX_FUNC];
	u16 size[MAX_MBOX_FUNC];
	spinlock_t lock;
	spinlock_t buf_lock;
};

struct airoha_npu {
	struct device *dev;
	void __iomem *rbus_base;
	void __iomem *pbus_base;
	phys_addr_t rsv_pa;
	void *rsv_va;
	struct tasklet_struct mbox_tasklet;
	struct airoha_npu_mbox mbox_obj[NPU_MAX_CORE_NUM];
	int irq_m2h;
	int irq[NPU_MAX_INT_NUM];
	int irq_wdg[NPU_MAX_CORE_NUM];
	u8 force_to_cpu;
};

static const char *wdg_irq_name[8] = {
	"npu_wdg0", "npu_wdg1", "npu_wdg2", "npu_wdg3",
	"npu_wdg4", "npu_wdg5", "npu_wdg6", "npu_wdg7"
};

static struct airoha_npu *g_npu;

static u32 g_npu_pkt_buf_addr;
static u32 g_npu_ba_node_addr;
static u32 g_npu_tx_pkt_buf_addr;
static u32 g_hwf_buf_addr[2];

void reserve_memblocks_npu(void)
{
	const phys_addr_t start = 0x80000000;
	phys_addr_t align;
	phys_addr_t pa;
	u32 len;

#ifdef CONFIG_RA_HW_NAT_WIFI
	 /* NPU support phy address range ~0xbfffffff */
	align = 64;
	len = NPU_PKT_BUF_SIZE;
	pa = memblock_alloc_range(len, align, start, 0xbfffffff, MEMBLOCK_NONE);
	g_npu_pkt_buf_addr = (u32)pa;
	pr_info("memblock: reserve %5uK for %s\n", len >> 10, "NPU wifi RX data");

#ifdef NPU_WIFI_TX
	len = NPU_TX_PKT_BUF_SIZE;
	pa = memblock_alloc_range(len, align, start, 0xbfffffff, MEMBLOCK_NONE);
	g_npu_tx_pkt_buf_addr = (u32)pa;
	pr_info("memblock: reserve %5uK for %s\n", len >> 10, "NPU wifi TX data");
#endif
	len = NPU_BA_NODE_SIZE;
	pa = memblock_alloc_range(len, align, start, 0xbfffffff, MEMBLOCK_NONE);
	g_npu_ba_node_addr = (u32)pa;
	pr_info("memblock: reserve %5uK for %s\n", len >> 10, "NPU wifi BA data");
#endif /* CONFIG_RA_HW_NAT_WIFI */

	align = 64 << 10;
	len = HWF_DSCP_SRAM_QDMA1 * 256;
	pa = memblock_alloc_range(len, align, start, 0xffffffff, MEMBLOCK_NONE);
	g_hwf_buf_addr[0] = (u32)pa;
	pr_info("memblock: reserve %5uK for %s\n", len >> 10, "QDMA1 HWF");

	len = HWF_DSCP_SRAM_QDMA2 * 256;
	pa = memblock_alloc_range(len, align, start, 0xffffffff, MEMBLOCK_NONE);
	g_hwf_buf_addr[1] = (u32)pa;
	pr_info("memblock: reserve %5uK for %s\n", len >> 10, "QDMA2 HWF");
}

u32 hwf_get_buf_addr(u8 id)
{
	if (id >= ARRAY_SIZE(g_hwf_buf_addr))
		return 0;

	return g_hwf_buf_addr[id];
}
EXPORT_SYMBOL(hwf_get_buf_addr);

u32 hwf_get_buf_size(u8 id)
{
	return id ? (HWF_DSCP_SRAM_QDMA2 * 256) : (HWF_DSCP_SRAM_QDMA1 * 256);
}
EXPORT_SYMBOL(hwf_get_buf_size);

u32 npu_wifi_offload_get_pkt_buf_addr(void)
{
	return g_npu_pkt_buf_addr;
}
EXPORT_SYMBOL(npu_wifi_offload_get_pkt_buf_addr);

u32 npu_wifi_offload_get_ba_node_addr(void)
{
	return g_npu_ba_node_addr;
}
EXPORT_SYMBOL(npu_wifi_offload_get_ba_node_addr);

u32 npu_wifi_offload_get_tx_pkt_buf_addr(void)
{
	return g_npu_tx_pkt_buf_addr;
}
EXPORT_SYMBOL(npu_wifi_offload_get_tx_pkt_buf_addr);

static u32 get_npu_reg_data(struct airoha_npu *npu, u32 reg)
{
	return readl(npu->pbus_base + reg);
}

static void set_npu_reg_data(struct airoha_npu *npu, u32 reg, u32 val)
{
	writel(val, npu->pbus_base + reg);
}

int get_npu_irq(int index)
{
	struct airoha_npu *npu = g_npu;

	if (npu && index < NPU_MAX_INT_NUM)
		return npu->irq[index];

	return -1;
}
EXPORT_SYMBOL(get_npu_irq);

u32 get_npu_hostadpt_reg(u32 hadap_ofs)
{
	struct airoha_npu *npu = g_npu;

	if (!npu)
		return 0;

	return get_npu_reg_data(npu, NPU_HOSTADAPTER_BASE + hadap_ofs);
}
EXPORT_SYMBOL(get_npu_hostadpt_reg);

void set_npu_hostadpt_reg(u32 hadap_ofs, u32 val)
{
	struct airoha_npu *npu = g_npu;

	if (!npu)
		return;

	set_npu_reg_data(npu, NPU_HOSTADAPTER_BASE + hadap_ofs, val);
}
EXPORT_SYMBOL(set_npu_hostadpt_reg);

void set_npu_sram_power_save(u32 reg, u32 val)
{
	struct airoha_npu *npu = g_npu;
	u32 tmp;

	if (!npu)
		return;

	tmp = get_npu_reg_data(npu, NPU_CLUSTER_BASE + reg);
	set_npu_reg_data(npu, NPU_CLUSTER_BASE + reg, tmp | val);
}
EXPORT_SYMBOL(set_npu_sram_power_save);

void boot_npu_all_cores(void)
{
	struct airoha_npu *npu = g_npu;
	u32 core, cores_en = 0;

	if (!npu)
		return;

	/* set booting address */
	for (core = 0; core < NPU_MAX_CORE_NUM; core++) {
		u32 reg_offs = CR_CORE_BOOT_BASE + (core << 2);

		set_npu_reg_data(npu, reg_offs, (u32)npu->rsv_pa);
		cores_en |= (1U << core);
	}

	usleep_range(1000, 2000);

	/* enable NPU cores (cores_en == 1 can boot all cores also) */
	set_npu_reg_data(npu, CR_CORE_BOOT_CONFIG, cores_en);

	/* start NPU cores */
	set_npu_reg_data(npu, CR_CORE_BOOT_TRIGGER, 0x1);

	msleep(100);
}
EXPORT_SYMBOL(boot_npu_all_cores);

void host_set_npu_core_on_off(int core, int on)
{
	struct airoha_npu *npu = g_npu;
	u32 tmp;

	if (!npu)
		return;

	tmp = get_npu_reg_data(npu, CR_CORE_BOOT_CONFIG);
	if (on)
		tmp |=  (1U << core);
	else
		tmp &= ~(1U << core);
	set_npu_reg_data(npu, CR_CORE_BOOT_CONFIG, tmp);

	set_npu_reg_data(npu, CR_CORE_BOOT_TRIGGER, 0);
	usleep_range(1000, 2000);
	set_npu_reg_data(npu, CR_CORE_BOOT_TRIGGER, 1);

	msleep(100);
}
EXPORT_SYMBOL(host_set_npu_core_on_off);

void set_npu_needed_info(void)
{
	struct airoha_npu *npu = g_npu;

	if (!npu)
		return;

	 /* set npu_test_area_base */
	set_npu_reg_data(npu, CR_NPU_MIB10, (u32)npu->rsv_pa + 0x200000);

	 /* set l2c_sram size */
	set_npu_reg_data(npu, CR_NPU_MIB11, airoha_get_l2c_sram());

	/* disable FPGA */
	set_npu_reg_data(npu, CR_NPU_MIB12, 0);

	 /* enable NPU TX uart */
	set_npu_reg_data(npu, CR_NPU_MIB21, 0);
}
EXPORT_SYMBOL(set_npu_needed_info);

u8 npu_wifi_offload_get_force_to_cpu_flag(void)
{
	struct airoha_npu *npu = g_npu;

	if (!npu)
		return 0;

	return READ_ONCE(npu->force_to_cpu);
}
EXPORT_SYMBOL(npu_wifi_offload_get_force_to_cpu_flag);

void npu_wifi_offload_set_force_to_cpu_flag(bool isForceToCpu)
{
	struct airoha_npu *npu = g_npu;

	if (!npu)
		return;

	WRITE_ONCE(npu->force_to_cpu, isForceToCpu);
}
EXPORT_SYMBOL(npu_wifi_offload_set_force_to_cpu_flag);

/*
 * buf_type: 0 for npu_init_code (to DRAM),
 *           1 for npu global data (to NPU_16K_SRAM)
 */
int npu_load_firmware(const u8 *buf, u32 buf_size, int buf_type)
{
	struct airoha_npu *npu = g_npu;
	u8 *dst;
	u32 i;

	if (!npu)
		return -1;

	if (buf_type == 0)
		dst = (u8 *)npu->rsv_va;
	else
		dst = (u8 *)npu->pbus_base;

	for (i = 0; i < buf_size; i++)
		WRITE_ONCE(dst[i], buf[i]);

	if (buf_type == 0)
		dma_sync_single_for_device(npu->dev, npu->rsv_pa, buf_size,
					   DMA_TO_DEVICE);

	return 0;
}
EXPORT_SYMBOL(npu_load_firmware);

/*
 * Returned Value:
 *  -- its range is 0~7.  0: error.  1~7: user defined and returned from NPU's callback function.
 *
 * npuMboxInfo_t's members are described as follows:
 *
 * core_id:
 *  -- the NPU core that Host is going to nority.
 *  -- its value is 0,1,2,or 3. corresponding enum "core0","core1","core2","core3"
 *     are defined in global_inc/modules/npu/npuMboxAPI.h
 *
 * func_id:
 *  -- the app (running on NPU core) that Host is going to nority.
 *  -- its value is defined in "enum npuMboxFuncId" in global_inc/modules/npu/npuMboxAPI.h
 *     user can add his app's func_id if needed. Note: max func_id is deifned as MAX_MBOX_FUNC.
 *
 * virtAddr,physAddr,len:
 *  -- the virtual address, physical address, and size of the memory buffer that Host wants
 *     NPU's (core,app) to handle.
 *
 * flags:
 *  -- its members are defined in npuMboxAPI.h. The following describes its members.
 *    -- isBlockingMode:
 *      -- isBlockingMode==1 means blocking mode. In this mode, host will be blocked until
 *          NPU's (core,app) finishs its corresponding callback function and returns
 *          status back to host.
 *      -- isBlockingMode==0 means non-blocking mode. In this mode, host will leave this
 *          API as soon as NPU's core receives mbox interrupt and returns 1 back to host.
 *          Make sure memory buffer used between Host and NPU can't be released when NPU's
 *          callback is still using it.
 *    -- isStaticBuffInit:
 *      -- only for internal use.
 *      -- This API's "cb" will be registered, and the memory buffer's (phyAddr,len) will
 *          passed to NPU.  Later NPU can notify host of information by the memory buffer.
 *
 * blockTimeout:
 *  -- valid when isBlockingMode==1.
 *  -- It means the max time that host can be blocked. Its unit is us.
 *
 * cb:
 *  -- should be "NULL" when isBlockingMode==1.
 *  -- when isBlockingMode==0, after NPU finishs its callback, NPU will notify Host to execute
 *      the "cb". Set "cb" as "NULL" if not needed.
 */
int host_notify_npuMbox(struct npuMboxInfo *mi)
{
	struct airoha_npu *npu = g_npu;
	struct airoha_npu_mbox *mbox;
	struct device *dev;
	union mboxArg arg;
	u32 val, reg_shift;
	int res = 0;
	int timeoutCnt = 300; /* default 30ms */
	unsigned long flags;

	if (!npu)
		return 0;

	if (mi->core_id >= NPU_MAX_CORE_NUM || mi->func_id >= MAX_MBOX_FUNC) {
		dev_err(dev, "(core_id:%d >= %d) || (func_id:%d >= %d)\n",
			mi->core_id, NPU_MAX_CORE_NUM, mi->func_id, MAX_MBOX_FUNC);
		return 0;
	}

	mbox = &npu->mbox_obj[mi->core_id];
	dev = npu->dev;

	arg.hWord = 0;

	if (mi->flags.isStaticBuffInit || mi->flags.isStaticBuffDel)
		arg.bits.static_buff = 1;

	if (mi->flags.isBlockingMode) {
		if (mi->blockTimeout)
			timeoutCnt = mi->blockTimeout;
		arg.bits.block_mode = 1;
	}

	spin_lock_irqsave(&mbox->lock, flags);

	mbox->mbox8_cb[mi->func_id] = mi->cb;

	arg.bits.func_id = (mi->func_id & MASK_FUNC_ID);

	reg_shift = mi->core_id << 4;

	if ((mi->va && mi->pa && mi->len) || mi->flags.isStaticBuffDel) {
		mbox->va[mi->func_id] = mi->va;
		mbox->pa[mi->func_id] = mi->pa;
		mbox->size[mi->func_id] = mi->len;
	} else {
		if (mbox->va[mi->func_id] == 0) {
			spin_unlock_irqrestore(&mbox->lock, flags);
			return 0;
		}
	}

	set_npu_reg_data(npu, CR_MBQ0_CTRL0 + reg_shift, mbox->pa[mi->func_id]);
	set_npu_reg_data(npu, CR_MBQ0_CTRL1 + reg_shift, mbox->size[mi->func_id]);
	set_npu_reg_data(npu, CR_MBQ0_CTRL3 + reg_shift, arg.hWord);

	val = get_npu_reg_data(npu, CR_MBQ0_CTRL2 + reg_shift);
	set_npu_reg_data(npu, CR_MBQ0_CTRL2 + reg_shift, val + 1);

	while (1) {
		arg.hWord = get_npu_reg_data(npu, CR_MBQ0_CTRL3 + reg_shift);
		if (arg.bits.done_bit == 1) {
			res = (arg.bits.ret_status & MASK_RET_STATUS);
			break;
		}

		timeoutCnt--;

		if (timeoutCnt >= 0) {
			if (mi->core_id == CORE5 && mi->func_id == MFUNC_DBA)
				udelay(40);
			else
				udelay(100);
		} else {
			dev_err(dev, "%s timeout for core_id: %d, func_id: %d\n",
				__func__, mi->core_id, mi->func_id);
			break;
		}
	}

	spin_unlock_irqrestore(&mbox->lock, flags);

	return res;
}
EXPORT_SYMBOL(host_notify_npuMbox);

static void host_dump_npu_csr(struct airoha_npu *npu, int core)
{
	const char *csr_name[] = {"PC", "SP", "LR"};
	const u32 csr_offs[] = {0x0, 0x4, 0x8};
	int i;

	printk("\n[H]Dump NPU core %d CSR START\n", core);
	for (i = 0; i < ARRAY_SIZE(csr_offs); i++) {
		u32 reg = CR_PC_BASE_ADDR + (core << 8) + csr_offs[i];
		u32 val = get_npu_reg_data(npu, reg);

		printk("%-15s<0x%x>\n", csr_name[i], val);
	}
	printk("[H]Dump CSR END\n\n");
}

static bool npu_is_wdg_enabled(struct airoha_npu *npu, int wdg_no)
{
	const u32 wdg_en_bit[] = {25, 26, 27, 28, 25, 26};
	u32 wdg_ctrl_reg, shift_bit, val;

#if defined(NPU_V2_S) || defined(NPU_V2_P)
	wdg_ctrl_reg = NPU_TIME0_BASE;
	shift_bit = wdg_en_bit[wdg_no];
#else
	wdg_ctrl_reg = NPU_TIME0_BASE + (wdg_no << 8);
	shift_bit = wdg_en_bit[0];
#endif

	if (wdg_no > 3)
		wdg_ctrl_reg = NPU_TIME1_BASE;

	val = get_npu_reg_data(npu, wdg_ctrl_reg);
	if (val & (1U << shift_bit))
		return true;

	return false;
}

static void npu_clear_wdg_intr(struct airoha_npu *npu, int wdg_no)
{
	const u32 wdg_intr_bit[] = {21, 22, 23, 24, 21, 22};
	u32 wdg_ctrl_reg, shift_bit, tmr_en_bits, val;

#if defined(NPU_V2_S) || defined(NPU_V2_P)
	wdg_ctrl_reg = NPU_TIME0_BASE;
	tmr_en_bits = 0x1e0001ef;	/* keep enable_bits (4 cntTimers + 4 wdogTimers) */
	shift_bit = wdg_intr_bit[wdg_no];
#else
	wdg_ctrl_reg = NPU_TIME0_BASE + (wdg_no << 8);
	tmr_en_bits = 0x02000027;	/* keep enable_bits (3 cntTimers + 1 wdogTimer) */
	shift_bit = wdg_intr_bit[0];
#endif

	if (wdg_no > 3)
		wdg_ctrl_reg = NPU_TIME1_BASE;

	val = get_npu_reg_data(npu, wdg_ctrl_reg);
	val &= tmr_en_bits;

	/* add wdg_intr_bit to clear wdog threshold interrupt */
	val |= (1U << shift_bit);

	/* clear timer source and keep timer enabled */
	set_npu_reg_data(npu, wdg_ctrl_reg, val);
}

static void mbox_tasklet_handler(unsigned long data)
{
	struct airoha_npu *npu = (struct airoha_npu *)data;
	struct airoha_npu_mbox *mbox;
	int status = 0;
	union mboxArg arg;
	u16 core_id, func_id, len;

	core_id = get_npu_reg_data(npu, CR_MBQ8_CTRL0) & MASK_CORE_ID;
	len = get_npu_reg_data(npu, CR_MBQ8_CTRL1);

	arg.hWord = get_npu_reg_data(npu, CR_MBQ8_CTRL3);
	func_id = arg.bits.func_id;

	mbox = &npu->mbox_obj[core_id];

	spin_lock(&mbox->buf_lock);

	if (mbox->mbox8_cb[func_id])
		status = mbox->mbox8_cb[func_id](mbox->va[func_id], len);

	spin_unlock(&mbox->buf_lock);

	arg.bits.ret_status = (status & MASK_RET_STATUS);
	arg.bits.done_bit = 1;

	/* let NPU know that host has done */
	set_npu_reg_data(npu, CR_MBQ8_CTRL3, arg.hWord);
}

static irqreturn_t npu_m2h_isr(int irq, void *dev_id)
{
	struct airoha_npu *npu = (struct airoha_npu *)dev_id;

	/* clear mbox8 interrupt */
	set_npu_reg_data(npu, CR_MBOX_INTR_STATUS, NPU_MBOX2HOST_BIT);
	tasklet_schedule(&npu->mbox_tasklet);

	return IRQ_HANDLED;
}

static irqreturn_t npu_wdg_isr(int irq, void *dev_id)
{
	struct airoha_npu *npu = (struct airoha_npu *)dev_id;
	int wdg_no;

	for (wdg_no = 0; wdg_no < NPU_MAX_CORE_NUM; wdg_no++) {
		if (irq == npu->irq_wdg[wdg_no])
			break;
	}

	if (wdg_no < NPU_MAX_CORE_NUM) {
		/* clear npu_wdog interrupt source */
		npu_clear_wdg_intr(npu, wdg_no);

		/* some npu timer and wdog share the same interupt, but only wdog case needs to print dbgMsg. */
		if (npu_is_wdg_enabled(npu, wdg_no)) {
			printk("[H]Recieve irq from npu_wdog%d with irq_num: %d\n",
				wdg_no, irq);
			host_dump_npu_csr(npu, wdg_no);
		}
	}

	return IRQ_HANDLED;
}

static int airoha_npu_probe(struct platform_device *pdev)
{
	struct airoha_npu *npu;
	struct resource *res, _res;
	struct device_node *ns;
	int i, ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no DT node found\n");
		return -EINVAL;
	}

	npu = devm_kzalloc(&pdev->dev, sizeof(*npu), GFP_KERNEL);
	if (!npu)
		return -ENOMEM;

	npu->dev = &pdev->dev;

	platform_set_drvdata(pdev, npu);

	/* get NPU RBUS base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	npu->rbus_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(npu->rbus_base))
		return PTR_ERR(npu->rbus_base);

	/* get NPU PBUS base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	npu->pbus_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(npu->pbus_base))
		return PTR_ERR(npu->pbus_base);

	/* get NPU interrupts */
	for (i = 0; i < NPU_MAX_INT_NUM; i++) {
		npu->irq[i] = platform_get_irq(pdev, i);
		if (npu->irq[i] < 0) {
			dev_err(npu->dev, "no IRQ[%d] resource found\n", i);
			return npu->irq[i];
		}
	}

	npu->irq_m2h = platform_get_irq(pdev, NPU_INT_M2H);
	if (npu->irq_m2h < 0) {
		dev_err(npu->dev, "no IRQ[%d] resource found\n", NPU_INT_M2H);
		return npu->irq_m2h;
	}

	/* get NPUv2 interrupts */
	for (i = 0; i < NPU_MAX_CORE_NUM; i++) {
		int irq = NPU_INT_WDG + i;

		npu->irq_wdg[i] = platform_get_irq(pdev, irq);
		if (npu->irq_wdg[i] < 0) {
			dev_err(npu->dev, "no IRQ[%d] resource found\n", irq);
			return npu->irq_wdg[i];
		}
	}

	ns = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!ns) {
		dev_err(npu->dev, "No memory-region specified\n");
		return -EINVAL;
	}

	res = &_res;
	ret = of_address_to_resource(ns, 0, res);

	of_node_put(ns);

	if (ret) {
		dev_err(npu->dev, "No memory address assigned to region\n");
		return -EINVAL;
	}

	npu->rsv_pa = res->start;
	npu->rsv_va = memremap(res->start, resource_size(res), MEMREMAP_WB);

	if (!npu->rsv_va)
		return -ENOMEM;

	if (dma_set_coherent_mask(npu->dev, NPU_DMA_MASK) == 0)
		dev_info(npu->dev, "NPU use 1GB DMA for coherent map\n");

	for (i = 0; i < NPU_MAX_CORE_NUM; i++) {
		spin_lock_init(&npu->mbox_obj[i].lock);
		spin_lock_init(&npu->mbox_obj[i].buf_lock);
	}

	tasklet_init(&npu->mbox_tasklet, mbox_tasklet_handler, (unsigned long)npu);

	ret = request_irq(npu->irq_m2h, npu_m2h_isr, 0, "npu_mbox2host", npu);
	if (ret) {
		dev_err(npu->dev, "unable to request NPU mbox2host IRQ#%i\n",
			npu->irq_m2h);
		return ret;
	}

	for (i = 0; i < NPU_MAX_CORE_NUM; i++) {
		ret = request_irq(npu->irq_wdg[i], npu_wdg_isr, 0,
				  wdg_irq_name[i], npu);
		if (ret) {
			u32 j;

			free_irq(npu->irq_m2h, npu);

			for (j = 0; j < i; j++)
				free_irq(npu->irq_wdg[j], npu);

			dev_err(npu->dev, "unable to request NPU watchdog IRQ#%i\n",
				npu->irq_wdg[i]);
			return ret;
		}
	}

	WRITE_ONCE(g_npu, npu);

	return 0;
}

static const struct of_device_id airoha_npu_of_id[] = {
	{ .compatible = "econet,ecnt-npu" },
	{ /* sentinel */ }
};

static struct platform_driver airoha_npu_driver = {
	.probe = airoha_npu_probe,
	.driver = {
		.name = "airoha-npu",
		.of_match_table = airoha_npu_of_id
	},
};
builtin_platform_driver(airoha_npu_driver);
