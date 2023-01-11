/*
 * drivers/video/mmp/panel/mipi_JD9365.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 * Authors:  Yonghai Huang <huangyh@marvell.com>
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
#include <video/mmp_esd.h>

/* FIX ME: This is no panel ID for panel JD9365,
set 0 for customer reference */
#define MIPI_DCS_NORMAL_STATE_JD9365 0x0

#define SUNLIGHT_ENALBE 1
#define SUNLIGHT_UPDATE_PEROID HZ

#ifdef SUNLIGHT_ENALBE
struct JD9365_slr {
	struct delayed_work work;
	u32 update;
	u32 enable;
	u32 lux_value;
	struct mutex lock;
};
#endif

struct JD9365_plat_data {
	struct mmp_panel *panel;
#ifdef SUNLIGHT_ENALBE
	struct JD9365_slr slr;
#endif
	u32 esd_enable;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char init_ctrl_1[] = {0xE0, 0x00};
static char init_ctrl_2[] = {0xE1, 0x93};
static char init_ctrl_3[] = {0xE2, 0x65};
static char init_ctrl_4[] = {0xE3, 0xF8};
static char init_ctrl_5[] = {0x70, 0x10};
static char init_ctrl_6[] = {0x71, 0x13};
static char init_ctrl_7[] = {0x72, 0x06};
static char init_ctrl_8[] = {0x80, 0x03};
static char init_ctrl_9[] = {0xE0, 0x01};
static char init_ctrl_10[] = {0x00, 0x00};
static char init_ctrl_11[] = {0x01, 0xEC};
static char init_ctrl_12[] = {0x03, 0x00};
static char init_ctrl_13[] = {0x04, 0xF8};
static char init_ctrl_14[] = {0x17, 0x10};
static char init_ctrl_15[] = {0x18, 0x01};
static char init_ctrl_16[] = {0x19, 0x0E};
static char init_ctrl_17[] = {0x1A, 0x10};
static char init_ctrl_18[] = {0x1B, 0x01};
static char init_ctrl_19[] = {0x1C, 0x0E};
static char init_ctrl_20[] = {0x1F, 0x7E};
static char init_ctrl_21[] = {0x20, 0x24};
static char init_ctrl_22[] = {0x21, 0x24};
static char init_ctrl_23[] = {0x22, 0x4E};
static char init_ctrl_24[] = {0x37, 0x09};
static char init_ctrl_25[] = {0x38, 0x04};
static char init_ctrl_26[] = {0x39, 0x08};
static char init_ctrl_27[] = {0x3A, 0x12};
static char init_ctrl_28[] = {0x3C, 0x78};
static char init_ctrl_29[] = {0x3D, 0xFF};
static char init_ctrl_30[] = {0x3E, 0xFF};
static char init_ctrl_31[] = {0x3F, 0x7F};
static char init_ctrl_32[] = {0x40, 0x04};
static char init_ctrl_33[] = {0x41, 0xA0};
static char init_ctrl_34[] = {0x0A, 0x18};
static char init_ctrl_35[] = {0x55, 0x02};
static char init_ctrl_36[] = {0x56, 0x01};
static char init_ctrl_37[] = {0x57, 0x69};
static char init_ctrl_38[] = {0x58, 0x0A};
static char init_ctrl_39[] = {0x59, 0x2A};
static char init_ctrl_40[] = {0x5A, 0x2B};
static char init_ctrl_41[] = {0x5B, 0x13};
static char init_ctrl_42[] = {0x5D, 0x7C};
static char init_ctrl_43[] = {0x5E, 0x56};
static char init_ctrl_44[] = {0x5F, 0x44};
static char init_ctrl_45[] = {0x60, 0x36};
static char init_ctrl_46[] = {0x61, 0x32};
static char init_ctrl_47[] = {0x62, 0x23};
static char init_ctrl_48[] = {0x63, 0x28};
static char init_ctrl_49[] = {0x64, 0x12};
static char init_ctrl_50[] = {0x65, 0x2B};
static char init_ctrl_51[] = {0x66, 0x29};
static char init_ctrl_52[] = {0x67, 0x29};
static char init_ctrl_53[] = {0x68, 0x47};
static char init_ctrl_54[] = {0x69, 0x36};
static char init_ctrl_55[] = {0x6A, 0x3E};
static char init_ctrl_56[] = {0x6B, 0x32};
static char init_ctrl_57[] = {0x6C, 0x31};
static char init_ctrl_58[] = {0x6D, 0x26};
static char init_ctrl_59[] = {0x6E, 0x1A};
static char init_ctrl_60[] = {0x6F, 0x01};
static char init_ctrl_61[] = {0x70, 0x7C};
static char init_ctrl_62[] = {0x71, 0x56};
static char init_ctrl_63[] = {0x72, 0x44};
static char init_ctrl_64[] = {0x73, 0x36};
static char init_ctrl_65[] = {0x74, 0x32};
static char init_ctrl_66[] = {0x75, 0x23};
static char init_ctrl_67[] = {0x76, 0x28};
static char init_ctrl_68[] = {0x77, 0x12};
static char init_ctrl_69[] = {0x78, 0x2B};
static char init_ctrl_70[] = {0x79, 0x29};
static char init_ctrl_71[] = {0x7A, 0x29};
static char init_ctrl_72[] = {0x7B, 0x47};
static char init_ctrl_73[] = {0x7C, 0x36};
static char init_ctrl_74[] = {0x7D, 0x3E};
static char init_ctrl_75[] = {0x7E, 0x32};
static char init_ctrl_76[] = {0x7F, 0x31};
static char init_ctrl_77[] = {0x80, 0x26};
static char init_ctrl_78[] = {0x81, 0x1A};
static char init_ctrl_79[] = {0x82, 0x01};
static char init_ctrl_80[] = {0xE0, 0x02};
static char init_ctrl_81[] = {0x00, 0x08};
static char init_ctrl_82[] = {0x01, 0x0A};
static char init_ctrl_83[] = {0x02, 0x04};
static char init_ctrl_84[] = {0x03, 0x06};
static char init_ctrl_85[] = {0x04, 0x00};
static char init_ctrl_86[] = {0x05, 0x02};
static char init_ctrl_87[] = {0x06, 0x1F};
static char init_ctrl_88[] = {0x07, 0x1F};
static char init_ctrl_89[] = {0x08, 0x1F};
static char init_ctrl_90[] = {0x09, 0x1F};
static char init_ctrl_91[] = {0x0A, 0x1E};
static char init_ctrl_92[] = {0x0B, 0x1F};
static char init_ctrl_93[] = {0x0C, 0x1F};
static char init_ctrl_94[] = {0x0D, 0x17};
static char init_ctrl_95[] = {0x0E, 0x37};
static char init_ctrl_96[] = {0x0F, 0x1F};
static char init_ctrl_97[] = {0x10, 0x1F};
static char init_ctrl_98[] = {0x11, 0x1F};
static char init_ctrl_99[] = {0x12, 0x1F};
static char init_ctrl_100[] = {0x13, 0x1F};
static char init_ctrl_101[] = {0x14, 0x1F};
static char init_ctrl_102[] = {0x15, 0x1F};
static char init_ctrl_103[] = {0x16, 0x09};
static char init_ctrl_104[] = {0x17, 0x0B};
static char init_ctrl_105[] = {0x18, 0x05};
static char init_ctrl_106[] = {0x19, 0x07};
static char init_ctrl_107[] = {0x1A, 0x01};
static char init_ctrl_108[] = {0x1B, 0x03};
static char init_ctrl_109[] = {0x1C, 0x1F};
static char init_ctrl_110[] = {0x1D, 0x1F};
static char init_ctrl_111[] = {0x1E, 0x1F};
static char init_ctrl_112[] = {0x1F, 0x1F};
static char init_ctrl_113[] = {0x20, 0x1E};
static char init_ctrl_114[] = {0x21, 0x1F};
static char init_ctrl_115[] = {0x22, 0x1F};
static char init_ctrl_116[] = {0x23, 0x17};
static char init_ctrl_117[] = {0x24, 0x37};
static char init_ctrl_118[] = {0x25, 0x1F};
static char init_ctrl_119[] = {0x26, 0x1F};
static char init_ctrl_120[] = {0x27, 0x1F};
static char init_ctrl_121[] = {0x28, 0x1F};
static char init_ctrl_122[] = {0x29, 0x1F};
static char init_ctrl_123[] = {0x2A, 0x1F};
static char init_ctrl_124[] = {0x2B, 0x1F};
static char init_ctrl_125[] = {0x2C, 0x07};
static char init_ctrl_126[] = {0x2D, 0x05};
static char init_ctrl_127[] = {0x2E, 0x0B};
static char init_ctrl_128[] = {0x2F, 0x09};
static char init_ctrl_129[] = {0x30, 0x03};
static char init_ctrl_130[] = {0x31, 0x01};
static char init_ctrl_131[] = {0x32, 0x1F};
static char init_ctrl_132[] = {0x33, 0x1F};
static char init_ctrl_133[] = {0x34, 0x1F};
static char init_ctrl_134[] = {0x35, 0x1F};
static char init_ctrl_135[] = {0x36, 0x1F};
static char init_ctrl_136[] = {0x37, 0x1E};
static char init_ctrl_137[] = {0x38, 0x1F};
static char init_ctrl_138[] = {0x39, 0x17};
static char init_ctrl_139[] = {0x3A, 0x37};
static char init_ctrl_140[] = {0x3B, 0x1F};
static char init_ctrl_141[] = {0x3C, 0x1F};
static char init_ctrl_142[] = {0x3D, 0x1F};
static char init_ctrl_143[] = {0x3E, 0x1F};
static char init_ctrl_144[] = {0x3F, 0x1F};
static char init_ctrl_145[] = {0x40, 0x1F};
static char init_ctrl_146[] = {0x41, 0x1F};
static char init_ctrl_147[] = {0x42, 0x06};
static char init_ctrl_148[] = {0x43, 0x04};
static char init_ctrl_149[] = {0x44, 0x0A};
static char init_ctrl_150[] = {0x45, 0x08};
static char init_ctrl_151[] = {0x46, 0x02};
static char init_ctrl_152[] = {0x47, 0x00};
static char init_ctrl_153[] = {0x48, 0x1F};
static char init_ctrl_154[] = {0x49, 0x1F};
static char init_ctrl_155[] = {0x4A, 0x1F};
static char init_ctrl_156[] = {0x4B, 0x1F};
static char init_ctrl_157[] = {0x4C, 0x1F};
static char init_ctrl_158[] = {0x4D, 0x1E};
static char init_ctrl_159[] = {0x4E, 0x1F};
static char init_ctrl_160[] = {0x4F, 0x17};
static char init_ctrl_161[] = {0x50, 0x37};
static char init_ctrl_162[] = {0x51, 0x1F};
static char init_ctrl_163[] = {0x52, 0x1F};
static char init_ctrl_164[] = {0x53, 0x1F};
static char init_ctrl_165[] = {0x54, 0x1F};
static char init_ctrl_166[] = {0x55, 0x1F};
static char init_ctrl_167[] = {0x56, 0x1F};
static char init_ctrl_168[] = {0x57, 0x1F};
static char init_ctrl_169[] = {0x58, 0x10};
static char init_ctrl_170[] = {0x59, 0x00};
static char init_ctrl_171[] = {0x5A, 0x00};
static char init_ctrl_172[] = {0x5B, 0x30};
static char init_ctrl_173[] = {0x5C, 0x05};
static char init_ctrl_174[] = {0x5D, 0x30};
static char init_ctrl_175[] = {0x5E, 0x01};
static char init_ctrl_176[] = {0x5F, 0x02};
static char init_ctrl_177[] = {0x60, 0x00};
static char init_ctrl_178[] = {0x61, 0x00};
static char init_ctrl_179[] = {0x62, 0x00};
static char init_ctrl_180[] = {0x63, 0x03};
static char init_ctrl_181[] = {0x64, 0x6A};
static char init_ctrl_182[] = {0x65, 0x00};
static char init_ctrl_183[] = {0x66, 0x00};
static char init_ctrl_184[] = {0x67, 0x73};
static char init_ctrl_185[] = {0x68, 0x09};
static char init_ctrl_186[] = {0x69, 0x06};
static char init_ctrl_187[] = {0x6A, 0x6A};
static char init_ctrl_188[] = {0x6B, 0x08};
static char init_ctrl_189[] = {0x6C, 0x00};
static char init_ctrl_190[] = {0x6D, 0x04};
static char init_ctrl_191[] = {0x6E, 0x04};
static char init_ctrl_192[] = {0x6F, 0x88};
static char init_ctrl_193[] = {0x70, 0x00};
static char init_ctrl_194[] = {0x71, 0x00};
static char init_ctrl_195[] = {0x72, 0x06};
static char init_ctrl_196[] = {0x73, 0x7B};
static char init_ctrl_197[] = {0x74, 0x00};
static char init_ctrl_198[] = {0x75, 0x80};
static char init_ctrl_199[] = {0x76, 0x00};
static char init_ctrl_200[] = {0x77, 0x5D};
static char init_ctrl_201[] = {0x78, 0x23};
static char init_ctrl_202[] = {0x79, 0x1F};
static char init_ctrl_203[] = {0x7A, 0x00};
static char init_ctrl_204[] = {0x7B, 0x00};
static char init_ctrl_205[] = {0x7C, 0x00};
static char init_ctrl_206[] = {0x7D, 0x03};
static char init_ctrl_207[] = {0x7E, 0x7B};
static char init_ctrl_208[] = {0xE0, 0x00};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

static struct mmp_dsi_cmd_desc JD9365_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_1), init_ctrl_1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_2), init_ctrl_2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_3), init_ctrl_3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_4), init_ctrl_4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_5), init_ctrl_5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_6), init_ctrl_6},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_7), init_ctrl_7},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_8), init_ctrl_8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_9), init_ctrl_9},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_10), init_ctrl_10},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_11), init_ctrl_11},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_12), init_ctrl_12},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_13), init_ctrl_13},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_14), init_ctrl_14},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_15), init_ctrl_15},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_16), init_ctrl_16},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_17), init_ctrl_17},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_18), init_ctrl_18},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_19), init_ctrl_19},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_20), init_ctrl_20},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_21), init_ctrl_21},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_22), init_ctrl_22},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_23), init_ctrl_23},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_24), init_ctrl_24},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_25), init_ctrl_25},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_26), init_ctrl_26},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_27), init_ctrl_27},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_28), init_ctrl_28},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_29), init_ctrl_29},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_30), init_ctrl_30},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_31), init_ctrl_31},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_32), init_ctrl_32},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_33), init_ctrl_33},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_34), init_ctrl_34},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_35), init_ctrl_35},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_36), init_ctrl_36},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_37), init_ctrl_37},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_38), init_ctrl_38},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_39), init_ctrl_39},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_40), init_ctrl_40},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_41), init_ctrl_41},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_42), init_ctrl_42},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_43), init_ctrl_43},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_44), init_ctrl_44},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_45), init_ctrl_45},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_46), init_ctrl_46},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_47), init_ctrl_47},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_48), init_ctrl_48},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_49), init_ctrl_49},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_50), init_ctrl_50},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_51), init_ctrl_51},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_52), init_ctrl_52},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_53), init_ctrl_53},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_54), init_ctrl_54},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_55), init_ctrl_55},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_56), init_ctrl_56},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_57), init_ctrl_57},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_58), init_ctrl_58},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_59), init_ctrl_59},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_60), init_ctrl_60},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_61), init_ctrl_61},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_62), init_ctrl_62},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_63), init_ctrl_63},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_64), init_ctrl_64},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_65), init_ctrl_65},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_66), init_ctrl_66},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_67), init_ctrl_67},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_68), init_ctrl_68},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_69), init_ctrl_69},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_70), init_ctrl_70},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_71), init_ctrl_71},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_72), init_ctrl_72},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_73), init_ctrl_73},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_74), init_ctrl_74},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_75), init_ctrl_75},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_76), init_ctrl_76},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_77), init_ctrl_77},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_78), init_ctrl_78},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_79), init_ctrl_79},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_80), init_ctrl_80},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_81), init_ctrl_81},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_82), init_ctrl_82},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_83), init_ctrl_83},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_84), init_ctrl_84},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_85), init_ctrl_85},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_86), init_ctrl_86},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_87), init_ctrl_87},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_88), init_ctrl_88},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_89), init_ctrl_89},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_90), init_ctrl_90},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_91), init_ctrl_91},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_92), init_ctrl_92},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_93), init_ctrl_93},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_94), init_ctrl_94},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_95), init_ctrl_95},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_96), init_ctrl_96},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_97), init_ctrl_97},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_98), init_ctrl_98},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_99), init_ctrl_99},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_100), init_ctrl_100},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_101), init_ctrl_101},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_102), init_ctrl_102},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_103), init_ctrl_103},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_104), init_ctrl_104},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_105), init_ctrl_105},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_106), init_ctrl_106},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_107), init_ctrl_107},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_108), init_ctrl_108},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_109), init_ctrl_109},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_110), init_ctrl_110},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_111), init_ctrl_111},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_112), init_ctrl_112},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_113), init_ctrl_113},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_114), init_ctrl_114},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_115), init_ctrl_115},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_116), init_ctrl_116},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_117), init_ctrl_117},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_118), init_ctrl_118},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_119), init_ctrl_119},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_120), init_ctrl_120},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_121), init_ctrl_121},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_122), init_ctrl_122},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_123), init_ctrl_123},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_124), init_ctrl_124},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_125), init_ctrl_125},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_126), init_ctrl_126},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_127), init_ctrl_127},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_128), init_ctrl_128},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_129), init_ctrl_129},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_130), init_ctrl_130},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_131), init_ctrl_131},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_132), init_ctrl_132},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_133), init_ctrl_133},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_134), init_ctrl_134},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_135), init_ctrl_135},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_136), init_ctrl_136},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_137), init_ctrl_137},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_138), init_ctrl_138},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_139), init_ctrl_139},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_140), init_ctrl_140},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_141), init_ctrl_141},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_142), init_ctrl_142},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_143), init_ctrl_143},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_144), init_ctrl_144},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_145), init_ctrl_145},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_146), init_ctrl_146},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_147), init_ctrl_147},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_148), init_ctrl_148},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_149), init_ctrl_149},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_150), init_ctrl_150},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_151), init_ctrl_151},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_152), init_ctrl_152},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_153), init_ctrl_153},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_154), init_ctrl_154},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_155), init_ctrl_155},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_156), init_ctrl_156},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_157), init_ctrl_157},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_158), init_ctrl_158},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_159), init_ctrl_159},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_160), init_ctrl_160},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_161), init_ctrl_161},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_162), init_ctrl_162},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_163), init_ctrl_163},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_164), init_ctrl_164},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_165), init_ctrl_165},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_166), init_ctrl_166},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_167), init_ctrl_167},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_168), init_ctrl_168},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_169), init_ctrl_169},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_170), init_ctrl_170},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_171), init_ctrl_171},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_172), init_ctrl_172},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_173), init_ctrl_173},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_174), init_ctrl_174},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_175), init_ctrl_175},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_176), init_ctrl_176},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_177), init_ctrl_177},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_178), init_ctrl_178},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_179), init_ctrl_179},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_180), init_ctrl_180},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_181), init_ctrl_181},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_182), init_ctrl_182},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_183), init_ctrl_183},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_184), init_ctrl_184},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_185), init_ctrl_185},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_186), init_ctrl_186},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_187), init_ctrl_187},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_188), init_ctrl_188},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_189), init_ctrl_189},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_190), init_ctrl_190},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_191), init_ctrl_191},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_192), init_ctrl_192},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_193), init_ctrl_193},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_194), init_ctrl_194},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_195), init_ctrl_195},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_196), init_ctrl_196},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_197), init_ctrl_197},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_198), init_ctrl_198},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_199), init_ctrl_199},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_200), init_ctrl_200},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_201), init_ctrl_201},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_202), init_ctrl_202},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_203), init_ctrl_203},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_204), init_ctrl_204},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_205), init_ctrl_205},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_206), init_ctrl_206},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_207), init_ctrl_207},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(init_ctrl_208), init_ctrl_208},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 150, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 10, sizeof(display_on), display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc JD9365_display_off_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

