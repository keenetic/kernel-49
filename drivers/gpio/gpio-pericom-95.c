// SPDX-License-Identifier: GPL-2.0
/*
 * Pericom PI4IOE5V9535/39/55 I2C GPIO EXPANDER DRIVER
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define PERICOM_GPIO_NAME		"pericom-95_gpio"
#define PERICOM_GPIO_PINS		(16)
#define PERICOM_GPIO_PORTS		(PERICOM_GPIO_PINS / 8)

#define PERICOM_MAX_INSTANCES		(4)

#define PERICOM_CMD_INP(p)		(0 + (p))
#define PERICOM_CMD_OUT(p)		(2 + (p))
#define PERICOM_CMD_POL(p)		(4 + (p))
#define PERICOM_CMD_CFG(p)		(6 + (p))

struct pericom_chip {
	struct i2c_client *client;
	struct mutex lock;
	struct gpio_chip gpio;
	u8 port_i[PERICOM_GPIO_PORTS];
	u8 port_o[PERICOM_GPIO_PORTS];
	u8 port_c[PERICOM_GPIO_PORTS];
	bool sync_input;
};

static struct pericom_chip *g_pericom_chip[PERICOM_MAX_INSTANCES];

static int _pericom_get(struct pericom_chip *chip, unsigned int offset)
{
	struct i2c_client *client = chip->client;
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	u8 port_val;

	if (chip->sync_input) {
		struct i2c_msg msgs[2];
		int ret;
		u8 cmd = PERICOM_CMD_INP(port_offset);

		msgs[0].addr = client->addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &cmd;

		msgs[1].addr = client->addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = 1;
		msgs[1].buf = &chip->port_i[port_offset];

		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0) {
			struct device *dev = &client->dev;

			dev_dbg(dev, "i2c read failed (%d) from addr %x\n",
				ret, client->addr);
		}
	}

	port_val = chip->port_i[port_offset];

	return (port_val >> data_offset) & 0x1;
}

static int _pericom_set(struct pericom_chip *chip, unsigned int offset, int value)
{
	struct i2c_client *client = chip->client;
	struct i2c_msg msgs[1];
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	u8 buf[2], port_old, port_new;
	int ret;

	port_old = chip->port_o[port_offset];
	port_new = port_old;

	if (value)
		port_new |=  BIT(data_offset);
	else
		port_new &= ~BIT(data_offset);

	if (port_new == port_old)
		return 0;

	buf[0] = PERICOM_CMD_OUT(port_offset);
	buf[1] = port_new;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buf);
	msgs[0].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0) {
		dev_dbg(&client->dev, "i2c write failed (%d) to addr %x\n",
			ret, client->addr);
		return ret;
	}

	if (!chip->sync_input)
		chip->port_i[port_offset] = port_new;

	chip->port_o[port_offset] = port_new;

	return 0;
}

static int _pericom_set_dir(struct pericom_chip *chip, unsigned int offset, bool out)
{
	struct i2c_client *client = chip->client;
	struct i2c_msg msgs[1];
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	u8 buf[2], port_old, port_new;
	int ret;

	port_old = chip->port_c[port_offset];
	port_new = port_old;

	if (out)
		port_new &= ~BIT(data_offset);
	else
		port_new |=  BIT(data_offset);

	if (port_new == port_old)
		return 0;

	buf[0] = PERICOM_CMD_CFG(port_offset);
	buf[1] = port_new;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buf);
	msgs[0].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0) {
		dev_dbg(&client->dev, "i2c write failed (%d) to addr %x\n",
			ret, client->addr);
		return ret;
	}

	chip->port_c[port_offset] = port_new;

	return 0;
}

static int pericom_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	u32 port_offset = offset / 8;
	u32 data_offset = offset % 8;
	u8 port_val;

	mutex_lock(&chip->lock);
	port_val = chip->port_c[port_offset];
	mutex_unlock(&chip->lock);

	if (port_val & (1u << data_offset))
		return GPIOF_DIR_IN;

	return GPIOF_DIR_OUT;
}

static int pericom_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	int ret;

	mutex_lock(&chip->lock);
	ret = _pericom_set_dir(chip, offset, false);
	mutex_unlock(&chip->lock);

	return ret;
}

static int pericom_direction_output(struct gpio_chip *gc, unsigned int offset,
				    int value)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	int ret;

	mutex_lock(&chip->lock);
	ret = _pericom_set_dir(chip, offset, true);
	if (ret == 0)
		ret = _pericom_set(chip, offset, value);
	mutex_unlock(&chip->lock);

	return ret;
}

static int pericom_get(struct gpio_chip *gc, unsigned int offset)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);
	int ret;

	mutex_lock(&chip->lock);
	ret = _pericom_get(chip, offset);
	mutex_unlock(&chip->lock);

	return ret;
}

static void pericom_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct pericom_chip *chip = gpiochip_get_data(gc);

	mutex_lock(&chip->lock);
	_pericom_set(chip, offset, value);
	mutex_unlock(&chip->lock);
}

/* export functions for phr */
struct mutex *phr_pericom_get_mutex(u8 index)
{
	struct pericom_chip *chip;

