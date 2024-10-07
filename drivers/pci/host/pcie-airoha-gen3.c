// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek PCIe host controller driver.
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>

#include "../pci.h"

#include <soc/airoha/pkgid.h>

#define PCIE_AN7581_P1_BASE		0x1fc20000

#define PCIE_SETTING_REG		0x80
#define PCIE_G3_SUPPORTED		BIT(13)
#define PCIE_RC_MODE			BIT(0)

#define PCIE_PCI_IDS_1			0x9c
#define PCI_CLASS(class)		(class << 8)

#define PCIE_EQ_PRESET_01_REG		0x100
#define PCIE_VAL_LN0_DOWNSTREAM		GENMASK(6, 0)
#define PCIE_VAL_LN0_UPSTREAM		GENMASK(14, 8)
#define PCIE_VAL_LN1_DOWNSTREAM		GENMASK(22, 16)
#define PCIE_VAL_LN1_UPSTREAM		GENMASK(30, 24)

#define PCIE_CFGNUM_REG			0x140
#define PCIE_CFG_DEVFN(devfn)		((devfn) & GENMASK(7, 0))
#define PCIE_CFG_BUS(bus)		(((bus) << 8) & GENMASK(15, 8))
#define PCIE_CFG_BYTE_EN(bytes)		(((bytes) << 16) & GENMASK(19, 16))
#define PCIE_CFG_FORCE_BYTE_EN		BIT(20)
#define PCIE_CFG_OFFSET_ADDR		0x1000
#define PCIE_CFG_HEADER(bus, devfn) \
	(PCIE_CFG_BUS(bus) | PCIE_CFG_DEVFN(devfn))

#define PCIE_RST_CTRL_REG		0x148
#define PCIE_MAC_RSTB			BIT(0)
#define PCIE_PHY_RSTB			BIT(1)
#define PCIE_BRG_RSTB			BIT(2)
#define PCIE_PE_RSTB			BIT(3)

#define PCIE_LTSSM_STATUS_REG		0x150
#define PCIE_LTSSM_STATE_MASK		GENMASK(28, 24)
#define PCIE_LTSSM_STATE(val)		((val & PCIE_LTSSM_STATE_MASK) >> 24)
#define PCIE_LTSSM_STATE_L2_IDLE	0x14

#define PCIE_LINK_STATUS_REG		0x154
#define PCIE_PORT_LINKUP		BIT(8)

#define PCIE_MSI_SET_NUM		8
#define PCIE_MSI_IRQS_PER_SET		32
#define PCIE_MSI_IRQS_NUM \
	(PCIE_MSI_IRQS_PER_SET * PCIE_MSI_SET_NUM)

#define PCIE_INT_ENABLE_REG		0x180
#define PCIE_MSI_ENABLE			GENMASK(PCIE_MSI_SET_NUM + 8 - 1, 8)
#define PCIE_MSI_SHIFT			8
#define PCIE_INTX_SHIFT			24
#define PCIE_INTX_ENABLE \
	GENMASK(PCIE_INTX_SHIFT + PCI_NUM_INTX - 1, PCIE_INTX_SHIFT)

#define PCIE_INT_STATUS_REG		0x184
#define PCIE_MSI_SET_ENABLE_REG		0x190
#define PCIE_MSI_SET_ENABLE		GENMASK(PCIE_MSI_SET_NUM - 1, 0)

#define PCIE_ISTATUS_LOCAL_CTRL		0x1ac
#define PCIE_ISTATUS_LOCAL_INTX_HWCLR	BIT(3)

#define PCIE_PIPE4_PIE8_REG		0x338
#define PCIE_K_FINETUNE_MAX		GENMASK(5, 0)
#define PCIE_K_FINETUNE_ERR		GENMASK(7, 6)
#define PCIE_K_PRESET_TO_USE		GENMASK(18, 8)
#define PCIE_K_PHYPARAM_QUERY		BIT(19)
#define PCIE_K_QUERY_TIMEOUT		BIT(20)
#define PCIE_K_PRESET_TO_USE_16G	GENMASK(31, 21)

