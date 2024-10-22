/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TZ_TEE_KERNEL_API_TEST_H__
#define __TZ_TEE_KERNEL_API_TEST_H__

#include <linux/types.h>

#define TZ_TEE_KERNEL_API_RUN_KERNEL_TEST		_IOWR('x', 0, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_INITIALIZE_CONTEXT		_IOWR('x', 1, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_FINALIZE_CONTEXT		_IOWR('x', 2, uint32_t)
#define TZ_TEE_KERNEL_API_ALLOCATE_SHARED_MEMORY	_IOWR('x', 3, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_REGISTER_SHARED_MEMORY	_IOWR('x', 4, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_RELEASE_SHARED_MEMORY		_IOWR('x', 5, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_OPEN_SESSION			_IOWR('x', 6, struct tz_tee_kernel_api_cmd)
#define TZ_TEE_KERNEL_API_CLOSE_SESSION			_IOWR('x', 7, uint32_t)
#define TZ_TEE_KERNEL_API_INVOKE_COMMAND		_IOWR('x', 8, struct tz_tee_kernel_api_cmd)

enum {
	TZ_TEE_KERNEL_API_TEE_TEST,
	TZ_TEE_KERNEL_API_TEE_TEST_SEND_TA,
	TZ_TEE_KERNEL_API_TEE_TEST_CANCELLATION,
	TZ_TEE_KERNEL_API_TEE_TEST_NULL_ARGS,
	TZ_TEE_KERNEL_API_TEE_TEST_SHARED_REF,
	TZ_TEE_KERNEL_API_TEE_TEST_SLEEP,
};

struct tz_tee_kernel_api_test_case {
	uint32_t test_id;
	uint64_t size;
	uint64_t address;
} __attribute__((packed));

struct tz_tee_kernel_api_initialize_context {
	uint64_t name_address;
};

struct tz_tee_kernel_api_create_shared_memory {
	uint64_t address;
	uint64_t size;
	uint32_t flags;
	uint32_t context_id;
} __attribute__((packed));

struct tz_tee_kernel_api_release_shared_memory {
	uint64_t ret_address;
	uint64_t ret_size;
	uint32_t shmem_id;
} __attribute__((packed));

struct tz_tee_kernel_api_operation {
	uint32_t exists;
	uint32_t param_types;
	union {
		struct {
			uint32_t a;
			uint32_t b;
		} value;
		struct {
			uint64_t address;
			uint64_t size;
			uint32_t offset;
			uint32_t shmem_id;
		} memref;
		struct {
			uint64_t address;
			uint64_t size;
		} tmpref;
	} params[4];
} __attribute__((packed));

struct tz_tee_kernel_api_uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_and_node[8];
} __attribute__((packed));

struct tz_tee_kernel_api_open_session {
	struct tz_tee_kernel_api_operation op;
	struct tz_tee_kernel_api_uuid uuid;
	uint64_t conn_data_address;
	uint32_t conn_method;
	uint32_t context_id;
} __attribute__((packed));

struct tz_tee_kernel_api_invoke_command {
	struct tz_tee_kernel_api_operation op;
	uint32_t command_id;
	uint32_t session_id;
} __attribute__((packed));

struct tz_tee_kernel_api_cmd {
	uint32_t result;
	uint32_t origin;
	union {
		struct tz_tee_kernel_api_test_case test_case;
		struct tz_tee_kernel_api_initialize_context initialize_context;
		struct tz_tee_kernel_api_create_shared_memory create_memory;
		struct tz_tee_kernel_api_release_shared_memory release_memory;
		struct tz_tee_kernel_api_open_session open_session;
		struct tz_tee_kernel_api_invoke_command invoke_command;
	};
} __attribute__((packed));

#endif /* __TZ_TEE_KERNEL_API_TEST_H__ */
