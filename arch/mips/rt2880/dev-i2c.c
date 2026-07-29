#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/rt2880/rt_mmap.h>

#include <soc/ralink/dev_i2c.h>

static u64 mt7621_i2c_dmamask = DMA_BIT_MASK(32);

static struct resource mt7621_i2c_resources[] = {
	{
		.start = RALINK_I2C_BASE,
		.end   = RALINK_I2C_BASE + 0xff,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = SURFBOARDINT_I2C,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mtk_i2c_platform_data mt7621_i2c_pdata = {
	.bus_freq = 400000,
	.clk_src  = 50000000,
};

static struct platform_device mt7621_i2c_device = {
	.name		 = MTK_I2C_DRV_NAME,
	.id		 = -1,
	.dev = {
		.platform_data = &mt7621_i2c_pdata,
		.dma_mask = &mt7621_i2c_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources	 = ARRAY_SIZE(mt7621_i2c_resources),
	.resource	 = mt7621_i2c_resources,
};

int __init mtk_i2c_register(void)
{
	int ret = 0;

	ret = platform_device_register(&mt7621_i2c_device);
	if (ret != 0) {
		printk(KERN_ERR "register %s device fail!\n", "I2C");
		return ret;
	}

	return ret;
}

arch_initcall(mtk_i2c_register);
