// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_RESOURCE_MGR_H
#define PABLO_RESOURCE_MGR_H

#include "pablo-dvfs.h"
#include "pablo-mem.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-lib.h"

#include <linux/of_reserved_mem.h>

#define RESOURCE_TYPE_SENSOR0	0
#define RESOURCE_TYPE_SENSOR1	1
#define RESOURCE_TYPE_SENSOR2	2
#define RESOURCE_TYPE_SENSOR3	3
#define RESOURCE_TYPE_SENSOR4	4
#define RESOURCE_TYPE_SENSOR5	5
#define RESOURCE_TYPE_ISCHAIN	6
#define RESOURCE_TYPE_MAX	8

#define CLUSTER_MIN_MASK	0x0000FFFF
#define CLUSTER_MIN_SHIFT	0
#define CLUSTER_MAX_MASK	0xFFFF0000
#define CLUSTER_MAX_SHIFT	16

enum is_freq_qos_req {
	IS_FREQ_QOS_CLUSTER0_MIN,
	IS_FREQ_QOS_CLUSTER0_MAX,
	IS_FREQ_QOS_CLUSTER1_MIN,
	IS_FREQ_QOS_CLUSTER1_MAX,
	IS_FREQ_QOS_CLUSTER1_HF_MIN,
	IS_FREQ_QOS_CLUSTER1_HF_MAX,
	IS_FREQ_QOS_CLUSTER2_MIN,
	IS_FREQ_QOS_CLUSTER2_MAX,
	IS_FREQ_QOS_MAX,
};

enum is_addr_type {
	KVADDR_TYPE,
	PHADDR_TYPE,
	DVADDR_TYPE,
};

struct is_resource {
        struct platform_device		*pdev;
        atomic_t			rsccount;
};

#define LLC_DISABLE	0
struct is_global_param {
	struct mutex			lock;
	ulong				llc_state;
	ulong				state;
	atomic_t			sensor_cnt;
};

struct is_lic_sram {
	size_t				taa_sram[10];
	atomic_t			taa_sram_sum;
};

struct is_bts_scen {
	unsigned int			index;
	const char			*name;
};

struct is_dev_resource {
	u32	setfile;
	bool	llc_mode; /* 0: LLC off, 1: LLC on */
	u32	instance; /* logical stream id */
	u32	taa_id;
	u32	width;
};

struct is_resourcemgr {
	unsigned long			state;
	atomic_t			rsccount;
	atomic_t			qos_refcount; /* For multi-instance with SOC sensor */
	struct is_resource		resource_preproc;
	struct is_resource		resource_device[RESOURCE_TYPE_MAX];

	struct is_mem			mem;
	struct is_minfo			*minfo;
	struct pablo_crta_bufmgr	crta_bufmgr[IS_STREAM_COUNT];

	struct is_dvfs_ctrl		dvfs_ctrl;
	u32				cluster[PABLO_CPU_CLUSTER_MAX];

	/* tmu */
	struct notifier_block		tmu_notifier;
	u32				tmu_state;
	u32				limited_fps;

	/* bus monitor */
	struct notifier_block		itmon_notifier;
	unsigned int			itmon_port_num;
	const char			**itmon_port_name;
	void				*private_data;

#ifdef ENABLE_KERNEL_LOG_DUMP
	unsigned long long		kernel_log_time;
	void				*kernel_log_buf;
#endif
	ulong				sfrdump_ptr;
	struct is_global_param		global_param;
	struct is_lic_sram		lic_sram;

	/* for critical section at get/put */
	struct mutex			rsc_lock;
	/* for sysreg setting */
	struct mutex			sysreg_lock;
	/* for qos setting */
	struct mutex			qos_lock;

	/* BTS */
	struct is_bts_scen		*bts_scen;
	u32				num_bts_scen;
	int				cur_bts_scen_idx;

	u32				shot_timeout;
	int				shot_timeout_tick;

#if defined(DISABLE_CORE_IDLE_STATE)
	struct work_struct		c2_disable_work;
#endif
	u32				streaming_cnt;

	struct list_head		regulator_list;

	struct regulator		**phy_ldos;
	int				num_phy_ldos;
	u32				scenario;
#if IS_ENABLED(CONFIG_CMU_EWF)
	unsigned int			idx_ewf;
#endif
	struct is_dev_resource		dev_resource;
};

struct pablo_rscmgr_ops {
	unsigned long (*set_addr)(struct is_priv_buf *pb, enum is_addr_type type);
	void (*configure_llc)(bool on, void *ischain, ulong *llc_state);
};

struct pablo_rscmgr_sys_ops {
	void *(*get_log_kernel)(void);
	unsigned int (*bts_scenidx)(const char *name);
	int (*bts_add_scen)(unsigned int scen_idx);
	int (*bts_del_scen)(unsigned int scen_idx);
	void *(*vmap)(struct page **pages, unsigned int npages, unsigned long type, pgprot_t prot);
};

int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl);
int is_logsync(u32 sync_id);
void is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, bool on);
int is_kernel_log_dump(struct is_resourcemgr *resourcemgr, bool overwrite);
void is_bts_scen(struct is_resourcemgr *resourcemgr, unsigned int index, bool enable);
u32 is_get_shot_timeout(struct is_resourcemgr *resourcemgr);

int pablo_rscmgr_init_log_rmem(struct is_resourcemgr *resourcemgr, struct reserved_mem *rmem);
int is_resourcemgr_init_mem(struct is_resourcemgr *resourcemgr);
int is_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr);
int is_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr);
int is_resourcemgr_init_secure_mem(struct is_resourcemgr *resourcemgr);
int is_resourcemgr_deinit_secure_mem(struct is_resourcemgr *resourcemgr);
void pablo_resource_clear_cluster_qos(struct is_resourcemgr *resourcemgr);

void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device);
void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device);

struct pablo_rscmgr_ops *pablo_get_rscmgr_ops(void);
struct pablo_rscmgr_sys_ops *pablo_get_rscmgr_sys_ops(void);
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pkt_rscmgr_ops {
	void (*get_sensor)(struct is_resourcemgr *, u32, struct is_resource *);
	void (*put_sensor)(struct is_resourcemgr *, u32, struct is_resource *);
	int (*get_ischain)(struct is_resourcemgr *);
	void (*put_ischain)(struct is_resourcemgr *);
};
struct pkt_rscmgr_ops *pablo_kunit_rscmgr(void);
#endif
#endif
