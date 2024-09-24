// SPDX-License-Identifier: GPL-2.0-only

#define MT_WDT_DRV_NAME				  "mt_wdt"
#define pr_fmt(fmt)				  MT_WDT_DRV_NAME ": " fmt

#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/bug.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/cpumask.h>
#include <linux/kallsyms.h>
#include <linux/kmsg_dump.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <asm/irq_regs.h>
#include <asm/stacktrace.h>

#define MT_WDT_DUMP_BYTES			  (128)
#define MT_WDT_DUMP_LINE_WORDS_32		  (8)
#define MT_WDT_DUMP_LINES			  \
	(MT_WDT_DUMP_BYTES / (MT_WDT_DUMP_LINE_WORDS_32 * sizeof(u32)))

#define MT_WDT_DUMP_CPU_REG_LWORDS		  (4)

#define MT_WDT_1SEC				  (1000)

#define MT_WDT_IRQ_NONE				  (-1)

/* valid values are [9, 30] with 3 seconds granularity */
#define MT_WDT_TIMEOUT_SEC_PROP			  "timeout-sec"
#define MT_WDT_TIMEOUT_GRANULARITY		  (3)
#define MT_WDT_TIMEOUT_MIN_PERIODS		  (3)
#define MT_WDT_TIMEOUT_MAX_PERIODS		  (10)

#define MT_WDT_TIMER_FREQ			  (32000) /* 32 KHz */
#define MT_WDT_TIMER_LENGTH_UNIT		  (512)   /* length unit */
#define MT_WDT_TIMER_TICKS_PER_SECOND		  \
	(MT_WDT_TIMER_FREQ / MT_WDT_TIMER_LENGTH_UNIT)
#define MT_WDT_TIMER_MSECS_TO_TICKS(msecs)	  \
	(((msecs) * MT_WDT_TIMER_TICKS_PER_SECOND) / MT_WDT_1SEC)
#define MT_WDT_TIMER_TICKS_TO_MSECS(ticks)	  \
	(((ticks) * MT_WDT_1SEC) / MT_WDT_TIMER_TICKS_PER_SECOND)

#define MT_WDT_SEC_TO_MSECS(sec)		  ((sec) * MT_WDT_1SEC)

#define MT_WDT_LENGTH_GRANULARITY		  \
	MT_WDT_TIMEOUT_GRANULARITY
#define MT_WDT_LENGTH_SECONDS_MIN		  \
	(MT_WDT_TIMEOUT_MIN_PERIODS * MT_WDT_TIMEOUT_GRANULARITY)
#define MT_WDT_LENGTH_SECONDS_MAX		  \
	(MT_WDT_TIMEOUT_MAX_PERIODS * MT_WDT_TIMEOUT_GRANULARITY)

#define MT_WDT_MODE				  (0x00)
#define MT_WDT_MODE_KEY				  (0x22 << 24)
#define MT_WDT_MODE_WDT_RESET			  (1 << 8)
#define MT_WDT_MODE_DUAL_EN			  (1 << 6)
#define MT_WDT_MODE_IRQ_EN			  (1 << 3)
#define MT_WDT_MODE_EXT_RST_EN			  (1 << 2)
#define MT_WDT_MODE_WDT_EN			  (1 << 0)

#define MT_WDT_LENGTH				  (0x04)
#define MT_WDT_LENGTH_VALUE(msecs)		  \
	((MT_WDT_TIMER_MSECS_TO_TICKS(msecs) & 0x7ff) << 5)
#define MT_WDT_LENGTH_UNLOCK_KEY		  (0x08)

#define MT_WDT_RESTART				  (0x08)
#define MT_WDT_RESTART_KEY			  (0x1971)

#define MT_WDT_STATUS				  (0x0c)
#define MT_WDT_STATUS_HW_WDT_RST		  (1 << 31)
#define MT_WDT_STATUS_SW_WDT_RST		  (1 << 30)
#define MT_WDT_STATUS_IRQ_WDT_RST		  (1 << 29)
#define MT_WDT_STATUS_SECURITY_RST		  (1 << 28)
#define MT_WDT_STATUS_SPM_WDT_RST		  (1 <<  1)
#define MT_WDT_STATUS_SPM_THERMAL_RST		  (1 <<  0)
#define MT_WDT_STATUS_HW_RST			  (0 <<  0)

#define MT_WDT_STATUS_HW_IRQ_WDT_RST		  \
	(MT_WDT_STATUS_HW_WDT_RST |		  \
	 MT_WDT_STATUS_IRQ_WDT_RST)

