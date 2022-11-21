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

static int32_t isp_k_pwd_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_pre_wavelet_info_v1 pwd_info;

	memset(&pwd_info, 0x00, sizeof(pwd_info));

	ret = copy_from_user((void *)&pwd_info, param->property_param, sizeof(pwd_info));
	if (0 != ret) {
		printk("isp_k_pwd_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((pwd_info.gain_thrs0 & 0x7FF) << 6) | ((pwd_info.gain_thrs1 & 0x7FF) << 17);
	REG_MWR(ISP_PWD_PARAM, 0x0FFFFFC0, val);

	val = (pwd_info.bitshift0 & 0x7) | ((pwd_info.bitshift1 & 0xFF) << 3);
	REG_MWR(ISP_PWD_PARAM1, 0x7FF, val);

	val = (pwd_info.offset & 0xFF) << 11;
	REG_MWR(ISP_PWD_PARAM1, 0x7F800, val);

	val = (pwd_info.nsr_slope & 0x3FF) << 4;
	REG_MWR(ISP_PWD_PARAM2, 0x3FF0, val);

	val = pwd_info.lum_ratio & 0xF;
	REG_MWR(ISP_PWD_PARAM2, 0xF, val);

	val = ((pwd_info.center_pos_x & 0x1FFF) << 12) | (pwd_info.center_pos_y & 0xFFF);
	REG_WR(ISP_PWD_PARAM3, val);

	val = pwd_info.delta_x2 & 0x3FFFFFF;
	REG_WR(ISP_PWD_PARAM4, val);
	val = pwd_info.delta_y2 & 0xFFFFFF;
	REG_WR(ISP_PWD_PARAM5, val);

	val = pwd_info.r2_thr & 0x3FFFFFF;
	REG_WR(ISP_PWD_PARAM6, val);

	val = ((pwd_info.p_param1 & 0x3FFF) << 12) | (pwd_info.p_param2 & 0xFFF);
	REG_WR(ISP_PWD_PARAM7, val);

	val = (pwd_info.addback & 0x7F) << 13;
	REG_MWR(ISP_PWD_PARAM8, 0xFE000, val);

	val = pwd_info.gain_max_thr & 0x1FFF;
	REG_MWR(ISP_PWD_PARAM8, 0x1FFF, val);

	val = ((pwd_info.pos_x & 0x1FFF) << 12) | (pwd_info.pos_y & 0xFFF);
	REG_WR(ISP_PWD_PARAM10, val);

	REG_MWR(ISP_PWD_PARAM, BIT_1, pwd_info.radial_bypass << 1);
	REG_MWR(ISP_PWD_PARAM, BIT_0, pwd_info.bypass);

	return ret;
}

static int32_t isp_k_pwd_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_pwd_slice_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.width & 0x1FFF) << 12) | (size.height & 0xFFF);
	REG_WR(ISP_PWD_PARAM9, val);

	return ret;
}

int32_t isp_k_cfg_pwd(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_pwd: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_pwd: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_PWD_BLOCK:
		ret = isp_k_pwd_block(param);
		break;
	case ISP_PRO_PWD_SLICE_SIZE:
		ret = isp_k_pwd_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_pwd: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

