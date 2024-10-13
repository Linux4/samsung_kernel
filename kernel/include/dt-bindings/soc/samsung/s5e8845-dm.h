/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for S5E8845
 */

#ifndef _DT_BINDINGS_S5E8845_DM_H
#define _DT_BINDINGS_S5E8845_DM_H

/* NUMBER FOR DVFS MANAGER */
#define DM_MIF				(0x00)
#define DM_INT				(0x01)
#define DM_CPUCL0			(0x02)
#define DM_CPUCL1			(0x03)
#define DM_DSU				(0x04)
#define DM_NPU				(0x05)
#define DM_DNC				(0x06)
#define DM_AUD				(0x07)
#define DM_G3D				(0x08)
#define DM_DISP				(0x09)
#define DM_INTCAM			(0x0A)
#define DM_CAM				(0x0B)
#define DM_ISP				(0x0C)
#define DM_MFC				(0x0D)
#define DM_CSIS				(0x0E)
#define DM_ICPU				(0x0F)

/* CONSTRAINT TYPE */
#define CONSTRAINT_MIN	0
#define CONSTRAINT_MAX	1

#endif	/* _DT_BINDINGS_S5E8845_DM_H */
