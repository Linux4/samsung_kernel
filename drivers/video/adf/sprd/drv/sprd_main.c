/*
 *Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#define pr_fmt(fmt)		"sprd-adf: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#endif
#include <linux/time.h>
#include <soc/sprd/arch_misc.h>
#include "sprd_chip_common.h"
#include "sprd_interface.h"
#ifdef CONFIG_FB_MMAP_CACHED
#include <asm/pgtable.h>
#include <linux/mm.h>
#endif
#include "sprd_adf_adapter.h"

u64 g_time_start, g_time_end;
enum {
	SPRD_IN_DATA_TYPE_ABGR888 = 0,
	SPRD_IN_DATA_TYPE_BGR565,
	/*
	   SPRD_IN_DATA_TYPE_RGB666,
	   SPRD_IN_DATA_TYPE_RGB555,
	   SPRD_IN_DATA_TYPE_PACKET,
	*/
	 SPRD_IN_DATA_TYPE_LIMIT
};
#define SPRDFB_IN_DATA_TYPE SPRD_IN_DATA_TYPE_ABGR888

static struct sprd_device *sprd_device_init(struct platform_device *pdev,
						int index);
int32_t sprd_adf_early_suspend(struct sprd_adf_interface *adf_intf)
{
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	int num_dev = pipe->n_sdev;
	ENTRY();
	for (index = 0; index < num_dev; index++) {
		PRINT("dispc%d\n", index);
		dev = pipe->sdev[index];
		dev->ctrl->suspend(dev);
	}
	LEAVE();
	return 0;
}

int32_t sprd_adf_late_resume(struct sprd_adf_interface *adf_intf)
{
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	int num_dev = pipe->n_sdev;
	ENTRY();
	for (index = 0; index < num_dev; index++) {
		PRINT("dispc%d\n", index);
		dev = pipe->sdev[index];
		dev->ctrl->resume(dev);
	}
	LEAVE();
	return 0;
}

uint32_t sprd_get_chip_id(void)
{
	uint32_t adie_chip_id = sci_get_chip_id();
	if ((adie_chip_id & 0xffff0000) < 0x50000000) {
		PRINT("chip id 0x%08x is invalidate\n", adie_chip_id);
		PRINT("try to get it by reading reg directly!\n");

		adie_chip_id = dispc_glb_read(SPRD_AONAPB_BASE + 0xFC);
		if ((adie_chip_id & 0xffff0000) < 0x50000000) {
			PRINT("chip id 0x%08x from reg is invalidate too!\n"
			     , adie_chip_id);
		}
	}

	PRINT("return chip id 0x%08x\n", adie_chip_id);
	return adie_chip_id;
}

size_t sprd_adf_get_platform_id(void)
{
	ADF_DEBUG_GENERAL("entern\n");
	return 0XFF;
}

struct sprd_restruct_config *sprd_adf_restruct_post_config(
				struct adf_post *post)
{
	struct sprd_restruct_config *config;
	struct sprd_adf_post_custom_data *data;
	struct adf_buffer *bufs;
	struct sprd_adf_hwlayer *dst;
	struct sprd_adf_hwlayer_custom_data *cust_src;
	size_t n_bufs;
	size_t i, j;
	size_t kzalloc_size;
	size_t custom_data_size;
	size_t display_mode;

	if (!post || !post->custom_data) {
		ADF_DEBUG_WARN("parameter is ilegal\n");
		goto error_out;
	}

	custom_data_size = (size_t)post->custom_data_size;
	if (custom_data_size < FLIP_CUSTOM_DATA_MIN_SIZE) {
		ADF_DEBUG_WARN("data size is ilegal data_size = %zd;\n"
				, custom_data_size);
		goto error_out;
	}

	display_mode = SPRD_SINGLE_DISPC;
	data = (struct sprd_adf_post_custom_data *)post->custom_data;
	bufs = post->bufs;
	n_bufs = post->n_bufs;

	kzalloc_size =
	    sizeof(struct sprd_restruct_config) +
	    n_bufs * sizeof(struct sprd_adf_hwlayer);
	if (display_mode == SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE
	    || display_mode == SPRD_DOUBLE_DISPC_SAME_CONTENT) {
		kzalloc_size = kzalloc_size << 1;
		n_bufs = n_bufs << 1;
	}

