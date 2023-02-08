/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Header for EXYNOS machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_EXYNOS_DEV_MCU_IPC_H__
#define __ARCH_ARM_MACH_EXYNOS_DEV_MCU_IPC_H__

enum mcu_ipc_int {
	MCU_IPC_INT0 = 0,
	MCU_IPC_INT1,
	MCU_IPC_INT2,
	MCU_IPC_INT3,
	MCU_IPC_INT4,
	MCU_IPC_INT5,
	MCU_IPC_INT6,
	MCU_IPC_INT7,
	MCU_IPC_INT8,
	MCU_IPC_INT9,
	MCU_IPC_INT10,
	MCU_IPC_INT11,
	MCU_IPC_INT12,
	MCU_IPC_INT13,
	MCU_IPC_INT14,
	MCU_IPC_INT15,
	MAX_MCU_IPC_INT
};

int init_exynos_dev_mcu_ipc(void);

#endif
