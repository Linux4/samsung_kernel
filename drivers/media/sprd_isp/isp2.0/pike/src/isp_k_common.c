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

static int32_t isp_k_common_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_common_info_v2 common_info;

	memset(&common_info, 0x00, sizeof(common_info));

	ret = copy_from_user((void *)&common_info, param->property_param, sizeof(common_info));
	if (0 != ret) {
		printk("isp_k_common_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_CTRL_CH0, 3, common_info.fetch_sel_0);
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 4, common_info.sizer_sel_0  << 4);
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 16, common_info.store_sel_0  << 16);
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 2, common_info.fetch_sel_1);
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 6, common_info.sizer_sel_1);
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 18, common_info.store_sel_1);
	//REG_MWR(ISP_COMMON_CTRL_CH1, 3, common_info.fetch_sel_1);
	//REG_MWR(ISP_COMMON_CTRL_CH1, 3 << 4, common_info.sizer_sel_1  << 4);
	//REG_MWR(ISP_COMMON_CTRL_CH1, 3 << 16, common_info.store_sel_1  << 16);
	REG_MWR(ISP_COMMON_SPACE_SEL, 3 , common_info.fetch_color_format) ;
	REG_MWR(ISP_COMMON_SPACE_SEL, 3  << 2, common_info.store_color_format << 2) ;
	//REG_MWR(ISP_COMMON_CTRL_AWBM, BIT_0 , common_info.awbm_pos) ;
	//REG_MWR(ISP_COMMON_3A_CTRL0, 3, common_info.y_afm_pos_0);
	//REG_MWR(ISP_COMMON_3A_CTRL0, 3 << 4, common_info.y_aem_pos_0  << 4);
	//REG_MWR(ISP_COMMON_3A_CTRL1, 3, common_info.y_afm_pos_1);
	//REG_MWR(ISP_COMMON_3A_CTRL1, 3 << 4, common_info.y_aem_pos_1  << 4);

	REG_MWR(ISP_COMMON_LBUF_OFFSET, 0xFFFF, 0x460);
	//REG_WR(ISP_COMMON_LBUF_OFFSET0, ((common_info.lbuf_off.cfae_lbuf_offset & 0xFFFF) << 16 ) | (common_info.lbuf_off.comm_lbuf_offset & 0xFFFF));
	//REG_WR(ISP_COMMON_LBUF_OFFSET1, ((common_info.lbuf_off.ydly_lbuf_offset& 0xFFFF) << 16 ) | (common_info.lbuf_off.awbm_lbuf_offset & 0xFFFF));

	return ret;
}

