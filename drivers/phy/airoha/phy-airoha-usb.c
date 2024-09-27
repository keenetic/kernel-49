// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

#include <soc/airoha/pkgid.h>

/* version V2 sub-banks offset base address */
/* u2 phy banks */
#define SSUSB_SIFSLV_V2_MISC		0x000
#define SSUSB_SIFSLV_V2_U2FREQ		0x100
#define SSUSB_SIFSLV_V2_U2PHY_COM	0x300
/* u3 phy banks */
#define SSUSB_SIFSLV_V2_SPLLC		0x000
#define SSUSB_SIFSLV_V2_CHIP		0x100
#define SSUSB_SIFSLV_V2_U3PHYD		0x200
#define SSUSB_SIFSLV_V2_U3PHYA		0x400

#define U3P_USBPHYACR4			0x010
#define PA4_RG_U2_FS_CR			GENMASK(10, 8)
#define PA4_RG_U2_FS_CR_VAL(x)		((0x7 & (x)) << 8)
#define PA4_RG_U2_FS_SR			GENMASK(2, 0)
#define PA4_RG_U2_FS_SR_VAL(x)		 (0x7 & (x))

#define U3P_USBPHYACR5			0x014
#define PA5_RG_U2_HSTX_SRCAL_EN		BIT(15)
#define PA5_RG_U2_HSTX_SRCTRL		GENMASK(14, 12)
#define PA5_RG_U2_HSTX_SRCTRL_VAL(x)	((0x7 & (x)) << 12)
#define PA5_RG_U2_HS_100U_U3_EN		BIT(11)

#define U3P_USBPHYACR6			0x018
#define PA6_RG_U2_BC11_SW_EN		BIT(23)
#define PA6_RG_U2_DISCTH		GENMASK(7, 4)
#define PA6_RG_U2_DISCTH_VAL(x)		((0xf & (x)) << 4)
#define PA6_RG_U2_SQTH			GENMASK(3, 0)
#define PA6_RG_U2_SQTH_VAL(x)		 (0xf & (x))

#define U3P_U3_CHIP_GPIO_CTLD		0x0c
#define P3C_REG_IP_SW_RST		BIT(31)
#define P3C_MCU_BUS_CK_GATE_EN		BIT(30)
#define P3C_FORCE_IP_SW_RST		BIT(29)

#define U3P_U3_CHIP_GPIO_CTLE		0x10
#define P3C_RG_SWRST_U3_PHYD		BIT(25)
#define P3C_RG_SWRST_U3_PHYD_FORCE_EN	BIT(24)

#define U3P_U3_PHYA_REG0		0x000
#define P3A_RG_BG_DIV			GENMASK(29, 28)
#define P3A_RG_BG_DIV_VAL(x)		((0x3 & (x)) << 28)

#define U3P_U3_PHYA_REG1		0x004
#define P3A_RG_XTAL_TOP_RESERVE		GENMASK(25, 10)
#define P3A_RG_XTAL_TOP_RESERVE_VAL(x)	((0xffff & (x)) << 10)

#define U3P_U3_PHYA_REG6		0x018
#define P3A_RG_CDR_RESERVE		GENMASK(31, 24)
#define P3A_RG_CDR_RESERVE_VAL(x)	((0xff & (x)) << 24)

#define U3P_U3_PHYA_REG8		0x020
#define P3A_RG_CDR_RST_DLY		GENMASK(7, 6)
#define P3A_RG_CDR_RST_DLY_VAL(x)	((0x3 & (x)) << 6)

#define U3P_U3_PHYA_DA_REG19		0x138
#define P3A_RG_PLL_SSC_DELTA1_U3	GENMASK(15, 0)
#define P3A_RG_PLL_SSC_DELTA1_U3_VAL(x)	 (0xffff & (x))

