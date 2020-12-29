#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timex.h>

#include <linux/jiffies.h>
#include <linux/delay.h>

#include <asm/mipsregs.h>
#include <asm/hardirq.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/time.h>

#include <asm/tc3162/tc3162.h>

extern unsigned int surfboard_sysclk;
extern unsigned int tc_mips_cpu_freq;
#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
extern void stop_dsl_dmt(void);
#endif

static void tc_timer_ctl(
	unsigned int timer_no,
	unsigned int timer_enable,
	unsigned int timer_mode,
	unsigned int timer_halt)
{
	unsigned int word;

	timer_enable &= 0x1;

	word = VPint(CR_TIMER_CTL);
#if defined(CONFIG_ECONET_EN7516) || \
    defined(CONFIG_ECONET_EN7527) || \
    defined(CONFIG_ECONET_EN7528)
	if (timer_enable)
		word |=  (1 << timer_no);
	else
		word &= ~(1 << timer_no);
#else
	word &= ~(1u << timer_no);
	word |=  (timer_enable << timer_no);
	word |=  (timer_mode << (timer_no + 8));
	word |=  (timer_halt << (timer_no + 26));
#endif
	VPint(CR_TIMER_CTL) = word;
}

void tc_timer_wdg(
	unsigned int tick_enable,
	unsigned int wdg_enable)
{
	unsigned int word;

	word = VPint(CR_TIMER_CTL);
	word &= 0xfdffffdf;
	word |= ((tick_enable & 0x1) << 5);
	word |= ((wdg_enable & 0x1) << 25);
	VPint(CR_TIMER_CTL) = word;
}
EXPORT_SYMBOL(tc_timer_wdg);

void tc_timer_set(
	unsigned int timer_no,
	unsigned int timerTime,
	unsigned int enable,
	unsigned int mode,
	unsigned int halt)
{
	unsigned int word;

	word = (timerTime * SYS_HCLK) * 500;
	timerLdvSet(timer_no, word);
	tc_timer_ctl(timer_no, enable, mode, halt);
}
EXPORT_SYMBOL(tc_timer_set);

static irqreturn_t tc_watchdog_timer_interrupt(int irq, void *dev_id)
{
	unsigned int word;

	word = VPint(CR_TIMER_CTL);
	word &= 0xffc0ffff;
	word |= 0x00200000;
	VPint(CR_TIMER_CTL) = word;

	printk(KERN_WARNING "watchdog timer interrupt\n");

#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
	stop_dsl_dmt();
#endif

	dump_stack();

	return IRQ_HANDLED;
}

static irqreturn_t tc_bus_timeout_interrupt(int irq, void *dev_id)
{
	unsigned int addr;

	/* write to clear interrupt */
	VPint(CR_PRATIR) = 1;

	addr = VPint(CR_ERR_ADDR);
	addr &= ~((1 << 30) | (1 << 31));

	printk(KERN_WARNING "bus timeout interrupt ERR ADDR=%08x\n",
		addr);

	dump_stack();

	return IRQ_HANDLED;
}

#ifdef CONFIG_ECONET_EN7528
irqreturn_t rbus_timeout_interrupt(int irq, void *dev_id)
{
	/*
	 * no chance to run this ISR because rbus timeout interrupt
	 * will be set as NMI
	 */
	return IRQ_HANDLED;
}

static struct irqaction rbus_timeout_irqaction = {
	.handler	 = rbus_timeout_interrupt,
	.flags		 = IRQF_NO_THREAD,
	.name		 = "rbus timeout",
};
#endif

static struct irqaction tc_watchdog_timer_irqaction = {
	.handler	 = tc_watchdog_timer_interrupt,
	.flags		 = IRQF_NO_THREAD,
	.name		 = "watchdog",
};

static struct irqaction tc_bus_timeout_irqaction = {
	.handler	 = tc_bus_timeout_interrupt,
	.flags		 = IRQF_NO_THREAD,
	.name		 = "bus timeout",
};

void __init __weak plat_hpt_init(void)
{
}

void __init plat_time_init(void)
{
	mips_hpt_frequency = tc_mips_cpu_freq / 2;

	/* enable CPU external timer */
	plat_hpt_init();

	tc_timer_set(1, TIMERTICKS_10MS, ENABLE, TIMER_TOGGLEMODE, TIMER_HALTDISABLE);

	/* setup watchdog timer interrupt */
	setup_irq(TIMER5_INT, &tc_watchdog_timer_irqaction);

	/* set countdown 2 seconds to issue WDG interrupt */
	VPint(CR_WDOG_THSLD) = (2 * TIMERTICKS_1S * SYS_HCLK) * 500;

	/* setup bus timeout interrupt */
	setup_irq(BUS_TOUT_INT, &tc_bus_timeout_irqaction);

#ifdef CONFIG_ECONET_EN7528
	setup_irq(RBUS_TOUT_INTR, &rbus_timeout_irqaction);

	/* ASIC bus_clk: 235MHz */
	VPint(CR_MON_TMR) = 0x10000000;	/* unit: one bus clock */
	VPint(CR_MON_TMR) = 0x50000000;	/* enable it */

	/* enable rbus timeout mechanism */
	regWrite32(RBUS_TIMEOUT_CFG0, 0x3ffffff);
	regWrite32(RBUS_TIMEOUT_CFG1, 0x3ffffff);
	regWrite32(RBUS_TIMEOUT_CFG2, 0x3ffffff);
	regWrite32(RBUS_TIMEOUT_STS0, 0x80000000);
#else
	VPint(CR_MON_TMR) = 0xcfffffff;
	VPint(CR_BUSTIMEOUT_SWITCH) = 0xffffffff;
#endif
}

unsigned int get_surfboard_sysclk(void)
{
	return surfboard_sysclk;
}

EXPORT_SYMBOL(get_surfboard_sysclk);
