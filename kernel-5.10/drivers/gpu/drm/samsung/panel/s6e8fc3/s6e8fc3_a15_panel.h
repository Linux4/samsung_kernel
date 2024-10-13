/*
 * linux/drivers/video/fbdev/exynos/panel/s6e8fc3/s6e8fc3_a15_panel.h
 *
 * Header file for S6E8FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E8FC3_A15_PANEL_H__
#define __S6E8FC3_A15_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "oled_function.h"
#include "oled_property.h"
#include "s6e8fc3.h"
#include "s6e8fc3_a15.h"
#include "s6e8fc3_dimming.h"
#ifdef CONFIG_USDM_MDNIE
#include "s6e8fc3_a15_panel_mdnie.h"
#endif
#include "s6e8fc3_a15_panel_dimming.h"

#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
#include "s6e8fc3_a15_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e8fc3_a15_resol.h"

#undef __pn_name__
#define __pn_name__	a15

#undef __PN_NAME__
#define __PN_NAME__

/* ===================================================================================== */
/* ============================= [S6E8FC3 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E8FC3 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E8FC3 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a15_brt_table[S6E8FC3_TOTAL_STEP][2] = {
	{0x00, 0x04},
	{0x00, 0x06}, {0x00, 0x08}, {0x00, 0x0A}, {0x00, 0x0D}, {0x00, 0x10}, {0x00, 0x14}, {0x00, 0x17}, {0x00, 0x1B}, {0x00, 0x1E}, {0x00, 0x22},
	{0x00, 0x26}, {0x00, 0x2B}, {0x00, 0x2F}, {0x00, 0x33}, {0x00, 0x37}, {0x00, 0x3C}, {0x00, 0x41}, {0x00, 0x45}, {0x00, 0x4A}, {0x00, 0x4F},
	{0x00, 0x54}, {0x00, 0x59}, {0x00, 0x5E}, {0x00, 0x63}, {0x00, 0x68}, {0x00, 0x6D}, {0x00, 0x73}, {0x00, 0x78}, {0x00, 0x7D}, {0x00, 0x83},
	{0x00, 0x88}, {0x00, 0x8E}, {0x00, 0x93}, {0x00, 0x99}, {0x00, 0x9F}, {0x00, 0xA5}, {0x00, 0xAA}, {0x00, 0xB0}, {0x00, 0xB6}, {0x00, 0xBC},
	{0x00, 0xC2}, {0x00, 0xC8}, {0x00, 0xCE}, {0x00, 0xD5}, {0x00, 0xDB}, {0x00, 0xE1}, {0x00, 0xE7}, {0x00, 0xED}, {0x00, 0xF4}, {0x00, 0xFA},
	{0x01, 0x01}, {0x01, 0x07}, {0x01, 0x0E}, {0x01, 0x14}, {0x01, 0x1B}, {0x01, 0x21}, {0x01, 0x28}, {0x01, 0x2F}, {0x01, 0x35}, {0x01, 0x3C},
	{0x01, 0x43}, {0x01, 0x4A}, {0x01, 0x50}, {0x01, 0x57}, {0x01, 0x5E}, {0x01, 0x65}, {0x01, 0x6C}, {0x01, 0x73}, {0x01, 0x7A}, {0x01, 0x82},
	{0x01, 0x89}, {0x01, 0x90}, {0x01, 0x98}, {0x01, 0x9F}, {0x01, 0xA7}, {0x01, 0xAE}, {0x01, 0xB6}, {0x01, 0xBE}, {0x01, 0xC5}, {0x01, 0xCD},
	{0x01, 0xD5}, {0x01, 0xDC}, {0x01, 0xE4}, {0x01, 0xEC}, {0x01, 0xF4}, {0x01, 0xFC}, {0x02, 0x04}, {0x02, 0x0C}, {0x02, 0x14}, {0x02, 0x1B},
	{0x02, 0x23}, {0x02, 0x2C}, {0x02, 0x34}, {0x02, 0x3C}, {0x02, 0x44}, {0x02, 0x4C}, {0x02, 0x54}, {0x02, 0x5C}, {0x02, 0x64}, {0x02, 0x6D},
	{0x02, 0x75}, {0x02, 0x7D}, {0x02, 0x86}, {0x02, 0x8E}, {0x02, 0x96}, {0x02, 0x9F}, {0x02, 0xA7}, {0x02, 0xAF}, {0x02, 0xB8}, {0x02, 0xC0},
	{0x02, 0xC9}, {0x02, 0xD1}, {0x02, 0xDA}, {0x02, 0xE3}, {0x02, 0xEB}, {0x02, 0xF4}, {0x02, 0xFC}, {0x03, 0x05}, {0x03, 0x0E}, {0x03, 0x16},
	{0x03, 0x1F}, {0x03, 0x28}, {0x03, 0x31}, {0x03, 0x39}, {0x03, 0x42}, {0x03, 0x4B}, {0x03, 0x54}, {0x03, 0x5D}, {0x03, 0x66}, {0x03, 0x6F},
	{0x03, 0x78}, {0x03, 0x81}, {0x03, 0x89}, {0x03, 0x92}, {0x03, 0x9B}, {0x03, 0xA3}, {0x03, 0xAC}, {0x03, 0xB4}, {0x03, 0xBD}, {0x03, 0xC5},
	{0x03, 0xCE}, {0x03, 0xD6}, {0x03, 0xDF}, {0x03, 0xE7}, {0x03, 0xF0}, {0x03, 0xF9}, {0x04, 0x01}, {0x04, 0x0A}, {0x04, 0x13}, {0x04, 0x1B},
	{0x04, 0x24}, {0x04, 0x2D}, {0x04, 0x35}, {0x04, 0x3E}, {0x04, 0x47}, {0x04, 0x50}, {0x04, 0x58}, {0x04, 0x61}, {0x04, 0x6A}, {0x04, 0x73},
	{0x04, 0x7C}, {0x04, 0x85}, {0x04, 0x8E}, {0x04, 0x96}, {0x04, 0x9F}, {0x04, 0xA8}, {0x04, 0xB1}, {0x04, 0xBA}, {0x04, 0xC3}, {0x04, 0xCC},
	{0x04, 0xD5}, {0x04, 0xDE}, {0x04, 0xE7}, {0x04, 0xF1}, {0x04, 0xFA}, {0x05, 0x03}, {0x05, 0x0C}, {0x05, 0x15}, {0x05, 0x1E}, {0x05, 0x27},
	{0x05, 0x31}, {0x05, 0x3A}, {0x05, 0x43}, {0x05, 0x4C}, {0x05, 0x55}, {0x05, 0x5F}, {0x05, 0x68}, {0x05, 0x71}, {0x05, 0x7B}, {0x05, 0x84},
	{0x05, 0x8D}, {0x05, 0x97}, {0x05, 0xA0}, {0x05, 0xA9}, {0x05, 0xB3}, {0x05, 0xBC}, {0x05, 0xC5}, {0x05, 0xCF}, {0x05, 0xD8}, {0x05, 0xE2},
	{0x05, 0xEB}, {0x05, 0xF5}, {0x05, 0xFE}, {0x06, 0x08}, {0x06, 0x11}, {0x06, 0x1B}, {0x06, 0x24}, {0x06, 0x2E}, {0x06, 0x38}, {0x06, 0x41},
	{0x06, 0x4B}, {0x06, 0x54}, {0x06, 0x5E}, {0x06, 0x68}, {0x06, 0x71}, {0x06, 0x7B}, {0x06, 0x85}, {0x06, 0x8F}, {0x06, 0x98}, {0x06, 0xA2},
	{0x06, 0xAC}, {0x06, 0xB6}, {0x06, 0xBF}, {0x06, 0xC9}, {0x06, 0xD3}, {0x06, 0xDD}, {0x06, 0xE7}, {0x06, 0xF0}, {0x06, 0xFA}, {0x07, 0x04},
	{0x07, 0x0E}, {0x07, 0x18}, {0x07, 0x22}, {0x07, 0x2C}, {0x07, 0x36}, {0x07, 0x40}, {0x07, 0x4A}, {0x07, 0x54}, {0x07, 0x5E}, {0x07, 0x68},
	{0x07, 0x72}, {0x07, 0x7C}, {0x07, 0x86}, {0x07, 0x90}, {0x07, 0x9A}, {0x07, 0xA4}, {0x07, 0xAE}, {0x07, 0xB8}, {0x07, 0xC2}, {0x07, 0xCC},
	{0x07, 0xD6}, {0x07, 0xE0}, {0x07, 0xEB}, {0x07, 0xF5}, {0x07, 0xFF},

	{0x08, 0x00}, {0x08, 0x0C}, {0x08, 0x15}, {0x08, 0x1E}, {0x08, 0x27}, {0x08, 0x30}, {0x08, 0x39}, {0x08, 0x42}, {0x08, 0x4B}, {0x08, 0x54},
	{0x08, 0x5D}, {0x08, 0x66}, {0x08, 0x6F}, {0x08, 0x78}, {0x08, 0x81}, {0x08, 0x8A}, {0x08, 0x93}, {0x08, 0x9C}, {0x08, 0xA5}, {0x08, 0xAF},
	{0x08, 0xB8}, {0x08, 0xC1}, {0x08, 0xCA}, {0x08, 0xD3}, {0x08, 0xDC}, {0x08, 0xE5}, {0x08, 0xEE}, {0x08, 0xF7}, {0x09, 0x00}, {0x09, 0x09},
	{0x09, 0x12}, {0x09, 0x1B}, {0x09, 0x24}, {0x09, 0x2D}, {0x09, 0x36}, {0x09, 0x3F}, {0x09, 0x48}, {0x09, 0x51}, {0x09, 0x5A}, {0x09, 0x63},
	{0x09, 0x6C}, {0x09, 0x75}, {0x09, 0x7E}, {0x09, 0x87}, {0x09, 0x90}, {0x09, 0x99}, {0x09, 0xA2}, {0x09, 0xAB}, {0x09, 0xB4}, {0x09, 0xBD},
	{0x09, 0xC6}, {0x09, 0xCF}, {0x09, 0xD8}, {0x09, 0xE1}, {0x09, 0xEA}, {0x09, 0xF3}, {0x09, 0xFC}, {0x0A, 0x05}, {0x0A, 0x0E}, {0x0A, 0x17},
	{0x0A, 0x20}, {0x0A, 0x29}, {0x0A, 0x32}, {0x0A, 0x3B}, {0x0A, 0x44}, {0x0A, 0x4D}, {0x0A, 0x56}, {0x0A, 0x5F}, {0x0A, 0x68}, {0x0A, 0x71},
	{0x0A, 0x7A}, {0x0A, 0x83}, {0x0A, 0x8C}, {0x0A, 0x95}, {0x0A, 0x9E}, {0x0A, 0xA7}, {0x0A, 0xB0}, {0x0A, 0xB9}, {0x0A, 0xC2}, {0x0A, 0xCB},
	{0x0A, 0xD4}, {0x0A, 0xDD}, {0x0A, 0xE6}, {0x0A, 0xEF}, {0x0A, 0xF8}, {0x0B, 0x01}, {0x0B, 0x0A}, {0x0B, 0x14}, {0x0B, 0x1D}, {0x0B, 0x26},
	{0x0B, 0x2F}, {0x0B, 0x38}, {0x0B, 0x41}, {0x0B, 0x4A}, {0x0B, 0x53}, {0x0B, 0x5C}, {0x0B, 0x65}, {0x0B, 0x6E}, {0x0B, 0x77}, {0x0B, 0x80},
	{0x0B, 0x89}, {0x0B, 0x92}, {0x0B, 0x9B}, {0x0B, 0xA4}, {0x0B, 0xAD}, {0x0B, 0xB6}, {0x0B, 0xBF}, {0x0B, 0xC8}, {0x0B, 0xD1}, {0x0B, 0xDA},
	{0x0B, 0xE3}, {0x0B, 0xEC}, {0x0B, 0xF5}, {0x0B, 0xFE}, {0x0C, 0x07}, {0x0C, 0x10}, {0x0C, 0x19}, {0x0C, 0x22}, {0x0C, 0x2B}, {0x0C, 0x34},
	{0x0C, 0x3D}, {0x0C, 0x46}, {0x0C, 0x4F}, {0x0C, 0x58}, {0x0C, 0x61}, {0x0C, 0x6A}, {0x0C, 0x73}, {0x0C, 0x7C}, {0x0C, 0x85}, {0x0C, 0x8E},
	{0x0C, 0x97}, {0x0C, 0xA0}, {0x0C, 0xA9}, {0x0C, 0xB2}, {0x0C, 0xBB}, {0x0C, 0xC4}, {0x0C, 0xCD}, {0x0C, 0xD6}, {0x0C, 0xDF}, {0x0C, 0xE8},
	{0x0C, 0xF1}, {0x0C, 0xFA}, {0x0D, 0x03}, {0x0D, 0x0C}, {0x0D, 0x15}, {0x0D, 0x1E}, {0x0D, 0x27}, {0x0D, 0x30}, {0x0D, 0x39}, {0x0D, 0x42},
	{0x0D, 0x4B}, {0x0D, 0x54}, {0x0D, 0x5D}, {0x0D, 0x66}, {0x0D, 0x6F}, {0x0D, 0x79}, {0x0D, 0x82}, {0x0D, 0x8B}, {0x0D, 0x94}, {0x0D, 0x9D},
	{0x0D, 0xA6}, {0x0D, 0xAF}, {0x0D, 0xB8}, {0x0D, 0xC1}, {0x0D, 0xCA}, {0x0D, 0xD3}, {0x0D, 0xDC}, {0x0D, 0xE5}, {0x0D, 0xEE}, {0x0D, 0xF7},
	{0x0E, 0x00}, {0x0E, 0x09}, {0x0E, 0x12}, {0x0E, 0x1B}, {0x0E, 0x24}, {0x0E, 0x2D}, {0x0E, 0x36}, {0x0E, 0x3F}, {0x0E, 0x48}, {0x0E, 0x51},
	{0x0E, 0x5A}, {0x0E, 0x63}, {0x0E, 0x6C}, {0x0E, 0x75}, {0x0E, 0x7E}, {0x0E, 0x87}, {0x0E, 0x90}, {0x0E, 0x99}, {0x0E, 0xA2}, {0x0E, 0xAB},
	{0x0E, 0xB4}, {0x0E, 0xBD}, {0x0E, 0xC6}, {0x0E, 0xCF}, {0x0E, 0xD8}, {0x0E, 0xE1}, {0x0E, 0xEA}, {0x0E, 0xF3}, {0x0E, 0xFC}, {0x0F, 0x05},
	{0x0F, 0x0E}, {0x0F, 0x17}, {0x0F, 0x20}, {0x0F, 0x29}, {0x0F, 0x32}, {0x0F, 0x3B}, {0x0F, 0x44}, {0x0F, 0x4D}, {0x0F, 0x56}, {0x0F, 0x5F},
	{0x0F, 0x68}, {0x0F, 0x71}, {0x0F, 0x7A}, {0x0F, 0x83}, {0x0F, 0x8C}, {0x0F, 0x95}, {0x0F, 0x9E}, {0x0F, 0xA7}, {0x0F, 0xB0}, {0x0F, 0xB9},
	{0x0F, 0xC2}, {0x0F, 0xCB}, {0x0F, 0xD5}, {0x0F, 0xDE}, {0x0F, 0xE7}, {0x0F, 0xF0}, {0x0F, 0xF9}, {0x0F, 0xFF},
};

static u8 a15_hbm_transition_table[SMOOTH_TRANS_MAX][MAX_S6E8FC3_NORMAL_HBM_TRANSITION][1] = {
	[SMOOTH_TRANS_OFF] = {
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_NORMAL] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_HBM] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_NORMAL] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_HBM] = { 0x20 },
	},
	[SMOOTH_TRANS_ON] = {
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_NORMAL] = { 0x28 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_HBM] = { 0x28 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_NORMAL] = { 0x28 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_HBM] = { 0x28 },
	},
};

static u8 a15_acl_frame_avg_table[][1] = {
	{ 0x44 }, /* 16 Frame Avg */
	{ 0x55 }, /* 32 Frame Avg */
};

