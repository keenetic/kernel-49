/*
 *  Interrupt service routines for Trendchip board
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>

#include <asm/tc3162/tc3162.h>

extern void vsmp_int_init(void);
extern int plat_set_irq_affinity(struct irq_data *d,
				 const struct cpumask *affinity, bool force);

__IMEM
static inline bool mips_mt_cpu_is_irq0(const struct irq_data *d)
{
	return d->irq == SI_SWINT_INT0 || d->irq == SI_SWINT1_INT0;
}

__IMEM
static inline unsigned long mips_mt_cpu_irq_mask(const struct irq_data *d)
{
	if (mips_mt_cpu_is_irq0(d))
		return CAUSEF_IP0;

	return CAUSEF_IP1;
}

__IMEM
static inline void unmask_mips_mt_irq(struct irq_data *d)
{
	const unsigned int vpflags = dvpe();

	set_c0_status(mips_mt_cpu_irq_mask(d));
	irq_enable_hazard();
	evpe(vpflags);
}

__IMEM
static inline void mask_mips_mt_irq(struct irq_data *d)
{
	const unsigned int vpflags = dvpe();

	clear_c0_status(mips_mt_cpu_irq_mask(d));
	irq_disable_hazard();
	evpe(vpflags);
}

__IMEM
static unsigned int mips_mt_cpu_irq_startup(struct irq_data *d)
{
	const unsigned int vpflags = dvpe();
	const unsigned int mask = mips_mt_cpu_irq_mask(d);
	unsigned int reg_imr = VPint(CR_INTC_IMR);

	if (mips_mt_cpu_is_irq0(d))
		reg_imr |= (1u << (SI_SWINT_INT0 - 1)) |
			   (1u << (SI_SWINT1_INT0 - 1));
	else
		reg_imr |= (1u << (SI_SWINT_INT1 - 1)) |
			   (1u << (SI_SWINT1_INT1 - 1));

	VPint(CR_INTC_IMR) = reg_imr;

	clear_c0_cause(mask);
	set_c0_status(mask);
	irq_enable_hazard();
	evpe(vpflags);

	return 0;
}

/*
 * While we ack the interrupt interrupts are disabled and thus we don't need
 * to deal with concurrency issues.  Same for mips_cpu_irq_end.
 */
__IMEM
static void mips_mt_cpu_irq_ack(struct irq_data *d)
{
	const unsigned int vpflags = dvpe();
	const unsigned int mask = mips_mt_cpu_irq_mask(d);

	clear_c0_cause(mask);
	clear_c0_status(mask);
	irq_disable_hazard();
	evpe(vpflags);
}

static struct irq_chip mips_mt_cpu_irq_controller = {
	.name		= "MIPS",
	.irq_startup	= mips_mt_cpu_irq_startup,
	.irq_ack	= mips_mt_cpu_irq_ack,
	.irq_mask	= mask_mips_mt_irq,
	.irq_mask_ack	= mips_mt_cpu_irq_ack,
	.irq_unmask	= unmask_mips_mt_irq,
	.irq_eoi	= unmask_mips_mt_irq,
	.irq_disable	= mask_mips_mt_irq,
	.irq_enable	= unmask_mips_mt_irq,
};

static DEFINE_SPINLOCK(tc3262_irq_lock);

__IMEM
static inline void unmask_mips_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int irq = d->irq;

	spin_lock_irqsave(&tc3262_irq_lock, flags);

	if (smp_processor_id() != 0 && irq == SI_TIMER_INT)
		irq = SI_TIMER1_INT;

	if (irq <= 32)
		VPint(CR_INTC_IMR) |= (1u << (irq - 1));
	else
		VPint(CR_INTC_IMR_1) |= (1u << (irq - 33));

	spin_unlock_irqrestore(&tc3262_irq_lock, flags);
}

__IMEM
static inline void mask_mips_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int irq = d->irq;

	spin_lock_irqsave(&tc3262_irq_lock, flags);

	if (smp_processor_id() != 0 && irq == SI_TIMER_INT)
		irq = SI_TIMER1_INT;

	if (irq <= 32)
		VPint(CR_INTC_IMR) &= ~(1u << (irq - 1));
	else
		VPint(CR_INTC_IMR_1) &= ~(1u << (irq - 33));

	spin_unlock_irqrestore(&tc3262_irq_lock, flags);
}

