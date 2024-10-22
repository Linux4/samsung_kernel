/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_MEMORY_H
#define MBRAINK_MEMORY_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "mbraink_ioctl_struct_def.h"
int mbraink_memory_getDdrInfo(struct mbraink_memory_ddrInfo *pMemoryDdrInfo);
int mbraink_memory_getMdvInfo(struct mbraink_memory_mdvInfo *pMemoryMdv);

#endif /*end of MBRAINK_MEMORY_H*/

