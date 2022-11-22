/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hae/s6e3hae_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAE_AOD_PANEL_H__
#define __S6E3HAE_AOD_PANEL_H__

#include "s6e3hae_aod.h"
#include "s6e3hae_self_mask_img.h"

static u8 S6E3HAE_AOD_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 S6E3HAE_AOD_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };

static u8 S6E3HAE_AOD_KEY2_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 S6E3HAE_AOD_KEY2_DISABLE[] = { 0xF1, 0xA5, 0xA5 };

static u8 S6E3HAE_AOD_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 S6E3HAE_AOD_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };

static DEFINE_STATIC_PACKET(s6e3hae_aod_l1_key_enable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3hae_aod_l1_key_disable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY1_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3hae_aod_l2_key_enable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3hae_aod_l2_key_disable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3hae_aod_l3_key_enable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3hae_aod_l3_key_disable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_KEY3_DISABLE, 0);

static DEFINE_PANEL_UDELAY(s6e3hae_aod_self_spsram_sel_delay, 1);
static DEFINE_PANEL_MDELAY(s6e3hae_aod_self_spsram_write_delay, 1);
static DEFINE_PANEL_UDELAY(s6e3hae_aod_self_mask_checksum_1frame_delay, 16700);
static DEFINE_PANEL_UDELAY(s6e3hae_aod_self_mask_checksum_2frame_delay, 33400);
static DEFINE_PANEL_UDELAY(s6e3hae_aod_time_update_delay, 33400);


static DEFINE_PANEL_KEY(s6e3hae_aod_l1_key_enable, CMD_LEVEL_1,
	KEY_ENABLE, &PKTINFO(s6e3hae_aod_l1_key_enable));
static DEFINE_PANEL_KEY(s6e3hae_aod_l1_key_disable, CMD_LEVEL_1,
	KEY_DISABLE, &PKTINFO(s6e3hae_aod_l1_key_disable));

static DEFINE_PANEL_KEY(s6e3hae_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(s6e3hae_aod_l2_key_enable));
static DEFINE_PANEL_KEY(s6e3hae_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(s6e3hae_aod_l2_key_disable));

static DEFINE_PANEL_KEY(s6e3hae_aod_l3_key_enable, CMD_LEVEL_3,
	KEY_ENABLE, &PKTINFO(s6e3hae_aod_l3_key_enable));
static DEFINE_PANEL_KEY(s6e3hae_aod_l3_key_disable, CMD_LEVEL_3,
	KEY_DISABLE, &PKTINFO(s6e3hae_aod_l3_key_disable));

static struct keyinfo KEYINFO(s6e3hae_aod_l1_key_enable);
static struct keyinfo KEYINFO(s6e3hae_aod_l1_key_disable);
static struct keyinfo KEYINFO(s6e3hae_aod_l2_key_enable);
static struct keyinfo KEYINFO(s6e3hae_aod_l2_key_disable);
static struct keyinfo KEYINFO(s6e3hae_aod_l3_key_enable);
static struct keyinfo KEYINFO(s6e3hae_aod_l3_key_disable);


static struct maptbl s6e3hae_aod_maptbl[] = {
	[SELF_MASK_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(s6e3hae_aod_self_mask, s6e3hae_init_self_mask_ctrl, NULL, s6e3hae_copy_self_mask_ctrl),
#if 0
	[SET_PARTIAL_MODE] = DEFINE_0D_MAPTBL(s6e3hae_aod_partial_mode, init_common_table, NULL, s6e3hae_copy_partial_mode),
#endif
};

// --------------------- Image for self mask image ---------------------

static char S6E3HAE_AOD_RESET_SD_PATH[] = {
	0x75,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_reset_sd_path, DSI_PKT_TYPE_WR, S6E3HAE_AOD_RESET_SD_PATH, 0);


static char S6E3HAE_AOD_SELF_MASK_SD_PATH[] = {
	0x75,
	0x10,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_sd_path, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_SD_PATH, 0);

// --------------------- End of self mask image ---------------------


// --------------------- Image for self mask control ---------------------
#ifdef CONFIG_SELFMASK_FACTORY
static char S6E3HAE_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x0C, 0x10, 0x0C, 0x11,
	0x0C, 0x12, 0x07, 0x13,
};
#else
static char S6E3HAE_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2B,
	0x0A, 0xE4, 0x0C, 0x0F,
};
#endif

static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_ena, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_ENA, 0);

static void *s6e3hae_aod_self_mask_ena_cmdtbl[] = {
	&KEYINFO(s6e3hae_aod_l1_key_enable),
	&PKTINFO(s6e3hae_aod_self_mask_ena),
	&KEYINFO(s6e3hae_aod_l1_key_disable),
};

static char S6E3HAE_AOD_SELF_MASK_DISABLE[] = {
	0x7A,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_disable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_DISABLE, 0);

