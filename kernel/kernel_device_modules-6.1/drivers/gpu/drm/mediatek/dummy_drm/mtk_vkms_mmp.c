// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <linux/dma-buf.h>

#include "mtk_vkms_drv.h"

MODULE_IMPORT_NS(DMA_BUF);
#define DISP_REG_OVL_L0_PITCH (0x044UL)
#define L_PITCH_FLD_SRC_PITCH REG_FLD_MSB_LSB(15, 0)

static struct DRM_MMP_Events g_DRM_MMP_Events;
static struct CRTC_MMP_Events g_CRTC_MMP_Events[MMP_CRTC_NUM];

/* need to update if add new mmp_event in DRM_MMP_Events */
void init_drm_mmp_event(void)
{
	int i;

	if (g_DRM_MMP_Events.drm)
		return;

	g_DRM_MMP_Events.drm = mmprofile_register_event(MMP_ROOT_EVENT, "DRM");

	/* init DRM mmp events */
	g_DRM_MMP_Events.IRQ =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "IRQ");
	g_DRM_MMP_Events.ovl =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "OVL");
	g_DRM_MMP_Events.ovl0 =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL0");
	g_DRM_MMP_Events.ovl1 =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL1");
	g_DRM_MMP_Events.ovl0_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL0_2L");
	g_DRM_MMP_Events.ovl1_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL1_2L");
	g_DRM_MMP_Events.ovl2_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL2_2L");
	g_DRM_MMP_Events.ovl3_2l =
		mmprofile_register_event(g_DRM_MMP_Events.ovl, "OVL3_2L");
	g_DRM_MMP_Events.rdma =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "RDMA");
	g_DRM_MMP_Events.rdma0 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA0");
	g_DRM_MMP_Events.rdma1 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA1");
	g_DRM_MMP_Events.rdma2 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA2");
	g_DRM_MMP_Events.rdma3 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA3");
	g_DRM_MMP_Events.rdma4 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA4");
	g_DRM_MMP_Events.rdma5 =
		mmprofile_register_event(g_DRM_MMP_Events.rdma, "RDMA5");
	g_DRM_MMP_Events.wdma =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "WDMA");
	g_DRM_MMP_Events.wdma0 =
		mmprofile_register_event(g_DRM_MMP_Events.wdma, "WDMA0");
	g_DRM_MMP_Events.dsi =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "DSI");
	g_DRM_MMP_Events.dsi0 =
		mmprofile_register_event(g_DRM_MMP_Events.dsi, "DSI0");
	g_DRM_MMP_Events.dsi1 =
		mmprofile_register_event(g_DRM_MMP_Events.dsi, "DSI1");
	g_DRM_MMP_Events.aal =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "AAL");
	g_DRM_MMP_Events.aal0 =
		mmprofile_register_event(g_DRM_MMP_Events.aal, "AAL0");
	g_DRM_MMP_Events.aal1 =
		mmprofile_register_event(g_DRM_MMP_Events.aal, "AAL1");
	g_DRM_MMP_Events.pmqos =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "PMQOS");
	g_DRM_MMP_Events.hrt_bw =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "HRT_BW");
	g_DRM_MMP_Events.layering =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "HRT");
	g_DRM_MMP_Events.layering_blob =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "HRT_BLOB");
	g_DRM_MMP_Events.mutex_lock =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "LOCK");
	g_DRM_MMP_Events.dma_alloc =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_ALLOC");
	g_DRM_MMP_Events.dma_free =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_FREE");
	g_DRM_MMP_Events.dma_get =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_GET");
	g_DRM_MMP_Events.dma_put =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "D_PUT");
	g_DRM_MMP_Events.ion_import_dma =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_DMA");
	g_DRM_MMP_Events.ion_import_fd =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_FD");
	g_DRM_MMP_Events.ion_import_free =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "I_FREE");
	g_DRM_MMP_Events.set_mode =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "SET_MODE");
	g_DRM_MMP_Events.top_clk =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "TOP_CLK");
	g_DRM_MMP_Events.ddp =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "MUTEX");
	g_DRM_MMP_Events.sram_alloc =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "S_ALLOC");
	g_DRM_MMP_Events.sram_free =
		mmprofile_register_event(g_DRM_MMP_Events.drm, "S_FREE");
	for (i = 0; i < 4; i++) {
		char name[32];

		snprintf(name, sizeof(name), "MUTEX%d", i);
		g_DRM_MMP_Events.mutex[i] =
			mmprofile_register_event(g_DRM_MMP_Events.ddp, name);
	}

	g_DRM_MMP_Events.postmask =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "POSTMASK");
	g_DRM_MMP_Events.postmask0 = mmprofile_register_event(
		g_DRM_MMP_Events.postmask, "POSTMASK0");
	g_DRM_MMP_Events.abnormal_irq =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "ABNORMAL_IRQ");
	g_DRM_MMP_Events.iova_tf =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "IOVA_TF");
	g_DRM_MMP_Events.dp_intf0 =
		mmprofile_register_event(g_DRM_MMP_Events.IRQ, "dp_intf0");
}

