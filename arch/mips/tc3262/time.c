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
#ifdef CONFIG_TC3162_ADSL
extern void stop_adsl_dmt(void);
#endif

#ifdef CONFIG_TC3262_CPU_TIMER

#ifdef CONFIG_MIPS_TC3262_1004K
#define MAX_CPU_VPE	4
#else
#define MAX_CPU_VPE	2
#endif

static unsigned long cycles_per_jiffy;
static unsigned int expirelo[MAX_CPU_VPE];

static const unsigned int cputmr_cnt[MAX_CPU_VPE] = {
	CR_CPUTMR_CNT0, CR_CPUTMR_CNT1,
#ifdef CONFIG_MIPS_TC3262_1004K
	CR_CPUTMR_CNT2, CR_CPUTMR_CNT3
#endif
};

static const unsigned int cputmr_cmr[MAX_CPU_VPE] = {
	CR_CPUTMR_CMR0, CR_CPUTMR_CMR1,
#ifdef CONFIG_MIPS_TC3262_1004K
	CR_CPUTMR_CMR2, CR_CPUTMR_CMR3
#endif
};

static cycle_t tc_hpt_read(struct clocksource *cs)
{
	return VPint(CR_CPUTMR_CNT0);
}

static struct clocksource tc_hpt_clocksource = {
	.name		= "TC HPT",
	.mask		= CLOCKSOURCE_MASK(32),
	.read		= tc_hpt_read,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static inline void tc_hpt_timer_init(void)
{
	unsigned int i;

	mips_hpt_frequency = CPUTMR_CLK;

#if 0
	if (isEN751221) {
		/* mips_hpt_frequency *= (1 - 0.00003650354); */
		mips_hpt_frequency -= 4370;
	}
#endif

	cycles_per_jiffy = (mips_hpt_frequency + HZ / 2) / HZ;

	for (i = 0; i < MAX_CPU_VPE; i++)
		VPint(cputmr_cnt[i]) = 0x0;

	expirelo[0] = cycles_per_jiffy;
	for (i = 1; i < MAX_CPU_VPE; i++)
		expirelo[i] = expirelo[0];

	for (i = 0; i < MAX_CPU_VPE; i++)
		VPint(cputmr_cmr[i]) = expirelo[i];

	VPint(CR_CPUTMR_CTL) |= ((1 << 1) | (1 << 0));
#ifdef CONFIG_MIPS_TC3262_1004K
	VPint(CR_CPUTMR_23_CTL) |= ((1 << 1) | (1 << 0));
#endif

	/* must be higher than MIPS original cd rating (300) */
	tc_hpt_clocksource.rating = 350;
	clocksource_register_hz(&tc_hpt_clocksource, CPUTMR_CLK);

	printk(KERN_INFO " Using %u.%03u MHz high precision timer\n",
		((mips_hpt_frequency + 500) / 1000) / 1000,
		((mips_hpt_frequency + 500) / 1000) % 1000);
}

void tc_hpt_timer_ack(int cpu)
{
	int vpe = cpu;

	/* Ack this timer interrupt and set the next one.  */
	expirelo[vpe] += cycles_per_jiffy;

	/* Check to see if we have missed any timer interrupts.  */
	while (unlikely((VPint(cputmr_cnt[vpe]) - expirelo[vpe]) < 0x7fffffff)) {
		/* missed_timer_count++; */
		expirelo[vpe] += cycles_per_jiffy;
	}

	/* update CR_CPUTMR_CMR */
	VPint(cputmr_cmr[vpe]) = expirelo[vpe];
}
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
    defined(CONFIG_ECONET_EN7527)
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

#ifdef CONFIG_TC3162_ADSL
	/* stop adsl */
	stop_adsl_dmt();
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

void __init plat_time_init(void)
{
	mips_hpt_frequency = tc_mips_cpu_freq / 2;

#ifdef CONFIG_TC3262_CPU_TIMER
	if (isEN751221 || isEN751627) {
		/* enable CPU external timer */
		tc_hpt_timer_init();
	}
#endif

	tc_timer_set(1, TIMERTICKS_10MS, ENABLE, TIMER_TOGGLEMODE, TIMER_HALTDISABLE);

	/* setup watchdog timer interrupt */
	setup_irq(TIMER5_INT, &tc_watchdog_timer_irqaction);

	/* set countdown 2 seconds to issue WDG interrupt */
	VPint(CR_WDOG_THSLD) = (2 * TIMERTICKS_1S * SYS_HCLK) * 500;

	/* setup bus timeout interrupt */
	setup_irq(BUS_TOUT_INT, &tc_bus_timeout_irqaction);
	VPint(CR_MON_TMR) = 0xcfffffff;
	VPint(CR_BUSTIMEOUT_SWITCH) = 0xffffffff;
}

unsigned int get_surfboard_sysclk(void)
{
	return surfboard_sysclk;
}

EXPORT_SYMBOL(get_surfboard_sysclk);
