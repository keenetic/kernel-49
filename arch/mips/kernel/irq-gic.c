/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2012 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/bitmap.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/gic.h>
#include <asm/setup.h>

#include <asm/traps.h>
#include <asm/mips-cpc.h>
#include <asm/smp-ops.h>

#include <linux/ioport.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <asm-generic/bitops/find.h>

unsigned int gic_present;
unsigned int gic_irq_base;
unsigned long _gic_base;

static unsigned long __gic_base_addr;
static unsigned int gic_irq_flags[GIC_NUM_INTRS];

#ifdef CONFIG_IRQ_GIC_EIC
#define EIC_NUM_VECTORS (64 + GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET)

/* The index into this array is the vector # of the interrupt. */
static struct gic_shared_intr_map gic_shared_intr_map[EIC_NUM_VECTORS];
#else
struct gic_pcpu_mask {
	DECLARE_BITMAP(pcpu_mask, GIC_NUM_INTRS);
};

static struct gic_pcpu_mask pcpu_masks[NR_CPUS];
#endif

int gic_get_usm_range(struct resource *gic_usm_res)
{
	if (!gic_present)
		return -1;

	gic_usm_res->start = __gic_base_addr + USM_VISIBLE_SECTION_OFS;
	gic_usm_res->end = gic_usm_res->start + (USM_VISIBLE_SECTION_SIZE - 1);

	return 0;
}

unsigned gic_read_local_vp_id(void)
{
	unsigned int ident;

	GICREAD(GIC_REG(VPE_LOCAL, GIC_VP_IDENT), ident);
	return ident & GIC_VP_IDENT_VCNUM_MSK;
}

#ifdef CONFIG_IRQ_GIC_EIC
static void gic_bind_eic_interrupt(int irq, int regset)
{
	/* Convert irq vector # to hw int # */
	irq -= GIC_PIN_TO_VEC_OFFSET;

	/* Set irq to use shadow set */
	GICWRITE(GIC_REG_ADDR(VPE_LOCAL, GIC_VPE_EIC_SS(irq)), regset);
}

static void gic_eic_irq_dispatch(void)
{
	unsigned int cause = read_c0_cause();
	int irq;

	irq = (cause & ST0_IM) >> STATUSB_IP2;
	if (irq == 0)
		irq = -1;

	if (irq >= 0)
		do_IRQ(gic_irq_base + irq);
	else
		spurious_interrupt();
}
#else
void gic_irq_dispatch(void)
{
	unsigned int i, intr;
	unsigned long *pcpu_mask;
	unsigned long *pending_abs, *intrmask_abs;
	DECLARE_BITMAP(pending, GIC_NUM_INTRS);
	DECLARE_BITMAP(intrmask, GIC_NUM_INTRS);

	/* Get per-cpu bitmaps */
	pcpu_mask = pcpu_masks[smp_processor_id()].pcpu_mask;

	pending_abs = (unsigned long *) GIC_REG_ABS_ADDR(SHARED,
							 GIC_SH_PEND_31_0_OFS);
	intrmask_abs = (unsigned long *) GIC_REG_ABS_ADDR(SHARED,
							  GIC_SH_MASK_31_0_OFS);

	for (i = 0; i < BITS_TO_LONGS(GIC_NUM_INTRS); i++) {
		GICREAD(*pending_abs, pending[i]);
		GICREAD(*intrmask_abs, intrmask[i]);
		pending_abs++;
		intrmask_abs++;
	}

	bitmap_and(pending, pending, intrmask, GIC_NUM_INTRS);
	bitmap_and(pending, pending, pcpu_mask, GIC_NUM_INTRS);

	intr = find_first_bit(pending, GIC_NUM_INTRS);
	while (intr < GIC_NUM_INTRS) {
		do_IRQ(gic_irq_base + intr);

		/* go to next pending bit */
		bitmap_clear(pending, intr, 1);
		intr = find_first_bit(pending, GIC_NUM_INTRS);
	}
}
#endif

static inline void gic_send_ipi(unsigned int intr)
{
	GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), GIC_SH_WEDGE_SET(intr));
}

