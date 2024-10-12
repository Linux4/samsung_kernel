// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_dsi.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/unaligned.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_vblank.h>
#include <exynos_display_common.h>

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/dma-heap.h>

#include <video/mipi_display.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-devfreq.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_migov.h>
#include <exynos_drm_freq_hop.h>
#include <regs-dsim.h>

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#include "exynos_drm_dsimfc.h"
#endif
#include <exynos_drm_recovery.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <linux/sec_debug.h>
#endif

struct dsim_device *dsim_drvdata[MAX_DSI_CNT];

static char panel_name[64];
module_param_string(panel_name, panel_name, sizeof(panel_name), 0644);
MODULE_PARM_DESC(panel_name, "preferred panel name");

static int dpu_dsim_log_level = 6;
module_param(dpu_dsim_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dsim_log_level, "log level for dpu dsim [default : 6]");

#define dsim_info(dsim, fmt, ...)	\
dpu_pr_info(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_warn(dsim, fmt, ...)	\
dpu_pr_warn(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_err(dsim, fmt, ...)	\
dpu_pr_err(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define dsim_debug(dsim, fmt, ...)	\
dpu_pr_debug(drv_name((dsim)), (dsim)->id, dpu_dsim_log_level, fmt, ##__VA_ARGS__)

#define host_to_dsi(host) container_of(host, struct dsim_device, dsi_host)

#define DSIM_ESCAPE_CLK_20MHZ	20

//#define DSIM_BIST

static inline enum dsim_panel_mode
get_op_mode(const struct exynos_display_mode *m)
{
	return m->mode_flags & MIPI_DSI_MODE_VIDEO ? DSIM_VIDEO_MODE :
				DSIM_COMMAND_MODE;
}

static inline struct dsim_device *encoder_to_dsim(struct exynos_drm_encoder *e)
{
	return container_of(e, struct dsim_device, encoder);
}

static inline bool is_dsim_enabled(const struct dsim_device *dsim)
{
	return	dsim->state == DSIM_STATE_HSCLKEN || dsim->state == DSIM_STATE_INIT;
}

static inline bool is_dsim_doze_suspended(const struct dsim_device *dsim)
{
	return	dsim->state == DSIM_STATE_SUSPEND && dsim->lp_mode_state;
}

static const struct of_device_id dsim_of_match[] = {
	{ .compatible = "samsung,exynos-dsim",
	  .data = NULL },
	{ }
};
MODULE_DEVICE_TABLE(of, dsim_of_match);

static int dsim_wait_fifo_empty(struct dsim_device *dsim, u32 frames);

void dsim_dump(struct dsim_device *dsim)
{
	struct dsim_regs regs;

	dsim_info(dsim, "=== DSIM SFR DUMP ===\n");

	regs.regs = dsim->res.regs;
	regs.ss_regs = dsim->res.ss_reg_base;
	regs.phy_regs = dsim->res.phy_regs;
	regs.phy_regs_ex = dsim->res.phy_regs_ex;
	__dsim_dump(dsim->id, &regs);
}

static int dsim_phy_power_on(struct dsim_device *dsim)
{
	int ret;

	dsim_debug(dsim, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return 0;

	ret = phy_power_on(dsim->res.phy);
	if (ret) {
		dsim_err(dsim, "failed to enable dphy(%d)\n", ret);
		return ret;
	}
	if (dsim->res.phy_ex) {
		ret = phy_power_on(dsim->res.phy_ex);
		if (ret) {
			dsim_err(dsim, "failed to enable ext dphy(%d)\n", ret);
			return ret;
		}
	}

	dsim_debug(dsim, "-\n");

	return 0;
}

static int dsim_phy_power_off(struct dsim_device *dsim)
{
	int ret;

	dsim_debug(dsim, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return 0;

	ret = phy_power_off(dsim->res.phy);
	if (ret) {
		dsim_err(dsim, "failed to disable dphy(%d)\n", ret);
		return ret;
	}
	if (dsim->res.phy_ex) {
		ret = phy_power_off(dsim->res.phy_ex);
		if (ret) {
			dsim_err(dsim, "failed to disable ext dphy(%d)\n", ret);
			return ret;
		}
	}

	dsim_debug(dsim, "-\n");

	return 0;
}

static void _dsim_init(struct dsim_device *dsim, bool ulps)
{
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	exynos_update_ip_idle_status(dsim->idle_ip_index, 0);

	dsim_phy_power_on(dsim);

	dpu_init_freq_hop(dsim);

	dsim_reg_init(dsim->id, &dsim->config, &dsim->clk_param, !ulps);
	DPU_EVENT_LOG("DSIM_INIT", exynos_crtc, 0, NULL);
}

static void _dsim_start(struct dsim_device *dsim, bool ulps)
{
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	bool irq_enabled = dsim->state == DSIM_STATE_INIT;

	if (!ulps)
		dsim_reg_start(dsim->id);
	else
		dsim_reg_exit_ulps_and_start(dsim->id, 0, 0x1F);

	dsim->state = DSIM_STATE_HSCLKEN;
	if (!irq_enabled)
		enable_irq(dsim->irq);
	DPU_EVENT_LOG("DSIM_START", exynos_crtc, 0, NULL);
}

void dsim_exit_ulps(struct dsim_device *dsim)
{
	dsim_debug(dsim, "+\n");

	mutex_lock(&dsim->cmd_lock);
	if (dsim->state != DSIM_STATE_ULPS)
		goto out;

	pm_runtime_get_sync(dsim->dev);

	_dsim_init(dsim, true);
	_dsim_start(dsim, true);
out:
	mutex_unlock(&dsim->cmd_lock);

	dsim_debug(dsim, "-\n");
}

static void dsim_enable_locked(struct exynos_drm_encoder *exynos_encoder)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	if (!mutex_is_locked(&dsim->cmd_lock)) {
		dsim_err(dsim, "is called w/o lock from %ps\n",
				__builtin_return_address(0));
		return;
	}

	if (dsim->state == DSIM_STATE_HSCLKEN) {
		dsim_info(dsim, "already enabled(%d)\n", dsim->state);
		return;
	}

	pm_runtime_get_sync(dsim->dev);
	pm_stay_awake(dsim->dev);
	dsim_info(dsim, "pm_stay_awake\n");
#if IS_ENABLED(CONFIG_PM_SLEEP)
	dsim_info(dsim, "(active : %lu, relax : %lu)\n",
			dsim->dev->power.wakeup->active_count,
			dsim->dev->power.wakeup->relax_count);
#endif

	_dsim_init(dsim, false);
	if (!dsim->lp11_reset)
		_dsim_start(dsim, false);

#if defined(DSIM_BIST)
	dsim_reg_set_bist(dsim->id, true, DSIM_GRAY_GRADATION);
	dsim_dump(dsim);
#endif

	DPU_EVENT_LOG("DSIM_ENABLED", exynos_crtc, 0, NULL);
}

static void dsim_atomic_enable(struct exynos_drm_encoder *exynos_encoder,
			       struct drm_atomic_state *state)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct exynos_drm_connector_state *exynos_conn_state;

	dsim_info(dsim, "+\n");

	conn = exynos_encoder_get_conn(exynos_encoder, state);
	if (!conn) {
		dsim_err(dsim, "can't find binded connector\n");
		return;
	}

	conn_state = drm_atomic_get_new_connector_state(state, conn);
	exynos_conn_state = to_exynos_connector_state(conn_state);

	mutex_lock(&dsim->cmd_lock);
	dsim_enable_locked(exynos_encoder);
	dsim->lp_mode_state = exynos_conn_state->exynos_mode.is_lp_mode;
	mutex_unlock(&dsim->cmd_lock);

	dsim_info(dsim, "-\n");
}

static void dsim_atomic_activate(struct exynos_drm_encoder *exynos_encoder)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	dsim_info(dsim, "+\n");

	mutex_lock(&dsim->cmd_lock);
	_dsim_start(dsim, false);
	mutex_unlock(&dsim->cmd_lock);

	dsim_info(dsim, "-\n");
}

static void _dsim_disable(struct dsim_device *dsim, bool ulps)
{
	u32 lanes = DSIM_LANE_CLOCK | GENMASK(dsim->config.data_lane_cnt, 1);

	disable_irq(dsim->irq);

	if (!ulps) {
		dsim->state = DSIM_STATE_SUSPEND;
		dsim_reg_stop(dsim->id, lanes);
	} else {
		dsim->state = DSIM_STATE_ULPS;
		dsim_reg_stop_and_enter_ulps(dsim->id, TYPE_OF_SM_DDI, lanes);
	}

	dsim_phy_power_off(dsim);

	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
}

void dsim_enter_ulps(struct dsim_device *dsim)
{
	dsim_debug(dsim, "+\n");

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->cmd_lock);

	if (!is_dsim_enabled(dsim))
		goto out;

	_dsim_disable(dsim, true);

	pm_runtime_put_sync(dsim->dev);
out:
	mutex_unlock(&dsim->cmd_lock);

	dsim_debug(dsim, "-\n");
}

static void dsim_disable_locked(struct exynos_drm_encoder *exynos_encoder)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);

	if (!mutex_is_locked(&dsim->cmd_lock)) {
		dsim_err(dsim, "is called w/o lock from %ps\n",
				__builtin_return_address(0));
		return;
	}

	if (dsim->state == DSIM_STATE_SUSPEND) {
		dsim_info(dsim, "already disabled(%d)\n", dsim->state);
		return;
	}

	/* Wait for current read & write CMDs. */
	dsim_wait_fifo_empty(dsim, 3);
	del_timer(&dsim->cmd_timer);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	del_timer(&dsim->fcmd_timer);
#endif
	_dsim_disable(dsim, false);

	pm_runtime_put_sync(dsim->dev);
	pm_relax(dsim->dev);

	dsim_info(dsim, "pm_relax\n");
#if IS_ENABLED(CONFIG_PM_SLEEP)
	dsim_info(dsim, "(active : %lu, relax : %lu)\n",
			dsim->dev->power.wakeup->active_count,
			dsim->dev->power.wakeup->relax_count);
#endif

	DPU_EVENT_LOG("DSIM_DISABLED", exynos_crtc, 0, NULL);
}

