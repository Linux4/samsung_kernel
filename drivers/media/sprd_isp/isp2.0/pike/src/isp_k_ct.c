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

#define ISP_3D_LUT_BUF0                0
#define ISP_3D_LUT_BUF1                1

static int32_t isp_k_ct_block(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t buf_size = 0;
	unsigned long dst_buf = 0;
	struct isp_dev_ct_info ct_info;
	void *data_ptr = NULL;

	buf_size = ISP_3D_LUT_ITEM * 4;
	memset(&ct_info, 0x00, sizeof(ct_info));

	ret = copy_from_user((void *)&ct_info, param->property_param, sizeof(ct_info));
	if (0 != ret) {
		printk("isp_k_ct_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

#ifdef CONFIG_64BIT
	data_ptr = (void*)(((unsigned long)ct_info.data_ptr[1] << 32) | ct_info.data_ptr[0]);
#else
	data_ptr = (void*)(ct_info.data_ptr[0]);
#endif
	if (NULL == data_ptr || ct_info.size > buf_size) {
		printk("isp_k_ct_block: memory error: %p %x.\n",
			data_ptr, ct_info.size);
	}

	dst_buf = ISP_3D_LUT0_OUTPUT;
	isp_private->ct_load_buf_id = ISP_3D_LUT_BUF0;

	ret = copy_from_user((void *)dst_buf, data_ptr, ct_info.size);
	if (0 != ret) {
		printk("isp_k_ct_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	//REG_MWR(ISP_CT_PARAM, BIT_1, (isp_private->ct_load_buf_id << 1));

	REG_MWR(ISP_CT_PARAM, BIT_0, ct_info.bypass);

	return ret;
}

int32_t isp_k_cfg_ct(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_ct: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_ct: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CT_BLOCK:
		ret = isp_k_ct_block(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_ct: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
