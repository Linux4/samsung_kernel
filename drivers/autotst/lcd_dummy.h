/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LCD_DUMMY_H_
#define _LCD_DUMMY_H_

/* LCD mode */
#define LCD_MODE_MCU			0
#define LCD_MODE_RGB			1
#define LCD_MODE_DSI			2

/* bus mode */
#define LCD_BUS_8080			0
#define LCD_BUS_6800			1

/* panel property */
/* lcdc refresh, TE on, double timing, partial update*/
#define PANEL_CAP_NORMAL               0x0

/* only not support partial update*/
#define PANEL_CAP_NOT_PARTIAL_UPDATE   0x1

/* write register, grame have the same timing */
#define PANEL_CAP_UNIQUE_TIMING        0x2

/* lcd not support TE */
#define PANEL_CAP_NOT_TEAR_SYNC        0x4

/* only do command/data register, such as some mono oled display device */
#define PANEL_CAP_MANUAL_REFRESH       0x8

enum{
	SPRDFB_PANEL_TYPE_MCU = 0,
	SPRDFB_PANEL_TYPE_RGB,
	SPRDFB_PANEL_TYPE_MIPI,
	SPRDFB_PANEL_TYPE_LIMIT
};

enum{
	SPRDFB_POLARITY_POS = 0,
	SPRDFB_POLARITY_NEG,
	SPRDFB_POLARITY_LIMIT
};

enum{
	SPRDFB_MIPI_MODE_CMD = 0,
	SPRDFB_MIPI_MODE_VIDEO,
	SPRDFB_MIPI_MODE_LIMIT
};

/* MCU LCD specific properties */
struct timing_mcu {
	uint16_t rcss; /*unit: ns*/
	uint16_t rlpw;
	uint16_t rhpw;
	uint16_t wcss;
	uint16_t wlpw;
	uint16_t whpw;
};

/* RGB LCD specific properties */
struct timing_rgb {
	uint16_t hfp;/*unit: pixel*/
	uint16_t hbp;
	uint16_t hsync;
	uint16_t vfp; /*unit: line*/
	uint16_t vbp;
	uint16_t vsync;
};

struct info_mipi {
	uint16_t work_mode; /*command_mode, video_mode*/
	uint16_t video_bus_width;
	uint32_t lan_number;
	uint32_t phy_feq;  /*unit:Hz*/
	uint16_t h_sync_pol;
	uint16_t v_sync_pol;
	uint16_t de_pol;
	uint16_t te_pol; /*only for command_mode*/
	uint16_t color_mode_pol;
	uint16_t shut_down_pol;
	struct timing_rgb *timing;
};

struct info_rgb {
	uint16_t video_bus_width;
	uint16_t h_sync_pol;
	uint16_t v_sync_pol;
	uint16_t de_pol;
	struct timing_rgb *timing;
};

struct info_mcu {
	uint16_t bus_mode; /*8080, 6800*/
	uint16_t bus_width;
	uint16_t bpp;
	uint16_t te_pol;
	uint32_t te_sync_delay;
	struct timing_mcu *timing;
};

/* LCD abstraction */
struct panel_spec {
	uint32_t cap;
	uint16_t width;
	uint16_t height;
	uint16_t type; /*mcu, rgb, mipi*/
	union {
		struct info_mcu *mcu;
		struct info_rgb *rgb;
		struct info_mipi * mipi;
	} info;
};

#endif
