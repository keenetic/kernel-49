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

#if defined(CONFIG_MACH_MT7622)
#include <linux/io.h>

#define MEDIATEK_TOPRGU_BASE	0x10000000
#define MEDIATEK_TOPRGU_SIZE	0x00480000

#define MEDIATEK_PHRSYS_BASE	0x11000000
#define MEDIATEK_PHRSYS_SIZE	0x002B0000

#define MEDIATEK_WBSYS_BASE	0x18000000
#define MEDIATEK_WBSYS_SIZE	0x00100000

#define MEDIATEK_HIFSYS_BASE	0x1A000000
#define MEDIATEK_HIFSYS_SIZE	0x00250000

#define MEDIATEK_ETHSYS_BASE	0x1B000000
#define MEDIATEK_ETHSYS_SIZE	0x00130000

#define RALINK_SYSCTL_BASE	MEDIATEK_ETHSYS_BASE
#define RALINK_11N_MAC_BASE	MEDIATEK_WBSYS_BASE

static void __iomem *toprgu_reg_base;
static void __iomem *phrsys_reg_base;
static void __iomem *wbsys_reg_base;
static void __iomem *hifsys_reg_base;
static void __iomem *ethsys_reg_base;
#endif

#define RDM_SYSCTL_ADDR		RALINK_SYSCTL_BASE // system control
#define RDM_WIRELESS_ADDR	RALINK_11N_MAC_BASE // wireless control
#define RDM_DEVNAME		"rdm0"
#define RDM_MAJOR		223

static unsigned long register_control = RDM_SYSCTL_ADDR;
static int rdm_major = RDM_MAJOR;

