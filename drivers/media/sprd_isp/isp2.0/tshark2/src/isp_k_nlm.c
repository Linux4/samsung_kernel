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

#define ISP_RAW_NLM_MOUDLE_BUF0                0
#define ISP_RAW_NLM_MOUDLE_BUF1                1

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
	struct isp_dev_nlm_info_v1 nlm_info;
	uint32_t *buff0 = NULL;
	uint32_t *buff1 = NULL;
	uint32_t *buff2 = NULL;
	void *vst_addr = NULL;
	void *ivst_addr = NULL;
	void *nlm_addr = NULL;

	ret = copy_from_user((void *)&nlm_info, param->property_param, sizeof(nlm_info));
	if (0 != ret) {
		printk("isp_k_nlm_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

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

	buff2 = vzalloc(nlm_info.nlm_len);
	if (NULL == buff2) {
		printk("isp_k_nlm_block:nlm kmalloc error\n");
		vfree(buff0);
		vfree(buff1);
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
		vfree(buff2);
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
		vfree(buff2);
		return -1;
	}

#ifdef CONFIG_64BIT
		nlm_addr = (void*)(((unsigned long)nlm_info.nlm_addr[1] << 32) | nlm_info.nlm_addr[0]);
#else
		nlm_addr = (void*)(nlm_info.nlm_addr[0]);
#endif
	ret = copy_from_user((void *)buff2, nlm_addr, nlm_info.nlm_len);
	if (0 != ret) {
		printk("isp_k_nlm_block: nlm copy error, ret=0x%x\n", (uint32_t)ret);
		vfree(buff0);
		vfree(buff1);
		vfree(buff2);
		return -1;
	}

	REG_MWR(ISP_NLM_PARA, BIT_1, nlm_info.imp_opt_bypass << 1);
	REG_MWR(ISP_NLM_PARA, BIT_2, nlm_info.flat_opt_bypass << 2);
	REG_MWR(ISP_NLM_PARA, BIT_3, nlm_info.flat_thr_bypass << 3);
	REG_MWR(ISP_NLM_PARA, BIT_4, nlm_info.direction_mode_bypass << 4);

	for (i = 0; i < 5; i++) {
		val = (nlm_info.thresh[i] & 0x3FFF) | ((nlm_info.cnt[i] & 0x1F) << 16)
			| ((nlm_info.strength[i] & 0xFF) << 24);
		REG_WR(ISP_NLM_FLAT_PARA_0 + i * 4, val);
	}

	REG_MWR(ISP_NLM_STERNGTH, 0x7F, nlm_info.streng_th);

	REG_MWR(ISP_NLM_STERNGTH, 0xFF00, nlm_info.texture_dec << 8);

	//REG_WR(ISP_NLM_IS_FLAT, nlm_info.is_flat & 0xFF);

	REG_MWR(ISP_NLM_ADD_BACK, 0x7F, nlm_info.addback);

#if defined(CONFIG_ARCH_SCX30G3)
		REG_MWR(ISP_NLM_ADD_BACK_NEW0, 0x7F, nlm_info.addback);
		REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 8), (nlm_info.addback << 8));
		REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 16), (nlm_info.addback << 16));
		REG_MWR(ISP_NLM_ADD_BACK_NEW0, (0x7F << 24), (nlm_info.addback << 24));
		REG_MWR(ISP_NLM_ADD_BACK_NEW1, 0x7F, nlm_info.addback);
#endif

	REG_MWR(ISP_NLM_ADD_BACK, BIT_7, nlm_info.opt_mode << 7);

	REG_MWR(ISP_NLM_DIRECTION_0, BIT_17 | BIT_16, nlm_info.dist_mode << 16);

	val = ((nlm_info.w_shift[0] & 0x3) << 4) | ((nlm_info.w_shift[1] & 0x3) << 6) | ((nlm_info.w_shift[2] & 0x3) << 8);
	REG_MWR(ISP_NLM_DIRECTION_0, 0x3F0, val);

	REG_MWR(ISP_NLM_DIRECTION_0, 0x7, nlm_info.cnt_th);

	REG_MWR(ISP_NLM_DIRECTION_1, 0xFFFF0000, nlm_info.tdist_min_th << 16);

	REG_MWR(ISP_NLM_DIRECTION_1, 0xFFFF, nlm_info.diff_th);

	for (i = 0; i < 24; i++) {
		val = (nlm_info.lut_w[i*3+0] & 0x3FF) | ((nlm_info.lut_w[i*3+1] & 0x3FF) << 10)
			| ((nlm_info.lut_w[i*3+2] & 0x3FF) << 20);
		REG_WR(ISP_NLM_LUT_W_0 + i * 4, val);
	}

	if (isp_private->raw_nlm_buf_id) {
		isp_private->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF0;
		memcpy((void *)ISP_NLM_BUF0_CH0, buff2, ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)ISP_VST_BUF0_CH0, buff0, ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)ISP_IVST_BUF0_CH0, buff1, ISP_VST_IVST_NUM * sizeof(uint32_t));
	} else {
		isp_private->raw_nlm_buf_id = ISP_RAW_NLM_MOUDLE_BUF1;
		memcpy((void *)ISP_NLM_BUF1_CH0, buff2, ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)ISP_VST_BUF1_CH0, buff0, ISP_VST_IVST_NUM * sizeof(uint32_t));
		memcpy((void *)ISP_IVST_BUF1_CH0, buff1, ISP_VST_IVST_NUM * sizeof(uint32_t));
	}

	REG_MWR(ISP_NLM_PARA, BIT_31, isp_private->raw_nlm_buf_id << 31);
	REG_MWR(ISP_VST_PARA, BIT_1, isp_private->raw_nlm_buf_id << 1);
	REG_MWR(ISP_IVST_PARA, BIT_1, isp_private->raw_nlm_buf_id << 1);

	REG_MWR(ISP_NLM_PARA, BIT_0, nlm_info.bypass);
	REG_MWR(ISP_IVST_PARA, BIT_0, nlm_info.bypass);
	REG_MWR(ISP_VST_PARA, BIT_0, nlm_info.bypass);

	vfree(buff0);
	vfree(buff1);
	vfree(buff2);
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
		ret = isp_k_ivst_block(param);;
		break;
	default:
		printk("isp_k_cfg_nlm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
