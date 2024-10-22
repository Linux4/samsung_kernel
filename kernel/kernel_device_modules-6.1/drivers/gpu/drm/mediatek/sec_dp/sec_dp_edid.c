// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SEC Displayport
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/list_sort.h>
#include <linux/bitmap.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "sec_dp_edid.h"
#include "sec_dp_api.h"
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include "displayport_bigdata.h"
#endif

#ifdef FEATURE_MTK_DRM_DP_ENABLED
extern struct mtk_dp *mtk_dp_get_dev(void);

struct drm_connector *mtk_dp_get_connector(void)
{
	struct drm_connector *connector = &mtk_dp_get_dev()->conn;

	return connector;
}
#endif

#ifdef FEATURE_DEX_SUPPORT
static struct dex_support_modes secdp_dex_resolution[] = {
	{1600,  900, MON_RATIO_16_9, DEX_FHD_SUPPORT},
	{1920, 1080, MON_RATIO_16_9, DEX_FHD_SUPPORT},
	{1920, 1200, MON_RATIO_16_10, DEX_WQHD_SUPPORT},
	{2560, 1080, MON_RATIO_21_9, DEX_WQHD_SUPPORT},
	{2560, 1440, MON_RATIO_16_9, DEX_WQHD_SUPPORT},
	{2560, 1600, MON_RATIO_16_10, DEX_WQHD_SUPPORT},
	{3440, 1440, MON_RATIO_21_9, DEX_WQHD_SUPPORT}
};

enum mon_aspect_ratio_t secdp_get_aspect_ratio(struct drm_display_mode *mode)
{
	enum mon_aspect_ratio_t aspect_ratio = MON_RATIO_NA;
	int hdisplay = mode->hdisplay;
	int vdisplay = mode->vdisplay;

	if ((hdisplay == 4096 && vdisplay == 2160) ||
		(hdisplay == 3840 && vdisplay == 2160) ||
		(hdisplay == 2560 && vdisplay == 1440) ||
		(hdisplay == 1920 && vdisplay == 1080) ||
		(hdisplay == 1600 && vdisplay ==  900) ||
		(hdisplay == 1366 && vdisplay ==  768) ||
		(hdisplay == 1280 && vdisplay ==  720))
		aspect_ratio = MON_RATIO_16_9;
	else if ((hdisplay == 2560 && vdisplay == 1600) ||
		(hdisplay  == 1920 && vdisplay == 1200) ||
		(hdisplay  == 1680 && vdisplay == 1050) ||
		(hdisplay  == 1440 && vdisplay ==  900) ||
		(hdisplay  == 1280 && vdisplay ==  800))
		aspect_ratio = MON_RATIO_16_10;
	else if ((hdisplay == 3840 && vdisplay == 1600) ||
		(hdisplay == 3440 && vdisplay == 1440) ||
		(hdisplay == 2560 && vdisplay == 1080))

		aspect_ratio = MON_RATIO_21_9;
	else if ((hdisplay == 1720 && vdisplay == 1440) ||
		(hdisplay == 1280 && vdisplay == 1080))
		aspect_ratio = MON_RATIO_10P5_9;
	else if (hdisplay == 2520 && vdisplay == 1200)
		aspect_ratio = MON_RATIO_21_10;
	else if (hdisplay == 1320 && vdisplay == 1200)
		aspect_ratio = MON_RATIO_11_10;
	else if ((hdisplay == 5120 && vdisplay == 1440) ||
		(hdisplay  == 3840 && vdisplay == 1080))
		aspect_ratio = MON_RATIO_32_9;
	else if (hdisplay == 3840 && vdisplay == 1200)
		aspect_ratio = MON_RATIO_32_10;
	else if ((hdisplay == 1280 && vdisplay == 1024) ||
		(hdisplay  ==  720 && vdisplay ==  576))
		aspect_ratio = MON_RATIO_5_4;
	else if (hdisplay == 1280 && vdisplay == 768)
		aspect_ratio = MON_RATIO_5_3;
	else if ((hdisplay == 1152 && vdisplay == 864) ||
		(hdisplay  == 1024 && vdisplay == 768) ||
		(hdisplay  ==  800 && vdisplay == 600) ||
		(hdisplay  ==  640 && vdisplay == 480))
		aspect_ratio = MON_RATIO_4_3;
	else if (hdisplay == 720 && vdisplay == 480)
		aspect_ratio = MON_RATIO_3_2;

