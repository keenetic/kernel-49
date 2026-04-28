/*
 * Driver for watchdog device controlled through GPIO-line
 *
 * Author: 2013, Alexander Shiyan <shc_work@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#else
#include "gpio_wdt.h"
#endif

#define SOFT_TIMEOUT_MIN	1
#define SOFT_TIMEOUT_DEF	60
#define SOFT_TIMEOUT_MAX	0xffff

#define HW_MARGIN_DEFAULT	 10000

enum {
	HW_ALGO_TOGGLE,
	HW_ALGO_LEVEL,
};

struct gpio_wdt_priv {
	int			gpio;
	bool			active_low;
	bool			always_running;
	bool			state;
	bool			armed;
	unsigned int		hw_algo;
	unsigned int		hw_margin;
	unsigned long		last_jiffies;
	struct timer_list	timer;
	struct watchdog_device	wdd;
};

static inline int
gpio_wdt_load_settings(struct gpio_wdt_priv *priv, struct device *dev)
{
	unsigned int hw_margin = HW_MARGIN_DEFAULT;
#ifdef CONFIG_OF
	enum of_gpio_flags flags;
	const char *algo;
	unsigned long f = 0;
	int ret;

	priv->gpio = of_get_gpio_flags(dev->of_node, 0, &flags);
	if (!gpio_is_valid(priv->gpio))
		return priv->gpio;

	priv->active_low = flags & OF_GPIO_ACTIVE_LOW;

	ret = of_property_read_string(dev->of_node, "hw_algo", &algo);
	if (ret)
		return ret;

	if (!strcmp(algo, "toggle")) {
		priv->hw_algo = HW_ALGO_TOGGLE;
		f = GPIOF_IN;
	} else if (!strcmp(algo, "level")) {
		priv->hw_algo = HW_ALGO_LEVEL;
		f = priv->active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
	} else {
		return -EINVAL;
	}

	/* Disallow values lower than 2 and higher than 65535 ms */
	ret = of_property_read_u32(dev->of_node, "hw_margin_ms", &hw_margin);
	if (ret || hw_margin < 2 || hw_margin > 65535)
		hw_margin = HW_MARGIN_DEFAULT;

	priv->always_running = of_property_read_bool(dev->of_node,
						     "always-running");

	ret = devm_gpio_request_one(dev, priv->gpio, f, dev_name(dev));
	if (ret)
		return ret;
#else
	priv->gpio = CONFIG_GPIO_WATCHDOG_LINE;
	if (priv->gpio < 0 || priv->gpio >= MT762X_MAX_PINS)
		return -EINVAL;

	priv->hw_algo = HW_ALGO_TOGGLE;
	priv->active_low = false;
	priv->always_running = true;
#endif

	/* Use safe value (1/2 of real timeout) */
	priv->hw_margin = msecs_to_jiffies(hw_margin / 2);

	return 0;
}

static inline void _gpio_set_value(int gpio, bool one)
{
#ifdef CONFIG_OF
	gpio_set_value_cansleep(gpio, one);
#else
	mt762x_gpio_set_value(gpio, one);
#endif
}

static inline void _gpio_set_direction(struct gpio_wdt_priv *priv, bool out)
{
#ifdef CONFIG_OF
	if (out)
		gpio_direction_output(priv->gpio, priv->state);
	else
		gpio_direction_input(priv->gpio);
#else
	/* set pin mode & direction from u-boot */
#endif
}

static void gpio_wdt_disable(struct gpio_wdt_priv *priv)
{
	_gpio_set_value(priv->gpio, !priv->active_low);

	/* Put GPIO back to tristate */
	if (priv->hw_algo == HW_ALGO_TOGGLE)
		_gpio_set_direction(priv, false);
}

static void gpio_wdt_hwping(unsigned long data)
{
	struct watchdog_device *wdd = (struct watchdog_device *)data;
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	if (priv->armed && time_after(jiffies, priv->last_jiffies +
				      msecs_to_jiffies(wdd->timeout * 1000))) {
		dev_crit(wdd->parent,
			 "Timer expired. System will reboot soon!\n");
		return;
	}

	/* Restart timer */
	mod_timer(&priv->timer, jiffies + priv->hw_margin);

	switch (priv->hw_algo) {
	case HW_ALGO_TOGGLE:
		/* Toggle output pin */
		priv->state = !priv->state;
		_gpio_set_value(priv->gpio, priv->state);
		break;
	case HW_ALGO_LEVEL:
		/* Pulse */
		_gpio_set_value(priv->gpio, !priv->active_low);
		udelay(1);
		_gpio_set_value(priv->gpio, priv->active_low);
		break;
	}
}

