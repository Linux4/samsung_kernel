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

#define ISP_MAX_WIDTH             3280
#define ISP_MAX_HEIGHT            2464
#define ISP_MAX_SLICE_WIDTH       3280
#define ISP_MAX_SLICE_HEIGHT      2464
#define ISP_AWB_WIN_UNIT          32
#define ISP_AWB_DEFAULT_GAIN      0x400
#define ISP_AF_MAX_WIN_NUM        9

static int32_t isp_k_capability_chip_id(struct isp_capability *param)
{
	int32_t ret = 0;
	uint32_t chip_id = 0;

	chip_id = ISP_CHIP_ID_PIKE;

	ret = copy_to_user(param->property_param, (void*)&chip_id, sizeof(chip_id));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_capability_chip_id: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_single_size(struct isp_capability *param)
{
	int32_t ret = 0;
	struct isp_img_size size = {0, 0};

	size.width = ISP_MAX_WIDTH;
	size.height = ISP_MAX_HEIGHT;

	ret = copy_to_user(param->property_param, (void*)&size, sizeof(size));
	if (0 != ret) {
		ret = -1;
		printk("isp_capability_single_size: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_continue_size(struct isp_capability *param)
{
	int32_t ret = 0;
	struct isp_img_size size = {0, 0};

	size.width = ISP_MAX_SLICE_WIDTH;
	size.height = ISP_MAX_SLICE_HEIGHT;

	ret = copy_to_user(param->property_param, (void*)&size, sizeof(size));
	if (0 != ret) {
		ret = -1;
		printk("isp_capability_continue_size: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_awb_win(struct isp_capability *param)
{
	int32_t ret = 0;
	struct isp_img_size size = {0, 0};

	size.width = ISP_AWB_WIN_UNIT;
	size.height = ISP_AWB_WIN_UNIT;

	ret = copy_to_user(param->property_param, (void*)&size, sizeof(size));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_capability_awb_win: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_awb_default_gain(struct isp_capability *param)
{
	int32_t ret = 0;
	uint32_t gain = 0;

	gain = ISP_AWB_DEFAULT_GAIN;

	ret = copy_to_user(param->property_param, (void*)&gain, sizeof(gain));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_capability_awb_default_gain: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_af_max_win_num(struct isp_capability *param)
{
	int32_t ret = 0;
	uint32_t win_num = 0;

	win_num = ISP_AF_MAX_WIN_NUM;

	ret = copy_to_user(param->property_param, (void*)&win_num, sizeof(win_num));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_capability_af_max_win_num: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_capability_time(struct isp_capability *param)
{
	int32_t ret = 0;
	struct isp_time time;
	struct timespec ts;

	ktime_get_ts(&ts);
	time.sec = ts.tv_sec;
	time.usec = ts.tv_nsec / NSEC_PER_USEC;

	ret = copy_to_user(param->property_param, (void*)&time, sizeof(time));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_capability_time: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_capability(void *param)
{
	int32_t ret = 0;
	struct isp_capability cap_param = {0, 0, NULL};

	if (!param) {
		printk("isp_capability: param is null error.\n");
		return -1;
	}

	ret = copy_from_user((void *)&cap_param, (void*)param, sizeof(cap_param));
	if ( 0 != ret) {
		printk("isp_capability: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (NULL == cap_param.property_param) {
		printk("isp_capability: property_param is null error.\n");
		return -1;
	}

	switch(cap_param.index) {
	case ISP_CAPABILITY_CHIP_ID:
		ret = isp_k_capability_chip_id(&cap_param);
		break;
	case ISP_CAPABILITY_SINGLE_SIZE:
		ret = isp_k_capability_single_size(&cap_param);
		break;
	case ISP_CAPABILITY_CONTINE_SIZE:
		ret = isp_k_capability_continue_size(&cap_param);
		break;
	case ISP_CAPABILITY_AWB_WIN:
		ret = isp_k_capability_awb_win(&cap_param);
		break;
	case ISP_CAPABILITY_AWB_DEFAULT_GAIN:
		ret = isp_k_capability_awb_default_gain(&cap_param);
		break;
	case ISP_CAPABILITY_AF_MAX_WIN_NUM:
		ret = isp_k_capability_af_max_win_num(&cap_param);
		break;
	case ISP_CAPABILITY_TIME:
		ret = isp_k_capability_time(&cap_param);
		break;
	default:
		printk("isp_capability: fail cmd id:%d, not supported.\n", cap_param.index);
		break;
	}

	return ret;
}