#define MT_WDT_BACKTRACE_POLL_RETRIES		  (20)
#define MT_WDT_BACKTRACE_POLL_DELAY_MSECS	  (50)

#define mt_wdt_info(wdt, fmt, arg...) dev_info(wdt->dev, fmt, ##arg)
#define mt_wdt_warn(wdt, fmt, arg...) dev_warn(wdt->dev, fmt, ##arg)
#define mt_wdt_err(wdt, fmt, arg...) dev_err(wdt->dev, fmt, ##arg)

#define MT_WDT_GPT_NODE				  "mediatek,timer"

#define MT_WDT_GPT_CON				  (0x00)
#define MT_WDT_GPT_CLK				  (0x04)
#define MT_WDT_GPT_COUNT			  (0x08)
#define MT_WDT_GPT_COMPARE			  (0x0c)

#define MT_WDT_GPT_CON_MODE_ONE_SHOT		  (0U)
#define MT_WDT_GPT_CON_MODE_REPEAT		  (1U)
#define MT_WDT_GPT_CON_MODE_KEEP_GO		  (2U)
#define MT_WDT_GPT_CON_MODE_FREERUN		  (3U)

#define MT_WDT_GPT_CON_CLR			  (1U << 1)

#define MT_WDT_GPT_CON_ENABLE			  (1U << 0)

#define MT_WDT_GPT_CLK_32KHZ			  (1U)
#define MT_WDT_GPT_CLK_13MHZ			  (0U)

#define MT_WDT_GPT_CLK_DIV_1			  (0U)
#define MT_WDT_GPT_CLK_DIV_2			  (1U)
#define MT_WDT_GPT_CLK_DIV_3			  (2U)
#define MT_WDT_GPT_CLK_DIV_4			  (3U)
#define MT_WDT_GPT_CLK_DIV_5			  (4U)
#define MT_WDT_GPT_CLK_DIV_6			  (5U)
#define MT_WDT_GPT_CLK_DIV_7			  (6U)
#define MT_WDT_GPT_CLK_DIV_8			  (7U)
#define MT_WDT_GPT_CLK_DIV_9			  (8U)
#define MT_WDT_GPT_CLK_DIV_10			  (9U)
#define MT_WDT_GPT_CLK_DIV_11			  (10U)
#define MT_WDT_GPT_CLK_DIV_12			  (11U)
#define MT_WDT_GPT_CLK_DIV_13			  (12U)
#define MT_WDT_GPT_CLK_DIV_16			  (13U)
#define MT_WDT_GPT_CLK_DIV_32			  (14U)
#define MT_WDT_GPT_CLK_DIV_64			  (15U)

struct mt_wdt;

struct mt_wdt_cpu {
	struct mt_wdt			 *wdt;
	atomic_t			  backtraced;
	unsigned long			  sw_timer_next;
	struct timer_list		  sw_timer;
	struct hrtimer			  hw_timer;
	u32				  hw_last_tick;
	u32				  sw_last_tick;
	u32				  stall_period;
} ____cacheline_aligned_in_smp;

struct mt_wdt_gpt {
	u8 __iomem			 *con;
	u8 __iomem			 *count;
};

struct mt_wdt_data {
	u8 gpt_status_reg;
	u8 gpt_num;
	u8 gpt_reg_size;
	u8 gpt_mode_clk_div_reg;
	u8 gpt_mode_offset;
	u8 gpt_clk_offset;
	u8 gpt_clk_div_offset;
	u8 wdt_sts_bit_thermal;
};

struct mt_wdt {
	spinlock_t			  lock;
	cpumask_t			  alive_cpus;
	bool				  stopping;
	u32				  refresh_interval;
	u32				  bind_threshold;
	u32				  stall_threshold;
	int				  irq_cpu;
	u8 __iomem			 *base;
	struct device			 *dev;
	struct irq_affinity_notify	  affinity_notify;
	struct mt_wdt_cpu		  cpu[NR_CPUS];
	struct mt_wdt_gpt		  gpt;
	const struct mt_wdt_data	 *data;
};

static const struct mt_wdt_data mt7622_data = {
	.gpt_status_reg		= 0x4c,
	.gpt_num		= 3,
	.gpt_reg_size		= 0x10,
	.gpt_mode_clk_div_reg	= MT_WDT_GPT_CLK,
	.gpt_mode_offset	= 4,
	.gpt_clk_offset		= 4,
	.gpt_clk_div_offset	= 0,
	.wdt_sts_bit_thermal	= 18,
};

