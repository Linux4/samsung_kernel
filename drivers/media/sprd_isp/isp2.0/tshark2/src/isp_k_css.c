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

static int32_t isp_k_css_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_css_info_v1 css_info;
	uint32_t val = 0;

	ret = copy_from_user((void *)&css_info, param->property_param, sizeof(css_info));
	if (0 != ret) {
		printk("isp_k_css_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (css_info.lh_chrom_th & 0x1F) << 24;
	REG_MWR(ISP_CSS_PARAM, 0x1F000000, val);

	val = ((css_info.chrom_lower_th[0] & 0x1F) << 16) | ((css_info.chrom_lower_th[1] & 0x1F) << 8)
		| (css_info.chrom_lower_th[2] & 0x1F);
	REG_MWR(ISP_CSS_CHROM_L_TH_0, 0x1F1F1F, val);

	val = ((css_info.chrom_lower_th[3] & 0x1F) << 24) | ((css_info.chrom_lower_th[4] & 0x1F) << 16)
		| ((css_info.chrom_lower_th[5] & 0x1F) << 8) | (css_info.chrom_lower_th[6] & 0x1F);
	REG_WR(ISP_CSS_CHROM_L_TH_1, val);

	val = ((css_info.chrom_high_th[0] & 0x1F) << 16) | ((css_info.chrom_high_th[1] & 0x1F) << 8)
		| (css_info.chrom_high_th[2] & 0x1F);
	REG_WR(ISP_CSS_CHROM_H_TH_0, val);

	val = ((css_info.chrom_high_th[3] & 0x1F) << 24) | ((css_info.chrom_high_th[4] & 0x1F) << 16)
		| ((css_info.chrom_high_th[5] & 0x1F) << 8) | (css_info.chrom_high_th[6] & 0x1F);
	REG_WR(ISP_CSS_CHROM_H_TH_1, val);

	val = ((css_info.lum_low_shift & 0x7) << 16) | ((css_info.lum_hig_shift & 0x7) << 8);
	REG_MWR(ISP_CSS_PARAM, 0x70700, val);

	val = (css_info.lh_ratio[7] & 0xF) | ((css_info.lh_ratio[6] & 0xF) << 4)
		| ((css_info.lh_ratio[5] & 0xF) <<  8) | ((css_info.lh_ratio[4] & 0xF) << 12)
		| ((css_info.lh_ratio[3] & 0xF) << 16) | ((css_info.lh_ratio[2] & 0xF) << 20)
		| ((css_info.lh_ratio[1] & 0xF) << 24) | ((css_info.lh_ratio[0] & 0xF) << 28);
	REG_WR(ISP_CSS_LH_RATIO, val);

	val = (css_info.ratio[7] & 0xF) | ((css_info.ratio[6] & 0xF) << 4)
		| ((css_info.ratio[5] & 0xF) <<  8) | ((css_info.ratio[4] & 0xF) << 12)
		| ((css_info.ratio[3] & 0xF) << 16) | ((css_info.ratio[2] & 0xF) << 20)
		| ((css_info.ratio[1] & 0xF) << 24) | ((css_info.ratio[0] & 0xF) << 28);
	REG_WR(ISP_CSS_RATIO, val);

	val = (css_info.lum_hh_th & 0xFF) | ((css_info.lum_hig_th & 0xFF) << 8)
		| ((css_info.lum_ll_th & 0xFF) << 16) | ((css_info.lum_low_th & 0xFF) << 24);
	REG_WR(ISP_CSS_THRD0, val);

	val = (css_info.v_th_0_h & 0xFF) | ((css_info.v_th_0_l & 0xFF) << 8)
		| ((css_info.u_th_0_h & 0xFF) << 16) | ((css_info.u_th_0_l & 0xFF) << 24);
	REG_WR(ISP_CSS_EX_TH_0, val);

	val = (css_info.v_th_1_h & 0xFF) | ((css_info.v_th_1_l & 0xFF) << 8)
		| ((css_info.u_th_1_h & 0xFF) << 16) | ((css_info.u_th_1_l & 0xFF) << 24);
	REG_WR(ISP_CSS_EX_TH_1, val);

	REG_MWR(ISP_CSS_CHROM_L_TH_0, 0x1F000000, css_info.cutoff_th << 24);

	if (css_info.bypass) {
		REG_OWR(ISP_CSS_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CSS_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_css(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_css: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_css: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CSS_BLOCK:
		ret = isp_k_css_block(param);
		break;
	default:
		printk("isp_k_cfg_css: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
