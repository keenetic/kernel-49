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
#include <asm/uaccess.h>

#include <asm/rt2880/surfboard.h>

#include "rt_timer.h"

static int wdg_load_value;
static struct timer_list wdg_timer;

void set_wdg_timer_ebl(unsigned int timer, unsigned int ebl)
{
	unsigned int result;

	result = sysRegRead(timer);

	if (ebl)
		result |= (1<<7);
	else
		result &= ~(1<<7);

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
	sysRegWrite(TMRSTAT, (1 << 9)); //WDTRST

	mod_timer(&wdg_timer, jiffies + (HZ * CONFIG_RALINK_TIMER_WDG_REFRESH_INTERVAL));
}

int __init ralink_wdt_init_module(void)
{
	// initialize WDG timer (Timer1)
	setup_timer(&wdg_timer, on_refresh_wdg_timer, 0);

	set_wdg_timer_clock_prescale(1000); //1ms
	wdg_load_value = CONFIG_RALINK_TIMER_WDG_REBOOT_DELAY * 1000;
	sysRegWrite(TMR1LOAD, wdg_load_value);

	on_refresh_wdg_timer(0);

	set_wdg_timer_ebl(TMR1CTL, 1);

	printk(KERN_INFO "Load Ralink WDG Timer Module\n");

	return 0;
}

void __exit ralink_wdt_exit_module(void)
{
	printk(KERN_INFO "Unload Ralink WDG Timer Module\n");

	set_wdg_timer_ebl(TMR1CTL, 0);
	del_timer_sync(&wdg_timer);
}

module_init(ralink_wdt_init_module);
module_exit(ralink_wdt_exit_module);

MODULE_DESCRIPTION("Ralink Kernel WDG Timer Module");
MODULE_AUTHOR("Steven Liu");
MODULE_LICENSE("GPL");
