/*
 * Copyright (C) 2011, Samsung Electronics. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
*/

#include "cmc624-8960.h"
#if defined(CONFIG_FB_MSM_MIPI_NOVATEK_VIDEO_WXGA_PT_PANEL)
#include "samsung_cmc624_tune_konalte-8960.h"
#else
#include "samsung_cmc624_tune-8960.h"
#endif
#define CMC624_INITSEQ cmc624_init
/*#define CMC624_INITSEQ cmc624_bypass*/
#define CMC624_MAX_SETTINGS	 100
#define TUNING_FILE_PATH "/sdcard/tuning/"

#define PMIC_GPIO_LVDS_nSHDN 18	/*  LVDS_nSHDN GPIO on PM8058 is 19 */
#define PMIC_GPIO_BACKLIGHT_RST 36	/*  LVDS_nSHDN GPIO on PM8058 is 37 */
#define PMIC_GPIO_LCD_LEVEL_HIGH	1
#define PMIC_GPIO_LCD_LEVEL_LOW		0

#define IMA_PWR_EN			70
#define IMA_nRST			72
#define IMA_SLEEP			103
#define IMA_CMC_EN			104
#define MLCD_ON				128
#define IMA_I2C_SDA			156
#define IMA_I2C_SCL			157

#define CMC624_FAILSAFE		106
#define P8LTE_LCDON			1
#define P8LTE_LCDOFF		0

/*  BYPASS Mode OFF */
#define BYPASS_MODE_ON		0

struct cmc624_state_type {
	enum eCabc_Mode cabc_mode;
	unsigned int brightness;
	unsigned int suspended;
	enum eLcd_mDNIe_UI scenario;
	enum SCENARIO_COLOR_TONE browser_scenario;
	enum eBackground_Mode background;
/*This value must reset to 0 (standard value) when change scenario*/
	enum eCurrent_Temp temperature;
	enum eOutdoor_Mode outdoor;
	enum eNegative_Mode negative;
	const struct str_sub_unit *sub_tune;
	const struct str_main_unit *main_tune;
};

/*  CMC624 function */
extern int cmc624_set_pwm_backlight(int level);
int cmc624_sysfs_init(void);
void bypass_onoff_ctrl(int value);
void cabc_onoff_ctrl(int value);
void set_backlight_pwm(int level);
int load_tuning_data(char *filename);
int apply_main_tune_value(enum eLcd_mDNIe_UI ui, enum eBackground_Mode bg,
			enum eCabc_Mode cabc, int force);
int apply_sub_tune_value(enum eCurrent_Temp temp, enum eOutdoor_Mode ove,
			enum eCabc_Mode cabc, int force);
int apply_browser_tune_value(enum SCENARIO_COLOR_TONE browser_mode, int force);
int apply_negative_tune_value(enum eNegative_Mode negative_mode,
			enum eCabc_Mode cabc);
void dump_cmc624_register(void);
int samsung_cmc624_init(void);
/*int samsung_cmc624_setup(void);*/
int samsung_cmc624_on(int enable);
int samsung_cmc624_bypass_mode(void);
int samsung_cmc624_normal_mode(void);
int samsung_lvds_on(int enable);
int samsung_backlight_en(int enable);
void cmc624_manual_brightness(int bl_lvl);
bool samsung_has_cmc624(void);
void samsung_get_id(unsigned char *buf);
void Check_Prog(void);
void change_mon_clk(void);
void cmc624_Set_Region_Ext(int enable, int hStart, int hEnd, int vStart,
			   int vEnd);
int cmc624_set_pwm_backlight( int level);
unsigned char LCD_Get_Value(void);
unsigned char LCD_ID3(void);
bool Is_4_8LCD_cmc(void);
bool Is_4_65LCD_cmc(void);
extern struct cmc624_state_type cmc624_state;
