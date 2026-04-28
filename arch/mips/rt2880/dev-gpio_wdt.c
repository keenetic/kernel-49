#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>

static int __init gpio_wdt_devinit(void)
{
	return IS_ERR(platform_device_register_simple("gpio-wdt", -1, NULL, 0));
}

device_initcall(gpio_wdt_devinit);
