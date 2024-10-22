/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */
#ifndef __MTK_MML_RROT_GOLDEN_H__
#define __MTK_MML_RROT_GOLDEN_H__

#include "mtk-mml.h"

#define DMABUF_CON_CNT		4

#define GOLDEN_PIXEL_FHD	(1920 * 1088)
#define GOLDEN_PIXEL_2K		(2560 * 1440)
#define GOLDEN_PIXEL_4K		(3840 * 2176)

struct golden_setting {
	u32 pixel;
	struct threshold {
		u32 preultra;
		u32 ultra;
		u32 urgent;
	} plane[DMABUF_CON_CNT];

	/* [0]: Rotate 0/180
	 * [1]: Rotate 90/270
	 */
	struct prefetch_rotate {
		u32 prefetch_ctrl1;
		u32 prefetch_ctrl2;
	} pfrot[2];
};

struct rrot_golden {
	/* golden settings by pixel, must order low to high (fhd->2K->4k) */
	const struct golden_setting *settings;
	u8 cnt;
};

/* begin of mt6989 golden settings */

/* !!Following code generate by golden parser (goldenparser.py)!! */

/* pre-ultra, ultra and urgent settings */
#define MT6989_3PLANE_4K_URGENT_0		(173 << 16 | 138)
#define MT6989_3PLANE_4K_ULTRA_0		(242 << 16 | 207)
#define MT6989_3PLANE_4K_PREULTRA_0		(311 << 16 | 276)
#define MT6989_3PLANE_4K_URGENT_1		(43 << 16 | 35)
#define MT6989_3PLANE_4K_ULTRA_1		(60 << 16 | 52)
#define MT6989_3PLANE_4K_PREULTRA_1		(78 << 16 | 69)
#define MT6989_3PLANE_4K_URGENT_2		(43 << 16 | 35)
#define MT6989_3PLANE_4K_ULTRA_2		(60 << 16 | 52)
#define MT6989_3PLANE_4K_PREULTRA_2		(78 << 16 | 69)
#define MT6989_3PLANE_4K_URGENT_3		0
#define MT6989_3PLANE_4K_ULTRA_3		0
#define MT6989_3PLANE_4K_PREULTRA_3		0
#define MT6989_2PLANE_4K_URGENT_0		(173 << 16 | 138)
#define MT6989_2PLANE_4K_ULTRA_0		(242 << 16 | 207)
#define MT6989_2PLANE_4K_PREULTRA_0		(311 << 16 | 276)
#define MT6989_2PLANE_4K_URGENT_1		(86 << 16 | 69)
#define MT6989_2PLANE_4K_ULTRA_1		(121 << 16 | 104)
#define MT6989_2PLANE_4K_PREULTRA_1		(155 << 16 | 138)
#define MT6989_2PLANE_4K_URGENT_2		0
#define MT6989_2PLANE_4K_ULTRA_2		0
#define MT6989_2PLANE_4K_PREULTRA_2		0
#define MT6989_2PLANE_4K_URGENT_3		0
#define MT6989_2PLANE_4K_ULTRA_3		0
#define MT6989_2PLANE_4K_PREULTRA_3		0
#define MT6989_RGB_4K_URGENT_0			(518 << 16 | 414)
#define MT6989_RGB_4K_ULTRA_0			(725 << 16 | 621)
#define MT6989_RGB_4K_PREULTRA_0		(932 << 16 | 829)
#define MT6989_RGB_4K_URGENT_1			0
#define MT6989_RGB_4K_ULTRA_1			0
#define MT6989_RGB_4K_PREULTRA_1		0
#define MT6989_RGB_4K_URGENT_2			0
#define MT6989_RGB_4K_ULTRA_2			0
#define MT6989_RGB_4K_PREULTRA_2		0
#define MT6989_RGB_4K_URGENT_3			0
#define MT6989_RGB_4K_ULTRA_3			0
#define MT6989_RGB_4K_PREULTRA_3		0
#define MT6989_ARGB_4K_URGENT_0			(691 << 16 | 552)
#define MT6989_ARGB_4K_ULTRA_0			(967 << 16 | 829)
#define MT6989_ARGB_4K_PREULTRA_0		(1243 << 16 | 1105)
#define MT6989_ARGB_4K_URGENT_1			0
#define MT6989_ARGB_4K_ULTRA_1			0
#define MT6989_ARGB_4K_PREULTRA_1		0
#define MT6989_ARGB_4K_URGENT_2			0
#define MT6989_ARGB_4K_ULTRA_2			0
#define MT6989_ARGB_4K_PREULTRA_2		0
#define MT6989_ARGB_4K_URGENT_3			0
#define MT6989_ARGB_4K_ULTRA_3			0
#define MT6989_ARGB_4K_PREULTRA_3		0
#define MT6989_AFBC_4K_URGENT_0			(691 << 16 | 552)
#define MT6989_AFBC_4K_ULTRA_0			(967 << 16 | 829)
#define MT6989_AFBC_4K_PREULTRA_0		(1243 << 16 | 1105)
#define MT6989_AFBC_4K_URGENT_1			0
#define MT6989_AFBC_4K_ULTRA_1			0
#define MT6989_AFBC_4K_PREULTRA_1		0
#define MT6989_AFBC_4K_URGENT_2			(5 << 16 | 4)
#define MT6989_AFBC_4K_ULTRA_2			(8 << 16 | 6)
#define MT6989_AFBC_4K_PREULTRA_2		(10 << 16 | 9)
#define MT6989_AFBC_4K_URGENT_3			0
#define MT6989_AFBC_4K_ULTRA_3			0
#define MT6989_AFBC_4K_PREULTRA_3		0
#define MT6989_HYFBC_4K_URGENT_0		(173 << 16 | 138)
#define MT6989_HYFBC_4K_ULTRA_0			(242 << 16 | 207)
#define MT6989_HYFBC_4K_PREULTRA_0		(311 << 16 | 276)
#define MT6989_HYFBC_4K_URGENT_1		(86 << 16 | 69)
#define MT6989_HYFBC_4K_ULTRA_1			(121 << 16 | 104)
#define MT6989_HYFBC_4K_PREULTRA_1		(155 << 16 | 138)
#define MT6989_HYFBC_4K_URGENT_2		(3 << 16 | 2)
#define MT6989_HYFBC_4K_ULTRA_2			(4 << 16 | 3)
#define MT6989_HYFBC_4K_PREULTRA_2		(4 << 16 | 3)
#define MT6989_HYFBC_4K_URGENT_3		(1 << 16)
#define MT6989_HYFBC_4K_ULTRA_3			(2 << 16 | 1)
#define MT6989_HYFBC_4K_PREULTRA_3		(3 << 16 | 2)
#define MT6989_3PLANE_2K_URGENT_0		(77 << 16 | 61)
#define MT6989_3PLANE_2K_ULTRA_0		(107 << 16 | 92)
#define MT6989_3PLANE_2K_PREULTRA_0		(138 << 16 | 123)
#define MT6989_3PLANE_2K_URGENT_1		(19 << 16 | 15)
#define MT6989_3PLANE_2K_ULTRA_1		(27 << 16 | 23)
#define MT6989_3PLANE_2K_PREULTRA_1		(35 << 16 | 31)
#define MT6989_3PLANE_2K_URGENT_2		(19 << 16 | 15)
#define MT6989_3PLANE_2K_ULTRA_2		(27 << 16 | 23)
#define MT6989_3PLANE_2K_PREULTRA_2		(35 << 16 | 31)
#define MT6989_3PLANE_2K_URGENT_3		0
#define MT6989_3PLANE_2K_ULTRA_3		0
#define MT6989_3PLANE_2K_PREULTRA_3		0
#define MT6989_2PLANE_2K_URGENT_0		(77 << 16 | 61)
#define MT6989_2PLANE_2K_ULTRA_0		(107 << 16 | 92)
#define MT6989_2PLANE_2K_PREULTRA_0		(138 << 16 | 123)
#define MT6989_2PLANE_2K_URGENT_1		(38 << 16 | 31)
#define MT6989_2PLANE_2K_ULTRA_1		(54 << 16 | 46)
#define MT6989_2PLANE_2K_PREULTRA_1		(69 << 16 | 61)
#define MT6989_2PLANE_2K_URGENT_2		0
#define MT6989_2PLANE_2K_ULTRA_2		0
#define MT6989_2PLANE_2K_PREULTRA_2		0
#define MT6989_2PLANE_2K_URGENT_3		0
#define MT6989_2PLANE_2K_ULTRA_3		0
#define MT6989_2PLANE_2K_PREULTRA_3		0
#define MT6989_RGB_2K_URGENT_0			(230 << 16 | 184)
#define MT6989_RGB_2K_ULTRA_0			(322 << 16 | 276)
#define MT6989_RGB_2K_PREULTRA_0		(414 << 16 | 368)
#define MT6989_RGB_2K_URGENT_1			0
#define MT6989_RGB_2K_ULTRA_1			0
#define MT6989_RGB_2K_PREULTRA_1		0
#define MT6989_RGB_2K_URGENT_2			0
#define MT6989_RGB_2K_ULTRA_2			0
#define MT6989_RGB_2K_PREULTRA_2		0
#define MT6989_RGB_2K_URGENT_3			0
#define MT6989_RGB_2K_ULTRA_3			0
#define MT6989_RGB_2K_PREULTRA_3		0
#define MT6989_ARGB_2K_URGENT_0			(307 << 16 | 246)
#define MT6989_ARGB_2K_ULTRA_0			(430 << 16 | 368)
#define MT6989_ARGB_2K_PREULTRA_0		(552 << 16 | 491)
#define MT6989_ARGB_2K_URGENT_1			0
#define MT6989_ARGB_2K_ULTRA_1			0
#define MT6989_ARGB_2K_PREULTRA_1		0
#define MT6989_ARGB_2K_URGENT_2			0
#define MT6989_ARGB_2K_ULTRA_2			0
#define MT6989_ARGB_2K_PREULTRA_2		0
#define MT6989_ARGB_2K_URGENT_3			0
#define MT6989_ARGB_2K_ULTRA_3			0
#define MT6989_ARGB_2K_PREULTRA_3		0
#define MT6989_AFBC_2K_URGENT_0			(307 << 16 | 246)
#define MT6989_AFBC_2K_ULTRA_0			(430 << 16 | 368)
#define MT6989_AFBC_2K_PREULTRA_0		(552 << 16 | 491)
#define MT6989_AFBC_2K_URGENT_1			0
#define MT6989_AFBC_2K_ULTRA_1			0
#define MT6989_AFBC_2K_PREULTRA_1		0
#define MT6989_AFBC_2K_URGENT_2			(1 << 16)
#define MT6989_AFBC_2K_ULTRA_2			(1 << 16)
#define MT6989_AFBC_2K_PREULTRA_2		(2 << 16 | 1)
#define MT6989_AFBC_2K_URGENT_3			0
#define MT6989_AFBC_2K_ULTRA_3			0
#define MT6989_AFBC_2K_PREULTRA_3		0
#define MT6989_HYFBC_2K_URGENT_0		(77 << 16 | 61)
#define MT6989_HYFBC_2K_ULTRA_0			(107 << 16 | 92)
#define MT6989_HYFBC_2K_PREULTRA_0		(138 << 16 | 123)
#define MT6989_HYFBC_2K_URGENT_1		(38 << 16 | 31)
#define MT6989_HYFBC_2K_ULTRA_1			(54 << 16 | 46)
#define MT6989_HYFBC_2K_PREULTRA_1		(69 << 16 | 61)
#define MT6989_HYFBC_2K_URGENT_2		(1 << 16)
#define MT6989_HYFBC_2K_ULTRA_2			(2 << 16 | 1)
#define MT6989_HYFBC_2K_PREULTRA_2		(3 << 16 | 2)
#define MT6989_HYFBC_2K_URGENT_3		(1 << 16)
#define MT6989_HYFBC_2K_ULTRA_3			(2 << 16 | 1)
#define MT6989_HYFBC_2K_PREULTRA_3		(3 << 16 | 2)
#define MT6989_3PLANE_FHD_URGENT_0		(43 << 16 | 35)
#define MT6989_3PLANE_FHD_ULTRA_0		(60 << 16 | 52)
#define MT6989_3PLANE_FHD_PREULTRA_0		(78 << 16 | 69)
#define MT6989_3PLANE_FHD_URGENT_1		(12 << 16 | 9)
#define MT6989_3PLANE_FHD_ULTRA_1		(16 << 16 | 13)
#define MT6989_3PLANE_FHD_PREULTRA_1		(20 << 16 | 17)
#define MT6989_3PLANE_FHD_URGENT_2		(12 << 16 | 9)
#define MT6989_3PLANE_FHD_ULTRA_2		(16 << 16 | 13)
#define MT6989_3PLANE_FHD_PREULTRA_2		(20 << 16 | 17)
#define MT6989_3PLANE_FHD_URGENT_3		0
#define MT6989_3PLANE_FHD_ULTRA_3		0
#define MT6989_3PLANE_FHD_PREULTRA_3		0
#define MT6989_2PLANE_FHD_URGENT_0		(43 << 16 | 35)
#define MT6989_2PLANE_FHD_ULTRA_0		(60 << 16 | 52)
#define MT6989_2PLANE_FHD_PREULTRA_0		(78 << 16 | 69)
#define MT6989_2PLANE_FHD_URGENT_1		(22 << 16 | 17)
#define MT6989_2PLANE_FHD_ULTRA_1		(30 << 16 | 26)
#define MT6989_2PLANE_FHD_PREULTRA_1		(39 << 16 | 35)
#define MT6989_2PLANE_FHD_URGENT_2		0
#define MT6989_2PLANE_FHD_ULTRA_2		0
#define MT6989_2PLANE_FHD_PREULTRA_2		0
#define MT6989_2PLANE_FHD_URGENT_3		0
#define MT6989_2PLANE_FHD_ULTRA_3		0
#define MT6989_2PLANE_FHD_PREULTRA_3		0
#define MT6989_RGB_FHD_URGENT_0			(129 << 16 | 104)
#define MT6989_RGB_FHD_ULTRA_0			(181 << 16 | 155)
#define MT6989_RGB_FHD_PREULTRA_0		(233 << 16 | 207)
#define MT6989_RGB_FHD_URGENT_1			0
#define MT6989_RGB_FHD_ULTRA_1			0
#define MT6989_RGB_FHD_PREULTRA_1		0
#define MT6989_RGB_FHD_URGENT_2			0
#define MT6989_RGB_FHD_ULTRA_2			0
#define MT6989_RGB_FHD_PREULTRA_2		0
#define MT6989_RGB_FHD_URGENT_3			0
#define MT6989_RGB_FHD_ULTRA_3			0
#define MT6989_RGB_FHD_PREULTRA_3		0
#define MT6989_ARGB_FHD_URGENT_0		(173 << 16 | 138)
#define MT6989_ARGB_FHD_ULTRA_0			(242 << 16 | 207)
#define MT6989_ARGB_FHD_PREULTRA_0		(311 << 16 | 276)
#define MT6989_ARGB_FHD_URGENT_1		0
#define MT6989_ARGB_FHD_ULTRA_1			0
#define MT6989_ARGB_FHD_PREULTRA_1		0
#define MT6989_ARGB_FHD_URGENT_2		0
#define MT6989_ARGB_FHD_ULTRA_2			0
#define MT6989_ARGB_FHD_PREULTRA_2		0
#define MT6989_ARGB_FHD_URGENT_3		0
#define MT6989_ARGB_FHD_ULTRA_3			0
#define MT6989_ARGB_FHD_PREULTRA_3		0
#define MT6989_AFBC_FHD_URGENT_0		(173 << 16 | 138)
#define MT6989_AFBC_FHD_ULTRA_0			(242 << 16 | 207)
#define MT6989_AFBC_FHD_PREULTRA_0		(311 << 16 | 276)
#define MT6989_AFBC_FHD_URGENT_1		0
#define MT6989_AFBC_FHD_ULTRA_1			0
#define MT6989_AFBC_FHD_PREULTRA_1		0
#define MT6989_AFBC_FHD_URGENT_2		(1 << 16)
#define MT6989_AFBC_FHD_ULTRA_2			(1 << 16)
#define MT6989_AFBC_FHD_PREULTRA_2		(2 << 16 | 1)
#define MT6989_AFBC_FHD_URGENT_3		0
#define MT6989_AFBC_FHD_ULTRA_3			0
#define MT6989_AFBC_FHD_PREULTRA_3		0
#define MT6989_HYFBC_FHD_URGENT_0		(43 << 16 | 35)
#define MT6989_HYFBC_FHD_ULTRA_0		(60 << 16 | 52)
#define MT6989_HYFBC_FHD_PREULTRA_0		(78 << 16 | 69)
#define MT6989_HYFBC_FHD_URGENT_1		(22 << 16 | 17)
#define MT6989_HYFBC_FHD_ULTRA_1		(30 << 16 | 26)
#define MT6989_HYFBC_FHD_PREULTRA_1		(39 << 16 | 35)
#define MT6989_HYFBC_FHD_URGENT_2		(1 << 16)
#define MT6989_HYFBC_FHD_ULTRA_2		(2 << 16 | 1)
#define MT6989_HYFBC_FHD_PREULTRA_2		(3 << 16 | 2)
#define MT6989_HYFBC_FHD_URGENT_3		(1 << 16)
#define MT6989_HYFBC_FHD_ULTRA_3		(2 << 16 | 1)
#define MT6989_HYFBC_FHD_PREULTRA_3		(3 << 16 | 2)