	config = kzalloc(kzalloc_size, GFP_KERNEL);
	if (!config) {
		ADF_DEBUG_WARN("kzalloc sprd_restruct_config failed\n");
		goto error_out;
	}

	config->number_hwlayer = n_bufs;

	for (i = 0; i < post->n_bufs; i++) {
		dst = &config->hwlayers[i];
		cust_src = &data->hwlayers[i];

		dst->base = bufs[i];
		dst->hwlayer_id = cust_src->hwlayer_id;
		dst->interface_id = cust_src->interface_id;
		dst->n_planes = bufs[i].n_planes;
		dst->alpha = cust_src->alpha;
		dst->dst_x = cust_src->dst_x;
		dst->dst_y = cust_src->dst_y;
		dst->dst_w = cust_src->dst_w;
		dst->dst_h = cust_src->dst_h;
		dst->format = bufs[i].format;
		dst->blending = cust_src->blending;
		dst->rotation = cust_src->rotation;
		dst->scale = cust_src->scale;
		dst->compression = cust_src->compression;

		if (display_mode == SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE ||
		    display_mode == SPRD_DOUBLE_DISPC_SAME_CONTENT) {
			memcpy((dst + post->n_bufs), dst,
			       sizeof(struct sprd_adf_hwlayer));
			(dst + post->n_bufs)->hwlayer_id =
			    cust_src->hwlayer_id + (SPRD_N_LAYER >> 1);
		}

		if (display_mode == SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE) {
			dst->dst_h = (data->hwlayers[i].dst_h >> 1);
			(dst + post->n_bufs)->dst_y =
			    cust_src->dst_y + (cust_src->dst_h >> 1);
			(dst + post->n_bufs)->dst_h = (cust_src->dst_h >> 1);
		}

		for (j = 0; j < bufs[i].n_planes; j++) {
			dst->pitch[j] = bufs[i].pitch[j];
			dst->iova_plane[j] = bufs[i].offset[j];
			if (display_mode ==
					SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE) {
				(dst + post->n_bufs)->pitch[j] =
				    bufs[i].pitch[j];
				(dst + post->n_bufs)->iova_plane[j] =
				    dst->iova_plane[j] +
				    bufs[i].pitch[j] * (cust_src->dst_h >> 1);
			} else if (display_mode ==
				   SPRD_DOUBLE_DISPC_SAME_CONTENT) {
				(dst + post->n_bufs)->pitch[j] =
				    bufs[i].pitch[j];
				(dst + post->n_bufs)->iova_plane[j] =
				    dst->iova_plane[j];
			}
		}
	}

	return config;

error_out:
	return NULL;
}

void sprd_adf_free_restruct_config(struct sprd_restruct_config
				*config)
{
	ADF_DEBUG_GENERAL("entern\n");
	kfree(config);
}

void sprd_adf_wait_flip_done(struct sprd_adf_interface *adf_intf)
{
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	unsigned long flag;
	int num_dev = pipe->n_sdev;

	for (index = 0; index < num_dev; index++) {
		PRINT("dispc%d\n", index);
		dev = pipe->sdev[index];
		dev->ctrl->wait_for_sync(dev);
	}
}