static long rdm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long baseaddr, offset, addr = 0;
	u32 rtvalue, count = 0;

	baseaddr = register_control;
	if (cmd == RT_RDM_CMD_SHOW) {
		rtvalue = (u32)(*(volatile u32 *)(baseaddr + (*(int *)arg)));
		printk("0x%x\n", rtvalue);
	} else if (cmd == RT_RDM_CMD_DUMP)  {
		for (count=0; count < RT_RDM_DUMP_RANGE ; count++) {
			addr = baseaddr + (*(int *)arg) + (count * 16);
			printk("%08lX: ", addr);
			printk("%08X %08X %08X %08X\n",
				(u32)(*(volatile u32 *)(addr)),
				(u32)(*(volatile u32 *)(addr+4)),
				(u32)(*(volatile u32 *)(addr+8)),
				(u32)(*(volatile u32 *)(addr+12)));
		}
	} else if (cmd == RT_RDM_CMD_DUMP_FPGA_EMU) {
		for (count=0; count < RT_RDM_DUMP_RANGE ; count++) {
			addr = baseaddr + (*(int *)arg) + (count*16);
			printk("this.cpu_gen.set_reg32('h%08lX,'h%08X);\n", addr, (u32)(*(volatile u32 *)(addr)));
			printk("this.cpu_gen.set_reg32('h%08lX,'h%08X);\n", addr+4, (u32)(*(volatile u32 *)(addr+4)));
			printk("this.cpu_gen.set_reg32('h%08lX,'h%08X);\n", addr+8, (u32)(*(volatile u32 *)(addr+8)));
			printk("this.cpu_gen.set_reg32('h%08lX,'h%08X);\n", addr+12, (u32)(*(volatile u32 *)(addr+12)));
		}
	} else if (cmd == RT_RDM_CMD_READ) {
		rtvalue = (u32)(*(volatile u32 *)(baseaddr + (*(int *)arg)));
		put_user(rtvalue, (int __user *)arg);
	} else if (cmd == RT_RDM_CMD_SET_BASE_SYS) {
#if defined(CONFIG_MACH_MT7622)
		register_control = (unsigned long)ethsys_reg_base;
#else
		register_control = RDM_SYSCTL_ADDR;
#endif
		printk("switch register base addr to system register 0x%x\n",RALINK_SYSCTL_BASE);
	} else if (cmd == RT_RDM_CMD_SET_BASE_WLAN) {
#if defined(CONFIG_MACH_MT7622)
		register_control = (unsigned long)wbsys_reg_base;
#else
		register_control = RDM_WIRELESS_ADDR;
#endif
		printk("switch register base addr to wireless register 0x%08x\n", RDM_WIRELESS_ADDR);
	} else if (cmd == RT_RDM_CMD_SHOW_BASE) {
		printk("current register base addr is 0x%08lx\n", register_control);
	} else if (cmd == RT_RDM_CMD_SET_BASE) {
		register_control = (*(unsigned long *)arg);
		printk("switch register base addr to 0x%08lx\n", register_control);
#if defined(CONFIG_MACH_MT7622)
		if (register_control > 0xffffff0000000000ull) {
			printk("This is virtual address\n");
		} else if (register_control >= MEDIATEK_ETHSYS_BASE) {
			offset = register_control - MEDIATEK_ETHSYS_BASE;
			register_control = offset + (unsigned long)ethsys_reg_base;
		} else if (register_control >= MEDIATEK_HIFSYS_BASE) {
			offset = register_control - MEDIATEK_HIFSYS_BASE;
			register_control = offset + (unsigned long)hifsys_reg_base;
		} else if (register_control >= MEDIATEK_WBSYS_BASE) {
			offset = register_control - MEDIATEK_WBSYS_BASE;
			register_control = offset + (unsigned long)wbsys_reg_base;
		} else if (register_control >= MEDIATEK_PHRSYS_BASE) {
			offset = register_control - MEDIATEK_PHRSYS_BASE;
			register_control = offset + (unsigned long)phrsys_reg_base;
		} else {
			offset = register_control - MEDIATEK_TOPRGU_BASE;
			register_control = offset + (unsigned long)toprgu_reg_base;
		}
#endif
	} else if (((cmd & 0xffff) == RT_RDM_CMD_WRITE) || ((cmd & 0xffff) == RT_RDM_CMD_WRITE_SILENT)) {
		offset = cmd >> 16;
		*(volatile u32 *)(baseaddr + offset) = (u32)((*(int *)arg));
		if ((cmd & 0xffff) == RT_RDM_CMD_WRITE)
			printk("write offset 0x%lx, value 0x%x\n", offset, (u32)(*(int *)arg));
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

int __init rdm_init(void)
{
	int result = register_chrdev(rdm_major, RDM_DEVNAME, &rdm_fops);

	if (result < 0) {
		printk(KERN_WARNING "ps: can't get major %d\n",rdm_major);
		return result;
	}

	if (rdm_major == 0)
		rdm_major = result; /* dynamic */

#if defined(CONFIG_MACH_MT7622)
	/* iomap registers */
	toprgu_reg_base = ioremap(MEDIATEK_TOPRGU_BASE, MEDIATEK_TOPRGU_SIZE);
	phrsys_reg_base = ioremap(MEDIATEK_PHRSYS_BASE, MEDIATEK_PHRSYS_SIZE);
	wbsys_reg_base  = ioremap(MEDIATEK_WBSYS_BASE,  MEDIATEK_WBSYS_SIZE);
	hifsys_reg_base = ioremap(MEDIATEK_HIFSYS_BASE, MEDIATEK_HIFSYS_SIZE);
	ethsys_reg_base = ioremap(MEDIATEK_ETHSYS_BASE, MEDIATEK_ETHSYS_SIZE);

	register_control = (unsigned long)ethsys_reg_base;
#endif

	return 0;
}

void __exit rdm_exit(void)
{
#if defined(CONFIG_MACH_MT7622)
	/* iounmap registers */
	iounmap((void *)toprgu_reg_base);
	iounmap((void *)phrsys_reg_base);
	iounmap((void *)wbsys_reg_base);
	iounmap((void *)hifsys_reg_base);
	iounmap((void *)ethsys_reg_base);
#endif

	unregister_chrdev(rdm_major, RDM_DEVNAME);
}

module_init(rdm_init);
module_exit(rdm_exit);

module_param (rdm_major, int, 0);

MODULE_LICENSE("GPL");
