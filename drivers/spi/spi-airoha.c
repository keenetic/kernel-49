// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 AIROHA Inc.
 * Author: Ray Liu <ray.liu@airoha.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/mtd/spinand.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>

/* SPI */
#define SPI_CTL_REGS_READ_MODE			 0x0000
#define SPI_CTL_REGS_READ_IDLE_EN		 0x0004
#define SPI_CTL_REGS_SIDLY			 0x0008
#define SPI_CTL_REGS_CSHEXT			 0x000C
#define SPI_CTL_REGS_CSLEXT			 0x0010
#define SPI_CTL_REGS_MTX_MODE_TOG		 0x0014
#define SPI_CTL_REGS_RDCTL_FSM			 0x0018
#define SPI_CTL_REGS_MACMUX_SEL			 0x001C
#define SPI_CTL_REGS_MANUAL_EN			 0x0020
#define SPI_CTL_REGS_MANUAL_OPFIFO_EMPTY	 0x0024
#define SPI_CTL_REGS_MANUAL_OPFIFO_WDATA	 0x0028
#define SPI_CTL_REGS_MANUAL_OPFIFO_FULL		 0x002C
#define SPI_CTL_REGS_MANUAL_OPFIFO_WR		 0x0030
#define SPI_CTL_REGS_MANUAL_DFIFO_FULL		 0x0034
#define SPI_CTL_REGS_MANUAL_DFIFO_WDATA		 0x0038
#define SPI_CTL_REGS_MANUAL_DFIFO_EMPTY		 0x003C
#define SPI_CTL_REGS_MANUAL_DFIFO_RD		 0x0040
#define SPI_CTL_REGS_MANUAL_DFIFO_RDATA		 0x0044
#define SPI_CTL_REGS_DUMMY			 0x0080
#define SPI_CTL_REGS_PROBE_SEL			 0x0088
#define SPI_CTL_REGS_INTERRUPT			 0x0090
#define SPI_CTL_REGS_INTERRUPT_EN		 0x0094
#define SPI_CTL_REGS_SI_CK_SEL			 0x009C
#define SPI_CTL_REGS_SW_CFGNANDADDR_VAL		 0x010C
#define SPI_CTL_REGS_SW_CFGNANDADDR_EN		 0x0110
#define SPI_CTL_REGS_SFC_STRAP			 0x0114
#define SPI_CTL_REGS_NFI2SPI_EN			 0x0130

/* NFI2SPI */
#define SPI_NFI_REGS_CNFG			 0x0000
#define SPI_NFI_REGS_PAGEFMT			 0x0004
#define SPI_NFI_REGS_CON			 0x0008
#define SPI_NFI_REGS_INTR_EN			 0x0010
#define SPI_NFI_REGS_INTR			 0x0014
#define SPI_NFI_REGS_CMD			 0x0020
#define SPI_NFI_REGS_STA			 0x0060
#define SPI_NFI_REGS_FIFOSTA			 0x0064
#define SPI_NFI_REGS_STRADDR			 0x0080
#define SPI_NFI_REGS_FDM0L			 0x00A0
#define SPI_NFI_REGS_FDM0M			 0x00A4
#define SPI_NFI_REGS_FDM7L			 0x00D8
#define SPI_NFI_REGS_FDM7M			 0x00DC
#define SPI_NFI_REGS_FIFODATA0			 0x0190
#define SPI_NFI_REGS_FIFODATA1			 0x0194
#define SPI_NFI_REGS_FIFODATA2			 0x0198
#define SPI_NFI_REGS_FIFODATA3			 0x019C
#define SPI_NFI_REGS_MASTERSTA			 0x0224
#define SPI_NFI_REGS_SECCUS_SIZE		 0x022C
#define SPI_NFI_REGS_RD_CTL2			 0x0510
#define SPI_NFI_REGS_RD_CTL3			 0x0514
#define SPI_NFI_REGS_PG_CTL1			 0x0524
#define SPI_NFI_REGS_PG_CTL2			 0x0528
#define SPI_NFI_REGS_NOR_PROG_ADDR		 0x052C
#define SPI_NFI_REGS_NOR_RD_ADDR		 0x0534
#define SPI_NFI_REGS_SNF_MISC_CTL		 0x0538
#define SPI_NFI_REGS_SNF_MISC_CTL2		 0x053C
#define SPI_NFI_REGS_SNF_STA_CTL1		 0x0550
#define SPI_NFI_REGS_SNF_STA_CTL2		 0x0554
#define SPI_NFI_REGS_SNF_NFI_CNFG		 0x055C

