// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_freq_hop.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 * Authors:
 *	Hwangjae Lee <hj-yo.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <uapi/linux/sched/types.h>

#include <dpu_trace.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_dsim.h>

#include <dt-bindings/display/rf-band-id.h>
#include <mcd_drm_dsim.h>
#include <linux/dev_ril_bridge.h>

#include <drm/drm_bridge.h>
#include "../panel/panel-samsung-drv.h"


struct __packed ril_noti_info {
	u8 rat;
	u32 band;
	u32 channel;
};


static int dsim_set_freq_hop(struct dsim_device *dsim, struct dsim_freq_hop *freq)
{
	struct stdphy_pms *pms;

	if (dsim->state != DSIM_STATE_HSCLKEN) {
		pr_info("dsim power is off state(0x%x)\n", dsim->state);
		return -EINVAL;
	}

	pms = &dsim->config.dphy_pms;
	dsim_reg_set_dphy_freq_hopping(dsim->id, pms->p, freq->target_m,
			freq->target_k,	(freq->target_m > 0) ? 1 : 0);

	return 0;
}

void dpu_update_freq_hop(struct exynos_drm_crtc *exynos_crtc)
{
	struct stdphy_pms *pms;
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_pll_params *pll_params;


	if (!IS_ENABLED(CONFIG_EXYNOS_FREQ_HOP))
		return;

	if (!dsim)
		return;

	if (dsim->freq_hop.enabled == false) {

		pll_params = dsim->pll_params;

		pms = &dsim->config.dphy_pms;
		pms->freq_hop_m = pll_params->params[0]->m;
		pms->freq_hop_k = pll_params->params[0]->k;

		pr_info("FREQ_HOP: %s Set initial value m: %d, k: %d\n",
			__func__, pms->freq_hop_m, pms->freq_hop_k);

		dsim->freq_hop.request_m = dsim->freq_hop.target_m = pms->freq_hop_m;
		dsim->freq_hop.request_k = dsim->freq_hop.target_k = pms->freq_hop_k;

		dsim->freq_hop.enabled = true;
	}

	if (dsim->freq_hop.target_m != dsim->freq_hop.request_m)
		dsim->freq_hop.target_m = dsim->freq_hop.request_m;

	if (dsim->freq_hop.target_k != dsim->freq_hop.request_k)
		dsim->freq_hop.target_k = dsim->freq_hop.request_k;

}


#ifdef CRTC_SET_DSI_CLCOK
struct drm_bridge *get_drm_dsi_bridge(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon;
	struct drm_encoder *encoder;

	decon = (struct decon_device *)exynos_crtc->ctx;
	if (!decon) {
		pr_err("ERR: %s - decon is null\n");
		goto exit;
	}

	encoder = decon_get_encoder(decon, DRM_MODE_ENCODER_DSI);
	if (!encoder)
		goto exit;

	return drm_bridge_chain_get_first_bridge(encoder);
exit:
	return NULL;
}

static void set_panel_clock_info(struct exynos_drm_crtc *exynos_crtc, struct panel_clock_info *info)
{
	struct drm_bridge *bridge;
	struct exynos_panel *panel;
	const struct exynos_panel_funcs *funcs;

	bridge = get_drm_dsi_bridge(exynos_crtc);
	if (!bridge)
		return;

	panel = bridge_to_exynos_panel(bridge);
	funcs = panel->desc->exynos_panel_func;
	if (funcs->req_set_clock)
		funcs->req_set_clock(panel, info);

	return;
}
#endif

