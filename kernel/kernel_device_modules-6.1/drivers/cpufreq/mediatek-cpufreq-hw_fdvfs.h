/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2023 MediaTek Inc.
 */

#define FDVFS_MAGICNUM	0xABCD0001
#define FDVFS_FREQU	26000

enum FDVFS_REG_TYPE {
	FDVFS_REG,
	FDVFS_CCI_REG,
	FDVFS_SUPPORT,
};

bool is_fdvfs_support(void);
int check_fdvfs_support(void);
int cpufreq_fdvfs_switch(unsigned int target_f, struct cpufreq_policy *policy);
int cpufreq_fdvfs_cci_switch(unsigned int target_f);
