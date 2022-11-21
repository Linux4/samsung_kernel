/*
 * Copyright (C) 2015 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "[VIB] dc_haptic_vib: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/vibrator/sec_vibrator.h>
#include <linux/pwm.h>
#include <linux/hrtimer.h>

#define DC_HAPTIC_VIB_NAME "dc_haptic_vib"

#define BRAKING_DUTY			500
#define HAPTIC_DUTY			49500
#define HAPTIC_PERIOD			50000
#define BRAKING_DURATION_THRESHOLD	135

struct dc_haptic_vib_pdata {
	const char *regulator_name;
	struct regulator *regulator;
	int ldo_en_gpio;
	int haptic_en_gpio;
	const char *motor_type;
	struct pwm_device *pwm;
#if defined(CONFIG_SEC_VIBRATOR)
	bool calibration;
	int steps;
	int *haptic_durations;
#endif
};

struct dc_haptic_vib_drvdata {
	struct sec_vibrator_drvdata sec_vib_ddata;
	struct dc_haptic_vib_pdata *pdata;
	struct hrtimer braking_timer;
	bool running;
	bool is_braking;
};

static void dc_haptic_vib_power_control(struct dc_haptic_vib_drvdata *ddata, bool on)
{
	if (on) {
		if (ddata->pdata->regulator) {
			pr_info("%s: regulator on\n", __func__);
			if (!regulator_is_enabled(ddata->pdata->regulator))
				regulator_enable(ddata->pdata->regulator);
		}
		if (gpio_is_valid(ddata->pdata->ldo_en_gpio)) {
			pr_info("%s: ldo en gpio on\n", __func__);
			gpio_direction_output(ddata->pdata->ldo_en_gpio, 1);
		}
	} else {
		if (ddata->pdata->regulator) {
			pr_info("%s: regulator off\n", __func__);
			if (regulator_is_enabled(ddata->pdata->regulator))
				regulator_disable(ddata->pdata->regulator);
		}
		if (gpio_is_valid(ddata->pdata->ldo_en_gpio)) {
			pr_info("%s: ldo en gpio off\n", __func__);
			gpio_direction_output(ddata->pdata->ldo_en_gpio, 0);
		}
	}
}

static enum hrtimer_restart braking_timer_func(struct hrtimer *timer)
{
	struct dc_haptic_vib_drvdata *ddata = container_of(timer, struct dc_haptic_vib_drvdata, braking_timer);

	pr_info("%s\n", __func__);

	pwm_config(ddata->pdata->pwm, BRAKING_DUTY, HAPTIC_PERIOD);

	return HRTIMER_NORESTART;
}

static int dc_haptic_vib_enable(struct device *dev, bool en)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	int timeout = ddata->sec_vib_ddata.timeout;
	int braking_time = 0;

	hrtimer_cancel(&ddata->braking_timer);

	if (en) {
		if (timeout <= BRAKING_DURATION_THRESHOLD) {
			pwm_config(ddata->pdata->pwm, HAPTIC_DUTY, HAPTIC_PERIOD);
			pwm_enable(ddata->pdata->pwm);

			braking_time = (timeout * 2) / 3;
			hrtimer_start(&ddata->braking_timer, ns_to_ktime((u64)braking_time * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);

			ddata->is_braking = true;
		}

		dc_haptic_vib_power_control(ddata, en);

		if (gpio_is_valid(ddata->pdata->haptic_en_gpio)) {
			pr_info("%s: haptic en gpio on\n", __func__);
			gpio_direction_output(ddata->pdata->haptic_en_gpio, 1);
		}

		ddata->running = true;
	} else {
		if (ddata->is_braking) {
			pwm_disable(ddata->pdata->pwm);
			ddata->is_braking = false;
		}

		if (gpio_is_valid(ddata->pdata->haptic_en_gpio)) {
			pr_info("%s: haptic en gpio off\n", __func__);
			gpio_direction_output(ddata->pdata->haptic_en_gpio, 0);
		}

		dc_haptic_vib_power_control(ddata, en);

		ddata->running = false;
	}

	return 0;
}

static int dc_haptic_vib_get_motor_type(struct device *dev, char *buf)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);

	return ret;
}

#if defined(CONFIG_SEC_VIBRATOR)
static bool dc_haptic_vib_get_calibration(struct device *dev)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;

	return pdata->calibration;
}

static int dc_haptic_vib_get_step_size(struct device *dev, int *step_size)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;

	pr_info("%s step_size=%d\n", __func__, pdata->steps);

	if (pdata->steps == 0)
		return -ENODATA;

	*step_size = pdata->steps;

	return 0;
}

#if defined(CONFIG_USE_HAPTIC_DURATIONS)
static int dc_haptic_vib_get_haptic_durations(struct device *dev, int *buf)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->haptic_durations[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->haptic_durations[i];

	return 0;
}

static int dc_haptic_vib_set_haptic_durations(struct device *dev, int *buf)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->haptic_durations[i] = buf[i];

	return 0;
}
#else
static int dc_haptic_vib_get_haptic_intensities(struct device *dev, int *buf)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->haptic_durations[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->haptic_durations[i];

	return 0;
}

static int dc_haptic_vib_set_haptic_intensities(struct device *dev, int *buf)
{
	struct dc_haptic_vib_drvdata *ddata = dev_get_drvdata(dev);
	struct dc_haptic_vib_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->haptic_durations[i] = buf[i];

	return 0;
}
#endif /* CONFIG_USE_HAPTIC_DURATIONS */
#endif /* CONFIG_SEC_VIBRATOR */