static struct irq_chip tc3262_irq_chip = {
	.name			= "INTC",
	.irq_ack		= mask_mips_irq,
	.irq_mask		= mask_mips_irq,
	.irq_mask_ack		= mask_mips_irq,
	.irq_unmask		= unmask_mips_irq,
	.irq_eoi		= unmask_mips_irq,
	.irq_disable		= mask_mips_irq,
	.irq_enable		= unmask_mips_irq,
#ifdef CONFIG_MIPS_MT_SMP
	.irq_set_affinity	= plat_set_irq_affinity,
#endif
};

#define __BUILD_IRQ_DISPATCH(irq_n)		\
__IMEM						\
static void __tc3262_irq_dispatch##irq_n(void)	\
{						\
	do_IRQ(irq_n);				\
}

#define __BUILD_IRQ_DISPATCH_FUNC(irq_n)  __tc3262_irq_dispatch##irq_n

/* pre-built 41 irq dispatch function */
__BUILD_IRQ_DISPATCH(0)
__BUILD_IRQ_DISPATCH(1)
__BUILD_IRQ_DISPATCH(2)
__BUILD_IRQ_DISPATCH(3)
__BUILD_IRQ_DISPATCH(4)
__BUILD_IRQ_DISPATCH(5)
__BUILD_IRQ_DISPATCH(6)
__BUILD_IRQ_DISPATCH(7)
__BUILD_IRQ_DISPATCH(8)
__BUILD_IRQ_DISPATCH(9)
__BUILD_IRQ_DISPATCH(10)
__BUILD_IRQ_DISPATCH(11)
__BUILD_IRQ_DISPATCH(12)
__BUILD_IRQ_DISPATCH(13)
__BUILD_IRQ_DISPATCH(14)
__BUILD_IRQ_DISPATCH(15)
__BUILD_IRQ_DISPATCH(16)
__BUILD_IRQ_DISPATCH(17)
__BUILD_IRQ_DISPATCH(18)
__BUILD_IRQ_DISPATCH(19)
__BUILD_IRQ_DISPATCH(20)
__BUILD_IRQ_DISPATCH(21)
__BUILD_IRQ_DISPATCH(22)
__BUILD_IRQ_DISPATCH(23)
__BUILD_IRQ_DISPATCH(24)
__BUILD_IRQ_DISPATCH(25)
__BUILD_IRQ_DISPATCH(26)
__BUILD_IRQ_DISPATCH(27)
__BUILD_IRQ_DISPATCH(28)
__BUILD_IRQ_DISPATCH(29)
__BUILD_IRQ_DISPATCH(30)
__BUILD_IRQ_DISPATCH(31)
__BUILD_IRQ_DISPATCH(32)
__BUILD_IRQ_DISPATCH(33)
__BUILD_IRQ_DISPATCH(34)
__BUILD_IRQ_DISPATCH(35)
__BUILD_IRQ_DISPATCH(36)
__BUILD_IRQ_DISPATCH(37)
__BUILD_IRQ_DISPATCH(38)
__BUILD_IRQ_DISPATCH(39)
__BUILD_IRQ_DISPATCH(40)

