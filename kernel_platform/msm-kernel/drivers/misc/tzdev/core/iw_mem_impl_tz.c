/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd All Rights Reserved
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

#include "tzdev_internal.h"
#include "core/iw_mem_impl.h"
#include "core/log.h"

static int init(struct page **pages, size_t pages_count, void *impl_data, int optimize)
{
	(void)pages;
	(void)pages_count;
	(void)impl_data;
	(void)optimize;

	return 0;
}

static int deinit(void *impl_data)
{
	(void)impl_data;

	return 0;
}

static int pack(struct tzdev_smc_channel *channel, struct page **pages, size_t pages_count,
		void *impl_data)
{
	sk_pfn_t pfn;
	unsigned int i;
	int ret;

	(void)impl_data;

	ret = tzdev_smc_channel_reserve(channel, sizeof(uint32_t) + sizeof(sk_pfn_t) * pages_count);
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to reserve %zu bytes (error %d)\n",
				sizeof(uint32_t) + sizeof(sk_pfn_t) * pages_count, ret);
		return ret;
	}

	ret = tzdev_smc_channel_write(channel, &pages_count, sizeof(uint32_t));
	if (ret) {
		log_error(tzdev_iw_mem, "Failed to write pfns count (error %d)\n", ret);
		return ret;
	}

	for (i = 0; i < pages_count; i++) {
		pfn = page_to_pfn(pages[i]);

		ret = tzdev_smc_channel_write(channel, &pfn, sizeof(sk_pfn_t));
		if (ret) {
			log_error(tzdev_iw_mem, "Failed to write %u pfn (error %d)\n", i, ret);
			return ret;
		}
	}

	return 0;
}

static const struct tzdev_iw_mem_impl impl = {
	.data_size = 0,
	.init = init,
	.pack = pack,
	.deinit = deinit,
};

const struct tzdev_iw_mem_impl *tzdev_get_iw_mem_impl(void)
{
	return &impl;
}
