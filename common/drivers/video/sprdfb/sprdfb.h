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
#include <linux/time.h>

/*#define FB_CHECK_ESD_BY_TE_SUPPORT*/
#define BIT_PER_PIXEL_SURPPORT

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

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
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

enum {
	SPRD_DISPLAY_UPDATE_NONE = 0,
	SPRD_DISPLAY_UPDATE_PAN,
	SPRD_DISPLAY_UPDATE_OVERLAY,
	SPRD_DISPLAY_UPDATE_MAX
};

typedef struct overlay_rect {
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} overlay_rect;

typedef struct overlay_info {
	int layer_index;
	int data_type;
	int y_endian;
	int uv_endian;
	bool rb_switch;
	overlay_rect rect;
	unsigned char *buffer;
} overlay_info;

typedef struct overlay_display {
	int layer_index;
	overlay_rect rect;
	int display_mode;
} overlay_display;
#endif

struct fps_info {
	u32 enable_dmesg;
	u32 interval_ms;
	struct timespec timestamp_last;
	u32 frame_counter_last;
	u32 frame_counter;
	u32 fpks;
	int fps_checking_mode;	/*pan or overlay*/
};

struct sprdfb_device {
	struct fb_info *fb;

	uint16_t enable;
	uint16_t dev_id;	/*main_lcd, sub_lcd*/

	uint32_t bpp;		/*input bit per pixel*/

	uint16_t panel_ready;	/*panel has been inited by uboot*/
	uint16_t panel_if_type;	/*panel IF*/

	union {
		uint32_t mcu_timing[MCU_LCD_TIMING_KIND_MAX];
		uint32_t rgb_timing[RGB_LCD_TIMING_KIND_MAX];
	} panel_timing;

	struct panel_spec *panel;
	struct display_ctrl *ctrl;

	uint32_t dpi_clock;
	struct semaphore refresh_lock;
	uint32_t logo_buffer_addr_v;
	uint32_t logo_buffer_size;
	struct fps_info fps;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	/*For Special features*/
#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
	uint32_t dyna_dpi_clk;
#endif
};

struct display_ctrl {
	const char *name;

	int32_t	(*early_init)(struct sprdfb_device *dev);
	int32_t	(*init)(struct sprdfb_device *dev);
	int32_t	(*uninit)(struct sprdfb_device *dev);

	int32_t (*refresh)(struct sprdfb_device *dev);
	int32_t (*logo_proc)(struct sprdfb_device *dev);

	int32_t	(*suspend)(struct sprdfb_device *dev);
	int32_t (*resume)(struct sprdfb_device *dev);
	void	(*update_clk)(struct sprdfb_device *dev);

#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
	int32_t (*change_pclk)(struct sprdfb_device *fb_dev, u32 new_pclk);
#endif
#ifdef CONFIG_LCD_ESD_RECOVERY
	int32_t	(*ESD_reset)(struct sprdfb_device *dev);
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	int32_t (*enable_overlay)(struct sprdfb_device *dev,
					struct overlay_info *info, int enable);
	int32_t	(*display_overlay)(struct sprdfb_device *dev,
					struct overlay_display *setting);
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	int32_t (*wait_for_vsync)(struct sprdfb_device *dev);
#endif

#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
	int32_t (*change_fps)(struct sprdfb_device *dev, int fps_level);
#endif

#ifdef CONFIG_FB_MMAP_CACHED
	void (*set_vma)(struct vm_area_struct *vma);
#endif

};

#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
int sprdfb_create_sysfs(struct sprdfb_device *fb_dev);
void sprdfb_remove_sysfs(struct sprdfb_device *fb_dev);
#endif

#ifdef CONFIG_FB_BL_EVENT_CTRL
#define FB_BL_EVENT_RESUME	0x20
#define FB_BL_EVENT_SUSPEND	0x21

extern int fb_bl_register_client(struct notifier_block *nb);
extern int fb_bl_unregister_client(struct notifier_block *nb);
extern int fb_bl_notifier_call_chain(unsigned long val, void *v);

struct fb_bl_event {
	struct sprdfb_device *info;
};
#endif

#endif