static u8 a15_acl_start_point_table[][2] = {
	[OLED_BR_HBM_OFF] = { 0x00, 0xB0 }, /* 50 Percent */
	[OLED_BR_HBM_ON] = { 0x00, 0xB0 }, /* 50 Percent */
};

static u8 a15_acl_dim_speed_table[MAX_S6E8FC3_ACL_DIM][1] = {
	[S6E8FC3_ACL_DIM_OFF] = { 0x00 }, /* 0x00 : ACL Dimming Off */
	[S6E8FC3_ACL_DIM_ON] = { 0x20 }, /* 0x20 : ACL Dimming 32 Frames */
};

static u8 a15_acl_opr_table[MAX_S6E8FC3_ACL_OPR][1] = {
	[S6E8FC3_ACL_OPR_0] = { 0x00 }, /* ACL OFF, OPR 0% */
	[S6E8FC3_ACL_OPR_1] = { 0x01 }, /* ACL ON, OPR 8% */
	[S6E8FC3_ACL_OPR_2] = { 0x03 }, /* ACL ON, OPR 15% */
	[S6E8FC3_ACL_OPR_3] = { 0x03 }, /* ACL ON, OPR 15% */
};

static u8 a15_tset_table[256][1] = {
	/* 0 ~ 127 */
	{ 0x00 }, { 0x01 }, { 0x02 }, { 0x03 }, { 0x04 }, { 0x05 }, { 0x06 }, { 0x07 },
	{ 0x08 }, { 0x09 }, { 0x0A }, { 0x0B }, { 0x0C }, { 0x0D }, { 0x0E }, { 0x0F },
	{ 0x10 }, { 0x11 }, { 0x12 }, { 0x13 }, { 0x14 }, { 0x15 }, { 0x16 }, { 0x17 },
	{ 0x18 }, { 0x19 }, { 0x1A }, { 0x1B }, { 0x1C }, { 0x1D }, { 0x1E }, { 0x1F },
	{ 0x20 }, { 0x21 }, { 0x22 }, { 0x23 }, { 0x24 }, { 0x25 }, { 0x26 }, { 0x27 },
	{ 0x28 }, { 0x29 }, { 0x2A }, { 0x2B }, { 0x2C }, { 0x2D }, { 0x2E }, { 0x2F },
	{ 0x30 }, { 0x31 }, { 0x32 }, { 0x33 }, { 0x34 }, { 0x35 }, { 0x36 }, { 0x37 },
	{ 0x38 }, { 0x39 }, { 0x3A }, { 0x3B }, { 0x3C }, { 0x3D }, { 0x3E }, { 0x3F },
	{ 0x40 }, { 0x41 }, { 0x42 }, { 0x43 }, { 0x44 }, { 0x45 }, { 0x46 }, { 0x47 },
	{ 0x48 }, { 0x49 }, { 0x4A }, { 0x4B }, { 0x4C }, { 0x4D }, { 0x4E }, { 0x4F },
	{ 0x50 }, { 0x51 }, { 0x52 }, { 0x53 }, { 0x54 }, { 0x55 }, { 0x56 }, { 0x57 },
	{ 0x58 }, { 0x59 }, { 0x5A }, { 0x5B }, { 0x5C }, { 0x5D }, { 0x5E }, { 0x5F },
	{ 0x60 }, { 0x61 }, { 0x62 }, { 0x63 }, { 0x64 }, { 0x65 }, { 0x66 }, { 0x67 },
	{ 0x68 }, { 0x69 }, { 0x6A }, { 0x6B }, { 0x6C }, { 0x6D }, { 0x6E }, { 0x6F },
	{ 0x70 }, { 0x71 }, { 0x72 }, { 0x73 }, { 0x74 }, { 0x75 }, { 0x76 }, { 0x77 },
	{ 0x78 }, { 0x79 }, { 0x7A }, { 0x7B }, { 0x7C }, { 0x7D }, { 0x7E }, { 0x7F },
	/* 0 ~ -127 */
	{ 0x80 }, { 0x81 }, { 0x82 }, { 0x83 }, { 0x84 }, { 0x85 }, { 0x86 }, { 0x87 },
	{ 0x88 }, { 0x89 }, { 0x8A }, { 0x8B }, { 0x8C }, { 0x8D }, { 0x8E }, { 0x8F },
	{ 0x90 }, { 0x91 }, { 0x92 }, { 0x93 }, { 0x94 }, { 0x95 }, { 0x96 }, { 0x97 },
	{ 0x98 }, { 0x99 }, { 0x9A }, { 0x9B }, { 0x9C }, { 0x9D }, { 0x9E }, { 0x9F },
	{ 0xA0 }, { 0xA1 }, { 0xA2 }, { 0xA3 }, { 0xA4 }, { 0xA5 }, { 0xA6 }, { 0xA7 },
	{ 0xA8 }, { 0xA9 }, { 0xAA }, { 0xAB }, { 0xAC }, { 0xAD }, { 0xAE }, { 0xAF },
	{ 0xB0 }, { 0xB1 }, { 0xB2 }, { 0xB3 }, { 0xB4 }, { 0xB5 }, { 0xB6 }, { 0xB7 },
	{ 0xB8 }, { 0xB9 }, { 0xBA }, { 0xBB }, { 0xBC }, { 0xBD }, { 0xBE }, { 0xBF },
	{ 0xC0 }, { 0xC1 }, { 0xC2 }, { 0xC3 }, { 0xC4 }, { 0xC5 }, { 0xC6 }, { 0xC7 },
	{ 0xC8 }, { 0xC9 }, { 0xCA }, { 0xCB }, { 0xCC }, { 0xCD }, { 0xCE }, { 0xCF },
	{ 0xD0 }, { 0xD1 }, { 0xD2 }, { 0xD3 }, { 0xD4 }, { 0xD5 }, { 0xD6 }, { 0xD7 },
	{ 0xD8 }, { 0xD9 }, { 0xDA }, { 0xDB }, { 0xDC }, { 0xDD }, { 0xDE }, { 0xDF },
	{ 0xE0 }, { 0xE1 }, { 0xE2 }, { 0xE3 }, { 0xE4 }, { 0xE5 }, { 0xE6 }, { 0xE7 },
	{ 0xE8 }, { 0xE9 }, { 0xEA }, { 0xEB }, { 0xEC }, { 0xED }, { 0xEE }, { 0xEF },
	{ 0xF0 }, { 0xF1 }, { 0xF2 }, { 0xF3 }, { 0xF4 }, { 0xF5 }, { 0xF6 }, { 0xF7 },
	{ 0xF8 }, { 0xF9 }, { 0xFA }, { 0xFB }, { 0xFC }, { 0xFD }, { 0xFE }, { 0xFF },
};

