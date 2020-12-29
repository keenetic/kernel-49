#define pr_fmt(fmt)				"mips_wdt: " fmt

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/utsname.h>
#include <linux/cpumask.h>
#include <linux/kallsyms.h>
#include <linux/notifier.h>
#include <linux/kmsg_dump.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <asm/irq_regs.h>
#include <asm/stacktrace.h>
#include <asm/pbus-timer.h>

/* it is supposed that second timer is available
 * for a calltrace on any platform
 */
#define MIPS_WDT_TIMER_CALLTRACE		  (PBUS_TIMER_2)
#define MIPS_WDT_TIMER_CALLTRACE_IRQ		  (PBUS_TIMER_2_IRQ)

#define MIPS_WDT_TIMER				  (PBUS_TIMER_WATCHDOG)

#define MIPS_WDT_IRQ_CPU			  (0)

#define MIPS_WDT_1SEC				  (1000)

#define MIPS_WDT_SEC_REBOOT			  \
	(CONFIG_MIPS_WDT_REBOOT_DELAY)
#define MIPS_WDT_SEC_REFRESH			  \
	(CONFIG_MIPS_WDT_REFRESH_INTERVAL)
#define MIPS_WDT_SEC_CALLTRACE			  \
	((MIPS_WDT_SEC_REBOOT) - (CONFIG_MIPS_WDT_CALLTRACE_THRESHOLD))

#define MIPS_WDT_MSECS_REBOOT			  \
	(MIPS_WDT_1SEC * MIPS_WDT_SEC_REBOOT)
#define MIPS_WDT_MSECS_CALLTRACE		  \
	(MIPS_WDT_1SEC * MIPS_WDT_SEC_CALLTRACE)
#define MIPS_WDT_MSECS_MAX_STALL		  \
	(MIPS_WDT_MSECS_REBOOT / 2)

enum mips_wdt_stall {
	MIPS_WDT_STALL_SOFT,
	MIPS_WDT_STALL_HARD
};

static void mips_wdt_irq_affinity_notify(struct irq_affinity_notify *notify,
					 const cpumask_t *mask);
static void mips_wdt_irq_affinity_release(struct kref *ref);

static DEFINE_SPINLOCK(mips_wdt_lock);
static bool mips_wdt_stopping;
static unsigned int mips_wdt_interrupt_cpu = UINT_MAX;
static DEFINE_PER_CPU(struct timer_list, mips_wdt_soft_timer);
static DEFINE_PER_CPU(struct hrtimer, mips_wdt_hard_timer);
static DEFINE_PER_CPU(u32, mips_wdt_soft_stall);
static DEFINE_PER_CPU(u32, mips_wdt_hard_stall);
static struct irq_affinity_notify mips_wdt_affinity_notify = {
	.irq		= MIPS_WDT_TIMER_CALLTRACE_IRQ,
	.notify		= mips_wdt_irq_affinity_notify,
	.release	= mips_wdt_irq_affinity_release
};

