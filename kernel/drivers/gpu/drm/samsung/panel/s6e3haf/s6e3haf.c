/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3haf/s6e3haf.c
 *
 * S6E3HAF Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel_kunit.h"
#include <linux/bits.h>
#include "../panel.h"
#include "../panel_function.h"
#include "../panel_resource.h"
#include "s6e3haf.h"
#include "s6e3haf_dimming.h"
#ifdef CONFIG_USDM_PANEL_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
#include "../mafpc/mafpc_drv.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../dpui.h"
#include "../util.h"
#include "oled_common.h"
#include "oled_property.h"
#ifdef CONFIG_USDM_PANEL_MAFPC
#include "s6e3haf_eureka_abc_fac_img.h"
#endif

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

__visible_for_testing int s6e3haf_vrr_to_lfd_index(u32 vrr_index)
{
	static u32 s6e3haf_lfd[MAX_S6E3HAF_VRR] = {
		[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_120HZ,
		[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_80HZ,
		[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_60HZ,
		[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_48HZ,
		[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_30HZ,
		[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_24HZ,
		[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
	};

	if (vrr_index >= MAX_S6E3HAF_VRR) {
		panel_err("invalid vrr_index %d\n", vrr_index);
		vrr_index = S6E3HAF_VRR_120HS;
	}

	return s6e3haf_lfd[vrr_index];
}

__visible_for_testing int s6e3haf_get_lfd_min_index(u32 scalability, u32 vrr_index)
{
	static u32 s6e3haf_lfd_min_freq[MAX_S6E3HAF_VRR_LFD_SCALABILITY][MAX_S6E3HAF_VRR] = {
		/* scalability default - same as 5 */
		[S6E3HAF_VRR_LFD_SCALABILITY_NONE] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_1] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_120HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_80HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_60HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_48HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_30HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_2] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_60HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_80HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_60HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_48HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_30HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_3] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_30HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_80HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_60HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_48HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_30HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_4] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_80HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_60HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_48HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_30HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_5] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_24HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_10HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_10HZ,
		},
		[S6E3HAF_VRR_LFD_SCALABILITY_6] = {
			[S6E3HAF_VRR_120HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_80HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_60HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_48HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_30HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_24HS] = S6E3HAF_LFD_1HZ,
			[S6E3HAF_VRR_10HS] = S6E3HAF_LFD_1HZ,
		},
	};

	if (scalability > S6E3HAF_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAF_VRR_LFD_SCALABILITY_MAX;
	}

	if (vrr_index >= MAX_S6E3HAF_VRR) {
		panel_err("invalid vrr_index %d\n", vrr_index);
		vrr_index = S6E3HAF_VRR_120HS;
	}

	return s6e3haf_lfd_min_freq[scalability][vrr_index];
}

__visible_for_testing int s6e3haf_get_lpm_lfd_min_index(u32 scalability)
{
	static u32 s6e3haf_lpm_lfd_min_freq[MAX_S6E3HAF_VRR_LFD_SCALABILITY] = {
		/* scalability default */
		[S6E3HAF_VRR_LFD_SCALABILITY_NONE] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_1] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_2] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_3] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_4] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_5] = S6E3HAF_LPM_LFD_1HZ,
		[S6E3HAF_VRR_LFD_SCALABILITY_6] = S6E3HAF_LPM_LFD_1HZ,
	};

	if (scalability > S6E3HAF_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAF_VRR_LFD_SCALABILITY_MAX;
	}

	return s6e3haf_lpm_lfd_min_freq[scalability];
}

