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
#include "core/smc_channel_impl.h"

struct tzdev_smc_channel_metadata {
	uint32_t write_offset;
	uint32_t pfns_count;
	sk_pfn_t pfns[];
} __packed;

static int init(struct tzdev_smc_channel_metadata *metadata,
		struct page **pages, size_t pages_count, void *impl_data)
{
	size_t i;

	(void)impl_data;

	for (i = 0; i < pages_count; i++) {
		metadata->pfns[i] = page_to_pfn(pages[i]);
	}

	metadata->pfns_count = pages_count;

	return 0;
}

static int init_swd(struct page *meta_pages)
{
	unsigned long pfn;

	pfn = page_to_pfn(meta_pages);

	BUG_ON(pfn > U32_MAX);

	return tzdev_smc_channels_init((unsigned int)pfn, NR_SW_CPU_IDS);
}

static int deinit(void *impl_data)
{
	return 0;
}

static void acquire(struct tzdev_smc_channel_metadata *metadata)
{
	metadata->write_offset = 0;
	metadata->pfns_count = 0;
}

static int reserve(struct tzdev_smc_channel_metadata *metadata, struct page **pages,
		size_t old_pages_count, size_t new_pages_count, void *impl_data)
{
	size_t i;

	(void)impl_data;

	for (i = old_pages_count; i < new_pages_count; i++) {
		metadata->pfns[i - CONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES] = page_to_pfn(pages[i]);
	}

	metadata->pfns_count = new_pages_count - CONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES;

	return 0;
}

static int release(struct tzdev_smc_channel_metadata *metadata, void *impl_data)
{
	(void)impl_data;

	metadata->pfns_count = 0;

	return 0;
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
	.data_size = 0,
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
