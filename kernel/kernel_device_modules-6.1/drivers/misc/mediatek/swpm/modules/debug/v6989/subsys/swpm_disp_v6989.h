/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MTK_DISP_SWPM_H
#define MTK_DISP_SWPM_H

/* only record display0 behavior so far */
#define SWPM_DISP_NUM 1

enum disp_cmd_action {
	DISP_GET_SWPM_ADDR = 0,
	DISP_SWPM_SET_POWER_STA = 1,
	DISP_SWPM_SET_IDLE_STA = 2,
};

enum disp_pq_enum {
	DISP_PQ_AAL = 0,
	DISP_PQ_CCORR,
	DISP_PQ_C3D,
	DISP_PQ_GAMMA,
	DISP_PQ_COLOR,
	DISP_PQ_TDSHP,
	DISP_PQ_DITHER,
	DISP_PQ_MAX,		/* ALWAYS keep at the end*/
};

struct disp_swpm_data {
	unsigned int dsi_lane_num;
	unsigned int dsi_phy_type;
	unsigned int dsi_data_rate;
	unsigned int disp_aal;
	unsigned int disp_ccorr;
	unsigned int disp_c3d;
	unsigned int disp_gamma;
	unsigned int disp_color;
	unsigned int disp_tdshp;
	unsigned int disp_dither;
};

extern int mtk_disp_get_pq_data(unsigned int info_idx);
extern int mtk_disp_get_dsi_data_rate(unsigned int info_idx);
extern int swpm_disp_v6989_init(void);
extern void swpm_disp_v6989_exit(void);
#endif

