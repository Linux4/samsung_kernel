/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "fimc-is-dt.h"
#include "fimc-is-companion-dt.h"

#ifdef CONFIG_OF
int fimc_is_sensor_parse_dt_with_companion(struct platform_device *pdev)
{
	int ret = 0;
	u32 temp;
	char *pprop;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;
	const char *name;
	u32 id;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_ch", &pdata->i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_addr", &pdata->i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}
	pdata->id = id;

	ret = of_property_read_u32(dnode, "actuator_id", &pdata->actuator_id);
	if (ret) {
		pr_info("actuator_id is not exist.(%d)\n", ret);
		pdata->actuator_id = ACTUATOR_NAME_NOTHING;
	}

	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	ret = of_property_read_string(dnode, "sensor_name", &name);
	if (ret) {
		err("sensor_name read is fail(%d)", ret);
		goto p_err;
	}
	strcpy(pdata->sensor_name, name);

	ret = of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	if (id == SENSOR_POSITION_REAR)
		ret = fimc_is_power_initpin(dev);
	else {
		ret = fimc_is_power_setpin(dev, id, pdata->sensor_id);
	}
	if (ret)
		err("power_setpin(or initpin) failed(%d). id %d", ret, id);

	pdev->id = id;

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	} else {
		ret = get_pin_lookup_state(dev, pdata);
		if (ret < 0) {
			err("fimc_is_get_pin_lookup_state is fail");
			goto p_err;
		}
	}

	return ret;
p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_companion_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_companion_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_companion_iclk_on;
	pdata->iclk_off = exynos_fimc_is_companion_iclk_off;
	pdata->mclk_on = exynos_fimc_is_companion_mclk_on;
	pdata->mclk_off = exynos_fimc_is_companion_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	pdata->companion_use_pmic = of_property_read_bool(dnode, "companion_use_pmic");
	if (!pdata->companion_use_pmic) {
		err("use_pmic not use(%d)", pdata->companion_use_pmic);
	}

	ret = of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	ret = fimc_is_power_setpin(dev, SENSOR_POSITION_REAR, pdata->sensor_id);
	if (ret)
		err("power_setpin failed(%d)", ret);

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	} else {
		ret = get_pin_lookup_state(dev, pdata);
		if (ret < 0) {
			err("fimc_is_get_pin_lookup_state is fail");
			goto p_err;
		}
	}

	return ret;
p_err:
	kfree(pdata);
	return ret;
}
#endif