/* need to update if add new mmp_event in CRTC_MMP_Events */
void init_crtc_mmp_event(void)
{
	int i = 0;
	int r = 0;

	for (i = 0; i < MMP_CRTC_NUM; i++) {
		char name[32];
		mmp_event crtc_mmp_root;

		/* create i th root of CRTC mmp events */
		r = snprintf(name, sizeof(name), "crtc%d", i);
		if (r < 0) {
			/* Handle snprintf() error */
			DDPPR_ERR("%s:snprintf error\n", __func__);
			return;
		}
		crtc_mmp_root =
			mmprofile_register_event(g_DRM_MMP_Events.drm, name);
		g_DRM_MMP_Events.crtc[i] = crtc_mmp_root;

		/* init CRTC mmp events */
		g_CRTC_MMP_Events[i].trig_loop_done = mmprofile_register_event(
			crtc_mmp_root, "trig_loop_done");
		g_CRTC_MMP_Events[i].enable =
			mmprofile_register_event(crtc_mmp_root, "enable");
		g_CRTC_MMP_Events[i].disable =
			mmprofile_register_event(crtc_mmp_root, "disable");
		g_CRTC_MMP_Events[i].release_fence = mmprofile_register_event(
			crtc_mmp_root, "release_fence");
		g_CRTC_MMP_Events[i].update_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"update_present_fence");
		g_CRTC_MMP_Events[i].release_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"release_present_fence");
		g_CRTC_MMP_Events[i].present_fence_timestamp_same =
			mmprofile_register_event(crtc_mmp_root,
				"present_fence_timestamp_same");
		g_CRTC_MMP_Events[i].present_fence_timestamp =
			mmprofile_register_event(crtc_mmp_root,
				"present_fence_timestamp");
		g_CRTC_MMP_Events[i].update_sf_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"update_sf_present_fence");
		g_CRTC_MMP_Events[i].release_sf_present_fence =
			mmprofile_register_event(crtc_mmp_root,
				"release_sf_present_fence");
		g_CRTC_MMP_Events[i].warn_sf_pf_0 =
			mmprofile_register_event(crtc_mmp_root, "warn_sf_pf_0");
		g_CRTC_MMP_Events[i].warn_sf_pf_2 =
			mmprofile_register_event(crtc_mmp_root, "warn_sf_pf_2");
		g_CRTC_MMP_Events[i].atomic_begin = mmprofile_register_event(
			crtc_mmp_root, "atomic_begin");
		g_CRTC_MMP_Events[i].atomic_flush = mmprofile_register_event(
			crtc_mmp_root, "atomic_flush");
		g_CRTC_MMP_Events[i].enable_vblank = mmprofile_register_event(
			crtc_mmp_root, "enable_vblank");
		g_CRTC_MMP_Events[i].disable_vblank = mmprofile_register_event(
			crtc_mmp_root, "disable_vblank");
		g_CRTC_MMP_Events[i].esd_check =
			mmprofile_register_event(crtc_mmp_root, "ESD check");
		g_CRTC_MMP_Events[i].esd_recovery =
			mmprofile_register_event(crtc_mmp_root, "ESD recovery");
		g_CRTC_MMP_Events[i].leave_idle = mmprofile_register_event(
			crtc_mmp_root, "leave_idle");
		g_CRTC_MMP_Events[i].enter_idle = mmprofile_register_event(
			crtc_mmp_root, "enter_idle");
		g_CRTC_MMP_Events[i].frame_cfg =
			mmprofile_register_event(crtc_mmp_root, "frame cfg");
		g_CRTC_MMP_Events[i].suspend = mmprofile_register_event(
			crtc_mmp_root, "suspend");
		g_CRTC_MMP_Events[i].resume = mmprofile_register_event(
			crtc_mmp_root, "resume");
		g_CRTC_MMP_Events[i].dsi_suspend = mmprofile_register_event(
			crtc_mmp_root, "dsi_suspend");
		g_CRTC_MMP_Events[i].dsi_resume = mmprofile_register_event(
			crtc_mmp_root, "dsi_resume");
		g_CRTC_MMP_Events[i].backlight = mmprofile_register_event(
			crtc_mmp_root, "backlight");
		g_CRTC_MMP_Events[i].backlight_grp = mmprofile_register_event(
			crtc_mmp_root, "backlight_grp");
		g_CRTC_MMP_Events[i].ddic_send_cmd = mmprofile_register_event(
			crtc_mmp_root, "ddic_send_cmd");
		g_CRTC_MMP_Events[i].ddic_read_cmd = mmprofile_register_event(
			crtc_mmp_root, "ddic_read_cmd");
		g_CRTC_MMP_Events[i].path_switch = mmprofile_register_event(
			crtc_mmp_root, "path_switch");
		g_CRTC_MMP_Events[i].user_cmd = mmprofile_register_event(
			crtc_mmp_root, "user_cmd");
		g_CRTC_MMP_Events[i].check_trigger = mmprofile_register_event(
			crtc_mmp_root, "check_trigger");
		g_CRTC_MMP_Events[i].kick_trigger = mmprofile_register_event(
			crtc_mmp_root, "kick_trigger");
		g_CRTC_MMP_Events[i].atomic_commit = mmprofile_register_event(
			crtc_mmp_root, "atomic_commit");
		g_CRTC_MMP_Events[i].user_cmd_cb =
			mmprofile_register_event(crtc_mmp_root, "user_cmd_cb");
		g_CRTC_MMP_Events[i].bl_cb =
			mmprofile_register_event(crtc_mmp_root, "bl_cb");
		g_CRTC_MMP_Events[i].clk_change = mmprofile_register_event(
			crtc_mmp_root, "clk_change");
		g_CRTC_MMP_Events[i].layerBmpDump =
					mmprofile_register_event(
					crtc_mmp_root, "LayerBmpDump");
		g_CRTC_MMP_Events[i].layer_dump[0] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer0_dump");
		g_CRTC_MMP_Events[i].layer_dump[1] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer1_dump");
		g_CRTC_MMP_Events[i].layer_dump[2] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer2_dump");
		g_CRTC_MMP_Events[i].layer_dump[3] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer3_dump");
		g_CRTC_MMP_Events[i].layer_dump[4] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer4_dump");
		g_CRTC_MMP_Events[i].layer_dump[5] =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].layerBmpDump,
					"layer5_dump");
		g_CRTC_MMP_Events[i].wbBmpDump =
			mmprofile_register_event(crtc_mmp_root, "wbBmpDump");
		g_CRTC_MMP_Events[i].wb_dump =
			mmprofile_register_event(g_CRTC_MMP_Events[i].wbBmpDump, "wb_dump");
		g_CRTC_MMP_Events[i].cwbBmpDump =
					mmprofile_register_event(
					crtc_mmp_root, "CwbBmpDump");
		g_CRTC_MMP_Events[i].cwb_dump =
					mmprofile_register_event(
					g_CRTC_MMP_Events[i].cwbBmpDump,
					"cwb_dump");
		/*Msync 2.0 mmp start*/
		g_CRTC_MMP_Events[i].ovl_status_err =
			mmprofile_register_event(crtc_mmp_root, "ovl_status_err");
		g_CRTC_MMP_Events[i].vfp_period =
			mmprofile_register_event(crtc_mmp_root, "vfp_period");
		g_CRTC_MMP_Events[i].not_vfp_period =
			mmprofile_register_event(crtc_mmp_root, "not_vfp_period");
		g_CRTC_MMP_Events[i].dsi_state_dbg7 =
			mmprofile_register_event(crtc_mmp_root, "dsi_state_dbg7");
		g_CRTC_MMP_Events[i].dsi_dbg7_after_sof =
			mmprofile_register_event(crtc_mmp_root, "dsi_dbg7_after_sof");
		g_CRTC_MMP_Events[i].msync_enable =
			mmprofile_register_event(crtc_mmp_root, "msync_enable");
		/*Msync 2.0 mmp end*/
		g_CRTC_MMP_Events[i].mode_switch = mmprofile_register_event(
			crtc_mmp_root, "mode_switch");
		g_CRTC_MMP_Events[i].ddp_clk = mmprofile_register_event(
			crtc_mmp_root, "ddp_clk");
		/*AAL MMP MARK*/
		g_CRTC_MMP_Events[i].aal_sof_thread = mmprofile_register_event(
			crtc_mmp_root, "aal_sof_thread");
		g_CRTC_MMP_Events[i].aal_dre30_rw = mmprofile_register_event(
			crtc_mmp_root, "aal_dre30_rw");
		g_CRTC_MMP_Events[i].aal_dre20_rh = mmprofile_register_event(
			crtc_mmp_root, "aal_dre20_rh");
		/*Gamma MMP MARK*/
		g_CRTC_MMP_Events[i].gamma_ioctl = mmprofile_register_event(
			crtc_mmp_root, "gamma_ioctl");
		g_CRTC_MMP_Events[i].gamma_sof = mmprofile_register_event(
			crtc_mmp_root, "gamma_sof");
	}
}
void drm_mmp_init(void)
{
	mmprofile_enable(1);
	DDPMSG("%s:%d\n", __func__, __LINE__);
	/* init mmp events */
	init_drm_mmp_event();
	init_crtc_mmp_event();

	/* enable all mmp events */
	mmprofile_enable_event_recursive(g_DRM_MMP_Events.drm, 1);

	mmprofile_start(1);
}

