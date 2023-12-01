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

#ifndef PABLO_ICPU_CMD_V1_H
#define PABLO_ICPU_CMD_V1_H

#define MSG_INSTANCE_SHIFT 28
#define MSG_TYPE_SHIFT 12
#define MSG_COMMAND_SHIFT 4
#define MSG_ERROR_SHIFT 0

#define ICPU_MSG_INSTANCE_MASK (0xF << MSG_INSTANCE_SHIFT)
#define ICPU_MSG_TYPE_MASK (0xF << MSG_TYPE_SHIFT)
#define ICPU_MSG_COMMAND_MASK (0xFF << MSG_COMMAND_SHIFT)
#define ICPU_MSG_ERROR_MASK (0xF << MSG_ERROR_SHIFT)

#define MSG_TYPE_CMD 0
#define MSG_TYPE_RSP 1

#define ICPU_FW_CMD_BASE 0x80

#define PABLO_IHC_READY (ICPU_FW_CMD_BASE)
#define PABLO_IHC_ERROR (ICPU_FW_CMD_BASE + 1)
#define PABLO_IHC_DEBUG_LOG (ICPU_FW_CMD_BASE + 2)

#define PABLO_HIC_MMU_MAP (ICPU_FW_CMD_BASE)
#define PABLO_HIC_MMU_UNMAP (ICPU_FW_CMD_BASE + 1)
#define PABLO_HIC_GET_STATUS (ICPU_FW_CMD_BASE + 2)
#define PABLO_HIC_POWER_DOWN (ICPU_FW_CMD_BASE + 3)
#define PABLO_HIC_SET_TIMESTAMP (ICPU_FW_CMD_BASE + 4)

#endif