	return aspect_ratio;
}

/*
 * mode comparison for Dex mode
 * from drm_mode_compare() at drm_modes.c
 *
 * Returns:
 * Negative if @lh_a is better than @lh_b, zero if they're equivalent, or
 * positive if @lh_b is better than @lh_a.
 */
int dp_mode_compare_for_dex(void *priv, const struct list_head *lh_a, const struct list_head *lh_b)
{
	struct sec_dp_dev *dp = dp_get_dev();
	struct drm_display_mode *a = list_entry(lh_a, struct drm_display_mode, head);
	struct drm_display_mode *b = list_entry(lh_b, struct drm_display_mode, head);
	enum mon_aspect_ratio_t ratio_p = secdp_get_aspect_ratio(&dp->sink_info.pref_mode);
	enum mon_aspect_ratio_t ratio_a = secdp_get_aspect_ratio(a);
	enum mon_aspect_ratio_t ratio_b = secdp_get_aspect_ratio(b);
	int diff;
	int res_t = secdp_dex_resolution[0].active_h * secdp_dex_resolution[0].active_v;
	int res_a = a->hdisplay * a->vdisplay;
	int res_b = b->hdisplay * b->vdisplay;

	/* The same ratio as the preferred mode is better */
	/* but not for less resolution than min dex support mode*/
	if (dp->prefer_found) {
		if (res_a >= res_t && res_b >= res_t &&
			ratio_a != MON_RATIO_NA && ratio_b != MON_RATIO_NA) {
			diff = (ratio_p == ratio_b) - (ratio_p == ratio_a);
			if (diff)
				return diff;
		}
	}

	diff = res_b - res_a;
	if (diff)
		return diff;

	diff = drm_mode_vrefresh(b) - drm_mode_vrefresh(a);
	if (diff)
		return diff;

	diff = b->clock - a->clock;

	return diff;
}

/*
 * sort for dex mode as priority
 * 1. preferred ratio
 * 2. higher resolution
 * 3. higher fps
 * 4. higher pixel clock
 */
void dp_mode_sort_for_dex(struct sec_dp_dev *dp)
{
	struct drm_connector *connector = mtk_dp_get_connector();

	list_sort(dp, &connector->probed_modes, dp_mode_compare_for_dex);
}

/* return -EINVAL: not support */
static int dp_mode_is_dex_support(struct drm_display_mode *mode)
{
	int i;
	int arr_size = ARRAY_SIZE(secdp_dex_resolution);
	int idx = -EINVAL;

	/* Support every modes under FHD */
	if (mode->vdisplay * mode->hdisplay < 1920 * 1080)
		return 0;

	for (i = 0; i < arr_size; i++) {
		if (mode->hdisplay == secdp_dex_resolution[i].active_h &&
				mode->vdisplay == secdp_dex_resolution[i].active_v) {
			idx = i;
			break;
		}
	}

	return idx;
}

static bool dp_mode_check_adapter_type(struct sec_dp_dev *dp, int idx)
{
	if (idx >= ARRAY_SIZE(secdp_dex_resolution) || idx < 0)
		return false;

	if (secdp_dex_resolution[idx].support_type > dp->dex.adapter_type)
		return false;

	return true;
}
#endif

struct drm_display_mode *dp_get_best_mode(void)
{
	struct sec_dp_dev *dp = dp_get_dev();

	return &dp->sink_info.best_mode;
}

