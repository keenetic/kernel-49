#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if defined(CONFIG_MACH_AN7581) || \
    defined(CONFIG_MACH_AN7583)
#define EMI_CLK_ASIC			(0x21C00000)	/* 566 MHz */
#else
#error Airoha SoC not defined
#endif

/* RBUS core */
#define CR_TIMEOUT_STS0			(0xd0)
#define CR_TIMEOUT_STS1			(0xd4)
#define CR_TIMEOUT_CFG0			(0xd8)
#define CR_TIMEOUT_CFG1			(0xdc)
#define CR_TIMEOUT_CFG2			(0xe0)

struct airoha_rbus {
	struct device *dev;
	u8 __iomem *base;
};

static inline u32 get_rbus_reg(struct airoha_rbus *rbus, u32 reg)
{
	return readl(rbus->base + reg);
}

static inline void set_rbus_reg(struct airoha_rbus *rbus, u32 reg, u32 val)
{
	writel(val, rbus->base + reg);
}

static inline void rbus_timeout_init(struct airoha_rbus *rbus)
{
	u32 emi_clk = EMI_CLK_ASIC;

	/* set cmd/wdata/rdata timeout_cnt as 100 ms */
	set_rbus_reg(rbus, CR_TIMEOUT_CFG0, emi_clk / 10);
	set_rbus_reg(rbus, CR_TIMEOUT_CFG1, emi_clk / 10);
	set_rbus_reg(rbus, CR_TIMEOUT_CFG2, emi_clk / 10);

	/* enable BUS timeout */
	set_rbus_reg(rbus, CR_TIMEOUT_STS0, 0x80000000);
}

static int airoha_rbus_probe(struct platform_device *pdev)
{
	struct airoha_rbus *rbus;
	struct resource *res;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no DT node found\n");
		return -EINVAL;
	}

	rbus = devm_kzalloc(&pdev->dev, sizeof(*rbus), GFP_KERNEL);
	if (!rbus)
		return -ENOMEM;

	platform_set_drvdata(pdev, rbus);

	/* get RBUS base address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rbus->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rbus->base))
		return PTR_ERR(rbus->base);

	rbus->dev = &pdev->dev;

	rbus_timeout_init(rbus);

	return 0;
}

static const struct of_device_id airoha_rbus_of_id[] = {
	{ .compatible = "econet,ecnt-rbus" },
	{ /* sentinel */ }
};

static struct platform_driver airoha_rbus_driver = {
	.probe = airoha_rbus_probe,
	.driver = {
		.name = "airoha-rbus",
		.of_match_table = airoha_rbus_of_id
	},
};
builtin_platform_driver(airoha_rbus_driver);
