/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef _SPRDFB_H_
#define _SPRDFB_H_

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#ifdef CONFIG_FB_MMAP_CACHED
#include <linux/mm_types.h>
#endif
#ifdef CONFIG_LCD_ESD_RECOVERY
#include "esd_detect.h"
#endif
/*#define FB_CHECK_ESD_BY_TE_SUPPORT*/
#define BIT_PER_PIXEL_SURPPORT
#define PHY_SET_CLASS		"phy"
#define PHY_SET_DEV		"phy_dev"
#define PHY_SET_BUF_MAX_SIZE	0x2FF
#define ECHO_MIPI_FP

enum {
	SPRDFB_PANEL_IF_DBI = 0,
	SPRDFB_PANEL_IF_DPI,
	SPRDFB_PANEL_IF_EDPI,
	SPRDFB_PANEL_IF_LIMIT
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

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
#define SPRD_LAYER_IMG (0x01)	/*support YUV & RGB*/
#define SPRD_LAYER_OSD (0x02)	/*support RGB only*/

enum {
	SPRD_DATA_TYPE_YUV422 = 0,
	SPRD_DATA_TYPE_YUV420,
	SPRD_DATA_TYPE_YUV400,
	SPRD_DATA_TYPE_RGB888,
	SPRD_DATA_TYPE_RGB666,
	SPRD_DATA_TYPE_RGB565,
	SPRD_DATA_TYPE_RGB555,
	SPRD_DATA_TYPE_LIMIT
};

enum {
	SPRD_IMG_DATA_ENDIAN_B0B1B2B3 = 0,
	SPRD_IMG_DATA_ENDIAN_B3B2B1B0,
	SPRD_IMG_DATA_ENDIAN_B2B3B0B1,
	SPRD_IMG_DATA_ENDIAN_B1B0B3B2,
	SPRD_IMG_DATA_ENDIAN_LIMIT
};

enum {
	SPRD_OVERLAY_STATUS_OFF = 0,
	SPRD_OVERLAY_STATUS_ON,
	SPRD_OVERLAY_STATUS_STARTED,
	SPRD_OVERLAY_STATUS_MAX
};

enum {
	SPRD_OVERLAY_DISPLAY_ASYNC = 0,
	SPRD_OVERLAY_DISPLAY_SYNC,
	SPRD_OVERLAY_DISPLAY_MAX
};

typedef struct overlay_rect {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} overlay_rect;

typedef struct overlay_info {
	int	layer_index;
	int	data_type;
	int	y_endian;
	int	uv_endian;
	bool	rb_switch;
	overlay_rect rect;
	unsigned char *buffer;
} overlay_info;

typedef struct overlay_display {
	int layer_index;
	overlay_rect rect;
	int display_mode;
} overlay_display;
#endif

struct sprdfb_device {
	struct fb_info	*fb;

	uint16_t	enable;
	uint16_t	dev_id;		/*main_lcd, sub_lcd*/

	uint32_t	bpp;		/*input bit per pixel*/
	uint32_t	framebuffer_nr;	/*number of framebuffer*/

	uint16_t	panel_ready;	/*panel has been inited by uboot*/
	uint16_t	panel_if_type;	/*panel IF*/

	union {
		uint32_t	mcu_timing[MCU_LCD_TIMING_KIND_MAX];
		uint32_t	rgb_timing[RGB_LCD_TIMING_KIND_MAX];
	} panel_timing;

	struct panel_spec	*panel;
	struct display_ctrl	*ctrl;

	uint32_t		dpi_clock;
	uint32_t		curr_mipi_clk;

	struct semaphore	refresh_lock;

#ifdef CONFIG_FB_ESD_SUPPORT
	struct delayed_work	ESD_work;

	uint32_t		ESD_timeout_val;
	bool			ESD_work_start;
	/*for debug only*/
	uint32_t		check_esd_time;
	uint32_t		panel_reset_time;
	uint32_t		reset_dsi_time;
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	uint32_t		esd_te_waiter;
	wait_queue_head_t	esd_te_queue;
	uint32_t		esd_te_done;
#endif
#endif
	uint32_t		logo_buffer_addr_v;
	uint32_t		logo_buffer_size;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif

#ifdef ECHO_MIPI_FP
	struct class	*phy_set_class;
	struct device	*phy_set_dev;
#endif
};

struct display_ctrl {
	const char	*name;

	int32_t	(*early_init)	(struct sprdfb_device *dev);
	int32_t	(*init)		(struct sprdfb_device *dev);
	int32_t	(*uninit)	(struct sprdfb_device *dev);

	int32_t (*refresh)	(struct sprdfb_device *dev);
	int32_t (*logo_proc)	(struct sprdfb_device *dev);

	int32_t	(*suspend)	(struct sprdfb_device *dev);
	int32_t (*resume)	(struct sprdfb_device *dev);
	void	(*update_clk)	(struct sprdfb_device *dev);

#ifdef CONFIG_FB_ESD_SUPPORT
	int32_t	(*ESD_check)	(struct sprdfb_device *dev);
#endif
#ifdef CONFIG_LCD_ESD_RECOVERY
	int32_t	(*ESD_reset)	(struct sprdfb_device *dev);
#endif

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	int32_t	(*enable_overlay) (struct sprdfb_device *dev, \
					struct overlay_info* info, int enable);
	int32_t	(*display_overlay) (struct sprdfb_device *dev, \
					struct overlay_display* setting);
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	int32_t (*wait_for_vsync) (struct sprdfb_device *dev);
#endif

#ifdef  CONFIG_FB_DYNAMIC_FPS_SUPPORT
	int32_t (*change_fps) (struct sprdfb_device *dev, int fps_level);
#endif

#ifdef CONFIG_FB_MMAP_CACHED
	void (*set_vma)	(struct vm_area_struct *vma);
#endif
	int32_t	(*is_refresh_done) (struct sprdfb_device *dev);

};
#endif
