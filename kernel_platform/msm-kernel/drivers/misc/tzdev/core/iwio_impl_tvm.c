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

#include "tzdev_internal.h"
#include "core/iwio.h"
#include "core/iwio_impl.h"
#include "core/log.h"
#include "core/tvm_shmem.h"

static int connect_aux_channel(struct page *page)
{
	int ret;
	int rel_ret = 0;
	uint32_t mem_handle;

	ret = tvm_shmem_share_batch(&page, 1, &mem_handle);
	if (ret) {
		log_error(tzdev_iwio, "Failed to share mem, error=%d\n", ret);
		return ret;
	}

	/* Memory handle is used instead of PFN */
	ret = tzdev_smc_connect_aux(mem_handle);
	if (ret)
		rel_ret = tvm_shmem_release_batch(mem_handle);

	return (rel_ret == -EADDRNOTAVAIL) ? rel_ret : ret;
}

static int connect(unsigned int mode, struct page **pages, unsigned int num_pages, void *impl_data)
{
	int ret;
	int rel_ret = 0;
	unsigned int i;
	struct tz_iwio_aux_channel *aux_ch;
	struct tvm_shmem_handles *handles = impl_data;

	log_debug(tzdev_platform, "prepare to tvm_mem_share\n");
	ret = tvm_shmem_share(pages, num_pages, handles);
	if (ret) {
		log_error(tzdev_iwio, "Failed to share mem, error=%d\n", ret);
		return ret;
	}

	for (i = 0; i < handles->count; i++)
		log_debug(tzdev_iwio, "mem_handles[%d]: %u\n", i, handles->ids[i]);

	/* Push mem handles list into aux channel */
	aux_ch = tz_iwio_get_aux_channel();

	memcpy(aux_ch->buffer, &handles->count, sizeof(uint32_t));
	memcpy(aux_ch->buffer + sizeof(uint32_t), handles->ids,
			handles->count * sizeof(uint32_t));

	ret = tzdev_smc_connect(mode, num_pages);
	if (ret)
		rel_ret = tvm_shmem_release(handles);

	tz_iwio_put_aux_channel();

	return (rel_ret == -EADDRNOTAVAIL) ? rel_ret : ret;
}


static int deinit(void *impl_data)
{
	struct tvm_shmem_handles *handles = impl_data;

	return tvm_shmem_release(handles);
}

static const struct tzdev_iwio_impl impl = {
	.data_size = sizeof(struct tvm_shmem_handles),
	.connect_aux_channel = connect_aux_channel,
	.connect = connect,
	.deinit = deinit,
};

const struct tzdev_iwio_impl *tzdev_get_iwio_impl(void)
{
	return &impl;
}
