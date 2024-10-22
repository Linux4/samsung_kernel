// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include "t-base-tui.h"

#include "tui_ioctl.h"
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#include "tui-hal_mt.h"

#include <mtk_heap.h>
#include <public/trusted_mem_api.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>


static struct dma_buf *tui_dma_buf[MAX_DCI_BUFFER_NUMBER];

#ifdef TUI_LOCK_I2C
static int i2c_tui_clock_enable(int id)
{
	int ret = 0;
	struct i2c_adapter *adap = NULL;
	struct device *i2c_device = NULL;
	struct clk *i2c_clk_main = NULL;
	struct clk *i2c_clk_dma = NULL;

	adap = i2c_get_adapter(id);
	if (adap) {
		i2c_device = adap->dev.parent;
		i2c_clk_main = devm_clk_get(i2c_device, "main");
		if (IS_ERR(i2c_clk_main)) {
			pr_notice("[TUI-HAL] %s() cannot get i2c main clock\n", __func__);
			return PTR_ERR(i2c_clk_main);
		}
		ret = clk_prepare_enable(i2c_clk_main);
		if (ret) {
			pr_notice("[TUI-HAL] %s() enable i2c main clock fail\n", __func__);
			return ret;
		}
		i2c_clk_dma = devm_clk_get(i2c_device, "dma");
		if (IS_ERR(i2c_clk_dma)) {
			pr_notice("[TUI-HAL] %s() cannot get i2c dma clock\n", __func__);
			return PTR_ERR(i2c_clk_dma);
		}
		ret = clk_prepare_enable(i2c_clk_dma);
		if (ret) {
			pr_notice("[TUI-HAL] %s() enable i2c dma clock fail\n", __func__);
			return ret;
		}
	} else {
		pr_notice("[TUI-HAL] %s() cannot get i2c adapter\n", __func__);
		ret = -1;
	}
	return ret;
}

static int i2c_tui_clock_disable(int id)
{
	int ret = 0;
	struct i2c_adapter *adap = NULL;
	struct device *i2c_device = NULL;
	struct clk *i2c_clk_main = NULL;
	struct clk *i2c_clk_dma = NULL;

	adap = i2c_get_adapter(id);
	if (adap) {
		i2c_device = adap->dev.parent;
		i2c_clk_main = devm_clk_get(i2c_device, "main");
		if (IS_ERR(i2c_clk_main)) {
			pr_notice("[TUI-HAL] %s() cannot get i2c main clock\n", __func__);
			return PTR_ERR(i2c_clk_main);
		}
		clk_disable_unprepare(i2c_clk_main);

		i2c_clk_dma = devm_clk_get(i2c_device, "dma");
		if (IS_ERR(i2c_clk_dma)) {
			pr_notice("[TUI-HAL] %s() cannot get i2c dma clock\n", __func__);
			return PTR_ERR(i2c_clk_dma);
		}
		clk_disable_unprepare(i2c_clk_dma);
	} else {
		pr_notice("[TUI-HAL] %s() cannot get i2c adapter\n", __func__);
		ret = -1;
	}
	return ret;
}
#endif

/**
 * hal_tui_init() - integrator specific initialization for kernel module
 *
 * This function is called when the kernel module is initialized, either at
 * boot time, if the module is built statically in the kernel, or when the
 * kernel is dynamically loaded if the module is built as a dynamic kernel
 * module. This function may be used by the integrator, for instance, to get a
 * memory pool that will be used to allocate the secure framebuffer and work
 * buffer for TUI sessions.
 *
 * Return: must return 0 on success, or non-zero on error. If the function
 * returns an error, the module initialization will fail.
 */
uint32_t hal_tui_init(void)
{
	pr_info("%s\n", __func__);

	/* Allocate memory pool for the framebuffer
	 */
	return TUI_DCI_OK;
}

/**
 * hal_tui_exit() - integrator specific exit code for kernel module
 *
 * This function is called when the kernel module exit. It is called when the
 * kernel module is unloaded, for a dynamic kernel module, and never called for
 * a module built into the kernel. It can be used to free any resources
 * allocated by hal_tui_init().
 */
void hal_tui_exit(void)
{
	/* delete memory pool if any */
}

/**
 * hal_tui_alloc() - allocator for secure framebuffer and working buffer
 * @allocbuffer:    input parameter that the allocator fills with the physical
 *                  addresses of the allocated buffers
 * @allocsize:      size of the buffer to allocate.  All the buffer are of the
 *                  same size
 * @number:         Number to allocate.
 *
 * This function is called when the module receives a CMD_TUI_SW_OPEN_SESSION
 * message from the secure driver.  The function must allocate 'number'
 * buffer(s) of physically contiguous memory, where the length of each buffer
 * is at least 'allocsize' bytes.  The physical address of each buffer must be
 * stored in the array of structure 'allocbuffer' which is provided as
 * arguments.
 *
 * Physical address of the first buffer must be put in allocate[0].pa , the
 * second one on allocbuffer[1].pa, and so on.  The function must return 0 on
 * success, non-zero on error.  For integrations where the framebuffer is not
 * allocated by the Normal World, this function should do nothing and return
 * success (zero).
 * If the working buffer allocation is different from framebuffers, ensure that
 * the physical address of the working buffer is at index 0 of the allocbuffer
 * table (allocbuffer[0].pa).
 */
