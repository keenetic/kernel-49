#define DRV_NAME				  "mt_wdt"
#define pr_fmt(fmt)				  DRV_NAME ": " fmt

#include <linux/io.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#define MT_WDT_1SEC				  (1000)

#define MT_WDT_TIMEOUT_SEC_PROP			  "timeout-sec"
#define MT_WDT_TIMEOUT_DIV			  (5)

#define MT_WDT_TIMER_FREQ			  (32000) /* 32 KHz */
#define MT_WDT_TIMER_LENGTH_UNIT		  (512)   /* length unit */
#define MT_WDT_TIMER_TICKS_PER_SECOND		  \
	(MT_WDT_TIMER_FREQ / MT_WDT_TIMER_LENGTH_UNIT)
#define MT_WDT_TIMER_MSECS_TO_TICKS(msecs)	  \
	(((msecs) * MT_WDT_TIMER_TICKS_PER_SECOND) / MT_WDT_1SEC)
#define MT_WDT_TIMER_TICKS_TO_MSECS(ticks)	  \
	(((ticks) * MT_WDT_1SEC) / MT_WDT_TIMER_TICKS_PER_SECOND)

#define MT_WDT_LENGTH_SECONDS_MIN		  (MT_WDT_TIMEOUT_DIV)
#define MT_WDT_LENGTH_SECONDS_MAX		  \
	(((1 << 11) - 1) / MT_WDT_TIMER_TICKS_PER_SECOND)

#define MT_WDT_MODE				  (0x00)
#define MT_WDT_MODE_KEY				  (0x22 << 24)
#define MT_WDT_MODE_WDT_RESET			  (1 << 8)
#define MT_WDT_MODE_DDR_RESERVE			  (1 << 7)
#define MT_WDT_MODE_DUAL_EN			  (1 << 6)
#define MT_WDT_MODE_IRQ_EN			  (1 << 3)
#define MT_WDT_MODE_EXT_RST_EN			  (1 << 2)
#define MT_WDT_MODE_WDT_EN			  (1 << 0)

#define MT_WDT_LENGTH				  (0x04)
#define MT_WDT_LENGTH_VALUE(msecs)		  \
	(MT_WDT_TIMER_MSECS_TO_TICKS(msecs) << 5)
#define MT_WDT_LENGTH_UNLOCK_KEY		  (0x08)

#define MT_WDT_RESTART				  (0x08)
#define MT_WDT_RESTART_KEY			  (0x1971)

#define MT_WDT_STATUS				  (0x0c)
#define MT_WDT_STATUS_HW_WDT_RST		  (1 << 31)
#define MT_WDT_STATUS_SW_WDT_RST		  (1 << 30)
#define MT_WDT_STATUS_IRQ_WDT_RST		  (1 << 29)
#define MT_WDT_STATUS_SECURITY_RST		  (1 << 28)
#define MT_WDT_STATUS_DEBUG_WDT_RST		  (1 << 19)
#define MT_WDT_STATUS_THERMAL_DIRECT_RST	  (1 << 18)
#define MT_WDT_STATUS_SPM_WDT_RST		  (1 <<  1)
#define MT_WDT_STATUS_SPM_THERMAL_RST		  (1 <<  0)
#define MT_WDT_STATUS_HW_RST			  (0 <<  0)

/* preloader preserved status register */
#define MT_WDT_GPT4_COMPARE			  (0x4c)

#define mt_wdt_info(wdt, fmt, arg...) dev_info(wdt->dev, fmt, ##arg)
#define mt_wdt_warn(wdt, fmt, arg...) dev_warn(wdt->dev, fmt, ##arg)
#define mt_wdt_err(wdt, fmt, arg...) dev_err(wdt->dev, fmt, ##arg)

struct mt_wdt {
	spinlock_t lock;
	cpumask_t alive_cpus;
	bool stopping;
	u32 refresh_interval;
	u8 __iomem *base;
	struct timer_list sw_timer[NR_CPUS];
	struct device *dev;
};

static inline void
mt_wdt_hw_configure(const struct mt_wdt *wdt, const u32 msecs)
{
	writel(MT_WDT_LENGTH_VALUE(msecs) |
	       MT_WDT_LENGTH_UNLOCK_KEY,
	       wdt->base + MT_WDT_LENGTH);
}

