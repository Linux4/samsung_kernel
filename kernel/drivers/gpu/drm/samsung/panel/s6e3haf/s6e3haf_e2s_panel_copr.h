/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3haf/s6e3haf_e2s_panel_copr.h
 *
 * Header file for COPR Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAF_E2S_PANEL_COPR_H__
#define __S6E3HAF_E2S_PANEL_COPR_H__

#include "../panel.h"
#include "../copr.h"
#include "oled_common.h"
#include "s6e3haf_e2s_panel.h"

#define S6E3HAF_E2S_COPR_EN	(1)
#define S6E3HAF_E2S_COPR_PWR	(1)
#define S6E3HAF_E2S_COPR_MASK	(0)
#define S6E3HAF_E2S_COPR_ROI_CTRL	(0x1F)
#define S6E3HAF_E2S_COPR_GAMMA	(0x1D)
#define S6E3HAF_E2S_COPR_FRAME_COUNT	(0) /* deprecated */

#define S6E3HAF_E2S_COPR_ROI1_ER		(128)
#define S6E3HAF_E2S_COPR_ROI1_EG		(128)
#define S6E3HAF_E2S_COPR_ROI1_EB		(128)

#define S6E3HAF_E2S_COPR_ROI2_ER		(256)
#define S6E3HAF_E2S_COPR_ROI2_EG		(256)
#define S6E3HAF_E2S_COPR_ROI2_EB		(256)

#define S6E3HAF_E2S_COPR_ROI3_ER		(256)
#define S6E3HAF_E2S_COPR_ROI3_EG		(256)
#define S6E3HAF_E2S_COPR_ROI3_EB		(256)

#define S6E3HAF_E2S_COPR_ROI4_ER		(256)
#define S6E3HAF_E2S_COPR_ROI4_EG		(256)
#define S6E3HAF_E2S_COPR_ROI4_EB		(256)

#define S6E3HAF_E2S_COPR_ROI5_ER		(256)
#define S6E3HAF_E2S_COPR_ROI5_EG		(256)
#define S6E3HAF_E2S_COPR_ROI5_EB		(256)

#define S6E3HAF_E2S_COPR_ROI1_X_S	(731)
#define S6E3HAF_E2S_COPR_ROI1_Y_S	(280)
#define S6E3HAF_E2S_COPR_ROI1_X_E	(763)
#define S6E3HAF_E2S_COPR_ROI1_Y_E	(310)

#define S6E3HAF_E2S_COPR_ROI2_X_S	(0)
#define S6E3HAF_E2S_COPR_ROI2_Y_S	(0)
#define S6E3HAF_E2S_COPR_ROI2_X_E	(1439)
#define S6E3HAF_E2S_COPR_ROI2_Y_E	(3119)

#define S6E3HAF_E2S_COPR_ROI3_X_S	(719)
#define S6E3HAF_E2S_COPR_ROI3_Y_S	(264)
#define S6E3HAF_E2S_COPR_ROI3_X_E	(779)
#define S6E3HAF_E2S_COPR_ROI3_Y_E	(326)

#define S6E3HAF_E2S_COPR_ROI4_X_S	(687)
#define S6E3HAF_E2S_COPR_ROI4_Y_S	(233)
#define S6E3HAF_E2S_COPR_ROI4_X_E	(807)
#define S6E3HAF_E2S_COPR_ROI4_Y_E	(357)

#define S6E3HAF_E2S_COPR_ROI5_X_S	(659)
#define S6E3HAF_E2S_COPR_ROI5_Y_S	(202)
#define S6E3HAF_E2S_COPR_ROI5_X_E	(839)
#define S6E3HAF_E2S_COPR_ROI5_Y_E	(388)

static struct pktinfo PKTINFO(e2s_level2_key_enable);
static struct pktinfo PKTINFO(e2s_level2_key_disable);

