// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc sipa driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Qingsheng Li <qingsheng.li@unisoc.com>
 */

#ifndef __SIPA_ROC1_LINUX_DEBUG_H__
#define __SIPA_ROC1_LINUX_DEBUG_H__

#include "sipa_priv.h"

void sipa_dbg(struct sipa_plat_drv_cfg *sipa, const char *fmt, ...);
#ifdef CONFIG_DEBUG_FS
int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa);
void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa);
#else
static inline int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa)
{
	return 0;
}

static inline void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa)
{
}
#endif

#endif /*  __SIPA_ROC1_LINUX_DEBUG_H__ */
