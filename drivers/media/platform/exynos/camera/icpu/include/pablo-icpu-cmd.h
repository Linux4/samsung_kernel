/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_CMD_H
#define PABLO_ICPU_CMD_H

#include "pablo-icpu-cmd-v1.h"

#define CLR_CMD(x) ((x) & ~ICPU_MSG_COMMAND_MASK)
#define CLR_TYPE(x) ((x) & ~ICPU_MSG_TYPE_MASK)
#define CLR_INSTANCE(x) ((x) & ~ICPU_MSG_INSTANCE_MASK)
#define CLR_ERR(x) ((x) & ~ICPU_MSG_ERROR_MASK)
#define CLR_TYPE_ERR(x) (((x) & ~ICPU_MSG_TYPE_MASK) & ((x) & ~ICPU_MSG_ERROR_MASK))

#define TO_CMD(x) ((x) << MSG_COMMAND_SHIFT)
#define TO_TYPE(x) ((x) << MSG_TYPE_SHIFT)
#define TO_INSTANCE(x) ((x) << MSG_INSTANCE_SHIFT)
#define TO_ERR(x) ((x) << MSG_ERROR_SHIFT)

#define SET_CMD(c, x) (CLR_CMD(c) | TO_CMD(x))
#define SET_TYPE(c, x) (CLR_TYPE(c) | TO_TYPE(x))
#define SET_INSTANCE(c, x) (CLR_INSTANCE(c) | TO_INSTANCE(x))
#define SET_ERR(c, x) (CLR_ERR(c) | TO_ERR(x))

#define SET_RSP(c) (SET_TYPE(c, MSG_TYPE_RSP))

#define GET_TYPE(x) (((x) & ICPU_MSG_TYPE_MASK) >> MSG_TYPE_SHIFT)
#define GET_CMD(x) (((x) & ICPU_MSG_COMMAND_MASK) >> MSG_COMMAND_SHIFT)
#define GET_INSTANCE(x) (((x) & ICPU_MSG_INSTANCE_MASK) >> MSG_INSTANCE_SHIFT)
#define GET_ERR(x) (((x) & ICPU_MSG_ERROR_MASK) >> MSG_ERROR_SHIFT)

#define CHECK_ICPU_FW_CMD(x) (GET_CMD(x) >= ICPU_FW_CMD_BASE ? true : false)

#endif
