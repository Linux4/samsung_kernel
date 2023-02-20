/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_rainbow_g0_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_RAINBOW_G0_AOD_PANEL_H__
#define __S6E3FAC_RAINBOW_G0_AOD_PANEL_H__

#include "s6e3fac_aod.h"
#include "s6e3fac_aod_panel.h"
#include "s6e3fac_rainbow_g0_self_mask_img.h"

/* RAINBOW_G0 */
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3fac_rainbow_g0_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_SR_FAST, RAINBOW_G0_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static void *s6e3fac_rainbow_g0_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3fac_aod_l1_key_enable),
	&PKTINFO(s6e3fac_aod_self_mask_sd_path),
	&DLYINFO(s6e3fac_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3fac_rainbow_g0_aod_self_mask_img_pkt),
	&DLYINFO(s6e3fac_aod_self_spsram_write_delay),
	&PKTINFO(s6e3fac_aod_reset_sd_path),
	&KEYINFO(s6e3fac_aod_l1_key_disable),
};

static struct seqinfo s6e3fac_rainbow_g0_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3fac_rainbow_g0_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3fac_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3fac_aod_self_mask_dis_cmdtbl),
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", s6e3fac_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", s6e3fac_aod_exit_aod_cmdtbl),
	[ENABLE_PARTIAL_SCAN] = SEQINFO_INIT("ENA_PARTIAL_SCAN", s6e3fac_aod_partial_enable_cmdtbl),
	[DISABLE_PARTIAL_SCAN] = SEQINFO_INIT("DIS_PARTIAL_SCAN", s6e3fac_aod_partial_disable_cmdtbl),
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum", s6e3fac_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3fac_rainbow_g0_aod = {
	.name = "s6e3fac_rainbow_g0_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3fac_rainbow_g0_aod_seqtbl),
	.seqtbl = s6e3fac_rainbow_g0_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3fac_aod_maptbl),
	.maptbl = s6e3fac_aod_maptbl,
	.self_mask_en = true,
};
#endif //__S6E3FAC_RAINBOW_G0_AOD_PANEL_H__
