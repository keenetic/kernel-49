// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <soc/airoha/efuse.h>

enum pxp_top {
	PXP_PMA,
	PXP_ANA,
	PXP_TOP_MAX
};

/**
 * struct airoha_pxp - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @top[]: IO mapped register base address
 * @xr_dtime: IO mapped register base address of Tx-Rx detection time
 * @rx_aeq: IO mapped register base address of Rx AEQ training
 */
struct airoha_pxp {
	struct device *dev;
	struct phy *phy;

	void __iomem *top[PXP_TOP_MAX];
	void __iomem *xr_dtime;
	void __iomem *rx_aeq;

	/* efuse data */
	u32 pcie_tx_term_sel;
	u32 pcie_rx_fe50_sel;
	u32 pcie_common_trim;
};

/* magic value, mean use delay */
#define PXP_DLY		0xffff

struct pxp_reg_entry {
	enum pxp_top top : 16;
	u16 reg;
	u32 val;
	u32 msk;
	u32 ofs;
};

static void pxp_reg_rw(void __iomem *pxp_reg, u32 val, u32 msk, u32 ofs)
{
	u32 tmp = readl(pxp_reg);

	tmp &= ~(msk << ofs);
	tmp |= ((val & msk) << ofs);
	writel(tmp, pxp_reg);
}

static void pxp_ana_md(struct airoha_pxp *pxp, u32 reg, u32 val, u32 msk, u32 ofs)
{
	pxp_reg_rw(pxp->top[PXP_ANA] + reg, val, msk, ofs);
}

static void pxp_pma_md(struct airoha_pxp *pxp, u32 reg, u32 val, u32 msk, u32 ofs)
{
	pxp_reg_rw(pxp->top[PXP_PMA] + reg, val, msk, ofs);
}

static void pxp_reg_batch(struct airoha_pxp *pxp, const struct pxp_reg_entry *tbl,
			  size_t tbl_size)
{
	size_t i;

	for (i = 0; i < tbl_size; i++) {
		const struct pxp_reg_entry *re = &tbl[i];

		if (re->top < PXP_TOP_MAX) {
			void __iomem *reg = pxp->top[re->top] + (u32)re->reg;

			pxp_reg_rw(reg, re->val, re->msk, re->ofs);
		} else if (re->val > 0) {
			/* treat as delay in microseconds */
			if (re->val <= 100)
				udelay(re->val);
			else
				usleep_range(re->val, re->val + 100);
		}
	}
}

static void airoha_pxp_apply_efuse(struct airoha_pxp *pxp)
{
	u32 pcie_tx_term_sel = pxp->pcie_tx_term_sel;
	u32 pcie_rx_fe50_sel = pxp->pcie_rx_fe50_sel;
	u32 pcie_common_trim = pxp->pcie_common_trim;

	if (pcie_tx_term_sel == EFUSE_INVALID_VAL)
		pcie_tx_term_sel = 0x1;

	if (pcie_rx_fe50_sel == EFUSE_INVALID_VAL)
		pcie_rx_fe50_sel = 0x1;

	if (pcie_common_trim == EFUSE_INVALID_VAL)
		pcie_common_trim = 0x10;

	pxp_ana_md(pxp, 0x000, pcie_common_trim, 0x1f, 24);
	pxp_pma_md(pxp, 0x77c, pcie_tx_term_sel, 0x7, 0);
	pxp_pma_md(pxp, 0x77c, 1, 0x1, 8);
	pxp_ana_md(pxp, 0x114, pcie_rx_fe50_sel, 0x3, 24);
}

