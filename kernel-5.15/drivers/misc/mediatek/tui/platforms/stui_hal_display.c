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

#include <mtk_heap.h>
#include <public/trusted_mem_api.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>

static struct dma_buf *tui_dma_buf = NULL;

#define DEFAULT_BPP		32

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
	void *disp_data = NULL;
	int ret = 0;

	pr_debug("[STUI] %s : state %d start\n", __func__, tui_en);

	ret = disp_tui_prepare(disp_data, tui_en);
	pr_info("[STUI] %s : state %d end\n", __func__, tui_en);
	return ret;
}

void stui_free_video_space(void)
{
	/* TODO: free contiguous memory allocated at stui_alloc_video_space */
	if (tui_dma_buf != NULL) {
		dma_heap_buffer_free(tui_dma_buf);
		tui_dma_buf = NULL;
	}
}

int stui_alloc_video_space(struct tui_hw_buffer *buffer)
{
	int ret = 0;
	uint64_t phys_addr = 0;
	size_t framebuf_size;
	size_t workbuf_size;
	size_t total_size;
	uint64_t sec_handle = 0;
	struct dma_heap *dma_heap;

	/* TODO: set width, height field with display resolution */

	uint32_t witdh = DISP_GetScreenWidth();
	uint32_t height = DISP_GetScreenHeight();

	framebuf_size = (witdh * height * (DEFAULT_BPP / 8));
	/* TODO: set different alingment, if needed */
	framebuf_size = STUI_ALIGN_UP(framebuf_size, STUI_ALIGN_1MB_SZ);
	workbuf_size = framebuf_size;
	total_size = framebuf_size + workbuf_size;

	dma_heap = dma_heap_find("mtk_tui_region-aligned");
	if (dma_heap == NULL) {
		pr_err("tui heap find failed!\n");
		ret = -1;
		goto error;
	}

	tui_dma_buf = dma_heap_buffer_alloc(dma_heap, total_size,
						DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
	if (tui_dma_buf == NULL) {
		pr_err("%s, alloc buffer fail, heap:%s", __func__,
								dma_heap_get_name(dma_heap));
		ret = -1;
		goto error;
	}

	sec_handle = dmabuf_to_secure_handle(tui_dma_buf);
	if (sec_handle == 0) {
		pr_err("%s, get tui frame buffer secure handle failed!\n", __func__);
		ret =  -1;
		goto error;
	}

	ret = trusted_mem_api_query_pa(0, 0, 0, 0, (uint32_t *)&sec_handle, 0, 0, 0, &phys_addr);
	if (ret != 0) {
		pr_notice("%s(%d): trusted_mem_api_query_pa failed!\n", __func__, __LINE__);
		ret= -1;
		goto error;
	}

	pr_debug("alloc (p= %08x s=0x%lx)\n", (uint64_t)phys_addr, total_size);

	buffer->width = witdh;
	buffer->height = height;
	buffer->fb_physical = (uint64_t)phys_addr;
	buffer->wb_physical = (uint64_t)((workbuf_size) ? (phys_addr + framebuf_size) : 0);
	buffer->disp_physical = 0;
	buffer->fb_size = framebuf_size;
	buffer->wb_size = workbuf_size;
	buffer->disp_size = 0;

	return ret;

error:
	if (tui_dma_buf != NULL) {
		dma_heap_buffer_free(tui_dma_buf);
		tui_dma_buf = NULL;
	}
	return ret;
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
