/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef _GSP_R9P0_CFG_H
#define _GSP_R9P0_CFG_H
/**---------------------------------------------------------------------------*
 **                             Indepence                                     *
 **---------------------------------------------------------------------------*
 */
#include <linux/types.h>
#include <uapi/drm/gsp_cfg.h>
#include <uapi/drm/gsp_r9p0_cfg.h>
#include <drm/gsp_cfg.h>

struct gsp_r9p0_img_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_img_layer_params	params;
};

struct gsp_r9p0_osd_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_osd_layer_params	params;
};

struct gsp_r9p0_des_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_des_layer_params	params;
};

struct gsp_r9p0_misc_cfg {
	uint8_t gsp_gap;
	uint8_t core_num;
	uint8_t co_work0;
	uint8_t co_work1;
	uint8_t work_mod;
	uint8_t pmargb_en;
	uint8_t secure_en;
	bool hdr_flag;
	uint32_t work_freq;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
	struct gsp_r9p0_hdr10_cfg hdr10_para;
};

struct gsp_r9p0_cfg {
	struct gsp_cfg common;
	struct gsp_r9p0_img_layer limg[R9P0_IMGL_NUM];
	struct gsp_r9p0_osd_layer losd[R9P0_OSDL_NUM];
	struct gsp_r9p0_des_layer ld1;
	struct gsp_r9p0_misc_cfg misc;
};

#endif
