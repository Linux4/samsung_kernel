/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fae/s6e3fae_e1s_panel_copr.h
 *
 * Header file for COPR Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAE_E1S_PANEL_COPR_H__
#define __S6E3FAE_E1S_PANEL_COPR_H__

#include "../panel.h"
#include "../copr.h"
#include "oled_common.h"
#include "s6e3fae_e1s_panel.h"

#define S6E3FAE_E1S_COPR_EN	(1)
#define S6E3FAE_E1S_COPR_PWR	(1)
#define S6E3FAE_E1S_COPR_MASK	(0)
#define S6E3FAE_E1S_COPR_ROI_CTRL	(0x1F)
#define S6E3FAE_E1S_COPR_GAMMA	(0x1D)
#define S6E3FAE_E1S_COPR_FRAME_COUNT	(0) /* deprecated */

#define S6E3FAE_E1S_COPR_ROI1_ER		(128)
#define S6E3FAE_E1S_COPR_ROI1_EG		(128)
#define S6E3FAE_E1S_COPR_ROI1_EB		(128)

#define S6E3FAE_E1S_COPR_ROI2_ER		(256)
#define S6E3FAE_E1S_COPR_ROI2_EG		(256)
#define S6E3FAE_E1S_COPR_ROI2_EB		(256)

#define S6E3FAE_E1S_COPR_ROI3_ER		(256)
#define S6E3FAE_E1S_COPR_ROI3_EG		(256)
#define S6E3FAE_E1S_COPR_ROI3_EB		(256)

#define S6E3FAE_E1S_COPR_ROI4_ER		(256)
#define S6E3FAE_E1S_COPR_ROI4_EG		(256)
#define S6E3FAE_E1S_COPR_ROI4_EB		(256)

#define S6E3FAE_E1S_COPR_ROI5_ER		(256)
#define S6E3FAE_E1S_COPR_ROI5_EG		(256)
#define S6E3FAE_E1S_COPR_ROI5_EB		(256)

#define S6E3FAE_E1S_COPR_ROI1_X_S	(551)
#define S6E3FAE_E1S_COPR_ROI1_Y_S	(232)
#define S6E3FAE_E1S_COPR_ROI1_X_E	(575)
#define S6E3FAE_E1S_COPR_ROI1_Y_E	(256)

#define S6E3FAE_E1S_COPR_ROI2_X_S	(0)
#define S6E3FAE_E1S_COPR_ROI2_Y_S	(0)
#define S6E3FAE_E1S_COPR_ROI2_X_E	(1079)
#define S6E3FAE_E1S_COPR_ROI2_Y_E	(2339)

#define S6E3FAE_E1S_COPR_ROI3_X_S	(539)
#define S6E3FAE_E1S_COPR_ROI3_Y_S	(219)
#define S6E3FAE_E1S_COPR_ROI3_X_E	(587)
#define S6E3FAE_E1S_COPR_ROI3_Y_E	(269)

#define S6E3FAE_E1S_COPR_ROI4_X_S	(515)
#define S6E3FAE_E1S_COPR_ROI4_Y_S	(194)
#define S6E3FAE_E1S_COPR_ROI4_X_E	(611)
#define S6E3FAE_E1S_COPR_ROI4_Y_E	(294)

#define S6E3FAE_E1S_COPR_ROI5_X_S	(491)
#define S6E3FAE_E1S_COPR_ROI5_Y_S	(169)
#define S6E3FAE_E1S_COPR_ROI5_X_E	(635)
#define S6E3FAE_E1S_COPR_ROI5_Y_E	(319)

static struct pktinfo PKTINFO(e1s_level2_key_enable);
static struct pktinfo PKTINFO(e1s_level2_key_disable);

