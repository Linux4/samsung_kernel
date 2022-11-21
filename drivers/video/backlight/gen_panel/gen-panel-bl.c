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

void print_brt_range(struct gen_panel_backlight_info *bl_info)
{
	pr_debug("[BRT_VALUE_OFF] brightness(%d), tune_level(%d)\n",
			bl_info->range[BRT_VALUE_OFF].brightness,
			bl_info->range[BRT_VALUE_OFF].tune_level);
	pr_debug("[BRT_VALUE_MIN] brightness(%d), tune_level(%d)\n",
			bl_info->range[BRT_VALUE_MIN].brightness,
			bl_info->range[BRT_VALUE_MIN].tune_level);
	pr_debug("[BRT_VALUE_DIM] brightness(%d), tune_level(%d)\n",
			bl_info->range[BRT_VALUE_DIM].brightness,
			bl_info->range[BRT_VALUE_DIM].tune_level);
	pr_debug("[BRT_VALUE_DEF] brightness(%d), tune_level(%d)\n",
			bl_info->range[BRT_VALUE_DEF].brightness,
			bl_info->range[BRT_VALUE_DEF].tune_level);
	pr_debug("[BRT_VALUE_MAX] brightness(%d), tune_level(%d)\n",
			bl_info->range[BRT_VALUE_MAX].brightness,
			bl_info->range[BRT_VALUE_MAX].tune_level);
	if (bl_info->hbm_en)
		pr_debug("[BRT_VALUE_HBM] brightness(%d), tune_level(%d)\n",
				bl_info->range[BRT_VALUE_HBM].brightness,
				bl_info->range[BRT_VALUE_HBM].tune_level);
}

int gen_panel_get_tune_level(struct gen_panel_backlight_info *bl_info,
		int brightness)
{
	int tune_level = 0, idx;
	struct brt_value *range = bl_info->range;

	if (unlikely(!range)) {
		pr_err("%s: brightness range not exist!\n", __func__);
		return -EINVAL;
	}
	if (IS_HBM(bl_info->auto_brightness) &&
			brightness == range[BRT_VALUE_HBM].brightness)
		return bl_info->range[BRT_VALUE_HBM].tune_level;

	if (brightness > range[BRT_VALUE_MAX].brightness ||
			brightness < 0) {
		pr_err("%s: out of range (%d)\n", __func__, brightness);
		return -EINVAL;
	}

	for (idx = 0; idx < (BRT_VALUE_MAX + 1); idx++)
		if (brightness <= range[idx].brightness)
			break;

	if (idx == (BRT_VALUE_MAX + 1)) {
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

int gen_panel_get_tune_level_from_maptbl(struct gen_panel_backlight_info *bl_info,
		int brightness)
{
	int tune_level = 0, idx;
	struct brt_value *maptbl = (struct brt_value *)bl_info->maptbl;
	unsigned int nr_table = bl_info->nr_maptbl;

	if (unlikely(!maptbl)) {
		pr_err("[BACKLIGHT] %s: brightness mapping not exist!\n", __func__);
		return -EINVAL;
	}

	if (brightness > maptbl[nr_table - 1].brightness ||
			brightness < 0) {
		pr_err("%[BACKLIGHT] s: out of range (%d)\n", __func__, brightness);
		return -EINVAL;
	}

	if (IS_HBM(bl_info->auto_brightness))
		return maptbl[nr_table - 1].tune_level;

	for (idx = 0; idx < nr_table; idx++)
		if (brightness <= maptbl[idx].brightness)
			break;

	if (idx == nr_table) {
		pr_err("[BACKLIGHT] %s: out of mapping table(%d)\n",
				__func__, brightness);
		return -EINVAL;
	}

	return maptbl[idx].tune_level;
}

static int gen_panel_backlight_set_brightness(struct backlight_device *bd,
		int brightness)
{
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);
	int tune_level;

	if (unlikely(!bl_info)) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	if (bl_info->nr_maptbl)
		tune_level =
			gen_panel_get_tune_level_from_maptbl(bl_info,
					brightness);
	else
		tune_level = gen_panel_get_tune_level(bl_info, brightness);
	if (unlikely(tune_level < 0)) {
		pr_err("%s, failed to find tune_level. (%d)\n",
				__func__, brightness);
		return -EINVAL;
	}
	pr_info("%s: brightness(%d), tune_level(%d)\n",
			__func__, brightness, tune_level);

	mutex_lock(&bl_info->ops_lock);
/* In case of OCTA AMOLED ,even if prev_tune_level = tune_level , then also
we need to invoke gen_panel_set_brightness.Hence skipping the check for OCTA panels */
#ifdef CONFIG_GEN_PANEL_OCTA
	if (bl_info->ops && bl_info->ops->set_brightness) {
		bl_info->ops->set_brightness(bl_info->prvinfo, tune_level);
		bl_info->prev_tune_level = tune_level;
	}
#else
	if (bl_info->ops && bl_info->ops->set_brightness &&
			bl_info->prev_tune_level != tune_level) {
		bl_info->ops->set_brightness(bl_info->prvinfo, tune_level);
		bl_info->prev_tune_level = tune_level;
	}
#endif
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

	gen_panel_backlight_set_brightness(bd, brightness);

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
		pr_info("%s, auto_brightness : %u\n",
				__func__, (unsigned int)value);
		bl_info->auto_brightness = (unsigned int)value;
	}
	mutex_unlock(&bl_info->ops_lock);
	backlight_update_status(bd);

	return count;
}
static DEVICE_ATTR(auto_brightness, 0644,
		auto_brightness_show, auto_brightness_store);

