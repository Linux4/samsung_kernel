/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <video/ion_sprd.h>
#include "gsp_kcfg.h"
#include "gsp_drv_common.h"
#include "gsp_config_if.h"



void gsp_kcfg_init(struct gsp_kcfg_info *kcfg)
{
	if (kcfg == NULL) {
		GSP_ERR("kcfg init null pointer\n");
		return;
	}
	memset(&kcfg->cfg, 0, sizeof(struct gsp_cfg_info));
	memset(&kcfg->data, 0, sizeof(struct gsp_fence_data));
	INIT_LIST_HEAD(&kcfg->kcfg_list);
	kcfg->done = 0;
}

void gsp_kcfg_scale_set(struct gsp_kcfg_info *kcfg)
{
	struct gsp_cfg_info *cfg = &kcfg->cfg;
	if (NULL == cfg) {
		GSP_ERR("scale set info is NULL\n");
		return;
	}

	if (cfg->layer0_info.layer_en == 1) {
		if(cfg->layer0_info.rot_angle & 0x1) { /*90 or 270 degree*/
			if((cfg->layer0_info.clip_rect.rect_w != cfg->layer0_info.des_rect.rect_h)
			|| (cfg->layer0_info.clip_rect.rect_h != cfg->layer0_info.des_rect.rect_w)) {
				cfg->layer0_info.scaling_en = 1;
			}
		} else { /* 0 degree*/
			if((cfg->layer0_info.clip_rect.rect_w != cfg->layer0_info.des_rect.rect_w)
			|| (cfg->layer0_info.clip_rect.rect_h != cfg->layer0_info.des_rect.rect_h)) {
				cfg->layer0_info.scaling_en = 1;
			}
		}
	}
}

struct dma_buf *gsp_get_dmabuf(int fd)
{
	struct dma_buf *dmabuf = NULL;

	GSP_DEBUG("%s, fd: %d\n", __func__, fd);
	if (fd < 0) {
		GSP_ERR("%s, fd=%d less than zero\n", __func__, fd);
		return dmabuf;
	}

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		GSP_ERR("%s, dmabuf=0x%lx dma_buf_get error!\n",
			__func__, (unsigned long)dmabuf);
		return NULL;
	}

	return dmabuf;
}

void gsp_put_dmabuf(struct dma_buf *buf)
{
	dma_buf_put(buf);
}

static int gsp_kcfg_dmabuf_set(struct gsp_cfg_info *cfg)
{
#define gsp_layer_dmabuf_get(lx_info)\
	do {\
		fd = (lx_info).mem_info.share_fd;\
		if (fd > 0 && (lx_info).layer_en)\
			dmabuf = gsp_get_dmabuf(fd);\
		else\
			break;\
		if (dmabuf == NULL)\
			return -1;\
		(lx_info).mem_info.dmabuf = dmabuf;\
	} while(0)
	int fd = -1;
	struct dma_buf *dmabuf = NULL;
	gsp_layer_dmabuf_get(cfg->layer0_info);
	gsp_layer_dmabuf_get(cfg->layer1_info);
	gsp_layer_dmabuf_get(cfg->layer_des_info);
	return 0;
}
int gsp_kcfg_fill(struct gsp_cfg_info __user *ucfg,
					struct gsp_kcfg_info *kcfg,
					struct gsp_sync_timeline *tl,
					uint32_t frame_id)
{
	struct gsp_fence_data *data = NULL;
	struct gsp_layer0_cfg_info *img_layer = NULL;
	struct gsp_layer1_cfg_info *osd_layer = NULL;
	int ret = 0;
	int async_flag = 0;
	if (ucfg == NULL || kcfg == NULL) {
		GSP_ERR("kcfg fill null pointer\n");
		return -1;
	}
	gsp_kcfg_init(kcfg);
	ret = copy_from_user((void*)&kcfg->cfg, (void*)ucfg,
			     sizeof(struct gsp_cfg_info));
	if (ret) {
		GSP_ERR("kcfg fill copy from user error\n");
		return -1;
	}
	gsp_kcfg_scale_set(kcfg);
	ret = gsp_kcfg_dmabuf_set(&kcfg->cfg);
	if (ret) {
		GSP_ERR("dmabuf set error\n");
		return -1;
	}

	kcfg->cfg.misc_info.gsp_clock = GSP_CLOCK;
	kcfg->cfg.misc_info.gsp_gap = GSP_GAP;
	kcfg->frame_id = frame_id;
	kcfg->mmu_id = ION_GSP;

	async_flag = kcfg->cfg.misc_info.async_flag;
	if (async_flag) {
		data = &kcfg->data;

		img_layer = &kcfg->cfg.layer0_info;
		if (img_layer->layer_en == 1
			&& img_layer->pallet_en == 0)
			data->enable.img_en = 1;

		osd_layer = &kcfg->cfg.layer1_info;
		if (osd_layer->layer_en == 1
			&& osd_layer->pallet_en == 0)
			data->enable.osd_en = 1;

		data->des_sig_ufd = &(ucfg->layer_des_info).sig_fd;

		data->img_wait_fd = img_layer->wait_fd;
		data->osd_wait_fd = osd_layer->wait_fd;
		data->des_wait_fd = kcfg->cfg.layer_des_info.wait_fd;

		ret = gsp_layer_list_fence_process(data, tl);
	}
	return ret;
}

