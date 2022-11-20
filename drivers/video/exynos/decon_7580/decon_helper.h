/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DECON_HELPER_H__
#define __SAMSUNG_DECON_HELPER_H__

#include <linux/device.h>

#include "decon.h"

/*for clock setting*/
struct clk_list_t {
	struct clk *c;
	struct clk *p;
	const char *c_name;
	const char *p_name;
};

enum disp_clks {
	disp_pll,
	bus_pll,
	mout_sclk_disp_decon_int_eclk_a,
	mout_sclk_disp_decon_int_eclk_b,
	mout_aclk_disp_333_user,
	mout_sclk_decon_int_eclk_user,
	sclk_disp_decon_int_eclk,
	dout_sclk_disp_decon_int_eclk,
	m_decon0_eclk,
	d_decon0_eclk,
	m_decon0_vclk,
	d_decon0_vclk,
	mout_aclk_disp_200,
	dout_aclk_disp_200,
	mout_aclk_disp_200_user,
	d_pclk_disp,
	disp_clks_max,
};

int decon_clk_set_parent(struct device *dev, enum disp_clks idx, const char *p);
int decon_clk_set_rate(struct device *dev, enum disp_clks idx, unsigned int rate);
int decon_clk_set_divide(struct device *dev, enum disp_clks idx, unsigned int divider);

void decon_to_psr_info(struct decon_device *decon, struct decon_psr_info *psr);
void decon_to_init_param(struct decon_device *decon, struct decon_init_param *p);
int decon_is_no_bootloader_fb(struct decon_device *decon);

#endif /* __SAMSUNG_DECON_HELPER_H__ */
