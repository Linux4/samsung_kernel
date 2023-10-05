/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SMC_H
#define PABLO_SMC_H

#include <linux/version.h>

#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos-smc.h>
#else
#include <linux/smc.h>
#endif
#else
/* dummy define */
#define SMC_CMD_PREAPRE_PD_ONOFF		0x82000410
#define SMC_SECCAM_SETENV			0x82002130
#define SMC_SECCAM_INIT				0x82002131
#define SMC_SECCAM_INIT_NSBUF			0x82002134
#define SMC_SECCAM_SYSREG_PROT			0x82002132
#define SMC_SECCAM_PREPARE			0x82002135
#define SMC_SECCAM_UNPREPARE			0x82002136
#define SMC_SECCAM_GETSTATUS			0x82002137
#define SMC_CMD_REG				(-101)
#define SMC_REG_CLASS_SFR_W			(0x1 << 30)
#define SMC_REG_CLASS_SFR_R			(0x3 << 30)
#define SMC_REG_ID_SFR_W(ADDR)			(SMC_REG_CLASS_SFR_W | ((ADDR) >> 2))
#define SMC_REG_ID_SFR_R(ADDR)			(SMC_REG_CLASS_SFR_R | ((ADDR) >> 2))
#endif

#define CALL_PSMC_OPS(ops, op, args...) \
	((ops && ops->op) ? ops->op(args) : 0)

struct pablo_smc_ops {
	unsigned long (*call)(unsigned long cmd, unsigned long arg0,
			unsigned long arg1, unsigned long arg2);
};

struct pablo_smc_ops *pablo_get_smc_ops(void);
#endif
