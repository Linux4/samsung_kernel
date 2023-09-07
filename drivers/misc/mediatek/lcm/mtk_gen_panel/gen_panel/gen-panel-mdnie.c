/* mediatek/lcm/mtk_gen_panel/gen_panel/gen-panel-mdnie.c
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "gen-panel-mdnie.h"
#include <linux/lockdep.h>
/*#define MTKFB*/

void update_mdnie_mode(struct mdnie_lite *mdnie)
{
	lockdep_off();
	if (mdnie->ops && mdnie->ops->set_mdnie)
		mdnie->ops->set_mdnie(&mdnie->config);
	lockdep_on();
}
EXPORT_SYMBOL(update_mdnie_mode);

#ifdef MTKFB
extern int mtkfb_set_lcm_func_call(void (*func4)(void *, void *, void *, void *),
		void *data1, void *data2, void *data3, void *data4);
void mtkfb_update_mdnie_mode(void *data1, void *data2, void *data3, void *data4)
{
	update_mdnie_mode(data1);
}
#endif

void update_accessibility_table(struct mdnie_lite *mdnie)
{
	lockdep_off();
	if (mdnie->ops && mdnie->ops->set_color_adj)
		mdnie->ops->set_color_adj(&mdnie->config);
	lockdep_on();
}
EXPORT_SYMBOL(update_accessibility_table);

void update_cabc_mode(struct mdnie_lite *mdnie)
{
	lockdep_off();
	if (mdnie->ops && mdnie->ops->set_cabc)
		mdnie->ops->set_cabc(&mdnie->config);
	lockdep_on();
}
EXPORT_SYMBOL(update_cabc_mode);

static ssize_t mdnie_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "mode : %d\n", mdnie->config.mode);
}

static ssize_t mdnie_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);
	unsigned int value;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;

	if (mdnie->config.mode != value) {
		mdnie->config.mode = value;
#ifdef MTKFB
		mtkfb_set_lcm_func_call(mtkfb_update_mdnie_mode, mdnie, NULL, NULL, NULL);
#else
		update_mdnie_mode(mdnie);
#endif
	}

	return size;
}

static ssize_t mdnie_scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "scenario : %d\n", mdnie->config.scenario);
}

static ssize_t mdnie_scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);
	unsigned int value;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;

	if (mdnie->config.scenario != value) {
		mdnie->config.scenario = value;
#ifdef MTKFB
		mtkfb_set_lcm_func_call(mtkfb_update_mdnie_mode, mdnie, NULL, NULL, NULL);
#else
		update_mdnie_mode(mdnie);
#endif
	}

	return size;
}

static ssize_t mdnie_accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "accessibility : %d\n",
			mdnie->config.accessibility);
}

static ssize_t mdnie_accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value, s[NUMBER_OF_SCR_DATA] = {0,};
	int ret, i;
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d %x %x %x %x %x %x %x %x %x",
			&value, &s[0], &s[1], &s[2], &s[3],
			&s[4], &s[5], &s[6], &s[7], &s[8]);

	for (i = 0; i < NUMBER_OF_SCR_DATA; i++)
		mdnie->scr[i] = s[i];

	/* Modify color_adjustment table */
	if (value == COLOR_BLIND)
		update_accessibility_table(mdnie);

	mdnie->config.accessibility = value;
#ifdef MTKFB
	mtkfb_set_lcm_func_call(mtkfb_update_mdnie_mode, mdnie, NULL, NULL, NULL);
#else
	update_mdnie_mode(mdnie);
#endif

	return size;
}
static ssize_t mdnie_negative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "negative : %d\n", mdnie->config.negative);
}

static ssize_t mdnie_negative_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;

	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;

	mdnie->config.negative = !!value;
#ifdef MTKFB
	mtkfb_set_lcm_func_call(mtkfb_update_mdnie_mode, mdnie, NULL, NULL, NULL);
#else
	update_mdnie_mode(mdnie);
#endif

	return size;
}

static ssize_t cabc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "cabc : %s\n", cabc_modes[mdnie->config.cabc]);
}

static ssize_t cabc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mdnie_lite *mdnie = dev_get_drvdata(dev);
	unsigned int value;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;

	if (mdnie->config.cabc < 0 ||
			mdnie->config.cabc >= MAX_CABC) {
		pr_err("%s: out of range (%d)\n",
				__func__, mdnie->config.cabc);
		return 0;
	}

	if (mdnie->config.cabc == value) {
		pr_info("ignore same cabc setting (%s)\n",
				cabc_modes[mdnie->config.cabc]);
		return size;
	}

	mdnie->config.cabc = value;
	update_cabc_mode(mdnie);

	return size;
}
static DEVICE_ATTR(mode, 0664, mdnie_mode_show, mdnie_mode_store);
static DEVICE_ATTR(scenario, 0664, mdnie_scenario_show, mdnie_scenario_store);
static DEVICE_ATTR(accessibility, 0664, mdnie_accessibility_show,
		mdnie_accessibility_store);
static DEVICE_ATTR(negative, 0664, mdnie_negative_show, mdnie_negative_store);
static DEVICE_ATTR(cabc, 0660, cabc_show, cabc_store);

int gen_panel_attach_mdnie(struct mdnie_lite *mdnie,
		const struct mdnie_ops *ops)
{
	pr_info("%s: start\n", __func__);
	if (mdnie->class || mdnie->dev)
		return 0;

	mdnie->class = class_create(THIS_MODULE, "mdnie");
	if (IS_ERR(mdnie->class)) {
		pr_warn("Unable to create mdnie class; errno = %ld\n",
				PTR_ERR(mdnie->class));
		return PTR_ERR(mdnie->class);
	}

	mdnie->dev = device_create(mdnie->class, NULL, 0, "%s", "mdnie");
	device_create_file(mdnie->dev, &dev_attr_mode);
	device_create_file(mdnie->dev, &dev_attr_scenario);
	device_create_file(mdnie->dev, &dev_attr_accessibility);
	device_create_file(mdnie->dev, &dev_attr_negative);
	device_create_file(mdnie->dev, &dev_attr_cabc);
	/*
	pr_info("%s: tuning command reg(%Xh) len(%d)\n", __func__,
			mdnie->cmd_reg,
			mdnie->cmd_len);
	*/
	mutex_init(&mdnie->ops_lock);
	mdnie->ops = ops;
	mdnie->config.mode = MDNIE_STANDARD;

	dev_set_drvdata(mdnie->dev, mdnie);
	pr_info("%s: done\n", __func__);

	return 0;
}
EXPORT_SYMBOL(gen_panel_attach_mdnie);

void gen_panel_detach_mdnie(struct mdnie_lite *mdnie)
{
	mutex_lock(&mdnie->ops_lock);
	mdnie->ops = NULL;
	mutex_unlock(&mdnie->ops_lock);
	device_remove_file(mdnie->dev, &dev_attr_cabc);
	device_remove_file(mdnie->dev, &dev_attr_accessibility);
	device_remove_file(mdnie->dev, &dev_attr_negative);
	device_remove_file(mdnie->dev, &dev_attr_scenario);
	device_remove_file(mdnie->dev, &dev_attr_mode);
	device_destroy(mdnie->class, 0);
	class_destroy(mdnie->class);

	pr_info("%s: done\n", __func__);
}
EXPORT_SYMBOL(gen_panel_detach_mdnie);

MODULE_DESCRIPTION("GENERIC PANEL MDNIE Driver");
MODULE_LICENSE("GPL");