#define U3P_U2FREQ_FMCR0		0x00
#define P2F_RG_MONCLK_SEL		GENMASK(27, 26)
#define P2F_RG_MONCLK_SEL_VAL(x)	((0x3 & (x)) << 26)
#define P2F_RG_FREQDET_EN		BIT(24)
#define P2F_RG_CYCLECNT			GENMASK(23, 0)
#define P2F_RG_CYCLECNT_VAL(x)		((P2F_RG_CYCLECNT) & (x))

#define U3P_U2FREQ_FMMONR0		0x0c

#define U3P_U2FREQ_FMMONR1		0x10
#define P2F_USB_FM_VALID		BIT(0)
#define P2F_RG_FRCK_EN			BIT(8)

#define U3P_REF_CLK			20	/* MHZ */
#define U3P_SLEW_RATE_COEF		28
#define U3P_SR_COEF_DIVISOR		1000
#define U3P_FM_DET_CYCLE_CNT		1024

struct u2phy_banks {
	void __iomem *misc;
	void __iomem *fmreg;
	void __iomem *com;
};

struct u3phy_banks {
	void __iomem *spllc;
	void __iomem *chip;
	void __iomem *phyd;	/* include u3phyd_bank2 */
	void __iomem *phya;	/* include u3phya_da */
};

struct airoha_uphy_instance {
	struct phy *phy;
	void __iomem *port_base;
	union {
		struct u2phy_banks u2_banks;
		struct u3phy_banks u3_banks;
	};
	u32 index;
	u32 type;
	bool type_sw;
};

struct airoha_uphy {
	struct device *dev;
	struct airoha_uphy_instance **phys;
	int nphys;
	int src_ref_clk;	/* MHZ, reference clock for slew rate calibrate */
	int src_coef;		/* coefficient for slew rate calibrate */
};

static void hs_slew_rate_calibrate(struct airoha_uphy *uphy,
				   struct airoha_uphy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *fmreg = u2_banks->fmreg;
	void __iomem *com = u2_banks->com;
	int calibration_val;
	int fm_out;
	u32 tmp;

	/* enable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp |= PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
	usleep_range(100, 200);

	/* enable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp |= P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	/* set cycle count as 1024, and select u2 channel */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~(P2F_RG_CYCLECNT | P2F_RG_MONCLK_SEL);
	tmp |= P2F_RG_CYCLECNT_VAL(U3P_FM_DET_CYCLE_CNT);
	tmp |= P2F_RG_MONCLK_SEL_VAL(instance->index + 1);
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* enable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp |= P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* poll frequency meter ready */
	readl_poll_timeout(fmreg + U3P_U2FREQ_FMMONR1, tmp,
			   (tmp & P2F_USB_FM_VALID), 100, 2000);

	fm_out = readl(fmreg + U3P_U2FREQ_FMMONR0);

	/* disable frequency meter */
	tmp = readl(fmreg + U3P_U2FREQ_FMCR0);
	tmp &= ~P2F_RG_FREQDET_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMCR0);

	/* disable free run clock */
	tmp = readl(fmreg + U3P_U2FREQ_FMMONR1);
	tmp &= ~P2F_RG_FRCK_EN;
	writel(tmp, fmreg + U3P_U2FREQ_FMMONR1);

	/* disable USB ring oscillator */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCAL_EN;
	writel(tmp, com + U3P_USBPHYACR5);
	usleep_range(100, 200);

	if (fm_out) {
		/* ( 1024 / FM_OUT ) x reference clock frequency x coef */
		tmp = uphy->src_ref_clk * uphy->src_coef;
		tmp = (tmp * U3P_FM_DET_CYCLE_CNT) / fm_out;
		calibration_val = DIV_ROUND_CLOSEST(tmp, U3P_SR_COEF_DIVISOR);
	} else {
		/* if FM detection fail, set default value */
		calibration_val = 5;
	}

	/* set HS slew rate */
	tmp = readl(com + U3P_USBPHYACR5);
	tmp &= ~PA5_RG_U2_HSTX_SRCTRL;
	tmp |=  PA5_RG_U2_HSTX_SRCTRL_VAL(calibration_val);
	writel(tmp, com + U3P_USBPHYACR5);
	udelay(1);

	dev_dbg(uphy->dev, "phy: %d, fm_out: %d, calib: %d\n",
		instance->index, fm_out, calibration_val);
}

