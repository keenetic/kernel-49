/*
 *  Serial Port driver for Open Firmware platform devices
 *
 *    Copyright (C) 2006 Arnd Bergmann <arnd@arndb.de>, IBM Corp.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */
#include <linux/console.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/clk.h>

#include "8250.h"

struct of_serial_info {
	struct clk *clk;
	int type;
	int line;
};

#ifdef CONFIG_ARCH_TEGRA
static void tegra_serial_handle_break(struct uart_port *p)
{
	unsigned int status, tmout = 10000;

	do {
		status = p->serial_in(p, UART_LSR);
		if (status & (UART_LSR_FIFOE | UART_LSR_BRK_ERROR_BITS))
			status = p->serial_in(p, UART_RX);
		else
			break;
		if (--tmout == 0)
			break;
		udelay(1);
	} while (1);
}
#else
static inline void tegra_serial_handle_break(struct uart_port *port)
{
}
#endif

/*
 * Fill a struct uart_port for a given device node
 */
static int of_platform_serial_setup(struct platform_device *ofdev,
			int type, struct uart_port *port,
			struct of_serial_info *info)
{
	struct resource resource;
	struct device_node *np = ofdev->dev.of_node;
	u32 clk, spd, prop;
	int ret;

	memset(port, 0, sizeof *port);
	if (of_property_read_u32(np, "clock-frequency", &clk)) {

		/* Get clk rate through clk driver if present */
		info->clk = devm_clk_get(&ofdev->dev, NULL);
		if (IS_ERR(info->clk)) {
			dev_warn(&ofdev->dev,
				"clk or clock-frequency not defined\n");
			return PTR_ERR(info->clk);
		}

		ret = clk_prepare_enable(info->clk);
		if (ret < 0)
			return ret;

		clk = clk_get_rate(info->clk);
	}
	/* If current-speed was set, then try not to change it. */
	if (of_property_read_u32(np, "current-speed", &spd) == 0)
		port->custom_divisor = clk / (16 * spd);

	ret = of_address_to_resource(np, 0, &resource);
	if (ret) {
		dev_warn(&ofdev->dev, "invalid address\n");
		goto err_unprepare;
	}

	spin_lock_init(&port->lock);
	port->mapbase = resource.start;
	port->mapsize = resource_size(&resource);

	/* Check for shifted address mapping */
	if (of_property_read_u32(np, "reg-offset", &prop) == 0) {
		if (prop >= port->mapsize) {
			dev_warn(&ofdev->dev, "reg-offset %u exceeds region size %pa\n",
				 prop, &port->mapsize);
			ret = -EINVAL;
			goto err_unprepare;
		}

		port->mapbase += prop;
		port->mapsize -= prop;
	}

	/* Compatibility with the deprecated pxa driver and 8250_pxa drivers. */
	if (of_device_is_compatible(np, "mrvl,mmp-uart"))
		port->regshift = 2;

	/* Check for registers offset within the devices address range */
	if (of_property_read_u32(np, "reg-shift", &prop) == 0)
		port->regshift = prop;

	/* Check for fifo size */
	if (of_property_read_u32(np, "fifo-size", &prop) == 0)
		port->fifosize = prop;

	/* Check for a fixed line number */
	ret = of_alias_get_id(np, "serial");
	if (ret >= 0)
		port->line = ret;

	port->irq = irq_of_parse_and_map(np, 0);
	port->iotype = UPIO_MEM;
	if (of_property_read_u32(np, "reg-io-width", &prop) == 0) {
		switch (prop) {
		case 1:
			port->iotype = UPIO_MEM;
			break;
		case 2:
			port->iotype = UPIO_MEM16;
			break;
		case 4:
			port->iotype = of_device_is_big_endian(np) ?
				       UPIO_MEM32BE : UPIO_MEM32;
			break;
		default:
			dev_warn(&ofdev->dev, "unsupported reg-io-width (%d)\n",
				 prop);
			ret = -EINVAL;
			goto err_dispose;
		}
	}

	port->type = type;
	port->uartclk = clk;
	port->flags = UPF_SHARE_IRQ | UPF_BOOT_AUTOCONF | UPF_IOREMAP
		| UPF_FIXED_PORT | UPF_FIXED_TYPE;

	if (of_find_property(np, "no-loopback-test", NULL))
		port->flags |= UPF_SKIP_TEST;

	port->dev = &ofdev->dev;

	switch (type) {
	case PORT_TEGRA:
		port->handle_break = tegra_serial_handle_break;
		break;

	case PORT_RT2880:
		port->iotype = UPIO_AU;
		break;
	}

	if (IS_ENABLED(CONFIG_SERIAL_8250_FSL) &&
	    (of_device_is_compatible(np, "fsl,ns16550") ||
	     of_device_is_compatible(np, "fsl,16550-FIFO64")))
		port->handle_irq = fsl8250_handle_irq;

	return 0;
err_dispose:
	irq_dispose_mapping(port->irq);
err_unprepare:
	if (info->clk)
		clk_disable_unprepare(info->clk);
	return ret;
}

