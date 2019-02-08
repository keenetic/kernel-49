#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/rt2880/rt_mmap.h>

#include "ralink-flash.h"

int ra_check_flash_type(void)
{
	int boot_from = BOOT_FROM_SPI;
	uint8_t Id[8];
	uint32_t chip_mode;

	memset(Id, 0, sizeof(Id));
	strncpy(Id, (char *)RALINK_SYSCTL_BASE, 6);
	chip_mode = *((volatile u32 *)(RALINK_SYSCTL_BASE + 0x10));
	chip_mode &= 0x0f;
#if defined(CONFIG_RALINK_MT7621)
	if (strcmp(Id, "MT7621") == 0) {
		switch (chip_mode) {
		case 0x00:
		case 0x02:
		case 0x03:
			boot_from = BOOT_FROM_SPI;
			break;
		case 0x01:
		case 0x0a:
		case 0x0b:
		case 0x0c:
			boot_from = BOOT_FROM_NAND;
			break;
		}
	} else
#elif defined(CONFIG_RALINK_MT7628)
	if (strcmp(Id, "MT7628") == 0)
		boot_from = BOOT_FROM_SPI;
	else
#endif
		printk(KERN_ERR "%s: %s is not supported\n", __func__, Id);

	return boot_from;
}

