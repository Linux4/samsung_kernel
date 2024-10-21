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

#include <linux/sort.h>

#include "tzdev_internal.h"
#include "core/iw_mem_impl.h"
#include "core/log.h"
#include "core/tvm_shmem.h"

static int compare_phys(const void *lhs, const void *rhs)
{
	phys_addr_t lhs_phys = page_to_phys(*(struct page **)(lhs));
	phys_addr_t rhs_phys = page_to_phys(*(struct page **)(rhs));

	if (lhs_phys < rhs_phys)
		return -1;
	if (lhs_phys > rhs_phys)
		return 1;

	return 0;
}

static int init(struct page **pages, size_t pages_count, void *impl_data, int optimize)
{
	unsigned int i;
	struct tvm_shmem_handles *handles = impl_data;

	if (!handles || !pages_count || !pages)
		return -EINVAL;

	for (i = 0; i < pages_count; i++)
		if (!pages[i])
			return -EINVAL;

	/* Optimization to minimize the count of contiguous chunks */
	if (optimize)
		sort(pages, pages_count, sizeof(struct page *), compare_phys, NULL);

	return tvm_shmem_share(pages, pages_count, handles);
}

static int deinit(void *impl_data)
{
	struct tvm_shmem_handles *handles = impl_data;

	if (!handles)
		return -EINVAL;

	return tvm_shmem_release(handles);
}

static int pack(struct tzdev_smc_channel *channel, struct page **pages, size_t pages_count,
		void *impl_data)
{
	int ret;
	uint32_t ids_size;
	struct tvm_shmem_handles *handles = impl_data;

	if (!handles)
		return -EINVAL;

	ids_size = handles->count * sizeof(uint32_t);

	ret = tzdev_smc_channel_reserve(channel, sizeof(uint32_t) + ids_size);
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to reserve %zu bytes (error %d)\n",
				sizeof(uint32_t) + ids_size, ret);
		return ret;
	}

	ret = tzdev_smc_channel_write(channel, &handles->count, sizeof(uint32_t));
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to write mem handles count (error %d)\n", ret);
		return ret;
	}

	ret = tzdev_smc_channel_write(channel, handles->ids, ids_size);
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to write mem handles array (error %d)\n", ret);
		return ret;
	}

	return 0;
}

static struct tzdev_iw_mem_impl impl = {
	.data_size = sizeof(struct tvm_shmem_handles),
	.init = init,
	.pack = pack,
	.deinit = deinit,
};

const struct tzdev_iw_mem_impl *tzdev_get_iw_mem_impl(void)
{
	return &impl;
}
