// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <soc/airoha/efuse.h>

enum qp_top {
	QP_DIG,
	QP_PMA,
	QP_ANA,
	QP_TOP_MAX
};

/**
 * struct airoha_qphy - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @top[]: IO mapped register base address
 */
struct airoha_qphy {
	struct device *dev;
	struct phy *phy;
	void __iomem *top[QP_TOP_MAX];

	/* efuse data */
	u32 pcie_tx_term_sel;
	u32 pcie_rx_imp_sel;
};

struct qp_reg_entry {
	enum qp_top top : 16;
	u16 reg;
	u32 val;
	u32 msk;
	u32 ofs;
};

static void qp_reg_rw(void __iomem *qp_reg, u32 val, u32 msk, u32 ofs)
{
	u32 tmp = readl(qp_reg);

	if (tmp == 0xdeadbeef)
		return;

	tmp &= ~(msk << ofs);
	tmp |= ((val & msk) << ofs);
	writel(tmp, qp_reg);
}

static void qp_pma_md(struct airoha_qphy *qp, u32 reg, u32 val, u32 msk, u32 ofs)
{
	qp_reg_rw(qp->top[QP_PMA] + reg, val, msk, ofs);
}

static void qp_ana_md(struct airoha_qphy *qp, u32 reg, u32 val, u32 msk, u32 ofs)
{
	qp_reg_rw(qp->top[QP_ANA] + reg, val, msk, ofs);
}

static void qp_reg_batch(struct airoha_qphy *qp, const struct qp_reg_entry *tbl,
			 size_t tbl_size)
{
	size_t i;

	for (i = 0; i < tbl_size; i++) {
		const struct qp_reg_entry *re = &tbl[i];
		void __iomem *reg = qp->top[re->top] + (u32)re->reg;

		qp_reg_rw(reg, re->val, re->msk, re->ofs);
	}
}