static void gpio_wdt_start_impl(struct gpio_wdt_priv *priv)
{
	priv->state = priv->active_low;
	_gpio_set_direction(priv, true);
	priv->last_jiffies = jiffies;
	gpio_wdt_hwping((unsigned long)&priv->wdd);
}

static int gpio_wdt_start(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	gpio_wdt_start_impl(priv);
	priv->armed = true;

	return 0;
}

static int gpio_wdt_stop(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->armed = false;
	if (!priv->always_running) {
		mod_timer(&priv->timer, 0);
		gpio_wdt_disable(priv);
	}

	return 0;
}

static int gpio_wdt_ping(struct watchdog_device *wdd)
{
	struct gpio_wdt_priv *priv = watchdog_get_drvdata(wdd);

	priv->last_jiffies = jiffies;

	return 0;
}

static int gpio_wdt_set_timeout(struct watchdog_device *wdd, unsigned int t)
{
	wdd->timeout = t;

	return gpio_wdt_ping(wdd);
}

static const struct watchdog_info gpio_wdt_ident = {
	.options	= WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING |
			  WDIOF_SETTIMEOUT,
	.identity	= "GPIO Watchdog",
};

static const struct watchdog_ops gpio_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= gpio_wdt_start,
	.stop		= gpio_wdt_stop,
	.ping		= gpio_wdt_ping,
	.set_timeout	= gpio_wdt_set_timeout,
};

static int gpio_wdt_probe(struct platform_device *pdev)
{
	struct gpio_wdt_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	ret = gpio_wdt_load_settings(priv, &pdev->dev);
	if (ret)
		return ret;

	watchdog_set_drvdata(&priv->wdd, priv);

	priv->wdd.info		= &gpio_wdt_ident;
	priv->wdd.ops		= &gpio_wdt_ops;
	priv->wdd.min_timeout	= SOFT_TIMEOUT_MIN;
	priv->wdd.max_timeout	= SOFT_TIMEOUT_MAX;
	priv->wdd.parent	= &pdev->dev;

	if (watchdog_init_timeout(&priv->wdd, 0, &pdev->dev) < 0)
		priv->wdd.timeout = SOFT_TIMEOUT_DEF;

	setup_timer(&priv->timer, gpio_wdt_hwping, (unsigned long)&priv->wdd);

	watchdog_stop_on_reboot(&priv->wdd);

	ret = watchdog_register_device(&priv->wdd);
	if (ret)
		return ret;

	if (priv->always_running)
		gpio_wdt_start_impl(priv);

	return 0;
}

static int gpio_wdt_remove(struct platform_device *pdev)
{
	struct gpio_wdt_priv *priv = platform_get_drvdata(pdev);

	del_timer_sync(&priv->timer);
	watchdog_unregister_device(&priv->wdd);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gpio_wdt_dt_ids[] = {
	{ .compatible = "linux,wdt-gpio", },
	{ }
};
MODULE_DEVICE_TABLE(of, gpio_wdt_dt_ids);
#endif

static struct platform_driver gpio_wdt_driver = {
	.driver	= {
		.name		= "gpio-wdt",
#ifdef CONFIG_OF
		.of_match_table	= gpio_wdt_dt_ids,
#endif
	},
	.probe	= gpio_wdt_probe,
	.remove	= gpio_wdt_remove,
};

#ifdef CONFIG_GPIO_WATCHDOG_ARCH_INITCALL
static int __init gpio_wdt_init(void)
{
	return platform_driver_register(&gpio_wdt_driver);
}
arch_initcall(gpio_wdt_init);
#else
module_platform_driver(gpio_wdt_driver);
#endif

MODULE_AUTHOR("Alexander Shiyan <shc_work@mail.ru>");
MODULE_DESCRIPTION("GPIO Watchdog");
MODULE_LICENSE("GPL");
