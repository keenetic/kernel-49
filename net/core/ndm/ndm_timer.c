#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/hrtimer.h>

ktime_t ndm_ktime(void)
{
	return ktime_get();
}
EXPORT_SYMBOL(ndm_ktime);

void ndm_timer_init(struct hrtimer *timer,
		    enum hrtimer_restart (*func)(struct hrtimer *timer))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_PINNED);
	timer->function = func;
}
EXPORT_SYMBOL(ndm_timer_init);

void ndm_mod_timer(struct hrtimer *timer, ktime_t tim)
{
	hrtimer_start(timer, tim, HRTIMER_MODE_ABS_PINNED);
}
EXPORT_SYMBOL(ndm_mod_timer);

void ndm_timer_restart(struct hrtimer *timer)
{
	hrtimer_start_expires(timer, HRTIMER_MODE_ABS_PINNED);
}
EXPORT_SYMBOL(ndm_timer_restart);

void ndm_timer_forward(struct hrtimer *timer, ktime_t interval)
{
	hrtimer_forward(timer, timer->base->get_time(), interval);
}
EXPORT_SYMBOL(ndm_timer_forward);

bool ndm_timer_pending(const struct hrtimer *timer)
{
	return hrtimer_active(timer);
}
EXPORT_SYMBOL(ndm_timer_pending);

int ndm_timer_del_sync(struct hrtimer *timer)
{
	return hrtimer_cancel(timer);
}
EXPORT_SYMBOL(ndm_timer_del_sync);

int ndm_timer_del(struct hrtimer *timer)
{
	return hrtimer_try_to_cancel(timer);
}
EXPORT_SYMBOL(ndm_timer_del);
