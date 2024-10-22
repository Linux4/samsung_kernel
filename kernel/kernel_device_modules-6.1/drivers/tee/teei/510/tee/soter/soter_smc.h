/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef SOTER_SMC_H
#define SOTER_SMC_H

#include <linux/string.h>

/* Let's reuse optee's message format */
#include <imsg_log.h>
#include "teei_client_main.h"
#include "optee_msg.h"
#include "soter_private.h"

enum {
	NQ_CMD_CLIENT_API_REQUEST,
	NQ_CMD_GPTEE_CLIENT_API_REQUEST,
	NQ_CMD_REGISTER_SHM_POOL,
};

extern struct semaphore keymaster_api_lock;

int teei_forward_call(unsigned long long cmd, unsigned long long cmd_addr,
				unsigned long long size);

static inline int soter_do_call_with_arg(struct tee_context *ctx,
					phys_addr_t parg)
{
	if (strncmp(ctx->hostname, "bta_loader", TEE_MAX_HOSTNAME_SIZE) == 0)
		return teei_forward_call(NQ_CMD_CLIENT_API_REQUEST, parg,
						sizeof(struct optee_msg_arg));

	if (strncmp(ctx->hostname, "tta", TEE_MAX_HOSTNAME_SIZE) == 0)
		return teei_forward_call(NQ_CMD_GPTEE_CLIENT_API_REQUEST, parg,
						sizeof(struct optee_msg_arg));

	IMSG_ERROR("Unrecognized hostname: '%s'\n", ctx->hostname);

	return -EINVAL;
}

static inline int soter_register_shm_pool(phys_addr_t shm_pa, int shm_size)
{
	int retVal = 0;

	retVal = soter_ffa_shm_register(
			(unsigned long)virt_to_page(phys_to_virt(shm_pa)),
			shm_size, 0,
			&soter_sec_world_id[SOTER_SEC_WORLD_ID_POOL]);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] TEEI: Failed to register memory!\n",
				__func__, __LINE__);
		return retVal;
	}

	retVal = teei_forward_call(NQ_CMD_REGISTER_SHM_POOL,
			soter_sec_world_id[SOTER_SEC_WORLD_ID_POOL],
			soter_sec_world_id[SOTER_SEC_WORLD_ID_POOL]>>32);

	return retVal;

}

#endif
