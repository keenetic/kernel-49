// SPDX-License-Identifier: GPL-2.0-only

#define AIROHA_WDT_DRV_NAME			  "airoha-wdt"
#define pr_fmt(fmt)				  AIROHA_WDT_DRV_NAME ": " fmt

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

#define AIROHA_WDT_DUMP_BYTES			  (128)
#define AIROHA_WDT_DUMP_LINE_WORDS_32		  (8)
#define AIROHA_WDT_DUMP_LINES			  \
	(AIROHA_WDT_DUMP_BYTES / (AIROHA_WDT_DUMP_LINE_WORDS_32 * sizeof(u32)))

#define AIROHA_WDT_DUMP_CPU_REG_LWORDS		  (4)

#define AIROHA_WDT_1SEC				  (1000)

#define AIROHA_WDT_IRQ_NONE			  (-1)

/* valid values are [9, 30] with 3 seconds granularity */
#define AIROHA_WDT_TIMEOUT_SEC_PROP		  "timeout-sec"
#define AIROHA_WDT_TIMEOUT_GRANULARITY		  (3)
#define AIROHA_WDT_TIMEOUT_MIN_PERIODS		  (3)
#define AIROHA_WDT_TIMEOUT_MAX_PERIODS		  (10)

#define AIROHA_WDT_SEC_TO_MSECS(sec)		  ((sec) * AIROHA_WDT_1SEC)

#define AIROHA_WDT_LENGTH_GRANULARITY		  \
	AIROHA_WDT_TIMEOUT_GRANULARITY
#define AIROHA_WDT_LENGTH_SECONDS_MIN		  \
	(AIROHA_WDT_TIMEOUT_MIN_PERIODS * AIROHA_WDT_TIMEOUT_GRANULARITY)

#define AIROHA_WDT_IRQ_DELAY			  (2 * AIROHA_WDT_1SEC)

#define AIROHA_TIMER_GPT			  (1)	/* 2 - reserved */
#define AIROHA_TIMER_WDG			  (5)

#define AIROHA_TIMER_CTRL			  (0x00)
#define AIROHA_TIMER_LVR(n)			  (0x04 + (n) * 8)
#define AIROHA_TIMER_CVR(n)			  (0x08 + (n) * 8)
#define AIROHA_TIMER_WDOGTHSLD			  (0x34)
#define AIROHA_TIMER_RLDWDOG			  (0x38)

#define AIROHA_TIMER_CTRL_WDG_ENABLE		  BIT(25)
#define AIROHA_TIMER_CTRL_INT_ACK(n)		  (1U << (16 + (n)))
#define AIROHA_TIMER_CTRL_INT_ACK_ALL		  (0x3f << 16)
#define AIROHA_TIMER_CTRL_ENABLE(n)		  (1U << (0  + (n)))

#define AIROHA_WDT_BACKTRACE_POLL_RETRIES	  (20)
#define AIROHA_WDT_BACKTRACE_POLL_DELAY_MSECS	  (50)

#define AIROHA_WDT_SCU_NODE			  "airoha,scu"

#define AIROHA_SCU_DRAMC_CONF			  (0x074)
#define AIROHA_SCU_DRAMC_EN			  BIT(2)

#define AIROHA_SCU_PMC				  (0x080)
#define AIROHA_SCU_PMC_WDG_RST_R		  BIT(31)
#define AIROHA_SCU_PMC_SYS_RST_R		  BIT(30)

#define AIROHA_SCU_SCREG_WR1			  (0x284)
#define AIROHA_SCU_SCREG_SYS_HCLK(v)		  (((v) >> 10) & 0x3ff)

#define airoha_wdt_info(wdt, fmt, arg...)	  dev_info(wdt->dev, fmt, ##arg)
#define airoha_wdt_warn(wdt, fmt, arg...)	  dev_warn(wdt->dev, fmt, ##arg)
#define airoha_wdt_err(wdt, fmt, arg...)	  dev_err(wdt->dev, fmt, ##arg)

struct airoha_wdt;

struct airoha_wdt_cpu {
	struct airoha_wdt		 *wdt;
	atomic_t			  backtraced;
	unsigned long			  sw_timer_next;
	struct timer_list		  sw_timer;
	struct hrtimer			  hw_timer;
	u32				  hw_last_tick;
	u32				  sw_last_tick;
	u32				  stall_period;
} ____cacheline_aligned_in_smp;