static u8 a15_fps_table_1[][1] = {
	[S6E8FC3_VRR_FPS_90] = { 0x01 },
	[S6E8FC3_VRR_FPS_60] = { 0x21 },
};

static u8 a15_dimming_speed_table_1[SMOOTH_TRANS_MAX][MAX_S6E8FC3_NORMAL_HBM_TRANSITION][1] = {
	[SMOOTH_TRANS_OFF] = {
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_NORMAL] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_HBM] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_NORMAL] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_HBM] = { 0x20 },
	},
	[SMOOTH_TRANS_ON] = {
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_NORMAL] = { 0x60 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_HBM] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_NORMAL] = { 0x20 },
		[S6E8FC3_NORMAL_HBM_TRANSITION_HBM_TO_HBM] = { 0x60 },
	},
};

static u8 a15_irc_mode_table[][6] = {
	[0 ... S6E8FC3_TOTAL_STEP - 2] = { 0x2F, 0xD3, 0x03, 0x64, 0x80, 0x01 },		/* flat gamma */
	[S6E8FC3_TOTAL_STEP - 1] = { 0x6F, 0xD3, 0x03, 0x00, 0x80, 0x01 },		/* moderato */
};

static struct maptbl a15_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(a15_brt_table, &DDI_FUNC(S6E8FC3_MAPTBL_INIT_GAMMA_MODE2_BRT), S6E8FC3_A15_BR_INDEX_PROPERTY),
	[HBM_ONOFF_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_hbm_transition_table, PANEL_BL_PROPERTY_SMOOTH_TRANSITION, S6E8FC3_NORMAL_HBM_TRANSITION_PROPERTY),
	[ACL_FRAME_AVG_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_acl_frame_avg_table, PANEL_BL_PROPERTY_ACL_PWRSAVE),
	[ACL_START_POINT_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_acl_start_point_table, OLED_BR_HBM_PROPERTY),
	[ACL_DIM_SPEED_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_acl_dim_speed_table, S6E8FC3_ACL_DIM_PROPERTY),
	[ACL_OPR_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_acl_opr_table, S6E8FC3_ACL_OPR_PROPERTY),
	[TSET_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_tset_table, OLED_TSET_PROPERTY),
	[FPS_MAPTBL_1] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_fps_table_1, S6E8FC3_VRR_PROPERTY),
	[DIMMING_SPEED] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_dimming_speed_table_1, PANEL_BL_PROPERTY_SMOOTH_TRANSITION, S6E8FC3_NORMAL_HBM_TRANSITION_PROPERTY),
	[IRC_MODE_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(a15_irc_mode_table, S6E8FC3_A15_BR_INDEX_PROPERTY),
};

