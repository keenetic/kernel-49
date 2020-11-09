#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <asm/uaccess.h>

#include <asm/tc3162/tc3162.h>

#define WDG_REFRESH_POLL	(HZ * CONFIG_RALINK_TIMER_WDG_REFRESH_INTERVAL)

static struct timer_list wdg_timer;

static void on_refresh_wdg_timer(unsigned long unused)
{
	VPint(CR_WDOG_RLD) = 0x1;

	if (!timer_pending(&wdg_timer)) {
		const int current_cpu = get_cpu();
		int next_cpu = cpumask_next(current_cpu, cpu_online_mask);

		if (next_cpu >= nr_cpu_ids)
			next_cpu = cpumask_first(cpu_online_mask);

		wdg_timer.expires = jiffies + WDG_REFRESH_POLL;
		add_timer_on(&wdg_timer, next_cpu);

		put_cpu();
	} else
		mod_timer(&wdg_timer, jiffies + WDG_REFRESH_POLL);
}

int __init tc3262_wdt_init(void)
{
	/* initialize WDG timer */
	setup_pinned_timer(&wdg_timer, on_refresh_wdg_timer, 0);

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
