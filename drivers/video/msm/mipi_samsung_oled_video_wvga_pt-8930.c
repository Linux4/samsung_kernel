/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "mipi_samsung_oled-8930.h"

#define MAX_GAMMA_VALUE 25
#define GEN_GAMMA_MAX 21

static struct msm_panel_info pinfo;
struct mipi_panel_data mipi_pd;

/*
 *  Panel Condition Set
 */
static char panel_cond_set1[] = {
	0xF8, 0x01,
	0x2C, 0x2C, 0x07, 0x07, 0x5F,
	0xB3, 0x6D, 0x97, 0x1D, 0x3A,
	0x0F, 0x00, 0x00
};
static char display_cond_set1[] = {
	0xF2,
	0x02, 0x9, 0x69, 0x14, 0x10,
};
static char display_cond_set2[] = {
	0xF7,
	0x03, 0x00, 0x00,
};
static char once_booting_set[] = {
	0xD5, 
	0xE7, 0x14, 0x60, 0x17, 0x0A,
	0x49, 0xc3, 0x8F, 0x19, 0x64,
	0x91, 0x84, 0x76, 0x20, 0x43,
	0x00,
};

static char gamma_set_cmd1[] = {
	0xFA,  
	0x02, 0x31, 0x00, 0x4F, 0x14,
	0x6E, 0x00, 0xA3, 0xC0, 0x92,
	0xA4, 0xBA, 0x93, 0xBD, 0xC8,
	0xAF, 0x00, 0xB0, 0x00, 0xA2,
	0x00, 0xD1,
};

static char gamma_set_update[] = {
	0xFA,
	0x03
};
static char etc_cond_set1[] = {
	0xF6, 0x00, 0x8E, 0x0F
};
static char etc_cond_set2[] = {
	0xB3, 0x6C
};
static char etc_cond_set3[] = {
	0xB5,
	0x2C, 0x12, 0x0C, 0x0A, 0x10,
	0x0E, 0x17, 0x13, 0x1F, 0x1A,
	0x2A, 0x24, 0x1F, 0x1B, 0x1A,
	0x17, 0x2B, 0x26, 0x22, 0x20,
	0x3A, 0x34, 0x30, 0x2C, 0x29,
	0x26, 0x25, 0x23, 0x21, 0x20,
	0x1E, 0x1E
};
static char etc_cond_set4[] = {
	0xB6,
	0x00, 0x00, 0x11, 0x22, 0x33,
	0x44, 0x44, 0x44, 0x55, 0x55,
	0x66, 0x66, 0x66, 0x66, 0x66,
	0x66
};
static char etc_cond_set5[] = {
	0xB7,
	0x2C, 0x12, 0x0C, 0x0A, 0x10,
	0x0E, 0x17, 0x13, 0x1F, 0x1A,
	0x2A, 0x24, 0x1F, 0x1B, 0x1A,
	0x17, 0x2B, 0x26, 0x22, 0x20,
	0x3A, 0x34, 0x30, 0x2C, 0x29,
	0x26, 0x25, 0x23, 0x21, 0x20,
	0x1E, 0x1E
};


static char etc_cond_set6[] = {
	0xB8,
	0x00, 0x00, 0x11, 0x22, 0x33,
	0x44, 0x44, 0x44, 0x55, 0x55,
	0x66, 0x66, 0x66, 0x66, 0x66,
	0x66
};

static char etc_cond_set7[] = {
	0xB9,
	0x2C, 0x12, 0x0C, 0x0A, 0x10,
	0x0E, 0x17, 0x13, 0x1F, 0x1A,
	0x2A, 0x24, 0x1F, 0x1B, 0x1A,
	0x17, 0x2B, 0x26, 0x22, 0x20,
	0x3A, 0x34, 0x30, 0x2C, 0x29,
	0x26, 0x25, 0x23, 0x21, 0x20,
	0x1E, 0x1E
};