static inline void
mips_wdt_show_task_regs(const struct pt_regs *regs,
			struct task_struct *task,
			const unsigned int cpu)
{
	const int field = 2 * sizeof(unsigned long);
	unsigned int cause = regs->cp0_cause;
	unsigned int exccode;
	int i;

	printk("CPU: %d PID: %d Comm: %.20s %s %s %.*s\n",
	       cpu, task->pid, task->comm,
	       print_tainted(), init_utsname()->release,
	       (int)strcspn(init_utsname()->version, " "),
	       init_utsname()->version);

	print_worker_info("", task);

	printk("task: %p task.stack: %p\n", task, task_stack_page(task));

	/* saved main processor registers
	 */
	for (i = 0; i < 32; ) {
		if ((i % 4) == 0)
			printk("$%2d   :", i);
		if (i == 0)
			pr_cont(" %0*lx", field, 0UL);
		else if (i == 26 || i == 27)
			pr_cont(" %*s", field, "");
		else
			pr_cont(" %0*lx", field, regs->regs[i]);

		i++;
		if ((i % 4) == 0)
			pr_cont("\n");
	}

	printk("Hi    : %0*lx\n", field, regs->hi);
	printk("Lo    : %0*lx\n", field, regs->lo);

	/* saved cp0 registers
	 */
	printk("epc   : %0*lx %pS\n", field, regs->cp0_epc,
	       (void *) regs->cp0_epc);
	printk("ra    : %0*lx %pS\n", field, regs->regs[31],
	       (void *) regs->regs[31]);

	printk("Status: %08x	", (uint32_t) regs->cp0_status);

	if (cpu_has_3kex) {
		if (regs->cp0_status & ST0_KUO)
			pr_cont("KUo ");
		if (regs->cp0_status & ST0_IEO)
			pr_cont("IEo ");
		if (regs->cp0_status & ST0_KUP)
			pr_cont("KUp ");
		if (regs->cp0_status & ST0_IEP)
			pr_cont("IEp ");
		if (regs->cp0_status & ST0_KUC)
			pr_cont("KUc ");
		if (regs->cp0_status & ST0_IEC)
			pr_cont("IEc ");
	} else if (cpu_has_4kex) {
		if (regs->cp0_status & ST0_KX)
			pr_cont("KX ");
		if (regs->cp0_status & ST0_SX)
			pr_cont("SX ");
		if (regs->cp0_status & ST0_UX)
			pr_cont("UX ");

		switch (regs->cp0_status & ST0_KSU) {
		case KSU_USER:
			pr_cont("USER ");
			break;
		case KSU_SUPERVISOR:
			pr_cont("SUPERVISOR ");
			break;
		case KSU_KERNEL:
			pr_cont("KERNEL ");
			break;
		default:
			pr_cont("BAD_MODE ");
			break;
		}

		if (regs->cp0_status & ST0_ERL)
			pr_cont("ERL ");
		if (regs->cp0_status & ST0_EXL)
			pr_cont("EXL ");
		if (regs->cp0_status & ST0_IE)
			pr_cont("IE ");
	}

	pr_cont("\n");

	exccode = (cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE;
	printk("Cause : %08x (ExcCode %02x)\n", cause, exccode);

	if (exccode >= 1 && exccode <= 5)
		printk("BadVA : %0*lx\n", field, regs->cp0_badvaddr);

	printk("PrId  : %08x (%s)\n", read_c0_prid(),
	       cpu_name_string());
}

static inline void
mips_wdt_show_raw_calltrace(unsigned long reg29)
{
	unsigned long *sp = (unsigned long *)(reg29 & ~3);
	unsigned long addr;

	while (!kstack_end(sp)) {
		unsigned long __user *p = (unsigned long __user *)sp++;

		if (__get_user(addr, p)) {
			printk(" (Bad stack address)");
			break;
		}

		if (__kernel_text_address(addr))
			print_ip_sym(addr);
	}

	pr_cont("\n");
}

static inline void
mips_wdt_show_stack(struct task_struct *task,
		    unsigned long sp,
		    unsigned long ra,
		    unsigned long pc)
{
	const int field = 2 * sizeof(unsigned long);
	unsigned long __user *spp = (unsigned long __user *)sp;
	long stackdata;
	int i;

	printk("Stack :");
	i = 0;

	while ((unsigned long)spp & (PAGE_SIZE - 1)) {
		if (i && ((i % (64 / field)) == 0)) {
			pr_cont("\n");
			printk("       ");
		}

		if (i > 39) {
			pr_cont(" ...");
			break;
		}

		if (__get_user(stackdata, spp++)) {
			pr_cont(" (Bad stack address)");
			break;
		}

		pr_cont(" %0*lx", field, stackdata);
		i++;
	}

	pr_cont("\n");
}

static inline void
mips_wdt_show_code(unsigned long pc)
{
	int i;

	printk("Code:");

	for (i = -3; i < 6; i++) {
		unsigned long insn;

		if (__get_user(insn, ((unsigned long *)pc) + i)) {
			printk(" (Bad address in EPC)");
			break;
		}

		pr_cont("%c%08lx%c", (i ? ' ' : '<'), insn, (i ? ' ' : '>'));
	}

	pr_cont("\n");
}

static inline unsigned int
mips_wdt_cpu_vpe(const unsigned int cpu)
{
#if defined(CONFIG_MIPS_CPS)
	return cpu_data[cpu].vpe_id;
#else
	return cpu;
#endif
}

static inline void
mips_wdt_get_last_task_regs(struct pt_regs *regs)
{
	BUG_ON(!in_interrupt());

	/* get a CPU state before an interrupt raised */
	memcpy(regs, get_irq_regs(), sizeof(*regs));
}

static inline void
mips_wdt_lock_cpu(const unsigned int cpu, int *vpflags, u32 *tchalt,
		  struct pt_regs *regs)
{
	*tchalt = TCHALT_H;
	*vpflags = 0;

	memset(regs, 0, sizeof(*regs));

#if defined(CONFIG_SMP)
	if (cpu == raw_smp_processor_id()) {
		mips_wdt_get_last_task_regs(regs);
		return;
	}

	/* enter a single VPE mode on this core */
	*vpflags = dvpe();
	settc(mips_wdt_cpu_vpe(cpu));

	if (read_tc_c0_tcbind() != read_c0_tcbind()) {
		/* reading a state of another TC, halt it temporary */
		*tchalt = read_tc_c0_tchalt();
		write_tc_c0_tchalt(TCHALT_H);
		ehb();
	}

	regs->regs[0] = mftgpr(0);
	regs->regs[1] = mftgpr(1);
	regs->regs[2] = mftgpr(2);
	regs->regs[3] = mftgpr(3);
	regs->regs[4] = mftgpr(4);
	regs->regs[5] = mftgpr(5);
	regs->regs[6] = mftgpr(6);
	regs->regs[7] = mftgpr(7);
	regs->regs[8] = mftgpr(8);
	regs->regs[9] = mftgpr(9);
	regs->regs[10] = mftgpr(10);
	regs->regs[11] = mftgpr(11);
	regs->regs[12] = mftgpr(12);
	regs->regs[13] = mftgpr(13);
	regs->regs[14] = mftgpr(14);
	regs->regs[15] = mftgpr(15);
	regs->regs[16] = mftgpr(16);
	regs->regs[17] = mftgpr(17);
	regs->regs[18] = mftgpr(18);
	regs->regs[19] = mftgpr(19);
	regs->regs[20] = mftgpr(20);
	regs->regs[21] = mftgpr(21);
	regs->regs[22] = mftgpr(22);
	regs->regs[23] = mftgpr(23);
	regs->regs[24] = mftgpr(24);
	regs->regs[25] = mftgpr(25);
	regs->regs[26] = mftgpr(26);
	regs->regs[27] = mftgpr(27);
	regs->regs[28] = mftgpr(28);
	regs->regs[29] = mftgpr(29);
	regs->regs[30] = mftgpr(30);
	regs->regs[31] = mftgpr(31);

	regs->cp0_badvaddr = read_vpe_c0_badvaddr();
	regs->cp0_cause = read_vpe_c0_cause();
	regs->cp0_status = read_tc_c0_tcstatus();
	regs->cp0_epc = read_tc_c0_tcrestart();
#else
	mips_wdt_get_last_task_regs(regs);
#endif
}

static inline void
mips_wdt_unlock_cpu(const unsigned int cpu, int *vpflags, u32 *tchalt)
{
#if defined(CONFIG_SMP)
	if (cpu == raw_smp_processor_id())
		return;

	if (*tchalt == 0) {
		write_tc_c0_tchalt(0);
		ehb();
	}

	evpe(*vpflags);
#endif
}

static inline u32
mips_wdt_now(void)
{
	/* jiffies or system hardware timers may stop to count
	 * when CPUs stall. The function returns up to MIPS_WDT_MSECS_REBOOT
	 * millisecond value.
	 */
	return MIPS_WDT_MSECS_REBOOT - pbus_timer_get(MIPS_WDT_TIMER);
}

static inline void
mips_wdt_backtrace(const unsigned int cpu,
		   const u32 stall_period)
{
	const unsigned int this_cpu = raw_smp_processor_id();
	unsigned long sp, pc;
	struct thread_info *thread;
	struct task_struct *task;
	struct pt_regs regs;
	mm_segment_t old_fs;
	int vpflags = 0;
	u32 tchalt = TCHALT_H;

	printk("CPU%u stall period is %u ms.\n", cpu, stall_period);

	if (cpu_data[cpu].core != cpu_data[this_cpu].core) {
		printk("unable to dump CPU%u state: "
		       "no alive CPUs on the same core\n", cpu);
		return;
	}

	mips_wdt_lock_cpu(cpu, &vpflags, &tchalt, &regs);

	thread = (struct thread_info *)regs.regs[28];
	task = thread->task;

	print_irqtrace_events(task);
	mips_wdt_show_task_regs(&regs, task, cpu);

	printk("Process %s (pid: %d, threadinfo=%p, task=%p, tls=%0*lx)\n",
	       task->comm, task->pid, thread, task,
	       2 * sizeof(thread->tp_value), thread->tp_value);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	sp = user_stack_pointer(&regs);
	pc = profile_pc(&regs);

	mips_wdt_show_stack(task, sp, regs.regs[31], pc);
	mips_wdt_show_raw_calltrace(sp);
	mips_wdt_show_code(pc);

	set_fs(old_fs);

	mips_wdt_unlock_cpu(cpu, &vpflags, &tchalt);
}

static inline bool
mips_wdt_is_stalled(const unsigned int cpu,
		    const enum mips_wdt_stall stall)
{
	u32 t;

	if (stall == MIPS_WDT_STALL_SOFT)
		t = per_cpu(mips_wdt_soft_stall, cpu);
	else
		t = per_cpu(mips_wdt_hard_stall, cpu);

	return t + MIPS_WDT_MSECS_MAX_STALL < mips_wdt_now();
}

static irqreturn_t mips_wdt_interrupt(int irq, void *dev_id)
{
	char hard_stalled_list[16];
	struct cpumask soft_stalled;
	struct cpumask hard_stalled;
	unsigned int i;
	u32 now;

	spin_lock(&mips_wdt_lock);

	mips_wdt_stopping = true;
	cpumask_clear(&soft_stalled);
	cpumask_clear(&hard_stalled);

	for_each_possible_cpu(i) {
		if (mips_wdt_is_stalled(i, MIPS_WDT_STALL_SOFT))
			cpumask_set_cpu(i, &soft_stalled);

		if (mips_wdt_is_stalled(i, MIPS_WDT_STALL_HARD))
			cpumask_set_cpu(i, &hard_stalled);
	}

	spin_unlock(&mips_wdt_lock);

	now = mips_wdt_now();
	console_verbose();
	bust_spinlocks(1);

	snprintf(hard_stalled_list, sizeof(hard_stalled_list), "%*pbl",
		 cpumask_pr_args(&hard_stalled));

	printk("watchdog timer interrupt on CPU%u, stalled CPUs: "
	       "%*pbl (soft), %s (hard)\n\n", raw_smp_processor_id(),
	       cpumask_pr_args(&soft_stalled),
	       (hard_stalled_list[0] == '\0') ? "none" : hard_stalled_list);

	print_modules();

	for_each_possible_cpu(i) {
		if (!cpumask_test_cpu(i, &soft_stalled))
			continue;

		mips_wdt_backtrace(i, now - per_cpu(mips_wdt_soft_stall, i));
	}

	printk_nmi_flush_on_panic();

	/*
	 * Run any panic handlers, including those that might need to
	 * add information to the kmsg dump output.
	 */
	atomic_notifier_call_chain(&panic_notifier_list,
				   PANIC_ACTION_HALT, NULL);

	/* Call flush even twice. It tries harder with a single online CPU */
	printk_nmi_flush_on_panic();
	kmsg_dump(KMSG_DUMP_PANIC);

	while (1)
		cpu_relax();

	return IRQ_HANDLED;
}

static inline unsigned int
mips_wdt_alive_cpu_for(const unsigned int stalled_cpu)
{
	/* try to select an alive CPU on the same core */
	const unsigned int core_cpus = core_nvpes();
	const unsigned int min_cpu = (stalled_cpu / core_cpus) * core_cpus;
	const unsigned int max_cpu = min_cpu + core_cpus;
	unsigned int i;

	for (i = min_cpu; i < max_cpu; i++) {
		if (mips_wdt_is_stalled(i, MIPS_WDT_STALL_HARD))
			continue;
		return i;
	}

	/* no alive CPUs found on the same core with stalled CPUs;
	 * return a knowlingly alive CPU on a different core
	 */
	return raw_smp_processor_id();
}

static inline bool
mips_wdt_bind(const unsigned int alive_cpu)
{
	if (alive_cpu != mips_wdt_interrupt_cpu) {
#if defined(CONFIG_SMP)
		if (irq_set_affinity(mips_wdt_affinity_notify.irq,
				     get_cpu_mask(alive_cpu)) != 0)
			return false;
#endif
		mips_wdt_interrupt_cpu = alive_cpu;
	}

	return true;
}

static enum hrtimer_restart
mips_wdt_timer_hard(struct hrtimer *hrtimer)
{
	enum hrtimer_restart restart = HRTIMER_NORESTART;

	spin_lock(&mips_wdt_lock);

	if (!mips_wdt_stopping) {
		const u32 now = mips_wdt_now();
		const unsigned int this_cpu = raw_smp_processor_id();
		unsigned long max_stall_period = 0;
		unsigned int i, first_stalled = 0;

		this_cpu_write(mips_wdt_hard_stall, now);

		for_each_possible_cpu(i) {
			unsigned long period;

			if (i == this_cpu)
				continue;

			period = now - per_cpu(mips_wdt_soft_stall, i);

			if (max_stall_period < period) {
				max_stall_period = period;
				first_stalled = i;
			}
		}

		/* move a calltrace IRQ handler
		 * to a relevant CPU with enabled interrupts
		 */
		if (max_stall_period > MIPS_WDT_MSECS_MAX_STALL)
			mips_wdt_bind(mips_wdt_alive_cpu_for(first_stalled));

		restart = HRTIMER_RESTART;
		hrtimer_forward_now(hrtimer,
				    ktime_set(MIPS_WDT_SEC_REFRESH, 0));

	}

	spin_unlock(&mips_wdt_lock);

	return restart;
}

static inline void
mips_wdt_timer_restart(struct timer_list *timer)
{
	mod_timer(timer, jiffies + HZ * MIPS_WDT_SEC_REFRESH);
}

static void mips_wdt_timer_soft(unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave(&mips_wdt_lock, flags);

	if (!mips_wdt_stopping) {
		static cpumask_t alive = CPU_MASK_NONE;

		cpumask_set_cpu(raw_smp_processor_id(), &alive);

		if (cpumask_full(&alive)) {
			unsigned int i;

			cpumask_clear(&alive);

			/* restart the watchdog timer
			 * if softirq context works on all CPUs
			 */
			pbus_timer_restart(MIPS_WDT_TIMER_CALLTRACE);
			pbus_timer_restart(MIPS_WDT_TIMER);

			for_each_possible_cpu(i) {
				per_cpu(mips_wdt_soft_stall, i) = 0;
				per_cpu(mips_wdt_hard_stall, i) = 0;
			}
		} else {
			this_cpu_write(mips_wdt_soft_stall, mips_wdt_now());
		}

		mips_wdt_timer_restart((struct timer_list *)data);
	}

	spin_unlock_irqrestore(&mips_wdt_lock, flags);
}

static void mips_wdt_timers_start(void *data)
{
	unsigned long flags;
	unsigned int this_cpu;
#if defined(CONFIG_SMP)
	struct hrtimer *hrtimer;
#endif

	spin_lock_irqsave(&mips_wdt_lock, flags);

	this_cpu = raw_smp_processor_id();

	mips_wdt_timer_restart(&per_cpu(mips_wdt_soft_timer, this_cpu));

#if defined(CONFIG_SMP)
	hrtimer = &per_cpu(mips_wdt_hard_timer, this_cpu);
	hrtimer_start(hrtimer, ktime_set(MIPS_WDT_SEC_REFRESH, 0),
		      HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);
#endif

	spin_unlock_irqrestore(&mips_wdt_lock, flags);
}

static inline void
mips_wdt_timers_stop(void)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&mips_wdt_lock, flags);

	pbus_timer_disable(MIPS_WDT_TIMER);
	pbus_timer_disable(MIPS_WDT_TIMER_CALLTRACE);

	pbus_timer_int_ack(MIPS_WDT_TIMER);
	pbus_timer_int_ack(MIPS_WDT_TIMER_CALLTRACE);

	mips_wdt_stopping = true;

	spin_unlock_irqrestore(&mips_wdt_lock, flags);

	for_each_possible_cpu(i) {
		del_timer_sync(&per_cpu(mips_wdt_soft_timer, i));
		hrtimer_cancel(&per_cpu(mips_wdt_hard_timer, i));
	}
}