static inline void qp_base_init(struct airoha_qphy *qp)
{
	static const struct qp_reg_entry QP_INIT_TBL[] = {
		/* RX USB PCIE enable */
		{ QP_PMA, 0x6b8,          1,        0x1, 26 },

		/* 50MHz XTAL */
		{ QP_ANA, 0x030,          1,        0x3,  2 },
		{ QP_ANA, 0x03c,          1,        0x3, 25 },
		{ QP_PMA, 0x410,          0,        0x1,  2 },
		{ QP_DIG, 0x340,          0,        0x1, 25 },
		{ QP_DIG, 0x340,          0,        0x1, 26 },
		{ QP_DIG, 0x340,          1,        0x1, 24 },
		{ QP_DIG, 0x340,          1,        0x1, 25 },
		{ QP_DIG, 0x340,          1,        0x1, 26 },

		/* PLL RG */
		{ QP_PMA, 0x408,          1,        0x1, 22 },
		{ QP_PMA, 0x408,          4,        0xf, 16 },
		{ QP_PMA, 0x408,          0,        0x3, 12 },
		{ QP_PMA, 0x408,          0,        0x3, 20 },
		{ QP_PMA, 0x408,          3,        0x3,  0 },
		{ QP_PMA, 0x408,          5,        0x7,  2 },
		{ QP_PMA, 0x408,          1,        0x3,  6 },
		{ QP_PMA, 0x408,          1,        0x1, 14 },
		{ QP_PMA, 0x408,          1,        0x1, 27 },
		{ QP_PMA, 0x410,          1,        0x3,  3 },
		{ QP_PMA, 0x408,          1,        0x1, 30 },
		{ QP_PMA, 0x40c,      0x1fd,     0xffff,  0 },
		{ QP_PMA, 0x40c,      0x19c,     0xffff, 16 },
		{ QP_PMA, 0x404,          1,        0x1, 30 },
		{ QP_PMA, 0x248, 0x48000000, 0x7fffffff,  0 },
		{ QP_PMA, 0x208,          1,        0x3, 16 },
		{ QP_PMA, 0x230, 0x48000000, 0x7fffffff,  0 },

		/* RX RG 5G */
		{ QP_ANA, 0x00c,          1,        0x3,  4 },
		{ QP_ANA, 0x004,          1,        0x1, 11 },
		{ QP_ANA, 0x018,        0xc,       0x1f,  8 },
		{ QP_ANA, 0x020,          0,        0x1, 13 },
		{ QP_ANA, 0x01c,          0,        0x1, 30 },
		{ QP_ANA, 0x01c,          1,        0x1,  6 },
		{ QP_ANA, 0x018,          3,        0x7, 24 },
		{ QP_ANA, 0x01c,       0x19,       0x3f,  0 },
		{ QP_PMA, 0x6e0,          1,        0x1, 26 },
		{ QP_PMA, 0x6e0,          1,        0x1, 27 },

		/* EQ all */
		{ QP_PMA, 0x654,          1,        0x1, 19 },
		{ QP_PMA, 0x6b8,          2,      0x3ff, 16 },
		{ QP_ANA, 0x004,          0,        0x1, 10 },
		{ QP_PMA, 0x614,       0x10,       0xff,  0 },
		{ QP_PMA, 0x650,          1,        0x1,  1 },
		{ QP_ANA, 0x040,          1,        0x1,  3 },
		{ QP_ANA, 0x040,          1,        0x1,  4 },
		{ QP_PMA, 0x414,          0,        0x1,  4 },
		{ QP_PMA, 0x414,          1,        0x1,  5 },
		{ QP_ANA, 0x034,          0,        0x1, 14 },
		{ QP_ANA, 0x034,        0x4,        0x7, 25 },
		{ QP_ANA, 0x034,          1,        0x3, 23 },
		{ QP_ANA, 0x034,       0x12,       0x1f,  0 },
		{ QP_ANA, 0x034,        0xc,       0x1f,  9 },
		{ QP_PMA, 0x00c,          1,        0x7, 28 },

		/* Common setting */
		{ QP_DIG, 0x30c,          1,        0x1, 24 },
		{ QP_DIG, 0x30c,          1,        0x1, 25 },
		{ QP_PMA, 0x000,        0xa,       0x3f,  4 },
		{ QP_PMA, 0x000,        0x4,        0xf, 12 },
		{ QP_ANA, 0x02c,          0,        0x1, 25 },
		{ QP_ANA, 0x02c,          1,        0x1, 27 },
		{ QP_PMA, 0x41c,          1,        0x1,  0 },
		{ QP_PMA, 0x41c,          1,        0x1,  1 },
		{ QP_PMA, 0x41c,          1,        0x1,  2 },

		/* RX speed up */
		{ QP_PMA, 0x63c,       0x28,    0xfffff, 10 },
		{ QP_PMA, 0x640,       0x64,    0xfffff,  0 },
		{ QP_PMA, 0x644,     0x2710,    0xfffff,  0 },
		{ QP_PMA, 0x630,      0x9c4,     0x1fff, 16 },
		{ QP_PMA, 0x6dc,      0x9c4,     0x1fff,  0 },
		{ QP_PMA, 0x6f0,      0x9c4,     0x1fff, 13 },
		{ QP_PMA, 0x6f0,      0x9c4,     0x1fff,  0 },

		/* TCL avoid LT noise impact */
		{ QP_ANA, 0x028,          1,        0x1, 24 },

		/* Digital CLK trigger reverse */
		{ QP_PMA, 0x100,          0,        0x1, 12 },

		/* PLL auto init */
		{ QP_PMA, 0x400,          1,        0x1,  0 },
	};

	/* QPHY base init */
	qp_reg_batch(qp, QP_INIT_TBL, ARRAY_SIZE(QP_INIT_TBL));
}

static bool qp_read_kband(struct airoha_qphy *qp, u32 *val)
{
	static const struct qp_reg_entry QP_ICO_TBL[] = {
		{ QP_PMA, 0x410,    0,  0x1,  2 },
		{ QP_ANA, 0x030, 0x15, 0x3f, 21 },
		{ QP_ANA, 0x03c,    1,  0x3,  5 },
		{ QP_ANA, 0x03c,    1,  0x1, 19 },
		{ QP_PMA, 0x408,    1,  0x1, 29 },
		{ QP_PMA, 0x320,    0,  0x3, 11 },
		{ QP_PMA, 0x320,    0,  0x1, 13 },
		{ QP_PMA, 0x408,    0,  0x1, 14 },
	};
	u32 tmp1, tmp2;

	usleep_range(50000, 51000);

	/* Default ICO setting */
	qp_reg_batch(qp, QP_ICO_TBL, ARRAY_SIZE(QP_ICO_TBL));

	/* Read KBand */
	qp_ana_md(qp, 0x038, 0, 0x1, 19);
	usleep_range(5000, 6000);

	tmp1 = readl(qp->top[QP_PMA] + 0x364);
	tmp1 = (tmp1 >> 16) & 0xff;

	qp_ana_md(qp, 0x038, 1, 0x1, 19);
	usleep_range(5000, 6000);

	tmp2 = readl(qp->top[QP_PMA] + 0x364);
	tmp2 = (tmp2 >> 16) & 0xff;

	*val = (tmp1 << 4) | tmp2;

	if ((*val & 0x800) && (*val != 0xfff))
		return true;

	/* KBAND_DONE = 0 or KBAND_Code = 0xFFF */
	return false;
}