/* SPI NAND Protocol OP */
#define SPI_NAND_OP_GET_FEATURE			 0x0F
#define SPI_NAND_OP_SET_FEATURE			 0x1F
#define SPI_NAND_OP_PAGE_READ			 0x13
#define SPI_NAND_OP_READ_FROM_CACHE_SINGLE	 0x03
#define SPI_NAND_OP_READ_FROM_CACHE_SINGLE_FAST	 0x0B
#define SPI_NAND_OP_READ_FROM_CACHE_DUAL	 0x3B
#define SPI_NAND_OP_READ_FROM_CACHE_DUAL_IO	 0xBB
#define SPI_NAND_OP_READ_FROM_CACHE_QUAD	 0x6B
#define SPI_NAND_OP_READ_FROM_CACHE_QUAD_IO	 0xEB
#define SPI_NAND_OP_WRITE_ENABLE		 0x06
#define SPI_NAND_OP_WRITE_DISABLE		 0x04
#define SPI_NAND_OP_PROGRAM_LOAD_SINGLE		 0x02
#define SPI_NAND_OP_PROGRAM_LOAD_QUAD		 0x32
#define SPI_NAND_OP_PROGRAM_LOAD_RANDOM_SINGLE	 0x84
#define SPI_NAND_OP_PROGRAM_LOAD_RANDOM_QUAD	 0x34
#define SPI_NAND_OP_PROGRAM_EXECUTE		 0x10
#define SPI_NAND_OP_READ_ID			 0x9F
#define SPI_NAND_OP_BLOCK_ERASE			 0xD8
#define SPI_NAND_OP_RESET			 0xFF
#define SPI_NAND_OP_DIE_SELECT			 0xC2

#define write_reg_spi(a, b)			 writel(b, as->spi_base + (a))
#define read_reg_spi(a)				 readl(as->spi_base + (a))

#define write_reg_nfi(a, b)			 writel(b, as->nfi_base + (a))
#define read_reg_nfi(a)				 readl(as->nfi_base + (a))
#define write_reg_nfi_mask(a, b, c)		\
	write_reg_nfi(a, (read_reg_nfi(a) & ~(b)) | (c))

#define SPI_TIMEOUT				(250 * USEC_PER_MSEC)
#define NFI_TIMEOUT				(  2 * USEC_PER_MSEC)

#define SPI_NAND_CACHE_SIZE			(4096 + 256)

/* Chip SCU */
#define RG_IOMUX_CONTROL2			(0x218)
#define   GPIO_SPI_QUAD_MODE			 BIT(4)

enum SPI_CONTROLLER_MODE {
	SPI_CONTROLLER_MODE_AUTO = 0,
	SPI_CONTROLLER_MODE_MANUAL,
	SPI_CONTROLLER_MODE_DMA
};

enum SPI_CONTROLLER_CHIP_SELECT {
	SPI_CONTROLLER_CHIP_SELECT_HIGH = 0,
	SPI_CONTROLLER_CHIP_SELECT_LOW,
};

struct airoha_nfi_conf {
	size_t page_size;
	size_t oob_size;
	size_t sec_size;
	u8 sec_num;
	u8 spare_size;
	u8 dummy_byte_num;
};

struct airoha_snand {
	struct spi_master *master;
	struct device *dev;
	void __iomem *spi_base;
	void __iomem *nfi_base;
	struct airoha_nfi_conf nfi_cfg;
	u8 *txrx_buf;
	dma_addr_t dma_addr;
	size_t buf_len;
	bool support_quad;
	bool done;
};

static int airoha_enable_quad_mode(struct device_node *np)
{
	struct device_node *ns;
	void __iomem *scu_base;
	u32 val;

	ns = of_parse_phandle(np, "airoha,scu", 0);
	if (!ns)
		return -ENODEV;

	/* map Chip SCU */
	scu_base = of_iomap(ns, 1);

	of_node_put(ns);

	if (!scu_base)
		return -ENOMEM;

	/* enable quad mode pins (SPI_WP + SPI_HOLD) */
	val = readl(scu_base + RG_IOMUX_CONTROL2);
	val |= GPIO_SPI_QUAD_MODE;
	writel(val, scu_base + RG_IOMUX_CONTROL2);

	iounmap(scu_base);

	return 0;
}