static int32_t isp_k_common_version(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t version = 0;

	version = REG_RD(ISP_COMMON_VERSION);

	ret = copy_to_user(param->property_param, (void*)&version, sizeof(version));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_common_version: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_status0(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_COMMON_STATUS0);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_common_status0: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_status1(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_COMMON_STATUS1);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_common_status1: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_channel0_fetch_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t fetch_sel = 0;

	ret = copy_from_user((void *)&fetch_sel, param->property_param, sizeof(fetch_sel));
	if (0 != ret) {
		printk("isp_k_channel0_fetch_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_CTRL_CH0, 3, fetch_sel);

	return ret;
}

static int32_t isp_k_common_channel0_sizer_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t sizer_sel = 0;

	ret = copy_from_user((void *)&sizer_sel, param->property_param, sizeof(sizer_sel));
	if (0 != ret) {
		printk("isp_k_channel0_sizer_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 4, sizer_sel  << 4);

	return ret;
}

static int32_t isp_k_common_channel0_store_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t store_sel = 0;

	ret = copy_from_user((void *)&store_sel, param->property_param, sizeof(store_sel));
	if (0 != ret) {
		printk("isp_k_common_channel0_store_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 16, store_sel  << 16);

	return ret;
}

static int32_t isp_k_common_channel1_fetch_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t fetch_sel = 0;

	ret = copy_from_user((void *)&fetch_sel, param->property_param, sizeof(fetch_sel));
	if (0 != ret) {
		printk("isp_k_channel1_fetch_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 2, fetch_sel << 2);

//	REG_MWR(ISP_COMMON_CTRL_CH1, 3, fetch_sel);

	return ret;
}

static int32_t isp_k_common_channel1_sizer_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t sizer_sel = 0;

	ret = copy_from_user((void *)&sizer_sel, param->property_param, sizeof(sizer_sel));
	if (0 != ret) {
		printk("isp_k_common_channel1_sizer_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 6, sizer_sel << 6);

	//REG_MWR(ISP_COMMON_CTRL_CH1, 3 << 4, sizer_sel  << 4);

	return ret;
}

static int32_t isp_k_common_channel1_store_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t store_sel = 0;

	ret = copy_from_user((void *)&store_sel, param->property_param, sizeof(store_sel));
	if (0 != ret) {
		printk("isp_k_common_channel1_store_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 18, store_sel << 18);

//	REG_MWR(ISP_COMMON_CTRL_CH1, 3 << 16, store_sel  << 16);

	return ret;
}

static int32_t isp_k_common_fetch_color_space_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t color_space = 0;

	ret = copy_from_user((void *)&color_space, param->property_param, sizeof(color_space));
	if (0 != ret) {
		printk("isp_k_common_fetch_color_space_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SPACE_SEL, 3 , color_space) ;

	return ret;
}

static int32_t isp_k_common_store_color_space_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t color_space = 0;

	ret = copy_from_user((void *)&color_space, param->property_param, sizeof(color_space));
	if (0 != ret) {
		printk("isp_k_common_store_color_space_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_COMMON_SPACE_SEL, 3  << 2, color_space << 2) ;

	return ret;
}

static int32_t isp_k_common_awbm_pos_sel(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t awbm_pos = 0;

	ret = copy_from_user((void *)&awbm_pos, param->property_param, sizeof(awbm_pos));
	if (0 != ret) {
		printk("isp_k_common_awbm_pos_sel: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	//REG_MWR(ISP_COMMON_CTRL_AWBM, BIT_0 , awbm_pos) ;

	return ret;
}

static int32_t isp_k_common_channel0_y_afm_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position, param->property_param, sizeof(position));
	if (0 != ret) {
		printk("isp_k_common_channel0_y_afm_pos: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

//	REG_MWR(ISP_COMMON_3A_CTRL0, 3, position);

	return ret;
}

static int32_t isp_k_common_channel0_y_aem_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position, param->property_param, sizeof(position));
	if (0 != ret) {
		printk("isp_k_common_channel0_y_aem_pos: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

//	REG_MWR(ISP_COMMON_3A_CTRL0, 3 << 4, position  << 4);

	return ret;
}

static int32_t isp_k_common_channel1_y_afm_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position, param->property_param, sizeof(position));
	if (0 != ret) {
		printk("isp_k_common_channel1_y_afm_pos: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

//	REG_MWR(ISP_COMMON_3A_CTRL1, 3, position);

	return ret;
}

static int32_t isp_k_common_channel1_y_aem_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position, param->property_param, sizeof(position));
	if (0 != ret) {
		printk("isp_k_common_channel1_y_aem_pos: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

//	REG_MWR(ISP_COMMON_3A_CTRL1, 3 << 4, position  << 4);

	return ret;
}

static int32_t isp_k_common_lbuf_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t lbuf_off;

	memset(&lbuf_off, 0x00, sizeof(lbuf_off));
	ret = copy_from_user((void *)&lbuf_off, param->property_param, sizeof(lbuf_off));
	if (0 != ret) {
		printk("isp_k_common_lbuf_offset: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}


	REG_MWR(ISP_COMMON_LBUF_OFFSET, 0xFFFF, lbuf_off);

//	REG_WR(ISP_COMMON_LBUF_OFFSET0, ((lbuf_off.cfae_lbuf_offset & 0xFFFF) << 16 ) | (lbuf_off.comm_lbuf_offset & 0xFFFF));
//	REG_WR(ISP_COMMON_LBUF_OFFSET1, ((lbuf_off.ydly_lbuf_offset& 0xFFFF) << 16 ) | (lbuf_off.awbm_lbuf_offset & 0xFFFF));

	return ret;
}

int32_t isp_k_cfg_common(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_common: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_common: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_COMMON_BLOCK:
		ret = isp_k_common_block(param);
		break;
	case ISP_PRO_COMMON_VERSION:
		ret = isp_k_common_version(param);
		break;
	case ISP_PRO_COMMON_STATUS0:
		ret = isp_k_common_status0(param);
		break;
	case ISP_PRO_COMMON_STATUS1:
		ret = isp_k_common_status1(param);
		break;
	case ISP_PRO_COMMON_CH0_FETCH_SEL:
		ret = isp_k_common_channel0_fetch_sel(param);
		break;
	case ISP_PRO_COMMON_CH0_SIZER_SEL:
		ret = isp_k_common_channel0_sizer_sel(param);
		break;
	case ISP_PRO_COMMON_CH0_STORE_SEL:
		ret = isp_k_common_channel0_store_sel(param);
		break;
	case ISP_PRO_COMMON_CH1_FETCH_SEL:
		ret = isp_k_common_channel1_fetch_sel(param);
		break;
	case ISP_PRO_COMMON_CH1_SIZER_SEL:
		ret = isp_k_common_channel1_sizer_sel(param);
		break;
	case ISP_PRO_COMMON_CH1_STORE_SEL:
		ret = isp_k_common_channel1_store_sel(param);
		break;
	case ISP_PRO_COMMON_FETCH_COLOR_SPACE_SEL:
		ret = isp_k_common_fetch_color_space_sel(param);
		break;
	case ISP_PRO_COMMON_STORE_COLOR_SPACE_SEL:
		ret = isp_k_common_store_color_space_sel(param);
		break;
	case ISP_PRO_COMMON_AWBM_POS_SEL:
		ret = isp_k_common_awbm_pos_sel(param);
		break;
	case ISP_PRO_COMMON_CH0_AEM2_POS:
		ret = isp_k_common_channel0_y_aem_pos(param);
		break;
	case ISP_PRO_COMMON_CH0_Y_AFM_POS:
		ret = isp_k_common_channel0_y_afm_pos(param);
		break;
	case ISP_PRO_COMMON_CH1_AEM2_POS:
		ret = isp_k_common_channel1_y_aem_pos(param);
		break;
	case ISP_PRO_COMMON_CH1_Y_AFM_POS:
		ret = isp_k_common_channel1_y_afm_pos(param);
		break;
	case ISP_PRO_COMMON_LBUF_OFFSET:
		ret = isp_k_common_lbuf_offset(param);
		break;
	default:
		printk("isp_k_cfg_common: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
