/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef __DSIM_H__
#define __DSIM_H__

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/workqueue.h>

#include <media/v4l2-subdev.h>
#include <media/media-entity.h>

#include "./panels/decon_lcd.h"
#include "regs-dsim.h"
#include "dsim_common.h"

#define DSIM_PAD_SINK		0
#define DSIM_PADS_NUM		1

#define DSIM_RX_FIFO_READ_DONE	(0x30800002)
#define DSIM_MAX_RX_FIFO	(64)

#define AID_INTERPOLATION

#define dsim_err(fmt, ...)					\
	do {							\
		pr_err(pr_fmt(fmt), ##__VA_ARGS__);		\
		exynos_ss_printk(fmt, ##__VA_ARGS__);		\
	} while (0)

#define dsim_info(fmt, ...)					\
	do {							\
		pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define dsim_dbg(fmt, ...)					\
	do {							\
		pr_debug(pr_fmt(fmt), ##__VA_ARGS__);		\
	} while (0)

#define call_panel_ops(q, op, args...)				\
	(((q)->panel_ops->op) ? ((q)->panel_ops->op(args)) : 0)

extern struct dsim_device *dsim0_for_decon;
extern struct dsim_device *dsim1_for_decon;

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED		1
#define PANEL_STATE_SUSPENDING	2

#define PANEL_DISCONNECTED		0
#define PANEL_CONNECTED			1

enum mipi_dsim_pktgo_state {
	DSIM_PKTGO_DISABLED,
	DSIM_PKTGO_ENABLED
};

/* operation state of dsim driver */
enum dsim_state {
	DSIM_STATE_HSCLKEN,	/* HS clock was enabled. */
	DSIM_STATE_ULPS,	/* DSIM was entered ULPS state */
	DSIM_STATE_SUSPEND	/* DSIM is suspend state */
};

#ifdef CONFIG_LCD_DOZE_MODE
enum dsim_doze_mode {
	DSIM_DOZE_STATE_NORMAL = 0,
	DSIM_DOZE_STATE_DOZE,
	DSIM_DOZE_STATE_SUSPEND,
	DSIM_DOZE_STATE_DOZE_SUSPEND,
};
#endif
struct dsim_resources {
	struct clk *pclk;
	struct clk *dphy_esc;
	struct clk *dphy_byte;
	struct clk *rgb_vclk0;
	struct clk *pclk_disp;
	int lcd_power[2];
	int lcd_reset;
	struct regulator *regulator_30V;
	struct regulator *regulator_18V;
	struct regulator *regulator_16V;
};

struct panel_private {

	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];
	unsigned char elvss_set[22];
	unsigned char tset[30];	// HA3 is 30
	unsigned char aid[16];
	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[7];
	unsigned int lcdConnected;
	unsigned int state;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int caps_enable;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_vint;
	unsigned int siop_enable;
#ifdef CONFIG_LCD_BURNIN_CORRECTION
	unsigned char ldu_correction_state;
#endif
	void *dim_data;
	void *dim_info;
	unsigned int *br_tbl;
	unsigned char *inter_aor_tbl;
	unsigned int *gallery_br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct mutex lock;
	struct dsim_panel_ops *ops;
	unsigned int panel_type;
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	unsigned int mdnie_support;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	unsigned int mcd_on;
#endif

#ifdef CONFIG_LCD_HMT
	unsigned int hmt_on;
	unsigned int hmt_br_index;
	unsigned int hmt_brightness;
	void *hmt_dim_data;
	void *hmt_dim_info;
	unsigned int *hmt_br_tbl;
#endif

#ifdef CONFIG_LCD_ALPM
	unsigned int 	alpm;
	unsigned int 	current_alpm;
	struct mutex	alpm_lock;
	unsigned char mtpForALPM[36];
	unsigned char prev_VT[2];
	unsigned char alpm_support;			// because zero2 use 2panel(ha2, hf3)
#endif

#ifdef CONFIG_LCD_DOZE_MODE
	unsigned int 	alpm_support;	// 0 : unsupport, 1 : 30hz, 2 : 1hz
	unsigned int	hlpm_support;	// 0 : unsupport, 1 : 30hz
	unsigned int alpm_mode;
	unsigned int curr_alpm_mode;
#endif
	unsigned int interpolation;
	unsigned char hbm_elvss;

	int is_br_override;
	int override_br_value;

	int esd_disable;

	unsigned int accessibility;
	unsigned int adaptive_control;
	struct class *mdnie_class;
	int lux;
#ifdef CONFIG_CHECK_OCTA_CHIP_ID
	unsigned char octa_id[25];
#endif
};

struct dsim_panel_ops {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*exit)(struct dsim_device *dsim);
	int	(*init)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
#ifdef CONFIG_LCD_DOZE_MODE
	int (*enteralpm)(struct dsim_device *dsim);
	int (*exitalpm)(struct dsim_device *dsim);
#endif
#ifdef CONFIG_FB_DSU
	int (*dsu_cmd)(struct dsim_device *dsim);
#endif
};

struct dsim_device {
	struct device *dev;
	void * decon;
	struct dsim_resources res;
	unsigned int irq;
	void __iomem *reg_base;

	enum dsim_state state;

	unsigned int data_lane;
	unsigned long hs_clk;
	unsigned long byte_clk;
	unsigned long escape_clk;
	unsigned char freq_band;
	struct notifier_block fb_notif;

	struct lcd_device	*lcd;
	unsigned int enabled;
	struct decon_lcd lcd_info;
	struct dphy_timing_value	timing;
	int	pktgo;
	int	glide_display_size;

	int id;
	u32 data_lane_cnt;
	struct mipi_dsim_lcd_driver *panel_ops;

	spinlock_t slock;
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct pinctrl *pinctrl;
	struct pinctrl_state *turnon_tes;
	struct pinctrl_state *turnoff_tes;
	struct mutex rdwr_lock;

	struct panel_private priv;

	struct dsim_clks_param clks_param;
#ifdef CONFIG_LCD_ALPM
	int 			alpm;
#endif
#ifdef CONFIG_LCD_DOZE_MODE
	unsigned int dsim_doze;
#endif
#ifdef CONFIG_FB_DSU
	int dsu_xres;
	int dsu_yres;
	struct workqueue_struct *dsu_sysfs_wq;
	struct delayed_work dsu_sysfs_work;
	unsigned int	dsu_param_offset;
	unsigned int	dsu_param_value;
#endif
};

/**
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 */

struct mipi_dsim_lcd_driver {
	int (*early_probe)(struct dsim_device *dsim);
	int	(*probe)(struct dsim_device *dsim);
	int	(*suspend)(struct dsim_device *dsim);
	int	(*displayon)(struct dsim_device *dsim);
	int	(*resume)(struct dsim_device *dsim);
	int (*dump)(struct dsim_device *dsim);
#ifdef CONFIG_LCD_DOZE_MODE
	int (*enteralpm)(struct dsim_device *dsim);
	int (*exitalpm)(struct dsim_device *dsim);
#endif
#ifdef CONFIG_FB_DSU
	int (*dsu_cmd)(struct dsim_device *dsim);
	int (*init)(struct dsim_device *dsim);
	int (*dsu_sysfs) (struct dsim_device *dsim);
#endif
};

int dsim_write_data(struct dsim_device *dsim, unsigned int data_id,
		unsigned long data0, unsigned int data1);
int dsim_read_data(struct dsim_device *dsim, u32 data_id, u32 addr,
		u32 count, u8 *buf);

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
void dsim_pkt_go_ready(struct dsim_device *dsim);
void dsim_pkt_go_enable(struct dsim_device *dsim, bool enable);
#endif

#if defined(CONFIG_LCD_ALPM) || defined(CONFIG_LCD_DOZE_MODE)
#define	ALPM_OFF							0
#define ALPM_ON							1
int alpm_set_mode(struct dsim_device *dsim, int enable);
#endif

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	if (id)
		return dsim1_for_decon;
	else
		return dsim0_for_decon;
}

static inline int dsim_wr_data(u32 id, u32 cmd_id, unsigned long d0, u32 d1)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_write_data(dsim, cmd_id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

/* register access subroutines */
static inline u32 dsim_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	return readl(dsim->reg_base + reg_id);
}

static inline u32 dsim_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dsim_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	writel(val, dsim->reg_base + reg_id);
}