#define PCIE_PHY_MAC_CFG_REG		0x33c
#define PCIE_EQU_PHASES_23		BIT(0)

#define PCIE_MSI_SET_BASE_REG		0xc00
#define PCIE_MSI_SET_OFFSET		0x10
#define PCIE_MSI_SET_STATUS_OFFSET	0x04
#define PCIE_MSI_SET_ENABLE_OFFSET	0x08

#define PCIE_MSI_SET_ADDR_HI_BASE	0xc80
#define PCIE_MSI_SET_ADDR_HI_OFFSET	0x04

#define PCIE_ICMD_PM_REG		0x198
#define PCIE_TURN_OFF_LINK		BIT(4)

#define PCIE_TRANS_TABLE_BASE_REG	0x800
#define PCIE_ATR_SRC_ADDR_MSB_OFFSET	0x4
#define PCIE_ATR_TRSL_ADDR_LSB_OFFSET	0x8
#define PCIE_ATR_TRSL_ADDR_MSB_OFFSET	0xc
#define PCIE_ATR_TRSL_PARAM_OFFSET	0x10
#define PCIE_ATR_TLB_SET_OFFSET		0x20

#define PCIE_MAX_TRANS_TABLES		8
#define PCIE_ATR_EN			BIT(0)
#define PCIE_ATR_SIZE(size) \
	(((((size) - 1) << 1) & GENMASK(6, 1)) | PCIE_ATR_EN)
#define PCIE_ATR_ID(id)			((id) & GENMASK(3, 0))
#define PCIE_ATR_TYPE_MEM		PCIE_ATR_ID(0)
#define PCIE_ATR_TYPE_IO		PCIE_ATR_ID(1)
#define PCIE_ATR_TLP_TYPE(type)		(((type) << 16) & GENMASK(18, 16))
#define PCIE_ATR_TLP_TYPE_MEM		PCIE_ATR_TLP_TYPE(0)
#define PCIE_ATR_TLP_TYPE_IO		PCIE_ATR_TLP_TYPE(2)

/* Airoha NP SCU */
#define CR_NP_SCU_PCIC			(0x088)
#define CR_NP_SCU_RSTCTRL2		(0x830)
#define CR_NP_SCU_RSTCTRL1		(0x834)

struct mtk_pcie_port;

/**
 * struct mtk_pcie_pdata - differentiate between host generations
 * @power_up: pcie power_up callback
 */
struct mtk_pcie_pdata {
	int (*power_up)(struct mtk_pcie_port *port);
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @dev: pointer to PCIe device
 * @base: IO mapped register base
 * @phy: PHY controller block
 * @irq: PCIe controller interrupt number
 * @soc: pointer to SoC-dependent operations
 */
struct mtk_pcie_port {
	struct device *dev;
	void __iomem *base;
	void __iomem *scu_base;
	struct phy *phy;

	struct gpio_desc *wifi_reset;
	u32 wifi_reset_delay_ms;

	int id;
	int irq;
	u32 busnr;

