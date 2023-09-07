/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/fb.h>
#include "ion_drv.h"
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/stui_inf.h>

#include "stui_core.h"
#include "stui_hal.h"
#include "tui_platform.h"

static struct ion_client *client;
static struct ion_handle *handle;
static struct dma_buf *dbuf;

static int buffer_allocated;

/* TODO:
 * Set ion driver parameters according to the device
 */
#define ION_DEV		g_ion_device
#define ION_ID_MASK	(1 << 4)		/* Use contiguous memory*/
#define ION_FLAGS	(1 << (32 - 11))	/* Use memory for video*/

static struct fb_info *get_fb_info_for_tui(struct device *fb_dev);

/* Framebuffer device driver identification
 * RETURN: 0 - Wrong device driver
 *         1 - Suitable device driver
 */
static int _is_dev_ok(struct device *fb_dev, const void *p)
{
	struct fb_info *fb_info;

	fb_info = get_fb_info_for_tui(fb_dev);
	if (!fb_info)
		return 0;

	/* TODO:
	 * Place your custom identification here
	 */

	return 1;
}

/* Find suitable framebuffer device driver */
static struct device *get_fb_dev_for_tui(void)
{
	struct device *fb_dev;

	/* get the first framebuffer device */
	fb_dev = class_find_device(fb_class, NULL, NULL, _is_dev_ok);
	if (!fb_dev)
		pr_notice("[STUI] class_find_device failed\n");

	return fb_dev;
}

/* Get framebuffer's internal data */
static struct fb_info *get_fb_info_for_tui(struct device *fb_dev)
{
	struct fb_info *fb_item;

	if (!fb_dev || !fb_dev->p) {
		pr_notice("[STUI] framebuffer device has no private data\n");
		return NULL;
	}
	fb_item = (struct fb_info *)dev_get_drvdata(fb_dev);
	if (!fb_item)
		pr_notice("[STUI] dev_get_drvdata failed\n");

	return fb_item;
}

/* Get display controller internal structure from the framebuffer info */
static void *get_disp_dev_for_tui(void)
{
	struct device *fb_dev;
	struct fb_info *fb_info;
	void *disp_dev_data = (void *)0xDEADBEAF; /* TODO: NULL;*/

	fb_dev = get_fb_dev_for_tui();
	if (!fb_dev)
		return NULL;

	fb_info = get_fb_info_for_tui(fb_dev);
	if (!fb_info)
		return NULL;

	/* TODO:
	 * Place your custom way to get internal data here
	disp_dev_data = fb_info->internal_data;
	 */

	if (!disp_dev_data)
		pr_notice("[STUI] disp_dev_data pointer is NULL\n");

	return disp_dev_data;
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
#ifdef TUI_ENABLE_DISPLAY
		ret = display_exit_tui();
#endif

		/* TODO:
		 * Restore display constroller's SFR registers here

#ifdef CONFIG_PM_RUNTIME
		pm_runtime_put_sync(disp_dev_data->dev);
#endif
		 */

	}
	msleep(100);
	return ret;
}


static int fb_protection_for_tui(void)
{
	void *disp_data = NULL;

	pr_debug("[STUI] fb_protection_for_tui() - start\n");
	disp_data = get_disp_dev_for_tui();
	if (!disp_data)
		return -1;

	return disp_tui_prepare(disp_data, true);
}

static int fb_unprotection_for_tui(void)
{
	void *disp_data = NULL;

	pr_debug("[STUI] fb_unprotection_for_tui() - start\n");
	disp_data = get_disp_dev_for_tui();
	if (!disp_data)
		return -1;

	return disp_tui_prepare(disp_data, false);
}

static void __maybe_unused stui_free_video_space_to_ion(void)
{
	dma_buf_put(dbuf);
	ion_free(client, handle);
	ion_client_destroy(client);
}

