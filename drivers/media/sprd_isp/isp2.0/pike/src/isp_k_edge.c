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

static int32_t isp_k_edge_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_edge_info_v1 edge_info;
	uint32_t val = 0;

	memset(&edge_info, 0x00, sizeof(edge_info));

	ret = copy_from_user((void *)&edge_info, param->property_param, sizeof(edge_info));
	if (0 != ret) {
		printk("isp_k_edge_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_EE_CFG0, BIT_29 | BIT_28, edge_info.mode << 28);

	val = (edge_info.ee_str_m_n & 0x7F) | ((edge_info.ee_str_m_p & 0x7F) << 7)
		| ((edge_info.ee_str_d_n & 0x7F) << 14) | ((edge_info.ee_str_d_p & 0x7F) << 21);
	REG_MWR(ISP_EE_CFG0, 0xFFFFFFF, val);

	val = ((edge_info.ee_incr_d_p & 0xFF) << 8) | (edge_info.ee_incr_d_n & 0xFF);
	REG_MWR(ISP_EE_CFG1, 0xFFFF, val);

	val = ((edge_info.ee_thr_d_n & 0xFF) << 16) | ((edge_info.ee_thr_d_p & 0xFF) << 24);
	REG_MWR(ISP_EE_CFG1, 0xFFFF0000, val);

	val = (edge_info.ee_flat_thr_1 & 0xFF) | ((edge_info.ee_flat_thr_2 & 0xFF) << 8);
	REG_MWR(ISP_EE_CFG2, 0xFFFF, val);

	val = ((edge_info.ee_incr_m_n & 0xFF) << 16) | ((edge_info.ee_incr_m_p & 0xFF) << 24);
	REG_MWR(ISP_EE_CFG2, 0xFFFF0000, val);

	val = (edge_info.ee_txt_thr_1 & 0xFF) | ((edge_info.ee_txt_thr_2 & 0xFF) << 8)
		| ((edge_info.ee_txt_thr_3 & 0xFF) << 16);
	REG_WR(ISP_EE_CFG3, val);

	val = ((edge_info.ee_corner_cor & 0x1) << 28) | ((edge_info.ee_corner_th_p & 0x1F) << 23)
		| ((edge_info.ee_corner_th_n & 0x1F) << 18) | ((edge_info.ee_corner_gain_p & 0x7F) << 11)
		| ((edge_info.ee_corner_gain_n & 0x7F) << 4) | ((edge_info.ee_corner_sm_p & 0x3) << 2)
		| (edge_info.ee_corner_sm_n & 0x3);
	REG_WR(ISP_EE_CFG4, val);

	val = ((edge_info.ee_edge_smooth_mode & 0x3) << 26) | ((edge_info.ee_flat_smooth_mode & 0x3) << 24)
		| ((edge_info.ee_smooth_thr & 0xFF) << 8) | (edge_info.ee_smooth_strength & 0xFF);
	REG_MWR(ISP_EE_CFG5, 0xF00FFFF, val);

	val = (edge_info.sigma & 0xFF) << 16;
	REG_MWR(ISP_EE_CFG5, 0xFF0000, val);

	val = ((edge_info.ee_str_b_p & 0xFF) << 24) | ((edge_info.ee_str_b_n & 0xFF) << 16)
		| ((edge_info.ee_incr_b_p & 0xFF) << 8) | (edge_info.ee_incr_b_n & 0xFF);
	REG_WR(ISP_EE_CFG6, val);

	val = ((edge_info.ratio[1] & 0xFF) << 8) | (edge_info.ratio[0] & 0xFF);
	REG_MWR(ISP_EE_CFG7, 0xFFFF, val);

	val = (edge_info.ee_clip_after_smooth_en & 0x1) << 25;
	REG_MWR(ISP_EE_CFG7, BIT_25, val);

	val = ((edge_info.ipd_bypass & 0x1) << 24) | ((edge_info.ipd_flat_thr & 0xFF) << 16);
	REG_MWR(ISP_EE_CFG7, 0x1FF0000, val);

/*pike has no ADP_CFG0 ADP_CFG1 ADP_CFG2 registers*/

/*	val = (edge_info.ee_t1_cfg & 0x3FF) | ((edge_info.ee_t2_cfg & 0x3FF) << 10)
		| ((edge_info.ee_t3_cfg & 0x3FF) << 20);
	REG_WR(ISP_EE_ADP_CFG0, val);

	val = (edge_info.ee_t4_cfg & 0x3FF) | ((edge_info.ee_cv_clip_n & 0xFF) << 16)
		| ((edge_info.ee_cv_clip_p & 0xFF) << 24);
	REG_WR(ISP_EE_ADP_CFG1, val);

	val = (edge_info.ee_r1_cfg & 0xFF) | ((edge_info.ee_r2_cfg & 0xFF) << 8)
		| ((edge_info.ee_r3_cfg & 0xFF) << 16);
	REG_WR(ISP_EE_ADP_CFG2, val);*/


	if (edge_info.bypass) {
		REG_OWR(ISP_EE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_EE_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_edge(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_edge: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_edge: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_EDGE_BLOCK:
		ret = isp_k_edge_block(param);
		break;
	default:
		printk("isp_k_cfg_edge: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
