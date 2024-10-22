// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_disp_vidle.h"
//#include "mtk_drm_ddp_comp.h"
//#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "platform/mtk_drm_platform.h"

static atomic_t g_ff_enabled = ATOMIC_INIT(0);
static bool vidle_paused;

struct mtk_disp_vidle_para mtk_disp_vidle_flag = {
	0,	/* vidle_en */
	0,	/* vidle_init */
	0,	/* vidle_stop */
	0,	/* rc_en */
	0,	/* wdt_en */
};

struct dpc_funcs disp_dpc_driver;
struct mtk_vdisp_funcs vdisp_func;

static atomic_t g_vidle_pq_ref = ATOMIC_INIT(0);
static DEFINE_MUTEX(g_vidle_pq_ref_lock);

struct mtk_disp_vidle {
	u8 level;
	u32 hrt_bw;
	u32 srt_bw;
	u32 te_duration;
	u32 vb_duration;
	enum mtk_panel_type panel_type;
};

static struct mtk_disp_vidle vidle_data = {
	.level = 0xff,
	.hrt_bw = 0xffffffff,
	.srt_bw = 0xffffffff,
	.te_duration = 0xffffffff,
	.vb_duration = 0xffffffff,
	.panel_type = PANEL_TYPE_COUNT,
};

void mtk_vidle_flag_init(void *_crtc)
{
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_crtc *crtc = NULL;
	enum mtk_panel_type type = PANEL_TYPE_COUNT;

	mtk_disp_vidle_flag.vidle_en = 0;

	if (_crtc == NULL)
		return;
	crtc = (struct drm_crtc *)_crtc;

	if (drm_crtc_index(crtc) != 0)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	priv = crtc->dev->dev_private;
	if (priv == NULL || output_comp == NULL) {
		DDPMSG("%s, invalid output\n", __func__);
		return;
	}

	if (!mtk_drm_lcm_is_connect(mtk_crtc)) {
		DDPMSG("%s, lcm is not connected\n", __func__);
		return;
	}

	if (mtk_dsi_is_cmd_mode(output_comp))
		type = PANEL_TYPE_CMD;
	else if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_VDO_PANEL))
		type = PANEL_TYPE_VDO;
	else {
		DDPMSG("%s, invalid panel type\n", __func__);
		return;
	}
	mtk_vidle_set_panel_type(type);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_TOP_EN))
		mtk_disp_vidle_flag.vidle_en |= DISP_VIDLE_TOP_EN;
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_MMINFRA_DT_EN))
		mtk_disp_vidle_flag.vidle_en |= DISP_VIDLE_MMINFRA_DT_EN;

	if (mtk_vidle_update_dt_by_type(crtc, type) < 0) {
		mtk_disp_vidle_flag.vidle_en = 0;
		DDPMSG("%s te duration is not set, disable vidle\n", __func__);
		return;
	}
}

void mtk_vidle_user_power_keep_by_gce(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt, u16 gpr)
{
	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (disp_dpc_driver.dpc_vidle_power_keep_by_gce)
		disp_dpc_driver.dpc_vidle_power_keep_by_gce(pkt, user, gpr);
}

void mtk_vidle_user_power_release_by_gce(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt)
{
	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (disp_dpc_driver.dpc_vidle_power_release_by_gce)
		disp_dpc_driver.dpc_vidle_power_release_by_gce(pkt, user);
}

int mtk_vidle_user_power_keep(enum mtk_vidle_voter_user user)
{
	if (!mtk_disp_vidle_flag.vidle_en)
		return 0;

	if (disp_dpc_driver.dpc_vidle_power_keep)
		return disp_dpc_driver.dpc_vidle_power_keep(user);

	return 0;
}

void mtk_vidle_user_power_release(enum mtk_vidle_voter_user user)
{
	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (disp_dpc_driver.dpc_vidle_power_release)
		disp_dpc_driver.dpc_vidle_power_release(user);
}

int mtk_vidle_pq_power_get(const char *caller)
{
	s32 ref;
	int pm_ret = 0;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_inc_return(&g_vidle_pq_ref);
	if (ref == 1) {
		pm_ret = mtk_vidle_user_power_keep(DISP_VIDLE_USER_PQ);
		if (pm_ret < 0) {
			DDPMSG("%s skipped, user %s\n", __func__, caller);
			ref = atomic_dec_return(&g_vidle_pq_ref);
		}
	}
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref < 0) {
		DDPPR_ERR("%s  get invalid cnt %d\n", caller, ref);
		return ref;
	}
	return pm_ret;
}

