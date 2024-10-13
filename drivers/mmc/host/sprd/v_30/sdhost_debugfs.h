/*
 *  linux/drivers/mmc/host/sdhost_debugfs.h - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2014-2014 Jason.Wu(Jishuang.Wu), All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#ifndef _SDHOST_DEBUGFS_H_
#define _SDHOST_DEBUGFS_H_

#ifdef CONFIG_DEBUG_FS

#include "sdhost_v30.h"

extern void sdhost_add_debugfs(struct sdhost_host *host);
extern void dumpSDIOReg(struct sdhost_host *host);
extern void get_ddr_info(struct device *dev);
#else
static inline void sdhost_add_debugfs(struct sdhost_host *host)
{
}

static inline void dumpSDIOReg(struct sdhost_host *host)
{
}

static inline void get_ddr_info(struct device *dev)
{
}
#endif /* CONFIG_DEBUG_FS */

#endif /* _SDHOST_DEBUGFS_H_ */
