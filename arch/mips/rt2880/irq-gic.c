/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     Interrupt routines for Ralink RT2880 solution
 *
 *  Copyright 2007 Ralink Inc. (bruce_chang@ralinktech.com.tw)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 **************************************************************************
 * May 2007 Bruce Chang
 * Initial Release
 *
 * May 2009 Bruce Chang
 * support RT3883 PCIe
 *
 **************************************************************************
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/mips-cm.h>
#include <asm/irq_cpu.h>
#include <asm/gic.h>

#include <asm/rt2880/rt_mmap.h>
#include <asm/rt2880/surfboardint.h>

#define X GIC_UNUSED
static const struct gic_intr_map gic_intr_map[GIC_NUM_INTRS] = {
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 00: CPU Coherence Manager Error
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 01: CPU CM Perf Cnt overflow
	{ X, X,            X,           X,              0 },	// 02: N/A
	{ 0, GIC_CPU_INT3, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 03: FE
	{ 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 04: PCIe0
	{ X, X,            X,           X,              0 },	// 05: AUX_STCK
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 06: SYSCTL
	{ X, X,            X,           X,              0 },	// 07: N/A
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 08: I2C
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 09: DRAMC
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 10: PCM
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 11: HS GDMA
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 12: GPIO
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 13: GDMA
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 14: NFI NAND
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 15: NFI ECC
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 16: I2S
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 17: SPI
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 18: SPDIF
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 19: CryptoEngine
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 20: SDXC
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 21: Rbus to Pbus
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 22: USB XHCI
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 23: ESW MT7530
	{ 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 24: PCIe1
	{ 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 25: PCIe2
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 26: UART 1
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 27: UART 2
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 28: UART 3
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 29: Timer WDG
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 30: Timer0
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, 0 },	// 31: Timer1
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 32: PCIE_P0_LINT_DOWN_RST
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 33: PCIE_P1_LINT_DOWN_RST
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 34: PCIE_P2_LINT_DOWN_RST
	{ X, X,            X,           X,              0 },	// 35: N/A
	{ X, X,            X,           X,              0 },	// 36: N/A
	{ X, X,            X,           X,              0 },	// 37: N/A
	{ X, X,            X,           X,              0 },	// 38: N/A
	{ X, X,            X,           X,              0 },	// 39: N/A
	{ X, X,            X,           X,              0 },	// 40: N/A
	{ X, X,            X,           X,              0 },	// 41: N/A
	{ X, X,            X,           X,              0 },	// 42: N/A
	{ X, X,            X,           X,              0 },	// 43: N/A
	{ X, X,            X,           X,              0 },	// 44: N/A
	{ X, X,            X,           X,              0 },	// 45: N/A
	{ X, X,            X,           X,              0 },	// 46: N/A
	{ X, X,            X,           X,              0 },	// 47: N/A
	{ X, X,            X,           X,              0 },	// 48: N/A
	{ X, X,            X,           X,              0 },	// 49: N/A
	{ X, X,            X,           X,              0 },	// 50: N/A
	{ X, X,            X,           X,              0 },	// 51: N/A
	{ X, X,            X,           X,              0 },	// 52: N/A
	{ X, X,            X,           X,              0 },	// 53: N/A
	{ X, X,            X,           X,              0 },	// 54: N/A
	{ X, X,            X,           X,              0 },	// 55: N/A
	{ 0, GIC_CPU_INT1, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 56: IPI resched 0
	{ 1, GIC_CPU_INT1, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 57: IPI resched 1
	{ 2, GIC_CPU_INT1, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 58: IPI resched 2
	{ 3, GIC_CPU_INT1, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 59: IPI resched 3
	{ 0, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 60: IPI call 0
	{ 1, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 61: IPI call 1
	{ 2, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_EDGE,  0 },	// 62: IPI call 2
	{ 3, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_EDGE,  0 }	// 63: IPI call 3
};
#undef X

static int mips_cpu_timer_irq = CP0_LEGACY_COMPARE_IRQ;

static void mips_timer_dispatch(void)
{
	do_IRQ(mips_cpu_timer_irq);
}

unsigned int get_c0_compare_int(void)
{
	mips_cpu_timer_irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;

	return mips_cpu_timer_irq;
}

void mtk_disable_irq_all(void)
{
	unsigned long flags;

	if (!gic_present)
		return;

	local_irq_save(flags);
	GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_RMASK_OFS + 0x00), 0xffffffff);
	GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_RMASK_OFS + 0x04), 0xffffffff);
	local_irq_restore(flags);
}

void __init gic_platform_init(int irqs, struct irq_chip *irq_controller)
{
	int i;

	for (i = 0; i < irqs; i++)
		irq_set_chip(gic_irq_base + i, irq_controller);

	for (i = 0; i < (GIC_NUM_INTRS - 8); i++) {
		if (gic_intr_map[i].cpunum == GIC_UNUSED)
			continue;

		if (gic_intr_map[i].trigtype == GIC_TRIG_EDGE)
			irq_set_handler(gic_irq_base + i, handle_edge_irq);
		else
			irq_set_handler(gic_irq_base + i, handle_level_irq);
	}
}

void __init arch_init_irq(void)
{
	phys_addr_t gic_base = RALINK_GIC_BASE;

	mips_cpu_irq_init();

	if (mips_cm_present()) {
		gic_base = read_gcr_gic_base() & ~CM_GCR_GIC_BASE_GICEN_MSK;

		write_gcr_gic_base(gic_base | CM_GCR_GIC_BASE_GICEN_MSK);
		__sync();
	}

	gic_present = true;

	gic_init(gic_base, RALINK_GIC_ADDRSPACE_SZ, gic_intr_map,
		ARRAY_SIZE(gic_intr_map), MIPS_GIC_IRQ_BASE + GIC_SHARED_HWIRQ_BASE);

	if (cpu_has_vint) {
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT0, gic_irq_dispatch);	// Other
#ifdef CONFIG_MIPS_MT_SMP
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT1, gic_irq_dispatch);	// IPI resched
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT2, gic_irq_dispatch);	// IPI call
#endif
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT3, gic_irq_dispatch);	// FE
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT4, gic_irq_dispatch);	// PCIe
		set_vi_handler(GIC_CPU_PIN_OFFSET + GIC_CPU_INT5, mips_timer_dispatch);
	}

#ifdef CONFIG_MIPS_MT_SMP
	set_c0_status(STATUSF_IP7 | STATUSF_IP6 | STATUSF_IP5 | STATUSF_IP2 |
		      STATUSF_IP4 | STATUSF_IP3);
#else
	set_c0_status(STATUSF_IP7 | STATUSF_IP6 | STATUSF_IP5 | STATUSF_IP2);
#endif
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending;

	pending = read_c0_status() & read_c0_cause() & ST0_IM;
	if (unlikely(!pending)) {
		spurious_interrupt();
		return;
	}

	if (pending & CAUSEF_IP7)
		mips_timer_dispatch();

	if (pending & (CAUSEF_IP6 | CAUSEF_IP5 | CAUSEF_IP4 | CAUSEF_IP3 | CAUSEF_IP2))
		gic_irq_dispatch();
}