static int airoha_spi_set_fifo_op(struct airoha_snand *as, u8 op_cmd, u32 op_len)
{
	u8 __iomem *poll_reg;
	u32 val;
	int err;

	write_reg_spi(SPI_CTL_REGS_MANUAL_OPFIFO_WDATA,
		      (((op_cmd & 0x1f) << 0x9) | (op_len & 0x1ff)));

	poll_reg = as->spi_base + SPI_CTL_REGS_MANUAL_OPFIFO_FULL;
	err = readl_poll_timeout(poll_reg, val, !(val & BIT(0)), 0, SPI_TIMEOUT);
	if (err)
		return err;

	write_reg_spi(SPI_CTL_REGS_MANUAL_OPFIFO_WR, 0x1);

	poll_reg = as->spi_base + SPI_CTL_REGS_MANUAL_OPFIFO_EMPTY;
	err = readl_poll_timeout(poll_reg, val, (val & BIT(0)), 0, SPI_TIMEOUT);

	return err;
}

static inline int airoha_spi_set_cs(struct airoha_snand *as,
				    enum SPI_CONTROLLER_CHIP_SELECT cs)
{
	return airoha_spi_set_fifo_op(as, cs, 1);
}

static int airoha_spi_write_data_fifo(struct airoha_snand *as,
				      const u8 *ptr_data, u32 data_len)
{
	u8 __iomem *poll_reg = as->spi_base + SPI_CTL_REGS_MANUAL_DFIFO_FULL;
	u32 idx, val, data;
	int err;

	for (idx = 0; idx < data_len; idx++) {
		/* wait until dfifo is not full */
		err = readl_poll_timeout(poll_reg, val, !(val & BIT(0)), 0,
					 SPI_TIMEOUT);
		if (err)
			return err;

		/* write data to register DFIFO_WDATA */
		data = *(ptr_data + idx);
		write_reg_spi(SPI_CTL_REGS_MANUAL_DFIFO_WDATA, data);

		/* wait until dfifo is not full */
		err = readl_poll_timeout(poll_reg, val, !(val & BIT(0)), 0,
					 SPI_TIMEOUT);
		if (err)
			return err;
	}

	return 0;
}

static int airoha_spi_read_data_fifo(struct airoha_snand *as,
				     u8 *ptr_data, u32 data_len)
{
	u8 __iomem *poll_reg = as->spi_base + SPI_CTL_REGS_MANUAL_DFIFO_EMPTY;
	u32 idx, val, data;
	int err;

	for (idx = 0; idx < data_len; idx++) {
		/* wait until dfifo is not empty */
		err = readl_poll_timeout(poll_reg, val, !(val & BIT(0)), 0,
					 SPI_TIMEOUT);
		if (err)
			return err;

		/* read from dfifo to register DFIFO_RDATA */
		data = read_reg_spi(SPI_CTL_REGS_MANUAL_DFIFO_RDATA);
		*(ptr_data + idx) = (u8)(data & 0xff);

		/* enable register DFIFO_RD to read next byte */
		write_reg_spi(SPI_CTL_REGS_MANUAL_DFIFO_RD, 0x1);
	}

	return 0;
}

static int airoha_spi_write_one_byte(struct airoha_snand *as, u8 cmd,
				     const u8 *ptr_data)
{
	int ret;

	ret = airoha_spi_set_fifo_op(as, cmd, 1);
	if (ret)
		return ret;

	return airoha_spi_write_data_fifo(as, ptr_data, 1);
}

static int airoha_spi_write_nbytes(struct airoha_snand *as, u8 cmd,
				   const u8 *ptr_data, u32 len)
{
	u32 remain_len, offs;
	int ret;

	remain_len = len;
	offs = 0;

	while (remain_len > 0) {
		u32 data_len = (remain_len > 0x1ff) ? 0x1ff : remain_len;

		ret = airoha_spi_set_fifo_op(as, cmd, data_len);
		if (ret)
			return ret;

		ret = airoha_spi_write_data_fifo(as, ptr_data + offs, data_len);
		if (ret)
			return ret;

		remain_len -= data_len;
		offs += data_len;
	}

	return 0;
}

