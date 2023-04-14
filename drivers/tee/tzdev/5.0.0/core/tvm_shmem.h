/*
 * Copyright (C) 2022, Samsung Electronics Co., Ltd.
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

#ifndef __TZ_TVM_SHMEM_H__
#define __TZ_TVM_SHMEM_H__

#include <linux/mm.h>

struct tvm_shmem_handles {
	uint32_t count;
	uint32_t *ids;
};

#ifdef CONFIG_TZDEV_TVM
int tvm_shmem_share_batch(struct page **pages, unsigned int num_pages, uint32_t *mem_handle);
int tvm_shmem_release_batch(uint32_t mem_handle);

int tvm_shmem_share(struct page **pages, unsigned int num_pages, struct tvm_shmem_handles *handles);
int tvm_shmem_release(struct tvm_shmem_handles *handles);

void tvm_handle_accepted(uint32_t mem_handle);
void tvm_handle_released(uint32_t mem_handle);
#else
static int tvm_shmem_share_batch(struct page **pages, unsigned int num_pages, uint32_t *mem_handle)
{
	return 0;
}

static int tvm_shmem_release_batch(uint32_t mem_handle)
{
	return 0;
}

static int tvm_shmem_share(struct page **pages, unsigned int num_pages, struct tvm_shmem_handles *handles)
{
	return 0;
}
static int tvm_shmem_release(struct tvm_shmem_handles *handles)
{
	return 0;
}

static void tvm_handle_accepted(uint32_t mem_handle)
{
}

static void tvm_handle_released(uint32_t mem_handle)
{
}
#endif

#endif /* __TZ_TVM_SHMEM_H__ */
