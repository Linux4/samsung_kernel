/*
 * linux/drivers/video/fbdev/exynos/panel/ana38407/ana38407_gts10u_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA38407_GTS10U_AOD_PANEL_H__
#define __ANA38407_GTS10U_AOD_PANEL_H__

#include "oled_common_aod.h"
#include "ana38407_gts10u_self_mask_img.h"
#include "ana38407_aod.h"

#define ANA38407_GTS10U_SELF_MASK_VALID_CRC_1 (0xD3)
#define ANA38407_GTS10U_SELF_MASK_VALID_CRC_2 (0x9B)

static u8 ana38407_gts10u_self_mask_crc[] = {
	ANA38407_GTS10U_SELF_MASK_VALID_CRC_1,
	ANA38407_GTS10U_SELF_MASK_VALID_CRC_2,
};

static u8 ANA38407_AOD_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 ANA38407_AOD_KEY2_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 ANA38407_AOD_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 ANA38407_AOD_KEY2_DISABLE[] = { 0xF1, 0xA5, 0xA5 };


static DEFINE_STATIC_PACKET(ana38407_aod_l1_key_enable, DSI_PKT_TYPE_WR, ANA38407_AOD_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(ana38407_aod_l1_key_disable, DSI_PKT_TYPE_WR, ANA38407_AOD_KEY1_DISABLE, 0);

static DEFINE_STATIC_PACKET(ana38407_aod_l2_key_enable, DSI_PKT_TYPE_WR, ANA38407_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(ana38407_aod_l2_key_disable, DSI_PKT_TYPE_WR, ANA38407_AOD_KEY2_DISABLE, 0);

static DEFINE_PANEL_MDELAY(ana38407_aod_self_spsram_write_delay, 1);
static DEFINE_PANEL_MDELAY(ana38407_aod_self_spsram_sel_delay, 1);

static DEFINE_PANEL_UDELAY(ana38407_aod_self_mask_checksum_1frame_delay, 16700);
static DEFINE_PANEL_UDELAY(ana38407_aod_self_mask_checksum_2frame_delay, 33400);
static DEFINE_PANEL_UDELAY(ana38407_aod_time_update_delay, 33400);


static DEFINE_PANEL_KEY(ana38407_aod_l1_key_enable, CMD_LEVEL_1,
	KEY_ENABLE, &PKTINFO(ana38407_aod_l1_key_enable));
static DEFINE_PANEL_KEY(ana38407_aod_l1_key_disable, CMD_LEVEL_1,
	KEY_DISABLE, &PKTINFO(ana38407_aod_l1_key_disable));

static DEFINE_PANEL_KEY(ana38407_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(ana38407_aod_l2_key_enable));
static DEFINE_PANEL_KEY(ana38407_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(ana38407_aod_l2_key_disable));

static struct keyinfo KEYINFO(ana38407_aod_l1_key_enable);
static struct keyinfo KEYINFO(ana38407_aod_l1_key_disable);
static struct keyinfo KEYINFO(ana38407_aod_l2_key_enable);
static struct keyinfo KEYINFO(ana38407_aod_l2_key_disable);

static struct maptbl ana38407_aod_maptbl[] = {
};

// --------------------- Image for self mask image ---------------------

static char ANA38407_ANA38407_AOD_SELF_MASK_MX_IP_ON_1[] = {
	0xC1, 0x23
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_mx_ip_on_1, DSI_PKT_TYPE_WR, ANA38407_ANA38407_AOD_SELF_MASK_MX_IP_ON_1, 0);

static char ANA38407_ANA38407_AOD_SELF_MASK_MX_IP_ON_2[] = {
	0xC0, 0x0F, 0x00, 0x00, 0x00, 0x09, 0xB2, 0x81
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_mx_ip_on_2, DSI_PKT_TYPE_WR, ANA38407_ANA38407_AOD_SELF_MASK_MX_IP_ON_2, 0x3);

static char ANA38407_AOD_RESET_SD_PATH_1[] = {
	0x75,
	0x00,
};
static DEFINE_STATIC_PACKET(ana38407_aod_reset_sd_path_1, DSI_PKT_TYPE_WR, ANA38407_AOD_RESET_SD_PATH_1, 0);

static char ANA38407_AOD_SELF_MASK_SD_PATH_1[] = {
	0x75,
	0x10,
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_sd_path_1, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_SD_PATH_1, 0);

// --------------------- End of self mask image ---------------------


// --------------------- Image for self mask control ---------------------
static char ANA38407_AOD_FACTORY_SELF_MASK_ENA[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03, 0xE7, 0x06, 0xD4, 0x07,
	0x37, 0x0A, 0x8C, 0x03, 0x5F, 0x0B, 0x07, 0x03, 0xDA, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF
};

static DEFINE_STATIC_PACKET(ana38407_aod_factory_self_mask_ctrl_ena,
		DSI_PKT_TYPE_WR, ANA38407_AOD_FACTORY_SELF_MASK_ENA, 0);

static char ANA38407_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x05, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03, 0xE7, 0x06, 0xD4, 0x07,
	0x37, 0x0A, 0x8C, 0x03, 0x5F, 0x0B, 0x07, 0x03, 0xDA, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF
};

static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_ctrl_ena,
		DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_ENA, 0);

static DEFINE_RULE_BASED_COND(gts10u_cond_is_factory_selfmask,
		PANEL_PROPERTY_IS_FACTORY_MODE, EQ, 1);

static void *ana38407_gts10u_aod_self_mask_ena_cmdtbl[] = {
	&KEYINFO(ana38407_aod_l2_key_enable),
	&PKTINFO(ana38407_aod_self_mask_ctrl_ena),
	&KEYINFO(ana38407_aod_l2_key_disable),
};
static DEFINE_SEQINFO(gts10u_self_mask_ena_seq, ana38407_gts10u_aod_self_mask_ena_cmdtbl);

static char ANA38407_AOD_SELF_MASK_DISABLE[] = {
	0x7A,
	0x00,
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_disable, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_DISABLE, 0);

static void *ana38407_gts10u_aod_self_mask_dis_cmdtbl[] = {
	&KEYINFO(ana38407_aod_l2_key_enable),
	&PKTINFO(ana38407_aod_self_mask_disable),
	&KEYINFO(ana38407_aod_l2_key_disable),
};
// --------------------- End of self mask control ---------------------

// --------------------- check sum control ----------------------------
static DEFINE_STATIC_PACKET_WITH_OPTION(ana38407_gts10u_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_WR_SR, ANA38407_GTS10U_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

#if 0
static DEFINE_STATIC_PACKET_WITH_OPTION(ana38407_gts10u_aod_self_mask_crc_img_pkt,
		DSI_PKT_TYPE_WR_SR, ANA38407_GTS10U_SELF_MASK_CRC_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static char ANA38407_AOD_SELF_MASK_CRC_ON1[] = {
	0xD8,
	0x16,
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_crc_on1, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_CRC_ON1, 0x27);

static char ANA38407_AOD_SELF_MASK_DBIST_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_DBIST_ON, 0);

static char ANA38407_AOD_SELF_MASK_DBIST_OFF[] = {
	0xBF, 0x00
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_DBIST_OFF, 0);

static char ANA38407_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0xF4, 0x02, 0x33,
	0x09, 0x60, 0x09, 0x61, 0x00, 0x00, 0xFF, 0xFF,
	0xFF
};

static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char ANA38407_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x60, 0x09, 0x61,
	0x09, 0x62, 0x09, 0x63, 0x00, 0x00, 0x00, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(ana38407_aod_self_mask_restore, DSI_PKT_TYPE_WR, ANA38407_AOD_SELF_MASK_RESTORE, 0);

static void *ana38407_gts10u_aod_self_mask_crc_cmdtbl[] = {
	&KEYINFO(ana38407_aod_l1_key_enable),
	&KEYINFO(ana38407_aod_l2_key_enable),
	&KEYINFO(ana38407_aod_l3_key_enable),
	&PKTINFO(ana38407_aod_self_mask_crc_on1),
	&PKTINFO(ana38407_aod_self_mask_dbist_on),
	&PKTINFO(ana38407_aod_self_mask_disable),
	&DLYINFO(ana38407_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(ana38407_aod_self_mask_sd_path),
	&DLYINFO(ana38407_aod_self_spsram_sel_delay),
	&PKTINFO(ana38407_gts10u_aod_self_mask_crc_img_pkt),
	&DLYINFO(ana38407_aod_self_spsram_write_delay),
	&PKTINFO(ana38407_aod_reset_sd_path),
	&DLYINFO(ana38407_aod_self_spsram_sel_delay),
	&PKTINFO(ana38407_aod_self_mask_for_checksum),
	&DLYINFO(ana38407_aod_self_mask_checksum_2frame_delay),
	&ana38407_dmptbl[DUMP_SELF_MASK_CRC],
	&PKTINFO(ana38407_aod_self_mask_restore),
	&PKTINFO(ana38407_aod_self_mask_dbist_off),
	&KEYINFO(ana38407_aod_l3_key_disable),
	&KEYINFO(ana38407_aod_l2_key_disable),
	&KEYINFO(ana38407_aod_l1_key_disable),
};
#endif
// --------------------- end of check sum control ----------------------------

static void *ana38407_gts10u_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(ana38407_aod_l1_key_enable),
	&KEYINFO(ana38407_aod_l2_key_enable),
	&PKTINFO(ana38407_aod_self_mask_mx_ip_on_1),
	&PKTINFO(ana38407_aod_self_mask_mx_ip_on_2),

	&PKTINFO(ana38407_aod_self_mask_sd_path_1),

	&DLYINFO(ana38407_aod_self_spsram_sel_delay),
	&PKTINFO(ana38407_gts10u_aod_self_mask_img_pkt),
	&DLYINFO(ana38407_aod_self_spsram_write_delay),

	&PKTINFO(ana38407_aod_reset_sd_path_1),
	&KEYINFO(ana38407_aod_l2_key_disable),
	&KEYINFO(ana38407_aod_l1_key_disable),
};
static DEFINE_SEQINFO(gts10u_self_mask_img_seq, ana38407_gts10u_aod_self_mask_img_cmdtbl);

static struct seqinfo ana38407_gts10u_aod_seqtbl[] = {
	SEQINFO_INIT(SELF_MASK_IMG_SEQ, ana38407_gts10u_aod_self_mask_img_cmdtbl),
	SEQINFO_INIT(SELF_MASK_ENA_SEQ, ana38407_gts10u_aod_self_mask_ena_cmdtbl),
	SEQINFO_INIT(SELF_MASK_DIS_SEQ, ana38407_gts10u_aod_self_mask_dis_cmdtbl),
//	SEQINFO_INIT(SELF_MASK_CRC_SEQ, ana38407_gts10u_aod_self_mask_crc_cmdtbl),
};

static struct aod_tune ana38407_gts10u_aod = {
	.name = "ana38407_gts10u_aod",
	.nr_seqtbl = ARRAY_SIZE(ana38407_gts10u_aod_seqtbl),
	.seqtbl = ana38407_gts10u_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(ana38407_aod_maptbl),
	.maptbl = ana38407_aod_maptbl,
	.self_mask_en = true,
	.self_mask_crc = ana38407_gts10u_self_mask_crc,
	.self_mask_crc_len = ARRAY_SIZE(ana38407_gts10u_self_mask_crc),
};
#endif