void dpu_set_freq_hop(struct exynos_drm_crtc *exynos_crtc, bool en)
{
	struct stdphy_pms *pms;
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop freq_hop;
	u32 target_m, target_k;

	if (!IS_ENABLED(CONFIG_EXYNOS_FREQ_HOP))
		return;

	if (!dsim || !dsim->freq_hop.enabled)
		return;

	target_m = dsim->freq_hop.target_m;
	target_k = dsim->freq_hop.target_k;

	pms = &dsim->config.dphy_pms;
	if ((pms->freq_hop_m != target_m) || (pms->freq_hop_k != target_k)) {
		if (en) {
			dsim_set_freq_hop(dsim, &dsim->freq_hop);
			DPU_EVENT_LOG("FREQ_HOP", exynos_crtc, 0, "[m: %d->%d, k: %d->%d]",
					pms->freq_hop_m, pms->freq_hop_k, target_m, target_k);
		} else {
			pms->freq_hop_m = dsim->freq_hop.target_m;
			pms->freq_hop_k = dsim->freq_hop.target_k;
			freq_hop.target_m = 0;

			dsim_set_freq_hop(dsim, &freq_hop);
		}
	}
}

struct dpu_freq_hop_ops dpu_freq_hop_control = {
	.set_freq_hop	 = dpu_set_freq_hop,
	.update_freq_hop = dpu_update_freq_hop,
};

static int dpu_debug_freq_hop_show(struct seq_file *s, void *unused)
{
	struct decon_device *decon = s->private;
	struct dsim_device *dsim = decon_get_dsim(decon);

	if (!dsim || !dsim->freq_hop.enabled)
		return 0;

	seq_printf(s, "m(%u) k(%u)\n", dsim->freq_hop.request_m,
			dsim->freq_hop.request_k);

	return 0;
}

static int dpu_debug_freq_hop_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_debug_freq_hop_show, inode->i_private);
}

static ssize_t dpu_debug_freq_hop_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_device *dsim = decon_get_dsim(decon);
	char *buf_data;
	int ret;

	if (!dsim || !dsim->freq_hop.enabled)
		return count;

	if (!count)
		return count;

	buf_data = kmalloc(count, GFP_KERNEL);
	if (buf_data == NULL)
		return count;

	memset(buf_data, 0, count);

	ret = copy_from_user(buf_data, buf, count);
	if (ret < 0)
		goto out;

	mutex_lock(&dsim->freq_hop_lock);
	ret = sscanf(buf_data, "%u %u", &dsim->freq_hop.request_m,
			&dsim->freq_hop.request_k);
	mutex_unlock(&dsim->freq_hop_lock);
	if (ret < 0)
		goto out;

out:
	kfree(buf_data);
	return count;
}

