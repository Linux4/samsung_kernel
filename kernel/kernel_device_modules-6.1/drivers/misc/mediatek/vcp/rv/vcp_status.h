/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef VCP_STATUS_H
#define VCP_STATUS_H

#include "vcp.h"

typedef phys_addr_t (*vcp_get_reserve_mem_phys_fp)(enum vcp_reserve_mem_id_t id);
typedef phys_addr_t (*vcp_get_reserve_mem_virt_fp)(enum vcp_reserve_mem_id_t id);
typedef int (*vcp_register_feature_fp)(enum feature_id id);
typedef int (*vcp_deregister_feature_fp)(enum feature_id id);
typedef unsigned int (*is_vcp_ready_fp)(enum vcp_core_id id);
typedef void (*vcp_A_register_notify_fp)(struct notifier_block *nb);
typedef void (*vcp_A_unregister_notify_fp)(struct notifier_block *nb);
typedef unsigned int (*vcp_cmd_fp)(enum vcp_cmd_id id, char *user);
typedef unsigned int (*is_vcp_suspending_fp)(void);

struct vcp_status_fp {
	vcp_get_reserve_mem_phys_fp	vcp_get_reserve_mem_phys;
	vcp_get_reserve_mem_virt_fp	vcp_get_reserve_mem_virt;
	vcp_register_feature_fp		vcp_register_feature;
	vcp_deregister_feature_fp	vcp_deregister_feature;
	is_vcp_ready_fp				is_vcp_ready;
	vcp_A_register_notify_fp	vcp_A_register_notify;
	vcp_A_unregister_notify_fp	vcp_A_unregister_notify;
	vcp_cmd_fp				vcp_cmd;
	is_vcp_suspending_fp		is_vcp_suspending;
};

typedef int (*mminfra_pwr_ptr)(void);
struct vcp_mminfra_on_off_st {
	mminfra_pwr_ptr mminfra_on;
	mminfra_pwr_ptr mminfra_off;
	int mminfra_ref;
};

typedef void (*mminfra_dump_ptr)(void);

extern int pwclkcnt;
extern bool is_suspending;
extern bool vcp_ao;
extern mminfra_dump_ptr mminfra_debug_dump;
int mmup_enable_count(void);
void vcp_set_fp(struct vcp_status_fp *fp);
void vcp_set_ipidev(struct mtk_ipi_device *ipidev);
struct mtk_ipi_device *vcp_get_ipidev(void);
phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id);
phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id);
int vcp_register_feature_ex(enum feature_id id);
int vcp_deregister_feature_ex(enum feature_id id);
unsigned int is_vcp_ready_ex(enum vcp_core_id id);
void vcp_A_register_notify_ex(struct notifier_block *nb);
void vcp_A_unregister_notify_ex(struct notifier_block *nb);
unsigned int vcp_cmd_ex(enum vcp_cmd_id id, char *user);
unsigned int is_vcp_suspending_ex(void);
void vcp_set_mminfra_cb(struct vcp_mminfra_on_off_st *str_ptr);
int vcp_register_mminfra_cb_ex(mminfra_pwr_ptr fpt_on, mminfra_pwr_ptr fpt_off,
	mminfra_dump_ptr mminfra_dump_func);

#endif
