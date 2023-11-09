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

#pragma once

#include <linux/mm.h>

#include "core/smc_channel.h"

struct tzdev_iw_mem_impl {
	size_t data_size;
	int (*init)(struct page **pages, size_t pages_count, void *impl_data, int optimize);
	int (*pack)(struct tzdev_smc_channel *channel, struct page **pages, size_t pages_count,
			void *impl_data);
	int (*deinit)(void *impl_data);
};

const struct tzdev_iw_mem_impl *tzdev_get_iw_mem_impl(void);
