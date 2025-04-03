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

#include <soc/airoha/pkgid.h>

/* Airoha NP SCU */
#define CR_NP_SCU_PCIC		(0x088)
#define CR_NP_SCU_RSTCTRL1	(0x834)

/* PCIe V2 per-port registers */
#define PCIE_K_GBL_1		0x000
#define PCIE_K_CONF_FUNC0_0	0x100
#define PCIE_K_CONF_FUNC0_1	0x104
#define PCIE_K_CONF_FUNC0_7	0x11c
#define   MSI_SUPP		BIT(5)
#define PCIE_INT_MASK		0x420
#define   MSI_MASK		BIT(23)
#define   INTX_MASK		GENMASK(19, 16)
#define PCIE_IMSI_ADDR		0x430
#define PCIE_AHB_TRANS_BASE0_L	0x438
#define PCIE_AHB_TRANS_BASE0_H	0x43c
#define   AHB2PCIE_SIZE(x)	((x) & GENMASK(4, 0))
#define PCIE_AXI_WINDOW0	0x448
#define   WIN_ENABLE		BIT(7)
#define PCIE_MMIO_CTRL		0x564
#define   MMIO_WR_LOCK_EN	BIT(0)
#define PCIE_LINK_STATUS_V2	0x804
#define   PORT_LINKUP_V2	BIT(10)

/* PCIe V2 configuration transaction header */
#define PCIE_AHB2PCIE_BASE0_L	0x438
#define PCIE_PCIE2AXI_WIN0	0x448
#define PCIE_CFG_HEADER0	0x460
#define PCIE_CFG_HEADER1	0x464
#define PCIE_CFG_HEADER2	0x468
#define PCIE_CFG_WDATA		0x470
#define PCIE_APP_TLP_REQ	0x488
#define PCIE_CFG_RDATA		0x48c
#define APP_CFG_REQ		BIT(0)
#define APP_CPL_STATUS		GENMASK(7, 5)

#define CFG_WRRD_TYPE_0		4
#define CFG_WRRD_TYPE_1		5
#define CFG_WR_FMT		2
#define CFG_RD_FMT		0

#define CFG_DW0_LENGTH(length)	((length) & GENMASK(9, 0))
#define CFG_DW0_TYPE(type)	(((type) << 24) & GENMASK(28, 24))
#define CFG_DW0_FMT(fmt)	(((fmt) << 29) & GENMASK(31, 29))
#define CFG_DW2_REGN(regn)	((regn) & GENMASK(11, 2))
#define CFG_DW2_FUN(fun)	(((fun) << 16) & GENMASK(18, 16))
#define CFG_DW2_DEV(dev)	(((dev) << 19) & GENMASK(23, 19))
#define CFG_DW2_BUS(bus)	(((bus) << 24) & GENMASK(31, 24))
#define CFG_HEADER_DW0(type, fmt) \
	(CFG_DW0_LENGTH(1) | CFG_DW0_TYPE(type) | CFG_DW0_FMT(fmt))
#define CFG_HEADER_DW1(where, size) \
	(GENMASK(((size) - 1), 0) << ((where) & 0x3))
#define CFG_HEADER_DW2(regn, fun, dev, bus) \
	(CFG_DW2_REGN(regn) | CFG_DW2_FUN(fun) | \
	CFG_DW2_DEV(dev) | CFG_DW2_BUS(bus))

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

	int rc_link;

	const struct mtk_pcie_pdata *soc;
};

static int mtk_pcie_check_cfg_cpld(struct mtk_pcie_port *port)
{
	u32 val;
	int err;

	err = readl_poll_timeout_atomic(port->base + PCIE_APP_TLP_REQ, val,
					!(val & APP_CFG_REQ), 10,
					100 * USEC_PER_MSEC);
	if (err)
		return PCIBIOS_SET_FAILED;

	if (readl_relaxed(port->base + PCIE_APP_TLP_REQ) & APP_CPL_STATUS)
		return PCIBIOS_SET_FAILED;

	return PCIBIOS_SUCCESSFUL;
}