static void pcie_phy_instance_init(struct airoha_uphy *uphy,
				   struct airoha_uphy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG8);
	tmp &= ~P3A_RG_CDR_RST_DLY;
	tmp |=  P3A_RG_CDR_RST_DLY_VAL(0);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG8);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG6);
	tmp &= ~P3A_RG_CDR_RESERVE;
	tmp |=  P3A_RG_CDR_RESERVE_VAL(0xe);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG6);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
	tmp &= ~P3A_RG_BG_DIV;
	tmp |=  P3A_RG_BG_DIV_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG1);
	tmp &= ~P3A_RG_XTAL_TOP_RESERVE;
	tmp |=  P3A_RG_XTAL_TOP_RESERVE_VAL(0x600);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG1);
}

static void u3_phy_instance_init(struct airoha_uphy *uphy,
				 struct airoha_uphy_instance *instance)
{
	struct u3phy_banks *u3_banks = &instance->u3_banks;
	u32 tmp;

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG8);
	tmp &= ~P3A_RG_CDR_RST_DLY;
	tmp |=  P3A_RG_CDR_RST_DLY_VAL(0);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG8);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG6);
	tmp &= ~P3A_RG_CDR_RESERVE;
	tmp |=  P3A_RG_CDR_RESERVE_VAL(0xe);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG6);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG0);
	tmp &= ~P3A_RG_BG_DIV;
	tmp |=  P3A_RG_BG_DIV_VAL(0x1);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG0);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_REG1);
	tmp &= ~P3A_RG_XTAL_TOP_RESERVE;
	tmp |=  P3A_RG_XTAL_TOP_RESERVE_VAL(0x600);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_REG1);

	tmp = readl(u3_banks->phya + U3P_U3_PHYA_DA_REG19);
	tmp &= ~P3A_RG_PLL_SSC_DELTA1_U3;
	tmp |=  P3A_RG_PLL_SSC_DELTA1_U3_VAL(0x43);
	writel(tmp, u3_banks->phya + U3P_U3_PHYA_DA_REG19);
}

static void u2_phy_instance_init(struct airoha_uphy *uphy,
				 struct airoha_uphy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	void __iomem *com = u2_banks->com;
	u32 tmp;

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_BC11_SW_EN;	/* DP/DM BC1.1 path Disable */
	writel(tmp, com + U3P_USBPHYACR6);
	usleep_range(500, 800);

	tmp = readl(com + U3P_USBPHYACR4);
	tmp &= ~PA4_RG_U2_FS_CR;
	tmp &= ~PA4_RG_U2_FS_SR;
	tmp |=  PA4_RG_U2_FS_CR_VAL(6);
	tmp |=  PA4_RG_U2_FS_SR_VAL(2);
	writel(tmp, com + U3P_USBPHYACR4);

	if (GET_PDIDR() >= 2) {
		tmp = readl(com + U3P_USBPHYACR6);
		tmp &= ~PA6_RG_U2_SQTH;
		tmp |=  PA6_RG_U2_SQTH_VAL(0x9);
		writel(tmp, com + U3P_USBPHYACR6);
	}

	tmp = readl(com + U3P_USBPHYACR6);
	tmp &= ~PA6_RG_U2_DISCTH;
	tmp |=  PA6_RG_U2_DISCTH_VAL(0xa);
	writel(tmp, com + U3P_USBPHYACR6);
}

static void pcie_phy_instance_power_on(struct airoha_uphy *uphy,
					struct airoha_uphy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp &= ~(P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp &= ~(P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD);
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);
}