static unsigned long __maybe_unused stui_alloc_video_from_ion(uint32_t allocsize, uint32_t count)
{
	unsigned long phys_addr = 0;
	unsigned long offset = 0;
	unsigned int size, work_size = 0;

	client = ion_client_create(ION_DEV, "STUI module");
	if (IS_ERR_OR_NULL(client)) {
		pr_notice("[STUI] ion_client_create() - failed: %ld\n", PTR_ERR(client));
		return 0;
	}

	size = STUI_ALIGN_UP(allocsize, STUI_ALIGN_1MB_SZ);
	if (count > 1)
		work_size = size;

	size += STUI_ALIGN_1MB_SZ;

try_alloc:
	handle = ion_alloc(client, size + work_size, 0, ION_ID_MASK,
			ION_FLAGS);

	if (IS_ERR_OR_NULL(handle)) {
		if (work_size) {
			work_size -= STUI_ALIGN_1MB_SZ;
			goto try_alloc;
		}
		pr_notice("[STUI] ion_alloc() - failed: %ld\n", PTR_ERR(handle));
		goto clean_client;
	}

	dbuf = ion_share_dma_buf(client, handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_notice("[STUI] ion_share_dma_buf() - failed: %ld\n", PTR_ERR(dbuf));
		goto clean_alloc;
	}

	ion_phys(client, handle, (unsigned long *)&phys_addr, &dbuf->size);
	if (!phys_addr)
		goto clean_share_dma;

	/* TUI frame buffer must be aligned 1M
	 * TODO:
	 * Set different alingment, if required
	 */
	if (phys_addr % STUI_ALIGN_1MB_SZ)
		offset = STUI_ALIGN_1MB_SZ - (phys_addr % STUI_ALIGN_1MB_SZ);

	phys_addr = phys_addr + offset;
	size -= offset;
	pr_debug("[STUI] phys_addr : %lx\n", phys_addr);

	g_stui_buf_info.pa[0] = (uint64_t)phys_addr;
	g_stui_buf_info.pa[1] =  (uint64_t)((work_size)?(phys_addr + size):0);
	g_stui_buf_info.size[0] = size;
	g_stui_buf_info.size[1] = work_size;
	return phys_addr;

clean_share_dma:
	dma_buf_put(dbuf);

clean_alloc:
	ion_free(client, handle);

clean_client:
	ion_client_destroy(client);

	return phys_addr;
}

static void __maybe_unused stui_free_video_space_to_ssvp(void)
{
	if (buffer_allocated) {
		tui_region_online();
		buffer_allocated = 0;
	}
}

static unsigned long __maybe_unused stui_alloc_video_from_ssvp(uint32_t allocsize, uint32_t count)
{
	int ret = 0;
	phys_addr_t pa;
	phys_addr_t pa_next;
	unsigned int work_size = 0;
	unsigned long size = 0;
	unsigned long align_size = 0;
	const int extra_size = 0x100000;

	align_size = STUI_ALIGN_UP(allocsize, STUI_ALIGN_1MB_SZ);
	if (count > 1)
		work_size = align_size;

	pr_debug("%s(%d): Requested size=0x%x(0x%lx) count=%u\n",
		 __func__, __LINE__, allocsize, align_size, count);

	size = align_size + work_size;
	/*extra buffer for display internal*/
	size += extra_size;
	/*extra buffer for touch  internal*/
	size += extra_size;
	ret = tui_region_offline(&pa, &size);
	if (ret) {
		pr_notice("%s(%d): tui_region_offline failed!\n",
				__func__, __LINE__);
		return 0;
	}
	buffer_allocated = 1;
	pr_debug("alloc (p=%p s=0x%lx)\n", (void *)pa, size);

	g_stui_buf_info.pa[0] = (unsigned long)pa;
	g_stui_buf_info.size[0] = align_size;
	/* working buffer pa*/
	pa_next = ((work_size)?(pa + align_size):0);
	g_stui_buf_info.pa[1] =  (unsigned long)pa_next;
	g_stui_buf_info.size[1] = work_size;

	/* dispaly internal buffer pa*/
	pa_next = ((work_size)?(pa_next + extra_size):(pa + align_size));
	g_stui_buf_info.pa[2] =  (unsigned long)pa_next;
	g_stui_buf_info.size[2] = extra_size;

	/* cmdq  internal buffer pa*/
	pa_next += extra_size;
	g_stui_buf_info.pa[3] =  (unsigned long)pa_next;
	g_stui_buf_info.size[3] = extra_size;

	pr_debug("alloc buf (0x%lx,%zx),(0x%lx,%zx),(0x%lx,%zx),(0x%lx,%zx)\n",
			g_stui_buf_info.pa[0], g_stui_buf_info.size[0],
			g_stui_buf_info.pa[1], g_stui_buf_info.size[1],
			g_stui_buf_info.pa[2], g_stui_buf_info.size[2],
			g_stui_buf_info.pa[3], g_stui_buf_info.size[3]);

	return pa;
}

void stui_free_video_space(void)
{
#ifdef TUI_ENABLE_MEMORY_SSVP
		return stui_free_video_space_to_ssvp();
#else
		return stui_free_video_space_to_ion();
#endif

}

unsigned long stui_alloc_video_space(uint32_t allocsize, uint32_t count)
{
#ifdef TUI_ENABLE_MEMORY_SSVP
	return stui_alloc_video_from_ssvp(allocsize, count);
#else
	return stui_alloc_video_from_ion(allocsize, count);
#endif
}

int stui_prepare_tui(void)
{
	pr_debug("[STUI] stui_prepare_tui() - start\n");
	return fb_protection_for_tui();
}

void stui_finish_tui(void)
{
	pr_debug("[STUI] stui_finish_tui() - start\n");
	fb_unprotection_for_tui();
	stui_free_video_space();
}
