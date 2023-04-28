/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_MAILBOX_H__
#define __DSP_HW_MAILBOX_H__

#include "dsp-util.h"

struct dsp_system;
struct dsp_mailbox_pool_manager;

enum dsp_mailbox_version {
	DSP_MAILBOX_VERSION_START,
	DSP_MAILBOX_V1,
	DSP_MAILBOX_VERSION_END
};

enum dsp_message_version {
	DSP_MESSAGE_VERSION_START,
	DSP_MESSAGE_V1,
	DSP_MESSAGE_V2,
	DSP_MESSAGE_V3,
	DSP_MESSAGE_VERSION_END
};

struct dsp_mailbox_to_fw {
	unsigned int			mailbox_version;
	unsigned int			message_version;
	unsigned int			task_id;
	unsigned int			pool_iova;
	unsigned int			pool_size;
	unsigned int			message_id;
	unsigned int			message_size;
	unsigned int			reserved[9];
};

struct dsp_mailbox_to_host {
	unsigned int			mailbox_version;
	unsigned int			message_version;
	unsigned int			task_id;
	unsigned int			task_ret;
	unsigned int			message_id;
	unsigned int			reserved[3];
};

struct dsp_mailbox {
	struct mutex			lock;
	unsigned int			mailbox_version;
	unsigned int			message_version;
	struct dsp_util_queue		*to_fw;
	struct dsp_mailbox_to_host	*to_host;

	struct dsp_mailbox_pool_manager	*pool_manager;
	struct dsp_system		*sys;
};

#endif