#ifdef CONFIG_USDM_PANEL_DIMMING
__visible_for_testing int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		} else {
			panel_err("invalid parameter, all table is NULL\n");
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for(i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step =
		(u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		panel_err("alloc fail\n");
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = disp_interpolation64(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i]) {

					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}

#endif /* CONFIG_USDM_PANEL_DIMMING */

#ifdef CONFIG_USDM_PANEL_LPM
#ifdef CONFIG_USDM_PANEL_AOD_BL
__visible_for_testing int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif
#endif

__visible_for_testing int s6e3haf_maptbl_init_lpm_brt(struct maptbl *tbl)
{
#ifdef CONFIG_USDM_PANEL_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return oled_maptbl_init_default(tbl);
#endif
}

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
/* TODO: remove this function */
static int s6e3haf_maptbl_init_gram_img_ptattern(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	props->gct_valid_chksum[0] = S6E3HAF_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAF_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAF_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAF_GRAM_CHECKSUM_VALID_2;

	return oled_maptbl_init_default(tbl);
}
#endif

__visible_for_testing void s6e3haf_maptbl_copy_elvss_cal(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int ret;
	u8 elvss_cal_offset_otp_value = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	ret = panel_resource_copy(panel,
			&elvss_cal_offset_otp_value, "elvss_cal_offset");
	if (unlikely(ret)) {
		panel_err("elvss_cal_offset not found in panel resource\n");
		return;
	}

	*dst = elvss_cal_offset_otp_value;
}

#ifdef CONFIG_USDM_PANEL_MAFPC
__visible_for_testing void s6e3haf_maptbl_copy_mafpc_ctrl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mafpc_device *mafpc;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (!mafpc->enable || !mafpc->written)
		return;

	memcpy(dst, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, mafpc->ctrl_cmd_len, false);

	return;
}

#define S6E3HAF_MAFPC_SCALE_MAX	75
__visible_for_testing int get_mafpc_scale_index(struct mafpc_device *mafpc, struct panel_device *panel)
{
	int ret = 0;
	int br_index, index = 0;
	struct panel_bl_device *panel_bl;

	panel_bl = &panel->panel_bl;
	if (!panel_bl) {
		panel_err("panel_bl is null\n");
		goto err_get_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_get_scale;
	}

	br_index = panel_bl->props.brightness;
	if (br_index >= mafpc->scale_map_br_tbl_len)
		br_index = mafpc->scale_map_br_tbl_len - 1;

	index = mafpc->scale_map_br_tbl[br_index];
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", br_index);
		goto err_get_scale;
	}
	return index;

err_get_scale:
	return ret;
}

__visible_for_testing void s6e3haf_maptbl_copy_mafpc_scale(struct maptbl *tbl, u8 *dst)
{
	int row = 0;
	int index = 0;
	struct mafpc_device *mafpc;
	struct panel_device *panel;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		if (!mafpc->scale_buf)
			panel_err("ABC: scale_buf is null\n");
		if (!mafpc->scale_map_br_tbl)
			panel_err("ABC: scale_map_br_tbl is null\n");
		return;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		return;
	}

	if (index >= S6E3HAF_MAFPC_SCALE_MAX)
		index = S6E3HAF_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

	return;
}

static void s6e3haf_maptbl_copy_mafpc_img(struct maptbl *tbl, u8 *dst)
{
	struct mafpc_device *mafpc;
	struct panel_device *panel;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (!mafpc->comp_img_buf) {
		panel_err("mafpc img buf is null\n");
		return;
	}

	if (dst == mafpc->comp_img_buf)
		return;

	memcpy(dst, mafpc->comp_img_buf, mafpc->comp_img_len);

	return;
}

static void s6e3haf_maptbl_copy_mafpc_crc_img(struct maptbl *tbl, u8 *dst)
{
	struct mafpc_device *mafpc;
	struct panel_device *panel;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (ARRAY_SIZE(S6E3HAF_MAFPC_CRC_IMG) !=
			mafpc->comp_img_len) {
		panel_err("mafpc crc image size mismatch(%ld != %d)\n",
				ARRAY_SIZE(S6E3HAF_MAFPC_CRC_IMG),
				mafpc->comp_img_len);
		return;
	}

	if (dst == mafpc->comp_img_buf)
		return;

	memcpy(dst, S6E3HAF_MAFPC_CRC_IMG, mafpc->comp_img_len);

	return;
}
#endif

__visible_for_testing int s6e3haf_maptbl_init_gamma_mode2_brt(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	//todo:remove
	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

#ifdef CONFIG_USDM_PANEL_HMD
__visible_for_testing int s6e3haf_maptbl_init_gamma_mode2_hmd_brt(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_HMD];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_HMD].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

#endif

