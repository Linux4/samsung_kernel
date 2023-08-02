/* inclue/linux/platform_data/gen-panel-mdnie.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 * Header file for Samsung Display Panel(LCD) mDNIe driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef	__GEN_PANEL_MDNIE_H__
#define __GEN_PANEL_MDNIE_H__
#include <linux/mutex.h>
#define NUMBER_OF_SCR_DATA	9

/* Split 16 bit as 8bit x 2 */
#define BIT(nr)			(1UL << (nr))
#define GET_MSB_8BIT(x)     ((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)     ((x >> 0) & (BIT(8) - 1))

struct mdnie_lite;

enum MDNIE_SCENARIO {
	MDNIE_UI_MODE		= 0,
	MDNIE_VIDEO_MODE	= 1,
	MDNIE_VIDEO_WARM_MODE	= 2,
	MDNIE_VIDEO_COLD_MODE	= 3,
	MDNIE_CAMERA_MODE	= 4,
	MDNIE_NAVI_MODE		= 5,
	MDNIE_GALLERY_MODE	= 6,
	MDNIE_VT_MODE		= 7,
	MDNIE_BROWSER_MODE	= 8,
	MDNIE_EBOOK_MODE	= 9,
	MDNIE_EMAIL_MODE	= 10,
	MDNIE_OUTDOOR_MODE	= 11,
	MDNIE_SCENARIO_MAX,
};

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	ACCESSIBILITY_MAX
};

static const char * const cabc_modes[] = {
	"off", "ui", "still", "movie"
};

enum CABC_MODE {
	CABC_OFF,
	CABC_UI,
	CABC_STILL,
	CABC_MOVIE,
	MAX_CABC
};


struct mdnie_config {
	bool tuning;
	bool negative;
	int accessibility;
	int scenario;
	int auto_brightness;
	int cabc;
};

struct mdnie_ops {
	int (*set_mdnie)(struct mdnie_config *);
	int (*get_mdnie)(struct mdnie_config *);
	int (*set_color_adj)(struct mdnie_config *);
	int (*set_cabc)(struct mdnie_config *);
};

struct mdnie_lite {
	struct device *dev;
	struct class *class;
	unsigned char cmd_reg;
	unsigned int cmd_len;
	struct mdnie_config config;
	struct mutex ops_lock;
	const struct mdnie_ops *ops;
	unsigned int scr[NUMBER_OF_SCR_DATA];
};
#ifdef CONFIG_GEN_PANEL_MDNIE
extern int gen_panel_attach_mdnie(struct mdnie_lite *,
		const struct mdnie_ops *);
extern void gen_panel_detach_mdnie(struct mdnie_lite *mdnie);
extern void update_mdnie_mode(struct mdnie_lite *mdnie);
extern void update_cabc_mode(struct mdnie_lite *mdnie);
#else
static inline int gen_panel_attach_mdnie(struct mdnie_lite *mdnie,
		const struct mdnie_ops *ops) { return 0; }
static inline void gen_panel_detach_mdnie(struct mdnie_lite *mdnie) {}
static inline void update_mdnie_mode(struct mdnie_lite *mdnie) {}
static inline void update_cabc_mode(struct mdnie_lite *mdnie) {}
#endif
#endif /* __GEN_PANEL_MDNIE_H__ */