static void dsim_atomic_disable(struct exynos_drm_encoder *exynos_encoder,
				struct drm_atomic_state *state)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	dsim_info(dsim, "+\n");

	mutex_lock(&dsim->cmd_lock);
	dsim_disable_locked(exynos_encoder);
	mutex_unlock(&dsim->cmd_lock);

	dsim_info(dsim, "-\n");
}

static int dsim_get_clock_mode(const struct dsim_device *dsim,
			const struct drm_display_mode *mode)
{
	const struct dsim_pll_params *pll_params = dsim->pll_params;
	const size_t mlen = strnlen(mode->name, DRM_DISPLAY_MODE_LEN);
	size_t plen;
	int i;

	for (i = 0; i < pll_params->num_modes; i++) {
		const struct dsim_pll_param *p = &pll_params->params[i];

		plen = strnlen(p->name, DRM_DISPLAY_MODE_LEN);

		if (!strncmp(mode->name, p->name, max(mlen, plen)))
			return i;
	}

	dsim_warn(dsim, "failed to find clock mode(%s)\n", mode->name);

	return -ENOENT;
}

static int dsim_set_clock_mode(struct dsim_device *dsim,
			       const struct drm_display_mode *mode)
{
	struct dsim_pll_params *pll_params = dsim->pll_params;
	const struct dsim_pll_param *p;
	struct stdphy_pms *dphy_pms;
	struct dsim_clks *clk_param;

	pll_params->curr_idx = dsim_get_clock_mode(dsim, mode);

	if (pll_params->curr_idx < 0)
		return -ENOENT;

	p = &pll_params->params[pll_params->curr_idx];

	dphy_pms = &dsim->config.dphy_pms;
	dphy_pms->p = p->p;
	dphy_pms->m = p->m;
	dphy_pms->s = p->s;
	dphy_pms->k = p->k;

	dphy_pms->mfr = p->mfr;
	dphy_pms->mrr = p->mrr;
	dphy_pms->sel_pf = p->sel_pf;
	dphy_pms->icp = p->icp;
	dphy_pms->afc_enb = p->afc_enb;
	dphy_pms->extafc = p->extafc;
	dphy_pms->feed_en = p->feed_en;
	dphy_pms->fsel = p->fsel;
	dphy_pms->fout_mask = p->fout_mask;
	dphy_pms->rsel = p->rsel;
	dphy_pms->dither_en = p->dither_en;

	clk_param = &dsim->clk_param;
	clk_param->hs_clk = p->pll_freq;
	clk_param->esc_clk = p->esc_freq;

	dsim_debug(dsim, "found proper pll parameter\n");
	dsim_debug(dsim, "\t%s(p:0x%x,m:0x%x,s:0x%x,k:0x%x)\n", p->name,
		 dphy_pms->p, dphy_pms->m, dphy_pms->s, dphy_pms->k);

	dsim_debug(dsim, "\t%s(hs:%d,esc:%d)\n", p->name, clk_param->hs_clk,
			clk_param->esc_clk);

	dsim->config.cmd_underrun_cnt = p->cmd_underrun_cnt;

	return 0;
}

static int dsim_of_parse_modes(struct device_node *entry,
			       struct dsim_pll_param *pll_param)
{
	u32 res[14];
	int cnt;

	pll_param->name = entry->full_name;

	cnt = of_property_count_u32_elems(entry, "exynos,pmsk");
	if (cnt != 4 && cnt != 14) {
		pr_err("mode %s has wrong pmsk elements number %d\n",
				pll_param->name, cnt);
		return -EINVAL;
	}

	of_property_read_u32_array(entry, "exynos,pmsk", res, cnt);
	pll_param->dither_en = false;
	pll_param->p = res[0];
	pll_param->m = res[1];
	pll_param->s = res[2];
	pll_param->k = res[3];
	if (cnt == 14) {
		pll_param->mfr = res[4];
		pll_param->mrr = res[5];
		pll_param->sel_pf = res[6];
		pll_param->icp = res[7];
		pll_param->afc_enb = res[8];
		pll_param->extafc = res[9];
		pll_param->feed_en = res[10];
		pll_param->fsel = res[11];
		pll_param->fout_mask = res[12];
		pll_param->rsel = res[13];
		if (pll_param->mfr || pll_param->mrr)
			pll_param->dither_en = true;
	}

	of_property_read_u32(entry, "exynos,hs-clk", &pll_param->pll_freq);

	of_property_read_u32(entry, "exynos,esc-clk", &pll_param->esc_freq);

	of_property_read_u32(entry, "exynos,cmd_underrun_cnt",
			&pll_param->cmd_underrun_cnt);

	return 0;
}

static struct dsim_pll_params *dsim_of_get_clock_mode(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *mode_np, *entry = NULL;
	struct dsim_pll_params *pll_params;

	mode_np = of_parse_phandle(dev->of_node, "dsim_mode", 0);
	if (!mode_np) {
		dsim_err(dsim, "failed to find dsim-mode phandle\n");
		return NULL;
	}

	pll_params = devm_kzalloc(dev, sizeof(*pll_params), GFP_KERNEL);
	if (!pll_params)
		goto err;

	pll_params->num_modes = of_get_child_count(mode_np);
	if (pll_params->num_modes == 0) {
		dsim_err(dsim, "%pOF: no modes specified\n", mode_np);
		pll_params = NULL;
		goto err;
	}

	pll_params->params = devm_kzalloc(dev, sizeof(struct dsim_pll_param) *
			pll_params->num_modes, GFP_KERNEL);
	if (!pll_params->params) {
		pll_params = NULL;
		goto err;
	}

	pll_params->num_modes = 0;
	for_each_child_of_node(mode_np, entry) {
		struct dsim_pll_param *pll_param;

		exynos_duplicate_display_props(dev, entry, mode_np);
		pll_param = &pll_params->params[pll_params->num_modes++];

		if (dsim_of_parse_modes(entry, pll_param) < 0)
			continue;
	}

err:
	of_node_put(entry);
	of_node_put(mode_np);

	return pll_params;
}

static void dsim_adjust_video_timing(struct dsim_device *dsim,
				     const struct drm_display_mode *mode)
{
	struct videomode vm;
	struct dpu_panel_timing *p_timing = &dsim->config.p_timing;

	drm_display_mode_to_videomode(mode, &vm);

	p_timing->vactive = vm.vactive;
	p_timing->vfp = vm.vfront_porch;
	p_timing->vbp = vm.vback_porch;
	p_timing->vsa = vm.vsync_len;

	p_timing->hactive = vm.hactive;
	p_timing->hfp = vm.hfront_porch;
	p_timing->hbp = vm.hback_porch;
	p_timing->hsa = vm.hsync_len;
	p_timing->vrefresh = drm_mode_vrefresh(mode);

	dsim_set_clock_mode(dsim, mode);
}

static void dsim_update_config_for_mode(struct dsim_reg_config *config,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode)
{
	config->mode = get_op_mode(exynos_mode);

	config->dsc.enabled = exynos_mode->dsc.enabled;
	if (config->dsc.enabled) {
		config->dsc.dsc_count = exynos_mode->dsc.dsc_count;
		config->dsc.slice_count = exynos_mode->dsc.slice_count;
		config->dsc.slice_height = exynos_mode->dsc.slice_height;
		config->dsc.slice_width = DIV_ROUND_UP(
				config->p_timing.hactive,
				config->dsc.slice_count);
	}
	config->bpc = exynos_mode->bpc;
}

static void dsim_set_display_mode(struct dsim_device *dsim,
				  const struct drm_display_mode *mode,
				  const struct exynos_display_mode *exynos_mode)
{
	struct dsim_reg_config *config = &dsim->config;

	if (!dsim->dsi_device)
		return;

	config->data_lane_cnt = dsim->dsi_device->lanes;
	if (config->intf_type == DSIM_DSI_INTF_CPHY)
		config->data_lane_cnt = MAX_DSIM_DATALANE_CNT - 1;

	dsim_adjust_video_timing(dsim, mode);

	dsim_update_config_for_mode(config, mode, exynos_mode);
	config->emul_mode = dsim->emul_mode;

	dsim_info(dsim, "dsim mode %s dsc is %s [%d %d %d %d]\n",
			config->mode == DSIM_VIDEO_MODE ? "video" : "cmd",
			config->dsc.enabled ? "enabled" : "disabled",
			config->dsc.dsc_count,
			config->dsc.slice_count,
			config->dsc.slice_width,
			config->dsc.slice_height);
}

static void dsim_atomic_seamless_mode_set(struct dsim_device *dsim,
			struct exynos_drm_connector_state *exynos_conn_state)
{
	struct dsim_reg_config *config = &dsim->config;

	mutex_lock(&dsim->cmd_lock);
	if (exynos_conn_state->seamless_modeset & SEAMLESS_MODESET_MRES)
		dsim_reg_set_mres(dsim->id, config);
	if (exynos_conn_state->seamless_modeset & SEAMLESS_MODESET_VREF) {
		if (get_op_mode(&exynos_conn_state->exynos_mode) == DSIM_COMMAND_MODE) {
			dsim_reg_set_cm_underrun_lp_ref(dsim->id,
					config->cmd_underrun_cnt);
			dsim_reg_set_cmd_ctrl(dsim->id, config, &dsim->clk_param);
		} else {
			dsim_reg_set_porch(dsim->id, config);
		}
	}
	if (exynos_conn_state->seamless_modeset & SEAMLESS_MODESET_LP)
		dsim->lp_mode_state = exynos_conn_state->exynos_mode.is_lp_mode;
	mutex_unlock(&dsim->cmd_lock);
}