/* ===================================================================================== */
/* ============================== [S6E3FAE MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl e1s_copr_maptbl[] = {
	[COPR_MAPTBL] = DEFINE_0D_MAPTBL(e1s_copr_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), NULL, &OLED_FUNC(OLED_MAPTBL_COPY_COPR)),
};

/* ===================================================================================== */
/* ============================== [S6E3FAE COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 E1S_COPR[] = {
	0xE1,
	0x03, 0x1F, 0x1D, 0x00, 0x00, 0x00, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x02, 0x27, 0x00, 0xE8, 0x02,
	0x3F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x37, 0x09, 0x23, 0x02, 0x1B, 0x00, 0xDB, 0x02,
	0x4B, 0x01, 0x0D, 0x02, 0x03, 0x00, 0xC2, 0x02,
	0x63, 0x01, 0x26, 0x01, 0xEB, 0x00, 0xA9, 0x02,
	0x7B, 0x01, 0x3F
};

static DEFINE_PKTUI(e1s_copr, &e1s_copr_maptbl[COPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(e1s_copr, DSI_PKT_TYPE_WR, E1S_COPR, 0);

static void *e1s_set_copr_cmdtbl[] = {
	&PKTINFO(e1s_level2_key_enable),
	&PKTINFO(e1s_copr),
	&PKTINFO(e1s_level2_key_disable),
};

static DEFINE_SEQINFO(e1s_set_copr_seq, e1s_set_copr_cmdtbl);

static void *e1s_get_copr_spi_cmdtbl[] = {
	&s6e3fae_restbl[RES_COPR_SPI],
};

static void *e1s_get_copr_dsi_cmdtbl[] = {
	&s6e3fae_restbl[RES_COPR_DSI],
};

static struct seqinfo e1s_copr_seqtbl[] = {
	SEQINFO_INIT(COPR_SET_SEQ, e1s_set_copr_cmdtbl),
	SEQINFO_INIT(COPR_SPI_GET_SEQ, e1s_get_copr_spi_cmdtbl),
	SEQINFO_INIT(COPR_DSI_GET_SEQ, e1s_get_copr_dsi_cmdtbl),
};

static struct panel_copr_data s6e3fae_e1s_copr_data = {
	.seqtbl = e1s_copr_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(e1s_copr_seqtbl),
	.maptbl = (struct maptbl *)e1s_copr_maptbl,
	.nr_maptbl = (sizeof(e1s_copr_maptbl) / sizeof(struct maptbl)),
	.version = COPR_VER_6,
	.options = {
		.thread_on = false,
		.check_avg = false,
	},
	.reg.v6 = {
		.copr_mask = S6E3FAE_E1S_COPR_MASK,
		.copr_pwr = S6E3FAE_E1S_COPR_PWR,
		.copr_en = S6E3FAE_E1S_COPR_EN,
		.copr_gamma = S6E3FAE_E1S_COPR_GAMMA,
		.copr_frame_count = S6E3FAE_E1S_COPR_FRAME_COUNT,
		.roi_on = S6E3FAE_E1S_COPR_ROI_CTRL,
		.roi = {
			[0] = {
				.roi_er = S6E3FAE_E1S_COPR_ROI1_ER, .roi_eg = S6E3FAE_E1S_COPR_ROI1_EG,
				.roi_eb = S6E3FAE_E1S_COPR_ROI1_EB,
				.roi_xs = S6E3FAE_E1S_COPR_ROI1_X_S, .roi_ys = S6E3FAE_E1S_COPR_ROI1_Y_S,
				.roi_xe = S6E3FAE_E1S_COPR_ROI1_X_E, .roi_ye = S6E3FAE_E1S_COPR_ROI1_Y_E,
			},
			[1] = {
				.roi_er = S6E3FAE_E1S_COPR_ROI2_ER, .roi_eg = S6E3FAE_E1S_COPR_ROI2_EG,
				.roi_eb = S6E3FAE_E1S_COPR_ROI2_EB,
				.roi_xs = S6E3FAE_E1S_COPR_ROI2_X_S, .roi_ys = S6E3FAE_E1S_COPR_ROI2_Y_S,
				.roi_xe = S6E3FAE_E1S_COPR_ROI2_X_E, .roi_ye = S6E3FAE_E1S_COPR_ROI2_Y_E,
			},
			[2] = {
				.roi_er = S6E3FAE_E1S_COPR_ROI3_ER, .roi_eg = S6E3FAE_E1S_COPR_ROI3_EG,
				.roi_eb = S6E3FAE_E1S_COPR_ROI3_EB,
				.roi_xs = S6E3FAE_E1S_COPR_ROI3_X_S, .roi_ys = S6E3FAE_E1S_COPR_ROI3_Y_S,
				.roi_xe = S6E3FAE_E1S_COPR_ROI3_X_E, .roi_ye = S6E3FAE_E1S_COPR_ROI3_Y_E,
			},
			[3] = {
				.roi_er = S6E3FAE_E1S_COPR_ROI4_ER, .roi_eg = S6E3FAE_E1S_COPR_ROI4_EG,
				.roi_eb = S6E3FAE_E1S_COPR_ROI4_EB,
				.roi_xs = S6E3FAE_E1S_COPR_ROI4_X_S, .roi_ys = S6E3FAE_E1S_COPR_ROI4_Y_S,
				.roi_xe = S6E3FAE_E1S_COPR_ROI4_X_E, .roi_ye = S6E3FAE_E1S_COPR_ROI4_Y_E,
			},
			[4] = {
				.roi_er = S6E3FAE_E1S_COPR_ROI5_ER, .roi_eg = S6E3FAE_E1S_COPR_ROI5_EG,
				.roi_eb = S6E3FAE_E1S_COPR_ROI5_EB,
				.roi_xs = S6E3FAE_E1S_COPR_ROI5_X_S, .roi_ys = S6E3FAE_E1S_COPR_ROI5_Y_S,
				.roi_xe = S6E3FAE_E1S_COPR_ROI5_X_E, .roi_ye = S6E3FAE_E1S_COPR_ROI5_Y_E,
			},
		},
	},
	.nr_roi = 5,
};

#endif /* __S6E3FAE_E1S_PANEL_COPR_H__ */

