/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Samsung Abox Logging API with memlog
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "abox_memlog.h"

int abox_memlog_printf(struct abox_data *data, int level, const char *fmt, ...)
{
	int ret = -EINVAL;

	if (data && data->drv_log_obj) {
		va_list args;

		va_start(args, fmt);
		ret = memlog_write_vsprintf(data->drv_log_obj, level, fmt, args);
		va_end(args);
	}

	return ret;
}