static inline u32 qp_reconfig_pll(struct airoha_qphy *qp)
{
	static const struct qp_reg_entry QP_CPR_TBL[] = {
		{ QP_PMA, 0x410,   1,  0x1,  2 },
		{ QP_ANA, 0x030,   0, 0x3f, 21 },
		{ QP_ANA, 0x03c,   3,  0x3,  5 },
		{ QP_ANA, 0x03c,   0,  0x1, 19 },
		{ QP_PMA, 0x408,   0,  0x1, 29 },
		{ QP_PMA, 0x320, 0x3,  0x3, 11 },
		{ QP_PMA, 0x320,   1,  0x1, 13 },
		{ QP_PMA, 0x408,   0,  0x1, 14 },
		{ QP_ANA, 0x03c,   1,  0x1, 14 },
		{ QP_ANA, 0x03c,   3,  0x3, 16 },
	};
	u32 i;

	for (i = 0; i < 50; i++) {
		u32 kband = 0;

		/* PLL disable */
		qp_pma_md(qp, 0x31c, 0, 0x1, 4);
		qp_pma_md(qp, 0x31c, 1, 0x1, 5);

		/* PLL config for CPR */
		qp_reg_batch(qp, QP_CPR_TBL, ARRAY_SIZE(QP_CPR_TBL));

		/* PLL force enable */
		qp_pma_md(qp, 0x31c, 1, 0x1, 4);
		qp_pma_md(qp, 0x31c, 1, 0x1, 5);

		if (qp_read_kband(qp, &kband))
			break;
	}

	return i + 1;
}

static inline void qp_enable_tx_pll(struct airoha_qphy *qp)
{
	static const struct qp_reg_entry QP_TX_PLL_TBL[] = {
		/* PLL reset to default */
		{ QP_PMA, 0x03c, 0, 0x1, 14 },
		{ QP_PMA, 0x03c, 0, 0x3, 16 },
		{ QP_PMA, 0x31c, 0, 0x1,  4 },
		{ QP_PMA, 0x31c, 0, 0x1,  5 },

		/* PLL force stable -> auto mode */
		{ QP_PMA, 0x418, 1, 0x1, 19 },
		{ QP_PMA, 0x418, 1, 0x1, 18 },
		{ QP_PMA, 0x418, 0, 0x1, 19 },
		{ QP_PMA, 0x418, 0, 0x1, 18 },

		/* TX force enable -> auto mode */
		{ QP_PMA, 0x318, 1, 0x1, 12 },
		{ QP_PMA, 0x318, 0, 0x1, 13 },
		{ QP_PMA, 0x318, 0, 0x1, 12 },
	};

	qp_reg_batch(qp, QP_TX_PLL_TBL, ARRAY_SIZE(QP_TX_PLL_TBL));
}