static const struct sec_vibrator_ops dc_haptic_vib_ops = {
	.enable	= dc_haptic_vib_enable,
	.get_motor_type = dc_haptic_vib_get_motor_type,
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = dc_haptic_vib_get_calibration,
	.get_step_size = dc_haptic_vib_get_step_size,
#if defined(CONFIG_USE_HAPTIC_DURATIONS)
	.get_haptic_durations = dc_haptic_vib_get_haptic_durations,
	.set_haptic_durations = dc_haptic_vib_set_haptic_durations,
#else
	.get_intensities = dc_haptic_vib_get_haptic_intensities,
	.set_intensities = dc_haptic_vib_set_haptic_intensities,
	.get_haptic_intensities = dc_haptic_vib_get_haptic_intensities,
	.set_haptic_intensities = dc_haptic_vib_set_haptic_intensities,
#endif /* CONFIG_USE_HAPTIC_DURATIONS */
#endif /* CONFIG_SEC_VIBRATOR */
};

#if defined(CONFIG_SEC_VIBRATOR)
static int of_sec_vibrator_dt(struct dc_haptic_vib_pdata *pdata, struct device_node *np)
{
	int ret = 0;
	int i;
	unsigned int val = 0;
	int *durations = NULL;

	pr_info("%s\n", __func__);
	pdata->calibration = false;

	/* number of steps */
	ret = of_property_read_u32(np, "samsung,steps", &val);
	if (ret) {
		pr_err("%s out of range(%d)\n", __func__, val);
		return -EINVAL;
	}
	pdata->steps = (int)val;

	pr_info("%s step_size=%d\n", __func__, pdata->steps);

	/* allocate memory for haptic_durations */
	pdata->haptic_durations = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->haptic_durations) {
		ret = -ENOMEM;
		goto err_alloc_haptic;
	}
	durations = pdata->haptic_durations;

	/* haptic durations */
	ret = of_property_read_u32_array(np, "dc_haptic_vib,durations", durations, pdata->steps);
	if (ret) {
		pr_err("haptic_durations are not specified\n");
		ret = -EINVAL;
		goto err_haptic;
	}
	for (i = 0; i < pdata->steps; i++) {
		pr_info("%s haptic_durations[%d]: %d\n", __func__, pdata->steps, durations[i]);
		if ((durations[i] < 0) || (durations[i] > MAX_INTENSITY)) {
			pr_err("%s out of range(%d)\n", __func__, durations[i]);
			ret = -EINVAL;
			goto err_haptic;
		}
	}

	/* update calibration statue */
	pdata->calibration = true;

	return ret;

err_haptic:
	kfree(pdata->haptic_durations);
err_alloc_haptic:
	pdata->haptic_durations = NULL;
	pdata->steps = 0;

	return ret;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

