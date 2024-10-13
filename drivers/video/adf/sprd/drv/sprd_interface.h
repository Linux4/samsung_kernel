/*
 *Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "sprd_display.h"

#include "dsi_1_21a/mipi_dsih_local.h"
#include "dsi_1_21a/mipi_dsih_dphy.h"
#include "dsi_1_21a/mipi_dsih_hal.h"
#include "dsi_1_21a/mipi_dsih_api.h"

#define FB_DSIH_VERSION_1P21A

#define SPRDFB_UNDEFINELCD_ID -1

#define SPRDFB_I2C_WRITE_DELAY 0xffff
enum {
	SPRDFB_MAINLCD_ID = 0,
	SPRDFB_SUBLCD_ID,
	SPRDFB_MAX_LCD_ID,
};

/* LCD mode */
#define LCD_MODE_MCU			0
#define LCD_MODE_RGB			1
#define LCD_MODE_DSI			2
#define LCD_MODE_LVDS			3

/* LCD supported FPS */
#define LCD_MAX_FPS 70
#define LCD_MIN_FPS 30

/* LCD suspend mode */

/* SEND_SLEEP_CMD refer to send enter sleep cmd to panel before reset panel,
ssd panel need hs clk before enter sleep or reset panel */
#define SEND_SLEEP_CMD		1

/* bus mode */
#define LCD_BUS_8080			0
#define LCD_BUS_6800			1
#define LCD_BUS_SPI			2

/* lcd directions */
#define LCD_DIRECT_NORMAL		0
#define LCD_DIRECT_ROT_90		1
#define LCD_DIRECT_ROT_180		2
#define LCD_DIRECT_ROT_270		3
#define LCD_DIRECT_MIR_H		4
#define LCD_DIRECT_MIR_V		5
#define LCD_DIRECT_MIR_HV		6

/* panel property */
/* lcdc refresh, TE on, double timing, partial update*/
#define PANEL_CAP_NORMAL               0x0

/* only not support partial update*/
#define PANEL_CAP_NOT_PARTIAL_UPDATE   0x1

/* write register, grame have the same timing */
#define PANEL_CAP_UNIQUE_TIMING        0x2

/* lcd not support TE */
#define PANEL_CAP_NOT_TEAR_SYNC        0x4

/* only do command/data register, such as some mono oled display device */
#define PANEL_CAP_MANUAL_REFRESH       0x8

enum {
	SPRDFB_PANEL_TYPE_MCU = 0,
	SPRDFB_PANEL_TYPE_RGB,
	SPRDFB_PANEL_TYPE_MIPI,
	SPRDFB_PANEL_TYPE_LVDS,
	SPRDFB_PANEL_TYPE_LIMIT
};

enum {
	SPRDFB_POLARITY_POS = 0,
	SPRDFB_POLARITY_NEG,
	SPRDFB_POLARITY_LIMIT
};

enum {
	SPRDFB_RGB_BUS_TYPE_I2C = 0,
	SPRDFB_RGB_BUS_TYPE_SPI,
	SPRDFB_RGB_BUS_TYPE_LVDS,
	SPRDFB_RGB_BUG_TYPE_LIMIT
};

enum {
	SPRDFB_MIPI_MODE_CMD = 0,
	SPRDFB_MIPI_MODE_VIDEO,
	SPRDFB_MIPI_MODE_LIMIT
};
enum {
	MIPI_PLL_TYPE_NONE = 0,
	MIPI_PLL_TYPE_1,
	MIPI_PLL_TYPE_2,
	MIPI_PLL_TYPE_LIMIT
};
enum {
	MCU_LCD_REGISTER_TIMING = 0,
	MCU_LCD_GRAM_TIMING,
	MCU_LCD_TIMING_KIND_MAX
};

enum {
	RGB_LCD_H_TIMING = 0,
	RGB_LCD_V_TIMING,
	RGB_LCD_TIMING_KIND_MAX
};

struct panel_spec;
struct sprd_dsi_context {
	struct clk *clk_dsi;
	bool is_inited;
	uint32_t status;	/*0- normal, 1- uninit, 2-abnormal */
	struct panel_if_device *intf;
	struct device_node *dsi_node;
	void (*dsi_irq_trick) (struct panel_if_device *intf);

	dsih_ctrl_t dsi_inst;
};
typedef int32_t(*send_cmd_t) (struct sprd_dsi_context *dsi_ctx,
			      uint32_t data);
typedef int32_t(*send_data_t) (struct sprd_dsi_context *dsi_ctx,
			       uint32_t data);
