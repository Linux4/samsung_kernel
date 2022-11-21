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
#include <linux/vmalloc.h>
#include "isp_reg.h"
#include "isp_drv.h"

static int32_t isp_k_vst_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_vst_info_v1 vst_info;

	ret = copy_from_user((void *)&vst_info, param->property_param, sizeof(vst_info));
	if (0 != ret) {
		printk("isp_k_vst_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_VST_PARA, BIT_0, vst_info.bypass);

	return ret;
}

static int32_t isp_k_nlm_block(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_nlm_info_v2 nlm_info;
	uint32_t *buff0 = NULL;
	uint32_t *buff1 = NULL;
	void *vst_addr = NULL;
	void *ivst_addr = NULL;

	ret = copy_from_user((void *)&nlm_info, param->property_param, sizeof(nlm_info));
	if (0 != ret) {
		printk("isp_k_nlm_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (isp_private->is_vst_ivst_update) {
		isp_private->is_vst_ivst_update = 0;

	buff0 = vzalloc(nlm_info.vst_len);
	if (NULL == buff0) {
		printk("isp_k_nlm_block:vst kmalloc error\n");
		return -1;
	}

	buff1 = vzalloc(nlm_info.ivst_len);
	if (NULL == buff1) {
		printk("isp_k_nlm_block:ivst kmalloc error\n");
		vfree(buff0);
		return -1;
	}

#ifdef CONFIG_64BIT
	vst_addr = (void*)(((unsigned long)nlm_info.vst_addr[1] << 32) | nlm_info.vst_addr[0]);
#else
	vst_addr = (void*)(nlm_info.vst_addr[0]);
#endif
	ret = copy_from_user((void *)buff0, vst_addr, nlm_info.vst_len);
	if (0 != ret) {
		printk("isp_k_nlm_block: vst copy error, ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		return -1;
	}

#ifdef CONFIG_64BIT
	ivst_addr = (void*)(((unsigned long)nlm_info.ivst_addr[1] << 32) | nlm_info.ivst_addr[0]);
#else
	ivst_addr = (void*)(nlm_info.ivst_addr[0]);
#endif
	ret = copy_from_user((void *)buff1, ivst_addr, nlm_info.ivst_len);
	if (0 != ret) {
		printk("isp_k_nlm_block: ivst copy error, ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		return -1;
	}

		memcpy((void *)ISP_VST_BUF0_CH0, buff0, ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)ISP_IVST_BUF0_CH0, buff1, ISP_VST_IVST_NUM * sizeof(uint32_t));

		vfree(buff0);
		vfree(buff1);
	}

	REG_MWR(ISP_NLM_PARA, BIT_1, nlm_info.imp_opt_bypass << 1);
	REG_MWR(ISP_NLM_PARA, BIT_2, nlm_info.flat_opt_bypass << 2);

	for (i = 0; i < 5; i++) {
		val = (nlm_info.thresh[i] & 0x3FFF) | ((nlm_info.cnt[i] & 0x1F) << 16)
			| ((nlm_info.strength[i] & 0x7F) << 24);
		REG_WR(ISP_NLM_FLAT_PARA_0 + i * 4, val);
	}

	REG_MWR(ISP_NLM_STERNGTH, 0x7F, nlm_info.streng_th);

	REG_MWR(ISP_NLM_STERNGTH, 0x7F80, nlm_info.texture_dec << 7);

	REG_WR(ISP_NLM_IS_FLAT, nlm_info.is_flat & 0xFF);

	REG_MWR(ISP_NLM_ADD_BACK, 0x7F, nlm_info.addback);


	for (i = 0; i < 24; i++) {
		val = (nlm_info.lut_w[i*3+0] & 0x3FF) | ((nlm_info.lut_w[i*3+1] & 0x3FF) << 10)
			| ((nlm_info.lut_w[i*3+2] & 0x3FF) << 20);
		REG_WR(ISP_NLM_LUT_W_0 + i * 4, val);
	}

	REG_MWR(ISP_NLM_PARA, BIT_0, nlm_info.bypass);
	REG_MWR(ISP_IVST_PARA, BIT_0, nlm_info.bypass);
	REG_MWR(ISP_VST_PARA, BIT_0, nlm_info.bypass);

	return ret;

}

static int32_t isp_k_ivst_block(struct isp_io_param *param)
{
	int32_t ret = 0;

	struct isp_dev_ivst_info_v1 ivst_info;

	ret = copy_from_user((void *)&ivst_info, param->property_param, sizeof(ivst_info));
	if (0 != ret) {
		printk("isp_k_ivst_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_IVST_PARA, BIT_0, ivst_info.bypass);

	return ret;
}

int32_t isp_k_cfg_nlm(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_nlm: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_nlm: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_VST_BLOCK:
		ret = isp_k_vst_block(param);
		break;
	case ISP_PRO_NLM_BLOCK:
		ret = isp_k_nlm_block(param, isp_private);
		break;
	case ISP_PRO_IVST_BLOCK:
		ret = isp_k_ivst_block(param);
		break;
	default:
		printk("isp_k_cfg_nlm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
