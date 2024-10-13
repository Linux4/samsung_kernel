/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3fc5/s6e3fc5_m35x_rcd.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2022 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC5_M35X_RCD_H__
#define __S6E3FC5_M35X_RCD_H__

#include "../panel.h"
#include "s6e3fc5.h"
#include "s6e3fc5_m35x_rcd_image.h"

static struct rcd_image s6e3fc5_m35x_rcd_1080x2340_top = {
	.name = "top",
	.image_rect = {
		.x = 0,
		.y = 0,
		.w = 1080,
		.h = 128,
	},
	.image_data = S6E3FC5_M35X_RCD_IMAGE_TOP,
	.image_data_len = ARRAY_SIZE(S6E3FC5_M35X_RCD_IMAGE_TOP),
};

static struct rcd_image s6e3fc5_m35x_rcd_1080x2340_bottom = {
	.name = "bottom",
	.image_rect = {
		.x = 0,
		.y = 2212,
		.w = 1080,
		.h = 128,
	},
	.image_data = S6E3FC5_M35X_RCD_IMAGE_BOTTOM,
	.image_data_len = ARRAY_SIZE(S6E3FC5_M35X_RCD_IMAGE_BOTTOM),
};

static struct rcd_image *s6e3fc5_m35x_rcd_1080x2340_images[] = {
	&s6e3fc5_m35x_rcd_1080x2340_top,
	&s6e3fc5_m35x_rcd_1080x2340_bottom,
};

static struct rcd_resol s6e3fc5_m35x_rcd_resol[] = {
	[S6E3FC5_RESOL_1080x2340] = {
		.name = "s6e3fc5_m35x_rcd_1080x2340",
		.resol_x = 1080,
		.resol_y = 2340,
		.block_rect = {
			.x = 0,
			.y = 128,
			.w = 1080,
			.h = 2084,
		},
		.images = s6e3fc5_m35x_rcd_1080x2340_images,
		.nr_images = ARRAY_SIZE(s6e3fc5_m35x_rcd_1080x2340_images),
	},
};

static struct rcd_resol *s6e3fc5_m35x_rcd_resol_array[] = {
	&s6e3fc5_m35x_rcd_resol[S6E3FC5_RESOL_1080x2340],
};

static struct panel_rcd_data s6e3fc5_m35x_rcd = {
	.version = 1,
	.name = "s6e3fc5_m35x_rcd",
	.rcd_resol = s6e3fc5_m35x_rcd_resol_array,
	.nr_rcd_resol = ARRAY_SIZE(s6e3fc5_m35x_rcd_resol_array),
};

#endif
