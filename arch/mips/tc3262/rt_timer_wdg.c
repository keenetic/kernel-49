#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <asm/uaccess.h>

#include <asm/tc3162/tc3162.h>

static struct timer_list wdg_timer;

static void on_refresh_wdg_timer(unsigned long unused)
{
	VPint(CR_WDOG_RLD) = 0x1;

	mod_timer(&wdg_timer, jiffies + (HZ * CONFIG_RALINK_TIMER_WDG_REFRESH_INTERVAL));
}

int __init tc3262_wdt_init(void)
{
	/* initialize WDG timer */
	setup_timer(&wdg_timer, on_refresh_wdg_timer, 0);

	/* setup WDG timeouts */
	tc_timer_set(TC_TIMER_WDG, CONFIG_RALINK_TIMER_WDG_REBOOT_DELAY * 100 * TIMERTICKS_10MS,
		     ENABLE, TIMER_TOGGLEMODE, TIMER_HALTDISABLE);

	/* enable WDG timer */
	tc_timer_wdg(ENABLE, ENABLE);

	/* reset WDG timer and start kernel timer */
	on_refresh_wdg_timer(0);

	printk(KERN_INFO "TC3262 WDG Timer Module\n");

	return 0;
}

void __exit tc3262_wdt_exit(void)
{
	/* disable WDG timer */
	tc_timer_wdg(DISABLE, DISABLE);

	del_timer_sync(&wdg_timer);
}

module_init(tc3262_wdt_init);
module_exit(tc3262_wdt_exit);

MODULE_DESCRIPTION("TC3262 WDG Timer Module");
MODULE_LICENSE("GPL");