struct DRM_MMP_Events *get_drm_mmp_events(void)
{
	return &g_DRM_MMP_Events;
}

struct CRTC_MMP_Events *get_crtc_mmp_events(unsigned long id)
{
	return &g_CRTC_MMP_Events[id];
}

struct drm_gem_object *mtk_fb_get_gem_obj(struct drm_framebuffer *fb)
{
	struct mtk_drm_fb *mtk_fb = to_mtk_fb(fb);

	return mtk_fb->gem_obj;
}

void *mtk_drm_buffer_map_kernel(struct drm_framebuffer *fb, struct iosys_map *map)
{
	struct drm_gem_object *gem_obj = NULL;
	struct dma_buf *dmabuf = NULL;
	struct mtk_drm_gem_obj *mtk_gem = NULL;
	void *dma_va = NULL;
	int ret = 0;

	if (!fb) {
		DDPINFO("[MMP]fb is null\n");
		return 0;
	}

	gem_obj = mtk_fb_get_gem_obj(fb);
	if (!gem_obj) {
		DDPINFO("[MMP]gem is null\n");
		return 0;
	}
	mtk_gem = to_mtk_gem_obj(gem_obj);
	if (!mtk_gem->cookie)
		DDPINFO("%s, [MMP]cookie is null\n", __func__);
	else
		DDPINFO("[TEST_drm]%s:%d,mtk_gem->cookie is %p\n", __func__, __LINE__,
			mtk_gem->cookie);

	if (!mtk_gem->dma_addr)
		DDPINFO("%s, [MMP]dma_addr is null\n", __func__);
	else
		DDPINFO("[TEST_drm]%s:%d,mtk_gem->dma_addr is %pad\n", __func__, __LINE__,
			&mtk_gem->dma_addr);

	dmabuf = gem_obj->dma_buf;
	if (!dmabuf) {
		DDPINFO("[MMP]dmabuf is null\n");
		return 0;
	}
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);

	ret = dma_buf_vmap(dmabuf, map);
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	if (ret) {
		DDPINFO("%s, [MMP]get dma_va fail,ret %d\n", __func__, ret);
		return 0;
	}

	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	dma_va = map->vaddr;

	return dma_va;
}

