/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __SAP_CUSOTM_CMD_H__
#define __SAP_CUSOTM_CMD_H__

#include <linux/kconfig.h>
#include "hf_sensor_type.h"
#include "custom_cmd.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SAP_SUPPORT)
int sap_custom_cmd_comm(int sensor_type, struct custom_cmd *cust_cmd);
int sap_custom_cmd_init(void);
void sap_custom_cmd_exit(void);
#else
static inline int sap_custom_cmd_comm(int sensor_type,
	struct custom_cmd *cust_cmd) { return 0; }
static inline int sap_custom_cmd_init(void) { return 0; }
static inline void sap_custom_cmd_exit(void) {}
#endif

#endif