	const struct mtk_pcie_pdata *soc;
};

/* LTSSM state in PCIE_LTSSM_STATUS_REG bit[28:24] */
static const char *const ltssm_str[] = {
	"detect.quiet",			/* 0x00 */
	"detect.active",		/* 0x01 */
	"polling.active",		/* 0x02 */
	"polling.compliance",		/* 0x03 */
	"polling.configuration",	/* 0x04 */
	"config.linkwidthstart",	/* 0x05 */
	"config.linkwidthaccept",	/* 0x06 */
	"config.lanenumwait",		/* 0x07 */
	"config.lanenumaccept",		/* 0x08 */
	"config.complete",		/* 0x09 */
	"config.idle",			/* 0x0A */
	"recovery.receiverlock",	/* 0x0B */
	"recovery.equalization",	/* 0x0C */
	"recovery.speed",		/* 0x0D */
	"recovery.receiverconfig",	/* 0x0E */
	"recovery.idle",		/* 0x0F */
	"L0",				/* 0x10 */
	"L0s",				/* 0x11 */
	"L1.entry",			/* 0x12 */
	"L1.idle",			/* 0x13 */
	"L2.idle",			/* 0x14 */
	"L2.transmitwake",		/* 0x15 */
	"disable",			/* 0x16 */
	"loopback.entry",		/* 0x17 */
	"loopback.active",		/* 0x18 */
	"loopback.exit",		/* 0x19 */
	"hotreset",			/* 0x1A */
};

/**
 * mtk_pcie_config_tlp_header() - Configure a configuration TLP header
 * @bus: PCI bus to query
 * @devfn: device/function number
 * @where: offset in config space
 * @size: data size in TLP header
 *
 * Set byte enable field and device information in configuration TLP header.
 */
static void mtk_pcie_config_tlp_header(struct pci_bus *bus, unsigned int devfn,
					int where, int size)
{
	struct mtk_pcie_port *port = bus->sysdata;
	int bytes;
	u32 val;

	bytes = (GENMASK(size - 1, 0) & 0xf) << (where & 0x3);

	val = PCIE_CFG_FORCE_BYTE_EN | PCIE_CFG_BYTE_EN(bytes) |
	      PCIE_CFG_HEADER(bus->number, devfn);

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
}

static void __iomem *mtk_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				      int where)
{
	struct mtk_pcie_port *port = bus->sysdata;

	return port->base + PCIE_CFG_OFFSET_ADDR + where;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	return pci_generic_config_read32(bus, devfn, where, size, val);
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	mtk_pcie_config_tlp_header(bus, devfn, where, size);

	if (size <= 2)
		val <<= (where & 0x3) * 8;

	return pci_generic_config_write32(bus, devfn, where, 4, val);
}

static struct pci_ops mtk_pcie_ops = {
	.map_bus = mtk_pcie_map_bus,
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static int mtk_pcie_set_trans_table(struct mtk_pcie_port *port,
				    resource_size_t cpu_addr,
				    resource_size_t pci_addr,
				    resource_size_t size,
				    unsigned long type, int num)
{
	void __iomem *table;
	u32 val;

	if (num >= PCIE_MAX_TRANS_TABLES) {
		dev_err(port->dev, "not enough translate table for addr: %#llx, limited to [%d]\n",
			(unsigned long long)cpu_addr, PCIE_MAX_TRANS_TABLES);
		return -ENODEV;
	}

	table = port->base + PCIE_TRANS_TABLE_BASE_REG +
		num * PCIE_ATR_TLB_SET_OFFSET;

	writel_relaxed(lower_32_bits(cpu_addr) | PCIE_ATR_SIZE(fls(size) - 1),
		       table);
	writel_relaxed(upper_32_bits(cpu_addr),
		       table + PCIE_ATR_SRC_ADDR_MSB_OFFSET);
	writel_relaxed(lower_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_LSB_OFFSET);
	writel_relaxed(upper_32_bits(pci_addr),
		       table + PCIE_ATR_TRSL_ADDR_MSB_OFFSET);

	if (type == IORESOURCE_IO)
		val = PCIE_ATR_TYPE_IO | PCIE_ATR_TLP_TYPE_IO;
	else
		val = PCIE_ATR_TYPE_MEM | PCIE_ATR_TLP_TYPE_MEM;

	writel_relaxed(val, table + PCIE_ATR_TRSL_PARAM_OFFSET);

	return 0;
}

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct resource_entry *entry;
	unsigned int table_index = 0;
	int err;
	u32 val;

	/* Set as RC mode */
	val = readl_relaxed(port->base + PCIE_SETTING_REG);
	val |= PCIE_RC_MODE;
	writel_relaxed(val, port->base + PCIE_SETTING_REG);

	/* Set class code */
	val = readl_relaxed(port->base + PCIE_PCI_IDS_1);
	val &= ~GENMASK(31, 8);
	val |= PCI_CLASS(PCI_CLASS_BRIDGE_PCI << 8);
	writel_relaxed(val, port->base + PCIE_PCI_IDS_1);

	/* Unmask all INTx interrupts */
	val = readl_relaxed(port->base + PCIE_INT_ENABLE_REG);
	val |= PCIE_INTX_ENABLE;
	writel_relaxed(val, port->base + PCIE_INT_ENABLE_REG);