/* prefetch control settings */
#define MT6989_ARGB_4KROT0_PF1			(7 << 16)
#define MT6989_ARGB_4KROT0_PF2			0
#define MT6989_RGB_4KROT0_PF1			(7 << 16)
#define MT6989_RGB_4KROT0_PF2			0
#define MT6989_2PLANE_4KROT0_PF1		(7 << 16 | 4)
#define MT6989_2PLANE_4KROT0_PF2		0
#define MT6989_3PLANE_4KROT0_PF1		(7 << 16 | 4)
#define MT6989_3PLANE_4KROT0_PF2		(4 << 16)
#define MT6989_AFBC_4KROT0_PF1			(52 << 16)
#define MT6989_AFBC_4KROT0_PF2			(1 << 16)
#define MT6989_HYFBC_4KROT0_PF1			(52 << 16 | 52)
#define MT6989_HYFBC_4KROT0_PF2			(1 << 16 | 1)
#define MT6989_ARGB_4KROT90_PF1			(415 << 16)
#define MT6989_ARGB_4KROT90_PF2			0
#define MT6989_RGB_4KROT90_PF1			(415 << 16)
#define MT6989_RGB_4KROT90_PF2			0
#define MT6989_2PLANE_4KROT90_PF1		(208 << 16 | 104)
#define MT6989_2PLANE_4KROT90_PF2		0
#define MT6989_3PLANE_4KROT90_PF1		(208 << 16 | 104)
#define MT6989_3PLANE_4KROT90_PF2		(104 << 16)
#define MT6989_AFBC_4KROT90_PF1			(52 << 16)
#define MT6989_AFBC_4KROT90_PF2			(52 << 16)
#define MT6989_HYFBC_4KROT90_PF1		(52 << 16 | 52)
#define MT6989_HYFBC_4KROT90_PF2		(52 << 16 | 52)
#define MT6989_ARGB_2KROT0_PF1			(5 << 16)
#define MT6989_ARGB_2KROT0_PF2			0
#define MT6989_RGB_2KROT0_PF1			(5 << 16)
#define MT6989_RGB_2KROT0_PF2			0
#define MT6989_2PLANE_2KROT0_PF1		(5 << 16 | 3)
#define MT6989_2PLANE_2KROT0_PF2		0
#define MT6989_3PLANE_2KROT0_PF1		(5 << 16 | 3)
#define MT6989_3PLANE_2KROT0_PF2		(3 << 16)
#define MT6989_AFBC_2KROT0_PF1			(24 << 16)
#define MT6989_AFBC_2KROT0_PF2			(1 << 16)
#define MT6989_HYFBC_2KROT0_PF1			(24 << 16 | 24)
#define MT6989_HYFBC_2KROT0_PF2			(1 << 16 | 1)
#define MT6989_ARGB_2KROT90_PF1			(185 << 16)
#define MT6989_ARGB_2KROT90_PF2			0
#define MT6989_RGB_2KROT90_PF1			(185 << 16)
#define MT6989_RGB_2KROT90_PF2			0
#define MT6989_2PLANE_2KROT90_PF1		(93 << 16 | 47)
#define MT6989_2PLANE_2KROT90_PF2		0
#define MT6989_3PLANE_2KROT90_PF1		(93 << 16 | 47)
#define MT6989_3PLANE_2KROT90_PF2		(47 << 16)
#define MT6989_AFBC_2KROT90_PF1			(24 << 16)
#define MT6989_AFBC_2KROT90_PF2			(24 << 16)
#define MT6989_HYFBC_2KROT90_PF1		(24 << 16 | 24)
#define MT6989_HYFBC_2KROT90_PF2		(24 << 16 | 24)
#define MT6989_ARGB_FHDROT0_PF1			(4 << 16)
#define MT6989_ARGB_FHDROT0_PF2			0
#define MT6989_RGB_FHDROT0_PF1			(4 << 16)
#define MT6989_RGB_FHDROT0_PF2			0
#define MT6989_2PLANE_FHDROT0_PF1		(4 << 16 | 2)
#define MT6989_2PLANE_FHDROT0_PF2		0
#define MT6989_3PLANE_FHDROT0_PF1		(4 << 16 | 2)
#define MT6989_3PLANE_FHDROT0_PF2		(2 << 16)
#define MT6989_AFBC_FHDROT0_PF1			(13 << 16)
#define MT6989_AFBC_FHDROT0_PF2			(1 << 16)
#define MT6989_HYFBC_FHDROT0_PF1		(13 << 16 | 13)
#define MT6989_HYFBC_FHDROT0_PF2		(1 << 16 | 1)
#define MT6989_ARGB_FHDROT90_PF1		(104 << 16)
#define MT6989_ARGB_FHDROT90_PF2		0
#define MT6989_RGB_FHDROT90_PF1			(104 << 16)
#define MT6989_RGB_FHDROT90_PF2			0
#define MT6989_2PLANE_FHDROT90_PF1		(52 << 16 | 26)
#define MT6989_2PLANE_FHDROT90_PF2		0
#define MT6989_3PLANE_FHDROT90_PF1		(52 << 16 | 26)
#define MT6989_3PLANE_FHDROT90_PF2		(26 << 16)
#define MT6989_AFBC_FHDROT90_PF1		(13 << 16)
#define MT6989_AFBC_FHDROT90_PF2		(13 << 16)
#define MT6989_HYFBC_FHDROT90_PF1		(13 << 16 | 13)
#define MT6989_HYFBC_FHDROT90_PF2		(13 << 16 | 13)

