// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 */

#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

/* PCIE phy lane switch control */
#define MT7987_PHY_LANE_MODE_MASK	BIT(5)
#define MT7987_RG_XTP_LN_SYNC_CK_SEL0	0x088    /* Offset from PEXTP base */
#define MT7987_RG_XTP_LN_SYNC_CK_SEL1	0x188    /* Offset from PEXTP base */
#define MT7987_XTP_SYNC_MASK		GENMASK(3, 2)
#define MT7987_XTP_SYNC_1PORT_2LANE	FIELD_PREP(MT7987_XTP_SYNC_MASK, 1)
#define MT7987_XTP_SYNC_2PORT_1LANE	FIELD_PREP(MT7987_XTP_SYNC_MASK, 0)
#define MT7987_LANE_1PORT_2LANE		0
#define MT7987_LANE_2PORT_1LANE		BIT(5)

/**
 * struct mtk_pcie_phy_data - phy data for each SoC
 * @num_lanes: supported lane numbers
 * @sw_efuse_supported: support software to load eFuse data
 * @lane_switch_supported: support switching between 2-lane and 1-lane
 */
struct mtk_pcie_phy_data {
	int num_lanes;
	bool lane_switch_supported;
};

/**
 * enum mtk_pcie_lane_mode - PCIe lane switch mode enumeration
 * @MTK_PCIE_LANE_DEFAULT: Default lane mode (bootloader/hw default)
 * @MTK_PCIE_LANE_1PORT_2LANE: 1-port, 2-lane mode
 * @MTK_PCIE_LANE_2PORT_1LANE: 2-port, 1-lane mode
 */
enum mtk_pcie_lane_mode {
	MTK_PCIE_LANE_DEFAULT = 0,
	MTK_PCIE_LANE_1PORT_2LANE,
	MTK_PCIE_LANE_2PORT_1LANE,
};

/**
 * struct mtk_pcie_lane_switch - PCIe lane switch configuration
 * @mode: Lane switch mode (see mtk_pcie_lane_mode)
 * @topmisc: Regmap for TOP_MISC register access
 * @topmisc_offset: Offset for lane mode register in TOP_MISC
 */
struct mtk_pcie_lane_switch {
	/* Default initialization in probe */
	int mode;
	struct regmap *topmisc;
	u32 topmisc_offset;
};

/**
 * struct mtk_pcie_phy - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @sif_base: IO mapped register base address of system interface
 * @data: pointer to SoC dependent data
 * @sw_efuse_en: software eFuse enable status
 * @efuse_glb_intr: internal resistor selection of TX bias current data
 * @efuse: pointer to eFuse data for each lane
 */
struct mtk_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *sif_base;
	const struct mtk_pcie_phy_data *data;

	struct mtk_pcie_lane_switch lane_switch;
};

