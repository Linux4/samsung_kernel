/*
 * linux/drivers/video/fbdev/exynos/panel/dynamic_freq.h
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../panel.h"
#include "../panel_drv.h"
#include "dynamic_freq.h"
#ifdef CONFIG_EXYNOS_DPU30_DUAL
#include "../../dpu30_dual/panels/exynos_panel.h"
#else
#include "../../dpu30/panels/exynos_panel.h"
#endif
#include <linux/dev_ril_bridge.h>

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"dynfq"
#endif

static struct dynamic_freq_range *search_dynamic_freq_idx(struct panel_device *panel, int band_idx, int freq)
{
	int i, ret = 0;
	int min, max, array_idx;
	struct df_freq_tbl_info *df_tbl;
	struct dynamic_freq_range *array = NULL;

	if (band_idx >= FREQ_RANGE_MAX) {
		panel_err("exceed max band idx : %d\n", band_idx);
		ret = -1;
		goto search_exit;
	}

	df_tbl = &panel->df_freq_tbl[band_idx];
	if (df_tbl == NULL) {
		panel_err("failed to find band_idx : %d\n", band_idx);
		ret = -1;
		goto search_exit;
	}
	array_idx = df_tbl->size;

	if (array_idx == 1) {
		array = &df_tbl->array[0];
		panel_info("Found adap_freq idx(0): %d, osc: %d\n",
				array->freq_idx, array->ddi_osc);
		return array;
	} else {
		for (i = 0; i < array_idx; i++) {
			array = &df_tbl->array[i];
			panel_info("min : %d, max : %d\n", array->min, array->max);

			min = (int)freq - array->min;
			max = (int)freq - array->max;

			if ((min >= 0) && (max <= 0)) {
				panel_info("Found adap_freq idx: %d, osc: %d\n",
						array->freq_idx, array->ddi_osc);
				return array;
			}
		}

		if (i >= array_idx) {
			panel_err("can't found freq idx\n");
			array = NULL;
			goto search_exit;
		}
	}
search_exit:
	return array;
}

int set_dynamic_freq_ffc(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	struct df_status_info *status = &panel->df_status;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return -ENODEV;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON || !IS_PANEL_ACTIVE(panel))
		return 0;

	mutex_lock(&panel->op_lock);

	if (status->target_df != status->ffc_df) {
		status->ffc_df = status->target_df;
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_DYNAMIC_FFC_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to set PANEL_FFC_SEQ\n");
			goto exit_changed;
		}
	}

exit_changed:
	mutex_unlock(&panel->op_lock);
	return ret;
}

int set_dynamic_freq_ffc_off(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	struct df_status_info *status = &panel->df_status;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return -ENODEV;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON || !IS_PANEL_ACTIVE(panel))
		return 0;

	if (!check_seqtbl_exist(&panel->panel_data, PANEL_DYNAMIC_FFC_OFF_SEQ)) {
		panel_dbg("no PANEL_DYNAMIC_FFC_OFF_SEQ\n");
		return 0;
	}

	mutex_lock(&panel->op_lock);

	if (status->target_df != status->ffc_df) {
		status->ffc_df = MAX_DYNAMIC_FREQ; /* off : make abnormal state */
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_DYNAMIC_FFC_OFF_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to set PANEL_FFC_OFF_SEQ\n", __func__);
			goto exit_changed;
		}
	}

exit_changed:
	mutex_unlock(&panel->op_lock);
	return ret;
}

int dynamic_freq_update(struct panel_device *panel, int idx)
{
	struct df_setting_info *df_setting;
	struct df_status_info *status = &panel->df_status;
	struct exynos_panel_info *lcd_info;

	lcd_info = panel->mipi_drv.get_lcd_info(panel->dsi_id);
	if (!lcd_info) {
		panel_err("failed to get lcd_info\n");
		return -EINVAL;
	}

	if ((idx >= lcd_info->df_set_info.df_cnt) || (idx < 0)) {
		panel_err("invalid idx : %d\n", idx);
		return -EINVAL;
	}

	df_setting = &lcd_info->df_set_info.setting_info[idx];
	panel_info("IDX : %d Setting HS : %d\n",
			idx, df_setting->hs);

	status->request_df = idx;
	status->context = DF_CONTEXT_RIL;

	return 0;
}

static int df_notifier(struct notifier_block *self, unsigned long size, void *buf)
{
	struct panel_device *panel;
	struct dev_ril_bridge_msg *msg;
	struct ril_noti_info *ch_info;
	struct df_status_info *dyn_status;
	struct dynamic_freq_range *freq_info;

	panel = container_of(self, struct panel_device, df_noti);
	if (panel == NULL) {
		panel_err("panel is null\n");
		goto exit_notifier;
	}

	dyn_status = &panel->df_status;
	if (dyn_status == NULL) {
		panel_err("dymanic status is null\n");
		goto exit_notifier;
	};

	if (!dyn_status->enabled) {
		panel_err("df is disabled\n");
		goto exit_notifier;
	}

	msg = (struct dev_ril_bridge_msg *)buf;
	if (msg == NULL) {
		panel_err("msg is null\n");
		goto exit_notifier;
	}

	if (msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO &&
		msg->data_len == sizeof(struct ril_noti_info)) {
		ch_info = (struct ril_noti_info *)msg->data;
		if (ch_info == NULL) {
			panel_err("ch_info is null\n");
			goto exit_notifier;
		}

		panel_info("(b:%d, c:%d)\n",
				ch_info->band, ch_info->channel);

		freq_info = search_dynamic_freq_idx(panel, ch_info->band, ch_info->channel);
		if (freq_info == NULL) {
			panel_info("failed to search freq idx\n");
			goto exit_notifier;
		}
#if 0
		if (freq_info->ddi_osc != 0)
			panel_info("not support dual osc\n");
#endif
		if (freq_info->ddi_osc != dyn_status->current_ddi_osc) {
			panel_info("ddi osc was changed %d -> %d\n",
				dyn_status->current_ddi_osc, freq_info->ddi_osc);
			dyn_status->request_ddi_osc = freq_info->ddi_osc;
		}

		if (freq_info->freq_idx != dyn_status->current_df)
			dynamic_freq_update(panel, freq_info->freq_idx);
	}
exit_notifier:
	return 0;
}

