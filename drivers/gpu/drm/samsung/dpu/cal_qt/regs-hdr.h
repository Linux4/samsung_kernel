/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
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
 * L0,L2,L4,L5,L6 : LSI_L_HDR
 * L1,L3,L7       : LSI_H_HDR - dynamic meta
 *
 * base address : 0x160E_0000
 * < Layer.offset >
 *  L0      L1      L2      L3      L4      L5      L6      L7
 *  0x0000  0x1000  0x2000  0x3000  0x4000  0x5000  0x6000  0x7000
 *
 * HDR_LSI_L_ : common for HDR High & Low
 *
 * (10b) - EOTF(65p) - (27b) - GM(coef:s2.20 / offs:s27.0) -(27b) -
 * DTM(x:27 / y:s10.20) -(27b) - OETF(65p) - (10b)
 *-------------------------------------------------------------------
 */

#define HDR_SHD_OFFSET			(0x0800)


#define HDR_LSI_VERSION			(0x0000)
#define HDR_VERSION			(0x01020000)
#define HDR_VERSION_GET(_v)		(((_v) >> 0) & 0xFFFFFFFF)

#define HDR_LSI_L_COM_CTRL		(0x0004)
#define COM_CTRL_ENABLE(_v)		((_v) << 0)
#define COM_CTRL_ENABLE_MASK		(1 << 0)

#define HDR_LSI_L_MOD_CTRL		(0x0008)
#define MOD_CTRL_PQ_EN(_v)		((_v) << 8)	/* H only */
#define MOD_CTRL_PQ_EN_MASK		(1 << 8)
#define MOD_CTRL_TEN(_v)		((_v) << 5)	/* H only */
#define MOD_CTRL_TEN_MASK		(1 << 5)
#define MOD_CTRL_GEN(_v)		((_v) << 2)
#define MOD_CTRL_GEN_MASK		(1 << 2)
#define MOD_CTRL_EEN(_v)		((_v) << 1)
#define MOD_CTRL_EEN_MASK		(1 << 1)
#define MOD_CTRL_OEN(_v)		((_v) << 0)
#define MOD_CTRL_OEN_MASK		(1 << 0)


/*-----[ OETF : Inverse EOTF ]-------------------------------------------------
 * 32-segment transfer function, 16-bit >> 10-bit
 *-----------------------------------------------------------------------------
 */

#define HDR_OETF_L_POSX_LUT_REG_CNT	(17)
#define HDR_OETF_H_POSX_LUT_REG_CNT	(65)

/*
 * L_OETF_POSX (0~16) : 0x000C ~ 0x004C
 * _n: [0, 16] / _x: [0, 32] -> 16b
 *
 * H_OETF_POSX (0~64) : 0x000C ~ 0x010C
 * _n: [0, 64] / _x: [0, 64] -> 27b
 */
#define HDR_LSI_L_OETF_POSX(_n)		(0x000C + ((_n) * 0x4))
#define OETF_POSX_H(_v)			((_v) << 16)
#define OETF_POSX_H_MASK		(0xFFFF << 16)
#define OETF_POSX_L(_v)			((_v) << 0)
#define OETF_POSX_L_MASK		(0xFFFF << 0)
#define OETF_POSX(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define OETF_POSX_MASK(_x)		(0xFFFF << (0 + (16 * ((_x) & 0x1))))

#define HDR_LSI_H_OETF_POSX(_n)		(0x000C + ((_n) * 0x4))
#define H_OETF_POSX(_v)			((_v) << 0)
#define H_OETF_POSX_MASK		(0x7FFFFFF << 0)


#define HDR_OETF_L_POSY_LUT_REG_CNT	(17)
#define HDR_OETF_H_POSY_LUT_REG_CNT	(33)

/*
 * L_OETF_POSY (0~16) : 0x0050 ~ 0x0090
 * _n: [0, 16] / _x: [0, 32] -> 10b
 *
 * H_OETF_POSY (0~32) : 0x0110 ~ 0x0190
 * _n: [0, 32] / _x: [0, 64] -> 10b
 */
