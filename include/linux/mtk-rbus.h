#ifndef _MTK_RBUS_H_
#define _MTK_RBUS_H_

#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/clk.h>

int rbus_driver_register(struct platform_driver *drv, struct module *owner);
void rbus_driver_unregister(struct platform_driver *drv);

struct platform_device *rbus_device_alloc(const char *name, int id);
void rbus_device_put(struct platform_device *pdev);

int rbus_get_irq(struct platform_device *pdev, unsigned int num);
int rbus_get_irq_byname(struct platform_device *pdev, const char *name);

int rbus_init_dummy_netdev(struct net_device *dev);

int rbus_init_wakeup(struct device *dev, bool enable);
int rbus_clk_enable(struct clk *clk, bool enable);
void rbus_pm_runtime_enable(struct device *dev, bool enable);

void rbus_set_dma_coherent(struct device *dev, bool coherent);

int rbus_pinctrl_get_select(struct device *dev, const char *name);

void *rbus_nvmem_read(struct device_node *np, const char *name, size_t *len);

#endif /* _MTK_RBUS_H_ */
