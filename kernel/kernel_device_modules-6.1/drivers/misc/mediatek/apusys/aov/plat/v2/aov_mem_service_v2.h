/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __AOV_MEM_SERVICE_V2_H__
#define __AOV_MEM_SERVICE_V2_H__

#include <linux/types.h>
#include <linux/platform_device.h>

enum npu_scp_mem_service_action {
	NPU_SCP_QUERY_DRAM_DATA_DEVADDR = 1,
	NPU_SCP_QUERY_DRAM_CTRL_DEVADDR,
	NPU_SCP_SEND_DRAM_DATA_DEVADDR,
	NPU_SCP_SEND_DRAM_CTRL_DEVADDR,
	NPU_SCP_SEND_ERROR,
};

int aov_mem_service_v2_init(struct platform_device *pdev);

void aov_mem_service_v2_exit(struct platform_device *pdev);

#endif // __AOV_MEM_SERVICE_V2_H__
