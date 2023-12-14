/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#include <linux/types.h>

/* SWd can write into shmem(== TEEC_MEM_OUTPUT) */
#define TZDEV_IW_SHMEM_FLAG_WRITE		(1 << 0)

/* Release function should wait until shmem is released */
#define TZDEV_IW_SHMEM_FLAG_SYNC		(1 << 1)

int tzdev_iw_shmem_register(void **ptr, size_t size, unsigned int flags);
int tzdev_iw_shmem_register_exist(void *ptr, size_t size, unsigned int flags);
int tzdev_iw_shmem_register_user(size_t size, unsigned int flags);
int tzdev_iw_shmem_release(unsigned int id);

int tzdev_iw_shmem_map_user(unsigned int id, struct vm_area_struct *vma);