void gsp_kcfg_destroy(struct gsp_kcfg_info *kcfg)
{
	if (kcfg->cfg.misc_info.async_flag)
		gsp_layer_list_fence_free(&kcfg->data);
}

int gsp_kcfg_iommu_map(struct gsp_kcfg_info *kcfg)
{
	struct ion_addr_data data;

	memset(&data, 0, sizeof(data));
#define GSP_LAYER_MAP(lx_info, name, id)\
	do {\
		if ((lx_info).layer_en == 1\
			&& (lx_info).src_addr.addr_y == 0\
			&& (lx_info).mem_info.share_fd > 0) {\
			data.dmabuf = (lx_info).mem_info.dmabuf;\
			data.fd_buffer = -1;\
			if(sprd_ion_get_addr(id, &data)) {\
				GSP_ERR("%s map failed!\n", (name));\
				return -1;\
			}\
			GSP_DEBUG("map: %s\n", (name));\
			if(data.iova_enabled) {\
				(lx_info).src_addr.addr_y = data.iova_addr;\
				GSP_DEBUG("dma buf address: %lu\n", data.iova_addr);\
			} else {\
				(lx_info).src_addr.addr_y = data.phys_addr;\
				GSP_DEBUG("dma buf address: %lu\n", data.iova_addr);\
			}\
			(lx_info).src_addr.addr_uv= (lx_info).src_addr.addr_y + (lx_info).mem_info.uv_offset;\
			(lx_info).src_addr.addr_v= (lx_info).src_addr.addr_y + (lx_info).mem_info.v_offset;\
		}\
	} while (0)

    GSP_LAYER_MAP(kcfg->cfg.layer0_info, "img_layer",
		  kcfg->mmu_id);
    GSP_LAYER_MAP(kcfg->cfg.layer1_info, "osd_layer",
		  kcfg->mmu_id);
    GSP_LAYER_MAP(kcfg->cfg.layer_des_info, "des_layer",
		  kcfg->mmu_id);

    return 0;
}

int gsp_kcfg_iommu_unmap(struct gsp_kcfg_info *kcfg)
{
	struct ion_addr_data data;

	memset(&data, 0, sizeof(data));
#define GSP_LAYER_UNMAP(lx_info, id)\
	if((lx_info).mem_info.share_fd > 0\
		&& (lx_info).layer_en == 1) {\
		GSP_DEBUG("unmap fd: %d\n", (lx_info).mem_info.share_fd);\
		data.dmabuf = (lx_info).mem_info.dmabuf;\
		GSP_DEBUG("unmap dmabuf: %p\n", data.dmabuf);\
		if (data.dmabuf) {\
			data.fd_buffer = -1;\
			sprd_ion_free_addr(kcfg->mmu_id, &data);\
			gsp_put_dmabuf(data.dmabuf);\
		}\
		else {\
			GSP_ERR("dmabuf has been released");\
		}\
	}
    GSP_LAYER_UNMAP(kcfg->cfg.layer0_info, kcfg->mmu_id);
    GSP_LAYER_UNMAP(kcfg->cfg.layer1_info, kcfg->mmu_id);
    GSP_LAYER_UNMAP(kcfg->cfg.layer_des_info, kcfg->mmu_id);
    return 0;
}

/*
func:GSP_Info_Config
desc:config info to register
*/
uint32_t gsp_kcfg_reg_set(struct gsp_kcfg_info *kcfg,
		ulong reg_addr)
{
    gsp_config_layer(GSP_MODULE_LAYER0, kcfg, reg_addr);
    gsp_config_layer(GSP_MODULE_LAYER1, kcfg, reg_addr);
    gsp_config_layer(GSP_MODULE_ID_MAX, kcfg, reg_addr);
    gsp_config_layer(GSP_MODULE_DST, kcfg, reg_addr);
    return GSP_ERRCODE_GET(reg_addr);
}

void gsp_current_kcfg_set(struct gsp_context *ctx,
		struct gsp_kcfg_info *kcfg)
{
	int ret = -1;
	if (ctx == NULL) {
		GSP_ERR("kcfg set null pointer\n");
		return;
	}

	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
	if (ret) {
		GSP_ERR("get an error gsp context\n");
		return;
	}

	ctx->current_kcfg = kcfg;
}

struct gsp_kcfg_info *gsp_current_kcfg_get(struct gsp_context *ctx)
{
	int ret = -1;
	if (ctx == NULL) {
		GSP_ERR("current kcfg get from null pointer\n");
		return NULL;
	}

	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
	if (ret) {
		GSP_ERR("get an error gsp context\n");
		return NULL;
	}

	return ctx->current_kcfg;
}

uint32_t gsp_kcfg_get_des_addr(struct gsp_kcfg_info *kcfg)
{
	if (kcfg == NULL)
		GSP_ERR("can't get des addr from null kcfg\n");

	return kcfg->cfg.layer_des_info.src_addr.addr_y;
}