static int airoha_spi_read_nbytes(struct airoha_snand *as,
				  u8 *ptr_data, u32 len)
{
	u32 remain_len, offs;
	int ret;

	remain_len = len;
	offs = 0;

	while (remain_len > 0) {
		u32 data_len = (remain_len > 0x1ff) ? 0x1ff : remain_len;

		ret = airoha_spi_set_fifo_op(as, 0x0c, data_len);
		if (ret)
			return ret;

		ret = airoha_spi_read_data_fifo(as, ptr_data + offs, data_len);
		if (ret)
			return ret;

		remain_len -= data_len;
		offs += data_len;
	}

	return 0;
}

static int airoha_snand_nfi_init(struct airoha_snand *as)
{
	/* switch to SPI NAND mode */
	write_reg_nfi(SPI_NFI_REGS_SNF_NFI_CNFG, 0x1);

	/* enable DMA done interrupt */
	write_reg_nfi_mask(SPI_NFI_REGS_INTR_EN, 0x7f, BIT(6));

	return 0;
}

static void airoha_snand_nfi_config(struct airoha_snand *as)
{
	u32 val;

	/* reset nfi */
	write_reg_nfi(SPI_NFI_REGS_CON, 0x3);

	/* disable AutoFDM, HW ECC, enable DMA burst */
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, BIT(9) | BIT(8) | BIT(2), BIT(2));

	/* page format */
	switch (as->nfi_cfg.spare_size) {
	case 28:
		val = (0x3 << 4);
		break;
	case 27:
		val = (0x2 << 4);
		break;
	case 26:
		val = (0x1 << 4);
		break;
	default:
		val = (0x0 << 4);
		break;
	}

	write_reg_nfi_mask(SPI_NFI_REGS_PAGEFMT, 0x0030, val);

	switch (as->nfi_cfg.page_size) {
	case 4096:
		val = (0x2 << 0);
		break;
	case 2048:
		val = (0x1 << 0);
		break;
	default:
		val = (0x0 << 0);
		break;
	}

	write_reg_nfi_mask(SPI_NFI_REGS_PAGEFMT, 0x0003, val);

	/* sec num */
	val = as->nfi_cfg.sec_num << 12;
	write_reg_nfi_mask(SPI_NFI_REGS_CON, 0xf000, val);

	/* enable custom sec size */
	write_reg_nfi_mask(SPI_NFI_REGS_SECCUS_SIZE, BIT(16), BIT(16));

	/* set custom sec size */
	val = as->nfi_cfg.sec_size;
	write_reg_nfi_mask(SPI_NFI_REGS_SECCUS_SIZE, 0x1fff, val);
}

static int airoha_spi_set_mode(struct airoha_snand *as,
			       enum SPI_CONTROLLER_MODE mode)
{
	u8 __iomem *poll_reg = as->spi_base + SPI_CTL_REGS_RDCTL_FSM;
	u32 val;
	int err;

	switch (mode) {
	case SPI_CONTROLLER_MODE_AUTO:
		write_reg_spi(SPI_CTL_REGS_READ_IDLE_EN, 0x0);
		write_reg_spi(SPI_CTL_REGS_MTX_MODE_TOG, 0x0);
		write_reg_spi(SPI_CTL_REGS_MANUAL_EN, 0x0);
		break;

	case SPI_CONTROLLER_MODE_MANUAL:
		write_reg_spi(SPI_CTL_REGS_NFI2SPI_EN, 0x0);
		write_reg_spi(SPI_CTL_REGS_READ_IDLE_EN, 0x0);

		err = readl_poll_timeout(poll_reg, val, !(val & 0xf), 0,
					 SPI_TIMEOUT);
		if (err)
			return err;

		write_reg_spi(SPI_CTL_REGS_MTX_MODE_TOG, 0x9);
		write_reg_spi(SPI_CTL_REGS_MANUAL_EN, 0x1);
		break;

	case SPI_CONTROLLER_MODE_DMA:
		write_reg_spi(SPI_CTL_REGS_NFI2SPI_EN, 0x1);
		write_reg_spi(SPI_CTL_REGS_MTX_MODE_TOG, 0x0);
		write_reg_spi(SPI_CTL_REGS_MANUAL_EN, 0x0);
		break;
	default:
		break;
	}

	write_reg_spi(SPI_CTL_REGS_DUMMY, as->nfi_cfg.dummy_byte_num);

