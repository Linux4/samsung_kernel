/*
 * linux/drivers/video/fbdev/exynos/panel/ili7806s/ili7806s_m12_panel.h
 *
 * Header file for ILI7806S Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7806S_M12_PANEL_H__
#define __ILI7806S_M12_PANEL_H__
#include "../panel_drv.h"
#include "ili7806s.h"

#include "ili7806s_m12_panel_dimming.h"
#include "ili7806s_m12_panel_i2c.h"

#include "ili7806s_m12_resol.h"

#undef __pn_name__
#define __pn_name__	m12

#undef __PN_NAME__
#define __PN_NAME__	M12

static struct seqinfo m12_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [ILI7806S READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [ILI7806S RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [ILI7806S MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 m12_brt_table[ILI7806S_TOTAL_NR_LUMINANCE][2] = {
	{0x00, 0x00},
	{0x00, 0x10},
	{0x00, 0x10},
	{0x00, 0x20},
	{0x00, 0x30},
	{0x00, 0x30},
	{0x00, 0x40},
	{0x00, 0x50},
	{0x00, 0x50},
	{0x00, 0x60},
	{0x00, 0x70},
	{0x00, 0x70},
	{0x00, 0x80},
	{0x00, 0x90},
	{0x00, 0x90},
	{0x00, 0xA0},
	{0x00, 0xB0},
	{0x00, 0xB0},
	{0x00, 0xC0},
	{0x00, 0xD0},
	{0x00, 0xE0},
	{0x00, 0xE0},
	{0x00, 0xF0},
	{0x01, 0x00},
	{0x01, 0x00},
	{0x01, 0x10},
	{0x01, 0x20},
	{0x01, 0x20},
	{0x01, 0x30},
	{0x01, 0x40},
	{0x01, 0x40},
	{0x01, 0x50},
	{0x01, 0x60},
	{0x01, 0x60},
	{0x01, 0x70},
	{0x01, 0x80},
	{0x01, 0x80},
	{0x01, 0x90},
	{0x01, 0xA0},
	{0x01, 0xB0},
	{0x01, 0xB0},
	{0x01, 0xC0},
	{0x01, 0xD0},
	{0x01, 0xD0},
	{0x01, 0xE0},
	{0x01, 0xF0},
	{0x01, 0xF0},
	{0x02, 0x00},
	{0x02, 0x10},
	{0x02, 0x10},
	{0x02, 0x20},
	{0x02, 0x30},
	{0x02, 0x30},
	{0x02, 0x40},
	{0x02, 0x50},
	{0x02, 0x50},
	{0x02, 0x60},
	{0x02, 0x70},
	{0x02, 0x80},
	{0x02, 0x80},
	{0x02, 0x90},
	{0x02, 0xA0},
	{0x02, 0xA0},
	{0x02, 0xB0},
	{0x02, 0xC0},
	{0x02, 0xC0},
	{0x02, 0xD0},
	{0x02, 0xE0},
	{0x02, 0xE0},
	{0x02, 0xF0},
	{0x03, 0x00},
	{0x03, 0x00},
	{0x03, 0x10},
	{0x03, 0x20},
	{0x03, 0x30},
	{0x03, 0x30},
	{0x03, 0x40},
	{0x03, 0x50},
	{0x03, 0x50},
	{0x03, 0x60},
	{0x03, 0x70},
	{0x03, 0x70},
	{0x03, 0x80},
	{0x03, 0x90},
	{0x03, 0x90},
	{0x03, 0xA0},
	{0x03, 0xB0},
	{0x03, 0xB0},
	{0x03, 0xC0},
	{0x03, 0xD0},
	{0x03, 0xD0},
	{0x03, 0xE0},
	{0x03, 0xF0},
	{0x04, 0x00},
	{0x04, 0x00},
	{0x04, 0x10},
	{0x04, 0x20},
	{0x04, 0x20},
	{0x04, 0x30},
	{0x04, 0x40},
	{0x04, 0x40},
	{0x04, 0x50},
	{0x04, 0x60},
	{0x04, 0x60},
	{0x04, 0x70},
	{0x04, 0x80},
	{0x04, 0x80},
	{0x04, 0x90},
	{0x04, 0xA0},
	{0x04, 0xA0},
	{0x04, 0xB0},
	{0x04, 0xC0},
	{0x04, 0xD0},
	{0x04, 0xD0},
	{0x04, 0xE0},
	{0x04, 0xF0},
	{0x04, 0xF0},
	{0x05, 0x00},
	{0x05, 0x10},
	{0x05, 0x10},
	{0x05, 0x20},
	{0x05, 0x30},
	{0x05, 0x30},
	{0x05, 0x40},
	{0x05, 0x50},
	{0x05, 0x50},
	{0x05, 0x60},
	{0x05, 0x70},
	{0x05, 0x80},
	{0x05, 0x80},
	{0x05, 0x90},
	{0x05, 0xA0},
	{0x05, 0xB0},
	{0x05, 0xC0},
	{0x05, 0xD0},
	{0x05, 0xE0},
	{0x05, 0xF0},
	{0x06, 0x00},
	{0x06, 0x10},
	{0x06, 0x20},
	{0x06, 0x30},
	{0x06, 0x40},
	{0x06, 0x50},
	{0x06, 0x60},
	{0x06, 0x70},
	{0x06, 0x80},
	{0x06, 0x90},
	{0x06, 0xA0},
	{0x06, 0xB0},
	{0x06, 0xC0},
	{0x06, 0xD0},
	{0x06, 0xE0},
	{0x06, 0xF0},
	{0x07, 0x00},
	{0x07, 0x10},
	{0x07, 0x20},
	{0x07, 0x30},
	{0x07, 0x40},
	{0x07, 0x50},
	{0x07, 0x60},
	{0x07, 0x60},
	{0x07, 0x70},
	{0x07, 0x80},
	{0x07, 0x90},
	{0x07, 0xA0},
	{0x07, 0xB0},
	{0x07, 0xC0},
	{0x07, 0xD0},
	{0x07, 0xE0},
	{0x07, 0xF0},
	{0x08, 0x00},
	{0x08, 0x10},
	{0x08, 0x20},
	{0x08, 0x30},
	{0x08, 0x40},
	{0x08, 0x50},
	{0x08, 0x60},
	{0x08, 0x70},
	{0x08, 0x80},
	{0x08, 0x90},
	{0x08, 0xA0},
	{0x08, 0xB0},
	{0x08, 0xC0},
	{0x08, 0xD0},
	{0x08, 0xE0},
	{0x08, 0xF0},
	{0x09, 0x00},
	{0x09, 0x10},
	{0x09, 0x20},
	{0x09, 0x30},
	{0x09, 0x40},
	{0x09, 0x50},
	{0x09, 0x50},
	{0x09, 0x60},
	{0x09, 0x70},
	{0x09, 0x80},
	{0x09, 0x90},
	{0x09, 0xA0},
	{0x09, 0xB0},
	{0x09, 0xC0},
	{0x09, 0xD0},
	{0x09, 0xE0},
	{0x09, 0xF0},
	{0x0A, 0x00},
	{0x0A, 0x10},
	{0x0A, 0x20},
	{0x0A, 0x30},
	{0x0A, 0x40},
	{0x0A, 0x50},
	{0x0A, 0x60},
	{0x0A, 0x70},
	{0x0A, 0x80},
	{0x0A, 0x90},
	{0x0A, 0xA0},
	{0x0A, 0xB0},
	{0x0A, 0xC0},
	{0x0A, 0xD0},
	{0x0A, 0xE0},
	{0x0A, 0xF0},
	{0x0B, 0x00},
	{0x0B, 0x10},
	{0x0B, 0x20},
	{0x0B, 0x30},
	{0x0B, 0x40},
	{0x0B, 0x40},
	{0x0B, 0x50},
	{0x0B, 0x60},
	{0x0B, 0x70},
	{0x0B, 0x80},
	{0x0B, 0x90},
	{0x0B, 0xA0},
	{0x0B, 0xB0},
	{0x0B, 0xC0},
	{0x0B, 0xD0},
	{0x0B, 0xE0},
	{0x0B, 0xF0},
	{0x0C, 0x00},
	{0x0C, 0x10},
	{0x0C, 0x20},
	{0x0C, 0x30},
	{0x0C, 0x40},
	{0x0C, 0x50},
	{0x0C, 0x60},
	{0x0C, 0x70},
	{0x0C, 0x80},
	{0x0C, 0x90},
	{0x0C, 0xA0},
	{0x0C, 0xB0},
	{0x0C, 0xC0},
	{0x0C, 0xD0},
	{0x0C, 0xE0},
	{0x0C, 0xF0},
	{0x0D, 0x00},
	{0x0D, 0x10},
	{0x0D, 0x20},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0F, 0xE0},
};


static struct maptbl m12_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(m12_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [ILI7806S COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 M12_SLEEP_OUT[] = { 0x11 };
static u8 M12_SLEEP_IN[] = { 0x10 };
static u8 M12_DISPLAY_OFF[] = { 0x28 };
static u8 M12_DISPLAY_ON[] = { 0x29 };

static u8 M12_BRIGHTNESS[] = {
	0x51,
	0x0F, 0xFF
};

static u8 M12_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

/*  */
static u8 M12_ILI7806S_01[] = {
	0xFF,
	0x78, 0x07, 0x00,
};

