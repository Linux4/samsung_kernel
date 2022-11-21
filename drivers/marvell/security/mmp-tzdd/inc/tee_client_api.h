/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#ifndef _TEE_CLIENT_API_H_
#define _TEE_CLIENT_API_H_

#include "teec_types.h"

/*
 * Constants
 */
#define TEEC_CONFIG_SHAREDMEM_MAX_SIZE  (0x8000000)     /* 128MB */

/*
 * API Error Codes
 */
#define TEEC_SUCCESS                (0x00000000)
#define TEEC_ERROR_GENERIC          (0xFFFF0000)
#define TEEC_ERROR_ACCESS_DENIED    (0xFFFF0001)
#define TEEC_ERROR_CANCEL           (0xFFFF0002)
#define TEEC_ERROR_ACCESS_CONFLICT  (0xFFFF0003)
#define TEEC_ERROR_EXCESS_DATA      (0xFFFF0004)
#define TEEC_ERROR_BAD_FORMAT       (0xFFFF0005)
#define TEEC_ERROR_BAD_PARAMETERS   (0xFFFF0006)
#define TEEC_ERROR_BAD_STATE        (0xFFFF0007)
#define TEEC_ERROR_ITEM_NOT_FOUND   (0xFFFF0008)
#define TEEC_ERROR_NOT_IMPLEMENTED  (0xFFFF0009)
#define TEEC_ERROR_NOT_SUPPORTED    (0xFFFF000A)
#define TEEC_ERROR_NO_DATA          (0xFFFF000B)
#define TEEC_ERROR_OUT_OF_MEMORY    (0xFFFF000C)
#define TEEC_ERROR_BUSY             (0xFFFF000D)
#define TEEC_ERROR_COMMUNICATION    (0xFFFF000E)
#define TEEC_ERROR_SECURITY         (0xFFFF000F)
#define TEEC_ERROR_SHORT_BUFFER     (0xFFFF0010)
#define TEEC_ERROR_TARGET_DEAD      (0xFFFF3024)

/*
 * Login Type Constants
 */
#define TEEC_LOGIN_PUBLIC               (0x00000000)
#define TEEC_LOGIN_USER                 (0x00000001)
#define TEEC_LOGIN_GROUP                (0x00000002)
#define TEEC_LOGIN_APPLICATION          (0x00000004)

/*
 * Return Code for returnOrigins
 */
#define TEEC_ORIGIN_API         (0x00000001)
#define TEEC_ORIGIN_COMMS       (0x00000002)
#define TEEC_ORIGIN_TEE         (0x00000003)
#define TEEC_ORIGIN_TRUSTED_APP (0x00000004)

/*
 * Parameter Types for paramTypes arg
 */
#define TEEC_NONE                   (0x00000000)
#define TEEC_VALUE_INPUT            (0x00000001)
#define TEEC_VALUE_OUTPUT           (0x00000002)
#define TEEC_VALUE_INOUT            (0x00000003)
#define TEEC_MEMREF_TEMP_INPUT      (0x00000005)
#define TEEC_MEMREF_TEMP_OUTPUT     (0x00000006)
#define TEEC_MEMREF_TEMP_INOUT      (0x00000007)
#define TEEC_MEMREF_WHOLE           (0x0000000C)
#define TEEC_MEMREF_PARTIAL_INPUT   (0x0000000D)
#define TEEC_MEMREF_PARTIAL_OUTPUT  (0x0000000E)
#define TEEC_MEMREF_PARTIAL_INOUT   (0x0000000F)

/*
 * Session Login Methods
 */
#define TEEC_LOGIN_PUBLIC               (0x00000000)
#define TEEC_LOGIN_USER                 (0x00000001)
#define TEEC_LOGIN_GROUP                (0x00000002)
#define TEEC_LOGIN_APPLICATION          (0x00000004)
#define TEEC_LOGIN_USER_APPLICATION     (0x00000005)
#define TEEC_LOGIN_GROUP_APPLICATION    (0x00000006)

#define TEEC_PARAM_TYPES(t0, t1, t2, t3) \
	((t0) | ((t1) << 4) | ((t2) << 8) | ((t3) << 12))
#define TEEC_PARAM_TYPE_GET(t, i) (((t) >> (i*4)) & 0xF)

/*
 * Shared Memory Control Flags
 */
#define TEEC_MEM_INPUT      (0x00000001)
#define TEEC_MEM_OUTPUT     (0x00000002)

typedef uint32_t TEEC_Result;
typedef void *TEEC_MRVL_Context;
typedef void *TEEC_MRVL_Session;
typedef void *TEEC_MRVL_SharedMemory;
typedef void *TEEC_MRVL_Operation;

typedef struct {
	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHiAndVersion;
	uint8_t  clockSeqAndNode[8];
} TEEC_UUID;

typedef struct {
	TEEC_MRVL_Context imp;
} TEEC_Context;

typedef struct {
	TEEC_MRVL_Session imp;
} TEEC_Session;

typedef struct {
	void *buffer;
	size_t size;
	uint32_t flags;
	TEEC_MRVL_SharedMemory imp;
} TEEC_SharedMemory;

typedef struct {
	void *buffer;
	size_t size;
} TEEC_TempMemoryReference;

typedef struct {
	TEEC_SharedMemory *parent;
	size_t size;
	size_t offset;
} TEEC_RegisteredMemoryReference;

typedef struct {
	uint32_t a;
	uint32_t b;
} TEEC_Value;

typedef union {
	TEEC_TempMemoryReference tmpref;
	TEEC_RegisteredMemoryReference memref;
	TEEC_Value value;
} TEEC_Parameter;

typedef struct {
	uint32_t started;
	uint32_t paramTypes;
	TEEC_Parameter params[4];
	TEEC_MRVL_Operation imp;
} TEEC_Operation;


TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *context);
void TEEC_FinalizeContext(TEEC_Context *context);
TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context,
						TEEC_SharedMemory *sharedMem);
TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context,
						TEEC_SharedMemory *sharedMem);
void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMem);
TEEC_Result TEEC_OpenSession(TEEC_Context *context, TEEC_Session *session,
						 const TEEC_UUID *destination,
						 uint32_t connectionMethod,
						 const void *connectionData,
						 TEEC_Operation *operation,
						 uint32_t *returnOrigin);
void TEEC_CloseSession(TEEC_Session *session);
TEEC_Result TEEC_InvokeCommand(TEEC_Session *session, uint32_t commandID,
						TEEC_Operation *operation,
						uint32_t *returnOrigin);
void TEEC_RequestCancellation(TEEC_Operation *operation);

#endif /* _TEE_CLIENT_API_H_ */
