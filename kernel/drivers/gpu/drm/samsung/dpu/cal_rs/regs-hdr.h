/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Dongho Lee <dh205.lee@samsung.com>
 *
 * Register definition file for Samsung Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _REGS_HDR_H_
#define _REGS_HDR_H_

/*
 *-------------------------------------------------------------------
 * HDR(L0~L7) SFR list
 * L0,L2,L3,L4,L5,L6 : V1P2
 * L1,L7       	     : V2P2
 *
 * base address : 0x160E_0000
 * < Layer.offset >
 *  L0      L1      L2      L3      L4      L5      L6      L7
 *  0x0000  0x1000  0x2000  0x3000  0x4000  0x5000  0x6000  0x7000
 *
 *-------------------------------------------------------------------
 */

#define HDR_SHD_OFFSET			(0x0800)
#define IS_HDR_LAYER(_l)		(_l == 1 || _l == 7)

/*---------------------------------- V1P2 ------------------------------------*/
#define HDR_LSI_L_VERSION		(0x0000)
#define HDR_L_VERSION			(0x01020000)

#define HDR_LSI_L_COM_CTRL		(0x0004)
#define HDR_LSI_L_MOD_CTRL		(0x0008)

/*-----[ OETF : Inverse EOTF ]------------------------------------------------*/
#define HDR_OETF_L_POSX_LUT_REG_CNT	(17)
#define HDR_LSI_L_OETF_POSX(_n)		(0x000C + ((_n) * 0x4))

#define HDR_OETF_L_POSY_LUT_REG_CNT	(17)
#define HDR_LSI_L_OETF_POSY(_n)		(0x0050 + ((_n) * 0x4))

/*-----[ EOTF : Electro-Optical Transfer Function ]---------------------------*/
#define HDR_EOTF_L_POSX_LUT_REG_CNT	(33)
#define HDR_LSI_L_EOTF_POSX(_n)		(0x0094 + ((_n) * 0x4))

#define HDR_EOTF_L_POSY_LUT_REG_CNT	(65)
#define HDR_LSI_L_EOTF_POSY(_n)		(0x0198 + ((_n) * 0x4))

/*-----[ GM : Gamut Mapping ]-------------------------------------------------*/
#define HDR_GM_L_COEF_REG_CNT		(9)
#define HDR_LSI_L_GM_COEF(_n)		(0x039C + ((_n) * 0x4))

#define HDR_GM_L_OFFS_REG_CNT		(3)
#define HDR_LSI_L_GM_OFFS(_n)		(0x03C0 + ((_n) * 0x4))
/*----------------------------------------------------------------------------*/


/*---------------------------------- V1P3 ------------------------------------*/
#define HDR_LSI_H_VERSION		(0x0000)
#define HDR_H_VERSION			(0x01030000)

#define HDR_LSI_H_COM_CTRL		(0x0004)
#define HDR_LSI_H_MOD_CTRL		(0x0008)

/*-----[ OETF : Inverse EOTF ]------------------------------------------------*/
#define HDR_OETF_H_POSX_LUT_REG_CNT	(65)
#define HDR_LSI_H_OETF_POSX(_n)		(0x000C + ((_n) * 0x4))

#define HDR_OETF_H_POSY_LUT_REG_CNT	(33)
#define HDR_LSI_H_OETF_POSY(_n)		(0x0110 + ((_n) * 0x4))

/*-----[ EOTF : Electro-Optical Transfer Function ]---------------------------*/
#define HDR_EOTF_H_POSX_LUT_REG_CNT	(33)
#define HDR_LSI_H_EOTF_POSX(_n)		(0x0194 + ((_n) * 0x4))

#define HDR_EOTF_H_POSY_LUT_REG_CNT	(65)
#define HDR_LSI_H_EOTF_POSY(_n)		(0x0218 + ((_n) * 0x4))

#define HDR_LSI_H_EOTF_NORMAL_COEF	(0x031C)
#define HDR_LSI_H_EOTF_NORMAL_SHIFT	(0x0320)

/*-----[ GM : Gamut Mapping ]-------------------------------------------------*/
#define HDR_GM_H_COEF_REG_CNT		(9)
#define HDR_LSI_H_GM_COEF(_n)		(0x0324 + ((_n) * 0x4))

#define HDR_GM_H_OFFS_REG_CNT		(3)
#define HDR_LSI_H_GM_OFFS(_n)		(0x0348 + ((_n) * 0x4))

/*-----[ TM : Tone Mapping ]--------------------------------------------------*/
#define HDR_LSI_H_TM_COEF		(0x0354)
#define HDR_LSI_H_TM_RNGX		(0x0358)
#define HDR_LSI_H_TM_RNGY		(0x035C)

#define HDR_TM_H_POSX_LUT_REG_CNT	(33)
#define HDR_LSI_H_TM_POSX(_n)		(0x0360 + ((_n) * 0x4))

#define HDR_TM_H_POSY_LUT_REG_CNT	(33)
#define HDR_LSI_H_TM_POSY(_n)		(0x03E4 + ((_n) * 0x4))
/*----------------------------------------------------------------------------*/


/*---------------------------------- V2P2 ------------------------------------*/
#define HDR_HDR_CON			(0x0000)

/*-----[ EOTF : Electro-Optical Transfer Function ]---------------------------*/
#define HDR_EOTF_SCALER			(0x0004)

#define HDR_EOTF_LUT_TS_REG_CNT		(20)
#define HDR_EOTF_LUT_TS(_n)		(0x0010 + ((_n) * 0x4))

#define HDR_EOTF_LUT_VS_REG_CNT		(20)
#define HDR_EOTF_LUT_VS(_n)		(0x0060 + ((_n) * 0x4))

/*-----[ GM : Gamut Mapping ]-------------------------------------------------*/
#define HDR_GM_COEF_REG_CNT		(9)
#define HDR_GM_COEF(_n)			(0x00B0 + ((_n) * 0x4))

#define HDR_GM_OFF_REG_CNT		(3)
#define HDR_GM_OFF(_n)			(0x00D4 + ((_n) * 0x4))

/*-----[ TM : Tone Mapping ]--------------------------------------------------*/
#define HDR_TM_COEF_REG_CNT		(3)
#define HDR_TM_COEF(_n)			(0x00E0 + ((_n) * 0x4))

#define HDR_TM_YMIX_TF			(0x00EC)
#define HDR_TM_YMIX_VF			(0x00F0)
#define HDR_TM_YMIX_SLOPE		(0x00F4)
#define HDR_TM_YMIX_DV			(0x00F8)

#define HDR_TM_LUT_TS_REG_CNT		(24)
#define HDR_TM_LUT_TS(_n)		(0x00FC + ((_n) * 0x4))

#define HDR_TM_LUT_VS_REG_CNT		(24)
#define HDR_TM_LUT_VS(_n)		(0x015C + ((_n) * 0x4))

/*-----[ OETF : Inverse EOTF ]------------------------------------------------*/
#define HDR_OETF_LUT_TS_REG_CNT		(24)
#define HDR_OETF_LUT_TS(_n)		(0x01BC + ((_n) * 0x4))

#define HDR_OETF_LUT_VS_REG_CNT		(24)
#define HDR_OETF_LUT_VS(_n)		(0x021C + ((_n) * 0x4))

#endif /* __HDR_REGS_H__ */