static void dsim_atomic_mode_set(struct exynos_drm_encoder *exynos_encoder,
				 struct drm_crtc_state *crtc_state,
				 struct drm_connector_state *conn_state)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct exynos_drm_connector_state *exynos_conn_state =
					to_exynos_connector_state(conn_state);

	dsim_set_display_mode(dsim, &crtc_state->adjusted_mode,
			&exynos_conn_state->exynos_mode);
	if (exynos_conn_state->seamless_modeset)
		dsim_atomic_seamless_mode_set(dsim, exynos_conn_state);
}

static enum drm_mode_status dsim_mode_valid(struct exynos_drm_encoder *exynos_encoder,
					    const struct drm_display_mode *mode)
{
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	if (dsim_get_clock_mode(dsim, mode) < 0)
		return MODE_NOMODE;

	return MODE_OK;
}

static bool dsim_check_seamless_modeset(const struct dsim_device *dsim,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode)
{
	if (dsim->config.mode != get_op_mode(exynos_mode)) {
		dsim_debug(dsim, "can't chang op mode w/o seamelss\n");
		return false;
	}
	/*
	 * If it needs to check whether to change the display mode seamlessly
	 * for dsim, please add to here.
	 */

	return true;
}

static int dsim_atomic_check(struct exynos_drm_encoder *exynos_encoder,
			     struct drm_crtc_state *crtc_state,
			     struct drm_connector_state *conn_state)
{
	const struct drm_display_mode *mode;
	const struct exynos_display_mode *exynos_mode;
	const struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct exynos_drm_connector_state *exynos_conn_state;

	if (!crtc_state->mode_changed)
		return 0;

	if (!is_exynos_drm_connector(conn_state->connector)) {
		dsim_warn(dsim, "mode set is only supported w/exynos connector\n");
		return -EINVAL;
	}

	mode = &crtc_state->adjusted_mode;

	exynos_conn_state = to_exynos_connector_state(conn_state);
	exynos_mode = &exynos_conn_state->exynos_mode;
	if (exynos_conn_state->seamless_modeset &&
			!dsim_check_seamless_modeset(dsim, mode, exynos_mode))
		exynos_conn_state->seamless_modeset = false;

	return 0;
}

static const struct exynos_drm_encoder_funcs exynos_dsim_encoder_funcs = {
	.mode_valid = dsim_mode_valid,
	.atomic_mode_set = dsim_atomic_mode_set,
	.atomic_enable = dsim_atomic_enable,
	.atomic_activate = dsim_atomic_activate,
	.atomic_disable = dsim_atomic_disable,
	.atomic_check = dsim_atomic_check,
};

static int dsim_add_mipi_dsi_device(struct dsim_device *dsim)
{
	struct mipi_dsi_device_info info = { };
	struct device_node *node;
	const char *name;

	dsim_debug(dsim, "preferred panel is %s\n", panel_name);

	for_each_available_child_of_node(dsim->dsi_host.dev->of_node, node) {
		/* panel w/ reg node will be added in mipi_dsi_host_register */
		if (of_find_property(node, "reg", NULL))
			continue;

		if (of_property_read_u32(node, "channel", &info.channel))
			continue;

		name = of_get_property(node, "label", NULL);
		if (name && !strcmp(name, panel_name)) {
			strlcpy(info.type, name, sizeof(info.type));
			info.node = of_node_get(node);
			mipi_dsi_device_register_full(&dsim->dsi_host, &info);

			return 0;
		}
	}

	return -ENODEV;
}

static int dsim_bind(struct device *dev, struct device *master, void *data)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	struct drm_device *drm_dev = data;
	int ret = 0;

	dsim_debug(dsim, "+\n");

	ret = exynos_drm_encoder_init(drm_dev, &dsim->encoder,
			&exynos_dsim_encoder_funcs, DRM_MODE_ENCODER_DSI,
			dsim->output_type);
	if (ret) {
		dsim_err(dsim, "failed to init exynos encoder\n");
		return ret;
	}

	/* add the dsi device for the detected panel */
	dsim_add_mipi_dsi_device(dsim);

	ret = mipi_dsi_host_register(&dsim->dsi_host);

	dsim_debug(dsim, "-\n");

	return ret;
}

static void dsim_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	dsim_debug(dsim, "+\n");

	mipi_dsi_host_unregister(&dsim->dsi_host);
}

static const struct component_ops dsim_component_ops = {
	.bind	= dsim_bind,
	.unbind	= dsim_unbind,
};

int dsim_free_fb_resource(struct dsim_device *dsim)
{
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	int upload_mode = secdbg_mode_enter_upload();

	if (upload_mode) {
		dsim_info(dsim, "%s: rmem_release skip! upload_mode:(%d)\n", __func__, upload_mode);
	} else {
		/* unreserve memory */
		of_reserved_mem_device_release(dsim->dev);
	}
#else
	/* unreserve memory */
	of_reserved_mem_device_release(dsim->dev);
#endif
	/* update state */
	dsim->fb_handover.reserved = false;
	dsim->fb_handover.phys_addr = 0xdead;
	dsim->fb_handover.phys_size = 0;

	return 0;
}

static const struct reserved_mem_ops rmem_ops;
static void dsim_acquire_fb_resource(struct dsim_device *dsim)
{
	int ret = 0;
	struct dsim_fb_handover *fb = &dsim->fb_handover;
	struct device_node *np = dsim->dev->of_node;
	struct device_node *rmem_np = NULL;
	struct reserved_mem *rmem;

	fb->reserved = false;
	rmem_np = of_parse_phandle(np, "memory-region", 0);
	if (rmem_np) {
		rmem = of_reserved_mem_lookup(rmem_np);
		if (!rmem) {
			dsim_info(dsim, "failed to reserve memory lookup(optional)\n");
			return;
		}
	} else {
		dsim_info(dsim, "failed to get reserve memory phandle(optional)\n");
		return;
	}
	of_node_put(rmem_np);

	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	rmem->ops = &rmem_ops;
	fb->rmem = rmem;

	/*
	 * If of_reserved_mem_device_init_by_idx returns error, it means
	 * framebuffer handover feature is disabled or reserved memory is
	 * not defined in DT.
	 *
	 * And phys_addr and phys_size is not initialized, because
	 * rmem_device_init callback is not called.
	 */
	ret = of_reserved_mem_device_init_by_idx(dsim->dev, dsim->dev->of_node, 0);
	if (ret) {
		dsim_warn(dsim, "fb handover memory is not reserved(%d)\n", ret);
		dsim_warn(dsim, "check whether fb handover memory is not reserved");
		dsim_warn(dsim, "or DT definition is missed\n");
		return;
	} else {
		fb->reserved = true;
	}

	ret = of_property_read_u32(np, "lk-fb-win-id", &fb->lk_fb_win_id);
	if (ret)
		dsim_info(dsim, "Default LK FB win id(%d)\n", fb->lk_fb_win_id);
	else
		dsim_info(dsim, "Configured LK FB win id(%d)\n", fb->lk_fb_win_id);
}

static int dsim_parse_dt(struct dsim_device *dsim)
{
	struct device_node *np = dsim->dev->of_node;
	int ret, val;

	if (!np) {
		dsim_err(dsim, "no device tree information\n");
		return -ENOTSUPP;
	}

	of_property_read_u32(np, "dsim,id", &dsim->id);
	if (dsim->id < 0 || dsim->id >= MAX_DSI_CNT) {
		dsim_err(dsim, "wrong dsim id(%d)\n", dsim->id);
		return -ENODEV;
	}

	if (of_property_read_u32(np, "dsi-burst-cmd",
				 &dsim->config.burst_cmd_en))
		dsim_err(dsim, "failed to parse dsi burst command\n");

	if (of_property_read_u32(np, "dsi-drive-strength",
				 &dsim->config.drive_strength))
		dsim->config.drive_strength = UINT_MAX;

	dsim->pll_params = dsim_of_get_clock_mode(dsim);

	ret = of_property_read_u32(np, "phy-type", &val);
	if (ret == -EINVAL || (ret == 0 && val != 1))
		dsim->config.intf_type = 0;
	else
		dsim->config.intf_type = 1;
	dsim_info(dsim, "interface = <%s>\n", dsim->config.intf_type ? "cphy" : "dphy");

	ret = of_property_read_u32(np, "dsim,emul-mode", &val);
	if (ret == -EINVAL || (ret == 0 && val == 0))
		dsim->emul_mode = false;
	else
		dsim->emul_mode = true;
	dsim_info(dsim, "emul_mode=%d\n", dsim->emul_mode);

	if (!dsim->emul_mode)
		dsim_acquire_fb_resource(dsim);

	return 0;
}

static int dsim_remap_by_name(struct dsim_device *dsim, struct device_node *np,
		void __iomem **base, const char *reg_name, enum dsim_regs_type type)
{
	int i, ret;
	struct resource res;

	i = of_property_match_string(np, "reg-names", reg_name);
	if (i < 0) {
		dsim_info(dsim, "failed to find %s SFR region\n", reg_name);
		return 0;
	}

	ret = of_address_to_resource(np, i, &res);
	if (ret)
		return ret;

	*base = devm_ioremap(dsim->dev, res.start, resource_size(&res));
	if (!(*base)) {
		dsim_err(dsim, "failed to remap %s SFR region\n", reg_name);
		return -EINVAL;
	}
	dsim_regs_desc_init(*base, reg_name, type, dsim->id);

	return 0;
}

