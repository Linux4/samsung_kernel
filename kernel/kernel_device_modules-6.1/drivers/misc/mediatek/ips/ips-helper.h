/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __IPS_HELPER_H
#define __IPS_HELPER_H

struct ips_soc_data {
	const int *regs;
	int id;
	bool clk_bypass;
};

struct ips_drv_data {
	struct clk *clk_src_0;
	struct clk *clk_src_1;
	struct clk *clk_src_2;
	struct clk *clk_src_3;
	struct clk *ips_clk;
	struct mutex pw_clk_mutex;
	void __iomem *regs;
	void __iomem *dvfsrc_regs;
	const struct ips_soc_data *dvd;
	int pwclkcnt;
	bool enable;
};

enum clk_freq {
	clk_src_0,
	clk_src_1,
	clk_src_2,
	clk_src_3,
};

extern int mtkips_enable(struct ips_drv_data *ips_data);
extern int mtkips_disable(struct ips_drv_data *ips_data);
extern int mtkips_getvmin(struct ips_drv_data *ips_data);
extern int mtkips_getvmin_clear(struct ips_drv_data *ips_data);
extern int mtkips_worst_vmin(struct ips_drv_data *ips_data);
extern int mtkips_set_clk(struct ips_drv_data *ips_data, int clk);
extern int ips_register_sysfs(struct device *dev);
extern void ips_unregister_sysfs(struct device *dev);

#endif

