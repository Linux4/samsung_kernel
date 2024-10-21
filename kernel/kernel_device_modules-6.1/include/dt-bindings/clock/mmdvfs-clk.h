/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _DT_BINDINGS_MMDVFS_CLK_H
#define _DT_BINDINGS_MMDVFS_CLK_H

/* v2 */
#define MMDVFS_MUX_DIS		0
#define MMDVFS_MUX_MDP		1
#define MMDVFS_MUX_MML		2
#define MMDVFS_MUX_SMI		3
#define MMDVFS_MUX_VEN		4
#define MMDVFS_MUX_VDE		5
#define MMDVFS_MUX_IMG		6
#define MMDVFS_MUX_CAM		7
#define MMDVFS_MUX_NUM		8

#define MMDVFS_USER_DISP	0
#define MMDVFS_USER_MDP		1
#define MMDVFS_USER_MML		2
#define MMDVFS_USER_SMI		3

#define MMDVFS_USER_JPEGDEC	4
#define MMDVFS_USER_JPEGENC	5
#define MMDVFS_USER_VENC	6

#define MMDVFS_USER_VFMT	7
#define MMDVFS_USER_IMG		8
#define MMDVFS_USER_CAM		9

#define MMDVFS_USER_VCORE	10
#define MMDVFS_USER_VMM		11
#define MMDVFS_USER_VDISP	12
#define MMDVFS_USER_VDEC	13

#define DUMMY_USER_DIS		14
#define DUMMY_USER_MDP		15
#define DUMMY_USER_MML		16
#define DUMMY_USER_SMI		17
#define DUMMY_USER_VEN		18
#define DUMMY_USER_VDE		19
#define DUMMY_USER_IMG		20
#define DUMMY_USER_CAM		21

#define MMDVFS_USER_IMG_SMI	22
/* next MMDVFS_USER */

#define MMDVFS_USER_NUM		23

#define MMDVFS_VCP_USER_VDEC	0
#define MMDVFS_VCP_USER_VENC	1
#define MMDVFS_VCP_USER_NUM	2

/* v1 */
/* clock consumer */
#define CLK_MMDVFS_DISP		0
#define CLK_MMDVFS_MDP		1
#define CLK_MMDVFS_MML		2
#define CLK_MMDVFS_SMI_COMMON0	3
#define CLK_MMDVFS_SMI_COMMON1	4

#define CLK_MMDVFS_VENC		5
#define CLK_MMDVFS_JPEGENC	6

#define CLK_MMDVFS_VDEC		7
#define CLK_MMDVFS_VFMT		8
#define CLK_MMDVFS_JPEGDEC	9

#define CLK_MMDVFS_IMG		10
#define CLK_MMDVFS_IPE		11
#define CLK_MMDVFS_CAM		12
#define CLK_MMDVFS_CCU		13
#define CLK_MMDVFS_AOV		14

#define CLK_MMDVFS_VCORE	15
#define CLK_MMDVFS_VMM		16
#define CLK_MMDVFS_VDISP	17

/* new clk append here */
#define CLK_MMDVFS_NUM		18

/* power supplier */
#define PWR_MMDVFS_VCORE	0
#define PWR_MMDVFS_VMM		1
#define PWR_MMDVFS_VDISP	2
#define PWR_MMDVFS_NUM		3

/* ipi type */
#define IPI_MMDVFS_VCP		0
#define IPI_MMDVFS_CCU		1

/* spec type */
#define SPEC_MMDVFS_NORMAL	0
#define SPEC_MMDVFS_ALONE	1
#define SPEC_MMDVFS_DVFSRC	2

#endif /* _DT_BINDINGS_MMDVFS_CLK_H */
