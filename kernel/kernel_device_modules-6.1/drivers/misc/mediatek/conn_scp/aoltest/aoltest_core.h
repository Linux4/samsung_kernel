/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _AOLTEST_CORE_H_
#define _AOLTEST_CORE_H_

#include <linux/types.h>
#include <linux/compiler.h>

int aoltest_core_init(void);
void aoltest_core_deinit(void);

int aoltest_core_send_dbg_msg(uint32_t param0, uint32_t param1);

#endif // _AOLTEST_CORE_H_
