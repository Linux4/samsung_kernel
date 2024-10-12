/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DQE CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DQE_CAL_DUMMY_H__
#define __SAMSUNG_DQE_CAL_DUMMY_H__

/*----------------------[ATC]-------------------------------------------------*/
#ifndef DQE_ATC_PIXMAP_EN
#define DQE_ATC_PIXMAP_EN(_v)		(0)
#endif

#ifndef ATC_ONE_DITHER
#define ATC_ONE_DITHER(_v)		(0)
#endif

#ifndef ATC_LA_W
#define ATC_LA_W(_v)			(0)
#endif

#ifndef ATC_LA_W_ON
#define ATC_LA_W_ON(_v)			(0)
#endif

/*----------------------[CGC_DITHER]-----------------------------------------*/
#ifndef CGC_DITHER_BIT
#define CGC_DITHER_BIT(_v)		(0)
#endif

/*----------------------[SCL]-----------------------------------------*/
#ifndef SCL_COEF
#define SCL_COEF(_v)			(0)
#endif

#ifndef SCL_COEF_MASK
#define SCL_COEF_MASK			(0)
#endif

#ifndef DQE_SCL_COEF_SET
#define DQE_SCL_COEF_SET		(0)
#endif

#ifndef DQE_SCL_VCOEF_MAX
#define DQE_SCL_VCOEF_MAX		(0)
#endif

#ifndef DQE_SCL_HCOEF_MAX
#define DQE_SCL_HCOEF_MAX		(0)
#endif

#ifndef DQE_SCL_COEF_MAX
#define DQE_SCL_COEF_MAX		(0)
#endif

#ifndef DQE_SCL_COEF_CNT
#define DQE_SCL_COEF_CNT		(0)
#endif

#ifndef DQE_SCL_REG_MAX
#define DQE_SCL_REG_MAX			(0)
#endif

#ifndef DQE_SCL_LUT_MAX
#define DQE_SCL_LUT_MAX			(0)
#endif

/*----------------------[DE]-----------------------------------------*/
#ifndef DE_BRATIO_SMOOTH
#define DE_BRATIO_SMOOTH(_v)		(0)
#endif

#ifndef DE_FILTER_TYPE
#define DE_FILTER_TYPE(_v)		(0)
#endif

#ifndef DE_SMOOTH_EN
#define DE_SMOOTH_EN(_v)		(0)
#endif

#ifndef DE_ROI_IN
#define DE_ROI_IN(_v)			(0)
#endif

#ifndef DE_ROI_EN
#define DE_ROI_EN(_v)			(0)
#endif

#ifndef DE_EN
#define DE_EN(_v)			(0)
#endif

#ifndef DE_ROI_START_Y
#define DE_ROI_START_Y(_v)		(0)
#endif

#ifndef DE_ROI_START_X
#define DE_ROI_START_X(_v)		(0)
#endif

#ifndef DE_ROI_END_Y
#define DE_ROI_END_Y(_v)		(0)
#endif

#ifndef DE_ROI_END_X
#define DE_ROI_END_X(_v)		(0)
#endif

#ifndef DE_EDGE_BALANCE_TH
#define DE_EDGE_BALANCE_TH(_v)		(0)
#endif

#ifndef DE_FLAT_EDGE_TH
#define DE_FLAT_EDGE_TH(_v)		(0)
#endif

#ifndef DE_MINUS_CLIP
#define DE_MINUS_CLIP(_v)		(0)
#endif

#ifndef DE_PLUS_CLIP
#define DE_PLUS_CLIP(_v)		(0)
#endif

#ifndef DF_MAX_LUMINANCE
#define DF_MAX_LUMINANCE(_v)		(0)
#endif

#ifndef DE_FLAT_DEGAIN_SHIFT
#define DE_FLAT_DEGAIN_SHIFT(_v)	(0)
#endif

#ifndef DE_FLAT_DEGAIN_FLAG
#define DE_FLAT_DEGAIN_FLAG(_v)		(0)
#endif

#ifndef DE_LUMA_DEGAIN_TH
#define DE_LUMA_DEGAIN_TH(_v)		(0)
#endif

#ifndef DE_GAIN
#define DE_GAIN(_v)			(0)
#endif

#ifndef DQE_DE_REG_MAX
#define DQE_DE_REG_MAX			(0)
#endif

#ifndef DQE_DE_LUT_MAX
#define DQE_DE_LUT_MAX			(0)
#endif

#endif /* __SAMSUNG_DQE_CAL_DUMMY_H__ */