static void mips_wdt_irq_affinity_notify(struct irq_affinity_notify *notify,
					 const cpumask_t *mask)
{
	unsigned long flags;

	spin_lock_irqsave(&mips_wdt_lock, flags);
	mips_wdt_interrupt_cpu = cpumask_first(mask);
	spin_unlock_irqrestore(&mips_wdt_lock, flags);
}

static void mips_wdt_irq_affinity_release(struct kref *ref)
{
}

int __init mips_wdt_init(void)
{
	bool enabled = false;
	unsigned int i;
	unsigned long flags;
	unsigned int this_cpu;
	int ret;

	ret = request_irq(mips_wdt_affinity_notify.irq, mips_wdt_interrupt,
			  IRQF_NO_THREAD, "watchdog", NULL);
	if (ret != 0) {
		pr_err("failed to request IRQ#%u: %i\n",
		       mips_wdt_affinity_notify.irq, ret);
		return ret;
	}

	ret = irq_set_affinity_notifier(mips_wdt_affinity_notify.irq,
					&mips_wdt_affinity_notify);
	if (ret != 0) {
		pr_err("unable to setup an IRQ%u affinity notifier: %i\n",
		       mips_wdt_affinity_notify.irq, ret);
		goto err_free_irq;
	}

	this_cpu = get_cpu();

	if (!mips_wdt_bind(this_cpu)) {
		pr_err("unable to bind IRQ#%u to CPU%u\n",
		       mips_wdt_affinity_notify.irq,
		       MIPS_WDT_IRQ_CPU);
		put_cpu();

		ret = -EINVAL;
		goto err_remove_notifier;
	}

	put_cpu();

	BUILD_BUG_ON(MIPS_WDT_SEC_REBOOT <= MIPS_WDT_SEC_REFRESH * 3);
	BUILD_BUG_ON(MIPS_WDT_SEC_REBOOT <= MIPS_WDT_SEC_CALLTRACE);
	BUILD_BUG_ON(MIPS_WDT_SEC_CALLTRACE <= 0);

	spin_lock_irqsave(&mips_wdt_lock, flags);

	if (pbus_timer_enable(MIPS_WDT_TIMER_CALLTRACE,
			      MIPS_WDT_MSECS_CALLTRACE,
			      PBUS_TIMER_MODE_INTERVAL)) {
		if (!pbus_timer_enable(MIPS_WDT_TIMER,
				       MIPS_WDT_MSECS_REBOOT,
				       PBUS_TIMER_MODE_WATCHDOG))
			pbus_timer_disable(MIPS_WDT_TIMER_CALLTRACE);
		else
			enabled = true;
	}

	this_cpu = raw_smp_processor_id();

	for_each_possible_cpu(i) {
		struct timer_list *timer;
		struct hrtimer *hrtimer;

		timer = &per_cpu(mips_wdt_soft_timer, i);
		setup_timer(timer, mips_wdt_timer_soft, (unsigned long)timer);
		timer->flags = TIMER_PINNED;

		hrtimer = &per_cpu(mips_wdt_hard_timer, i);
		hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hrtimer->function = mips_wdt_timer_hard;

		per_cpu(mips_wdt_soft_stall, i) = 0;
		per_cpu(mips_wdt_hard_stall, i) = 0;
	}

	spin_unlock_irqrestore(&mips_wdt_lock, flags);

	if (!enabled) {
		ret = -EINVAL;
		goto err_remove_notifier;
	}

	ret = on_each_cpu(mips_wdt_timers_start, NULL, 0);

	if (ret != 0) {
		pr_err("unable to start kernel timers: %i\n", ret);
		goto err_timers_stop;
	}

	pr_info("watchdog timer activated with %u second delay\n",
		MIPS_WDT_SEC_REBOOT);

	return 0;

err_timers_stop:
	mips_wdt_timers_stop();

err_remove_notifier:
	irq_set_affinity_notifier(mips_wdt_affinity_notify.irq, NULL);

err_free_irq:
	free_irq(mips_wdt_affinity_notify.irq, NULL);

	return ret;
}

void __exit mips_wdt_exit(void)
{
	mips_wdt_timers_stop();
	irq_set_affinity_notifier(mips_wdt_affinity_notify.irq, NULL);
	free_irq(mips_wdt_affinity_notify.irq, NULL);

	pr_info("watchdog timer deactivated\n");
}

module_init(mips_wdt_init);
module_exit(mips_wdt_exit);

MODULE_DESCRIPTION("MIPS watchdog");
MODULE_AUTHOR("Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_LICENSE("GPL");