static void pcie_phy_instance_power_off(struct airoha_uphy *uphy,
					struct airoha_uphy_instance *instance)
{
	struct u3phy_banks *bank = &instance->u3_banks;
	u32 tmp;

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLD);
	tmp |= P3C_FORCE_IP_SW_RST | P3C_REG_IP_SW_RST;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLD);

	tmp = readl(bank->chip + U3P_U3_CHIP_GPIO_CTLE);
	tmp |= P3C_RG_SWRST_U3_PHYD_FORCE_EN | P3C_RG_SWRST_U3_PHYD;
	writel(tmp, bank->chip + U3P_U3_CHIP_GPIO_CTLE);
}

static void phy_v2_banks_init(struct airoha_uphy *uphy,
			      struct airoha_uphy_instance *instance)
{
	struct u2phy_banks *u2_banks = &instance->u2_banks;
	struct u3phy_banks *u3_banks = &instance->u3_banks;

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_banks->misc = instance->port_base + SSUSB_SIFSLV_V2_MISC;
		u2_banks->fmreg = instance->port_base + SSUSB_SIFSLV_V2_U2FREQ;
		u2_banks->com = instance->port_base + SSUSB_SIFSLV_V2_U2PHY_COM;
		break;
	case PHY_TYPE_USB3:
	case PHY_TYPE_PCIE:
		u3_banks->spllc = instance->port_base + SSUSB_SIFSLV_V2_SPLLC;
		u3_banks->chip = instance->port_base + SSUSB_SIFSLV_V2_CHIP;
		u3_banks->phyd = instance->port_base + SSUSB_SIFSLV_V2_U3PHYD;
		u3_banks->phya = instance->port_base + SSUSB_SIFSLV_V2_U3PHYA;
		break;
	default:
		dev_err(uphy->dev, "incompatible PHY type\n");
		return;
	}
}

/* type switch for usb3/pcie */
static int phy_type_switch_get(struct airoha_uphy_instance *instance,
			       struct device_node *dn)
{
	/* type switch function is optional */
	if (!of_property_read_bool(dn, "airoha,phy-type-switch"))
		return 0;

	instance->type_sw = true;

	return 0;
}

static int phy_type_set(struct airoha_uphy_instance *instance)
{
	if (!instance->type_sw)
		return 0;

	switch (instance->type) {
	case PHY_TYPE_USB3:
		break;
	case PHY_TYPE_PCIE:
		break;
	default:
		return 0;
	}

	/* set usb_pcie_sel via SCU SSTR */

	return 0;
}

static int airoha_uphy_init(struct phy *phy)
{
	struct airoha_uphy_instance *instance = phy_get_drvdata(phy);
	struct airoha_uphy *uphy = dev_get_drvdata(phy->dev.parent);

	switch (instance->type) {
	case PHY_TYPE_USB2:
		u2_phy_instance_init(uphy, instance);
		break;
	case PHY_TYPE_USB3:
		u3_phy_instance_init(uphy, instance);
		break;
	case PHY_TYPE_PCIE:
		pcie_phy_instance_init(uphy, instance);
		break;
	default:
		dev_err(uphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int airoha_uphy_power_on(struct phy *phy)
{
	struct airoha_uphy_instance *instance = phy_get_drvdata(phy);
	struct airoha_uphy *uphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_USB2) {
		hs_slew_rate_calibrate(uphy, instance);
	} else if (instance->type == PHY_TYPE_PCIE) {
		pcie_phy_instance_power_on(uphy, instance);
	}

	return 0;
}

static int airoha_uphy_power_off(struct phy *phy)
{
	struct airoha_uphy_instance *instance = phy_get_drvdata(phy);
	struct airoha_uphy *uphy = dev_get_drvdata(phy->dev.parent);

	if (instance->type == PHY_TYPE_PCIE)
		pcie_phy_instance_power_off(uphy, instance);

	return 0;
}

static int airoha_uphy_exit(struct phy *phy)
{
	return 0;
}

static struct phy *airoha_uphy_xlate(struct device *dev,
				     struct of_phandle_args *args)
{
	struct airoha_uphy *uphy = dev_get_drvdata(dev);
	struct airoha_uphy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of cells in 'phy' property\n");
		return ERR_PTR(-EINVAL);
	}

	for (index = 0; index < uphy->nphys; index++) {
		if (phy_np == uphy->phys[index]->phy->dev.of_node) {
			instance = uphy->phys[index];
			break;
		}
	}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-EINVAL);
	}

	instance->type = args->args[0];
	if (!(instance->type == PHY_TYPE_USB2 ||
	      instance->type == PHY_TYPE_USB3 ||
	      instance->type == PHY_TYPE_PCIE)) {
		dev_err(dev, "unsupported device type: %d\n", instance->type);
		return ERR_PTR(-EINVAL);
	}

	phy_v2_banks_init(uphy, instance);

	phy_type_set(instance);

	return instance->phy;
}

