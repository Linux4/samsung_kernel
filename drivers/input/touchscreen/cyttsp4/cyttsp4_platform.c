/*
 * cyttsp4_platform.c
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
#include <linux/regulator/consumer.h>
#include "cyttsp4_regs.h"

#ifdef CYTTSP4_PLATFORM_FW_UPGRADE
#include "cyttsp4_heat_fw.h"
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
	.hw_version = 0x02,
	.fw_version = 0x0900,
	.cfg_version = 0x09,
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
#include "cyttsp4_params.h"
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *)&cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *)&cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = &cyttsp4_sett_param_regs,
	.param_size = &cyttsp4_sett_param_size,
	.fw_ver = ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_fw_ver),
};
#else
static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};
#endif

struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.ttconfig = &cyttsp4_ttconfig,
	.sdcard_path = "/sdcard/cypress_touchscreen/fw.bin",
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

static int pinctrl_init(struct cyttsp4_core_platform_data *pdata)
{
	int retval;

	pdata->gpio_state_active
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(pdata->gpio_state_active)) {
		pr_err("[TSP] %s : Can not get ts default pinstate\n",
			CYTTSP4_I2C_NAME);
		retval = PTR_ERR(pdata->gpio_state_active);
		pdata->ts_pinctrl = NULL;
		return retval;
	}

	pdata->gpio_state_suspend
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(pdata->gpio_state_suspend)) {
		pr_err("[TSP] %s : Can not get ts sleep pinstate\n",
			CYTTSP4_I2C_NAME);
		retval = PTR_ERR(pdata->gpio_state_suspend);
		pdata->ts_pinctrl = NULL;
		return retval;
	}

	return 0;
}

static int pinctrl_select(struct cyttsp4_core_platform_data *pdata,
						bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? pdata->gpio_state_active
		: pdata->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(pdata->ts_pinctrl, pins_state);
		if (ret) {
			pr_err("[TSP] %s : can not set %s pins\n",
				CYTTSP4_I2C_NAME,
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else
		pr_err("[TSP] %s : not a valid '%s' pinstate\n",
			CYTTSP4_I2C_NAME,
			on ? "pmx_ts_active" : "pmx_ts_suspend");

	return 0;
}

/*************************************************************************************************
 * Power
 *************************************************************************************************/

static int cy_hw_power(struct cyttsp4_core_platform_data *pdata, int on, int use_irq)
{
	int ret = 0;

	pr_info("[TSP] %s : power %s\n", CYTTSP4_I2C_NAME, on ? "on" : "off");

	if (use_irq) {
		if (on) {
			//enable_irq(gpio_to_irq(irq_gpio));
			//pr_debug("Enabled IRQ %d for TSP\n", gpio_to_irq(irq_gpio));
		} else {
			//pr_debug("Disabling IRQ %d for TSP\n", gpio_to_irq(irq_gpio));
			//disable_irq(gpio_to_irq(irq_gpio));
		}
	}

	ret = gpio_direction_output(pdata->vddo_gpio, on);
	if (ret) {
		pr_err("[TSP] %s : %s: unable to set_direction for gpio[%d] %d\n",
			 CYTTSP4_I2C_NAME, __func__, pdata->vddo_gpio, ret);
		return -EINVAL;
	}
//	msleep(50);
	pinctrl_select(pdata, on);

	ret = gpio_direction_output(pdata->avdd_gpio, on);
	if (ret) {
		pr_err("[TSP] %s : %s: unable to set_direction for gpio[%d] %d\n",
			 CYTTSP4_I2C_NAME, __func__, pdata->avdd_gpio, ret);
		return -EINVAL;
	}
	msleep(50);
	return 0;
}
/*************************************************************************************************
 *
 *************************************************************************************************/
int cyttsp4_xres(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
   int irq_gpio = pdata->irq_gpio;

	int rc = 0;
	printk(" The TOUCH  IRQ no in cyttsp4_xres() is %d",irq_gpio );
	cy_hw_power(pdata, 0, true);

	cy_hw_power(pdata, 1, true);

	return rc;
}

int cyttsp4_init(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
	int rc = 0;
	int irq_gpio = pdata->irq_gpio;

	if (on) {
		pinctrl_init(pdata);
		rc = gpio_request(irq_gpio, "TSP_INT");
		if(rc < 0){
			dev_err(dev, "%s: unable to request TSP_INT\n", __func__);
			return rc;
		}
		gpio_direction_input(irq_gpio);
		rc = gpio_request(pdata->avdd_gpio, "TSP_AVDD_gpio");
		if(rc < 0){
			dev_err(dev, "%s: unable to request TSP_AVDD_gpio\n", __func__);
			return rc;
		}
		rc = gpio_request(pdata->vddo_gpio, "TSP_VDDO_gpio");
		if(rc < 0){
			dev_err(dev, "%s: unable to request TSP_VDDO_gpio\n", __func__);
			return rc;
		}

		cy_hw_power(pdata, 1, false);
	} else {
		cy_hw_power(pdata, 0, false);
		gpio_free(irq_gpio);
	}
	dev_info(dev,
		"%s: INIT CYTTSP IRQ gpio=%d onoff=%d r=%d\n",
		__func__, irq_gpio, on, rc);
	return rc;
}

static int cyttsp4_wakeup(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	return cy_hw_power(pdata, 1, true);
}

static int cyttsp4_sleep(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	return cy_hw_power(pdata, 0, true);
}

int cyttsp4_power(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on)
		return cyttsp4_wakeup(pdata, dev, ignore_irq);

	return cyttsp4_sleep(pdata, dev, ignore_irq);
}

int cyttsp4_irq_stat(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

#ifdef CYTTSP4_DETECT_HW
int cyttsp4_detect(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, cyttsp4_platform_read read)
{
	int retry = 3;
	int rc;
	char buf[1];

	while (retry--) {
		/* Perform reset, wait for 100 ms and perform read */
		dev_vdbg(dev, "%s: Performing a reset\n", __func__);
		pdata->xres(pdata, dev);
		msleep(100);
		rc = read(dev, 0, buf, 1);
		if (!rc)
			return 0;

		dev_vdbg(dev, "%s: Read unsuccessful, try=%d\n",
			__func__, 3 - retry);
	}

	return rc;
}
#endif

