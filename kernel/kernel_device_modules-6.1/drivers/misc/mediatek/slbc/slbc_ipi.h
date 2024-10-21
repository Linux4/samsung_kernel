/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_IPI_H_
#define _SLBC_IPI_H_

#include <slbc_ops.h>

enum {
	IPI_SLBC_ENABLE,
	IPI_SLBC_SYNC_FROM_AP,
	IPI_SLBC_SYNC_TO_AP,
	IPI_SLBC_CACHE_REQUEST_FROM_AP,
	IPI_SLBC_CACHE_REQUEST_TO_AP,
	IPI_SLBC_CACHE_RELEASE_FROM_AP,
	IPI_SLBC_CACHE_RELEASE_TO_AP,
	IPI_SLBC_BUFFER_REQUEST_FROM_AP,
	IPI_SLBC_BUFFER_REQUEST_TO_AP,
	IPI_SLBC_BUFFER_RELEASE_FROM_AP,
	IPI_SLBC_BUFFER_RELEASE_TO_AP,
	IPI_SLBC_ACP_REQUEST_FROM_AP,
	IPI_SLBC_ACP_REQUEST_TO_AP,
	IPI_SLBC_ACP_RELEASE_FROM_AP,
	IPI_SLBC_ACP_RELEASE_TO_AP,
	IPI_SLBC_SUSPEND_RESUME_NOTIFY,
	IPI_SLBC_BUFFER_POWER_ON,
	IPI_SLBC_BUFFER_POWER_OFF,
	IPI_SLBC_FORCE,
	IPI_SLBC_MIC_NUM,
	IPI_SLBC_INNER,
	IPI_SLBC_OUTER,
	IPI_SLBC_MEM_BARRIER,
	IPI_SLB_DISABLE,
	IPI_SLC_DISABLE,
	IPI_SLBC_BUFFER_STATUS,
	IPI_SLBC_SRAM_UPDATE,
	IPI_SLBC_GID_REQUEST_FROM_AP,
	IPI_SLBC_GID_RELEASE_FROM_AP,
	IPI_SLBC_ROI_UPDATE_FROM_AP,
	IPI_SLBC_GID_VALID_FROM_AP,
	IPI_SLBC_GID_INVALID_FROM_AP,
	IPI_SLBC_GID_READ_INVALID_FROM_AP,
	IPI_SLBC_TABLE_GID_SET,
	IPI_SLBC_TABLE_GID_RELEASE,
	IPI_SLBC_TABLE_GID_GET,
	IPI_SLBC_TABLE_IDT_SET,
	IPI_SLBC_TABLE_IDT_RELEASE,
	IPI_SLBC_TABLE_IDT_GET,
	IPI_SLBC_TABLE_GID_AXI_SET,
	IPI_SLBC_TABLE_GID_AXI_RELEASE,
	IPI_SLBC_TABLE_GID_AXI_GET,
	IPI_EMI_SLB_SELECT,
	IPI_SLBC_BUFFER_CB_NOTIFY,
	IPI_EMI_PMU_COUNTER,
	IPI_EMI_PMU_SET_CTRL,
	IPI_EMI_GID_PMU_COUNTER,
	IPI_EMI_PMU_READ_COUNTER,
	IPI_EMI_GID_PMU_READ_COUNTER,
	IPI_EMI_SLC_TEST_RESULT,
	IPI_SLBC_CACHE_USER_PMU,
	IPI_SLBC_CACHE_USER_STATUS,
	IPI_SLBC_CACHE_USER_INFO,
	IPI_SLBC_CACHE_USER_CONFIG,
	IPI_SLBC_CACHE_USER_CEIL_SET,
	IPI_SLBC_CACHE_USER_FLOOR_SET,
	IPI_SLBC_CACHE_WINDOW_SET,
	IPI_SLBC_CACHE_WINDOW_GET,
	IPI_SLBC_CACHE_USAGE,
	NR_IPI_SLBC,
};

struct slbc_ipi_data {
	unsigned int cmd;
	unsigned int arg;
	unsigned int arg2;
	unsigned int arg3;
};

struct slbc_ipi_ops {
	int (*slbc_request_acp)(void *ptr);
	int (*slbc_release_acp)(void *ptr);
	void (*slbc_mem_barrier)(void);
	void (*slbc_buffer_cb_notify)(u32 arg, u32 arg2, u32 arg3);
};

extern int slbc_scmi_set(void *buffer);
extern int slbc_scmi_get(void *buffer, void *ptr);