struct airoha_wdt_data {
	bool stop_dramc;
};

struct airoha_wdt {
	spinlock_t			  lock;
	cpumask_t			  alive_cpus;
	bool				  stopping;
	u32				  refresh_interval;
	u32				  bind_threshold;
	u32				  stall_threshold;
	int				  irq_cpu;
	u32				  sys_hclk;
	u8 __iomem			 *base;
	u8 __iomem			 *scu_base;
	struct device			 *dev;
	struct irq_affinity_notify	  affinity_notify;
	struct airoha_wdt_cpu		  cpu[NR_CPUS];
	const struct airoha_wdt_data	 *data;
};

static inline bool airoha_wdt_bind(struct airoha_wdt *wdt, const int alive_cpu)
{
	if (wdt->irq_cpu == alive_cpu)
		return true;

	if (irq_set_affinity(wdt->affinity_notify.irq,
			     get_cpu_mask(alive_cpu)) != 0)
		return false;

	wdt->irq_cpu = alive_cpu;
	return true;
}

static inline u32 airoha_wdt_timer_get_ticks_in_ms(const struct airoha_wdt *wdt)
{
	return wdt->sys_hclk * 500;
}

static u32 airoha_wdt_timer_get_max_interval(const struct airoha_wdt *wdt)
{
	u32 interval_max = U32_MAX / airoha_wdt_timer_get_ticks_in_ms(wdt);

	interval_max /= AIROHA_WDT_TIMEOUT_GRANULARITY;
	interval_max *= AIROHA_WDT_TIMEOUT_GRANULARITY;

	return interval_max;
}

static void airoha_wdt_gpt_reset(const struct airoha_wdt *wdt)
{
	u8 __iomem *reg;
	u32 val;

	/* timer disable */
	reg = wdt->base + AIROHA_TIMER_CTRL;
	val = ioread32(reg);
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val &= ~AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_GPT);
	iowrite32(val, reg);

	/* timer load value */
	reg = wdt->base + AIROHA_TIMER_LVR(AIROHA_TIMER_GPT);
	iowrite32(U32_MAX, reg);

	/* timer enable */
	reg = wdt->base + AIROHA_TIMER_CTRL;
	val = ioread32(reg);
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val |=  AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_GPT);
	iowrite32(val, reg);
}

static inline u32 airoha_wdt_gpt_msecs(const struct airoha_wdt *wdt)
{
	u8 __iomem *reg = wdt->base + AIROHA_TIMER_CVR(AIROHA_TIMER_GPT);
	u32 val = U32_MAX - ioread32(reg);

	return val / airoha_wdt_timer_get_ticks_in_ms(wdt);
}

static void
airoha_wdt_hw_configure(const struct airoha_wdt *wdt, u32 msecs, bool irq_en)
{
	const u32 ticks_in_ms = airoha_wdt_timer_get_ticks_in_ms(wdt);
	u8 __iomem *reg;
	u32 val;

	reg = wdt->base + AIROHA_TIMER_CTRL;
	val = ioread32(reg);
	val &= ~AIROHA_TIMER_CTRL_WDG_ENABLE;
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val &= ~AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_WDG);
	iowrite32(val, reg);

	reg = wdt->base + AIROHA_TIMER_LVR(AIROHA_TIMER_WDG);
	val = msecs * ticks_in_ms;
	iowrite32(val, reg);

	reg = wdt->base + AIROHA_TIMER_WDOGTHSLD;
	if (irq_en) {
		val = AIROHA_WDT_IRQ_DELAY * ticks_in_ms;
		iowrite32(val, reg);
	} else
		iowrite32(U32_MAX, reg);
}

static inline void airoha_wdt_hw_enable(const struct airoha_wdt *wdt)
{
	u8 __iomem *reg = wdt->base + AIROHA_TIMER_CTRL;
	u32 val;

	val = ioread32(reg);
	val |=  AIROHA_TIMER_CTRL_WDG_ENABLE;
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val |=  AIROHA_TIMER_CTRL_INT_ACK(AIROHA_TIMER_WDG);
	val |=  AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_WDG);
	iowrite32(val, reg);
}

