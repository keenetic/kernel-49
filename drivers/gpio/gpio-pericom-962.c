// SPDX-License-Identifier: GPL-2.0
/*
 * Pericom PI4IOE5V96224/48 I2C GPIO EXPANDER DRIVER
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define PERICOM_GPIO_NAME		"pericom-962_gpio"
#define PERICOM_GPIO_PINS_24		(24)
#define PERICOM_GPIO_PINS_48		(48)
#define PERICOM_GPIO_PINS_MAX		 PERICOM_GPIO_PINS_48
#define PERICOM_GPIO_PORTS_MAX		(PERICOM_GPIO_PINS_MAX / 8)

struct pericom_gpio_config {
	u8 ngpio;
};

struct pericom_chip {
	struct i2c_client *client;
	struct mutex lock;
	struct gpio_chip gpio;
	u8 port[PERICOM_GPIO_PORTS_MAX];
	bool sync_input;
};

static struct gpio_chip *g_pericom_gc;

static int pericom_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	return GPIOF_DIR_OUT;
}

static int pericom_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	return 0;
}

static int pericom_direction_output(struct gpio_chip *gc, unsigned int offset,
				    int value)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	struct i2c_client *client = chip->client;
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	u8 port_old, port_new;
	int ret = 0;

	if (offset >= PERICOM_GPIO_PINS_MAX)
		return -EINVAL;

	mutex_lock(&chip->lock);

	port_old = chip->port[port_offset];
	port_new = port_old;

	if (value)
		port_new |=  BIT(data_offset);
	else
		port_new &= ~BIT(data_offset);

	if (port_new == port_old)
		goto exit_unlock;

	chip->port[port_offset] = port_new;

	ret = i2c_master_send(client, chip->port, port_offset + 1);
	if (ret < 0) {
		chip->port[port_offset] = port_old;
		goto exit_unlock;
	}

	ret = 0;

exit_unlock:
	mutex_unlock(&chip->lock);

	if (ret < 0)
		dev_err(&client->dev, "i2c write failed (%d) to addr %x\n",
			ret, client->addr);

	return ret;
}

static int pericom_get(struct gpio_chip *gc, unsigned int offset)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	struct i2c_client *client = chip->client;
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	int ret = 0;
	u8 port_val;

	if (offset >= PERICOM_GPIO_PINS_MAX)
		return -EINVAL;

	mutex_lock(&chip->lock);

	if (chip->sync_input)
		ret = i2c_master_recv(client, chip->port, port_offset + 1);

	port_val = chip->port[port_offset];

	mutex_unlock(&chip->lock);

	if (ret < 0)
		dev_warn(&client->dev, "i2c read failed (%d) from addr %x\n",
			 ret, client->addr);

	return (port_val >> data_offset) & 0x1;
}

static void pericom_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	pericom_direction_output(gc, offset, value);
}

int pericom_phr_get(unsigned int offset)
{
	struct gpio_chip *gc = g_pericom_gc;

	if (!gc)
		return -ENODEV;

	return pericom_get(gc, offset);
}
EXPORT_SYMBOL(pericom_phr_get);

int pericom_phr_set(unsigned int offset, int value)
{
	struct gpio_chip *gc = g_pericom_gc;

	if (!gc)
		return -ENODEV;

	return pericom_direction_output(gc, offset, value);
}
EXPORT_SYMBOL(pericom_phr_set);

static const struct pericom_gpio_config pins24_config = {
	.ngpio = PERICOM_GPIO_PINS_24,
};

static const struct pericom_gpio_config pins48_config = {
	.ngpio = PERICOM_GPIO_PINS_48,
};

static const struct of_device_id pericom_gpio_of_ids[] = {
	{ .compatible = "pericom,pi4ioe5v96224", .data = &pins24_config },
	{ .compatible = "pericom,pi4ioe5v96248", .data = &pins48_config },
	{ }
};
MODULE_DEVICE_TABLE(of, pericom_gpio_of_ids);

static int pericom_gpio_probe(struct i2c_client *client,
			      const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
	struct i2c_adapter *ad = to_i2c_adapter(dev->parent);
	struct device_node *np = dev->of_node;
	struct pericom_chip *chip;
	const struct of_device_id *match;
	int ret;
	u16 ngpio;

	match = of_match_device(pericom_gpio_of_ids, dev);
	if (match) {
		const struct pericom_gpio_config *config = match->data;

		ngpio = config->ngpio;
	} else if (i2c_id)
		ngpio = i2c_id->driver_data & 0xff;
	else
		return -ENODEV;

	if (ngpio > PERICOM_GPIO_PINS_MAX)
		return -EINVAL;

	if (!i2c_check_functionality(ad, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->lock);

	/* chip pins state HIGH on init */
	memset(&chip->port[0], 0xff, PERICOM_GPIO_PORTS_MAX);

	/* cache all pins state */
	ret = i2c_master_recv(client, chip->port, ngpio / 8);
	if (ret < 0)
		dev_warn(dev, "i2c read failed (%d) from addr %x\n",
			 ret, client->addr);

	chip->client = client;
	chip->sync_input = of_property_read_bool(np, "gpio-sync-input");

	chip->gpio.label		 = client->name;
	chip->gpio.owner		 = THIS_MODULE;
	chip->gpio.parent		 = dev;
	chip->gpio.can_sleep		 = true;
	chip->gpio.base			 = -1;
	chip->gpio.ngpio		 = ngpio;
	chip->gpio.get_direction	 = pericom_get_direction,
	chip->gpio.direction_input	 = pericom_direction_input;
	chip->gpio.direction_output	 = pericom_direction_output;
	chip->gpio.get			 = pericom_get;
	chip->gpio.set			 = pericom_set;

	ret = devm_gpiochip_add_data(dev, &chip->gpio, chip);
	if (ret < 0)
		return ret;

	g_pericom_gc = &chip->gpio;

	i2c_set_clientdata(client, chip);

	dev_info(dev, "registered %s GPIO controller with %d pins\n",
		 client->name, chip->gpio.ngpio);

	return 0;
}

static const struct i2c_device_id pericom_gpio_id_table[] = {
	{ "pi4ioe5v96224", PERICOM_GPIO_PINS_24 },
	{ "pi4ioe5v96248", PERICOM_GPIO_PINS_48 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pericom_gpio_id_table);

static struct i2c_driver pericom_gpio_driver = {
	.driver = {
		.name = PERICOM_GPIO_NAME,
		.of_match_table = pericom_gpio_of_ids,
	},
	.probe = pericom_gpio_probe,
	.id_table = pericom_gpio_id_table,
};

module_i2c_driver(pericom_gpio_driver);

MODULE_LICENSE("GPL v2");
