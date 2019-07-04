#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/irq_cpu.h>

#include <asm/rt2880/rt_mmap.h>

#ifndef CONFIG_CLKSRC_MIPS_GIC
unsigned int get_c0_compare_int(void)
{
	return gic_get_c0_compare_int();
}
#endif

#if IS_ENABLED(CONFIG_HW_PERF_EVENTS) || IS_ENABLED(CONFIG_OPROFILE)
int get_c0_perfcount_int(void)
{
	/*
	 * Performance counter events are routed through GIC.
	 * Prevent them from firing on CPU IRQ7 as well
	 */
	clear_c0_status(IE_SW0 << 7);

	return gic_get_c0_perfcount_int();
}
EXPORT_SYMBOL_GPL(get_c0_perfcount_int);
#endif

#ifdef CONFIG_MIPS_EJTAG_FDC_TTY
int get_c0_fdc_int(void)
{
	return gic_get_c0_fdc_int();
}
#endif

void __init arch_init_irq(void)
{
	phys_addr_t gic_base = RALINK_GIC_BASE;
	size_t gic_len = RALINK_GIC_ADDRSPACE_SZ;

	mips_cpu_irq_init();

	if (mips_cm_present()) {
		gic_base = read_gcr_gic_base() & ~CM_GCR_GIC_BASE_GICEN_MSK;

		write_gcr_gic_base(gic_base | CM_GCR_GIC_BASE_GICEN_MSK);
		__sync();
	}

	gic_present = true;

	gic_init(gic_base, gic_len, GIC_CPU_PIN_OFFSET, 0);

	/* set EDGE type int for MT7530 MCM (prevent race) */
	irq_set_irq_type(SURFBOARDINT_ESW, IRQ_TYPE_EDGE_RISING);
}