void mtk_vidle_pq_power_put(const char *caller)
{
	s32 ref;

	mutex_lock(&g_vidle_pq_ref_lock);
	ref = atomic_dec_return(&g_vidle_pq_ref);
	if (!ref)
		mtk_vidle_user_power_release(DISP_VIDLE_USER_PQ);
	mutex_unlock(&g_vidle_pq_ref_lock);
	if (ref < 0)
		DDPPR_ERR("%s  put invalid cnt %d\n", caller, ref);
}

void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en)
{
	mtk_disp_vidle_flag.rc_en = rc_en;
	/* TODO: action for mmdvfsrc_status_rc */
}

void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en)
{
	mtk_disp_vidle_flag.wdt_en = wdt_en;
	/* TODO: action for mmdvfsrc_status_wdt */
}

/* for debug only, DONT use in flow */
void mtk_vidle_set_all_flag(unsigned int en, unsigned int stop)
{
	mtk_disp_vidle_flag.vidle_en = en;
	mtk_disp_vidle_flag.vidle_stop = stop;
}

void mtk_vidle_get_all_flag(unsigned int *en, unsigned int *stop)
{
	*en = mtk_disp_vidle_flag.vidle_en;
	*stop = mtk_disp_vidle_flag.vidle_stop;
}

static void mtk_vidle_pause(bool en)
{
	if (disp_dpc_driver.dpc_pause == NULL)
		return;

	if (!mtk_vidle_is_ff_enabled())
		return;

	if (en && !vidle_paused) {
		CRTC_MMP_EVENT_START(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				vidle_data.te_duration);
		disp_dpc_driver.dpc_pause(DPC_SUBSYS_DISP, en);
		vidle_paused = true;
	} else if (!en && vidle_paused) {
		disp_dpc_driver.dpc_pause(DPC_SUBSYS_DISP, en);
		CRTC_MMP_EVENT_END(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				vidle_data.te_duration);
		vidle_paused = false;
	}
}

void mtk_set_vidle_stop_flag(unsigned int flag, unsigned int stop)
{
	unsigned int origignal_flag = mtk_disp_vidle_flag.vidle_stop;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	if (stop)
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop | flag;
	else
		mtk_disp_vidle_flag.vidle_stop =
			mtk_disp_vidle_flag.vidle_stop & ~flag;

	if (origignal_flag != mtk_disp_vidle_flag.vidle_stop)
		CRTC_MMP_MARK(0, pause_vidle, mtk_disp_vidle_flag.vidle_stop,
				mtk_vidle_is_ff_enabled());

	if (mtk_disp_vidle_flag.vidle_stop)
		mtk_vidle_pause(true);
	else
		mtk_vidle_pause(false);
}

static int mtk_set_dt_configure(unsigned int dur_frame, unsigned int dur_vblank)
{
	if (disp_dpc_driver.dpc_dt_set)
		return disp_dpc_driver.dpc_dt_set(dur_frame, dur_vblank);

	return 0;
}

