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

static int32_t isp_k_cfa_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cfa_info_v1 cfa_info;

	memset(&cfa_info, 0x00, sizeof(cfa_info));

	ret = copy_from_user((void *)&cfa_info, param->property_param, sizeof(cfa_info));
	if (0 != ret) {
		printk("isp_k_cfa_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_CFAE_EE_CFG0, BIT_0, cfa_info.bypass);

	REG_MWR(ISP_CFAE_EE_CFG0, 0xF<<28, cfa_info.grid_gain << 28);

	REG_MWR(ISP_CFAE_EE_CFG0, 0x3<<24, cfa_info.avg_mode << 24);

	REG_MWR(ISP_CFAE_EE_CFG0, 0xFFF<<12, cfa_info.gbuf_addr_max << 12);

	REG_MWR(ISP_CFAE_EE_CFG0, BIT_1, cfa_info.ee_bypass << 1);

	REG_MWR(ISP_CFAE_EE_CFG0, 0x3F<<4, cfa_info.doee_base << 4);

	REG_MWR(ISP_CFAE_EE_CFG1, 0xF<<16, cfa_info.inter_chl_gain << 16);

	val = (cfa_info.cfa_uni_dir_intplt_tr & 0xFFFF) | ((cfa_info.cfai_ee_uni_dir_tr & 0xFFFF) << 16);
	REG_WR(ISP_CFAE_THRD_0,val);
	val = (cfa_info.cfai_ee_edge_tr & 0xFFF) | ((cfa_info.cfai_ee_diagonal_tr & 0xFFF) << 16);
	REG_WR(ISP_CFAE_THRD_1,val);
	val =(cfa_info.cfai_ee_grid_tr & 0xFFF) | ((cfa_info.cfai_doee_clip_tr & 0x3F) << 12)
		| ((cfa_info.cfai_ee_saturation_level & 0x3FF) << 22);
	REG_WR(ISP_CFAE_THRD_2,val);
	val = (cfa_info.plt_diff_tr & 0xFFFF) | ((cfa_info.grid_min_tr & 0xFFFF) << 16);
	REG_WR(ISP_CFAE_THRD_3,val);
	val = (cfa_info.strength_tr_neg & 0xFFFF) | ((cfa_info.strength_tr_pos & 0xFFFF) << 16);
	REG_WR(ISP_CFAE_THRD_4,val);
	REG_MWR(ISP_CFAE_EE_CFG1, 0x3F, cfa_info.ee_strength_neg);
	REG_MWR(ISP_CFAE_EE_CFG1, 0x3F << 8, cfa_info.ee_strength_pos << 8);

	return ret;
}

int32_t isp_k_cfg_cfa(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_cfa: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_cfa: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CFA_BLOCK:
		ret = isp_k_cfa_block(param);
		break;
	default:
		printk("isp_k_cfg_cfa: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

