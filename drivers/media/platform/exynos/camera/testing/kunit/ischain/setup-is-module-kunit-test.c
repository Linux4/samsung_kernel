// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/suspend.h>
#include <linux/ww_mutex.h>
#include <linux/regulator/driver.h>

#include "pablo-kunit-test.h"
#include "exynos-is-module.h"
#include "is-core.h"
#include "is-device-sensor.h"

struct pinctrl {
	struct list_head node;
	struct device *dev;
	struct list_head states;
	struct pinctrl_state *state;
	struct list_head dt_maps;
	struct kref users;
};

static DEFINE_WW_CLASS(regulator_ww_class);
#define REGULATOR_STATES_NUM	(PM_SUSPEND_MAX + 1)

struct regulator_voltage {
	int min_uV;
	int max_uV;
};

struct regulator_enable_gpio {
	struct list_head list;
	struct gpio_desc *gpiod;
	u32 enable_count;	/* a number of enabled shared GPIO */
	u32 request_count;	/* a number of requested shared GPIO */
};

struct regulator {
	struct device *dev;
	struct list_head list;
	unsigned int always_on:1;
	unsigned int bypass:1;
	unsigned int device_link:1;
	int uA_load;
	unsigned int enable_count;
	unsigned int deferred_disables;
	struct regulator_voltage voltage[REGULATOR_STATES_NUM];
	const char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
	struct dentry *debugfs;
};

static struct pablo_kunit_test_ctx {
	struct device *dev;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev;
	struct exynos_platform_is_sensor *sensor_pdata;
	struct is_device_sensor *sensor;
	struct is_resourcemgr *rscmgr;
	spinlock_t shared_rsc_slock;
} pkt_ctx;

static int pkt_mclk_on(struct device *dev, u32 scen, u32 ch, u32 freq)
{
	return 0;
}

static int pkt_mclk_off(struct device *dev, u32 scen, u32 ch)
{
	return 0;
}

/* Define testcases */
static void pablo_setup_module_pins_cfg_negative_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* TC #1. Invalid pinctrl index */
	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 0;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #2. Acquire the unbalanced shared resource type pin */
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];
	pin_ctrl->shared_rsc_type = SRT_ACQUIRE;
	pin_ctrl->shared_rsc_slock= &pkt_ctx.shared_rsc_slock;
	pin_ctrl->shared_rsc_count = 0;
	pin_ctrl->shared_rsc_active = 0;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #3. Release the unbalanced shared resource type pin */
	pin_ctrl->shared_rsc_type = SRT_RELEASE;
	pin_ctrl->shared_rsc_active = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. Module without subdev */
	pin_ctrl->shared_rsc_active = 0;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #5. Module subdev without sensor device */
	pin_ctrl->shared_rsc_type = 0;
	module->subdev = pkt_ctx.subdev;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #6. Sensor device without resource manager */
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);
}

