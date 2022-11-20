/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/delay.h>
#if defined(CONFIG_ICE4_FPGA)
#include <linux/platform_data/ice4_irda.h>
#endif /* CONFIG_ICE4_FPGA */

#include <plat/gpio-cfg.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include "board-universal5260.h"
#if defined(CONFIG_ICE4_FPGA)
#include "board-universal5260-fpga-fw.h"
#endif /* CONFIG_ICE4_FPGA */

#if defined(CONFIG_ICE4_FPGA)
#define DUMMY_BIT_COUNT	49

struct fpga_gpio_table {
	int gpio;
	int type;
	int level;
	int pull;
};

static struct fpga_gpio_table fpga_gpio_table[] = {
	{
		.gpio	= GPIO_IRDA_IRQ,
		.type	= S3C_GPIO_INPUT,
		.level	= GPIO_LEVEL_NONE,
		.pull	= S3C_GPIO_PULL_NONE
	},
	{
		.gpio	= GPIO_CRESET_B,
		.type	= S3C_GPIO_OUTPUT,
		.level	= GPIO_LEVEL_HIGH,
		.pull	= S3C_GPIO_PULL_NONE
	},
	{
		.gpio	= GPIO_FPGA_RST_N,
		.type	= S3C_GPIO_OUTPUT,
		.level	= GPIO_LEVEL_LOW,
		.pull	= S3C_GPIO_PULL_NONE
	},
	{
		.gpio	= GPIO_IRDA_SDA,
		.type	= S3C_GPIO_OUTPUT,
		.level	= GPIO_LEVEL_LOW,
		.pull	= S3C_GPIO_PULL_NONE
	},
	{
		.gpio	= GPIO_IRDA_SCL,
		.type	= S3C_GPIO_OUTPUT,
		.level	= GPIO_LEVEL_HIGH,
		.pull	= S3C_GPIO_PULL_NONE
	},
	{
		.gpio	= GPIO_IRDA_EN,
		.type	= S3C_GPIO_OUTPUT,
		.level	= GPIO_LEVEL_LOW,
		.pull	= S3C_GPIO_PULL_NONE
	},
};

static void fpga_config_gpio_table(struct fpga_gpio_table *table, int size)
{
	unsigned int i, gpio;

	for (i = 0; i < size; i++) {
		gpio = table[i].gpio;

		s3c_gpio_setpull(gpio, table[i].pull);
		s3c_gpio_cfgpin(gpio, table[i].type);
		if (table[i].level != GPIO_LEVEL_NONE)
			gpio_set_value(gpio, table[i].level);
	}
}

/* I2C22 */
static struct i2c_gpio_platform_data gpio_i2c_data22 = {
	.scl_pin = GPIO_IRDA_SCL,
	.sda_pin = GPIO_IRDA_SDA,
	.udelay = 2,
};

static struct platform_device s3c_device_i2c22 = {
	.name = "i2c-gpio",
	.id = 22,
	.dev.platform_data = &gpio_i2c_data22,
};

static int irled_on(int on)
{
	gpio_set_value(GPIO_IRDA_EN, !!on);

	return 0;
}

static struct ice4_irda_platform_data ice4_irda_platdata = {
	.gpio_irda_irq		= GPIO_IRDA_IRQ,
	.gpio_fpga_rst_n	= GPIO_FPGA_RST_N,
	.gpio_creset		= GPIO_CRESET_B,

	.irled_on		= irled_on,
};

static struct i2c_board_info i2c_devs22_emul[] = {
	{
		I2C_BOARD_INFO("ice4_irda", 0xA0 >> 1),
		.platform_data = &ice4_irda_platdata,
	},
};

static struct platform_device *universal5260_fpga_devices[] = {
	&s3c_device_i2c22,
};

static void fpga_write_configuration_data(unsigned char *data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		int bit;
		unsigned char spibit = *data++;

		for (bit = 0; bit < BITS_PER_BYTE; spibit <<= 1, bit++) {
			gpio_set_value(GPIO_IRDA_SCL, GPIO_LEVEL_LOW);

			gpio_set_value(GPIO_IRDA_SDA, !!(spibit & 0x80));

			gpio_set_value(GPIO_IRDA_SCL, GPIO_LEVEL_HIGH);
		}
	}

	for (i = 0; i < DUMMY_BIT_COUNT; i++) {
		gpio_set_value(GPIO_IRDA_SCL, GPIO_LEVEL_LOW);
		udelay(1);
		gpio_set_value(GPIO_IRDA_SCL, GPIO_LEVEL_HIGH);
	}
}

