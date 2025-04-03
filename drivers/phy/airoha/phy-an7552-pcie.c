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

#define AN7552_PCIE_PHY_NUM	2

/**
 * struct airoha_pcie_phy - PCIe phy driver main structure
 * @dev: pointer to device
 * @phy: pointer to generic phy
 * @pc[]: IO mapped register base address
 */
struct airoha_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *pc[AN7552_PCIE_PHY_NUM];
};

/**
 * airoha_pcie_phy_init() - Initialize the phy
 * @phy: the phy to be initialized
 */
static int airoha_pcie_phy_init(struct phy *phy)
{
	struct airoha_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	u32 i;

	for (i = 0; i < AN7552_PCIE_PHY_NUM; i++) {
		writel(0x0c010000, pcie_phy->pc[i] + 0x418);
		writel(0x60080007, pcie_phy->pc[i] + 0x404);
		writel(0x04100803, pcie_phy->pc[i] + 0x318);
	}

	usleep_range(1000, 1100);

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

static int airoha_pcie_phy_probe(struct platform_device *pdev)
{
	struct airoha_pcie_phy *pcie_phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct phy_provider *provider;
	u32 i;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	for (i = 0; i < AN7552_PCIE_PHY_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		pcie_phy->pc[i] = devm_ioremap_resource(dev, res);
		if (IS_ERR(pcie_phy->pc[i])) {
			dev_err(dev, "Failed to map PHY%u base\n", i);
			return PTR_ERR(pcie_phy->pc[i]);
		}
	}

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &airoha_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_err(dev, "Failed to create PCIe phy\n");
		return PTR_ERR(pcie_phy->phy);
	}

	pcie_phy->dev = dev;
	platform_set_drvdata(pdev, pcie_phy);
	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider)) {
		dev_err(dev, "PCIe phy probe failed\n");
		return PTR_ERR(provider);
	}

	return 0;
}

static const struct of_device_id airoha_pcie_phy_of_match[] = {
	{ .compatible = "airoha,an7552-pcie-phy" },
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

MODULE_DESCRIPTION("Airoha PCIe Gen2 PHY driver");
MODULE_LICENSE("GPL");
