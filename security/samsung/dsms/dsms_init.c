/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include "dsms_init.h"
#include "dsms_kernel_api.h"
#include "dsms_rate_limit.h"
#include "dsms_test.h"

static int is_dsms_initialized_flag = false;

int dsms_is_initialized(void)
{
	return is_dsms_initialized_flag;
}

kunit_init_module(dsms_init)
{
	DSMS_LOG_INFO("Started.");
	dsms_rate_limit_init();
	is_dsms_initialized_flag = true;
	return 0;
}
module_init(dsms_init);
