// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Exynos System MMU.
 */

#ifndef _DT_BINDINGS_SAMSUNG_SYSMMU_V9_H
#define _DT_BINDINGS_SAMSUNG_SYSMMU_V9_H

/* combine stlb id, ptlb id, fetch size, prefetch direction, prefetch enable in STREAM_CFG*/
#define STLB_ID(id)	(((id) & 0xFF) << 24)
#define PTLB_ID(id)	(((id) & 0xFF) << 16)

/* define fetch size */
#define SIZE1		0x0
#define SIZE2		0x1
#define SIZE4		0x2
#define SIZE8		0x3
#define SIZE16		0x4
#define SIZE32		0x5
#define SIZE64		0x6
#define FETCH_SIZE(size)	(size << 4)

/* define prefetch enable in STREAM_CFG */
#define DEFAULT_DIR	(0x2 << 2)
#define STLB_EN		(0x1 << 1)
#define STLB_DIS	(0x0 << 1)
#define PTLB_EN		0x1
#define PTLB_DIS	0x0

#define STREAM_CFG(ptlb, stlb, fetchsize, ptlb_pf, stlb_pf)	\
			(ptlb |	stlb | fetchsize | DEFAULT_DIR | ptlb_pf | stlb_pf)
#define STREAM_CFG_DEFAULT					0x0

/* define direction in STREAM_MATCH_CFG */
#define DIR_NONE		(0x0 << 8)
#define DIR_READ		(0x1 << 8)
#define DIR_WRITE		(0x2 << 8)
#define DIR_RW			(0x3 << 8)

#define STREAM_VALID		0x1

#define STREAM_MATCH_CFG(dir)	(dir | STREAM_VALID)

/* define STREAM_MATCH_SID_VALUE */
#define STREAM_MATCH_SID_VALUE(val)	((val) & 0x7FF)

/* define STREAM_MATCH_SID_MASK */
#define STREAM_MATCH_SID_MASK(mask)	((mask) & 0x7FF)

#endif /* _DT_BINDINGS_SAMSUNG_SYSMMU_V9_H */
