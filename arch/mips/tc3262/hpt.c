#define pr_fmt(fmt)				"hpt: " fmt

#include <linux/io.h>
#include <linux/cache.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/cpumask.h>
#include <linux/irqflags.h>
#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/sched_clock.h>
#include <asm/tc3162/tc3162.h>

#define HPT_BITS				32

#define HPT_MIN_DELTA				0x00000300
#define HPT_MAX_DELTA				GENMASK(HPT_BITS - 1, 0)

#define HPT_IRQ					SI_TIMER_INT

static inline void hpt_w32(const u32 reg, const u32 val)
{
	__raw_writel(val, (void __iomem *)(unsigned long)reg);
}

static inline u32 hpt_r32(const u32 reg)
{
	return __raw_readl((void __iomem *)(unsigned long)reg);
}

struct hpt_regs {
	u32 lvr;		/* compare value */
	u32 cvr;		/* count	 */
	u32 ctl;		/* control	 */
	u32 ctl_enabled;	/* enabled mask  */
	u32 ctl_pending;	/* pending mask  */
};

struct hpt {
	struct clock_event_device cd;
	struct hpt_regs		  regs;
} ____cacheline_aligned_in_smp;

static inline u32 hpt_read_count(const struct hpt *hpt)
{
	return hpt_r32(hpt->regs.cvr);
}

static inline void hpt_write_compare(const struct hpt *hpt,
				     const u32 lvr)
{
	hpt_w32(hpt->regs.lvr, lvr);
}

static inline bool hpt_is_pending(const struct hpt *hpt)
{
	const struct hpt_regs *regs = &hpt->regs;

	return hpt_r32(regs->ctl) & regs->ctl_pending;
}

static void hpt_init(const struct hpt *hpt)
{
	const struct hpt_regs *regs = &hpt->regs;

	hpt_w32(regs->cvr, 0);
	hpt_w32(regs->lvr, HPT_MAX_DELTA);
}

static void hpt_enable(const struct hpt *hpt)
{
	const struct hpt_regs *regs = &hpt->regs;
	u32 ctl = hpt_r32(regs->ctl);

	ctl |= regs->ctl_enabled;
	hpt_w32(regs->ctl, ctl);
}

static DEFINE_PER_CPU(struct hpt, hpt_pcpu);

static irqreturn_t hpt_compare_interrupt(int irq, void *dev_id)
{
	struct hpt *hpt = dev_id;

	if (!hpt_is_pending(hpt))
		return IRQ_NONE;

	hpt_write_compare(hpt, hpt_read_count(hpt));
	hpt->cd.event_handler(&hpt->cd);

	return IRQ_HANDLED;
}

static int hpt_set_next_event(unsigned long delta,
			      struct clock_event_device *cd)
{
	/* called by the kernel with disabled local hardware interrupts */
	const struct hpt *hpt = container_of(cd, struct hpt, cd);
	const u32 next = hpt_read_count(hpt) + delta;
	u32 cnt;

	hpt_write_compare(hpt, next);
	cnt = hpt_read_count(hpt);

	if (next >= cnt)
		return 0; /* next - cnt <= delta */

	if (HPT_MAX_DELTA - cnt + next > delta)
		return -ETIME;

	return 0;
}

static void hpt_event_handler(struct clock_event_device *cd)
{
}

int r4k_clockevent_init(void)
{
	static struct irqaction hpt_compare_irqaction = {
		.handler	= hpt_compare_interrupt,
		.percpu_dev_id	= &hpt_pcpu,
		.flags		= IRQF_PERCPU | IRQF_TIMER | IRQF_SHARED,
		.name		= "timer"
	};
	static atomic_t irq_installed = ATOMIC_INIT(0);
	const unsigned int irq = HPT_IRQ;
	const unsigned int cpu = smp_processor_id();
	struct hpt *hpt = &per_cpu(hpt_pcpu, cpu);
	struct clock_event_device *cd = &hpt->cd;

	cd->name		= "hpt";
	cd->features		= CLOCK_EVT_FEAT_ONESHOT |
				  CLOCK_EVT_FEAT_C3STOP |
				  CLOCK_EVT_FEAT_PERCPU;
	cd->rating		= 300;
	cd->irq			= irq;
	cd->cpumask		= cpumask_of(cpu);
	cd->set_next_event	= hpt_set_next_event;
	cd->event_handler	= hpt_event_handler;

	clockevents_config_and_register(cd, CPUTMR_CLK,
					HPT_MIN_DELTA,
					HPT_MAX_DELTA);

	hpt_enable(hpt);

	if (atomic_cmpxchg(&irq_installed, 0, 1) == 0) {
		int rc = irq_set_percpu_devid(irq);

		if (rc) {
			pr_err("unable to configure IRQ%u (%i)\n", irq, rc);
			return rc;
		}

		rc = setup_percpu_irq(irq, &hpt_compare_irqaction);
		if (rc) {
			pr_err("unable to setup IRQ%u (%i)\n", irq, rc);
			return rc;
		}
	}

	enable_percpu_irq(irq, IRQ_TYPE_NONE);

	return 0;
}

/* the monotonic clocksource and the scheduler clock are global
 * objects and must use the same value for all threads */
static inline u32 hpt_read(void)
{
	return hpt_read_count(&per_cpu(hpt_pcpu, 0));
}

static u64 notrace hpt_read_sched_clock(void)
{
	return (u64)hpt_read();
}

static cycle_t hpt_clocksource_read(struct clocksource *cs)
{
	return (cycle_t)hpt_read();
}

int __init init_r4k_clocksource(void)
{
	static struct clocksource hpt_clocksource = {
		.name		= "hpt",
		.mask		= CLOCKSOURCE_MASK(HPT_BITS),
		.read		= hpt_clocksource_read,
		.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
		.rating		= 300
	};
	const int rc = clocksource_register_hz(&hpt_clocksource, CPUTMR_CLK);

	if (rc) {
		pr_err("unable to register a clocksource (%i)\n", rc);
		return rc;
	}

#ifndef CONFIG_CPU_FREQ
	sched_clock_register(hpt_read_sched_clock, HPT_BITS, CPUTMR_CLK);
#endif

	return 0;
}

#define HPT_REGS_DEF(cmr, cnt, ctl, enabled_bit, pending_bit)		\
	CR_CPUTMR_##cmr,						\
	CR_CPUTMR_##cnt,						\
	CR_CPUTMR_##ctl,						\
	1U << enabled_bit,						\
	1U << pending_bit

void __init plat_hpt_init(void)
{
	unsigned int i;
	const unsigned int freq = CPUTMR_CLK;

	for (i = 0; i < NR_CPUS; i++) {
		static const struct hpt_regs HPT_REGS[] = {
			{ HPT_REGS_DEF(CMR0, CNT0, CTL,    0, 16) },
			{ HPT_REGS_DEF(CMR1, CNT1, CTL,    1, 17) },
#ifdef CONFIG_MIPS_TC3262_1004K
			{ HPT_REGS_DEF(CMR2, CNT2, 23_CTL, 0, 16) },
			{ HPT_REGS_DEF(CMR3, CNT3, 23_CTL, 1, 17) },
#endif
		};
		struct hpt *hpt = &per_cpu(hpt_pcpu, i);

		hpt->regs = HPT_REGS[i];
		hpt_init(hpt);
	}

	pr_info("using %u.%03u MHz high precision timer\n",
		((freq + 500) / 1000) / 1000,
		((freq + 500) / 1000) % 1000);
}