static const struct mt_wdt_data mt7986_data = {
	.gpt_status_reg		= 0x6c,
	.gpt_num		= 2,
	.gpt_reg_size		= 0x20,
	.gpt_mode_clk_div_reg	= MT_WDT_MODE,
	.gpt_mode_offset	= 5,
	.gpt_clk_offset		= 2,
	.gpt_clk_div_offset	= 10,
	.wdt_sts_bit_thermal	= 18,
};

static const struct mt_wdt_data mt7988_data = {
	.gpt_status_reg		= 0x6c,
	.gpt_num		= 2,
	.gpt_reg_size		= 0x20,
	.gpt_mode_clk_div_reg	= MT_WDT_MODE,
	.gpt_mode_offset	= 5,
	.gpt_clk_offset		= 2,
	.gpt_clk_div_offset	= 10,
	.wdt_sts_bit_thermal	= 16,
};

static inline void
mt_wdt_gpt_init(struct mt_wdt_gpt *gpt,
		u8 __iomem *base,
		const struct mt_wdt_data *data)
{
	u8 __iomem *gpt_base = base + data->gpt_num * data->gpt_reg_size;

	gpt->con = gpt_base + MT_WDT_GPT_CON;
	gpt->count = gpt_base + MT_WDT_GPT_COUNT;

	/* stop timer */
	iowrite32(0, gpt->con);

	/* configure clock */
	iowrite32((MT_WDT_GPT_CLK_13MHZ << data->gpt_clk_offset) |
		  (MT_WDT_GPT_CLK_DIV_13 << data->gpt_clk_div_offset),
		  gpt_base + data->gpt_mode_clk_div_reg);

	/* start timer and clear counter */
	iowrite32(ioread32(gpt->con) |
		  (MT_WDT_GPT_CON_MODE_FREERUN << data->gpt_mode_offset) |
		  MT_WDT_GPT_CON_CLR |
		  MT_WDT_GPT_CON_ENABLE,
		  gpt->con);
}

static inline void
mt_wdt_gpt_fini(const struct mt_wdt_gpt *gpt)
{
	iowrite32(0, gpt->con);
}

static inline void
mt_wdt_gpt_reset(const struct mt_wdt_gpt *gpt)
{
	iowrite32(ioread32(gpt->con) | MT_WDT_GPT_CON_CLR, gpt->con);
}

static inline u32
mt_wdt_gpt_msecs(const struct mt_wdt_gpt *gpt)
{
	return ioread32(gpt->count) / 1000;
}

static inline bool
mt_wdt_bind(struct mt_wdt *wdt, const int alive_cpu)
{
	if (wdt->irq_cpu == alive_cpu)
		return true;

	if (irq_set_affinity(wdt->affinity_notify.irq,
			     get_cpu_mask(alive_cpu)) != 0)
		return false;

	wdt->irq_cpu = alive_cpu;
	return true;
}

static inline void
mt_wdt_hw_configure(const struct mt_wdt *wdt, const u32 msecs)
{
	iowrite32(MT_WDT_LENGTH_VALUE(msecs) |
		  MT_WDT_LENGTH_UNLOCK_KEY,
		  wdt->base + MT_WDT_LENGTH);
}

static inline void
mt_wdt_hw_restart(const struct mt_wdt *wdt)
{
	iowrite32(MT_WDT_RESTART_KEY, wdt->base + MT_WDT_RESTART);
}

static inline void
mt_wdt_hw_enable(const struct mt_wdt *wdt, const bool dual_mode)
{
	u8 __iomem *reg = wdt->base + MT_WDT_MODE;
	u32 mode = ioread32(reg);

	mode &= ~(MT_WDT_MODE_WDT_RESET |
		  MT_WDT_MODE_DUAL_EN |
		  MT_WDT_MODE_IRQ_EN |
		  MT_WDT_MODE_EXT_RST_EN);
	mode |=   MT_WDT_MODE_WDT_EN |
		  MT_WDT_MODE_KEY;

	if (dual_mode)
		mode |= MT_WDT_MODE_DUAL_EN |
			MT_WDT_MODE_IRQ_EN;

	iowrite32(mode, reg);
}

static inline void
mt_wdt_hw_disable(const struct mt_wdt *wdt)
{
	u8 __iomem *reg = wdt->base + MT_WDT_MODE;
	u32 mode = ioread32(reg);

	mode &= ~MT_WDT_MODE_WDT_EN;
	mode |=  MT_WDT_MODE_KEY;
	iowrite32(mode, reg);
}

static inline bool
mt_wdt_is_stalled(const struct mt_wdt *wdt,
		  const u32 time,
		  const u32 now)
{
	BUG_ON(now < time);

	return now - time >= wdt->stall_threshold;
}