#define SLBC_IPI(x, y)			((x) & 0xffff | ((y) & 0xffff) << 16)
#define SLBC_IPI_CMD_GET(x)		((x) & 0xffff)
#define SLBC_IPI_UID_GET(x)		((x) >> 16 & 0xffff)

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
extern int slbc_suspend_resume_notify(int suspend);
extern int slbc_scmi_init(void);
extern int slbc_sspm_slb_disable(int disable);
extern int slbc_sspm_slc_disable(int disable);
extern int slbc_sspm_enable(int enable);
extern int slbc_force_scmi_cmd(unsigned int force);
extern int slbc_mic_num_cmd(unsigned int num);
extern int slbc_inner_cmd(unsigned int inner);
extern int slbc_outer_cmd(unsigned int outer);
extern int slbc_set_scmi_info(int uid, uint16_t cmd, int arg, int arg2, int arg3);
extern int slbc_get_scmi_info(int uid, uint16_t cmd, void *ptr);
extern int _slbc_request_cache_scmi(void *ptr);
extern int _slbc_release_cache_scmi(void *ptr);
extern int _slbc_buffer_status_scmi(void *ptr);
extern int _slbc_request_buffer_scmi(void *ptr);
extern int _slbc_release_buffer_scmi(void *ptr);
extern void slbc_register_ipi_ops(struct slbc_ipi_ops *ops);
extern void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops);
extern int slbc_sspm_sram_update(void);
extern int slbc_table_gid_set(int gid, int quota, int pri);
extern int slbc_table_gid_release(int gid);
extern int slbc_table_gid_get(int gid);
extern int slbc_table_idt_set(int index, int arid, int idt);
extern int slbc_table_idt_release(int index);
extern int slbc_table_idt_get(int index);
extern int slbc_table_gid_axi_set(int index, int axiid, int pg);
extern int slbc_table_gid_axi_release(int index);
extern int slbc_table_gid_axi_get(int index);
extern int emi_slb_select(int argv1, int argv2, int argv3);
extern int emi_pmu_counter(int argv1, int argv2, int argv3);
extern int emi_pmu_set_ctrl(int argv1, int argv2, int argv3);
extern int emi_gid_pmu_counter(int argv1, int argv2);
extern int emi_pmu_read_counter(int idx);
extern int emi_gid_pmu_read_counter(void *ptr);
extern int emi_slc_test_result(void);
extern int _slbc_ach_scmi(unsigned int cmd, enum slc_ach_uid uid, int gid,
			struct slbc_gid_data *data);

#else
__weak int slbc_suspend_resume_notify(int) {}
__weak int slbc_scmi_init(void) { return 0; }
__weak int slbc_sspm_slb_disable(int disable) {}
__weak int slbc_sspm_slc_disable(int disable) {}
__weak int slbc_sspm_enable(int enable) {}
__weak int slbc_force_scmi_cmd(unsigned int force) {}
__weak int slbc_mic_num_cmd(unsigned int num) {}
__weak int slbc_inner_cmd(unsigned int inner) {}
__weak int slbc_outer_cmd(unsigned int outer) {}
__weak int slbc_set_scmi_info(int uid, uint16_t cmd, int arg, int arg2, int arg3) {}
__weak int slbc_get_scmi_info(int uid, uint16_t cmd, void *ptr) {}
__weak int slbc_get_cache_user_pmu(int uid, void *ptr) {}
__weak int slbc_get_cache_user_status(int uid, void *ptr) {}
__weak int _slbc_request_cache_scmi(void *ptr) {}
__weak int _slbc_release_cache_scmi(void *ptr) {}
__weak int _slbc_buffer_status_scmi(void *ptr) {}
__weak int _slbc_request_buffer_scmi(void *ptr) {}
__weak int _slbc_release_buffer_scmi(void *ptr) {}
__weak void slbc_register_ipi_ops(struct slbc_ipi_ops *ops) {}
__weak void slbc_unregister_ipi_ops(struct slbc_ipi_ops *ops) {}
__weak int slbc_sspm_sram_update(void) {}
__weak int slbc_table_gid_set(int gid, int quota, int pri) {}
__weak int slbc_table_gid_release(int gid) {}
__weak int slbc_table_gid_get(int gid) {}
__weak int slbc_table_idt_set(int index, int arid, int idt) {}
__weak int slbc_table_idt_release(int index) {}
__weak int slbc_table_idt_get(int index) {}
__weak int slbc_table_gid_axi_set(int index, int axiid, int pg) {}
__weak int slbc_table_gid_axi_release(int index) {}
__weak int slbc_table_gid_axi_get(int index) {}
__weak int emi_slb_select(int argv1, int argv2, int argv3) {}
__weak int emi_pmu_counter(int argv1, int argv2, int argv3) {}
__weak int emi_pmu_set_ctrl(int argv1, int argv2, int argv3) {}
__weak int emi_gid_pmu_counter(int argv1, int argv2) {}
__weak int emi_pmu_read_counter(int idx) {}
__weak int emi_gid_pmu_read_counter(void *ptr) {}
__weak int emi_slc_test_result(void) {}
__weak int _slbc_ach_scmi(unsigned int cmd, enum slc_ach_uid uid, int gid,
			struct slbc_gid_data *data)
{
	return -EDISABLED;
}
#endif /* CONFIG_MTK_SLBC_IPI */

#endif /* _SLBC_IPI_H_ */

