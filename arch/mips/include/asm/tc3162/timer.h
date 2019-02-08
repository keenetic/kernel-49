
#ifndef _TIMER_TC3262_H_
#define _TIMER_TC3262_H_

#define TC_TIMER_WDG	5

void tc_timer_wdg(
	unsigned int tick_enable,
	unsigned int wdg_enable);

void tc_timer_set(
	unsigned int timer_no,
	unsigned int timerTime,
	unsigned int enable,
	unsigned int mode,
	unsigned int halt);

#endif
