/*
 * Samsung Exynos SoC series Pablo driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_IS_H_
#define EXYNOS_IS_H_

#include <dt-bindings/camera/exynos_is_dt.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include "is-dvfs-config.h"
#define MHZ (1000 * 1000)

struct is_clk {
	const char *name;
	struct clk *clk;
};

enum is_dvfs_val {
	IS_DVFS_VAL_LEV,
	IS_DVFS_VAL_FRQ,
	IS_DVFS_VAL_QOS,
	IS_DVFS_VAL_END,
};

enum is_dvfs_lv {
	IS_DVFS_LV_END = 20,
};

struct is_num_of_ip {
	unsigned int taa;
	unsigned int isp;
	unsigned int byrp;
	unsigned int rgbp;
	unsigned int mcsc;
	unsigned int vra;
	unsigned int ypp;
	unsigned int shrp;
	unsigned int mcfp;
	unsigned int dlfe;
	unsigned int lme;
	unsigned int orbmch;
};
/**
* struct exynos_platform_is - camera host interface platform data
*
* @isp_info: properties of camera sensor required for host interface setup
*/
struct exynos_platform_is {
	int hw_ver;
	bool clock_on;
	int (*clk_get)(struct device *dev);
	int (*clk_cfg)(struct device *dev);
	int (*clk_on)(struct device *dev);
	int (*clk_off)(struct device *dev);
	int (*clk_get_rate)(struct device *dev);
	int (*clk_print)(struct device *dev);
	int (*clk_dump)(struct device *dev);
	struct pinctrl *pinctrl;
	/* These fields are to return qos value for dvfs scenario */
	u32 dvfs_data[IS_SN_END][IS_DVFS_END];
	u32 qos_tb[IS_DVFS_END][IS_DVFS_LV_END][IS_DVFS_VAL_END];
	bool qos_otf[IS_DVFS_END];
	const char *qos_name[IS_DVFS_END];
	const char *dvfs_cpu[IS_SN_END];

	/* For host clock gating */
	struct exynos_is_clk_gate_info *gate_info;

	/* number of available IP */
	struct is_num_of_ip num_of_ip;
};

int is_set_parent_dt(struct device *dev, const char *child, const char *parent);
int is_set_rate_dt(struct device *dev, const char *conid, unsigned int rate);
ulong is_get_rate_dt(struct device *dev, const char *conid);
ulong is_dump_rate_dt(struct device *dev, const char *conid);
int is_enable_dt(struct device *dev, const char *conid);
int is_disable_dt(struct device *dev, const char *conid);

int is_set_rate(struct device *dev, const char *name, ulong frequency);
ulong is_get_rate(struct device *dev, const char *conid);
ulong is_get_hw_rate(struct device *dev, const char *name);
int is_enable(struct device *dev, const char *conid);
int is_disable(struct device *dev, const char *conid);
int is_enabled_clk_disable(struct device *dev, const char *conid);

/* platform specific clock functions */
void is_get_clk_ops(struct exynos_platform_is *pdata);

#endif /* EXYNOS_IS_H_ */