static void __init vpe_local_setup(unsigned int numvpes)
{
	unsigned int timer_intr = GIC_INT_TMR;
	unsigned int perf_intr = GIC_INT_PERFCTR;
	unsigned int vpe_ctl;
	int i;

#ifdef CONFIG_IRQ_GIC_EIC
	if (cpu_has_veic) {
		/*
		 * GIC timer interrupt -> CPU HW Int X (vector X+2) ->
		 * map to pin X+2-1 (since GIC adds 1)
		 */
		timer_intr += (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);
		/*
		 * GIC perfcnt interrupt -> CPU HW Int X (vector X+2) ->
		 * map to pin X+2-1 (since GIC adds 1)
		 */
		perf_intr += (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);
	}
#endif

	/*
	 * Setup the default performance counter timer interrupts
	 * for all VPEs
	 */
	for (i = 0; i < numvpes; i++) {
		GICWRITE(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), i);

		/* Are Interrupts locally routable? */
		GICREAD(GIC_REG(VPE_OTHER, GIC_VPE_CTL), vpe_ctl);
		if (vpe_ctl & GIC_VPE_CTL_TIMER_RTBL_MSK)
			GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_TIMER_MAP),
				 GIC_MAP_TO_PIN_MSK | timer_intr);

		if (vpe_ctl & GIC_VPE_CTL_PERFCNT_RTBL_MSK)
			GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_PERFCTR_MAP),
				 GIC_MAP_TO_PIN_MSK | perf_intr);
	}

#ifdef CONFIG_IRQ_GIC_EIC
	if (cpu_has_veic) {
		int veic;

		veic = timer_intr + GIC_PIN_TO_VEC_OFFSET;
		set_vi_handler(veic, gic_eic_irq_dispatch);
		gic_shared_intr_map[veic].local_intr_mask |= GIC_VPE_RMASK_TIMER_MSK;

		veic = perf_intr + GIC_PIN_TO_VEC_OFFSET;
		set_vi_handler(veic, gic_eic_irq_dispatch);
		gic_shared_intr_map[veic].local_intr_mask |= GIC_VPE_RMASK_PERFCNT_MSK;
	}
#endif
}

static void gic_mask_irq(struct irq_data *d)
{
	int intr = (d->irq - gic_irq_base);
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	if (map_ptr->shared_intr_flags & GIC_FLAG_PERCPU)
		intr = map_ptr->shared_intr_list[smp_processor_id()];
	else
		intr = map_ptr->shared_intr_list[0];
#endif
	/* disable interrupt */
	GIC_CLR_INTR_MASK(intr);
}

static void gic_unmask_irq(struct irq_data *d)
{
	int intr = (d->irq - gic_irq_base);
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	if (map_ptr->shared_intr_flags & GIC_FLAG_PERCPU)
		intr = map_ptr->shared_intr_list[smp_processor_id()];
	else
		intr = map_ptr->shared_intr_list[0];
#endif
	/* enable interrupt */
	GIC_SET_INTR_MASK(intr);
}

static void gic_mask_ack_irq(struct irq_data *d)
{
	int intr = (d->irq - gic_irq_base);
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	if (map_ptr->shared_intr_flags & GIC_FLAG_PERCPU)
		intr = map_ptr->shared_intr_list[smp_processor_id()];
	else
		intr = map_ptr->shared_intr_list[0];
#endif
	/* disable interrupt */
	GIC_CLR_INTR_MASK(intr);

	/* clear edge detector */
	if (gic_irq_flags[intr] & GIC_TRIG_EDGE)
		GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), GIC_SH_WEDGE_CLR(intr));
}

#ifdef CONFIG_SMP
static DEFINE_SPINLOCK(gic_lock);

static int gic_set_affinity(struct irq_data *d, const struct cpumask *cpumask,
			    bool force)
{
	int intr = (d->irq - gic_irq_base);
	cpumask_t	tmp = CPU_MASK_NONE;
	unsigned long	flags;
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	if (map_ptr->shared_intr_flags & GIC_FLAG_PERCPU)
		return -EINVAL;

	intr = map_ptr->shared_intr_list[0];
#else
	int i;
#endif

	cpumask_and(&tmp, cpumask, cpu_online_mask);
	if (cpumask_empty(&tmp))
		return -EINVAL;

	/* Assumption : cpumask refers to a single CPU */
	spin_lock_irqsave(&gic_lock, flags);

	/* Re-route this IRQ */
	GIC_SH_MAP_TO_VPE_SMASK(intr, cpumask_first(&tmp));

#ifndef CONFIG_IRQ_GIC_EIC
	/* Update the pcpu_masks */
	for (i = 0; i < NR_CPUS; i++)
		clear_bit(intr, pcpu_masks[i].pcpu_mask);
	set_bit(intr, pcpu_masks[cpumask_first(&tmp)].pcpu_mask);
#endif
	cpumask_copy(d->common->affinity, cpumask);

	spin_unlock_irqrestore(&gic_lock, flags);

	return IRQ_SET_MASK_OK_NOCOPY;
}
#endif