static void airoha_pxp_init(struct airoha_pxp *pxp)
{
	static const struct pxp_reg_entry PXP_INIT_TBL[] = {
		{ PXP_PMA, 0x8b8,          1,        0x1,  8 },
		{ PXP_PMA, 0x908,          0,        0x1, 16 },
		{ PXP_PMA, 0x924, 0xcccbcccb, 0xffffffff,  0 },
		{ PXP_PMA, 0x928,     0xcccb, 0xffffffff,  0 },
		{ PXP_ANA, 0x000,          1,        0x1,  0 },

		/* CLK out */
		{ PXP_ANA, 0x098, 0x90000205, 0xff030307,  0 },
		{ PXP_ANA, 0x0a0,        0xd,       0xff,  8 },

		/* ANA enable */
		{ PXP_PMA, 0x460,      0x122,      0x122,  0 },
		{ PXP_PMA, 0x260,      0x101,      0x101,  0 },
		{ PXP_DLY,     0,          1,          0,  0 }, // delay

		/* RX PLL */
		{ PXP_PMA, 0x914, 0x2a00090b, 0xffffffff,  0 },
		{ PXP_ANA, 0x10c,          1,        0x1,  2 },
		{ PXP_ANA, 0x0dc,          0,        0x1,  8 },
		{ PXP_ANA, 0x0d8,          1,        0x3,  8 },

		/* JCPLL */
		{ PXP_PMA, 0x828,  0x1000000,  0x1010000,  0 },
		{ PXP_ANA, 0x048,       0x20,       0xff,  8 },
		{ PXP_ANA, 0x01c,          1,        0x1,  8 },
		{ PXP_ANA, 0x038,          0,     0xffff,  0 },
		{ PXP_ANA, 0x034,          0,     0xffff,  8 },
		{ PXP_ANA, 0x038,          0,     0xffff, 16 },
		{ PXP_ANA, 0x030,          0,        0x1, 17 },
		{ PXP_ANA, 0x034,          0,        0x1,  0 },
		{ PXP_ANA, 0x008,    0xc1f0a,   0x1f1f1f,  0 },
		{ PXP_ANA, 0x00c,       0x1e,       0x1f,  0 },
		{ PXP_ANA, 0x008,        0xa,       0x1f, 24 },
		{ PXP_ANA, 0x014,          1,        0x3,  0 },
		{ PXP_ANA, 0x018,          0,        0x3, 24 },
		{ PXP_PMA, 0x79c,          1,        0x1, 16 },
		{ PXP_PMA, 0x800, 0x50000000, 0x7fffffff,  0 },
		{ PXP_ANA, 0x014,  0x1010000,  0x1010000,  0 },
		{ PXP_ANA, 0x01c,          4,  0x3000007,  0 },
		{ PXP_ANA, 0x04c,          0,        0x1,  8 },
		{ PXP_ANA, 0x004,    0xc0000, 0x3f3f0000,  0 },
		{ PXP_ANA, 0x014,          1,        0x3,  0 },
		{ PXP_ANA, 0x02c,  0x4010100,  0x7010300,  0 },
		{ PXP_ANA, 0x004,          0,        0x1,  8 },
		{ PXP_ANA, 0x010,  0x1000300,  0x1030303,  0 },
		{ PXP_ANA, 0x00c,          1,        0x7, 24 },
		{ PXP_PMA, 0x884,      0x100,      0x101,  0 },
		{ PXP_ANA, 0x00c,       0xe4,       0xff, 16 },
		{ PXP_ANA, 0x024,          1,        0x1,  8 },
		{ PXP_ANA, 0x028,          1,        0x1, 16 },
		{ PXP_ANA, 0x048,        0xf,       0x1f, 16 },
		{ PXP_ANA, 0x024,  0x5010000, 0x1f030000,  0 },
		{ PXP_ANA, 0x028,          1,        0x7, 24 },
		{ PXP_ANA, 0x030,          3,        0x7,  0 },
		{ PXP_PMA, 0x828,  0x1010101,  0x1010101,  0 },
		{ PXP_DLY,     0,        200,          0,  0 }, // delay

		/* TX PLL */
		{ PXP_PMA, 0x854,  0x1000000,  0x1010000,  0 },
		{ PXP_ANA, 0x064,          1,        0x1, 24 },
		{ PXP_ANA, 0x080,          0, 0xffffffff,  0 },
		{ PXP_ANA, 0x084,          0,     0xffff,  0 },
		{ PXP_ANA, 0x050,      0x12d,     0x3f3f,  0 },
		{ PXP_ANA, 0x064,          0,        0x3,  8 },
		{ PXP_ANA, 0x074,          3,        0x3, 24 },
		{ PXP_PMA, 0x794,          1,        0x1, 24 },
		{ PXP_PMA, 0x798,  0xc800000, 0x7fffffff,  0 },
		{ PXP_ANA, 0x068,          0,        0x1, 16 },
		{ PXP_ANA, 0x07c,          0,        0x1,  8 },
		{ PXP_ANA, 0x064,          4,        0x7, 16 },
		{ PXP_ANA, 0x068,          0,        0x3,  8 },
		{ PXP_ANA, 0x06c,          3,        0x3,  0 },
		{ PXP_ANA, 0x094,          0,        0x1, 24 },
		{ PXP_ANA, 0x080,          0, 0xffffffff,  0 },
		{ PXP_ANA, 0x054,          1,       0x1f,  0 },
		{ PXP_ANA, 0x050, 0x1805012d, 0x1f1f3f3f,  0 },
		{ PXP_ANA, 0x090,          1,       0xff, 24 },
		{ PXP_ANA, 0x054,          0,       0x1f, 16 },
		{ PXP_ANA, 0x05c,          0,        0x3, 16 },
		{ PXP_ANA, 0x064,          0,        0x3,  8 },
		{ PXP_ANA, 0x078,      0x701,      0x701,  0 },
		{ PXP_ANA, 0x074,          3,        0x3, 24 },
		{ PXP_PMA, 0x794,          1,        0x1, 24 },
		{ PXP_ANA, 0x07c,          0,        0x1,  8 },
		{ PXP_ANA, 0x054,          0,       0x1f,  8 },
		{ PXP_ANA, 0x064,          1,        0x1,  0 },
		{ PXP_ANA, 0x094,          0,        0x1, 24 },
		{ PXP_ANA, 0x088,          0,        0x1,  0 },
		{ PXP_ANA, 0x060,          0,        0x1,  0 },
		{ PXP_ANA, 0x064,          1,        0x1,  0 },
		{ PXP_ANA, 0x07c,          0,        0x1,  0 },
		{ PXP_ANA, 0x04c,          0,        0x1, 25 },
		{ PXP_ANA, 0x05c,          0,        0x1,  8 },
		{ PXP_ANA, 0x058,  0x3000000,  0x3030000,  0 },
		{ PXP_ANA, 0x05c,          1,        0x3,  0 },
		{ PXP_ANA, 0x058,      0x4e4,      0x7ff,  0 },
		{ PXP_ANA, 0x06c,          1,        0x1, 24 },
		{ PXP_ANA, 0x074,          1,        0x1,  0 },
		{ PXP_ANA, 0x094,        0xf,       0x1f,  0 },
		{ PXP_ANA, 0x070,      0xb03,     0x1f07,  0 },
		{ PXP_ANA, 0x074,          3,        0x7,  8 },
		{ PXP_PMA, 0x854,  0x1010101,  0x1010101,  0 },
		{ PXP_DLY,     0,        200,          0,  0 }, // delay

		/* SSC JCPLL */
		{ PXP_ANA, 0x038,      0x106,     0xffff,  0 },
		{ PXP_ANA, 0x034,      0x106,     0xffff,  8 },
		{ PXP_ANA, 0x038,      0x31b,     0xffff, 16 },
		{ PXP_ANA, 0x030,          3,        0x3, 16 },
		{ PXP_ANA, 0x020,          1,        0x1,  0 },
		{ PXP_ANA, 0x024,          1,        0x1,  0 },
		{ PXP_ANA, 0x01c,          0,        0x1, 16 },
		{ PXP_ANA, 0x034,          1,        0x1,  0 },
		{ PXP_DLY,     0,         30,          0,  0 }, // delay

		/* RX lane signal detect */
		{ PXP_ANA, 0x108,          1,        0x1, 16 },
		{ PXP_DLY,     0,         10,          0,  0 }, // delay
		{ PXP_PMA, 0x91c,     0x18b0,     0xffff, 16 },
		{ PXP_PMA, 0x920, 0x103018b0, 0xffffffff,  0 },
		{ PXP_ANA, 0x114,    0x50200,   0x1f0300,  0 },
		{ PXP_ANA, 0x0d4,          8,        0xc, 16 },
		{ PXP_ANA, 0x110,          1,        0x3, 24 },
		{ PXP_PMA, 0x164,          0,        0xf,  8 },
		{ PXP_ANA, 0x11c,          1,        0x1,  0 },
		{ PXP_PMA, 0x88c,      0x103,      0x103,  0 },
		{ PXP_PMA, 0x294,          1,        0x3,  0 },
		{ PXP_PMA, 0x168,          3,       0x1f, 24 },
		{ PXP_PMA, 0x10c,          0,        0x1,  0 },
		{ PXP_PMA, 0x114,          1,        0x1,  0 },
		{ PXP_DLY,     0,        100,          0,  0 }, // delay
		{ PXP_PMA, 0x114,          0,        0x1,  0 },

		/* RX flow */
		{ PXP_PMA, 0x84c,  0x1010000,  0x1010000,  0 },
		{ PXP_PMA, 0x81c,      0x101,      0x101,  0 },
		{ PXP_PMA, 0x894,      0x101,      0x101,  0 },
		{ PXP_ANA, 0x0d8,  0x1010000,  0x1010000,  0 },
		{ PXP_PMA, 0x460,       0x5d,       0x5d,  0 },
		{ PXP_ANA, 0x118,  0x1010100,  0x1010100,  0 },
		{ PXP_ANA, 0x0d4,      0x440,      0x770, 16 },
		{ PXP_DLY,     0,         10,          0,  0 }, // delay

		/* PR setting */
		{ PXP_ANA, 0x0fc,      0x505,      0x707,  0 },
		{ PXP_ANA, 0x100,          0,        0x3,  0 },
		{ PXP_ANA, 0x108,          0,        0x3, 24 },
		{ PXP_ANA, 0x0e8,    0x20000,    0x7ffff,  8 },
		{ PXP_ANA, 0x0f8,      0x402,      0x70f, 16 },

		/* TX flow */
		{ PXP_ANA, 0x0c4,  0x1000001,  0x1000001,  0 },
		{ PXP_DLY,     0,         10,          0,  0 }, // delay

		/* PR / RX mode setting, divider, etc */
		{ PXP_PMA, 0x93c,   0x804000, 0xffffffff,  0 },
		{ PXP_PMA, 0x918,    0x50505,   0x1f1f1f,  0 },
		{ PXP_PMA, 0x948,    0x77700,    0x77700,  0 },
		{ PXP_ANA, 0x104,    0x20000,    0xf0001,  0 },
		{ PXP_ANA, 0x128,     0x1919,     0x3f3f, 16 },
		{ PXP_ANA, 0x12c,       0x14,       0x3f,  0 },

#if 0
		/* for eye scan, remove if not used for power saving */
		{ PXP_PMA, 0x790,          0,       0x7f, 16 },
		{ PXP_PMA, 0x790,          1,        0x1, 24 },
		{ PXP_PMA, 0x824,          1,        0x1,  0 },
		{ PXP_PMA, 0x824,          1,        0x1,  8 },
#endif

		{ PXP_PMA, 0x34c,          0,        0x1,  8 },
		{ PXP_DLY,     0,         10,          0,  0 }, // delay
		{ PXP_PMA, 0x34c,          1,        0x1,  8 },
		{ PXP_DLY,     0,        100,          0,  0 }, // delay
	};

	pxp_reg_batch(pxp, PXP_INIT_TBL, ARRAY_SIZE(PXP_INIT_TBL));
}

