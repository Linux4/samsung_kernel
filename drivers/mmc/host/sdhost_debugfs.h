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

#include "sdhost.h"

void sdhost_add_debugfs(struct sdhost_host *host);
void dumpSDIOReg(struct sdhost_host *host);

#endif