static inline void
mt_wdt_hw_restart(const struct mt_wdt *wdt)
{
	writel(MT_WDT_RESTART_KEY, wdt->base + MT_WDT_RESTART);
}

static inline void
mt_wdt_hw_enable(const struct mt_wdt *wdt)
{
	void __iomem *reg = wdt->base + MT_WDT_MODE;
	u32 mode = readl(reg);

	mode &= ~(MT_WDT_MODE_WDT_RESET |
		  MT_WDT_MODE_DUAL_EN |
		  MT_WDT_MODE_EXT_RST_EN);
	mode |=   MT_WDT_MODE_WDT_EN |
		  MT_WDT_MODE_KEY;
	writel(mode, reg);
}

static inline void
mt_wdt_hw_disable(const struct mt_wdt *wdt)
{
	void __iomem *reg = wdt->base + MT_WDT_MODE;
	u32 mode = readl(reg);

	mode &= ~MT_WDT_MODE_WDT_EN;
	mode |=  MT_WDT_MODE_KEY;
	writel(mode, reg);
}

static inline void
mt_wdt_sw_timer_restart(struct timer_list *timer, const u32 refresh_interval)
{
	mod_timer(timer, jiffies + HZ * refresh_interval);
}

static void mt_wdt_sw_timer(unsigned long data)
{
	struct mt_wdt *wdt = (struct mt_wdt *)data;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	if (!wdt->stopping) {
		const int this_cpu = raw_smp_processor_id();
		cpumask_t *alive_cpus = &wdt->alive_cpus;

		cpumask_set_cpu(this_cpu, alive_cpus);

		if (cpumask_full(alive_cpus)) {
			cpumask_clear(alive_cpus);
			mt_wdt_hw_restart(wdt);
		}

		mt_wdt_sw_timer_restart(&wdt->sw_timer[this_cpu],
					wdt->refresh_interval);
	}

	spin_unlock_irqrestore(&wdt->lock, flags);
}

static inline void
mt_wdt_timers_start(void *data)
{
	struct mt_wdt *wdt = data;

	spin_lock(&wdt->lock);
	mt_wdt_sw_timer_restart(&wdt->sw_timer[raw_smp_processor_id()],
				wdt->refresh_interval);
	spin_unlock(&wdt->lock);
}

static inline void
mt_wdt_timers_stop(struct mt_wdt *wdt)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	mt_wdt_hw_restart(wdt);
	wdt->stopping = true;

	spin_unlock_irqrestore(&wdt->lock, flags);

	for_each_possible_cpu(i) {
		del_timer_sync(&wdt->sw_timer[i]);
	}

	mt_wdt_hw_disable(wdt);
}

static inline const char *mt_wdt_status_text(const u32 status)
{
	switch (status) {
	case MT_WDT_STATUS_HW_RST:
		return "power-on reset";
	case MT_WDT_STATUS_SPM_THERMAL_RST:
		return "system power manager thermal reset";
	case MT_WDT_STATUS_SPM_WDT_RST:
		return "system power manager timeout";
	case MT_WDT_STATUS_THERMAL_DIRECT_RST:
		return "thermal controller reset";
	case MT_WDT_STATUS_DEBUG_WDT_RST:
		return "debug reset";
	case MT_WDT_STATUS_SECURITY_RST:
		return "security reset";
	case MT_WDT_STATUS_IRQ_WDT_RST:
		return "interrupt reset";
	case MT_WDT_STATUS_SW_WDT_RST:
		return "software reset";
	case MT_WDT_STATUS_HW_WDT_RST:
		return "hardware watchdog reset";
	}
	return NULL;
}

static inline struct device_node *
mt_wdt_of_node_get_gpt(const struct mt_wdt *wdt)
{
	struct device_node *np = wdt->dev->of_node;

	if (np == NULL)
		return NULL;

	return of_parse_phandle(np, "mediatek,timer", 0);
}

