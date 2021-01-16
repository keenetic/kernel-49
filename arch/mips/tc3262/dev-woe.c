#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/tc3162/rt_mmap.h>

#include <soc/ralink/dev_woe.h>

static struct resource en75xx_woe_resources[] = {
	[0] = {
		.start  = RALINK_WOE0_BASE,
		.end    = RALINK_WOE0_BASE + 0xfff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = RALINK_WOE1_BASE,
		.end    = RALINK_WOE1_BASE + 0xfff,
		.flags  = IORESOURCE_MEM,
	},
	[2] = {
		.start  = WOE0_INTR,
		.flags  = IORESOURCE_IRQ,
	},
	[3] = {
		.start  = WOE1_INTR,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct whnat_mtk_pdata en75xx_woe_pdata = {
	.wdma_base = {
		RALINK_WDMA0_BASE,
		RALINK_WDMA1_BASE
	},
	.wdma_irq = {
		{ WDMA0_P0_INTR, WDMA0_P1_INTR, WDMA0_WOE_INTR },
		{ WDMA1_P0_INTR, WDMA1_P1_INTR, WDMA1_WOE_INTR }
	},
};

static u64 en75xx_woe_dmamask = DMA_BIT_MASK(32);

static struct platform_device en75xx_woe_device = {
	.name		= WHNAT_MTK_DRV_NAME,
	.id		= -1,
	.dev		= {
		.platform_data = &en75xx_woe_pdata,
		.dma_mask = &en75xx_woe_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources	= ARRAY_SIZE(en75xx_woe_resources),
	.resource	= en75xx_woe_resources,
};

int __init init_en75xx_woe(void)
{
	int retval = 0;

	retval = platform_device_register(&en75xx_woe_device);
	if (retval != 0) {
		printk(KERN_ERR "register %s device fail!\n", "WoE");
		return retval;
	}

	return retval;
}

device_initcall(init_en75xx_woe);