static char etc_cond_set8[] = {
	0xBA,
	0x00, 0x00, 0x11, 0x22, 0x33,
	0x44, 0x44, 0x44, 0x55, 0x55,
	0x66, 0x66, 0x66, 0x66, 0x66,
	0x66
};

static char esd_set[] = {
	0x05,
	0xE7, 0x14, 0x60, 0x17, 0x0A,
	0x49, 0xC3, 0x8F, 0x19, 0x64,
	0x91, 0x84, 0x76, 0x20, 0x4F
};
static char all_pixel_off[] = { 0x22, /* no param */ };
static char normal_mode_on[] = { 0x13, /* no parm */ };
static char sleep_in[2] = { 0x10, /* no param */ };
static char sleep_out[2] = { 0x11, /* no param */ };
static char display_on[2] = { 0x29, /* no param */ };
static char display_off[2] = { 0x28, /* no param */ };

static char acl_on[] = {
	0xC0,
	0x01
};
static char acl_off_cmd[] = {
	0xC0,
	0x00
};

static char acl_off[] = {
	0xC1,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static char elvss_cond_set[] = {
	0xB2,
	0x10, 0x10, 0x10, 0x10
};

static char elvss_on[] = {
	0xB1,
	0x0B,
};

enum {
	GAMMA_30CD,
	GAMMA_40CD,
	GAMMA_50CD,
	GAMMA_60CD,
	GAMMA_70CD,
	GAMMA_80CD,
	GAMMA_90CD,
	GAMMA_100CD,
	GAMMA_110CD,
	GAMMA_120CD,
	GAMMA_130CD,
	GAMMA_140CD,
	GAMMA_150CD,
	GAMMA_160CD,
	GAMMA_170CD,
	GAMMA_180CD,
	GAMMA_190CD,
	GAMMA_200CD,
	GAMMA_210CD,
	GAMMA_220CD,
	GAMMA_230CD,
	GAMMA_240CD,
	GAMMA_250CD,
	GAMMA_300CD,
};

static int lux_tbl_acl[] = {
	  30, 40, 50, 60, 70, 80,
	  90, 100, 110, 120, 130, 140,
	  150, 160, 170, 180, 190, 200,
	  210, 220, 230, 240, 250, 300
};

static char GAMMA_SmartDimming_COND_SET[] = {
	0xFA,
	0x02, 0x31, 0x00, 0x4F, 0x14,
	0x6E, 0x00, 0xA3, 0xC0, 0x92,
	0xA4, 0xBA, 0x93, 0xBD, 0xC8,
	0xAF, 0x00, 0xB0, 0x00, 0xA2,
	0x00, 0xD1,
};

static char prepare_mtp_read1[] = {
	0xF0,
	0x5A, 0x5A
};

static char prepare_mtp_read2[] = {
	0xF1,
	0x5A, 0x5A
};

static char contention_error_remove[] = {
	0xD5,
	0xE7, 0x14, 0x60, 0x17, 0x0A,
	0x49, 0xC3, 0x8F, 0x19, 0x64,
	0x91, 0x84, 0x76, 0x20, 0x43,
	0x00,
};

static struct dsi_cmd_desc samsung_display_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(prepare_mtp_read1), prepare_mtp_read1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(prepare_mtp_read2), prepare_mtp_read2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(once_booting_set), once_booting_set},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 10,
		sizeof(sleep_out), sleep_out},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(contention_error_remove), contention_error_remove},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(panel_cond_set1), panel_cond_set1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_cond_set1), display_cond_set1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_cond_set2), display_cond_set2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(gamma_set_cmd1), gamma_set_cmd1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(gamma_set_update), gamma_set_update},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set1), etc_cond_set1},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,		
		sizeof(etc_cond_set2), etc_cond_set2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set3), etc_cond_set3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set4), etc_cond_set4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set5), etc_cond_set5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set6), etc_cond_set6},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(etc_cond_set7), etc_cond_set7},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(etc_cond_set8), etc_cond_set8},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(esd_set), esd_set},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(acl_off), acl_off}, //joann_test
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(acl_on), acl_on},		

	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc samsung_panel_ready_to_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_off), display_off},
};

