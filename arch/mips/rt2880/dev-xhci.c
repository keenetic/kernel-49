
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/irq.h>
#include <asm/rt2880/rt_mmap.h>

#include <soc/ralink/dev_xhci.h>

static struct resource mt7621_xhci_resources[] = {
	[0] = {
		.start  = RALINK_USB_HOST_BASE,
		.end    = RALINK_USB_HOST_BASE + RALINK_USB_HOST_SIZE - 1,
		.flags  = IORESOURCE_MEM,
		.name   = "mac",
	},
	[1] = {
		.start  = RALINK_USB_IPPC_BASE,
		.end    = RALINK_USB_IPPC_BASE + 0xff,
		.flags  = IORESOURCE_MEM,
		.name   = "ippc",
	},
	[2] = {
		.start  = SURFBOARDINT_USB,
		.flags  = IORESOURCE_IRQ,
	},
};

static u64 mt7621_xhci_dmamask = DMA_BIT_MASK(32);

extern void uphy_init(void);

static struct xhci_mtk_pdata mt7621_xhci_pdata = {
	.uphy_init		= uphy_init,
	.usb3_lpm_capable	= true,
};

static struct platform_device mt7621_xhci_device = {
	.name		= XHCI_MTK_DRV_NAME,
	.id		= -1,
	.dev		= {
		.platform_data = &mt7621_xhci_pdata,
		.dma_mask = &mt7621_xhci_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources	= ARRAY_SIZE(mt7621_xhci_resources),
	.resource	= mt7621_xhci_resources,
};

int __init init_mt7621_xhci(void)
{
	int retval = 0;

	retval = platform_device_register(&mt7621_xhci_device);
	if (retval != 0) {
		printk(KERN_ERR "register %s device fail!\n", "xHCI");
		return retval;
	}

	return retval;
}

device_initcall(init_mt7621_xhci);