	return 0;
}

static bool airoha_snand_is_page_ops(const struct airoha_snand *as,
				     const struct spi_mem_op *op)
{
	if (op->addr.nbytes != 2)
		return false;

	if (op->addr.buswidth != 1 && op->addr.buswidth != 2 &&
	    op->addr.buswidth != 4)
		return false;

	/* match read from page instructions */
	if (op->data.dir == SPI_MEM_DATA_IN) {
		/* check dummy cycle first */
		if (op->dummy.nbytes * BITS_PER_BYTE / op->dummy.buswidth > 0xf)
			return false;

		/* quad io / quad out */
		if ((op->addr.buswidth == 4 || op->addr.buswidth == 1) &&
		    op->data.buswidth == 4 && as->support_quad)
			return true;

		/* dual io / dual out */
		if ((op->addr.buswidth == 2 || op->addr.buswidth == 1) &&
		     op->data.buswidth == 2)
			return true;

		/* standard spi */
		if (op->addr.buswidth == 1 && op->data.buswidth == 1)
			return true;
	} else if (op->data.dir == SPI_MEM_DATA_OUT) {
		/* check dummy cycle first */
		if (op->dummy.nbytes)
			return false;

		/* program load quad out */
		if (op->addr.buswidth == 1 && op->data.buswidth == 4 &&
		    as->support_quad)
			return true;

		/* standard spi */
		if (op->addr.buswidth == 1 && op->data.buswidth == 1)
			return true;
	}

	return false;
}

static int airoha_snand_adjust_op_size(struct spi_mem *mem,
				       struct spi_mem_op *op)
{
	struct airoha_snand *as = spi_master_get_devdata(mem->spi->master);
	size_t max_len;

	if (!as->done)
		return 0;

	if (airoha_snand_is_page_ops(as, op)) {
		max_len = as->nfi_cfg.sec_size + as->nfi_cfg.spare_size;
		max_len *= as->nfi_cfg.sec_num;

		if (op->data.nbytes > max_len)
			op->data.nbytes = max_len;
	} else {
		max_len = 1 + op->addr.nbytes + op->dummy.nbytes;

		if (max_len >= 160)
			return -EOPNOTSUPP;

		if (op->data.nbytes > 160 - max_len)
			op->data.nbytes = 160 - max_len;
	}

	return 0;
}

static bool airoha_snand_supports_op(struct spi_mem *mem,
				     const struct spi_mem_op *op)
{
	struct airoha_snand *as = spi_master_get_devdata(mem->spi->master);

	if (!spi_mem_default_supports_op(mem, op))
		return false;

	if (op->cmd.buswidth != 1)
		return false;

	if (airoha_snand_is_page_ops(as, op))
		return true;

	return ((op->addr.nbytes == 0 || op->addr.buswidth == 1) &&
		(op->dummy.nbytes == 0 || op->dummy.buswidth == 1) &&
		(op->data.nbytes == 0 || op->data.buswidth == 1));
}

static int airoha_snand_dirmap_create(struct spi_mem_dirmap_desc *desc)
{
	struct airoha_snand *as = spi_master_get_devdata(desc->mem->spi->master);

	if (!as->txrx_buf)
		return -EINVAL;

	if (desc->info.offset + desc->info.length > U32_MAX)
		return -EINVAL;

	if (!airoha_snand_supports_op(desc->mem, &desc->info.op_tmpl))
		return -EOPNOTSUPP;

	return 0;
}

