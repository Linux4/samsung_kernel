/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __MIF_REG_PCIE_REDWOOD_H
#define __MIF_REG_PCIE_REDWOOD_H

#define PMU_BOOT_RAM_START_ADDR		0

/* PERT Registers */
#define PMU_SECOND_STAGE_BOOT_ADDR	0x3e8
#define PMU_INTERRUPT_FROM_HOST_INTGR	0x3ec
//#define PMU_INTERRUPT_FROM_HOST_INTMS	0x3fc // unused
#define WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET 0x400
#define WPAN_INTERRUPT_FROM_HOST_INTGR	0x46c
#define PMU_WLAN_MCPU0_RMP_BOOT_ADDR	0x48c
#define PMU_WLAN_MCPU1_RMP_BOOT_ADDR	0x490
#define PMU_WPAN_RMP_BOOT_ADDR 		0x494

#define SECOND_STAGE_BOOT_ADDR		0x60000200
#define DRAM_BASE_ADDR			0x90000000

#endif /* ___MIF_REG_PCIE_REDWOOD_H */
