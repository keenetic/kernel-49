// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/arm-smccc.h>

#include <soc/airoha/efuse.h>

#define DEV_NAME		"airoha_efuse"

#define SMC_EFUSE_FID		0x82000001
#define SMC_EFUSE_SUB_ID_READ	0x44414552
#define SMC_EFUSE_BUF_SIZE	256

#if defined(CONFIG_64BIT) && defined(CONFIG_ZONE_DMA32)
#define GFP_ZONE_DMA32		GFP_DMA32
#elif defined(CONFIG_64BIT) && defined(CONFIG_ZONE_DMA)
#define GFP_ZONE_DMA32		GFP_DMA
#else
#define GFP_ZONE_DMA32		0
#endif

static struct platform_device *efuse_pdev;

static inline unsigned long efuse_smc_call(unsigned long efuse_phys)
{
#ifdef __aarch64__
	struct arm_smccc_res res;

	arm_smccc_smc(SMC_EFUSE_FID, SMC_EFUSE_SUB_ID_READ, efuse_phys,
		      SMC_EFUSE_BUF_SIZE, 0, 0, 0, 0, &res);

	return res.a0;
#else
	return 1;
#endif
}

void *airoha_efuse_load(void)
{
	const u32 efuse_size = PAGE_ALIGN(SMC_EFUSE_BUF_SIZE);
	dma_addr_t efuse_phys;
	struct device *dev;
	unsigned long status;
	void *mem;

	if (efuse_pdev == NULL)
		return NULL;

	dev = &efuse_pdev->dev;

	mem = kzalloc(efuse_size, GFP_KERNEL | GFP_ZONE_DMA32);
	if (!mem)
		return NULL;

	efuse_phys = dma_map_single(dev, mem, efuse_size, DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, efuse_phys)) {
		kfree(mem);
		return NULL;
	}

	status = efuse_smc_call(efuse_phys);

	dma_unmap_single(dev, efuse_phys, efuse_size, DMA_FROM_DEVICE);

	if (status) {
		dev_err(dev, "eFuse load data failed, status: 0x%lx\n", status);
		kfree(mem);
		mem = NULL;
	}

	return mem;
}
EXPORT_SYMBOL(airoha_efuse_load);

u32 airoha_efuse_get(const void *mem, u32 start_bit, u32 len)
{
	u32 bit;
	u32 data = 0;
	u8 *pdata = (u8 *)&data;
	const u8 *buf = (const u8 *)mem;

	if (!mem)
		return EFUSE_INVALID_VAL;

	if (len < 1 || len > 32 || (start_bit + len) > SMC_EFUSE_BUF_SIZE * 8)
		return EFUSE_INVALID_VAL;

	for (bit = 0; bit < len; bit++) {
		u32 ofs = start_bit + bit;
		u32 d8_idx = bit >> 3;		/* bit / 8 */
		u32 d1_idx = bit & 0x7;		/* bit % 8 */
		u32 e8_idx = ofs >> 3;		/* ofs / 8 */
		u32 e1_idx = ofs & 0x7;		/* ofs % 8 */

		pdata[d8_idx] |= (((buf[e8_idx] >> e1_idx) & 0x1) << d1_idx);
	}

	return data;
}
EXPORT_SYMBOL(airoha_efuse_get);

void airoha_efuse_free(void *mem)
{
	kfree(mem);
}
EXPORT_SYMBOL(airoha_efuse_free);

static int __init airoha_efuse_init(void)
{
	efuse_pdev = platform_device_register_simple(DEV_NAME, -1, NULL, 0);
	if (IS_ERR(efuse_pdev)) {
		pr_err("failed to register %s device\n", DEV_NAME);
		return PTR_ERR(efuse_pdev);
	}

	arch_setup_dma_ops(&efuse_pdev->dev, 0, 0, NULL, false);

	dma_set_mask(&efuse_pdev->dev, DMA_BIT_MASK(32));

	return 0;
}

subsys_initcall(airoha_efuse_init);