/* register pre-built 41 irq dispatch function */
static void (*irq_dispatch_tab[])(void) =
{
__BUILD_IRQ_DISPATCH_FUNC(0),
__BUILD_IRQ_DISPATCH_FUNC(1),
__BUILD_IRQ_DISPATCH_FUNC(2),
__BUILD_IRQ_DISPATCH_FUNC(3),
__BUILD_IRQ_DISPATCH_FUNC(4),
__BUILD_IRQ_DISPATCH_FUNC(5),
__BUILD_IRQ_DISPATCH_FUNC(6),
__BUILD_IRQ_DISPATCH_FUNC(7),
__BUILD_IRQ_DISPATCH_FUNC(8),
__BUILD_IRQ_DISPATCH_FUNC(9),
__BUILD_IRQ_DISPATCH_FUNC(10),
__BUILD_IRQ_DISPATCH_FUNC(11),
__BUILD_IRQ_DISPATCH_FUNC(12),
__BUILD_IRQ_DISPATCH_FUNC(13),
__BUILD_IRQ_DISPATCH_FUNC(14),
__BUILD_IRQ_DISPATCH_FUNC(15),
__BUILD_IRQ_DISPATCH_FUNC(16),
__BUILD_IRQ_DISPATCH_FUNC(17),
__BUILD_IRQ_DISPATCH_FUNC(18),
__BUILD_IRQ_DISPATCH_FUNC(19),
__BUILD_IRQ_DISPATCH_FUNC(20),
__BUILD_IRQ_DISPATCH_FUNC(21),
__BUILD_IRQ_DISPATCH_FUNC(22),
__BUILD_IRQ_DISPATCH_FUNC(23),
__BUILD_IRQ_DISPATCH_FUNC(24),
__BUILD_IRQ_DISPATCH_FUNC(25),
__BUILD_IRQ_DISPATCH_FUNC(26),
__BUILD_IRQ_DISPATCH_FUNC(27),
__BUILD_IRQ_DISPATCH_FUNC(28),
__BUILD_IRQ_DISPATCH_FUNC(29),
__BUILD_IRQ_DISPATCH_FUNC(30),
__BUILD_IRQ_DISPATCH_FUNC(31),
__BUILD_IRQ_DISPATCH_FUNC(32),
__BUILD_IRQ_DISPATCH_FUNC(33),
__BUILD_IRQ_DISPATCH_FUNC(34),
__BUILD_IRQ_DISPATCH_FUNC(35),
__BUILD_IRQ_DISPATCH_FUNC(36),
__BUILD_IRQ_DISPATCH_FUNC(37),
__BUILD_IRQ_DISPATCH_FUNC(38),
__BUILD_IRQ_DISPATCH_FUNC(39),
__BUILD_IRQ_DISPATCH_FUNC(40)
};

void tc_disable_irq_all(void)
{
	unsigned long flags;

	spin_lock_irqsave(&tc3262_irq_lock, flags);
	VPint(CR_INTC_IMR) = 0x0;
	VPint(CR_INTC_IMR_1) = 0x0;
	spin_unlock_irqrestore(&tc3262_irq_lock, flags);
}

/* called to initialize MIPS timers only */
unsigned int get_c0_compare_int(void)
{
	if (smp_processor_id() != 0) {
		struct irq_data d = {
			.irq = SI_TIMER_INT
		};

		unmask_mips_irq(&d);
	}

	return SI_TIMER_INT;
}

void __init arch_init_irq(void)
{
	unsigned int i;

	/* disable all hardware interrupts */
	clear_c0_status(ST0_IM);
	clear_c0_cause(CAUSEF_IP);

	for (i = 0; i < NR_IRQS; i++) {
		if (i == SI_SWINT_INT0 || i == SI_SWINT1_INT0 ||
		    i == SI_SWINT_INT1 || i == SI_SWINT1_INT1) {
			irq_set_chip(i, &mips_mt_cpu_irq_controller);
		} else if (i == SI_TIMER_INT || i == SI_TIMER1_INT)
			irq_set_chip_and_handler(i, &tc3262_irq_chip,
						 handle_percpu_devid_irq);
		else
			irq_set_chip_and_handler(i, &tc3262_irq_chip,
						 handle_level_irq);
		set_vi_handler(i, irq_dispatch_tab[i]);
	}

#ifdef CONFIG_MIPS_MT_SMP
	vsmp_int_init();
#endif

	/* enable MIPS IRQ0 and IRQ1 */
	write_c0_status((read_c0_status() & ~ST0_IM) |
			(STATUSF_IP0 | STATUSF_IP1));
}

__IMEM
asmlinkage void plat_irq_dispatch(void)
{
	unsigned int irq = (read_c0_cause() & ST0_IM) >> 10;

	do_IRQ(irq);
}
