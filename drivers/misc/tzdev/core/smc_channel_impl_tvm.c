/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>

#include "tzdev_internal.h"
#include "core/log.h"
#include "core/smc_channel_impl.h"
#include "core/tvm_shmem.h"

struct impl_data {
	struct tvm_shmem_handles p_handles; /* persistent */
	struct tvm_shmem_handles np_handles;
};

struct tzdev_smc_channel_metadata {
	uint32_t write_offset;
	uint32_t handles_count;
	uint32_t handles_ids[];
} __packed;

static int init(struct tzdev_smc_channel_metadata *metadata,
		struct page **pages, size_t pages_count, void *impl_data)
{
	int ret;
	uint32_t i;
	struct impl_data *data = impl_data;

	ret = tvm_shmem_share(pages, pages_count, &data->p_handles);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to share mem, error=%d\n", ret);
		return ret;
	}

	metadata->handles_count = data->p_handles.count;
	for (i = 0; i < data->p_handles.count; i++) {
		metadata->handles_ids[i] = data->p_handles.ids[i];
	}

	return 0;
}

static int init_swd(struct page *meta_pages)
{
	uint32_t mem_handle;
	int ret;
	int rel_ret = 0;

	ret = tvm_shmem_share_batch(&meta_pages, NR_SW_CPU_IDS, &mem_handle);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to share mem, error=%d\n", ret);
		return ret;
	}

	ret = tzdev_smc_channels_init(mem_handle, NR_SW_CPU_IDS);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to init smc channels (error %d)", ret);
		rel_ret = tvm_shmem_release_batch(mem_handle);
	}

	return (rel_ret == -EADDRNOTAVAIL) ? rel_ret : ret;
}

static int deinit(void *impl_data)
{
	struct impl_data *data = impl_data;

	return tvm_shmem_release(&data->p_handles);
}

static void acquire(struct tzdev_smc_channel_metadata *metadata)
{
	metadata->write_offset = 0;
	metadata->handles_count = 0;
}

static int reserve(struct tzdev_smc_channel_metadata *metadata, struct page **pages,
		size_t old_pages_count, size_t new_pages_count, void *impl_data)
{
	int ret;
	uint32_t i;
	struct impl_data *data = impl_data;

	ret = tvm_shmem_share(pages, new_pages_count - old_pages_count, &data->np_handles);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to share mem, error=%d\n", ret);
		return ret;
	}

	metadata->handles_count = data->np_handles.count;
	for (i = 0; i < data->np_handles.count; i++) {
		metadata->handles_ids[i] = data->np_handles.ids[i];
	}

	return 0;
}

static int release(struct tzdev_smc_channel_metadata *metadata, void *impl_data)
{
	struct impl_data *data = impl_data;

	metadata->handles_count = 0;
	data->np_handles.count = 0;

	return tvm_shmem_release(&data->np_handles);
}

static void set_write_offset(struct tzdev_smc_channel_metadata *metadata, uint32_t value)
{
	metadata->write_offset = value;
}

static uint32_t get_write_offset(struct tzdev_smc_channel_metadata *metadata)
{
	return metadata->write_offset;
}

static struct tzdev_smc_channel_impl tzdev_smc_channel_impl = {
	.data_size = sizeof(struct impl_data),
	.init = init,
	.init_swd = init_swd,
	.deinit = deinit,
	.acquire = acquire,
	.reserve = reserve,
	.release = release,
	.set_write_offset = set_write_offset,
	.get_write_offset = get_write_offset,
};

struct tzdev_smc_channel_impl *tzdev_get_smc_channel_impl(void)
{
	return &tzdev_smc_channel_impl;
}