static const struct golden_setting th_argb_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_ARGB_FHD_PREULTRA_0,
				.ultra		= MT6989_ARGB_FHD_ULTRA_0,
				.urgent		= MT6989_ARGB_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_ARGB_FHD_PREULTRA_1,
				.ultra		= MT6989_ARGB_FHD_ULTRA_1,
				.urgent		= MT6989_ARGB_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_ARGB_FHD_PREULTRA_2,
				.ultra		= MT6989_ARGB_FHD_ULTRA_2,
				.urgent		= MT6989_ARGB_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_ARGB_FHD_PREULTRA_3,
				.ultra		= MT6989_ARGB_FHD_ULTRA_3,
				.urgent		= MT6989_ARGB_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_ARGB_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_ARGB_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_ARGB_2K_PREULTRA_0,
				.ultra		= MT6989_ARGB_2K_ULTRA_0,
				.urgent		= MT6989_ARGB_2K_URGENT_0,
			}, {
				.preultra	= MT6989_ARGB_2K_PREULTRA_1,
				.ultra		= MT6989_ARGB_2K_ULTRA_1,
				.urgent		= MT6989_ARGB_2K_URGENT_1,
			}, {
				.preultra	= MT6989_ARGB_2K_PREULTRA_2,
				.ultra		= MT6989_ARGB_2K_ULTRA_2,
				.urgent		= MT6989_ARGB_2K_URGENT_2,
			}, {
				.preultra	= MT6989_ARGB_2K_PREULTRA_3,
				.ultra		= MT6989_ARGB_2K_ULTRA_3,
				.urgent		= MT6989_ARGB_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_ARGB_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_ARGB_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_ARGB_4K_PREULTRA_0,
				.ultra		= MT6989_ARGB_4K_ULTRA_0,
				.urgent		= MT6989_ARGB_4K_URGENT_0,
			}, {
				.preultra	= MT6989_ARGB_4K_PREULTRA_1,
				.ultra		= MT6989_ARGB_4K_ULTRA_1,
				.urgent		= MT6989_ARGB_4K_URGENT_1,
			}, {
				.preultra	= MT6989_ARGB_4K_PREULTRA_2,
				.ultra		= MT6989_ARGB_4K_ULTRA_2,
				.urgent		= MT6989_ARGB_4K_URGENT_2,
			}, {
				.preultra	= MT6989_ARGB_4K_PREULTRA_3,
				.ultra		= MT6989_ARGB_4K_ULTRA_3,
				.urgent		= MT6989_ARGB_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_ARGB_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_ARGB_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_ARGB_4KROT90_PF2,
			},
		},
	},
};

