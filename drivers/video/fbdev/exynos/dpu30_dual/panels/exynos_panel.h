/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS Panel Information.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PANEL_H__
#define __EXYNOS_PANEL_H__

enum decon_psr_mode {
	DECON_VIDEO_MODE = 0,
	DECON_DP_PSR_MODE = 1,
	DECON_MIPI_COMMAND_MODE = 2,
};

enum type_of_ddi {
	TYPE_OF_SM_DDI = 0,
	TYPE_OF_MAGNA_DDI,
	TYPE_OF_NORMAL_DDI,
};

#define MAX_RES_NUMBER		5
#define HDR_CAPA_NUM		4
#define MAX_DDI_NAME_LEN	50

struct lcd_res_info {
	unsigned int width;
	unsigned int height;
	unsigned int dsc_en;
	unsigned int dsc_width;
	unsigned int dsc_height;
};

struct lcd_mres_info {
	u32 en;
	u32 number;
	struct lcd_res_info res_info[MAX_RES_NUMBER];
};

struct lcd_hdr_info {
	unsigned int num;
	unsigned int type[HDR_CAPA_NUM];
	unsigned int max_luma;
	unsigned int max_avg_luma;
	unsigned int min_luma;
};

/*
 * dec_sw : slice width in DDI side
 * enc_sw : slice width in AP(DECON & DSIM) side
 */
struct dsc_slice {
	unsigned int dsc_dec_sw[MAX_RES_NUMBER];
	unsigned int dsc_enc_sw[MAX_RES_NUMBER];
};

struct stdphy_pms {
	unsigned int p;
	unsigned int m;
	unsigned int s;
	unsigned int k;
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	unsigned int mfr;
	unsigned int mrr;
	unsigned int sel_pf;
	unsigned int icp;
	unsigned int afc_enb;
	unsigned int extafc;
	unsigned int feed_en;
	unsigned int fsel;
	unsigned int fout_mask;
	unsigned int rsel;
#endif
};

struct exynos_dsc {
	u32 en;
	u32 cnt;
	u32 slice_num;
	u32 slice_h;
	u32 dec_sw;
	u32 enc_sw;
};

#ifdef CONFIG_EXYNOS_SET_ACTIVE
#define MAX_DISPLAY_MODE		32
struct exynos_display_mode {
	u32 index;
	u32 width;
	u32 height;
	u32 mm_width;
	u32 mm_height;
	u32 fps;
};
#endif

#ifdef CONFIG_DYNAMIC_FREQ

#define MAX_DYNAMIC_FREQ	5

struct df_status_info {
	bool enabled;
	u32 request_df;
	u32 target_df;
	u32 current_df;
	u32 ffc_df;
	u32 context;
};

struct df_setting_info {
	unsigned int hs;
	struct stdphy_pms dphy_pms;
	unsigned int cmd_underrun_lp_ref[MAX_RES_NUMBER];
};

struct df_dt_info {
	int dft_index;
	int df_cnt;
	struct df_setting_info setting_info[MAX_DYNAMIC_FREQ];
};

struct df_param {
	struct stdphy_pms pms;
	unsigned int cmd_underrun_lp_ref[MAX_RES_NUMBER];
	u32 context;
};
#endif


struct exynos_panel_info {
	unsigned int id; /* panel id. It is used for finding connected panel */
	enum decon_psr_mode mode;

	unsigned int vfp;
	unsigned int vbp;
	unsigned int hfp;
	unsigned int hbp;
	unsigned int vsa;
	unsigned int hsa;

	/* resolution */
	unsigned int xres;
	unsigned int yres;

	/* physical size */
	unsigned int width;
	unsigned int height;

	unsigned int hs_clk;
	struct stdphy_pms dphy_pms;
	unsigned int esc_clk;

	unsigned int fps;
	unsigned int vrr_mode;
	unsigned int target_fps;
	unsigned int target_vrr_mode;

	struct exynos_dsc dsc;

	enum type_of_ddi ddi_type;
	unsigned int data_lane;
	unsigned int cmd_underrun_cnt[MAX_RES_NUMBER];
	unsigned int vt_compensation;
	unsigned int mres_mode;
	struct lcd_mres_info mres;
	struct lcd_hdr_info hdr;
	struct dsc_slice dsc_slice;
	unsigned int bpc;
#ifdef CONFIG_EXYNOS_SET_ACTIVE
	int display_mode_count;
	struct exynos_display_mode display_mode[MAX_DISPLAY_MODE];
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	struct df_dt_info df_set_info;
#endif
#if defined(CONFIG_EXYNOS_DECON_DQE)
	char ddi_name[MAX_DDI_NAME_LEN];
#endif
};
#endif /* __EXYNOS_PANEL_H__ */
