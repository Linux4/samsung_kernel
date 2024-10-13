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
#include <linux/of.h>
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
	int gpio_reset = 0, gpio_standby = 0;
#ifdef CONFIG_SOC_EXYNOS5422
	int gpios_cam_en = 0;
#endif
#if !defined(CONFIG_USE_VENDER_FEATURE) /* LSI Patch */
	int gpio_cam_en = 0;
	int gpio_comp_en, gpio_comp_rst;
#else
	const char *name;
#endif
#ifdef CONFIG_SOC_EXYNOS5433
	struct pinctrl *pinctrl_ch = NULL;
#endif
	int gpio_none = 0;
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

	DT_READ_U32(dnode, "flash_first_gpio",   pdata->flash_first_gpio );
	DT_READ_U32(dnode, "flash_second_gpio",  pdata->flash_second_gpio);

#ifdef CONFIG_USE_VENDER_FEATURE
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
#endif
	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		ret = -EINVAL;
		goto p_err;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}
#ifdef CONFIG_SOC_EXYNOS5422
	gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpios_cam_en)) {
		dev_err(dev, "failed to get main/front cam en gpio\n");
	} else {
		gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_cam_en);
	}
#endif

#if !defined(CONFIG_USE_VENDER_FEATURE) /* LSI Patch */
	/* Optional Feature */
	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en))
	dev_err(dev, "failed to get main comp en gpio\n");

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst))
	dev_err(dev, "failed to get main comp reset gpio\n");

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby))
		dev_err(dev, "failed to get gpio_standby\n");

	gpio_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpio_cam_en))
		dev_err(dev, "failed to get gpio_cam_en\n");

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_cam_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "ch", PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_comp_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_comp_rst, 0, NULL, PIN_RESET);
	if (id == SENSOR_POSITION_REAR) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "af", PIN_FUNCTION);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_comp_rst, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_cam_en, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_cam_en, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_reset, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_standby, 0, NULL, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpio_none, 0, NULL, PIN_END);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, PIN_RESET);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, PIN_INPUT);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_cam_en, 0, NULL, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpio_none, 0, NULL, PIN_END);

#else /* TN CODE */

	if (id == SENSOR_POSITION_FRONT) {
		gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
		if (!gpio_is_valid(gpio_standby)) {
			dev_err(dev, "failed to get gpio_standby\n");
		} else {
			gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(gpio_standby);
		}
#ifdef CONFIG_SOC_EXYNOS5433
		/* initial - i2c off */
		pinctrl_ch = devm_pinctrl_get_select(&pdev->dev, "off1");
		if (IS_ERR_OR_NULL(pinctrl_ch)) {
			pr_err("%s: cam %s pins are not configured\n", __func__, "off1");
		} else {
			devm_pinctrl_put(pinctrl_ch);
		}
#endif
	}
#ifdef CONFIG_SOC_EXYNOS5422
	if (id == SENSOR_POSITION_REAR) {
		/* BACK CAMERA	- POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, NULL, 0, PIN_END);

		/* BACK CAMERA	- POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, NULL, 0, PIN_END);
	} else {
		/* FRONT CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpios_cam_en, 0, NULL, 1000, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* FRONT CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpios_cam_en, 0, NULL, 1000, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4, gpio_standby, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);
	}
#else /*CONFIG_SOC_EXYNOS5430*/
	if (id == SENSOR_POSITION_REAR) {
		/* BACK CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, NULL, 0, PIN_END);

		/* BACK CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, NULL, 0, PIN_END);
	} else {
		/* FRONT CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "VT_CAM_1.2V", 1000, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* FRONT CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_none, 0, "VT_CAM_1.2V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER ON */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 1, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 2, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 3, gpio_none, 0, "VT_CAM_1.2V", 1000, PIN_REGULATOR_ON);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 4, gpio_standby, 0, NULL, 0, PIN_OUTPUT_HIGH);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 5, gpio_none, 0, "ch", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 6, gpio_none, 0, NULL, 0, PIN_END);

		/* VISION CAMERA  - POWER OFF */
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 2, gpio_standby, 0, NULL, 0, PIN_OUTPUT_LOW);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 3, gpio_none, 0, "VT_CAM_1.2V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VT_CAM_2.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VT_CAM_1.8V", 0, PIN_REGULATOR_OFF);
		SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);
	}
