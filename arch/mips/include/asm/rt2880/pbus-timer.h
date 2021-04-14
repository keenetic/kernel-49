#ifndef __ASM_MACH_RALINK_PBUS_TIMER_H
#define __ASM_MACH_RALINK_PBUS_TIMER_H

#include <linux/io.h>
#include <linux/kernel.h>
#include <asm/rt2880/rt_mmap.h>

#define RSTSTAT					  (RALINK_SYSCTL_BASE + 0x38)

#define RSTSTAT_WDT2SYSRST_EN			  (1U << 31)
#define RSTSTAT_WDT2RSTO_EN			  (1U << 30)
#define RSTSTAT_SWSYSRST			  (1U <<  2)
#define RSTSTAT_WDRST				  (1U <<  1)

#define RSTSTAT_WDT_EN				  \
	(RSTSTAT_WDT2SYSRST_EN |		  \
	 RSTSTAT_WDT2RSTO_EN)

#define RSTSTAT_RST_STATE_MASK			  \
	(RSTSTAT_SWSYSRST |			  \
	 RSTSTAT_WDRST)

#define PBUS_TIMER_0				  (0)
#define PBUS_TIMER_1				  (1)
#define PBUS_TIMER_2				  (2)

#define PBUS_TIMER_WATCHDOG			  (PBUS_TIMER_1)

#define PBUS_TIMER_0_IRQ			  (SURFBOARDINT_TIMER0)
#define PBUS_TIMER_1_IRQ			  (SURFBOARDINT_WDG)
#define PBUS_TIMER_2_IRQ			  (SURFBOARDINT_TIMER1)

#define PBUS_TIMER_GLB				  (RALINK_TIMER_BASE + 0x00)
#define PBUS_TIMER_GLB_RST_SHIFT		  (8)
#define PBUS_TIMER_GLB_INT_SHIFT		  (0)

#define PBUS_TIMER_GLB_RST(n)			  \
	(1U << ((n) + PBUS_TIMER_GLB_RST_SHIFT))
#define PBUS_TIMER_GLB_INT_ACK(n)		  \
	(1U << ((n) + PBUS_TIMER_GLB_INT_SHIFT))

#define PBUS_TIMER_GLB_RST_MASK			  \
	(PBUS_TIMER_GLB_RST(PBUS_TIMER_0) |	  \
	 PBUS_TIMER_GLB_RST(PBUS_TIMER_1) |	  \
	 PBUS_TIMER_GLB_RST(PBUS_TIMER_2))

#define PBUS_TIMER_GLB_INT_ACK_MASK		  \
	(PBUS_TIMER_GLB_INT_ACK(PBUS_TIMER_0) |	  \
	 PBUS_TIMER_GLB_INT_ACK(PBUS_TIMER_1) |	  \
	 PBUS_TIMER_GLB_INT_ACK(PBUS_TIMER_2))

#define PBUS_TIMER_CTL(n)			  \
	((RALINK_TIMER_BASE) + 0x10 * (n) + 0x10)

#define PBUS_TIMER_LMT(n)			  \
	((RALINK_TIMER_BASE) + 0x10 * (n) + 0x14)

#define PBUS_TIMER_VAL(n)			  \
	((RALINK_TIMER_BASE) + 0x10 * (n) + 0x18)

#define PBUS_TIMER_SCALE_MS			  (1000)

#define PBUS_TIMER_CTL_PRES_MASK		  (0xffff0000)
#define PBUS_TIMER_CTL_PRES(scale)		  \
	(((scale) << 16) & PBUS_TIMER_CTL_PRES_MASK)
#define PBUS_TIMER_CTL_PRES_GET(ctl)		  \
	(((ctl) & PBUS_TIMER_CTL_PRES_MASK) >> 16)

#define PBUS_TIMER_CTL_EN			  (1U << 7)
#define PBUS_TIMER_CTL_AL			  (1U << 4)

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
	const u32 ctl_reg = PBUS_TIMER_CTL(n);
	u32 limit = interval_ms;
	u32 scale = 1;
	u32 ctl;

	if (mode == PBUS_TIMER_MODE_WATCHDOG) {
		u32 val;

		/* enable a watchdog function for a timer
		 * that does not support it
		 */
		if (n != PBUS_TIMER_WATCHDOG)
			return false;

		val = pbus_timer_r32(RSTSTAT);

		val &= ~RSTSTAT_RST_STATE_MASK;
		val |=  RSTSTAT_WDT_EN;
		pbus_timer_w32(RSTSTAT, val);
	} else {
		/* use a watchdog timer as an interval or periodic timer
		 */
		if (n == PBUS_TIMER_WATCHDOG) {
			u32 val = pbus_timer_r32(RSTSTAT);

			val &= ~RSTSTAT_RST_STATE_MASK;
			val &= ~RSTSTAT_WDT_EN;
			pbus_timer_w32(RSTSTAT, val);
		}
	}

	while (limit > U16_MAX) {
		scale <<= 1;
		limit >>= 1;

		/* an interval is too large */
		if (PBUS_TIMER_SCALE_MS * scale > U16_MAX)
			return false;
	}

	pbus_timer_w32(PBUS_TIMER_LMT(n), limit);

	ctl = pbus_timer_r32(ctl_reg);

	ctl &= PBUS_TIMER_CTL_PRES_MASK;
	ctl |= PBUS_TIMER_CTL_PRES(PBUS_TIMER_SCALE_MS * scale);
	ctl |= PBUS_TIMER_CTL_EN;

	if (mode == PBUS_TIMER_MODE_PERIODIC)
		ctl |=  PBUS_TIMER_CTL_AL;
	else
		ctl &= ~PBUS_TIMER_CTL_AL;

	pbus_timer_w32(ctl_reg, ctl);

	return true;
}

static inline void
pbus_timer_disable(const unsigned int n)
{
	const u32 ctl_reg = PBUS_TIMER_CTL(n);
	const u32 ctl = pbus_timer_r32(ctl_reg);

	pbus_timer_w32(ctl_reg, ctl & ~PBUS_TIMER_CTL_EN);
}

static inline void
pbus_timer_restart(const unsigned int n)
{
	u32 glb = pbus_timer_r32(PBUS_TIMER_GLB);

	glb &= ~PBUS_TIMER_GLB_RST_MASK;
	glb &= ~PBUS_TIMER_GLB_INT_ACK_MASK;
	glb |=  PBUS_TIMER_GLB_RST(n);
	pbus_timer_w32(PBUS_TIMER_GLB, glb);
}

static inline void
pbus_timer_int_ack(const unsigned int n)
{
	u32 glb = pbus_timer_r32(PBUS_TIMER_GLB);

	glb &= ~PBUS_TIMER_GLB_RST_MASK;
	glb &= ~PBUS_TIMER_GLB_INT_ACK_MASK;
	glb |=  PBUS_TIMER_GLB_INT_ACK(n);
	pbus_timer_w32(PBUS_TIMER_GLB, glb);
}

static inline unsigned int
pbus_timer_get(const unsigned int n)
{
	const u32 val = pbus_timer_r32(PBUS_TIMER_VAL(n));
	const u32 ctl = pbus_timer_r32(PBUS_TIMER_CTL(n));
	const u32 pres = PBUS_TIMER_CTL_PRES_GET(ctl);

	return val * (pres / PBUS_TIMER_SCALE_MS);
}

#endif /* __ASM_MACH_RALINK_PBUS_TIMER_H */

