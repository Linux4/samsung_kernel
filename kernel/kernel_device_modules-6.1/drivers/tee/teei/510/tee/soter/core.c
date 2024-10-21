// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (c) 2015, Linaro Limited
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <tee_drv.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define IMSG_TAG "[tz_driver]"
#include "imsg_log.h"


#include "soter_private.h"
#include "soter_smc.h"
#include <nt_smc_call.h>
#include "../../common/include/teei_private.h"


#define SOTER_SHM_NUM_PRIV_PAGES	1

struct reserved_mem *reserved_mem;
atomic_t is_shm_pool_available = ATOMIC_INIT(0);
DECLARE_COMPLETION(shm_pool_registered);

int teei_new_capi_init(void)
{
	if (reserved_mem) {
		int ret = soter_register_shm_pool(
				reserved_mem->base, reserved_mem->size);

		if (!ret) {
			atomic_set(&is_shm_pool_available, 1);
			complete_all(&shm_pool_registered);
		}
		return ret;
	}

	IMSG_ERROR("capi reserve memory is NULL!\n");

	return -ENOMEM;
}

int soter_driver_init(void)
{
	int retVal = 0;

	retVal = teei_ffa_abi_register();
	if (retVal != 0)
		return retVal;

	return 0;

}
EXPORT_SYMBOL_GPL(soter_driver_init);

void soter_driver_exit(void)
{
	teei_ffa_abi_unregister();
}
EXPORT_SYMBOL_GPL(soter_driver_exit);
