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

