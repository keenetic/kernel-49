/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2006, Ralink Technology, Inc.
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
 ***************************************************************************

    Module Name:
    rt_timer.c

    Abstract:

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name        Date            Modification logs
    Steven Liu  2007-07-04      Initial version
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <asm/uaccess.h>

#include <asm/rt2880/surfboard.h>

#include "rt_timer.h"

#define WDG_REFRESH_POLL	(HZ * CONFIG_RALINK_TIMER_WDG_REFRESH_INTERVAL)

static struct timer_list wdg_timer;

void set_wdg_timer_ebl(unsigned int timer, unsigned int ebl)
{
	unsigned int result;

	result = sysRegRead(timer);

	if (ebl)
		result |= (1 << 7);
	else
		result &= ~(1 << 7);

	sysRegWrite(timer, result);

	// timer1 used for watchdog timer
#if defined(CONFIG_RALINK_TIMER_WDG_RESET_OUTPUT)
	if (timer != TMR1CTL)
		return;

#if defined(CONFIG_RALINK_MT7621)
	/*
	 * GPIOMODE[9:8] GPIO_MODE
	 * 00: Watch dog
	 * 01: GPIO
	 * 10: Reference clock
	 * 11: Reference clock
	 */
	result = sysRegRead(GPIOMODE);
	result &= ~(0x3 << 8);
	if (!ebl)
		result |= (0x1 << 8);
	sysRegWrite(GPIOMODE, result);
#elif defined(CONFIG_RALINK_MT7628)
	/*
	 * GPIOMODE[14] GPIO_MODE
	 * 0: Watch dog
	 * 1: GPIO
	 */
	result = sysRegRead(GPIOMODE);
	if (ebl)
		result &= ~(0x1 << 14);
	else
		result |=  (0x1 << 14);
	sysRegWrite(GPIOMODE, result);
#endif
#endif
}

void set_wdg_timer_clock_prescale(int prescale)
{
	unsigned int result;

	result  = sysRegRead(TMR1CTL);
	result &= 0x0000FFFF;
	result |= (prescale << 16); //unit = 1u
	sysRegWrite(TMR1CTL, result);
}

static void on_refresh_wdg_timer(unsigned long unused)
{
	/* WDTRST */
	sysRegWrite(TMRSTAT, (1 << 9));

#ifdef CONFIG_SMP
	if (!timer_pending(&wdg_timer)) {
		const int current_cpu = get_cpu();
		int next_cpu = cpumask_next(current_cpu, cpu_online_mask);

		if (next_cpu >= nr_cpu_ids)
			next_cpu = cpumask_first(cpu_online_mask);

		wdg_timer.expires = jiffies + WDG_REFRESH_POLL;
		add_timer_on(&wdg_timer, next_cpu);

		put_cpu();
	} else
#endif
		mod_timer(&wdg_timer, jiffies + WDG_REFRESH_POLL);
}

int __init ralink_wdt_init_module(void)
{
#ifdef CONFIG_SMP
	setup_pinned_timer(&wdg_timer, on_refresh_wdg_timer, 0);
#else
	setup_timer(&wdg_timer, on_refresh_wdg_timer, 0);
#endif

	/* initialize WDG timer (Timer1) */
	set_wdg_timer_clock_prescale(1000); /* 1ms */
	sysRegWrite(TMR1LOAD, CONFIG_RALINK_TIMER_WDG_REBOOT_DELAY * 1000);

	on_refresh_wdg_timer(0);

	set_wdg_timer_ebl(TMR1CTL, 1);

	printk(KERN_INFO "Load Ralink WDG Timer Module\n");

	return 0;
}

void __exit ralink_wdt_exit_module(void)
{
	set_wdg_timer_ebl(TMR1CTL, 0);

	del_timer_sync(&wdg_timer);
}

module_init(ralink_wdt_init_module);
module_exit(ralink_wdt_exit_module);

MODULE_DESCRIPTION("Ralink Kernel WDG Timer Module");
MODULE_AUTHOR("Steven Liu");
MODULE_LICENSE("GPL");