/* ===================================================================================== */
/* ============================== [S6E3HAF MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl e2s_copr_maptbl[] = {
	[COPR_MAPTBL] = DEFINE_0D_MAPTBL(e2s_copr_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), NULL, &OLED_FUNC(OLED_MAPTBL_COPY_COPR)),
};

/* ===================================================================================== */
/* ============================== [S6E3HAF COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 E2S_COPR[] = {
	0xE1,
	0x03, 0x1F, 0x1D, 0x00, 0x00, 0x00, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x02, 0xDB, 0x01, 0x18, 0x02,
	0xFB, 0x01, 0x36, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x9F, 0x0C, 0x2F, 0x02, 0xCF, 0x01, 0x08, 0x03,
	0x0B, 0x01, 0x46, 0x02, 0xAF, 0x00, 0xE9, 0x03,
	0x27, 0x01, 0x65, 0x02, 0x93, 0x00, 0xCA, 0x03,
	0x47, 0x01, 0x84
};

static DEFINE_PKTUI(e2s_copr, &e2s_copr_maptbl[COPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(e2s_copr, DSI_PKT_TYPE_WR, E2S_COPR, 0);

static void *e2s_set_copr_cmdtbl[] = {
	&PKTINFO(e2s_level2_key_enable),
	&PKTINFO(e2s_copr),
	&PKTINFO(e2s_level2_key_disable),
};

static DEFINE_SEQINFO(e2s_set_copr_seq, e2s_set_copr_cmdtbl);

static void *e2s_get_copr_spi_cmdtbl[] = {
	&s6e3haf_restbl[RES_COPR_SPI],
};

static void *e2s_get_copr_dsi_cmdtbl[] = {
	&s6e3haf_restbl[RES_COPR_DSI],
};

static struct seqinfo e2s_copr_seqtbl[] = {
	SEQINFO_INIT(COPR_SET_SEQ, e2s_set_copr_cmdtbl),
	SEQINFO_INIT(COPR_SPI_GET_SEQ, e2s_get_copr_spi_cmdtbl),
	SEQINFO_INIT(COPR_DSI_GET_SEQ, e2s_get_copr_dsi_cmdtbl),
};

static struct panel_copr_data s6e3haf_e2s_copr_data = {
	.seqtbl = e2s_copr_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(e2s_copr_seqtbl),
	.maptbl = (struct maptbl *)e2s_copr_maptbl,
	.nr_maptbl = (sizeof(e2s_copr_maptbl) / sizeof(struct maptbl)),
	.version = COPR_VER_6,
	.options = {
		.thread_on = false,
		.check_avg = false,
	},
	.reg.v6 = {
		.copr_mask = S6E3HAF_E2S_COPR_MASK,
		.copr_pwr = S6E3HAF_E2S_COPR_PWR,
		.copr_en = S6E3HAF_E2S_COPR_EN,
		.copr_gamma = S6E3HAF_E2S_COPR_GAMMA,
		.copr_frame_count = S6E3HAF_E2S_COPR_FRAME_COUNT,
		.roi_on = S6E3HAF_E2S_COPR_ROI_CTRL,
		.roi = {
			[0] = {
				.roi_er = S6E3HAF_E2S_COPR_ROI1_ER, .roi_eg = S6E3HAF_E2S_COPR_ROI1_EG,
				.roi_eb = S6E3HAF_E2S_COPR_ROI1_EB,
				.roi_xs = S6E3HAF_E2S_COPR_ROI1_X_S, .roi_ys = S6E3HAF_E2S_COPR_ROI1_Y_S,
				.roi_xe = S6E3HAF_E2S_COPR_ROI1_X_E, .roi_ye = S6E3HAF_E2S_COPR_ROI1_Y_E,
			},
			[1] = {
				.roi_er = S6E3HAF_E2S_COPR_ROI2_ER, .roi_eg = S6E3HAF_E2S_COPR_ROI2_EG,
				.roi_eb = S6E3HAF_E2S_COPR_ROI2_EB,
				.roi_xs = S6E3HAF_E2S_COPR_ROI2_X_S, .roi_ys = S6E3HAF_E2S_COPR_ROI2_Y_S,
				.roi_xe = S6E3HAF_E2S_COPR_ROI2_X_E, .roi_ye = S6E3HAF_E2S_COPR_ROI2_Y_E,
			},
			[2] = {
				.roi_er = S6E3HAF_E2S_COPR_ROI3_ER, .roi_eg = S6E3HAF_E2S_COPR_ROI3_EG,
				.roi_eb = S6E3HAF_E2S_COPR_ROI3_EB,
				.roi_xs = S6E3HAF_E2S_COPR_ROI3_X_S, .roi_ys = S6E3HAF_E2S_COPR_ROI3_Y_S,
				.roi_xe = S6E3HAF_E2S_COPR_ROI3_X_E, .roi_ye = S6E3HAF_E2S_COPR_ROI3_Y_E,
			},
			[3] = {
				.roi_er = S6E3HAF_E2S_COPR_ROI4_ER, .roi_eg = S6E3HAF_E2S_COPR_ROI4_EG,
				.roi_eb = S6E3HAF_E2S_COPR_ROI4_EB,
				.roi_xs = S6E3HAF_E2S_COPR_ROI4_X_S, .roi_ys = S6E3HAF_E2S_COPR_ROI4_Y_S,
				.roi_xe = S6E3HAF_E2S_COPR_ROI4_X_E, .roi_ye = S6E3HAF_E2S_COPR_ROI4_Y_E,
			},
			[4] = {
				.roi_er = S6E3HAF_E2S_COPR_ROI5_ER, .roi_eg = S6E3HAF_E2S_COPR_ROI5_EG,
				.roi_eb = S6E3HAF_E2S_COPR_ROI5_EB,
				.roi_xs = S6E3HAF_E2S_COPR_ROI5_X_S, .roi_ys = S6E3HAF_E2S_COPR_ROI5_Y_S,
				.roi_xe = S6E3HAF_E2S_COPR_ROI5_X_E, .roi_ye = S6E3HAF_E2S_COPR_ROI5_Y_E,
			},
		},
	},
	.nr_roi = 5,
};

#endif /* __S6E3HAF_E2S_PANEL_COPR_H__ */

