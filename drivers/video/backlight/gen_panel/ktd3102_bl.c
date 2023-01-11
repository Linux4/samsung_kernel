/* drivers/video/backlight/ktd3102_bl.c
 *
 * Copyright (C) 2015 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/gen-panel-bl.h>

#define KTD3102_BL_NAME		("kinetic,backlight-ktd3102")
#define IS_HBM(level)	(level >= 6)

static DEFINE_SPINLOCK(bl_ctrl_lock);

struct ktd3102_backlight_info {
	int pin_ctrl;
	int pin_pwm;
	int prev_tune_level;
};

int ktd3102_set_bl_level(void *prvinfo, int level)
{
	struct ktd3102_backlight_info *info =
		(struct ktd3102_backlight_info *)prvinfo;
	unsigned long flags;
	int i, bl_ctrl = info->pin_ctrl;
	int bl_pwm = info->pin_pwm;
	int pulse;

	pr_debug("%s, level(%d)\n", __func__, level);

	if (level == 0) {
		/* power off */
		gpio_set_value(bl_ctrl, 0);
		gpio_set_value(bl_pwm, 0);
		mdelay(3);
		goto fin;
	}

	if (info->prev_tune_level == 0) {
		/* power on */
		gpio_set_value(bl_pwm, 1);
		gpio_set_value(bl_ctrl, 1);
		udelay(100);
	}

	pulse = info->prev_tune_level - level;
	if (pulse < 0)
		pulse += 32;
	pr_info("%s: pre lev=%d, cur lev=%d, pulse=%d\n",
			__func__, info->prev_tune_level, level, pulse);

	spin_lock_irqsave(&bl_ctrl_lock, flags);
	for(i = 0; i < pulse; i++) {
		udelay(2);
		gpio_set_value(bl_ctrl, 0);
		udelay(2);
		gpio_set_value(bl_ctrl, 1);
	}
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);

fin:
	info->prev_tune_level = level;

	return 0;
}

static const struct gen_panel_backlight_ops ktd3102_gen_backlight_ops = {
	.set_brightness = ktd3102_set_bl_level,
	.get_brightness = NULL,
};

static int ktd3102_parse_dt_gpio(struct device_node *np, const char *prop)
{
	int gpio;

	gpio = of_get_named_gpio(np, prop, 0);
	if (unlikely(gpio < 0)) {
		pr_err("%s: of_get_named_gpio failed: %d\n",
				__func__, gpio);
		return -EINVAL;
	}

	pr_info("%s, get gpio(%d)\n", __func__, gpio);

	return gpio;
}

static int ktd3102_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct ktd3102_backlight_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (unlikely(!info))
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		struct device_node *brt_node;
		int arr[MAX_BRT_VALUE_IDX * 2], i;

		info->pin_ctrl = ktd3102_parse_dt_gpio(np, "bl-ctrl");
		if (info->pin_ctrl < 0) {
			pr_err("%s, failed to parse dt\n", __func__);
			return -EINVAL;
		}
		/* PWM pin isn't mandatory */
		info->pin_pwm = ktd3102_parse_dt_gpio(np, "bl-pwm");
		pr_info("%s: ctrl=%d, pwm=%d\n",
				__func__, info->pin_ctrl, info->pin_pwm);
	} else {
		if (unlikely(pdev->dev.platform_data == NULL)) {
			dev_err(&pdev->dev, "no platform data!\n");
			return -EINVAL;
		}
		/* TODO: fill info data */
	}

	ret = gpio_request(info->pin_ctrl, "BL_CTRL");
	if (unlikely(ret < 0)) {
		pr_err("%s, request gpio(%d) failed\n", __func__, info->pin_ctrl);
		goto err_bl_gpio_request;
	}

	ret = gpio_request(info->pin_pwm, "BL_PWM");
	if (unlikely(ret < 0)) {
		pr_err("%s, request gpio(%d) failed\n", __func__, info->pin_pwm);
		goto err_bl_gpio_request;
	}

	/* register gen backlight device */
	bd = gen_panel_backlight_device_register(pdev, info,
			&ktd3102_gen_backlight_ops, &info->prev_tune_level);
	if (!bd) {
		dev_err(&pdev->dev, "failed to register gen backlight\n");
		ret = -1;
		goto err_gen_bl_register;
	}

	pm_runtime_enable(&pdev->dev);
	platform_set_drvdata(pdev, bd);
	pm_runtime_get_sync(&pdev->dev);

	return 0;

err_gen_bl_register:
err_bl_gpio_request:
	devm_kfree(&pdev->dev, info);

	return ret;
}

static int ktd3102_backlight_remove(struct platform_device *pdev)
{
	return gen_panel_backlight_remove(pdev);
}

static void ktd3102_backlight_shutdown(struct platform_device *pdev)
{
	gen_panel_backlight_shutdown(pdev);
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = KTD3102_BL_NAME },
	{ }
};

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int ktd3102_backlight_runtime_suspend(struct device *dev)
{
	return gen_panel_backlight_runtime_suspend(dev);
}

static int ktd3102_backlight_runtime_resume(struct device *dev)
{
	return gen_panel_backlight_runtime_resume(dev);
}

const struct dev_pm_ops ktd3102_backlight_pm_ops = {
	SET_RUNTIME_PM_OPS(ktd3102_backlight_runtime_suspend,
			ktd3102_backlight_runtime_resume, NULL)
};
#endif

static struct platform_driver ktd3102_backlight_driver = {
	.driver		= {
		.name	= KTD3102_BL_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
		.pm	= &ktd3102_backlight_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = backlight_of_match,
#endif
	},
	.probe		= ktd3102_backlight_probe,
	.remove		= ktd3102_backlight_remove,
	.shutdown      = ktd3102_backlight_shutdown,
};

static int __init ktd3102_backlight_init(void)
{
	return platform_driver_register(&ktd3102_backlight_driver);
}
module_init(ktd3102_backlight_init);

static void __exit ktd3102_backlight_exit(void)
{
	platform_driver_unregister(&ktd3102_backlight_driver);
}
module_exit(ktd3102_backlight_exit);

MODULE_DESCRIPTION("ktd3102 based Backlight Driver");
MODULE_LICENSE("GPL");
