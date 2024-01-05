/*
 * cyttsp5_platform.c
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-shannon222ap.h>

/* cyttsp */
#include <linux/cyttsp5/cyttsp5_bus.h>
#include <linux/cyttsp5/cyttsp5_core.h>
#include <linux/cyttsp5/cyttsp5_platform.h>

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_PLATFORM_FW_UPGRADE
#include "cyttsp5_fw.h"
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp5_params.h"
static struct touch_settings cyttsp5_sett_param_regs = {
	.data = (uint8_t *)&cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp5_sett_param_size = {
	.data = (uint8_t *)&cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp5_touch_config cyttsp5_ttconfig = {
	.param_regs = &cyttsp5_sett_param_regs,
	.param_size = &cyttsp5_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp5_touch_config cyttsp5_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data = {
	.fw = &cyttsp5_firmware,
	.ttconfig = &cyttsp5_ttconfig,
	.flags = CY_FLAG_LOAD_NONE,
};

int enabled = 0;

int cyttsp5_hw_power(int on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "vdd_tsp_1v8");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_vdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, "vtsp_a3v3");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd regulator_get failed\n");
		regulator_put(regulator_vdd);
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_vdd);
		regulator_enable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		regulator_disable(regulator_vdd);
		regulator_disable(regulator_avdd);

		/* TODO: Delay time should be adjusted */
		msleep(10);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

	return 0;
}
int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	int rc;

	rc = cyttsp5_hw_power(0);
	if (rc) {
		dev_err(dev, "%s: Fail power down HW\n", __func__);
		goto exit;
	}

	msleep(200);

	rc = cyttsp5_hw_power(1);
	if (rc) {
		dev_err(dev, "%s: Fail power up HW\n", __func__);
		goto exit;
	}
	msleep(100);

exit:
	return rc;
}

int cyttsp5_init(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	enabled = 0;

	if (on) {
		gpio_request(irq_gpio, "TSP_INT");
		s3c_gpio_cfgpin(irq_gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(irq_gpio);
		cyttsp5_hw_power(1);
	} else {
		cyttsp5_hw_power(0);
		gpio_free(irq_gpio);
	}

	dev_info(dev,
		"%s: INIT CYTTSP IRQ gpio=%d r=%d\n",
		__func__, irq_gpio, rc);
	return rc;
}

int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	return cyttsp5_hw_power(on);
}

int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}