static u8 M12_ILI7806S_02[] = {
	0x51,
	0x0F, 0xFF,
};

static u8 M12_ILI7806S_03[] = {
	0x53,
	0x2C,
};

static u8 M12_ILI7806S_04[] = {
	0x35,
	0x00,
};

static u8 M12_ILI7806S_05[] = {
	0x55,
	0x01,
};

static u8 M12_ILI7806S_06[] = {
	0xFF,
	0x78, 0x07, 0x01,
};

static u8 M12_ILI7806S_07[] = {
	0x02,
	0x2E,
};

static u8 M12_ILI7806S_08[] = {
	0x03,
	0x08,
};

static u8 M12_ILI7806S_09[] = {
	0x0E,
	0x52,
};

static u8 M12_ILI7806S_10[] = {
	0xFF,
	0x78, 0x07, 0x11,
};

static u8 M12_ILI7806S_11[] = {
	0x00,
	0x05,
};

static u8 M12_ILI7806S_12[] = {
	0x01,
	0x4D,
};

static u8 M12_ILI7806S_13[] = {
	0xFF,
	0x78, 0x07, 0x12,
};

static u8 M12_ILI7806S_14[] = {
	0x10,
	0x10,
};

static u8 M12_ILI7806S_15[] = {
	0x12,
	0x0C,
};

