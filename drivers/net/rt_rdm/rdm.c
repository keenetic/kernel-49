#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "rdm.h"

#ifdef CONFIG_MIPS
#include <asm/rt2880/rt_mmap.h>
#endif

#ifdef CONFIG_ARCH_MEDIATEK
#include <linux/io.h>

struct bus_desc {
	void __iomem *base_map;
	u32 base;
	u32 size;
};

#if defined(CONFIG_MACH_MT7622)
#include "rdm_mt7622.h"

static struct bus_desc mtk_bus_desc[] = {
	{ NULL, MEDIATEK_TOPRGU_BASE, MEDIATEK_TOPRGU_SIZE },
	{ NULL, MEDIATEK_PHRSYS_BASE, MEDIATEK_PHRSYS_SIZE },
	{ NULL, MEDIATEK_WBSYS_BASE,  MEDIATEK_WBSYS_SIZE  },
	{ NULL, MEDIATEK_HIFSYS_BASE, MEDIATEK_HIFSYS_SIZE },
	{ NULL, MEDIATEK_HIFAXI_BASE, MEDIATEK_HIFAXI_SIZE },
	{ NULL, MEDIATEK_ETHSYS_BASE, MEDIATEK_ETHSYS_SIZE },
};

#else

#error "please define MACH for CONFIG_ARCH_MEDIATEK"

#endif
#endif /* CONFIG_ARCH_MEDIATEK */

#define RDM_SYSCTL_ADDR		RALINK_SYSCTL_BASE // system control
#define RDM_WIRELESS_ADDR	RALINK_11N_MAC_BASE // wireless control
#define RDM_DEVNAME		"rdm0"
#define RDM_MAJOR		223

static int rdm_major = RDM_MAJOR;

static unsigned long register_control = RDM_SYSCTL_ADDR;
static u32 register_bus = RDM_SYSCTL_ADDR;

static long rdm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long baseaddr, addr = 0;
	u32 rtvalue, offset, count = 0;
#ifdef CONFIG_ARCH_MEDIATEK
	size_t i;
#endif

	baseaddr = register_control;
	if (cmd == RT_RDM_CMD_SHOW) {
		offset = *(u32 *)arg;
		rtvalue = (u32)(*(volatile u32 *)(baseaddr + offset));
		printk("0x%x\n", rtvalue);
	} else if (cmd == RT_RDM_CMD_DUMP)  {
		for (count=0; count < RT_RDM_DUMP_RANGE ; count++) {
			offset = *(u32 *)arg + (count * 16);
			addr = baseaddr + offset;
			printk("%08X: ", register_bus + offset);
			printk("%08X %08X %08X %08X\n",
				(u32)(*(volatile u32 *)(addr)),
				(u32)(*(volatile u32 *)(addr+4)),
				(u32)(*(volatile u32 *)(addr+8)),
				(u32)(*(volatile u32 *)(addr+12)));
		}
	} else if (cmd == RT_RDM_CMD_READ) {
		offset = *(u32 *)arg;
		rtvalue = (u32)(*(volatile u32 *)(baseaddr + offset));
		put_user(rtvalue, (int __user *)arg);
	} else if (cmd == RT_RDM_CMD_SET_BASE_SYS) {
#ifdef CONFIG_ARCH_MEDIATEK
		register_control = (unsigned long)mtk_bus_desc[ETHSYS].base_map;
#else
		register_control = RDM_SYSCTL_ADDR;
#endif
		register_bus = RDM_SYSCTL_ADDR;
		printk("switch register base addr to system register 0x%08x\n", register_bus);
	} else if (cmd == RT_RDM_CMD_SET_BASE_WLAN) {
#ifdef CONFIG_ARCH_MEDIATEK
		register_control = (unsigned long)mtk_bus_desc[WBSYS].base_map;
#else
		register_control = RDM_WIRELESS_ADDR;
#endif
		register_bus = RDM_WIRELESS_ADDR;
		printk("switch register base addr to wireless register 0x%08x\n", register_bus);
	} else if (cmd == RT_RDM_CMD_SHOW_BASE) {
		printk("current register base addr is 0x%08x\n", register_bus);
	} else if (cmd == RT_RDM_CMD_SET_BASE) {
		register_bus = *(u32 *)arg;
#ifdef CONFIG_ARCH_MEDIATEK
		offset = 0;

		if (register_bus < mtk_bus_desc[0].base)
			register_bus = mtk_bus_desc[0].base;

		for (i = ARRAY_SIZE(mtk_bus_desc) - 1; i >= 0; i--) {
			struct bus_desc *bd = &mtk_bus_desc[i];

			if (register_bus >= bd->base) {
				offset = register_bus - bd->base;
				if (offset > bd->size - 4)
					offset = bd->size - 4;
				register_control = (unsigned long)(bd->base_map + offset);
				break;
			}
		}
#else
		register_control = (unsigned long)register_bus;
#endif
		printk("switch register base addr to 0x%08x\n", register_bus);
	} else if (((cmd & 0xffff) == RT_RDM_CMD_WRITE) || ((cmd & 0xffff) == RT_RDM_CMD_WRITE_SILENT)) {
		offset = cmd >> 16;
		*(volatile u32 *)(baseaddr + offset) = *(u32 *)arg;
		if ((cmd & 0xffff) == RT_RDM_CMD_WRITE)
			printk("write offset 0x%x, value 0x%x\n", offset, *(u32 *)arg);
	} else {
		return -EOPNOTSUPP;
	}

	return 0;
}

static int rdm_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);
	return 0;
}

static int rdm_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations rdm_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= rdm_ioctl,
	.open		= rdm_open,
	.release	= rdm_release,
};

static void free_bus_resources(void)
{
#ifdef CONFIG_ARCH_MEDIATEK
	size_t i;

	/* iounmap registers */
	for (i = 0; i < ARRAY_SIZE(mtk_bus_desc); i++) {
		struct bus_desc *bd = &mtk_bus_desc[i];

		if (bd->base_map) {
			iounmap(bd->base_map);
			bd->base_map = NULL;
		}
	}
#endif
}

int __init rdm_init(void)
{
	int result;
#ifdef CONFIG_ARCH_MEDIATEK
	size_t i;

	/* iomap registers */
	for (i = 0; i < ARRAY_SIZE(mtk_bus_desc); i++) {
		struct bus_desc *bd = &mtk_bus_desc[i];

		bd->base_map = ioremap(bd->base, bd->size);
		if (!bd->base_map) {
			free_bus_resources();
			return -ENOMEM;
		}
	}

	register_control = (unsigned long)mtk_bus_desc[ETHSYS].base_map;
#endif

	result = register_chrdev(rdm_major, RDM_DEVNAME, &rdm_fops);
	if (result < 0) {
		printk(KERN_WARNING "ps: can't get major %d\n", rdm_major);
		free_bus_resources();
		return result;
	}

	if (rdm_major == 0)
		rdm_major = result; /* dynamic */

	return 0;
}

void __exit rdm_exit(void)
{
	free_bus_resources();

	unregister_chrdev(rdm_major, RDM_DEVNAME);
}

module_init(rdm_init);
module_exit(rdm_exit);

module_param (rdm_major, int, 0);

MODULE_LICENSE("GPL");
