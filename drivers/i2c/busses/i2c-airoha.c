// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Airoha company
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/reset.h>
#include <linux/platform_device.h>

#define REG_SIFMCTL0_REG		0x40
#define REG_SIFMCTL1_REG		0x44
#define REG_SIFMD0_REG			0x50
#define REG_SIFMD1_REG			0x54
#define REG_PINTEN_REG			0x5c
#define REG_PINTST_REG			0x60
#define REG_PINTCL_REG			0x64

/* REG_SM0CFG2_REG */
#define SM0CFG2_IS_AUTOMODE		BIT(0)

/* REG_SIFMCTL0_REG */
#define SM0CTL0_ODRAIN			BIT(31)
#define SM0CTL0_CLK_DIV_MASK		(0xfff << 16)
#define SM0CTL0_CLK_DIV_MAX		0xfff
#define SM0CTL0_CS_STATUS		BIT(4)
#define SM0CTL0_SCL_STATE		BIT(3)
#define SM0CTL0_SDA_STATE		BIT(2)
#define SM0CTL0_EN			BIT(1)
#define SM0CTL0_SCL_STRETCH		BIT(0)

/* REG_SIFMCTL1_REG */
#define SM0CTL1_ACK_MASK		(0xff << 16)
#define SM0CTL1_PGLEN_MASK		(0x7 << 8)
#define SM0CTL1_PGLEN(x)		((((x) - 1) << 8) & SM0CTL1_PGLEN_MASK)
#define SM0CTL1_READ			(5 << 4)
#define SM0CTL1_READ_LAST		(4 << 4)
#define SM0CTL1_STOP			(3 << 4)
#define SM0CTL1_WRITE			(2 << 4)
#define SM0CTL1_START			(1 << 4)
#define SM0CTL1_MODE_MASK		(0x7 << 4)
#define SM0CTL1_TRI			BIT(0)

/* timeout waiting for I2C devices to respond */
#define TIMEOUT_MS			1000
#define SYS_CLOCK			20000000
#define I2C_MAX_STANDARD_MODE_FREQ	100000

static const char i2c_driver_version[] = "v1.01";

struct airoha_i2c {
	void __iomem *base;
	struct device *dev;
	struct i2c_adapter adap;
	u32 clk_div;
	u32 flags;
};

static int airoha_i2c_wait_idle(struct airoha_i2c *i2c)
{
	int ret;
	u32 val;

	ret = readl_relaxed_poll_timeout_atomic(i2c->base + REG_SIFMCTL1_REG,
			 val, !(val & SM0CTL1_TRI), 10, TIMEOUT_MS * 1000);

	if (ret)
		dev_dbg(i2c->dev, "idle err(%d)\n", ret);

	return ret;
}

static void airoha_i2c_reset(struct airoha_i2c *i2c)
{
	/*
	 * Don't set SM0CTL0_ODRAIN as its bit meaning is inverted. To
	 * configure open-drain mode, this bit needs to be cleared.
	 */
	iowrite32(((i2c->clk_div << 16) & SM0CTL0_CLK_DIV_MASK) | SM0CTL0_EN |
		  SM0CTL0_SCL_STRETCH, i2c->base + REG_SIFMCTL0_REG);
}

static void airoha_i2c_dump_reg(struct airoha_i2c *i2c)
{
	dev_dbg(i2c->dev,
		"SM0CTL0 %08x, SM0CTL1 %08x, SM0D0 %08x, SM0D1 %08x\n",
		ioread32(i2c->base + REG_SIFMCTL0_REG),
		ioread32(i2c->base + REG_SIFMCTL1_REG),
		ioread32(i2c->base + REG_SIFMD0_REG),
		ioread32(i2c->base + REG_SIFMD1_REG));
}

