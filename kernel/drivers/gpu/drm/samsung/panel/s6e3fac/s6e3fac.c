/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac.c
 *
 * S6E3FAC Dimming Driver
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
#include "s6e3fac.h"
#include "s6e3fac_dimming.h"
#ifdef CONFIG_USDM_PANEL_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../dpui.h"
#include "../util.h"
#include "oled_common.h"
#include "oled_property.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

unsigned int s6e3fac_gamma_bits[S6E3FAC_NR_TP] = {
	12, 12, 11, 11, 11, 11, 11, 11,
	11, 11, 11, 11, 12,
};

unsigned int s6e3fac_glut_bits[S6E3FAC_NR_GLUT_POINT] = {
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
};

static const char *s6e3fac_get_vrr_name(int index)
{
	const char *s6e3fac_vrr_name[MAX_S6E3FAC_VRR] = {
		[S6E3FAC_VRR_60NS] = "60NS",
		[S6E3FAC_VRR_48NS] = "48NS",
		[S6E3FAC_VRR_120HS] = "120HS",
		[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1] = "60HS_120HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3] = "30HS_120HS_TE_HW_SKIP_3",
		[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4] = "24HS_120HS_TE_HW_SKIP_4",
		[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = "10HS_120HS_TE_HW_SKIP_11",
		[S6E3FAC_VRR_96HS] = "96HS",
		[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = "48HS_96HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_60HS] = "60HS",
		[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = "30HS_60HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_48HS] = "48HS",
		[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1] = "24HS_48HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = "10HS_48HS_TE_HW_SKIP_4",
	};

	if (index >= MAX_S6E3FAC_VRR)
		return NULL;

	return s6e3fac_vrr_name[index];
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

__visible_for_testing int find_s6e3fac_vrr(struct panel_vrr *vrr)
{
	size_t i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3FAC_VRR_FPS); i++) {
		if (vrr->fps == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_REFRESH_RATE] &&
				vrr->mode == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_REFRESH_MODE] &&
				vrr->te_sw_skip_count == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_TE_SW_SKIP_COUNT] &&
				vrr->te_hw_skip_count == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_TE_HW_SKIP_COUNT])
			return (int)i;
	}

	return -EINVAL;
}

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

__visible_for_testing int s6e3fac_maptbl_init_lpm_brt(struct maptbl *tbl)
{
#ifdef CONFIG_USDM_PANEL_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return oled_maptbl_init_default(tbl);
#endif
}

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
/* TODO: remove this function */
static int s6e3fac_maptbl_init_gram_img_pattern(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	props->gct_valid_chksum[0] = S6E3FAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3FAC_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3FAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3FAC_GRAM_CHECKSUM_VALID_2;

	return oled_maptbl_init_default(tbl);
}
#endif

__visible_for_testing void s6e3fac_maptbl_copy_elvss_cal(struct maptbl *tbl, u8 *dst)
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
__visible_for_testing void s6e3fac_maptbl_copy_mafpc_enable(struct maptbl *tbl, u8 *dst)
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

	panel_info("MCD:ABC:enabled: %x, written: %x\n", mafpc->enable, mafpc->written);

	if (!mafpc->enable) {
		dst[0] = 0;
		goto err_enable;
	}

	if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
		(mafpc->written & MAFPC_UPDATED_TO_DEV)) {

		dst[0] = S6E3FAC_MAFPC_ENABLE;

		if (mafpc->written & MAFPC_UPDATED_FROM_SVC)
			memcpy(&dst[S6E3FAC_MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	}

err_enable:
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, S6E3FAC_MAFPC_CTRL_CMD_OFFSET + mafpc->ctrl_cmd_len, false);
	return;
}

__visible_for_testing void s6e3fac_maptbl_copy_mafpc_ctrl(struct maptbl *tbl, u8 *dst)
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

	if (mafpc->enable) {
		if (mafpc->written)
			memcpy(dst, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);

		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, dst, mafpc->ctrl_cmd_len, false);
	}

	return;
}


#define S6E3FAC_MAFPC_SCALE_MAX	75


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

__visible_for_testing void s6e3fac_maptbl_copy_mafpc_scale(struct maptbl *tbl, u8 *dst)
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
			panel_err("MCD:ABC: scale_buf is null\n");
		if (!mafpc->scale_map_br_tbl)
			panel_err("MCD:ABC: scale_map_br_tbl is null\n");
		return;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		return;
	}

	if (index >= S6E3FAC_MAFPC_SCALE_MAX)
		index = S6E3FAC_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

	return;
}
#endif

__visible_for_testing int s6e3fac_maptbl_init_gamma_mode2_brt(struct maptbl *tbl)
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
__visible_for_testing int s6e3fac_maptbl_init_gamma_mode2_hmd_brt(struct maptbl *tbl)
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