static const struct golden_setting th_rgb_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_RGB_FHD_PREULTRA_0,
				.ultra		= MT6989_RGB_FHD_ULTRA_0,
				.urgent		= MT6989_RGB_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_RGB_FHD_PREULTRA_1,
				.ultra		= MT6989_RGB_FHD_ULTRA_1,
				.urgent		= MT6989_RGB_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_RGB_FHD_PREULTRA_2,
				.ultra		= MT6989_RGB_FHD_ULTRA_2,
				.urgent		= MT6989_RGB_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_RGB_FHD_PREULTRA_3,
				.ultra		= MT6989_RGB_FHD_ULTRA_3,
				.urgent		= MT6989_RGB_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_RGB_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_RGB_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_RGB_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_RGB_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_RGB_2K_PREULTRA_0,
				.ultra		= MT6989_RGB_2K_ULTRA_0,
				.urgent		= MT6989_RGB_2K_URGENT_0,
			}, {
				.preultra	= MT6989_RGB_2K_PREULTRA_1,
				.ultra		= MT6989_RGB_2K_ULTRA_1,
				.urgent		= MT6989_RGB_2K_URGENT_1,
			}, {
				.preultra	= MT6989_RGB_2K_PREULTRA_2,
				.ultra		= MT6989_RGB_2K_ULTRA_2,
				.urgent		= MT6989_RGB_2K_URGENT_2,
			}, {
				.preultra	= MT6989_RGB_2K_PREULTRA_3,
				.ultra		= MT6989_RGB_2K_ULTRA_3,
				.urgent		= MT6989_RGB_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_RGB_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_RGB_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_RGB_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_RGB_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_RGB_4K_PREULTRA_0,
				.ultra		= MT6989_RGB_4K_ULTRA_0,
				.urgent		= MT6989_RGB_4K_URGENT_0,
			}, {
				.preultra	= MT6989_RGB_4K_PREULTRA_1,
				.ultra		= MT6989_RGB_4K_ULTRA_1,
				.urgent		= MT6989_RGB_4K_URGENT_1,
			}, {
				.preultra	= MT6989_RGB_4K_PREULTRA_2,
				.ultra		= MT6989_RGB_4K_ULTRA_2,
				.urgent		= MT6989_RGB_4K_URGENT_2,
			}, {
				.preultra	= MT6989_RGB_4K_PREULTRA_3,
				.ultra		= MT6989_RGB_4K_ULTRA_3,
				.urgent		= MT6989_RGB_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_RGB_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_RGB_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_RGB_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_RGB_4KROT90_PF2,
			},
		},
	},
};