	/* INTx deassert TLP enable (0x1ac bit3=1) */
	val = readl_relaxed(port->base + PCIE_ISTATUS_LOCAL_CTRL);
	val |= PCIE_ISTATUS_LOCAL_INTX_HWCLR;
	writel_relaxed(val, port->base + PCIE_ISTATUS_LOCAL_CTRL);

	if (port->wifi_reset) {
		gpiod_set_value_cansleep(port->wifi_reset, 0);
		msleep(port->wifi_reset_delay_ms);
		gpiod_set_value_cansleep(port->wifi_reset, 1);
	}

	/* Check if the link is up or not */
	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_REG, val,
				 !!(val & PCIE_PORT_LINKUP), 20,
				 PCI_PM_D3COLD_WAIT * USEC_PER_MSEC);
	if (err) {
		const char *ltssm_state;
		int ltssm_index;

		val = readl_relaxed(port->base + PCIE_LTSSM_STATUS_REG);
		ltssm_index = PCIE_LTSSM_STATE(val);
		ltssm_state = ltssm_index >= ARRAY_SIZE(ltssm_str) ?
			      "Unknown state" : ltssm_str[ltssm_index];
		dev_err(port->dev,
			"PCIe link down, current LTSSM state: %s (%#x)\n",
			ltssm_state, val);
		return err;
	}

	/* Set PCIe translation windows */
	resource_list_for_each_entry(entry, &host->windows) {
		struct resource *res = entry->res;
		unsigned long type = resource_type(res);
		resource_size_t cpu_addr;
		resource_size_t pci_addr;
		resource_size_t size;
		const char *range_type;

		if (type == IORESOURCE_IO) {
			cpu_addr = pci_pio_to_address(res->start);
			range_type = "IO";
		} else if (type == IORESOURCE_MEM) {
			cpu_addr = res->start;
			range_type = "MEM";
		} else {
			continue;
		}

		pci_addr = res->start - entry->offset;
		size = resource_size(res);
		err = mtk_pcie_set_trans_table(port, cpu_addr, pci_addr, size,
					       type, table_index);
		if (err)
			return err;

		dev_dbg(port->dev, "set %s trans window[%d]: cpu_addr = %#llx, pci_addr = %#llx, size = %#llx\n",
			range_type, table_index, (unsigned long long)cpu_addr,
			(unsigned long long)pci_addr, (unsigned long long)size);

		table_index++;
	}

	return 0;
}

static int mtk_pcie_setup_irq(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct platform_device *pdev = to_platform_device(dev);

	port->irq = platform_get_irq(pdev, 0);
	if (port->irq < 0)
		return port->irq;

	return 0;
}

static int mtk_pcie_parse_port(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct platform_device *pdev = to_platform_device(dev);
	struct list_head *windows = &host->windows;
	struct resource *regs, *bus;
	struct device_node *ns;
	enum of_gpio_flags flags;
	enum gpiod_flags wifi_reset_init_flags;
	int ret;

	ret = pci_parse_request_of_pci_ranges(dev, windows, &bus);
	if (ret) {
		dev_err(dev, "failed to parse pci ranges\n");
		return ret;
	}

	port->busnr = bus->start;

	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-mac");
	port->base = devm_ioremap_resource(dev, regs);
	if (IS_ERR(port->base)) {
		dev_err(dev, "failed to map register base\n");
		return PTR_ERR(port->base);
	}

	port->id = of_get_pci_domain_nr(dev->of_node);

	port->phy = devm_phy_optional_get(dev, "pcie-phy");
	if (IS_ERR(port->phy)) {
		ret = PTR_ERR(port->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get PHY\n");

		return ret;
	}

	/* traverse to SCU node */
	ns = of_parse_phandle(dev->of_node, "airoha,scu", 0);
	if (ns) {
		struct resource scu_res;

		ret = of_address_to_resource(ns, 0, &scu_res);
		if (ret == 0)
			port->scu_base = devm_ioremap(dev, scu_res.start,
						      resource_size(&scu_res));

		of_node_put(ns);
	}

	ret = of_get_named_gpio_flags(dev->of_node, "wifi-reset-gpios", 0,
				      &flags);
	if (ret >= 0) {
		if (flags & OF_GPIO_ACTIVE_LOW)
			wifi_reset_init_flags = GPIOD_OUT_HIGH;
		else
			wifi_reset_init_flags = GPIOD_OUT_LOW;
		port->wifi_reset = devm_gpiod_get_optional(dev, "wifi-reset",
							   wifi_reset_init_flags);
		if (IS_ERR(port->wifi_reset)) {
			ret = PTR_ERR(port->wifi_reset);
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
					"failed to request WIFI reset gpio\n");
			port->wifi_reset = NULL;
			return ret;
		}
		of_property_read_u32(dev->of_node, "wifi-reset-msleep",
				     &port->wifi_reset_delay_ms);
	} else if (ret == -EPROBE_DEFER) {
		return ret;
	}

	return 0;
}

