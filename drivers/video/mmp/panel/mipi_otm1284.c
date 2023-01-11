/*
 * drivers/video/mmp/panel/mipi_otm1284.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yonghai Huang <huangyh@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Qing Zhu <qzhu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mipi_display.h>

struct otm1284_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char REG_INIT_C0[] = {0x00,0x00};
static char REG_INIT_C1[] = {0xff,0x12,0x84,0x01};
static char REG_INIT_C2[] = {0x00,0x80};
static char REG_INIT_C3[] = {0xff,0x12,0x84};

/* -------------------- panel setting --------------------*/
static char REG_PANEL_C0[] = {0x00,0x80};
static char REG_PANEL_C1[] = {0xc0,0x00,0x64,0x00,0x0f,0x11,0x00,0x64,0x0f,0x11};
static char REG_PANEL_C2[] = {0x00,0x90};
static char REG_PANEL_C3[] = {0xc0,0x00,0x5c,0x00,0x01,0x00,0x04};
static char REG_PANEL_C4[] = {0x00,0xa4};
static char REG_PANEL_C5[] = {0xc0,0x00};
static char REG_PANEL_C6[] = {0x00,0xb3};
static char REG_PANEL_C7[] = {0xc0,0x00,0x55};
static char REG_PANEL_C8[] = {0x00,0x81};
static char REG_PANEL_C9[] = {0xc1,0x55};

/*------------for CPT 050WA01 / GP 053WA1 / BOE Power IC------------*/
static char REG_CPT_C0[] = {0x00,0x90};
static char REG_CPT_C1[] = {0xf5,0x02,0x11,0x02,0x15};
static char REG_CPT_C2[] = {0x00,0x90};
static char REG_CPT_C3[] = {0xc5,0x50};
static char REG_CPT_C4[] = {0x00,0x94};
static char REG_CPT_C5[] = {0xc5,0x66};

/*------------------VGLO1/O2 disable----------------*/
static char REG_VGL_C0[] = {0x00,0xb2};
static char REG_VGL_C1[] = {0xf5,0x00,0x00};
static char REG_VGL_C2[] = {0x00,0xb6};
static char REG_VGL_C3[] = {0xf5,0x00,0x00};
static char REG_VGL_C4[] = {0x00,0x94};
static char REG_VGL_C5[] = {0xf5,0x00,0x00};
static char REG_VGL_C6[] = {0x00,0xd2};
static char REG_VGL_C7[] = {0xf5,0x06,0x15};
static char REG_VGL_C8[] = {0x00,0xb4};
static char REG_VGL_C9[] = {0xc5,0xcc};

/*-------------------- power setting --------------------*/
static char REG_POWER_C0[] = {0x00,0xa0};
static char REG_POWER_C1[] = {0xc4,0x05,0x10,0x06,0x02,0x05,0x15,0x10,0x05,0x10,0x07,0x02,0x05,0x15,0x10};
static char REG_POWER_C2[] = {0x00,0xb0};
static char REG_POWER_C3[] = {0xc4,0x00,0x00};
static char REG_POWER_C4[] = {0x00,0x91};
static char REG_POWER_C5[] = {0xc5,0x19,0x52};
static char REG_POWER_C6[] = {0x00,0x00};
static char REG_POWER_C7[] = {0xd8,0xbc,0xbc};
static char REG_POWER_C8[] = {0x00,0xb3};
static char REG_POWER_C9[] = {0xc5,0x84};
static char REG_POWER_C10[] = {0x00,0xbb};
static char REG_POWER_C11[] = {0xc5,0x8a};
static char REG_POWER_C12[] = {0x00,0x82};
static char REG_POWER_C13[] = {0xc4,0x0a};
static char REG_POWER_C14[] = {0x00,0xc6};
static char REG_POWER_C15[] = {0xB0,0x03};
static char REG_POWER_C16[] = {0x00,0xc2};
static char REG_POWER_C17[] = {0xf5,0x40};
static char REG_POWER_C18[] = {0x00,0xc3};
static char REG_POWER_C19[] = {0xf5,0x85};
static char REG_POWER_C20[] = {0x00,0x87};
static char REG_POWER_C21[] = {0xc4,0x18};