static inline void dsim_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->reg_base + reg_id);
}

#define DSIM_IOC_ENTER_ULPS		_IOW('D', 0, u32)
#define DSIM_IOC_LCD_OFF		_IOW('D', 1, u32)
#define DSIM_IOC_PKT_GO_ENABLE		_IOW('D', 2, u32)
#define DSIM_IOC_PKT_GO_DISABLE		_IOW('D', 3, u32)
#define DSIM_IOC_PKT_GO_READY		_IOW('D', 4, u32)
#define DSIM_IOC_GET_LCD_INFO		_IOW('D', 5, struct decon_lcd *)
#define DSIM_IOC_PARTIAL_CMD		_IOW('D', 6, u32)
#define DSIM_IOC_SET_PORCH		_IOW('D', 7, struct decon_lcd *)
#define DSIM_IOC_DUMP			_IOW('D', 8, u32)

#ifdef CONFIG_FB_DSU
#define DSIM_IOC_DSU_CMD            _IOW('D', 12, u32)
//#define DSIM_IOC_DSU_DSC            _IOW('D', 13, u32)
#define DSIM_IOC_TE_ONOFF           _IOW('D', 14, u32)
#define DSIM_IOC_DSU_RECONFIG   _IOW('D', 15, u32)
#define DSIM_IOC_DISPLAY_ONOFF	    _IOW('D', 16, u32)
//#define DSIM_IOC_REG_LOCK	_IOW('D', 17, u32)
#endif

#define DSIM_REQ_POWER_OFF		0
#define DSIM_REQ_POWER_ON		1
#ifdef CONFIG_LCD_DOZE_MODE
#define DSIM_REQ_DOZE_MODE		2
#define DSIM_REQ_DOZE_SUSPEND 	3
#endif

u32 dsim_reg_get_lineval(u32 id);
u32 dsim_reg_get_hozval(u32 id);

int dsim_write_hl_data(struct dsim_device *dsim, const u8 *cmd, u32 cmdSize);
int dsim_read_hl_data(struct dsim_device *dsim, u8 addr, u32 size, u8 *buf);
#endif /* __DSIM_H__ */