void dp_mode_clean_up(struct sec_dp_dev *dp)
{
	struct drm_display_mode *mode, *t;
	struct drm_connector *connector = mtk_dp_get_connector();

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		list_del(&mode->head);
		drm_mode_destroy(connector->dev, mode);
	}
}

/* return true: support */
bool dp_mode_filter(struct sec_dp_dev *dp, struct drm_display_mode *mode, int flag)
{
	int refresh = drm_mode_vrefresh(mode);
	struct drm_connector *connector = mtk_dp_get_connector();

	if (dp->max_width != 0 && dp->max_height != 0 &&
			(mode->hdisplay > dp->max_width || mode->vdisplay > dp->max_height))
		return false;

	/* YCbCr420 but not preferred */
	if (!(mode->type & DRM_MODE_TYPE_PREFERRED) &&
			drm_mode_is_420_only(&connector->display_info, mode))
		return false;

	/* interlace */
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return false;

	/* for test sink, do not apply filter */
	if (flag & MODE_FILTER_TEST_SINK_SUPPORT) {
		if (dp->sink_info.test_sink)
			return true;
	}

	/* max resolution */
	if (flag & MODE_FILTER_MAX_RESOLUTION) {
		if ((mode->hdisplay * mode->vdisplay * refresh) > MODE_MAX_RESOLUTION)
			return false;
	}

	if (flag & MODE_FILTER_PIXEL_CLOCK) {
		int clock = mode->clock;

		if (dp->sink_info.max_pix_clk < clock)
			return false;
	}

	if (flag & MODE_FILTER_MIN_FPS) {
		if (refresh < MODE_MIN_FPS)
			return false;
	}

	if (flag & MODE_FILTER_MAX_FPS) {
		if (refresh > MODE_MAX_FPS)
			return false;
	}

#ifdef FEATURE_DEX_SUPPORT
	if (flag & MODE_FILTER_DEX_MODE) {
		int idx = dp_mode_is_dex_support(mode);

		if (idx < 0)
			return false;

		if (flag & MODE_FILTER_DEX_ADAPTER) {
			if (!dp_mode_check_adapter_type(dp, idx))
				return false;
		}
	}
#endif

	return true;
}

static void dp_get_sink_info(struct sec_dp_dev *dp, struct edid *edid)
{
	char *edid_vendor = dp->sink_info.edid_manufacturer;
	u32 prod_id = 0;

	edid_vendor[0] = ((edid->mfg_id[0] & 0x7c) >> 2) + '@';
	edid_vendor[1] = (((edid->mfg_id[0] & 0x3) << 3) |
				((edid->mfg_id[1] & 0xe0) >> 5)) + '@';
	edid_vendor[2] = (edid->mfg_id[1] & 0x1f) + '@';

	prod_id |= (edid->prod_code[0] << 8) | (edid->prod_code[1]);
	dp->sink_info.edid_product = prod_id;

	dp->sink_info.edid_serial = edid->serial;

	drm_edid_get_monitor_name(edid, dp->sink_info.monitor_name,
		sizeof(dp->sink_info.monitor_name));

	dp_info("manufacturer: %s, product: %x, serial: %x\n",
			dp->sink_info.edid_manufacturer,
			dp->sink_info.edid_product,
			dp->sink_info.edid_serial);
}