/* ===================================================================================== */
/* ============================== [S6E8FC3 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A15_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A15_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A15_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 A15_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A15_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A15_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A15_SLEEP_OUT[] = { 0x11 };
static u8 A15_SLEEP_IN[] = { 0x10 };
static u8 A15_DISPLAY_OFF[] = { 0x28 };
static u8 A15_DISPLAY_ON[] = { 0x29 };


static DEFINE_STATIC_PACKET(a15_level1_key_enable, DSI_PKT_TYPE_WR, A15_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a15_level2_key_enable, DSI_PKT_TYPE_WR, A15_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a15_level3_key_enable, DSI_PKT_TYPE_WR, A15_KEY3_ENABLE, 0);

static DEFINE_STATIC_PACKET(a15_level1_key_disable, DSI_PKT_TYPE_WR, A15_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a15_level2_key_disable, DSI_PKT_TYPE_WR, A15_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a15_level3_key_disable, DSI_PKT_TYPE_WR, A15_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a15_sleep_out, DSI_PKT_TYPE_WR, A15_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a15_sleep_in, DSI_PKT_TYPE_WR, A15_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a15_display_on, DSI_PKT_TYPE_WR, A15_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a15_display_off, DSI_PKT_TYPE_WR, A15_DISPLAY_OFF, 0);

static u8 A15_TSET_SET[] = {
	0xCD,
	0x00,
};
static DEFINE_PKTUI(a15_tset_set, &a15_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a15_tset_set, DSI_PKT_TYPE_WR, A15_TSET_SET, 0x0F);

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static DEFINE_PANEL_MDELAY(a15_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a15_wait_6msec, 6);
#endif
static DEFINE_PANEL_MDELAY(a15_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a15_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a15_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(a15_wait_17msec, 17);

static DEFINE_PANEL_MDELAY(a15_wait_34msec, 34);

static DEFINE_PANEL_MDELAY(a15_wait_sleep_out_20msec, 20);
static DEFINE_PANEL_MDELAY(a15_wait_display_off, 20);

static DEFINE_PANEL_MDELAY(a15_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a15_wait_1usec, 1);


static DEFINE_PANEL_KEY(a15_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a15_level1_key_enable));
static DEFINE_PANEL_KEY(a15_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a15_level2_key_enable));
static DEFINE_PANEL_KEY(a15_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a15_level3_key_enable));

static DEFINE_PANEL_KEY(a15_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a15_level1_key_disable));
static DEFINE_PANEL_KEY(a15_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a15_level2_key_disable));
static DEFINE_PANEL_KEY(a15_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a15_level3_key_disable));

static DEFINE_PANEL_VSYNC_DELAY(a15_wait_1_vsync, 1);
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static DEFINE_PANEL_VSYNC_DELAY(a15_wait_2_vsync, 2);
static DEFINE_RULE_BASED_COND(a15_cond_is_60hz,
		PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 60);
static DEFINE_RULE_BASED_COND(a15_cond_is_90hz,
		PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 90);
#endif

static u8 A15_HBM_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(a15_hbm_transition, &a15_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a15_hbm_transition, DSI_PKT_TYPE_WR, A15_HBM_TRANSITION, 0);

static u8 A15_ACL_SET[] = {
	0xB4,
	0x44, 0x00, 0xB0, 0x57, 0x66, 0xA1, 0x15,
	0x55, 0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48,
	0x40, 0x00, 0x00
};

/*
 * ACL SETTING
 * 1st Para.
 * 0x44 : 16 Frame Average
 * 0x55 : 32 Frame Average
 * 2nd 3rd Para
 * 0x00, 0xB0 : ACL Start Step 50%
 * 0x40, 0x28 : ACL Start Step 60%
 * 17th Para.
 * 0x00 : ACL Dimming Off
 * 0x20 : ACL 32 Frame Dimming
 */