static inline int
mt_wdt_unwind_frame(struct stackframe *frame)
{
	const int cpu = raw_smp_processor_id();
	const unsigned long irq_sp = IRQ_STACK_PTR(cpu);
	const unsigned long fp = frame->fp;
	const unsigned long low = frame->sp;
	const unsigned long high = on_irq_stack(low, cpu) ?
		irq_sp : (ALIGN(low, THREAD_SIZE) - 0x20);

	if (fp < low || fp > high || fp & 0xf)
		return -EINVAL;

	frame->sp = fp + 0x10;
	frame->fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 0));
	frame->pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));

	if (frame->sp == irq_sp) {
		const unsigned long tsk_sp = IRQ_STACK_TO_TASK_STACK(irq_sp);

		if (!object_is_on_stack((void *)tsk_sp) ||
		    !object_is_on_stack((void *)frame->fp))
			return -EINVAL;

		frame->sp = tsk_sp;
		frame->pc = ((struct pt_regs *)tsk_sp)->pc;
	}

	return 0;
}

static void mt_wdt_dump_data(const unsigned long addr,
			     const char *const name)
{
	size_t i;
	u32 *p = (u32 *)((addr & ~(sizeof(*p) - 1)) - MT_WDT_DUMP_BYTES / 2);

	printk("\n%s: %pS:\n", name, (void *)addr);

	for (i = 0; i < MT_WDT_DUMP_LINES; i++) {
		size_t j;

		printk("%04lx:", (unsigned long)p & 0xffff);

		for (j = 0; j < MT_WDT_DUMP_LINE_WORDS_32; j++, p++) {
			u32 data;

			if (probe_kernel_address(p, data))
				pr_cont(" ????????");
			else
				pr_cont(" %08x", data);
		}

		printk("\n");
	}
}

static void
mt_wdt_dump_cpu_state(void *info)
{
	size_t i = 0;
	struct mt_wdt_cpu *cpu = info;
	struct pt_regs *regs = get_irq_regs();
	struct stackframe frame = {
		.fp = frame_pointer(regs),
		.sp = kernel_stack_pointer(regs),
		.pc = instruction_pointer(regs)
	};

	printk("\nCPU%i stall period is %u ms.\n",
	       raw_smp_processor_id(), cpu->stall_period);

	show_regs_print_info(KERN_DEFAULT);
	printk("pstate: %08llx\n", regs->pstate);

	while (i < ARRAY_SIZE(regs->regs)) {
		const char *ident = "";
		size_t j;

		printk(" x%-2zu:", i);

		for (j = 0; j < MT_WDT_DUMP_CPU_REG_LWORDS; j++, i++) {
			if (i >= ARRAY_SIZE(regs->regs))
				break;

			pr_cont(" %016llx%s", regs->regs[i], ident);
			ident = " ";
		}

		printk("\n");
	}

	if (!user_mode(regs)) {
		mt_wdt_dump_data(regs->pc, "PC");
		mt_wdt_dump_data(regs->sp, "SP");
	}

	printk("\nCall trace:\n");

	while (1) {
		int ret;

		printk("[<%p>] %pS\n", (void *)frame.pc, (void *)frame.pc);
		ret = mt_wdt_unwind_frame(&frame);

		if (ret < 0 || frame.fp == 0)
			break;
	}

	atomic_inc(&cpu->backtraced);
}

