/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * DDI ABC operation
 * Author: Samsung Display Driver Team <cj1225.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SS_DSI_MAFPC_H__
#define __SS_DSI_MAFPC_H__

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>


 /* Align & Paylod Size for Embedded mode Transfer */
#define MAFPC_CMD_ALIGN 16
#define MAFPC_MAX_PAYLOAD_SIZE 200

 /* Align & Paylod Size for Non-Embedded mode(Mass) Command Transfer */
#define MAFPC_MASS_CMD_ALIGN 256
#define MAFPC_MAX_PAYLOAD_SIZE_MASS 0xFFFF0 /* 983,024 */

/*
 * ioctl
 */
#define MAFPC_IOCTL_MAGIC	'M'
#define IOCTL_MAFPC_ON		_IOW(MAFPC_IOCTL_MAGIC, 1, int)
#define IOCTL_MAFPC_ON_INSTANT	_IOW(MAFPC_IOCTL_MAGIC, 2, int)
#define IOCTL_MAFPC_OFF		_IOW(MAFPC_IOCTL_MAGIC, 3, int)
#define IOCTL_MAFPC_OFF_INSTANT	_IOW(MAFPC_IOCTL_MAGIC, 4, int)
#define IOCTL_MAFPC_MAX		10

struct ss_cmd_desc;

struct MAFPC {
	struct miscdevice dev;

	int is_support;
	int factory_support;
	int file_open;
	int need_to_write;
	int is_br_table_updated;
	int force_delay;

	int en;

	char *enable_cmd_buf;
	u32 enable_cmd_size;
	char *img_buf;
	u32 img_size;
	u32 brightness_scale_table_size;

	u32 wpos;
	u64 wsize;

	int img_checksum;
	u32 img_checksum_cal;

	struct mutex vdd_mafpc_lock;
	struct mutex vdd_mafpc_crc_check_lock;

	char *crc_img_buf;
	u32 crc_img_size;

	u8 crc_pass_data[2];	/*implemented in dtsi */
	u8 crc_read_data[2];
	int crc_size;

	/* mafpc Function */
	int (*init)(struct samsung_display_driver_data *vdd);
	int (*data_init)(struct samsung_display_driver_data *vdd);
	int (*make_img_cmds)(struct samsung_display_driver_data *vdd, char *data, u32 data_size, int cmd_type);
	int (*make_img_mass_cmds)(struct samsung_display_driver_data *vdd, char *data, u32 data_size, struct ss_cmd_desc *ss_cmd);
	int (*img_write)(struct samsung_display_driver_data *vdd, bool is_instant);
	int (*enable)(struct samsung_display_driver_data *vdd, int enable);
	int (*crc_check)(struct samsung_display_driver_data *vdd);
	int (*debug)(struct samsung_display_driver_data *vdd);
};

int ss_mafpc_init(struct samsung_display_driver_data *vdd);

/* TBR: move below numbers and brightness_scale_idx table to PDF */
#define MAFPC_ENABLE_COMMAND_LEN 66
#define MAFPC_BRIGHTNESS_SCALE_STEP 75
#define MAFPC_BRIGHTNESS_SCALE_CMD 3

/* 2551 (Normal: 0~2550) + 1 (HBM) */
#define MAX_MAFPC_BL_SCALE	2552

static int brightness_scale_idx[MAX_MAFPC_BL_SCALE] = {
	[0 ... 9] = 0,
	[10 ... 19] = 1,
	[20 ... 30] = 2,
	[31 ... 40] = 3,
	[41 ... 50] = 4,
	[51 ... 59] = 5,
	[60 ... 68] = 6,
	[69 ... 79] = 7,
	[80 ... 90] = 8,
	[91 ... 100] = 9,
	[101 ... 120] = 10,
	[121 ... 129] = 11,
	[130 ... 139] = 12,
	[140 ... 158] = 13,
	[159 ... 178] = 14,
	[179 ... 188] = 15,
	[189 ... 200] = 16,
	[201 ... 210] = 17,
	[211 ... 229] = 18,
	[230 ... 249] = 19,
	[250 ... 258] = 20,
	[259 ... 269] = 21,
	[270 ... 299] = 22,
	[300 ... 319] = 23,
	[320 ... 339] = 24,
	[340 ... 359] = 25,
	[360 ... 379] = 26,
	[380 ... 409] = 27,
	[410 ... 439] = 28,
	[440 ... 459] = 29,
	[460 ... 499] = 30,
	[500 ... 530] = 31,
	[531 ... 570] = 32,
	[571 ... 599] = 33,
	[600 ... 639] = 34,
	[640 ... 678] = 35,
	[679 ... 729] = 36,
	[730 ... 769] = 37,
	[770 ... 829] = 38,
	[830 ... 880] = 39,
	[881 ... 930] = 40,
	[931 ... 999] = 41,
	[1000 ... 1059] = 42,
	[1060 ... 1129] = 43,
	[1130 ... 1199] = 44,
	[1200 ... 1280] = 45,
	[1281 ... 1339] = 46,
	[1340 ... 1409] = 47,
	[1410 ... 1479] = 48,
	[1480 ... 1559] = 49,
	[1560 ... 1599] = 50,
	[1600 ... 1640] = 51,
	[1641 ... 1679] = 52,
	[1680 ... 1729] = 53,
	[1730 ... 1769] = 54,
	[1770 ... 1819] = 55,
	[1820 ... 1919] = 56,
	[1920 ... 2000] = 57,
	[2001 ... 2099] = 58,
	[2100 ... 2179] = 59,
	[2180 ... 2189] = 60,
	[2190 ... 2209] = 61,
	[2210 ... 2229] = 62,
	[2230 ... 2249] = 63,
	[2250 ... 2269] = 64,
	[2270 ... 2289] = 65,
	[2290 ... 2309] = 66,
	[2310 ... 2329] = 67,
	[2330 ... 2350] = 68,
	[2351 ... 2400] = 69,
	[2401 ... 2449] = 70,
	[2450 ... 2499] = 71,
	[2500 ... 2549] = 72,
	[2550] = 73,
	[2551] = 74, /* for HBM */
};