__visible_for_testing int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src)
{
	unsigned int i, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		mask = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		upper_mask = (mask >> 8) & 0xFF;
		lower_mask = mask & 0xFF;

		dst[i][RED] = (((src[i * 5 + 0] & upper_mask) << 8) | (src[i * 5 + 2] & lower_mask));
		dst[i][GREEN] = ((((src[i * 5 + 1] >> 4) & upper_mask) << 8) | (src[i * 5 + 3] & lower_mask));
		dst[i][BLUE] = (((src[i * 5 + 1] & upper_mask) << 8) | (src[i * 5 + 4] & lower_mask));
	}

	return 0;
}

__visible_for_testing int gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR])
{
	unsigned int i, mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		mask = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		dst[i * 5 + 0] = (src[i][RED] >> 8) & (mask >> 8);
		dst[i * 5 + 1] = ((src[i][GREEN] >> 8) & (mask >> 8)) << 4 |
						 ((src[i][BLUE] >> 8) & (mask >> 8));
		dst[i * 5 + 2] = src[i][RED] & (mask & 0xFF);
		dst[i * 5 + 3] = src[i][GREEN] & (mask & 0xFF);
		dst[i * 5 + 4] = src[i][BLUE] & (mask & 0xFF);
	}

	return 0;
}

__visible_for_testing int gamma_int_array_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR])
{
	unsigned int i, c;
	int max_value;

	if (!src || !dst || !offset)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		max_value = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), max_value);
	}

	return 0;
}

__visible_for_testing int gamma_byte_array_sum(u8 *dst, u8 *src,
		s32 (*offset)[MAX_COLOR])
{
	s32 gamma_org[S6E3FAC_NR_TP][MAX_COLOR];
	s32 gamma_new[S6E3FAC_NR_TP][MAX_COLOR];

	if (!dst || !src || !offset)
		return -EINVAL;

	gamma_ctoi(gamma_org, src);
	gamma_int_array_sum(gamma_new, gamma_org, offset);
	gamma_itoc(dst, gamma_new);

	return 0;
}

__visible_for_testing struct dimming_color_offset *get_dimming_data(struct panel_device *panel, int idx)
{
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	struct dimming_color_offset *dimming_data;

	if (!panel)
		return ERR_PTR(-EINVAL);

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (!panel_dim_info)
		return ERR_PTR(-EINVAL);

	if (!panel_dim_info->dimming_data || panel_dim_info->nr_dimming_data < 1)
		return ERR_PTR(-ENODATA);

	if (idx < 0 || idx >= panel_dim_info->nr_dimming_data)
		return ERR_PTR(-ERANGE);

	dimming_data = panel_dim_info->dimming_data;
	return (dimming_data + idx);
}

/*
 * glut_ctoi - trasnform GLUT byte array to int array
 */
int s6e3fac_glut_ctoi(s32 (*dst)[S6E3FAC_NR_GLUT_POINT], u8 *src)
{
	unsigned int i, j = 0, c, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for_each_color(c) {
		for (i = 0; i < ARRAY_SIZE(s6e3fac_glut_bits); i += 2, j += 3) {
			/* set GLUT_DEC[c][i + 0] */
			mask = GENMASK(s6e3fac_glut_bits[i + 0] - 1, 0);
			upper_mask = (mask >> 8) & 0xFF;
			lower_mask = mask & 0xFF;
			dst[c][i + 0] = hextos32((((src[j + 0] >> 4) & upper_mask) << 8) |
					(src[j + 1] & lower_mask), s6e3fac_glut_bits[i + 0]);

			/* set GLUT_DEC[c][i + 0] */
			mask = GENMASK(s6e3fac_glut_bits[i + 1] - 1, 0);
			upper_mask = (mask >> 8) & 0xFF;
			lower_mask = mask & 0xFF;
			dst[c][i + 1] = hextos32((((src[j + 0]) & upper_mask) << 8) |
					(src[j + 2] & lower_mask), s6e3fac_glut_bits[i + 1]);
		}
	}

	return 0;
}

/*
 * glut_itoc - trasnform GLUT int array to byte array
 */
int s6e3fac_glut_itoc(u8 *dst, s32(*src)[S6E3FAC_NR_GLUT_POINT])
{
	unsigned int i, j = 0, c, mask1, mask2, hex1, hex2;

	if (!src || !dst)
		return -EINVAL;

	for_each_color(c) {
		for (i = 0; i < ARRAY_SIZE(s6e3fac_glut_bits); i += 2, j += 3) {
			mask1 = GENMASK(s6e3fac_glut_bits[i + 0] - 1, 0);
			mask2 = GENMASK(s6e3fac_glut_bits[i + 1] - 1, 0);
			hex1 = s32tohex(src[c][i + 0], s6e3fac_glut_bits[i + 0]);
			hex2 = s32tohex(src[c][i + 1], s6e3fac_glut_bits[i + 1]);

			/* set GLUT_HEX */
			dst[j + 0] = ((hex1 >> 8) & (mask1 >> 8)) << 4 |
							 ((hex2 >> 8) & (mask2 >> 8));
			dst[j + 1] = hex1;
			dst[j + 2] = hex2;
		}
	}

	return 0;
}

