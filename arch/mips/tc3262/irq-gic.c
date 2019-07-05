#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/mips-cm.h>
#include <asm/gic.h>

#include <asm/tc3162/tc3162.h>
#include <asm/tc3162/rt_mmap.h>
#include <asm/tc3162/surfboardint.h>

#define MAX_CPU_VPE		4

#define TIMER0_INTSRC		30
#define TIMER1_INTSRC		29
#define TIMER2_INTSRC		37
#define TIMER3_INTSRC		36

static const int timers_intSrcNum[MAX_CPU_VPE] = {
	TIMER0_INTSRC,
	TIMER1_INTSRC,
	TIMER2_INTSRC,
	TIMER3_INTSRC
};

#define X GIC_UNUSED

static const struct gic_intr_map gic_intr_map[GIC_NUM_INTRS] = {
/*	cpu,  irqNum - 1,           polarity,   triggerType, flags,        Src Name */
	{ 0, CPU_CM_ERR - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  0  CPU Coherence Manager Error */
	{ 0, CPU_CM_PCINT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  1  CPU CM Perf Cnt overflow */
	{ 0, UART_INT - 1,         GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  2  uart  */
	{ X, DRAM_PROTECTION - 1,  GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  3  dram illegal access */
	{ 0, TIMER0_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  4  timer 0 */
	{ 0, TIMER1_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  5  timer 1 */
	{ 0, TIMER2_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  6  timer 2 */
	{ 0, IPI_RESCHED_INT0 - 1, GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/*  7  ipi resched 0 */
	{ 1, IPI_RESCHED_INT1 - 1, GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/*  8  ipi resched 1 */
	{ 0, TIMER5_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/*  9  timer 3 for wdog */
	{ 0, GPIO_INT - 1,         GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 10  GPIO */
	{ 0, PCM1_INT - 1,         GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 11  PCM 1 */
	{ 2, IPI_RESCHED_INT2 - 1, GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 12  ipi resched 2 */
	{ 3, IPI_RESCHED_INT3 - 1, GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 13  ipi resched 3 */
	{ 0, GDMA_INTR - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 14  GDMA */
	{ 0, MAC1_INT - 1,         GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 15  LAN Giga Switch */
	{ 0, UART2_INT - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 16  uart 2 */
	{ 0, IRQ_RT3XXX_USB - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 17  USB host */
	{ 0, DYINGGASP_INT - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 18  Dying gasp */
	{ 0, DMT_INT - 1,          GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 19  xDSL DMT */
	{ 0, GIC_EDGE_NMI - 1,     GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 20  gic edge NMI */
	{ 0, QDMA_LAN0_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 21  QDMA LAN 0 */
	{ 0, QDMA_WAN0_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 22  QDMA WAN 0 */
	{ 0, PCIE_0_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 23  PCIE port 0 */
	{ 0, PCIE_A_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 24  PCIE port 1 */
	{ 0, PCIE_SERR_INT - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 25  PCIE error */
	{ 0, XPON_MAC_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 26  XPON MAC */
	{ 0, XPON_PHY_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 27  XPON PHY */
	{ 0, CRYPTO_INT - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 28  Crypto engine */
	{ 1, SI_TIMER_INT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_PERCPU },	/* 29  external CPU timer 1 when bfbf0400[1]=1 */
	{ 0, SI_TIMER_INT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_PERCPU },	/* 30  external CPU timer 0 when bfbf0400[0]=1 */
	{ 0, BUS_TOUT_INT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 31  Pbus timeout */
	{ 0, PCM2_INT - 1,         GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 32  PCM 2 */
	{ 0, FE_ERR_INTR - 1,      GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 33  Frame Engine Error */
	{ 0, IPI_CALL_INT0 - 1,    GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 34  ipi call 0 */
	{ 0, AUTO_MANUAL_INT - 1,  GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 35  SPI */
	{ 3, SI_TIMER_INT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_PERCPU },	/* 36  external CPU timer 3 when bfbe0000[1]=1 */
	{ 2, SI_TIMER_INT - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_PERCPU },	/* 37  external CPU timer 2 when bfbe0000[1]=1 */
	{ 0, UART3_INT - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 38  UART3 */
	{ 0, QDMA_LAN1_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 39  QDMA LAN 1 */
	{ 0, QDMA_LAN2_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 40  QDMA LAN 2 */
	{ 0, QDMA_LAN3_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 41  QDMA LAN 3 */
	{ 0, QDMA_WAN1_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 42  QDMA WAN 1 */
	{ 0, QDMA_WAN2_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 43  QDMA WAN 2 */
	{ 0, QDMA_WAN3_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 44  QDMA WAN 3 */
	{ 0, UART4_INT - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 45  UART 4 */
	{ 0, UART5_INT - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 46  UART 5 */
	{ 0, HSDMA_INTR - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 47  High Speed DMA */
	{ 0, USB_HOST_2 - 1,       GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 48  USB host 2 (port1) */
	{ 0, XSI_MAC_INTR - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 49  XFI/HGSMII MAC interface */
	{ 0, XSI_PHY_INTR - 1,     GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 50  XFI/HGSMII PHY interface */
	{ 0, WOE0_INTR - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 51  WIFI Offload Engine 0 */
	{ 0, WOE1_INTR - 1,        GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 52  WIFI Offload Engine 1 */
	{ 0, WDMA0_P0_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 53  WIFI DMA 0 port 0 */
	{ 0, WDMA0_P1_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 54  WIFI DMA 0 port 1 */
	{ 0, WDMA0_WOE_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 55  WIFI DMA 0 for WOE */
	{ 0, WDMA1_P0_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 56  WIFI DMA 1 port 0 */
	{ 0, WDMA1_P1_INTR - 1,    GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 56  WIFI DMA 1 port 1 */
	{ 0, WDMA1_WOE_INTR - 1,   GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 58  WIFI DMA 1 for WOE */
	{ 0, EFUSE_ERR0_INTR - 1,  GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 59  efuse error for not setting key */
	{ 0, EFUSE_ERR1_INTR - 1,  GIC_POL_POS, GIC_TRIG_LEVEL, 0		},	/* 60  efuse error for prev action not finished */
	{ 1, IPI_CALL_INT1 - 1,    GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 61  ipi call 1 */
	{ 2, IPI_CALL_INT2 - 1,    GIC_POL_POS, GIC_TRIG_EDGE,  0		},	/* 62  ipi call 2 */
	{ 3, IPI_CALL_INT3 - 1,    GIC_POL_POS, GIC_TRIG_EDGE,  0		}	/* 63  ipi call 3 */
};

#undef X

static int get_gic_shared_intr(unsigned int irqNum)
{
	int intSrc;

	for (intSrc = 0; intSrc < GIC_NUM_INTRS; intSrc++) {
		if ((gic_intr_map[intSrc].pin + 1) == irqNum)
			return intSrc;
	}

	return -1;
}

void tc_enable_irq(unsigned int irq)
{
	int intr = get_gic_shared_intr(irq);

	if (intr < 0 || !gic_present)
		return;

	GIC_SET_INTR_MASK(intr);
}
EXPORT_SYMBOL(tc_enable_irq);

void tc_disable_irq(unsigned int irq)
{
	int intr = get_gic_shared_intr(irq);

	if (intr < 0 || !gic_present)
		return;

	GIC_CLR_INTR_MASK(intr);
}
EXPORT_SYMBOL(tc_disable_irq);

void tc_disable_irq_all(void)
{
	unsigned long flags;

	if (!gic_present)
		return;

	local_irq_save(flags);
	GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_RMASK_OFS + 0x00), 0xffffffff);
	GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_RMASK_OFS + 0x04), 0xffffffff);
	local_irq_restore(flags);
}

unsigned int get_c0_compare_int(void)
{
	unsigned int cpu = smp_processor_id();

	/* enable interrupt masks for external cpu timer 1/2/3 */
	if (cpu > 0 && gic_present)
		GIC_SET_INTR_MASK(timers_intSrcNum[cpu]);

	return SI_TIMER_INT;
}

void __init gic_platform_init(int irqs, struct irq_chip *irq_controller)
{
	int i;

	/* irqVec starts from 1 and ends at 63 */
	for (i = 1; i < INTR_SOURCES_NUM; i++)
		irq_set_chip(gic_irq_base + i, irq_controller);

	/* Initialize IRQ action handlers */
	for (i = 1; i < INTR_SOURCES_NUM; i++) {
		if (i >= IPI_RESCHED_INT0)
			break;

		if (i == SI_TIMER_INT)
			irq_set_handler(gic_irq_base + i, handle_percpu_irq);
		else if (i == GIC_EDGE_NMI)
			irq_set_handler(gic_irq_base + i, handle_edge_irq);
		else
			irq_set_handler(gic_irq_base + i, handle_level_irq);
	}

	/* bind watchdog Intr to CPU1 */
	GIC_SH_MAP_TO_VPE_SMASK(get_gic_shared_intr(TIMER5_INT), 1);
}

void __init arch_init_irq(void)
{
	phys_addr_t gic_base = RALINK_GIC_BASE;

	/* Disable all hardware interrupts */
	clear_c0_status(ST0_IM);
	clear_c0_cause(CAUSEF_IP);

	if (mips_cm_present()) {
		gic_base = read_gcr_gic_base() & ~CM_GCR_GIC_BASE_GICEN_MSK;

		write_gcr_gic_base(gic_base | CM_GCR_GIC_BASE_GICEN_MSK);
		__sync();
	}

	gic_present = true;

	gic_init(gic_base, RALINK_GIC_ADDRSPACE_SZ, gic_intr_map,
			ARRAY_SIZE(gic_intr_map), MIPS_GIC_IRQ_BASE);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int irq = (read_c0_cause() & ST0_IM) >> 10;

	do_IRQ(irq);
}
