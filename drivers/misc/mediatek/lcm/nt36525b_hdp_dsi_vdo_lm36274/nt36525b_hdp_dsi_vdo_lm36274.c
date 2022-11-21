/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#  include <linux/string.h>
#  include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

#ifdef BUILD_LK
#  define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#  define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#  define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#  define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#  include <linux/kernel.h>
#  include <linux/module.h>
#  include <linux/fs.h>
#  include <linux/slab.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/i2c.h>
#  include <linux/irq.h>
#  include <linux/uaccess.h>
#  include <linux/interrupt.h>
#  include <linux/io.h>
#  include <linux/platform_device.h>
#endif

#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT		(1600)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include "disp_dts_gpio.h"

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[230];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF, 0x1, {0x20} }, /* SEQ_NT36525B_001 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_002 */
	{0x00, 0x1, {0x01} }, /* SEQ_NT36525B_003 */
	{0x01, 0x1, {0x55} }, /* SEQ_NT36525B_004 */
	{0x03, 0x1, {0x55} }, /* SEQ_NT36525B_005 */
	{0x05, 0x1, {0xA9} }, /* SEQ_NT36525B_006 */
	{0x07, 0x1, {0x73} }, /* SEQ_NT36525B_007 */
	{0x08, 0x1, {0xC1} }, /* SEQ_NT36525B_008 */
	{0x0E, 0x1, {0x91} }, /* SEQ_NT36525B_009 */
	{0x0F, 0x1, {0x5F} }, /* SEQ_NT36525B_010 */
	{0x1F, 0x1, {0x00} }, /* SEQ_NT36525B_011 */
	{0x69, 0x1, {0xA9} }, /* SEQ_NT36525B_012 */
	{0x94, 0x1, {0x00} }, /* SEQ_NT36525B_013 */
	{0x95, 0x1, {0xEB} }, /* SEQ_NT36525B_014 */
	{0x96, 0x1, {0xEB} }, /* SEQ_NT36525B_015 */
	{0xFF, 0x1, {0x24} }, /* SEQ_NT36525B_016 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_017 */
	{0x00, 0x1, {0x1E} }, /* SEQ_NT36525B_018 */
	{0x04, 0x1, {0x21} }, /* SEQ_NT36525B_019 */
	{0x06, 0x1, {0x22} }, /* SEQ_NT36525B_020 */
	{0x07, 0x1, {0x20} }, /* SEQ_NT36525B_021 */
	{0x08, 0x1, {0x1D} }, /* SEQ_NT36525B_022 */
	{0x0A, 0x1, {0x0C} }, /* SEQ_NT36525B_023 */
	{0x0B, 0x1, {0x0D} }, /* SEQ_NT36525B_024 */
	{0x0C, 0x1, {0x0E} }, /* SEQ_NT36525B_025 */
	{0x0D, 0x1, {0x0F} }, /* SEQ_NT36525B_026 */
	{0x0F, 0x1, {0x04} }, /* SEQ_NT36525B_027 */
	{0x10, 0x1, {0x05} }, /* SEQ_NT36525B_028 */
	{0x12, 0x1, {0x1E} }, /* SEQ_NT36525B_029 */
	{0x13, 0x1, {0x1E} }, /* SEQ_NT36525B_030 */
	{0x14, 0x1, {0x1E} }, /* SEQ_NT36525B_031 */
	{0x16, 0x1, {0x1E} }, /* SEQ_NT36525B_032 */
	{0x1A, 0x1, {0x21} }, /* SEQ_NT36525B_033 */
	{0x1C, 0x1, {0x22} }, /* SEQ_NT36525B_034 */
	{0x1D, 0x1, {0x20} }, /* SEQ_NT36525B_035 */
	{0x1E, 0x1, {0x1D} }, /* SEQ_NT36525B_036 */
	{0x20, 0x1, {0x0C} }, /* SEQ_NT36525B_037 */
	{0x21, 0x1, {0x0D} }, /* SEQ_NT36525B_038 */
	{0x22, 0x1, {0x0E} }, /* SEQ_NT36525B_039 */
	{0x23, 0x1, {0x0F} }, /* SEQ_NT36525B_040 */
	{0x25, 0x1, {0x04} }, /* SEQ_NT36525B_041 */
	{0x26, 0x1, {0x05} }, /* SEQ_NT36525B_042 */
	{0x28, 0x1, {0x1E} }, /* SEQ_NT36525B_043 */
	{0x29, 0x1, {0x1E} }, /* SEQ_NT36525B_044 */
	{0x2A, 0x1, {0x1E} }, /* SEQ_NT36525B_045 */
	{0x2F, 0x1, {0x0C} }, /* SEQ_NT36525B_046 */
	{0x30, 0x1, {0x0A} }, /* SEQ_NT36525B_047 */
	{0x33, 0x1, {0x0A} }, /* SEQ_NT36525B_048 */
	{0x34, 0x1, {0x0C} }, /* SEQ_NT36525B_049 */
	{0x37, 0x1, {0x66} }, /* SEQ_NT36525B_050 */
	{0x39, 0x1, {0x00} }, /* SEQ_NT36525B_051 */
	{0x3A, 0x1, {0x10} }, /* SEQ_NT36525B_052 */
	{0x3B, 0x1, {0x90} }, /* SEQ_NT36525B_053 */
	{0x3D, 0x1, {0x92} }, /* SEQ_NT36525B_054 */
	{0x4D, 0x1, {0x12} }, /* SEQ_NT36525B_055 */
	{0x4E, 0x1, {0x34} }, /* SEQ_NT36525B_056 */
	{0x51, 0x1, {0x43} }, /* SEQ_NT36525B_057 */
	{0x52, 0x1, {0x21} }, /* SEQ_NT36525B_058 */
	{0x55, 0x1, {0x87} }, /* SEQ_NT36525B_059 */
	{0x56, 0x1, {0x44} }, /* SEQ_NT36525B_060 */
	{0x5A, 0x1, {0x9F} }, /* SEQ_NT36525B_061 */
	{0x5B, 0x1, {0x90} }, /* SEQ_NT36525B_062 */
	{0x5C, 0x1, {0x00} }, /* SEQ_NT36525B_063 */
	{0x5D, 0x1, {0x00} }, /* SEQ_NT36525B_064 */
	{0x5E, 0x1, {0x04} }, /* SEQ_NT36525B_065 */
	{0x5F, 0x1, {0x00} }, /* SEQ_NT36525B_066 */
	{0x60, 0x1, {0x80} }, /* SEQ_NT36525B_067 */
	{0x61, 0x1, {0x90} }, /* SEQ_NT36525B_068 */
	{0x64, 0x1, {0x11} }, /* SEQ_NT36525B_069 */
	{0x82, 0x1, {0x0D} }, /* SEQ_NT36525B_070 */
	{0x83, 0x1, {0x05} }, /* SEQ_NT36525B_071 */
	{0x85, 0x1, {0x00} }, /* SEQ_NT36525B_072 */
	{0x86, 0x1, {0x51} }, /* SEQ_NT36525B_073 */
	{0x92, 0x1, {0xAD} }, /* SEQ_NT36525B_074 */
	{0x93, 0x1, {0x08} }, /* SEQ_NT36525B_075 */
	{0x94, 0x1, {0x0E} }, /* SEQ_NT36525B_076 */
	{0xAB, 0x1, {0x00} }, /* SEQ_NT36525B_077 */
	{0xAC, 0x1, {0x00} }, /* SEQ_NT36525B_078 */
	{0xAD, 0x1, {0x00} }, /* SEQ_NT36525B_079 */
	{0xAF, 0x1, {0x04} }, /* SEQ_NT36525B_080 */
	{0xB0, 0x1, {0x05} }, /* SEQ_NT36525B_081 */
	{0xB1, 0x1, {0xA8} }, /* SEQ_NT36525B_082 */
	{0xC2, 0x1, {0x86} }, /* SEQ_NT36525B_083 */
	{0xFF, 0x1, {0x25} }, /* SEQ_NT36525B_084 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_085 */
	{0x0A, 0x1, {0x82} }, /* SEQ_NT36525B_086 */
	{0x0B, 0x1, {0x26} }, /* SEQ_NT36525B_087 */
	{0x0C, 0x1, {0x01} }, /* SEQ_NT36525B_088 */
	{0x17, 0x1, {0x82} }, /* SEQ_NT36525B_089 */
	{0x18, 0x1, {0x06} }, /* SEQ_NT36525B_090 */
	{0x19, 0x1, {0x0F} }, /* SEQ_NT36525B_091 */
	{0x58, 0x1, {0x00} }, /* SEQ_NT36525B_092 */
	{0x59, 0x1, {0x00} }, /* SEQ_NT36525B_093 */
	{0x5A, 0x1, {0x40} }, /* SEQ_NT36525B_094 */
	{0x5B, 0x1, {0x80} }, /* SEQ_NT36525B_095 */
	{0x5C, 0x1, {0x00} }, /* SEQ_NT36525B_096 */
	{0x5D, 0x1, {0x9F} }, /* SEQ_NT36525B_097 */
	{0x5E, 0x1, {0x90} }, /* SEQ_NT36525B_098 */
	{0x5F, 0x1, {0x9F} }, /* SEQ_NT36525B_099 */
	{0x60, 0x1, {0x90} }, /* SEQ_NT36525B_100 */
	{0x61, 0x1, {0x9F} }, /* SEQ_NT36525B_101 */
	{0x62, 0x1, {0x90} }, /* SEQ_NT36525B_102 */
	{0x65, 0x1, {0x05} }, /* SEQ_NT36525B_103 */
	{0x66, 0x1, {0xA8} }, /* SEQ_NT36525B_104 */
	{0xC0, 0x1, {0x03} }, /* SEQ_NT36525B_105 */
	{0xCA, 0x1, {0x1C} }, /* SEQ_NT36525B_106 */
	{0xCC, 0x1, {0x1C} }, /* SEQ_NT36525B_107 */
	{0xD3, 0x1, {0x11} }, /* SEQ_NT36525B_108 */
	{0xD4, 0x1, {0xC8} }, /* SEQ_NT36525B_109 */
	{0xD5, 0x1, {0x11} }, /* SEQ_NT36525B_110 */
	{0xD6, 0x1, {0x1C} }, /* SEQ_NT36525B_111 */
	{0xD7, 0x1, {0x11} }, /* SEQ_NT36525B_112 */
	{0xFF, 0x1, {0x26} }, /* SEQ_NT36525B_113 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_114 */
	{0x00, 0x1, {0xA0} }, /* SEQ_NT36525B_115 */
	{0xFF, 0x1, {0x27} }, /* SEQ_NT36525B_116 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_117 */
	{0x13, 0x1, {0x00} }, /* SEQ_NT36525B_118 */
	{0x15, 0x1, {0xB4} }, /* SEQ_NT36525B_119 */
	{0x1F, 0x1, {0x55} }, /* SEQ_NT36525B_120 */
	{0x26, 0x1, {0x0F} }, /* SEQ_NT36525B_121 */
	{0xC0, 0x1, {0x18} }, /* SEQ_NT36525B_122 */
	{0xC1, 0x1, {0xF0} }, /* SEQ_NT36525B_123 */
	{0xC2, 0x1, {0x00} }, /* SEQ_NT36525B_124 */
	{0xC3, 0x1, {0x00} }, /* SEQ_NT36525B_125 */
	{0xC4, 0x1, {0xF0} }, /* SEQ_NT36525B_126 */
	{0xC5, 0x1, {0x00} }, /* SEQ_NT36525B_127 */
	{0xC6, 0x1, {0x00} }, /* SEQ_NT36525B_128 */
	{0xC7, 0x1, {0x03} }, /* SEQ_NT36525B_129 */
	{0xFF, 0x1, {0x23} }, /* SEQ_NT36525B_130 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_131 */
	{0x12, 0x1, {0xB4} }, /* SEQ_NT36525B_132 */
	{0x15, 0x1, {0xE9} }, /* SEQ_NT36525B_133 */
	{0x16, 0x1, {0x0B} }, /* SEQ_NT36525B_134 */
	{0xFF, 0x1, {0x20} }, /* SEQ_NT36525B_135 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_136 */

	{0xB0, 0x10,		/* SEQ_NT36525B_137 */
	 {0x00, 0x08, 0x00, 0x18, 0x00, 0x33, 0x00, 0x4B, 0x00, 0x5F,
	  0x00, 0x72, 0x00, 0x83, 0x00, 0x94 } },
	{0xB1, 0x10,		/* SEQ_NT36525B_138 */
	 {0x00, 0xA2, 0x00, 0xD7, 0x00, 0xFF, 0x01, 0x41, 0x01, 0x6F,
	  0x01, 0xBD, 0x01, 0xF8, 0x01, 0xFA } },
	{0xB2, 0x10,		/* SEQ_NT36525B_139 */
	 {0x02, 0x36, 0x02, 0x71, 0x02, 0x9C, 0x02, 0xD0, 0x02, 0xF5,
	  0x03, 0x20, 0x03, 0x30, 0x03, 0x3E } },
	{0xB3, 0x0C,		/* SEQ_NT36525B_140 */
	 {0x03, 0x4F, 0x03, 0x63, 0x03, 0x7B, 0x03, 0x95, 0x03, 0xA6,
	  0x03, 0xA7 } },
	{0xB4, 0x10,		/* SEQ_NT36525B_141 */
	 {0x00, 0x08, 0x00, 0x18, 0x00, 0x33, 0x00, 0x4B, 0x00, 0x5F,
	  0x00, 0x72, 0x00, 0x83, 0x00, 0x94 } },
	{0xB5, 0x10,		/* SEQ_NT36525B_142 */
	 {0x00, 0xA2, 0x00, 0xD7, 0x00, 0xFF, 0x01, 0x41, 0x01, 0x6F,
	  0x01, 0xBD, 0x01, 0xF8, 0x01, 0xFA } },
	{0xB6, 0x10,		/* SEQ_NT36525B_143 */
	 {0x02, 0x36, 0x02, 0x71, 0x02, 0x9C, 0x02, 0xD0, 0x02, 0xF5,
	  0x03, 0x20, 0x03, 0x30, 0x03, 0x3E } },
	{0xB7, 0x0C,		/* SEQ_NT36525B_144 */
	 {0x03, 0x4F, 0x03, 0x63, 0x03, 0x7B, 0x03, 0x95, 0x03, 0xA6,
	  0x03, 0xA7 } },
	{0xB8, 0x10,		/* SEQ_NT36525B_145 */
	 {0x00, 0x08, 0x00, 0x18, 0x00, 0x33, 0x00, 0x4B, 0x00, 0x5F,
	  0x00, 0x72, 0x00, 0x83, 0x00, 0x94 } },
	{0xB9, 0x10,		/* SEQ_NT36525B_146 */
	 {0x00, 0xA2, 0x00, 0xD7, 0x00, 0xFF, 0x01, 0x41, 0x01, 0x6F,
	  0x01, 0xBD, 0x01, 0xF8, 0x01, 0xFA } },
	{0xBA, 0x10,		/* SEQ_NT36525B_147 */
	 {0x02, 0x36, 0x02, 0x71, 0x02, 0x9C, 0x02, 0xD0, 0x02, 0xF5,
	  0x03, 0x20, 0x03, 0x30, 0x03, 0x3E } },
	{0xBB, 0x0C,		/* SEQ_NT36525B_148 */
	 {0x03, 0x4F, 0x03, 0x63, 0x03, 0x7B, 0x03, 0x95, 0x03, 0xA6,
	  0x03, 0xA7 } },

	{0xFF, 0x1, {0x21} }, /* SEQ_NT36525B_149 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_150 */

	{0xB0, 0x10,		/* SEQ_NT36525B_151 */
	 {0x00, 0x00, 0x00, 0x10, 0x00, 0x2B, 0x00, 0x43, 0x00, 0x57,
	  0x00, 0x6A, 0x00, 0x7B, 0x00, 0x8C } },
	{0xB1, 0x10,		/* SEQ_NT36525B_152 */
	 {0x00, 0x9A, 0x00, 0xCF, 0x00, 0xF7, 0x01, 0x39, 0x01, 0x67,
	  0x01, 0xB5, 0x01, 0xF0, 0x01, 0xF2 } },
	{0xB2, 0x10,		/* SEQ_NT36525B_153 */
	 {0x02, 0x2E, 0x02, 0x7B, 0x02, 0xAE, 0x02, 0xEA, 0x03, 0x13,
	  0x03, 0x42, 0x03, 0x52, 0x03, 0x62 } },
	{0xB3, 0x0C,		/* SEQ_NT36525B_154 */
	 {0x03, 0x73, 0x03, 0x89, 0x03, 0xA1, 0x03, 0xBD, 0x03, 0xCE,
	  0x03, 0xD9 } },
	{0xB4, 0x10,		/* SEQ_NT36525B_155 */
	 {0x00, 0x00, 0x00, 0x10, 0x00, 0x2B, 0x00, 0x43, 0x00, 0x57,
	  0x00, 0x6A, 0x00, 0x7B, 0x00, 0x8C } },
	{0xB5, 0x10,		/* SEQ_NT36525B_156 */
	 {0x00, 0x9A, 0x00, 0xCF, 0x00, 0xF7, 0x01, 0x39, 0x01, 0x67,
	  0x01, 0xB5, 0x01, 0xF0, 0x01, 0xF2 } },
	{0xB6, 0x10,		/* SEQ_NT36525B_157 */
	 {0x02, 0x2E, 0x02, 0x7B, 0x02, 0xAE, 0x02, 0xEA, 0x03, 0x13,
	  0x03, 0x42, 0x03, 0x52, 0x03, 0x62 } },
	{0xB7, 0x0C,		/* SEQ_NT36525B_158 */
	 {0x03, 0x73, 0x03, 0x89, 0x03, 0xA1, 0x03, 0xBD, 0x03, 0xCE,
	  0x03, 0xD9 } },
	{0xB8, 0x10,		/* SEQ_NT36525B_159 */
	 {0x00, 0x00, 0x00, 0x10, 0x00, 0x2B, 0x00, 0x43, 0x00, 0x57,
	  0x00, 0x6A, 0x00, 0x7B, 0x00, 0x8C } },
	{0xB9, 0x10,		/* SEQ_NT36525B_160 */
	 {0x00, 0x9A, 0x00, 0xCF, 0x00, 0xF7, 0x01, 0x39, 0x01, 0x67,
	  0x01, 0xB5, 0x01, 0xF0, 0x01, 0xF2 } },
	{0xBA, 0x10,		/* SEQ_NT36525B_161 */
	 {0x02, 0x2E, 0x02, 0x7B, 0x02, 0xAE, 0x02, 0xEA, 0x03, 0x13,
	  0x03, 0x42, 0x03, 0x52, 0x03, 0x62 } },
	{0xBB, 0x0C,		/* SEQ_NT36525B_162 */
	 {0x03, 0x73, 0x03, 0x89, 0x03, 0xA1, 0x03, 0xBD, 0x03, 0xCE,
	  0x03, 0xD9 } },

	{0xFF, 0x1, {0x23} }, /* SEQ_NT36525B_163 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_164 */
	{0x07, 0x1, {0x20} }, /* SEQ_NT36525B_165 */
	{0x08, 0x1, {0x0F} }, /* SEQ_NT36525B_166 */
	{0xFF, 0x1, {0x10} }, /* SEQ_NT36525B_167 */
	{0xFB, 0x1, {0x01} }, /* SEQ_NT36525B_168 */
	{0xBA, 0x1, {0x03} }, /* SEQ_NT36525B_169 */

	{0x53, 0x1, {0x24} }, /* SEQ_NT36525B_BRIGHTNESS_MODE */
	{0x51, 0x1, {0x88} }, /* SEQ_NT36525B_BRIGHTNESS */
	{0x11, 0x0, {} },	/* a02_sleep_out */
	{REGFLAG_DELAY, 150, {} },/* wait 4 frame */
	{0x29, 0x0, {} },	/* a02_display_on */
};