static ssize_t tune_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct backlight_device *bd = to_backlight_device(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);
	int len = 0;

	len += sprintf(buf + len, "[MIN] %d --> %d\n",
			bl_info->range[BRT_VALUE_MIN].brightness,
			bl_info->range[BRT_VALUE_MIN].tune_level);
	len += sprintf(buf + len, "[DIM] %d --> %d\n",
			bl_info->range[BRT_VALUE_DIM].brightness,
			bl_info->range[BRT_VALUE_DIM].tune_level);
	len += sprintf(buf + len, "[DEF] %d --> %d\n",
			bl_info->range[BRT_VALUE_DEF].brightness,
			bl_info->range[BRT_VALUE_DEF].tune_level);
	len += sprintf(buf + len, "[MAX] %d --> %d\n",
			bl_info->range[BRT_VALUE_MAX].brightness,
			bl_info->range[BRT_VALUE_MAX].tune_level);
	if (bl_info->hbm_en)
		len += sprintf(buf + len, "[HBM] %d --> %d\n",
				bl_info->range[BRT_VALUE_HBM].brightness,
				bl_info->range[BRT_VALUE_HBM].tune_level);
	return len;
}

static ssize_t tune_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);
	int ret, min, dim, def, max, hbm;

	if (bl_info->hbm_en) {
		ret = sscanf(buf, "%d %d %d %d %d",
				&min, &dim, &def, &max, &hbm);
		if (ret != 5) {
			pr_warn("%s, invalid parameter\n", __func__);
			return count;
		}
	} else {
		ret = sscanf(buf, "%d %d %d %d",
				&min, &dim, &def, &max);
		if (ret != 4) {
			pr_warn("%s, invalid parameter\n", __func__);
			return count;
		}
	}

	mutex_lock(&bl_info->ops_lock);
	bl_info->range[BRT_VALUE_MIN].tune_level = min;
	bl_info->range[BRT_VALUE_DIM].tune_level  = dim;
	bl_info->range[BRT_VALUE_DEF].tune_level  = def;
	bl_info->range[BRT_VALUE_MAX].tune_level  = max;
	if (bl_info->hbm_en)
		bl_info->range[BRT_VALUE_HBM].tune_level  = hbm;
	print_brt_range(bl_info);
	mutex_unlock(&bl_info->ops_lock);
	backlight_update_status(bd);

	return count;
}
static DEVICE_ATTR(tune_brightness, 0644,
		tune_brightness_show, tune_brightness_store);

static const struct backlight_ops gen_panel_backlight_ops = {
	.update_status	= gen_panel_backlight_update_status,
	.get_brightness	= gen_panel_backlight_get_brightness,
};