static struct irq_chip gic_irq_controller = {
	.name			= "MIPS GIC",
	.irq_ack		= gic_mask_ack_irq,
	.irq_mask		= gic_mask_irq,
	.irq_mask_ack		= gic_mask_ack_irq,
	.irq_unmask		= gic_unmask_irq,
	.irq_eoi		= gic_unmask_irq,
	.irq_disable		= gic_mask_irq,
	.irq_enable		= gic_unmask_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity	= gic_set_affinity,
#endif
};

static void __init gic_setup_intr(unsigned int intr, unsigned int cpu,
	unsigned int pin, unsigned int polarity, unsigned int trigtype,
	unsigned int flags)
{
	/* Setup Intr to Pin mapping */
	if (pin & GIC_MAP_TO_NMI_MSK) {
		int i;

		GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_MAP_TO_PIN(intr)), pin);
		/* FIXME: hack to route NMI to all cpu's */
		for (i = 0; i < NR_CPUS; i += 32) {
			GICWRITE(GIC_REG_ADDR(SHARED,
					  GIC_SH_MAP_TO_VPE_REG_OFF(intr, i)),
				 0xffffffff);
		}
	} else {
		GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_MAP_TO_PIN(intr)),
			 GIC_MAP_TO_PIN_MSK | pin);
		/* Setup Intr to CPU mapping */
		GIC_SH_MAP_TO_VPE_SMASK(intr, cpu);
#ifdef CONFIG_IRQ_GIC_EIC
		if (cpu_has_veic) {
			struct gic_shared_intr_map *map_ptr;

			set_vi_handler(pin + GIC_PIN_TO_VEC_OFFSET,
				gic_eic_irq_dispatch);
			map_ptr = &gic_shared_intr_map[pin + GIC_PIN_TO_VEC_OFFSET];
			map_ptr->shared_intr_flags = flags;
			if (flags & GIC_FLAG_PERCPU) {
				if (cpu < NR_CPUS)
					map_ptr->shared_intr_list[cpu] = intr;
			} else
				map_ptr->shared_intr_list[0] = intr;
		}
#endif
	}

	/* Setup Intr Polarity */
	GIC_SET_POLARITY(intr, polarity);

	/* Setup Intr Trigger Type */
	GIC_SET_TRIGGER(intr, trigtype);

	/* Init Intr Masks */
	GIC_CLR_INTR_MASK(intr);

#ifndef CONFIG_IRQ_GIC_EIC
	/* Initialise per-cpu Interrupt software masks */
	set_bit(intr, pcpu_masks[cpu].pcpu_mask);
#endif
	if ((flags & GIC_FLAG_TRANSPARENT) && (cpu_has_veic == 0))
		GIC_SET_INTR_MASK(intr);
	if (trigtype == GIC_TRIG_EDGE)
		gic_irq_flags[intr] |= GIC_TRIG_EDGE;
}

#ifdef CONFIG_MIPS_MT_SMP
static int gic_resched_int_base;
static int gic_call_int_base;

static inline unsigned int plat_ipi_resched_int_xlate(int cpu)
{
	unsigned int intr = gic_resched_int_base + cpu;
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	intr = map_ptr->shared_intr_list[0];
#endif
	return intr;
}

static inline unsigned int plat_ipi_call_int_xlate(int cpu)
{
	unsigned int intr = gic_call_int_base + cpu;
#ifdef CONFIG_IRQ_GIC_EIC
	struct gic_shared_intr_map *map_ptr;

	map_ptr = &gic_shared_intr_map[intr];
	intr = map_ptr->shared_intr_list[0];
#endif
	return intr;
}

void mips_smp_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;
	unsigned int intr;
	unsigned int core = cpu_data[cpu].core;

	local_irq_save(flags);

	switch (action) {
	case SMP_CALL_FUNCTION:
		intr = plat_ipi_call_int_xlate(cpu);
		break;

	case SMP_RESCHEDULE_YOURSELF:
		intr = plat_ipi_resched_int_xlate(cpu);
		break;

	default:
		BUG();
	}

	gic_send_ipi(intr);

	if (mips_cpc_present() && (core != current_cpu_data.core)) {
		while (!cpumask_test_cpu(cpu, &cpu_coherent_mask)) {
			mips_cm_lock_other(core, 0);
			mips_cpc_lock_other(core);
			write_cpc_co_cmd(CPC_Cx_CMD_PWRUP);
			mips_cpc_unlock_other();
			mips_cm_unlock_other();
		}
	}

	local_irq_restore(flags);
}

