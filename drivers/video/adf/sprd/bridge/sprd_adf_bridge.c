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

#include "sprd_adf_bridge.h"
#include "sprd_main.h"

static struct sprd_display_config_entry *g_display_configs[] = {
	/*SHARK*/
	&shark_display_config_entry,
};

/**
 * sprd_adf_get_config_entry - get drv's callback entry
 *
 *
 * sprd_adf_get_config_entry according to the platform id,
 * get the dispc hw id
 */
struct sprd_display_config_entry *sprd_adf_get_config_entry(void)
{
	size_t i;
	size_t platform_id;

	ADF_DEBUG_GENERAL("entern\n");
	platform_id = sprd_adf_get_platform_id();
	for (i = 0; i < ARRAY_SIZE(g_display_configs); i++) {
		if (g_display_configs[i]->id == platform_id)
			break;
	}

	if (i >= ARRAY_SIZE(g_display_configs)) {
		ADF_DEBUG_WARN("get display config entry failed\n");
		goto error_out;
	}

	return g_display_configs[i];

error_out:
	return NULL;
}

static struct sprd_adf_device_ops device_ops = {
	.restruct_post_config = sprd_adf_restruct_post_config,
	.free_restruct_config = sprd_adf_free_restruct_config,
	.flip = sprd_adf_device_flip,
	.wait_flip_done = sprd_adf_wait_flip_done,
};

static struct sprd_adf_interface_ops dsi0_ops = {
	.init = sprd_adf_interface_init,
	.uninit = sprd_adf_interface_uninit,
	.suspend = sprd_adf_early_suspend,
	.resume = sprd_adf_late_resume,
	.shutdown = sprd_adf_shutdown,
	.dpms_on = sprd_adf_interface_dpms_on,
	.dpms_off = sprd_adf_interface_dpms_off,
	.enable_vsync_irq = sprd_adf_enable_vsync_irq,
	.disable_vsync_irq = sprd_adf_disable_vsync_irq,
	.get_screen_size = sprd_adf_get_screen_size,
	.get_preferred_mode = sprd_adf_get_preferred_mode,
};

/**
 * sprd_adf_get_device_ops - get sprd device callback function
 *
 * @index: device index
 *
 * sprd_adf_get_device_ops() get the target device's callback
 * functions
 */
struct sprd_adf_device_ops *sprd_adf_get_device_ops(size_t index)
{
	ADF_DEBUG_GENERAL("entern\n");
	if (index != 0) {
		ADF_DEBUG_WARN("index is illegal\n");
		return NULL;
	}

	return &device_ops;
}

/**
 * sprd_adf_get_device_private_data - get device some private data
 *
 * @pdev: parent device
 * @index: device index
 *
 * sprd_adf_get_device_private_data() get the target device's private
 * data.
 */
void *sprd_adf_get_device_private_data(struct platform_device *pdev,
			size_t index)
{
	ADF_DEBUG_GENERAL("entern\n");

	return NULL;
}

/**
 * sprd_adf_destory_device_private_data - free some resource
 *
 * @data: private data ptr
 *
 * sprd_adf_destory_device_private_data() free private data.
 */
void sprd_adf_destory_device_private_data(void *data)
{
	ADF_DEBUG_GENERAL("entern\n");
	kfree(data);
}

/**
 * sprd_adf_get_interface_ops - get sprd interface callback function
 *
 * @index: interface index
 *
 * sprd_adf_get_interface_ops() get the target interface's callback
 * functions
 */
struct sprd_adf_interface_ops *sprd_adf_get_interface_ops(size_t index)
{
	ADF_DEBUG_GENERAL("entern\n");
	return &dsi0_ops;
}

/**
 * sprd_adf_get_interface_private_data - get interface some private data
 *
 * @pdev: parent device
 * @index: device index
 *
 * sprd_adf_get_interface_private_data() get the target interface's private
 * data.
 */
void *sprd_adf_get_interface_private_data(struct platform_device *pdev,
			size_t index)
{
	size_t display_mode;
	struct pipe *pipe;

	ADF_DEBUG_GENERAL("entern\n");
	if (!pdev) {
		ADF_DEBUG_WARN("index is illegal\n");
		return NULL;
	}

	display_mode = SPRD_SINGLE_DISPC;

	pipe = kzalloc(sizeof(struct pipe), GFP_KERNEL);
	if (!pipe) {
		ADF_DEBUG_WARN("kzalloc pipe faile\n");
		return NULL;
	}

	pipe_config(pipe, display_mode, index, pdev);

	return (void *)pipe;
}

/**
 * sprd_adf_destory_interface_private_data - free some resource
 *
 * @data: private data ptr
 *
 * sprd_adf_destory_interface_private_data() free private data.
 */
void sprd_adf_destory_interface_private_data(void *data)
{
	ADF_DEBUG_GENERAL("entern\n");
	kfree(data);
}
