/*
 * Copyright (c) 2012 Samsung Electronics Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_MODEMIF_H__
#define __EXYNOS_MODEMIF_H__

unsigned long shm_get_phys_base(void);
unsigned long shm_get_phys_size(void);
unsigned long shm_get_ipc_rgn_offset(void);
unsigned long shm_get_ipc_rgn_size(void);

#endif