static void pablo_setup_module_pins_cfg_gpio_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = pkt_ctx.rscmgr;

	/* TC #1. PIN_NONE */
	pin_ctrl->act = PIN_NONE;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #2. PIN_OUTPUT with HIGH */
	pin_ctrl->act = PIN_OUTPUT;
	pin_ctrl->value = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #3. PIN_OUTPUT with LOW */
	pin_ctrl->act = PIN_OUTPUT;
	pin_ctrl->value = 0;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. PIN_INPUT */
	pin_ctrl->act = PIN_INPUT;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #5. PIN_RESET */
	pin_ctrl->act = PIN_RESET;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #6. PIN_FUNCTION */
	pin_ctrl->act = PIN_FUNCTION;
	pin_ctrl->pin = (ulong)pdata->pinctrl->state;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_module_pins_cfg_regulator_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct is_resourcemgr *rscmgr = pkt_ctx.rscmgr;
	struct exynos_sensor_pin *pin_ctrl;
	struct is_module_regulator *im_regulator;
	struct regulator *regulator;
	struct regulator_dev *rdev;
	struct regulator_enable_gpio ena_pin;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = rscmgr;

	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];
	pin_ctrl->name = __getname();
	snprintf(pin_ctrl->name, sizeof(pin_ctrl->name), "%s", __func__);

	/* TC #1. There is no regulator to turning off. */
	pin_ctrl->act = PIN_REGULATOR;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, (int)PTR_ERR(0));

	/* TC #2. There is no regulator to turning on. */
	pin_ctrl->value = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* Setup regulator */
	im_regulator = kunit_kzalloc(test, sizeof(struct is_module_regulator), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, im_regulator);

	regulator = kzalloc(sizeof(struct regulator), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regulator);

	INIT_LIST_HEAD(&regulator->list);

	rdev = kunit_kzalloc(test, sizeof(struct regulator_dev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rdev);

	rdev->dev.kobj.state_initialized = 1;
	refcount_set(&rdev->dev.kobj.kref.refcount, 2);
	ww_mutex_init(&rdev->mutex, &regulator_ww_class);
	regulator->rdev = rdev;

	im_regulator->regulator = regulator;
	im_regulator->name = pin_ctrl->name;
	list_add_tail(&im_regulator->list, &rscmgr->regulator_list);

	/* TC #3. Turn on regulator which is already on. */
	regulator->always_on = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. Turn on regulator. */
	regulator->always_on = 0;
	regulator->voltage[PM_SUSPEND_ON].min_uV = 1;
	regulator->voltage[PM_SUSPEND_ON].max_uV = 1;
	rdev->ena_pin = &ena_pin;
	rdev->ena_gpio_state = 0;
	rdev->use_count = 1; /* Prevent to trigger actual regulator operation */
	pin_ctrl->voltage = 1;
	pin_ctrl->actuator_i2c_delay = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, regulator->enable_count, (unsigned int)1);
	KUNIT_EXPECT_EQ(test, rdev->use_count, (u32)2);
	KUNIT_EXPECT_GT(test, module->act_available_time, (ktime_t)0);

	/* TC #5. Turn off regulator which is already off. */
	rdev->ena_gpio_state = 0;
	regulator->enable_count = 0;
	pin_ctrl->value = 0;

	/* This releases the regulator object. */
	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)im_regulator->regulator, NULL);

	/* TC #5. Turn off regulator. */
	rdev->ena_gpio_state = 1;

	regulator = kzalloc(sizeof(struct regulator), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regulator);

	INIT_LIST_HEAD(&regulator->list);
	regulator->rdev = rdev;
	im_regulator->regulator = regulator;
	regulator->enable_count = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, regulator->enable_count, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, rdev->use_count, (u32)1);

	kunit_kfree(test, rdev);
	kunit_kfree(test, im_regulator);
	__putname(pin_ctrl->name);
}

static void pablo_setup_module_pins_cfg_regulator_opt_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct is_resourcemgr *rscmgr = pkt_ctx.rscmgr;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = rscmgr;

	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];

	/* TC #1. Fail to get regulator. */
	pin_ctrl->act = PIN_REGULATOR_OPTION;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #2. Fail to set NORMAL mode. */
	pin_ctrl->name = __getname();
	snprintf(pin_ctrl->name, sizeof(pin_ctrl->name), "%s", __func__);

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #3. Fail to set FAST mode. */
	pin_ctrl->value = 1;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);
}

static void pablo_setup_module_pins_cfg_i2c_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct is_resourcemgr *rscmgr = pkt_ctx.rscmgr;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = rscmgr;

	scen = SENSOR_SCENARIO_SECURE;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];

	/* TC #1. Trigger I2C control */
	pin_ctrl->act = PIN_I2C;

	ret = pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_module_pins_cfg_mclk_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *module_pdata = pkt_ctx.module_pdata;
	struct exynos_platform_is_sensor *sensor_pdata = pkt_ctx.sensor_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct is_resourcemgr *rscmgr = pkt_ctx.rscmgr;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = rscmgr;

	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	module_pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &module_pdata->pin_ctrls[scen][gpio_scen][0];
	pin_ctrl->shared_rsc_slock= &pkt_ctx.shared_rsc_slock;

	/* TC #1. Turn off MCLK, but there is no ops. */
	pin_ctrl->act = PIN_MCLK;
	pin_ctrl->shared_rsc_type = SRT_ACQUIRE;
	pin_ctrl->shared_rsc_active = 1;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #2. Turn off the shared type MCLK. */
	sensor_pdata->mclk_off = pkt_mclk_off;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #3. Turn on MCLK, but there is no ops. */
	pin_ctrl->value = 1;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. Turn on the shared type MCLK. */
	sensor_pdata->mclk_on = pkt_mclk_on;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #5. Turn on single MCLK. */
	pin_ctrl->shared_rsc_type = 0;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #6. Turn off single MCLK. */
	pin_ctrl->value = 0;

	ret = module_pdata->gpio_cfg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);
}

