/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_canvas2_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_CANVAS2_AOD_PANEL_H__
#define __S6E3HAC_CANVAS2_AOD_PANEL_H__

#include "s6e3hac_aod.h"
#include "s6e3hac_aod_panel.h"

#include "s6e3hac_canvas2_self_mask_img.h"
#include "s6e3hac_self_icon_img.h"
#include "s6e3hac_self_analog_clock_img.h"
#include "s6e3hac_self_digital_clock_img.h"

/* CANVAS2 */
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3hac_canvas2_aod_self_mask_img_pkt,
	DSI_PKT_TYPE_WR_SR, CANVAS2_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static void *s6e3hac_canvas2_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3hac_aod_l1_key_enable),
	&PKTINFO(s6e3hac_aod_self_mask_sd_path),
	&DLYINFO(s6e3hac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hac_canvas2_aod_self_mask_img_pkt),
	&PKTINFO(s6e3hac_aod_reset_sd_path),
	&KEYINFO(s6e3hac_aod_l1_key_disable),
};

// --------------------- Image for self mask control ---------------------
#ifdef CONFIG_SELFMASK_FACTORY
static char S6E3HAC_CANVAS2_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x0C,
	0x10, 0x0C, 0x11, 0x0C, 0x12,
	0x0C, 0x13, 0x09, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10
};
#else
static char S6E3HAC_CANVAS2_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x8F, 0x0B, 0x48,
	0x0C, 0x0F, 0x09, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10
};
#endif

static DEFINE_STATIC_PACKET(s6e3hac_canvas2_aod_self_mask_ctrl_ena,
	DSI_PKT_TYPE_WR, S6E3HAC_CANVAS2_AOD_SELF_MASK_ENA, 0);
static void *s6e3hac_aod_self_mask_ena_cmdtbl[] = {
	&KEYINFO(s6e3hac_aod_l1_key_enable),
	&PKTINFO(s6e3hac_canvas2_aod_self_mask_ctrl_ena),
	&KEYINFO(s6e3hac_aod_l1_key_disable),
};


/*****************************************/
/* use 40, 50, 51 panels: vporch 0x1C */
#ifdef CONFIG_SELFMASK_FACTORY
static char S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x0C,
	0x10, 0x0C, 0x11, 0x0C, 0x12,
	0x0C, 0x13, 0x09, 0x1C, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10
};
#else
static char S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x8F, 0x0B, 0x48,
	0x0C, 0x0F, 0x09, 0x1C, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10
};
#endif

static DEFINE_STATIC_PACKET(s6e3hac_canvas2_rev51_aod_self_mask_ctrl_ena,
	DSI_PKT_TYPE_WR, S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_ENA, 0);
static void *s6e3hac_canvas2_rev51_aod_self_mask_ena_cmdtbl[] = {
	&KEYINFO(s6e3hac_aod_l1_key_enable),
	&PKTINFO(s6e3hac_canvas2_rev51_aod_self_mask_ctrl_ena),
	&KEYINFO(s6e3hac_aod_l1_key_disable),
};
/*****************************************/

// --------------------- self mask checksum ----------------------------
static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_img_pkt, DSI_PKT_TYPE_WR_SR, S6E3HAC_CRC_SELF_MASK_IMG, 0);

static char S6E3HAC_AOD_SELF_MASK_CRC_ON1[] = {
	0xD8,
	0x15,
};
static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_crc_on1, DSI_PKT_TYPE_WR, S6E3HAC_AOD_SELF_MASK_CRC_ON1, 0x27);

static char S6E3HAC_AOD_SELF_MASK_DBIST_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, S6E3HAC_AOD_SELF_MASK_DBIST_ON, 0);

static char S6E3HAC_AOD_SELF_MASK_DBIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, S6E3HAC_AOD_SELF_MASK_DBIST_OFF, 0);

static char S6E3HAC_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x01,
	0xF4, 0x02, 0x33, 0x0C, 0x10,
	0x0C, 0x11, 0x09, 0x20, 0x3F,
	0xFF, 0xFF, 0xFF,
};

static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, S6E3HAC_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char S6E3HAC_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x0C,
	0x10, 0x0C, 0x11, 0x0C, 0x12,
	0x0C, 0x13, 0x09, 0x20, 0x00,
	0x00, 0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3hac_aod_self_mask_restore, DSI_PKT_TYPE_WR, S6E3HAC_AOD_SELF_MASK_RESTORE, 0);

