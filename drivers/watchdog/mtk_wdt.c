/*
 * Mediatek Watchdog Driver
 *
 * Copyright (C) 2014 Matthias Brugger
 *
 * Matthias Brugger <matthias.bgg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on sunxi_wdt.c
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/delay.h>

#define WDT_MAX_TIMEOUT			31
#define WDT_MIN_TIMEOUT			1
#define WDT_LENGTH_TIMEOUT(n)		((n) << 5)

#define WDT_LENGTH			0x04
#define WDT_LENGTH_KEY			0x8

#define WDT_RST				0x08
#define WDT_RST_RELOAD			0x1971

#define WDT_MODE			0x00
#define WDT_MODE_EN			(1 << 0)
#define WDT_MODE_EXT_POL_LOW		(0 << 1)
#define WDT_MODE_EXT_POL_HIGH		(1 << 1)
#define WDT_MODE_EXRST_EN		(1 << 2)
#define WDT_MODE_IRQ_EN			(1 << 3)
#define WDT_MODE_AUTO_START		(1 << 4)
#define WDT_MODE_DUAL_EN		(1 << 6)
#define WDT_MODE_KEY			0x22000000

#define WDT_STATUS_HW_RST		(0 << 0)	/* Hardware reset				*/
#define WDT_STATUS_SPM_THERMAL_RST	(1 << 0)	/* Thermal reset (by SCPSYS)			*/
#define WDT_STATUS_SPM_WDT_RST		(1 << 1)	/* SCPSYS time-out generated reset		*/
#define WDT_STATUS_THERMAL_DIRECT_RST	(1 << 18)	/* Thermal reset (by thermal controller)	*/
#define WDT_STATUS_DEBUG_WDT_RST	(1 << 19)	/* Debug generated reset			*/
#define WDT_STATUS_SECURITY_RST		(1 << 28)	/* Security reset				*/
#define WDT_STATUS_IRQ_WDT_RST		(1 << 29)	/* IRQ is asserted instead of reset		*/
#define WDT_STATUS_SW_WDT_RST		(1 << 30)	/* Software watchdog generated reset		*/
#define WDT_STATUS_HW_WDT_RST		(1 << 31)	/* Hardware watchdog generated reset		*/

#define WDT_SWRST			0x14
#define WDT_SWRST_KEY			0x1209

#define DRV_NAME			"mtk-wdt"
#define DRV_VERSION			"1.0"

static bool nowayout = WATCHDOG_NOWAYOUT;
static unsigned int timeout;

struct mtk_wdt_dev {
	struct watchdog_device wdt_dev;
	void __iomem *wdt_base;
	void __iomem *sta_reg;
};

static void mtk_wdt_set_bootstatus(struct watchdog_device *wdt_dev)
{
	struct device *dev;
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	u32 reg;

	dev = wdt_dev->parent;
	reg = readl(mtk_wdt->sta_reg);

	switch (reg) {
	case WDT_STATUS_HW_RST:
		dev_info(dev, "last reset due to hard reset\n");
		break;
	case WDT_STATUS_SPM_THERMAL_RST:
	case WDT_STATUS_THERMAL_DIRECT_RST:
		wdt_dev->bootstatus |= WDIOF_OVERHEAT;
		dev_warn(dev, "last reset due to overheat (%s)\n",
			 (reg & WDT_STATUS_SPM_THERMAL_RST) ? "SPM" : "direct");
		break;
	case WDT_STATUS_HW_WDT_RST:
	case WDT_STATUS_SPM_WDT_RST:
		wdt_dev->bootstatus |= WDIOF_CARDRESET;
		dev_warn(dev, "last reset due to %stime out\n",
			 (reg & WDT_STATUS_HW_WDT_RST) ? "" : "SPM ");
		break;
	case WDT_STATUS_DEBUG_WDT_RST:
		dev_warn(dev, "last reset due to debug reset\n");
		break;
	case WDT_STATUS_SECURITY_RST:
		dev_warn(dev, "last reset due to security reset\n");
		break;
	case WDT_STATUS_IRQ_WDT_RST:
		dev_warn(dev, "last reset due to IRQ\n");
		break;
	case WDT_STATUS_SW_WDT_RST:
		dev_info(dev, "last reset due to soft reset\n");
		break;
	default:
		dev_warn(dev, "illegal status code (%08x)\n", reg);
		break;
	}
}

static int mtk_wdt_restart(struct watchdog_device *wdt_dev,
			   unsigned long action, void *data)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base;

	wdt_base = mtk_wdt->wdt_base;

	while (1) {
		writel(WDT_SWRST_KEY, wdt_base + WDT_SWRST);
		mdelay(5);
	}

	return 0;
}

static int mtk_wdt_ping(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;

	iowrite32(WDT_RST_RELOAD, wdt_base + WDT_RST);

	return 0;
}

static int mtk_wdt_set_timeout(struct watchdog_device *wdt_dev,
				unsigned int timeout)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	wdt_dev->timeout = timeout;

	/*
	 * One bit is the value of 512 ticks
	 * The clock has 32 KHz
	 */
	reg = WDT_LENGTH_TIMEOUT(timeout << 6) | WDT_LENGTH_KEY;
	iowrite32(reg, wdt_base + WDT_LENGTH);

	mtk_wdt_ping(wdt_dev);

	return 0;
}

