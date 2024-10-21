/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __NPU_SCP_IPI_H__
#define __NPU_SCP_IPI_H__

#include <linux/types.h>

enum npu_scp_ipi_cmd {
	NPU_SCP_IPI_CMD_START,
	NPU_SCP_RESPONSE,
	NPU_SCP_STATE_CHANGE,
	NPU_SCP_TEST,
	NPU_SCP_SYSTEM,
	NPU_SCP_NP_MDW,
	NPU_SCP_RECOVERY,
	NPU_SCP_MEM_SERVICE,
	NPU_SCP_IPI_CMD_END,
};

enum npu_scp_state_change_action {
	NPU_SCP_STATE_CHANGE_TO_SUSPEND = 1,
	NPU_SCP_STATE_CHANGE_TO_RESUME,
	NPU_SCP_STATE_CHANGE_TO_TRANSITION,
};

enum npu_scp_system_action {
	NPU_SCP_SYSTEM_GET_VERSION = 1,
	NPU_SCP_SYSTEM_FUNCTION_ENABLE,
	NPU_SCP_SYSTEM_FUNCTION_DISABLE,
	NPU_SCP_SYSTEM_FORCE_TO_SUSPEND,
	NPU_SCP_SYSTEM_FORCE_TO_RESUME,
};

enum npu_scp_test_action {
	NPU_SCP_TEST_START = 1,
	NPU_SCP_TEST_STOP,
	NPU_SCP_TEST_LAST_RESULT,
};

enum npu_scp_test_status {
	NPU_SCP_TEST_PASS,
	NPU_SCP_TEST_TIMEOUT,
	NPU_SCP_TEST_EXEC_FAIL,
	NPU_SCP_TEST_POWER_TIMEOUT,
};

#define SCP_IPI_TIMEOUT_MS (10)
#define TESTCASE_TIMEOUT_MS (10000)

#define NPU_SCP_RET_OK			(0)
#define NPU_SCP_RET_TEST_START_ERR	(-1)
#define NPU_SCP_RET_TEST_STOP_ERR	(-2)
#define NPU_SCP_RET_TEST_ERROR		(-3)
#define NPU_SCP_RET_TEST_ABORTED	(-4)
#define NPU_SCP_RET_TEST_EMPTY		(-5)

struct npu_scp_ipi_param {
	uint32_t cmd;
	uint32_t act;
	uint32_t arg;
	uint32_t ret;
};

int npu_scp_ipi_send(struct npu_scp_ipi_param *send_msg, struct npu_scp_ipi_param *recv_msg,
		     uint32_t timeout_ms);

typedef int (*npu_scp_ipi_handler)(struct npu_scp_ipi_param *recv_param);

int npu_scp_ipi_register_handler(enum npu_scp_ipi_cmd id, const npu_scp_ipi_handler top_half,
				 const npu_scp_ipi_handler buttom_half);

int npu_scp_ipi_unregister_handler(enum npu_scp_ipi_cmd id);

#endif // __NPU_SCP_IPI_H__