/*
 * Try to register a serial port
 */
static const struct of_device_id of_platform_serial_table[];
static int of_platform_serial_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct of_serial_info *info;
	struct uart_8250_port port8250;
	u32 tx_threshold;
	int port_type;
	int ret;

	match = of_match_device(of_platform_serial_table, &ofdev->dev);
	if (!match)
		return -EINVAL;

	if (of_find_property(ofdev->dev.of_node, "used-by-rtas", NULL))
		return -EBUSY;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	port_type = (unsigned long)match->data;
	memset(&port8250, 0, sizeof(port8250));
	ret = of_platform_serial_setup(ofdev, port_type, &port8250.port, info);
	if (ret)
		goto err_free;

	if (port8250.port.fifosize)
		port8250.capabilities = UART_CAP_FIFO;

	/* Check for TX FIFO threshold & set tx_loadsz */
	if ((of_property_read_u32(ofdev->dev.of_node, "tx-threshold",
				  &tx_threshold) == 0) &&
	    (tx_threshold < port8250.port.fifosize))
		port8250.tx_loadsz = port8250.port.fifosize - tx_threshold;

	if (of_property_read_bool(ofdev->dev.of_node, "auto-flow-control"))
		port8250.capabilities |= UART_CAP_AFE;

	ret = serial8250_register_8250_port(&port8250);
	if (ret < 0)
		goto err_dispose;

	info->type = port_type;
	info->line = ret;
	platform_set_drvdata(ofdev, info);
	return 0;
err_dispose:
	irq_dispose_mapping(port8250.port.irq);
	if (info->clk)
		clk_disable_unprepare(info->clk);
err_free:
	kfree(info);
	return ret;
}

/*
 * Release a line
 */
static int of_platform_serial_remove(struct platform_device *ofdev)
{
	struct of_serial_info *info = platform_get_drvdata(ofdev);

	serial8250_unregister_port(info->line);

	if (info->clk)
		clk_disable_unprepare(info->clk);
	kfree(info);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int of_serial_suspend(struct device *dev)
{
	struct of_serial_info *info = dev_get_drvdata(dev);
	struct uart_8250_port *port8250 = serial8250_get_port(info->line);
	struct uart_port *port = &port8250->port;

	serial8250_suspend_port(info->line);

	if (info->clk && (!uart_console(port) || console_suspend_enabled))
		clk_disable_unprepare(info->clk);

	return 0;
}

static int of_serial_resume(struct device *dev)
{
	struct of_serial_info *info = dev_get_drvdata(dev);
	struct uart_8250_port *port8250 = serial8250_get_port(info->line);
	struct uart_port *port = &port8250->port;

	if (info->clk && (!uart_console(port) || console_suspend_enabled))
		clk_prepare_enable(info->clk);

	serial8250_resume_port(info->line);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(of_serial_pm_ops, of_serial_suspend, of_serial_resume);

/*
 * A few common types, add more as needed.
 */
static const struct of_device_id of_platform_serial_table[] = {
	{ .compatible = "ns8250",   .data = (void *)PORT_8250, },
	{ .compatible = "ns16450",  .data = (void *)PORT_16450, },
	{ .compatible = "ns16550a", .data = (void *)PORT_16550A, },
	{ .compatible = "ns16550",  .data = (void *)PORT_16550, },
	{ .compatible = "ns16750",  .data = (void *)PORT_16750, },
	{ .compatible = "ns16850",  .data = (void *)PORT_16850, },
	{ .compatible = "nvidia,tegra20-uart", .data = (void *)PORT_TEGRA, },
	{ .compatible = "nxp,lpc3220-uart", .data = (void *)PORT_LPC3220, },
	{ .compatible = "ralink,rt2880-uart", .data = (void *)PORT_RT2880, },
	{ .compatible = "airoha,airoha-uart", .data = (void *)PORT_AIROHA, },
	{ .compatible = "altr,16550-FIFO32",
		.data = (void *)PORT_ALTR_16550_F32, },
	{ .compatible = "altr,16550-FIFO64",
		.data = (void *)PORT_ALTR_16550_F64, },
	{ .compatible = "altr,16550-FIFO128",
		.data = (void *)PORT_ALTR_16550_F128, },
	{ .compatible = "mrvl,mmp-uart",
		.data = (void *)PORT_XSCALE, },
	{ .compatible = "mrvl,pxa-uart",
		.data = (void *)PORT_XSCALE, },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, of_platform_serial_table);

static struct platform_driver of_platform_serial_driver = {
	.driver = {
		.name = "of_serial",
		.of_match_table = of_platform_serial_table,
		.pm = &of_serial_pm_ops,
	},
	.probe = of_platform_serial_probe,
	.remove = of_platform_serial_remove,
};

module_platform_driver(of_platform_serial_driver);

MODULE_AUTHOR("Arnd Bergmann <arnd@arndb.de>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Serial Port driver for Open Firmware platform devices");