/*-------------------- GAMMA TUNING ------------------------------------------*/
static char REG_GAMA_C0[] = {0x00,0x00};
static char REG_GAMA_C1[] = {0xE1,0x1F,0x3E,0x59,0x67,0x74,0x7F,0x7C,0xA4,0x91,0xA8,0x5B,0x46,0x5B,0x3C,0x40,0x37,0x2C,0x22,0x0E,0x00};
static char REG_GAMA_C2[] = {0x00,0x00};
static char REG_GAMA_C3[] = {0xE2,0x1F,0x3E,0x59,0x65,0x71,0x7D,0x78,0x9F,0x8F,0xA8,0x5A,0x46,0x59,0x3B,0x3F,0x36,0x2C,0x22,0x0E,0x00};
static char REG_GAMA_C4[] = {0x00,0x00};
static char REG_GAMA_C5[] = {0xd9,0x55};

/*-------------------- panel timing state control --------------------*/
static char REG_TIMING_C0[] = {0x00,0x80};
static char REG_TIMING_C1[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING_C2[] = {0x00,0x90};
static char REG_TIMING_C3[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING_C4[] = {0x00,0xa0};
static char REG_TIMING_C5[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING_C6[] = {0x00,0xb0};
static char REG_TIMING_C7[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING_C8[] = {0x00,0xc0};
static char REG_TIMING_C9[] = {0xcb,0x05,0x05,0x05,0x05,0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING_C10[] = {0x00,0xd0};
static char REG_TIMING_C11[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x00,0x00};
static char REG_TIMING_C12[] = {0x00,0xe0};
static char REG_TIMING_C13[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x05};
static char REG_TIMING_C14[] = {0x00,0xf0};
static char REG_TIMING_C15[] = {0xcb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

/*-------------------- panel pad mapping control --------------------*/
static char REG_PAD_C0[] = {0x00,0x80};
static char REG_PAD_C1[] = {0xcc,0x0a,0x0c,0x0e,0x10,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_PAD_C2[] = {0x00,0x90};
static char REG_PAD_C3[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x2e,0x2d,0x09,0x0b,0x0d,0x0f,0x01,0x03,0x00,0x00};
static char REG_PAD_C4[] = {0x00,0xa0};
static char REG_PAD_C5[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2e,0x2d};
static char REG_PAD_C6[] = {0x00,0xb0};
static char REG_PAD_C7[] = {0xcc,0x0F,0x0D,0x0B,0x09,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_PAD_C8[] = {0x00,0xc0};
static char REG_PAD_C9[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x2d,0x2e,0x10,0x0E,0x0C,0x0A,0x04,0x02,0x00,0x00};
static char REG_PAD_C10[] = {0x00,0xd0};
static char REG_PAD_C11[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2d,0x2e};

/*-------------------- panel timing setting --------------------*/
static char REG_TIMING1_C0[] = {0x00,0x80};
static char REG_TIMING1_C1[] = {0xce,0x8f,0x03,0x00,0x8e,0x03,0x00,0x8d,0x03,0x00,0x8c,0x03,0x00};
static char REG_TIMING1_C2[] = {0x00,0x90};
static char REG_TIMING1_C3[] = {0xce,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING1_C4[] = {0x00,0xa0};
static char REG_TIMING1_C5[] = {0xce,0x38,0x0b,0x05,0x00,0x00,0x0a,0x0a,0x38,0x0a,0x05,0x01,0x00,0x0a,0x0a};
static char REG_TIMING1_C6[] = {0x00,0xb0};
static char REG_TIMING1_C7[] = {0xce,0x38,0x09,0x05,0x02,0x00,0x0a,0x0a,0x38,0x08,0x05,0x03,0x00,0x0a,0x0a};
static char REG_TIMING1_C8[] = {0x00,0xc0};
static char REG_TIMING1_C9[] = {0xce,0x38,0x07,0x05,0x04,0x00,0x0a,0x0a,0x38,0x06,0x05,0x05,0x00,0x0a,0x0a};
static char REG_TIMING1_C10[] = {0x00,0xd0};
static char REG_TIMING1_C11[] = {0xce,0x38,0x05,0x05,0x06,0x00,0x0a,0x0a,0x38,0x04,0x05,0x07,0x00,0x0a,0x0a};
static char REG_TIMING1_C12[] = {0x00,0x80};
static char REG_TIMING1_C13[] = {0xcf,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING1_C14[] = {0x00,0x90};
static char REG_TIMING1_C15[] = {0xcf,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING1_C16[] = {0x00,0xa0};
static char REG_TIMING1_C17[] = {0xcf,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING1_C18[] = {0x00,0xb0};
static char REG_TIMING1_C19[] = {0xcf,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static char REG_TIMING1_C20[] = {0x00,0xc0};
static char REG_TIMING1_C21[] = {0xcf,0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x02,0x00,0x00,0x08};
static char REG_TIMING1_C22[] = {0x00,0xb5};
static char REG_TIMING1_C23[] = {0xc5,0x33,0xf1,0xff,0x33,0xf1,0xff};

static char REG_BACLIGHT_C0[] = {0x51, 0x00};
static char REG_BACLIGHT_C1[] = {0x53, 0x24};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};
static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

static struct mmp_dsi_cmd_desc otm1284_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_INIT_C0), REG_INIT_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_INIT_C1), REG_INIT_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_INIT_C2), REG_INIT_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_INIT_C3), REG_INIT_C3},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C0), REG_PANEL_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C1), REG_PANEL_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C2), REG_PANEL_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C3), REG_PANEL_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C4), REG_PANEL_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C5), REG_PANEL_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C6), REG_PANEL_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C7), REG_PANEL_C7},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C8), REG_PANEL_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PANEL_C9), REG_PANEL_C9},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C0), REG_CPT_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C1), REG_CPT_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C2), REG_CPT_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C3), REG_CPT_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C4), REG_CPT_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_CPT_C5), REG_CPT_C5},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C0), REG_VGL_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C1), REG_VGL_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C2), REG_VGL_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C3), REG_VGL_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C4), REG_VGL_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C5), REG_VGL_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C6), REG_VGL_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C7), REG_VGL_C7},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C8), REG_VGL_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_VGL_C9), REG_VGL_C9},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C0), REG_POWER_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C1), REG_POWER_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C2), REG_POWER_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C3), REG_POWER_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C4), REG_POWER_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C5), REG_POWER_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C6), REG_POWER_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C7), REG_POWER_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C8), REG_POWER_C9},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C10), REG_POWER_C10},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C11), REG_POWER_C11},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C12), REG_POWER_C12},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C13), REG_POWER_C13},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C14), REG_POWER_C14},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C15), REG_POWER_C15},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C16), REG_POWER_C16},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C17), REG_POWER_C17},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C18), REG_POWER_C18},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C19), REG_POWER_C19},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C20), REG_POWER_C20},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_POWER_C21), REG_POWER_C21},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C0), REG_GAMA_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C1), REG_GAMA_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C2), REG_GAMA_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C3), REG_GAMA_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C4), REG_GAMA_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_GAMA_C5), REG_GAMA_C5},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C0), REG_TIMING_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C1), REG_TIMING_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C2), REG_TIMING_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C3), REG_TIMING_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C4), REG_TIMING_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C5), REG_TIMING_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C6), REG_TIMING_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C7), REG_TIMING_C7},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C8), REG_TIMING_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C9), REG_TIMING_C9},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C10), REG_TIMING_C10},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C11), REG_TIMING_C11},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C12), REG_TIMING_C12},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C13), REG_TIMING_C13},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C14), REG_TIMING_C14},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING_C15), REG_TIMING_C15},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C0), REG_PAD_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C1), REG_PAD_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C2), REG_PAD_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C3), REG_PAD_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C4), REG_PAD_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C5), REG_PAD_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C6), REG_PAD_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C7), REG_PAD_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C8), REG_PAD_C9},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C10), REG_PAD_C10},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_PAD_C11), REG_PAD_C11},

	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C0), REG_TIMING1_C0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C1), REG_TIMING1_C1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C2), REG_TIMING1_C2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C3), REG_TIMING1_C3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C4), REG_TIMING1_C4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C5), REG_TIMING1_C5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C6), REG_TIMING1_C6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C7), REG_TIMING1_C7},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C8), REG_TIMING1_C8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C9), REG_TIMING1_C9},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C10), REG_TIMING1_C10},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C11), REG_TIMING1_C11},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C12), REG_TIMING1_C12},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C13), REG_TIMING1_C13},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C14), REG_TIMING1_C14},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C15), REG_TIMING1_C15},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C16), REG_TIMING1_C16},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C17), REG_TIMING1_C17},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C18), REG_TIMING1_C18},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C19), REG_TIMING1_C19},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C20), REG_TIMING1_C20},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C21), REG_TIMING1_C21},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C22), REG_TIMING1_C22},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(REG_TIMING1_C23), REG_TIMING1_C23},

	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, 0, sizeof(REG_BACLIGHT_C0), REG_BACLIGHT_C0},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, 0, sizeof(REG_BACLIGHT_C1), REG_BACLIGHT_C1},

	{MIPI_DSI_DCS_SHORT_WRITE, 1, 150, sizeof(exit_sleep),
		exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 5, sizeof(display_on),
		display_on},
};