static void *s6e3hac_aod_self_mask_checksum_cmdtbl[] = {
	&KEYINFO(s6e3hac_aod_l1_key_enable),
	&KEYINFO(s6e3hac_aod_l2_key_enable),
	&KEYINFO(s6e3hac_aod_l3_key_enable),
	&PKTINFO(s6e3hac_aod_self_mask_crc_on1),
	&PKTINFO(s6e3hac_aod_self_mask_dbist_on),
	&PKTINFO(s6e3hac_aod_self_mask_disable),
	&DLYINFO(s6e3hac_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(s6e3hac_aod_self_mask_sd_path),
	&DLYINFO(s6e3hac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hac_aod_self_mask_img_pkt),
	&PKTINFO(s6e3hac_aod_sd_path_analog),
	&DLYINFO(s6e3hac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hac_aod_self_mask_for_checksum),
	&DLYINFO(s6e3hac_aod_self_mask_checksum_2frame_delay),
	&s6e3hac_restbl[RES_SELF_MASK_CHECKSUM],
	&PKTINFO(s6e3hac_aod_self_mask_restore),
	&PKTINFO(s6e3hac_aod_self_mask_dbist_off),
	&KEYINFO(s6e3hac_aod_l3_key_disable),
	&KEYINFO(s6e3hac_aod_l2_key_disable),
	&KEYINFO(s6e3hac_aod_l1_key_disable),
};


/*****************************************/
/* use 40, 50, 51 panels: vporch 0x1C */

static char S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x01,
	0xF4, 0x02, 0x33, 0x0C, 0x10,
	0x0C, 0x11, 0x09, 0x1C, 0x3F,
	0xFF, 0xFF, 0xFF,
};

static DEFINE_STATIC_PACKET(s6e3hac_canvas2_rev51_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR,
	S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x21, 0x00, 0x00, 0x00, 0x0C,
	0x10, 0x0C, 0x11, 0x0C, 0x12,
	0x0C, 0x13, 0x09, 0x1C, 0x00,
	0x00, 0x00, 0x00,
};
static DEFINE_STATIC_PACKET(s6e3hac_canvas2_rev51_aod_self_mask_restore, DSI_PKT_TYPE_WR,
	S6E3HAC_CANVAS2_REV51_AOD_SELF_MASK_RESTORE, 0);

static void *s6e3hac_canvas2_rev51_aod_self_mask_checksum_cmdtbl[] = {
	&KEYINFO(s6e3hac_aod_l1_key_enable),
	&KEYINFO(s6e3hac_aod_l2_key_enable),
	&KEYINFO(s6e3hac_aod_l3_key_enable),
	&PKTINFO(s6e3hac_aod_self_mask_crc_on1),
	&PKTINFO(s6e3hac_aod_self_mask_dbist_on),
	&PKTINFO(s6e3hac_aod_self_mask_disable),
	&DLYINFO(s6e3hac_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(s6e3hac_aod_self_mask_sd_path),
	&DLYINFO(s6e3hac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hac_aod_self_mask_img_pkt),
	&PKTINFO(s6e3hac_aod_sd_path_analog),
	&DLYINFO(s6e3hac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3hac_canvas2_rev51_aod_self_mask_for_checksum),
	&DLYINFO(s6e3hac_aod_self_mask_checksum_2frame_delay),
	&s6e3hac_restbl[RES_SELF_MASK_CHECKSUM],
	&PKTINFO(s6e3hac_canvas2_rev51_aod_self_mask_restore),
	&PKTINFO(s6e3hac_aod_self_mask_dbist_off),
	&KEYINFO(s6e3hac_aod_l3_key_disable),
	&KEYINFO(s6e3hac_aod_l2_key_disable),
	&KEYINFO(s6e3hac_aod_l1_key_disable),
};

// --------------------- end of check sum control ----------------------------

static struct seqinfo s6e3hac_canvas2_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3hac_canvas2_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3hac_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3hac_aod_self_mask_dis_cmdtbl),
	[ANALOG_IMG_SEQ] = SEQINFO_INIT("analog_img", s6e3hac_aod_analog_img_cmdtbl),
	[ANALOG_CTRL_SEQ] = SEQINFO_INIT("analog_ctrl", s6e3hac_aod_analog_init_cmdtbl),
	[DIGITAL_IMG_SEQ] = SEQINFO_INIT("digital_img", s6e3hac_aod_digital_img_cmdtbl),
	[DIGITAL_CTRL_SEQ] = SEQINFO_INIT("digital_ctrl", s6e3hac_aod_digital_init_cmdtbl),
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", s6e3hac_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", s6e3hac_aod_exit_aod_cmdtbl),
	[SELF_MOVE_ON_SEQ] = SEQINFO_INIT("self_move_on", s6e3hac_aod_self_move_on_cmdtbl),
	[SELF_MOVE_OFF_SEQ] = SEQINFO_INIT("self_move_off", s6e3hac_aod_self_move_off_cmdtbl),
	[SELF_MOVE_RESET_SEQ] = SEQINFO_INIT("self_move_reset", s6e3hac_aod_self_move_reset_cmdtbl),
	[SELF_ICON_IMG_SEQ] = SEQINFO_INIT("SELF_ICON_IMG", s6e3hac_aod_icon_img_cmdtbl),
	[ICON_GRID_ON_SEQ] = SEQINFO_INIT("icon_grid_on", s6e3hac_aod_icon_grid_on_cmdtbl),
	[ICON_GRID_OFF_SEQ] = SEQINFO_INIT("icon_grid_off", s6e3hac_aod_icon_grid_off_cmdtbl),
	[SET_TIME_SEQ] = SEQINFO_INIT("SET_TIME", s6e3hac_aod_set_time_cmdtbl),
	[CTRL_ICON_SEQ] = SEQINFO_INIT("CTRL_ICON", s6e3hac_aod_ctrl_icon_cmdtbl),
	[DISABLE_ICON_SEQ] = SEQINFO_INIT("DISABLE_ICON", s6e3hac_aod_disable_cmdtbl),
	[ENABLE_PARTIAL_SCAN] = SEQINFO_INIT("ENA_PARTIAL_SCAN", s6e3hac_aod_partial_enable_cmdtbl),
	[DISABLE_PARTIAL_SCAN] = SEQINFO_INIT("DIS_PARTIAL_SCAN", s6e3hac_aod_partial_disable_cmdtbl),
