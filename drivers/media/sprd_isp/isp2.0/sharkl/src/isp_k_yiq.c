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

static int32_t isp_k_yiq_block_ygamma(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_yiq_ygamma_info ygamma_info;

	memset(&ygamma_info, 0x00, sizeof(ygamma_info));

	ret = copy_from_user((void *)&ygamma_info, param->property_param, sizeof(ygamma_info));
	if (0 != ret) {
		printk("isp_k_yiq_block_ygamma: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (ygamma_info.x_node[0] << 24)
		| (ygamma_info.x_node[1] << 16)
		| (ygamma_info.x_node[2] << 8)
		| ygamma_info.x_node[3];
	REG_WR(ISP_YGAMMA_X0, val);

	val = (ygamma_info.x_node[4] << 24)
		| (ygamma_info.x_node[5] << 16)
		| (ygamma_info.x_node[6] << 8)
		| ygamma_info.x_node[7];
	REG_WR(ISP_YGAMMA_X1, val);

	val = (ygamma_info.y_node[0] << 24)
		| (ygamma_info.y_node[1] << 16)
		| (ygamma_info.y_node[2] << 8)
		| ygamma_info.y_node[3];
	REG_WR(ISP_YGAMMA_Y0, val);

	val = (ygamma_info.y_node[4] << 24)
		| (ygamma_info.y_node[5] << 16)
		| (ygamma_info.y_node[6] << 8)
		| ygamma_info.y_node[7];
	REG_WR(ISP_YGAMMA_Y1, val);

	val = (ygamma_info.y_node[8] << 24)
		| (ygamma_info.y_node[9] << 16);
	REG_WR(ISP_YGAMMA_Y2, val);

	val = ((ygamma_info.node_index[0] & 0x07) << 24)
		| ((ygamma_info.node_index[1] & 0x07) << 21)
		| ((ygamma_info.node_index[2] & 0x07) << 18)
		| ((ygamma_info.node_index[3] & 0x07) << 15)
		| ((ygamma_info.node_index[4] & 0x07) << 12)
		| ((ygamma_info.node_index[5] & 0x07) << 9)
		| ((ygamma_info.node_index[6] & 0x07) << 6)
		| ((ygamma_info.node_index[7] & 0x07) << 3)
		| (ygamma_info.node_index[8] & 0x07);
	REG_WR(ISP_YGAMMA_NODE_IDX, val);

	if (ygamma_info.bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_block_ae(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_yiq_ae_info ae_info;

	memset(&ae_info, 0x00, sizeof(ae_info));

	ret = copy_from_user((void *)&ae_info, param->property_param, sizeof(ae_info));
	if (0 != ret) {
		printk("isp_k_yiq_block_ae: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, (BIT_4 | BIT_3), (ae_info.src_sel << 3));
	REG_MWR(ISP_YIQ_PARAM, BIT_16, (ae_info.mode << 16));
	REG_MWR(ISP_YIQ_PARAM, (BIT_20 | BIT_19 | BIT_18 | BIT_17), (ae_info.skip_num << 17));

	if (ae_info.bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_block_flicker(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_yiq_flicker_info flicker_info;

	memset(&flicker_info, 0x00, sizeof(flicker_info));

	ret = copy_from_user((void *)&flicker_info, param->property_param, sizeof(flicker_info));
	if (0 != ret) {
		printk("isp_k_yiq_block_flicker: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, BIT_21, (flicker_info.mode << 21));
	REG_MWR(ISP_AF_V_HEIGHT, 0xFFFF, flicker_info.v_height);
	REG_MWR(ISP_AF_LINE_COUNTER, 0x1FF, flicker_info.line_counter);
	REG_MWR(ISP_AF_LINE_STEP, 0x0F, flicker_info.line_step);
	REG_MWR(ISP_AF_LINE_START, 0xFFF, flicker_info.line_start);

	if (flicker_info.bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_2, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_ygamma_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_yiq_ygamma_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_ygamma_xnode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_ygamma_xnode gamma_node;

	memset(&gamma_node, 0x00, sizeof(gamma_node));

	ret = copy_from_user((void *)&(gamma_node.x_node), param->property_param, sizeof(gamma_node.x_node));
	if (0 != ret) {
		printk("isp_k_yiq_ygamma_xnode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (gamma_node.x_node[0] << 24)
		| (gamma_node.x_node[1] << 16)
		| (gamma_node.x_node[2] << 8)
		| gamma_node.x_node[3];
	REG_WR(ISP_YGAMMA_X0, val);

	val = (gamma_node.x_node[4] << 24)
		| (gamma_node.x_node[5] << 16)
		| (gamma_node.x_node[6] << 8)
		| gamma_node.x_node[7];
	REG_WR(ISP_YGAMMA_X1, val);

	return ret;
}

static int32_t isp_k_yiq_ygamma_ynode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_ygamma_ynode gamma_node;

	memset(&gamma_node, 0x00, sizeof(gamma_node));

	ret = copy_from_user((void *)&(gamma_node.y_node), param->property_param, sizeof(gamma_node.y_node));
	if (0 != ret) {
		printk("isp_k_yiq_ygamma_ynode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (gamma_node.y_node[0] << 24)
		| (gamma_node.y_node[1] << 16)
		| (gamma_node.y_node[2] << 8)
		| gamma_node.y_node[3];
	REG_WR(ISP_YGAMMA_Y0, val);

	val = (gamma_node.y_node[4] << 24)
		| (gamma_node.y_node[5] << 16)
		| (gamma_node.y_node[6] << 8)
		| gamma_node.y_node[7];
	REG_WR(ISP_YGAMMA_Y1, val);

	val = (gamma_node.y_node[8] << 24)
		| (gamma_node.y_node[9] << 16);
	REG_WR(ISP_YGAMMA_Y2, val);

	return ret;
}

static int32_t isp_k_yiq_ygamma_index(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_ygamma_node_index ygamma_node_index;

	memset(&ygamma_node_index, 0x00, sizeof(ygamma_node_index));

	ret = copy_from_user((void *)&(ygamma_node_index.node_index), param->property_param, sizeof(ygamma_node_index.node_index));
	if (0 != ret) {
		printk("isp_k_yiq_ygamma_index: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((ygamma_node_index.node_index[0] & 0x07) << 24)
		| ((ygamma_node_index.node_index[1] & 0x07) << 21)
		| ((ygamma_node_index.node_index[2] & 0x07) << 18)
		| ((ygamma_node_index.node_index[3] & 0x07) << 15)
		| ((ygamma_node_index.node_index[4] & 0x07) << 12)
		| ((ygamma_node_index.node_index[5] & 0x07) << 9)
		| ((ygamma_node_index.node_index[6] & 0x07) << 6)
		| ((ygamma_node_index.node_index[7] & 0x07) << 3)
		| (ygamma_node_index.node_index[8] & 0x07);
	REG_WR(ISP_YGAMMA_NODE_IDX, val);

	return ret;
}

static int32_t isp_k_yiq_ae_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_yiq_ae_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_ae_src_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t src_sel = 0;

	ret = copy_from_user((void *)&src_sel, param->property_param, sizeof(src_sel));
	if (0 != ret) {
		printk("isp_k_yiq_ae_src_sel: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, (BIT_4 | BIT_3), (src_sel << 3));

	return ret;
}

static int32_t isp_k_yiq_ae_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_yiq_ae_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, BIT_16, (mode << 16));

	return ret;
}

static int32_t isp_k_yiq_ae_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t skip_num = 0;

	ret = copy_from_user((void *)&skip_num, param->property_param, sizeof(skip_num));
	if (0 != ret) {
		printk("isp_k_yiq_ae_skip_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, (BIT_20 | BIT_19 | BIT_18 | BIT_17), (skip_num << 17));

	return ret;
}

static int32_t isp_k_yiq_flicker_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_YIQ_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_YIQ_PARAM, BIT_2, 0);
	}

	return ret;
}

static int32_t isp_k_yiq_flicker_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_PARAM, BIT_21, (mode << 21));

	return ret;
}

static int32_t isp_k_yiq_flicker_vheight(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t v_height = 0;

	ret = copy_from_user((void *)&v_height, param->property_param, sizeof(v_height));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_vheight: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AF_V_HEIGHT, 0xFFFF, v_height);

	return ret;
}

static int32_t isp_k_yiq_flicker_line_conter(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t line_counter = 0;

	ret = copy_from_user((void *)&line_counter, param->property_param, sizeof(line_counter));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_line_conter: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AF_LINE_COUNTER, 0x1FF, line_counter);

	return ret;
}

static int32_t isp_k_yiq_flicker_line_step(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t line_step = 0;

	ret = copy_from_user((void *)&line_step, param->property_param, sizeof(line_step));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_line_step: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AF_LINE_STEP, 0xF, line_step);

	return ret;
}

static int32_t isp_k_yiq_flicker_line_start(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t line_start = 0;

	ret = copy_from_user((void *)&line_start, param->property_param, sizeof(line_start));
	if (0 != ret) {
		printk("isp_k_yiq_flicker_line_start: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AF_LINE_START, 0xFFF, line_start);

	return ret;
}

int32_t isp_k_cfg_yiq(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_yiq: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_yiq: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_RPO_YIQ_BLOCK_YGAMMA:
		ret = isp_k_yiq_block_ygamma(param);
		break;
	case ISP_RPO_YIQ_BLOCK_AE:
		ret = isp_k_yiq_block_ae(param);
		break;
	case ISP_RPO_YIQ_BLOCK_FLICKER:
		ret = isp_k_yiq_block_flicker(param);
		break;
	case ISP_PRO_YIQ_YGAMMA_BYPASS:
		ret = isp_k_yiq_ygamma_bypass(param);
		break;
	case ISP_PRO_YIQ_YGAMMA_XNODE:
		ret = isp_k_yiq_ygamma_xnode(param);
		break;
	case ISP_PRO_YIQ_YGAMMA_YNODE:
		ret = isp_k_yiq_ygamma_ynode(param);
		break;
	case ISP_PRO_YIQ_YGAMMA_INDEX:
		ret = isp_k_yiq_ygamma_index(param);
		break;
	case ISP_PRO_YIQ_AE_BYPASS:
		ret = isp_k_yiq_ae_bypass(param);
		break;
	case ISP_PRO_YIQ_AE_SOURCE_SEL:
		ret = isp_k_yiq_ae_src_sel(param);
		break;
	case ISP_PRO_YIQ_AE_MODE:
		ret = isp_k_yiq_ae_mode(param);
		break;
	case ISP_PRO_YIQ_AE_SKIP_NUM:
		ret = isp_k_yiq_ae_skip_num(param);
		break;
	case ISP_PRO_YIQ_FLICKER_BYPASS:
		ret = isp_k_yiq_flicker_bypass(param);
		break;
	case ISP_PRO_YIQ_FLICKER_MODE:
		ret = isp_k_yiq_flicker_mode(param);
		break;
	case ISP_PRO_YIQ_FLICKER_VHEIGHT:
		ret = isp_k_yiq_flicker_vheight(param);
		break;
	case ISP_PRO_YIQ_FLICKER_LINE_CONTER:
		ret = isp_k_yiq_flicker_line_conter(param);
		break;
	case ISP_PRO_YIQ_FLICKER_LINE_STEP:
		ret = isp_k_yiq_flicker_line_step(param);
		break;
	case ISP_PRO_YIQ_FLICKER_LINE_START:
		ret = isp_k_yiq_flicker_line_start(param);
		break;
	default:
		printk("isp_k_cfg_yiq: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