static struct dsi_cmd_desc samsung_panel_on_cmds[] = {
	{DTYPE_GEN_WRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc samsung_panel_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(sleep_in), sleep_in},
};

static struct dsi_cmd_desc samsung_panel_late_on_cmds[] = {
	{DTYPE_GEN_WRITE, 1, 0, 0, 0,
		sizeof(normal_mode_on), normal_mode_on},
	{DTYPE_GEN_WRITE, 1, 0, 0, 5,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc samsung_panel_early_off_cmds[] = {
	{DTYPE_GEN_WRITE, 1, 0, 0, 0,
		sizeof(all_pixel_off), all_pixel_off},
};
static struct dsi_cmd_desc samsung_mtp_read_cmds[] = {

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(prepare_mtp_read1), prepare_mtp_read1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(prepare_mtp_read2), prepare_mtp_read2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 120,
		sizeof(sleep_out), sleep_out},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(contention_error_remove), contention_error_remove},
};
/********************* ACL *******************/
/*
static char ACL_COND_SET_50[] = {
	0xC1,
	0x4D, 0x96, 0x1D, 0x00, 0x00, 0x01,
	0xDF, 0x00, 0x00, 0x03, 0x1F, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01,
	0x08, 0x0F, 0x16, 0x1D, 0x24, 0x2A,
	0x31, 0x38, 0x3F, 0x46,
};
*/
static char ACL_COND_SET_40[] = {
	0xC1,
	0x4D, 0x96, 0x1D, 0x00, 0x00, 0x01,
	0xDF, 0x00, 0x00, 0x03, 0x1F, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01,
	0x06, 0x0C, 0x11, 0x16, 0x1C, 0x21,
	0x26, 0x2B, 0x31, 0x36,
};
/*
static struct dsi_cmd_desc DSI_CMD_ACL_50 = {
	DTYPE_DCS_LWRITE, 1, 0, 0, 0,
	sizeof(ACL_COND_SET_50), ACL_COND_SET_50 };
*/
static struct dsi_cmd_desc DSI_CMD_ACL_40 = {
	DTYPE_DCS_LWRITE, 1, 0, 0, 0,
	sizeof(ACL_COND_SET_40), ACL_COND_SET_40 };

static struct dsi_cmd_desc_LCD lcd_acl_table[] = {
	{0, "30", NULL},
	{0, "40", NULL},
	{40, "50", &DSI_CMD_ACL_40},
	{40, "60", &DSI_CMD_ACL_40},
	{40, "70", &DSI_CMD_ACL_40},
	{40, "80", &DSI_CMD_ACL_40},
	{40, "90", &DSI_CMD_ACL_40},
	{40, "100", &DSI_CMD_ACL_40},
	{40, "110", &DSI_CMD_ACL_40},
	{40, "120", &DSI_CMD_ACL_40},
	{40, "130", &DSI_CMD_ACL_40},
	{40, "140", &DSI_CMD_ACL_40},
	{40, "150", &DSI_CMD_ACL_40},
	{40, "160", &DSI_CMD_ACL_40},
	{40, "170", &DSI_CMD_ACL_40},
	{40, "180", &DSI_CMD_ACL_40},
	{40, "190", &DSI_CMD_ACL_40},
	{40, "200", &DSI_CMD_ACL_40},
	{40, "210", &DSI_CMD_ACL_40},
	{40, "220", &DSI_CMD_ACL_40},
	{40, "230", &DSI_CMD_ACL_40},
	{40, "240", &DSI_CMD_ACL_40},
	{40, "250", &DSI_CMD_ACL_40},
	{40, "300", &DSI_CMD_ACL_40},
};

static struct dsi_cmd_desc samsung_panel_acl_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(acl_on), acl_on},
};

static struct dsi_cmd_desc samsung_panel_acl_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(acl_off), acl_off},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(acl_off_cmd), acl_off_cmd},
};
static struct dsi_cmd_desc samsung_panel_acl_update_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
	 sizeof(ACL_COND_SET_40), ACL_COND_SET_40},
};

