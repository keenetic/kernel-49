#ifndef _GPIO_WATCHDOG_MT762X_H
#define _GPIO_WATCHDOG_MT762X_H

#if defined(CONFIG_RALINK_MT7621)
#define MT762X_PIO_BASE			0xbe000600
#define MT762X_MAX_PINS			62
#elif defined(CONFIG_RALINK_MT7628)
#define MT762X_PIO_BASE			0xb0000600
#define MT762X_MAX_PINS			47
#else
#error MT762X SoC not defined
#endif

#define MT762X_REG_PIO_DSET		(MT762X_PIO_BASE + 0x30)
#define MT762X_REG_PIO_DCLR		(MT762X_PIO_BASE + 0x40)
#define MT762X_PIO_MSK(pin_no)		(1U << ((pin_no) % 32))
#define MT762X_PIO_REG(pin_no, offs)	((offs) + ((pin_no) / 32) * 4)

static inline void mt762x_gpio_set_value(unsigned int gpio, bool one)
{
	unsigned int offs = one ? MT762X_REG_PIO_DSET : MT762X_REG_PIO_DCLR;
	unsigned int mask = MT762X_PIO_MSK(gpio);
	unsigned long reg = MT762X_PIO_REG(gpio, offs);

	__raw_writel(mask, (void __iomem *)reg);
}

#endif /* _GPIO_WATCHDOG_MT762X_H */