static void dump_config_info(struct sprd_restruct_config *config)
{
	size_t i, j;

	ADF_DEBUG_GENERAL("flip number_hwlayer = 0x%x\n",
			  config->number_hwlayer);
	for (i = 0; i < config->number_hwlayer; i++) {
		ADF_DEBUG_GENERAL("flip hwlayer_id = 0x%x;index = %zx;\n",
				  config->hwlayers[i].hwlayer_id, i);
		ADF_DEBUG_GENERAL("flip n_planes = 0x%x\n",
				  config->hwlayers[i].n_planes);
		ADF_DEBUG_GENERAL("flip alpha = 0x%x\n",
				  config->hwlayers[i].alpha);
		ADF_DEBUG_GENERAL("flip dst_x = 0x%x\n",
				  config->hwlayers[i].dst_x);
		ADF_DEBUG_GENERAL("flip dst_y = 0x%x\n",
				  config->hwlayers[i].dst_y);
		ADF_DEBUG_GENERAL("flip dst_w = 0x%x\n",
				  config->hwlayers[i].dst_w);
		ADF_DEBUG_GENERAL("flip dst_h = 0x%x\n",
				  config->hwlayers[i].dst_h);
		ADF_DEBUG_GENERAL("flip format = 0x%x\n",
				  config->hwlayers[i].format);
		ADF_DEBUG_GENERAL("flip blending = 0x%x\n",
				  config->hwlayers[i].blending);
		ADF_DEBUG_GENERAL("flip rotation = 0x%x\n",
				  config->hwlayers[i].rotation);
		ADF_DEBUG_GENERAL("flip scale = 0x%x\n",
				  config->hwlayers[i].scale);
		ADF_DEBUG_GENERAL("flip compression = 0x%x\n",
				  config->hwlayers[i].compression);

		for (j = 0; j < config->hwlayers[i].n_planes; j++) {
			ADF_DEBUG_GENERAL("flip pitch = 0x%x;index = %zx;\n",
					  config->hwlayers[i].pitch[j], j);
			ADF_DEBUG_GENERAL("flip iova_plane = 0x%x\n",
					  config->hwlayers[i].iova_plane[j]);
		}
	}
}

void sprd_adf_device_flip(struct sprd_adf_interface *adf_intf,
			  struct sprd_restruct_config *config)
{
	struct sprd_dispc_context *dispc_ctx;
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	int num_dev = pipe->n_sdev;
	dump_config_info(config);
	for (index = 0; index < num_dev; index++) {
		dev = pipe->sdev[index];
		dispc_ctx = dev->dispc_ctx;
		sprd_dispc_flip(dev, config);
	}
}

int32_t sprd_adf_shutdown(struct sprd_adf_interface *intf)
{
	ADF_DEBUG_GENERAL("entern\n");
	sprd_adf_early_suspend(intf);
	return 0;
}

int sprd_adf_get_screen_size(struct sprd_adf_interface *intf,
		u16 *width_mm, u16 *height_mm)
{
	ADF_DEBUG_GENERAL("entern\n");
	*width_mm = 245;
	*height_mm = 445;
	ADF_DEBUG_GENERAL("entern w = 245;h = 445\n");
	return 0;
}

int sprd_adf_get_preferred_mode(struct sprd_adf_interface *intf,
		struct drm_mode_modeinfo *mode)
{
	struct pipe *pipe;
	struct panel_if_device *panel_intf;
	struct panel_spec *panel;
	if (!mode) {
		ADF_DEBUG_WARN("para mode is illegal\n");
		goto out;
	}

	pipe = (struct pipe *)intf->data;
	panel_intf = pipe->sdev[0]->intf;
	panel = panel_intf->get_panel(panel_intf);
	mode->hdisplay = panel->width;
	mode->vdisplay = panel->height;
	mode->vrefresh = panel->fps * 1000;

out:
	return 0;
}

int sprd_adf_interface_dpms_on(struct sprd_adf_interface *intf)
{
	PRINT("Reserved function\n");
	return 0;
}

int sprd_adf_interface_dpms_off(struct sprd_adf_interface *intf)
{
	PRINT("Reserved function\n");
	return 0;
}

void sprd_adf_enable_vsync_irq(struct sprd_adf_interface *intf)
{
	PRINT("Reserved function\n");
}

void sprd_adf_disable_vsync_irq(struct sprd_adf_interface *adf_intf)
{
	PRINT("Reserved function\n");
}

int32_t sprd_adf_interface_init(struct sprd_adf_interface *adf_intf)
{
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	int num_dev = pipe->n_sdev;
	for (index = 0; index < num_dev; index++) {
		dev = pipe->sdev[index];
		dev->ctrl->logo_proc(dev);
		dev->ctrl->early_init(dev);
		dev->ctrl->init(dev);
	}
	return 0;
}

