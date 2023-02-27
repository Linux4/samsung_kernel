/* drivers/video/backlight/ktd2801_bl.c
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define KTD2801_BL_NAME		("kinetic,backlight-ktd2801")

#define IS_HBM(level)	(level >= 6)

static struct backlight_device *bl_dev;
static DEFINE_SPINLOCK(bl_ctrl_lock);

enum {
	BRT_VALUE_OFF = 0,
	BRT_VALUE_MIN,
	BRT_VALUE_DIM,
	BRT_VALUE_DEF,
	BRT_VALUE_MAX,
	MAX_BRT_VALUE_IDX,
};

struct brt_value {
	int brightness;	/* brightness level from user */
	int tune_level;	/* tuning value be sent */
};

struct brt_range {
	struct brt_value off;
	struct brt_value dim;
	struct brt_value min;
	struct brt_value def;
	struct brt_value max;
};

struct ktd2801_backlight_info {
	const char *name;
	bool enable;
	struct mutex ops_lock;
	struct brt_value range[MAX_BRT_VALUE_IDX];
	struct brt_value outdoor_value;
	unsigned int auto_brightness;
	int current_brightness;
	int prev_tune_level;
	int gpio_bl_ctrl;
};

#define EW_DELAY (150)
#define EW_DETECT (270)
#define EW_WINDOW (1000)
#define DATA_START (5)
#define LOW_BIT_H (5)
#define LOW_BIT_L (4 * HIGH_BIT_L)
#define HIGH_BIT_L (5)
#define HIGH_BIT_H (4 * HIGH_BIT_L)
#define END_DATA_L (10)
#define END_DATA_H (350)

int ktd2801_get_tune_level(struct ktd2801_backlight_info *bl_info,
		int brightness)
{
	int tune_level = 0, idx;
	struct brt_value *range = bl_info->range;

	if (unlikely(!range)) {
		pr_err("%s: brightness range not exist!\n", __func__);
		return -EINVAL;
	}

	if (brightness > range[BRT_VALUE_MAX].brightness ||
			brightness < 0) {
		pr_err("%s: out of range (%d)\n", __func__, brightness);
		return -EINVAL;
	}

	if (IS_HBM(bl_info->auto_brightness) &&
			brightness == range[BRT_VALUE_MAX].brightness)
		return bl_info->outdoor_value.tune_level;

	for (idx = 0; idx < MAX_BRT_VALUE_IDX; idx++)
		if (brightness <= range[idx].brightness)
			break;

	if (idx == MAX_BRT_VALUE_IDX) {
		pr_err("%s: out of brt_value table (%d)\n",
				__func__, brightness);
		return -EINVAL;
	}

	if (idx <= BRT_VALUE_MIN)
		tune_level = range[idx].tune_level;
	else
		tune_level = range[idx].tune_level -
			(range[idx].brightness - brightness) *
			(range[idx].tune_level - range[idx - 1].tune_level) /
			(range[idx].brightness - range[idx - 1].brightness);

	return tune_level;
}

void ktd2801_set_bl_level(struct ktd2801_backlight_info *bl_info, int level)
{
	unsigned long flags;
	int i, bl_ctrl = bl_info->gpio_bl_ctrl;

	pr_debug("%s, level(%d)\n", __func__, level);

	if (level == 0) {
		gpio_set_value(bl_ctrl, 0);
		usleep_range(10, 20);
		return;
	}

	spin_lock_irqsave(&bl_ctrl_lock, flags);
	if (bl_info->prev_tune_level == 0) {
		pr_debug("%s, set expresswire mode\n", __func__);
		/* Set ExpressWire Mode */
		gpio_set_value(bl_ctrl, 1);
		udelay(EW_DELAY);
		gpio_set_value(bl_ctrl, 0);
		udelay(EW_DETECT);
	}

	gpio_set_value(bl_ctrl, 1);
	udelay(DATA_START);
	for (i = 0; i < 8; i++) {
		if (level & (0x80 >> i)) {
			gpio_set_value(bl_ctrl, 0);
			udelay(HIGH_BIT_L);
			gpio_set_value(bl_ctrl, 1);
			udelay(HIGH_BIT_H);
		} else {
			gpio_set_value(bl_ctrl, 0);
			udelay(LOW_BIT_L);
			gpio_set_value(bl_ctrl, 1);
			udelay(LOW_BIT_H);
		}
	}
	gpio_set_value(bl_ctrl, 0);
	udelay(END_DATA_L);
	gpio_set_value(bl_ctrl, 1);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
	udelay(END_DATA_H);

	return;
}

