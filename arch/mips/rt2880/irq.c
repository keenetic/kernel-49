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

static unsigned int fiq_mask;

unsigned int get_c0_compare_int(void)
{
	return SURFBOARDINT_MIPS_TIMER;
}

#if IS_ENABLED(CONFIG_HW_PERF_EVENTS) || IS_ENABLED(CONFIG_OPROFILE)
int get_c0_perfcount_int(void)
{
	return SURFBOARDINT_PCTRL;
}
EXPORT_SYMBOL_GPL(get_c0_perfcount_int);
#endif

static void mask_ralink_irq(struct irq_data *id)
{
	unsigned int irq_mask = BIT(id->hwirq);

	if (irq_mask & fiq_mask)
		*(volatile u32 *)(RALINK_FIQDIS) = irq_mask;
	else
		*(volatile u32 *)(RALINK_INTDIS) = irq_mask;
}

static void unmask_ralink_irq(struct irq_data *id)
{
	unsigned int irq_mask = BIT(id->hwirq);

	if (irq_mask & fiq_mask)
		*(volatile u32 *)(RALINK_FIQENA) = irq_mask;
	else
		*(volatile u32 *)(RALINK_INTENA) = irq_mask;
}

static struct irq_chip ralink_irq_chip = {
	.name		= "INTC",
	.irq_mask	= mask_ralink_irq,
	.irq_mask_ack	= mask_ralink_irq,
	.irq_unmask	= unmask_ralink_irq,
	.irq_disable	= mask_ralink_irq,
	.irq_enable	= unmask_ralink_irq,
};

static int ralink_intc_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &ralink_irq_chip, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops ralink_intc_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = ralink_intc_map,
};

static void ralink_intc_irq_handler(struct irq_desc *desc)
{
	struct irq_domain *domain = irq_desc_get_handler_data(desc);
	u32 pending;

	if (irq_desc_get_irq(desc) == MIPS_INTC_CHAIN_HW1)
		pending = *(volatile u32 *)(RALINK_IRQ1STAT);
	else
		pending = *(volatile u32 *)(RALINK_IRQ0STAT);

	if (unlikely(!pending)) {
		spurious_interrupt();
		return;
	}

	generic_handle_irq(irq_find_mapping(domain, __ffs(pending)));
}

static void dispatch_mips_line_timer(void)
{
	do_IRQ(SURFBOARDINT_MIPS_TIMER);
}

static void dispatch_mips_line_wlan(void)
{
	do_IRQ(SURFBOARDINT_WLAN);
}

static void dispatch_mips_line_fe(void)
{
	do_IRQ(SURFBOARDINT_FE);
}

static void dispatch_mips_line_pci(void)
{
	do_IRQ(SURFBOARDINT_PCIE0);
}

static void dispatch_mips_chain_hw1(void)
{
	do_IRQ(MIPS_INTC_CHAIN_HW1);
}

static void dispatch_mips_chain_hw0(void)
{
	do_IRQ(MIPS_INTC_CHAIN_HW0);
}

void mtk_disable_irq_all(void)
{
	unsigned long flags;

	local_irq_save(flags);
	*(volatile u32 *)(RALINK_FIQDIS) = ~0u;
	*(volatile u32 *)(RALINK_INTDIS) = ~0u;
	local_irq_restore(flags);
}

void __init arch_init_irq(void)
{
	struct irq_domain *domain;
	unsigned int int_type = 0;

	mips_cpu_irq_init();

	/* disable all interrupts */
	*(volatile u32 *)(RALINK_FIQDIS) = ~0u;
	*(volatile u32 *)(RALINK_INTDIS) = ~0u;

	/* route some INTC interrupts to MIPS HW1 interrupt (high priority) */
#ifdef RALINK_INTCTL_UHST
	int_type |= RALINK_INTCTL_UHST;
#endif
#ifdef RALINK_INTCTL_SDXC
	int_type |= RALINK_INTCTL_SDXC;
#endif
#ifdef RALINK_INTCTL_CRYPTO
	int_type |= RALINK_INTCTL_CRYPTO;
#endif
	int_type |= RALINK_INTCTL_DMA;
	*(volatile u32 *)(RALINK_INTTYPE) = int_type;

	if (cpu_has_vint) {
		pr_info("Setting up vectored interrupts\n");
		set_vi_handler(SURFBOARDINT_MIPS_TIMER, dispatch_mips_line_timer);
		set_vi_handler(SURFBOARDINT_WLAN,	dispatch_mips_line_wlan);
		set_vi_handler(SURFBOARDINT_FE,		dispatch_mips_line_fe);
		set_vi_handler(SURFBOARDINT_PCIE0,	dispatch_mips_line_pci);
		set_vi_handler(MIPS_INTC_CHAIN_HW1,	dispatch_mips_chain_hw1);
		set_vi_handler(MIPS_INTC_CHAIN_HW0,	dispatch_mips_chain_hw0);
	}

	domain = irq_domain_add_legacy(NULL, INTC_NUM_INTRS, MIPS_INTC_IRQ_BASE, 0, &ralink_intc_domain_ops, NULL);
	if (!domain)
		panic("Failed to add irqdomain");

	irq_set_chained_handler_and_data(MIPS_INTC_CHAIN_HW0, ralink_intc_irq_handler, domain);
	irq_set_chained_handler_and_data(MIPS_INTC_CHAIN_HW1, ralink_intc_irq_handler, domain);

	/* enable global interrupt bit */
	fiq_mask = int_type;
	*(volatile u32 *)(RALINK_FIQENA) = BIT(31);
	*(volatile u32 *)(RALINK_INTENA) = BIT(31);
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
		do_IRQ(SURFBOARDINT_MIPS_TIMER);

	if (pending & CAUSEF_IP5)
		do_IRQ(SURFBOARDINT_FE);

	if (pending & CAUSEF_IP6)
		do_IRQ(SURFBOARDINT_WLAN);

	if (pending & CAUSEF_IP4)
		do_IRQ(SURFBOARDINT_PCIE0);

	if (pending & CAUSEF_IP3)
		do_IRQ(MIPS_INTC_CHAIN_HW1);

	if (pending & CAUSEF_IP2)
		do_IRQ(MIPS_INTC_CHAIN_HW0);
}

