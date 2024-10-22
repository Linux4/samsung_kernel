/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUEB_COMMON_H__
#define __GPUEB_COMMON_H__

/**************************************************
 * Definition
 **************************************************/
#define SRAM_GPR_SIZE_4B                    (0x4) /* 4 Bytes */

/**************************************************
 * Enumeration
 **************************************************/
enum gpueb_sram_gpr_id {
	GPUEB_SRAM_GPR0  = 0,
	GPUEB_SRAM_GPR1  = 1,
	GPUEB_SRAM_GPR2  = 2,
	GPUEB_SRAM_GPR3  = 3,
	GPUEB_SRAM_GPR4  = 4,
	GPUEB_SRAM_GPR5  = 5,
	GPUEB_SRAM_GPR6  = 6,
	GPUEB_SRAM_GPR7  = 7,
	GPUEB_SRAM_GPR8  = 8,
	GPUEB_SRAM_GPR9  = 9,
	GPUEB_SRAM_GPR10 = 10,
	GPUEB_SRAM_GPR11 = 11,
	GPUEB_SRAM_GPR12 = 12,
	GPUEB_SRAM_GPR13 = 13,
	GPUEB_SRAM_GPR14 = 14,
	GPUEB_SRAM_GPR15 = 15,
	GPUEB_SRAM_GPR16 = 16,
	GPUEB_SRAM_GPR17 = 17,
	GPUEB_SRAM_GPR18 = 18,
	GPUEB_SRAM_GPR19 = 19,
	GPUEB_SRAM_GPR20 = 20,
	GPUEB_SRAM_GPR21 = 21,
	GPUEB_SRAM_GPR22 = 22,
	GPUEB_SRAM_GPR23 = 23,
	GPUEB_SRAM_GPR24 = 24,
};

/**************************************************
 * Function
 **************************************************/
void __iomem *gpueb_get_gpr_addr(enum gpueb_sram_gpr_id gpr_id);

#endif /* __GPUEB_COMMON_H__ */