static u8 M12_ILI7806S_16[] = {
	0x13,
	0x24,
};

static u8 M12_ILI7806S_17[] = {
	0x16,
	0x09,
};

static u8 M12_ILI7806S_18[] = {
	0xC0,
	0x6B,
};

static u8 M12_ILI7806S_19[] = {
	0xFF,
	0x78, 0x07, 0x03,
};

static u8 M12_ILI7806S_20[] = {
	0x80,
	0x36,
};

static u8 M12_ILI7806S_21[] = {
	0x83,
	0x60,
};

static u8 M12_ILI7806S_22[] = {
	0x84,
	0x00,
};

static u8 M12_ILI7806S_23[] = {
	0x88,
	0xD9,
};

static u8 M12_ILI7806S_24[] = {
	0x89,
	0xE1,
};

static u8 M12_ILI7806S_25[] = {
	0x8A,
	0xE8,
};

static u8 M12_ILI7806S_26[] = {
	0x8B,
	0xF0,
};

static u8 M12_ILI7806S_27[] = {
	0xC6,
	0x14,
};

static u8 M12_ILI7806S_28[] = {
	0xFF,
	0x78, 0x07, 0x06,
};

static u8 M12_ILI7806S_29[] = {
	0xDD,
	0x1F,
};

static u8 M12_ILI7806S_30[] = {
	0xCD,
	0x00,
};

static u8 M12_ILI7806S_31[] = {
	0xD6,
	0x00,
};

