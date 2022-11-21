/* linux/drivers/video/exynos/decon/mdnie_reg_7420.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "mdnie_reg.h"
#include "regs-mdnie.h"

void mdnie_reg_set_img_size(u32 w, u32 h)
{
	mdnie_write(IHSIZE, w);
	mdnie_write(IVSIZE, h);
}

void mdnie_reg_set_hsync_period(u32 hync)
{
	mdnie_write(HSYNC_PERIOD, hync);
}

void mdnie_reg_enable_input_data(u32 en)
{
	mdnie_write_mask(INPUT_DATA_DISABLE, en ? 0 : ~0, 0x1);
}

void mdnie_reg_unmask_all(void)
{
	mdnie_write(REGISTER_MASK, 0);
}

void mdnie_reg_start(u32 w, u32 h)
{
	u32 hsync_period;
	hsync_period = (w >> 1) + MDNIE_FSM_HFP + MDNIE_FSM_HSW + MDNIE_FSM_HBP;
	decon_reg_set_mdnie_pclk(0, 1);

	mdnie_reg_set_img_size(w, h);
	mdnie_reg_set_hsync_period(hsync_period);
	mdnie_reg_enable_input_data(1);
	mdnie_reg_unmask_all();

	decon_reg_set_mdnie_blank(0, MDNIE_FSM_VFP, MDNIE_FSM_VSW, MDNIE_FSM_VBP,
			MDNIE_FSM_HFP + MDNIE_FSM_HSW + MDNIE_FSM_HBP);
	decon_reg_enable_mdnie(0, 1);
	decon_reg_update_standalone(0);
}

void mdnie_reg_update_frame(u32 w, u32 h)
{
	mdnie_reg_set_img_size(w, h);
	mdnie_reg_enable_input_data(1);
	mdnie_reg_unmask_all();
}

void mdnie_reg_stop(void)
{
	decon_reg_enable_mdnie(0, 0);
	decon_reg_set_mdnie_pclk(0, 0);
}
