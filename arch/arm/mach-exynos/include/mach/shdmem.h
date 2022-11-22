/*
 * Copyright (c) 2012 Samsung Electronics Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_SHARED_MEMORY_H__
#define __EXYNOS_SHARED_MEMORY_H__

#include <mach/mcu_ipc.h>

struct shdmem_info {
	unsigned int base;
	unsigned int size;
};

unsigned long shm_get_phys_base(void);
unsigned long shm_get_phys_size(void);

unsigned long shm_get_boot_rgn_offset(void);
unsigned long shm_get_boot_rgn_size(void);

unsigned long shm_get_ipc_rgn_offset(void);
unsigned long shm_get_ipc_rgn_size(void);

void __iomem *shm_request_region(unsigned long phys_addr, unsigned long size);
void shm_release_region(void *rgn);

#endif