static struct LCM_setting_table
__maybe_unused lcm_deep_sleep_mode_in_setting[] = {
	{0x28, 0x0, {} },
	{REGFLAG_DELAY, 50, {} },
	{0x10, 0x0, {} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table __maybe_unused lcm_sleep_out_setting[] = {
	{0x11, 0x0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0x0, {} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x53, 0x1, {0x24} }, /* SEQ_NT36525B_BRIGHTNESS_MODE */
	{0x51, 0x1, {0x88} }, /* SEQ_NT36525B_BRIGHTNESS */
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

/* i2c control start */
//#define LCM_I2C_ADDR 0x11
//#define LCM_I2C_BUSNUM  1	/* for I2C channel 0 */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{ .compatible = "mediatek,i2c_lcd_bias", },
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect               = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	pr_info("[LCM][I2C] %s\n", __func__);
	pr_info("[LCM][I2C] NT: info==>name=%s addr=0x%x\n",
				client->name, client->addr);
	_lcm_i2c_client = client;

	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_debug("ERROR!! _lcm_i2c_client is null\n");
	return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	int ret;

	pr_debug("[LCM][I2C] %s\n", __func__);
	ret = i2c_add_driver(&_lcm_i2c_driver);
	if (ret < 0) {
		pr_info("[LCM][I2C] %s fail, device delete\n", __func__);
		i2c_del_driver(&_lcm_i2c_driver);
	} else
		pr_info("[LCM][I2C] %s success\n", __func__);

	return ret;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);
/* i2c control end */

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = BURST_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = BURST_VDO_MODE;
	LCM_LOGI("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */
	params->dsi.data_rate = 639;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 252;
	params->dsi.vertical_frontporch = 8;
	params->dsi.vertical_frontporch_for_low_power = 168;//84;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 30;
	params->dsi.horizontal_backporch = 78;
	params->dsi.horizontal_frontporch = 74;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.cont_clock = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 277;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;

	/* for ARR 2.0 */
	params->max_refresh_rate = 60;
	params->min_refresh_rate = 45;

}

int lcm_bias_regulator_init(void)
{
	return 0;
}

int lcm_bias_disable(void)
{
	return 0;
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	pr_info("%s\n", __func__);

	/*HT BL enable*/
	_lcm_i2c_write_bytes(0x0C, 0x2C);
	_lcm_i2c_write_bytes(0x0D, 0x26);
	_lcm_i2c_write_bytes(0x0E, 0x26);
	_lcm_i2c_write_bytes(0x09, 0xBE);
	_lcm_i2c_write_bytes(0x02, 0x6B);
	_lcm_i2c_write_bytes(0x03, 0x0D);
	_lcm_i2c_write_bytes(0x11, 0x74);
	_lcm_i2c_write_bytes(0x04, 0x05);
	_lcm_i2c_write_bytes(0x05, 0xCC);
	_lcm_i2c_write_bytes(0x10, 0x67);
	_lcm_i2c_write_bytes(0x08, 0x13);
}

static void lcm_suspend_power(void)
{
	pr_info("nt36525b : %s\n", __func__);
	SET_RESET_PIN(0);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);

	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENN);
}

static void lcm_resume_power(void)
{
	pr_info("nt36525b : %s\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_ENP);
	SET_RESET_PIN(0);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);

	MDELAY(1);

	lcm_init_power();
}

static void lcm_init(void)
{
	pr_info("nt36525b : %s\n", __func__);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT0);
	MDELAY(10);

	SET_RESET_PIN(1);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCM_RST_OUT1);
	MDELAY(5);

	push_table(NULL, init_setting_vdo, ARRAY_SIZE(init_setting_vdo), 1);
	pr_info("td4150_fhdp----lm36274----lcm mode = vdo mode :%d----\n",
		 lcm_dsi_mode);
}

static void lcm_suspend(void)
{
	pr_info("nt36525b : %s\n", __func__);
	push_table(NULL, lcm_suspend_setting,
		   ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	pr_info("nt36525b : %s\n", __func__);
	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	pr_info("%s,nt36525b_hdp backlight: level = %d\n", __func__, level);

	bl_level[1].para_list[0] = level;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

struct LCM_DRIVER nt36525b_hdp_dsi_vdo_lm36274_lcm_drv = {
	.name = "nt36525b_hdp_dsi_vdo_lm36274_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.update = lcm_update,
};