static void mt_wdt_cpu_stalled(struct mt_wdt *wdt)
{
	struct cpumask soft_stalled;
	struct cpumask hard_stalled;
	char hard_stalled_list[16];
	u32 now;
	int i;

	spin_lock(&wdt->lock);

	now = mt_wdt_gpt_msecs(&wdt->gpt);
	wdt->stopping = true;
	cpumask_clear(&soft_stalled);
	cpumask_clear(&hard_stalled);

	for_each_possible_cpu(i) {
		struct mt_wdt_cpu *cpu = &wdt->cpu[i];

		cpu->stall_period = now - cpu->sw_last_tick;

		if (mt_wdt_is_stalled(wdt, cpu->sw_last_tick, now))
			cpumask_set_cpu(i, &soft_stalled);

		if (mt_wdt_is_stalled(wdt, cpu->hw_last_tick, now))
			cpumask_set_cpu(i, &hard_stalled);
	}

	spin_unlock(&wdt->lock);

	console_verbose();
	bust_spinlocks(1);

	snprintf(hard_stalled_list, sizeof(hard_stalled_list), "%*pbl",
		 cpumask_pr_args(&hard_stalled));

	printk("watchdog timer interrupt on CPU%i, stalled CPUs: "
	       "%*pbl (soft), %s (hard)\n\n", raw_smp_processor_id(),
	       cpumask_pr_args(&soft_stalled),
	       (hard_stalled_list[0] == '\0') ? "none" : hard_stalled_list);

	print_modules();
	printk_nmi_flush_on_panic();

	for_each_possible_cpu(i) {
		size_t j;
		struct mt_wdt_cpu *cpu = &wdt->cpu[i];

		if (!cpumask_test_cpu(i, &soft_stalled)) {
			printk("\nCPU%i is not stalled\n", i);
			continue;
		}

		if (i == raw_smp_processor_id()) {
			mt_wdt_dump_cpu_state(cpu);
			continue;
		}

		if (cpumask_test_cpu(i, &hard_stalled)) {
			printk("\nCPU%i stalls with disabled "
			       "hardware interrupts\n", i);
			continue;
		}

		smp_call_function_single(i, mt_wdt_dump_cpu_state, cpu, 0);

		for (j = 0; j < MT_WDT_BACKTRACE_POLL_RETRIES; j++) {
			mdelay(MT_WDT_BACKTRACE_POLL_DELAY_MSECS);

			if (atomic_read(&cpu->backtraced))
				break;
		}

		if (!atomic_read(&cpu->backtraced))
			printk("\nCPU%i backtrace timed out\n", i);
	}

	printk("\n");
	printk_nmi_flush_on_panic();

	/*
	 * Run any panic handlers, including those that might need to
	 * add information to the kmsg dump output.
	 */
	atomic_notifier_call_chain(&panic_notifier_list,
				   PANIC_ACTION_HALT, NULL);

	/*
	 * Call flush even twice. It tries harder with a single online CPU.
	 */
	printk_nmi_flush_on_panic();
	kmsg_dump(KMSG_DUMP_PANIC);

	/*
	 * Restart the system immediately.
	 */
	mt_wdt_hw_configure(wdt, 1);
	mt_wdt_hw_restart(wdt);

	while (1)
		cpu_relax();
}

static inline void
mt_wdt_hw_timer_restart(struct mt_wdt *wdt, struct mt_wdt_cpu *cpu)
{
	hrtimer_forward_now(&cpu->hw_timer,
			    ktime_set(wdt->refresh_interval, 0));
}

static enum hrtimer_restart
mt_wdt_hw_timer(struct hrtimer *hrtimer)
{
	enum hrtimer_restart restart = HRTIMER_NORESTART;
	bool stalled = false;
	struct mt_wdt_cpu *cpu;
	struct mt_wdt *wdt;

	cpu = container_of(hrtimer, struct mt_wdt_cpu, hw_timer);
	wdt = cpu->wdt;

	spin_lock(&wdt->lock);

	if (!wdt->stopping) {
		const u32 now = mt_wdt_gpt_msecs(&wdt->gpt);
		u32 sw_oldest_tick = U32_MAX, stall_period;
		int i;

		cpu->hw_last_tick = now;

		for_each_possible_cpu(i) {
			const struct mt_wdt_cpu *c = &wdt->cpu[i];

			if (sw_oldest_tick > c->sw_last_tick)
				sw_oldest_tick = c->sw_last_tick;
		}

		stall_period = now - sw_oldest_tick;

		if (stall_period >= wdt->bind_threshold)
			mt_wdt_bind(wdt, raw_smp_processor_id());

		if (wdt->affinity_notify.irq == MT_WDT_IRQ_NONE &&
		    stall_period >= wdt->stall_threshold)
			stalled = true;

		restart = HRTIMER_RESTART;
		mt_wdt_hw_timer_restart(wdt, cpu);
	}

	spin_unlock(&wdt->lock);

	if (stalled)
		mt_wdt_cpu_stalled(wdt);

	return restart;
}

static irqreturn_t mt_wdt_interrupt(int irq, void *dev_id)
{
	struct mt_wdt *wdt = (struct mt_wdt *)dev_id;

	mt_wdt_cpu_stalled(wdt);

	return IRQ_HANDLED;
}

static inline void
mt_wdt_sw_timer_restart(struct mt_wdt *wdt, struct mt_wdt_cpu *cpu)
{
	cpu->sw_timer_next += HZ * wdt->refresh_interval;
	mod_timer(&cpu->sw_timer, cpu->sw_timer_next);
}

