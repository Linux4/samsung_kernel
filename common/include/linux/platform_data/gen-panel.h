/* inclue/linux/platform_data/gen-panel.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 * Header file for Samsung Display Panel(LCD) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _GEN_PANEL_GENERIC_H
#define _GEN_PANEL_GENERIC_H
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/platform_data/gen-panel-mdnie.h>

#define MAX_OLED_BRT	(255)
struct lcd;
struct oled_brt_map {
	unsigned int candela:12;
	unsigned int aid:12;
	unsigned int elvss:8;
};

struct candela_map {
	unsigned int *candela;
	unsigned int *value;
	size_t size;
};

enum {
	PANEL_INIT_CMD,
	PANEL_ENABLE_CMD,
	PANEL_DISABLE_CMD,
#ifdef CONFIG_GEN_PANEL_BACKLIGHT
	PANEL_BL_ON_CMD,
	PANEL_BL_SET_BRT_CMD,
#endif
	PANEL_NV_ENABLE_CMD,
	PANEL_NV_DISABLE_CMD,
	PANEL_HBM_ON,
	PANEL_HBM_OFF,
	PANEL_AID_CMD,
	PANEL_ELVSS_CMD,
	PANEL_GAMMA_CMD,
#ifdef CONFIG_GEN_PANEL_ACL
	PANEL_ACL_ON_CMD,
	PANEL_ACL_OFF_CMD,
#endif
	PANEL_CABC_ON_CMD,
	PANEL_CABC_OFF_CMD,
#ifdef CONFIG_GEN_PANEL_MDNIE
	PANEL_MDNIE_UI_CMD,
	PANEL_MDNIE_VIDEO_CMD,
	PANEL_MDNIE_CAMERA_CMD,
	PANEL_MDNIE_GALLERY_CMD,
	PANEL_MDNIE_NEGATIVE_CMD,
	PANEL_MDNIE_VT_CMD,
	PANEL_MDNIE_BROWSER_CMD,
	PANEL_MDNIE_EBOOK_CMD,
	PANEL_MDNIE_EMAIL_CMD,
	PANEL_MDNIE_COLOR_ADJ_CMD,
	PANEL_MDNIE_OUTDOOR_CMD,
#endif
	PANEL_OP_CMD_MAX,
};

static const char * const op_cmd_names[] = {
	"gen-panel-init-cmds",
	"gen-panel-enable-cmds",
	"gen-panel-disable-cmds",
#ifdef CONFIG_GEN_PANEL_BACKLIGHT
	"gen-panel-backlight-on-cmds",
	"gen-panel-backlight-set-brightness-cmds",
#endif
	"gen-panel-nv-read-enable-cmds",
	"gen-panel-nv-read-disable-cmds",
	"gen-panel-hbm-on-cmds",
	"gen-panel-hbm-off-cmds",
	"gen-panel-aid-cmds",
	"gen-panel-elvss-cmds",
	"gen-panel-gamma-cmds",
#ifdef CONFIG_GEN_PANEL_ACL
	"gen-panel-acl-on-cmds",
	"gen-panel-acl-off-cmds",
#endif
	"gen-panel-cabc-on-cmds",
	"gen-panel-cabc-off-cmds",
#ifdef CONFIG_GEN_PANEL_MDNIE
	"gen-panel-mdnie-ui-mode-cmds",
	"gen-panel-mdnie-video-mode-cmds",
	"gen-panel-mdnie-camera-mode-cmds",
	"gen-panel-mdnie-gallery-mode-cmds",
	"gen-panel-mdnie-negative-mode-cmds",
	"gen-panel-mdnie-vt-mode-cmds",
	"gen-panel-mdnie-browser-mode-cmds",
	"gen-panel-mdnie-ebook-mode-cmds",
	"gen-panel-mdnie-email-mode-cmds",
	"gen-panel-mdnie-color-adjustment-mode-cmds",
	"gen-panel-mdnie-outdoor-mode-cmds",
#endif
};

enum dsi_tx_mode {
	DSI_HS_MODE = 0,
	DSI_LP_MODE = 1,
};

static const char * const tx_modes[] = {
	"dsi-hs-mode", "dsi-lp-mode",
};

struct gen_cmd_hdr {
	u8 dtype;	/* data type */
	u8 txmode;	/* tx mode */
	u16 wait;	/* ms */
	u16 dlen;	/* data length */
};

struct gen_cmd_desc {
	u8 data_type;
	u8 lp;
	unsigned int delay;
	unsigned int length;
	u8 *data;
};

struct gen_cmds_info {
	char *name;
	struct gen_cmd_desc *desc;
	unsigned int nr_desc;
};

struct manipulate_table {
	struct list_head list;
	struct device_node *np;
	u32 type;
	struct gen_cmds_info cmd;
};

struct manipulate_action {
	struct list_head list;
	const char *name;
	struct manipulate_table **pmani;
	unsigned int nr_mani;
};

struct gen_dev_attr {
	struct list_head list;
	const char *attr_name;
	struct device_attribute dev_attr;
	struct list_head action_list;
	struct manipulate_action *action;
};

