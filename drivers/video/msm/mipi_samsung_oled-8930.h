/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "mipi_dsi.h"
#include <linux/wakelock.h>

#ifndef MIPI_SAMSUNG_OLED_H
#define MIPI_SAMSUNG_OLED_H

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
#define USE_ACL
#define USE_ELVSS
#include "smart_dimming.h"

#define SmartDimming_useSampleValue
#define SmartDimming_CANDELA_UPPER_LIMIT (300)
#define MTP_DATA_SIZE (24)
#define MTP_DATA_SIZE_S6E63M0 (21)
#define MTP_DATA_SIZE_EA8868 (21)
#define ELVSS_DATA_SIZE (24)
#define MTP_REGISTER	(0xD3)
#define ELVSS_REGISTER	 (0xD4)
#define SmartDimming_GammaUpdate_Pos (2)
#define DIMMING_BL (20)
#endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
#include "smart_mtp_s6e39a0x02.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
#include "smart_mtp_s6e88a-8930.h"
#elif defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
#include "smart_mtp_s6e63m0-8930.h"
#endif

#define USE_READ_ID

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
enum mipi_samsung_cmd_list {
	PANEL_READY_TO_ON,
	PANEL_READY_TO_OFF,
	PANEL_ON,
	PANEL_OFF,
	PANEL_LATE_ON,
	PANEL_EARLY_OFF,
	PANEL_BRIGHT_CTRL,
	PANEL_MTP_ENABLE,
	PANEL_MTP_DISABLE,
	PANEL_NEED_FLIP,
	PANEL_ACL_CONTROL,
	PANLE_TEMPERATURE,
	PANLE_TOUCH_KEY,
};
#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	
enum mipi_samsung_cmd_list {
	PANEL_READY_TO_ON,
	PANEL_READY_TO_OFF,
	PANEL_ON,
	PANEL_OFF,
	PANEL_LATE_ON,
	PANEL_EARLY_OFF,
	PANEL_GAMMA_UPDATE,
	PANEL_ELVSS_UPDATE,
	PANEL_ACL_ON,
	PANEL_ACL_OFF,
	PANEL_ACL_UPDATE,
	MTP_READ_ENABLE,
	PANEL_BRIGHT_CTRL,
};
#endif

enum gamma_mode_list {
	GAMMA_2_2 = 0,
	GAMMA_1_9 = 1,
	GAMMA_SMART = 2,
};

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

struct cmd_set {
	struct dsi_cmd_desc *cmd;
	int size;
};

struct gamma_table {
	char *table;
	int table_cnt;
	int data_size;
};
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT) \
	|| defined(CONFIG_FB_MSM_MIPI_AMS367_OLED_VIDEO_WVGA_PT_PANEL)
struct mipi_panel_data {
	const char panel_name[20];
	struct cmd_set on;
	struct cmd_set off;
	struct cmd_set late_on;
	struct cmd_set early_off;
	struct cmd_set mtp_enable;
	struct cmd_set mtp_disable;
	struct cmd_set need_flip;
	struct cmd_set brightness;
	struct cmd_set acl_cmds;
	struct cmd_set temperature;
	struct cmd_set touch_key;
	unsigned int manufacture_id;

	struct mipi_samsung_driver_data *msd;

	int (*backlight_control)(int bl_level);
	int (*acl_control)(int bl_level);
	int (*cmd_set_change)(int cmd_set, int panel_id);

	int *lux_table;
	int lux_table_max_cnt;
	struct SMART_DIM smart_se6e8fa;

	void(*reset_bl_level)(void);
	int brightness_level;

	int acl_status;
	
#if defined(CONFIG_MDNIE_LITE_TUNING)
	int coordinate[2];
#endif
	int siop_pre_acl_status;
	int siop_status;

	int first_bl_hbm_psre;
	char ldi_rev;
};

#elif defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
struct mipi_panel_data {
	const char panel_name[20];
	struct cmd_set ready_to_on;
	struct cmd_set ready_to_off;
	struct cmd_set on;
	struct cmd_set off;
	struct cmd_set late_on;
	struct cmd_set early_off;
	struct cmd_set gamma_update;
	struct cmd_set elvss_update;
	struct cmd_set mtp_read_enable;

	struct cmd_set acl_update;
#ifdef USE_ACL
	struct cmd_set acl_on;
	struct cmd_set acl_off;
	boolean ldi_acl_stat;
#endif
	struct str_smart_dim smart;
	signed char lcd_current_cd_idx;
	unsigned char lcd_mtp_data[MTP_DATA_SIZE+16] ;
	unsigned char lcd_elvss_data[ELVSS_DATA_SIZE+16];
	unsigned char *gamma_smartdim;
	unsigned char *gamma_initial;

	unsigned int manufacture_id;
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	struct SMART_DIM smart_s6e63m0;
#endif	
#if  defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)
	struct SMART_DIM smart_s6e39a0x02;
#endif
	int (*set_gamma)(int bl_level, enum gamma_mode_list gamma_mode);
	int (*set_acl)(int bl_level);
	int (*set_elvss)(int bl_level);
#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
	int (*prepare_brightness_control_cmd_array)(int lcd_type, int bl_level);
	struct cmd_set combined_ctrl;
#endif
	struct mipi_samsung_driver_data *msd;
	struct workqueue_struct *esd_workqueue;
	struct delayed_work esd_work;
	struct wake_lock esd_wake_lock;
};
#endif

struct display_status {
	unsigned char acl_on;
	unsigned char gamma_mode; /* 1: 1.9 gamma, 0: 2.2 gamma */
	unsigned char is_smart_dim_loaded;
	unsigned char is_elvss_loaded;
	unsigned char auto_brightness;
};
struct mipi_samsung_driver_data {
	struct dsi_buf samsung_tx_buf;
	struct dsi_buf samsung_rx_buf;
	struct msm_panel_common_pdata *mipi_samsung_disp_pdata;
	struct mipi_panel_data *mpd;
	struct display_status dstat;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_LCD_CLASS_DEVICE)
	struct platform_device *msm_pdev;
#endif
};

struct dsi_cmd_desc_LCD {
	int lux;
	char strID[8];
	struct dsi_cmd_desc *cmd;
};
int mipi_samsung_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel,
					struct mipi_panel_data *mpd);

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_CMD_QHD_PT)\
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
void reset_gamma_level(void);
unsigned char bypass_LCD_Id(void);
#endif

extern struct mutex dsi_tx_mutex;
#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_QHD_PT)
int get_lcd_attached(void);
#endif
unsigned char get_auto_brightness(void);
char* get_b5_reg(void);
char get_b5_reg_19(void);
char* get_b6_reg(void);
char get_b6_reg_17(void);

#if defined (CONFIG_FB_MSM_MIPI_SAMSUNG_OLED_VIDEO_WVGA_PT)
extern int poweroff_charging;
#endif

#endif  /* MIPI_SAMSUNG_OLED_H */
