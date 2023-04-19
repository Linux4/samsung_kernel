/*
 * linux/drivers/video/fbdev/exynos/panel/sw83109c/sw83109c_m54x_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SW83109C_M54X_AOD_PANEL_H__
#define __SW83109C_M54X_AOD_PANEL_H__

#include "sw83109c_aod.h"
#include "sw83109c_m54x_self_mask_img.h"
#include "sw83109c_m54x_self_mask_img_factory.h"

#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_1 (0x08)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_2 (0x08)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_3 (0x71)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_4 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_5 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_6 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_7 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_8 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_9 (0x16)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_10 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_11 (0x00)
#define SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_12 (0xE1)

#if defined(__PANEL_NOT_USED_VARIABLE__)
static unsigned char M54X_AOD_LEVEL0[] = {
	0xB0, 0xA0
};
#endif

static unsigned char M54X_AOD_LEVEL1[] = {
	0xB0, 0xA1
};

static unsigned char M54X_AOD_LEVEL8[] = {
	0xB0, 0xA8
};

#if defined(__PANEL_NOT_USED_VARIABLE__)
static DEFINE_STATIC_PACKET(m54x_aod_level0, DSI_PKT_TYPE_WR, M54X_AOD_LEVEL0, 0);
#endif
static DEFINE_STATIC_PACKET(m54x_aod_level1, DSI_PKT_TYPE_WR, M54X_AOD_LEVEL1, 0);
static DEFINE_STATIC_PACKET(m54x_aod_level8, DSI_PKT_TYPE_WR, M54X_AOD_LEVEL8, 0);

static DEFINE_PANEL_UDELAY(sw83109c_aod_self_mask_checksum_1frame_delay, 16700);
static DEFINE_PANEL_UDELAY(sw83109c_aod_self_mask_checksum_2frame_delay, 33400);
static DEFINE_PANEL_UDELAY(sw83109c_aod_time_update_delay, 33400);
static DEFINE_PANEL_MDELAY(sw83109c_aod_self_mask_spsram_sel_delay, 1);
static DEFINE_PANEL_MDELAY(sw83109c_aod_self_maks_img_write_delay, 1);

static struct maptbl sw83109c_aod_maptbl[] = {
};

// --------------------- Image for self mask image ---------------------

static char SW83109C_AOD_RESET_SD_PATH[] = {
	0x75,
	0x01,
};
static DEFINE_STATIC_PACKET(sw83109c_aod_reset_sd_path, DSI_PKT_TYPE_WR, SW83109C_AOD_RESET_SD_PATH, 0);


static char SW83109C_AOD_SELF_MASK_SD_PATH[] = {
	0x75,
	0x10,
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_sd_path, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_SD_PATH, 0);

static char SW83109C_AOD_SELF_MASK_PRE_SETTING[] = {
	0xE0,
	0x08, 0x08, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x06, 0x00
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_pre_setting, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_PRE_SETTING, 0);


// --------------------- End of self mask image ---------------------


// --------------------- Image for self mask control ---------------------
#ifdef CONFIG_SELFMASK_FACTORY
static char SW83109C_AOD_SELF_MASK_ENABLE[] = {
	0x7A,
	0x11, 0x00, 0x00, 0x00, 0x09, 0x60, 0x09, 0x61,
	0x09, 0x62, 0x09, 0x63
};
#else
static char SW83109C_AOD_SELF_MASK_ENABLE[] = {
	0x7A,
	0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2b,
	0x08, 0x34, 0x09, 0x5f
};
#endif

static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_ctrl_enable, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_ENABLE, 0);

static void *sw83109c_m54x_aod_self_mask_ena_cmdtbl[] = {
	&PKTINFO(m54x_aod_level8),
	&PKTINFO(sw83109c_aod_self_mask_ctrl_enable),
};

static char SW83109C_AOD_SELF_MASK_DISABLE[] = {
	0x7A,
	0x00,
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_disable, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_DISABLE, 0);

static void *sw83109c_m54x_aod_self_mask_dis_cmdtbl[] = {
	&PKTINFO(m54x_aod_level8),
	&PKTINFO(sw83109c_aod_self_mask_disable),
};
// --------------------- End of self mask control ---------------------

// --------------------- check sum control ----------------------------
static DEFINE_STATIC_PACKET_WITH_OPTION(sw83109c_m54x_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_WR_SR, SW83109C_M54X_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static DEFINE_STATIC_PACKET_WITH_OPTION(sw83109c_m54x_aod_self_mask_crc_img_pkt,
		DSI_PKT_TYPE_WR_SR, SW83109C_M54X_SELF_MASK_CRC_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static char SW83109C_AOD_SELF_MASK_PRE_CRC_SET[] = {
	0xBB,
	0x24, 0x10, 0x08, 0x80
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_pre_crc_set, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_PRE_CRC_SET, 0);

static char SW83109C_AOD_SELF_MASK_POST_CRC_SET[] = {
	0xBB,
	0x24, 0x16, 0x08, 0x80
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_post_crc_set, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_POST_CRC_SET, 0);

static char SW83109C_AOD_SELF_MASK_CRC_ON[] = {
	0xE0,
	0x08, 0x08, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_crc_on, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_CRC_ON, 0);

static char SW83109C_AOD_SELF_MASK_DBIST_ON[] = {
	0xFD,
	0x41, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_DBIST_ON, 0);

static char SW83109C_AOD_SELF_MASK_DBIST_OFF[] = {
	0xFD, 0x40
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_DBIST_OFF, 0);

static char SW83109C_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x11, 0x00, 0x00, 0x00, 0x01, 0xf4, 0x02, 0x33,
	0x09, 0x60, 0x09, 0x61, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff,
	0xff, 0xff
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char SW83109C_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x11, 0x00, 0x00, 0x00, 0x09, 0x60, 0x09, 0x61,
	0x09, 0x62, 0x09, 0x63, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x3f, 0xff,
	0xff, 0xff
};
static DEFINE_STATIC_PACKET(sw83109c_aod_self_mask_restore, DSI_PKT_TYPE_WR, SW83109C_AOD_SELF_MASK_RESTORE, 0);

static void *sw83109c_m54x_aod_self_mask_checksum_cmdtbl[] = {
	&PKTINFO(m54x_aod_level1),
	&PKTINFO(sw83109c_aod_self_mask_pre_crc_set),
	&PKTINFO(m54x_aod_level8),
	&PKTINFO(sw83109c_aod_self_mask_crc_on),
	&PKTINFO(sw83109c_aod_self_mask_dbist_on),
	&PKTINFO(sw83109c_aod_self_mask_disable),
	&DLYINFO(sw83109c_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(sw83109c_aod_self_mask_sd_path),
	&DLYINFO(sw83109c_aod_self_mask_spsram_sel_delay),
	&PKTINFO(sw83109c_m54x_aod_self_mask_crc_img_pkt),
	&DLYINFO(sw83109c_aod_self_maks_img_write_delay),
	&PKTINFO(sw83109c_aod_reset_sd_path),
	&PKTINFO(sw83109c_aod_self_mask_for_checksum),
	&DLYINFO(sw83109c_aod_self_mask_checksum_2frame_delay),
	&sw83109c_restbl[RES_SELF_MASK_CHECKSUM],
	&PKTINFO(sw83109c_aod_self_mask_restore),
	&PKTINFO(sw83109c_aod_self_mask_dbist_off),
	&PKTINFO(m54x_aod_level1),
	&PKTINFO(sw83109c_aod_self_mask_post_crc_set),
 };

// --------------------- end of check sum control ----------------------------

static void *sw83109c_m54x_aod_self_mask_img_cmdtbl[] = {
	&PKTINFO(m54x_aod_level8),
	&PKTINFO(sw83109c_aod_self_mask_pre_setting),
	&PKTINFO(sw83109c_aod_self_mask_sd_path),
	&DLYINFO(sw83109c_aod_self_mask_spsram_sel_delay),
	&PKTINFO(sw83109c_m54x_aod_self_mask_img_pkt),
	&DLYINFO(sw83109c_aod_self_maks_img_write_delay),
	&PKTINFO(sw83109c_aod_reset_sd_path),
};

static struct seqinfo sw83109c_m54x_aod_seqtbl[] = {
	SEQINFO_INIT(SELF_MASK_IMG_SEQ, sw83109c_m54x_aod_self_mask_img_cmdtbl),
	SEQINFO_INIT(SELF_MASK_ENA_SEQ, sw83109c_m54x_aod_self_mask_ena_cmdtbl),
	SEQINFO_INIT(SELF_MASK_DIS_SEQ, sw83109c_m54x_aod_self_mask_dis_cmdtbl),
	SEQINFO_INIT(SELF_MASK_CHECKSUM_SEQ, sw83109c_m54x_aod_self_mask_checksum_cmdtbl),
};

static u8 sw83109c_m54x_self_mask_checksum[] = {
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_1,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_2,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_3,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_4,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_5,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_6,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_7,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_8,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_9,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_10,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_11,
	SW83109C_M54X_SELF_MASK_VALID_CHECKSUM_12,
};

static struct aod_tune sw83109c_m54x_aod = {
	.name = "sw83109c_m54x_aod",
	.nr_seqtbl = ARRAY_SIZE(sw83109c_m54x_aod_seqtbl),
	.seqtbl = sw83109c_m54x_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(sw83109c_aod_maptbl),
	.maptbl = sw83109c_aod_maptbl,
	.self_mask_en = true,
	.self_mask_checksum = sw83109c_m54x_self_mask_checksum,
	.self_mask_checksum_len = ARRAY_SIZE(sw83109c_m54x_self_mask_checksum),
};
#endif