#endif
#endif
	pdev->id = id;

	dev->platform_data = pdata;

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
	int gpio_comp_en = 0, gpio_comp_rst = 0;
	int gpio_none = 0;
	int gpio_reset = 0;
	int gpios_cam_en = -EINVAL;
#ifdef CONFIG_OIS_USE
	int gpios_ois_en = 0;
#endif

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

	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en)) {
		dev_err(dev, "failed to get main comp en gpio\n");
	} else {
		gpio_request_one(gpio_comp_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_en);
	}

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst)) {
		dev_err(dev, "failed to get main comp reset gpio\n");
	} else {
		gpio_request_one(gpio_comp_rst, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_rst);
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		ret = -EINVAL;
		goto p_err;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}
#ifdef CONFIG_SOC_EXYNOS5422
	gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
	if (!gpio_is_valid(gpios_cam_en)) {
		dev_err(dev, "failed to get main/front cam en gpio\n");
	} else {
		gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_cam_en);
	}
#else /* EXYNOS5430, EXYNOS5433 */
	if (of_get_property(dnode, "gpios_cam_en", NULL)) {
		gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
		if (!gpio_is_valid(gpios_cam_en)) {
			dev_err(dev, "failed to get main cam en gpio\n");
		} else {
			gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(gpios_cam_en);
		}
	}
#endif
#ifdef CONFIG_OIS_USE
	gpios_ois_en = of_get_named_gpio(dnode, "gpios_ois_en", 0);
	pdata->pin_ois_en = gpios_ois_en;
	if (!gpio_is_valid(gpios_ois_en)) {
		dev_err(dev, "failed to get ois en gpio\n");
	} else {
		gpio_request_one(gpios_ois_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_ois_en);
	}
#endif

	pdata->companion_use_pmic = of_property_read_bool(dnode, "companion_use_pmic");
	if (!pdata->companion_use_pmic) {
		err("use_pmic not use(%d)", pdata->companion_use_pmic);
	}

#ifdef CONFIG_SOC_EXYNOS5422
	/* COMPANION - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_comp_en, 0, NULL, 150, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9, gpio_none, 0, "af", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10, gpio_none, 0, NULL, 0, PIN_END);


	/* COMPANION - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_comp_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9, gpio_none, 0, NULL, 0, PIN_END);
#else /* CONFIG_SOC_EXYNOS5430, CONFIG_SOC_EXYNOS5433 */
	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
#ifdef CONFIG_OIS_USE
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_ON);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_comp_en, 0, NULL, 150, PIN_OUTPUT_HIGH);
	if (pdata->companion_use_pmic) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8, gpio_none, 0, "VDD_MIPI_1.0V_COMP", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11, gpio_none, 0, "af", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 12, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 13, gpio_none, 0, NULL, 0, PIN_END);

	/* BACK CAMERA  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_LOW);
	if (pdata->companion_use_pmic) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VDD_MIPI_1.0V_COMP", 0, PIN_REGULATOR_OFF);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_comp_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_OFF);
	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_OFF);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
#ifdef CONFIG_OIS_USE
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 10, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 11, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_OFF);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 12, gpio_none, 0, NULL, 0, PIN_END);
#endif

#ifdef CONFIG_OIS_USE
	/* OIS_FACTORY  - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 1, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 2, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 5, gpio_none, 0, NULL, 0, PIN_END);

	/* OIS_FACTORY  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 2, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 3, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 5, gpio_none, 0, NULL, 0, PIN_END);
#endif

	dev->platform_data = pdata;

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

