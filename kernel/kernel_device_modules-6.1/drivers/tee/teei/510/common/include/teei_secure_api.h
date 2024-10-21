/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef _TEEI_KERN_API_
#define _TEEI_KERN_API_

#include <imsg_log.h>
#include <linux/arm-smccc.h>
#include "../../tee/soter/soter_private.h"

#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#define ARCH_EXTENSION_SEC
#endif

#define TEEI_FC_CPU_ON			(0xb4000080)
#define TEEI_FC_CPU_OFF			(0xb4000081)
#define TEEI_FC_CPU_DORMANT		(0xb4000082)
#define TEEI_FC_CPU_DORMANT_CANCEL	(0xb4000083)
#define TEEI_FC_CPU_ERRATA_802022	(0xb4000084)

static inline long teei_secure_call(u64 function_id,
		u64 arg0, u64 arg1, u64 arg2)
{
	struct ffa_send_direct_data data = {
		.data0 = function_id,
		.data1 = arg0,
		.data2 = arg1,
		.data3 = arg2,
	};

	int retVal = 0;

	retVal = soter_priv->ffa.ffa_ops->msg_ops->sync_send_receive(
					soter_priv->ffa.ffa_dev, &data);
	if (retVal) {
		IMSG_ERROR("smc is wrong in func = %s line = %d\n",
							__func__, __LINE__);
		return -1;
	}

	return data.data0;
}
#endif /* _TEEI_KERN_API_ */