/*HDMI_EOTF_TRADITIONAL_GAMMA_HDR, HDMI_EOTF_SMPTE_ST2084, HDMI_EOTF_BT_2100_HLG*/
#define HDR_SUPPORT_TYPE (BIT(HDMI_EOTF_SMPTE_ST2084))
static void dp_check_hdr_support(struct sec_dp_dev *dp)
{
#ifdef FEATURE_HDR_SUPPORT
	struct drm_connector *connector = mtk_dp_get_connector();
	u8 eotf = connector->hdr_sink_metadata.hdmi_type1.eotf;
	struct hdr_static_metadata *hdmi_type1 = &connector->hdr_sink_metadata.hdmi_type1;

	if ((eotf & HDR_SUPPORT_TYPE) != 0) {
		dp->rx_edid_data.hdr_support = true;
		dp->rx_edid_data.min_lumi_data = hdmi_type1->min_cll;
		dp->rx_edid_data.max_lumi_data = hdmi_type1->max_cll;
		dp->rx_edid_data.max_average_lumi_data = hdmi_type1->max_fall;
		dp_info("SINK supports HDR(max: %hu, max_fall: %hu, min: %hu)\n",
			hdmi_type1->max_cll, hdmi_type1->max_fall, hdmi_type1->min_cll);
	}
#endif
}

static void dp_check_test_device(struct sec_dp_dev *dp)
{
	if (!strcmp(dp->sink_info.monitor_name, "UNIGRAF TE") ||
			!strcmp(dp->sink_info.monitor_name, "UFG DPR-120") ||
			!strcmp(dp->sink_info.monitor_name, "UCD-400 DP") ||
			!strcmp(dp->sink_info.monitor_name, "AGILENT ATR") ||
			!strcmp(dp->sink_info.monitor_name, "UFG DP SINK")) {
		dp->sink_info.test_sink = true;
		dp_info("test sink\n");
	}
}

static int dp_mode_compare_timing(struct drm_display_mode *a, struct drm_display_mode *b)
{
	int diff;

	diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
	if (diff)
		return diff;

	diff = drm_mode_vrefresh(b) - drm_mode_vrefresh(a);
	if (diff)
		return diff;

	diff = b->clock - a->clock;
	return diff;
}

void dp_print_all_modes(struct list_head *modes)
{
	struct drm_display_mode *mode, *t;
	int i = 0;

	list_for_each_entry_safe(mode, t, modes, head) {
		i++;
		dp_info("mode(%d) %s@%d, type: %d, stat: %d, ratio: %d\n", i,
			mode->name, drm_mode_vrefresh(mode), mode->type,
			mode->status, mode->picture_aspect_ratio);
	}
}

static bool dp_find_prefer_mode(struct sec_dp_dev *dp, int flag)
{
	struct drm_display_mode *mode, *t, *best_pref_mode = NULL;
	struct drm_connector *connector = mtk_dp_get_connector();

	dp->prefer_found = false;
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
#ifdef FEATURE_8K_PREFER_SUPPORT
		if (dp_is_8k_resolution(mode))
			mode->type |= DRM_MODE_TYPE_PREFERRED;
#endif
		if ((mode->type & DRM_MODE_TYPE_PREFERRED) &&
				dp_mode_filter(dp, mode, flag)) {
			dp_info("pref: %s@%d, type: %d, stat: %d, ratio: %d\n",
				mode->name, drm_mode_vrefresh(mode), mode->type,
				mode->status, mode->picture_aspect_ratio);
			if ((best_pref_mode == NULL) ||
					(dp_mode_compare_timing(best_pref_mode, mode) > 0))
				best_pref_mode = mode;
		}
	}

	if (best_pref_mode) {
		dp_info("best_pref: %s@%d, type: %d, stat: %d, ratio: %d\n",
			best_pref_mode->name, drm_mode_vrefresh(best_pref_mode), best_pref_mode->type,
			best_pref_mode->status, best_pref_mode->picture_aspect_ratio);
		drm_mode_copy(&dp->sink_info.pref_mode, best_pref_mode);
		drm_mode_copy(&dp->sink_info.best_mode, best_pref_mode); //need for test sink
		dp->prefer_found = true;
	} else {
		dp_info("no prefer timing\n");
	}

	return (best_pref_mode != NULL);
}

/* return true: modes's resolution are equal */
static bool dp_mode_resolution_match(struct drm_display_mode *ma, struct drm_display_mode *mb)
{
	if (ma->hdisplay == mb->hdisplay && ma->vdisplay == mb->vdisplay)
		return true;

	return false;
}