static void mt_wdt_sw_timer(unsigned long data)
{
	struct mt_wdt *wdt = (struct mt_wdt *)data;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	if (!wdt->stopping) {
		const int this_cpu = raw_smp_processor_id();
		struct mt_wdt_cpu *cpu = &wdt->cpu[this_cpu];
		cpumask_t *alive_cpus = &wdt->alive_cpus;

		cpumask_set_cpu(this_cpu, alive_cpus);

		if (cpumask_full(alive_cpus)) {
			int i;

			cpumask_clear(alive_cpus);
			mt_wdt_hw_restart(wdt);
			mt_wdt_gpt_reset(&wdt->gpt);

			for_each_possible_cpu(i) {
				struct mt_wdt_cpu *c = &wdt->cpu[i];

				c->sw_last_tick = 0;
				c->hw_last_tick = 0;

				c->sw_timer_next = jiffies;
			}
		} else {
			cpu->sw_last_tick = mt_wdt_gpt_msecs(&wdt->gpt);
		}

		mt_wdt_sw_timer_restart(wdt, cpu);
	}

	spin_unlock_irqrestore(&wdt->lock, flags);
}

static inline void
mt_wdt_timers_start(void *data)
{
	unsigned long flags;
	struct mt_wdt *wdt = data;
	struct mt_wdt_cpu *cpu;

	spin_lock_irqsave(&wdt->lock, flags);

	cpu = &wdt->cpu[raw_smp_processor_id()];
	cpu->sw_timer_next = jiffies;

	mt_wdt_sw_timer_restart(wdt, cpu);
	hrtimer_start(&cpu->hw_timer,
		      ktime_set(wdt->refresh_interval, 0),
		      HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);

	spin_unlock_irqrestore(&wdt->lock, flags);
}

static inline void
mt_wdt_timers_stop(struct mt_wdt *wdt)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	mt_wdt_hw_restart(wdt);
	mt_wdt_gpt_reset(&wdt->gpt);

	wdt->stopping = true;

	spin_unlock_irqrestore(&wdt->lock, flags);

	for_each_possible_cpu(i) {
		struct mt_wdt_cpu *cpu = &wdt->cpu[i];

		del_timer_sync(&cpu->sw_timer);
		hrtimer_cancel(&cpu->hw_timer);
	}

	mt_wdt_hw_disable(wdt);
}

static void mt_wdt_irq_affinity_notify(struct irq_affinity_notify *notify,
				       const cpumask_t *mask)
{
	struct mt_wdt *wdt;
	unsigned long flags;

	wdt = container_of(notify, struct mt_wdt, affinity_notify);

	spin_lock_irqsave(&wdt->lock, flags);
	wdt->irq_cpu = cpumask_first(mask);
	spin_unlock_irqrestore(&wdt->lock, flags);
}

static void mt_wdt_irq_affinity_release(struct kref *ref)
{
}

static inline const char *
mt_wdt_status_text(const struct mt_wdt_data *data, const u32 status)
{
	u32 wdt_sts_thermal = BIT(data->wdt_sts_bit_thermal);

	switch (status) {
	case MT_WDT_STATUS_HW_RST:
		return "power-on reset";
	case MT_WDT_STATUS_SPM_THERMAL_RST:
		return "system power manager thermal reset";
	case MT_WDT_STATUS_SPM_WDT_RST:
		return "system power manager timeout";
	case MT_WDT_STATUS_SECURITY_RST:
		return "security reset";
	case MT_WDT_STATUS_SW_WDT_RST:
		return "software reset";
	case MT_WDT_STATUS_IRQ_WDT_RST:
	case MT_WDT_STATUS_HW_WDT_RST:
	case MT_WDT_STATUS_HW_IRQ_WDT_RST:
		return "hardware watchdog reset";
	}

	if (status & wdt_sts_thermal)
		return "thermal controller reset";

	return NULL;
}

static inline u8 __iomem *
mt_wdt_devm_ioremap_gpt_base(const struct mt_wdt *wdt)
{
	int ret;
	struct resource res;
	const struct device_node *wdt_node = wdt->dev->of_node;
	struct device_node *gpt_node;
	void __iomem *base;
	struct clk *clk_src;

	if (wdt_node == NULL)
		return ERR_PTR(-ENOENT);

	gpt_node = of_parse_phandle(wdt_node, MT_WDT_GPT_NODE, 0);
	if (gpt_node == NULL)
		return ERR_PTR(-ENODEV);

	clk_src = of_clk_get(gpt_node, 0);
	if (!IS_ERR(clk_src)) {
		clk_prepare_enable(clk_src);
		clk_put(clk_src);
	}

	ret = of_address_to_resource(gpt_node, 0, &res);
	of_node_put(gpt_node);

	if (ret != 0)
		return ERR_PTR(ret);

	base = devm_ioremap(wdt->dev, res.start, resource_size(&res));
	if (!base)
		return ERR_PTR(-ENOMEM);

	return base;
}