int s6e3fac_do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
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

__visible_for_testing bool s6e3fac_cond_is_vsync_for_mode_change(struct panel_device *panel)
{
	struct panel_properties *props;

	if (!panel)
		return -EINVAL;
	props = &panel->panel_data.props;

	if (props->vrr_idx == props->vrr_origin_idx)
		return false;

	if ((props->vrr_origin_idx == S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1 ||
		props->vrr_origin_idx == S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1 ||
		props->vrr_origin_idx == S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4)) {
		panel_info("skip 1-frame(%s->%s)\n",
				s6e3fac_get_vrr_name(props->vrr_origin_idx),
				s6e3fac_get_vrr_name(props->vrr_idx));
		return true;
	}
	return false;
}

static inline bool is_analog_gamma_supported(int vrr_idx)
{
	if (vrr_idx >= S6E3FAC_VRR_60HS && vrr_idx < MAX_S6E3FAC_VRR)
		return true;
	return false;
}

__visible_for_testing int s6e3fac_maptbl_init_analog_gamma_offset(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct dimming_color_offset *dim_data;
	u8 gamma_data[S6E3FAC_ANALOG_GAMMA_WRITE_LEN];
	int i, ret, len, ofs = 0;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("invalid maptbl\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	for (i = 0; i < S6E3FAC_ANALOG_GAMMA_WRITE_SET_COUNT; i++) {
		len = get_panel_resource_size(panel, S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i]);
		if (len < 0)
			return len;

		if (maptbl_get_sizeof_maptbl(tbl) < ofs + len) {
			//out of range
			return -EINVAL;
		}
		ret = panel_resource_copy(panel, gamma_data + ofs, S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i]);
		if (ret < 0)
			return ret;

		panel_dbg("copy resource %s[%d]\n", S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i], i);

		dim_data = get_dimming_data(panel, i);
		if (!IS_ERR_OR_NULL(dim_data) && dim_data->rgb_color_offset) {
			gamma_byte_array_sum(gamma_data + ofs, gamma_data + ofs, dim_data->rgb_color_offset);
			panel_info("update gamma %s with offset[%d]\n", S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i], i);
		}
		ofs += len;
	}

	memcpy(tbl->arr, gamma_data, maptbl_get_sizeof_maptbl(tbl));

	return 0;
}

__visible_for_testing u32 s6e3fac_get_ddi_rev(struct panel_device *panel)
{
	struct panel_info *panel_data;
	u32 ddi_rev;

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;
	ddi_rev = (panel_data->id[2] & 0x30) >> 4;

	if (ddi_rev >= MAX_S6E3FAC_DDI_REV) {
		panel_err("invalid ddi_rev range %u", ddi_rev);
		return 0;
	}
	return ddi_rev;
}

int s6e3fac_get_octa_id(struct panel_device *panel, void *buf)
{
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	int len = 0;
	bool cell_id_exist = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, octa_id, "octa_id");

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

int s6e3fac_get_cell_id(struct panel_device *panel, void *buf)
{
	u8 date[PANEL_DATE_LEN] = { 0, }, coordinate[4] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, date, "date");
	panel_resource_copy(panel, coordinate, "coordinate");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		date[0], date[1], date[2], date[3], date[4], date[5], date[6],
		coordinate[0], coordinate[1], coordinate[2], coordinate[3]);
	return 0;

}

int s6e3fac_get_manufacture_code(struct panel_device *panel, void *buf)
{
	u8 code[5] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_resource_copy(panel, code, "code");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4]);

	return 0;
}

int s6e3fac_get_manufacture_date(struct panel_device *panel, void *buf)
{
	u16 year;
	u8 month, day, hour, min, date[PANEL_DATE_LEN] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_resource_copy(panel, date, "date");

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;

	snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d:%d\n",
			year, month, day, hour, min);

	return 0;
}

static int s6e3fac_smooth_dim_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_properties *props = &panel->panel_data.props;

	return panel_property_set_value(prop,
			(props->vrr_idx != props->vrr_origin_idx ||
			 panel->state.disp_on == PANEL_DISPLAY_OFF) ?
			S6E3FAC_SMOOTH_DIM_NOT_USE : S6E3FAC_SMOOTH_DIM_USE);
}