static bool scu_an7581_reset_pcie(struct mtk_pcie_port *port, bool assert)
{
	u32 val, scu_ofs, mask;

	if (!port->scu_base)
		return false;

	switch (port->id) {
	case 0:
		/* reset PCIe0 & PCIe1 IP */
		mask = BIT(27) | BIT(26);
		scu_ofs = CR_NP_SCU_RSTCTRL1;
		break;
	case 2:
		/* reset PCIe2 IP */
		mask = BIT(27);
		scu_ofs = CR_NP_SCU_RSTCTRL2;
		break;
	default:
		return false;
	}

	val = readl(port->scu_base + scu_ofs);
	if (assert)
		val |=  mask;
	else
		val &= ~mask;
	writel(val, port->scu_base + scu_ofs);

	return true;
}

static bool scu_an7581_reset_perst(struct mtk_pcie_port *port, bool assert)
{
	u32 val, mask;

	if (!port->scu_base)
		return false;

	switch (port->id) {
	case 0:
		/* reset PCIe0 & PCIe1 PERST */
		mask = BIT(29) | BIT(26);
		break;
	case 2:
		/* reset PCIe2 PERST */
		mask = BIT(16);
		break;
	default:
		return false;
	}

	val = readl(port->scu_base + CR_NP_SCU_PCIC);
	if (assert)
		val &= ~mask;
	else
		val |=  mask;
	writel(val, port->scu_base + CR_NP_SCU_PCIC);

	return true;
}

static inline void mac_an7581_p0_p1_init(struct mtk_pcie_port *port)
{
	void __iomem *port1_base;
	u32 val;

	/* remap P1 regs */
	port1_base = ioremap(PCIE_AN7581_P1_BASE, 0x400);
	if (port1_base == NULL)
		return;

	if (isAN7581CT || isAN7566PT || isAN7581IT || isAN7581FP) {
		/* disable P0 Gen3 */
		val = readl_relaxed(port->base + PCIE_SETTING_REG);
		val &= ~PCIE_G3_SUPPORTED;
		writel_relaxed(val, port->base + PCIE_SETTING_REG);
	}

	if (isAN7581CT) {
		/* disable P1 Gen3 */
		val = readl_relaxed(port1_base + PCIE_SETTING_REG);
		val &= ~PCIE_G3_SUPPORTED;
		writel_relaxed(val, port1_base + PCIE_SETTING_REG);
	}

	/* tune eq preset 1 */
	val = FIELD_PREP(PCIE_VAL_LN0_DOWNSTREAM, 0x47) |
	      FIELD_PREP(PCIE_VAL_LN0_UPSTREAM, 0x41) |
	      FIELD_PREP(PCIE_VAL_LN1_DOWNSTREAM, 0x47) |
	      FIELD_PREP(PCIE_VAL_LN1_UPSTREAM, 0x41);
	writel_relaxed(val, port->base + PCIE_EQ_PRESET_01_REG);
	writel_relaxed(val, port1_base + PCIE_EQ_PRESET_01_REG);

	val = PCIE_K_PHYPARAM_QUERY | PCIE_K_QUERY_TIMEOUT |
	      FIELD_PREP(PCIE_K_PRESET_TO_USE_16G, 0x80) |
	      FIELD_PREP(PCIE_K_PRESET_TO_USE, 0x2) |
	      FIELD_PREP(PCIE_K_FINETUNE_MAX, 0xf);
	writel_relaxed(val, port->base + PCIE_PIPE4_PIE8_REG);
	writel_relaxed(val, port1_base + PCIE_PIPE4_PIE8_REG);

	iounmap(port1_base);
}

