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
#include "isp_block.h"

typedef int32_t (*isp_cfg_fun_ptr)(struct isp_io_param *isp_param);

struct isp_cfg_fun {
	uint32_t sub_block;
	isp_cfg_fun_ptr cfg_fun;
};

static struct isp_cfg_fun isp_cfg_fun_tab[] = {
	{ISP_BLOCK_PRE_GLB_GAIN,           isp_k_cfg_pgg},
	{ISP_BLOCK_BLC,                    isp_k_cfg_blc},
	{ISP_BLOCK_RGB_GAIN,               isp_k_cfg_rgb_gain},
	{ISP_BLOCK_PRE_WAVELET,            isp_k_cfg_pwd},
	{ISP_BLOCK_NLC,                    isp_k_cfg_nlc},
	/*{ISP_BLOCK_2D_LSC,                    isp_k_cfg_2d_lsc},*/
	{ISP_BLOCK_1D_LSC,                 isp_k_cfg_1d_lsc},
	/*{ISP_BLOCK_BINNING4AWB,                isp_k_cfg_binning},*/
	/*{ISP_BLOCK_AWB,                    isp_k_cfg_awb},*/
	/*{ISP_BLOCK_RAW_AEM,                isp_k_cfg_raw_aem},*/
	{ISP_BLOCK_BPC,                 isp_k_cfg_bpc},
	{ISP_BLOCK_BDN,               isp_k_cfg_bdn},
	{ISP_BLOCK_GRGB,                   isp_k_cfg_grgb},
	{ISP_BLOCK_RGB_GAIN2,              isp_k_cfg_rgb_gain2},
	/*{ISP_BLOCK_NLM,                    isp_k_cfg_nlm},*/
	{ISP_BLOCK_CFA,                 isp_k_cfg_cfa},
	{ISP_BLOCK_CMC,                    isp_k_cfg_cmc10},
	{ISP_BLOCK_HDR,                    isp_k_cfg_hdr},//pike add module,this module is same to sharkl's hdr module
	/*{ISP_BLOCK_GAMMA,                  isp_k_cfg_gamma},*/
	{ISP_BLOCK_CMC8,                   isp_k_cfg_cmc8},
	/*{ISP_BLOCK_CT,                     isp_k_cfg_ct},*/
	{ISP_BLOCK_CCE,                    isp_k_cfg_cce},
	{ISP_BLOCK_HSV,                    isp_k_cfg_hsv},
	{ISP_BLOCK_CSC,                    isp_k_cfg_csc},
	{ISP_BLOCK_PRE_CDN_RGB,            isp_k_cfg_pre_cdn_rgb},
	{ISP_BLOCK_POSTERIZE,              isp_k_cfg_posterize},
	{ISP_BLOCK_AFM_V1,                 isp_k_cfg_rgb_afm},
	{ISP_BLOCK_RGB2Y,                  isp_k_cfg_rgb2y},//pike add module
	{ISP_BLOCK_YIQ_AEM,                isp_k_cfg_yiq_aem},
	/*{ISP_BLOCK_ANTI_FLICKER,           isp_k_cfg_anti_flicker},*/
	{ISP_BLOCK_YIQ_AFM,                isp_k_cfg_yiq_afm},
	{ISP_BLOCK_YUV_PRECDN,             isp_k_cfg_yuv_precdn},
	{ISP_BLOCK_PRE_FILTER,             isp_k_cfg_prefilter},
	{ISP_BLOCK_UV_PREFILTER,	       isp_k_cfg_uv_prefilter},//pike add module
	{ISP_BLOCK_BRIGHTNESS,             isp_k_cfg_brightness},
	{ISP_BLOCK_CONTRAST,               isp_k_cfg_contrast},
	{ISP_BLOCK_HIST,                   isp_k_cfg_hist},
	{ISP_BLOCK_HIST2,                  isp_k_cfg_hist2},
	{ISP_BLOCK_ACA,                    isp_k_cfg_aca},
	{ISP_BLOCK_YUV_CDN,                isp_k_cfg_yuv_cdn},
	{ISP_BLOCK_EDGE,                   isp_k_cfg_edge},
	{ISP_BLOCK_CSS,                    isp_k_cfg_css},
	{ISP_BLOCK_CSA,                    isp_k_cfg_csa},
	{ISP_BLOCK_HUE,                    isp_k_cfg_hue},
	{ISP_BLOCK_POST_CDN,               isp_k_cfg_post_cdn},
	{ISP_BLOCK_EMBOSS,                 isp_k_cfg_emboss},
	/*{ISP_BLOCK_YGAMMA,                 isp_k_cfg_ygamma},*/
	{ISP_BLOCK_YDELAY,                 isp_k_cfg_ydelay},
	{ISP_BLOCK_YUV_NLM,                isp_k_cfg_yuv_nlm},//pike add module
	//{ISP_BLOCK_IIRCNR,                 isp_k_cfg_iircnr},//pike has no this module
	{ISP_BLOCK_FETCH,                  isp_k_cfg_fetch},
	{ISP_BLOCK_STORE,                  isp_k_cfg_store},
	{ISP_BLOCK_DISPATCH,               isp_k_cfg_dispatch},
	{ISP_BLOCK_ARBITER_V1,             isp_k_cfg_arbiter},
	//{ISP_BLOCK_RAW_SIZER,              isp_k_cfg_raw_sizer},//pike has no this module
	{ISP_BLOCK_COMMON_V1,              isp_k_cfg_common},
};

