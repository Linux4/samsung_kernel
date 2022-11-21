/* drivers/video/exynos_decon/lcd.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_LCD__
#define __DECON_LCD__

enum decon_psr_mode {
	DECON_VIDEO_MODE = 0,
	DECON_DP_PSR_MODE = 1,
	DECON_MIPI_COMMAND_MODE = 2,
};

/* Mic ratio: 0: 1/2 ratio, 1: 1/3 ratio */
enum decon_mic_comp_ratio {
	MIC_COMP_RATIO_1_2 = 0,
	MIC_COMP_RATIO_1_3 = 1,
	MIC_COMP_BYPASS
};

enum mic_ver {
	MIC_VER_1_1,
	MIC_VER_1_2,
	MIC_VER_2_0,
};

enum type_of_ddi {
	TYPE_OF_SM_DDI = 0,
	TYPE_OF_MAGNA_DDI,
	TYPE_OF_NORMAL_DDI,
};

struct stdphy_pms {
	unsigned int p;
	unsigned int m;
	unsigned int s;
};

struct decon_lcd {
	enum decon_psr_mode mode;
	unsigned int dsim_vfp;
	unsigned int dsim_vbp;
	unsigned int dsim_hfp;
	unsigned int dsim_hbp;

	unsigned int dsim_vsa;
	unsigned int dsim_hsa;

	unsigned int decon_vfp;
	unsigned int decon_vbp;
	unsigned int decon_hfp;
	unsigned int decon_hbp;

	unsigned int decon_vsa;
	unsigned int decon_hsa;

	unsigned int xres;
	unsigned int yres;

	unsigned int width;
	unsigned int height;

	unsigned int dispif_w;
	unsigned int dispif_h;

	unsigned int hs_clk;
	struct stdphy_pms dphy_pms;
	unsigned int esc_clk;

	unsigned int fps;
	unsigned int mic_enabled;
	enum decon_mic_comp_ratio mic_ratio;
	unsigned int dsc_enabled;
	unsigned int dsc_slice;
	enum mic_ver mic_ver;
	enum type_of_ddi ddi_type;
	unsigned int clklane_onoff;

	unsigned int rev;
};

#endif