static int airoha_pcie_phy_init(struct phy *phy)
{
	struct airoha_qphy *qp = phy_get_drvdata(phy);
	u32 pcie_tx_term_sel = qp->pcie_tx_term_sel;
	u32 pcie_rx_imp_sel = qp->pcie_rx_imp_sel;
	u32 kband = 0;

	if (pcie_tx_term_sel == EFUSE_INVALID_VAL)
		pcie_tx_term_sel = 0x1;

	if (pcie_rx_imp_sel == EFUSE_INVALID_VAL)
		pcie_rx_imp_sel = 0x10;

	/* apply eFUSE data */
	qp_pma_md(qp, 0x320, pcie_tx_term_sel, 0x3, 5);
	qp_pma_md(qp, 0x320, 1, 0x1, 7);

	qp_pma_md(qp, 0x324, pcie_rx_imp_sel, 0x1f, 26);
	qp_pma_md(qp, 0x324, 1, 0x1, 31);

	/* TX force disable */
	qp_pma_md(qp, 0x318, 0, 0x1, 12);
	qp_pma_md(qp, 0x318, 1, 0x1, 13);

	/* PLL force unstable */
	qp_pma_md(qp, 0x418, 1, 0x1, 19);

	/* PLL disable */
	qp_pma_md(qp, 0x400, 0, 0x1, 0);
	usleep_range(1000, 2000);
	qp_pma_md(qp, 0x400, 1, 0x1, 0);

	qp_pma_md(qp, 0x31c, 0, 0x1, 4);
	qp_pma_md(qp, 0x31c, 1, 0x1, 5);

	udelay(1);

	qp_base_init(qp);

	/* PLL force enable */
	qp_pma_md(qp, 0x31c, 1, 0x1, 4);
	qp_pma_md(qp, 0x31c, 1, 0x1, 5);

	if (!qp_read_kband(qp, &kband)) {
		u32 elapsed = qp_reconfig_pll(qp);

		dev_warn(qp->dev, "PLL config for CPR: %u\n", elapsed);
	}

	qp_enable_tx_pll(qp);

	usleep_range(1000, 2000);

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

static void read_efuse_cal_data(struct airoha_qphy *qp)
{
	void *mem;
	u32 efuse_vaild;

	qp->pcie_tx_term_sel = EFUSE_INVALID_VAL;
	qp->pcie_rx_imp_sel  = EFUSE_INVALID_VAL;

	mem = airoha_efuse_load();
	if (!mem)
		return;

	efuse_vaild = airoha_efuse_get(mem, SDS_PCIe_efuse_valid_LSB, 3);
	if (efuse_vaild & BIT(1)) {
		qp->pcie_tx_term_sel = airoha_efuse_get(mem,
						DA_QP_PCIE_TX_TERM_SEL_LSB, 2);

		qp->pcie_rx_imp_sel = airoha_efuse_get(mem,
						DA_QP_PCIE_RX_IMP_SEL_LSB, 5);
	} else
		dev_warn(qp->dev, "PCIe QPHY eFuse not vaild\n");

	airoha_efuse_free(mem);
}

static void __iomem *phy_map_resource_byname(struct device *dev,
					     const char *name)
{
	struct device_node *np = dev->of_node;
	struct resource res;
	int index;

	index = of_property_match_string(np, "reg-names", name);
	if (index < 0 || of_address_to_resource(np, index, &res))
		return ERR_PTR(-ENODEV);

	return devm_ioremap_resource(dev, &res);
}

static int airoha_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct airoha_qphy *qp;

	qp = devm_kzalloc(dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	qp->top[QP_DIG] = phy_map_resource_byname(dev, "dig");
	if (IS_ERR(qp->top[QP_DIG])) {
		dev_err(dev, "failed to remap %s regs\n", "dig");
		return PTR_ERR(qp->top[QP_DIG]);
	}

	qp->top[QP_ANA] = phy_map_resource_byname(dev, "ana");
	if (IS_ERR(qp->top[QP_ANA])) {
		dev_err(dev, "failed to remap %s regs\n", "ana");
		return PTR_ERR(qp->top[QP_ANA]);
	}

	qp->top[QP_PMA] = phy_map_resource_byname(dev, "pma");
	if (IS_ERR(qp->top[QP_PMA])) {
		dev_err(dev, "failed to remap %s regs\n", "pma");
		return PTR_ERR(qp->top[QP_PMA]);
	}

	qp->phy = devm_phy_create(dev, dev->of_node, &airoha_pcie_phy_ops);
	if (IS_ERR(qp->phy)) {
		dev_err(dev, "Failed to create PCIe phy\n");
		return PTR_ERR(qp->phy);
	}

	qp->dev = dev;
	platform_set_drvdata(pdev, qp);
	phy_set_drvdata(qp->phy, qp);

	read_efuse_cal_data(qp);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_err(dev, "PCIe phy probe failed\n");
		return PTR_ERR(provider);
	}

	return 0;
}

static const struct of_device_id airoha_pcie_qphy_of_match[] = {
	{ .compatible = "airoha,an7583-pcie-qphy" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, airoha_pcie_qphy_of_match);

static struct platform_driver airoha_pcie_qphy_driver = {
	.probe	= airoha_pcie_phy_probe,
	.driver	= {
		.name = "airoha-pcie-qphy",
		.of_match_table = airoha_pcie_qphy_of_match,
	},
};

module_platform_driver(airoha_pcie_qphy_driver);

MODULE_DESCRIPTION("Airoha PCIe Gen2 QPHY driver");
MODULE_LICENSE("GPL");
