/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_PANEL_H_
#define _SPRD_PANEL_H_

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include "aw99703_bl.h"

enum {
	CMD_CODE_INIT = 0,
	CMD_CODE_SLEEP_IN,
	CMD_CODE_SLEEP_OUT,

	/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 start*/
	CMD_CODE_ESD_CHECK_ENABLE_CMD,
	CMD_CODE_ESD_CHECK_READ_MASTER_CMD,
	CMD_CODE_ESD_CHECK_READ_SLAVE_CMD,
	CMD_CODE_ESD_CHECK_RETURN_TO_NORMAL,
	/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 end*/
	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 start*/
	CMD_CODE_PWM_DIMING_OFF,
	/*Tab A8 code for SR-AX6300-01-1079 by hehaoran at 20211001 end*/
	CMD_OLED_BRIGHTNESS,
	/*Tab A8 code for SR-AX6300-01-32 by fengzhigang at 20210907 start*/
	CMD_CODE_DEEP_SLEEP_IN,
	/*Tab A8 code for SR-AX6300-01-32 by fengzhigang at 20210907 end*/
	CMD_OLED_REG_LOCK,
	CMD_OLED_REG_UNLOCK,
	CMD_CODE_DOZE_IN,
	CMD_CODE_DOZE_OUT,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_RESERVED4,
	CMD_CODE_RESERVED5,
	CMD_CODE_MAX,
};

enum {
	SPRD_DSI_MODE_CMD = 0,
	SPRD_DSI_MODE_VIDEO_BURST,
	SPRD_DSI_MODE_VIDEO_SYNC_PULSE,
	SPRD_DSI_MODE_VIDEO_SYNC_EVENT,
};

enum {
	ESD_MODE_REG_CHECK,
	ESD_MODE_TE_CHECK,
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	ESD_MODE_MIX_CHECK,
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
};

struct dsi_cmd_desc {
	u8 data_type;
	u8 wait;
	u8 wc_h;
	u8 wc_l;
	u8 payload[];
};

struct gpio_timing {
	u32 level;
	u32 delay;
};

struct reset_sequence {
	u32 items;
	struct gpio_timing *timing;
};

/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
struct panel_esd_config {
	bool esd_check_en;
	u8 esd_check_mode;
	u8 esd_check_reg_count;
	u16 esd_check_period;
	uint8_t *reg_seq;
	uint8_t *val_seq;
	uint8_t reg_items;
	uint8_t val_items;
	uint32_t *val_len_array;
	uint32_t total_esd_val_count;
};
/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

struct panel_info {
	/* common parameters */
	struct device_node *of_node;
	struct drm_display_mode mode;
	struct drm_display_mode *buildin_modes;
	int num_buildin_modes;
	struct gpio_desc *avdd_gpio;
	struct gpio_desc *avee_gpio;
	struct gpio_desc *reset_gpio;
	struct reset_sequence rst_on_seq;
	struct reset_sequence rst_off_seq;
	const void *cmds[CMD_CODE_MAX];
	int cmds_len[CMD_CODE_MAX];

	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	/* esd check parameters*/
	/*
	bool esd_check_en;
	u8 esd_check_mode;
	u16 esd_check_period;
	u32 esd_check_reg;
	u32 *esd_check_val;
	u32 esd_check_len;
	*/
	struct panel_esd_config esd_conf;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

	/* MIPI DSI specific parameters */
	u32 format;
	u32 lanes;
	u32 mode_flags;
	bool use_dcs;

	/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 start*/
	int esd_check_double_reg_mode;
	/*Tab A8 code for AX6300DEV-3466 by huangzhongjie at 20211126 end*/
	/*Tab A8 code for SR-AX6300-01-32 by fengzhigang at 20210907 start*/
	int deep_sleep_flag;
	/*Tab A8 code for SR-AX6300-01-32 by fengzhigang at 20210907 end*/
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 start*/
	int reset_force_pull_low;
	int reset_delay_vspn_ms;
	/*Tab A8 code for AX6300DEV-787 by fengzhigang at 20211020 end*/

	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 start */
	u32 power_vsp_out;
	/* HS03 code for SL6215TDEV-652 by gaoxue at 20221020 end */
	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 start*/
	u32 reset_low_before_power_delay;
	/*Tab A8 code for SR-AX6300-01-441 by huangzhongjie at 20211129 end*/
	/* delay time between set lcd avdd and avee */
	u32 power_gpio_delay;
	/* HS03 code for SR-SL6215-01-118 by LiChao at 20210830 start */
	u32 power_off_sequence;
	u32 ddi_need_tp_reset;
	u32 ddi_tp_reset_gpio;
	/* HS03 code for SR-SL6215-01-118 by LiChao at 20210830 end */
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 start */
	u32 power_vsp_on_delay;
	u32 power_vsn_on_delay;
	u32 power_vsp_off_delay;
	u32 power_vsn_off_delay;
	/* HS03 code for SL6215DEV-1777 by LiChao at 20210916 end */
};

struct sprd_panel {
	struct device dev;
	struct drm_panel base;
	struct mipi_dsi_device *slave;
	struct panel_info info;
	struct backlight_device *backlight;
	struct backlight_device *oled_bdev;
	struct regulator *supply;
	struct notifier_block panic_nb;
	struct delayed_work esd_work;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	bool esd_work_backup;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	bool esd_work_pending;
	bool is_enabled;
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 start*/
	bool esd_flag;
	/*Tab A8 code for AX6300DEV-3002 by huangzhongjie at 20211111 end*/
};

struct sprd_oled {
	struct backlight_device *bdev;
	struct sprd_panel *panel;
	struct dsi_cmd_desc *cmds[256];
	int cmd_len;
	int cmds_total;
	int max_level;
};

int sprd_panel_parse_lcddtb(struct device_node *lcd_node,
	struct sprd_panel *panel);
void  sprd_panel_enter_doze(struct drm_panel *p);
void  sprd_panel_exit_doze(struct drm_panel *p);

int sprd_oled_set_brightness(struct backlight_device *bdev);
/*Tab A8 code for AX6300DEV-1494 by luxinjun at 2021/11/03 start*/
/*Tab A8 code for AX6300DEV-805 by suyurui at 2021/10/9 start*/
#if defined (CONFIG_TARGET_UMS512_1H10) || defined (CONFIG_TARGET_UMS512_25C10)
extern enum tp_module_used tp_is_used;
extern void himax_resume_by_ddi(void);
extern void nvt_resume_for_earlier(void);
#endif
/*Tab A8 code for AX6300DEV-805 by suyurui at 2021/10/9 end*/
/*Tab A8 code for AX6300DEV-1494 by luxinjun at 2021/11/03 end*/
/*HS03 code for SR-SL6215-01-1148 by wenghailong at 20220421 start */
/*HS03 code for SL6215DEV-3850 by zhoulingyun at 20211216 start*/
#ifdef CONFIG_TARGET_UMS9230_4H10
extern enum tp_module_used tp_is_used;
extern void nvt_resume_for_earlier(void);
extern int gcore_tp_esd_fail;
#endif
/*HS03 code for SL6215DEV-3850 by zhoulingyun at 20211216 end*/
/*HS03 code for SR-SL6215-01-1148 by wenghailong at 20220421 end */
#endif
