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

#if defined(CONFIG_FB_MSM_MDSS_PANEL_COMMON)
#include "mdss_dsi_panel_common.h"
#else
#include "mdss_samsung_dsi_panel.h"
#endif

#if defined(CONFIG_FB_MSM_MIPI_HX8394C_PANEL)
#define MDNIE_TUNE_FIRST_SIZE 2
#define MDNIE_TUNE_SECOND_SIZE 52
#define MDNIE_TUNE_THIRD_SIZE 2
#define MDNIE_TUNE_FOURTH_SIZE 46

#define MAX_TUNE_SIZE	4

#define PAYLOAD1 mdnie_tune_cmd[0]
#define PAYLOAD2 mdnie_tune_cmd[1]
#define PAYLOAD3 mdnie_tune_cmd[2]
#define PAYLOAD4 mdnie_tune_cmd[3]

#define INPUT_PAYLOAD1(x) PAYLOAD1.payload = x
#define INPUT_PAYLOAD2(x) PAYLOAD2.payload = x
#define INPUT_PAYLOAD3(x) PAYLOAD3.payload = x
#define INPUT_PAYLOAD4(x) PAYLOAD4.payload = x

#elif defined(CONFIG_FB_MSM_MIPI_EA8061V_PANEL)
#define CONFIG_AMOLED
#define MDNIE_TUNE_FIRST_SIZE 92
#define MDNIE_TUNE_SECOND_SIZE 5
#define MDNIE_COLOR_BLIND_FIRST_SIZE 92
#define MDNIE_COLOR_BLIND_SECOND_SIZE 5
#define MAX_TUNE_SIZE	2
#define PAYLOAD1 mdnie_tune_cmd[3]
#define PAYLOAD2 mdnie_tune_cmd[2]
#define INPUT_PAYLOAD1(x) PAYLOAD1.payload = x
#define INPUT_PAYLOAD2(x) PAYLOAD2.payload = x

#else
#define MDNIE_TUNE_FIRST_SIZE 127
#define MAX_TUNE_SIZE	1

#define PAYLOAD1 mdnie_tune_cmd[0]
#define INPUT_PAYLOAD1(x) PAYLOAD1.payload = x
#endif

#define MDNIE_COLOR_BLINDE_CMD 18
#define MDNIE_COLOR_BLINDE_OFFSET 18

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
	MAX_mDNIe_MODE,
};

enum BACKGROUND {
	DYNAMIC_MODE = 0,
	STANDARD_MODE,
#if defined(CONFIG_AMOLED)
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
	ACCESSIBILITY_MAX,
};

struct mdnie_lite_tune_type {
	bool mdnie_enable;
	enum SCENARIO scenario;
	enum BACKGROUND background;
	enum OUTDOOR outdoor;
	enum ACCESSIBILITY accessibility;
};

void mdnie_lite_tuning_init(struct mdss_samsung_driver_data *msd);
void is_negative_on(void);
void init_mdnie_class(void);
void mDNIe_Set_Mode(void);
#if defined(CONFIG_CABC_TUNING)
unsigned char mdss_dsi_panel_cabc_show(void);
void mdss_dsi_panel_cabc_store(unsigned char cabc);
#endif /* CONFIG_CABC_TUNING */
#endif /*_MDNIE_LITE_TUNING_H_*/