static int mtk_pcie_hw_rd_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
			      int where, int size, u32 *val)
{
	u32 tmp;

	/* Write PCIe configuration transaction header for Cfgrd */
	writel_relaxed(CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_RD_FMT),
		port->base + PCIE_CFG_HEADER0);
	writel_relaxed(CFG_HEADER_DW1(where, size),
		port->base + PCIE_CFG_HEADER1);
	writel_relaxed(CFG_HEADER_DW2(where, PCI_FUNC(devfn), PCI_SLOT(devfn), bus),
		port->base + PCIE_CFG_HEADER2);

	/* Trigger h/w to transmit Cfgrd TLP */
	tmp = readl_relaxed(port->base + PCIE_APP_TLP_REQ);
	tmp |= APP_CFG_REQ;
	writel_relaxed(tmp, port->base + PCIE_APP_TLP_REQ);

	/* Check completion status */
	if (mtk_pcie_check_cfg_cpld(port))
		return PCIBIOS_SET_FAILED;

	/* Read cpld payload of Cfgrd */
	*val = readl_relaxed(port->base + PCIE_CFG_RDATA);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int mtk_pcie_hw_wr_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
			      int where, int size, u32 val)
{
	/* Write PCIe configuration transaction header for Cfgwr */
	writel_relaxed(CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_WR_FMT),
		port->base + PCIE_CFG_HEADER0);
	writel_relaxed(CFG_HEADER_DW1(where, size),
		port->base + PCIE_CFG_HEADER1);
	writel_relaxed(CFG_HEADER_DW2(where, PCI_FUNC(devfn), PCI_SLOT(devfn), bus),
		port->base + PCIE_CFG_HEADER2);

	/* Write Cfgwr data */
	val = val << 8 * (where & 3);
	writel_relaxed(val, port->base + PCIE_CFG_WDATA);

	/* Trigger h/w to transmit Cfgwr TLP */
	val = readl_relaxed(port->base + PCIE_APP_TLP_REQ);
	val |= APP_CFG_REQ;
	writel_relaxed(val, port->base + PCIE_APP_TLP_REQ);

	/* Check completion status */
	return mtk_pcie_check_cfg_cpld(port);
}

static inline struct mtk_pcie_port *mtk_pcie_find_port(struct pci_bus *bus)
{
	return (struct mtk_pcie_port *)bus->sysdata;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct mtk_pcie_port *port;
	u32 bn = bus->number;
	int ret;

	if (PCI_SLOT(devfn) > 0)
		return 0;

	port = mtk_pcie_find_port(bus);
	if (!port) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	ret = mtk_pcie_hw_rd_cfg(port, bn, devfn, where, size, val);
	if (ret)
		*val = ~0;

	return ret;
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct mtk_pcie_port *port;
	u32 bn = bus->number;

	port = mtk_pcie_find_port(bus);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return mtk_pcie_hw_wr_cfg(port, bn, devfn, where, size, val);
}

static struct pci_ops mtk_pcie_ops = {
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(port);
	struct resource_entry *entry;
	unsigned int table_index = 0;
	int err;
	u32 val;

	/* Set RC mode */
	writel_relaxed(0x00804201, port->base + PCIE_K_GBL_1);

	/* Set class code */
	writel_relaxed(0x06040001, port->base + PCIE_K_CONF_FUNC0_1);

	/* Disable MSI interrupt */
	val = readl_relaxed(port->base + PCIE_K_CONF_FUNC0_7);
	val &= ~MSI_SUPP;
	writel_relaxed(val, port->base + PCIE_K_CONF_FUNC0_7);

	/* Set INTx mask */
	val = readl_relaxed(port->base + PCIE_INT_MASK);
	val &= ~INTX_MASK;
	writel_relaxed(val, port->base + PCIE_INT_MASK);

	if (port->wifi_reset) {
		gpiod_set_value_cansleep(port->wifi_reset, 0);
		msleep(port->wifi_reset_delay_ms);
		gpiod_set_value_cansleep(port->wifi_reset, 1);
	}

	/* Check if the link is up or not */
	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_V2, val,
				 !!(val & PORT_LINKUP_V2), 20,
				 PCI_PM_D3COLD_WAIT * USEC_PER_MSEC);
	if (err) {
		port->rc_link = 0;
		dev_err(port->dev, "PCIe RC%d link down\n", port->id);
		return err;
	}

	port->rc_link = 1;

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

		/* Set AHB to PCIe translation windows */
		pci_addr = res->start - entry->offset;
		size = resource_size(res);
		val = lower_32_bits(cpu_addr) | AHB2PCIE_SIZE(fls(size));
		writel_relaxed(val, port->base + PCIE_AHB_TRANS_BASE0_L);
		val = upper_32_bits(cpu_addr);
		writel_relaxed(val, port->base + PCIE_AHB_TRANS_BASE0_H);

		dev_dbg(port->dev, "set %s trans window[%d]: cpu_addr = %#llx, pci_addr = %#llx, size = %#llx\n",
			range_type, table_index, (unsigned long long)cpu_addr,
			(unsigned long long)pci_addr, (unsigned long long)size);

		table_index++;
	}

	/* Set PCIe to AXI translation memory space */
	val = WIN_ENABLE;
	writel_relaxed(val, port->base + PCIE_AXI_WINDOW0);

	/* Kite coherence bugfix */
	val = readl_relaxed(port->base + PCIE_MMIO_CTRL);
	val |= MMIO_WR_LOCK_EN;
	writel_relaxed(val, port->base + PCIE_MMIO_CTRL);

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