static int dsim_remap_regs(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *np = dev->of_node;
	struct device_node *sys_np = NULL;

	if (dsim_remap_by_name(dsim, np, &dsim->res.regs, "dsi", REGS_DSIM_DSI))
		goto err;

	if (dsim_remap_by_name(dsim, np, &dsim->res.phy_regs, "dphy", REGS_DSIM_PHY))
		goto err;

	if (dsim_remap_by_name(dsim, np, &dsim->res.phy_regs_ex, "dphy-extra",
			REGS_DSIM_PHY_BIAS))
		goto err;

	sys_np = of_find_compatible_node(NULL, NULL, "samsung,exynos9-disp_ss");
	if (IS_ERR_OR_NULL(sys_np)) {
		dsim_err(dsim, "failed to find disp_ss node");
		goto err;
	}

	if (dsim_remap_by_name(dsim, sys_np, &dsim->res.ss_reg_base, "sys",
			REGS_DSIM_SYS))
		goto err;

	of_node_put(sys_np);

	return 0;

err:
	of_node_put(sys_np);

	return -EINVAL;
}

static void dsim_print_underrun_info(struct dsim_device *dsim)
{
	const struct decon_device *decon = dsim_get_decon(dsim);
	struct exynos_drm_crtc *exynos_crtc;
	u32 line_cnt;

	if (!decon || (decon->state != DECON_STATE_ON))
		return;

	exynos_crtc = decon->crtc;
	line_cnt = dsim_reg_get_linecount(dsim->id, dsim->config);
	DPU_EVENT_LOG("DSIM_UNDERRUN", exynos_crtc,
			EVENT_FLAG_REPEAT | EVENT_FLAG_ERROR,
			FREQ_FMT" underrun count(%d) line(%d)",
			FREQ_ARG(&decon->bts), ++exynos_crtc->d.underrun_cnt,
			line_cnt);

	dsim_info(dsim, "underrun(%d) irq occurs at line %d\n",
			exynos_crtc->d.underrun_cnt, line_cnt);
	if (IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->print_info(decon->crtc);
}

static irqreturn_t dsim_irq_handler(int irq, void *dev_id)
{
	struct dsim_device *dsim = dev_id;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	unsigned int int_src;

	spin_lock(&dsim->slock);

	dsim_debug(dsim, "+\n");

	if (!is_dsim_enabled(dsim)) {
		dsim_info(dsim, "dsim power is off state(0x%x)\n", dsim->state);
		goto out;
	}

	int_src = dsim_reg_get_int(dsim->id);
	if (int_src & DSIM_INTSRC_SFR_PH_FIFO_EMPTY) {
		del_timer(&dsim->cmd_timer);
		complete(&dsim->ph_wr_comp);
		dsim_debug(dsim, "PH_FIFO_EMPTY irq occurs\n");
	}
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	if (int_src & DSIM_INTSRC_SFR_PL_FIFO_EMPTY) {
		del_timer(&dsim->fcmd_timer);
		complete(&dsim->pl_wr_comp);
		dsim_debug(dsim, "PL_FIFO_EMPTY irq occurs\n");
	}
#endif
	if (int_src & DSIM_INTSRC_RX_DATA_DONE)
		complete(&dsim->rd_comp);
	if (int_src & DSIM_INTSRC_FRAME_DONE) {
		dsim_debug(dsim, "framedone irq occurs\n");

		if (exynos_crtc && exynos_crtc->crc_state == EXYNOS_DRM_CRC_PEND)
			if (exynos_crtc->ops->get_crc_data)
				exynos_crtc->ops->get_crc_data(exynos_crtc);

		DPU_EVENT_LOG("DSIM_FRAMEDONE", exynos_crtc, 0, NULL);
	}
	if (int_src & DSIM_INTSRC_ERR_RX_ECC)
		dsim_err(dsim, "RX ECC Multibit error was detected!\n");

	if (int_src & DSIM_INTSRC_UNDER_RUN)
		dsim_print_underrun_info(dsim);

	if (int_src & DSIM_INTSRC_VT_STATUS) {
		dsim_debug(dsim, "vt_status irq occurs\n");
		if ((dsim->config.mode == DSIM_VIDEO_MODE) && exynos_crtc) {
			drm_crtc_handle_vblank(&exynos_crtc->base);
			DPU_EVENT_LOG("SIGNAL_CRTC_OUT_FENCE", exynos_crtc,
				EVENT_FLAG_REPEAT | EVENT_FLAG_FENCE, NULL);
			if (exynos_crtc->migov)
				exynos_migov_update_vsync_cnt(exynos_crtc->migov);
		}
	}
	dsim_reg_clear_int(dsim->id, int_src);

out:
	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

static int dsim_register_irq(struct dsim_device *dsim)
{
	struct device *dev = dsim->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	/* Clear any interrupt triggered in LK. */
	dsim_reg_clear_int(dsim->id, 0xffffffff);

	dsim->irq = of_irq_get_byname(np, "dsim");
	ret = devm_request_irq(dsim->dev, dsim->irq, dsim_irq_handler, 0,
			pdev->name, dsim);
	if (ret) {
		dsim_err(dsim, "failed to install DSIM irq\n");
		return -EINVAL;
	}
	disable_irq(dsim->irq);

	return 0;
}

static int dsim_get_phys(struct dsim_device *dsim)
{
	dsim->res.phy = devm_phy_get(dsim->dev, "dsim_dphy");
	if (IS_ERR(dsim->res.phy)) {
		dsim_err(dsim, "failed to get dsim phy\n");
		return PTR_ERR(dsim->res.phy);
	}

	dsim->res.phy_ex = devm_phy_get(dsim->dev, "dsim_dphy_extra");
	if (IS_ERR(dsim->res.phy_ex)) {
		dsim_warn(dsim, "failed to get dsim extra phy\n");
		dsim->res.phy_ex = NULL;
	}

	return 0;
}

static int dsim_init_resources(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_remap_regs(dsim);
	if (ret)
		goto err;

	ret = dsim_register_irq(dsim);
	if (ret)
		goto err;

	ret = dsim_get_phys(dsim);
	if (ret)
		goto err;

err:
	return ret;
}

static int dsim_host_attach(struct mipi_dsi_host *host,
				  struct mipi_dsi_device *device)
{
	struct dsim_device *dsim = host_to_dsi(host);
	struct drm_bridge *bridge;
	int ret;

	dsim_debug(dsim, "+\n");

	bridge = of_drm_find_bridge(device->dev.of_node);
	if (!bridge) {
		struct drm_panel *panel;

		panel = of_drm_find_panel(device->dev.of_node);
		if (IS_ERR(panel)) {
			dsim_err(dsim, "failed to find panel\n");
			return PTR_ERR(panel);
		}

		bridge = devm_drm_panel_bridge_add_typed(host->dev, panel,
						   DRM_MODE_CONNECTOR_DSI);
		if (IS_ERR(bridge)) {
			dsim_err(dsim, "failed to create panel bridge\n");
			return PTR_ERR(bridge);
		}
	}

	ret = drm_bridge_attach(&dsim->encoder.base, bridge, NULL, 0);
	if (ret) {
		dsim_err(dsim, "Unable to attach panel bridge\n");
	} else {
		dsim->panel_bridge = bridge;
		dsim->dsi_device = device;
	}

	dsim_debug(dsim, "-\n");

	return ret;
}

static int dsim_host_detach(struct mipi_dsi_host *host,
				  struct mipi_dsi_device *device)
{
	struct dsim_device *dsim = host_to_dsi(host);

	dsim_info(dsim, "+\n");

	dsim_atomic_disable(&dsim->encoder, NULL);
	if (dsim->panel_bridge) {
		struct drm_bridge *bridge = dsim->panel_bridge;

		if (bridge->funcs && bridge->funcs->detach)
			bridge->funcs->detach(bridge);
		dsim->panel_bridge = NULL;
	}
	dsim->dsi_device = NULL;

	dsim_info(dsim, "-\n");
	return 0;
}

#define DFT_FPS		60
static int dsim_wait_fifo_empty(struct dsim_device *dsim, u32 frames)
{
	int cnt, fps;
	u64 time_us;

	fps = dsim->config.p_timing.vrefresh ? : DFT_FPS;
	time_us = frames * USEC_PER_SEC / fps;
	cnt = time_us / 10;

	do {
		if (dsim_reg_header_fifo_is_empty(dsim->id) &&
			dsim_reg_payload_fifo_is_empty(dsim->id))
			break;
		usleep_range(10, 11);
	} while (--cnt);

	if (!cnt)
		dsim_err(dsim, "failed usec(%lld)\n", time_us);

	return cnt;
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define FCMD_SEND_START		0x4c
#define FCMD_SEND_CONTINUE	0x5c
#define FCMD_DATA_MAX_SIZE	0x00100000
static int dsim_wait_for_fcmd_xfer_done(struct dsim_device *dsim)
{
	const struct decon_device *decon = dsim_get_decon(dsim);
	int ret, fps;
	u32 dsimfc_timeout;

	if (!decon)
		dsim_info(dsim, "no decon drvdata!!!\n");

	fps = decon ? decon->config.fps : DFT_FPS;
	dsimfc_timeout = 10 * 1000000 / fps / 1000;

	ret = wait_event_interruptible_timeout(dsim->dsimfc->xferdone_wq,
			dsim->dsimfc->config->done, msecs_to_jiffies(dsimfc_timeout));
	if (ret == 0) {
		dsim_err(dsim, "timeout of dsim%d fcmd xferdone\n", dsim->id);
		dsimfc_dump(dsim->dsimfc);
		return -ETIMEDOUT;
	} else
		dsim_info(dsim, "dsim%d fcmd xferdone\n", dsim->id);

	return 0;
}

static int dsim_fcmd_check(struct dsim_device *dsim, u32 id, u32 unit)
{
	u32 max_unit = DSIM_PL_FIFO_THRESHOLD;

	/* Check command id : FCMD support only below 2 types */
	if ((id != MIPI_DSI_GENERIC_LONG_WRITE) && (id != MIPI_DSI_DCS_LONG_WRITE)) {
		dsim_err(dsim, "data id %x is not supported in dma access mode.\n", id);
		return -EINVAL;
	}

	/* Check unit : FCMD xfer unit should not be over DSIM Paylod Theshold */
	if (unit > max_unit) {
		dsim_err(dsim, "packet unit should not be over %d\n", max_unit);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	/* Check align : Xfer unit should be 8byte-aligned */
	if (unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		dsim_err(dsim, "packet unit should be 8byte-aligned\n");
		return -EINVAL;
	}
#endif

	return 0;
}

int dsim_write_data_fcmd(struct dsim_device *dsim, u32 id, dma_addr_t d0,
			u32 d1, u32 unit, bool wait_empty)
{
	int ret = 0;
	struct dsimfc_config dsimfc_config;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	const struct exynos_drm_crtc_ops *ops;

	if (dsim_fcmd_check(dsim, id, unit))
		return -EINVAL;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_warn(dsim, "DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	/* Wait 3 frames for ph & pl fifo to be empty before writing command */
	if (!dsim_wait_fifo_empty(dsim, 3)) {
		ret = -EINVAL;
		dsim_err(dsim, "ID(%d): DSIM cmd wr timeout @ don't available ph or pl 0x%lx\n", id, d0);
		goto err_exit;
	}

	dsimfc_config.di = (u8)id;
	dsimfc_config.size = d1;
	dsimfc_config.unit = (unit <= d1)? unit: d1;
	dsimfc_config.buf = d0;
	dsimfc_config.done = 0;

	ret = dsim->dsimfc->ops->atomic_config(dsim->dsimfc, &dsimfc_config);
	if (ret) {
		dsim_err(dsim, "failed to config fcmd\n");
		goto err_exit;
	}

	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, false);

	dsim_reg_set_cmd_access_mode(dsim->id, 1);

	if (exynos_crtc) {
		ops = exynos_crtc->ops;
		/* force to exit pll sleep before starting command transfer */
		if (ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, true);
	}

	ret = dsim->dsimfc->ops->atomic_start(dsim->dsimfc);
	if (ret) {
		dsim_err(dsim, "failed to start fcmd\n");
		goto err_fcmd_exit;
	}

	ret = dsim_wait_for_fcmd_xfer_done(dsim);
	if (ret)
		goto err_fcmd_exit;

	if (!wait_empty) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id) &&
			dsim_reg_payload_fifo_is_empty(dsim->id))
			dsim_debug(dsim, "Doesn't need to wait fifo_completion\n");
		else {
			dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
			/* Run write-fail dectector */
			reinit_completion(&dsim->pl_wr_comp);
			mod_timer(&dsim->fcmd_timer, jiffies + MIPI_WR_TIMEOUT);
		}
	} else {
		/* wating time for empty : 10 frames  */
		if (!dsim_wait_fifo_empty(dsim, 10)) {
			dsim_err(dsim, "MIPI fcmd tx timeout(ID:%d, dma_addr:%lx)\n", id, d0);
			ret = -EINVAL;
		}
	}

err_fcmd_exit:
	dsim_reg_set_cmd_access_mode(dsim->id, 0);
	ret |= dsim->dsimfc->ops->atomic_stop(dsim->dsimfc);
	if (dsim->config.burst_cmd_en)
		dsim_reg_enable_packetgo(dsim->id, true);

err_exit:
	mutex_unlock(&dsim->cmd_lock);
	if (exynos_crtc) {
		DPU_EVENT_LOG("DSIM_FAST_COMMAND", exynos_crtc, 0,
				"CMD_ID(%#x) TX_LEN(%d) buf_vaddr(%#x)",
				id, d1, (unsigned long)dsim->fcmd_buf_vaddr);
		hibernation_unblock(exynos_crtc->hibernation);
	}

	return ret;
}

static void dsim_fcmd_timeout_handler(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, fcmd_timer);
	queue_work(system_unbound_wq, &dsim->fcmd_work);
}