static const struct golden_setting th_yuv420_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_2PLANE_FHD_PREULTRA_0,
				.ultra		= MT6989_2PLANE_FHD_ULTRA_0,
				.urgent		= MT6989_2PLANE_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_2PLANE_FHD_PREULTRA_1,
				.ultra		= MT6989_2PLANE_FHD_ULTRA_1,
				.urgent		= MT6989_2PLANE_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_2PLANE_FHD_PREULTRA_2,
				.ultra		= MT6989_2PLANE_FHD_ULTRA_2,
				.urgent		= MT6989_2PLANE_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_2PLANE_FHD_PREULTRA_3,
				.ultra		= MT6989_2PLANE_FHD_ULTRA_3,
				.urgent		= MT6989_2PLANE_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_2PLANE_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_2PLANE_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_2PLANE_2K_PREULTRA_0,
				.ultra		= MT6989_2PLANE_2K_ULTRA_0,
				.urgent		= MT6989_2PLANE_2K_URGENT_0,
			}, {
				.preultra	= MT6989_2PLANE_2K_PREULTRA_1,
				.ultra		= MT6989_2PLANE_2K_ULTRA_1,
				.urgent		= MT6989_2PLANE_2K_URGENT_1,
			}, {
				.preultra	= MT6989_2PLANE_2K_PREULTRA_2,
				.ultra		= MT6989_2PLANE_2K_ULTRA_2,
				.urgent		= MT6989_2PLANE_2K_URGENT_2,
			}, {
				.preultra	= MT6989_2PLANE_2K_PREULTRA_3,
				.ultra		= MT6989_2PLANE_2K_ULTRA_3,
				.urgent		= MT6989_2PLANE_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_2PLANE_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_2PLANE_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_2PLANE_4K_PREULTRA_0,
				.ultra		= MT6989_2PLANE_4K_ULTRA_0,
				.urgent		= MT6989_2PLANE_4K_URGENT_0,
			}, {
				.preultra	= MT6989_2PLANE_4K_PREULTRA_1,
				.ultra		= MT6989_2PLANE_4K_ULTRA_1,
				.urgent		= MT6989_2PLANE_4K_URGENT_1,
			}, {
				.preultra	= MT6989_2PLANE_4K_PREULTRA_2,
				.ultra		= MT6989_2PLANE_4K_ULTRA_2,
				.urgent		= MT6989_2PLANE_4K_URGENT_2,
			}, {
				.preultra	= MT6989_2PLANE_4K_PREULTRA_3,
				.ultra		= MT6989_2PLANE_4K_ULTRA_3,
				.urgent		= MT6989_2PLANE_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_2PLANE_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_2PLANE_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_2PLANE_4KROT90_PF2,
			},
		},
	},
};

