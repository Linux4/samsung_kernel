#ifndef __CMUCAL_VCLK_H__
#define __CMUCAL_VCLK_H__

#include "../../cmucal.h"

enum vclk_id {

/* DVFS VCLK*/
    VCLK_VDDI = DFS_VCLK_TYPE,
    VCLK_VDD_ALIVE,
    VCLK_VDD_MIF,
    VCLK_VDD_CAM,
    VCLK_VDD_CHUBVTS,
    VCLK_VDD_NPU,
    VCLK_VDD_G3D,
    VCLK_VDD_CPUCL0_LIT,
    VCLK_VDD_CPUCL1_BIG,
    end_of_dfs_vclk,
	num_of_dfs_vclk = end_of_dfs_vclk - DFS_VCLK_TYPE,

/* SPECIAL VCLK*/
    VCLK_DIV_CLKCMU_HSI_UFS_EMBD = (MASK_OF_ID & end_of_dfs_vclk) | COMMON_VCLK_TYPE,
    VCLK_DIV_CMGP_USI0,
	VCLK_DIV_CMGP_USI1,
	VCLK_DIV_CMGP_USI2,
	VCLK_DIV_CMGP_USI3,
	VCLK_DIV_CMGP_USI4,
    VCLK_DIV_CMGP_USI_I2c,
    VCLK_DIV_CMGP_USI_I3c,
    VCLK_DIV_PERIC_USI00_USI,
    VCLK_DIV_PERIC_USI01_USI,
    VCLK_DIV_PERIC_USI02_USI,
    VCLK_DIV_PERIC_USI03_USI,
    VCLK_DIV_PERIC_USI04_USI,
    VCLK_DIV_PERIC_USI09_USI_OIS,
    VCLK_DIV_PERIC_USI10_USI_OIS,
    VCLK_DIV_PERIC_USI_I2C,
    VCLK_DIV_USI_USI06_USI,
    VCLK_DIV_USI_USI07_USI,
    VCLK_DIV_USI_USI_I2C,
	end_of_common_vclk,
	num_of_common_vclk = end_of_common_vclk - ((MASK_OF_ID & end_of_dfs_vclk) | COMMON_VCLK_TYPE),

/* GATE VCLK*/
	end_of_gate_vclk,
	num_of_gate_vclk = end_of_gate_vclk - ((MASK_OF_ID & end_of_common_vclk) | GATE_VCLK_TYPE),
};
#endif