static inline void airoha_wdt_hw_disable(const struct airoha_wdt *wdt)
{
	u8 __iomem *reg = wdt->base + AIROHA_TIMER_CTRL;
	u32 val;

	val = ioread32(reg);
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val &= ~AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_WDG);
	val &= ~AIROHA_TIMER_CTRL_ENABLE(AIROHA_TIMER_GPT);
	iowrite32(val, reg);
}

static inline void airoha_wdt_hw_restart(const struct airoha_wdt *wdt)
{
	iowrite32(0x1, wdt->base + AIROHA_TIMER_RLDWDOG);
}

static inline void airoha_wdt_hw_ack_irq(const struct airoha_wdt *wdt)
{
	u8 __iomem *reg = wdt->base + AIROHA_TIMER_CTRL;
	u32 val;

	val = ioread32(reg);
	val &= ~AIROHA_TIMER_CTRL_INT_ACK_ALL;
	val |=  AIROHA_TIMER_CTRL_INT_ACK(AIROHA_TIMER_WDG);
	iowrite32(val, reg);
}

static inline bool airoha_wdt_is_stalled(const struct airoha_wdt *wdt,
					 const u32 time, const u32 now)
{
	BUG_ON(now < time);

	return now - time >= wdt->stall_threshold;
}

#ifdef CONFIG_ARM64
static inline int airoha_wdt_unwind_frame(struct stackframe *frame)
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

static void airoha_wdt_dump_data(const unsigned long addr,
				 const char *const name)
{
	size_t i;
	u32 *p = (u32 *)((addr & ~(sizeof(*p) - 1)) - AIROHA_WDT_DUMP_BYTES / 2);

	printk("\n%s: %pS:\n", name, (void *)addr);

	for (i = 0; i < AIROHA_WDT_DUMP_LINES; i++) {
		size_t j;

		printk("%04lx:", (unsigned long)p & 0xffff);

		for (j = 0; j < AIROHA_WDT_DUMP_LINE_WORDS_32; j++, p++) {
			u32 data;

			if (probe_kernel_address(p, data))
				pr_cont(" ????????");
			else
				pr_cont(" %08x", data);
		}

		printk("\n");
	}
}
#endif

static void airoha_wdt_dump_cpu_state(void *info)
{
	struct airoha_wdt_cpu *cpu = info;
#ifdef CONFIG_ARM64
	struct pt_regs *regs = get_irq_regs();
	struct stackframe frame = {
		.fp = frame_pointer(regs),
		.sp = kernel_stack_pointer(regs),
		.pc = instruction_pointer(regs)
	};
	size_t i = 0;
#endif

	printk("\nCPU%i stall period is %u ms.\n",
	       raw_smp_processor_id(), cpu->stall_period);

	show_regs_print_info(KERN_DEFAULT);

#ifdef CONFIG_ARM64
	printk("pstate: %08llx\n", regs->pstate);

	while (i < ARRAY_SIZE(regs->regs)) {
		const char *ident = "";
		size_t j;

		printk(" x%-2zu:", i);

		for (j = 0; j < AIROHA_WDT_DUMP_CPU_REG_LWORDS; j++, i++) {
			if (i >= ARRAY_SIZE(regs->regs))
				break;

			pr_cont(" %016llx%s", regs->regs[i], ident);
			ident = " ";
		}

		printk("\n");
	}

	if (!user_mode(regs)) {
		airoha_wdt_dump_data(regs->pc, "PC");
		airoha_wdt_dump_data(regs->sp, "SP");
	}

	printk("\nCall trace:\n");

	while (1) {
		int ret;

		printk("[<%p>] %pS\n", (void *)frame.pc, (void *)frame.pc);
		ret = airoha_wdt_unwind_frame(&frame);

		if (ret < 0 || frame.fp == 0)
			break;
	}
#endif

	atomic_inc(&cpu->backtraced);
}

