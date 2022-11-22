/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Motto Support Driver
 * Author: Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DISPLAY_PROFILER_MASKGEN_H__
#define __DISPLAY_PROFILER_MASKGEN_H__

#define MASK_CHAR_WIDTH 5
#define MASK_CHAR_HEIGHT 9
#define MASK_OUTPUT_WIDTH 1440
#define MASK_OUTPUT_HEIGHT 400
#define BGP_DATA_MAX 0x3FFF

#define MASK_DATA_SIZE_DEFAULT 4096

enum MPRINT_PACKET_TYPE {
	MPRINT_PACKET_TYPE_TRANS = 0b10,
	MPRINT_PACKET_TYPE_BLACK = 0b11,
	MAX_MPRINT_PACKET_TYPE,
};

struct mprint_config {
	int debug;
	int scale;
	int color;
	int skip_y;
	int spacing;
	int padd_x;
	int padd_y;
	int resol_x;
	int resol_y;
	int max_len;
};

struct mprint_props {
	struct mprint_config *conf;
	struct mprint_packet *pkts;
	int cursor_x;
	int cursor_y;
	int pkts_max;
	int pkts_size;
	int pkts_pos;
	u8 *data;
	int data_size;
	int data_max;
	char output[32];
};

struct mprint_packet {
	unsigned char type;
	unsigned short size;
};

static const char *mprint_config_names[] = {
	"debug",
	"scale",
	"color",
	"skip_y",
	"spacing_x",
	"padding_x",
	"padding_y",
	"resol_x",
	"resol_y",
	"max_len",
};

static unsigned char MASK_CHAR_MAP[][9] = {
	['.'] = { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100, 0b00000 },
	[' '] = { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 },
	['0'] = { 0b01110, 0b11011, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11011, 0b01110 },
	['1'] = { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },
	['2'] = { 0b01110, 0b11011, 0b10001, 0b00001, 0b00011, 0b00110, 0b01100, 0b11000, 0b11111 },
	['3'] = { 0b01110, 0b11011, 0b10001, 0b00011, 0b00110, 0b00011, 0b10001, 0b11011, 0b01110 },
	['4'] = { 0b00010, 0b00110, 0b01110, 0b11010, 0b10010, 0b10010, 0b11111, 0b00010, 0b00010 },
	['5'] = { 0b11111, 0b10000, 0b10000, 0b11110, 0b11011, 0b00001, 0b00001, 0b11011, 0b01110 },
	['6'] = { 0b01110, 0b11011, 0b10000, 0b11110, 0b11011, 0b10001, 0b10001, 0b11011, 0b01110 },
	['7'] = { 0b11111, 0b10001, 0b10001, 0b00011, 0b00010, 0b00110, 0b00100, 0b01100, 0b01000 },
	['8'] = { 0b01110, 0b11011, 0b10001, 0b11011, 0b01110, 0b11011, 0b10001, 0b11011, 0b01110 },
	['9'] = { 0b01110, 0b11011, 0b10001, 0b10001, 0b11011, 0b01111, 0b00001, 0b11011, 0b01110 },
	['H'] = { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001, 0b00000, 0b00000 },
	[127] = { 0, },
};

int char_to_mask_img(struct mprint_props *p, char* str);
#endif