static u8 M12_ILI7806S_32[] = {
	0x48,
	0x03,
};

static u8 M12_ILI7806S_33[] = {
	0x69,
	0xDF,
};

static u8 M12_ILI7806S_34[] = {
	0x3E,
	0xE2,
};

static u8 M12_ILI7806S_35[] = {
	0xFF,
	0x78, 0x07, 0x0B,
};

static u8 M12_ILI7806S_36[] = {
	0xC0,
	0x85,
};

static u8 M12_ILI7806S_37[] = {
	0xC1,
	0x67,
};

static u8 M12_ILI7806S_38[] = {
	0xC2,
	0x04,
};

static u8 M12_ILI7806S_39[] = {
	0xC3,
	0x04,
};

static u8 M12_ILI7806S_40[] = {
	0xC4,
	0x87,
};

static u8 M12_ILI7806S_41[] = {
	0xC5,
	0x87,
};

static u8 M12_ILI7806S_42[] = {
	0xD2,
	0x43,
};

static u8 M12_ILI7806S_43[] = {
	0xD3,
	0x5C,
};

static u8 M12_ILI7806S_44[] = {
	0xD4,
	0x03,
};

static u8 M12_ILI7806S_45[] = {
	0xD5,
	0x03,
};

static u8 M12_ILI7806S_46[] = {
	0xD6,
	0x54,
};

static u8 M12_ILI7806S_47[] = {
	0xD7,
	0x54,
};

static u8 M12_ILI7806S_48[] = {
	0xFF,
	0x78, 0x07, 0x0E,
};

static u8 M12_ILI7806S_49[] = {
	0x0B,
	0x00,
};

static u8 M12_ILI7806S_50[] = {
	0x21,
	0x14,
};

static u8 M12_ILI7806S_51[] = {
	0x25,
	0x0A,
};

static u8 M12_ILI7806S_52[] = {
	0x26,
	0xD5,
};

static u8 M12_ILI7806S_53[] = {
	0x2D,
	0xB8,
};

static u8 M12_ILI7806S_54[] = {
	0x30,
	0x00,
};

static u8 M12_ILI7806S_55[] = {
	0xFF,
	0x78, 0x07, 0x1E,
};

static u8 M12_ILI7806S_56[] = {
	0xA5,
	0x96,
};

static u8 M12_ILI7806S_57[] = {
	0xA6,
	0x96,
};

static u8 M12_ILI7806S_58[] = {
	0xAD,
	0x01,
};

static u8 M12_ILI7806S_59[] = {
	0x00,
	0x5C,
};

static u8 M12_ILI7806S_60[] = {
	0x08,
	0x5C,
};

static u8 M12_ILI7806S_61[] = {
	0x09,
	0x5C,
};

static u8 M12_ILI7806S_62[] = {
	0x0A,
	0x10,
};

static u8 M12_ILI7806S_63[] = {
	0x0C,
	0x0C,
};

static u8 M12_ILI7806S_64[] = {
	0x0D,
	0x24,
};

static u8 M12_ILI7806S_65[] = {
	0xFF,
	0x78, 0x07, 0x00,
};



