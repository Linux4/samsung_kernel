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
#include "isp_drv.h"


#define ISP_YUV_YGAMMA_BUF0      0
#define ISP_YUV_YGAMMA_BUF1      1

static int32_t isp_k_pingpang_yuv_ygamma(struct coordinate_xy *nodes,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t i = 0, j = 0, buf_id = 0;
	unsigned long ybuf_addr = 0;
	struct coordinate_xy *p_nodes = NULL;

	if (!nodes) {
		ret = -1;
		printk("isp_k_pingpang_yuv_ygamma: node is null error .\n");
		return ret;
	}

	p_nodes = nodes;

		ybuf_addr = ISP_YGAMMA_BUF0_CH0;

	if (ISP_YUV_YGAMMA_BUF0 == isp_private->yuv_ygamma_buf_id) {
		buf_id = ISP_YUV_YGAMMA_BUF0;
		isp_private->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF1;
	} else {
		buf_id = ISP_YUV_YGAMMA_BUF1;
		isp_private->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF0;
	}

	REG_MWR(ISP_YGAMMA_PARAM, BIT_1, buf_id << 1);

	for (i = 0, j = 0; i < ISP_PINGPANG_YUV_YGAMMA_NUM; i++, j += 4) {
		REG_WR(ybuf_addr + j, p_nodes[i].node_y & 0xff);
	}

	REG_MWR(ISP_YGAMMA_PARAM, BIT_1, isp_private->yuv_ygamma_buf_id << 1);

	return ret;
}

static int32_t isp_k_ygamma_block(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	struct isp_dev_ygamma_info ygamma_info;

	ret = copy_from_user((void *)&ygamma_info, param->property_param, sizeof(ygamma_info));
	if (0 != ret) {
		printk("isp_k_ygamma_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ret = isp_k_pingpang_yuv_ygamma(ygamma_info.nodes, isp_private);
	if (0 != ret) {
		printk("isp_k_ygamma_block: pingpang error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}
	REG_MWR(ISP_YGAMMA_PARAM, BIT_0, ygamma_info.bypass);

	return ret;
}

int32_t isp_k_cfg_ygamma(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_ygamma: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_ygamma: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_YGAMMA_BLOCK:
		ret = isp_k_ygamma_block(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_ygamma: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
