/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 *
 */

#ifndef _MDNIE_LITE_TUNING_H_
#define _MDNIE_LITE_TUNING_H_

#include "mdss_samsung_dsi_panel_common.h"

#define LDI_COORDINATE_REG 0xA1

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
#define MDNIE_TUNE_BODY_SIZE 128
#define MDNIE_TUNE_HEAD_SIZE 22
/*temp*/
#define MDNIE_COLOR_BLIND_FIRST_SIZE 128
#define MDNIE_COLOR_BLIND_SECOND_SIZE 22
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)  // TR
#define MDNIE_TUNE_BODY_SIZE 157
#define MDNIE_TUNE_HEAD_SIZE 4
#define MDNIE_COLOR_BLIND_FIRST_SIZE 157
#define MDNIE_COLOR_BLIND_SECOND_SIZE 4
#define SUPPORT_WHITE_RGB
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FULL_HD_PT_PANEL)  // H
#define MDNIE_TUNE_BODY_SIZE 108
#define MDNIE_TUNE_HEAD_SIZE 5
#define MDNIE_COLOR_BLIND_FIRST_SIZE 118
#define MDNIE_COLOR_BLIND_SECOND_SIZE 5
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) // KS01
#define MDNIE_TUNE_BODY_SIZE 108
#define MDNIE_TUNE_HEAD_SIZE 5
#define MDNIE_COLOR_BLIND_FIRST_SIZE 118
#define MDNIE_COLOR_BLIND_SECOND_SIZE 5
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_YOUM_CMD_FULL_HD_PT_PANEL) // F
#define MDNIE_TUNE_BODY_SIZE 108
#define MDNIE_TUNE_HEAD_SIZE 5
#else
#define MDNIE_TUNE_BODY_SIZE 108
#define MDNIE_TUNE_HEAD_SIZE 5
#endif

#define MDNIE_COLOR_BLINDE_CMD 18

/* blind setting value offset (ascr_Cr ~ ascr_Bb) */
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FHD_FA2_PT_PANEL)
#define MDNIE_COLOR_BLINDE_OFFSET 104
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQHD_PT_PANEL) || defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_S6E3HA2_CMD_WQXGA_PT_PANEL)
#define MDNIE_COLOR_BLINDE_OFFSET 133
#else
#define MDNIE_COLOR_BLINDE_OFFSET 18
#endif

#define BROWSER_COLOR_TONE_SET

#define SIG_MDNIE_UI_MODE				0
#define SIG_MDNIE_VIDEO_MODE			1
#define SIG_MDNIE_VIDEO_WARM_MODE		2
#define SIG_MDNIE_VIDEO_COLD_MODE		3
#define SIG_MDNIE_CAMERA_MODE			4
#define SIG_MDNIE_NAVI					5
#define SIG_MDNIE_GALLERY				6
#define SIG_MDNIE_VT					7
#define SIG_MDNIE_BROWSER				8
#define SIG_MDNIE_eBOOK					9
#define SIG_MDNIE_EMAIL					10

#define SIG_MDNIE_DYNAMIC				0
#define SIG_MDNIE_STANDARD				1
#define SIG_MDNIE_MOVIE				2

#if defined(CONFIG_TDMB)
#define SIG_MDNIE_DMB_MODE			20
#define SIG_MDNIE_DMB_WARM_MODE	21
#define SIG_MDNIE_DMB_COLD_MODE	22
#endif

#define SIG_MDNIE_ISDBT_MODE		30
#define SIG_MDNIE_ISDBT_WARM_MODE	31
#define SIG_MDNIE_ISDBT_COLD_MODE	32

#ifdef BROWSER_COLOR_TONE_SET
#define SIG_MDNIE_BROWSER_TONE1	40
#define SIG_MDNIE_BROWSER_TONE2	41
#define SIG_MDNIE_BROWSER_TONE3	42
#endif

enum SCENARIO {
	mDNIe_UI_MODE,
	mDNIe_VIDEO_MODE,
	mDNIe_VIDEO_WARM_MODE,
	mDNIe_VIDEO_COLD_MODE,
	mDNIe_CAMERA_MODE,
	mDNIe_NAVI,
	mDNIe_GALLERY,
	mDNIe_VT_MODE,
	mDNIe_BROWSER_MODE,
	mDNIe_eBOOK_MODE,
	mDNIe_EMAIL_MODE,
#if defined(CONFIG_LCD_HMT)
	mDNIe_HMT_1_MODE,
	mDNIe_HMT_2_MODE,
	mDNIe_HMT_3_MODE,
	mDNIe_HMT_4_MODE,
	mDNIe_HMT_5_MODE,
#endif
#if defined(CONFIG_TDMB)
	mDNIe_DMB_MODE = 20,
	mDNIe_DMB_WARM_MODE,
	mDNIe_DMB_COLD_MODE,
#endif
	MAX_mDNIe_MODE,
#ifdef BROWSER_COLOR_TONE_SET
	mDNIe_BROWSER_TONE1 = 40,
	mDNIe_BROWSER_TONE2,
	mDNIe_BROWSER_TONE3,
#endif
};

enum BACKGROUND {
	DYNAMIC_MODE = 0,
	STANDARD_MODE,
#if !defined(CONFIG_SUPPORT_DISPLAY_OCTA_TFT)
	NATURAL_MODE,
#endif
	MOVIE_MODE,
	AUTO_MODE,
	MAX_BACKGROUND_MODE,
};

enum OUTDOOR {
	OUTDOOR_OFF_MODE = 0,
	OUTDOOR_ON_MODE,
	MAX_OUTDOOR_MODE,
};

enum ACCESSIBILITY {
    ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	ACCESSIBILITY_MAX,
};

#if defined(CONFIG_TDMB)
enum DMB {
	DMB_MODE_OFF = -1,
	DMB_MODE,
	DMB_WARM_MODE,
	DMB_COLD_MODE,
	MAX_DMB_MODE,
};
#endif

#if defined(CONFIG_LCD_HMT)
enum HMT_COLOR_TEMPERATURE {
	HMT_COLOR_TEMP_OFF = 0,
	HMT_COLOR_TEMP_3000K, // 3000K
	HMT_COLOR_TEMP_4000K, // 4000K
	HMT_COLOR_TEMP_5000K, // 5000K
	HMT_COLOR_TEMP_6500K, // 6500K + gamma 2.2
	HMT_COLOR_TEMP_7500K, // 7500K + gamma 2.2
	HMT_COLOR_TEMP_MAX
};
#endif

struct mdnie_lite_tun_type {
	bool mdnie_enable;
	enum SCENARIO scenario;
	enum BACKGROUND background;
	enum OUTDOOR outdoor;
	enum ACCESSIBILITY accessibility;
#if defined(CONFIG_TDMB)
	enum DMB dmb;
#endif
#if defined(CONFIG_LCD_HMT)
	enum HMT_COLOR_TEMPERATURE hmt_color_temperature;
#endif
#if defined(SUPPORT_WHITE_RGB)
	int scr_white_red;
	int scr_white_green;
	int scr_white_blue;
#endif
};

void mdnie_lite_tuning_init(struct mipi_samsung_driver_data *msd);
void init_mdnie_class(void);
void is_negative_on(void);
void coordinate_tunning(int x, int y);
void mDNIe_Set_Mode(void);

#endif /*_MDNIE_LITE_TUNING_H_*/