static int airoha_i2c_check_ack(struct airoha_i2c *i2c, u32 expected)
{
	u32 ack = readl_relaxed(i2c->base + REG_SIFMCTL1_REG);
	u32 ack_expected = (expected << 16) & SM0CTL1_ACK_MASK;

	return ((ack & ack_expected) == ack_expected) ? 0 : -ENXIO;
}

static int airoha_i2c_master_start(struct airoha_i2c *i2c)
{
	iowrite32(SM0CTL1_START | SM0CTL1_TRI, i2c->base + REG_SIFMCTL1_REG);
	return airoha_i2c_wait_idle(i2c);
}

static int airoha_i2c_master_stop(struct airoha_i2c *i2c)
{
	iowrite32(SM0CTL1_STOP | SM0CTL1_TRI, i2c->base + REG_SIFMCTL1_REG);
	return airoha_i2c_wait_idle(i2c);
}

static int airoha_i2c_master_cmd(struct airoha_i2c *i2c, u32 cmd, int page_len)
{
	iowrite32(cmd | SM0CTL1_TRI | SM0CTL1_PGLEN(page_len),
		  i2c->base + REG_SIFMCTL1_REG);
	return airoha_i2c_wait_idle(i2c);
}

static int airoha_i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
				  int num)
{
	struct airoha_i2c *i2c = i2c_get_adapdata(adap);
	int i, j, ret, len, page_len;
	u32 cmd;
	u32 data[2];
	u32 read_data;
	u16 addr;

	/* set clock div for i2c clock frequency */
	if (i2c->clk_div > SM0CTL0_CLK_DIV_MAX) {
		dev_dbg(i2c->dev, "clock-frequency div > 4095, set to max value\n");
		i2c->clk_div = SM0CTL0_CLK_DIV_MAX;
	}

	read_data = ioread32(i2c->base + REG_SIFMCTL0_REG);
	read_data &= ~SM0CTL0_CLK_DIV_MASK;
	read_data |= ((i2c->clk_div << 16) & SM0CTL0_CLK_DIV_MASK);
	iowrite32(read_data, i2c->base + REG_SIFMCTL0_REG);

	for (i = 0; i < num; i++) {
		struct i2c_msg *pmsg = &msgs[i];

		/* wait hardware idle */
		ret = airoha_i2c_wait_idle(i2c);
		if (ret)
			goto err_timeout;

		/* start sequence */
		ret = airoha_i2c_master_start(i2c);
		if (ret)
			goto err_timeout;

		/* write address */
		if (pmsg->flags & I2C_M_TEN) {
			/* 10 bits address */
			addr = 0xf0 | ((pmsg->addr >> 7) & 0x06);
			addr |= (pmsg->addr & 0xff) << 8;
			if (pmsg->flags & I2C_M_RD)
				addr |= 1;
			iowrite32(addr, i2c->base + REG_SIFMD0_REG);
			ret = airoha_i2c_master_cmd(i2c, SM0CTL1_WRITE, 2);
			if (ret)
				goto err_timeout;
		} else {
			/* 7 bits address */
			addr = i2c_8bit_addr_from_msg(pmsg);
			iowrite32(addr, i2c->base + REG_SIFMD0_REG);
			ret = airoha_i2c_master_cmd(i2c, SM0CTL1_WRITE, 1);
			if (ret)
				goto err_timeout;
		}

		/* check address ACK */
		if (!(pmsg->flags & I2C_M_IGNORE_NAK)) {
			ret = airoha_i2c_check_ack(i2c, BIT(0));
			if (ret)
				goto err_ack;
		}

		/* transfer data */
		for (len = pmsg->len, j = 0; len > 0; len -= 8, j += 8) {
			page_len = (len >= 8) ? 8 : len;

			if (pmsg->flags & I2C_M_RD) {
				cmd = (len > 8) ?
					SM0CTL1_READ : SM0CTL1_READ_LAST;
			} else {
				memcpy(data, &pmsg->buf[j], page_len);
				iowrite32(data[0], i2c->base + REG_SIFMD0_REG);
				iowrite32(data[1], i2c->base + REG_SIFMD1_REG);
				cmd = SM0CTL1_WRITE;
			}

			ret = airoha_i2c_master_cmd(i2c, cmd, page_len);
			if (ret)
				goto err_timeout;

			if (pmsg->flags & I2C_M_RD) {
				data[0] = ioread32(i2c->base + REG_SIFMD0_REG);
				data[1] = ioread32(i2c->base + REG_SIFMD1_REG);
				memcpy(&pmsg->buf[j], data, page_len);
			} else {
				if (!(pmsg->flags & I2C_M_IGNORE_NAK)) {
					ret = airoha_i2c_check_ack(i2c,
								(1 << page_len)
								- 1);
					if (ret)
						goto err_ack;
				}
			}
		}
	}

	ret = airoha_i2c_master_stop(i2c);
	if (ret)
		goto err_timeout;

	/* the return value is number of executed messages */
	return i;

err_ack:
	ret = airoha_i2c_master_stop(i2c);
	if (ret)
		goto err_timeout;

	return -ENXIO;

err_timeout:
	airoha_i2c_dump_reg(i2c);
	airoha_i2c_reset(i2c);
	return ret;
}

