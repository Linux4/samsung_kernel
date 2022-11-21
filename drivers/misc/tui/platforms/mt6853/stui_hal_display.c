/*
 * Samsung TUI HW Handler driver. Display functions.
 *
 * Copyright (c) 2015-2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stui_core.h"
#include "stui_hal.h"
#include "stui_inf.h"
#include "tui_platform.h"
#include "memory_ssmr.h"

#include <linux/fb.h>
#include <linux/version.h>

#define DEFAULT_BPP		32
static int buffer_allocated;

static struct fb_info *get_fb_info_for_tui(struct device *fb_dev);

/* Framebuffer device driver identification
 * RETURN: 0 - Wrong device driver
 *         1 - Suitable device driver
 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 9, 0)
static int _is_dev_ok(struct device *fb_dev, void *p)
#else
static int _is_dev_ok(struct device *fb_dev, const void *p)
#endif
{
	struct fb_info *fb_info;

	fb_info = get_fb_info_for_tui(fb_dev);
	if (!fb_info)
		return 0;

	return 1;
}

/* Find suitable framebuffer device driver */
static struct device *get_fb_dev_for_tui(void)
{
	struct device *fb_dev;
	/* get the first framebuffer device */
	fb_dev = class_find_device(fb_class, NULL, NULL, _is_dev_ok);
	if (!fb_dev)
		pr_info("[STUI] class_find_device failed\n");

	return fb_dev;
}

/* Get framebuffer's internal data */
static struct fb_info *get_fb_info_for_tui(struct device *fb_dev)
{
	struct fb_info *fb_item;

	if (!fb_dev || !fb_dev->p) {
		pr_info("[STUI] framebuffer device has no private data\n");
		return NULL;
	}
	fb_item = (struct fb_info *)dev_get_drvdata(fb_dev);
	if (!fb_item)
		pr_info("[STUI] dev_get_drvdata failed\n");

	return fb_item;
}

/* Prepare / restore display controller's registers */
static int disp_tui_prepare(void *disp_dev_data, bool tui_en)
{
	int ret = 0;
	pr_debug("[STUI] disp_tui_protection(%d) - start\n", (int)tui_en);
	if (tui_en) {
		/* TODO:
		 * Prepare display constroller's SFR registers to start TUI here
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_get_sync(disp_dev_data->dev);
#endif
		 */
#ifdef TUI_ENABLE_DISPLAY
		ret = display_enter_tui();
#endif
	} else {
		/* TODO:
		 * Restore display constroller's SFR registers here
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_put_sync(disp_dev_data->dev);
#endif
		 */
#ifdef TUI_ENABLE_DISPLAY
		ret = display_exit_tui();
#endif
	}
	return ret;
}

static int fb_protection_for_tui(bool tui_en)
{
	struct device *fb_dev;
	struct fb_info *fb_info;
	void *disp_data = NULL;
	int ret = 0;

	pr_debug("[STUI] %s : state %d start\n", __func__, tui_en);

	fb_dev = get_fb_dev_for_tui();
	if (!fb_dev)
		return -1;

	fb_info = get_fb_info_for_tui(fb_dev);
	if (!fb_info)
		return -1;

	ret = disp_tui_prepare(disp_data, tui_en);
	pr_info("[STUI] %s : state %d end\n", __func__, tui_en);
	return ret;
}

void stui_free_video_space(void)
{
	/* TODO: free contiguous memory allocated at stui_alloc_video_space */
	if (buffer_allocated) {
		ssmr_online(SSMR_FEAT_TUI);
		buffer_allocated = 0;
	}
}

int stui_alloc_video_space(struct tui_hw_buffer *buffer)
{
	int ret;
	dma_addr_t phys_addr = 0;
	size_t framebuf_size;
	size_t workbuf_size;
	size_t total_size;
	/* TODO: set width, height field with display resolution */

	uint32_t witdh = DISP_GetScreenWidth();
	uint32_t height = DISP_GetScreenHeight();

	framebuf_size = (witdh * height * (DEFAULT_BPP / 8));
	/* TODO: set different alingment, if needed */
	framebuf_size = STUI_ALIGN_UP(framebuf_size, STUI_ALIGN_1MB_SZ);
	workbuf_size = framebuf_size;
	total_size = framebuf_size + workbuf_size;

	/* TODO: get contiguous memory for framebuf_size + workbuf_size */
	ret = ssmr_offline((phys_addr_t *)&phys_addr, &total_size, true, SSMR_FEAT_TUI);
	if (ret) {
		pr_err("%s(%d): ssmr_offline failed!\n",
				__func__, __LINE__);
		return -1;
	}
	buffer_allocated = 1;
	pr_debug("alloc (p=%p s=0x%lx)\n", (void *)phys_addr, total_size);

	buffer->width = witdh;
	buffer->height = height;
	buffer->fb_physical = (uint64_t)phys_addr;
	buffer->wb_physical = (uint64_t)((workbuf_size) ? (phys_addr + framebuf_size) : 0);
	buffer->disp_physical = 0;
	buffer->fb_size = framebuf_size;
	buffer->wb_size = workbuf_size;
	buffer->disp_size = 0;

	return 0;
}

int stui_get_resolution(struct tui_hw_buffer *buffer)
{
	/* TODO: set width, height field with display resolution */
	buffer->width = DISP_GetScreenWidth();
	buffer->height = DISP_GetScreenHeight();

	return 0;
}

int stui_prepare_tui(void)
{
	pr_debug("[STUI] %s - start\n", __func__);
	return fb_protection_for_tui(true);
}

void stui_finish_tui(void)
{
	pr_debug("[STUI] %s - start\n", __func__);
	if (fb_protection_for_tui(false))
		pr_err("[STUI] failed to unprotect tui\n");
}
