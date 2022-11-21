/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_LOG_H__
#define __VERTEX_LOG_H__

#include <linux/kernel.h>

#include "vertex-config.h"

#define vertex_err(fmt, args...) \
	pr_err("[VIPx-VERTEX][ERR](%d):" fmt, __LINE__, ##args)

#define vertex_warn(fmt, args...) \
	pr_warn("[VIPx-VERTEX][WRN](%d):" fmt, __LINE__, ##args)

#define vertex_info(fmt, args...) \
	pr_info("[VIPx-VERTEX]:" fmt, ##args)

#define vertex_dbg(fmt, args...) \
	pr_info("[VIPx-VERTEX][DBG](%d):" fmt, __LINE__, ##args)

#if defined(DEBUG_LOG_CALL_TREE)
#define vertex_enter()		vertex_info("[%s] enter\n", __func__)
#define vertex_leave()		vertex_info("[%s] leave\n", __func__)
#define vertex_check()		vertex_info("[%s] check\n", __func__)
#else
#define vertex_enter()
#define vertex_leave()
#define vertex_check()
#endif

#endif