uint32_t hal_tui_alloc(
	struct tui_alloc_buffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
	size_t allocsize, uint32_t number)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	uint64_t pa = 0;
	uint64_t sec_handle = 0;
	struct dma_heap *dma_heap;
	uint32_t i = 0;

	if (!allocbuffer) {
		pr_notice("%s(%d): allocbuffer is null\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	pr_info("%s(%d): Requested size=0x%zx x %u chunks\n",
		 __func__, __LINE__, allocsize, number);

	if ((size_t)allocsize == 0 || number > MAX_DCI_BUFFER_NUMBER) {
		pr_notice("%s(%d): Nothing to allocate\n", __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	dma_heap = dma_heap_find("mtk_tui_region-aligned");
	if (!dma_heap) {
		pr_notice("heap find failed!\n");
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	for (i = 0; i < number; i++) {
		tui_dma_buf[i] = dma_heap_buffer_alloc(dma_heap, allocsize,
					DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
		if (IS_ERR(tui_dma_buf[i])) {
			pr_notice("%s, alloc buffer fail, heap:%s", __func__,
						dma_heap_get_name(dma_heap));
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			goto error;
		}

		sec_handle = dmabuf_to_secure_handle(tui_dma_buf[i]);
		if (!sec_handle) {
			pr_notice("%s, get tui frame buffer secure handle failed!\n", __func__);
			ret =  TUI_DCI_ERR_INTERNAL_ERROR;
			goto error;
		}

		ret = trusted_mem_api_query_pa(0, 0, 0, 0, &sec_handle, 0, 0, 0, &pa);
		if (ret == 0) {
			allocbuffer[i].pa = (uint64_t) pa;
			allocbuffer[i].ffa_handle = (uint64_t)sec_handle;
			pr_info("%s(%d):%d: buf 0x%llx, handle 0x%llx\n",
			__func__, __LINE__, i, allocbuffer[i].pa, allocbuffer[i].ffa_handle);
		} else {
			pr_notice("%s(%d): trusted_mem_api_query_pa failed!\n",
							 __func__, __LINE__);
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			goto error;
		}
	}

	return TUI_DCI_OK;

error:
	for (i = 0; i < MAX_DCI_BUFFER_NUMBER; i++) {
		if (tui_dma_buf[i] != NULL) {
			dma_heap_buffer_free(tui_dma_buf[i]);
			tui_dma_buf[i] = NULL;
		}
	}

	return ret;
}

/**
 * hal_tui_free() - free memory allocated by hal_tui_alloc()
 *
 * This function is called at the end of the TUI session, when the TUI module
 * receives the CMD_TUI_SW_CLOSE_SESSION message. The function should free the
 * buffers allocated by hal_tui_alloc(...).
 */
void hal_tui_free(void)
{
	int i = 0;

	pr_debug("[TUI-HAL] %s\n", __func__);

	for (i = 0; i < MAX_DCI_BUFFER_NUMBER; i++) {
		if (tui_dma_buf[i] != NULL) {
			dma_heap_buffer_free(tui_dma_buf[i]);
			tui_dma_buf[i] = NULL;
		}
	}
}

/**
 * hal_tui_deactivate() - deactivate Normal World display and input
 *
 * This function should stop the Normal World display and, if necessary, Normal
 * World input. It is called when a TUI session is opening, before the Secure
 * World takes control of display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_deactivate(void)
{
	int ret = TUI_DCI_OK;
	int __maybe_unused tmp = 0;

	pr_debug("%s+\n", __func__);
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	/*
	 * Stop NWd display here.  After this function returns, SWd will take
	 * control of the display and input.  Therefore the NWd should no longer
	 * access it
	 * This can be done by calling the fb_blank(FB_BLANK_POWERDOWN) function
	 * on the appropriate framebuffer device
	 */

#ifdef TUI_ENABLE_TOUCH
	tpd_enter_tui();
#endif

#ifdef TUI_LOCK_I2C
	i2c_tui_clock_enable(0);
#endif

#ifdef TUI_ENABLE_DISPLAY
	tmp = display_enter_tui();
	if (tmp) {
		pr_notice("[TUI-HAL] %s() failed because display\n", __func__);
		ret = TUI_DCI_ERR_OUT_OF_DISPLAY;
	}
#endif

	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			   TRUSTEDUI_MODE_INPUT_SECURED);

	pr_debug("[TUI-HAL] %s()\n", __func__);

	return ret;
}

/**
 * hal_tui_activate() - restore Normal World display and input after a TUI
 * session
 *
 * This function should enable Normal World display and, if necessary, Normal
 * World input. It is called after a TUI session, after the Secure World has
 * released the display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_activate(void)
{
	pr_info("[TUI-HAL] %s+\n", __func__);
	/* Protect NWd */
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|
			     TRUSTEDUI_MODE_INPUT_SECURED);
	/*
	 * Restart NWd display here.  TUI session has ended, and therefore the
	 * SWd will no longer use display and input.
	 * This can be done by calling the fb_blank(FB_BLANK_UNBLANK) function
	 * on the appropriate framebuffer device
	 */
	/* Clear linux TUI flag */
#ifdef TUI_ENABLE_TOUCH
	tpd_exit_tui();
#endif

#ifdef TUI_LOCK_I2C
	i2c_tui_clock_disable(0);
#endif

#ifdef TUI_ENABLE_DISPLAY
	display_exit_tui();
#endif

	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_process_cmd(struct tui_hal_cmd_t *cmd,
			     struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
uint32_t hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("%s\n", __func__);
}
