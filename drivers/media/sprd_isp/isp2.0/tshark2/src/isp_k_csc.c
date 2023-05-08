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

static int32_t isp_k_csc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_csc_info csc_info;

	memset(&csc_info, 0x00, sizeof(csc_info));

	ret = copy_from_user((void *)&csc_info, param->property_param, sizeof(csc_info));
	if (0 != ret) {
		printk("isp_k_csc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_CSC_CTRL, BIT_0, csc_info.bypass);

	val = (csc_info.red_centre_x & 0x1FFF) | ((csc_info.red_centre_y & 0xFFF) << 16);
	REG_WR(ISP_CSC_RCC, val);

	val = (csc_info.blue_centre_x & 0x1FFF) | ((csc_info.blue_centre_y & 0xFFF) << 16);
	REG_WR(ISP_CSC_BCC, val);

	REG_WR(ISP_CSC_REDTHR, csc_info.red_threshold & 0x3FFFFFF);

	REG_WR(ISP_CSC_BLUETHR, (csc_info.blue_threshold & 0x3FFFFFF));

	val = (csc_info.red_p1_param & 0x1FFF) | ((csc_info.red_p2_param & 0x7FF) << 16);
	REG_WR(ISP_CSC_REDPOLY, val);

	val = (csc_info.blue_p1_param & 0x1FFF) | ((csc_info.blue_p2_param & 0x7FF) << 16);
	REG_WR(ISP_CSC_BLUEPOLY, val);

	REG_WR(ISP_CSC_MAXGAIN, (csc_info.max_gain_thr & 0x1FFF));

	val = (csc_info.start_pos.x & 0xFFF) | ((csc_info.start_pos.y & 0x1FFF) << 12);
	REG_WR(ISP_CSC_START_ROW_COL, val );

	val = csc_info.red_x2_init;
	REG_WR( ISP_CSC_REDX2INIT, (val & 0x3FFFFFF));
	val = csc_info.red_y2_init;
	REG_WR( ISP_CSC_REDY2INIT, (val & 0x3FFFFFF));

	val = csc_info.blue_x2_init;
	REG_WR( ISP_CSC_BLUEX2INIT, (val & 0x3FFFFFF));
	val = csc_info.blue_y2_init;
	REG_WR( ISP_CSC_BLUEY2INIT, (val & 0x3FFFFFF));

	return ret;

}

static int32_t isp_k_csc_pic_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size pic_size = {0, 0};

	ret = copy_from_user((void *)&pic_size, param->property_param, sizeof(pic_size));
	if (0 != ret) {
		printk("isp_k_csc_pic_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	val = (pic_size.width& 0x1FFF) | ((pic_size.height& 0xFFF) << 16);
	REG_WR(ISP_CSC_PICSIZE, val );

	return ret;
}

int32_t isp_k_cfg_csc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_csc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_csc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CSC_BLOCK:
		ret = isp_k_csc_block(param);
		break;
	case ISP_PRO_CSC_PIC_SIZE:
		ret = isp_k_csc_pic_size(param);
		break;
	default:
		printk("isp_k_cfg_csc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
