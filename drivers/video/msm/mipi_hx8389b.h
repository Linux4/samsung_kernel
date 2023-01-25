/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include "mipi_dsi.h"
#include <linux/wakelock.h>

#if defined(CONFIG_MACH_CRATERTD_CHN_3G) || defined(CONFIG_MACH_BAFFINVETD_CHN_3G) || defined(CONFIG_MACH_CRATER_CHN_CTC)
#define CONFIG_BACKLIGHT_IC_KTD3102
#endif

#ifndef MIPI_HX8389B_H
#define MIPI_HX8389B_H

int mipi_hx8389b_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

#if defined(CONFIG_BACKLIGHT_IC_KTD3102)
#if defined(CONFIG_MACH_CRATER_CHN_CTC)
#define DISP_BL_CONT_GPIO 51
#define CONFIG_BL_CTRL_MODE_2
#else
#define DISP_BL_CONT_GPIO 63
#endif
#define MAX_BRIGHTNESS_IN_BLU	32

#if defined(CONFIG_BL_CTRL_MODE_2)
#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	31
#define AAT_DIMMING_VALUE	    31

struct brt_value {
	int level;			/* Platform setting values*/
	int tune_level;			/* Chip Setting values*/
};
#endif

extern struct mutex dsi_tx_mutex;

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

struct cmd_set {
	struct dsi_cmd_desc *cmd;
	int size;
};


struct mipi_panel_data {
	const char panel_name[20];
	struct cmd_set ready_to_on;
	struct cmd_set ready_to_on_t05;
	struct cmd_set ready_to_off;
	struct cmd_set on;
	struct cmd_set off;
	struct cmd_set late_on;
	struct cmd_set early_off;
	struct cmd_set mtp_read_enable;
	struct cmd_set tune;

	/*struct str_smart_dim smart;*/
	signed char lcd_current_cd_idx;
	unsigned int manufacture_id;

	int (*set_brightness_level)(int bl_level);
	struct mipi_samsung_driver_data *msd;
	struct workqueue_struct *esd_workqueue;
	struct delayed_work esd_work;
	struct wake_lock esd_wake_lock;
};


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
#endif

#endif  /* MIPI_HX8389B_H */