static ssize_t airoha_snand_dirmap_read(struct spi_mem_dirmap_desc *desc,
					u64 offs, size_t len, void *buf)
{
	struct airoha_snand *as = spi_master_get_devdata(desc->mem->spi->master);
	struct spi_mem_op *op = &desc->info.op_tmpl;
	u8 __iomem *poll_reg;
	u32 rd_mode, val;
	int err;

	if (op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_SINGLE ||
	    op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_SINGLE_FAST)
		rd_mode = 0x0;
	else if (op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_DUAL)
		rd_mode = 0x1;
	else if (op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_DUAL_IO)
		rd_mode = 0x5;
	else if (op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_QUAD)
		rd_mode = 0x2;
	else if (op->cmd.opcode == SPI_NAND_OP_READ_FROM_CACHE_QUAD_IO)
		rd_mode = 0x6;
	else
		rd_mode = 0x0;

	airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_DMA);

	airoha_snand_nfi_config(as);

	dma_sync_single_for_device(as->dev, as->dma_addr, as->buf_len,
				   DMA_BIDIRECTIONAL);

	/* set dma addr */
	write_reg_nfi(SPI_NFI_REGS_STRADDR, (u32)as->dma_addr);

	/* set read data byte num */
	val = as->nfi_cfg.sec_size * as->nfi_cfg.sec_num;
	write_reg_nfi_mask(SPI_NFI_REGS_SNF_MISC_CTL2, 0x1fff, val);

	/* set read command, mode and addr */
	write_reg_nfi(SPI_NFI_REGS_RD_CTL2, op->cmd.opcode);
	write_reg_nfi(SPI_NFI_REGS_SNF_MISC_CTL, (rd_mode << 16));
	write_reg_nfi(SPI_NFI_REGS_RD_CTL3, 0x0);

	/* set nfi read */
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, 0x7000, (6 << 12));
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, 0x0003, 0x0003);
	write_reg_nfi(SPI_NFI_REGS_CMD, 0x00);

	/* trigger dma start read */
	write_reg_nfi_mask(SPI_NFI_REGS_CON, BIT(8), 0);
	write_reg_nfi_mask(SPI_NFI_REGS_CON, BIT(8), BIT(8));

	poll_reg = as->nfi_base + SPI_NFI_REGS_SNF_STA_CTL1;
	err = readl_poll_timeout(poll_reg, val, (val & BIT(25)), 0, NFI_TIMEOUT);
	if (err) {
		dev_err(as->dev, "READ FROM CACHE done timeout\n");
		return err;
	}

	/*
	 * SPI_NFI_READ_FROM_CACHE_DONE bit must be written at the end
	 * of dirmap_read operation even if it is already set.
	 */
	write_reg_nfi_mask(SPI_NFI_REGS_SNF_STA_CTL1, BIT(25), BIT(25));

	poll_reg = as->nfi_base + SPI_NFI_REGS_INTR;
	err = readl_poll_timeout(poll_reg, val, (val & BIT(6)), 0, NFI_TIMEOUT);
	if (err) {
		dev_err(as->dev, "AHB done timeout\n");
		return err;
	}

	/* DMA read need delay for data ready from controller to DRAM */
	udelay(1);

	dma_sync_single_for_cpu(as->dev, as->dma_addr, as->buf_len,
				DMA_BIDIRECTIONAL);

	airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_MANUAL);

	memcpy(buf, as->txrx_buf + offs, len);

	return len;
}

static ssize_t airoha_snand_dirmap_write(struct spi_mem_dirmap_desc *desc,
					 u64 offs, size_t len, const void *buf)
{
	struct airoha_snand *as = spi_master_get_devdata(desc->mem->spi->master);
	struct spi_mem_op *op = &desc->info.op_tmpl;
	u8 __iomem *poll_reg;
	u32 wr_mode, val;
	int err;

	if (op->cmd.opcode == SPI_NAND_OP_PROGRAM_LOAD_QUAD ||
	    op->cmd.opcode == SPI_NAND_OP_PROGRAM_LOAD_RANDOM_QUAD)
		wr_mode = 0x2;
	else
		wr_mode = 0x0;

	airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_MANUAL);

	dma_sync_single_for_cpu(as->dev, as->dma_addr, as->buf_len,
				DMA_BIDIRECTIONAL);
	memcpy(as->txrx_buf + offs, buf, len);
	dma_sync_single_for_device(as->dev, as->dma_addr, as->buf_len,
				   DMA_BIDIRECTIONAL);

	airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_DMA);

	airoha_snand_nfi_config(as);

	/* set dma addr */
	write_reg_nfi(SPI_NFI_REGS_STRADDR, (u32)as->dma_addr);

	/* set prog load byte num */
	val = (as->nfi_cfg.sec_size * as->nfi_cfg.sec_num) << 16;
	write_reg_nfi_mask(SPI_NFI_REGS_SNF_MISC_CTL2, 0x1fff0000, val);

	/* set write command and mode */
	write_reg_nfi(SPI_NFI_REGS_PG_CTL1, ((u32)op->cmd.opcode << 8));
	write_reg_nfi(SPI_NFI_REGS_SNF_MISC_CTL, (wr_mode << 16));
	write_reg_nfi(SPI_NFI_REGS_PG_CTL2, 0x0);

	/* set nfi write */
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, BIT(1), 0);
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, 0x7000, (3 << 12));
	write_reg_nfi_mask(SPI_NFI_REGS_CNFG, BIT(0), BIT(0));
	write_reg_nfi(SPI_NFI_REGS_CMD, 0x80);

	/* trigger dma start write */
	write_reg_nfi_mask(SPI_NFI_REGS_CON, BIT(9), 0);
	write_reg_nfi_mask(SPI_NFI_REGS_CON, BIT(9), BIT(9));

	poll_reg = as->nfi_base + SPI_NFI_REGS_INTR;
	err = readl_poll_timeout(poll_reg, val, (val & BIT(6)), 0, NFI_TIMEOUT);
	if (err) {
		dev_err(as->dev, "AHB done timeout\n");
		return err;
	}

	poll_reg = as->nfi_base + SPI_NFI_REGS_SNF_STA_CTL1;
	err = readl_poll_timeout(poll_reg, val, (val & BIT(26)), 0, NFI_TIMEOUT);
	if (err) {
		dev_err(as->dev, "LOAD TO CACHE done timeout\n");
		return err;
	}

	/*
	 * SPI_NFI_LOAD_TO_CACHE_DONE bit must be written at the end
	 * of dirmap_write operation even if it is already set.
	 */
	write_reg_nfi_mask(SPI_NFI_REGS_SNF_STA_CTL1, BIT(26), BIT(26));

	airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_MANUAL);

	return len;
}