static int mt_wdt_show_power_status(const struct mt_wdt *wdt,
				    u8 __iomem *gpt_base)
{
	const u32 status = ioread32(gpt_base + wdt->data->gpt_status_reg);
	const char *status_text = mt_wdt_status_text(wdt->data, status);

	if (status_text == NULL) {
		mt_wdt_err(wdt, "SoC power status: unknown (%08lx)\n",
			   (unsigned long)status);
	} else if (status == MT_WDT_STATUS_HW_RST ||
		   status == MT_WDT_STATUS_SW_WDT_RST) {
		mt_wdt_info(wdt, "SoC power status: %s\n", status_text);
	} else {
		mt_wdt_warn(wdt, "SoC power status: %s\n", status_text);
	}

	return 0;
}

static int mt_wdt_irq_init(struct mt_wdt *wdt, struct platform_device *pdev)
{
	bool bound;
	int ret, this_cpu;
	struct irq_affinity_notify *an = &wdt->affinity_notify;
	const int irq = platform_get_irq(pdev, 0);

	an->irq = MT_WDT_IRQ_NONE;

	if (irq < 0)
		return 0; /* not configured */

	ret = request_irq(irq, mt_wdt_interrupt, 0, "watchdog", wdt);
	if (ret < 0)
		return 0; /* blocked for a non-secure mode */

	an->irq = irq;
	an->notify = mt_wdt_irq_affinity_notify;
	an->release = mt_wdt_irq_affinity_release;

	ret = irq_set_affinity_notifier(irq, an);
	if (ret != 0) {
		mt_wdt_err(wdt,
			   "unable to setup an IRQ#%u "
			   "affinity notifier: %i\n",
			   irq, ret);
		goto err_free_irq;
	}

	this_cpu = get_cpu();
	bound = mt_wdt_bind(wdt, this_cpu);
	put_cpu();

	if (!bound) {
		pr_err("unable to bind IRQ#%u to CPU%u\n", irq, this_cpu);
		ret = -EINVAL;
		goto err_remove_notifier;
	}

	mt_wdt_info(wdt, "requested IRQ#%u\n", irq);

	return 0;

err_remove_notifier:
	irq_set_affinity_notifier(irq, NULL);

err_free_irq:
	free_irq(irq, wdt);
	an->irq = MT_WDT_IRQ_NONE;

	return ret;
}

static void mt_wdt_irq_fini(struct mt_wdt *wdt)
{
	struct irq_affinity_notify *an = &wdt->affinity_notify;

	if (an->irq != MT_WDT_IRQ_NONE) {
		irq_set_affinity_notifier(an->irq, NULL);
		free_irq(an->irq, wdt);
		an->irq = MT_WDT_IRQ_NONE;
	}
}