static struct dsi_cmd_desc samsung_panel_gamma_update_cmds[2] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
	 sizeof(gamma_set_cmd1), gamma_set_cmd1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
	 sizeof(gamma_set_update), gamma_set_update},
};

static struct dsi_cmd_desc samsung_panel_elvss_update_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(elvss_on), elvss_on},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	 sizeof(elvss_cond_set), elvss_cond_set},
};

/********************* ELVSS *******************/
#define LCD_ELVSS_DELTA_300CD (0)
#define LCD_ELVSS_DELTA_200CD (0x07)
#define LCD_ELVSS_DELTA_160CD (0x09)
#define LCD_ELVSS_DELTA_100CD (0x0D)
#define LCD_ELVSS_RESULT_LIMIT	(0x1F)
static int GET_ELVSS_ID[] = {
	LCD_ELVSS_DELTA_100CD,/* 0 = 30_dimming,*/
	LCD_ELVSS_DELTA_100CD,/* 1 = 40*/
	LCD_ELVSS_DELTA_100CD,/* 2 = 50*/
	LCD_ELVSS_DELTA_100CD,/* 3 = 60,*/
	LCD_ELVSS_DELTA_100CD,/* 4 = 70,*/
	LCD_ELVSS_DELTA_100CD,/* 5 = 80,*/
	LCD_ELVSS_DELTA_100CD,/* 6 = 90,*/
	LCD_ELVSS_DELTA_100CD,/* 7 = 100,*/
	LCD_ELVSS_DELTA_160CD,/* 8 = 110,*/
	LCD_ELVSS_DELTA_160CD,/* 9 = 120,*/
	LCD_ELVSS_DELTA_160CD,/* 10 = 130,*/
	LCD_ELVSS_DELTA_160CD,/* 11 = 140,*/
	LCD_ELVSS_DELTA_160CD,/* 12 = 150,*/
	LCD_ELVSS_DELTA_160CD,/* 13 = 160,*/
	LCD_ELVSS_DELTA_200CD,/* 14 = 170,*/
	LCD_ELVSS_DELTA_200CD,/* 15 = 180,*/
	LCD_ELVSS_DELTA_200CD,/* 16 = 190,*/
	LCD_ELVSS_DELTA_200CD,/* 17 = 200,*/
	LCD_ELVSS_DELTA_300CD,/* 18 = 210,*/
	LCD_ELVSS_DELTA_300CD,/* 19 = 220,*/
	LCD_ELVSS_DELTA_300CD,/* 20 = 230,*/
	LCD_ELVSS_DELTA_300CD,/* 21 = 240,*/
	LCD_ELVSS_DELTA_300CD,/* 22 = 250,*/
	LCD_ELVSS_DELTA_300CD/* 23 = 300,*/
};