static u32 airoha_i2c_func(struct i2c_adapter *a)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}

static const struct i2c_algorithm airoha_i2c_algo = {
	.master_xfer	= airoha_i2c_master_xfer,
	.functionality	= airoha_i2c_func,
};

static void airoha_i2c_init(struct airoha_i2c *i2c, u32 bus_freq)
{
	i2c->clk_div = SYS_CLOCK / bus_freq - 1;

	if (i2c->clk_div > SM0CTL0_CLK_DIV_MAX) {
		dev_dbg(i2c->dev, "clock-frequency div > 4095, set to max value\n");
		i2c->clk_div = SM0CTL0_CLK_DIV_MAX;
	}

	airoha_i2c_reset(i2c);
}

static int airoha_i2c_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	struct airoha_i2c *i2c;
	struct i2c_adapter *adap;
	u32 bus_freq = 0;
	int ret;

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->base))
		return PTR_ERR(i2c->base);

	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency",
				 &bus_freq) || bus_freq == 0) {
		dev_warn(i2c->dev, "set standard frequency 100kHz\n");
		bus_freq = I2C_MAX_STANDARD_MODE_FREQ;
	}

	i2c->dev = &pdev->dev;

	adap = &i2c->adap;
	adap->owner = THIS_MODULE;
	adap->algo = &airoha_i2c_algo;
	adap->timeout = 2 * HZ;
	adap->retries = 3;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;
	strlcpy(adap->name, dev_name(&pdev->dev), sizeof(adap->name));

	i2c_set_adapdata(adap, i2c);

	platform_set_drvdata(pdev, i2c);

	airoha_i2c_init(i2c, bus_freq);

	ret = i2c_add_adapter(adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add i2c bus to i2c core\n");
		return ret;
	}

	dev_info(&pdev->dev, "registered I2C driver %s, clock %u kHz\n",
		 i2c_driver_version, bus_freq / 1000);

	return 0;
}

static int airoha_i2c_remove(struct platform_device *pdev)
{
	struct airoha_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);

	return 0;
}

static const struct of_device_id i2c_airoha_dt_ids[] = {
	{ .compatible = "airoha,airoha-i2c" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, i2c_airoha_dt_ids);

static struct platform_driver airoha_i2c_driver = {
	.probe		= airoha_i2c_probe,
	.remove		= airoha_i2c_remove,
	.driver = {
		.name	= "airoha-i2c",
		.of_match_table = i2c_airoha_dt_ids,
	},
};

module_platform_driver(airoha_i2c_driver);

MODULE_AUTHOR("Iverson Chan");
MODULE_DESCRIPTION("Airoha I2C host driver");
MODULE_LICENSE("GPL v2");