#if defined(CONFIG_OF)
static struct dc_haptic_vib_pdata *dc_haptic_vib_get_dt(struct device *dev)
{
	struct device_node *node;
	struct dc_haptic_vib_pdata *pdata;
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->ldo_en_gpio = of_get_named_gpio(node, "dc_haptic_vib,ldo_en_gpio", 0);
	if (gpio_is_valid(pdata->ldo_en_gpio)) {
		ret = gpio_request(pdata->ldo_en_gpio, "vib_ldo_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->ldo_en_gpio, 0);
	} else {
		pr_info("%s: ldo en gpio isn't used\n", __func__);
	}

	pdata->haptic_en_gpio = of_get_named_gpio(node, "dc_haptic_vib,haptic_en_gpio", 0);
	if (gpio_is_valid(pdata->haptic_en_gpio)) {
		ret = gpio_request(pdata->haptic_en_gpio, "vib_haptic_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->haptic_en_gpio, 0);
	} else {
		pr_info("%s: haptic en gpio isn't used\n", __func__);
	}

	ret = of_property_read_string(node, "dc_haptic_vib,regulator_name",
			&pdata->regulator_name);
	if (!ret) {
		pdata->regulator = regulator_get(NULL, pdata->regulator_name);
		if (IS_ERR(pdata->regulator)) {
			ret = PTR_ERR(pdata->regulator);
			pdata->regulator = NULL;
			pr_err("%s: regulator get fail\n", __func__);
			goto err_out;
		}
	} else {
		pr_info("%s: regulator isn't used\n", __func__);
		pdata->regulator = NULL;
	}

	ret = of_property_read_string(node, "dc_haptic_vib,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	pdata->pwm = devm_of_pwm_get(dev, node, NULL);
	if (IS_ERR(pdata->pwm)) {
		pr_err("%s: error to get pwms\n", __func__);
		goto err_out;
	}

#if defined(CONFIG_SEC_VIBRATOR)
	ret = of_sec_vibrator_dt(pdata, node);
	if (ret < 0)
		pr_err("sec_vibrator dt read fail\n");
#endif

	return pdata;

err_out:
	return ERR_PTR(ret);
}
#endif

static int dc_haptic_vib_probe(struct platform_device *pdev)
{
	struct dc_haptic_vib_pdata *pdata = pdev->dev.platform_data;
	struct dc_haptic_vib_drvdata *ddata;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = dc_haptic_vib_get_dt(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			ret = -ENODEV;
			goto err_pdata;
		}
#else
		ret = -ENODEV;
		pr_err("there is no platform data!\n");
		goto err_pdata;
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(struct dc_haptic_vib_drvdata),
			GFP_KERNEL);
	if (!ddata) {
		ret = -ENOMEM;
		pr_err("Failed to memory alloc\n");
		goto err_ddata;
	}
	ddata->pdata = pdata;
	platform_set_drvdata(pdev, ddata);
	ddata->sec_vib_ddata.dev = &pdev->dev;
	ddata->sec_vib_ddata.vib_ops = &dc_haptic_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	hrtimer_init(&ddata->braking_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->braking_timer.function = braking_timer_func;

	pwm_disable(ddata->pdata->pwm);

	return 0;

err_ddata:
err_pdata:
	return ret;
}

static int dc_haptic_vib_remove(struct platform_device *pdev)
{
	struct dc_haptic_vib_drvdata *ddata = platform_get_drvdata(pdev);

	if (ddata->pdata->regulator)
		regulator_put(ddata->pdata->regulator);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id dc_haptic_vib_dt_ids[] = {
	{ .compatible = "samsung,dc_haptic_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, dc_haptic_vib_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver dc_haptic_vib_driver = {
	.probe		= dc_haptic_vib_probe,
	.remove		= dc_haptic_vib_remove,
	.driver		= {
		.name		= DC_HAPTIC_VIB_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(dc_haptic_vib_dt_ids),
	},
};

static int __init dc_haptic_vib_init(void)
{
	return platform_driver_register(&dc_haptic_vib_driver);
}
module_init(dc_haptic_vib_init);

static void __exit dc_haptic_vib_exit(void)
{
	platform_driver_unregister(&dc_haptic_vib_driver);
}
module_exit(dc_haptic_vib_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("dc haptic vibrator driver");
MODULE_LICENSE("GPL");
