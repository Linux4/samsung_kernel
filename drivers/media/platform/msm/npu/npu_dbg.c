/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include "npu_common.h"
#include "npu_firmware.h"
#include "npu_hw.h"
#include "npu_hw_access.h"
#include "npu_mgr.h"

/* -------------------------------------------------------------------------
 * Function Definitions - Debug
 * -------------------------------------------------------------------------
 */
void npu_dump_debug_timeout_stats(struct npu_device *npu_dev)
{
	uint32_t reg_val;

	reg_val = REGR(npu_dev, REG_FW_JOB_CNT_START);
	pr_info("fw jobs execute started count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_FW_JOB_CNT_END);
	pr_info("fw jobs execute finished count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_NPU_FW_DEBUG_DATA);
	pr_info("fw jobs aco parser debug = %d\n", reg_val);
}

void npu_dump_ipc_packet(struct npu_device *npu_dev, void *cmd_ptr)
{
	int32_t *ptr = (int32_t *)cmd_ptr;
	uint32_t cmd_pkt_size = 0;
	int i;

	cmd_pkt_size = (*(uint32_t *)cmd_ptr);

	pr_err("IPC packet size %d content:\n", cmd_pkt_size);
	for (i = 0; i < cmd_pkt_size/4; i++)
		pr_err("%x\n", ptr[i]);
}

void npu_dump_ipc_queue(struct npu_device *npu_dev, uint32_t target_que)
{
	struct hfi_queue_header queue;
	size_t offset = (size_t)IPC_ADDR +
		sizeof(struct hfi_queue_tbl_header) +
		target_que * sizeof(struct hfi_queue_header);
	int32_t *ptr = (int32_t *)&queue;
	int i;

	MEMR(npu_dev, (void *)((size_t)offset), (uint8_t *)&queue,
		HFI_QUEUE_HEADER_SIZE);

	pr_err("IPC queue %d size %d content:\n", target_que,
		HFI_QUEUE_HEADER_SIZE);
	for (i = 0; i < HFI_QUEUE_HEADER_SIZE/4; i++)
		pr_err("%x\n", ptr[i]);
}
