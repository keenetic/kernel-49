/*
 * Copyright (C) 2000, 2001, 2004 MIPS Technologies, Inc.
 * Copyright (C) 2001 Ralf Baechle
 * Copyright (C) 2010 PMC-Sierra, Inc.
 *
 *  VSMP support for MSP platforms . Derived from malta vsmp support.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/tc3162/tc3162.h>

#ifdef CONFIG_MIPS_MT_SMP

#define MIPS_CPU_IPI_RESCHED_IRQ	SI_SWINT_INT0		/* SW int 0 for resched */
#define MIPS_CPU_IPI_CALL_IRQ		SI_SWINT_INT1		/* SW int 1 for call */

static void ipi_resched_dispatch(void)
{
	do_IRQ(MIPS_CPU_IPI_RESCHED_IRQ);
}

static void ipi_call_dispatch(void)
{
	do_IRQ(MIPS_CPU_IPI_CALL_IRQ);
}

static irqreturn_t ipi_resched_interrupt(int irq, void *dev_id)
{
	scheduler_ipi();

	return IRQ_HANDLED;
}

static irqreturn_t ipi_call_interrupt(int irq, void *dev_id)
{
	generic_smp_call_function_interrupt();

	return IRQ_HANDLED;
}

static struct irqaction irq_resched = {
	.handler	= ipi_resched_interrupt,
	.flags		= IRQF_PERCPU,
	.name		= "ipi_resched"
};

static struct irqaction irq_call = {
	.handler	= ipi_call_interrupt,
	.flags		= IRQF_PERCPU,
	.name		= "ipi_call"
};

void __init arch_init_ipiirq(int irq, struct irqaction *action)
{
	setup_irq(irq, action);
	irq_set_handler(irq, handle_percpu_irq);
}

void __init vsmp_int_init(void)
{
	set_vi_handler(MIPS_CPU_IPI_RESCHED_IRQ, ipi_resched_dispatch);
	set_vi_handler(MIPS_CPU_IPI_CALL_IRQ, ipi_call_dispatch);

	arch_init_ipiirq(MIPS_CPU_IPI_RESCHED_IRQ, &irq_resched);
	arch_init_ipiirq(MIPS_CPU_IPI_CALL_IRQ, &irq_call);
}

/*
 * IRQ affinity hook
 */
int plat_set_irq_affinity(struct irq_data *d, const struct cpumask *affinity,
			  bool force)
{
	cpumask_t tmask;
	int cpu = 0;
	int irq_vpe0 = 0;
	int irq_vpe1 = 0;
	unsigned int offset1, offset2;

	/*
	 * On the legacy Malta development board, all I/O interrupts
	 * are routed through the 8259 and combined in a single signal
	 * to the CPU daughterboard, and on the CoreFPGA2/3 34K models,
	 * that signal is brought to IP2 of both VPEs. To avoid racing
	 * concurrent interrupt service events, IP2 is enabled only on
	 * one VPE, by convention VPE0.  So long as no bits are ever
	 * cleared in the affinity mask, there will never be any
	 * interrupt forwarding.  But as soon as a program or operator
	 * sets affinity for one of the related IRQs, we need to make
	 * sure that we don't ever try to forward across the VPE boundry,
	 * at least not until we engineer a system where the interrupt
	 * _ack() or _end() function can somehow know that it corresponds
	 * to an interrupt taken on another VPE, and perform the appropriate
	 * restoration of Status.IM state using MFTR/MTTR instead of the
	 * normal local behavior. We also ensure that no attempt will
	 * be made to forward to an offline "CPU".
	 */

	cpumask_copy(&tmask, affinity);
	for_each_cpu(cpu, affinity) {
		if (!cpu_online(cpu)) {
			cpumask_clear_cpu(cpu, &tmask);
		} else {
			if (cpu == 0)
				irq_vpe0++;
			else
				irq_vpe1++;
		}
	}

	cpumask_copy(irq_data_get_affinity_mask(d), &tmask);

	/* change IRQ binding to VPE0 or VPE1 */
	offset1 = 32 - d->irq;
	offset2 = ((d->irq - 1) % 4) * 8 + 4;
	offset1 = (offset1 >> 2) << 2;
	if (irq_vpe0 >= irq_vpe1)
		VPint(CR_INTC_IVSR0 + offset1) &= ~(1u << offset2);
	else
		VPint(CR_INTC_IVSR0 + offset1) |= (1u << offset2);

	if (cpumask_empty(&tmask)) {
		/*
		 * We could restore a default mask here, but the
		 * runtime code can anyway deal with the null set
		 */
		printk(KERN_WARNING
			"IRQ affinity leaves no legal CPU for IRQ %d\n", d->irq);
	}

	return IRQ_SET_MASK_OK_NOCOPY;
}

#endif /* CONFIG_MIPS_MT_SMP */