#define LCD_ELVSS_DEFAULT_100CD (0x1D)
#define LCD_ELVSS_DEFAULT_160CD (0x17)
#define LCD_ELVSS_DEFAULT_200CD (0x14)
#define LCD_ELVSS_DEFAULT_300CD (0x10)
static int GET_DEFAULT_ELVSS_ID[] = {
	LCD_ELVSS_DEFAULT_100CD,/* 0 = 30_dimming,*/
	LCD_ELVSS_DEFAULT_100CD,/* 1 = 40*/
	LCD_ELVSS_DEFAULT_100CD,/* 2 = 50*/
	LCD_ELVSS_DEFAULT_100CD,/* 3 = 60,*/
	LCD_ELVSS_DEFAULT_100CD,/* 4 = 70,*/
	LCD_ELVSS_DEFAULT_100CD,/* 5 = 80,*/
	LCD_ELVSS_DEFAULT_100CD,/* 6 = 90,*/
	LCD_ELVSS_DEFAULT_100CD,/* 7 = 100,*/
	LCD_ELVSS_DEFAULT_160CD,/* 8 = 110,*/
	LCD_ELVSS_DEFAULT_160CD,/* 9 = 120,*/
	LCD_ELVSS_DEFAULT_160CD,/* 10 = 130,*/
	LCD_ELVSS_DEFAULT_160CD,/* 11 = 140,*/
	LCD_ELVSS_DEFAULT_160CD,/* 12 = 150,*/
	LCD_ELVSS_DEFAULT_160CD,/* 13 = 160,*/
	LCD_ELVSS_DEFAULT_200CD,/* 14 = 170,*/
	LCD_ELVSS_DEFAULT_200CD,/* 15 = 180,*/
	LCD_ELVSS_DEFAULT_200CD,/* 16 = 190,*/
	LCD_ELVSS_DEFAULT_200CD,/* 17 = 200,*/
	LCD_ELVSS_DEFAULT_300CD,/* 18 = 210,*/
	LCD_ELVSS_DEFAULT_300CD,/* 19 = 220,*/
	LCD_ELVSS_DEFAULT_300CD,/* 20 = 230,*/
	LCD_ELVSS_DEFAULT_300CD,/* 21 = 240,*/
	LCD_ELVSS_DEFAULT_300CD,/* 22 = 250,*/
	LCD_ELVSS_DEFAULT_300CD/* 23 = 300,*/
};
static int get_candela_index(int bl_level)
{
	int backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */

	switch (bl_level) {
	case 0 ... 39:
		backlightlevel = GAMMA_30CD; /* 0*/
		break;
	case 40 ... 49:
		backlightlevel = GAMMA_40CD; /* 1 */
		break;
	case 50 ... 59:
		backlightlevel = GAMMA_50CD; /* 2 */
		break;
	case 60 ... 69:
		backlightlevel = GAMMA_60CD; /* 3 */
		break;
	case 70 ... 79:
		backlightlevel = GAMMA_70CD; /* 4 */
		break;
	case 80 ... 89:
		backlightlevel = GAMMA_80CD; /* 5 */
		break;
	case 90 ... 99:
		backlightlevel = GAMMA_90CD; /* 6 */
		break;
	case 100 ... 109:
		backlightlevel = GAMMA_100CD; /* 7 */
		break;
	case 110 ... 119:
		backlightlevel = GAMMA_110CD; /* 8 */
		break;
	case 120 ... 129:
		backlightlevel = GAMMA_120CD; /* 9 */
		break;
	case 130 ... 139:
		backlightlevel = GAMMA_130CD; /* 10 */
		break;
	case 140 ... 149:
		backlightlevel = GAMMA_140CD; /* 11 */
		break;
	case 150 ... 159:
		backlightlevel = GAMMA_150CD; /* 12 */
		break;
	case 160 ... 169:
		backlightlevel = GAMMA_160CD; /* 13 */
		break;
	case 170 ... 179:
		backlightlevel = GAMMA_170CD; /* 14 */
		break;
	case 180 ... 189:
		backlightlevel = GAMMA_180CD; /* 15 */
		break;
	case 190 ... 199:
		backlightlevel = GAMMA_190CD; /* 16 */
		break;
	case 200 ... 209:
#if defined(CONFIG_MACH_APEXQ)
		if (poweroff_charging)
			backlightlevel = GAMMA_210CD; /* 17 */
		else
			backlightlevel = GAMMA_200CD; /* 17 */
#else
		backlightlevel = GAMMA_200CD; /* 17 */
#endif
		break;
	case 210 ... 219:
		backlightlevel = GAMMA_210CD; /* 18 */
		break;
	case 220 ... 229:
		backlightlevel = GAMMA_220CD; /* 10 */
		break;
	case 230 ... 239:
		backlightlevel = GAMMA_230CD; /* 20 */
		break;
	case 240 ... 249:
		backlightlevel = GAMMA_240CD; /* 21 */
		break;
	case 250 ... 254:
		backlightlevel = GAMMA_250CD; /* 22 */
		break;
	case 255:
		backlightlevel = GAMMA_300CD; /* 23 */
		break;
	default:
		backlightlevel = GAMMA_40CD; /* 1 */
		break;
	}
	return backlightlevel;
}