void mips_smp_send_ipi_mask(const struct cpumask *mask, unsigned int action)
{
	unsigned int i;

	for_each_cpu(i, mask)
		mips_smp_send_ipi_single(i, action);
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
	.name		= "IPI_resched"
};

static struct irqaction irq_call = {
	.handler	= ipi_call_interrupt,
	.flags		= IRQF_PERCPU,
	.name		= "IPI_call"
};

static __init void gic_ipi_init_one(unsigned int irq, struct irqaction *action)
{
	irq_set_handler(gic_irq_base + irq, handle_percpu_irq);
	setup_irq(gic_irq_base + irq, action);
}

static inline void gic_ipi_init(int intnumintrs, int numvpes)
{
	int cpu;

	gic_call_int_base = intnumintrs - numvpes;
	gic_resched_int_base = gic_call_int_base - numvpes;

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		gic_ipi_init_one(gic_resched_int_base + cpu, &irq_resched);
		gic_ipi_init_one(gic_call_int_base + cpu, &irq_call);
	}
}
#else
static inline void gic_ipi_init(int intnumintrs, int numvpes)
{
}
#endif

static void __init gic_basic_init(int numintrs,
				  int numvpes,
				  const struct gic_intr_map *intrmap,
				  unsigned int mapsize)
{
	unsigned int i, cpu;
	unsigned int pin_offset = 0;

	/* Setup defaults */
	for (i = 0; i < numintrs; i++) {
		GIC_SET_POLARITY(i, GIC_POL_POS);
		GIC_SET_TRIGGER(i, GIC_TRIG_LEVEL);
		GIC_CLR_INTR_MASK(i);
		if (i < GIC_NUM_INTRS)
			gic_irq_flags[i] = 0;
	}

#ifdef CONFIG_IRQ_GIC_EIC
	board_bind_eic_interrupt = gic_bind_eic_interrupt;

	for (i = 0; i < EIC_NUM_VECTORS; i++) {
		gic_shared_intr_map[i].shared_intr_flags = 0;
		gic_shared_intr_map[i].local_intr_mask = 0;
	}

	/*
	 * In EIC mode, the HW_INT# is offset by (2-1). Need to subtract
	 * one because the GIC will add one (since 0=no intr).
	 */
	if (cpu_has_veic)
		pin_offset = (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);
#endif

	/* Setup specifics */
	for (i = 0; i < mapsize; i++) {
		cpu = intrmap[i].cpunum;
		if (cpu == GIC_UNUSED)
			continue;
		gic_setup_intr(i,
			intrmap[i].cpunum,
			intrmap[i].pin + pin_offset,
			intrmap[i].polarity,
			intrmap[i].trigtype,
			intrmap[i].flags);
	}

	vpe_local_setup(numvpes);
}

void __init gic_init(unsigned long gic_base_addr,
		     unsigned long gic_addrspace_size,
		     const struct gic_intr_map *intr_map,
		     unsigned int intr_map_size,
		     unsigned int irqbase)
{
	unsigned int gicconfig, gicrev;
	int numvpes, numintrs;

	__gic_base_addr = gic_base_addr;

	_gic_base = (unsigned long) ioremap_nocache(gic_base_addr,
						    gic_addrspace_size);
	gic_irq_base = irqbase;

	GICREAD(GIC_REG(SHARED, GIC_SH_REVISIONID), gicrev);
	pr_info("MIPS GIC RevID: %d.%d\n", (gicrev >> 8) & 0xff, gicrev & 0xff);

	GICREAD(GIC_REG(SHARED, GIC_SH_CONFIG), gicconfig);
	numintrs = (gicconfig & GIC_SH_CONFIG_NUMINTRS_MSK) >>
		   GIC_SH_CONFIG_NUMINTRS_SHF;
	numintrs = ((numintrs + 1) * 8);

	numvpes = (gicconfig & GIC_SH_CONFIG_NUMVPES_MSK) >>
		  GIC_SH_CONFIG_NUMVPES_SHF;
	numvpes = numvpes + 1;

	gic_basic_init(numintrs, numvpes, intr_map, intr_map_size);

	gic_platform_init(numintrs, &gic_irq_controller);

	gic_ipi_init(numintrs, numvpes);
}
