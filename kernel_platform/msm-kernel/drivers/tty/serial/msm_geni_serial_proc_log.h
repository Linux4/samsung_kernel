
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MSM_GENI_SERIAL_EXPORT_PROC_HEADER__
#define __MSM_GENI_SERIAL_EXPORT_PROC_HEADER__
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include <linux/ipc_logging.h>
#include <uapi/linux/msm_geni_serial.h>

#include "../../../kernel/trace/ipc_logging_private.h"

#if defined(CONFIG_SEC_FACTORY)
int register_serial_ipc_log_context(struct ipc_log_context *ctxt);
int create_proc_log_file(void);
#else
#define register_serial_ipc_log_context(ctxt) do {} while(0)
static int create_proc_log_file(void) {return 0;}
#endif

#endif