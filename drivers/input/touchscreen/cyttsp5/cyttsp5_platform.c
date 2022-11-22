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

#define HW_VERSION 0x01
#define FW_VERSION 0x0200
#define GPIO_TSP_INT EXYNOS4_GPX3(5)

struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
	.hw_version = HW_VERSION,
	.fw_version = FW_VERSION
};
#else
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif
#if 0
static struct regulator_consumer_supply s2m_ldo30_data =
	REGULATOR_SUPPLY("vtsp_a3v3", NULL);

static struct regulator_consumer_supply s2m_ldo31_data =
	REGULATOR_SUPPLY("vdd_tsp_1v8", NULL);

static struct regulator_init_data s2m_ldo30_consumer = {
		.constraints	= {
			.name		= "vtsp_a3v3",
			.min_uV		= 3300000,
			.max_uV		= 3300000,
			.apply_uV	= 1,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.state_mem	= {
				.disabled	= 1,
			},
		},
		.num_consumer_supplies	= 1,
		.consumer_supplies	= &s2m_ldo30_consumer,
	};

static struct regulator_init_data s2m_ldo31_consumer = {
		.constraints	= {
			.name		= "vdd_tsp_1v8",
			.min_uV		= 1800000,
			.max_uV		= 1800000,
			.apply_uV	= 1,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.state_mem	= {
				.disabled	= 1,
			},
		},
		.num_consumer_supplies	= 1,
		.consumer_supplies	= &s2m_ldo31_consumer,
	};

static struct sec_regulator_data exynos_regulators[] = {

	{S2MPU01A_LDO30, &s2m_ldo30_data},
	{S2MPU01A_LDO31, &s2m_ldo31_data},
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

int enabled = 0;

static int cyttsp5_hw_power(struct cyttsp5_core_platform_data *pdata, int on)
{

	int ret;
	unsigned irq_gpio = pdata->irq_gpio;
//	struct device *dev = &client->dev;     //
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
//	struct pinctrl *pinctrl_irq;

	if (enabled == on)
		return 0;
#if 0
	ret = gpio_direction_output(31, on);
	if (ret) {
		pr_err("[TKEY]%s: unable to set_direction for vdd_led [%d]\n",
			 __func__, 31);
		return -EINVAL;
	}
/*
	ret = gpio_direction_output(74, on);
	if (ret) {
		pr_err("[TKEY]%s: unable to set_direction for vdd_led [%d]\n",
			 __func__, 74);
		return -EINVAL;
	}
*/
	msleep(100);
#else

	regulator_vdd = regulator_get(NULL, pdata->regulator_vdd);
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_vdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd regulator_get failed\n");
		regulator_put(regulator_vdd);
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		if (!regulator_is_enabled(regulator_vdd)){
			ret = regulator_enable(regulator_vdd);
			if (ret) {
				printk(KERN_ERR "[TSP] %s: Failed to enable vdd: %d\n", __func__, ret);
				return -EINVAL;
			}
		}
		if (!regulator_is_enabled(regulator_avdd)){
			ret =regulator_enable(regulator_avdd);
			if (ret) {
				printk(KERN_ERR "[TSP] %s: Failed to enable avdd: %d\n", __func__, ret);
				return -EINVAL;
			}
		}
//		pinctrl_irq = devm_pinctrl_get_select(dev, "on_state");
//		if (IS_ERR(pinctrl_irq))
//			printk(KERN_ERR "%s: Failed to configure tsp_attn pin\n", __func__);
   //pinctrl_irq manual method
		s3c_gpio_cfgpin(irq_gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_NONE);

	} else {
		//pinctrl_irq manual method
		s3c_gpio_cfgpin(irq_gpio, S3C_GPIO_INPUT);
		s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_NONE);


		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

//		pinctrl_irq = devm_pinctrl_get_select(dev, "off_state");
//		if (IS_ERR(pinctrl_irq))
//			printk(KERN_ERR "%s: Failed to configure tsp_attn pin\n", __func__);

		/* TODO: Delay time should be adjusted */
		msleep(10);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);
#endif
	pr_info("[wisdyd] %s: - \n", __func__);
	return 0;
}
int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	int rc;

	rc = cyttsp5_hw_power(pdata,0);
	if (rc) {
		dev_err(dev, "%s: Fail power down HW\n", __func__);
		goto exit;
	}

	msleep(200);

	rc = cyttsp5_hw_power(pdata,1);
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
	unsigned irq_gpio = pdata->irq_gpio;
	int rc = 0;
	pr_info("[wisdyd] %s: + \n", __func__);

	enabled = 0;

	if (on) {
		gpio_request(irq_gpio, "TSP_INT");
		s3c_gpio_cfgpin(irq_gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(irq_gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(irq_gpio);
		cyttsp5_hw_power(pdata,1);
	} else {
		cyttsp5_hw_power(pdata,0);
		gpio_free(irq_gpio);
	}

	dev_info(dev,
		"%s: INIT CYTTSP IRQ gpio=%d onoff=%d r=%d\n",
		__func__, irq_gpio, on, rc);
	pr_info("[wisdyd] %s: - \n", __func__);
	return rc;
}

int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	return cyttsp5_hw_power(pdata,on);
}

int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

#ifndef CONFIG_TOUCHSCREEN_CYTTSP5_DEVICETREE_SUPPORT // platform data of board file
#include <linux/input.h>
#include <linux/export.h>

#include <linux/cyttsp5/cyttsp5_core.h>
#include <linux/cyttsp5/cyttsp5_platform.h>

#define CYTTSP5_HID_DESC_REGISTER 1


#define CY_MAXX 720
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
	.irq_gpio = GPIO_TSP_INT,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.init = cyttsp5_init,
	.power = cyttsp5_power,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		NULL, /* &cyttsp5_sett_btn_keys, */	/* button-to-keycode table */
	},
	//.flags = CY_FLAG_CORE_POWEROFF_ON_SLEEP,
};

static const uint16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -128, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
};

struct touch_framework cyttsp5_framework = {
	.abs = (uint16_t *)&cyttsp5_abs[0],
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = CY_MT_FLAG_NONE,
	.inp_dev_name = "sec_touchscreen",
};

static struct cyttsp5_btn_platform_data _cyttsp5_btn_platform_data = {
	.inp_dev_name = "sec_touchkey",
};

struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
	.btn_pdata = &_cyttsp5_btn_platform_data,
};

EXPORT_SYMBOL(_cyttsp5_platform_data);

#endif //#if 1