static void scu_an7552_reset_perst(struct mtk_pcie_port *port, bool assert)
{
	u32 val, mask;

	/* reset PCIe0 & PCIe1 PERST */
	mask = BIT(29) | BIT(26);

	val = readl(port->scu_base + CR_NP_SCU_PCIC);
	if (assert)
		val &= ~mask;
	else
		val |=  mask;
	writel(val, port->scu_base + CR_NP_SCU_PCIC);
}

static void scu_an7552_reset_pcie(struct mtk_pcie_port *port, bool assert)
{
	u32 val, mask;

	/* reset RC0 & RC1 & HB */
	mask = BIT(29) | BIT(27) | BIT(26);

	val = readl(port->scu_base + CR_NP_SCU_RSTCTRL1);
	if (assert)
		val |=  mask;
	else
		val &= ~mask;
	writel(val, port->scu_base + CR_NP_SCU_RSTCTRL1);
}

static inline void scu_an7552_enable_clk(struct mtk_pcie_port *port)
{
	u32 val;

	/* pcie_ref_clk_enable1 */
	val = readl(port->scu_base + CR_NP_SCU_PCIC);
	val |= BIT(22);
	writel(val, port->scu_base + CR_NP_SCU_PCIC);
}

static int mtk_pcie_en7552_power_up(struct mtk_pcie_port *port)
{
	struct device *dev = port->dev;
	int err;

	/* power up both ports at port0 init */
	if (port->id != 0)
		return 0;

	if (!port->scu_base)
		return -EFAULT;

	/* assert reset PCIe peripheral */
	scu_an7552_reset_perst(port, true);
	msleep(1);

	/* enable PCIe RC1 reference clock */
	scu_an7552_enable_clk(port);
	msleep(1);

	/* de-assert reset PCIe RC0/RC1/HB */
	scu_an7552_reset_pcie(port, false);
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

	/* assert reset PCIe RC0/RC1/HB */
	scu_an7552_reset_pcie(port, true);
	msleep(100);

	/* de-assert reset PCIe RC0/RC1/HB */
	scu_an7552_reset_pcie(port, false);
	msleep(10);

	/* de-assert reset PCIe peripheral */
	scu_an7552_reset_perst(port, false);
	msleep(150);

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
					 u8 dev, u8 fun, u32 reg, u32 value)
{
	u32 tmp;

	/* bus/dev remap */
	if (bus == 0)
		dev = 0;
	else
		bus = 1;

	tmp = CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_WR_FMT);
	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER0);

	tmp = ((u32)port->id << 19) | 0x070f;
	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER1);

	tmp = CFG_HEADER_DW2(reg, fun, dev, bus);
	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER2);

	writel_relaxed(value, port->base + PCIE_CFG_WDATA);

	tmp = APP_CFG_REQ;
	writel_relaxed(tmp, port->base + PCIE_APP_TLP_REQ);

	udelay(10);

	/* check completion status */
	mtk_pcie_check_cfg_cpld(port);
}

static u32 pcr_read_config_word_extend(struct mtk_pcie_port *port, u8 bus,
				       u8 dev, u8 fun, u32 reg)
{
	u32 tmp;

	/* bus/dev remap */
	if (bus == 0)
		dev = 0;
	else
		bus = 1;

	if (bus == 0 && dev == 0)
		tmp = CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_RD_FMT);
	else
		tmp = CFG_HEADER_DW0(CFG_WRRD_TYPE_1, CFG_RD_FMT);

	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER0);

	tmp = ((u32)port->id << 19) | 0x070f;
	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER1);

	tmp = CFG_HEADER_DW2(reg, fun, dev, bus);
	writel_relaxed(tmp, port->base + PCIE_CFG_HEADER2);

	tmp = APP_CFG_REQ;
	writel_relaxed(tmp, port->base + PCIE_APP_TLP_REQ);

	udelay(10);

	/* check completion status */
	if (mtk_pcie_check_cfg_cpld(port))
		return ~0;

	return readl_relaxed(port->base + PCIE_CFG_RDATA);
}

