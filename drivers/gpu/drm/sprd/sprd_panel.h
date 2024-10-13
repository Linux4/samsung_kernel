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
	CMD_OLED_BRIGHTNESS,
	CMD_OLED_REG_LOCK,
	CMD_OLED_REG_UNLOCK,
	CMD_CODE_DOZE_IN,
	CMD_CODE_DOZE_OUT,
	CMD_CODE_PRE_WRITE,
	CMD_CODE_RESERVED2,
	CMD_CODE_RESERVED3,
	CMD_CODE_RESERVED4,
	CMD_CODE_RESERVED5,
	CMD_CODE_CABC_UI,
	CMD_CODE_CABC_STILL,
	CMD_CODE_CABC_MOV,
	CMD_CODE_CABC_OFF,
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
	ESD_MODE_MIX_CHECK,
};
#define PANEL_CABC_NONE  0
#define PANEL_CABC_OFF   0
#define PANEL_CABC_UI    (1<<0)
#define PANEL_CABC_STILL (1<<1)
#define PANEL_CABC_MOV   (1<<2)

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

	/* esd check parameters*/
	struct panel_esd_config esd_conf;
	bool bta_en;
	u32 bta_check_reg;

	/* MIPI DSI specific parameters */
	u32 format;
	u32 lanes;
	u32 mode_flags;
	bool use_dcs;

	/* delay time between set lcd avdd and avee */
	u32 power_gpio_delay;
	/* CABC setting */
	u8 cabc_valid;
	u8 cabc_mode;
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
	struct delayed_work esd_work_new;
	bool esd_work_pending;
	bool esd_work_new_pending;
	bool esd_work_backup;
	bool is_enabled;
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
int sprd_panel_send_cmds(struct mipi_dsi_device *dsi,
				const void *data, int size);
#endif
