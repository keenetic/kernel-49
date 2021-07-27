#include <linux/init.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/cpu.h>
#include <asm/cpu-info.h>
#include <asm/bootinfo.h>

#include <asm/time.h>
#include <asm/reboot.h>
#include <asm/pbus-timer.h>

#include <asm/tc3162/tc3162.h>

#include "irq.h"

#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
struct sk_buff;

#define ADSL_SET_DMT_CLOSE				(0x001a)

typedef struct {
	void (*query)(unsigned short query_id, void *result1, void *result2);
	void (*set)(unsigned short set_id, void *value1, void *value2);

	void (*rts_rcv)(struct sk_buff *skb);

	int  (*rts_cmd)(int argc,char *argv[],void *p);
	int  (*dmt_cmd)(int argc,char *argv[],void *p);
	int  (*dmt2_cmd)(int argc,char *argv[],void *p);
	int  (*hw_cmd)(int argc,char *argv[],void *p);
	int  (*sw_cmd)(int argc,char *argv[],void *p);
	int  (*ghs_cmd)(int argc,char *argv[],void *p);
	int  (*tcif_cmd)(int argc,char *argv[],void *p);
} adsldev_ops;

adsldev_ops *adsl_dev_ops = NULL;
EXPORT_SYMBOL(adsl_dev_ops);

static inline void stop_dsl_dmt(void)
{
	if (adsl_dev_ops)
		adsl_dev_ops->set(ADSL_SET_DMT_CLOSE, NULL, NULL);
}
#else
static inline void stop_dsl_dmt(void) {}
#endif

static inline void hw_uninit(void)
{
	/* stop the DMT module */
	stop_dsl_dmt();

	/* stop each module dma task */
	tc_disable_irq_all();

#if !defined(CONFIG_ECONET_EN7528)
	/* stop atm sar dma */
	TSARM_GFR &= ~((1 << 1) | (1 << 0));
#endif

	/* stop all APB module timers except an active watchdog */
	pbus_timer_disable(PBUS_TIMER_0);
	pbus_timer_disable(PBUS_TIMER_1);
	pbus_timer_disable(PBUS_TIMER_2);

	/* timer 3 is in a non-watchdog mode */
	if (!(pbus_timer_r32(PBUS_TIMER_CTRL) & PBUS_TIMER_CTRL_WDG_ENABLE))
		pbus_timer_disable(PBUS_TIMER_3);
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

static int tc_panic_event(struct notifier_block *nb,
			  unsigned long action, void *data)
{
	if (action == PANIC_ACTION_RESTART)
		tc_machine_restart(NULL);
	else
		tc_machine_halt();

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