static const struct file_operations dpu_freq_hop_fops = {
	.open = dpu_debug_freq_hop_open,
	.write = dpu_debug_freq_hop_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* This function is called decon_late_register */
void dpu_freq_hop_debugfs(struct exynos_drm_crtc *exynos_crtc)
{
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debug_request_mk;
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc *crtc = &exynos_crtc->base;

	decon->crtc->freq_hop = &dpu_freq_hop_control;
	debug_request_mk = debugfs_create_file("request_mk", 0444,
			crtc->debugfs_entry, decon, &dpu_freq_hop_fops);

	if (!debug_request_mk)
		pr_err("failed to create debugfs debugfs request_mk directory\n");
#endif
}

enum {
	RADIO_RANGE_FROM = 0,
	RADIO_RANGE_TO,
	RADIO_RANGE_MAX
};

struct radio_band_info {
	u32 ranges[RADIO_RANGE_MAX];
	u32 dsi_freq;
	u32 osc_freq;
	struct list_head list;
};

#define DT_NAME_FREQ_TABLE		"freq-hop-table"
#define DT_NAME_FREQ_HOP_RF_ID		"id"
#define DT_NAME_FREQ_HOP_RF_RANGE	"range"
#define DT_NAME_FREQ_HOP_HS_FREQ	"dsi_freq"
#define DT_NAME_FREQ_HOP_OSC_FREQ	"osc_freq"

static int of_get_radio_band_info(struct device_node *np, u32 *band_id, struct radio_band_info *band_info)
{
	int ret;

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_RF_ID, band_id);
	if (ret) {
		pr_err("%s: failed to get rf band id\n", __func__);
		return ret;
	}

	ret = of_property_read_u32_array(np, DT_NAME_FREQ_HOP_RF_RANGE, band_info->ranges, RADIO_RANGE_MAX);
	if (ret) {
		pr_err("%s: failed to get rf range info : %d\n", __func__, ret);
		return ret;
	}

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_HS_FREQ, &band_info->dsi_freq);
	if (ret) {
		pr_err("%s: failed to get dsi frequency\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_OSC_FREQ, &band_info->osc_freq);
	if (ret) {
		pr_err("%s: failed to get osc frequency\n", __func__);
		return ret;
	}

	return ret;
}


static void store_hs_clock_list(struct mcd_dsim_device *freq_hop, u32 clock)
{
	int i;

	for (i = 0; i < freq_hop->hs_clk_cnt; i++) {
		if (freq_hop->hs_clk_list[i] == clock)
			return;
	}

	freq_hop->hs_clk_list[freq_hop->hs_clk_cnt] = clock;
	freq_hop->hs_clk_cnt++;
}

static int check_hs_clock_list(struct mcd_dsim_device *freq_hop, struct device_node *np)
{
	int i;
	int ret;

	if (!np) {
		pr_err("%s: Invalid device node\n", __func__);
		return -EINVAL;
	}

	for (i  = 0; i < freq_hop->hs_clk_cnt; i++) {
		ret = mcd_dsim_of_get_pll_param(np, freq_hop->hs_clk_list[i], NULL);
		pr_info("FREQ_HOP: HS clock[%d]: %dkhz, finding pll param .. %s\n",
			i, freq_hop->hs_clk_list[i], ret == 0 ? "Ok" : "Faul");
		if (ret) {
			pr_err("FREQ_HOP: ERR: %s - failed to get pll param\n", __func__);
			BUG();
		}
	}

	return ret;
}


static int get_radio_band_info(struct device_node *np, struct mcd_dsim_device *freq_hop)
{
	int ret;
	u32 band_id;
	struct radio_band_info *band_info;

	if ((!np) || (!freq_hop)) {
		pr_err("FREQ_HOP: ERR: %s - Invalid device node\n", __func__);
		return -EINVAL;
	}

	band_info = kzalloc(sizeof(struct radio_band_info), GFP_KERNEL);
	if (!band_info) {
		pr_err("FREQ_HOP: ERR: %s - out of memory\n", __func__);
		return -ENOMEM;
	}

	ret = of_get_radio_band_info(np, &band_id, band_info);
	if (ret < 0) {
		pr_err("FREQ_HOP: ERR: %s - failed to get radio band info\n", __func__);
		goto err;
	}
	store_hs_clock_list(freq_hop, band_info->dsi_freq);

	list_add_tail(&band_info->list, &freq_hop->channel_head[band_id]);

	return 0;
err:
	kfree(band_info);

	return ret;
}

#ifdef FREQ_HOP_DEBUG
static void dump_link(struct mcd_dsim_device *freq_hop)
{
	int i;
	struct radio_band_info *band_info;

	for (i = 0; i < MAX_BAND_ID; i++) {
		list_for_each_entry(band_info, &freq_hop->channel_head[i], list) {
			pr_info("FREQ_HOP: band_info channel: %3d, range: %6d ~ %6d, dsi:%dkz, osc: %dkz\n", i,
				band_info->ranges[RADIO_RANGE_FROM], band_info->ranges[RADIO_RANGE_TO],
				band_info->dsi_freq, band_info->osc_freq);
		}
	}
}
#endif

static int freq_hop_parse_dt(struct device_node *np, struct mcd_dsim_device *freq_hop)
{
	struct device_node *child, *entry;
	int child_cnt;
	int ret;

	if ((!np) || (!freq_hop)) {
		pr_err("FREQ_HOP: ERR: %s - Invalid device node\n", __func__);
		return -EINVAL;
	}

	child = of_parse_phandle(np, DT_NAME_FREQ_TABLE, 0);
	if (!child) {
		pr_err("FREQ_HOP: ERR: %s - failed to get freq-hop-info\n", __func__);
		return -EINVAL;
	}

	child_cnt = of_get_child_count(child);
	pr_info("FREQ_HOP: %s - freq_hop rf band size: %d\n", __func__, child_cnt);
	if (child_cnt <= 0) {
		pr_err("FREQ_HOP: ERR: %s - can't found rf band info for freq hop\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(child, entry) {
		ret = get_radio_band_info(entry, freq_hop);
		if (ret) {
			pr_err("FREQ_HOP: ERR: %s - failed to get hop_freq\n", __func__);
			return -EINVAL;
		}
	}

	ret = check_hs_clock_list(freq_hop, np);
	if (ret) {
		pr_err("FREQ_HOP: ERR: %s - failed to parse dt\n", __func__);
		WARN_ON(1);
	}

#ifdef FREQ_HOP_DEBUG
	dump_link(freq_hop);
#endif
	return ret;
}



static void freq_hop_init_list(struct mcd_dsim_device *freq_hop)
{
	int i;

	for (i = 0; i < MAX_BAND_ID; i++)
		INIT_LIST_HEAD(&freq_hop->channel_head[i]);
}


static bool is_valid_msg_info(struct dev_ril_bridge_msg *msg)
{
	if (!msg) {
		pr_err("FREQ_HOP: ERR: %s - invalid message\n", __func__);
		return false;
	}

	if (msg->dev_id != IPC_SYSTEM_CP_CHANNEL_INFO) {
		pr_err("FREQ_HOP: ERR: %s - invalid channel info: %d\n", msg->dev_id);
		return false;
	}

	if (msg->data_len != sizeof(struct ril_noti_info)) {
		pr_err("FREQ_HOP: ERR: %s - invalid data len: %d\n", msg->data_len);
		return false;
	}

	return true;
}


struct radio_band_info *freq_hop_search_band_info(struct dev_ril_bridge_msg *msg, struct mcd_dsim_device *freq_hop)
{
	struct list_head *head;
	struct radio_band_info *band_info;
	struct ril_noti_info *rf_info;
	int gap1, gap2;

	if (!freq_hop) {
		pr_err("FREQ_HOP: ERR: %s Invalid freq_hop info\n", __func__);
		return NULL;
	}

	if (!is_valid_msg_info(msg)) {
		pr_err("FREQ_HOP: ERR: %s Invalid ril msg\n", __func__);
		return NULL;
	}

	rf_info = (struct ril_noti_info *)msg->data;
	pr_info("FREQ_HOP: %s - (band:%d, channel:%d)\n", __func__, rf_info->band, rf_info->channel);
	if (rf_info->band >= MAX_BAND_ID) {
		pr_err("FREQ_HOP: ERR: %s - invalid band: %d\n", __func__, rf_info->band);
		return NULL;
	}

	head = &freq_hop->channel_head[rf_info->band];
	list_for_each_entry(band_info, head, list) {

		gap1 = (int)rf_info->channel - (int)band_info->ranges[RADIO_RANGE_FROM];
		gap2 = (int)rf_info->channel - (int)band_info->ranges[RADIO_RANGE_TO];

		if ((gap1 >= 0) && (gap2 <= 0)) {
			pr_info("FREQ_HOP: %s: found!! band: %d, channel: %d ~ %d, hs: %d, ddi osc: %d\n",
				__func__, rf_info->band, band_info->ranges[RADIO_RANGE_FROM],
				band_info->ranges[RADIO_RANGE_TO], band_info->dsi_freq, band_info->osc_freq);

			return band_info;
		}
	}
	pr_err("FREQ_HOP: ERR: %s - Can't found band info (band:%d, channel:%d)\n",
		__func__, rf_info->band, rf_info->channel);

	return NULL;
}


static inline void update_freq_hop_info(struct mcd_dsim_device *hop_info, struct radio_band_info *band_info)
{
	if ((!hop_info) || (!band_info)) {
		pr_err("FREQ_HOP: ERR: %s - invalid param\n", __func__);
		return;
	}

	hop_info->dsi_freq = band_info->dsi_freq;
	hop_info->osc_freq = band_info->osc_freq;

}

static inline void request_set_osc_clock(struct exynos_panel *panel, u32 frequency)
{
	struct panel_clock_info info;
	const struct exynos_panel_funcs *funcs;

	if (!panel) {
		pr_err("FREQ_HOP: ERR: %s - invalid param\n", __func__);
		return;
	}

	info.clock_id = CLOCK_ID_OSC;
	info.clock_rate = frequency;

	funcs = panel->desc->exynos_panel_func;
	if (funcs->req_set_clock)
		funcs->req_set_clock(panel, &info);
}


static inline void request_set_dsi_clock(struct exynos_panel *panel, u32 frequency)
{
	struct panel_clock_info info;
	const struct exynos_panel_funcs *funcs;

	if (!panel) {
		pr_err("FREQ_HOP: ERR: %s - invalid param\n", __func__);
		return;
	}

	info.clock_id = CLOCK_ID_DSI;
	info.clock_rate = frequency;

	funcs = panel->desc->exynos_panel_func;
	if (funcs->req_set_clock)
		funcs->req_set_clock(panel, &info);

}

static void request_set_panel_clock(struct exynos_panel *panel, struct mcd_dsim_device *freq_hop)
{
	if ((!panel) || (!freq_hop)) {
		pr_err("FREQ_HOP: ERR: %s - invalid param\n", __func__);
		return;
	}

	request_set_dsi_clock(panel, freq_hop->dsi_freq);
	request_set_osc_clock(panel, freq_hop->osc_freq);
}

static int radio_notifier(struct notifier_block *self, unsigned long size, void *buf)
{
	int ret;
	struct stdphy_pms pms;
	struct exynos_panel *panel;
	struct radio_band_info *band_info = NULL;
	struct mcd_dsim_device *freq_hop = container_of(self, struct mcd_dsim_device, radio_noti);
	struct dsim_device *dsim = container_of(freq_hop, struct dsim_device, mcd_dsim);

	band_info = freq_hop_search_band_info((struct dev_ril_bridge_msg *)buf, freq_hop);
	if (!band_info) {
		pr_err("ERR:%s failed to search band info\n", __func__);
		goto exit_notifier;
	}

	ret = mcd_dsim_of_get_pll_param(dsim->dev->of_node, band_info->dsi_freq, &pms);
	if (ret) {
		pr_err("FREQ_HOP: ERR: %s: failed to get pll param\n", __func__);
		goto exit_notifier;
	}

	pr_info("FREQ_HOP: %s - found hs: %d, pmsk:%d, %d, %d, %d\n",
		__func__, band_info->dsi_freq, pms.p, pms.m, pms.s, pms.k);

	dsim->freq_hop.request_m = pms.m;
	dsim->freq_hop.request_k = pms.k;

	update_freq_hop_info(freq_hop, band_info);

	panel = bridge_to_exynos_panel(dsim->panel_bridge);
	request_set_panel_clock(panel, freq_hop);

exit_notifier:
	return 0;
}


/* This function is called dsim_probe */
void dpu_init_freq_hop(struct dsim_device *dsim)
{
	int ret;
	struct mcd_dsim_device *freq_hop = &dsim->mcd_dsim;

	if (!IS_ENABLED(CONFIG_EXYNOS_FREQ_HOP)) {
		dsim->freq_hop.enabled = false;
		return;
	}

	/* init value */
	freq_hop->hs_clk_cnt = 0;

	freq_hop_init_list(freq_hop);
	ret = freq_hop_parse_dt(dsim->dev->of_node, freq_hop);
	if (ret) {
		pr_err("FREQ_HOP: ERR: %s: failed to parse dt\n", __func__);
		return;
	}

	freq_hop->radio_noti.notifier_call = radio_notifier;
	register_dev_ril_bridge_event_notifier(&freq_hop->radio_noti);

}