static u32 pcr_get_pos(struct mtk_pcie_port *port, u8 bus, u8 dev)
{
	u32 val, pos;

	val = pcr_read_config_word_extend(port, bus, dev, 0, 0x34);
	pos = val & 0xff;

	while (pos && pos != 0xff) {
		val = pcr_read_config_word_extend(port, bus, dev, 0, pos);
		if ((val & 0xff) == 0x10)
			return pos;

		pos = (val >> 0x08) & 0xff;
	}

	return 0;
}

static inline void pcr_fixup_rc(struct mtk_pcie_port *port)
{
	u32 dev = port->id;
	u32 val, tmp, i;

	val = pcr_read_config_word_extend(port, 0, dev, 0, 0x20);
	tmp = (val & 0x0000ffff) << 16;
	val = (val & 0xffff0000) + 0x100000;
	val -= tmp;

	i = 0;
	while (i < 32) {
		if ((1u << i) >= val)
			break;
		i++;
	}

	/* config RC to EP addr window */
	tmp |= i;
	writel_relaxed(tmp, port->base + PCIE_AHB2PCIE_BASE0_L);

	msleep(1);

	/* enable EP to RC access */
	writel_relaxed(0x80, port->base + PCIE_PCIE2AXI_WIN0);
}

static inline void pcr_retrain_rc(struct mtk_pcie_port *port)
{
	u32 bus = port->id + 1;
	u32 dev = port->id;
	u32 ppos, epos, plink_cap, elink_cap, plink_sta[2];

	ppos = pcr_get_pos(port, 0, dev);
	epos = pcr_get_pos(port, bus, 0);

	if (epos < 0x40 || ppos < 0x40)
		return;

	plink_cap = pcr_read_config_word_extend(port, 0, dev, 0, ppos + 0x0c);
	elink_cap = pcr_read_config_word_extend(port, bus, 0, 0, epos + 0x0c);

	if ((elink_cap & 0x0f) == 1 || (plink_cap & 0x0f) == 1)
		return;

	plink_sta[0] = pcr_read_config_word_extend(port, 0, dev, 0, ppos + 0x10);
	if (((plink_sta[0] >> 16) & 0x0f) == (plink_cap & 0x0f))
		return;

	/* train link Gen1 -> Gen2 */
	plink_sta[0] |= 0x20;
	pcr_write_config_word_extend(port, 0, dev, 0, ppos + 0x10, plink_sta[0]);

	msleep(250);

	plink_sta[1] = pcr_read_config_word_extend(port, 0, dev, 0, ppos + 0x10);

	dev_info(port->dev, "PCIe RC%d link trained: %x -> %x\n", port->id,
		 (plink_sta[0] >> 16) & 0x0f, (plink_sta[1] >> 16) & 0x0f);
}

static void pcr_update_irq_line(struct mtk_pcie_port *port)
{
	u32 bus = port->id + 1;
	u32 dev = port->id;
	u32 val;

	val = pcr_read_config_word_extend(port, 0, dev, 0, PCI_INTERRUPT_LINE);
	val &= ~0xff;
	val |=  port->irq;
	pcr_write_config_word_extend(port, 0, dev, 0, PCI_INTERRUPT_LINE, val);
	pcr_write_config_word_extend(port, bus, 0, 0, PCI_INTERRUPT_LINE, val);
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
	}

	host->busnr = port->busnr;
	host->dev.parent = port->dev;
	host->ops = &mtk_pcie_ops;
	host->map_irq = mtk_pcie_map_irq;
	host->swizzle_irq = pci_common_swizzle;
	host->sysdata = port;

	err = pci_host_probe(host);

	if (port->rc_link) {
		pcr_fixup_rc(port);
		pcr_retrain_rc(port);
	}

	pcr_update_irq_line(port);

	if (err < 0) {
		mtk_pcie_power_down(port);
		return err;
	}

	return 0;
}

static const struct mtk_pcie_pdata pcie_soc_an7552 = {
	.power_up = mtk_pcie_en7552_power_up,
};

static const struct of_device_id mtk_pcie_of_match[] = {
	{ .compatible = "airoha,an7552-pcie", .data = &pcie_soc_an7552 },
	{},
};

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.driver = {
		.name = "airoha-pcie",
		.of_match_table = mtk_pcie_of_match,
	},
};

builtin_platform_driver(mtk_pcie_driver);
MODULE_LICENSE("GPL v2");