static DECLARE_PKTUI(a15_acl_set) = {
	{ .offset = 1, .maptbl = &a15_maptbl[ACL_FRAME_AVG_MAPTBL] },
	{ .offset = 2, .maptbl = &a15_maptbl[ACL_START_POINT_MAPTBL] },
	{ .offset = 17, .maptbl = &a15_maptbl[ACL_DIM_SPEED_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a15_acl_set, DSI_PKT_TYPE_WR, A15_ACL_SET, 0x0D);

static u8 A15_ACL[] = {
	0x55, 0x03
};
static DEFINE_PKTUI(a15_acl_control, &a15_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a15_acl_control, DSI_PKT_TYPE_WR, A15_ACL, 0);

static u8 A15_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(a15_wrdisbv, &a15_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a15_wrdisbv, DSI_PKT_TYPE_WR, A15_WRDISBV, 0);

static u8 A15_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 A15_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };
static DEFINE_STATIC_PACKET(a15_caset, DSI_PKT_TYPE_WR, A15_CASET, 0);
static DEFINE_STATIC_PACKET(a15_paset, DSI_PKT_TYPE_WR, A15_PASET, 0);

#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
static u8 A15_FAST_DISCHARGE[] = {
	0xB5,
	0x40, 0x80	/* FD disable */
};
#else
static u8 A15_FAST_DISCHARGE[] = {
	0xB5,
	0x40, 0x40	/* FD enable */
};
#endif
static DEFINE_STATIC_PACKET(a15_fast_discharge, DSI_PKT_TYPE_WR, A15_FAST_DISCHARGE, 0x0A);

static u8 A15_SMOOTH_DIMMING_1F[] = {
	0xB2,
	0x01,
};
static DEFINE_STATIC_PACKET(a15_smooth_dimming_1f, DSI_PKT_TYPE_WR, A15_SMOOTH_DIMMING_1F, 0x0D);

static u8 A15_SMOOTH_DIMMING_15F[] = {
	0xB2,
	0x0F,
};
static DEFINE_STATIC_PACKET(a15_smooth_dimming_15f, DSI_PKT_TYPE_WR, A15_SMOOTH_DIMMING_15F, 0x0D);
static u8 A15_PCD_SET_DET_LOW[] = {
	0xEA,
	0x5C, 0x51	/* 1st 0x5C: default HIGH, 2nd 0x51 : Enable SW RESET */
};
static DEFINE_STATIC_PACKET(a15_pcd_det_set, DSI_PKT_TYPE_WR, A15_PCD_SET_DET_LOW, 0);

static u8 A15_ERR_FG_ON[] = {
	0xE1, 0x83
};
static DEFINE_STATIC_PACKET(a15_err_fg_on, DSI_PKT_TYPE_WR, A15_ERR_FG_ON, 0);

static u8 A15_ERR_FG_OFF[] = {
	0xE1, 0x03
};
static DEFINE_STATIC_PACKET(a15_err_fg_off, DSI_PKT_TYPE_WR, A15_ERR_FG_OFF, 0);

static u8 A15_ERR_FG_SETTING_1[] = {
	0xED,
	0x01, 0x26,
	/* Vlin1, ELVDD, Vlin3 Monitor On */
	/* Defalut LOW */
};
static DEFINE_STATIC_PACKET(a15_err_fg_setting_1, DSI_PKT_TYPE_WR, A15_ERR_FG_SETTING_1, 0);

static u8 A15_ERR_FG_SETTING_2[] = {
	0xF4, 0x1C
};
static DEFINE_STATIC_PACKET(a15_err_fg_setting_2, DSI_PKT_TYPE_WR, A15_ERR_FG_SETTING_2, 0x06);

static u8 A15_VSYNC_SET[] = {
	0xF2,
	0x54
};
static DEFINE_STATIC_PACKET(a15_vsync_set, DSI_PKT_TYPE_WR, A15_VSYNC_SET, 0x04);

static u8 A15_FREQ_SET[] = {
	0xF2,
	0x00
};
static DEFINE_STATIC_PACKET(a15_freq_set, DSI_PKT_TYPE_WR, A15_FREQ_SET, 0x27);

static u8 A15_PORCH_SET[] = {
	0xF2,
	0x55
};
static DEFINE_STATIC_PACKET(a15_porch_set, DSI_PKT_TYPE_WR, A15_PORCH_SET, 0x2E);

static u8 A15_FPS_1[] = { 0x60, 0x00, 0x00};
static DEFINE_PKTUI(a15_fps_1, &a15_maptbl[FPS_MAPTBL_1], 1);
static DEFINE_VARIABLE_PACKET(a15_fps_1, DSI_PKT_TYPE_WR, A15_FPS_1, 0);

static u8 A15_PANEL_UPDATE[] = {
	0xF7,
	0x0B
};
static DEFINE_STATIC_PACKET(a15_panel_update, DSI_PKT_TYPE_WR, A15_PANEL_UPDATE, 0);

static u8 A15_GLOBAL_PARAM_SETTING[] = {
	0xF2,
	0x00, 0x05, 0x0E, 0x58, 0x50, 0x00, 0x0C, 0x00,
	0x04, 0x30, 0xB1, 0x30, 0xB1, 0x0C, 0x04, 0xBC,
	0x26, 0xE9, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};
static DEFINE_STATIC_PACKET(a15_global_param_setting, DSI_PKT_TYPE_WR, A15_GLOBAL_PARAM_SETTING, 0);

static u8 A15_MIN_ROI_SETTING[] = {
	0xC2,
	0x1B, 0x41, 0xB0, 0x0E, 0x00, 0x3C, 0x5A, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(a15_min_roi_setting, DSI_PKT_TYPE_WR, A15_MIN_ROI_SETTING, 0);


/* WARNING - S6E8FC3 using normal data type for DSC setting */
static u8 A15_DSC[] = { 0x9D, 0x01 };
static DEFINE_STATIC_PACKET(a15_dsc, DSI_PKT_TYPE_WR, A15_DSC, 0);
static u8 A15_PPS[] = {
	//1080x2340 Slice Info : 540x117 / 2 slice mode
	0x9E,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
	0x04, 0x38, 0x00, 0x2D, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x04, 0x5A,
	0x00, 0x07, 0x00, 0x0C, 0x02, 0x2F, 0x02, 0x43,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00
};
static DEFINE_STATIC_PACKET(a15_pps, DSI_PKT_TYPE_WR, A15_PPS, 0);

static u8 A15_NORMAL_MODE[] = {
	0x53, 0x20

};
static DEFINE_STATIC_PACKET(a15_normal_mode, DSI_PKT_TYPE_WR, A15_NORMAL_MODE, 0);

static u8 A15_BLACK_INSERT_OFF[] = { 0xF2, 0x0C };
static DEFINE_STATIC_PACKET(a15_black_insert_off, DSI_PKT_TYPE_WR, A15_BLACK_INSERT_OFF, 0);

static u8 A15_TSP_VSYNC_ON[] = {
	0xE8,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01
};
static DEFINE_STATIC_PACKET(a15_tsp_vsync_on, DSI_PKT_TYPE_WR, A15_TSP_VSYNC_ON, 0);

static u8 A15_WO_DSC[] = {
	0xC3,
	0x04, /* 0x02 : w/o DSC 0x04 : w/ DSC */
};
static DEFINE_STATIC_PACKET(a15_wo_dsc, DSI_PKT_TYPE_WR, A15_WO_DSC, 0x8A);

static u8 A15_OSC_SETTING_1[] = {
	0xD7,
	0x90
};
static DEFINE_STATIC_PACKET(a15_osc_setting_1, DSI_PKT_TYPE_WR, A15_OSC_SETTING_1, 0x1B);

static u8 A15_OSC_SETTING_2[] = {
	0xFE,
	0xB0
};
static DEFINE_STATIC_PACKET(a15_osc_setting_2, DSI_PKT_TYPE_WR, A15_OSC_SETTING_2, 0);

static u8 A15_OSC_SETTING_3[] = {
	0xFE,
	0x30
};
static DEFINE_STATIC_PACKET(a15_osc_setting_3, DSI_PKT_TYPE_WR, A15_OSC_SETTING_3, 0);

/* 1 ~ 3 has same value */
static u8 A15_ETC_SETTING_1[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a15_etc_setting_1, DSI_PKT_TYPE_WR, A15_ETC_SETTING_1, 0x15);

static u8 A15_ETC_SETTING_2[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a15_etc_setting_2, DSI_PKT_TYPE_WR, A15_ETC_SETTING_2, 0x28);

static u8 A15_ETC_SETTING_3[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a15_etc_setting_3, DSI_PKT_TYPE_WR, A15_ETC_SETTING_3, 0x3B);

static u8 A15_SEED_SETTING[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x5F, 0x8F, 0x00, 0x5F, 0x8F
};
static DEFINE_STATIC_PACKET(a15_seed_setting, DSI_PKT_TYPE_WR, A15_SEED_SETTING, 0);

static u8 A15_FFC_DEFAULT[] = {
	0xDF,
	0x09, 0x30, 0x95, 0x57, 0x0B, 0x57, 0x0B /* 898 Mbps */
};
static DEFINE_STATIC_PACKET(a15_ffc_default, DSI_PKT_TYPE_WR, A15_FFC_DEFAULT, 0);

static u8 A15_IRC_MDOE[] = {
	0xB1,
	0x2F, 0xD3, 0x03, 0x64, 0x80, 0x01	/* Flat mode */
};
static DEFINE_PKTUI(a15_irc_mode, &a15_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a15_irc_mode, DSI_PKT_TYPE_WR, A15_IRC_MDOE, 0x89);

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 A15_KEY_F1_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 A15_KEY_F1_DISABLE[] = { 0xF1, 0xA5, 0xA5 };
static DEFINE_STATIC_PACKET(a15_key_f1_enable, DSI_PKT_TYPE_WR, A15_KEY_F1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a15_key_f1_disable, DSI_PKT_TYPE_WR, A15_KEY_F1_DISABLE, 0);

static u8 A15_CCD_ENABLE[] = { 0xEA, 0x5C, 0x51, 0x01, 0x00 };
static u8 A15_CCD_DISABLE[] = { 0xEA, 0x5C, 0x51, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a15_ccd_test_enable, DSI_PKT_TYPE_WR, A15_CCD_ENABLE, 0x00);
static DEFINE_STATIC_PACKET(a15_ccd_test_disable, DSI_PKT_TYPE_WR, A15_CCD_DISABLE, 0x00);
#endif

static DEFINE_PNOBJ_CONFIG(a15_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(a15_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static struct seqinfo SEQINFO(a15_set_bl_param_seq);
static struct seqinfo SEQINFO(a15_set_fps_param_seq);
#if defined(CONFIG_USDM_FACTORY)
static struct seqinfo SEQINFO(a15_res_init_seq);
#endif

static DEFINE_PNOBJ_CONFIG(a15_set_separate_tx_off, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_OFF);
static DEFINE_PNOBJ_CONFIG(a15_set_separate_tx_on, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_ON);

static void *a15_common_setting_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),

	&s6e8fc3_restbl[RES_ID],

	&PKTINFO(a15_dsc),
	&PKTINFO(a15_pps),

	&PKTINFO(a15_wo_dsc),
	&PKTINFO(a15_black_insert_off),
	&PKTINFO(a15_panel_update),
	&PKTINFO(a15_ffc_default),
	&PKTINFO(a15_tsp_vsync_on),
	&PKTINFO(a15_acl_set),

	&PKTINFO(a15_err_fg_setting_1),
	&PKTINFO(a15_err_fg_setting_2),
	&PKTINFO(a15_err_fg_on),
	&PKTINFO(a15_pcd_det_set),

	&PKTINFO(a15_smooth_dimming_1f),
	&PKTINFO(a15_panel_update),

	&SEQINFO(a15_set_bl_param_seq), /* includes FPS setting also */

	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};

static DEFINE_SEQINFO(a15_common_setting_seq,
		a15_common_setting_cmdtbl);

static void *a15_init_cmdtbl[] = {
	&PNOBJ_CONFIG(a15_set_separate_tx_on),
	&DLYINFO(a15_wait_10msec),
	&PKTINFO(a15_sleep_out),
	&DLYINFO(a15_wait_100msec),

	&SEQINFO(a15_common_setting_seq),

#if defined(CONFIG_USDM_FACTORY)
	&SEQINFO(a15_res_init_seq),
#endif
	&PNOBJ_CONFIG(a15_set_separate_tx_off),
};

static void *a15_res_init_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&s6e8fc3_restbl[RES_ID],
	&s6e8fc3_restbl[RES_COORDINATE],
	&s6e8fc3_restbl[RES_CODE],
	&s6e8fc3_restbl[RES_DATE],
	&s6e8fc3_restbl[RES_OCTA_ID],
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e8fc3_restbl[RES_SELF_DIAG],
	&s6e8fc3_restbl[RES_ERR_FG],
	&s6e8fc3_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};


#if defined(CONFIG_USDM_FACTORY)
static DEFINE_SEQINFO(a15_res_init_seq, a15_res_init_cmdtbl);
#endif

/* bl for normal */
static void *a15_set_bl_param_cmdtbl[] = {
	&PKTINFO(a15_fps_1),
	&PKTINFO(a15_tset_set),
	&PKTINFO(a15_acl_control),
	&PKTINFO(a15_hbm_transition),
	&PKTINFO(a15_wrdisbv),
	&PKTINFO(a15_irc_mode),
	&PKTINFO(a15_panel_update),
};

static DEFINE_SEQINFO(a15_set_bl_param_seq, a15_set_bl_param_cmdtbl);

static void *a15_set_bl_cmdtbl[] = {
	&KEYINFO(a15_level2_key_enable),
	&SEQINFO(a15_set_bl_param_seq),
	&KEYINFO(a15_level2_key_disable),
};

static DEFINE_RULE_BASED_COND(a15_cond_is_panel_state_not_lpm,
		PANEL_PROPERTY_PANEL_STATE, NE, PANEL_STATE_ALPM);
static DEFINE_RULE_BASED_COND(a15_cond_is_panel_state_lpm,
		PANEL_PROPERTY_PANEL_STATE, EQ, PANEL_STATE_ALPM);

static void *a15_set_fps_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&SEQINFO(a15_set_bl_param_seq),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};