typedef int32_t(*send_cmd_data_t) (struct sprd_dsi_context *dsi_ctx,
				   uint32_t cmd, uint32_t data);
typedef uint32_t(*read_data_t) (struct sprd_dsi_context *dsi_ctx);
typedef int32_t(*mipi_set_cmd_mode_t) (struct sprd_dsi_context *dsi_ctx);
typedef int32_t(*mipi_set_video_mode_t) (struct sprd_dsi_context *dsi_ctx);
typedef int32_t(*mipi_set_lp_mode_t) (struct sprd_dsi_context *dsi_ctx);
typedef int32_t(*mipi_set_hs_mode_t) (struct sprd_dsi_context *dsi_ctx);
typedef int32_t(*mipi_gen_write_t) (struct sprd_dsi_context *dsi_ctx,
				    uint8_t *param, uint16_t param_length);
typedef int32_t(*mipi_gen_read_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t *param, uint16_t param_length,
				uint8_t bytes_to_read,
				uint8_t *read_buffer);
typedef int32_t(*mipi_dcs_write_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t *param, uint16_t param_length);
typedef int32_t(*mipi_dcs_read_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t command, uint8_t bytes_to_read,
				uint8_t *read_buffer);
typedef int32_t(*mipi_force_write_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t data_type, uint8_t *param,
				uint16_t param_length);
typedef int32_t(*mipi_force_read_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t command, uint8_t bytes_to_read,
				uint8_t *read_buffer);
typedef int32_t(*mipi_eotp_set_t) (struct sprd_dsi_context *dsi_ctx,
				uint8_t rx_en, uint8_t tx_en);

typedef int32_t(*i2c_write_8bits_t) (uint8_t, uint8_t);
typedef int32_t(*i2c_read_8bits_t) (uint8_t, uint8_t *);
typedef int32_t(*i2c_write_16bits_t) (uint16_t, bool, uint16_t, bool);
typedef int32_t(*i2c_read_16bits_t) (uint16_t, bool, uint16_t *, bool);
typedef int32_t(*i2c_write_burst_t) (uint8_t *, int);

typedef void (*spi_send_cmd_t) (uint32_t cmd);
typedef void (*spi_send_data_t) (uint32_t data);
typedef void (*spi_read_t) (uint32_t *data);

/* LCD operations */
struct panel_operations {
	int32_t(*panel_init) (struct panel_if_device *intf);
	int32_t(*panel_close) (struct panel_if_device *intf);
	int32_t(*panel_reset) (struct panel_if_device *intf);
	int32_t(*panel_enter_sleep) (struct panel_if_device *intf,
				     uint8_t is_sleep);
	int32_t(*panel_after_suspend) (struct panel_if_device *intf);
	int32_t(*panel_before_resume) (struct panel_if_device *intf);
	int32_t(*panel_set_contrast) (struct panel_if_device *intf,
				      uint16_t contrast);
	int32_t(*panel_set_brightness) (struct panel_if_device *intf,
					uint16_t brightness);
	int32_t(*panel_set_window) (struct panel_if_device *intf,
				    uint16_t left, uint16_t top, uint16_t right,
				    uint16_t bottom);
	int32_t(*panel_invalidate) (struct panel_if_device *intf);
	int32_t(*panel_invalidate_rect) (struct panel_if_device *intf,
					 uint16_t left, uint16_t top,
					 uint16_t right, uint16_t bottom);
	int32_t(*panel_rotate_invalidate_rect) (struct panel_if_device *intf,
						uint16_t left, uint16_t top,
						uint16_t right, uint16_t bottom,
						uint16_t angle);
	int32_t(*panel_set_direction) (struct panel_if_device *intf,
				       uint16_t direction);
	uint32_t(*panel_readid) (struct panel_if_device *intf);
	int32_t(*panel_esd_check) (struct panel_if_device *intf);
	int32_t(*panel_change_fps) (struct panel_if_device *intf,
				    int fps_level);
	int32_t(*panel_change_epf) (struct panel_if_device *intf,
				    bool is_default);
};

/* MCU LCD specific properties */
struct timing_mcu {
	uint16_t rcss;		/*unit: ns */
	uint16_t rlpw;
	uint16_t rhpw;
	uint16_t wcss;
	uint16_t wlpw;
	uint16_t whpw;
};

/* RGB LCD specific properties */
struct timing_rgb {
	uint16_t hfp;		/*unit: pixel */
	uint16_t hbp;
	uint16_t hsync;
	uint16_t vfp;		/*unit: line */
	uint16_t vbp;
	uint16_t vsync;
};