static struct mmp_dsi_cmd_desc otm1284_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void otm1284_set_brightness(struct mmp_panel *panel, int level)
{
	struct mmp_dsi_cmd_desc brightness_cmds;
	char cmds[2];

	/*Prepare cmds for brightness control*/
	cmds[0] = 0x51;
	/* birghtness 1~4 is too dark, add 5 to correctness */
	if (level)
		level += 5;
	cmds[1] = level;

	/*Prepare dsi commands to send*/
	brightness_cmds.data_type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	brightness_cmds.lp = 0;
	brightness_cmds.delay = 0;
	brightness_cmds.length = sizeof(cmds);
	brightness_cmds.data = cmds;

	mmp_panel_dsi_tx_cmd_array(panel, &brightness_cmds, 1);
}

static void otm1284_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, otm1284_display_on_cmds,
			ARRAY_SIZE(otm1284_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, otm1284_display_off_cmds,
			ARRAY_SIZE(otm1284_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void otm1284_panel_power(struct mmp_panel *panel, int skip_on, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;
	int lcd_rst_n, ret = 0;

	lcd_rst_n = of_get_named_gpio(panel->dev->of_node, "rst_gpio", 0);
	if (lcd_rst_n < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return;
	}

	/* FIXME: LCD_IOVDD, 1.8V from buck2 */
	if (panel->is_iovdd && (!lcd_iovdd)) {
		lcd_iovdd = regulator_get(panel->dev, "iovdd");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (panel->is_avdd && (!lcd_avdd)) {
		lcd_avdd = regulator_get(panel->dev, "avdd");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (on) {
		if (panel->is_avdd && lcd_avdd) {
			regulator_set_voltage(lcd_avdd, 2800000, 2800000);
			ret = regulator_enable(lcd_avdd);
			if (ret < 0)
				goto regu_lcd_iovdd;
		}

		if (panel->is_iovdd && lcd_iovdd) {
			regulator_set_voltage(lcd_iovdd, 1800000, 1800000);
			ret = regulator_enable(lcd_iovdd);
			if (ret < 0)
				goto regu_lcd_iovdd;
		}
		usleep_range(10000, 12000);
		if (!skip_on) {
			gpio_direction_output(lcd_rst_n, 0);
			usleep_range(10000, 12000);
			gpio_direction_output(lcd_rst_n, 1);
			usleep_range(10000, 12000);
		}
	} else {
		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);

		/* disable LCD_IOVDD 1.8v */
		if (panel->is_iovdd && lcd_iovdd)
			regulator_disable(lcd_iovdd);
		if (panel->is_avdd && lcd_avdd)
			regulator_disable(lcd_avdd);
	}

regu_lcd_iovdd:
	gpio_free(lcd_rst_n);
	if (ret < 0) {
		lcd_iovdd = NULL;
		lcd_avdd = NULL;
	}
}
#else
static void otm1284_panel_power(struct mmp_panel *panel, int skip_on, int on)
{}
#endif

static void otm1284_set_status(struct mmp_panel *panel, int status)
{
	struct otm1284_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			otm1284_panel_power(panel, skip_on, 1);
		if (!skip_on)
			otm1284_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		otm1284_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			otm1284_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_otm1284[] = {
	[0] = {
		.pixclock_freq = 69264000,
		.refresh = 60,
		.xres = 720,
		.yres = 1280,
		.real_xres = 720,
		.real_yres = 1280,
		.hsync_len = 2,
		.left_margin = 44,
		.right_margin = 136,
		.vsync_len = 2,
		.upper_margin = 14,
		.lower_margin = 16,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int otm1284_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_otm1284;
	return 1;
}

static struct mmp_panel panel_otm1284 = {
	.name = "otm1284",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = otm1284_get_modelist,
	.set_status = otm1284_set_status,
	.set_brightness = otm1284_set_brightness,
};

static int otm1284_bl_update_status(struct backlight_device *bl)
{
	struct otm1284_plat_data *data = dev_get_drvdata(&bl->dev);
	struct mmp_panel *panel = data->panel;
	int level;

	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		level = bl->props.brightness;
	else
		level = 0;

	/* If there is backlight function of board, use it */
	if (data && data->plat_set_backlight) {
		data->plat_set_backlight(panel, level);
		return 0;
	}

	if (panel && panel->set_brightness)
		panel->set_brightness(panel, level);

	return 0;
}

static int otm1284_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops otm1284_bl_ops = {
	.get_brightness = otm1284_bl_get_brightness,
	.update_status  = otm1284_bl_update_status,
};

static int otm1284_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct otm1284_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
				&path_name);
		if (ret < 0) {
			kfree(plat_data);
			return ret;
		}
		panel_otm1284.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_otm1284.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_otm1284.is_avdd = 1;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_otm1284.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	plat_data->panel = &panel_otm1284;
	panel_otm1284.plat_data = plat_data;
	panel_otm1284.dev = &pdev->dev;
	mmp_register_panel(&panel_otm1284);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_otm1284.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&otm1284_bl_ops, &props);
	if (IS_ERR(bl)) {
		ret = PTR_ERR(bl);
		dev_err(&pdev->dev, "failed to register lcd-backlight\n");
		return ret;
	}

	bl->props.fb_blank = FB_BLANK_UNBLANK;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.brightness = 40;

	return 0;
}

static int otm1284_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_otm1284);
	kfree(panel_otm1284.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_otm1284_dt_match[] = {
	{ .compatible = "marvell,mmp-otm1284" },
	{},
};
#endif

static struct platform_driver otm1284_driver = {
	.driver		= {
		.name	= "mmp-otm1284",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_otm1284_dt_match),
	},
	.probe		= otm1284_probe,
	.remove		= otm1284_remove,
};

static int otm1284_init(void)
{
	return platform_driver_register(&otm1284_driver);
}
static void otm1284_exit(void)
{
	platform_driver_unregister(&otm1284_driver);
}
module_init(otm1284_init);
module_exit(otm1284_exit);

MODULE_AUTHOR("huangyh <huangyh@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel OTM1283A");
MODULE_LICENSE("GPL");