static const struct golden_setting th_yv12_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_3PLANE_FHD_PREULTRA_0,
				.ultra		= MT6989_3PLANE_FHD_ULTRA_0,
				.urgent		= MT6989_3PLANE_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_3PLANE_FHD_PREULTRA_1,
				.ultra		= MT6989_3PLANE_FHD_ULTRA_1,
				.urgent		= MT6989_3PLANE_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_3PLANE_FHD_PREULTRA_2,
				.ultra		= MT6989_3PLANE_FHD_ULTRA_2,
				.urgent		= MT6989_3PLANE_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_3PLANE_FHD_PREULTRA_3,
				.ultra		= MT6989_3PLANE_FHD_ULTRA_3,
				.urgent		= MT6989_3PLANE_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_3PLANE_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_3PLANE_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_3PLANE_2K_PREULTRA_0,
				.ultra		= MT6989_3PLANE_2K_ULTRA_0,
				.urgent		= MT6989_3PLANE_2K_URGENT_0,
			}, {
				.preultra	= MT6989_3PLANE_2K_PREULTRA_1,
				.ultra		= MT6989_3PLANE_2K_ULTRA_1,
				.urgent		= MT6989_3PLANE_2K_URGENT_1,
			}, {
				.preultra	= MT6989_3PLANE_2K_PREULTRA_2,
				.ultra		= MT6989_3PLANE_2K_ULTRA_2,
				.urgent		= MT6989_3PLANE_2K_URGENT_2,
			}, {
				.preultra	= MT6989_3PLANE_2K_PREULTRA_3,
				.ultra		= MT6989_3PLANE_2K_ULTRA_3,
				.urgent		= MT6989_3PLANE_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_3PLANE_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_3PLANE_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_3PLANE_4K_PREULTRA_0,
				.ultra		= MT6989_3PLANE_4K_ULTRA_0,
				.urgent		= MT6989_3PLANE_4K_URGENT_0,
			}, {
				.preultra	= MT6989_3PLANE_4K_PREULTRA_1,
				.ultra		= MT6989_3PLANE_4K_ULTRA_1,
				.urgent		= MT6989_3PLANE_4K_URGENT_1,
			}, {
				.preultra	= MT6989_3PLANE_4K_PREULTRA_2,
				.ultra		= MT6989_3PLANE_4K_ULTRA_2,
				.urgent		= MT6989_3PLANE_4K_URGENT_2,
			}, {
				.preultra	= MT6989_3PLANE_4K_PREULTRA_3,
				.ultra		= MT6989_3PLANE_4K_ULTRA_3,
				.urgent		= MT6989_3PLANE_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_3PLANE_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_3PLANE_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_3PLANE_4KROT90_PF2,
			},
		},
	},
};

