/*
 * drivers/video/mmp/hw/mmp_apical.h
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *           Zhou Zhu <zzhu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMP_APICAL_H_
#define _MMP_APICAL_H_

#include <video/mmp_disp.h>

#define APICAL_GEN4	(apical->version == 4)

struct mmp_apical_reg {
	u32 fmt_ctrl;
	u32 reserved0;
	u32 frm_size;
	u32 iridix_ctrl1;
	u32 iridix_ctrl2;
	u32 level_ctrl;
	u32 op_ctrl;
	u32 strength_filt;
	u32 ctrl_in1;
	u32 ctrl_in2;
	u32 calibrat1;
	u32 calibrat2;
	u32 bl_range;
	u32 bl_scale;
	u32 iridix_config;
	u32 ctrl_out;
	u32 lut_index;
	u32 asymmetry_lut;
	u32 color_correct_lut;
	u32 calibrat_lut;
	u32 strength_out;
};

struct mmp_apical {
	const char *name;
	void *reg_base;
	void *lcd_reg_base;
	struct device *dev;
	u32 version;
	int apical_channel_num;
	struct mmp_apical_info apical_info[0];
};

extern int apical_dbg_init(struct device *dev);
extern void apical_dbg_uninit(struct device *dev);
#endif	/* _MMP_APICAL_H_ */
