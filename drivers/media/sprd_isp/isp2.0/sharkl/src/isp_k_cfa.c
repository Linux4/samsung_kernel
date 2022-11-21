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
	struct isp_dev_cfa_info cfa_info;

	memset(&cfa_info, 0x00, sizeof(cfa_info));

	ret = copy_from_user((void *)&cfa_info, param->property_param, sizeof(cfa_info));
	if (0 != ret) {
		printk("isp_k_cfa_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((cfa_info.ctrl_thrd & 0x3) << 7) | ((cfa_info.edge_thrd & 0x3F) << 1);
	REG_MWR(ISP_CFA_PARAM, 0x1FE, val);

	return ret;
}

static int32_t isp_k_cfa_thrd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cfa_thrd thrd = {0, 0};

	ret = copy_from_user((void *)&thrd, param->property_param, sizeof(thrd));
	if (0 != ret) {
		printk("isp_k_cfa_thrd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((thrd.ctrl & 0x3) << 7) | ((thrd.edge & 0x3F) << 1);
	REG_MWR(ISP_CFA_PARAM, 0x1FE, val);

	return ret;
}

static int32_t isp_k_cfa_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_cfa_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_CFA_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_cfa_slice_info(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t info = 0;

	ret = copy_from_user((void *)&info, param->property_param, sizeof(info));
	if (0 != ret) {
		printk("isp_k_cfa_slice_info: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_CFA_SLICE_INFO, (info & 0x0F));

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
	case ISP_PRO_CFA_THRD:
		ret = isp_k_cfa_thrd(param);
		break;
	case ISP_PRO_CFA_SLICE_SIZE:
		ret = isp_k_cfa_slice_size(param);
		break;
	case ISP_PRO_CFA_SLICE_INFO:
		ret = isp_k_cfa_slice_info(param);
		break;
	default:
		printk("isp_k_cfg_cfa: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
