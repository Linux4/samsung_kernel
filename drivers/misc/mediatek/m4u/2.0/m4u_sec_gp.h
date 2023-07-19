/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "tz_m4u.h"
#include "tee_client_api.h"

#if defined(CONFIG_TEEGRIS_TEE_SUPPORT)
#define TA_UUID NULL
#define TDRV_UUID "000000004D544B5F42464D3455445441"
#else
#define TA_UUID "98fb95bcb4bf42d26473eae48690d7ea"
#define TDRV_UUID "9073F03A9618383BB1856EB3F990BABD"
#endif

#define CTX_TYPE_TA		0
#define CTX_TYPE_TDRV	1

struct m4u_sec_gp_context {
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT) || \
		defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
/* Universally Unique Identifier of secure tl/dr */
	struct TEEC_UUID uuid;
#else
/* Universally Unique Identifier of secure tl/dr */
	TEEC_UUID uuid;
#endif
	struct TEEC_Context ctx; /* basic context */
	struct TEEC_Session session; /* session handle */
	struct TEEC_SharedMemory shared_mem; /* shared memory */
	struct mutex ctx_lock;
	int init;
	int ctx_type;
};

struct m4u_sec_context {
	const char *name;
	struct m4u_msg *m4u_msg;
	void *imp;
};

#if defined(CONFIG_TEEGRIS_TEE_SUPPORT)
#define M4U_DRV_UUID \
	{{0x00, 0x00, 0x00, 0x00, 0x4D, 0x54, 0x4B, \
	  0x5F, 0x42, 0x46, 0x4D, 0x34, 0x55, 0x44, \
	  0x54, 0x41} }
#define M4U_TA_UUID \
	{0x00000000, 0x4D54, 0x4B5F, \
	 {0x42, 0x46, 0x00, 0x00, 0x00, \
	  0x4D, 0x34, 0x55} }
#else
#define M4U_DRV_UUID \
	{{0x90, 0x73, 0xF0, 0x3A, 0x96, 0x18, 0x38, \
	  0x3B, 0xB1, 0x85, 0x6E, 0xB3, 0xF9, 0x90, \
	  0xBA, 0xBD} }
#define M4U_TA_UUID \
	{0x98fb95bc, 0xb4bf, 0x42d2, \
	 {0x64, 0x73, 0xea, 0xe4, 0x86, \
	  0x90, 0xd7, 0xea} }
#endif

struct m4u_sec_context *m4u_sec_ctx_get(unsigned int cmd);
int m4u_sec_context_init(void);
int m4u_sec_context_deinit(void);
void m4u_sec_set_context(void);
int m4u_sec_ctx_put(struct m4u_sec_context *ctx);
int m4u_exec_cmd(struct m4u_sec_context *ctx);

