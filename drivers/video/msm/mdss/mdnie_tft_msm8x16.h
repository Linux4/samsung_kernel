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

#ifndef _MDNIE_TFT_H_
#define _MDNIE_TFT_H_

#include "mdss_msm8x16_panel.h"

#if defined (CONFIG_FB_MSM_MIPI_HIMAX_WVGA_VIDEO_PANEL)
#define MDNIE_TUNE_FIRST_SIZE 17
#define MDNIE_TUNE_SECOND_SIZE 25
#define MDNIE_TUNE_THIRD_SIZE 49
#define MDNIE_TUNE_FOURTH_SIZE 19
#define MDNIE_TUNE_FIFTH_SIZE 5
#else
#define MDNIE_TUNE_FIRST_SIZE 92
#define MDNIE_TUNE_SECOND_SIZE 5
#endif
#define MDNIE_COLOR_BLIND_FIRST_SIZE 118
#define MDNIE_COLOR_BLIND_SECOND_SIZE 5
#define MDNIE_COLOR_BLINDE_CMD 18

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

enum Lcd_mDNIe_UI {
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
	mDNIE_BLINE_MODE,
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

enum Lcd_mDNIe_Negative {
	mDNIe_NEGATIVE_OFF = 0,
	mDNIe_NEGATIVE_ON,
};

enum Background_Mode {
	DYNAMIC_MODE = 0,
	STANDARD_MODE,
	NATURAL_MODE,
	MOVIE_MODE,
	AUTO_MODE,
	MAX_BACKGROUND_MODE,
};

enum Outdoor_Mode {
	OUTDOOR_OFF_MODE = 0,
	OUTDOOR_ON_MODE,
	MAX_OUTDOOR_MODE,
};

enum ACCESSIBILITY {
        ACCESSIBILITY_OFF,
        NEGATIVE,
        COLOR_BLIND,
        ACCESSIBILITY_MAX,
};

struct mdnie_tft_type {
	bool mdnie_enable;
	enum Background_Mode background;
	enum Outdoor_Mode outdoor;
	enum Lcd_mDNIe_UI scenario;
	enum Lcd_mDNIe_Negative negative;
	enum ACCESSIBILITY blind;
};

void mdnie_tft_init(struct mdss_samsung_driver_data *);
unsigned int mdss_dsi_show_cabc(void );
#if defined(CONFIG_CABC_TUNING)
unsigned char mdss_dsi_panel_cabc_show(void);
void mdss_dsi_panel_cabc_store(unsigned char cabc);
#endif /* CONFIG_CABC_TUNING */
void is_negative_on(void);
void init_mdnie_class(void);
#endif /*_MDNIE_LITE_TUNING_H_*/