static inline int airoha_snand_op_transfer(struct airoha_snand *as,
					   const struct spi_mem_op *op)
{
	u32 idx;
	u8 cmd, addr, data;
	int ret;

	/* switch to manual mode */
	ret = airoha_spi_set_mode(as, SPI_CONTROLLER_MODE_MANUAL);
	if (ret)
		return ret;

	ret = airoha_spi_set_cs(as, SPI_CONTROLLER_CHIP_SELECT_LOW);
	if (ret)
		return ret;

	/* opcode */
	ret = airoha_spi_write_one_byte(as, 0x08, &op->cmd.opcode);
	if (ret)
		goto unselect_chip;

	/* addr part */
	cmd = op->cmd.opcode == SPI_NAND_OP_GET_FEATURE ? 0x11 : 0x08;
	for (idx = 0; idx < op->addr.nbytes; idx++) {
		addr = (op->addr.val >> ((op->addr.nbytes - idx - 1) * 8)) & 0xff;
		ret = airoha_spi_write_one_byte(as, cmd, &addr);
		if (ret)
			goto unselect_chip;
	}

	/* dummy part */
	data = 0xff;
	for (idx = 0; idx < op->dummy.nbytes; idx++) {
		ret = airoha_spi_write_one_byte(as, 0x08, &data);
		if (ret)
			goto unselect_chip;
	}

	/* data part */
	if (op->data.dir == SPI_MEM_DATA_IN)
		ret = airoha_spi_read_nbytes(as, op->data.buf.in,
					     op->data.nbytes);
	else
		ret = airoha_spi_write_nbytes(as, 0x08, op->data.buf.out,
					      op->data.nbytes);

unselect_chip:
	airoha_spi_set_cs(as, SPI_CONTROLLER_CHIP_SELECT_HIGH);

	return ret;
}

static int airoha_snand_exec_op(struct spi_mem *mem,
				const struct spi_mem_op *op)
{
	struct airoha_snand *as = spi_master_get_devdata(mem->spi->master);
	struct spinand_device *spinand = (struct spinand_device *)mem->drvpriv;

	if (!as->done && spinand != NULL && spinand->manufacturer != NULL) {
		struct nand_device *nand = spinand_to_nand(spinand);
		int pagesize = nand->memorg.pagesize;
		int oobsize = nand->memorg.oobsize;
		int sec_num, sec_size;

		if (pagesize == 4096)
			sec_num = 8;
		else if (pagesize == 2048)
			sec_num = 4;
		else
			sec_num = 1;

		sec_size = (pagesize + oobsize) / sec_num;
		as->nfi_cfg.sec_num = sec_num;
		as->nfi_cfg.sec_size = sec_size;
		as->nfi_cfg.page_size = ((sec_size * sec_num) / 1024) * 1024;
		as->nfi_cfg.oob_size = (sec_size * sec_num) % 1024;

		airoha_snand_nfi_init(as);
		airoha_snand_nfi_config(as);

		as->done = true;
	}

	return airoha_snand_op_transfer(as, op);
}

