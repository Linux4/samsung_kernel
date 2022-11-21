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

static int32_t isp_k_glb_gain_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_glb_gain_info glb_gain_info;

	memset(&glb_gain_info, 0x00, sizeof(glb_gain_info));

	ret = copy_from_user((void *)&glb_gain_info, param->property_param, sizeof(glb_gain_info));
	if (0 != ret) {
		printk("isp_k_glb_gain_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_GAIN_GLB_PARAM, 0xFF, glb_gain_info.gain);

	if (glb_gain_info.bypass) {
		REG_OWR(ISP_GAIN_GLB_PARAM, BIT_31);
	} else {
		REG_MWR(ISP_GAIN_GLB_PARAM, BIT_31, 0);
	}

	return ret;
}

static int32_t isp_k_glb_gain_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_glb_gain_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_GAIN_GLB_PARAM, BIT_31);
	} else {
		REG_MWR(ISP_GAIN_GLB_PARAM, BIT_31, 0);
	}

	return ret;
}

static int32_t isp_k_glb_gain_set(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t glb_gain = 0;

	ret = copy_from_user((void *)&glb_gain, param->property_param, sizeof(glb_gain));
	if (0 != ret) {
		printk("isp_k_glb_gain_set: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_GAIN_GLB_PARAM, 0xFF, glb_gain & 0xFF);

	return ret;
}

static int32_t isp_k_glb_gain_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t gain_slice_size = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_glb_gain_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	gain_slice_size = (size.height & 0xFFFF) << 16 | (size.width & 0xFFFF);

	REG_WR(ISP_GAIN_GLB_SLICE_SIZE, gain_slice_size);

	return ret;
}

int32_t isp_k_cfg_glb_gain(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_glb_gain: param is null error.\n");
		return -1;
	}

	if (!param->property_param) {
		printk("isp_k_cfg_glb_gain: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_GLB_GAIN_BLOCK:
		ret = isp_k_glb_gain_block(param);
		break;
	case ISP_PRO_GLB_GAIN_BYPASS:
		ret = isp_k_glb_gain_bypass(param);
		break;
	case ISP_PRO_GLB_GAIN_SET:
		ret = isp_k_glb_gain_set(param);
		break;
	case ISP_PRO_GLB_GAIN_SLICE_SIZE:
		ret = isp_k_glb_gain_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_glb_gain: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