static void *s6e3hae_aod_self_mask_dis_cmdtbl[] = {
	&KEYINFO(s6e3hae_aod_l1_key_enable),
	&PKTINFO(s6e3hae_aod_self_mask_disable),
	&KEYINFO(s6e3hae_aod_l1_key_disable),
};
// --------------------- End of self mask control ---------------------

// --------------------- self mask checksum ----------------------------
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3hae_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_SR_FAST, S6E3HAE_CRC_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static char S6E3HAE_AOD_SELF_MASK_CRC_ON1[] = {
	0xD8,
	0x17,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_crc_on1, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_CRC_ON1, 0x27);

static char S6E3HAE_AOD_SELF_MASK_DBIST_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_DBIST_ON, 0);

static char S6E3HAE_AOD_SELF_MASK_DBIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_DBIST_OFF, 0);

static char S6E3HAE_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x01, 0xF4, 0x02, 0x33,
	0x0C, 0x10, 0x0C, 0x11, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF,
	0xFF, 0xFF
};

static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char S6E3HAE_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x00, 0x0C, 0x10, 0x0C, 0x11,
	0x0C, 0x12, 0x0C, 0x13, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF,
	0xFF, 0xFF
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_self_mask_restore, DSI_PKT_TYPE_WR, S6E3HAE_AOD_SELF_MASK_RESTORE, 0);

static void *s6e3hae_aod_self_mask_checksum_cmdtbl[] = {
	&KEYINFO(s6e3hae_aod_l1_key_enable),
	&KEYINFO(s6e3hae_aod_l2_key_enable),
	&KEYINFO(s6e3hae_aod_l3_key_enable),
	&PKTINFO(s6e3hae_aod_self_mask_crc_on1),
	&PKTINFO(s6e3hae_aod_self_mask_dbist_on),
	&PKTINFO(s6e3hae_aod_self_mask_disable),
	&DLYINFO(s6e3hae_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(s6e3hae_aod_self_mask_sd_path),
	&DLYINFO(s6e3hae_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hae_aod_self_mask_img_pkt),
	&DLYINFO(s6e3hae_aod_self_spsram_write_delay),
	&PKTINFO(s6e3hae_aod_reset_sd_path),
	&DLYINFO(s6e3hae_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hae_aod_self_mask_for_checksum),
	&DLYINFO(s6e3hae_aod_self_mask_checksum_2frame_delay),
	&s6e3hae_restbl[RES_SELF_MASK_CHECKSUM],
	&PKTINFO(s6e3hae_aod_self_mask_restore),
	&PKTINFO(s6e3hae_aod_self_mask_dbist_off),
	&KEYINFO(s6e3hae_aod_l3_key_disable),
	&KEYINFO(s6e3hae_aod_l2_key_disable),
	&KEYINFO(s6e3hae_aod_l1_key_disable),
};

// --------------------- end of check sum control ----------------------------

/* setting for aod porch */
static char S6E3HAE_AOD_PORCH[] = {
	0x76,
	0x08, 0x1F, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_porch, DSI_PKT_TYPE_WR, S6E3HAE_AOD_PORCH, 0);

static char S6E3HAE_AOD_ENABLE[] = {
	0x76,
	0x11,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_enable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_ENABLE, 5);

static char S6E3HAE_AOD_DISABLE[] = {
	0x76,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3hae_aod_disable, DSI_PKT_TYPE_WR, S6E3HAE_AOD_DISABLE, 5);


static char S6E3HAE_PARTIAL_MODE[] = {
	0x85,
	0x81, /* enable partial scan & enable partial hlpm*/
	0x00, /*enable partial hlpm area*/
	0x0F, 0x0F, 0x00, 0x00, 0x1F,
};
DEFINE_STATIC_PACKET(s6e3hae_enable_partial_mode, DSI_PKT_TYPE_WR, S6E3HAE_PARTIAL_MODE, 0);


static char S6E3HAE_DIS_PARTIAL_SCAN[] = {
	0x85,
	0x00,
};
DEFINE_STATIC_PACKET(s6e3hae_disable_partial_mode, DSI_PKT_TYPE_WR, S6E3HAE_DIS_PARTIAL_SCAN, 0);


static void *s6e3hae_aod_enter_aod_cmdtbl[] = {
	&KEYINFO(s6e3hae_aod_l1_key_enable),
	&PKTINFO(s6e3hae_aod_porch),
	&PKTINFO(s6e3hae_aod_enable),
	&PKTINFO(s6e3hae_enable_partial_mode),
	&KEYINFO(s6e3hae_aod_l1_key_disable),
};

static void *s6e3hae_aod_exit_aod_cmdtbl[] = {
	&KEYINFO(s6e3hae_aod_l1_key_enable),
	&PKTINFO(s6e3hae_aod_disable),
	&PKTINFO(s6e3hae_disable_partial_mode),
	&PKTINFO(s6e3hae_aod_reset_sd_path),
	&KEYINFO(s6e3hae_aod_l1_key_disable),
};

#endif //__S6E3HAE_AOD_PANEL_H__