/**
 * airoha_pcie_phy_init() - Initialize the phy
 * @phy: the phy to be initialized
 *
 * Initialize the phy registers.
 * The hardware settings will be reset during suspend, it should be
 * reinitialized when the consumer calls phy_init() again on resume.
 */
static int airoha_pcie_phy_init(struct phy *phy)
{
	struct airoha_pxp *pxp = phy_get_drvdata(phy);

	/* setup Tx-Rx detection time */
	writel(0x23020133, pxp->xr_dtime);

	/* setup Rx AEQ training time */
	writel(0x50500032, pxp->rx_aeq);

	/* write back debug_selector to default value */
	pxp_pma_md(pxp, 0x908, 0x110648, 0xffffffff, 0);

	/* apply eFUSE data */
	airoha_pxp_apply_efuse(pxp);

	/* original flow */
	airoha_pxp_init(pxp);

	/* wait for the PCIe PHY to complete initialization before returning */
	usleep_range(20000, 21000);

	return 0;
}

static int airoha_pcie_phy_exit(struct phy *phy)
{
	return 0;
}

static const struct phy_ops airoha_pcie_phy_ops = {
	.init = airoha_pcie_phy_init,
	.exit = airoha_pcie_phy_exit,
	.owner = THIS_MODULE,
};