int32_t sprd_adf_interface_uninit(struct sprd_adf_interface *adf_intf)
{
	struct sprd_device *dev;
	struct pipe *pipe = (struct pipe *)(adf_intf->data);
	int index;
	int num_dev = pipe->n_sdev;
	for (index = 0; index < num_dev; index++) {
		PRINT("dispc%d\n", index);
		dev = pipe->sdev[index];
		dev->ctrl->uninit(dev);
		kfree(dev->dispc_ctx);
		kfree(dev);
	}
	return 0;
}

int pipe_config(struct pipe *pipe, int display_mode, int interface_id,
		struct platform_device *pdev)
{
	switch (display_mode) {
	case SPRD_SINGLE_DISPC:
		PRINT("interface_id = %d\n", interface_id);
		pipe->n_sdev = 1;
		pipe->sdev[0] = sprd_device_init(pdev, 0);
		break;
	case SPRD_DOUBLE_DISPC_MASTER_AND_SLAVE:
		PRINT("interface_id = %d\n", interface_id);
		pipe->n_sdev = 2;
		pipe->sdev[0] = sprd_device_init(pdev, 0);
		pipe->sdev[1] = sprd_device_init(pdev, 1);
		break;
	case SPRD_DOUBLE_DISPC_SAME_CONTENT:
		PRINT("interface_id = %d\n", interface_id);
		pipe->n_sdev = 2;
		pipe->sdev[0] = sprd_device_init(pdev, 0);
		pipe->sdev[1] = sprd_device_init(pdev, 1);
		break;
	case SPRD_DOUBLE_DISPC_INDEPENDENT_CONTENT:
		PRINT("interface_id = %d\n", interface_id);
		pipe->n_sdev = 1;
		pipe->sdev[interface_id] =
			sprd_device_init(pdev, interface_id);
		break;
	default:
		return -1;
	}
	return 0;
}

static struct sprd_device *sprd_device_init(struct platform_device *pdev,
						int index)
{

	int ret;
	char *p;
	struct resource r;
	struct sprd_device *dev;
	struct sprd_dispc_context *dispc_ctx;
	dev = kzalloc(sizeof(struct sprd_device), GFP_KERNEL);
	if (dev == NULL) {
		PRINT("error alloc sprd_device\n");
		return NULL;
	}
	dev->pdev = pdev;
	dev->of_dev = &(pdev->dev);
	dev->dev_id = index;
	if ((SPRDFB_MAINLCD_ID != dev->dev_id)
	    && (SPRDFB_SUBLCD_ID != dev->dev_id)) {
		PERROR("sprd_probe fail. (unsupported device id)\n");
		goto err0;
	}
	PERROR("sprd_get_chip_id = 0x08%x\n" , sprd_get_chip_id());
	switch (SPRDFB_IN_DATA_TYPE) {
	case SPRD_IN_DATA_TYPE_ABGR888:
		dev->bpp = 32;
		break;
	case SPRD_IN_DATA_TYPE_BGR565:
		dev->bpp = 16;
		break;
	default:
		dev->bpp = 32;
		break;
	}

	dev->ctrl = &sprd_dispc_ctrl;
	if (0 != dc_connect_interface(dev, dev->dev_id))
		BUG();
	dev->panel_ready = dev->intf->panel_ready;
	dev->panel_if_type = dev->intf->panel_if_type;
	DBG(dev->panel_if_type);
	dev->logo_buffer_addr_v = 0;

	p = kzalloc(sizeof(struct sprd_dispc_context), GFP_KERNEL);
	if (!p)
		BUG();
	dispc_ctx = (struct sprd_dispc_context *)p;
	dispc_ctx->dev = dev;
	dev->dispc_ctx = dispc_ctx;
	if (0 != of_address_to_resource(pdev->dev.of_node, index, &r)) {
		PRINT("sprd_probe fail. (can't get register base address)\n");
		ret = -ENODEV;
		goto err0;
	}
	dispc_ctx->base.address =
		(unsigned long)ioremap_nocache(r.start, resource_size(&r));
	if (!dispc_ctx->base.address)
		BUG();
	if (index == 0)
		g_dispc_base_addr = dispc_ctx->base.address;
	PRINT("set dispc_ctx->base.address = 0x%lx\n", dispc_ctx->base.address);

	return dev;
err0:
	dev_err(&pdev->dev, "failed to probe sprd\n");
	return NULL;
}
