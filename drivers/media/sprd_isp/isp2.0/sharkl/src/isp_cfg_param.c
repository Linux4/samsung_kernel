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
	{ISP_BLOCK_FETCH,          isp_k_cfg_fetch},
	{ISP_BLOCK_BLC,            isp_k_cfg_blc},
	/*{ISP_BLOCK_2D_LSC,            NULL},*/
	/*{ISP_BLOCK_AWB,            isp_k_cfg_awb},*/
	{ISP_BLOCK_BPC,            isp_k_cfg_bpc},
	{ISP_BLOCK_BDN,       isp_k_cfg_bdn},
	{ISP_BLOCK_GRGB,           isp_k_cfg_grgb},
	{ISP_BLOCK_CFA,            isp_k_cfg_cfa},
	{ISP_BLOCK_CMC,            isp_k_cfg_cmc},
	{ISP_BLOCK_GAMMA,          isp_k_cfg_gamma},
	{ISP_BLOCK_CCE,            isp_k_cfg_cce},
	{ISP_BLOCK_PRE_FILTER,     isp_k_cfg_prefilter},
	{ISP_BLOCK_BRIGHTNESS,     isp_k_cfg_brightness},
	{ISP_BLOCK_CONTRAST,       isp_k_cfg_contrast},
	{ISP_BLOCK_HIST,           isp_k_cfg_hist},
	{ISP_BLOCK_ACA,            isp_k_cfg_aca},
	{ISP_BLOCK_AFM,            isp_k_cfg_afm},
	{ISP_BLOCK_EDGE,           isp_k_cfg_edge},
	{ISP_BLOCK_EMBOSS,         isp_k_cfg_emboss},
	{ISP_BLOCK_FCS,            isp_k_cfg_fcs},
	{ISP_BLOCK_CSS,            isp_k_cfg_css},
	{ISP_BLOCK_CSA,            isp_k_cfg_csa},
	{ISP_BLOCK_STORE,          isp_k_cfg_store},
	{ISP_BLOCK_FEEDER,         isp_k_cfg_feeder},
	/*{ISP_BLOCK_ARBITER,        NULL},*/
	{ISP_BLOCK_HDR,            isp_k_cfg_hdr},
	{ISP_BLOCK_NLC,            isp_k_cfg_nlc},
	{ISP_BLOCK_NAWBM,          isp_k_cfg_nawbm},
	{ISP_BLOCK_PRE_WAVELET,    isp_k_cfg_pre_wavelet},
	{ISP_BLOCK_BINNING4AWB,    isp_k_cfg_binning4awb},
	{ISP_BLOCK_PRE_GLB_GAIN,   isp_k_cfg_pgg},
	{ISP_BLOCK_COMMON,         isp_k_cfg_comm},
	{ISP_BLOCK_GLB_GAIN,       isp_k_cfg_glb_gain},
	{ISP_BLOCK_RGB_GAIN,       isp_k_cfg_rgb_gain},
	{ISP_BLOCK_YIQ,            isp_k_cfg_yiq},
	{ISP_BLOCK_HUE,            isp_k_cfg_hue},
	{ISP_BLOCK_NBPC,           isp_k_cfg_nbpc},
};

int32_t isp_cfg_param(void *param, struct isp_k_private *isp_private)
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

	if (ISP_BLOCK_2D_LSC == isp_param.sub_block) {
		ret = isp_k_cfg_2d_lsc(&isp_param, isp_private);
	} else if (ISP_BLOCK_AWB == isp_param.sub_block) {
		ret = isp_k_cfg_awb(&isp_param, isp_private);
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
