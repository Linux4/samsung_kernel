// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * Device Tree binding constants for Exynos GNSS interface
 */

#ifndef _DT_BINDINGS_EXYNOS_GNSS_H
#define _DT_BINDINGS_EXYNOS_GNSS_H

#define INIT_DONE		0x80000000
#define ACCESS_ENABLE_AW	0x00000002
#define ACCESS_ENABLE_AR	0x00000001
#define ID_AW_AR		(INIT_DONE | ACCESS_ENABLE_AW | ACCESS_ENABLE_AR)

#define USE_SHMEM		1
/* SHMEM_SIZE - 1 */
#define USE_SHMEM_ADDR_IDX	2

#endif /* _DT_BINDINGS_EXYNOS_GNSS_H */