struct ops_mcu {
	int32_t(*send_cmd) (uint32_t cmd);
	int32_t(*send_cmd_data) (uint32_t cmd, uint32_t data);
	int32_t(*send_data) (uint32_t data);
	uint32_t(*read_data) (void);
};

struct ops_i2c {
	int32_t(*i2c_write_8bits) (uint8_t reg, uint8_t val);
	int32_t(*i2c_read_8bits) (uint8_t reg, uint8_t *val);
	int32_t(*i2c_write_16bits) (uint16_t reg, bool reg_is_8bit,
				    uint16_t val, bool val_is_8bit);
	int32_t(*i2c_read_16bits) (uint16_t reg, bool reg_is_8bit,
				   uint16_t *val, bool val_is_8bit);
	int32_t(*i2c_write_burst) (uint8_t *buf, int num);
};

struct ops_spi {
	void (*spi_send_cmd) (uint32_t cmd);
	void (*spi_send_data) (uint32_t data);
	void (*spi_read) (uint32_t *data);
};


struct i2c_info {
	uint32_t i2c_addr;
	struct ops_i2c *ops;
};

struct spi_info {
	struct ops_spi *ops;
};

struct dsi_controller_info {
	unsigned long address;
	int dev_id;
};
struct dsi_cmd_header {
	char data_type;
	char wait;
	short len;
} __packed;
struct dsi_cmd_desc {
	struct dsi_cmd_header header;
	char *payload;
};
struct panel_cmds {
	struct dsi_cmd_desc *cmds;
	unsigned int cmd_total;
	unsigned char *cmd_base;
	unsigned int byte_len;
};
struct ops_mipi {
	int32_t(*mipi_set_cmd_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_set_video_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_set_lp_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_set_hs_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_set_data_lp_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_set_data_hs_mode) (struct sprd_dsi_context *dsi_ctx);
	int32_t(*mipi_gen_write) (struct sprd_dsi_context *dsi_ctx,
				uint8_t *param, uint16_t param_length);
	int32_t(*mipi_gen_read) (struct sprd_dsi_context *dsi_ctx,
				uint8_t *param, uint16_t param_length,
				uint8_t bytes_to_read, uint8_t *read_buffer);
	int32_t(*mipi_dcs_write) (struct sprd_dsi_context *dsi_ctx,
			uint8_t *param, uint16_t param_length);
	int32_t(*mipi_dcs_read) (struct sprd_dsi_context *dsi_ctx,
				uint8_t command, uint8_t bytes_to_read,
				uint8_t *read_buffer);
	int32_t(*mipi_force_write) (struct sprd_dsi_context *dsi_ctx,
				uint8_t data_type, uint8_t *param,
				uint16_t param_length);
	int32_t(*mipi_force_read) (struct sprd_dsi_context *dsi_ctx,
				uint8_t command, uint8_t bytes_to_read,
				uint8_t *read_buffer);
	int32_t(*mipi_eotp_set) (struct sprd_dsi_context *dsi_ctx,
				uint8_t rx_en, uint8_t tx_en);
};

struct info_mipi {
	struct sprd_dsi_context *dsi_ctx;
	struct ops_mipi *ops;
	struct panel_spec *panel;

};

struct info_rgb {
	uint16_t cmd_bus_mode;	/*spi, i2c */
	struct timing_rgb *timing;
	union {
		struct i2c_info *i2c;
		struct spi_info *spi;
	} bus_info;
	struct panel_spec *panel;
};

struct info_mcu {
	uint16_t bus_mode;	/*8080, 6800 */
	uint16_t bus_width;
	uint16_t bpp;
	uint16_t te_pol;
	uint32_t te_sync_delay;
	struct timing_mcu *timing;
	struct ops_mcu *ops;
	struct panel_spec *panel;
};

struct panel_if_ctrl {
	const char *if_name;
	int32_t(*panel_if_chg_freq) (struct panel_if_device *intf,
		int new_val);
	int32_t(*panel_if_esd)(struct panel_if_device *intf);
	/*check if panel can attach its interface */
	int32_t(*panel_if_attach)(struct panel_spec *panel,
		struct panel_if_device *intf);
	int32_t(*panel_if_check) (struct panel_spec *panel);
	void (*panel_if_mount) (struct panel_if_device *intf);
	bool(*panel_if_init) (struct panel_if_device *intf);
	void (*panel_if_ready) (struct panel_if_device *intf);
	void (*panel_if_uninit) (struct panel_if_device *intf);
	void (*panel_if_before_refresh) (struct panel_if_device *intf);
	void (*panel_if_after_refresh) (struct panel_if_device *intf);
	void (*panel_if_before_panel_reset) (struct panel_if_device *intf);
	void (*panel_if_enter_ulps) (struct panel_if_device *intf);
	void (*panel_if_suspend) (struct panel_if_device *intf);
	void (*panel_if_resume) (struct panel_if_device *intf);
	uint32_t(*panel_if_get_status) (struct panel_if_device *intf);
};

