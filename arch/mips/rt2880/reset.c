/**************************************************************************
 *
 *  BRIEF MODULE DESCRIPTION
 *     reboot/reset setting for Ralink RT2880 solution
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
 *
 * Initial Release
 *
 *
 *
 **************************************************************************
 */

#include <linux/init.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>

#include <asm/time.h>
#include <asm/reboot.h>
#include <asm/pbus-timer.h>

#include <asm/rt2880/generic.h>
#include <asm/rt2880/eureka_ep430.h>

extern void mtk_disable_irq_all(void);

static void hw_uninit(bool do_reboot)
{
	mtk_disable_irq_all();

	/* stop all APB module timers except an active watchdog */
	pbus_timer_disable(PBUS_TIMER_0);
	pbus_timer_disable(PBUS_TIMER_2);

	/* timer 3 is in a non-watchdog mode */
	if (!(pbus_timer_r32(RSTSTAT) & RSTSTAT_WDT_EN))
		pbus_timer_disable(PBUS_TIMER_1);

#if defined(CONFIG_RALINK_MT7628)
#ifdef CONFIG_PCI
	/* assert PERST */
	RALINK_PCI_PCICFG_ADDR |= (1 << 1);
	udelay(100);
#endif
	/* reset PCIe */
	*(volatile u32 *)(SOFTRES_REG) = RALINK_PCIE0_RST;

	if (do_reboot)
		mdelay(10);
#endif

	if (do_reboot) {
		/* system software reset */
		*(volatile u32 *)(SOFTRES_REG) = GORESET;
		*(volatile u32 *)(SOFTRES_REG) = 0;
	}
}

static void mips_machine_restart(char *command)
{
	printk(KERN_WARNING "Machine restart ... \n");

	hw_uninit(true);
}

static void mips_machine_halt(void)
{
	printk(KERN_WARNING "Machine halted ... \n");

	hw_uninit(false);
}

static void mips_machine_power_off(void)
{
	printk(KERN_WARNING "Machine poweroff ... \n");

	*(volatile u32*)(POWER_DIR_REG) = POWER_DIR_OUTPUT;
	*(volatile u32*)(POWER_POL_REG) = 0;
	*(volatile u32*)(POWEROFF_REG) = POWEROFF;
}

static int mtk_panic_event(struct notifier_block *nb,
			   unsigned long action, void *data)
{
	if (action == PANIC_ACTION_RESTART)
		mips_machine_restart(NULL);
	else
		mips_machine_halt();

	return NOTIFY_DONE;
}

static struct notifier_block mtk_panic_block = {
	.notifier_call = mtk_panic_event,
};

void mips_reboot_setup(void)
{
	_machine_restart = mips_machine_restart;
	_machine_halt = mips_machine_halt;
	pm_power_off = mips_machine_power_off;

	atomic_notifier_chain_register(&panic_notifier_list, &mtk_panic_block);
}