int32_t isp_cfg_param(void  *param, struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t i = 0, cnt = 0;
	isp_cfg_fun_ptr cfg_fun_ptr = NULL;
	struct isp_io_param isp_param = {0, 0, 0, NULL};

	if (!param) {
		printk("isp_cfg_param: param is null error.\n");
		return -1;
	}

	ret = copy_from_user((void *)&isp_param, (void *)param, sizeof(isp_param));
	if ( 0 != ret) {
		printk("isp_cfg_param: copy error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (ISP_BLOCK_ANTI_FLICKER == isp_param.sub_block) {
		ret = isp_k_cfg_anti_flicker(&isp_param, isp_private);
	} else if (ISP_BLOCK_2D_LSC == isp_param.sub_block) {
		ret = isp_k_cfg_2d_lsc(&isp_param, isp_private);
	} else if (ISP_BLOCK_RAW_AEM == isp_param.sub_block) {
		ret = isp_k_cfg_raw_aem(&isp_param, isp_private);
	} else if (ISP_BLOCK_GAMMA == isp_param.sub_block) {
		ret = isp_k_cfg_gamma(&isp_param, isp_private);
	} else if (ISP_BLOCK_CT == isp_param.sub_block) {
		ret = isp_k_cfg_ct(&isp_param, isp_private);
	} else if (ISP_BLOCK_YGAMMA == isp_param.sub_block) {
		ret = isp_k_cfg_ygamma(&isp_param, isp_private);
	} else if (ISP_BLOCK_AWB == isp_param.sub_block) {
		ret = isp_k_cfg_awb(&isp_param, isp_private);
	} else if(ISP_BLOCK_BINNING4AWB == isp_param.sub_block){
		ret = isp_k_cfg_binning(&isp_param, isp_private);
	} else if(ISP_BLOCK_NLM == isp_param.sub_block){
		ret = isp_k_cfg_nlm(&isp_param, isp_private);
	} else {
		cnt = sizeof(isp_cfg_fun_tab) / sizeof(isp_cfg_fun_tab[0]);
		for (i = 0; i < cnt; i++) {
			if (isp_param.sub_block == isp_cfg_fun_tab[i].sub_block) {
				cfg_fun_ptr = isp_cfg_fun_tab[i].cfg_fun;
				break;
			}
		}

		if (NULL != cfg_fun_ptr)
			ret = cfg_fun_ptr(&isp_param);
	}

	return ret;
}
