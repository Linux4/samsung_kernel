/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for S5E9925
 */

#ifndef _DT_BINDINGS_S5E8535_DM_H
#define _DT_BINDINGS_S5E8535_DM_H

/* NUMBER FOR DVFS MANAGER */
#define DM_MIF		0
#define DM_INT		1
#define DM_CPUCL0	2
#define DM_CPUCL1	3
#define DM_DSU		4
#define DM_M2M		5
#define DM_DISP		6
#define DM_AUD		7
#define DM_G3D		8
#define DM_CP_CPU	9
#define DM_CP		10
#define DM_GNSS		11
#define DM_CP_MCW	12
#define DM_CAM		13
#define DM_CSIS		14
#define DM_ISP		15
#define DM_MFC		16
#define DM_WLBT		17

/* CONSTRAINT TYPE */
#define CONSTRAINT_MIN	0
#define CONSTRAINT_MAX	1

#endif	/* _DT_BINDINGS_S5E8535_DM_H */
