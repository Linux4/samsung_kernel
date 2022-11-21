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
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/gen-panel-bl.h>

#define KTD3102_BL_NAME		("kinetic,backlight-ktd3102")
#define IS_HBM(level)	(level >= 6)

static DEFINE_SPINLOCK(bl_ctrl_lock);

struct ktd3102_backlight_info {
	struct gen_panel_backlight_info ginfo;
	int pin_ctrl;
	int pin_pwm;
};

int ktd3102_get_tune_level(struct ktd3102_backlight_info *bl_info,
		int brightness)
{
	struct gen_panel_backlight_info *ginfo = &bl_info->ginfo;
	struct brt_value *range = ginfo->range;
	int tune_level = 0, idx;

	if (unlikely(!range)) {
		pr_err("%s: brightness range not exist!\n", __func__);
		return -EINVAL;
	}

	if (brightness > range[BRT_VALUE_MAX].brightness ||
			brightness < 0) {
		pr_err("%s: out of range (%d)\n", __func__, brightness);
		return -EINVAL;
	}

	if (IS_HBM(ginfo->auto_brightness) &&
			brightness == range[BRT_VALUE_MAX].brightness)
		return ginfo->outdoor_value.tune_level;

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

void ktd3102_set_bl_level(struct ktd3102_backlight_info *bl_info, int level)
{
	struct gen_panel_backlight_info *ginfo = &bl_info->ginfo;
	unsigned long flags;
	int i, bl_ctrl = bl_info->pin_ctrl;
	int bl_pwm = bl_info->pin_pwm;
	int pulse;

	pr_debug("%s, level(%d)\n", __func__, level);

	if (level == 0) {
		/* power off */
		gpio_set_value(bl_ctrl, 0);
		gpio_set_value(bl_pwm, 0);
		mdelay(3);
		return;
	}

	if (ginfo->prev_tune_level == 0) {
		/* power on */
		gpio_set_value(bl_pwm, 1);
		gpio_set_value(bl_ctrl, 1);
		udelay(100);
	}

	pulse = ginfo->prev_tune_level - level;
	if (pulse < 0)
		pulse += 32;
	pr_info("%s: pre lev=%d, cur lev=%d, pulse=%d\n",
			__func__, ginfo->prev_tune_level, level, pulse);

	spin_lock_irqsave(&bl_ctrl_lock, flags);
	for(i = 0; i < pulse; i++) {
		udelay(2);
		gpio_set_value(bl_ctrl, 0);
		udelay(2);
		gpio_set_value(bl_ctrl, 1);
	}
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);

	return;
}

static int ktd3102_backlight_set_brightness(
		struct ktd3102_backlight_info *bl_info, int brightness)
{
	struct gen_panel_backlight_info *ginfo = &bl_info->ginfo;
	int tune_level;

	tune_level = ktd3102_get_tune_level(bl_info, brightness);
	if (unlikely(tune_level < 0)) {
		pr_err("%s, failed to find tune_level. (%d) tune_level(%d)\n",
				__func__, brightness, tune_level);
		return -EINVAL;
	}
	pr_info("[BACKLIGHT] %s: brightness(%d), tune_level(%d)\n",
			__func__, brightness, tune_level);
	mutex_lock(&ginfo->ops_lock);
	if (ginfo->prev_tune_level != tune_level) {
		ktd3102_set_bl_level(bl_info, tune_level);
		ginfo->prev_tune_level = tune_level;
	}
	mutex_unlock(&ginfo->ops_lock);

	return tune_level;
}

static int ktd3102_backlight_update_status(struct backlight_device *bd)
{
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (unlikely(!bl_info)) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	if (bd->props.power != FB_BLANK_UNBLANK ||
			bd->props.fb_blank != FB_BLANK_UNBLANK ||
			!bl_info->ginfo.enable)
		brightness = 0;

	ktd3102_backlight_set_brightness(bl_info, brightness);

	return 0;

}

static int ktd3102_backlight_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static struct backlight_ops ktd3102_backlight_ops = {
	.update_status	= ktd3102_backlight_update_status,
	.get_brightness	= ktd3102_backlight_get_brightness,
};

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = to_backlight_device(dev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);

	return sprintf(buf, "auto_brightness : %d\n", bl_info->ginfo.auto_brightness);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);
	struct gen_panel_backlight_info *ginfo = &bl_info->ginfo;
	unsigned long value;

	rc = kstrtoul(buf, 0, &value);
	if (rc)
		return rc;

	mutex_lock(&ginfo->ops_lock);
	if (ginfo->auto_brightness != (unsigned int)value) {
		pr_info("%s, auto_brightness : %u\n",
				__func__, (unsigned int)value);
		ginfo->auto_brightness = (unsigned int)value;
	}
	mutex_unlock(&ginfo->ops_lock);
	backlight_update_status(bd);

	return count;
}
static DEVICE_ATTR(auto_brightness, 0644,
		auto_brightness_show, auto_brightness_store);

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