static int set_acl_on_level(int bl_level)
{
	int cd;
	cd = get_candela_index(bl_level);

	if (!lcd_acl_table[cd].lux)
		return 1;

	if (lcd_acl_table[cd].lux) {
		samsung_panel_acl_update_cmds[0].dlen =
		    lcd_acl_table[cd].cmd->dlen;
		samsung_panel_acl_update_cmds[0].payload =
		    lcd_acl_table[cd].cmd->payload;
	}
	return 0;
}

static int set_elvss_level(int bl_level)
{
	unsigned char calc_elvss;
	int cd;
	int id3;
	int id2 = (mipi_pd.manufacture_id>>8) & 0xFF;

	cd = get_candela_index(bl_level);
	id3 = mipi_pd.manufacture_id & 0xFF;

	if ((id2 == 0xA4) || (id2 == 0xB4) || (id2 == 0xA6) ||(id2 == 0xB6))
		calc_elvss = id3 + GET_ELVSS_ID[cd];
	else
		calc_elvss = GET_DEFAULT_ELVSS_ID[cd];

	pr_debug("%s: ID2=%x, ID3=%x, calc_elvss = %x\n", __func__, id2, id3,
		calc_elvss);

	/*
	*	COMANCHE DC-DC : STOD13CM
	*	AEGIS2 DC-DC : STOD13AS
	*	APEXQ DC-DC : STOD13AS
	*/
	if (calc_elvss > LCD_ELVSS_RESULT_LIMIT)
		calc_elvss = LCD_ELVSS_RESULT_LIMIT;

	if (elvss_cond_set[2] == calc_elvss)
		return 1;

	elvss_cond_set[1] = calc_elvss;
	elvss_cond_set[2] = calc_elvss;
	elvss_cond_set[3] = calc_elvss;
	elvss_cond_set[4] = calc_elvss;

	return 0;
}

void reset_gamma_level(void)
{
	pr_info("reset_gamma_level\n");
	mipi_pd.lcd_current_cd_idx = -1;
	mipi_pd.ldi_acl_stat = false;
	elvss_cond_set[2] = 0x00;
}

static int set_gamma_level(int bl_level, enum gamma_mode_list gamma_mode)
{
	int cd;
	int *lux_tbl = lux_tbl_acl;

	cd = get_candela_index(bl_level);

	if (mipi_pd.lcd_current_cd_idx == cd)
		return -1;
	else
	    mipi_pd.lcd_current_cd_idx = cd;

	pr_debug(" Gamma mode: %d\n", gamma_mode);

	if (gamma_mode == GAMMA_SMART) {

		/*  SMART Dimming gamma_lux;  */
		char pBuffer[256];
		int i;
		int gamma_lux;

		gamma_lux = lux_tbl[cd];

		if (gamma_lux > SmartDimming_CANDELA_UPPER_LIMIT)
			gamma_lux = SmartDimming_CANDELA_UPPER_LIMIT;

		mipi_pd.smart_s6e63m0.brightness_level = gamma_lux;

		for (i = SmartDimming_GammaUpdate_Pos;
		     i < sizeof(GAMMA_SmartDimming_COND_SET); i++)
			GAMMA_SmartDimming_COND_SET[i] = 0;
		generate_gamma(&(mipi_pd.smart_s6e63m0),
						GAMMA_SmartDimming_COND_SET +
						SmartDimming_GammaUpdate_Pos,
								GEN_GAMMA_MAX);

		samsung_panel_gamma_update_cmds[0].dlen =
		    sizeof(GAMMA_SmartDimming_COND_SET);
		samsung_panel_gamma_update_cmds[0].payload =
		    GAMMA_SmartDimming_COND_SET;
		pBuffer[0] = 0;
		for (i = 0; i < sizeof(GAMMA_SmartDimming_COND_SET); i++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x",
				 GAMMA_SmartDimming_COND_SET[i]);
		}
		pr_debug("SD: %03d %s\n", gamma_lux, pBuffer);
		pr_info("bl_level:%d,cd:%d:Candela:%d\n", bl_level, cd,
			gamma_lux);
		}

	return 0;
}