static int ktd2801_backlight_set_brightness(
		struct ktd2801_backlight_info *bl_info, int brightness)
{
	int tune_level;
	tune_level = ktd2801_get_tune_level(bl_info, brightness);
	if (unlikely(tune_level < 0)) {
		pr_err("%s, failed to find tune_level. (%d)\n",
				__func__, brightness);
		return -EINVAL;
	}
	pr_info("%s: brightness(%d), tune_level(%d)\n",
			__func__, brightness, tune_level);
	mutex_lock(&bl_info->ops_lock);
	ktd2801_set_bl_level(bl_info, tune_level);
	bl_info->prev_tune_level = tune_level;
	mutex_unlock(&bl_info->ops_lock);

	return tune_level;
}

static int ktd2801_backlight_update_status(struct backlight_device *bd)
{
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (unlikely(!bl_info)) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	if (bd->props.power != FB_BLANK_UNBLANK ||
		bd->props.fb_blank != FB_BLANK_UNBLANK ||
		!bl_info->enable)
		brightness = 0;

	ktd2801_backlight_set_brightness(bl_info, brightness);

	return 0;
}

static int ktd2801_backlight_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops ktd2801_backlight_ops = {
	.update_status	= ktd2801_backlight_update_status,
	.get_brightness	= ktd2801_backlight_get_brightness,
};

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = to_backlight_device(dev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	return sprintf(buf, "auto_brightness : %d\n", bl_info->auto_brightness);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);
	unsigned long value;

	rc = kstrtoul(buf, 0, &value);
	if (rc)
		return rc;

	mutex_lock(&bl_info->ops_lock);
	if (bl_info->auto_brightness != (unsigned int)value) {
		pr_info("%s, auto_brightness : %u\n", __func__, value);
		bl_info->auto_brightness = (unsigned int)value;
	}
	mutex_unlock(&bl_info->ops_lock);
	backlight_update_status(bd);

	return count;
}

static DEVICE_ATTR(auto_brightness, 0644,
		auto_brightness_show, auto_brightness_store);

static void ktd2801_bl_early_suspend(struct early_suspend *h)
{
	struct backlight_device *bd = bl_dev;
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	bl_info->enable = false;
	backlight_update_status(bd);

	pr_info("ktd2801_backlight suspended\n");
}

static void ktd2801_bl_late_resume(struct early_suspend *h)
{
	struct backlight_device *bd = bl_dev;
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	bl_info->enable = true;
	backlight_update_status(bd);

	pr_info("ktd2801_backlight resumed.\n");
}

static struct early_suspend ktd2801_bl_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1,
	.suspend = ktd2801_bl_early_suspend,
	.resume = ktd2801_bl_late_resume,
};

static int ktd2801_parse_dt_gpio(struct device_node *np, const char *prop)
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

