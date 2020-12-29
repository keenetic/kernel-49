#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <asm/time.h>
#include <asm/pbus-timer.h>
#include <asm/tc3162/tc3162.h>

extern unsigned int surfboard_sysclk;
extern unsigned int tc_mips_cpu_freq;

static irqreturn_t tc_bus_timeout_interrupt(int irq, void *dev_id)
{
	/* clear an interrupt */
	VPint(CR_PRATIR) = 1;

	pr_warn("bus timeout interrupt, error address is %08lx\n",
		VPint(CR_ERR_ADDR) & 0x3fffffff);

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

static struct irqaction tc_bus_timeout_irqaction = {
	.handler	= tc_bus_timeout_interrupt,
	.flags		= IRQF_NO_THREAD,
	.name		= "bus timeout",
};

void __init __weak plat_hpt_init(void)
{
}

void __init plat_time_init(void)
{
	mips_hpt_frequency = tc_mips_cpu_freq / 2;

	/* enable a CPU external timer */
	plat_hpt_init();

#if defined(CONFIG_TC3162_ADSL) || \
    defined(CONFIG_RALINK_VDSL)
	/*
	 * Configure first timer to 10 ms. periodic infinite mode.
	 * The active timer used in the DMT module
	 * to implement a microsecond delay function.
	 */
	pbus_timer_enable(PBUS_TIMER_1, 10, PBUS_TIMER_MODE_PERIODIC);
#endif

	/* setup a bus timeout interrupt */
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
