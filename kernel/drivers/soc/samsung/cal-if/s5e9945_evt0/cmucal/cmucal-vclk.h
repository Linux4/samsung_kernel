#ifndef __CMUCAL_VCLK_H__
#define __CMUCAL_VCLK_H__

#include "../../cmucal.h"

enum vclk_id {
	end_of_dfs_vclk,
	num_of_dfs_vclk = end_of_dfs_vclk - DFS_VCLK_TYPE,

/* SPECIAL VCLK*/
	VCLK_CMGP_USI0 = (MASK_OF_ID & end_of_dfs_vclk) | COMMON_VCLK_TYPE,
	VCLK_CMGP_USI1,
	VCLK_CMGP_USI2,
	VCLK_CMGP_USI3,
	VCLK_CMGP_USI4,
	VCLK_CMGP_USI5,
	VCLK_CMGP_USI6,
	VCLK_CMGP_SPI_I2C0,
	VCLK_CMGP_SPI_I2C1,
	VCLK_PERIC0_USI04,
	VCLK_PERIC1_USI07,
	VCLK_PERIC1_USI08,
	VCLK_PERIC1_USI09,
	VCLK_PERIC1_USI10,
	VCLK_PERIC1_USI07_SPI_I2C,
	VCLK_PERIC1_USI08_SPI_I2C,
	VCLK_PERIC2_USI00,
	VCLK_PERIC2_USI01,
	VCLK_PERIC2_USI02,
	VCLK_PERIC2_USI03,
	VCLK_PERIC2_USI05,
	VCLK_PERIC2_USI06,
	VCLK_PERIC2_USI11,
	VCLK_PERIC2_USI00_SPI_I2C,
	VCLK_PERIC2_USI01_SPI_I2C,
	end_of_common_vclk,
	num_of_common_vclk = end_of_common_vclk - ((MASK_OF_ID & end_of_dfs_vclk) | COMMON_VCLK_TYPE),

	end_of_gate_vclk,
	num_of_gate_vclk = end_of_gate_vclk - ((MASK_OF_ID & end_of_common_vclk) | GATE_VCLK_TYPE),

};
#endif