static int ktd2801_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct ktd2801_backlight_info *bl_info;
	int ret, gpio_bl_ctrl;
	bool outdoor_mode_en;

	pr_info("called %s\n", __func__);

	bl_info = devm_kzalloc(&pdev->dev,
			sizeof(*bl_info), GFP_KERNEL);
	if (unlikely(!bl_info))
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		int arr[MAX_BRT_VALUE_IDX * 2], i;

		ret = of_property_read_string(np,
				"backlight-name",
				&bl_info->name);

		gpio_bl_ctrl = ktd2801_parse_dt_gpio(np, "bl-ctrl");
		if (gpio_bl_ctrl < 0) {
			pr_err("%s, failed to parse dt\n", __func__);
			return -EINVAL;
		}

		outdoor_mode_en = of_property_read_bool(np,
				"gen-panel-outdoor-mode-en");
		if (outdoor_mode_en) {
			ret = of_property_read_u32_array(np,
					"backlight-brt-outdoor",arr, 2);
			bl_info->outdoor_value.brightness = arr[0];
			bl_info->outdoor_value.tune_level = arr[1];
		}

		ret = of_property_read_u32_array(np,
				"backlight-brt-range",
				arr, MAX_BRT_VALUE_IDX * 2);
		for (i = 0; i < MAX_BRT_VALUE_IDX; i++) {
			bl_info->range[i].brightness = arr[i * 2];
			bl_info->range[i].tune_level = arr[i * 2 + 1];
		}

		pr_info("backlight device : %s\n", bl_info->name);
		pr_info("[BRT_VALUE_OFF] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_OFF].brightness,
				bl_info->range[BRT_VALUE_OFF].tune_level);
		pr_info("[BRT_VALUE_MIN] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_MIN].brightness,
				bl_info->range[BRT_VALUE_MIN].tune_level);
		pr_info("[BRT_VALUE_DIM] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_DIM].brightness,
				bl_info->range[BRT_VALUE_DIM].tune_level);
		pr_info("[BRT_VALUE_DEF] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_DEF].brightness,
				bl_info->range[BRT_VALUE_DEF].tune_level);
		pr_info("[BRT_VALUE_MAX] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_MAX].brightness,
				bl_info->range[BRT_VALUE_MAX].tune_level);
	} else {
		if (unlikely(pdev->dev.platform_data == NULL)) {
			dev_err(&pdev->dev, "no platform data!\n");
			ret = -EINVAL;
		}
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = bl_info->range[BRT_VALUE_MAX].brightness;
	props.brightness = (u8)bl_info->range[BRT_VALUE_DEF].brightness;
	bl_info->current_brightness =
		(u8)bl_info->range[BRT_VALUE_DEF].brightness;
	bl_info->prev_tune_level =
		(u8)bl_info->range[BRT_VALUE_DEF].tune_level;

	bd = backlight_device_register(bl_info->name, &pdev->dev, bl_info,
			&ktd2801_backlight_ops, &props);
	if (IS_ERR(bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bd);
	}
	bl_dev = bd;

	if (gpio_bl_ctrl >= 0) {
		ret = gpio_request(gpio_bl_ctrl, "BL_CTRL");
		if (unlikely(ret < 0)) {
			pr_err("%s, request gpio(%d) failed\n",
					__func__, gpio_bl_ctrl);
			goto err_bl_gpio_request;
		}
		gpio_direction_output(gpio_bl_ctrl, 1);
		bl_info->gpio_bl_ctrl = gpio_bl_ctrl;
	}

	mutex_init(&bl_info->ops_lock);
	if (outdoor_mode_en) {
		ret = device_create_file(&bd->dev, &dev_attr_auto_brightness);
		if (unlikely(ret < 0)) {
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_auto_brightness.attr.name);
		}
	}
	bl_info->enable = true;
	pm_runtime_enable(&pdev->dev);
	platform_set_drvdata(pdev, bd);
	pm_runtime_get_sync(&pdev->dev);

	return 0;

err_bl_gpio_request:
	backlight_device_unregister(bd);
	devm_kfree(&pdev->dev, bl_info);

	return ret;
}

static int ktd2801_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	ktd2801_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
	backlight_device_unregister(bd);

	return 0;
}

void ktd2801_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}
	ktd2801_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id ktd2801_backlight_dt_match[] __initconst = {
	{ .compatible = KTD2801_BL_NAME },
	{},
};
#endif

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int ktd2801_backlight_runtime_suspend(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->enable = false;
	backlight_update_status(bd);

	dev_info(dev, "ktd2801_backlight suspended\n");
	return 0;
}

static int ktd2801_backlight_runtime_resume(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct ktd2801_backlight_info *bl_info =
		(struct ktd2801_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->enable = true;
	backlight_update_status(bd);

	dev_info(dev, "ktd2801_backlight resumed.\n");
	return 0;
}
#endif

const struct dev_pm_ops ktd2801_backlight_pm_ops = {
	SET_RUNTIME_PM_OPS(ktd2801_backlight_runtime_suspend,
		ktd2801_backlight_runtime_resume, NULL)
};

static struct platform_driver ktd2801_backlight_driver = {
	.driver		= {
		.name	= KTD2801_BL_NAME,
		.owner	= THIS_MODULE,
		.pm	= &ktd2801_backlight_pm_ops,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(ktd2801_backlight_dt_match),
#endif
	},
	.probe		= ktd2801_backlight_probe,
	.remove		= ktd2801_backlight_remove,
	.shutdown       = ktd2801_backlight_shutdown,
};

static int __init ktd2801_backlight_init(void)
{
	return platform_driver_register(&ktd2801_backlight_driver);
}
module_init(ktd2801_backlight_init);

static void __exit ktd2801_backlight_exit(void)
{
	platform_driver_unregister(&ktd2801_backlight_driver);
}
module_exit(ktd2801_backlight_exit);

MODULE_DESCRIPTION("KTD2801 PANEL BACKLIGHT Driver");
MODULE_LICENSE("GPL");