static int mtk_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	u32 reg;

	reg = readl(wdt_base + WDT_MODE);
	reg &= ~WDT_MODE_EN;
	reg |= WDT_MODE_KEY;
	iowrite32(reg, wdt_base + WDT_MODE);

	return 0;
}

static int mtk_wdt_start(struct watchdog_device *wdt_dev)
{
	u32 reg;
	struct mtk_wdt_dev *mtk_wdt = watchdog_get_drvdata(wdt_dev);
	void __iomem *wdt_base = mtk_wdt->wdt_base;
	int ret;

	ret = mtk_wdt_set_timeout(wdt_dev, wdt_dev->timeout);
	if (ret < 0)
		return ret;

	reg = ioread32(wdt_base + WDT_MODE);
	reg &= ~(WDT_MODE_IRQ_EN | WDT_MODE_DUAL_EN);
	reg |= (WDT_MODE_EN | WDT_MODE_KEY);
	iowrite32(reg, wdt_base + WDT_MODE);

	return 0;
}

static const struct watchdog_info mtk_wdt_info = {
	.identity	= DRV_NAME,
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE |
			  WDIOF_CARDRESET |
			  WDIOF_OVERHEAT,
};

static const struct watchdog_ops mtk_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= mtk_wdt_start,
	.stop		= mtk_wdt_stop,
	.ping		= mtk_wdt_ping,
	.set_timeout	= mtk_wdt_set_timeout,
	.restart	= mtk_wdt_restart,
};

static int mtk_wdt_probe(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt;
	struct resource *res;
	int err;

	mtk_wdt = devm_kzalloc(&pdev->dev, sizeof(*mtk_wdt), GFP_KERNEL);
	if (!mtk_wdt)
		return -ENOMEM;

	platform_set_drvdata(pdev, mtk_wdt);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mtk_wdt->wdt_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mtk_wdt->wdt_base))
		return PTR_ERR(mtk_wdt->wdt_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	mtk_wdt->sta_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mtk_wdt->sta_reg))
		return PTR_ERR(mtk_wdt->sta_reg);

	mtk_wdt->wdt_dev.info = &mtk_wdt_info;
	mtk_wdt->wdt_dev.ops = &mtk_wdt_ops;
	mtk_wdt->wdt_dev.timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.max_timeout = WDT_MAX_TIMEOUT;
	mtk_wdt->wdt_dev.min_timeout = WDT_MIN_TIMEOUT;
	mtk_wdt->wdt_dev.parent = &pdev->dev;

	watchdog_init_timeout(&mtk_wdt->wdt_dev, timeout, &pdev->dev);
	watchdog_set_nowayout(&mtk_wdt->wdt_dev, nowayout);
	watchdog_set_restart_priority(&mtk_wdt->wdt_dev, 128);

	watchdog_set_drvdata(&mtk_wdt->wdt_dev, mtk_wdt);

	mtk_wdt_set_bootstatus(&mtk_wdt->wdt_dev);

	mtk_wdt_start(&mtk_wdt->wdt_dev);
	set_bit(WDOG_HW_RUNNING, &mtk_wdt->wdt_dev.status);

	err = watchdog_register_device(&mtk_wdt->wdt_dev);
	if (unlikely(err))
		return err;

	dev_info(&pdev->dev, "Watchdog enabled (timeout=%d sec, nowayout=%d)\n",
			mtk_wdt->wdt_dev.timeout, nowayout);

	return 0;
}

static void mtk_wdt_shutdown(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt = platform_get_drvdata(pdev);

	if (watchdog_active(&mtk_wdt->wdt_dev))
		mtk_wdt_stop(&mtk_wdt->wdt_dev);
}

static int mtk_wdt_remove(struct platform_device *pdev)
{
	struct mtk_wdt_dev *mtk_wdt = platform_get_drvdata(pdev);

	watchdog_unregister_device(&mtk_wdt->wdt_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_wdt_suspend(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev))
		mtk_wdt_stop(&mtk_wdt->wdt_dev);

	return 0;
}

static int mtk_wdt_resume(struct device *dev)
{
	struct mtk_wdt_dev *mtk_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&mtk_wdt->wdt_dev)) {
		mtk_wdt_start(&mtk_wdt->wdt_dev);
		mtk_wdt_ping(&mtk_wdt->wdt_dev);
	}

	return 0;
}
#endif

static const struct of_device_id mtk_wdt_dt_ids[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_wdt_dt_ids);

static const struct dev_pm_ops mtk_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_wdt_suspend,
				mtk_wdt_resume)
};

static struct platform_driver mtk_wdt_driver = {
	.probe		= mtk_wdt_probe,
	.remove		= mtk_wdt_remove,
	.shutdown	= mtk_wdt_shutdown,
	.driver		= {
		.name		= DRV_NAME,
		.pm		= &mtk_wdt_pm_ops,
		.of_match_table	= mtk_wdt_dt_ids,
	},
};

module_platform_driver(mtk_wdt_driver);

module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
			__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matthias Brugger <matthias.bgg@gmail.com>");
MODULE_DESCRIPTION("Mediatek WatchDog Timer Driver");
MODULE_VERSION(DRV_VERSION);