static void dsim_fcmd_fail_detector(struct work_struct *work)
{
	struct dsim_device *dsim = container_of(work, struct dsim_device, fcmd_work);
	dsim_debug(dsim, "+\n");

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "DSIM is not ready. state(%d)\n", dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->fcmd_timer)
			&& dsim_reg_payload_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->pl_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
		goto exit;
	}

exit:
	mutex_unlock(&dsim->cmd_lock);

	dsim_debug(dsim, "-\n");
}

static unsigned int dsim_fcmd_map_handle(struct device *dev,
		struct dsim_dma_buf_data *dma,
		struct dma_buf *buf)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	dma->dma_buf = buf;

	if (IS_ERR_OR_NULL(dev)) {
		dsim_err(dsim, "dev ptr is invalid\n");
		goto err_buf_map_attach;
	}

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		dsim_err(dsim, "dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		dsim_err(dsim, "dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	/* This is DVA(Device Virtual Address) for setting base address SFR */
	dma->dma_addr = sg_dma_address(dma->sg_table->sgl);
	if (IS_ERR_VALUE(dma->dma_addr)) {
		dsim_err(dsim, "sg_dma_address() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	return dma->dma_buf->size;

err_iovmm_map:
err_buf_map_attachment:
err_buf_map_attach:
	return 0;
}

static int dsim_alloc_fcmd_memory(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	dma_addr_t map_dma;
	struct dma_heap *dma_heap;
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(NULL);
	unsigned int ret;
	u32 size = FCMD_DATA_MAX_SIZE;

	dsim_debug(dsim, "+\n");

	dev_info(dsim->dev, "allocating memory for fcmd%d\n", id);

	size = PAGE_ALIGN(size);

	dev_info(dsim->dev, "want %u bytes\n", size);

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		dsim->fcmd_buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
		dma_heap_put(dma_heap);
	} else {
		pr_err("dma_heap_find() failed\n");
		goto err_share_dma_buf;
	}
	if (IS_ERR(dsim->fcmd_buf)) {
		dev_err(dsim->dev, "ion_alloc() failed\n");
		goto err_share_dma_buf;
	}

	ret = dma_buf_vmap(dsim->fcmd_buf, &map);
	if (ret) {
		dev_err(dsim->dev, "failed to vmap buffer [%x]\n", ret);
		goto err_map;
	}
	dsim->fcmd_buf_vaddr = map.vaddr;

	ret = dsim_fcmd_map_handle(dsim->dev, &dsim->fcmd_buf_data, dsim->fcmd_buf);
	if (!ret)
		goto err_map;

	map_dma = dsim->fcmd_buf_data.dma_addr;

	dev_info(dsim->dev, "alloated memory for fcmd%d\n", id);
	dev_info(dsim->dev, "fcmd start addr = 0x%x\n", (u32)map_dma);

	dsim->fcmd_buf_allocated = true;

	dsim_debug(dsim, "-\n");

	return 0;

err_map:
	dma_buf_put(dsim->fcmd_buf);
err_share_dma_buf:
	return -ENOMEM;
}

int dsim_wr_data_fcmd(struct dsim_device *dsim,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_fcmd *fcmd = to_dsim_fcmd(msg);
	int ret;

	if (msg->tx_len > FCMD_DATA_MAX_SIZE) {
		dsim_err(dsim, "fcmd transfer size error(%d, %d)\n", msg->tx_len, FCMD_DATA_MAX_SIZE);
		return -EINVAL;
	}

	if (fcmd->xfer_unit > DSIM_PL_FIFO_THRESHOLD)
		dsim_err(dsim, "fcmd transfer align error(%d, %d)\n", fcmd->xfer_unit, DSIM_PL_FIFO_THRESHOLD);

	if (!dsim->fcmd_buf_allocated) {
		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret) {
			dsim_err(dsim, "dsim%d : dsim_alloc_fcmd_memory fail\n", dsim->id);
			return ret;
		}
	}

	if (msg->tx_buf != dsim->fcmd_buf_vaddr)
		memcpy((u8 *)dsim->fcmd_buf_vaddr, msg->tx_buf, msg->tx_len);

	return dsim_write_data_fcmd(dsim, msg->type, dsim->fcmd_buf_data.dma_addr,
					msg->tx_len, fcmd->xfer_unit, true);
}

ssize_t dsim_host_fcmd_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	ret = dsim_wr_data_fcmd(dsim, msg);
	if (ret < 0)
		dsim_err(dsim, "dsim%d : failed to transfer fast-command\n", dsim->id);

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}
EXPORT_SYMBOL(dsim_host_fcmd_transfer);