static int fpga_init_gpio(void)
{
	int ret;

	ret = gpio_request_one(GPIO_IRDA_SCL, GPIOF_OUT_INIT_HIGH,
			"FPGA_SPI_CLK");
	if (ret < 0) {
		pr_err("%s: GPIO (FPGA_SPI_CLK) request failed: %d\n",
				__func__, ret);
		goto err_out;
	}
	ret = gpio_request_one(GPIO_IRDA_SDA, GPIOF_OUT_INIT_LOW,
			"FPGA_SPI_MOSI");
	if (ret < 0) {
		pr_err("%s: GPIO (FPGA_SPI_MOSI) request failed: %d\n",
				__func__, ret);
		goto free_irda_scl;
	}

	ret = gpio_request_one(GPIO_FPGA_RST_N, GPIOF_OUT_INIT_LOW,
			"FPGA_RST_N");
	if (ret < 0) {
		pr_err("%s: GPIO (FPGA_RST_N) request failed: %d\n",
				__func__, ret);
		goto free_irda_sda;
	}
	ret = gpio_request_one(GPIO_CRESET_B, GPIOF_OUT_INIT_HIGH,
			"FPGA_CRESET_B");
	if (ret < 0) {
		pr_err("%s: GPIO (FPGA_CRESET_B) request failed: %d\n",
				__func__, ret);
		goto free_fpga_rst_n;
	}

	ret = gpio_request_one(GPIO_IRDA_IRQ, GPIOF_IN, "IRDA_IRQ");
	if (ret < 0) {
		pr_err("%s: GPIO (IRDA_IRQ) request failed: %d\n",
				__func__, ret);
		goto free_creset_b;
	}

	ret = gpio_request_one(GPIO_IRDA_EN, GPIOF_OUT_INIT_LOW, "IRDA_EN");
	if (ret < 0) {
		pr_err("%s: GPIO (IRDA_EN) request failed: %d\n",
				__func__, ret);
		goto free_irda_irq;
	}

	fpga_config_gpio_table(fpga_gpio_table, ARRAY_SIZE(fpga_gpio_table));

	return 0;

free_irda_irq:
	gpio_free(GPIO_IRDA_IRQ);
free_creset_b:
	gpio_free(GPIO_CRESET_B);
free_fpga_rst_n:
	gpio_free(GPIO_FPGA_RST_N);
free_irda_sda:
	gpio_free(GPIO_IRDA_SDA);
free_irda_scl:
	gpio_free(GPIO_IRDA_SCL);
err_out:
	return ret;
}

static int fpga_write_configuration(void)
{
	gpio_set_value(GPIO_FPGA_RST_N, GPIO_LEVEL_LOW);
	gpio_set_value(GPIO_CRESET_B, GPIO_LEVEL_HIGH);
	msleep(10);

	gpio_set_value(GPIO_CRESET_B, GPIO_LEVEL_LOW);
	usleep_range(30, 40);

	gpio_set_value(GPIO_CRESET_B, GPIO_LEVEL_HIGH);
	usleep_range(1000, 1100);

	fpga_write_configuration_data(fpga_irda_fw, ARRAY_SIZE(fpga_irda_fw));

	pr_info("GPIO_FPGA_RST_N(%d) will be HIGH\n", GPIO_FPGA_RST_N);
	gpio_set_value(GPIO_FPGA_RST_N, GPIO_LEVEL_HIGH);
	gpio_export(GPIO_FPGA_RST_N, false);

	gpio_free(GPIO_IRDA_SDA);	/* for using gpio-i2c */
	gpio_free(GPIO_IRDA_SCL);	/* for using gpio-i2c */

	return 0;
}

static struct delayed_work configuration_work;
static void deffered_configuration(struct work_struct *work)
{
	if (fpga_write_configuration() < 0) {
		pr_err("%s: FPGA Configuration failed\n", __func__);
		return;
	}

	platform_add_devices(universal5260_fpga_devices,
			ARRAY_SIZE(universal5260_fpga_devices));
}
#endif /* CONFIG_ICE4_FPGA */

void __init exynos5_universal5260_fpga_init(void)
{
	pr_info("%s: initialization start!\n", __func__);
#if defined(CONFIG_ICE4_FPGA)
	if (fpga_init_gpio() == 0) {
        i2c_register_board_info(22, i2c_devs22_emul,
			ARRAY_SIZE(i2c_devs22_emul));
		INIT_DELAYED_WORK_DEFERRABLE(&configuration_work,
				deffered_configuration);
		schedule_delayed_work(&configuration_work, HZ);
	} else
		pr_err("%s: Initialization GPIOs is failed for the fpga.\n",
				__func__);
#endif /* CONFIG_ICE4_FPGA */
}