int mtk_drm_buffer_unmap_kernel(struct drm_framebuffer *fb, struct iosys_map *map)
{
	struct drm_gem_object *gem_obj;
	struct dma_buf *dmabuf;

	gem_obj = mtk_fb_get_gem_obj(fb);
	dmabuf = gem_obj->dma_buf;

	dma_buf_vunmap(dmabuf, map);

	return 0;
}

int mmp_fb_to_bitmap(struct drm_framebuffer *fb)
{
	struct mmp_metadata_bitmap_t bitmap;
	mmp_event *event_base = NULL;
	struct drm_gem_object *gem_obj = NULL;
	struct mtk_drm_gem_obj *mtk_gem = NULL;
	int i = 0;
	void *dma_va;
	struct iosys_map map;

	CRTC_MMP_EVENT_START(0, layerBmpDump, 0, 0);

	memset(&bitmap, 0, sizeof(struct mmp_metadata_bitmap_t));

	bitmap.format = MMPROFILE_BITMAP_RGBA8888;
	bitmap.bpp = 32;
	bitmap.data1 = 0;
	bitmap.width = (default_mode.hdisplay / 32 + 1) * 32;
	bitmap.height = default_mode.vdisplay;

	bitmap.pitch = bitmap.width * 4;
	bitmap.start_pos = 0;
	bitmap.data_size = bitmap.pitch * bitmap.height;
	bitmap.down_sample_x = 10;
	bitmap.down_sample_y = 10;

	gem_obj = mtk_fb_get_gem_obj(fb);
	if (!gem_obj) {
		DDPINFO("[MMP]%s gem is null\n", __func__);
		return 0;
	}
	mtk_gem = to_mtk_gem_obj(gem_obj);

	if (!mtk_gem->cookie) {
		DDPINFO("[MMP]%s cookie is null\n", __func__);
		return -1;
	}
	DDPINFO("[TEST_drm]%s:%d,mtk_gem->cookie is %p\n", __func__, __LINE__, mtk_gem->cookie);
	dma_va = mtk_drm_buffer_map_kernel(fb, &map);

	if (dma_va == NULL) {
		CRTC_MMP_EVENT_END(0, layerBmpDump, 0, 0);
		return 0;
	}

	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	bitmap.p_data = (void *)(dma_va);
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	for (i = 0; i < (0); i = i + 4)
		DDPMSG("0x%x, 0x%x, 0x%x, 0x%x\n",
			*((char *)dma_va+i), *((char *)dma_va+i+1),
			*((char *)dma_va+i+2), *((char *)dma_va+i+3));
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);

	event_base = g_CRTC_MMP_Events[0].layer_dump;
	DDPINFO("[TEST_drm]%s:%d\n", __func__, __LINE__);
	if (event_base) {
		DDPMSG("%s:%d\n", __func__, __LINE__);
		mmprofile_log_meta_bitmap(
		event_base[0],
		MMPROFILE_FLAG_PULSE,
		&bitmap);
	}

	mtk_drm_buffer_unmap_kernel(fb, &map);
	DDPMSG("%s:%d\n", __func__, __LINE__);
	return 0;
}

