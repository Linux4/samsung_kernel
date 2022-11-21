/*
 * Cal header file for Exynos Generic power domain.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* ########################
   ##### BLK_G3D info #####
   ######################## */

#include "S5E7880-cmusfr.h"
#include "S5E7880-pmusfr.h"

static struct exynos_pd_clk top_clks_g3d[] = {
};

static struct exynos_pd_clk local_clks_g3d[] = {
};

static struct exynos_pd_clk asyncbridge_clks_g3d[] = {
};

static struct exynos_pd_reg sys_pwr_regs_g3d[] = {
};

static struct sfr_save save_list_g3d[] = {
};

/* #########################
   ##### BLK_DISPAUD info #####
   ######################### */

static struct exynos_pd_clk top_clks_dispaud[] = {
};

static struct exynos_pd_clk local_clks_dispaud[] = {
};

static struct exynos_pd_clk asyncbridge_clks_dispaud[] = {
};

static struct exynos_pd_reg sys_pwr_regs_dispaud[] = {
};

static struct sfr_save save_list_dispaud[] = {
};

/* #########################
   ##### BLK_ISP info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp[] = {
};

static struct exynos_pd_clk local_clks_isp[] = {
};

static struct exynos_pd_clk asyncbridge_clks_isp[] = {
};

static struct exynos_pd_reg sys_pwr_regs_isp[] = {
};

static struct sfr_save save_list_isp[] = {
};

/* #########################
   ##### BLK_MFCMSCL info #####
   ######################### */

static struct exynos_pd_clk top_clks_mfcmscl[] = {
};

static struct exynos_pd_clk local_clks_mfcmscl[] = {
};

static struct exynos_pd_clk asyncbridge_clks_mfcmscl[] = {
};

static struct exynos_pd_reg sys_pwr_regs_mfcmscl[] = {
};

static struct sfr_save save_list_mfcmscl[] = {
};
