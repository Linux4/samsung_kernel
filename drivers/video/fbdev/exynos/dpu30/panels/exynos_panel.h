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

/*
 * TODO : VRR NS/HS MODE need to be unified.
 * - dpu30/decon.h : WIN_VRR_NORMAL_MODE, WIN_VRR_HS_MODE
 * - dpu30/exynos_panel.h : EXYNOS_PANEL_VRR_NS_MODE, EXYNOS_PANEL_VRR_HS_MODE
 * - panel/panel.h : VRR_NORMAL_MODE, VRR_HS_MODE,
 */
enum {
	EXYNOS_PANEL_VRR_NS_MODE = 0,
	EXYNOS_PANEL_VRR_HS_MODE = 1,
};

static inline char *EXYNOS_VRR_MODE_STR(int vrr_mode)
{
	return (vrr_mode == EXYNOS_PANEL_VRR_NS_MODE) ?  "NS" : "HS";
}

#define MAX_DISPLAY_MODE		32
/* exposed to user */
struct exynos_display_mode_old {
	u32 index;
	u32 width;
	u32 height;
	u32 mm_width;
	u32 mm_height;
	u32 fps;
};

struct exynos_display_mode {
	unsigned int index;
	unsigned int width;
	unsigned int height;
	unsigned int mm_width;
	unsigned int mm_height;
	unsigned int fps;
	unsigned int group;
};

/* used internally by driver */
struct exynos_display_mode_info {
	struct exynos_display_mode mode;
	u32 cmd_lp_ref;
	bool dsc_en;
	u32 dsc_width;
	u32 dsc_height;
	u32 dsc_dec_sw;
	u32 dsc_enc_sw;
	void *pdata;		/* used by common panel driver */
};

struct exynos_display_modes {
	/*
	 * exynos_display_mode_info get from
	 * panel_display_modes.
	 */
	unsigned int num_mode_infos;
	unsigned int native_mode_info;
	struct exynos_display_mode_info **mode_infos;

	/*
	 * exynos_display_mode get from @mode_infos
	 */
	unsigned int num_modes;
	unsigned int native_mode;
	struct exynos_display_mode **modes;
};

#ifdef CONFIG_DYNAMIC_FREQ

#define MAX_DYNAMIC_FREQ	5

struct df_status_info {
	bool enabled;
	u32 request_df;
	u32 target_df;
	u32 current_df;
	u32 ffc_df;
	u32 context;

	u32 current_ddi_osc;
	u32 request_ddi_osc;
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

#define MAX_COLOR_MODE		5

struct  panel_color_mode {
	int cnt;
	int mode[MAX_COLOR_MODE];
};

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
	unsigned int old_xres;
	unsigned int old_yres;

	/* physical size */
	unsigned int width;
	unsigned int height;

	unsigned int hs_clk;
	struct stdphy_pms dphy_pms;
	unsigned int esc_clk;

	unsigned int req_vrr_fps;
	unsigned int req_vrr_mode;
	unsigned int fps;
	unsigned int vrr_mode;
	unsigned int panel_vrr_fps;
	unsigned int panel_vrr_mode;
	unsigned int panel_te_sw_skip_count;
	unsigned int panel_te_hw_skip_count;

	struct exynos_dsc dsc;

	enum type_of_ddi ddi_type;
	unsigned int data_lane;
	unsigned int vt_compensation;
	struct lcd_mres_info mres;
	struct lcd_hdr_info hdr;
	unsigned int bpc;
#ifdef CONFIG_DYNAMIC_FREQ
	struct df_dt_info df_set_info;
#endif
#if defined(CONFIG_EXYNOS_DECON_DQE)
	char ddi_name[MAX_DDI_NAME_LEN];
#endif
	int display_mode_count;
	unsigned int cur_mode_idx;
	unsigned int req_mode_idx;
	struct exynos_display_mode_info display_mode[MAX_DISPLAY_MODE];
	struct panel_color_mode color_mode;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	unsigned int cur_exynos_mode;
	unsigned int req_exynos_mode;
	struct exynos_display_modes *exynos_modes;
	struct panel_display_modes *panel_modes;
#endif
};

#if defined(CONFIG_DECON_VRR_MODULATION)
/*
 * exynos_panel_vsync_hz() function returns
 * ddi's scan fps according to vrr mode.
 */
static inline int exynos_panel_vsync_hz(struct exynos_panel_info *lcd_info)
{
	if (!lcd_info)
		return -EINVAL;

	return lcd_info->panel_vrr_fps;
}

static inline int exynos_panel_div_count(struct exynos_panel_info *lcd_info)
{
	if (!lcd_info)
		return -EINVAL;

	return lcd_info->panel_te_sw_skip_count + 1;
}
#endif
#endif /* __EXYNOS_PANEL_H__ */