static struct panel_prop_enum_item s6e3fac_smooth_dim_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_SMOOTH_DIM_NOT_USE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_SMOOTH_DIM_USE),
};

static int s6e3fac_ddi_rev_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;

	return panel_property_set_value(prop, s6e3fac_get_ddi_rev(panel));
}

static struct panel_prop_enum_item s6e3fac_ddi_rev_enum_items[MAX_S6E3FAC_DDI_REV] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_DDI_REV_EVT0),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_DDI_REV_EVT0_OSC),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_DDI_REV_EVT1),
};

static int s6e3fac_acl_opr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	unsigned int acl_opr = panel_bl_get_acl_opr(panel_bl);

	return panel_property_set_value(prop,
			min(acl_opr, (u32)S6E3FAC_ACL_OPR_3));
}

static struct panel_prop_enum_item s6e3fac_acl_opr_enum_items[MAX_S6E3FAC_ACL_OPR] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_ACL_OPR_0),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_ACL_OPR_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_ACL_OPR_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_ACL_OPR_3),
};

static int s6e3fac_vrr_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;

	return panel_property_set_value(prop,
			find_s6e3fac_vrr(get_panel_vrr(panel)));
}

static struct panel_prop_enum_item s6e3fac_vrr_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_60NS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_48NS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_120HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_96HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_60HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_48HS),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4),
};

static struct panel_prop_list s6e3fac_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FAC_SMOOTH_DIM_PROPERTY,
			S6E3FAC_SMOOTH_DIM_USE, s6e3fac_smooth_dim_enum_items,
			s6e3fac_smooth_dim_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FAC_DDI_REV_PROPERTY,
			S6E3FAC_DDI_REV_EVT0, s6e3fac_ddi_rev_enum_items,
			s6e3fac_ddi_rev_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FAC_ACL_OPR_PROPERTY,
			S6E3FAC_ACL_OPR_0, s6e3fac_acl_opr_enum_items,
			s6e3fac_acl_opr_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FAC_VRR_PROPERTY,
			S6E3FAC_VRR_60NS, s6e3fac_vrr_enum_items,
			s6e3fac_vrr_property_update),
	/* range property */
};

struct pnobj_func s6e3fac_function_table[MAX_S6E3FAC_FUNCTION] = {
	[S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_BRT, s6e3fac_maptbl_init_gamma_mode2_brt),
	[S6E3FAC_MAPTBL_INIT_LPM_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_INIT_LPM_BRT, s6e3fac_maptbl_init_lpm_brt),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[S6E3FAC_MAPTBL_INIT_GRAM_IMG_PATTERN] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_INIT_GRAM_IMG_PATTERN, s6e3fac_maptbl_init_gram_img_pattern),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	[S6E3FAC_MAPTBL_COPY_MAFPC_ENABLE] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_COPY_MAFPC_ENABLE, s6e3fac_maptbl_copy_mafpc_enable),
	[S6E3FAC_MAPTBL_COPY_MAFPC_CTRL] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_COPY_MAFPC_CTRL, s6e3fac_maptbl_copy_mafpc_ctrl),
	[S6E3FAC_MAPTBL_COPY_MAFPC_SCALE] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_COPY_MAFPC_SCALE, s6e3fac_maptbl_copy_mafpc_scale),
#endif
	[S6E3FAC_MAPTBL_COPY_ELVSS_CAL] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_COPY_ELVSS_CAL, s6e3fac_maptbl_copy_elvss_cal),
#ifdef CONFIG_USDM_PANEL_HMD
	[S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT, s6e3fac_maptbl_init_gamma_mode2_hmd_brt),
#endif
	[S6E3FAC_MAPTBL_INIT_ANALOG_GAMMA_OFFSET] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_MAPTBL_INIT_ANALOG_GAMMA_OFFSET, s6e3fac_maptbl_init_analog_gamma_offset),
	[S6E3FAC_COND_IS_VSYNC_FOR_MODE_CHANGE] = __PNOBJ_FUNC_INITIALIZER(S6E3FAC_COND_IS_VSYNC_FOR_MODE_CHANGE, s6e3fac_cond_is_vsync_for_mode_change),
};

int s6e3fac_init(struct common_panel_info *cpi)
{
	static bool once;
	int ret;

	if (once)
		return 0;

	ret = panel_function_insert_array(s6e3fac_function_table,
			ARRAY_SIZE(s6e3fac_function_table));
	if (ret < 0)
		panel_err("failed to insert s6e3fac_function_table\n");

	cpi->prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array_size;
	cpi->prop_lists[USDM_DRV_LEVEL_DDI] = s6e3fac_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_DDI] = ARRAY_SIZE(s6e3fac_property_array);

	once = true;

	return 0;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
