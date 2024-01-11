/*
 * Copyright (C) 2018-2020 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_dele: " fmt

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "sipa_dele_priv.h"

#define FIFO_NODE_BYTES	16

static struct ap_delegator *s_ap_delegator;

static int ap_dele_local_req_prod(void *user_data)
{
	struct sipa_delegator *delegator = user_data;

	pr_debug("sipa rm id %d stat = %d connected = %d\n",
		 delegator->prod_id, delegator->stat,
		 delegator->connected);

	return 0;
}

static int ap_dele_local_rls_prod(void *user_data)
{
	struct sipa_delegator *delegator = user_data;

	pr_debug("sipa rm id %d stat = %d\n",
		 delegator->prod_id, delegator->stat);

	return 0;
}

int sipa_third_ap_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_ap_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_ap_delegator),
				      GFP_KERNEL);
	if (!s_ap_delegator)
		return -ENOMEM;

	ret = sipa_delegator_init(&s_ap_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_ap_delegator->delegator.local_request_prod = ap_dele_local_req_prod;
	s_ap_delegator->delegator.local_release_prod = ap_dele_local_rls_prod;

	sipa_delegator_start(&s_ap_delegator->delegator);

	return 0;
}
EXPORT_SYMBOL(sipa_third_ap_delegator_init);