static void airoha_wdt_cpu_stalled(struct airoha_wdt *wdt)
{
	struct cpumask soft_stalled;
	struct cpumask hard_stalled;
	char hard_stalled_list[16];
	u32 now;
	int i;

	spin_lock(&wdt->lock);

	now = airoha_wdt_gpt_msecs(wdt);
	wdt->stopping = true;
	cpumask_clear(&soft_stalled);
	cpumask_clear(&hard_stalled);

	for_each_possible_cpu(i) {
		struct airoha_wdt_cpu *cpu = &wdt->cpu[i];

		cpu->stall_period = now - cpu->sw_last_tick;

		if (airoha_wdt_is_stalled(wdt, cpu->sw_last_tick, now))
			cpumask_set_cpu(i, &soft_stalled);

		if (airoha_wdt_is_stalled(wdt, cpu->hw_last_tick, now))
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
		struct airoha_wdt_cpu *cpu = &wdt->cpu[i];
		size_t j;

		if (!cpumask_test_cpu(i, &soft_stalled)) {
			printk("\nCPU%i is not stalled\n", i);
			continue;
		}

		if (i == raw_smp_processor_id()) {
			airoha_wdt_dump_cpu_state(cpu);
			continue;
		}

		if (cpumask_test_cpu(i, &hard_stalled)) {
			printk("\nCPU%i stalls with disabled "
			       "hardware interrupts\n", i);
			continue;
		}

		smp_call_function_single(i, airoha_wdt_dump_cpu_state, cpu, 0);

		for (j = 0; j < AIROHA_WDT_BACKTRACE_POLL_RETRIES; j++) {
			mdelay(AIROHA_WDT_BACKTRACE_POLL_DELAY_MSECS);

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
	airoha_wdt_hw_configure(wdt, 50, false);
	airoha_wdt_hw_enable(wdt);
	airoha_wdt_hw_restart(wdt);

	/* disable DRAM controller */
	if (wdt->data->stop_dramc) {
		u32 val = ioread32(wdt->scu_base + AIROHA_SCU_DRAMC_CONF);

		val &= ~AIROHA_SCU_DRAMC_EN;
		iowrite32(val, wdt->scu_base + AIROHA_SCU_DRAMC_CONF);
	}

	while (1)
		cpu_relax();
}

static inline void
airoha_wdt_hw_timer_restart(struct airoha_wdt *wdt, struct airoha_wdt_cpu *cpu)
{
	hrtimer_forward_now(&cpu->hw_timer,
			    ktime_set(wdt->refresh_interval, 0));
}

static enum hrtimer_restart
airoha_wdt_hw_timer(struct hrtimer *hrtimer)
{
	enum hrtimer_restart restart = HRTIMER_NORESTART;
	bool stalled = false;
	struct airoha_wdt_cpu *cpu;
	struct airoha_wdt *wdt;

	cpu = container_of(hrtimer, struct airoha_wdt_cpu, hw_timer);
	wdt = cpu->wdt;

	spin_lock(&wdt->lock);

	if (!wdt->stopping) {
		const u32 now = airoha_wdt_gpt_msecs(wdt);
		u32 sw_oldest_tick = U32_MAX, stall_period;
		int i;

		cpu->hw_last_tick = now;

		for_each_possible_cpu(i) {
			const struct airoha_wdt_cpu *c = &wdt->cpu[i];

			if (sw_oldest_tick > c->sw_last_tick)
				sw_oldest_tick = c->sw_last_tick;
		}

		stall_period = now - sw_oldest_tick;

		if (stall_period >= wdt->bind_threshold)
			airoha_wdt_bind(wdt, raw_smp_processor_id());

		if (wdt->affinity_notify.irq == AIROHA_WDT_IRQ_NONE &&
		    stall_period >= wdt->stall_threshold)
			stalled = true;

		restart = HRTIMER_RESTART;
		airoha_wdt_hw_timer_restart(wdt, cpu);
	}

	spin_unlock(&wdt->lock);

	if (stalled)
		airoha_wdt_cpu_stalled(wdt);

	return restart;
}

static irqreturn_t airoha_wdt_interrupt(int irq, void *dev_id)
{
	struct airoha_wdt *wdt = (struct airoha_wdt *)dev_id;

	airoha_wdt_hw_ack_irq(wdt);

	airoha_wdt_cpu_stalled(wdt);

	return IRQ_HANDLED;
}

static inline void
airoha_wdt_sw_timer_restart(struct airoha_wdt *wdt, struct airoha_wdt_cpu *cpu)
{
	cpu->sw_timer_next += HZ * wdt->refresh_interval;
	mod_timer(&cpu->sw_timer, cpu->sw_timer_next);
}

static void airoha_wdt_sw_timer(unsigned long data)
{
	struct airoha_wdt *wdt = (struct airoha_wdt *)data;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	if (!wdt->stopping) {
		const int this_cpu = raw_smp_processor_id();
		struct airoha_wdt_cpu *cpu = &wdt->cpu[this_cpu];
		cpumask_t *alive_cpus = &wdt->alive_cpus;

		cpumask_set_cpu(this_cpu, alive_cpus);

		if (cpumask_full(alive_cpus)) {
			int i;

			cpumask_clear(alive_cpus);
			airoha_wdt_hw_restart(wdt);
			airoha_wdt_gpt_reset(wdt);

			for_each_possible_cpu(i) {
				struct airoha_wdt_cpu *c = &wdt->cpu[i];

				c->sw_last_tick = 0;
				c->hw_last_tick = 0;

				c->sw_timer_next = jiffies;
			}
		} else {
			cpu->sw_last_tick = airoha_wdt_gpt_msecs(wdt);
		}

		airoha_wdt_sw_timer_restart(wdt, cpu);
	}

	spin_unlock_irqrestore(&wdt->lock, flags);
}

static inline void
airoha_wdt_timers_start(void *data)
{
	unsigned long flags;
	struct airoha_wdt *wdt = data;
	struct airoha_wdt_cpu *cpu;

	spin_lock_irqsave(&wdt->lock, flags);

	cpu = &wdt->cpu[raw_smp_processor_id()];
	cpu->sw_timer_next = jiffies;

	airoha_wdt_sw_timer_restart(wdt, cpu);
	hrtimer_start(&cpu->hw_timer,
		      ktime_set(wdt->refresh_interval, 0),
		      HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);

	spin_unlock_irqrestore(&wdt->lock, flags);
}

static inline void
airoha_wdt_timers_stop(struct airoha_wdt *wdt)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	airoha_wdt_hw_restart(wdt);
	airoha_wdt_gpt_reset(wdt);

	wdt->stopping = true;

	spin_unlock_irqrestore(&wdt->lock, flags);

	for_each_possible_cpu(i) {
		struct airoha_wdt_cpu *cpu = &wdt->cpu[i];

		del_timer_sync(&cpu->sw_timer);
		hrtimer_cancel(&cpu->hw_timer);
	}

	airoha_wdt_hw_disable(wdt);
}

static void airoha_wdt_irq_affinity_notify(struct irq_affinity_notify *notify,
					const cpumask_t *mask)
{
	struct airoha_wdt *wdt;
	unsigned long flags;

	wdt = container_of(notify, struct airoha_wdt, affinity_notify);

	spin_lock_irqsave(&wdt->lock, flags);
	wdt->irq_cpu = cpumask_first(mask);
	spin_unlock_irqrestore(&wdt->lock, flags);
}

static void airoha_wdt_irq_affinity_release(struct kref *ref)
{
}

static inline void airoha_wdt_show_power_status(struct airoha_wdt *wdt)
{
	const u32 pmc = ioread32(wdt->scu_base + AIROHA_SCU_PMC);
	const char *status = "power-on reset";

	if (pmc & AIROHA_SCU_PMC_WDG_RST_R)
		status = "hardware watchdog reset";
	else if (pmc & AIROHA_SCU_PMC_SYS_RST_R)
		status = "software reset";

	airoha_wdt_info(wdt, "SoC power status: %s\n", status);

	/* clear PMC status */
	if (pmc & (AIROHA_SCU_PMC_WDG_RST_R | AIROHA_SCU_PMC_SYS_RST_R))
		iowrite32(pmc, wdt->scu_base + AIROHA_SCU_PMC);
}

static inline void airoha_wdt_get_sysclk(struct airoha_wdt *wdt)
{
	const u32 val = ioread32(wdt->scu_base + AIROHA_SCU_SCREG_WR1);
	u32 sys_hclk = AIROHA_SCU_SCREG_SYS_HCLK(val);

	if (sys_hclk < 100)
		sys_hclk = 100;

	wdt->sys_hclk = sys_hclk;
}

static inline u8 __iomem *
airoha_wdt_devm_ioremap_scu_base(struct airoha_wdt *wdt)
{
	const struct device_node *wdt_node = wdt->dev->of_node;
	struct device_node *scu_node;
	struct resource res;
	void __iomem *base;
	int ret;

	if (wdt_node == NULL)
		return ERR_PTR(-ENOENT);

	scu_node = of_parse_phandle(wdt_node, AIROHA_WDT_SCU_NODE, 0);
	if (scu_node == NULL)
		return ERR_PTR(-ENODEV);

	ret = of_address_to_resource(scu_node, 0, &res);
	of_node_put(scu_node);

	if (ret != 0)
		return ERR_PTR(ret);

	base = devm_ioremap(wdt->dev, res.start, resource_size(&res));
	if (!base)
		return ERR_PTR(-ENOMEM);

	return base;
}

static int airoha_wdt_irq_init(struct airoha_wdt *wdt,
			       struct platform_device *pdev)
{
	bool bound;
	int ret, this_cpu;
	struct irq_affinity_notify *an = &wdt->affinity_notify;
	const int irq = platform_get_irq(pdev, 0);

	an->irq = AIROHA_WDT_IRQ_NONE;

	if (irq < 0)
		return -ENODEV;

	ret = request_irq(irq, airoha_wdt_interrupt, 0, "watchdog", wdt);
	if (ret < 0)
		return ret;

	an->irq = irq;
	an->notify = airoha_wdt_irq_affinity_notify;
	an->release = airoha_wdt_irq_affinity_release;

	ret = irq_set_affinity_notifier(irq, an);
	if (ret != 0) {
		airoha_wdt_err(wdt, "unable to setup an IRQ#%u "
			       "affinity notifier: %i\n", irq, ret);
		goto err_free_irq;
	}

	this_cpu = get_cpu();
	bound = airoha_wdt_bind(wdt, this_cpu);
	put_cpu();

	if (!bound) {
		pr_err("unable to bind IRQ#%u to CPU%u\n", irq, this_cpu);
		ret = -EINVAL;
		goto err_remove_notifier;
	}

	airoha_wdt_info(wdt, "requested IRQ#%u\n", irq);

	return 0;

err_remove_notifier:
	irq_set_affinity_notifier(irq, NULL);

err_free_irq:
	free_irq(irq, wdt);
	an->irq = AIROHA_WDT_IRQ_NONE;

	return ret;
}

static void airoha_wdt_irq_fini(struct airoha_wdt *wdt)
{
	struct irq_affinity_notify *an = &wdt->affinity_notify;

	if (an->irq != AIROHA_WDT_IRQ_NONE) {
		irq_set_affinity_notifier(an->irq, NULL);
		free_irq(an->irq, wdt);
		an->irq = AIROHA_WDT_IRQ_NONE;
	}
}

static int airoha_wdt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 refresh_interval_msecs;
	struct airoha_wdt *wdt;
	struct resource *res;
	unsigned long flags;
	unsigned int i, timeout, timeout_max;
	int ret;

	if (np == NULL) {
		dev_err(&pdev->dev, "no device tree description found\n");
		return -ENODEV;
	}

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (wdt == NULL)
		return -ENOMEM;

	wdt->data = of_device_get_match_data(&pdev->dev);
	if (wdt->data == NULL) {
		dev_err(&pdev->dev, "no mach data!!!\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, wdt);

	wdt->affinity_notify.irq = AIROHA_WDT_IRQ_NONE;
	wdt->irq_cpu = INT_MAX;
	wdt->sys_hclk = 300;

	spin_lock_init(&wdt->lock);
	cpumask_clear(&wdt->alive_cpus);
	wdt->dev = &pdev->dev;
	wdt->stopping = false;

	for_each_possible_cpu(i) {
		struct airoha_wdt_cpu *cpu = &wdt->cpu[i];
		struct timer_list *timer = &cpu->sw_timer;
		struct hrtimer *hrtimer = &cpu->hw_timer;

		setup_timer(timer, airoha_wdt_sw_timer, (unsigned long)wdt);
		timer->flags = TIMER_PINNED;

		hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hrtimer->function = airoha_wdt_hw_timer;

		atomic_set(&cpu->backtraced, 0);
		cpu->wdt = wdt;
		cpu->sw_last_tick = 0;
		cpu->hw_last_tick = 0;
		cpu->stall_period = 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt->base = devm_ioremap_resource(wdt->dev, res);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	wdt->scu_base = airoha_wdt_devm_ioremap_scu_base(wdt);
	if (IS_ERR(wdt->scu_base)) {
		airoha_wdt_err(wdt, "unable to get SCU resource\n");
		return PTR_ERR(wdt->scu_base);
	}

	airoha_wdt_get_sysclk(wdt);
	airoha_wdt_show_power_status(wdt);

	timeout_max = airoha_wdt_timer_get_max_interval(wdt);

	ret = of_property_read_u32(np, AIROHA_WDT_TIMEOUT_SEC_PROP, &timeout);
	if (ret != 0) {
		if (ret != -EINVAL) {
			airoha_wdt_err(wdt,
				       "wrong \"" AIROHA_WDT_TIMEOUT_SEC_PROP
				       "\" value\n");
			return ret;
		}

		/* no value defined */
		timeout = timeout_max;
	} else if (timeout < AIROHA_WDT_LENGTH_SECONDS_MIN ||
		   timeout > timeout_max) {
		airoha_wdt_warn(wdt,
				 "\"" AIROHA_WDT_TIMEOUT_SEC_PROP "\" value %u "
				"is out of range [%i, %i]\n", timeout,
				AIROHA_WDT_LENGTH_SECONDS_MIN, timeout_max);
		timeout = timeout_max;
	} else if (timeout % AIROHA_WDT_LENGTH_GRANULARITY) {
		airoha_wdt_warn(wdt,
				"\"" AIROHA_WDT_TIMEOUT_SEC_PROP "\" value %u "
				"is not a multiple of %i\n", timeout,
				AIROHA_WDT_LENGTH_GRANULARITY);
		timeout /= AIROHA_WDT_TIMEOUT_GRANULARITY;
		timeout *= AIROHA_WDT_TIMEOUT_GRANULARITY;
	}

	wdt->refresh_interval = AIROHA_WDT_LENGTH_GRANULARITY;
	refresh_interval_msecs = AIROHA_WDT_SEC_TO_MSECS(wdt->refresh_interval);
	wdt->bind_threshold = AIROHA_WDT_SEC_TO_MSECS(timeout) -
				refresh_interval_msecs * 2;
	wdt->stall_threshold = AIROHA_WDT_SEC_TO_MSECS(timeout) -
				refresh_interval_msecs * 3 / 2;

	ret = airoha_wdt_irq_init(wdt, pdev);
	if (ret != 0)
		return ret;

	spin_lock_irqsave(&wdt->lock, flags);

	airoha_wdt_hw_configure(wdt, AIROHA_WDT_SEC_TO_MSECS(timeout), true);
	airoha_wdt_hw_enable(wdt);
	airoha_wdt_hw_restart(wdt);
	airoha_wdt_gpt_reset(wdt);

	spin_unlock_irqrestore(&wdt->lock, flags);

	ret = on_each_cpu(airoha_wdt_timers_start, wdt, 1);
	if (ret != 0) {
		airoha_wdt_err(wdt, "unable to start timers: %i\n", ret);

		airoha_wdt_timers_stop(wdt);
		airoha_wdt_irq_fini(wdt);
		return ret;
	}

	airoha_wdt_info(wdt, "activated with %u second delay\n", timeout);
	return 0;
}

static int airoha_wdt_remove(struct platform_device *pdev)
{
	struct airoha_wdt *wdt = platform_get_drvdata(pdev);

	airoha_wdt_timers_stop(wdt);
	airoha_wdt_irq_fini(wdt);
	airoha_wdt_info(wdt, "deactivated\n");

	return 0;
}

static const struct airoha_wdt_data an7552_data = {
	.stop_dramc = false,
};

static const struct airoha_wdt_data an7581_data = {
	.stop_dramc = false,
};

static const struct airoha_wdt_data an7583_data = {
	.stop_dramc = true,
};

static const struct of_device_id AIROHA_WDT_DT_IDS[] = {
	{ .compatible = "airoha,an7552-wdt", .data = &an7552_data },
	{ .compatible = "airoha,an7581-wdt", .data = &an7581_data },
	{ .compatible = "airoha,an7583-wdt", .data = &an7583_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, AIROHA_WDT_DT_IDS);

static struct platform_driver airoha_wdt_driver = {
	.probe		= airoha_wdt_probe,
	.remove		= airoha_wdt_remove,
	.driver		= {
		.name		= AIROHA_WDT_DRV_NAME,
		.of_match_table	= AIROHA_WDT_DT_IDS,
	},
};
module_platform_driver(airoha_wdt_driver);

MODULE_DESCRIPTION("Airoha SoC watchdog");
MODULE_AUTHOR("Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_LICENSE("GPL");