static DEFINE_STATIC_PACKET(m12_sleep_out, DSI_PKT_TYPE_WR, M12_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(m12_sleep_in, DSI_PKT_TYPE_WR, M12_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(m12_display_on, DSI_PKT_TYPE_WR, M12_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(m12_display_off, DSI_PKT_TYPE_WR, M12_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(m12_brightness_mode, DSI_PKT_TYPE_WR, M12_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(m12_brightness, &m12_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(m12_brightness, DSI_PKT_TYPE_WR, M12_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(m12_ili7806s_01, DSI_PKT_TYPE_WR, M12_ILI7806S_01, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_02, DSI_PKT_TYPE_WR, M12_ILI7806S_02, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_03, DSI_PKT_TYPE_WR, M12_ILI7806S_03, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_04, DSI_PKT_TYPE_WR, M12_ILI7806S_04, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_05, DSI_PKT_TYPE_WR, M12_ILI7806S_05, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_06, DSI_PKT_TYPE_WR, M12_ILI7806S_06, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_07, DSI_PKT_TYPE_WR, M12_ILI7806S_07, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_08, DSI_PKT_TYPE_WR, M12_ILI7806S_08, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_09, DSI_PKT_TYPE_WR, M12_ILI7806S_09, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_10, DSI_PKT_TYPE_WR, M12_ILI7806S_10, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_11, DSI_PKT_TYPE_WR, M12_ILI7806S_11, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_12, DSI_PKT_TYPE_WR, M12_ILI7806S_12, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_13, DSI_PKT_TYPE_WR, M12_ILI7806S_13, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_14, DSI_PKT_TYPE_WR, M12_ILI7806S_14, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_15, DSI_PKT_TYPE_WR, M12_ILI7806S_15, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_16, DSI_PKT_TYPE_WR, M12_ILI7806S_16, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_17, DSI_PKT_TYPE_WR, M12_ILI7806S_17, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_18, DSI_PKT_TYPE_WR, M12_ILI7806S_18, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_19, DSI_PKT_TYPE_WR, M12_ILI7806S_19, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_20, DSI_PKT_TYPE_WR, M12_ILI7806S_20, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_21, DSI_PKT_TYPE_WR, M12_ILI7806S_21, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_22, DSI_PKT_TYPE_WR, M12_ILI7806S_22, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_23, DSI_PKT_TYPE_WR, M12_ILI7806S_23, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_24, DSI_PKT_TYPE_WR, M12_ILI7806S_24, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_25, DSI_PKT_TYPE_WR, M12_ILI7806S_25, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_26, DSI_PKT_TYPE_WR, M12_ILI7806S_26, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_27, DSI_PKT_TYPE_WR, M12_ILI7806S_27, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_28, DSI_PKT_TYPE_WR, M12_ILI7806S_28, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_29, DSI_PKT_TYPE_WR, M12_ILI7806S_29, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_30, DSI_PKT_TYPE_WR, M12_ILI7806S_30, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_31, DSI_PKT_TYPE_WR, M12_ILI7806S_31, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_32, DSI_PKT_TYPE_WR, M12_ILI7806S_32, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_33, DSI_PKT_TYPE_WR, M12_ILI7806S_33, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_34, DSI_PKT_TYPE_WR, M12_ILI7806S_34, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_35, DSI_PKT_TYPE_WR, M12_ILI7806S_35, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_36, DSI_PKT_TYPE_WR, M12_ILI7806S_36, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_37, DSI_PKT_TYPE_WR, M12_ILI7806S_37, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_38, DSI_PKT_TYPE_WR, M12_ILI7806S_38, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_39, DSI_PKT_TYPE_WR, M12_ILI7806S_39, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_40, DSI_PKT_TYPE_WR, M12_ILI7806S_40, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_41, DSI_PKT_TYPE_WR, M12_ILI7806S_41, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_42, DSI_PKT_TYPE_WR, M12_ILI7806S_42, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_43, DSI_PKT_TYPE_WR, M12_ILI7806S_43, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_44, DSI_PKT_TYPE_WR, M12_ILI7806S_44, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_45, DSI_PKT_TYPE_WR, M12_ILI7806S_45, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_46, DSI_PKT_TYPE_WR, M12_ILI7806S_46, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_47, DSI_PKT_TYPE_WR, M12_ILI7806S_47, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_48, DSI_PKT_TYPE_WR, M12_ILI7806S_48, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_49, DSI_PKT_TYPE_WR, M12_ILI7806S_49, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_50, DSI_PKT_TYPE_WR, M12_ILI7806S_50, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_51, DSI_PKT_TYPE_WR, M12_ILI7806S_51, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_52, DSI_PKT_TYPE_WR, M12_ILI7806S_52, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_53, DSI_PKT_TYPE_WR, M12_ILI7806S_53, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_54, DSI_PKT_TYPE_WR, M12_ILI7806S_54, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_55, DSI_PKT_TYPE_WR, M12_ILI7806S_55, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_56, DSI_PKT_TYPE_WR, M12_ILI7806S_56, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_57, DSI_PKT_TYPE_WR, M12_ILI7806S_57, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_58, DSI_PKT_TYPE_WR, M12_ILI7806S_58, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_59, DSI_PKT_TYPE_WR, M12_ILI7806S_59, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_60, DSI_PKT_TYPE_WR, M12_ILI7806S_60, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_61, DSI_PKT_TYPE_WR, M12_ILI7806S_61, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_62, DSI_PKT_TYPE_WR, M12_ILI7806S_62, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_63, DSI_PKT_TYPE_WR, M12_ILI7806S_63, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_64, DSI_PKT_TYPE_WR, M12_ILI7806S_64, 0);
static DEFINE_STATIC_PACKET(m12_ili7806s_65, DSI_PKT_TYPE_WR, M12_ILI7806S_65, 0);



static DEFINE_PANEL_MDELAY(m12_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(m12_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(m12_wait_60msec, 60);
static DEFINE_PANEL_MDELAY(m12_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(m12_wait_blic_off, 1);


static void *m12_init_cmdtbl[] = {
	&ili7806s_restbl[RES_ID],
	&PKTINFO(m12_ili7806s_01),
	&PKTINFO(m12_ili7806s_02),
	&PKTINFO(m12_ili7806s_03),
	&PKTINFO(m12_ili7806s_04),
	&PKTINFO(m12_ili7806s_05),
	&PKTINFO(m12_ili7806s_06),
	&PKTINFO(m12_ili7806s_07),
	&PKTINFO(m12_ili7806s_08),
	&PKTINFO(m12_ili7806s_09),
	&PKTINFO(m12_ili7806s_10),
	&PKTINFO(m12_ili7806s_11),
	&PKTINFO(m12_ili7806s_12),
	&PKTINFO(m12_ili7806s_13),
	&PKTINFO(m12_ili7806s_14),
	&PKTINFO(m12_ili7806s_15),
	&PKTINFO(m12_ili7806s_16),
	&PKTINFO(m12_ili7806s_17),
	&PKTINFO(m12_ili7806s_18),
	&PKTINFO(m12_ili7806s_19),
	&PKTINFO(m12_ili7806s_20),
	&PKTINFO(m12_ili7806s_21),
	&PKTINFO(m12_ili7806s_22),
	&PKTINFO(m12_ili7806s_23),
	&PKTINFO(m12_ili7806s_24),
	&PKTINFO(m12_ili7806s_25),
	&PKTINFO(m12_ili7806s_26),
	&PKTINFO(m12_ili7806s_27),
	&PKTINFO(m12_ili7806s_28),
	&PKTINFO(m12_ili7806s_29),
	&PKTINFO(m12_ili7806s_30),
	&PKTINFO(m12_ili7806s_31),
	&PKTINFO(m12_ili7806s_32),
	&PKTINFO(m12_ili7806s_33),
	&PKTINFO(m12_ili7806s_34),
	&PKTINFO(m12_ili7806s_35),
	&PKTINFO(m12_ili7806s_36),
	&PKTINFO(m12_ili7806s_37),
	&PKTINFO(m12_ili7806s_38),
	&PKTINFO(m12_ili7806s_39),
	&PKTINFO(m12_ili7806s_40),
	&PKTINFO(m12_ili7806s_41),
	&PKTINFO(m12_ili7806s_42),
	&PKTINFO(m12_ili7806s_43),
	&PKTINFO(m12_ili7806s_44),
	&PKTINFO(m12_ili7806s_45),
	&PKTINFO(m12_ili7806s_46),
	&PKTINFO(m12_ili7806s_47),
	&PKTINFO(m12_ili7806s_48),
	&PKTINFO(m12_ili7806s_49),
	&PKTINFO(m12_ili7806s_50),
	&PKTINFO(m12_ili7806s_51),
	&PKTINFO(m12_ili7806s_52),
	&PKTINFO(m12_ili7806s_53),
	&PKTINFO(m12_ili7806s_54),
	&PKTINFO(m12_ili7806s_55),
	&PKTINFO(m12_ili7806s_56),
	&PKTINFO(m12_ili7806s_57),
	&PKTINFO(m12_ili7806s_58),
	&PKTINFO(m12_ili7806s_59),
	&PKTINFO(m12_ili7806s_60),
	&PKTINFO(m12_ili7806s_61),
	&PKTINFO(m12_ili7806s_62),
	&PKTINFO(m12_ili7806s_63),
	&PKTINFO(m12_ili7806s_64),
	&PKTINFO(m12_ili7806s_65),
	&PKTINFO(m12_ili7806s_01),
	&PKTINFO(m12_sleep_out),

};

static void *m12_res_init_cmdtbl[] = {
	&ili7806s_restbl[RES_ID],
};

static void *m12_set_bl_cmdtbl[] = {
	&PKTINFO(m12_brightness), //51h
};

static void *m12_display_on_cmdtbl[] = {
	&DLYINFO(m12_wait_60msec),
	&PKTINFO(m12_display_on),
	&PKTINFO(m12_brightness_mode),
};

static void *m12_display_off_cmdtbl[] = {
	&PKTINFO(m12_display_off),
	&DLYINFO(m12_wait_display_off),
};

static void *m12_exit_cmdtbl[] = {
	&PKTINFO(m12_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 ILI7806S_M12_I2C_INIT[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0xBE,
	0x02, 0x6B,
	0x03, 0x2F,
	0x11, 0x74,
	0x04, 0x06,
	0x05, 0xC0,
	0x10, 0x67,
	0x08, 0x13,
};

static u8 ILI7806S_M12_I2C_EXIT_VSN[] = {
	0x09, 0xBC,
};

static u8 ILI7806S_M12_I2C_EXIT_VSP[] = {
	0x09, 0xB8,
};

static DEFINE_STATIC_PACKET(ili7806s_m12_i2c_init, I2C_PKT_TYPE_WR, ILI7806S_M12_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ili7806s_m12_i2c_exit_vsn, I2C_PKT_TYPE_WR, ILI7806S_M12_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(ili7806s_m12_i2c_exit_vsp, I2C_PKT_TYPE_WR, ILI7806S_M12_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(ili7806s_m12_i2c_dump, I2C_PKT_TYPE_RD, ILI7806S_M12_I2C_INIT, 0);

static void *ili7806s_m12_init_cmdtbl[] = {
	&PKTINFO(ili7806s_m12_i2c_init),
};

static void *ili7806s_m12_exit_cmdtbl[] = {
	&PKTINFO(ili7806s_m12_i2c_exit_vsn),
	&DLYINFO(m12_wait_blic_off),
	&PKTINFO(ili7806s_m12_i2c_exit_vsp),
};

static void *ili7806s_m12_dump_cmdtbl[] = {
	&PKTINFO(ili7806s_m12_i2c_dump),
};


static struct seqinfo m12_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", m12_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", m12_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", m12_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", m12_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", m12_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", m12_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ili7806s_m12_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ili7806s_m12_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ili7806s_m12_dump_cmdtbl),
#endif
};

struct common_panel_info ili7806s_m12_default_panel_info = {
	.ldi_name = "ili7806s",
	.name = "ili7806s_m12_default",
	.model = "TIANMA_6_517_inch",
	.vendor = "TMC",
	.id = 0xAA7250,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ili7806s_m12_resol),
		.resol = ili7806s_m12_resol,
	},
	.maptbl = m12_maptbl,
	.nr_maptbl = ARRAY_SIZE(m12_maptbl),
	.seqtbl = m12_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(m12_seqtbl),
	.rditbl = ili7806s_rditbl,
	.nr_rditbl = ARRAY_SIZE(ili7806s_rditbl),
	.restbl = ili7806s_restbl,
	.nr_restbl = ARRAY_SIZE(ili7806s_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ili7806s_m12_panel_dimming_info,
	},
	.i2c_data = &ili7806s_m12_i2c_data,
};

static int __init ili7806s_m12_panel_init(void)
{
	register_common_panel(&ili7806s_m12_default_panel_info);

	return 0;
}
arch_initcall(ili7806s_m12_panel_init)
#endif /* __ILI7806S_M12_PANEL_H__ */