int s6e3haf_do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
{
	int ret, state;
	struct dim_flash_result *result;
	char read_buf[16] = { 0, };

	result = (struct dim_flash_result *)data;

	if (!panel)
		return -EINVAL;

	if (!result)
		return -ENODATA;

	if (atomic_cmpxchg(&result->running, 0, 1) != 0) {
		panel_info("already running\n");
		return -EBUSY;
	}

	memset(result->result, 0, ARRAY_SIZE(result->result));
	result->exist = 0;
	result->state = state = GAMMA_FLASH_PROGRESS;

	ret = panel_resource_copy(panel, read_buf, "flash_loaded");
	if (ret < 0) {
		panel_err("flash_loaded copy failed\n");
		state = GAMMA_FLASH_ERROR_READ_FAIL;
		goto out;
	}

	result->exist = 1;
	state = panel_is_dump_status_success(panel, "flash_loaded") ?
		GAMMA_FLASH_SUCCESS : GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;

out:
	snprintf(result->result, ARRAY_SIZE(result->result), "1\n%d %02X%02X%02X%02X",
		state, read_buf[0], read_buf[1], 0x00, 0x00);

	result->state = state;
	atomic_xchg(&result->running, 0);

	return ret;
}

int s6e3haf_get_octa_id(struct panel_device *panel, void *buf)
{
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	int len = 0;
	bool cell_id_exist = true;
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	ret = panel_resource_copy(panel, octa_id, "octa_id");
	if (ret < 0) {
		panel_err("failed to copy resources\n");
		return -ENODATA;
	}

	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	panel_dbg("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	panel_dbg("<CELL ID>\n");
	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		panel_dbg("%x -> %c\n", octa_id[i + 4], cell_id[i]);
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			len += snprintf(buf + len, PAGE_SIZE - len, "%c", cell_id[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return 0;
}

int s6e3haf_get_cell_id(struct panel_device *panel, void *buf)
{
	u8 date[PANEL_DATE_LEN] = { 0, }, coordinate[4] = { 0, };
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	ret = panel_resource_copy(panel, date, "date");
	ret |= panel_resource_copy(panel, coordinate, "coordinate");
	if (ret < 0) {
		panel_err("failed to copy resources\n");
		return -ENODATA;
	}

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		date[0], date[1], date[2], date[3], date[4], date[5], date[6],
		coordinate[0], coordinate[1], coordinate[2], coordinate[3]);
	return 0;

}

int s6e3haf_get_manufacture_code(struct panel_device *panel, void *buf)
{
	u8 code[5] = { 0, };
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	ret = panel_resource_copy(panel, code, "code");
	if (ret < 0) {
		panel_err("failed to copy resources\n");
		return -ENODATA;
	}

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4]);

	return 0;
}

int s6e3haf_get_manufacture_date(struct panel_device *panel, void *buf)
{
	u16 year;
	u8 month, day, hour, min, date[PANEL_DATE_LEN] = { 0, };
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	ret = panel_resource_copy(panel, date, "date");
	if (ret < 0) {
		panel_err("failed to copy resources\n");
		return -ENODATA;
	}

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;

	snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d:%d\n",
			year, month, day, hour, min);

	return 0;
}

int s6e3haf_get_temperature_range(struct panel_device *panel, void *buf)
{
	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	snprintf(buf, PAGE_SIZE, "-15, -14, -10, -9, 0, 1, 39, 40\n");
	return 0;
}

static int s6e3haf_smooth_dim_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;

	return panel_property_set_value(prop,
			(props->vrr_idx != props->vrr_origin_idx ||
			 panel->state.disp_on == PANEL_DISPLAY_OFF) ?
			S6E3HAF_SMOOTH_DIM_NOT_USE : S6E3HAF_SMOOTH_DIM_USE);
}

static struct panel_prop_enum_item s6e3haf_smooth_dim_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_SMOOTH_DIM_NOT_USE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_SMOOTH_DIM_USE),
};

static int s6e3haf_acl_opr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	unsigned int acl_opr = panel_bl_get_acl_opr(panel_bl);

	return panel_property_set_value(prop,
			min(acl_opr, (u32)S6E3HAF_ACL_OPR_6));
}

static struct panel_prop_enum_item s6e3haf_acl_opr_enum_items[MAX_S6E3HAF_ACL_OPR] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_0),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_3),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_4),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_5),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_ACL_OPR_6),
};

static int s6e3haf_vrr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;

	return panel_property_set_value(prop, props->vrr_idx);
}

