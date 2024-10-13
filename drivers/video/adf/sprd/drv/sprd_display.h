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
#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sched.h>
#ifdef CONFIG_FB_MMAP_CACHED
#include <linux/mm_types.h>
#endif

#include "sprd_chip_common.h"
#include <linux/io.h>
#include "sprd_adf_common.h"

#if ((!defined(CONFIG_FB_SCX35)) && (!defined(CONFIG_FB_SCX15)))
#define FB_CHECK_ESD_IN_VFP
#endif

#define BIT_PER_PIXEL_SURPPORT

#define PRINT(fmt, args...) pr_debug("sprd-adf:[%20s] "fmt, __func__, ##args)
#define XDBG(x)  PRINT(#x" = 0x%x\n", x)
#define DBG(x)  PRINT(#x" = %d\n", x)
#define PDBG(x)  PRINT(#x" = %p\n", x)
#define PERROR(fmt, args...) pr_err("sprd-adf:[%20s] "fmt, __func__, ##args)

static inline uint32_t dispc_glb_read(unsigned long reg)
{
	return sci_glb_read(reg, 0xffffffff);
}

#if 1
#define ENTRY()
#define LEAVE()
#else
extern u64 g_time_start, g_time_end;
static inline u64 sprd_print_time(u64 ts, const char *s)
{
	unsigned long rem_nsec;
	rem_nsec = do_div(ts, 1000000000);
	pr_err_ratelimited("sprd-adf:%20s time: <%lu.%06lu>\n", s,
		       (unsigned long)ts, rem_nsec / 1000);
	return ts;
}

#define ENTRY()  (g_time_start = local_clock())
#define LEAVE() do { \
		u64 delta; \
		g_time_end = local_clock(); \
		delta = g_time_end - g_time_start; \
		sprd_print_time(delta, "###### get delta ######"); \
		} while (0)

#endif

enum {
	SPRDFB_PANEL_IF_DBI = 0,
	SPRDFB_PANEL_IF_DPI,
	SPRDFB_PANEL_IF_EDPI,
	SPRDFB_PANEL_IF_LIMIT
};

enum {
	SPRDFB_UNKNOWN = 0,
	SPRDFB_DYNAMIC_PCLK = 0x1,
	SPRDFB_DYNAMIC_FPS,
	SPRDFB_DYNAMIC_MIPI_CLK,
	SPRDFB_FORCE_FPS,
	SPRDFB_FORCE_PCLK,
};

enum {
	SPRD_DATA_TYPE_YUV422 = 0,
	SPRD_DATA_TYPE_YUV420,
	SPRD_DATA_TYPE_YUV400,
	SPRD_DATA_TYPE_RGB888,
	SPRD_DATA_TYPE_RGB666,
	SPRD_DATA_TYPE_RGB565,
	SPRD_DATA_TYPE_RGB555,
	SPRD_DATA_TYPE_YUV422_3P,
	SPRD_DATA_TYPE_YUV420_3P,
	SPRD_DATA_TYPE_RGB888_PACK,
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


struct dispc_base {
	unsigned long address;
	uint32_t index;
	int dc_mode;
};
struct sprd_dispc_context {
	struct dispc_base base;
	struct clk *clk_dispc;
	struct clk *clk_dispc_dpi;
	struct clk *clk_dispc_dbi;
	struct clk *clk_dispc_emc;
	bool is_inited;
	bool is_first_frame;
	bool clk_is_open;
	bool clk_is_refreshing;
	int clk_open_count;
	struct semaphore clk_sem;
	struct sprd_device *dev;

	uint32_t vsync_waiter;
	wait_queue_head_t vsync_queue;
	uint32_t vsync_done;
	struct semaphore refresh_lock;
	int enable;
};
struct sprd_device {
	struct platform_device *pdev;
	struct display_ctrl *ctrl;
	struct sprd_dispc_context *dispc_ctx;
	struct panel_if_device *intf;
	void *logo_buffer_addr_v;
	void *priv1;
	uint32_t logo_buffer_size;

	struct device *of_dev;
	uint16_t panel_if_type;	/* DPI , EDPI */
	uint16_t panel_ready;
	uint16_t dev_id;	/*main_lcd, sub_lcd */
	uint16_t dc_mode;
	uint32_t format;
	uint32_t bpp;
	uint32_t dpi_clock;
};

struct display_ctrl {
	const char *name;

	int32_t(*early_init) (struct sprd_device *dev);
	int32_t(*init) (struct sprd_device *dev);
	int32_t(*uninit) (struct sprd_device *dev);
	int32_t(*wait_for_sync) (struct sprd_device *dev);
	int32_t(*flip) (struct sprd_device *dev);
	void (*logo_proc) (struct sprd_device *dev);
	int32_t(*shutdown) (struct sprd_device *dev);
	int32_t(*suspend) (struct sprd_device *dev);
	int32_t(*resume) (struct sprd_device *dev);
	int32_t(*update_clk) (struct sprd_device *dev);
};
struct pipe {
	int n_sdev;
	struct sprd_device *sdev[2];
};
#ifndef ROUND
#define ROUND(a, b) (((a) + (b) / 2) / (b))
#endif


extern struct display_ctrl sprd_dispc_ctrl;
extern unsigned long g_dispc_base_addr;
extern unsigned long lcd_base_from_uboot;

/* function declaration begin */
int sprd_create_sysfs(struct sprd_device *dev);
void sprd_remove_sysfs(struct sprd_device *dev);
int sprd_dispc_chg_clk(struct sprd_device *dev, int type, u32 new_val);
int32_t sprd_dispc_flip(struct sprd_device *dev,
			  struct sprd_restruct_config *config);

#endif