struct reset_timing_s {
	uint16_t time1;		/*high ms*/
	uint16_t time2;		/*low ms*/
	uint16_t time3;		/*high ms*/
};

/* LCD abstraction */
struct panel_spec {
	uint32_t dev_id;	/*main_lcd, sub_lcd, undefined */
	uint32_t lcd_id;
	uint32_t type;
	const char *lcd_name;
	uint16_t work_mode;	/*command_mode, video_mode */
	uint16_t video_bus_width;
	uint16_t suspend_mode;
	uint16_t h_sync_pol;
	uint16_t v_sync_pol;
	uint16_t de_pol;
	uint16_t te_pol;	/*only for command_mode */
	uint16_t color_mode_pol;
	uint16_t shut_down_pol;
	uint16_t direction;
	uint32_t lan_number;
	uint32_t phy_feq;	/*unit:Hz */
	uint32_t cap;
	uint32_t fps;
	uint16_t width;
	uint16_t height;
	uint16_t phy_width;
	uint16_t phy_height;
	uint32_t esd_return_code;
	struct panel_cmds *lcd_cmds;
	struct panel_cmds *esd_cmds;
	struct panel_cmds *sleep_cmds;
	struct panel_cmds *read_id_cmds;
	struct panel_if_device *parent;
	struct reset_timing_s reset_timing;
	union {
		struct timing_rgb rgb_timing;
		struct timing_mcu mcu_timing;
	} timing;
	struct panel_operations *ops;
	bool is_clean_lcd;
};

struct panel_if_device {
	struct device *of_dev;
	struct panel_if_ctrl *ctrl;
	struct panel_spec *(*get_panel)(struct panel_if_device *intf);
	int dev_id;
	int panel_ready;	/* for dsi init */
	int type;		/*mcu, rgb, mipi */
	int panel_if_type;	/*SPRDFB_PANEL_IF_DPI / DBI  */
	union {
		uint32_t mcu_timing[MCU_LCD_TIMING_KIND_MAX];
		uint32_t rgb_timing[RGB_LCD_TIMING_KIND_MAX];
	} panel_timing;
	union {
		struct info_mipi *mipi;
		struct info_rgb *rgb;
		struct info_mcu *mcu;
	} info;
	struct delayed_work esd_work;
	int esd_timeout;
	bool need_do_esd;
	uint32_t pclk_src;
	uint32_t dpi_clock;

};

#define INTERFACE_TO_DSI_CONTEXT(p)  ((p)->info.mipi->dsi_ctx)
#define RGB_CALC_H_TIMING(timing) ((timing).hsync|((timing).hbp<<8) \
						|((timing).hfp<<20))
#define RGB_CALC_V_TIMING(timing) ((timing).vsync|((timing).vbp<<8) \
						|((timing).vfp<<20))

extern struct panel_if_ctrl sprd_mcu_ctrl;
extern struct panel_if_ctrl sprd_rgb_ctrl;
extern struct panel_if_ctrl sprd_mipi_ctrl;
extern struct ops_mipi sprd_mipi_ops;
extern struct panel_operations lcd_mipi_ops;

extern int32_t sprd_dsi_init(struct panel_if_device *intf);
extern int32_t sprd_dsi_uninit(struct sprd_dsi_context *dsi_ctx);
extern int32_t sprd_dsi_ready(struct sprd_dsi_context *dsi_ctx);
extern int32_t sprd_dsi_suspend(struct sprd_dsi_context *dsi_ctx);
extern int32_t sprd_dsi_resume(struct sprd_dsi_context *dsi_ctx);
extern int32_t sprd_dsi_before_panel_reset(struct sprd_dsi_context
					     *dsi_ctx);
extern uint32_t sprd_dsi_get_status(struct sprd_dsi_context *dsi_ctx);
extern int32_t sprd_dsi_enter_ulps(struct sprd_dsi_context *dsi_ctx);
extern int sprd_dsi_chg_dphy_freq(struct panel_if_device *intf,
				    u32 dphy_freq);
int dc_connect_interface(struct sprd_device *dev, int index);
#endif