static int mtk_pcie_en7581_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err;

	/* assert reset PCIe IP */
	if (scu_an7581_reset_pcie(port, true))
		msleep(100);

	/* assert reset PCIe peripheral */
	if (scu_an7581_reset_perst(port, true))
		msleep(1);

	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize PHY\n");
		return err;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on PHY\n");
		goto err_phy_on;
	}

	/* de-assert reset PCIe IP */
	scu_an7581_reset_pcie(port, false);

	/* configure both ports at-once */
	if (port->id == 0) {
		mac_an7581_p0_p1_init(port);
		msleep(10);
	}

	/* de-assert reset PCIe peripheral */
	if (scu_an7581_reset_perst(port, false))
		msleep(100);

	return 0;

err_phy_on:
	phy_exit(port->phy);

	return err;
}

static bool scu_an7583_reset_pcie(struct mtk_pcie_port *port, bool hb, bool assert)
{
	u32 val, mask;

	if (!port->scu_base)
		return false;

	if (hb)
		mask = BIT(29);	/* reset PCIe0 HB */
	else
		mask = BIT(26);	/* reset PCIe0 IP */

	val = readl(port->scu_base + CR_NP_SCU_RSTCTRL1);
	if (assert)
		val |=  mask;
	else
		val &= ~mask;
	writel(val, port->scu_base + CR_NP_SCU_RSTCTRL1);

	return true;
}

static bool scu_an7583_reset_perst(struct mtk_pcie_port *port, bool assert)
{
	u32 val, mask;

	if (!port->scu_base)
		return false;

	/* reset PCIe0 PERST */
	mask = BIT(29);

	val = readl(port->scu_base + CR_NP_SCU_PCIC);
	if (assert)
		val &= ~mask;
	else
		val |=  mask;
	writel(val, port->scu_base + CR_NP_SCU_PCIC);

	return true;
}

static inline void mac_an7583_p0_init(struct mtk_pcie_port *port)
{
	u32 val;

	/* skip eq phase 2/3 */
	val = readl_relaxed(port->base + PCIE_PHY_MAC_CFG_REG);
	val &= ~PCIE_EQU_PHASES_23;
	writel_relaxed(val, port->base + PCIE_PHY_MAC_CFG_REG);

	/* tune eq preset 1 */
	val = FIELD_PREP(PCIE_VAL_LN0_DOWNSTREAM, 0x32) |
	      FIELD_PREP(PCIE_VAL_LN0_UPSTREAM, 0x00) |
	      FIELD_PREP(PCIE_VAL_LN1_DOWNSTREAM, 0x50) |
	      FIELD_PREP(PCIE_VAL_LN1_UPSTREAM, 0x50);
	writel_relaxed(val, port->base + PCIE_EQ_PRESET_01_REG);

	val = PCIE_K_PHYPARAM_QUERY | PCIE_K_QUERY_TIMEOUT |
	      FIELD_PREP(PCIE_K_PRESET_TO_USE_16G, 0x80) |
	      FIELD_PREP(PCIE_K_PRESET_TO_USE, 0x2) |
	      FIELD_PREP(PCIE_K_FINETUNE_MAX, 0xf);
	writel_relaxed(val, port->base + PCIE_PIPE4_PIE8_REG);
}