static void mtk_phy_update_bits(void __iomem *reg, u32 mask, u32 val)
{
	u32 tmp = readl(reg);

	tmp &= ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

/**
 * mtk_pcie_phy_init() - Initialize the phy
 * @phy: the phy to be initialized
 *
 * Initialize the phy by setting the efuse data.
 * The hardware settings will be reset during suspend, it should be
 * reinitialized when the consumer calls phy_init() again on resume.
 */
static int mtk_pcie_phy_init(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret;
	u32 val;

	if (pcie_phy->data->lane_switch_supported &&
	    pcie_phy->lane_switch.mode != MTK_PCIE_LANE_DEFAULT) {
		/* Configure lane mode in TOP_MISC */
		if (!pcie_phy->lane_switch.topmisc)
			return -EINVAL;

		val = (pcie_phy->lane_switch.mode == MTK_PCIE_LANE_2PORT_1LANE) ?
			MT7987_LANE_2PORT_1LANE : MT7987_LANE_1PORT_2LANE;
		ret = regmap_update_bits(pcie_phy->lane_switch.topmisc,
					 pcie_phy->lane_switch.topmisc_offset,
					 MT7987_PHY_LANE_MODE_MASK, val);
		if (ret)
			return ret;

		/* Configure PEXTP sync clock selection */
		val = (pcie_phy->lane_switch.mode == MTK_PCIE_LANE_2PORT_1LANE) ?
			MT7987_XTP_SYNC_2PORT_1LANE : MT7987_XTP_SYNC_1PORT_2LANE;
		mtk_phy_update_bits(pcie_phy->sif_base + MT7987_RG_XTP_LN_SYNC_CK_SEL0,
				   MT7987_XTP_SYNC_MASK, val);
		mtk_phy_update_bits(pcie_phy->sif_base + MT7987_RG_XTP_LN_SYNC_CK_SEL1,
				   MT7987_XTP_SYNC_MASK, val);
	}

	return 0;
}

static const struct phy_ops mtk_pcie_phy_ops = {
	.init	= mtk_pcie_phy_init,
	.owner	= THIS_MODULE,
};

static int mtk_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct mtk_pcie_phy *pcie_phy;
	struct resource *res;
	int ret;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sif");
	pcie_phy->sif_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie_phy->sif_base)) {
		dev_err(dev, "Failed to map %s base\n", "phy-sif");
		return PTR_ERR(pcie_phy->sif_base);
	}

	pcie_phy->dev = dev;
	pcie_phy->data = of_device_get_match_data(dev);
	if (!pcie_phy->data) {
		dev_err(dev, "Failed to get phy data\n");
		return -EINVAL;
	}

	if (pcie_phy->data->lane_switch_supported) {
		struct of_phandle_args args;

		/* Get syscon regmap for TOP_MISC */
		ret = of_parse_phandle_with_fixed_args(dev->of_node,
						       "mediatek,syscon-type",
						       1, 0, &args);
		if (ret == 0) {
			pcie_phy->lane_switch.topmisc = syscon_node_to_regmap(args.np);
			of_node_put(args.np);
			if (IS_ERR(pcie_phy->lane_switch.topmisc))
				return PTR_ERR(pcie_phy->lane_switch.topmisc);

			pcie_phy->lane_switch.topmisc_offset = args.args[0];
		}

		/* Get lane mode configuration from device tree */
		if (of_property_read_bool(dev->of_node, "mediatek,2port-1lane"))
			pcie_phy->lane_switch.mode = MTK_PCIE_LANE_2PORT_1LANE;
		else if (of_property_read_bool(dev->of_node, "mediatek,1port-2lane"))
			pcie_phy->lane_switch.mode = MTK_PCIE_LANE_1PORT_2LANE;
		else
			pcie_phy->lane_switch.mode = MTK_PCIE_LANE_DEFAULT;
	}

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &mtk_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_err(dev, "Failed to create PCIe phy\n");
		return PTR_ERR(pcie_phy->phy);
	}

	platform_set_drvdata(pdev, pcie_phy);
	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_err(dev, "PCIe phy probe failed\n");
		return PTR_ERR(provider);
	}

	return 0;
}

static const struct mtk_pcie_phy_data mt7987_data = {
	.num_lanes = 2,
	.lane_switch_supported = true,
};

static const struct of_device_id mtk_pcie_phy_of_match[] = {
	{ .compatible = "mediatek,mt7987-pcie-phy", .data = &mt7987_data },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_pcie_phy_of_match);

static struct platform_driver mtk_pcie_phy_driver = {
	.probe	= mtk_pcie_phy_probe,
	.driver	= {
		.name = "mtk-pcie-phy",
		.of_match_table = mtk_pcie_phy_of_match,
	},
};
module_platform_driver(mtk_pcie_phy_driver);

MODULE_DESCRIPTION("MediaTek PCIe PHY driver");
MODULE_AUTHOR("Jianjun Wang <jianjun.wang@mediatek.com>");
MODULE_LICENSE("GPL");
