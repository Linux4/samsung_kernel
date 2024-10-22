/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef TRUSTONIC_FFA_API_H
#define TRUSTONIC_FFA_API_H

#include <linux/version.h> /* KERNEL_VERSION */
#include <linux/kconfig.h> /* IS_REACHABLE */

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#if IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT)

#include <linux/arm_ffa.h>

int tt_ffa_memory_lend(struct ffa_mem_ops_args *args);
int tt_ffa_memory_reclaim(u64 handle, u64 tag);

#endif /* CONFIG_ARM_FFA_TRANSPORT */
#endif /* KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE */

#endif /* TRUSTONIC_FFA_API_H */