static const struct phy_ops airoha_uphy_ops = {
	.init		= airoha_uphy_init,
	.exit		= airoha_uphy_exit,
	.power_on	= airoha_uphy_power_on,
	.power_off	= airoha_uphy_power_off,
	.owner		= THIS_MODULE,
};

static int airoha_uphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct phy_provider *provider;
	struct airoha_uphy *uphy;
	struct resource res;
	int port, retval;

	uphy = devm_kzalloc(dev, sizeof(*uphy), GFP_KERNEL);
	if (!uphy)
		return -ENOMEM;

	uphy->nphys = of_get_child_count(np);
	uphy->phys = devm_kcalloc(dev, uphy->nphys,
				  sizeof(*uphy->phys), GFP_KERNEL);
	if (!uphy->phys)
		return -ENOMEM;

	uphy->dev = dev;
	platform_set_drvdata(pdev, uphy);

	uphy->src_ref_clk = U3P_REF_CLK;
	uphy->src_coef = U3P_SLEW_RATE_COEF;

	port = 0;
	for_each_child_of_node(np, child_np) {
		struct airoha_uphy_instance *instance;
		struct phy *phy;

		instance = devm_kzalloc(dev, sizeof(*instance), GFP_KERNEL);
		if (!instance) {
			retval = -ENOMEM;
			goto put_child;
		}

		uphy->phys[port] = instance;

		phy = devm_phy_create(dev, child_np, &airoha_uphy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			retval = PTR_ERR(phy);
			goto put_child;
		}

		retval = of_address_to_resource(child_np, 0, &res);
		if (retval) {
			dev_err(dev, "failed to get address resource(id-%d)\n",
				port);
			goto put_child;
		}

		instance->port_base = devm_ioremap_resource(&phy->dev, &res);
		if (IS_ERR(instance->port_base)) {
			dev_err(dev, "failed to remap phy regs\n");
			retval = PTR_ERR(instance->port_base);
			goto put_child;
		}

		instance->phy = phy;
		instance->index = port;
		phy_set_drvdata(phy, instance);
		port++;

		retval = phy_type_switch_get(instance, child_np);
		if (retval)
			goto put_child;
	}

	provider = devm_of_phy_provider_register(dev, airoha_uphy_xlate);

	return PTR_ERR_OR_ZERO(provider);
put_child:
	of_node_put(child_np);
	return retval;
}

static const struct of_device_id airoha_uphy_id_table[] = {
	{ .compatible = "airoha,an7581-uphy" },
	{ },
};
MODULE_DEVICE_TABLE(of, airoha_uphy_id_table);

static struct platform_driver airoha_uphy_driver = {
	.probe		= airoha_uphy_probe,
	.driver		= {
		.name	= "airoha-uphy",
		.of_match_table = airoha_uphy_id_table,
	},
};

module_platform_driver(airoha_uphy_driver);

MODULE_DESCRIPTION("Airoha USB PHY driver");
MODULE_LICENSE("GPL v2");