static void *a15_display_on_cmdtbl[] = {
	&DLYINFO(a15_wait_sleep_out_20msec),
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&PKTINFO(a15_display_on),
	&DLYINFO(a15_wait_17msec),
	&PKTINFO(a15_smooth_dimming_15f),
	&PKTINFO(a15_panel_update),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};

static void *a15_display_off_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&PKTINFO(a15_err_fg_off),
	&PKTINFO(a15_display_off),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
	&DLYINFO(a15_wait_display_off),
};

static void *a15_exit_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e8fc3_dmptbl[DUMP_DSI_ERR],
	&s6e8fc3_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(a15_sleep_in),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
	&DLYINFO(a15_wait_sleep_in),
};

static void *a15_dump_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&s6e8fc3_dmptbl[DUMP_RDDPM],
	&s6e8fc3_dmptbl[DUMP_RDDSM],
	&s6e8fc3_dmptbl[DUMP_DSI_ERR],
	&s6e8fc3_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};


static void *a15_check_condition_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&s6e8fc3_dmptbl[DUMP_RDDPM],
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
};

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static void *a15_mask_layer_workaround_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	&PKTINFO(a15_wrdisbv),
	&PKTINFO(a15_hbm_transition),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),
	&DLYINFO(a15_wait_2_vsync),
};

