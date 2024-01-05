/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


struct panel_info {
	const char *name;
	u32 refresh;
	u32 limit;
	u32 xres, yres;
	u32 hbp, hfp, hsw;
	u32 vbp, vfp, vsw;
	u32 width_mm, height_mm;
};

int exynos_fimd_set_rate(struct device *dev, const char *clk_name, const char *parent_clk_name, struct panel_info *info);
void exynos5_keep_disp_clock(struct device *dev);

