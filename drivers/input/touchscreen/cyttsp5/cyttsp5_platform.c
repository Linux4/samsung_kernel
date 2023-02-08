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

/* cyttsp */
#include <linux/cyttsp5/cyttsp5_core.h>
#include <linux/cyttsp5/cyttsp5_platform.h>

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_PLATFORM_FW_UPGRADE
/* #include "TSG5M_Kmini_rev_0C HW0x01FW0x0200.h" */
#include "cyttsp5_firmware.h"

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <mach/sec_debug.h>
#endif

/*#define dev_dbg(dev, fmt, arg...) dev_info(dev, fmt, ##arg)*/

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
	.hw_version = CY_HW_VERSION,
	.fw_version = CY_FW_VERSION,
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
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

static int enabled = 0;
extern unsigned int system_rev;
static int cyttsp5_hw_power(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, int on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;

	mutex_lock(&pdata->poweronoff_lock);
	if (enabled == on) {
		tsp_debug_err(true, dev, "%s: same command. not excute(%d)\n",
			__func__, enabled);
		mutex_unlock(&pdata->poweronoff_lock);
		return 0;
	}

	regulator_vdd = regulator_get(NULL, "vdd_tsp_1v8");
	if (IS_ERR(regulator_vdd)) {
		tsp_debug_err(true, dev, "%s: tsp_vdd regulator_get failed\n",
			__func__);
		mutex_unlock(&pdata->poweronoff_lock);
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, "vtsp_a3v3");
	if (IS_ERR(regulator_avdd)) {
		tsp_debug_err(true, dev, "%s: tsp_avdd regulator_get failed\n",
			__func__);
		regulator_put(regulator_vdd);
		mutex_unlock(&pdata->poweronoff_lock);
		return PTR_ERR(regulator_avdd);
	}

	tsp_debug_err(true, dev, "%s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_avdd);
		regulator_enable(regulator_vdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
#ifdef CONFIG_MACH_KMINI
		if (system_rev >= 1)
			s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_UP);
		else
			s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
#else
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
#endif
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);

		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		regulator_disable(regulator_vdd);
		regulator_disable(regulator_avdd);

		/* TODO: Delay time should be adjusted */
		//msleep(10);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

	mutex_unlock(&pdata->poweronoff_lock);

	return 0;
}
int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	int rc;

	rc = cyttsp5_hw_power(pdata, dev, 0);
	if (rc) {
		dev_err(dev, "%s: Fail power down HW\n", __func__);
		goto exit;
	}

	msleep(50);

	rc = cyttsp5_hw_power(pdata, dev, 1);
	if (rc) {
		dev_err(dev, "%s: Fail power up HW\n", __func__);
		goto exit;
	}
	msleep(50);

exit:
	return rc;
}

int cyttsp5_init(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	enabled = 0;
#if 0
	if (on)
		cyttsp5_hw_power(pdata, dev, 1);
	else
		cyttsp5_hw_power(pdata, dev, 0);
#else
	if (!on)
		cyttsp5_hw_power(pdata, dev, 0);
#endif
	dev_info(dev,
		"%s: INIT CYTTSP IRQ gpio=%d onoff=%d r=%d\n",
		__func__, irq_gpio, on, rc);
	return rc;
}

int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	return cyttsp5_hw_power(pdata, dev, on);
}

int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}