#define HDR_LSI_L_OETF_POSY(_n)		(0x0050 + ((_n) * 0x4))
#define HDR_LSI_H_OETF_POSY(_n)		(0x0110 + ((_n) * 0x4))
#define OETF_POSY_H(_v)			((_v) << 16)
#define OETF_POSY_H_MASK		(0x3FF << 16)
#define OETF_POSY_L(_v)			((_v) << 0)
#define OETF_POSY_L_MASK		(0x3FF << 0)
#define OETF_POSY(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1)))
#define OETF_POSY_MASK(_x)		(0x3FF << (0 + (16 * ((_x) & 0x1)))


/*-----[ EOTF : Electro-Optical Transfer Function ]----------------------------
 * 64-segment transfer function, 10-bit >> 16-bit
 *-----------------------------------------------------------------------------
 */
#define HDR_EOTF_POSX_LUT_REG_CNT	(33)
/*
 * L_EOTF_POSX (0~32) : 0x0094 ~ 0x0114
 * _n: [0, 32] / _x: [0, 64] -> 10b
 *
 * H_EOTF_POSX (0~32) : 0x0194 ~ 0x0214
 * _n: [0, 32] / _x: [0, 64] -> 10b
 */
#define HDR_LSI_L_EOTF_POSX(_n)		(0x0094 + ((_n) * 0x4))
#define HDR_LSI_H_EOTF_POSX(_n)		(0x0194 + ((_n) * 0x4))
#define EOTF_POSX_H(_v)			((_v) << 16)
#define EOTF_POSX_H_MASK		(0x3FF << 16)
#define EOTF_POSX_L(_v)			((_v) << 0)
#define EOTF_POSX_L_MASK		(0x3FF << 0)
#define EOTF_POSX(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define EOTF_POSX_MASK(_x)		(0x3FF << (0 + (16 * ((_x) & 0x1))))

#define HDR_EOTF_POSY_LUT_REG_CNT	(65)
/*
 * L_EOTF_POSY (0~64) : 0x0198 ~ 0x0298
 * _n: [0, 64] / _x: [0, 64] -> 16b
 *
 * H_EOTF_POSY (0~64) : 0x0218 ~ 0x0314
 * _n: [0, 64] / _x: [0, 64] -> 16b
 */
#define HDR_LSI_L_EOTF_POSY(_n)		(0x0198 + ((_n) * 0x4))
#define HDR_LSI_H_EOTF_POSY(_n)		(0x0218 + ((_n) * 0x4))
#define EOTF_POSY_L(_v)			((_v) << 0)
#define EOTF_POSY_L_MASK		(0xFFFF << 0)

#define HDR_LSI_H_EOTF_NORMAL_COEF	(0x031C)
#define H_EOTF_NORMAL_COEF(_v)		((_v) << 0)
#define H_EOTF_NORMAL_COEF_MASK		(0xFFFF << 0)

#define HDR_LSI_H_EOTF_NORMAL_SHIFT	(0x0320)
#define H_EOTF_NORMAL_SHIFT(_v)		((_v) << 0)
#define H_EOTF_NORMAL_SHIFT_MASK	(0xFFFF << 0)


/*-----[ GM : Gamut Mapping ]--------------------------------------------------
 * 3x3 matrix, S2.16
 * S16.0 offset to the result of matrix calculation
 *-----------------------------------------------------------------------------
 */
#define HDR_GM_COEF_REG_CNT		(9)
/*
 * L_GM_COEFF (0~8) : 0x039C ~ 0x03BC (19b)
 * H_GM_COEFF (0~8) : 0x0324 ~ 0x0344 (23b)
 * |Rconv| = |C00 C01 C02| |Rin| + |Roffset0|
 * |Gconv| = |C10 C11 C12| |Gin| + |Goffset1|
 * |Bconv| = |C20 C21 C22| |Bin| + |Boffset2|
 */
#define HDR_LSI_L_GM_COEF(_n)		(0x039C + ((_n) * 0x4))
#define HDR_LSI_L_GM_COEF_0		(0x039C)
#define HDR_LSI_L_GM_COEF_1		(0x03A0)
#define HDR_LSI_L_GM_COEF_2		(0x03A4)
#define HDR_LSI_L_GM_COEF_3		(0x03A8)
#define HDR_LSI_L_GM_COEF_4		(0x03AC)
#define HDR_LSI_L_GM_COEF_5		(0x03B0)
#define HDR_LSI_L_GM_COEF_6		(0x03B4)
#define HDR_LSI_L_GM_COEF_7		(0x03B8)
#define HDR_LSI_L_GM_COEF_8		(0x03BC)
#define GM_COEF(_v)			((_v) << 0)
#define GM_COEF_MASK			(0x7FFFF << 0)