static int mtk_pcie_en7583_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err;

	/* reset PCIe0 HB */
	if (scu_an7583_reset_pcie(port, true, true)) {
		msleep(100);
		scu_an7583_reset_pcie(port, true, false);
		msleep(100);
	}

	/* assert reset PCIe0 IP */
	if (scu_an7583_reset_pcie(port, false, true))
		msleep(100);

	/* assert reset PCIe0 peripheral */
	if (scu_an7583_reset_perst(port, true))
		msleep(1);

	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize PHY\n");
		return err;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on PHY\n");
		goto err_phy_on;
	}

	/* de-assert reset PCIe0 IP */
	scu_an7583_reset_pcie(port, false, false);

	mac_an7583_p0_init(port);
	msleep(10);

	/* de-assert reset PCIe0 peripheral */
	if (scu_an7583_reset_perst(port, false))
		msleep(100);

	return 0;

err_phy_on:
	phy_exit(port->phy);

	return err;
}

static void mtk_pcie_power_down(struct mtk_pcie_port *port)
{
	phy_power_off(port->phy);
	phy_exit(port->phy);
}

static int mtk_pcie_setup(struct mtk_pcie_port *port)
{
	int err;

	err = mtk_pcie_parse_port(port);
	if (err)
		return err;

	/* Don't touch the hardware registers before power up */
	err = port->soc->power_up(port);
	if (err)
		return err;

	/* Try link up */
	err = mtk_pcie_startup_port(port);
	if (err)
		return err;

	err = mtk_pcie_setup_irq(port);
	if (err)
		return err;

	return 0;
}

/* PCI interrupt mapping */
static int mtk_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct mtk_pcie_port *port = dev->bus->sysdata;

	return port->irq;
}

static void pcr_write_config_word_extend(struct mtk_pcie_port *port, u8 bus,
					  u32 reg, u32 value)
{
	u32 val = (bus == 0) ? 0x1f0000 : 0x1f0100;

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
	writel(value, port->base + PCIE_CFG_OFFSET_ADDR + reg);
}

static u32 pcr_read_config_word_extend(struct mtk_pcie_port *port, u8 bus,
					u32 reg)
{
	u32 val = (bus == 0) ? 0x1f0000 : 0x1f0100;

	writel_relaxed(val, port->base + PCIE_CFGNUM_REG);
	return readl(port->base + PCIE_CFG_OFFSET_ADDR + reg);
}

static void pcr_update_irq_line(struct mtk_pcie_port *port)
{
	u32 value;

	value = pcr_read_config_word_extend(port, 0, PCI_INTERRUPT_LINE);
	value &= ~0xff;
	value |=  port->irq;
	pcr_write_config_word_extend(port, 0, PCI_INTERRUPT_LINE, value);
	pcr_write_config_word_extend(port, 1, PCI_INTERRUPT_LINE, value);
}

static int mtk_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pcie_port *port;
	struct pci_host_bridge *host;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*port));
	if (!host)
		return -ENOMEM;

	port = pci_host_bridge_priv(host);

	port->dev = dev;
	port->soc = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, port);

	err = mtk_pcie_setup(port);
	if (err) {
		/* work around for accidental calltrace when NPU access port1
		 * mac register but only port0 linkup
		 */
		if (port->id == 2) {
			mtk_pcie_power_down(port);
			return err;
		}
	}

	host->busnr = port->busnr;
	host->dev.parent = port->dev;
	host->ops = &mtk_pcie_ops;
	host->map_irq = mtk_pcie_map_irq;
	host->swizzle_irq = pci_common_swizzle;
	host->sysdata = port;

	err = pci_host_probe(host);

	pcr_update_irq_line(port);

	if (err < 0) {
		mtk_pcie_power_down(port);
		return err;
	}

	return 0;
}

static const struct mtk_pcie_pdata pcie_soc_an7581 = {
	.power_up = mtk_pcie_en7581_power_up,
};

static const struct mtk_pcie_pdata pcie_soc_an7583 = {
	.power_up = mtk_pcie_en7583_power_up,
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "airoha,an7581-pcie", .data = &pcie_soc_an7581 },
	{ .compatible = "airoha,an7583-pcie", .data = &pcie_soc_an7583 },
	{},
};

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.driver = {
		.name = "airoha-pcie-gen3",
		.of_match_table = mtk_pcie_of_match,
	},
};

builtin_platform_driver(mtk_pcie_driver);
MODULE_LICENSE("GPL v2");
