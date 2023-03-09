#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_VDDI[] = {
    MUX_CLKCMU_NOCL1A_NOC,
    DIV_CLKCMU_NOCL1A_NOC,
};
enum clk_id cmucal_VDD_ALIVE[] = {
    DIV_CLK_ALIVE_NOC,
    DIV_CLK_ALIVE_SPMI,
    DIV_CLK_PMU_SUB,
    DIV_CLK_ALIVE_I2C0,
};
enum clk_id cmucal_VDD_CHUB[] = {
    DIV_CLK_CHUB_USI1,
    DIV_CLK_CHUB_USI0,
    DIV_CLK_CHUB_NOC,
    DIV_CLK_CHUB_USI2,
    DIV_CLK_CHUB_ECU,
};
enum clk_id cmucal_VDD_CPUCL0[] = {
    PLL_CPUCL0,
    PLL_DSU,
    DIV_CLK_DSU_SHORTSTOP,
    DIV_CLK_CLUSTER_ACLK,
};
enum clk_id cmucal_VDD_CPUCL1[] = {
    PLL_CPUCL1,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_usi[] = {
    DIV_CLK_PERI_USI00_USI,
    MUX_CLK_PERI_USI00,
};
enum clk_id cmucal_vclk_div_clk_peri_usi01_usi[] = {
    DIV_CLK_PERI_USI01_USI,
    MUX_CLK_PERI_USI01,
};
enum clk_id cmucal_vclk_div_clk_peri_usi02_usi[] = {
    DIV_CLK_PERI_USI02_USI,
    MUX_CLK_PERI_USI02,
};
enum clk_id cmucal_vclk_div_clk_peri_usi03_usi[] = {
    DIV_CLK_PERI_USI03_USI,
    MUX_CLK_PERI_USI03,
};
enum clk_id cmucal_vclk_div_clk_peri_usi04_usi[] = {
    DIV_CLK_PERI_USI04_USI,
    MUX_CLK_PERI_USI04,
};
enum clk_id cmucal_vclk_div_clk_peri_usi05_usi[] = {
    DIV_CLK_PERI_USI05_USI,
    MUX_CLK_PERI_USI05,
};
enum clk_id cmucal_vclk_div_clk_peri_usi06_usi[] = {
    DIV_CLK_PERI_USI06_USI,
    MUX_CLK_PERI_USI06,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_div_clk_hsi_ufs_embd[] = {
	DIV_CLKCMU_HSI_UFS_EMBD,
	MUX_CLKCMU_HSI_UFS_EMBD,
};
/* GATE VCLK -> Clock Node List */
/* SPECIAL VCLK -> LUT List */

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_VDDI_lut[] = {
    { 800000, cmucal_VDDI_NM_lut_params },
    { 400000, cmucal_VDDI_UD_lut_params },
    { 200000, cmucal_VDDI_SUD_lut_params },
    { 160000, cmucal_VDDI_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_ALIVE_lut[] = {
    { 430000, cmucal_VDD_ALIVE_SOD_lut_params },
    { 430000, cmucal_VDD_ALIVE_OD_lut_params },
    { 430000, cmucal_VDD_ALIVE_NM_lut_params },
    { 430000, cmucal_VDD_ALIVE_UD_lut_params },
    { 430000, cmucal_VDD_ALIVE_SUD_lut_params },
    { 430000, cmucal_VDD_ALIVE_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CHUB_lut[] = {
    { 430000, cmucal_VDD_CHUB_SOD_lut_params },
    { 430000, cmucal_VDD_CHUB_OD_lut_params },
    { 430000, cmucal_VDD_CHUB_NM_lut_params },
    { 430000, cmucal_VDD_CHUB_UD_lut_params },
    { 430000, cmucal_VDD_CHUB_SUD_lut_params },
    { 430000, cmucal_VDD_CHUB_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL0_lut[] = {
    { 52000, cmucal_VDD_CPUCL0_SOD_lut_params },
    { 52000, cmucal_VDD_CPUCL0_OD_lut_params },
    { 52000, cmucal_VDD_CPUCL0_NM_lut_params },
    { 52000, cmucal_VDD_CPUCL0_UD_lut_params },
    { 52000, cmucal_VDD_CPUCL0_SUD_lut_params },
    { 52000, cmucal_VDD_CPUCL0_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL1_lut[] = {
    { 2500000, cmucal_VDD_CPUCL1_SOD_lut_params },
    { 2000000, cmucal_VDD_CPUCL1_OD_lut_params },
    { 1700000, cmucal_VDD_CPUCL1_NM_lut_params },
    { 1100000, cmucal_VDD_CPUCL1_UD_lut_params },
    { 500000, cmucal_VDD_CPUCL1_SUD_lut_params },
    { 500000, cmucal_VDD_CPUCL1_UUD_lut_params },
};
struct vclk_lut cmucal_vclk_div_clk_hsi_ufs_embd_lut[] = {
	{114200000, div_clk_hsi_ufs_embd_1142_params},
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
    CMUCAL_VCLK(VCLK_VDD_CHUB, cmucal_VDD_CHUB_lut, cmucal_VDD_CHUB, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_VDD_CPUCL0_lut, cmucal_VDD_CPUCL0, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_VDD_CPUCL1_lut, cmucal_VDD_CPUCL1, NULL, NULL),

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
unsigned int cmucal_vclk_size = 5;