static void read_efuse_cal_data(struct airoha_pxp *pxp)
{
	void *mem;
	u32 efuse_vaild;

	pxp->pcie_tx_term_sel = EFUSE_INVALID_VAL;
	pxp->pcie_rx_fe50_sel = EFUSE_INVALID_VAL;
	pxp->pcie_common_trim = EFUSE_INVALID_VAL;

	mem = airoha_efuse_load();
	if (!mem)
		return;

	efuse_vaild = airoha_efuse_get(mem, SDS_PCIe_efuse_valid_LSB, 3);

	if (efuse_vaild & BIT(0)) {
		pxp->pcie_tx_term_sel = airoha_efuse_get(mem,
						RG_PXP_PCIE_TX0_TERM_SEL_LSB, 2);

		pxp->pcie_rx_fe50_sel = airoha_efuse_get(mem,
						RG_PXP_PCIE_RX0_FE_50OHMS_SEL_LSB, 2);
	} else
		dev_warn(pxp->dev, "PCIe PXP eFuse not vaild\n");

	if (efuse_vaild & BIT(2))
		pxp->pcie_common_trim = airoha_efuse_get(mem,
						RG_PXP_PCIE_CMN_TRIM_LSB, 5);

	airoha_efuse_free(mem);
}

static int airoha_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct airoha_pxp *pxp;
	struct resource *res;

	pxp = devm_kzalloc(dev, sizeof(*pxp), GFP_KERNEL);
	if (!pxp)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csr-1l");
	pxp->top[PXP_ANA] = devm_ioremap_resource(dev, res);
	if (IS_ERR(pxp->top[PXP_ANA])) {
		dev_err(dev, "Failed to map %s base\n", "csr-1l");
		return PTR_ERR(pxp->top[PXP_ANA]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pma");
	pxp->top[PXP_PMA] = devm_ioremap_resource(dev, res);
	if (IS_ERR(pxp->top[PXP_PMA])) {
		dev_err(dev, "Failed to map %s base\n", "pma");
		return PTR_ERR(pxp->top[PXP_PMA]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "p0-xr-dtime");
	pxp->xr_dtime = devm_ioremap_resource(dev, res);
	if (IS_ERR(pxp->xr_dtime)) {
		dev_err(dev, "Failed to map %s base\n", "p0-xr-dtime");
		return PTR_ERR(pxp->xr_dtime);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rx-aeq");
	pxp->rx_aeq = devm_ioremap_resource(dev, res);
	if (IS_ERR(pxp->rx_aeq)) {
		dev_err(dev, "Failed to map %s base\n", "rx-aeq");
		return PTR_ERR(pxp->rx_aeq);
	}

	pxp->phy = devm_phy_create(dev, dev->of_node, &airoha_pcie_phy_ops);
	if (IS_ERR(pxp->phy)) {
		dev_err(dev, "Failed to create PCIe phy\n");
		return PTR_ERR(pxp->phy);
	}

	pxp->dev = dev;
	platform_set_drvdata(pdev, pxp);
	phy_set_drvdata(pxp->phy, pxp);

	read_efuse_cal_data(pxp);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_err(dev, "PCIe phy probe failed\n");
		return PTR_ERR(provider);
	}

	return 0;
}

static const struct of_device_id airoha_pcie_phy_of_match[] = {
	{ .compatible = "airoha,an7583-pcie-phy"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, airoha_pcie_phy_of_match);

static struct platform_driver airoha_pcie_phy_driver = {
	.probe	= airoha_pcie_phy_probe,
	.driver	= {
		.name = "airoha-pcie-phy",
		.of_match_table = airoha_pcie_phy_of_match,
	},
};
module_platform_driver(airoha_pcie_phy_driver);

MODULE_DESCRIPTION("Airoha PCIe Gen3 PHY driver");
MODULE_LICENSE("GPL");
