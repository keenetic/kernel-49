#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <soc/airoha/pkgid.h>

#define ASIC_PBUS_CLK			(0x14000000)	/* 300MHz */
#define ASIC_PBUS_PCIE0_BASE		(0x20000000)

#if defined(CONFIG_MACH_AN7552)
#define ASIC_PBUS_PCIE0_MASK		(0xffc00000)
#define ASIC_PBUS_PCIE1_BASE		(0x24000000)
#define ASIC_PBUS_PCIE1_MASK		(0xffc00000)
#elif defined(CONFIG_MACH_AN7581)
#define ASIC_PBUS_PCIE0_MASK		(0xfc000000)
#define ASIC_PBUS_PCIE1_BASE		(0x24000000)
#define ASIC_PBUS_PCIE1_MASK		(0xfc000000)
#define ASIC_PBUS_PCIE2_BASE		(0x28000000)
#define ASIC_PBUS_PCIE2_MASK		(0xfc000000)
#elif defined(CONFIG_MACH_AN7583)
#define ASIC_PBUS_PCIE0_MASK		(0xfc000000)
#define ASIC_PBUS_PCIE1_BASE		(0x24000000)
#define ASIC_PBUS_PCIE1_MASK		(0xffc00000)
#else
#error Airoha SoC not defined
#endif

/* PBUS */
#define PBUS_PCIE0_MEM_BASE		(0x00)
#define PBUS_PCIE0_MEM_MASK		(0x04)
#define PBUS_PCIE1_MEM_BASE		(0x08)
#define PBUS_PCIE1_MEM_MASK		(0x0C)
#define PBUS_PCIE2_MEM_BASE		(0x10)
#define PBUS_PCIE2_MEM_MASK		(0x14)

/* NP SCU */
#define CR_NP_SCU_PB_TO_ERR		(0x03c)
#define CR_NP_SCU_PRATIR		(0x058)
#define CR_NP_SCU_PDIDR			(0x05c)
#define CR_NP_SCU_MON_TMR		(0x060)
#define CR_NP_SCU_HIR			(0x064)
#define CR_NP_SCU_PCIC			(0x088)
#define CR_NP_SCU_SSTR			(0x09c)
#define CR_NP_SCU_SCREG_WR0		(0x280)
#define CR_NP_SCU_SCREG_WR1		(0x284)

#define PBUS_TOUT_EN			(0x40000000)

#define SCREG_WR1_PID(v)		((v) & 0xf)
#define SCREG_WR1_PID_EXT(v)		(((v) >> 3) & 0x1)
#define SCREG_WR1_SYS_HCLK(v)		(((v) >> 10) & 0x3ff)

/* CHIP SCU */
#define CR_CHIP_SCU_RGS_OPEN_DRAIN	(0x018c)
#define CR_CHIP_SCU_RGS_CLK_GSW		(0x01b4)
#define CR_CHIP_SCU_RGS_CLK_EMI		(0x01b8)
#define CR_CHIP_SCU_RGS_CLK_BUS		(0x01bc)
#define CR_CHIP_SCU_RGS_CLK_FE		(0x01c0)
#define CR_CHIP_SCU_RGS_CLK_BUS_ICG	(0x01e4)
#define CR_CHIP_SCU_RGS_CLK_NPU		(0x01fc)

struct airoha_pbus {
	struct device *dev;
	u8 __iomem *base;
	u8 __iomem *scu_base;
	int irq;
};

static u32 np_scu_pid;
static u32 np_scu_pdidr;
static u32 np_scu_sys_clk;
static u32 np_scu_l2c_sram;

u32 airoha_get_pid(void)
{
	return np_scu_pid;
}
EXPORT_SYMBOL(airoha_get_pid);

u32 airoha_get_pdidr(void)
{
	return np_scu_pdidr;
}
EXPORT_SYMBOL(airoha_get_pdidr);

u32 airoha_get_sys_clk(void)
{
	return np_scu_sys_clk;
}
EXPORT_SYMBOL(airoha_get_sys_clk);

u32 airoha_get_l2c_sram(void)
{
	return np_scu_l2c_sram;
}
EXPORT_SYMBOL(airoha_get_l2c_sram);

static inline void set_pbus_reg(struct airoha_pbus *pbus, u32 reg, u32 val)
{
	writel(val, pbus->base + reg);
}

static inline void set_scu_reg(struct airoha_pbus *pbus, u32 reg, u32 val)
{
	writel(val, pbus->scu_base + reg);
}

static inline u32 get_scu_reg(struct airoha_pbus *pbus, u32 reg)
{
	return readl(pbus->scu_base + reg);
}

static inline void get_package_info(struct airoha_pbus *pbus)
{
	u32 scr, hir;

	hir = get_scu_reg(pbus, CR_NP_SCU_HIR);
	scr = get_scu_reg(pbus, CR_NP_SCU_SCREG_WR1);

	np_scu_pid = hir | SCREG_WR1_PID(scr) | SCREG_WR1_PID_EXT(scr);
	np_scu_sys_clk = SCREG_WR1_SYS_HCLK(scr);

	np_scu_pdidr = get_scu_reg(pbus, CR_NP_SCU_PDIDR);
	np_scu_l2c_sram = get_scu_reg(pbus, CR_NP_SCU_SCREG_WR0);
}