static const struct golden_setting th_afbc_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_AFBC_FHD_PREULTRA_0,
				.ultra		= MT6989_AFBC_FHD_ULTRA_0,
				.urgent		= MT6989_AFBC_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_AFBC_FHD_PREULTRA_1,
				.ultra		= MT6989_AFBC_FHD_ULTRA_1,
				.urgent		= MT6989_AFBC_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_AFBC_FHD_PREULTRA_2,
				.ultra		= MT6989_AFBC_FHD_ULTRA_2,
				.urgent		= MT6989_AFBC_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_AFBC_FHD_PREULTRA_3,
				.ultra		= MT6989_AFBC_FHD_ULTRA_3,
				.urgent		= MT6989_AFBC_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_AFBC_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_AFBC_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_AFBC_2K_PREULTRA_0,
				.ultra		= MT6989_AFBC_2K_ULTRA_0,
				.urgent		= MT6989_AFBC_2K_URGENT_0,
			}, {
				.preultra	= MT6989_AFBC_2K_PREULTRA_1,
				.ultra		= MT6989_AFBC_2K_ULTRA_1,
				.urgent		= MT6989_AFBC_2K_URGENT_1,
			}, {
				.preultra	= MT6989_AFBC_2K_PREULTRA_2,
				.ultra		= MT6989_AFBC_2K_ULTRA_2,
				.urgent		= MT6989_AFBC_2K_URGENT_2,
			}, {
				.preultra	= MT6989_AFBC_2K_PREULTRA_3,
				.ultra		= MT6989_AFBC_2K_ULTRA_3,
				.urgent		= MT6989_AFBC_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_AFBC_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_AFBC_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_AFBC_4K_PREULTRA_0,
				.ultra		= MT6989_AFBC_4K_ULTRA_0,
				.urgent		= MT6989_AFBC_4K_URGENT_0,
			}, {
				.preultra	= MT6989_AFBC_4K_PREULTRA_1,
				.ultra		= MT6989_AFBC_4K_ULTRA_1,
				.urgent		= MT6989_AFBC_4K_URGENT_1,
			}, {
				.preultra	= MT6989_AFBC_4K_PREULTRA_2,
				.ultra		= MT6989_AFBC_4K_ULTRA_2,
				.urgent		= MT6989_AFBC_4K_URGENT_2,
			}, {
				.preultra	= MT6989_AFBC_4K_PREULTRA_3,
				.ultra		= MT6989_AFBC_4K_ULTRA_3,
				.urgent		= MT6989_AFBC_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_AFBC_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_AFBC_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_AFBC_4KROT90_PF2,
			},
		},
	},
};

