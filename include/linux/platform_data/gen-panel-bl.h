/* inclue/linux/platform_data/gen-panel-bl.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 * Header file for Samsung Display Backlight(LCD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _GEN_PANEL_BACKLIGHT_H
#define _GEN_PANEL_BACKLIGHT_H
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/platform_data/gen-panel.h>

#define GEN_PANEL_BL_NAME	("gen-panel-backlight")
#define IS_HBM(level)	(level >= 6)

enum {
	BL_CTRL_PWM,
	BL_CTRL_EASY_SCALE,
	BL_CTRL_STEP_CTRL,
	MIPI_CTRL,
};

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

struct gen_panel_backlight_ops {
	int (*set_brightness)(struct lcd *, int level);
	int (*get_brightness)(struct lcd *);
};

struct gen_panel_backlight_info {
	const char *name;
	bool enable;
	struct lcd *lcd;
	struct mutex ops_lock;
	const struct gen_panel_backlight_ops *ops;
	struct brt_value range[MAX_BRT_VALUE_IDX];
	struct brt_value outdoor_value;
	unsigned int auto_brightness;
	int current_brightness;
	int prev_tune_level;
};

#ifdef CONFIG_OF
static inline struct device_node *
gen_panel_find_dt_backlight(struct device_node *node, const char *pname)
{
	struct device_node *backlight_node =
		of_parse_phandle(node, pname, 0);

	if (!backlight_node) {
		pr_err("%s: backlight_node not found\n", __func__);
		return NULL;
	}

	return backlight_node;
}
#else
static inline struct device_node *
gen_panel_find_dt_backlight(struct platform_device *pdev, const char *pname)
{
	return NULL;
}
#endif

static inline int
gen_panel_backlight_enable(struct backlight_device *bd)
{
#ifdef CONFIG_PM_RUNTIME
	if (bd && bd->dev.parent)
		return pm_runtime_get_sync(bd->dev.parent);
#endif
	return 1;
}

static inline int
gen_panel_backlight_disable(struct backlight_device *bd)
{
#ifdef CONFIG_PM_RUNTIME
	if (bd && bd->dev.parent)
		return pm_runtime_put_sync(bd->dev.parent);
#endif
	return 1;
}

static inline int
gen_panel_backlight_is_on(struct backlight_device *bd)
{
#ifdef CONFIG_PM_RUNTIME
	if (bd && bd->dev.parent)
		return atomic_read(&bd->dev.parent->power.usage_count);
#endif
	return 0;
}

static inline int
gen_panel_backlight_onoff(struct backlight_device *bd, int on)
{
	int ret;

	if (on)
		ret = gen_panel_backlight_enable(bd);
	else
		ret = gen_panel_backlight_disable(bd);

	if (ret)
		pr_err("%s: error enter %s\n",
				__func__, on ? "resume" : "suspend");

	return ret;
}

#ifdef CONFIG_GEN_PANEL_BACKLIGHT
extern int gen_panel_attach_backlight(struct lcd *,
		const struct gen_panel_backlight_ops *);
extern void gen_panel_detach_backlight(struct lcd *);
extern bool gen_panel_match_backlight(struct backlight_device *, const char *);
#else
static inline int gen_panel_attach_backlight(struct lcd *lcd,
		const struct gen_panel_backlight_ops *ops) { return 0; }
static inline void gen_panel_detach_backlight(struct lcd *lcd) {}
static inline bool gen_panel_match_backlight(struct backlight_device *bd,
		const char *match) { return false; }
#endif
#endif	/* _GEN_PANEL_BACKLIGHT_H */