static void *a15_mask_layer_enter_br_cmdtbl[] = {
	&DLYINFO(a15_wait_1_vsync),
	/* VFP start */
	&DLYINFO(a15_wait_1msec),
	/* VFP CMD TX ALLOW END */

	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	/* BL CMD */
	&SEQINFO(a15_set_bl_param_seq),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),

	/* VFP start */
	/* CMD will be flushed here */
	&CONDINFO_IF(a15_cond_is_90hz),
		&DLYINFO(a15_wait_1msec),
	&CONDINFO_FI(a15_cond_is_90hz),

	&CONDINFO_IF(a15_cond_is_60hz),
		&DLYINFO(a15_wait_6msec),
	&CONDINFO_FI(a15_cond_is_60hz),
	/* VSYNC start */
	/* DECON update for next frame */
};

static void *a15_mask_layer_exit_br_cmdtbl[] = {
	&KEYINFO(a15_level1_key_enable),
	&KEYINFO(a15_level2_key_enable),
	&KEYINFO(a15_level3_key_enable),
	/* BL CMD */
	&SEQINFO(a15_set_bl_param_seq),
	&KEYINFO(a15_level3_key_disable),
	&KEYINFO(a15_level2_key_disable),
	&KEYINFO(a15_level1_key_disable),

	/* VFP start */
	/* CMD will be flushed here */
	&CONDINFO_IF(a15_cond_is_90hz),
		&DLYINFO(a15_wait_1msec),
	&CONDINFO_FI(a15_cond_is_90hz),

	&CONDINFO_IF(a15_cond_is_60hz),
		&DLYINFO(a15_wait_6msec),
	&CONDINFO_FI(a15_cond_is_60hz),
	/* VSYNC start */
	/* DECON update for next frame */
};
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static void *a15_ccd_test_cmdtbl[] = {
	&PKTINFO(a15_key_f1_enable),
	&KEYINFO(a15_level3_key_enable),
	&PKTINFO(a15_ccd_test_enable),
	&DLYINFO(a15_wait_17msec),
	&s6e8fc3_restbl[RES_CCD_STATE],
	&PKTINFO(a15_ccd_test_disable),
	&PKTINFO(a15_key_f1_disable),
	&KEYINFO(a15_level3_key_disable),
};
#endif