	if (index >= PERICOM_MAX_INSTANCES)
		return NULL;

	chip = g_pericom_chip[index];
	if (!chip)
		return NULL;

	return &chip->lock;
}
EXPORT_SYMBOL(phr_pericom_get_mutex);

int phr_pericom_get(u8 index, u8 offset, bool *on)
{
	struct pericom_chip *chip;
	int value;

	if (index >= PERICOM_MAX_INSTANCES)
		return -ENODEV;

	chip = g_pericom_chip[index];
	if (!chip)
		return -ENODEV;

	if (offset >= chip->gpio.ngpio)
		return -EINVAL;

	if (!on)
		return -EINVAL;

	value = _pericom_get(chip, offset);
	if (value < 0)
		return value;

	*on = !!value;

	return 0;
}
EXPORT_SYMBOL(phr_pericom_get);

int phr_pericom_set(u8 index, u8 offset, bool on)
{
	struct pericom_chip *chip;

	if (index >= PERICOM_MAX_INSTANCES)
		return -ENODEV;

	chip = g_pericom_chip[index];
	if (!chip)
		return -ENODEV;

	if (offset >= chip->gpio.ngpio)
		return -EINVAL;

	return _pericom_set(chip, offset, on);
}
EXPORT_SYMBOL(phr_pericom_set);

int phr_pericom_set_dir(u8 index, u8 offset, bool out)
{
	struct pericom_chip *chip;

	if (index >= PERICOM_MAX_INSTANCES)
		return -ENODEV;

	chip = g_pericom_chip[index];
	if (!chip)
		return -ENODEV;

	if (offset >= chip->gpio.ngpio)
		return -EINVAL;

	return _pericom_set_dir(chip, offset, out);
}
EXPORT_SYMBOL(phr_pericom_set_dir);

static const struct of_device_id pericom_gpio_of_ids[] = {
	{ .compatible = "pericom,pi4ioe5v9535" },
	{ .compatible = "pericom,pi4ioe5v9539" },
	{ .compatible = "pericom,pi4ioe5v9555" },
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
	int i, ret;

	if (!i2c_check_functionality(ad, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->lock);

	/* chip pins state default on init */
	memset(chip->port_i, 0xff, PERICOM_GPIO_PORTS);
	memset(chip->port_o, 0xff, PERICOM_GPIO_PORTS);
	memset(chip->port_c, 0xff, PERICOM_GPIO_PORTS);

	chip->client = client;
	chip->sync_input = of_property_read_bool(np, "gpio-sync-input");

	/* cache input pins state */
	if (!chip->sync_input) {
		struct i2c_msg msgs[2];
		u8 cmd = PERICOM_CMD_INP(0);

		msgs[0].addr = client->addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &cmd;

		msgs[1].addr = client->addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = PERICOM_GPIO_PORTS;
		msgs[1].buf = chip->port_i;

		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_warn(dev, "i2c read failed (%d) from addr %x\n",
				 ret, client->addr);
		else
			memcpy(chip->port_o, chip->port_i, PERICOM_GPIO_PORTS);
	}

	chip->gpio.label		 = client->name;
	chip->gpio.owner		 = THIS_MODULE;
	chip->gpio.parent		 = dev;
	chip->gpio.can_sleep		 = true;
	chip->gpio.base			 = -1;
	chip->gpio.ngpio		 = PERICOM_GPIO_PINS;
	chip->gpio.get_direction	 = pericom_get_direction,
	chip->gpio.direction_input	 = pericom_direction_input;
	chip->gpio.direction_output	 = pericom_direction_output;
	chip->gpio.get			 = pericom_get;
	chip->gpio.set			 = pericom_set;

	ret = devm_gpiochip_add_data(dev, &chip->gpio, chip);
	if (ret < 0)
		return ret;

	i2c_set_clientdata(client, chip);

	for (i = 0; i < PERICOM_MAX_INSTANCES; i++) {
		if (!g_pericom_chip[i]) {
			g_pericom_chip[i] = chip;
			break;
		}
	}

	dev_info(dev, "registered %s GPIO controller with %d pins\n",
		 client->name, chip->gpio.ngpio);

	return 0;
}

static const struct i2c_device_id pericom_gpio_id_table[] = {
	{ "pi4ioe5v9535" },
	{ "pi4ioe5v9539" },
	{ "pi4ioe5v9555" },
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