int mtk_vidle_update_dt_by_period(void *_crtc, unsigned int dur_frame, unsigned int dur_vblank)
{
	struct drm_crtc *crtc = NULL;
	int ret = 0;

	if (!mtk_disp_vidle_flag.vidle_en) {
		DDPMSG("%s, invalid vidle_en:0x%x\n", __func__,
			mtk_disp_vidle_flag.vidle_en);
		return -1;
	}

	if (_crtc == NULL || dur_frame == 0) {
		DDPMSG("%s, invalid crtc, dur_frame:%uus\n", __func__, dur_frame);
		return -1;
	}

	crtc = (struct drm_crtc *)_crtc;
	if (drm_crtc_index(crtc) != 0) {
		DDPMSG("%s, invalid crtc id:%d\n", __func__,
			drm_crtc_index(crtc));
		return -1;
	}

	if (dur_frame == vidle_data.te_duration &&
		dur_vblank == vidle_data.vb_duration)
		return dur_frame;

	/* update DTs affected by TE dur_frame */
	ret = mtk_set_dt_configure(dur_frame, dur_vblank);
	vidle_data.te_duration = dur_frame;
	vidle_data.vb_duration = dur_vblank;

	if (ret < 0 &&
		!(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 1);
		DDPINFO("%s forbid vidle mode, dur_frame:%uus, dur_vb:%uus, flag:0x%x\n",
			__func__, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	} else if (ret >= 0 &&
		(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 0);
		DDPINFO("%s allow vidle mode, dur_frame:%uus, dur_vb:%uus, flag:0x%x\n",
			__func__, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	}

	return dur_frame;
}

int mtk_vidle_update_dt_by_type(void *_crtc, enum mtk_panel_type type)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;
	struct mtk_panel_params *panel_ext = NULL;
	unsigned int dur_frame = 0, dur_vblank = 0, fps = 0;
	struct mtk_drm_private *priv = NULL;
	int ret = 0;

	if (!mtk_disp_vidle_flag.vidle_en)
		return -1;

	if (_crtc == NULL) {
		DDPMSG("%s, invalid crtc\n", __func__);
		return -1;
	}

	crtc = (struct drm_crtc *)_crtc;
	if (drm_crtc_index(crtc) != 0)
		return -1;

	priv = crtc->dev->dev_private;
	if (priv == NULL) {
		DDPMSG("%s, invalid priv\n", __func__);
		return -1;
	}

	panel_ext = mtk_drm_get_lcm_ext_params(crtc);

	if (panel_ext && panel_ext->real_te_duration &&
		type == PANEL_TYPE_CMD) {
		dur_frame = panel_ext->real_te_duration;
		dur_vblank = 0;
	} else if (type == PANEL_TYPE_VDO &&
		mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_VDO_PANEL)) {
		fps = drm_mode_vrefresh(&crtc->state->adjusted_mode);
		mtk_crtc = to_mtk_crtc(crtc);
		comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (comp == NULL) {
			DDPMSG("%s, invalid output comp\n", __func__);
			return -2;
		}

		dur_frame = (fps == 0) ? 16666 : 1000000 / fps;
		mtk_ddp_comp_io_cmd(comp, NULL,
				DSI_GET_PANEL_VBLANK_PERIOD_US, &dur_vblank);
	}

	if (dur_frame == 0) {
		DDPMSG("%s, invalid dur_frame:%uus\n", __func__, dur_frame);
		return -1;
	}
	if (dur_frame == vidle_data.te_duration &&
		dur_vblank == vidle_data.vb_duration)
		return dur_frame;

	/* update DTs affected by TE dur_frame */
	ret = mtk_set_dt_configure(dur_frame, dur_vblank);
	vidle_data.te_duration = dur_frame;
	vidle_data.vb_duration = dur_vblank;

	if (ret < 0 &&
		!(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 1);
		DDPINFO("%s forbid vidle mode, panel:%d, dur_frame:%u, dur_vb:%uus, flag:0x%x\n",
			__func__, type, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	} else if (ret >= 0 &&
		(mtk_disp_vidle_flag.vidle_stop & VIDLE_STOP_VDO_HIGH_FPS)) {
		mtk_set_vidle_stop_flag(VIDLE_STOP_VDO_HIGH_FPS, 0);
		DDPINFO("%s allow vidle mode, panel:%d, dur_frame:%u, dur_vb:%uus, flag:0x%x\n",
			__func__, type, dur_frame, dur_vblank, mtk_disp_vidle_flag.vidle_stop);
	}

	return dur_frame;
}

bool mtk_vidle_is_ff_enabled(void)
{
	return (bool)atomic_read(&g_ff_enabled);
}

void mtk_vidle_enable(bool en, void *_drm_priv)
{
	if (!disp_dpc_driver.dpc_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop && en)
		return;

	if (en == mtk_vidle_is_ff_enabled())
		return;
	atomic_set(&g_ff_enabled, en);

	if (_drm_priv) {
		struct mtk_drm_private *drm_priv = _drm_priv;

		if (drm_priv->dpc_dev) {
			if (en)
				pm_runtime_put_sync(drm_priv->dpc_dev);
			else
				pm_runtime_get_sync(drm_priv->dpc_dev);
		}
	}
	disp_dpc_driver.dpc_enable(en);
	if (!en && vidle_paused) {
		CRTC_MMP_EVENT_END(0, pause_vidle,
				mtk_disp_vidle_flag.vidle_stop, 0xFFFFFFFF);
		vidle_paused = false;
	}
	/* TODO: enable timestamp */

	DDPINFO("%s, en:%d, stop:0x%x, pause:%d\n", __func__, en,
		mtk_disp_vidle_flag.vidle_stop, vidle_paused);
}

void mtk_vidle_force_enable_mml(bool en)
{
	if (!disp_dpc_driver.dpc_dc_force_enable)
		return;

	if (!mtk_disp_vidle_flag.vidle_en)
		return;

	/* some case, like multi crtc we need to stop V-idle */
	if (mtk_disp_vidle_flag.vidle_stop)
		return;

	disp_dpc_driver.dpc_dc_force_enable(en);
}

void mtk_vidle_set_panel_type(enum mtk_panel_type type)
{
	vidle_data.panel_type = type;

	if (!disp_dpc_driver.dpc_init_panel_type)
		return;
	disp_dpc_driver.dpc_init_panel_type(type);
}

bool mtk_vidle_support_hrt_bw(void)
{
	if (disp_dpc_driver.dpc_hrt_bw_set)
		return true;
	return false;
}

void mtk_vidle_hrt_bw_set(const u32 bw_in_mb)
{
	vidle_data.hrt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_hrt_bw_set)
		disp_dpc_driver.dpc_hrt_bw_set(DPC_SUBSYS_DISP, bw_in_mb,
					       !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);

}
void mtk_vidle_srt_bw_set(const u32 bw_in_mb)
{
	vidle_data.srt_bw = bw_in_mb;
	if (disp_dpc_driver.dpc_srt_bw_set)
		disp_dpc_driver.dpc_srt_bw_set(DPC_SUBSYS_DISP, bw_in_mb,
					       !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, bw_in_mb);
}
void mtk_vidle_dvfs_set(const u8 level)
{
	vidle_data.level = level;
	if (disp_dpc_driver.dpc_dvfs_set)
		disp_dpc_driver.dpc_dvfs_set(DPC_SUBSYS_DISP, level,
					     !atomic_read(&g_ff_enabled));
	else
		DDPINFO("%s NOT SET:%d\n", __func__, level);
}
void mtk_vidle_dvfs_bw_set(const u32 bw_in_mb)
{
	if (disp_dpc_driver.dpc_dvfs_bw_set)
		disp_dpc_driver.dpc_dvfs_bw_set(DPC_SUBSYS_DISP, bw_in_mb);
}
void mtk_vidle_config_ff(bool en)
{
	if (en && !mtk_disp_vidle_flag.vidle_en)
		return;

	if (disp_dpc_driver.dpc_config)
		disp_dpc_driver.dpc_config(DPC_SUBSYS_DISP, en);
}

