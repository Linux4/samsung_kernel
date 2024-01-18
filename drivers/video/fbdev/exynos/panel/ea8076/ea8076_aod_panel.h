/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_AOD_PANEL_H__
#define __EA8076_AOD_PANEL_H__

#include "ea8076_aod.h"

static u8 EA8076_AOD_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 EA8076_AOD_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };

static u8 EA8076_AOD_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 EA8076_AOD_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };

static u8 EA8076_AOD_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 EA8076_AOD_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };

static DEFINE_STATIC_PACKET(ea8076_aod_l1_key_enable, DSI_PKT_TYPE_WR, EA8076_AOD_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(ea8076_aod_l1_key_disable, DSI_PKT_TYPE_WR,EA8076_AOD_KEY1_DISABLE, 0);

static DEFINE_STATIC_PACKET(ea8076_aod_l2_key_enable, DSI_PKT_TYPE_WR, EA8076_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(ea8076_aod_l2_key_disable, DSI_PKT_TYPE_WR,EA8076_AOD_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(ea8076_aod_l3_key_enable, DSI_PKT_TYPE_WR, EA8076_AOD_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(ea8076_aod_l3_key_disable, DSI_PKT_TYPE_WR, EA8076_AOD_KEY3_DISABLE, 0);


static DEFINE_PANEL_KEY(ea8076_aod_l1_key_enable, CMD_LEVEL_1,
	KEY_ENABLE, &PKTINFO(ea8076_aod_l1_key_enable));
static DEFINE_PANEL_KEY(ea8076_aod_l1_key_disable, CMD_LEVEL_1,
	KEY_DISABLE, &PKTINFO(ea8076_aod_l1_key_disable));

static DEFINE_PANEL_KEY(ea8076_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(ea8076_aod_l2_key_enable));
static DEFINE_PANEL_KEY(ea8076_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(ea8076_aod_l2_key_disable));

static DEFINE_PANEL_KEY(ea8076_aod_l3_key_enable, CMD_LEVEL_3,
	KEY_ENABLE, &PKTINFO(ea8076_aod_l3_key_enable));
static DEFINE_PANEL_KEY(ea8076_aod_l3_key_disable, CMD_LEVEL_3,
	KEY_DISABLE, &PKTINFO(ea8076_aod_l3_key_disable));

static struct keyinfo KEYINFO(ea8076_aod_l1_key_enable);
static struct keyinfo KEYINFO(ea8076_aod_l1_key_disable);
static struct keyinfo KEYINFO(ea8076_aod_l2_key_enable);
static struct keyinfo KEYINFO(ea8076_aod_l2_key_disable);
static struct keyinfo KEYINFO(ea8076_aod_l3_key_enable);
static struct keyinfo KEYINFO(ea8076_aod_l3_key_disable);

static struct maptbl ea8076_aod_maptbl[] = {};
static void *ea8076_aod_enter_aod_cmdtbl[] = {};
static void *ea8076_aod_exit_aod_cmdtbl[] = {};

#endif //__EA8076_AOD_PANEL_H__