#ifdef SUPPORT_NORMAL_SELF_MOVE
	[ENABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("enable_self_move", s6e3hac_enable_self_move),
	[DISABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("disable_self_move", s6e3hac_disable_self_move),
#endif
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum", s6e3hac_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3hac_canvas2_aod = {
	.name = "s6e3hac_canvas2_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3hac_canvas2_aod_seqtbl),
	.seqtbl = s6e3hac_canvas2_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3hac_aod_maptbl),
	.maptbl = s6e3hac_aod_maptbl,
	.self_mask_en = true,
};

static struct seqinfo s6e3hac_canvas2_rev51_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3hac_canvas2_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3hac_canvas2_rev51_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3hac_aod_self_mask_dis_cmdtbl),
	[ANALOG_IMG_SEQ] = SEQINFO_INIT("analog_img", s6e3hac_aod_analog_img_cmdtbl),
	[ANALOG_CTRL_SEQ] = SEQINFO_INIT("analog_ctrl", s6e3hac_aod_analog_init_cmdtbl),
	[DIGITAL_IMG_SEQ] = SEQINFO_INIT("digital_img", s6e3hac_aod_digital_img_cmdtbl),
	[DIGITAL_CTRL_SEQ] = SEQINFO_INIT("digital_ctrl", s6e3hac_aod_digital_init_cmdtbl),
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", s6e3hac_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", s6e3hac_aod_exit_aod_cmdtbl),
	[SELF_MOVE_ON_SEQ] = SEQINFO_INIT("self_move_on", s6e3hac_aod_self_move_on_cmdtbl),
	[SELF_MOVE_OFF_SEQ] = SEQINFO_INIT("self_move_off", s6e3hac_aod_self_move_off_cmdtbl),
	[SELF_MOVE_RESET_SEQ] = SEQINFO_INIT("self_move_reset", s6e3hac_aod_self_move_reset_cmdtbl),
	[SELF_ICON_IMG_SEQ] = SEQINFO_INIT("SELF_ICON_IMG", s6e3hac_aod_icon_img_cmdtbl),
	[ICON_GRID_ON_SEQ] = SEQINFO_INIT("icon_grid_on", s6e3hac_aod_icon_grid_on_cmdtbl),
	[ICON_GRID_OFF_SEQ] = SEQINFO_INIT("icon_grid_off", s6e3hac_aod_icon_grid_off_cmdtbl),
	[SET_TIME_SEQ] = SEQINFO_INIT("SET_TIME", s6e3hac_aod_set_time_cmdtbl),
	[CTRL_ICON_SEQ] = SEQINFO_INIT("CTRL_ICON", s6e3hac_aod_ctrl_icon_cmdtbl),
	[DISABLE_ICON_SEQ] = SEQINFO_INIT("DISABLE_ICON", s6e3hac_aod_disable_cmdtbl),
	[ENABLE_PARTIAL_SCAN] = SEQINFO_INIT("ENA_PARTIAL_SCAN", s6e3hac_aod_partial_enable_cmdtbl),
	[DISABLE_PARTIAL_SCAN] = SEQINFO_INIT("DIS_PARTIAL_SCAN", s6e3hac_aod_partial_disable_cmdtbl),
#ifdef SUPPORT_NORMAL_SELF_MOVE
	[ENABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("enable_self_move", s6e3hac_enable_self_move),
	[DISABLE_SELF_MOVE_SEQ] = SEQINFO_INIT("disable_self_move", s6e3hac_disable_self_move),
#endif
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum",
		s6e3hac_canvas2_rev51_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3hac_canvas2_rev51_aod = {
	.name = "s6e3hac_canvas2_rev51_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3hac_canvas2_rev51_aod_seqtbl),
	.seqtbl = s6e3hac_canvas2_rev51_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3hac_aod_maptbl),
	.maptbl = s6e3hac_aod_maptbl,
	.self_mask_en = true,
};

#endif //__S6E3HAC_CANVAS2_AOD_PANEL_H__
