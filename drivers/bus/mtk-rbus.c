#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/mtk-rbus.h>

int rbus_driver_register(struct platform_driver *drv, struct module *owner)
{
	return __platform_driver_register(drv, owner);
}
EXPORT_SYMBOL(rbus_driver_register);

void rbus_driver_unregister(struct platform_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(rbus_driver_unregister);

struct platform_device *rbus_device_alloc(const char *name, int id)
{
	return platform_device_alloc(name, id);
}
EXPORT_SYMBOL(rbus_device_alloc);

void rbus_device_put(struct platform_device *pdev)
{
	return platform_device_put(pdev);
}
EXPORT_SYMBOL(rbus_device_put);

int rbus_get_irq(struct platform_device *pdev, unsigned int num)
{
	return platform_get_irq(pdev, num);
}
EXPORT_SYMBOL(rbus_get_irq);

int rbus_init_dummy_netdev(struct net_device *dev)
{
	return init_dummy_netdev(dev);
}
EXPORT_SYMBOL(rbus_init_dummy_netdev);

int rbus_clk_enable(struct clk *clk, bool enable)
{
	if (enable)
		return clk_prepare_enable(clk);

	clk_disable_unprepare(clk);

	return 0;
}
EXPORT_SYMBOL(rbus_clk_enable);

void rbus_pm_runtime_enable(struct device *dev, bool enable)
{
	if (enable) {
		pm_runtime_enable(dev);
		pm_runtime_get_sync(dev);
	} else {
		pm_runtime_put_sync(dev);
		pm_runtime_disable(dev);
	}
}
EXPORT_SYMBOL(rbus_pm_runtime_enable);

int rbus_init_wakeup(struct device *dev, bool enable)
{
	return device_init_wakeup(dev, enable);
}
EXPORT_SYMBOL(rbus_init_wakeup);

void rbus_set_dma_coherent(struct device *dev, bool coherent)
{
	arch_setup_dma_ops(dev, 0, 0, NULL, coherent);
}
EXPORT_SYMBOL(rbus_set_dma_coherent);