static int mt_wdt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 refresh_interval_msecs;
	struct mt_wdt *wdt;
	struct resource *res;
	unsigned long flags;
	unsigned int i, timeout;
	u8 __iomem *gpt_base;
	int ret;

	if (np == NULL) {
		dev_err(&pdev->dev, "no device tree description found\n");
		return -ENODEV;
	}

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (wdt == NULL)
		return -ENOMEM;

	wdt->data = of_device_get_match_data(&pdev->dev);
	if (wdt->data == NULL)
		return -EINVAL;

	platform_set_drvdata(pdev, wdt);

	wdt->affinity_notify.irq = MT_WDT_IRQ_NONE;
	wdt->irq_cpu = INT_MAX;

	spin_lock_init(&wdt->lock);
	cpumask_clear(&wdt->alive_cpus);
	wdt->dev = &pdev->dev;
	wdt->stopping = false;

	for_each_possible_cpu(i) {
		struct mt_wdt_cpu *cpu = &wdt->cpu[i];
		struct timer_list *timer = &cpu->sw_timer;
		struct hrtimer *hrtimer = &cpu->hw_timer;

		setup_timer(timer, mt_wdt_sw_timer, (unsigned long)wdt);
		timer->flags = TIMER_PINNED;

		hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hrtimer->function = mt_wdt_hw_timer;

		atomic_set(&cpu->backtraced, 0);
		cpu->wdt = wdt;
		cpu->sw_last_tick = 0;
		cpu->hw_last_tick = 0;
		cpu->stall_period = 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		mt_wdt_err(wdt, "unable to find watchdog resources\n");
		return -ENODEV;
	}

	wdt->base = devm_ioremap_resource(wdt->dev, res);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	gpt_base = mt_wdt_devm_ioremap_gpt_base(wdt);
	if (IS_ERR(gpt_base)) {
		mt_wdt_err(wdt, "unable to get GPT resources\n");
		return PTR_ERR(gpt_base);
	}

	ret = mt_wdt_show_power_status(wdt, gpt_base);
	if (ret != 0)
		return ret;

	ret = of_property_read_u32(np, MT_WDT_TIMEOUT_SEC_PROP, &timeout);
	if (ret != 0) {
		if (ret != -EINVAL) {
			mt_wdt_err(wdt,
				   "wrong \"" MT_WDT_TIMEOUT_SEC_PROP
				   "\" value\n");
			return ret;
		}

		/* no value defined */
		timeout = MT_WDT_LENGTH_SECONDS_MAX;
	} else if (timeout < MT_WDT_LENGTH_SECONDS_MIN ||
		   timeout > MT_WDT_LENGTH_SECONDS_MAX) {
		mt_wdt_err(wdt, "\"" MT_WDT_TIMEOUT_SEC_PROP "\" value %u "
			   "is out of range [%i, %i]\n", timeout,
			   MT_WDT_LENGTH_SECONDS_MIN,
			   MT_WDT_LENGTH_SECONDS_MAX);
		return -EINVAL;
	} else if (timeout % MT_WDT_LENGTH_GRANULARITY) {
		mt_wdt_err(wdt, "\"" MT_WDT_TIMEOUT_SEC_PROP "\" value %u "
			   "is not a multiple of %i\n", timeout,
			   MT_WDT_LENGTH_GRANULARITY);
		return -EINVAL;
	}

	wdt->refresh_interval = MT_WDT_LENGTH_GRANULARITY;
	refresh_interval_msecs = MT_WDT_SEC_TO_MSECS(wdt->refresh_interval);
	wdt->bind_threshold = MT_WDT_SEC_TO_MSECS(timeout) -
			      refresh_interval_msecs * 2;
	wdt->stall_threshold = MT_WDT_SEC_TO_MSECS(timeout) -
			       refresh_interval_msecs * 3 / 2;

	ret = mt_wdt_irq_init(wdt, pdev);
	if (ret != 0)
		return ret;

	spin_lock_irqsave(&wdt->lock, flags);

	mt_wdt_hw_configure(wdt, MT_WDT_SEC_TO_MSECS(timeout));
	mt_wdt_hw_enable(wdt, wdt->affinity_notify.irq != MT_WDT_IRQ_NONE);
	mt_wdt_hw_restart(wdt);
	mt_wdt_gpt_init(&wdt->gpt, gpt_base, wdt->data);

	spin_unlock_irqrestore(&wdt->lock, flags);

	ret = on_each_cpu(mt_wdt_timers_start, wdt, 1);
	if (ret != 0) {
		mt_wdt_err(wdt, "unable to start timers: %i\n", ret);

		mt_wdt_timers_stop(wdt);
		mt_wdt_gpt_fini(&wdt->gpt);
		mt_wdt_irq_fini(wdt);
		return ret;
	}

	mt_wdt_info(wdt, "activated with %u second delay\n", timeout);
	return 0;
}

static int mt_wdt_remove(struct platform_device *pdev)
{
	struct mt_wdt *wdt = platform_get_drvdata(pdev);

	mt_wdt_timers_stop(wdt);
	mt_wdt_gpt_fini(&wdt->gpt);
	mt_wdt_irq_fini(wdt);
	mt_wdt_info(wdt, "deactivated\n");

	return 0;
}

static const struct of_device_id MT_WDT_DT_IDS[] = {
	{ .compatible = "mediatek,mt7622-wdt", .data = &mt7622_data },
	{ .compatible = "mediatek,mt7986-wdt", .data = &mt7986_data },
	{ .compatible = "mediatek,mt7988-wdt", .data = &mt7988_data },
	{ /* sentinel */		      }
};
MODULE_DEVICE_TABLE(of, MT_WDT_DT_IDS);

static struct platform_driver mt_wdt_driver = {
	.probe		= mt_wdt_probe,
	.remove		= mt_wdt_remove,
	.driver		= {
		.name		= MT_WDT_DRV_NAME,
		.of_match_table	= MT_WDT_DT_IDS,
	},
};
module_platform_driver(mt_wdt_driver);

MODULE_DESCRIPTION("MediaTek MT7622 watchdog");
MODULE_AUTHOR("Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_LICENSE("GPL");