#define HDR_LSI_H_GM_COEF(_n)		(0x0324 + ((_n) * 0x4))
#define H_GM_COEF(_v)			((_v) << 0)
#define H_GM_COEF_MASK			(0x7FFFFF << 0)


#define HDR_GM_OFFS_REG_CNT		(3)
/*
 * L_GM_OFFS (0~2) : 0x03C0 ~ 0x03C8 (17b)
 *
 * H_GM_OFFS (0~2) : 0x0348 ~ 0x0350 (28b)
 */
#define HDR_LSI_L_GM_OFFS(_n)		(0x03C0 + ((_n) * 0x4))
#define HDR_LSI_L_GM_OFFS_0		(0x03C0)
#define HDR_LSI_L_GM_OFFS_1		(0x03C4)
#define HDR_LSI_L_GM_OFFS_2		(0x03C8)
#define GM_OFFS(_v)			((_v) << 0)
#define GM_OFFS_MASK			(0x1FFFF << 0)

#define HDR_LSI_H_GM_OFFS(_n)		(0x0348 + ((_n) * 0x4))
#define H_GM_OFFS(_v)			((_v) << 0)
#define H_GM_OFFS_MASK			(0xFFFFFFF << 0)




/*-----[ TM : Tone Mapping ]---------------------------------------------------
 * Available only in high-end IP
 * 32-segment transfer function, 16-bit >> S10.16
 * Index calculation using MaxRGB and Y (luminance) in parallel
 * Supports for both fixed-ratio mixing and adaptive mixing
 *-----------------------------------------------------------------------------
 */

#define HDR_LSI_H_TM_COEF		(0x0354)
#define TM_COEF_COEFB(_v)		((_v) << 20)
#define TM_COEF_COEFB_MASK		(0x3FF << 20)
#define TM_COEF_COEFG(_v)		((_v) << 10)
#define TM_COEF_COEFG_MASK		(0x3FF << 10)
#define TM_COEF_COEFR(_v)		((_v) << 0)
#define TM_COEF_COEFR_MASK		(0x3FF << 0)

#define HDR_LSI_H_TM_RNGX		(0x0358)
#define TM_RNGX_MAXX(_v)		((_v) << 16)
#define TM_RNGX_MAXX_MASK		(0xFFFF << 16)
#define TM_RNGX_MINX(_v)		((_v) << 0)
#define TM_RNGX_MINX_MASK		(0xFFFF << 0)

#define HDR_LSI_H_TM_RNGY		(0x035C)
#define TM_RNGY_MAXY(_v)		((_v) << 9)
#define TM_RNGY_MAXY_MASK		(0x1FF << 9)
#define TM_RNGY_MINY(_v)		((_v) << 0)
#define TM_RNGY_MINY_MASK		(0x1FF << 0)


#define HDR_TM_POSX_LUT_REG_CNT		(33)
/*
 * TM_POSX (0~32) : 0x0360 ~ 0x03E0
 * _n: [0, 32] / _x: [0, 32] -> 27b
 */
#define HDR_LSI_H_TM_POSX(_n)		(0x0360 + ((_n) * 0x4))
#define TM_POSX(_v)			((_v) << 0)
#define TM_POSX_MASK			(0x7FFFFFF << 0)


#define HDR_TM_POSY_LUT_REG_CNT		(33)
/*
 * TM_POSY (0~32) : 0x03E4 ~ 0x0464
 * _n: [0, 32] / _x: [0, 32] -> 31b
 */
#define HDR_LSI_H_TM_POSY(_n)		(0x03E4 + ((_n) * 0x4))
#define TM_POSY(_v)			((_v) << 0)
#define TM_POSY_MASK			(0x7FFFFFFF << 0)

#endif /* __HDR_REGS_H__ */
