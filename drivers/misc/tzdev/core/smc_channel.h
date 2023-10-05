/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
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

struct tzdev_smc_channel;

struct tzdev_smc_channel *tzdev_smc_channel_acquire(void);
void tzdev_smc_channel_release(struct tzdev_smc_channel *channel);

int tzdev_smc_channel_reserve(struct tzdev_smc_channel *channel, size_t size);
int tzdev_smc_channel_write(struct tzdev_smc_channel *channel, const void *buffer, size_t size);
int tzdev_smc_channel_read(struct tzdev_smc_channel *channel, void *buffer, size_t size);

int tzdev_smc_channel_init(void);
