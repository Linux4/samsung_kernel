/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _AOLTEST_V2_CORE_H_
#define _AOLTEST_V2_CORE_H_

#include <linux/types.h>
#include <linux/compiler.h>

enum aoltestv2_module {
	AOLTESTV2_MODULE_EM,
	AOLTESTV2_MODULE_BLE,
	AOLTESTV2_MODULE_SIZE
};

int aoltestv2_core_init(void);
void aoltestv2_core_deinit(void);

#endif // _AOLTEST_V2_CORE_H_