static struct panel_prop_enum_item s6e3haf_vrr_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_120HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_80HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_60HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_48HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_30HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_24HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_VRR_10HS),
};

static int s6e3haf_scaler_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	int index;

	if (mres->nr_resol == 0 || mres->resol == NULL)
		return -EINVAL;

	if (props->mres_mode >= mres->nr_resol) {
		panel_err("invalid mres_mode %d, nr_resol %d\n",
				props->mres_mode, mres->nr_resol);
		return -EINVAL;
	}

	index = search_table_u32(S6E3HAF_SCALER_1440,
			ARRAY_SIZE(S6E3HAF_SCALER_1440),
			mres->resol[props->mres_mode].w);
	if (index < 0)
		return -EINVAL;

	return panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item s6e3haf_scaler_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_SCALER_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_SCALER_x1_78),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_SCALER_x4),
};

static int s6e3haf_lfd_min_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *lfd = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	int lfd_min_index = s6e3haf_get_lfd_min_index(lfd->scalability, props->vrr_idx);
	int lfd_index;

	if (lfd->fix == VRR_LFD_FREQ_HIGH) {
		lfd_index = s6e3haf_vrr_to_lfd_index(props->vrr_idx);
	} else if (lfd->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ) {
		lfd_index = S6E3HAF_LFD_120HZ;
	} else if (lfd->fix == VRR_LFD_FREQ_LOW || lfd->min == 0) {
		lfd_index = lfd_min_index;
	} else if (lfd->fix == VRR_LFD_FREQ_LOW_HIGHDOT_TEST) {
		lfd_index = S6E3HAF_LFD_0_5HZ;
	} else {
		lfd_index = s6e3haf_vrr_to_lfd_index(props->vrr_idx);
		lfd_index = min(lfd_index, lfd_min_index);
	}

	return panel_property_set_value(prop, lfd_index);
}

static int s6e3haf_lfd_max_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *lfd = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	int lfd_min_index = s6e3haf_get_lfd_min_index(lfd->scalability, props->vrr_idx);
	int lfd_index;

	if (lfd->fix == VRR_LFD_FREQ_HIGH || lfd->max == 0) {
		lfd_index = s6e3haf_vrr_to_lfd_index(props->vrr_idx);
	} else if (lfd->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ) {
		lfd_index = S6E3HAF_LFD_120HZ;
	} else if (lfd->fix == VRR_LFD_FREQ_LOW) {
		lfd_index = lfd_min_index;
	} else if (lfd->fix == VRR_LFD_FREQ_LOW_HIGHDOT_TEST) {
		lfd_index = S6E3HAF_LFD_0_5HZ;
	} else {
		lfd_index = s6e3haf_vrr_to_lfd_index(props->vrr_idx);
		lfd_index = min(lfd_index, lfd_min_index);
	}

	return panel_property_set_value(prop, lfd_index);
}

static struct panel_prop_enum_item s6e3haf_lfd_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_120HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_80HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_60HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_48HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_30HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_24HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_10HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_1HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LFD_0_5HZ),
};

static int s6e3haf_lpm_lfd_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *lfd = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_LPM];
	int lfd_min_index = s6e3haf_get_lpm_lfd_min_index(lfd->scalability);
	int lfd_index;

	if (lfd->fix == VRR_LFD_FREQ_HIGH ||
		lfd->fix == VRR_LFD_FREQ_HIGH_UPTO_SCAN_FREQ)
		lfd_index = S6E3HAF_LPM_LFD_30HZ;
	else if (lfd->fix == VRR_LFD_FREQ_LOW ||
		lfd->fix == VRR_LFD_FREQ_LOW_HIGHDOT_TEST)
		lfd_index = S6E3HAF_LPM_LFD_1HZ;
	else if (lfd_min_index == S6E3HAF_LPM_LFD_30HZ)
		lfd_index = S6E3HAF_LPM_LFD_30HZ;
	else
		lfd_index = props->lpm_fps;

	return panel_property_set_value(prop, lfd_index);
}

static struct panel_prop_enum_item s6e3haf_lpm_lfd_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LPM_LFD_1HZ),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_LPM_LFD_30HZ),
};