static inline void xsi_serdes_init(struct airoha_pbus *pbus)
{
#ifdef CONFIG_PCI
#ifdef CONFIG_MACH_AN7581
	u32 reg;

	if (!(isAN7581ST || isAN7581FD || isAN7581DT)) {
		/*
		 * pcie_xsi0_sel -> PCIe P0 mode
		 * pcie_xsi1_sel -> PCIe P1 mode
		 * usb_pcie_sel  -> USB mode
		 */
		reg = get_scu_reg(pbus, CR_NP_SCU_SSTR);
		reg &= ~(0x3 << 13);
		reg &= ~(0x3 << 11);
		reg |= BIT(3);
		set_scu_reg(pbus, CR_NP_SCU_SSTR, reg);

		/* PCIe mode: 0 - 2LANE (0x2), 1 - 2x1LANE (0x3) */
		reg = get_scu_reg(pbus, CR_NP_SCU_PCIC);
		reg &= ~0x3;
		reg |=  0x3;
		set_scu_reg(pbus, CR_NP_SCU_PCIC, reg);
	}
#endif
#endif
}

static inline void pci_monitor_init(struct airoha_pbus *pbus)
{
#ifdef CONFIG_PCI
	set_pbus_reg(pbus, PBUS_PCIE0_MEM_BASE, ASIC_PBUS_PCIE0_BASE);
	set_pbus_reg(pbus, PBUS_PCIE0_MEM_MASK, ASIC_PBUS_PCIE0_MASK);

	set_pbus_reg(pbus, PBUS_PCIE1_MEM_BASE, ASIC_PBUS_PCIE1_BASE);
	set_pbus_reg(pbus, PBUS_PCIE1_MEM_MASK, ASIC_PBUS_PCIE1_MASK);

#ifdef ASIC_PBUS_PCIE2_BASE
	set_pbus_reg(pbus, PBUS_PCIE2_MEM_BASE, ASIC_PBUS_PCIE2_BASE);
	set_pbus_reg(pbus, PBUS_PCIE2_MEM_MASK, ASIC_PBUS_PCIE2_MASK);
#endif
#endif
}

static inline void bus_timeout_init(struct airoha_pbus *pbus)
{
	set_scu_reg(pbus, CR_NP_SCU_MON_TMR, ASIC_PBUS_CLK);
	set_scu_reg(pbus, CR_NP_SCU_MON_TMR, ASIC_PBUS_CLK | PBUS_TOUT_EN);
}

static inline void chip_scu_init(u8 __iomem *base)
{
#ifdef CONFIG_PCI
#if !IS_ENABLED(CONFIG_MT7916_AP)
	u32 val;

	/* set PCIe PERST reset as open-drain */
	val = readl(base + CR_CHIP_SCU_RGS_OPEN_DRAIN);
	val |= 0x7;
	writel(val, base + CR_CHIP_SCU_RGS_OPEN_DRAIN);
#endif
#endif
}

static irqreturn_t bus_to_interrupt(int irq, void *dev_id)
{
	struct airoha_pbus *pbus = (struct airoha_pbus *)dev_id;
	u32 pratir;
	u32 addr;

	/* clear interrupt */
	pratir = get_scu_reg(pbus, CR_NP_SCU_PRATIR) & 0x1;
	set_scu_reg(pbus, CR_NP_SCU_PRATIR, 0x1);

	addr = get_scu_reg(pbus, CR_NP_SCU_PB_TO_ERR);
	addr &= ~((1u << 30) | (1u << 31));

	dev_warn(pbus->dev, "bus timeout interrupt(%x), error address is %08x\n",
		 pratir, addr);

//	dump_stack();

	return IRQ_HANDLED;
}

static int airoha_pbus_probe(struct platform_device *pdev)
{
	struct airoha_pbus *pbus;
	struct device_node *ns;
	struct resource *res, _res;
	u8 __iomem *scu_base;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no DT node found\n");
		return -EINVAL;
	}

	/* traverse to SCU node */
	ns = of_parse_phandle(pdev->dev.of_node, "airoha,scu", 0);
	if (!ns) {
		dev_err(&pdev->dev, "SCU node not found\n");
		return -ENODEV;
	}

	/* get NP_SCU resource */
	res = &_res;
	ret = of_address_to_resource(ns, 0, res);

	/* map Chip_SCU */
	scu_base = of_iomap(ns, 1);
	if (scu_base) {
		chip_scu_init(scu_base);
		iounmap(scu_base);
	}

	of_node_put(ns);

	if (ret)
		return ret;

	pbus = devm_kzalloc(&pdev->dev, sizeof(*pbus), GFP_KERNEL);
	if (!pbus)
		return -ENOMEM;

	pbus->dev = &pdev->dev;

	/* map NP_SCU */
	pbus->scu_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!pbus->scu_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pbus->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pbus->base))
		return PTR_ERR(pbus->base);

	platform_set_drvdata(pdev, pbus);

	get_package_info(pbus);

	xsi_serdes_init(pbus);
	pci_monitor_init(pbus);
	bus_timeout_init(pbus);

	/* bus timeout interrupt */
	pbus->irq = platform_get_irq(pdev, 0);
	if (pbus->irq < 0) {
		dev_err(&pdev->dev, "irq number failed\n");
		return pbus->irq;
	}

	ret = devm_request_irq(pbus->dev, pbus->irq, bus_to_interrupt, 0,
			       "bus-timeout", pbus);
	if (ret) {
		dev_err(&pdev->dev, "request irq (%d) failed\n", pbus->irq);
		return ret;
	}

	return 0;
}

static const struct of_device_id airoha_pbus_of_id[] = {
	{ .compatible = "econet,ecnt-pbus_monitor" },
	{ /* sentinel */ }
};

static struct platform_driver airoha_pbus_driver = {
	.probe = airoha_pbus_probe,
	.driver = {
		.name = "airoha-pbus",
		.of_match_table = airoha_pbus_of_id
	},
};
builtin_platform_driver(airoha_pbus_driver);
