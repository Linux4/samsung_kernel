#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_VDDI[] = {
	TOP_DIV_CLKCMU_NOCL1A_NOC,
	TOP_MUX_CLKCMU_NOCL1A_NOC,
};
enum clk_id cmucal_VDD_ALIVE[] = {
    ALIVE_DIV_CLK_ALIVE_NOC,
    ALIVE_DIV_CLK_ALIVE_CMGP_PERI_ALIVE,
    ALIVE_DIV_CLK_ALIVE_CMGP_NOC,
    ALIVE_DIV_CLK_ALIVE_CHUBVTS_NOC,
    ALIVE_DIV_CLK_PMU_SUB,
    ALIVE_DIV_CLK_ALIVE_USI0,
    ALIVE_DIV_CLK_ALIVE_DBGCORE_NOC,
    ALIVE_DIV_CLK_ALIVE_CHUB_PERI,
    ALIVE_DIV_CLK_ALIVE_ECU,
    ALIVE_DIV_CLK_ALIVE_SPMI,
    CMGP_DIV_CLK_CMGP_USI00_USI,
    CMGP_DIV_CLK_CMGP_USI01_USI,
    CMGP_DIV_CLK_CMGP_USI02_USI,
    CMGP_DIV_CLK_CMGP_USI03_USI,
    CMGP_DIV_CLK_CMGP_USI04_USI,
    CMGP_DIV_CLK_CMGP_USI05_USI,
    CMGP_DIV_CLK_CMGP_USI06_USI,
};
enum clk_id cmucal_VDD_CAM[] = {
    CSIS_DIV_CLK_CSIS_DCPHY,
    RGBP_DIV_CLK_RGBP_ECU,
    RGBP_DIV_CLK_RGBP_NOCL0,
};
enum clk_id cmucal_VDD_CHUB[] = {
    CHUB_DIV_CLK_CHUB_USI3,
    CHUB_DIV_CLK_CHUB_USI1,
    CHUB_DIV_CLK_CHUB_USI0,
    CHUB_DIV_CLK_CHUB_NOC,
    CHUB_DIV_CLK_CHUB_USI2,
};
enum clk_id cmucal_VDD_ICPU_CSILINK[] = {
    ICPU_DIV_CLK_ICPU_NOCP,
    ICPU_DIV_CLK_ICPU_PCLKDBG,
};
enum clk_id cmucal_VDD_CPUCL0[] = {
    CPUCL0_PLL_CPUCL0,
    DSU_PLL_DSU,
    DSU_DIV_CLK_CLUSTER_ACLK,
};
enum clk_id cmucal_VDD_G3D[] = {
    G3D_DIV_CLK_G3D_NOCP,
    G3D_DIV_CLK_G3D_ECU,
};
enum clk_id cmucal_VDD_NPU[] = {
    NPU0_DIV_CLK_NPU0_NOCP,
    NPUS_DIV_CLK_NPUS_NOCP,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_usi[] = {
    PERI_DIV_CLK_PERI_USI00_USI,
    PERI_MUX_CLK_PERI_USI00,
};
enum clk_id cmucal_vclk_div_clk_peri_usi01_usi[] = {
    PERI_DIV_CLK_PERI_USI01_USI,
    PERI_MUX_CLK_PERI_USI01,
};
enum clk_id cmucal_vclk_div_clk_peri_usi02_usi[] = {
    PERI_DIV_CLK_PERI_USI02_USI,
    PERI_MUX_CLK_PERI_USI02,
};
enum clk_id cmucal_vclk_div_clk_peri_usi03_usi[] = {
    PERI_DIV_CLK_PERI_USI03_USI,
    PERI_MUX_CLK_PERI_USI03,
};
enum clk_id cmucal_vclk_div_clk_peri_usi04_usi[] = {
    PERI_DIV_CLK_PERI_USI04_USI,
    PERI_MUX_CLK_PERI_USI04,
};
enum clk_id cmucal_vclk_div_clk_peri_usi05_usi[] = {
    PERI_DIV_CLK_PERI_USI05_USI_OIS,
    PERI_MUX_CLK_PERI_USI05_USI_OIS,
};
enum clk_id cmucal_vclk_div_clk_peri_usi06_usi[] = {
    PERI_DIV_CLK_PERI_USI06_USI_OIS,
    PERI_MUX_CLK_PERI_USI06_USI_OIS,
};

/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_div_clk_hsi_ufs_embd[] = {
	TOP_MUX_CLKCMU_HSI_UFS_EMBD,
	TOP_DIV_CLKCMU_HSI_UFS_EMBD,
};

/* GATE VCLK -> Clock Node List */

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_div_clk_hsi_ufs_embd_lut[] = {
	{177600000, div_clk_hsi_ufs_embd_1776_params},
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_VDDI_lut[] = {
    { 800000, cmucal_VDDI_NM_lut_params },
    { 400000, cmucal_VDDI_UD_lut_params },
    { 200000, cmucal_VDDI_SUD_lut_params },
    { 160000, cmucal_VDDI_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_ALIVE_lut[] = {
    { 52000, cmucal_VDD_ALIVE_SOD_lut_params },
    { 52000, cmucal_VDD_ALIVE_OD_lut_params },
    { 52000, cmucal_VDD_ALIVE_NM_lut_params },
    { 52000, cmucal_VDD_ALIVE_UD_lut_params },
    { 52000, cmucal_VDD_ALIVE_SUD_lut_params },
    { 52000, cmucal_VDD_ALIVE_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CAM_lut[] = {
    { 167000, cmucal_VDD_CAM_SOD_lut_params },
    { 167000, cmucal_VDD_CAM_OD_lut_params },
    { 167000, cmucal_VDD_CAM_NM_lut_params },
    { 167000, cmucal_VDD_CAM_UD_lut_params },
    { 167000, cmucal_VDD_CAM_SUD_lut_params },
    { 167000, cmucal_VDD_CAM_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CHUB_lut[] = {
    { 52000, cmucal_VDD_CHUB_SOD_lut_params },
    { 52000, cmucal_VDD_CHUB_OD_lut_params },
    { 52000, cmucal_VDD_CHUB_NM_lut_params },
    { 52000, cmucal_VDD_CHUB_UD_lut_params },
    { 52000, cmucal_VDD_CHUB_SUD_lut_params },
    { 52000, cmucal_VDD_CHUB_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_ICPU_CSILINK_lut[] = {
    { 1334000, cmucal_VDD_ICPU_CSILINK_SOD_lut_params },
    { 1334000, cmucal_VDD_ICPU_CSILINK_OD_lut_params },
    { 1334000, cmucal_VDD_ICPU_CSILINK_NM_lut_params },
    { 1334000, cmucal_VDD_ICPU_CSILINK_UD_lut_params },
    { 1334000, cmucal_VDD_ICPU_CSILINK_SUD_lut_params },
    { 1334000, cmucal_VDD_ICPU_CSILINK_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL0_lut[] = {
    { 2000000, cmucal_VDD_CPUCL0_SOD_lut_params },
    { 1700000, cmucal_VDD_CPUCL0_OD_lut_params },
    { 1500000, cmucal_VDD_CPUCL0_NM_lut_params },
    { 1000000, cmucal_VDD_CPUCL0_UD_lut_params },
    { 450000, cmucal_VDD_CPUCL0_SUD_lut_params },
    { 200000, cmucal_VDD_CPUCL0_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_G3D_lut[] = {
    { 951000, cmucal_VDD_G3D_SOD_lut_params },
    { 951000, cmucal_VDD_G3D_OD_lut_params },
    { 951000, cmucal_VDD_G3D_NM_lut_params },
    { 751000, cmucal_VDD_G3D_UD_lut_params },
    { 551000, cmucal_VDD_G3D_SUD_lut_params },
    { 221000, cmucal_VDD_G3D_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_NPU_lut[] = {
    { 52000, cmucal_VDD_NPU_SOD_lut_params },
    { 52000, cmucal_VDD_NPU_OD_lut_params },
    { 52000, cmucal_VDD_NPU_NM_lut_params },
    { 52000, cmucal_VDD_NPU_UD_lut_params },
    { 52000, cmucal_VDD_NPU_SUD_lut_params },
    { 52000, cmucal_VDD_NPU_UUD_lut_params },
};
struct vclk_lut cmucal_vclk_div_clk_peri_usixx_usi_lut[] = {
	{400000, div_clk_peri_400_lut_params},
	{200000, div_clk_peri_200_lut_params},
	{133000, div_clk_peri_133_lut_params},
	{100000, div_clk_peri_100_lut_params},
	{80000, div_clk_peri_80_lut_params},
	{66000, div_clk_peri_66_lut_params},
	{57000, div_clk_peri_57_lut_params},
	{50000, div_clk_peri_50_lut_params},
	{44000, div_clk_peri_44_lut_params},
	{40000, div_clk_peri_40_lut_params},
	{36000, div_clk_peri_36_lut_params},
	{33000, div_clk_peri_33_lut_params},
	{30000, div_clk_peri_30_lut_params},
	{28000, div_clk_peri_28_lut_params},
	{26000, div_clk_peri_26_lut_params},
	{25000, div_clk_peri_25_lut_params},
	{13000, div_clk_peri_13_lut_params},
	{8600, div_clk_peri_8_lut_params},
	{6500, div_clk_peri_6_lut_params},
	{5200, div_clk_peri_5_lut_params},
	{4300, div_clk_peri_4_lut_params},
	{3700, div_clk_peri_3_lut_params},
};

/* Switch VCLK -> LUT Parameter List */

/*================================ SWPLL List =================================*/

/*================================ VCLK List =================================*/
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
    CMUCAL_VCLK(VCLK_VDDI, cmucal_VDDI_lut, cmucal_VDDI, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_ALIVE, cmucal_VDD_ALIVE_lut, cmucal_VDD_ALIVE, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_VDD_CAM_lut, cmucal_VDD_CAM, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CHUB, cmucal_VDD_CHUB_lut, cmucal_VDD_CHUB, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_ICPU_CSILINK, cmucal_VDD_ICPU_CSILINK_lut, cmucal_VDD_ICPU_CSILINK, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_VDD_CPUCL0_lut, cmucal_VDD_CPUCL0, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_G3D, cmucal_VDD_G3D_lut, cmucal_VDD_G3D, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_NPU, cmucal_VDD_NPU_lut, cmucal_VDD_NPU, NULL, NULL),

	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI00_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI01_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI02_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI03_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI04_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI05_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI06_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi06_usi, NULL, NULL),
/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_HSI_UFS_EMBD, cmucal_vclk_div_clk_hsi_ufs_embd_lut, cmucal_vclk_div_clk_hsi_ufs_embd, NULL, NULL),

/* GATE VCLK*/
};
unsigned int cmucal_vclk_size = 8;