static ssize_t dsim_fcmd_write_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	int ret = 0;
	unsigned long size;
	u32 id = MIPI_DSI_DCS_LONG_WRITE;
	int xfer_cnt, xfer_sz, xfer_unit;
	u32 padd_u, padd_c, padd_t;
	u8 *fcmd_data;
	int i;

	dsim_info(dsim, "+\n");

	if (count <= 0) {
		dsim_err(dsim, "fcmd count input error\n");
		return ret;
	}

	ret = kstrtoul(buf, 0, &size);
	if (ret) {
		dsim_err(dsim, "kstrtoul error\n");
		return ret;
	}

	/* xfer_unit does not have to be aligned with 8 from EVT1. */
	xfer_unit = DSIM_PL_FIFO_THRESHOLD;
	if (size <= (xfer_unit - 1))
		xfer_cnt = 1;
	else {
		xfer_cnt = size / (xfer_unit - 1);
		if (size % (xfer_unit - 1))
			xfer_cnt++;
	}
	xfer_sz = size + xfer_cnt;

	/*
	 * when xfer unit is not aligned with 8,,
	 *  padding should be considered.
	*/
	if (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		padd_u = DSIM_FCMD_ALIGN_CONSTRAINT -(xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT);
		padd_c = xfer_cnt - 1;
		padd_t = padd_u * padd_c;
		xfer_sz += padd_t;
	}

	if (xfer_sz > FCMD_DATA_MAX_SIZE) {
		dsim_err(dsim, "fcmd transfer size error(%d, %d)\n", size, xfer_sz);
		return -1;
	}

	if (dsim->fcmd_buf_allocated)
		dev_info(dsim->dev, "memory for fcmd%d has been already allocated\n", dsim->id);
	else {
		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret) {
			dsim_err(dsim, "dsim%d : dsim_alloc_fcmd_memory fail\n", dsim->id);
			return -1;
		}
	}

	fcmd_data = (u8 *)dsim->fcmd_buf_vaddr;
	fcmd_data[0] = FCMD_SEND_START;
	for (i = 1; i < xfer_sz; i++) {
		if ((i % xfer_unit) == 0)
			fcmd_data[i] = FCMD_SEND_CONTINUE;
		else
			fcmd_data[i] = i % 256;
	}

	dsim_info(dsim, "input size=%d, xfer size=%d, xfer unit=%d, fcmd_buf_addr=0x%lx\n",
			size, xfer_sz, xfer_unit, dsim->fcmd_buf_data.dma_addr);

	ret = dsim_write_data_fcmd(dsim, id, dsim->fcmd_buf_data.dma_addr,
					xfer_sz, xfer_unit, true);
	if (ret)
		dsim_err(dsim, "fcmd write error(%d)\n", ret);

	dsim_info(dsim, "-\n");

	return count;
}

static DEVICE_ATTR(fcmd_wr, 0200,
	NULL,
	dsim_fcmd_write_sysfs_store);
#endif

static void dsim_cmd_timeout_handler(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, cmd_timer);
	queue_work(system_unbound_wq, &dsim->cmd_work);
}

static void dsim_cmd_fail_detector(struct work_struct *work)
{
	struct dsim_device *dsim = container_of(work, struct dsim_device, cmd_work);

	dsim_debug(dsim, "+\n");

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_err(dsim, "DSIM is not ready. state(%d)\n",
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->cmd_timer)
			&& dsim_reg_header_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->ph_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		goto exit;
	}

exit:
	mutex_unlock(&dsim->cmd_lock);

	dsim_debug(dsim, "-\n");
}

static int dsim_wait_datalane_stop_state(struct dsim_device *dsim)
{
	int cnt = 5000;
	enum dsim_datalane_status s;

	do {
		s = dsim_reg_get_datalane_status(dsim->id);

		if (dsim_reg_payload_fifo_is_empty(dsim->id) &&
				(s == DSIM_DATALANE_STATUS_STOPDATA))

			break;

		usleep_range(10, 11);
		cnt--;
	} while (cnt);

	return cnt;
}

static int dsim_wait_for_cmd_fifo_empty(struct dsim_device *dsim, bool must_wait)
{
	int ret = 0;

	if (!must_wait) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id))
			del_timer(&dsim->cmd_timer);

		dsim_debug(dsim, "Doesn't need to wait fifo_completion\n");
		goto exit;
	}

	del_timer(&dsim->cmd_timer);
	dsim_debug(dsim, "Waiting for fifo_completion...\n");

	if (!wait_for_completion_timeout(&dsim->ph_wr_comp, MIPI_WR_TIMEOUT)) {
		if (dsim_reg_header_fifo_is_empty(dsim->id)) {
			reinit_completion(&dsim->ph_wr_comp);
			dsim_reg_clear_int(dsim->id,
					DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
			goto exit;
		}
		ret = -ETIMEDOUT;
	}

	if ((is_dsim_enabled(dsim)) && (ret == -ETIMEDOUT))
		dsim_err(dsim, "have timed out\n");

exit:
	if (ret == 0 && dsim->wait_lp11_after_cmds)
		dsim_wait_datalane_stop_state(dsim);

	return ret;
}

static void dsim_long_data_wr(struct dsim_device *dsim, const u8 buf[], size_t len)
{
	const u8 *end = buf + len;
	u32 payload = 0;

	dsim_debug(dsim, "payload length(%zu)\n", len);

	while (buf < end) {
		size_t pkt_size = min_t(size_t, 4, end - buf);

		if (pkt_size == 4)
			payload = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
		else if (pkt_size == 3)
			payload = buf[0] | buf[1] << 8 | buf[2] << 16;
		else if (pkt_size == 2)
			payload = buf[0] | buf[1] << 8;
		else if (pkt_size == 1)
			payload = buf[0];

		dsim_reg_wr_tx_payload(dsim->id, payload);
		dsim_debug(dsim, "pkt_size %zu payload: %#x\n", pkt_size, payload);

		buf += pkt_size;
	}

	dsim->burst_cmd.pl_count += ALIGN(len, 4);
}

static bool dsim_fifo_empty_needed(struct dsim_device *dsim,
		unsigned int data_id, u8 data0)
{
	/* read case or partial update command */
	if (data_id == MIPI_DSI_DCS_READ
			|| data0 == MIPI_DCS_SET_COLUMN_ADDRESS
			|| data0 == MIPI_DCS_SET_PAGE_ADDRESS) {
		dsim_debug(dsim, "id:%d, data=%#x\n", data_id, data0);
		return true;
	}

	/* Check a FIFO level whether writable or not */
	if (!dsim_reg_is_writable_fifo_state(dsim->id))
		return true;

	return false;
}

static bool dsim_is_writable_pl_fifo_status(struct dsim_device *dsim, u32 word_cnt)
{
	if ((DSIM_PL_FIFO_THRESHOLD - dsim->burst_cmd.pl_count) > word_cnt)
		return true;
	else
		return false;
}