static void *a15_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo a15_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, a15_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, a15_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, a15_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, a15_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, a15_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, a15_exit_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, a15_set_fps_cmdtbl),
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	SEQINFO_INIT(PANEL_MASK_LAYER_STOP_DIMMING_SEQ, a15_mask_layer_workaround_cmdtbl),
	SEQINFO_INIT(PANEL_MASK_LAYER_ENTER_BR_SEQ, a15_mask_layer_enter_br_cmdtbl),
	SEQINFO_INIT(PANEL_MASK_LAYER_EXIT_BR_SEQ, a15_mask_layer_exit_br_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	SEQINFO_INIT(PANEL_CCD_TEST_SEQ, a15_ccd_test_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_DUMP_SEQ, a15_dump_cmdtbl),
	SEQINFO_INIT(PANEL_CHECK_CONDITION_SEQ, a15_check_condition_cmdtbl),
	SEQINFO_INIT(PANEL_DUMMY_SEQ, a15_dummy_cmdtbl),
};

struct common_panel_info s6e8fc3_a15_panel_info = {
	.ldi_name = "s6e8fc3",
	.name = "s6e8fc3_a15",
	.model = "AMS638YQ01",
	.vendor = "SDC",
	.id = 0x800004,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_2BYTE_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
	},
	.ddi_ops = {
		.get_cell_id = s6e8fc3_get_cell_id,
		.get_octa_id = s6e8fc3_get_octa_id,
		.get_manufacture_code = s6e8fc3_get_manufacture_code,
		.get_manufacture_date = s6e8fc3_get_manufacture_date,
	},

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e8fc3_a15_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e8fc3_a15_default_resol),
		.resol = s6e8fc3_a15_default_resol,
	},
	.vrrtbl = s6e8fc3_a15_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e8fc3_a15_default_vrrtbl),
	.maptbl = a15_maptbl,
	.nr_maptbl = ARRAY_SIZE(a15_maptbl),
	.seqtbl = a15_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a15_seqtbl),
	.rditbl = s6e8fc3_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e8fc3_rditbl),
	.restbl = s6e8fc3_restbl,
	.nr_restbl = ARRAY_SIZE(s6e8fc3_restbl),
	.dumpinfo = s6e8fc3_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e8fc3_dmptbl),

#ifdef CONFIG_USDM_MDNIE
	.mdnie_tune = &s6e8fc3_a15_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e8fc3_a15_panel_dimming_info,
	},
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	.aod_tune = &s6e8fc3_a15_aod,
#endif
};

#endif /* __S6E8FC3_A15_PANEL_H__ */
