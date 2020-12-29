#ifndef __ASM_MACH_TC3262_PBUS_TIMER_H
#define __ASM_MACH_TC3262_PBUS_TIMER_H

#include <linux/io.h>
#include <linux/kernel.h>
#include <asm/tc3162/tc3162.h>
#include <asm/tc3162/tc3262_int_source.h>

#define PBUS_TIMER_0				  (0)
#define PBUS_TIMER_1				  (1)
#define PBUS_TIMER_2				  (2)
#define PBUS_TIMER_3				  (5)

#define PBUS_TIMER_WATCHDOG			  (PBUS_TIMER_3)

#define PBUS_TIMER_0_IRQ			  (TIMER0_INT)
#define PBUS_TIMER_1_IRQ			  (TIMER1_INT)
#define PBUS_TIMER_2_IRQ			  (TIMER2_INT)
#define PBUS_TIMER_3_IRQ			  (TIMER5_INT)

#define PBUS_TIMER_CTRL				  (0xbfbf0100)
#define PBUS_TIMER_LVR(n)			  (0xbfbf0104 + (n) * 8)
#define PBUS_TIMER_CVR(n)			  (0xbfbf0108 + (n) * 8)
#define PBUS_TIMER_WDOGTHSLD			  (0xbfbf0134)
#define PBUS_TIMER_RLDWDOG			  (0xbfbf0138)

#define PBUS_TIMER_CTRL_HALT(n)			  (1U << (26 + (n)))
#define PBUS_TIMER_CTRL_WDG_ENABLE		  (1U << (25))
#define PBUS_TIMER_CTRL_INT_ACK(n)		  (1U << (16 + (n)))
#define PBUS_TIMER_CTRL_PERIODIC(n)		  (1U << (8  + (n)))
#define PBUS_TIMER_CTRL_ENABLE(n)		  (1U << (0  + (n)))

#define PBUS_TIMER_CTRL_INT_ACK_MASK		  \
	(PBUS_TIMER_CTRL_INT_ACK(PBUS_TIMER_0) |  \
	 PBUS_TIMER_CTRL_INT_ACK(PBUS_TIMER_1) |  \
	 PBUS_TIMER_CTRL_INT_ACK(PBUS_TIMER_2) |  \
	 PBUS_TIMER_CTRL_INT_ACK(PBUS_TIMER_3))

#define PBUS_TIMER_TICKS_IN_MSEC		  ((SYS_HCLK) / 2 * 1000)

static inline void
pbus_timer_w32(const u32 reg, const u32 val)
{
	__raw_writel(val, (void __iomem *)(unsigned long)reg);
}

static inline u32
pbus_timer_r32(const u32 reg)
{
	return __raw_readl((void __iomem *)(unsigned long)reg);
}

static inline bool
pbus_timer_enable(const unsigned int n,
		  const unsigned int interval_ms,
		  const enum pbus_timer_mode mode)
{
	u32 ctrl;

	/* an interval is too large */
	if (U32_MAX / interval_ms < PBUS_TIMER_TICKS_IN_MSEC)
		return false;

	ctrl = pbus_timer_r32(PBUS_TIMER_CTRL);

	if (mode == PBUS_TIMER_MODE_WATCHDOG) {
		/* enable a watchdog function for a timer
		 * that does not support it
		 */
		if (n != PBUS_TIMER_WATCHDOG)
			return false;

		ctrl |= PBUS_TIMER_CTRL_WDG_ENABLE;
		pbus_timer_w32(PBUS_TIMER_WDOGTHSLD, U32_MAX);
	} else {
		/* use a watchdog timer as an interval or periodic timer
		 */
		if (n == PBUS_TIMER_WATCHDOG)
			ctrl &= ~PBUS_TIMER_CTRL_WDG_ENABLE;
	}

	pbus_timer_w32(PBUS_TIMER_LVR(n),
		       interval_ms * PBUS_TIMER_TICKS_IN_MSEC);

	ctrl &= ~PBUS_TIMER_CTRL_INT_ACK_MASK;
	ctrl &= ~PBUS_TIMER_CTRL_HALT(n);
	ctrl |=  PBUS_TIMER_CTRL_ENABLE(n);

	if (mode == PBUS_TIMER_MODE_PERIODIC)
		ctrl |=  PBUS_TIMER_CTRL_PERIODIC(n);
	else
		ctrl &= ~PBUS_TIMER_CTRL_PERIODIC(n);

	pbus_timer_w32(PBUS_TIMER_CTRL, ctrl);

	return true;
}

static inline void
pbus_timer_disable(const unsigned int n)
{
	u32 ctrl = pbus_timer_r32(PBUS_TIMER_CTRL);

	ctrl &= ~PBUS_TIMER_CTRL_INT_ACK_MASK;
	ctrl &= ~PBUS_TIMER_CTRL_ENABLE(n);
	pbus_timer_w32(PBUS_TIMER_CTRL, ctrl);
}

static inline void
pbus_timer_restart(const unsigned int n)
{
	u32 ctrl = pbus_timer_r32(PBUS_TIMER_CTRL);

	ctrl &= ~PBUS_TIMER_CTRL_INT_ACK_MASK;
	ctrl &= ~PBUS_TIMER_CTRL_ENABLE(n);
	pbus_timer_w32(PBUS_TIMER_CTRL, ctrl);

	ctrl |=  PBUS_TIMER_CTRL_ENABLE(n);
	pbus_timer_w32(PBUS_TIMER_CTRL, ctrl);
}

static inline void
pbus_timer_int_ack(const unsigned int n)
{
	u32 ctrl = pbus_timer_r32(PBUS_TIMER_CTRL);

	ctrl &= ~PBUS_TIMER_CTRL_INT_ACK_MASK;
	ctrl |=  PBUS_TIMER_CTRL_INT_ACK(n);
	pbus_timer_w32(PBUS_TIMER_CTRL, ctrl);
}

static inline unsigned int
pbus_timer_get(const unsigned int n)
{
	return pbus_timer_r32(PBUS_TIMER_CVR(n)) / PBUS_TIMER_TICKS_IN_MSEC;
}

#endif /* __ASM_MACH_TC3262_PBUS_TIMER_H */