static void pablo_setup_module_pins_dbg_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_module *pdata = pkt_ctx.module_pdata;
	struct is_module_enum *module = pkt_ctx.module;
	struct exynos_sensor_pin *pin_ctrl;
	u32 scen, gpio_scen;
	int ret;

	/* Setup pinctrl */
	scen = SENSOR_SCENARIO_NORMAL;
	gpio_scen = GPIO_SCENARIO_ON;
	pdata->pinctrl_index[scen][gpio_scen] = 1;
	pin_ctrl = &pdata->pin_ctrls[scen][gpio_scen][0];
	module->subdev = pkt_ctx.subdev;
	v4l2_set_subdev_hostdata(pkt_ctx.subdev, pkt_ctx.sensor);
	pkt_ctx.sensor->resourcemgr = pkt_ctx.rscmgr;

	/* TC #1. PIN_NONE */
	pin_ctrl->act = PIN_NONE;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #2. PIN_FUNCTION */
	pin_ctrl->act = PIN_FUNCTION;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #3. PIN_OUTPUT */
	pin_ctrl->act = PIN_OUTPUT;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #4. PIN_INPUT */
	pin_ctrl->act = PIN_INPUT;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #5. PIN_RESET */
	pin_ctrl->act = PIN_RESET;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC #6. PIN_REGULATOR without name */
	pin_ctrl->act = PIN_REGULATOR;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #7. PIN_REGULATOR */
	pin_ctrl->name = __getname();
	snprintf(pin_ctrl->name, sizeof(pin_ctrl->name), "%s", __func__);

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #8. PIN_REGULATOR_OPTION */
	pin_ctrl->act = PIN_REGULATOR_OPTION;

	ret = pdata->gpio_dbg(module, scen, gpio_scen);

	KUNIT_EXPECT_EQ(test, ret, 0);

	__putname(pin_ctrl->name);
}
/* End of testcase definition */

static int pablo_setup_module_kunit_test_init(struct kunit *test)
{
	struct device *dev;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev;
	struct exynos_platform_is_sensor *sensor_pdata;
	struct is_device_sensor *sensor;
	struct is_resourcemgr *rscmgr;

	dev = is_get_is_dev();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	module_pdata = kunit_kzalloc(test, sizeof(struct exynos_platform_is_module), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module_pdata);

	is_get_gpio_ops(module_pdata);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module_pdata->gpio_cfg);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module_pdata->gpio_dbg);

	module_pdata->pinctrl = devm_pinctrl_get(dev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module_pdata->pinctrl);

	module = kunit_kzalloc(test, sizeof(struct is_module_enum), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module);

	subdev = kunit_kzalloc(test, sizeof(struct v4l2_subdev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, subdev);

	sensor_pdata = kunit_kzalloc(test, sizeof(struct exynos_platform_is_sensor), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_pdata);

	sensor = kunit_kzalloc(test, sizeof(struct is_device_sensor), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor);

	rscmgr = kunit_kzalloc(test, sizeof(struct is_resourcemgr), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rscmgr);

	spin_lock_init(&pkt_ctx.shared_rsc_slock);
	INIT_LIST_HEAD(&rscmgr->regulator_list);

	pkt_ctx.dev = dev;
	pkt_ctx.module_pdata = module_pdata;
	pkt_ctx.module = module;
	pkt_ctx.subdev = subdev;
	pkt_ctx.sensor_pdata = sensor_pdata;
	pkt_ctx.sensor = sensor;
	pkt_ctx.rscmgr = rscmgr;

	module->pdata = module_pdata;
	sensor->pdata = sensor_pdata;

	return 0;
}

static void pablo_setup_module_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, pkt_ctx.rscmgr);
	kunit_kfree(test, pkt_ctx.sensor);
	kunit_kfree(test, pkt_ctx.sensor_pdata);
	kunit_kfree(test, pkt_ctx.subdev);
	kunit_kfree(test, pkt_ctx.module);
	devm_pinctrl_put(pkt_ctx.module_pdata->pinctrl);
	kunit_kfree(test, pkt_ctx.module_pdata);
}

static struct kunit_case pablo_setup_module_kunit_test_cases[] = {
	KUNIT_CASE(pablo_setup_module_pins_cfg_negative_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_cfg_gpio_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_cfg_regulator_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_cfg_regulator_opt_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_cfg_i2c_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_cfg_mclk_kunit_test),
	KUNIT_CASE(pablo_setup_module_pins_dbg_kunit_test),
	{},
};

struct kunit_suite pablo_setup_module_kunit_test_suite = {
	.name = "setup-is-module-kunit-test",
	.init = pablo_setup_module_kunit_test_init,
	.exit = pablo_setup_module_kunit_test_exit,
	.test_cases = pablo_setup_module_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_setup_module_kunit_test_suite);

MODULE_LICENSE("GPL");