static const struct golden_setting th_hyfbc_mt6989[] = {
	{
		.pixel = GOLDEN_PIXEL_FHD,
		.plane = {
			{
				.preultra	= MT6989_HYFBC_FHD_PREULTRA_0,
				.ultra		= MT6989_HYFBC_FHD_ULTRA_0,
				.urgent		= MT6989_HYFBC_FHD_URGENT_0,
			}, {
				.preultra	= MT6989_HYFBC_FHD_PREULTRA_1,
				.ultra		= MT6989_HYFBC_FHD_ULTRA_1,
				.urgent		= MT6989_HYFBC_FHD_URGENT_1,
			}, {
				.preultra	= MT6989_HYFBC_FHD_PREULTRA_2,
				.ultra		= MT6989_HYFBC_FHD_ULTRA_2,
				.urgent		= MT6989_HYFBC_FHD_URGENT_2,
			}, {
				.preultra	= MT6989_HYFBC_FHD_PREULTRA_3,
				.ultra		= MT6989_HYFBC_FHD_ULTRA_3,
				.urgent		= MT6989_HYFBC_FHD_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_HYFBC_FHDROT0_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_FHDROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_HYFBC_FHDROT90_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_FHDROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_2K,
		.plane = {
			{
				.preultra	= MT6989_HYFBC_2K_PREULTRA_0,
				.ultra		= MT6989_HYFBC_2K_ULTRA_0,
				.urgent		= MT6989_HYFBC_2K_URGENT_0,
			}, {
				.preultra	= MT6989_HYFBC_2K_PREULTRA_1,
				.ultra		= MT6989_HYFBC_2K_ULTRA_1,
				.urgent		= MT6989_HYFBC_2K_URGENT_1,
			}, {
				.preultra	= MT6989_HYFBC_2K_PREULTRA_2,
				.ultra		= MT6989_HYFBC_2K_ULTRA_2,
				.urgent		= MT6989_HYFBC_2K_URGENT_2,
			}, {
				.preultra	= MT6989_HYFBC_2K_PREULTRA_3,
				.ultra		= MT6989_HYFBC_2K_ULTRA_3,
				.urgent		= MT6989_HYFBC_2K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_HYFBC_2KROT0_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_2KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_HYFBC_2KROT90_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_2KROT90_PF2,
			},
		},
	}, {
		.pixel = GOLDEN_PIXEL_4K,
		.plane = {
			{
				.preultra	= MT6989_HYFBC_4K_PREULTRA_0,
				.ultra		= MT6989_HYFBC_4K_ULTRA_0,
				.urgent		= MT6989_HYFBC_4K_URGENT_0,
			}, {
				.preultra	= MT6989_HYFBC_4K_PREULTRA_1,
				.ultra		= MT6989_HYFBC_4K_ULTRA_1,
				.urgent		= MT6989_HYFBC_4K_URGENT_1,
			}, {
				.preultra	= MT6989_HYFBC_4K_PREULTRA_2,
				.ultra		= MT6989_HYFBC_4K_ULTRA_2,
				.urgent		= MT6989_HYFBC_4K_URGENT_2,
			}, {
				.preultra	= MT6989_HYFBC_4K_PREULTRA_3,
				.ultra		= MT6989_HYFBC_4K_ULTRA_3,
				.urgent		= MT6989_HYFBC_4K_URGENT_3,
			},
		},
		.pfrot = {
			{
				.prefetch_ctrl1 = MT6989_HYFBC_4KROT0_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_4KROT0_PF2,
			}, {
				.prefetch_ctrl1 = MT6989_HYFBC_4KROT90_PF1,
				.prefetch_ctrl2 = MT6989_HYFBC_4KROT90_PF2,
			},
		},
	},
};



/* !!Above code generate by golden parser (goldenparser.py)!! */


/* end of mt6989 */

#endif	/* __MTK_MML_RROT_GOLDEN_H__ */