/*
 * mode comparison
 * from drm_mode_compare() at drm_modes.c
 * Returns:
 * Negative if @lh_a is better than @lh_b, zero if they're equivalent, or
 * positive if @lh_b is better than @lh_a.
 */
int dp_mode_compare_for_mirror(void *priv, const struct list_head *lh_a, const struct list_head *lh_b)
{
	struct sec_dp_dev *dp = (struct sec_dp_dev *)priv;
	struct drm_display_mode *a = list_entry(lh_a, struct drm_display_mode, head);
	struct drm_display_mode *b = list_entry(lh_b, struct drm_display_mode, head);
	int diff;

	if (dp->prefer_found) {
		/* The same resolution as the preferred mode is better */
		diff = dp_mode_resolution_match(&dp->sink_info.pref_mode, b) -
				dp_mode_resolution_match(&dp->sink_info.pref_mode, a);
		if (diff)
			return diff;

		/* do not compare preferred if both have the same resolution as the preferred */
		if (!dp_mode_resolution_match(&dp->sink_info.pref_mode, a)) {
			diff = ((b->type & DRM_MODE_TYPE_PREFERRED) != 0) -
				((a->type & DRM_MODE_TYPE_PREFERRED) != 0);
			if (diff)
				return diff;
		}
	}

	diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
	if (diff)
		return diff;

	diff = drm_mode_vrefresh(b) - drm_mode_vrefresh(a);
	if (diff)
		return diff;

	diff = b->clock - a->clock;
	return diff;
}

/*
 * sort for mirror mode as priority
 * 1. preferred resolution
 * 2. preferred type
 * 3. higher resolution
 * 4. higher fps
 * 5. higher pixel clock
 */
void dp_mode_sort_for_mirror(struct sec_dp_dev *dp)
{
	struct drm_connector *connector = mtk_dp_get_connector();

	list_sort(dp, &connector->probed_modes, dp_mode_compare_for_mirror);
}

#ifdef FEATURE_DEX_SUPPORT
void dp_find_best_mode_for_dex(struct sec_dp_dev *dp, bool do_print)
{
	struct drm_display_mode *mode, *t;
	struct drm_connector *connector = mtk_dp_get_connector();
	int flag = MODE_FILTER_DEX;

	dp_mode_sort_for_dex(dp);

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		if (do_print && mode->status == 0) {
			dp_info("%s@%d, pclk:%d\n",
				mode->name, drm_mode_vrefresh(mode), mode->clock);
		}

		if (dp_mode_filter(dp, mode, flag)) {
			drm_mode_copy(&dp->dex.best_mode, mode);
			break;
		}
	}

	dp_info("best mode for dex: %s@%d\n", dp->dex.best_mode.name,
		drm_mode_vrefresh(&dp->dex.best_mode));
}
#endif

static void dp_find_best_mode(struct sec_dp_dev *dp, int flag, bool do_print)
{
	struct drm_display_mode *mode, *t;
	struct drm_connector *connector = mtk_dp_get_connector();
	bool found = false;

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		if (dp_mode_filter(dp, mode, flag)) {
			if (!found && !dp->sink_info.test_sink) { /* for link cts */
				drm_mode_copy(&dp->sink_info.best_mode, mode);
				found = true;
			}
		} else {
			mode->status |= MODE_BAD;
		}

		if (do_print && mode->status == 0) {
			dp_info("%s@%d, pclk:%d\n",
				mode->name, drm_mode_vrefresh(mode), mode->clock);
		}
	}

	dp_info("best mirror mode: %s@%d\n", dp->sink_info.best_mode.name,
		drm_mode_vrefresh(&dp->sink_info.best_mode));
}

