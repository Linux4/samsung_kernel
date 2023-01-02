/* drivers/video/backlight/gen_panel/gen-panel-bl.c
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
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_data/gen-panel.h>
#include <linux/platform_data/gen-panel-bl.h>
#include <linux/earlysuspend.h>

static struct backlight_device *bl_dev;

int gen_panel_attach_backlight(struct lcd *lcd,
		const struct gen_panel_backlight_ops *ops)
{
	struct gen_panel_backlight_info *bl_info;
	struct backlight_device *bd = lcd->bd;

	if (!bd) {
		pr_err("%s, no backlight device\n", __func__);
		return -ENODEV;
	}

	bl_info = (struct gen_panel_backlight_info *)bl_get_data(lcd->bd);
	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->lcd = lcd;
	bl_info->ops = ops;
	backlight_update_status(lcd->bd);
	pr_info("%s: done\n", __func__);

	return 0;
}
EXPORT_SYMBOL(gen_panel_attach_backlight);

void gen_panel_detach_backlight(struct lcd *lcd)
{
	struct gen_panel_backlight_info *bl_info;

	if (!lcd->bd) {
		pr_err("%s, no backlight device\n", __func__);
		return;
	}

	bl_info = (struct gen_panel_backlight_info *)bl_get_data(lcd->bd);
	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	mutex_lock(&bl_info->ops_lock);
	bl_info->ops = NULL;
	mutex_unlock(&bl_info->ops_lock);
	return;
}
EXPORT_SYMBOL(gen_panel_detach_backlight);

bool gen_panel_match_backlight(struct backlight_device *bd,
		const char *match)
{
	struct platform_device *pdev;

	if (!bd || !bd->dev.parent) {
		pr_err("%s, no backlight device\n", __func__);
		return false;
	}

	if (!match) {
		pr_err("%s, invalid parameter\n", __func__);
		return false;
	}

	pdev = to_platform_device(bd->dev.parent);
	if (!pdev) {
		pr_err("%s, platform device not exist\n", __func__);
		return false;
	}

	if (!pdev->name) {
		pr_err("%s, device name not exist\n", __func__);
		return false;
	}

	if (!strncmp(pdev->name, match, strlen(match))) {
		pr_info("%s, %s, %s match\n", __func__, pdev->name, match);
		return true;
	}

	pr_info("%s, %s, %s not match\n", __func__, pdev->name, match);
	return false;
}
EXPORT_SYMBOL(gen_panel_match_backlight);

int gen_panel_get_tune_level(struct gen_panel_backlight_info *bl_info,
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

static int gen_panel_backlight_set_brightness(
		struct gen_panel_backlight_info *bl_info, int brightness)
{
	int tune_level;
	tune_level = gen_panel_get_tune_level(bl_info, brightness);
	if (unlikely(tune_level < 0)) {
		pr_err("%s, failed to find tune_level. (%d)\n",
				__func__, brightness);
		return -EINVAL;
	}

	mutex_lock(&bl_info->ops_lock);
	if (bl_info->prev_tune_level == tune_level) {
		pr_debug("%s, already set tune_level(%d)\n",
				__func__, tune_level);
		mutex_unlock(&bl_info->ops_lock);
		return tune_level;
	}
	pr_info("%s: brightness(%d), tune_level(%d)\n",
			__func__, brightness, tune_level);
	if (bl_info->lcd && bl_info->ops &&
			bl_info->ops->set_brightness)
		bl_info->ops->set_brightness(bl_info->lcd, tune_level);
	bl_info->prev_tune_level = tune_level;
	mutex_unlock(&bl_info->ops_lock);

	return tune_level;
}

static int gen_panel_backlight_update_status(struct backlight_device *bd)
{
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (unlikely(!bl_info)) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	if (bd->props.power != FB_BLANK_UNBLANK ||
		bd->props.fb_blank != FB_BLANK_UNBLANK ||
		!bl_info->enable)
		brightness = 0;

	gen_panel_backlight_set_brightness(bl_info, brightness);

	return 0;
}

static int gen_panel_backlight_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static ssize_t auto_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = to_backlight_device(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	return sprintf(buf, "auto_brightness : %d\n", bl_info->auto_brightness);
}

static ssize_t auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);
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

static const struct backlight_ops gen_panel_backlight_ops = {
	.update_status	= gen_panel_backlight_update_status,
	.get_brightness	= gen_panel_backlight_get_brightness,
};

static void gen_panel_bl_early_suspend(struct early_suspend *h)
{
	struct backlight_device *bd = bl_dev;
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	bl_info->enable = false;
	backlight_update_status(bd);

	pr_info("gen_panel_backlight suspended\n");
}

static void gen_panel_bl_late_resume(struct early_suspend *h)
{
	struct backlight_device *bd = bl_dev;
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	bl_info->enable = true;
	backlight_update_status(bd);

	pr_info("gen_panel_backlight resumed.\n");
}

static struct early_suspend gen_panel_bl_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1,
	.suspend = gen_panel_bl_early_suspend,
	.resume = gen_panel_bl_late_resume,
};


static int gen_panel_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct gen_panel_backlight_info *bl_info;
	int ret;
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
				"gen-panel-backlight-name",
				&bl_info->name);

		outdoor_mode_en = of_property_read_bool(np,
				"gen-panel-outdoor-mode-en");
		if (outdoor_mode_en) {
			ret = of_property_read_u32_array(np,
					"backlight-brt-outdoor",arr, 2);
			bl_info->outdoor_value.brightness = arr[0];
			bl_info->outdoor_value.tune_level = arr[1];
		}
		ret = of_property_read_u32_array(np,
				"gen-panel-backlight-brt-range",
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

	bd = backlight_device_register(bl_info->name, &pdev->dev, bl_info,
			&gen_panel_backlight_ops, &props);
	if (IS_ERR(bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bd);
	}
	bl_dev = bd;

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

}

static int gen_panel_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	gen_panel_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
	backlight_device_unregister(bd);

	return 0;
}

void gen_panel_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}
	gen_panel_backlight_set_brightness(bl_info, 0);
	pm_runtime_disable(&pdev->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id gen_panel_backlight_dt_match[] __initconst = {
	{ .compatible = GEN_PANEL_BL_NAME },
	{},
};
#endif

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int gen_panel_backlight_runtime_suspend(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->enable = false;
	backlight_update_status(bd);

	dev_info(dev, "gen_panel_backlight suspended\n");
	return 0;
}

static int gen_panel_backlight_runtime_resume(struct device *dev)
{
	struct backlight_device *bd = dev_get_drvdata(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return -EINVAL;
	}

	bl_info->enable = true;
	backlight_update_status(bd);

	dev_info(dev, "gen_panel_backlight resumed.\n");
	return 0;
}
#endif

const struct dev_pm_ops gen_panel_backlight_pm_ops = {
	SET_RUNTIME_PM_OPS(gen_panel_backlight_runtime_suspend,
		gen_panel_backlight_runtime_resume, NULL)
};

static struct platform_driver gen_panel_backlight_driver = {
	.driver		= {
		.name	= GEN_PANEL_BL_NAME,
		.owner	= THIS_MODULE,
		.pm	= &gen_panel_backlight_pm_ops,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(gen_panel_backlight_dt_match),
#endif
	},
	.probe		= gen_panel_backlight_probe,
	.remove		= gen_panel_backlight_remove,
	.shutdown       = gen_panel_backlight_shutdown,
};

static int __init gen_panel_backlight_init(void)
{
	return platform_driver_register(&gen_panel_backlight_driver);
}
module_init(gen_panel_backlight_init);

static void __exit gen_panel_backlight_exit(void)
{
	platform_driver_unregister(&gen_panel_backlight_driver);
}
module_exit(gen_panel_backlight_exit);

MODULE_DESCRIPTION("GENERIC PANEL BACKLIGHT Driver");
MODULE_LICENSE("GPL");
