#ifndef _ASM_PBUS_TIMER_H
#define _ASM_PBUS_TIMER_H

enum pbus_timer_mode {
	PBUS_TIMER_MODE_INTERVAL,
	PBUS_TIMER_MODE_PERIODIC,
	PBUS_TIMER_MODE_WATCHDOG
};

static bool pbus_timer_enable(const unsigned int n,
			      const unsigned int interval_ms,
			      const enum pbus_timer_mode mode);

static void pbus_timer_disable(const unsigned int n);

static void pbus_timer_restart(const unsigned int n);

static void pbus_timer_int_ack(const unsigned int n);

static unsigned int pbus_timer_get(const unsigned int n);

#include <pbus-timer.h>

#endif /* _ASM_PBUS_TIMER_H */