static int is_acl_para_change(int bl_level)
{
	int cd = get_candela_index(bl_level);
	int change = 0;

	if (!lcd_acl_table[cd].lux)
		return 0;

	change = memcmp(samsung_panel_acl_update_cmds[0].payload,
			lcd_acl_table[cd].cmd->payload,
			lcd_acl_table[cd].cmd->dlen);
	return change;
}

static struct dsi_cmd_desc combined_ctrl[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 5,
	 sizeof(GAMMA_SmartDimming_COND_SET),
	 GAMMA_SmartDimming_COND_SET}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 5,
	 sizeof(gamma_set_update), gamma_set_update}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	 sizeof(elvss_cond_set), elvss_cond_set}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	 sizeof(elvss_on), elvss_on}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	sizeof(acl_on),	acl_on}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	sizeof(acl_off),	acl_off}
	,
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0,
	sizeof(ACL_COND_SET_40), ACL_COND_SET_40}
	,
};

static int prepare_brightness_control_cmd_array(int lcd_type, int bl_level)
{
	int cmd_size = 0, gamma_change = 0;
	unsigned char cmds_send_flag = 0;

	gamma_change = set_gamma_level(bl_level,
				mipi_pd.msd->dstat.gamma_mode);
	if (gamma_change < 0)
		return -1;

	/* Prepare the list */
	if (!set_elvss_level(bl_level))
			cmds_send_flag |= 1<<0;

	if (mipi_pd.msd->dstat.acl_on) {
		int acl_change = is_acl_para_change(bl_level);
		int acl_30_40_case = set_acl_on_level(bl_level);
		if (acl_30_40_case  &&
			mipi_pd.ldi_acl_stat == true) {

			cmds_send_flag |= 1<<1;
			mipi_pd.ldi_acl_stat = false;
		}
		if (!acl_30_40_case) {
				if (mipi_pd.ldi_acl_stat == false) {

					cmds_send_flag |= 0x3<<2;
					mipi_pd.ldi_acl_stat = true;

				} else if (acl_change)
					cmds_send_flag |= 1<<3;
		}
	}
	combined_ctrl[cmd_size].payload =
		samsung_panel_gamma_update_cmds[0].payload;
	combined_ctrl[cmd_size].dlen =
		samsung_panel_gamma_update_cmds[0].dlen;
	cmd_size++;

	combined_ctrl[cmd_size].payload = gamma_set_update;
	combined_ctrl[cmd_size].dlen = sizeof(gamma_set_update);
	cmd_size++;

	if (cmds_send_flag & 0x1) { /* elvss change */

		combined_ctrl[cmd_size].payload =
			samsung_panel_elvss_update_cmds[0].payload;
		combined_ctrl[cmd_size].dlen =
			samsung_panel_elvss_update_cmds[0].dlen;
		cmd_size++;

		combined_ctrl[cmd_size].payload =
			samsung_panel_elvss_update_cmds[1].payload;
		combined_ctrl[cmd_size].dlen =
			samsung_panel_elvss_update_cmds[1].dlen;
		cmd_size++;
	}
	if (cmds_send_flag & 0x2) { /* acl off */

		combined_ctrl[cmd_size].payload = acl_off;
		combined_ctrl[cmd_size].dlen = sizeof(acl_off);
		cmd_size++;
	}
	if (cmds_send_flag & 0x8) { /* acl update */

		combined_ctrl[cmd_size].payload =
			samsung_panel_acl_update_cmds[0].payload;
		combined_ctrl[cmd_size].dlen =
			samsung_panel_acl_update_cmds[0].dlen;
		cmd_size++;
	}
	mipi_pd.combined_ctrl.size = cmd_size;
	return cmds_send_flag;
}