/* 0~73 for Normal, 74 for HBM */
static char brightness_scale_table[MAFPC_BRIGHTNESS_SCALE_STEP][MAFPC_BRIGHTNESS_SCALE_CMD] = {
	{0xFF, 0xFF, 0xFF},	/* 0 */
	{0xFF, 0xFF, 0xFF},	/* 1 */
	{0xFF, 0xFF, 0xFF},	/* 2 */
	{0xFF, 0xFF, 0xFF},	/* 3 */
	{0xFF, 0xFF, 0xFF},	/* 4 */
	{0xFF, 0xFF, 0xFF},	/* 5 */
	{0xFF, 0xFF, 0xFF},	/* 6 */
	{0xFF, 0xFF, 0xFF},	/* 7 */
	{0xFF, 0xFF, 0xFF},	/* 8 */
	{0xFF, 0xFF, 0xFF},	/* 9 */
	{0xFF, 0xFF, 0xFF},	/* 10 */
	{0xFF, 0xFF, 0xFF},	/* 11 */
	{0xFF, 0xFF, 0xFF},	/* 12 */
	{0xFF, 0xFF, 0xFF},	/* 13 */
	{0xFF, 0xFF, 0xFF},	/* 14 */
	{0xFF, 0xFF, 0xFF},	/* 15 */
	{0xFF, 0xFF, 0xFF},	/* 16 */
	{0xFF, 0xFF, 0xFF},	/* 17 */
	{0xFF, 0xFF, 0xFF},	/* 18 */
	{0xFF, 0xFF, 0xFF},	/* 19 */
	{0xFF, 0xFF, 0xFF},	/* 20 */
	{0xFF, 0xFF, 0xFF},	/* 21 */
	{0xFF, 0xFF, 0xFF},	/* 22 */
	{0xFF, 0xFF, 0xFF},	/* 23 */
	{0xFF, 0xFF, 0xFF},	/* 24 */
	{0xFF, 0xFF, 0xFF},	/* 25 */
	{0xFF, 0xFF, 0xFF},	/* 26 */
	{0xFF, 0xFF, 0xFF},	/* 27 */
	{0xFF, 0xFF, 0xFF},	/* 28 */
	{0xFF, 0xFF, 0xFF},	/* 29 */
	{0xFF, 0xFF, 0xFF},	/* 30 */
	{0xFF, 0xFF, 0xFF},	/* 31 */
	{0xFF, 0xFF, 0xFF},	/* 32 */
	{0xFF, 0xFF, 0xFF},	/* 33 */
	{0xFF, 0xFF, 0xFF},	/* 34 */
	{0xFF, 0xFF, 0xFF},	/* 35 */
	{0xFF, 0xFF, 0xFF},	/* 36 */
	{0xFF, 0xFF, 0xFF},	/* 37 */
	{0xFF, 0xFF, 0xFF},	/* 38 */
	{0xFF, 0xFF, 0xFF},	/* 39 */
	{0xFF, 0xFF, 0xFF},	/* 40 */
	{0xFF, 0xFF, 0xFF},	/* 41 */
	{0xFF, 0xFF, 0xFF},	/* 42 */
	{0xFF, 0xFF, 0xFF},	/* 43 */
	{0xFF, 0xFF, 0xFF},	/* 44 */
	{0xFF, 0xFF, 0xFF},	/* 45 */
	{0xFF, 0xFF, 0xFF},	/* 46 */
	{0xFF, 0xFF, 0xFF},	/* 47 */
	{0xFF, 0xFF, 0xFF},	/* 48 */
	{0xFF, 0xFF, 0xFF},	/* 49 */
	{0xFF, 0xFF, 0xFF},	/* 50 */
	{0xFF, 0xFF, 0xFF},	/* 51 */
	{0xFF, 0xFF, 0xFF},	/* 52 */
	{0xFF, 0xFF, 0xFF},	/* 53 */
	{0xFF, 0xFF, 0xFF},	/* 54 */
	{0xFF, 0xFF, 0xFF},	/* 55 */
	{0xFF, 0xFF, 0xFF},	/* 56 */
	{0xFF, 0xFF, 0xFF},	/* 57 */
	{0xFF, 0xFF, 0xFF},	/* 58 */
	{0xFF, 0xFF, 0xFF},	/* 59 */
	{0xFF, 0xFF, 0xFF},	/* 60 */
	{0xFF, 0xFF, 0xFF},	/* 61 */
	{0xFF, 0xFF, 0xFF},	/* 62 */
	{0xFF, 0xFF, 0xFF},	/* 63 */
	{0xFF, 0xFF, 0xFF},	/* 64 */
	{0xFF, 0xFF, 0xFF},	/* 65 */
	{0xFF, 0xFF, 0xFF},	/* 66 */
	{0xFF, 0xFF, 0xFF},	/* 67 */
	{0xFF, 0xFF, 0xFF},	/* 68 */
	{0xFF, 0xFF, 0xFF},	/* 69 */
	{0xFF, 0xFF, 0xFF},	/* 70 */
	{0xFF, 0xFF, 0xFF},	/* 71 */
	{0xFF, 0xFF, 0xFF},	/* 72 */
	{0xFF, 0xFF, 0xFF},	/* 73 */
	{0xFF, 0xFF, 0xFF},	/* 74 for HBM*/
};

#endif /* __SELF_DISPLAY_H__ */