void dp_parse_edid(struct edid *edid)
{
	struct sec_dp_dev *dp = dp_get_dev();
	struct drm_connector *connector = mtk_dp_get_connector();
	u8 *raw_edid = (u8 *)edid;
	u8 block_count = 0;
	int ret;

	if (dp->test_edid_size) {
		u8 *new;
		int new_size = dp->test_edid_size;

		dp_info("Using Test EDID\n");
		if (new_size > MAX_EDID_BLOCK * EDID_LENGTH)
			new_size = MAX_EDID_BLOCK * EDID_LENGTH;

		new  = krealloc(raw_edid, new_size, GFP_KERNEL);
		if (!new)
			return;

		raw_edid = new;
		edid = (struct edid *)new;
		memcpy(raw_edid, dp->edid_buf, new_size);
		dp->max_width = 0;
		dp->max_height = 0;

	} else {
		dp->max_width = connector->dev->mode_config.max_width;
		dp->max_height = connector->dev->mode_config.max_height;
	}
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_EDID, raw_edid);
#endif
	block_count = raw_edid[0x7e];
	if (block_count > 3)
		block_count = 3;

	dp_hex_dump("EDID: ", raw_edid, (block_count + 1) * EDID_LENGTH);

	mutex_lock(&connector->dev->mode_config.mutex);
	dp_mode_clean_up(dp);
	ret = drm_add_edid_modes(connector, edid);
	mutex_unlock(&connector->dev->mode_config.mutex);
	dp_info("mode num: %d\n", ret);

	if (ret) {
		int flag = MODE_FILTER_MAX_RESOLUTION | MODE_FILTER_TEST_SINK_SUPPORT;

		/* get sink info */
		dp_get_sink_info(dp, edid);
		dp_check_hdr_support(dp);
		dp_check_test_device(dp);
		dp_info("mon name: %s, bpc: %d, EOTF: %d\n",
				dp->sink_info.monitor_name,
				connector->display_info.bpc,
				connector->hdr_sink_metadata.hdmi_type1.eotf);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_SINK_NAME, dp->sink_info.monitor_name);
#endif
		dp_find_prefer_mode(dp, flag);

		dp_mode_sort_for_mirror(dp);
		dp_find_best_mode(dp, flag, true);
#ifdef FEATURE_DEX_SUPPORT
		if (dp->dex.ui_setting) {
			dp->sink_info.max_pix_clk = dp_get_pixel_clock(LINKRATE_HBR2, dp->pdic_lane_count);
			dp_find_best_mode_for_dex(dp, true);
		}
#endif
	}
}

void dp_link_training_postprocess(u8 link_rate, u8 lane_count)
{
	struct sec_dp_dev *dp = dp_get_dev();

	dp->sink_info.link_rate = link_rate;
	dp->sink_info.lane_count = lane_count;
	dp->sink_info.max_pix_clk = dp_get_pixel_clock(link_rate, lane_count);
	// check if bandwidth is enough for best mode
	if (dp->sink_info.max_pix_clk < dp->sink_info.best_mode.clock) {
		int flag = MODE_FILTER_MIRROR | MODE_FILTER_PIXEL_CLOCK;

		dp_find_best_mode(dp, flag, false);

	}
#ifdef FEATURE_DEX_SUPPORT
	if (!dp->sink_info.test_sink)
		dp_find_best_mode_for_dex(dp, false);
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (link_rate > 0) {
		secdp_bigdata_clr_error_cnt(ERR_LINK_TRAIN);
		secdp_bigdata_save_item(BD_CUR_LINK_RATE, link_rate);
		secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_count);
	} else {
		secdp_bigdata_inc_error_cnt(ERR_LINK_TRAIN);
		secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_count);
	}
	if (dp->dex.ui_setting) {
		secdp_bigdata_save_item(BD_DP_MODE, "DEX");
		secdp_bigdata_save_item(BD_RESOLUTION, dp->dex.best_mode.name);
	} else {
		secdp_bigdata_save_item(BD_DP_MODE, "MIRROR");
		secdp_bigdata_save_item(BD_RESOLUTION, dp->sink_info.best_mode.name);
	}
