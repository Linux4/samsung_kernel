// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/errno.h>
#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
#include "../tee_private.h"

struct soter_priv *soter_priv;

unsigned long soter_sec_world_id[SOTER_SEC_WORLD_ID_MAX];

struct tee_device *isee_get_teedev(void)
{
	IMSG_ERROR("[%s][%d] soter_priv is NULL!\n", __func__, __LINE__);
	return NULL;
}

int soter_ffa_shm_register(unsigned long page_link, unsigned int length,
				unsigned int offset, unsigned long *sec_id)
{
	return -EFAULT;
}

int teei_ffa_abi_register(void)
{
	IMSG_ERROR("[%s][%d] teei ffa is dummy!\n", __func__, __LINE__);
	return -EOPNOTSUPP;
}

void teei_ffa_abi_unregister(void)
{
}