static const struct spi_controller_mem_ops airoha_snand_mem_ops = {
	.adjust_op_size = airoha_snand_adjust_op_size,
	.supports_op = airoha_snand_supports_op,
	.exec_op = airoha_snand_exec_op,
	.dirmap_create = airoha_snand_dirmap_create,
	.dirmap_read = airoha_snand_dirmap_read,
	.dirmap_write = airoha_snand_dirmap_write,
};

static int airoha_snand_setup(struct spi_device *spi)
{
	struct airoha_snand *as = spi_master_get_devdata(spi->master);
	int ret;

	/* init value */
	as->nfi_cfg.spare_size = 16;
	as->nfi_cfg.dummy_byte_num = 0;

	/* prepare buffer */
	as->buf_len = SPI_NAND_CACHE_SIZE;
	as->txrx_buf = kzalloc(as->buf_len, GFP_KERNEL);
	if (!as->txrx_buf)
		return -ENOMEM;

	as->dma_addr = dma_map_single(as->dev, as->txrx_buf,
				      as->buf_len, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(as->dev, as->dma_addr);
	if (ret) {
		dev_err(as->dev, "dma mapping error\n");
		goto free_buf;
	}

	return 0;

free_buf:
	kfree(as->txrx_buf);
	as->txrx_buf = NULL;

	return ret;
}

static void airoha_snand_cleanup(struct spi_device *spi)
{
	struct airoha_snand *as = spi_master_get_devdata(spi->master);

	if (as->txrx_buf) {
		dma_unmap_single(as->dev, as->dma_addr, as->buf_len,
				 DMA_BIDIRECTIONAL);
		kfree(as->txrx_buf);
		as->txrx_buf = NULL;
	}
}

static int airoha_snand_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct airoha_snand *as;
	struct resource *res;
	int ret;

	master = spi_alloc_master(&pdev->dev, sizeof(*as));
	if (!master) {
		dev_err(&pdev->dev, "failed to alloc spi master\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);

	/* spi platform */
	as = spi_master_get_devdata(master);
	as->master = master;
	as->dev = &pdev->dev;
	as->done = false;
	as->support_quad = false;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	as->spi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(as->spi_base)) {
		ret = PTR_ERR(as->spi_base);
		goto err_put_controller;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	as->nfi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(as->nfi_base)) {
		ret = PTR_ERR(as->nfi_base);
		goto err_put_controller;
	}

	ret = dma_set_mask(as->dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_put_controller;

	if (of_property_read_bool(pdev->dev.of_node, "support_quad"))
		as->support_quad = true;

	if (as->support_quad) {
		/* control SPI QUAD mode pins */
		ret = airoha_enable_quad_mode(pdev->dev.of_node);
		if (ret)
			as->support_quad = false;
	}

	/* Hook function pointer and settings to kernel */
	master->num_chipselect = 2;
	master->mem_ops = &airoha_snand_mem_ops;
	master->bits_per_word_mask = SPI_BPW_MASK(8);
	if (as->support_quad)
		master->mode_bits = SPI_RX_QUAD | SPI_TX_QUAD |
				    SPI_RX_DUAL | SPI_TX_DUAL;
	else
		master->mode_bits = SPI_RX_DUAL;
	master->dev.of_node = pdev->dev.of_node;
	master->setup = airoha_snand_setup;
	master->cleanup = airoha_snand_cleanup;

	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "failed to register master (%d)\n", ret);
		goto err_put_controller;
	}

	return 0;

err_put_controller:
	spi_master_put(master);
	return ret;
}

static const struct of_device_id airoha_snand_ids[] = {
	{ .compatible = "airoha,airoha-snand" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, airoha_snand_ids);

static struct platform_driver airoha_snand_driver = {
	.probe = airoha_snand_probe,
	.driver = {
		.name = "airoha-snand",
		.of_match_table = airoha_snand_ids,
	},
};

module_platform_driver(airoha_snand_driver);

MODULE_DESCRIPTION("AIROHA SPI NAND driver");
MODULE_AUTHOR("Ray Liu <ray.liu@airoha.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:airoha-snand");