int dsim_check_ph_threshold(struct dsim_device *dsim, u32 cmd_cnt)
{
	int cnt = 5000;
	u32 available = 0;

	available = dsim_reg_is_writable_ph_fifo_state(dsim->id, cmd_cnt);

	/* Wait FIFO empty status during 50ms */
	if (!available) {
		dsim_debug(dsim, "waiting for PH FIFO empty\n");
		do {
			if (dsim_reg_header_fifo_is_empty(dsim->id))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}
	return cnt;
}

int dsim_check_pl_threshold(struct dsim_device *dsim, u32 d1)
{
	int cnt = 5000;

	if (!dsim_is_writable_pl_fifo_status(dsim, d1)) {
		do {
			if (dsim_reg_payload_fifo_is_empty(dsim->id)) {
				dsim->burst_cmd.pl_count = 0;
				break;
			}
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}

	return cnt;
}

#define PACKET_TYPE(p) ((p)->header[0] & 0x3f)
static int
__dsim_write_data(struct dsim_device *dsim, const struct mipi_dsi_packet *packet)
{
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	u8 type = PACKET_TYPE(packet);

	DPU_EVENT_LOG("DSIM_COMMAND", exynos_crtc, EVENT_FLAG_LONG,
			dpu_dsi_packet_to_string, packet);

	if (dsim_reg_payload_fifo_is_empty(dsim->id))
		dsim->burst_cmd.pl_count = 0;

	if (!dsim_check_ph_threshold(dsim, 1)) {
		dsim_err(dsim, "ID(%#x): ph(%#x) don't available\n",
				type, packet->header[1]);
		return -EINVAL;
	}

	if (mipi_dsi_packet_format_is_long(type)) {
		if (!dsim_check_pl_threshold(dsim, ALIGN(packet->payload_length, 4))) {
			dsim_err(dsim, "ID(%#x): pl(%#x) don't available\n",
					type, packet->header[1]);
			dsim_err(dsim, "pl_threshold(%d) pl_cnt(%d) wc(%d)\n",
					DSIM_PL_FIFO_THRESHOLD,
					dsim->burst_cmd.pl_count,
					packet->payload_length);
			return -EINVAL;
		}
		dsim_long_data_wr(dsim, packet->payload, packet->payload_length);
	}

	dsim_reg_wr_tx_header(dsim->id, type, packet->header[1], packet->header[2],
			exynos_mipi_dsi_packet_format_is_read(type));

	return 0;
}

int dsim_write_cmd_set(struct dsim_device *dsim,
		       struct mipi_dsi_msg *msgs, int msg_cnt,
		       bool wait_vsync, bool wait_fifo)
{
	struct mipi_dsi_msg *msg;
	struct mipi_dsi_packet packet;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int i;
	int ret = 0;

	dsim_debug(dsim, "+\n");

	if (dsim->config.burst_cmd_en == 0) {
		dsim_err(dsim, "MIPI burst command is not supported\n");
		ret = -EINVAL;
		goto err_exit;
	}

	if (!dsim->burst_cmd.init_done) {
		dsim_reg_set_burst_cmd(dsim->id, dsim->config.mode);
		dsim->burst_cmd.init_done = 1;
	}

	dsim_debug(dsim, "total_msg_cnt:%d\n", msg_cnt);

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	for (i = 0; i < msg_cnt; i++) {
		msg = &msgs[i];

		if (exynos_mipi_dsi_packet_format_is_read(msg->type))
			goto err_exit;

		ret = mipi_dsi_create_packet(&packet, msg);
		if (ret) {
			dsim_info(dsim, "ID(%#x): is not supported.\n",	msg->type);
			goto err_exit;
		}

		ret = __dsim_write_data(dsim, &packet);
		if (ret)
			goto err_exit;
	}

	if (wait_vsync && exynos_crtc)
		drm_crtc_wait_one_vblank(&exynos_crtc->base);

	/* set packet go ready*/
	dsim_reg_set_packetgo_ready(dsim->id);
	ret = dsim_wait_for_cmd_fifo_empty(dsim, wait_fifo);
	if (ret < 0)
		dsim_err(dsim, "MIPI cmd set tx timeout\n");


	dsim_debug(dsim, "-\n");

err_exit:

	return ret;
}

static void dsim_check_cmd_transfer_mode(struct dsim_device *dsim,
		const struct mipi_dsi_msg *msg)
{
	bool use_lp = 0;
	bool status_lp = 0;

	use_lp = (msg->flags & MIPI_DSI_MSG_USE_LPM ? 1 : 0);
	status_lp = dsim_reg_get_cmd_tansfer_mode(dsim->id);

	if (use_lp != status_lp) {
		dsim_info(dsim, "MIPI %s command transfer\n",
				use_lp ? "LowPower" : "HighSpeed");
		dsim_reg_wait_pl_fifo_is_empty_timeout(dsim->id, 100000);
		dsim_reg_set_cmd_transfer_mode(dsim->id, use_lp);
	}
}

int dsim_host_cmdset_transfer(struct mipi_dsi_host *host,
			      struct mipi_dsi_msg *msgs, int msg_cnt,
			      bool wait_vsync, bool wait_fifo)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	bool doze_suspend;
	int ret = 0;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	mutex_lock(&dsim->cmd_lock);
	doze_suspend = is_dsim_doze_suspended(dsim);
	if (!is_dsim_enabled(dsim) && !doze_suspend) {
		dsim_err(dsim, "Not ready(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	if (doze_suspend)
		dsim_enable_locked(&dsim->encoder);

	dsim_check_cmd_transfer_mode(dsim, &msgs[0]);

	ret = dsim_write_cmd_set(dsim, msgs, msg_cnt, wait_vsync, wait_fifo);

	if (doze_suspend)
		dsim_disable_locked(&dsim->encoder);

err_exit:
	mutex_unlock(&dsim->cmd_lock);

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}
EXPORT_SYMBOL(dsim_host_cmdset_transfer);

static bool dsim_check_set_tear_on(const struct mipi_dsi_msg *msg)
{
	u8 data_id = *(u8 *)msg->tx_buf;
	int len = msg->tx_len;

	if (data_id == MIPI_DCS_SET_TEAR_ON && len == 2)
		return true;

	return false;
}

static void
dsim_create_new_dsi_msg(const struct mipi_dsi_msg *old_msg, struct mipi_dsi_msg *new_msg)
{
	memcpy(new_msg, old_msg, sizeof(struct mipi_dsi_msg));
	new_msg->type = MIPI_DSI_DCS_LONG_WRITE;
};

static int
dsim_write_data(struct dsim_device *dsim, const struct mipi_dsi_msg *msg)
{
	struct mipi_dsi_packet packet;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	int ret = 0;
	bool must_wait;
	u8 type;
	bool use_new_msg = false, cmd_allow = false;
	struct mipi_dsi_msg new_msg;

	if (dsim_check_set_tear_on(msg)) {
		if (is_version_above(&dsim->config.version, 2, 9))
			cmd_allow = true;
		else {
			dsim_debug(dsim, "tear_on : sending as long packet\n");
			dsim_create_new_dsi_msg(msg, &new_msg);
			use_new_msg = true;
		}
	}

	if (!use_new_msg)
		ret = mipi_dsi_create_packet(&packet, msg);
	else
		ret = mipi_dsi_create_packet(&packet, &new_msg);
	type = PACKET_TYPE(&packet);
	if (ret) {
		dsim_info(dsim, "ID(%#x): is not supported.\n", type);
		return ret;
	}

	/* W/A: sending 0x35 packet as long packet to avoid dsim stuck */
	if (*((const u8 *)msg->tx_buf) == MIPI_DCS_SET_TEAR_ON && msg->tx_len == 2) {
		dsim_info(dsim, "tear_on : sending as long packet\n");
		packet.header[0] = ((msg->channel & 0x3) << 6) | MIPI_DSI_DCS_LONG_WRITE;
		packet.header[1] = (msg->tx_len >> 0) & 0xff;
		packet.header[2] = (msg->tx_len >> 8) & 0xff;
		packet.payload_length = msg->tx_len;
		packet.payload = msg->tx_buf;
	}

	if (exynos_crtc) {
		const struct exynos_drm_crtc_ops *ops = exynos_crtc->ops;

		/* force to exit pll sleep before starting command transfer */
		if (ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, true);
	}

	if (dsim->config.burst_cmd_en && !dsim->burst_cmd.init_done) {
		dsim_reg_set_burst_cmd(dsim->id, dsim->config.mode);
		dsim->burst_cmd.init_done = 1;
	}

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Run write-fail dectector */
	mod_timer(&dsim->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	if (cmd_allow)
		dsim_reg_set_cmd_allow(dsim->id, 1);
	ret = __dsim_write_data(dsim, &packet);
	if (cmd_allow)
		dsim_reg_set_cmd_allow(dsim->id, 0);
	if (ret)
		goto err_exit;

	must_wait = dsim_fifo_empty_needed(dsim, type, packet.header[1]);

	/* set packet go ready*/
	if (dsim->config.burst_cmd_en)
		dsim_reg_set_packetgo_ready(dsim->id);

	ret = dsim_wait_for_cmd_fifo_empty(dsim, must_wait);
	if (ret < 0)
		dsim_err(dsim, "ID(%#x): cmd wr timeout %#x\n",	type, packet.header[1]);

err_exit:

	return ret ? ret : msg->tx_len;
}

void dsim_wait_pending_vblank(struct dsim_device *dsim)
{
	if (!dsim) {
		pr_err("dsim is NULL!\n");
		return;
	}

	dsim_reg_wait_clear_int(dsim->id, DSIM_INTSRC_VT_STATUS);
}

static int
dsim_set_max_pkt_size(struct dsim_device *dsim, const struct mipi_dsi_msg *msg)
{
	u8 tx[2] = { msg->rx_len & 0xff, msg->rx_len >> 8 };
	struct mipi_dsi_msg pkt_size_msg = {
		.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
		.tx_len = sizeof(tx),
		.tx_buf = tx,
	};

	/* Set the maximum packet size returned */
	return dsim_write_data(dsim, &pkt_size_msg);
}

static void dsim_clear_rx_fifo(struct dsim_device *dsim)
{
	u32 depth =  DSIM_RX_FIFO_MAX_DEPTH;

	while (depth-- && !dsim_reg_rx_fifo_is_empty(dsim->id))
		dsim_reg_get_rx_fifo(dsim->id);

	if (!depth)
		dsim_err(dsim, "rxfifo was incompletely cleared\n");
}

#define DSIM_RX_PHK_HEADER_SIZE	4
static int dsim_read_data(struct dsim_device *dsim, const struct mipi_dsi_msg *msg)
{
	u8 *rx_buf = msg->rx_buf;
	u32 rx_fifo, rx_size = 0;
	int i = 0, ret = 0;

	if (msg->rx_len > DSIM_RX_FIFO_MAX_DEPTH * 4 - DSIM_RX_PHK_HEADER_SIZE) {
		dsim_err(dsim, "requested rx size is wrong(%zu)\n", msg->rx_len);
		return -EINVAL;
	}

	dsim_debug(dsim, "type[%#x], cmd[%#x], rx_len[%zu]\n",
			msg->type, *(u8 *)msg->tx_buf, msg->rx_len);

	/* Init RX FIFO before read and clear DSIM_INTSRC */
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_RX_DATA_DONE);

	reinit_completion(&dsim->rd_comp);

	dsim_set_max_pkt_size(dsim, msg);

	/* Read request */
	dsim_write_data(dsim, msg);

	if (!wait_for_completion_timeout(&dsim->rd_comp, MIPI_RD_TIMEOUT)) {
		dsim_err(dsim, "read timeout\n");
		return -ETIMEDOUT;
	}

	rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
	dsim_debug(dsim, "rx fifo:0x%8x\n", rx_fifo);

	/* Parse the RX packet data types */
	switch (rx_fifo & 0xff) {
	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		ret = dsim_reg_rx_err_handler(dsim->id, rx_fifo);
		if (ret < 0) {
			dsim_dump(dsim);
			goto exit;
		}
		break;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dsim_debug(dsim, "EoTp was received\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		rx_buf[1] = (rx_fifo >> 16) & 0xff;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
		rx_buf[0] = (rx_fifo >> 8) & 0xff;
		dsim_debug(dsim, "short packet was received\n");
		rx_size = msg->rx_len;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		dsim_debug(dsim, "long packet was received\n");
		rx_size = (rx_fifo & 0x00ffff00) >> 8;

		while (i < rx_size) {
			const u32 rx_max =
				min_t(u32, rx_size, i + sizeof(rx_fifo));

			rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
			dsim_debug(dsim, "payload: 0x%x i=%d max=%d\n", rx_fifo,
					i, rx_max);
			for (; i < rx_max; i++, rx_fifo >>= 8)
				rx_buf[i] = rx_fifo & 0xff;
		}
		break;
	default:
		dsim_err(dsim, "packet format is invalid.\n");
		dsim_dump(dsim);
		ret = -EBUSY;
		goto exit;
	}

	if (!dsim_reg_rx_fifo_is_empty(dsim->id)) {
		dsim_err(dsim, "RX FIFO is not empty\n");
		rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
		if (rx_fifo && 0xFF == MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT)
			dsim_reg_rx_err_handler(dsim->id, rx_fifo);

		dsim_dump(dsim);
		dsim_clear_rx_fifo(dsim);
		ret = -EBUSY;
	} else  {
		ret = rx_size;
	}
exit:

	return ret;
}

static ssize_t dsim_host_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim = host_to_dsi(host);
	const struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	bool doze_suspend;
	int ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	mutex_lock(&dsim->cmd_lock);
	doze_suspend = is_dsim_doze_suspended(dsim);
	if (!is_dsim_enabled(dsim) && !doze_suspend) {
		dsim_err(dsim, "Not ready(%d)\n", dsim->state);
		ret = -EINVAL;
		goto exit;
	}

	if (doze_suspend)
		dsim_enable_locked(&dsim->encoder);

	dsim_check_cmd_transfer_mode(dsim, msg);

	if (exynos_mipi_dsi_packet_format_is_read(msg->type))
		ret = dsim_read_data(dsim, msg);
	else
		ret = dsim_write_data(dsim, msg);

	if (doze_suspend)
		dsim_disable_locked(&dsim->encoder);

exit:
	mutex_unlock(&dsim->cmd_lock);

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}

/* TODO: Below operation will be registered after panel driver is created. */
static const struct mipi_dsi_host_ops dsim_host_ops = {
	.attach = dsim_host_attach,
	.detach = dsim_host_detach,
	.transfer = dsim_host_transfer,
};

static ssize_t bist_mode_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	return scnprintf(buf, PAGE_SIZE, "%d\n", dsim->bist_mode);
}

static ssize_t bist_mode_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);
	int rc;
	unsigned int bist_mode;
	bool bist_en;

	rc = kstrtouint(buf, 0, &bist_mode);
	if (rc < 0)
		return rc;

	/*
	 * BIST modes:
	 * 0: Disable, 1: Color Bar, 2: GRAY Gradient, 3: User-Defined,
	 * 4: Prbs7 Random
	 */
	if (bist_mode > DSIM_BIST_MODE_MAX) {
		dsim_err(dsim, "invalid bist mode\n");
		return -EINVAL;
	}

	bist_en = bist_mode > 0;

	mutex_lock(&dsim->cmd_lock);
	if (bist_en && dsim->state == DSIM_STATE_SUSPEND)
		dsim_enable_locked(&dsim->encoder);

	dsim_reg_set_bist(dsim->id, bist_en, bist_mode - 1);
	dsim->bist_mode = bist_mode;

	if (!bist_en && dsim->state == DSIM_STATE_HSCLKEN)
		dsim_disable_locked(&dsim->encoder);
	mutex_unlock(&dsim->cmd_lock);

	dsim_info(dsim, "0:Disable 1:ColorBar 2:GRAY Gradient 3:UserDefined\n");
	dsim_info(dsim, "4:Prbs7 Random (%d)\n", dsim->bist_mode);

	return len;
}
static DEVICE_ATTR_RW(bist_mode);