#endif
}

void dp_validate_modes(void)
{
	struct sec_dp_dev *dp = dp_get_dev();
	struct drm_connector *connector = mtk_dp_get_connector();
	struct drm_display_mode *mode, *t;
	bool is_dex_mode = false;
	bool is_before_best = true;
	int flag = MODE_FILTER_MIRROR | MODE_FILTER_PIXEL_CLOCK;

	/* should use the same sort as connector driver */
	drm_mode_sort(&connector->probed_modes);

#ifdef FEATURE_DEX_SUPPORT
	if (dp->dex.ui_setting)
		is_dex_mode = true;
#endif

	if (is_dex_mode)
		drm_mode_copy(&dp->sink_info.cur_mode, &dp->dex.best_mode);
	else
		drm_mode_copy(&dp->sink_info.cur_mode, &dp->sink_info.best_mode);

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		if (mode->type & DRM_MODE_TYPE_PREFERRED &&
				drm_mode_is_420_only(&connector->display_info, mode)) {
			u8 vic = drm_match_cea_mode(mode);

			/* HACK: prevent preferred from becoming ycbcr420 */
			bitmap_clear(connector->display_info.hdmi.y420_vdb_modes, vic, 1);
			dp_info("unset ycbcr420 for prefer %s@%d vic %d\n",
				mode->name, drm_mode_vrefresh(mode), vic);
		}
		if (is_before_best && drm_mode_match(&dp->sink_info.cur_mode, mode, DRM_MODE_MATCH_TIMINGS)) {
			mode->type |= DRM_MODE_TYPE_PREFERRED;
			is_before_best = false;
			continue;
		}
		//filter out mode which has higher priority than best mode;
		if (is_before_best)
			mode->status |= MODE_BAD;
		if (!dp_mode_filter(dp, mode, flag))
			mode->status |= MODE_BAD;

		dp_debug("filter out: %s@%d, pclk:%d, stat: %d\n",
			mode->name, drm_mode_vrefresh(mode), mode->clock, mode->status);
	}
}
static bool dp_mode_check_proaud_support(int vic)
{
	int proaudio_support_vics[] = {
		4,	/* 720P60 */
		34, /* 1920X1080P30 */
		16, /* 1920X1080P60 */
		95, /* 3840X2160P30 */
		97, /* 3840X2160P60 */
		};
	int arr_size = ARRAY_SIZE(proaudio_support_vics);
	int i;

	for (i = 0; i < arr_size; i++) {
		if (vic == proaudio_support_vics[i])
			return true;
	}

	return false;
}

#ifndef DP_CAPABILITY_SAMPLERATE_MASK
	#define DP_CAPABILITY_SAMPLERATE_MASK 0x1F
#endif
#ifndef DP_CAPABILITY_SAMPLERATE_SFT
	#define DP_CAPABILITY_SAMPLERATE_SFT 8
#endif
u32 dp_reduce_audio_capa(u32 caps)
{
	struct sec_dp_dev *dp = dp_get_dev();
	int vic = 0;
	int sf = 0;

	vic = drm_match_cea_mode(&dp->sink_info.cur_mode);
	sf = (caps >> DP_CAPABILITY_SAMPLERATE_SFT) & DP_CAPABILITY_SAMPLERATE_MASK;
	/* if cur_mode does not support pro audio, then reduce sample frequency. */
	if (!dp_mode_check_proaud_support(vic) && sf > 0x7) {
		dp_info("reduce audio sample freq.(0x%X) caps(0x%X\n", sf, caps);
		sf &= 0x7; /* reduce to under 48KHz */
		caps = (caps & ~(DP_CAPABILITY_SAMPLERATE_MASK << DP_CAPABILITY_SAMPLERATE_SFT)) |
			(sf << DP_CAPABILITY_SAMPLERATE_SFT);
	}

	return caps;
}

