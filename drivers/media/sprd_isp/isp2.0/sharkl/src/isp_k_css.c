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
	uint32_t val = 0;
	struct isp_dev_css_info cs_info;

	memset(&cs_info, 0x00, sizeof(cs_info));

	ret = copy_from_user((void *)&cs_info, param->property_param, sizeof(cs_info));
	if (0 != ret) {
		printk("isp_k_css_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (cs_info.lower_thrd[0] << 24)
		| (cs_info.lower_thrd[1] << 16)
		| (cs_info.lower_thrd[2] << 8)
		| cs_info.lower_thrd[3];
	REG_WR(ISP_CSS_THRD0, val);

	val = (cs_info.lower_thrd[4] << 24)
		| (cs_info.lower_thrd[5] << 16)
		| (cs_info.lower_thrd[6] << 8)
		| cs_info.luma_thrd;
	REG_WR(ISP_CSS_THRD1, val);

	val = (cs_info.lower_sum_thrd[0] << 24)
		| (cs_info.lower_sum_thrd[1] << 16)
		| (cs_info.lower_sum_thrd[2] << 8)
		| cs_info.lower_sum_thrd[3];
	REG_WR(ISP_CSS_THRD2, val);

	val = (cs_info.lower_sum_thrd[4] << 24)
		| (cs_info.lower_sum_thrd[5] << 16)
		| (cs_info.lower_sum_thrd[6] << 8)
		| cs_info.chroma_thrd;
	REG_WR(ISP_CSS_THRD3, val);

	val = ((cs_info.ratio[0] & 0x0F) << 28)
		| ((cs_info.ratio[1] & 0x0F) << 24)
		| ((cs_info.ratio[2] & 0x0F) << 20)
		| ((cs_info.ratio[3] & 0x0F) << 16)
		| ((cs_info.ratio[4] & 0x0F) << 12)
		| ((cs_info.ratio[5] & 0x0F) << 8)
		| ((cs_info.ratio[6] & 0x0F) << 4)
		| (cs_info.ratio[7] & 0x0F);
	REG_WR(ISP_CSS_RATIO, val);

	if (cs_info.bypass) {
		REG_OWR(ISP_CSS_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CSS_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_css_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_css_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_CSS_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CSS_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_css_thrd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_css_thrd thrd;

	memset(&thrd, 0x00, sizeof(thrd));

	ret = copy_from_user((void *)&thrd, param->property_param, sizeof(thrd));
	if (0 != ret) {
		printk("isp_k_css_thrd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (thrd.lower_thrd[0] << 24)
		| (thrd.lower_thrd[1] << 16)
		| (thrd.lower_thrd[2] << 8)
		| thrd.lower_thrd[3];
	REG_WR(ISP_CSS_THRD0, val);

	val = (thrd.lower_thrd[4] << 24)
		| (thrd.lower_thrd[5] << 16)
		| (thrd.lower_thrd[6] << 8)
		| thrd.luma_thrd;
	REG_WR(ISP_CSS_THRD1, val);

	val = (thrd.lower_sum_thrd[0] << 24)
		| (thrd.lower_sum_thrd[1] << 16)
		| (thrd.lower_sum_thrd[2] << 8)
		| thrd.lower_sum_thrd[3];
	REG_WR(ISP_CSS_THRD2, val);

	val = (thrd.lower_sum_thrd[4] << 24)
		| (thrd.lower_sum_thrd[5] << 16)
		| (thrd.lower_sum_thrd[6] << 8)
		| thrd.chroma_thrd;
	REG_WR(ISP_CSS_THRD3, val);

	return ret;
}

static int32_t isp_k_css_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_css_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_CSS_SLICE_INFO, val);

	return ret;
}

static int32_t isp_k_css_ratio(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_css_ratio css_ratio;

	memset(&css_ratio, 0x00, sizeof(css_ratio));

	ret = copy_from_user((void *)&(css_ratio.ratio), param->property_param, sizeof(css_ratio.ratio));
	if (0 != ret) {
		printk("isp_k_css_ratio: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((css_ratio.ratio[0] & 0x0F) << 28)
		| ((css_ratio.ratio[1] & 0x0F) << 24)
		| ((css_ratio.ratio[2] & 0x0F) << 20)
		| ((css_ratio.ratio[3] & 0x0F) << 16)
		| ((css_ratio.ratio[4] & 0x0F) << 12)
		| ((css_ratio.ratio[5] & 0x0F) << 8)
		| ((css_ratio.ratio[6] & 0x0F) << 4)
		| (css_ratio.ratio[7] & 0x0F);
	REG_WR(ISP_CSS_RATIO, val);

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

	switch (param->property) {
	case ISP_PRO_CSS_BLOCK:
		ret = isp_k_css_block(param);
		break;
	case ISP_PRO_CSS_BYPASS:
		ret = isp_k_css_bypass(param);
		break;
	case ISP_PRO_CSS_THRD:
		ret = isp_k_css_thrd(param);
		break;
	case ISP_PRO_CSS_SLICE_SIZE:
		ret = isp_k_css_slice_size(param);
		break;
	case ISP_PRO_CSS_RATIO:
		ret = isp_k_css_ratio(param);
		break;
	default:
		printk("isp_k_cfg_css: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