/*
 * rmem_device_init is called in of_reserved_mem_device_init_by_idx function
 * when reserved memory is required.
 */
static int rmem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct exynos_drm_encoder *exynos_encoder = dev_get_drvdata(dev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	dsim_info(dsim, "+\n");
	dsim->fb_handover.phys_addr = rmem->base;
	dsim->fb_handover.phys_size = rmem->size;
	dsim_info(dsim, " base %pa size %pa\n",
			&dsim->fb_handover.phys_addr,
			&dsim->fb_handover.phys_size);
	dsim_info(dsim, "-\n");

	return 0;
}

/*
 * rmem_device_release is called in of_reserved_mem_device_release function
 * when reserved memory is no longer required.
 */
static void rmem_device_release(struct reserved_mem *rmem, struct device *dev)
{
	struct page *first = phys_to_page(PAGE_ALIGN(rmem->base));
	struct page *last = phys_to_page((rmem->base + rmem->size) & PAGE_MASK);
	struct page *page;

	pr_info("%s: base=%pa, size=%pa, first=%pa, last=%pa\n",
			__func__, &rmem->base, &rmem->size, first, last);

	for (page = first; page != last; page++) {
		__ClearPageReserved(page);
		set_page_count(page, 1);
		__free_pages(page, 0);
		adjust_managed_page_count(page, 1);
	}
}

static const struct reserved_mem_ops rmem_ops = {
	.device_init    = rmem_device_init,
	.device_release = rmem_device_release,
};

static int dsim_probe(struct platform_device *pdev)
{
	int ret;
	struct dsim_device *dsim;

	pr_info("%s +\n", __func__);

	dsim = devm_kzalloc(&pdev->dev, sizeof(*dsim), GFP_KERNEL);
	if (!dsim)
		return -ENOMEM;

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	dsim->dsi_host.ops = &dsim_host_ops;
	dsim->dsi_host.dev = &pdev->dev;
	dsim->dev = &pdev->dev;

	platform_set_drvdata(pdev, &dsim->encoder);

	ret = dsim_parse_dt(dsim);
	if (ret)
		goto err;

	dsim_drvdata[dsim->id] = dsim;

	dsim->output_type = (dsim->id == 0) ?
			EXYNOS_DISPLAY_TYPE_DSI0 : EXYNOS_DISPLAY_TYPE_DSI1;

	spin_lock_init(&dsim->slock);
	mutex_init(&dsim->cmd_lock);
	init_completion(&dsim->rd_comp);
	init_completion(&dsim->ph_wr_comp);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	init_completion(&dsim->pl_wr_comp);
#endif

	/* prevent sleep enter during dsim on */
	ret = device_init_wakeup(dsim->dev, true);
	if (ret) {
		dev_err(dsim->dev, "failed to init wakeup device\n");
		goto err;
	}

	ret = dsim_init_resources(dsim);
	if (ret)
		goto err;

	ret = device_create_file(dsim->dev, &dev_attr_bist_mode);
	if (ret < 0)
		dsim_err(dsim, "failed to add sysfs entries\n");

	timer_setup(&dsim->cmd_timer, dsim_cmd_timeout_handler, 0);
	INIT_WORK(&dsim->cmd_work, dsim_cmd_fail_detector);

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	timer_setup(&dsim->fcmd_timer, dsim_fcmd_timeout_handler, 0);
	dsim->dsimfc = get_dsimfc_drvdata(dsim->id);
	INIT_WORK(&dsim->fcmd_work, dsim_fcmd_fail_detector);
#endif

	dsim->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev), 1);
	dsim_info(dsim, "dsim idle_ip_index[%d]\n", dsim->idle_ip_index);
	if (dsim->idle_ip_index < 0)
		dsim_warn(dsim, "idle ip index is not provided\n");

	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);

	dsim->state = DSIM_STATE_SUSPEND;
	pm_runtime_enable(dsim->dev);

	pm_runtime_get_sync(dsim->dev);

	/* check dsim already init from LK */
	if (dsim_reg_get_link_clock(dsim->id)) {
		bool is_vm = false;

		dsim->state = DSIM_STATE_INIT;
		enable_irq(dsim->irq);
		is_vm = dsim_reg_get_display_mode(dsim->id);
		if (is_vm && dsim->fb_handover.reserved)
			exynos_update_ip_idle_status(dsim->idle_ip_index, 0);
	}
	pm_runtime_put_sync(dsim->dev);

	if (!IS_ENABLED(CONFIG_BOARD_EMULATOR)) {
		phy_init(dsim->res.phy);
		if (dsim->res.phy_ex)
			phy_init(dsim->res.phy_ex);
	}

	dsim->freq_hop = dpu_register_freq_hop(dsim);

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	if (!dsim->id) {
		ret = device_create_file(dsim->dev, &dev_attr_fcmd_wr);
		if (ret)
			dsim_err(dsim, "failed to create fast command write sysfs\n");
		else
			dsim_info(dsim, "succeed in creating fast command write sysfs");

		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret < 0)
			dsim_err(dsim, "failed to alloc fast command memory\n");
	}
#endif

	dsim_info(dsim, "driver has been probed.\n");
	return component_add(dsim->dev, &dsim_component_ops);

err:
	dsim_err(dsim, "failed to probe exynos dsim driver\n");
	return ret;
}

bool dsim_condition_check(const struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct dsim_device *dsim;
	bool ret = false;

	exynos_crtc = to_exynos_crtc(crtc);
	if (!exynos_crtc)
		return ret;

	dsim = decon_get_dsim(exynos_crtc->ctx);
	if (!dsim)
		return ret;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	mutex_lock(&dsim->cmd_lock);
	if (!is_dsim_enabled(dsim))
		goto exit;

	ret = dsim_reg_check_ppi_stuck(dsim->id);

exit:
	mutex_unlock(&dsim->cmd_lock);
	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return ret;
}

static int dsim_remove(struct platform_device *pdev)
{
	struct exynos_drm_encoder *exynos_encoder = platform_get_drvdata(pdev);
	struct dsim_device *dsim = encoder_to_dsim(exynos_encoder);

	device_remove_file(dsim->dev, &dev_attr_bist_mode);
	pm_runtime_disable(&pdev->dev);

	component_del(&pdev->dev, &dsim_component_ops);

	return 0;
}

struct platform_driver dsim_driver = {
	.probe = dsim_probe,
	.remove = dsim_remove,
	.driver = {
		.name = "exynos-drmdsim",
		.owner = THIS_MODULE,
		.of_match_table = dsim_of_match,
	},
};

MODULE_SOFTDEP("pre: samsung_dma_heap");
MODULE_SOFTDEP("pre: phy-exynos-mipi");
MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC MIPI DSI Master");
MODULE_LICENSE("GPL v2");
