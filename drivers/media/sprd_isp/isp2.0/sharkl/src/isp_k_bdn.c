/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"

#define DENOISE_DISWEI_NUM_MAX      256
#define DENOISE_RANWEI_NUM_MAX      256

static const uint8_t diswei_tab[DENOISE_DISWEI_NUM_MAX][19] = {
#include "denoise_diswei.h"
};

static const uint8_t ranwei_tab[DENOISE_RANWEI_NUM_MAX][31] = {
#include "denoise_ranwei.h"
};

int32_t isp_k_bdn_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	struct isp_dev_wavelet_denoise_info denoise_info;

	memset(&denoise_info, 0x00, sizeof(denoise_info));

	ret = copy_from_user((void *)&denoise_info, param->property_param, sizeof(denoise_info));
	if (0 != ret) {
		printk("isp_k_wavelet_denoise_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (DENOISE_DISWEI_NUM_MAX < denoise_info.diswei_level
		|| DENOISE_RANWEI_NUM_MAX < denoise_info.ranwei_level) {
		printk("isp_k_wavelet_denoise_block: index error: %d %d\n",
			denoise_info.diswei_level, denoise_info.ranwei_level);
		return -1;
	}

	addr = ISP_WAVE_DISWEI_0;
	for (i = 0; i < 16; i += 4) {
		val = (diswei_tab[denoise_info.diswei_level][i + 3] << 24)
			| (diswei_tab[denoise_info.diswei_level][i + 2] << 16)
			| (diswei_tab[denoise_info.diswei_level][i + 1] << 8)
			| diswei_tab[denoise_info.diswei_level][i];
		REG_WR(addr, val);
		addr += 4;
	}
	val = (diswei_tab[denoise_info.diswei_level][18] << 16)
		| (diswei_tab[denoise_info.diswei_level][17] << 8)
		| diswei_tab[denoise_info.diswei_level][16];
	REG_WR(ISP_WAVE_DISWEI_4, val);

	addr = ISP_WAVE_RANWEI_0;
	for (i = 0; i < 28; i += 4) {
		val = (ranwei_tab[denoise_info.ranwei_level][i + 3] << 24)
			| (ranwei_tab[denoise_info.ranwei_level][i + 2] << 16)
			| (ranwei_tab[denoise_info.ranwei_level][i + 1] << 8)
			| ranwei_tab[denoise_info.ranwei_level][i];
		REG_WR(addr, val);
		addr += 4;
	}
	val = (ranwei_tab[denoise_info.ranwei_level][30] << 16)
		| (ranwei_tab[denoise_info.ranwei_level][29] << 8)
		| ranwei_tab[denoise_info.ranwei_level][28];
	REG_WR(ISP_WAVE_RANWEI_7, val);

	if (denoise_info.bypass) {
		REG_OWR(ISP_WAVE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_WAVE_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_bdn_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_wavelet_denoise_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_WAVE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_WAVE_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_bdn_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_wavelet_denoise_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_WAVE_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_bdn_diswei(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	uint32_t diswei_index = 0;

	ret = copy_from_user((void *)&diswei_index, param->property_param, sizeof(diswei_index));
	if (0 != ret) {
		printk("isp_k_wavelet_denoise_diswei: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (DENOISE_DISWEI_NUM_MAX < diswei_index) {
		printk("isp_k_wavelet_denoise_diswei: index error: %d\n", diswei_index);
		return -1;
	}

	addr = ISP_WAVE_DISWEI_0;
	for (i = 0; i < 16; i += 4) {
		val = (diswei_tab[diswei_index][i + 3] << 24)
			| (diswei_tab[diswei_index][i + 2] << 16)
			| (diswei_tab[diswei_index][i + 1] << 8)
			| diswei_tab[diswei_index][i];
		REG_WR(addr, val);
		addr += 4;
	}
	val = (diswei_tab[diswei_index][18] << 16)
		| (diswei_tab[diswei_index][17] << 8)
		| diswei_tab[diswei_index][16];
	REG_WR(ISP_WAVE_DISWEI_4, val);

	return ret;
}

static int32_t isp_k_bdn_ranwei(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	uint32_t ranwei_index = 0;

	ret = copy_from_user((void *)&ranwei_index, param->property_param, sizeof(ranwei_index));
	if (0 != ret) {
		printk("isp_k_wavelet_denoise_ranwei: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (DENOISE_RANWEI_NUM_MAX < ranwei_index) {
		printk("isp_k_wavelet_denoise_ranwei: index error: %d\n", ranwei_index);
		return -1;
	}

	addr = ISP_WAVE_RANWEI_0;
	for (i = 0; i < 28; i += 4) {
		val = (ranwei_tab[ranwei_index][i + 3] << 24)
			| (ranwei_tab[ranwei_index][i + 2] << 16)
			| (ranwei_tab[ranwei_index][i + 1] << 8)
			| ranwei_tab[ranwei_index][i];
		REG_WR(addr, val);
		addr += 4;
	}
	val = (ranwei_tab[ranwei_index][30] << 16)
		| (ranwei_tab[ranwei_index][29] << 8)
		| ranwei_tab[ranwei_index][28];
	REG_WR(ISP_WAVE_RANWEI_7, val);

	return ret;
}

int32_t isp_k_cfg_bdn(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_bdn: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_bdn: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_BDN_BLOCK:
		ret = isp_k_bdn_block(param);
		break;
	case ISP_PRO_BDN_BYPASS:
		ret = isp_k_bdn_bypass(param);
		break;
	case ISP_PRO_BDN_SLICE_SIZE:
		ret = isp_k_bdn_slice_size(param);
		break;
	case ISP_PRO_BDN_DISWEI:
		ret = isp_k_bdn_diswei(param);
		break;
	case ISP_PRO_BDN_RANWEI:
		ret = isp_k_bdn_ranwei(param);
		break;
	default:
		printk("isp_k_cfg_bdn: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