struct mipi_panel_data mipi_pd = {
	.panel_name = "SMD_AMS397GEXX\n",
	.ready_to_on = {samsung_display_on_cmds
				, ARRAY_SIZE(samsung_display_on_cmds)},
	.ready_to_off	= {samsung_panel_ready_to_off_cmds
				, ARRAY_SIZE(samsung_panel_ready_to_off_cmds)},
	.on		= {samsung_panel_on_cmds
				, ARRAY_SIZE(samsung_panel_on_cmds)},
	.off		= {samsung_panel_off_cmds
				, ARRAY_SIZE(samsung_panel_off_cmds)},
	.late_on	= {samsung_panel_late_on_cmds
				, ARRAY_SIZE(samsung_panel_late_on_cmds)},
	.early_off	= {samsung_panel_early_off_cmds
				, ARRAY_SIZE(samsung_panel_early_off_cmds)},
	.acl_on = {samsung_panel_acl_on_cmds
			, ARRAY_SIZE(samsung_panel_acl_on_cmds)},
	.acl_off = {samsung_panel_acl_off_cmds
			, ARRAY_SIZE(samsung_panel_acl_off_cmds)},
	.acl_update = {samsung_panel_acl_update_cmds,
	     ARRAY_SIZE(samsung_panel_acl_update_cmds)},
	.set_acl = set_acl_on_level,
	.elvss_update = {samsung_panel_elvss_update_cmds,
	     ARRAY_SIZE(samsung_panel_elvss_update_cmds)},
	.set_elvss = set_elvss_level,
	.gamma_update = {samsung_panel_gamma_update_cmds,
			 ARRAY_SIZE(samsung_panel_gamma_update_cmds)},
	.set_gamma = set_gamma_level,
	.lcd_current_cd_idx = -1,
	.mtp_read_enable = {samsung_mtp_read_cmds
				, ARRAY_SIZE(samsung_mtp_read_cmds)},
	.combined_ctrl = {combined_ctrl, ARRAY_SIZE(combined_ctrl)},
	.prepare_brightness_control_cmd_array =
		prepare_brightness_control_cmd_array,
	.gamma_initial = gamma_set_cmd1,
};

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
/* regulator */
	{0x03, 0x0a, 0x04, 0x00, 0x20},
	/* timing */
	{0x67, 0x2B, 0x0D, 0x00, 0x38, 0x3C, 0x12, 0x2F, 0x18, 0x03, 0x04, 0x0},
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xee, 0x02, 0x86, 0x00},
	/* pll control */
	{0x0, 0x7f, 0x31, 0xda, 0x00, 0x50, 0x48, 0x63,
	0x41, 0x0f, 0x01,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },
};

static int __init mipi_video_samsung_oled_wvga_pt_init(void)
{
	int ret;

#ifdef CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("mipi_video_samsung_oled_wvga"))
		return 0;
#endif

	pinfo.xres = 480;
	pinfo.yres = 800;
	/*
	 *
	 * Panel's Horizontal input timing requirement is to
	 * include dummy(pad) data of 200 clk in addition to
	 * width and porch/sync width values
	 */
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 2;

	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

	pinfo.lcdc.h_back_porch  = 16;
	pinfo.lcdc.h_front_porch = 16;
	pinfo.lcdc.h_pulse_width = 4;
	pinfo.lcdc.v_back_porch  = 7;
	pinfo.lcdc.v_front_porch = 105;
	pinfo.lcdc.v_pulse_width = 2;

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.clk_rate = 341936640;
	pinfo.mipi.mode = DSI_VIDEO_MODE;

	pinfo.mipi.pulse_mode_hsa_he = FALSE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.dlane_swap = 0x01;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	/*pinfo.mipi.tx_eot_append = TRUE;*/

	pinfo.mipi.t_clk_post = 0x19;
	pinfo.mipi.t_clk_pre = 0x2D;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.force_clk_lane_hs = 1;
	pinfo.mipi.esc_byte_ratio = 1;


	ret = mipi_samsung_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT,
						&mipi_pd);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_samsung_oled_wvga_pt_init);
