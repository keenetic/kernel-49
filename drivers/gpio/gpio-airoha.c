// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2021-2023 Airoha Inc.
/*
 * GPIO implementation based on Airoha SDK
 *
 * Author: Aman Srivastava <aman.srivastava@airoha.com>
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>

#define AIROHA_GPIO_MAX		32

/**
 * airoha_gpio_ctrl - Airoha GPIO driver data
 * @gc: Associated gpio_chip instance.
 * @data: The data register.
 * @dir0: The direction register for the lower 16 pins.
 * @dir1: The direction register for the higher 16 pins.
 * @output: The output enable register.
 */
struct airoha_gpio_ctrl {
	struct gpio_chip gc;
	void __iomem *data;
	void __iomem *dir[2];
	void __iomem *output;
};

static DEFINE_SPINLOCK(airoha_gpio_lock);

spinlock_t *airoha_pinctrl_lock = &airoha_gpio_lock;
EXPORT_SYMBOL(airoha_pinctrl_lock);

static struct airoha_gpio_ctrl *gc_to_ctrl(struct gpio_chip *gc)
{
	return container_of(gc, struct airoha_gpio_ctrl, gc);
}

static int airoha_dir_set(struct gpio_chip *gc, unsigned int gpio,
			  int val, int out)
{
	struct airoha_gpio_ctrl *ctrl = gc_to_ctrl(gc);
	u32 dir = ioread32(ctrl->dir[gpio >> 4]);
	u32 output = ioread32(ctrl->output);
	u32 dir_shift = (gpio & 0xf) << 1;
	u32 dir_mask = 0x3 << dir_shift;

	dir &= ~dir_mask;

	if (out) {
		dir |= BIT(dir_shift);
		output |= BIT(gpio);
	} else {
		output &= ~BIT(gpio);
	}

	iowrite32(dir, ctrl->dir[gpio >> 4]);

	if (out)
		gc->set(gc, gpio, val);

	iowrite32(output, ctrl->output);

	return 0;
}

static int airoha_dir_out(struct gpio_chip *gc, unsigned int gpio,
			  int val)
{
	return airoha_dir_set(gc, gpio, val, 1);
}

static int airoha_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return airoha_dir_set(gc, gpio, 0, 0);
}

static int airoha_get_dir(struct gpio_chip *gc, unsigned int gpio)
{
	struct airoha_gpio_ctrl *ctrl = gc_to_ctrl(gc);
	u32 dir = ioread32(ctrl->dir[gpio >> 4]);
	u32 mask = BIT((gpio & 0xf) << 1);

	return (dir & mask) ? 0 : 1;
}

static int airoha_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct airoha_gpio_ctrl *ctrl;
	struct resource *res;
	u32 gpio_base = 0;
	int err;

	if (device_property_read_u32(dev, "gpio_base", &gpio_base))
		return -EINVAL;

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctrl->data = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl->data))
		return PTR_ERR(ctrl->data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ctrl->dir[0] = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl->dir[0]))
		return PTR_ERR(ctrl->dir[0]);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	ctrl->dir[1] = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl->dir[1]))
		return PTR_ERR(ctrl->dir[1]);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	ctrl->output = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctrl->output))
		return PTR_ERR(ctrl->output);

	err = bgpio_init(&ctrl->gc, dev, 4, ctrl->data, NULL,
			 NULL, NULL, NULL, 0);
	if (err) {
		dev_err(dev, "unable to init generic GPIO");
		return err;
	}

	/* override bgpio spinlock */
	ctrl->gc.bgpio_lock = &airoha_gpio_lock;

	ctrl->gc.base = gpio_base;
	ctrl->gc.ngpio = AIROHA_GPIO_MAX;
	ctrl->gc.owner = THIS_MODULE;
	ctrl->gc.direction_output = airoha_dir_out;
	ctrl->gc.direction_input = airoha_dir_in;
	ctrl->gc.get_direction = airoha_get_dir;

	return devm_gpiochip_add_data(dev, &ctrl->gc, ctrl);
}

static const struct of_device_id airoha_gpio_of_match[] = {
	{ .compatible = "airoha,airoha-gpio" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, airoha_gpio_of_match);

static struct platform_driver airoha_gpio_driver = {
	.probe = airoha_gpio_probe,
	.driver = {
		.name = "airoha-gpio",
		.of_match_table = airoha_gpio_of_match,
	},
};
module_platform_driver(airoha_gpio_driver);

MODULE_DESCRIPTION("Airoha GPIO support");
MODULE_AUTHOR("John Crispin <john@phrozen.org>");
MODULE_LICENSE("GPL v2");