void mtk_vidle_dpc_analysis(bool detail)
{
	if (disp_dpc_driver.dpc_analysis)
		disp_dpc_driver.dpc_analysis(detail);
}

void mtk_vidle_dsi_pll_set(const u32 value)
{
	if (disp_dpc_driver.dpc_dsi_pll_set)
		disp_dpc_driver.dpc_dsi_pll_set(value);
}

void mtk_vidle_register(const struct dpc_funcs *funcs)
{
	int ret = 0;

	DDPMSG("%s, panel:%d,dur:%u,l:0x%x,hrt:0x%x,srt:0x%x\n",
		__func__, vidle_data.panel_type, vidle_data.te_duration,
		vidle_data.level, vidle_data.hrt_bw, vidle_data.srt_bw);

	disp_dpc_driver = *funcs;

	if(vidle_data.panel_type != PANEL_TYPE_COUNT) {
		DDPINFO("%s need set panel:%d\n", __func__, vidle_data.panel_type);
		mtk_vidle_set_panel_type(vidle_data.panel_type);
	}

	if(vidle_data.te_duration != 0xffffffff ||
		vidle_data.vb_duration != 0xffffffff) {
		ret = mtk_set_dt_configure(vidle_data.te_duration, vidle_data.vb_duration);
		DDPINFO("%s set duration:%u, ret:%d\n",
			__func__, vidle_data.te_duration, ret);
	}

	if(vidle_data.level != 0xff) {
		DDPINFO("%s need set level:%d\n", __func__, vidle_data.level);
		mtk_vidle_dvfs_set(vidle_data.level);
	}

	if(vidle_data.hrt_bw != 0xffffffff) {
		DDPINFO("%s need set hrt bw:%d\n", __func__, vidle_data.hrt_bw);
		mtk_vidle_hrt_bw_set(vidle_data.hrt_bw);
	}

	if(vidle_data.srt_bw != 0xffffffff) {
		DDPINFO("%s need set srt bw:%d\n", __func__, vidle_data.srt_bw);
		mtk_vidle_srt_bw_set(vidle_data.srt_bw);
	}
}
EXPORT_SYMBOL(mtk_vidle_register);

void mtk_vdisp_register(const struct mtk_vdisp_funcs *fp)
{
	vdisp_func.genpd_put = fp->genpd_put;
}
EXPORT_SYMBOL(mtk_vdisp_register);