#ifdef CONFIG_USDM_PANEL_MAFPC
static int s6e3haf_mafpc_enable_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct mafpc_device *mafpc = get_mafpc_device(panel);

	if (!mafpc) {
		panel_err("failed to get mafpc device\n");
		return -EINVAL;
	}

	return panel_property_set_value(prop, !!mafpc->enable);
}
#endif

static struct panel_prop_list s6e3haf_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_SMOOTH_DIM_PROPERTY,
			S6E3HAF_SMOOTH_DIM_USE, s6e3haf_smooth_dim_enum_items,
			s6e3haf_smooth_dim_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_ACL_OPR_PROPERTY,
			S6E3HAF_ACL_OPR_0, s6e3haf_acl_opr_enum_items,
			s6e3haf_acl_opr_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_VRR_PROPERTY,
			S6E3HAF_VRR_120HS, s6e3haf_vrr_enum_items,
			s6e3haf_vrr_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_SCALER_PROPERTY,
			S6E3HAF_SCALER_OFF, s6e3haf_scaler_enum_items,
			s6e3haf_scaler_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_LPM_LFD_PROPERTY,
			S6E3HAF_LPM_LFD_30HZ, s6e3haf_lpm_lfd_enum_items,
			s6e3haf_lpm_lfd_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_LFD_MIN_PROPERTY,
			S6E3HAF_LFD_120HZ, s6e3haf_lfd_enum_items,
			s6e3haf_lfd_min_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3HAF_LFD_MAX_PROPERTY,
			S6E3HAF_LFD_120HZ, s6e3haf_lfd_enum_items,
			s6e3haf_lfd_max_property_update),
	/* range property */
#ifdef CONFIG_USDM_PANEL_MAFPC
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3HAF_MAFPC_ENABLE_PROPERTY,
			0, 0, 1, s6e3haf_mafpc_enable_property_update),
#endif
};

struct pnobj_func s6e3haf_function_table[MAX_S6E3HAF_FUNCTION] = {
	/* maptbl functions */
	[S6E3HAF_MAPTBL_INIT_GAMMA_MODE2_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_INIT_GAMMA_MODE2_BRT, s6e3haf_maptbl_init_gamma_mode2_brt),
	[S6E3HAF_MAPTBL_INIT_LPM_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_INIT_LPM_BRT, s6e3haf_maptbl_init_lpm_brt),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[S6E3HAF_MAPTBL_INIT_GRAM_IMG_PATTERN] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_INIT_GRAM_IMG_PATTERN, s6e3haf_maptbl_init_gram_img_ptattern),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	[S6E3HAF_MAPTBL_COPY_MAFPC_CTRL] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_COPY_MAFPC_CTRL, s6e3haf_maptbl_copy_mafpc_ctrl),
	[S6E3HAF_MAPTBL_COPY_MAFPC_SCALE] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_COPY_MAFPC_SCALE, s6e3haf_maptbl_copy_mafpc_scale),
	[S6E3HAF_MAPTBL_COPY_MAFPC_IMG] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_COPY_MAFPC_IMG, s6e3haf_maptbl_copy_mafpc_img),
	[S6E3HAF_MAPTBL_COPY_MAFPC_CRC_IMG] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_COPY_MAFPC_CRC_IMG, s6e3haf_maptbl_copy_mafpc_crc_img),
#endif
	[S6E3HAF_MAPTBL_COPY_ELVSS_CAL] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_COPY_ELVSS_CAL, s6e3haf_maptbl_copy_elvss_cal),
#ifdef CONFIG_USDM_PANEL_HMD
	[S6E3HAF_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3HAF_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT, s6e3haf_maptbl_init_gamma_mode2_hmd_brt),
#endif
	/* condition functions */
};

int s6e3haf_init(struct common_panel_info *cpi)
{
	static bool once;
	int ret;

	if (once)
		return 0;

	ret = panel_function_insert_array(s6e3haf_function_table,
			ARRAY_SIZE(s6e3haf_function_table));
	if (ret < 0)
		panel_err("failed to insert s6e3haf_function_table\n");

	cpi->prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array_size;
	cpi->prop_lists[USDM_DRV_LEVEL_DDI] = s6e3haf_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_DDI] = ARRAY_SIZE(s6e3haf_property_array);

	once = true;

	return 0;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
