/*
 * Copyright (C) 2020 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>

#include "sec_star.h"

#define V9RRP_ERR_MSG(msg...)	pr_err("[star-v9rrp] : " msg)
#define V9RRP_INFO_MSG(msg...)	pr_info("[star-v9rrp] : " msg)

#define SPI_DEFAULT_SPEED 6500000L

struct v9rrp_dev {
	struct spi_device *client;
	struct regulator *vdd;
	sec_star_t *star;
};

static struct v9rrp_dev g_v9rrp;

static int v9rrp_poweron(void)
{
	int ret = 0;

	V9RRP_INFO_MSG("v9rrp_poweron\n");

	ret = regulator_enable(g_v9rrp.vdd);
	if (ret) {
		V9RRP_ERR_MSG("%s - enable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(10000, 10500);
	return 0;
}

static int v9rrp_poweroff(void)
{
	int ret = 0;

	V9RRP_INFO_MSG("v9rrp_poweroff\n");

	ret = regulator_disable(g_v9rrp.vdd);
	if (ret) {
		V9RRP_ERR_MSG("%s - disable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(20000, 25000);
	return 0;
}

static int v9rrp_reset(void)
{
	int ret = 0;

	ret = regulator_disable(g_v9rrp.vdd);
	if (ret) {
		V9RRP_ERR_MSG("%s - disable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(20000, 25000);

	ret = regulator_enable(g_v9rrp.vdd);
	if (ret) {
		V9RRP_ERR_MSG("%s - enable vdd failed, ret=%d\n", __func__, ret);
		return ret;
	}

	usleep_range(10000, 10500);
	return 0;
}

static star_dev_t star_dev = {
	.name = "sec-v9rrp",
	.hal_type = SEC_HAL_SPI,
	.client = NULL,
	.power_on = v9rrp_poweron,
	.power_off = v9rrp_poweroff,
	.reset = v9rrp_reset
};

static int v9rrp_probe(struct spi_device *spi)
{
	int ret = -1;

	V9RRP_INFO_MSG("Entry : %s\n", __func__);

	if (spi->dev.of_node) {
		g_v9rrp.vdd = devm_regulator_get(&spi->dev, "1p8_pvdd");
		if (IS_ERR(g_v9rrp.vdd)) {
			V9RRP_ERR_MSG("%s - Failed to get 1p8_pvdd\n", __func__);
			return PTR_ERR(g_v9rrp.vdd);
		}
	} else {
		return -ENODEV;
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = SPI_DEFAULT_SPEED;
	ret = spi_setup(spi);
	if (ret < 0) {
		V9RRP_ERR_MSG("failed to do spi_setup()\n");
		return -ENODEV;
	}
	dev_set_drvdata(&spi->dev, &g_v9rrp);

	g_v9rrp.client = spi;
	star_dev.client = spi;
	g_v9rrp.star = star_open(&star_dev);
	if (g_v9rrp.star == NULL) {
		return -ENODEV;
	}
	V9RRP_INFO_MSG("Exit : %s\n", __func__);
	return ret;
}

static int v9rrp_remove(struct spi_device *spi)
{
	V9RRP_INFO_MSG("Entry : %s\n", __func__);
	star_close(g_v9rrp.star);
	V9RRP_INFO_MSG("Exit : %s\n", __func__);
	return 0;
}

static const struct of_device_id v9rrp_match_table[] = {
	{ .compatible = "sec_v9rrp",},
	{},
};

static struct spi_driver v9rrp_driver = {
	.probe = v9rrp_probe,
	.remove = v9rrp_remove,
	.driver = {
		.name = "ese_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = v9rrp_match_table,
	},
};

static int __init v9rrp_init(void)
{
	V9RRP_INFO_MSG("Entry : %s\n", __func__);
	return spi_register_driver(&v9rrp_driver);
}
module_init(v9rrp_init);

static void __exit v9rrp_exit(void)
{
	V9RRP_INFO_MSG("Entry : %s\n", __func__);
	spi_unregister_driver(&v9rrp_driver);
}

module_exit(v9rrp_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("ese spi driver");
MODULE_LICENSE("GPL");