static int mt_wdt_show_power_status(const struct mt_wdt *wdt)
{
	u32 status;
	const char *status_text;
	void __iomem *gpt_base;
	struct device_node *np = mt_wdt_of_node_get_gpt(wdt);

	if (np == NULL)
		return -ENODEV;

	gpt_base = of_iomap(np, 0);
	of_node_put(np);

	if (gpt_base == NULL)
		return -ENOMEM;

	status = readl(gpt_base + MT_WDT_GPT4_COMPARE);
	iounmap(gpt_base);

	status_text = mt_wdt_status_text(status);
	if (status_text == NULL) {
		mt_wdt_err(wdt, "SoC power status: unknown (%08lx)\n",
			   (unsigned long)status);
	} else if (status == MT_WDT_STATUS_HW_RST ||
		   status == MT_WDT_STATUS_SW_WDT_RST) {
		mt_wdt_info(wdt, "SoC power status: %s\n", status_text);
	} else {
		mt_wdt_warn(wdt, "SoC power status: %s\n", status_text);
	}

	return 0;
}

static int mt_wdt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mt_wdt *wdt;
	struct resource *res;
	unsigned long flags;
	unsigned int i, timeout;
	int ret;

	if (np == NULL) {
		dev_err(&pdev->dev, "no device tree description found\n");
		return -ENODEV;
	}

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (wdt == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, wdt);

	spin_lock_init(&wdt->lock);
	cpumask_clear(&wdt->alive_cpus);
	wdt->dev = &pdev->dev;
	wdt->stopping = false;

	for_each_possible_cpu(i) {
		struct timer_list *timer = &wdt->sw_timer[i];

		setup_timer(timer, mt_wdt_sw_timer, (unsigned long)wdt);
		timer->flags = TIMER_PINNED;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		mt_wdt_err(wdt, "unable to find watchdog resources\n");
		return -ENODEV;
	}

	wdt->base = devm_ioremap_resource(wdt->dev, res);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	ret = mt_wdt_show_power_status(wdt);
	if (ret != 0)
		return ret;

	ret = of_property_read_u32(np, MT_WDT_TIMEOUT_SEC_PROP, &timeout);
	if (ret != 0) {
		if (ret != -EINVAL) {
			mt_wdt_err(wdt, "wrong "
				   "\"" MT_WDT_TIMEOUT_SEC_PROP "\" value\n");
			return ret;
		}

		/* no value defined */
		timeout = MT_WDT_LENGTH_SECONDS_MAX;
	} else if (timeout < MT_WDT_LENGTH_SECONDS_MIN ||
		   timeout > MT_WDT_LENGTH_SECONDS_MAX) {
		mt_wdt_err(wdt, "\"" MT_WDT_TIMEOUT_SEC_PROP "\" value %u "
			   "is out of range [%i...%i]\n", timeout,
			   MT_WDT_LENGTH_SECONDS_MIN,
			   MT_WDT_LENGTH_SECONDS_MAX);
		return -EINVAL;
	}

	wdt->refresh_interval = timeout / MT_WDT_TIMEOUT_DIV;

	spin_lock_irqsave(&wdt->lock, flags);

	mt_wdt_hw_configure(wdt, timeout * MT_WDT_1SEC);
	mt_wdt_hw_enable(wdt);
	mt_wdt_hw_restart(wdt);

	spin_unlock_irqrestore(&wdt->lock, flags);

	ret = on_each_cpu(mt_wdt_timers_start, wdt, 1);
	if (ret != 0) {
		mt_wdt_timers_stop(wdt);
		mt_wdt_err(wdt, "unable to start timers: %i\n", ret);
		return ret;
	}

	mt_wdt_info(wdt, "activated with %u second delay\n", timeout);
	return 0;
}

static int mt_wdt_remove(struct platform_device *pdev)
{
	struct mt_wdt *wdt = platform_get_drvdata(pdev);

	mt_wdt_timers_stop(wdt);
	mt_wdt_info(wdt, "deactivated\n");

	return 0;
}

static const struct of_device_id MT_WDT_DT_IDS[] = {
	{ .compatible = "mediatek,mt7622-wdt" },
	{ /* sentinel */		      }
};
MODULE_DEVICE_TABLE(of, MT_WDT_DT_IDS);

static struct platform_driver mt_wdt_driver = {
	.probe		= mt_wdt_probe,
	.remove		= mt_wdt_remove,
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= MT_WDT_DT_IDS,
	},
};
module_platform_driver(mt_wdt_driver);

MODULE_DESCRIPTION("MediaTek MT7622 watchdog");
MODULE_AUTHOR("Sergey Korolev <s.korolev@ndmsystems.com>");
MODULE_LICENSE("GPL");
