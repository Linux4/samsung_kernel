/*
 * linux/drivers/video/fbdev/exynos/panel/ana6705/ana6705_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6705_AOD_PANEL_H__
#define __ANA6705_AOD_PANEL_H__

#include "ana6705_aod.h"

static u8 ANA6705_AOD_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 ANA6705_AOD_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };

static u8 ANA6705_AOD_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 ANA6705_AOD_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };

static DEFINE_STATIC_PACKET(ana6705_aod_l2_key_enable, DSI_PKT_TYPE_WR, ANA6705_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(ana6705_aod_l2_key_disable, DSI_PKT_TYPE_WR,ANA6705_AOD_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(ana6705_aod_l3_key_enable, DSI_PKT_TYPE_WR, ANA6705_AOD_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(ana6705_aod_l3_key_disable, DSI_PKT_TYPE_WR, ANA6705_AOD_KEY3_DISABLE, 0);

static DEFINE_PANEL_KEY(ana6705_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(ana6705_aod_l2_key_enable));
static DEFINE_PANEL_KEY(ana6705_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(ana6705_aod_l2_key_disable));

static DEFINE_PANEL_KEY(ana6705_aod_l3_key_enable, CMD_LEVEL_3,
	KEY_ENABLE, &PKTINFO(ana6705_aod_l3_key_enable));
static DEFINE_PANEL_KEY(ana6705_aod_l3_key_disable, CMD_LEVEL_3,
	KEY_DISABLE, &PKTINFO(ana6705_aod_l3_key_disable));

static struct keyinfo KEYINFO(ana6705_aod_l2_key_enable);
static struct keyinfo KEYINFO(ana6705_aod_l2_key_disable);
static struct keyinfo KEYINFO(ana6705_aod_l3_key_enable);
static struct keyinfo KEYINFO(ana6705_aod_l3_key_disable);

/* -------------------------- End self move --------------------------*/
#endif //__ANA6705_AOD_PANEL_H__