#ifdef SUNLIGHT_ENALBE
static char slr1[] = {0xE0, 0x03};
static char slr2[] = {0xA8, 0x01};
static char slr3[] = {0xE0, 0x00};
static char slr4[] = {0x55, 0x70};

static char slr_value1[] = {0xE0, 0x00};
static char slr_value2[] = {0x88, 0x00};
static char slr_value3[] = {0x89, 0x00};
static char slr_value4[] = {0x8A, 0x00};
static char slr_value5[] = {0x8B, 0x00};

#define slr_high_pos 3
#define slr_low_pos 4

static struct mmp_dsi_cmd_desc JD9365_sunlight_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value1), slr_value1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value2), slr_value2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value3), slr_value3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value4), slr_value4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value5), slr_value5},
};

static struct mmp_dsi_cmd_desc JD9365_sunlight_onoff_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr1), slr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr2), slr2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr3), slr3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr4), slr4},
};

static void JD9365_slr_onoff(struct mmp_panel *panel, int onoff)
{
	struct JD9365_plat_data *plat = panel->plat_data;

	if (onoff)
		JD9365_sunlight_onoff_cmds[3].data[1] = 0x70;
	else
		JD9365_sunlight_onoff_cmds[3].data[1] = 0x00;

	mmp_panel_dsi_tx_cmd_array(panel, JD9365_sunlight_onoff_cmds,
		ARRAY_SIZE(JD9365_sunlight_onoff_cmds));

	plat->slr.enable = onoff;
}

