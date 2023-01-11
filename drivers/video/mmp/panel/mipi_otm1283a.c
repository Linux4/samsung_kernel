/*
 * drivers/video/mmp/panel/mipi_otm1283a.c
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

#define OTM8019_SLEEP_OUT_DELAY 120
#define OTM8019_DISP_ON_DELAY   20

struct otm1283a_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char otm1283ab_cmi_exit_sleep[] = {0x11};
static char otm1283ab_cmi_display_on[] = {0x29};

static char otm1283ab_cmi_reg_cmd_01[] = {0x00};
static char otm1283ab_cmi_reg_cmd_02[] = {0xFF, 0x12, 0x83, 0x01};

static char otm1283ab_cmi_reg_cmd_03[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_04[] = {0xFF, 0x12, 0x83};

static char otm1283ab_cmi_reg_cmd_05[] = {0x00, 0xB9};
static char otm1283ab_cmi_reg_cmd_06[] = {0xB0, 0x51};

static char otm1283ab_cmi_reg_cmd_07[] = {0x00, 0xC6};
static char otm1283ab_cmi_reg_cmd_08[] = {0xB0, 0x03};

static char otm1283ab_cmi_reg_cmd_09[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_10[] = {0xC0, 0x00, 0x64, 0x00,
	0x10, 0x00, 0x64, 0x10, 0x10};

static char otm1283ab_cmi_reg_cmd_11[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_12[] = {0xC0, 0x00, 0x5C, 0x00,
	0x01, 0x00, 0x04};

static char otm1283ab_cmi_reg_cmd_13[] = {0x00, 0xB3};
static char otm1283ab_cmi_reg_cmd_14[] = {0xC0, 0x00, 0x50};

static char otm1283ab_cmi_reg_cmd_15[] = {0x00, 0x81};
static char otm1283ab_cmi_reg_cmd_16[] = {0xC1, 0x55};

static char otm1283ab_cmi_reg_cmd_17[] = {0x00, 0x82};
static char otm1283ab_cmi_reg_cmd_18[] = {0xC4, 0x02};

static char otm1283ab_cmi_reg_cmd_19[] = {0x00, 0x8B};
static char otm1283ab_cmi_reg_cmd_20[] = {0xC4, 0x40};

static char otm1283ab_cmi_reg_cmd_21[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_22[] = {0xC4, 0x49};

static char otm1283ab_cmi_reg_cmd_23[] = {0x00, 0xA0};
static char otm1283ab_cmi_reg_cmd_24[] = {0xC4, 0x05, 0x10, 0x04, 0x02, 0x05,
	0x15, 0x11, 0x05, 0x10, 0x07, 0x02, 0x05, 0x15, 0x11};

static char otm1283ab_cmi_reg_cmd_25[] = {0x00, 0xB0};
static char otm1283ab_cmi_reg_cmd_26[] = {0xC4, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_27[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_28[] = {0xC5, 0x50, 0xa6, 0xd0, 0x66};

static char otm1283ab_cmi_reg_cmd_29[] = {0x00, 0xB0};
static char otm1283ab_cmi_reg_cmd_30[] = {0xC5, 0x04, 0x38};

static char otm1283ab_cmi_reg_cmd_31[] = {0x00, 0xB4};
static char otm1283ab_cmi_reg_cmd_32[] = {0xC5, 0xC0};

static char otm1283ab_cmi_reg_cmd_33[] = {0x00, 0xB5};
static char otm1283ab_cmi_reg_cmd_34[] = {0xC5, 0x0B, 0x95, 0xFF,
	0x0B, 0x95, 0xFF};

static char otm1283ab_cmi_reg_cmd_35[] = {0x00, 0xBB};
static char otm1283ab_cmi_reg_cmd_36[] = {0xC5, 0x80};

static char otm1283ab_cmi_reg_cmd_37[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_38[] = {0xF5, 0x02, 0x11, 0x02, 0x11};

static char otm1283ab_cmi_reg_cmd_39[] = {0x00, 0x94};
static char otm1283ab_cmi_reg_cmd_40[] = {0xF5, 0x02};

static char otm1283ab_cmi_reg_cmd_41[] = {0x00, 0xB2};
static char otm1283ab_cmi_reg_cmd_42[] = {0xF5, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_43[] = {0x00, 0xBA};
static char otm1283ab_cmi_reg_cmd_44[] = {0xF5, 0x03};

static char otm1283ab_cmi_reg_cmd_45[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_46[] = {0xCB, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_47[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_48[] = {0xCB, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_49[] = {0x00, 0xA0};
static char otm1283ab_cmi_reg_cmd_50[] = {0xCB, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_51[] = {0x00, 0xB0};
static char otm1283ab_cmi_reg_cmd_52[] = {0xCB, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_53[] = {0x00, 0xC0};
static char otm1283ab_cmi_reg_cmd_54[] = {0xCB, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_55[] = {0x00, 0xD0};
static char otm1283ab_cmi_reg_cmd_56[] = {0xCB, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};

static char otm1283ab_cmi_reg_cmd_57[] = {0x00, 0xE0};
static char otm1283ab_cmi_reg_cmd_58[] = {0xCB, 0x04, 0x00, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_59[] = {0x00, 0xF0};
static char otm1283ab_cmi_reg_cmd_60[] = {0xCB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static char otm1283ab_cmi_reg_cmd_61[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_62[] = {0xCC, 0x29, 0x2A, 0x0A, 0x0C, 0x0E,
	0x10, 0x12, 0x14, 0x06, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_63[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_64[] = {0xCC, 0x00, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x29, 0x2A, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x13};

static char otm1283ab_cmi_reg_cmd_65[] = {0x00, 0xA0};
static char otm1283ab_cmi_reg_cmd_66[] = {0xCC, 0x05, 0x00, 0x07, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_67[] = {0x00, 0xB0};
static char otm1283ab_cmi_reg_cmd_68[] = {0xCC, 0x29, 0x2A, 0x13, 0x11, 0x0F,
	0x0D, 0x0B, 0x09, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_69[] = {0x00, 0xC0};
static char otm1283ab_cmi_reg_cmd_70[] = {0xCC, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x00, 0x00, 0x29, 0x2A, 0x14, 0x12, 0x10, 0x0E, 0x0C, 0x0A};

static char otm1283ab_cmi_reg_cmd_71[] = {0x00, 0xD0};
static char otm1283ab_cmi_reg_cmd_72[] = {0xCC, 0x02, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_73[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_74[] = {0xCE, 0x89, 0x05, 0x10, 0x88, 0x05,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_75[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_76[] = {0xCE, 0x54, 0xFD, 0x10, 0x54, 0xFE,
	0x10, 0x55, 0x01, 0x10, 0x55, 0x02, 0x10, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_77[] = {0x00, 0xA0};
static char otm1283ab_cmi_reg_cmd_78[] = {0xCE, 0x58, 0x07, 0x04, 0xFD, 0x00,
	0x10, 0x00, 0x58, 0x06, 0x04, 0xFE, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_79[] = {0x00, 0xB0};
static char otm1283ab_cmi_reg_cmd_80[] = {0xCE, 0x58, 0x05, 0x04, 0xFF, 0x00,
	0x10, 0x00, 0x58, 0x04, 0x05, 0x00, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_81[] = {0x00, 0xC0};
static char otm1283ab_cmi_reg_cmd_82[] = {0xCE, 0x58, 0x03, 0x05, 0x01, 0x00,
	0x10, 0x00, 0x58, 0x02, 0x05, 0x02, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_83[] = {0x00, 0xD0};
static char otm1283ab_cmi_reg_cmd_84[] = {0xCE, 0x58, 0x01, 0x05, 0x03, 0x00,
	0x10, 0x00, 0x58, 0x00, 0x05, 0x04, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_85[] = {0x00, 0x80};
static char otm1283ab_cmi_reg_cmd_86[] = {0xCF, 0x50, 0x00, 0x05, 0x05, 0x00,
	0x10, 0x00, 0x50, 0x01, 0x05, 0x06, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_87[] = {0x00, 0x90};
static char otm1283ab_cmi_reg_cmd_88[] = {0xCF, 0x50, 0x02, 0x05, 0x07, 0x00,
	0x10, 0x00, 0x50, 0x03, 0x05, 0x08, 0x00, 0x10, 0x00};

static char otm1283ab_cmi_reg_cmd_89[] = {0x00, 0xC0};
static char otm1283ab_cmi_reg_cmd_90[] = {0xCF, 0x39, 0x39, 0x20, 0x20, 0x00,
	0x00, 0x01, 0x01, 0x20, 0x00, 0x00};

static char otm1283ab_cmi_reg_cmd_91[] = {0x00, 0x00};
static char otm1283ab_cmi_reg_cmd_92[] = {0xD8, 0xBE, 0xBE};

static char otm1283ab_cmi_reg_cmd_93[] = {0x00, 0x00};
static char otm1283ab_cmi_reg_cmd_94[] = {0xD9, 0x77};


static char otm1283ab_cmi_reg_cmd_95[] = {0x00, 0x00};
static char otm1283ab_cmi_reg_cmd_96[] = {0xE1, 0x02, 0x12, 0x18, 0x0E, 0x07,
	0x0F, 0x0B, 0x09, 0x04, 0x07, 0x0E, 0x08, 0x0F, 0x12, 0x0C, 0x08};

static char otm1283ab_cmi_reg_cmd_97[] = {0x00, 0x00};
static char otm1283ab_cmi_reg_cmd_98[] = {0xE2, 0x02, 0x12, 0x18, 0x0E, 0x07,
	0x10, 0x0B, 0x0A, 0x04, 0x08, 0x0E, 0x08, 0x0F, 0x12, 0x0C, 0x08};

static char otm1283ab_cmi_reg_cmd_99[] = {0x00, 0x00};
static char otm1283ab_cmi_reg_cmd_100[] = {0xFF, 0xFF, 0xFF, 0xFF};

static char otm1283ab_cmi_te_on[] = {0x35, 0x00};
static char otm1283ab_cmi_reg_cmd_101[] = {0x53, 0x24};

static struct mmp_dsi_cmd_desc otm1283a_display_on_cmds[] = {
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_01), otm1283ab_cmi_reg_cmd_01},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_02), otm1283ab_cmi_reg_cmd_02},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_03), otm1283ab_cmi_reg_cmd_03},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_04), otm1283ab_cmi_reg_cmd_04},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_05), otm1283ab_cmi_reg_cmd_05},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_06), otm1283ab_cmi_reg_cmd_06},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_07), otm1283ab_cmi_reg_cmd_07},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_08), otm1283ab_cmi_reg_cmd_08},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_09), otm1283ab_cmi_reg_cmd_09},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_10), otm1283ab_cmi_reg_cmd_10},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_11), otm1283ab_cmi_reg_cmd_11},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_12), otm1283ab_cmi_reg_cmd_12},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_13), otm1283ab_cmi_reg_cmd_13},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_14), otm1283ab_cmi_reg_cmd_14},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_15), otm1283ab_cmi_reg_cmd_15},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_16), otm1283ab_cmi_reg_cmd_16},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_17), otm1283ab_cmi_reg_cmd_17},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_18), otm1283ab_cmi_reg_cmd_18},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_19), otm1283ab_cmi_reg_cmd_19},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_20), otm1283ab_cmi_reg_cmd_20},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_21), otm1283ab_cmi_reg_cmd_21},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_22), otm1283ab_cmi_reg_cmd_22},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_23), otm1283ab_cmi_reg_cmd_23},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_24), otm1283ab_cmi_reg_cmd_24},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_25), otm1283ab_cmi_reg_cmd_25},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_26), otm1283ab_cmi_reg_cmd_26},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_27), otm1283ab_cmi_reg_cmd_27},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_28), otm1283ab_cmi_reg_cmd_28},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_29), otm1283ab_cmi_reg_cmd_29},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_30), otm1283ab_cmi_reg_cmd_30},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_31), otm1283ab_cmi_reg_cmd_31},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_32), otm1283ab_cmi_reg_cmd_32},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_33), otm1283ab_cmi_reg_cmd_33},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_34), otm1283ab_cmi_reg_cmd_34},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_35), otm1283ab_cmi_reg_cmd_35},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_36), otm1283ab_cmi_reg_cmd_36},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_37), otm1283ab_cmi_reg_cmd_37},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_38), otm1283ab_cmi_reg_cmd_38},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_39), otm1283ab_cmi_reg_cmd_39},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_40), otm1283ab_cmi_reg_cmd_40},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_41), otm1283ab_cmi_reg_cmd_41},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_42), otm1283ab_cmi_reg_cmd_42},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_43), otm1283ab_cmi_reg_cmd_43},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_44), otm1283ab_cmi_reg_cmd_44},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_45), otm1283ab_cmi_reg_cmd_45},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_46), otm1283ab_cmi_reg_cmd_46},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_47), otm1283ab_cmi_reg_cmd_47},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_48), otm1283ab_cmi_reg_cmd_48},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_49), otm1283ab_cmi_reg_cmd_49},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_50), otm1283ab_cmi_reg_cmd_50},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_51), otm1283ab_cmi_reg_cmd_51},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_52), otm1283ab_cmi_reg_cmd_52},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_53), otm1283ab_cmi_reg_cmd_53},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_54), otm1283ab_cmi_reg_cmd_54},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_55), otm1283ab_cmi_reg_cmd_55},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_56), otm1283ab_cmi_reg_cmd_56},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_57), otm1283ab_cmi_reg_cmd_57},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_58), otm1283ab_cmi_reg_cmd_58},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_59), otm1283ab_cmi_reg_cmd_59},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_60), otm1283ab_cmi_reg_cmd_60},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_61), otm1283ab_cmi_reg_cmd_61},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_62), otm1283ab_cmi_reg_cmd_62},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_63), otm1283ab_cmi_reg_cmd_63},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_64), otm1283ab_cmi_reg_cmd_64},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_65), otm1283ab_cmi_reg_cmd_65},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_66), otm1283ab_cmi_reg_cmd_66},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_67), otm1283ab_cmi_reg_cmd_67},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_68), otm1283ab_cmi_reg_cmd_68},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_69), otm1283ab_cmi_reg_cmd_69},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_70), otm1283ab_cmi_reg_cmd_70},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_71), otm1283ab_cmi_reg_cmd_71},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_72), otm1283ab_cmi_reg_cmd_72},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_73), otm1283ab_cmi_reg_cmd_73},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_74), otm1283ab_cmi_reg_cmd_74},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_75), otm1283ab_cmi_reg_cmd_75},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_76), otm1283ab_cmi_reg_cmd_76},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_77), otm1283ab_cmi_reg_cmd_77},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_78), otm1283ab_cmi_reg_cmd_78},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_79), otm1283ab_cmi_reg_cmd_79},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_80), otm1283ab_cmi_reg_cmd_80},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_81), otm1283ab_cmi_reg_cmd_81},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_82), otm1283ab_cmi_reg_cmd_82},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_83), otm1283ab_cmi_reg_cmd_83},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_84), otm1283ab_cmi_reg_cmd_84},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_85), otm1283ab_cmi_reg_cmd_85},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_86), otm1283ab_cmi_reg_cmd_86},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_87), otm1283ab_cmi_reg_cmd_87},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_88), otm1283ab_cmi_reg_cmd_88},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_89), otm1283ab_cmi_reg_cmd_89},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_90), otm1283ab_cmi_reg_cmd_90},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_91), otm1283ab_cmi_reg_cmd_91},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_92), otm1283ab_cmi_reg_cmd_92},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_93), otm1283ab_cmi_reg_cmd_93},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_94), otm1283ab_cmi_reg_cmd_94},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_95), otm1283ab_cmi_reg_cmd_95},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_96), otm1283ab_cmi_reg_cmd_96},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_97), otm1283ab_cmi_reg_cmd_97},

	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_98), otm1283ab_cmi_reg_cmd_98},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_99), otm1283ab_cmi_reg_cmd_99},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_100), otm1283ab_cmi_reg_cmd_100},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0,
		sizeof(otm1283ab_cmi_te_on), otm1283ab_cmi_te_on},

	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, 0,
		sizeof(otm1283ab_cmi_reg_cmd_101), otm1283ab_cmi_reg_cmd_101},

	{MIPI_DSI_DCS_SHORT_WRITE, 1, OTM8019_SLEEP_OUT_DELAY,
		sizeof(otm1283ab_cmi_exit_sleep), otm1283ab_cmi_exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, OTM8019_DISP_ON_DELAY,
		sizeof(otm1283ab_cmi_display_on), otm1283ab_cmi_display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc otm1283a_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void otm1283a_set_brightness(struct mmp_panel *panel, int level)
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

static void otm1283a_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, otm1283a_display_on_cmds,
			ARRAY_SIZE(otm1283a_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, otm1283a_display_off_cmds,
			ARRAY_SIZE(otm1283a_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void otm1283a_panel_power(struct mmp_panel *panel, int skip_on, int on)
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
static void otm1283a_panel_power(struct mmp_panel *panel, int skip_on, int on)
{}
#endif

static void otm1283a_set_status(struct mmp_panel *panel, int status)
{
	struct otm1283a_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			otm1283a_panel_power(panel, skip_on, 1);
		if (!skip_on)
			otm1283a_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		otm1283a_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			otm1283a_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_otm1283a[] = {
	[0] = {
		.pixclock_freq = 69264000,
		.refresh = 60,
		.xres = 720,
		.yres = 1280,
		.real_xres = 720,
		.real_yres = 1280,
		.hsync_len = 2,
		.left_margin = 30,
		.right_margin = 136,
		.vsync_len = 2,
		.upper_margin = 8,
		.lower_margin = 10,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int otm1283a_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_otm1283a;
	return 1;
}

static struct mmp_panel panel_otm1283a = {
	.name = "otm1283a",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = otm1283a_get_modelist,
	.set_status = otm1283a_set_status,
	.set_brightness = otm1283a_set_brightness,
};

static int otm1283a_bl_update_status(struct backlight_device *bl)
{
	struct otm1283a_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int otm1283a_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops otm1283a_bl_ops = {
	.get_brightness = otm1283a_bl_get_brightness,
	.update_status  = otm1283a_bl_update_status,
};

static int otm1283a_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct otm1283a_plat_data *plat_data;
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
		panel_otm1283a.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_otm1283a.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_otm1283a.is_avdd = 1;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_otm1283a.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	plat_data->panel = &panel_otm1283a;
	panel_otm1283a.plat_data = plat_data;
	panel_otm1283a.dev = &pdev->dev;
	mmp_register_panel(&panel_otm1283a);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_otm1283a.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&otm1283a_bl_ops, &props);
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

static int otm1283a_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_otm1283a);
	kfree(panel_otm1283a.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_otm1283a_dt_match[] = {
	{ .compatible = "marvell,mmp-otm1283a" },
	{},
};
#endif

static struct platform_driver otm1283a_driver = {
	.driver		= {
		.name	= "mmp-otm1283a",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_otm1283a_dt_match),
	},
	.probe		= otm1283a_probe,
	.remove		= otm1283a_remove,
};

static int otm1283a_init(void)
{
	return platform_driver_register(&otm1283a_driver);
}
static void otm1283a_exit(void)
{
	platform_driver_unregister(&otm1283a_driver);
}
module_init(otm1283a_init);
module_exit(otm1283a_exit);

MODULE_AUTHOR("Qing Zhu <qzhu@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel OTM1283A");
MODULE_LICENSE("GPL");
