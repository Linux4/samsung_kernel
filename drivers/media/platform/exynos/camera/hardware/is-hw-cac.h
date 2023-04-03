/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CAC_H
#define IS_HW_CAC_H

#define CAC_MAX_NI_DEPENDED_CFG     (9)
struct cac_map_thr_cfg {
	u32	map_spot_thr_l;
	u32	map_spot_thr_h;
	u32	map_spot_thr;
	u32	map_spot_nr_strength;
};

struct cac_crt_thr_cfg {
	u32	crt_color_thr_l_dot;
	u32	crt_color_thr_l_line;
	u32	crt_color_thr_h;
};

struct cac_cfg_by_ni {
	struct cac_map_thr_cfg	map_thr_cfg;
	struct cac_crt_thr_cfg	crt_thr_cfg;
};

struct cac_setfile_contents {
	bool	cac_en;
	u32	ni_max;
	u32	ni_vals[CAC_MAX_NI_DEPENDED_CFG];
	struct cac_cfg_by_ni	cfgs[CAC_MAX_NI_DEPENDED_CFG];
};
#endif
