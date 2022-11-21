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
#include <linux/vmalloc.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"
#include "isp_drv.h"

#define ISP_FRGB_GAMC_BUF0                0
#define ISP_FRGB_GAMC_BUF1                1

static int32_t isp_k_pingpang_frgb_gamc(struct coordinate_xy *nodes,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t i, j;
	uint32_t val = 0;
	unsigned long r_buf_addr;
	unsigned long g_buf_addr;
	unsigned long b_buf_addr;
	struct coordinate_xy *p_nodes = NULL;

	if (!nodes) {
		ret = -1;
		printk("isp_k_pingpang_frgb_gamc: node is null error .\n");
		return ret;
	}

	p_nodes = nodes;

	if (ISP_FRGB_GAMC_BUF0 == isp_private->full_gamma_buf_id) {
		r_buf_addr = ISP_FGAMMA_R_BUF1_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF1_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF1_CH0;
		isp_private->full_gamma_buf_id = ISP_FRGB_GAMC_BUF1;
	} else {
		r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
		isp_private->full_gamma_buf_id = ISP_FRGB_GAMC_BUF0;
	}

	for(i = 0,j = 0;i < ISP_PINGPANG_FRGB_GAMC_NUM;i++,j+=4) {
		REG_WR(r_buf_addr + j, p_nodes[i].node_y & 0xff);
		REG_WR(g_buf_addr + j, p_nodes[i].node_y & 0xff);
		REG_WR(b_buf_addr + j, p_nodes[i].node_y & 0xff);
	}

	val = ((isp_private->full_gamma_buf_id & 0x1 ) << 1) | ((isp_private->full_gamma_buf_id & 0x1) << 2) | ((isp_private->full_gamma_buf_id & 0x1 ) << 3);
	REG_MWR(ISP_GAMMA_PARAM, 0x0000000E, val);

	return ret;
}

static int32_t isp_k_gamma_block(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t bypass;
	struct isp_dev_gamma_info_v1 *param_p= (struct isp_dev_gamma_info_v1 *)param->property_param;
	struct coordinate_xy *nodes = NULL;

	nodes = (struct coordinate_xy *)isp_private->full_gamma_buf_addr;
	if (!nodes) {
		ret = -1;
		printk("isp_k_gamma_block: alloc memory error.\n");
		return -1;
	}

	ret = copy_from_user((void *)&bypass, (void *)&param_p->bypass, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_gamma_block: copy bypass error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ret = copy_from_user((void *)nodes, (void *)param_p->nodes, ISP_FRGB_GAMMA_BUF_SIZE);
	if (0 != ret) {
		printk("isp_k_gamma_block: copy nodes error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ret = isp_k_pingpang_frgb_gamc(nodes, isp_private);
	if (0 != ret) {
		printk("isp_k_gamma_block: pingpang error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_GAMMA_PARAM, BIT_0, bypass);

	return ret;

}

int32_t isp_k_cfg_gamma(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_gamma: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_gamma: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_GAMMA_BLOCK:
		ret = isp_k_gamma_block(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_gamma: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
