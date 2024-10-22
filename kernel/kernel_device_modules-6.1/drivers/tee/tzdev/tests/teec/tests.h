/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TZDEV_TEEC_TESTS_H__
#define __TZDEV_TEEC_TESTS_H__

#include <linux/kernel.h>
#include <linux/workqueue.h>

struct test_data {
	uint64_t address;
	uint64_t size;
};

uint32_t tz_tee_test(uint32_t *origin);
uint32_t tz_tee_test_send_ta(uint32_t *origin, struct test_data *data);
uint32_t tz_tee_test_cancellation(uint32_t *origin);
uint32_t tz_tee_test_null_args(uint32_t *origin);
uint32_t tz_tee_test_shared_ref(uint32_t *origin);
uint32_t tz_tee_test_sleep(uint32_t *origin, struct test_data *data);

#endif /* __TZDEV_TEEC_TESTS_H__ */
