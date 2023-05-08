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

#include "isp_drv.h"

int32_t isp_k_cfg_pgg(struct isp_io_param *param);
int32_t isp_k_cfg_blc(struct isp_io_param *param);
int32_t isp_k_cfg_rgb_gain(struct isp_io_param *param);
int32_t isp_k_cfg_pwd(struct isp_io_param *param);
int32_t isp_k_cfg_nlc(struct isp_io_param *param);
int32_t isp_k_cfg_2d_lsc(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_1d_lsc(struct isp_io_param *param);
int32_t isp_k_cfg_binning(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_awb(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_raw_aem(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_bpc(struct isp_io_param *param);
int32_t isp_k_cfg_bdn(struct isp_io_param *param);
int32_t isp_k_cfg_grgb(struct isp_io_param *param);
int32_t isp_k_cfg_rgb_gain2(struct isp_io_param *param);
int32_t isp_k_cfg_nlm(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_cfa(struct isp_io_param *param);
int32_t isp_k_cfg_cmc10(struct isp_io_param *param);
int32_t isp_k_cfg_hdr(struct isp_io_param *param);
int32_t isp_k_cfg_gamma(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_cmc8(struct isp_io_param *param);
int32_t isp_k_cfg_ct(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_hsv(struct isp_io_param *param);
int32_t isp_k_cfg_rgb_afm(struct isp_io_param *param);
int32_t isp_k_cfg_cce(struct isp_io_param *param);
int32_t isp_k_cfg_pre_cdn_rgb(struct isp_io_param *param);
int32_t isp_k_cfg_yuv_precdn(struct isp_io_param *param);
int32_t isp_k_cfg_posterize(struct isp_io_param *param);
int32_t isp_k_cfg_csc(struct isp_io_param *param);
int32_t isp_k_cfg_yiq_aem(struct isp_io_param *param);
int32_t isp_k_cfg_anti_flicker(struct isp_io_param *param, struct isp_k_private *isp_private);
int32_t isp_k_cfg_yiq_afm(struct isp_io_param *param);
int32_t isp_k_cfg_prefilter(struct isp_io_param *param);
int32_t isp_k_cfg_brightness(struct isp_io_param *param);
int32_t isp_k_cfg_contrast(struct isp_io_param *param);
int32_t isp_k_cfg_hist(struct isp_io_param *param);
int32_t isp_k_cfg_hist2(struct isp_io_param *param);
int32_t isp_k_cfg_aca(struct isp_io_param *param);
int32_t isp_k_cfg_yuv_cdn(struct isp_io_param *param);
int32_t isp_k_cfg_edge(struct isp_io_param *param);
int32_t isp_k_cfg_css(struct isp_io_param *param);
int32_t isp_k_cfg_csa(struct isp_io_param *param);
int32_t isp_k_cfg_post_cdn(struct isp_io_param *param);
int32_t isp_k_cfg_hue(struct isp_io_param *param);
int32_t isp_k_cfg_emboss(struct isp_io_param *param);
int32_t isp_k_cfg_ydelay(struct isp_io_param *param);
int32_t isp_k_cfg_ygamma(struct isp_io_param *param, struct isp_k_private *isp_private);;
int32_t isp_k_cfg_iircnr(struct isp_io_param *param);
int32_t isp_k_cfg_fetch(struct isp_io_param *param);
int32_t isp_k_cfg_store(struct isp_io_param *param);
int32_t isp_k_cfg_dispatch(struct isp_io_param *param);
int32_t isp_k_cfg_arbiter(struct isp_io_param *param);
int32_t isp_k_cfg_common(struct isp_io_param *param);
int32_t isp_k_cfg_raw_sizer(struct isp_io_param *param);
//pike add
int32_t isp_k_cfg_yuv_nlm(struct isp_io_param *param);
int32_t isp_k_cfg_uv_prefilter(struct isp_io_param *param);
int32_t isp_k_cfg_rgb2y(struct isp_io_param *param);