static int init_dynamic_freq_status(struct panel_device *panel)
{
	int ret = 0;
	int cur_idx;
	struct df_status_info *status;
	struct df_setting_info *tune;
	struct exynos_panel_info *lcd_info;

	lcd_info = panel->mipi_drv.get_lcd_info(panel->dsi_id);
	if (!lcd_info) {
		panel_err("failed to get lcd_info\n");
		return -EINVAL;
	}

	status = &panel->df_status;
	if (status == NULL) {
		panel_err("dynamic status is null\n");
		ret = -EINVAL;
		goto init_exit;
	}
	cur_idx = lcd_info->df_set_info.dft_index;
	tune = &lcd_info->df_set_info.setting_info[cur_idx];

	status->enabled = true;
	status->context = DF_CONTEXT_INIT;

	status->request_df = lcd_info->df_set_info.dft_index;

	status->current_df = MAX_DYNAMIC_FREQ;
	status->target_df = MAX_DYNAMIC_FREQ;
	status->ffc_df = MAX_DYNAMIC_FREQ;

init_exit:
	return ret;
}

static int parse_dynamic_freq(struct panel_device *panel)
{
	int ret = 0, i, cnt = 0;
	unsigned int dft_hs = 0;
	struct device_node *freq_node;
	struct df_setting_info *set_info;
	struct device *dev = panel->dev;
	struct device_node *node = panel->ddi_node;
	struct df_dt_info *df;
	struct exynos_panel_info *lcd_info;

	lcd_info = panel->mipi_drv.get_lcd_info(panel->dsi_id);
	if (!lcd_info) {
		panel_err("failed to get lcd_info\n");
		return -EINVAL;
	}

	df = &lcd_info->df_set_info;

	if (node == NULL) {
		panel_err("ddi node is NULL\n");
		node = of_parse_phandle(dev->of_node, "ddi_info", 0);
	}

	cnt = of_property_count_u32_elems(node, "dynamic_freq");
	if (cnt  <= 0) {
		panel_warn("can't found dynamic freq info\n");
		return -EINVAL;
	}

	if (cnt > MAX_DYNAMIC_FREQ) {
		panel_info("freq cnt exceed max freq num (%d:%d)\n",
				cnt, MAX_DYNAMIC_FREQ);
		cnt = MAX_DYNAMIC_FREQ;
	}
	df->df_cnt = cnt;

	of_property_read_u32(node, "timing,dsi-hs-clk", &dft_hs);
	panel_info("default hs clock(%d)\n", dft_hs);
	panel_info("FREQ CNT : %d\n", cnt);

	for (i = 0; i < cnt; i++) {
		freq_node = of_parse_phandle(node, "dynamic_freq", i);
		set_info = &df->setting_info[i];

		of_property_read_u32(freq_node, "hs-clk", &set_info->hs);
		if (dft_hs == set_info->hs) {
			df->dft_index = i;
			panel_info("found default hs idx  : %d\n",
					df->dft_index);
		}
#if 0
		of_property_read_u32_array(freq_node, "cmd_underrun_lp_ref",
				set_info->cmd_underrun_lp_ref,
				lcd_info->dt_lcd_mres.mres_number);
#endif
		of_property_read_u32_array(freq_node, "pmsk", (u32 *)&set_info->dphy_pms,
			sizeof(struct stdphy_pms)/sizeof(unsigned int));

		panel_info("HS_FREQ : %d\n", set_info->hs);
		panel_info("PMS[p] : %d\n", set_info->dphy_pms.p);
		panel_info("PMS[m] : %d\n", set_info->dphy_pms.m);
		panel_info("PMS[s] : %d\n", set_info->dphy_pms.s);
		panel_info("PMS[k] : %d\n", set_info->dphy_pms.k);
	}

	return ret;
}

int dynamic_freq_probe(struct panel_device *panel, struct df_freq_tbl_info *freq_tbl)
{
	int ret = 0;

	if (freq_tbl == NULL) {
		panel_err("frequence set is null");
		panel_err("can't support DF\n");
		goto exit_probe;
	}

	ret = parse_dynamic_freq(panel);
	if (ret) {
		panel_err("faied to parse df\n");
		goto exit_probe;
	}
	panel->df_freq_tbl = freq_tbl;

	panel->df_noti.notifier_call = df_notifier;
	register_dev_ril_bridge_event_notifier(&panel->df_noti);

	init_dynamic_freq_status(panel);

exit_probe:
	return ret;
}
