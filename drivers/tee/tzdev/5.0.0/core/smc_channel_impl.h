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

#pragma once

#include <linux/types.h>

struct tzdev_smc_channel_metadata;

struct tzdev_smc_channel_impl {
	size_t data_size;
	int (*init)(struct tzdev_smc_channel_metadata *metadata,
			struct page **pages, size_t pages_count, void *impl_data);
	int (*init_swd)(struct page *meta_pages);
	int (*deinit)(void *impl_data);
	void (*acquire)(struct tzdev_smc_channel_metadata *metadata);
	int (*reserve)(struct tzdev_smc_channel_metadata *metadata, struct page **pages,
			size_t old_pages_count, size_t new_pages_count, void *impl_data);
	int (*release)(struct tzdev_smc_channel_metadata *metadata, void *impl_data);
	void (*set_write_offset)(struct tzdev_smc_channel_metadata *metadata, uint32_t value);
	uint32_t (*get_write_offset)(struct tzdev_smc_channel_metadata *metadata);
};

struct tzdev_smc_channel_impl *tzdev_get_smc_channel_impl(void);
