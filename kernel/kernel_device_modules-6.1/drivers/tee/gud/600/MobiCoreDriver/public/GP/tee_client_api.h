/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * This header file corresponds to V1.0 of the GlobalPlatform
 * TEE Client API Specification
 */
#ifndef TEE_CLIENT_API_H
#define TEE_CLIENT_API_H

#include <linux/version.h> /* KERNEL_VERSION */
#include <linux/kconfig.h> /* IS_REACHABLE */

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
#if IS_REACHABLE(CONFIG_ARM_FFA_TRANSPORT)
#include <linux/arm_ffa.h>
#endif /* CONFIG_ARM_FFA_TRANSPORT */
#endif /* KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE */

#include "tee_client_types.h"
#include "tee_client_error.h"
#include "tee_client_api_imp.h"

/* Include GP spec naming (TEEC_*) data type and functions */
#include "tee_client_api_cust.h"

#define TEEC_EXPORT

/*
 * The header tee_client_api_imp.h must define implementation-dependent types,
 * constants and macros.
 *
 * The implementation-dependent types are:
 *   - teec_context_imp
 *   - teec_session_imp
 *   - teec_shared_memory_imp
 *   - teec_operation_imp
 *
 * The implementation-dependent constants are:
 *   - TEEC_CONFIG_SHAREDMEM_MAX_SIZE
 * The implementation-dependent macros are:
 *   - TEEC_PARAM_TYPES
 */

/** teec_value definition */
struct teec_value {
	u32 a;	/**< 1st parameter */
	u32 b;	/**< 2nd parameter */
};

/** teec_context definition */
struct teec_context {
	struct teec_context_imp imp;	/**< Opaque parameter */
};

/** teec_session definition */
struct teec_session {
	struct teec_session_imp imp;	/**< Opaque parameter */
};

/** teec_shared_memory definition */
struct teec_shared_memory {
	void		*buffer;	/**< Memory buffer */
	size_t		size;		/**< Memory size */
	u32			flags;		/**< Associated flags */
	struct teec_shared_memory_imp imp;	/**< Opaque parameter */
};

/** teec_temp_memory_reference definition */
struct teec_temp_memory_reference {
	void   *buffer;		/**< Memory buffer */
	size_t size;		/**< Memory size */
};

/** teec_registered_memory_reference definition */
struct teec_registered_memory_reference {
	struct teec_shared_memory *parent;	/**< Shared memory parent */
	size_t			  size;		/**< Memory size */
	size_t			  offset;	/**< Memory offset */
};

/** teec_parameter definition */
union teec_parameter {
	/** Temporary memory reference */
	struct teec_temp_memory_reference	tmpref;
	/** Registered memory reference */
	struct teec_registered_memory_reference	memref;
	/** TEEC value */
	struct teec_value			value;
};

/** teec_operation definition */
struct teec_operation {
	u32			  started;	/**< started flag */
	union {
		u32		  param_types;	/**< Parameter types */
		u32		  paramTypes;
	};
	union teec_parameter	  params[4];	/**< Parameters */
	struct teec_operation_imp imp;		/**< Opaque parameter */
};

#define TEEC_ORIGIN_API                     0x00000001
#define TEEC_ORIGIN_COMMS                   0x00000002
#define TEEC_ORIGIN_TEE                     0x00000003
#define TEEC_ORIGIN_TRUSTED_APP             0x00000004

#define TEEC_MEM_INPUT                      0x00000001
#define TEEC_MEM_OUTPUT                     0x00000002
#define TEEC_MEM_ION                        0x01000000

#define TEEC_NONE                           0x0
#define TEEC_VALUE_INPUT                    0x1
#define TEEC_VALUE_OUTPUT                   0x2
#define TEEC_VALUE_INOUT                    0x3
#define TEEC_MEMREF_TEMP_INPUT              0x5
#define TEEC_MEMREF_TEMP_OUTPUT             0x6
#define TEEC_MEMREF_TEMP_INOUT              0x7
#define TEEC_MEMREF_WHOLE                   0xC
#define TEEC_MEMREF_PARTIAL_INPUT           0xD
#define TEEC_MEMREF_PARTIAL_OUTPUT          0xE
#define TEEC_MEMREF_PARTIAL_INOUT           0xF

#define TEEC_LOGIN_PUBLIC                   0x00000000
#define TEEC_LOGIN_USER                     0x00000001
#define TEEC_LOGIN_GROUP                    0x00000002
#define TEEC_LOGIN_APPLICATION              0x00000004
#define TEEC_LOGIN_USER_APPLICATION         0x00000005
#define TEEC_LOGIN_GROUP_APPLICATION        0x00000006

#define TEEC_TIMEOUT_INFINITE               0xFFFFFFFF

#pragma GCC visibility push(default)

/**
 *  **Equivalent to TEEC_InitializeContext()**
 */
TEEC_EXPORT u32
teec_initialize_context(const char *name, struct teec_context *context);

/**
 *  **Equivalent to TEEC_FinalizeContext()**
 */
TEEC_EXPORT void
teec_finalize_context(struct teec_context *context);

/**
 *  **Equivalent to TEEC_RegisterSharedMemory()**
 */
TEEC_EXPORT u32
teec_register_shared_memory(struct teec_context *context,
			    struct teec_shared_memory *shared_mem);

/**
 *  **Equivalent to TEEC_AllocateSharedMemory()**
 */
TEEC_EXPORT u32
teec_allocate_shared_memory(struct teec_context *context,
			    struct teec_shared_memory *shared_mem);

/**
 *  **Equivalent to TEEC_ReleaseSharedMemory()**
 */
TEEC_EXPORT void
teec_release_shared_memory(struct teec_shared_memory *shared_mem);

/**
 *  **Equivalent to TEEC_OpenSession()**
 *
 *  <table>
 *  <tr>
 *      <th>connection_method</th>
 *      <th>connection_data</th>
 *      <th>API Level</th>
 *  </tr>
 *  <tr>
 *      <td>TEEC_LOGIN_PUBLIC</td>
 *      <td>connection_data should be NULL</td>
 *      <td>10 and higher</td>
 *  </tr>
 *  </table>
 */
TEEC_EXPORT u32
teec_open_session(struct teec_context *context,
		  struct teec_session *session,
		  const struct teec_uuid *destination,
		  u32 connection_method, /* Should be 0 */
		  const void *connection_data,
		  struct teec_operation *operation,
		  u32 *return_origin);

/**
 *  **Equivalent to TEEC_CloseSession()**
 */
TEEC_EXPORT void
teec_close_session(struct teec_session *session);

/**
 *  **Equivalent to TEEC_InvokeCommand()**
 */
TEEC_EXPORT u32
teec_invoke_command(struct teec_session *session,
		    u32 command_id,
		    struct teec_operation *operation,
		    u32 *return_origin);

/**
 *  **Equivalent to TEEC_RequestCancellation()**
 */
TEEC_EXPORT void
teec_request_cancellation(struct teec_operation *operation);

TEEC_EXPORT u32
teec_tt_lend_shared_memory(struct teec_context *context,
			   struct teec_shared_memory *shared_mem,
			   u64 tag, u64 *handle);

#pragma GCC visibility pop

#endif /* TEE_CLIENT_API_H */