extern u32 boot_panel_id; /* panel ID from commandline */
struct backlight_device *gen_panel_backlight_device_register(
		struct platform_device *pdev, void *prvinfo,
		const struct gen_panel_backlight_ops *ops,
		int *def_lev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct gen_panel_backlight_info *bl_info;
	u32 panel_id;
	bool outdoor_mode_en;
	bool found_brt;
	int ret;

	bl_info = devm_kzalloc(&pdev->dev,
			sizeof(*bl_info), GFP_KERNEL);
	if (unlikely(!bl_info)) {
		dev_err(&pdev->dev, "fail to allocate for bl_info\n");
		return NULL;
	}

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		struct device_node *cnp;
		int arr[MAX_BRT_VALUE_IDX * 2], i, sz, nr_range;

		ret = of_property_read_string(np,
				"backlight-name",
				&bl_info->name);
		bl_info->hbm_en = of_property_read_bool(np,
				"gen-panel-bl-hbm-en");
		bl_info->tune_en = of_property_read_bool(np,
				"gen-panel-bl-tune-en");

		/* With one board, different brightness tables are required
		   for each panels.
		   1. get the panel ID from each brightness table child node.
		   2. if it matches with current panel ID, updates brightness table.
		 */
		found_brt = false;
		for_each_child_of_node(np, cnp) {
			of_property_read_u32(cnp, "brt-panel-id", &panel_id);
			if (panel_id == boot_panel_id) {
				pr_debug("%s: found: panel_id=0x%x, update brt table\n",
						__func__, panel_id);
				found_brt = true;
				break;
			}
		}

		if (!found_brt) {
			dev_info(&pdev->dev, "no child node of brightness table\n");
			cnp = np;
		}

		/* outdoor brightness table */
		outdoor_mode_en = of_property_read_bool(cnp,
				"gen-panel-outdoor-mode-en");
		if (outdoor_mode_en) {
			of_property_read_u32_array(cnp,
					"backlight-brt-outdoor",
					arr, 2);
			bl_info->outdoor_value.brightness = arr[0];
			bl_info->outdoor_value.tune_level = arr[1];
			pr_debug("%s: outdoor brt={%d, %d}\n",
					__func__, arr[0], arr[1]);
		}

		/* brightness table */
		of_property_read_u32_array(cnp,
				"backlight-brt-range",
				arr, MAX_BRT_VALUE_IDX * 2);
		for (i = 0; i < MAX_BRT_VALUE_IDX; i++) {
			bl_info->range[i].brightness = arr[i * 2];
			bl_info->range[i].tune_level = arr[i * 2 + 1];
			pr_debug("%s: brt[%d]={%d, %d}\n", __func__,
					i, arr[i * 2], arr[i * 2 + 1]);
		}

		if (of_find_property(np, "backlight-brt-map", &sz)) {
			nr_range = (sz / sizeof(u32)) / 2;
			if (sz % 2) {
				pr_err("%s, invalid brt-range\n", __func__);
				return -EINVAL;
			}

			bl_info->maptbl =
				kzalloc(sizeof(struct brt_value) * nr_range, GFP_KERNEL);
			if (of_property_read_u32_array(np,
						"backlight-brt-map",
						bl_info->maptbl, nr_range * 2)) {
				pr_err("%s, failed to parse table\n", __func__);
				kfree(bl_info->maptbl);
				return -ENODATA;
			}
			bl_info->nr_maptbl = nr_range;
		}

		print_brt_range(bl_info);
		pr_debug("backlight device : %s\n", bl_info->name);
	} else {
		if (unlikely(pdev->dev.platform_data == NULL)) {
			dev_err(&pdev->dev, "no platform data!\n");
			return NULL;
		}
		/* TODO: fill info data */
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = bl_info->range[BRT_VALUE_MAX].brightness;
	props.brightness = (u8)bl_info->range[BRT_VALUE_DEF].brightness;
	bl_info->current_brightness =
		(u8)bl_info->range[BRT_VALUE_DEF].brightness;
	bl_info->prev_tune_level =
		(u8)bl_info->range[BRT_VALUE_DEF].tune_level;
	if (def_lev)
		*def_lev = bl_info->prev_tune_level;
	bl_info->ops = ops;
	bl_info->prvinfo = prvinfo;

	bd = backlight_device_register(bl_info->name, &pdev->dev, bl_info,
			&gen_panel_backlight_ops, &props);
	if (IS_ERR(bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return NULL;
	}

	mutex_init(&bl_info->ops_lock);
	if (bl_info->hbm_en) {
		ret = device_create_file(&bd->dev, &dev_attr_auto_brightness);
		if (unlikely(ret < 0))
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_auto_brightness.attr.name);
	}
	if (bl_info->tune_en) {
		ret = device_create_file(&bd->dev, &dev_attr_tune_brightness);
		if (unlikely(ret < 0))
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_tune_brightness.attr.name);
	}

	bl_info->enable = true;

	return bd;
}
EXPORT_SYMBOL(gen_panel_backlight_device_register);

void gen_panel_backlight_device_unregister(struct backlight_device *bd)
{
	struct gen_panel_backlight_info *bl_info =
		(struct gen_panel_backlight_info *)bl_get_data(bd);

	if (!bl_info) {
		pr_err("%s: fail to get bl_info\n", __func__);
		return;
	}
	if (bl_info->maptbl)
		kfree(bl_info->maptbl);

	mutex_lock(&bl_info->ops_lock);
	bl_info->ops = NULL;
	mutex_unlock(&bl_info->ops_lock);
}
EXPORT_SYMBOL(gen_panel_backlight_device_unregister);

int gen_panel_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

	gen_panel_backlight_set_brightness(bd, 0);
	pm_runtime_disable(&pdev->dev);
	backlight_device_unregister(bd);

	return 0;
}
EXPORT_SYMBOL(gen_panel_backlight_remove);

void gen_panel_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

	gen_panel_backlight_set_brightness(bd, 0);
	pm_runtime_disable(&pdev->dev);
}
EXPORT_SYMBOL(gen_panel_backlight_shutdown);

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
int gen_panel_backlight_runtime_suspend(struct device *dev)
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
EXPORT_SYMBOL(gen_panel_backlight_runtime_suspend);

int gen_panel_backlight_runtime_resume(struct device *dev)
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
EXPORT_SYMBOL(gen_panel_backlight_runtime_resume);
#endif

MODULE_DESCRIPTION("GENERIC PANEL BACKLIGHT Driver");
MODULE_LICENSE("GPL");