static void JD9365_slr_set(struct mmp_panel *panel, int level)
{
	JD9365_sunlight_cmds[slr_high_pos].data[1] = (level & 0xFFFF) >> 8;
	JD9365_sunlight_cmds[slr_low_pos].data[1] = level & 0xFF;

	mmp_panel_dsi_tx_cmd_array(panel, JD9365_sunlight_cmds,
		ARRAY_SIZE(JD9365_sunlight_cmds));
}

static int JD9365_slr_report(struct notifier_block *b, unsigned long state, void *val)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");
	struct mmp_panel *panel = (path) ? path->panel : NULL;
	struct JD9365_plat_data *plat = (panel) ? panel->plat_data : NULL;

	if (path->status && plat && !plat->slr.enable)
		return NOTIFY_OK;

	if (path->status && plat && !plat->slr.update) {
		mutex_lock(&plat->slr.lock);
		plat->slr.update = 1;
		plat->slr.lux_value = *((u32 *)val);
		mutex_unlock(&plat->slr.lock);
		schedule_delayed_work(&plat->slr.work, SUNLIGHT_UPDATE_PEROID);
	}

	return NOTIFY_OK;
}

static struct notifier_block JD9365_slr_notifier = {
	.notifier_call = JD9365_slr_report,
};

static inline void JD9365_slr_work(struct work_struct *work)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");
	struct mmp_panel *panel = (path) ? path->panel : NULL;
	struct JD9365_plat_data *plat = (panel) ? panel->plat_data : NULL;

	if (panel && plat && plat->slr.update) {
		mutex_lock(&plat->slr.lock);
		JD9365_slr_set(panel, plat->slr.lux_value);
		plat->slr.update = 0;
		mutex_unlock(&plat->slr.lock);
	}
}