enum {
	EXT_PIN_OFF = 0,
	EXT_PIN_ON = 1,
	EXT_PIN_LOCK = 2,
};

enum {
	/* gpio controlled LDO based supply to LCD */
	EXT_PIN_GPIO = 0,
	/* PMIC Regulator based supply to LCD */
	EXT_PIN_REGULATOR = 1,
};

enum {
	TEMPERATURE_LOW = 0,
	TEMPERATURE_HIGH = 1,
};

struct extpin {
	struct list_head list;
	const char *name;
	u32 type;
	union {
		int gpio;
		struct regulator *supply;
	};
	struct mutex expires_lock;
	unsigned long expires;
};

struct extpin_ctrl {
	struct list_head list;
	struct extpin *pin;
	u32 on;
	u32 usec;
};

struct extpin_ctrl_list {
	unsigned int nr_ctrls;
	struct extpin_ctrl *ctrls;
};

struct temp_compensation {
	u8 *new_data;
	u8 *old_data;
	u32 data_len;
	u32 trig_type;
	int temperature;
};

struct read_info {
	u8 reg;
	u8 idx;
	u8 len;
} __attribute__((__packed__));

struct gen_panel_ops {
	int (*tx_cmds)(const struct lcd *, const void *, int);
	int (*rx_cmds)(const struct lcd *, u8 *, const void *, int);
#if CONFIG_OF
	int (*parse_dt)(const struct device_node *);
#endif
};

struct gen_panel_mode {
	const char *name;
	unsigned int refresh;
	unsigned int xres;
	unsigned int yres;
	unsigned int real_xres;
	unsigned int real_yres;
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
	unsigned int hsync_invert;
	unsigned int vsync_invert;
	unsigned int invert_pixclock;
	unsigned int pixclock_freq;
	int pix_fmt_out;
	u32 height; /* screen height in mm */
	u32 width; /* screen width in mm */
};

struct lcd {
	struct device *dev;
	struct class *class;
	void *pdata;
	struct backlight_device *bd;
	u32 (*set_panel_id)(struct lcd *lcd);
	const struct gen_panel_ops *ops;
	/* Further fields can be added here */
	struct mutex access_ok;
	const char *manufacturer_name;
	const char *panel_model_name;
	const char *panel_name;
	unsigned int id;
	unsigned int esd_type;
	int esd_gpio;
	unsigned rst_gpio_en:1;
	unsigned int rst_gpio;
	unsigned active:1;
	unsigned esd_en:1;
	unsigned temp_comp_en:1;
	unsigned power:1;
	unsigned acl:1;
	unsigned hbm:1;
	struct gen_panel_mode mode;
	/* mdnie lite */
	struct mdnie_lite mdnie;
	/* external pins */
	struct extpin_ctrl_list extpin_on_seq;
	struct extpin_ctrl_list extpin_off_seq;
	/* pin ctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_enable;
	struct pinctrl_state *pin_disable;
	/* command tables */
	struct gen_cmds_info *op_cmds;
	/* temperature compensation */
	struct temp_compensation *temp_comp;
	unsigned int nr_temp_comp;
	struct read_info id_rd_info[3];
	struct read_info mtp_rd_info[1];
	u32 set_brt_reg;
	u32 color_adj_reg;
	u32 status_reg;
	u32 status_ok;
};

/* status types */
enum {
	/* status of what object at */
	GEN_PANEL_OFF = 0,
	GEN_PANEL_ON,
	GEN_PANEL_ON_REDUCED,
	GEN_PANEL_OFF_DMA,
	GEN_PANEL_ON_DMA,
	GEN_PANEL_RESET,
};

static inline int gen_panel_is_on(int status)
{
	return (status == GEN_PANEL_ON ||
		status == GEN_PANEL_ON_REDUCED ||
		status == GEN_PANEL_ON_DMA);
}

static inline int gen_panel_is_off(int status)
{
	return (status == GEN_PANEL_OFF ||
		status == GEN_PANEL_OFF_DMA);
}

u32 get_panel_id(void);
int gen_dsi_panel_verify_reg(struct lcd *lcd,
		struct gen_cmd_desc desc[], int count);

extern bool lcd_connected(void);
#ifdef CONFIG_GEN_PANEL_TUNING
extern int gen_panel_attach_tuning(struct lcd *);
extern void gen_panel_detach_tuning(struct lcd *);
#else
static inline int gen_panel_attach_tuning(struct lcd *lcd) { return 0; }
static inline void gen_panel_detach_tuning(struct lcd *lcd) {}
#endif

extern int gen_panel_probe(struct device_node *, struct lcd *lcd);
extern int gen_panel_remove(struct lcd *);
extern void gen_panel_set_status(struct lcd *lcd, int status);
extern void gen_panel_start(struct lcd *lcd, int status);

#define panel_usleep(t)				\
do {						\
	unsigned int usec = (t);		\
	if (usec < 20 * 1000)			\
		usleep_range(usec, usec + 10);	\
	else					\
		msleep(usec / 1000);		\
} while (0)
#endif /* _GEN_PANEL_GENERIC_H */
