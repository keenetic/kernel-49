#include <linux/init.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/cpu.h>
#include <asm/cpu-info.h>
#include <asm/bootinfo.h>

#include <asm/reboot.h>
#include <asm/time.h>

#include <asm/tc3162/tc3162.h>
#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
struct sk_buff;
#include <asm/tc3162/TCIfSetQuery_os.h>
#endif

extern void tc_disable_irq_all(void);

#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
adsldev_ops *adsl_dev_ops = NULL;
EXPORT_SYMBOL(adsl_dev_ops);

void stop_dsl_dmt(void)
{
	/* stop a DMT module */
	if (adsl_dev_ops)
		adsl_dev_ops->set(ADSL_SET_DMT_CLOSE, NULL, NULL);
}
#endif

static inline void hw_uninit(void)
{
#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
	/* stop adsl */
	stop_dsl_dmt();
#endif

	/* stop each module dma task */
	tc_disable_irq_all();
	VPint(CR_TIMER_CTL) = 0x0;

	/* stop atm sar dma */
	TSARM_GFR &= ~((1 << 1) | (1 << 0));

	/* reset USB */
	/* reset USB DMA */
	VPint(CR_USB_SYS_CTRL_REG) |= (1U << 31);
	/* reset USB SIE */
	VPint(CR_USB_DEV_CTRL_REG) |= (1 << 30);
	mdelay(5);

	/* restore USB SIE */
	VPint(CR_USB_DEV_CTRL_REG) &= ~(1 << 30);
	mdelay(5);
	VPint(CR_USB_SYS_CTRL_REG) &= ~(1U << 31);
}

static void hw_reset(bool do_reboot)
{
	hw_uninit();

	if (do_reboot) {
		/* system software reset */
		VPint(CR_AHB_RSTCR) |= (1U << 31);
	}
}

static void tc_machine_restart(char *command)
{
	printk(KERN_WARNING "Machine restart ... \n");
	hw_reset(true);
}

static void tc_machine_halt(void)
{
	printk(KERN_WARNING "Machine halted ... \n");
	hw_reset(false);
}

static void tc_machine_power_off(void)
{
	printk(KERN_WARNING "Machine poweroff ... \n");
	hw_reset(false);
}

static int tc_panic_event(struct notifier_block *this,
			  unsigned long event, void *ptr)
{
	tc_machine_restart(NULL);

	return NOTIFY_DONE;
}

static struct notifier_block tc_panic_block = {
	.notifier_call = tc_panic_event,
};

static void tc_reboot_setup(void)
{
	_machine_restart = tc_machine_restart;
	_machine_halt = tc_machine_halt;
	pm_power_off = tc_machine_power_off;

	atomic_notifier_chain_register(&panic_notifier_list, &tc_panic_block);
}

void __init plat_mem_setup(void)
{
	iomem_resource.start = 0;
	iomem_resource.end = ~0;

	ioport_resource.start = 0;
	ioport_resource.end = 0x1fffffff;

	tc_reboot_setup();
}