ssize_t JD9365_slr_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_panel *panel = dev_get_drvdata(dev);
	struct JD9365_plat_data *plat_data = panel->plat_data;

	return sprintf(buf, "%d\n", plat_data->slr.enable);
}

ssize_t JD9365_slr_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_panel *panel = dev_get_drvdata(dev);
	struct JD9365_plat_data *plat_data = panel->plat_data;
	unsigned long val, ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0) {
		dev_err(dev, "strtoul err.\n");
		return ret;
	}

	mutex_lock(&plat_data->slr.lock);
	plat_data->slr.enable = val;
	if (val)
		JD9365_slr_onoff(panel, 1);
	else
		JD9365_slr_onoff(panel, 0);
	mutex_unlock(&plat_data->slr.lock);

	return size;
}

static DEVICE_ATTR(slr, S_IRUGO | S_IWUSR, JD9365_slr_show,
	JD9365_slr_store);

#endif

static void JD9365_set_brightness(struct mmp_panel *panel, int level)
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

static void JD9365_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, JD9365_display_on_cmds,
			ARRAY_SIZE(JD9365_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, JD9365_display_off_cmds,
			ARRAY_SIZE(JD9365_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void JD9365_panel_power(struct mmp_panel *panel, int skip_on, int on)
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
static void JD9365_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void JD9365_set_status(struct mmp_panel *panel, int status)
{
	struct JD9365_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			JD9365_panel_power(panel, skip_on, 1);
		if (!skip_on)
			JD9365_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		JD9365_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			JD9365_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static void JD9365_esd_onoff(struct mmp_panel *panel, int status)
{
	struct JD9365_plat_data *plat = panel->plat_data;

	if (status) {
		if (plat->esd_enable)
			esd_start(&panel->esd);
	} else {
		if (plat->esd_enable)
			esd_stop(&panel->esd);
	}
}

static void JD9365_esd_recover(struct mmp_panel *panel)
{
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	esd_panel_recover(path);
}

static char pkt_size_cmd[] = {0x1};
static char read_status[] = {0xdb};

static struct mmp_dsi_cmd_desc JD9365_read_status_cmds[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status), read_status},
};

static int JD9365_get_status(struct mmp_panel *panel)
{
	struct mmp_dsi_buf dbuf;
	u8 read_status = 0;
	int ret;

	ret = mmp_panel_dsi_rx_cmd_array(panel, &dbuf,
		JD9365_read_status_cmds,
		ARRAY_SIZE(JD9365_read_status_cmds));
	if (ret < 0) {
		pr_err("[ERROR] DSI receive failure!\n");
		return 1;
	}

	read_status = dbuf.data[0];
	if (read_status != MIPI_DCS_NORMAL_STATE_JD9365) {
		pr_err("[ERROR] panel status is 0x%x\n", read_status);
		return 1;
	} else {
		pr_debug("panel status is 0x%x\n", read_status);
	}

	return 0;
}

static struct mmp_mode mmp_modes_JD9365[] = {
	[0] = {
		.pixclock_freq = 69264000,
		.refresh = 60,
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

static int JD9365_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_JD9365;
	return 1;
}

static struct mmp_panel panel_JD9365 = {
	.name = "JD9365",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = JD9365_get_modelist,
	.set_status = JD9365_set_status,
	.set_brightness = JD9365_set_brightness,
	.get_status = JD9365_get_status,
	.panel_esd_recover = JD9365_esd_recover,
	.esd_set_onoff = JD9365_esd_onoff,
};

static int JD9365_bl_update_status(struct backlight_device *bl)
{
	struct JD9365_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int JD9365_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops JD9365_bl_ops = {
	.get_brightness = JD9365_bl_get_brightness,
	.update_status  = JD9365_bl_update_status,
};

static int JD9365_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct JD9365_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;
	u32 esd_enable = 0;

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
		panel_JD9365.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_JD9365.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_JD9365.is_avdd = 1;
		if (of_property_read_u32(np, "panel_esd", &esd_enable))
			plat_data->esd_enable = 0;
		else
			plat_data->esd_enable = esd_enable;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_JD9365.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
		plat_data->esd_enable = mi->esd_enable;
	}

	plat_data->panel = &panel_JD9365;
	panel_JD9365.plat_data = plat_data;
	panel_JD9365.dev = &pdev->dev;
	mmp_register_panel(&panel_JD9365);
	if (plat_data->esd_enable)
		esd_init(&panel_JD9365);

#ifdef SUNLIGHT_ENALBE
	mutex_init(&plat_data->slr.lock);
	INIT_DEFERRABLE_WORK(&plat_data->slr.work, JD9365_slr_work);
	/*
	 * disable slr feature on default, enable it by "echo 1 >
	 * /sys/devices/platform/JD9365* /slr" in console
	 */
	plat_data->slr.enable = 0;

	/*
	 * notify slr when light sensor is updated, if use different light sensor,
	 * need register notifier accordingly.
	 */
	mmp_register_notifier(&JD9365_slr_notifier);

	platform_set_drvdata(pdev, &panel_JD9365);
	device_create_file(&pdev->dev, &dev_attr_slr);
#endif

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_JD9365.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&JD9365_bl_ops, &props);
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

static int JD9365_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_JD9365);
	kfree(panel_JD9365.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_JD9365_dt_match[] = {
	{ .compatible = "marvell,mmp-JD9365" },
	{},
};
#endif

static struct platform_driver JD9365_driver = {
	.driver		= {
		.name	= "mmp-JD9365",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_JD9365_dt_match),
	},
	.probe		= JD9365_probe,
	.remove		= JD9365_remove,
};

static int JD9365_init(void)
{
	return platform_driver_register(&JD9365_driver);
}
static void JD9365_exit(void)
{
	platform_driver_unregister(&JD9365_driver);
}
module_init(JD9365_init);
module_exit(JD9365_exit);

MODULE_AUTHOR("Xiaolong Ye <yexl@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel JD9365");
MODULE_LICENSE("GPL");