static void ktd3102_backlight_dump(struct ktd3102_backlight_info *info)
{
	struct gen_panel_backlight_info *ginfo = &info->ginfo;

	pr_info("KTD3102 backlight device : %s\n", ginfo->name);
	pr_info("pin num: ctrl=%d, pwm=%d\n", info->pin_ctrl, info->pin_pwm);
	pr_info("[BRT_VALUE_OFF] brightness(%d), tune_level(%d)\n",
			ginfo->range[BRT_VALUE_OFF].brightness,
			ginfo->range[BRT_VALUE_OFF].tune_level);
	pr_info("[BRT_VALUE_MIN] brightness(%d), tune_level(%d)\n",
			ginfo->range[BRT_VALUE_MIN].brightness,
			ginfo->range[BRT_VALUE_MIN].tune_level);
	pr_info("[BRT_VALUE_DIM] brightness(%d), tune_level(%d)\n",
			ginfo->range[BRT_VALUE_DIM].brightness,
			ginfo->range[BRT_VALUE_DIM].tune_level);
	pr_info("[BRT_VALUE_DEF] brightness(%d), tune_level(%d)\n",
			ginfo->range[BRT_VALUE_DEF].brightness,
			ginfo->range[BRT_VALUE_DEF].tune_level);
	pr_info("[BRT_VALUE_MAX] brightness(%d), tune_level(%d)\n",
			ginfo->range[BRT_VALUE_MAX].brightness,
			ginfo->range[BRT_VALUE_MAX].tune_level);
}

static int ktd3102_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct ktd3102_backlight_info *bl_info;
	struct gen_panel_backlight_info *ginfo;
	int ret;
	bool outdoor_mode_en;

	bl_info = devm_kzalloc(&pdev->dev, sizeof(*bl_info), GFP_KERNEL);
	if (unlikely(!bl_info))
		return -ENOMEM;
	ginfo = &bl_info->ginfo;

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		struct device_node *brt_node;
		int arr[MAX_BRT_VALUE_IDX * 2], i;

		ret = of_property_read_string(np, "backlight-name",
				&ginfo->name);

		bl_info->pin_ctrl = ktd3102_parse_dt_gpio(np, "bl-ctrl");
		if (bl_info->pin_ctrl < 0) {
			pr_err("%s, failed to parse dt\n", __func__);
			return -EINVAL;
		}
		/* PWM pin isn't mandatory */
		bl_info->pin_pwm = ktd3102_parse_dt_gpio(np, "bl-pwm");
		pr_info("%s: ctrl=%d, pwm=%d\n",
				__func__, bl_info->pin_ctrl, bl_info->pin_pwm);

		/* get default brightness table */
		brt_node = of_get_next_child(np, NULL);
		outdoor_mode_en = of_property_read_bool(brt_node,
				"gen-panel-outdoor-mode-en");
		if (outdoor_mode_en) {
			ret = of_property_read_u32_array(brt_node,
					"backlight-brt-outdoor", arr, 2);
			ginfo->outdoor_value.brightness = arr[0];
			ginfo->outdoor_value.tune_level = arr[1];
		}

		ret = of_property_read_u32_array(brt_node,
				"backlight-brt-range",
				arr, MAX_BRT_VALUE_IDX * 2);
		for (i = 0; i < MAX_BRT_VALUE_IDX; i++) {
			ginfo->range[i].brightness = arr[i * 2];
			ginfo->range[i].tune_level = arr[i * 2 + 1];
		}
	} else {
		if (unlikely(pdev->dev.platform_data == NULL)) {
			dev_err(&pdev->dev, "no platform data!\n");
			return -EINVAL;
		}
		/* TODO: fill bl_info data */
	}
	ktd3102_backlight_dump(bl_info);

	ret = gpio_request(bl_info->pin_ctrl, "BL_CTRL");
	if (unlikely(ret < 0)) {
		pr_err("%s, request gpio(%d) failed\n", __func__, bl_info->pin_ctrl);
		goto err_bl_gpio_request;
	}

	ret = gpio_request(bl_info->pin_pwm, "BL_PWM");
	if (unlikely(ret < 0)) {
		pr_err("%s, request gpio(%d) failed\n", __func__, bl_info->pin_pwm);
		goto err_bl_gpio_request;
	}

	mutex_init(&ginfo->ops_lock);

	/* register backlight device */
	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = ginfo->range[BRT_VALUE_MAX].brightness;
	props.brightness = ginfo->range[BRT_VALUE_DEF].brightness;
	ginfo->current_brightness = ginfo->range[BRT_VALUE_DEF].brightness;
	ginfo->prev_tune_level = ginfo->range[BRT_VALUE_DEF].tune_level;
	pr_info("%s: cur brt=%d, prev tune lev=%d\n", __func__,
			ginfo->current_brightness, ginfo->prev_tune_level);

	bd = backlight_device_register(ginfo->name, &pdev->dev, bl_info,
			&ktd3102_backlight_ops, &props);
	if (IS_ERR(bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bd);
	}

	if (outdoor_mode_en) {
		ret = device_create_file(&bd->dev, &dev_attr_auto_brightness);
		if (unlikely(ret < 0)) {
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_auto_brightness.attr.name);
		}
	}
	ginfo->enable = true;

	pm_runtime_enable(&pdev->dev);
	platform_set_drvdata(pdev, bd);
	pm_runtime_get_sync(&pdev->dev);

	return 0;

err_bl_gpio_request:
	devm_kfree(&pdev->dev, bl_info);

	return ret;
}

static int ktd3102_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	ktd3102_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
	backlight_device_unregister(bd);

	return 0;
}

static void ktd3102_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}
	ktd3102_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = KTD3102_BL_NAME },
	{ }
};

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int ktd3102_backlight_runtime_suspend(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->ginfo.enable = false;
	backlight_update_status(bd);

	dev_info(dev, "ktd3102_backlight suspended\n");
	return 0;
}

static int ktd3102_backlight_runtime_resume(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct ktd3102_backlight_info *bl_info =
		(struct ktd3102_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->ginfo.enable = true;
	backlight_update_status(bd);

	dev_info(dev, "ktd3102_backlight resumed.\n");
	return 0;
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
