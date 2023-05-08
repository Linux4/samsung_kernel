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

static int32_t isp_k_bdn_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	struct isp_dev_bdn_info_v1 bdn_info;

	memset(&bdn_info, 0x00, sizeof(bdn_info));

	ret = copy_from_user((void *)&bdn_info, param->property_param, sizeof(bdn_info));
	if (0 != ret) {
		printk("isp_k_bdn_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_BDN_PARAM, BIT_1, bdn_info.radial_bypass << 1);

	REG_MWR(ISP_BDN_PARAM, 0x7F0, bdn_info.addback << 4);

	for (i = 0; i < 10; i++) {
		REG_WR(ISP_BDN_DISWEI0_0 + 40 * i, bdn_info.dis[i][0]);

		REG_WR(ISP_BDN_DISWEI0_1 + 40 * i, bdn_info.dis[i][1]);
	}

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 7; j++) {
			REG_WR(ISP_BDN_RANWEI0_0 + i * 40 + j * 4, bdn_info.ran[i][j]);
		}

		REG_WR(ISP_BDN_RANWEI0_7 + i * 40, bdn_info.ran[i][7]);
	}

	val = ((bdn_info.offset_x & 0x1FFF) << 12) | (bdn_info.offset_y & 0xFFF);
	REG_WR(ISP_BDN_1DLNC_CENTER, val);

	val = bdn_info.squ_x2 & 0x3FFFFFF;
	REG_WR(ISP_BDN_1DLNC_SQUARE_X, val);
	val = bdn_info.squ_y2 & 0xFFFFFF;
	REG_WR(ISP_BDN_1DLNC_SQUARE_Y, val);

	val = ((bdn_info.coef2 & 0xFFFF) << 11) | (bdn_info.coef & 0x7FF);
	REG_WR(ISP_BDN_1DLNC_P, val);

	val = ((bdn_info.start_pos_x & 0x1FFF) << 12) | (bdn_info.start_pos_y & 0xFFF);
	REG_WR(ISP_BDN_1DLNC_POS, val);

	REG_WR(ISP_BDN_1DLNC_OFFSET, bdn_info.offset & 0x3FFFFFF);

	REG_MWR(ISP_BDN_PARAM, BIT_0, bdn_info.bypass);

	return ret;

}

static int32_t isp_k_bdn_1d_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size = {0, 0};
	uint32_t val = 0;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_bdn_1d_slice_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.width & 0x1FFF) << 12) | (size.height & 0xFFF);
	REG_WR(ISP_BDN_1DLNC_SIZE, val);

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
	case ISP_PRO_BDN_SLICE_SIZE:
		ret = isp_k_bdn_1d_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_bdn: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

