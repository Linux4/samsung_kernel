/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_RESOURCE_MGR_H
#define IS_RESOURCE_MGR_H

#include <linux/notifier.h>
#include <linux/reboot.h>
#include "is-hw-config.h"
#include "exynos_is_dt.h"
#include "pablo-mem.h"
#include "pablo-crta-bufmgr.h"

#define RESOURCE_TYPE_SENSOR0	0
#define RESOURCE_TYPE_SENSOR1	1
#define RESOURCE_TYPE_SENSOR2	2
#define RESOURCE_TYPE_SENSOR3	3
#define RESOURCE_TYPE_SENSOR4	4
#define RESOURCE_TYPE_SENSOR5	5
#define RESOURCE_TYPE_ISCHAIN	6
#define RESOURCE_TYPE_MAX	8

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#define is_pm_qos_request		exynos_pm_qos_request
#define is_pm_qos_add_request		exynos_pm_qos_add_request
#define is_pm_qos_update_request	exynos_pm_qos_update_request
#define is_pm_qos_remove_request	exynos_pm_qos_remove_request
#define is_pm_qos_request_active	exynos_pm_qos_request_active
#else
#define is_pm_qos_request		dev_pm_qos_request
#define is_pm_qos_add_request(arg...)
#define is_pm_qos_update_request(arg...)
#define is_pm_qos_remove_request(arg...)
#define is_pm_qos_request_active(arg...) 0
#endif

enum is_resourcemgr_state {
	IS_RM_COM_POWER_ON,
	IS_RM_SS0_POWER_ON,
	IS_RM_SS1_POWER_ON,
	IS_RM_SS2_POWER_ON,
	IS_RM_SS3_POWER_ON,
	IS_RM_SS4_POWER_ON,
	IS_RM_SS5_POWER_ON,
	IS_RM_ISC_POWER_ON,
	IS_RM_POWER_ON
};

enum is_dvfs_state {
	IS_DVFS_TMU_THROTT,
	IS_DVFS_TICK_THROTT
};

enum is_pm_qos_req {
	IS_PM_QOS_CPU_ONLINE_MIN = IS_DVFS_END,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	IS_PM_QOS_CLUSTER0_MIN,
	IS_PM_QOS_CLUSTER0_MAX,
	IS_PM_QOS_CLUSTER1_MIN,
	IS_PM_QOS_CLUSTER1_MAX,
	IS_PM_QOS_CLUSTER2_MIN,
	IS_PM_QOS_CLUSTER2_MAX,
#endif
	IS_PM_QOS_MAX,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
enum is_freq_qos_req {
	IS_FREQ_QOS_CLUSTER0_MIN,
	IS_FREQ_QOS_CLUSTER0_MAX,
	IS_FREQ_QOS_CLUSTER1_MIN,
	IS_FREQ_QOS_CLUSTER1_MAX,
	IS_FREQ_QOS_CLUSTER2_MIN,
	IS_FREQ_QOS_CLUSTER2_MAX,
	IS_FREQ_QOS_MAX,
};
#endif

/* Reserved memory data */
struct is_resource_rmem {
	void *virt_addr;
	unsigned long phys_addr;
	size_t size;
};

struct is_dvfs_ctrl {
	struct mutex lock;
	u32 cur_lv[IS_DVFS_END];
	int cur_hpg_qos;
	int cur_hmp_bst;
	u32 dvfs_scenario;
	u32 dvfs_rec_size;
	ulong state;
	const char *cur_cpus;

	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;

	u32 cur_instance;
	u32 dec_dtime;
	struct delayed_work dec_dwork;

	bool bundle_operating;
};

struct is_resource {
        struct platform_device                  *pdev;
        void __iomem                            *regs;
        atomic_t                                rsccount;
        u32                                     private_data;
};

#define LLC_DISABLE	0
struct is_global_param {
	struct mutex				lock;
	bool					video_mode;
	ulong					llc_state;
	ulong					state;
	atomic_t				sensor_cnt;
};

struct is_lic_sram {
	size_t					taa_sram[10];
	atomic_t				taa_sram_sum;
};

struct is_bts_scen {
	unsigned int		index;
	const char		*name;
};

struct is_resourcemgr {
	unsigned long				state;
	atomic_t				rsccount;
	atomic_t				qos_refcount; /* For multi-instance with SOC sensor */
	struct is_resource			resource_preproc;
	struct is_resource			resource_sensor0;
	struct is_resource			resource_sensor1;
	struct is_resource			resource_sensor2;
	struct is_resource			resource_sensor3;
	struct is_resource			resource_sensor4;
	struct is_resource			resource_sensor5;
	struct is_resource			resource_ischain;

	struct is_mem				mem;
	struct is_minfo				*minfo;
	struct pablo_crta_bufmgr		crta_bufmgr[IS_STREAM_COUNT];

	struct is_dvfs_ctrl			dvfs_ctrl;
	u32					cluster0;
	u32					cluster1;
	u32					cluster2;

	/* tmu */
	struct notifier_block			tmu_notifier;
	u32					tmu_state;
	u32					limited_fps;

	/* bus monitor */
	struct notifier_block			itmon_notifier;
	unsigned int				itmon_port_num;
	const char				**itmon_port_name;

	void					*private_data;

#ifdef ENABLE_KERNEL_LOG_DUMP
	unsigned long long			kernel_log_time;
	void					*kernel_log_buf;
#endif
	ulong					sfrdump_ptr;
	struct is_global_param			global_param;

	struct is_lic_sram			lic_sram;

	/* for critical section at get/put */
	struct mutex				rsc_lock;
	/* for sysreg setting */
	struct mutex				sysreg_lock;
	/* for qos setting */
	struct mutex				qos_lock;

	/* BTS */
	struct is_bts_scen			*bts_scen;
	u32					num_bts_scen;
	int					cur_bts_scen_idx;

	u32					shot_timeout;
	int					shot_timeout_tick;

#if defined(DISABLE_CORE_IDLE_STATE)
	struct work_struct                      c2_disable_work;
#endif
	u32					streaming_cnt;

	struct list_head			regulator_list;

	struct regulator			**phy_ldos;
	int					num_phy_ldos;
};

struct emstune_mode_request *is_get_emstune_req(void);
struct is_pm_qos_request *is_get_pm_qos(void);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
struct freq_qos_request *is_get_freq_qos(void);
#endif
int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr, void *private_data, struct platform_device *pdev);
int is_resource_open(struct is_resourcemgr *resourcemgr, u32 rsc_type, void **device);
int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl);
int is_logsync(u32 sync_id, u32 msg_test_id);
void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device);
void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device);
int is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, void *device, bool on);
int is_resource_dump(void);
int is_kernel_log_dump(bool overwrite);
#ifdef ENABLE_HWACG_CONTROL
extern void is_hw_csi_qchannel_enable_all(bool enable);
#endif
void is_bts_scen(struct is_resourcemgr *resourcemgr,
	unsigned int index, bool enable);

int is_resource_cdump(void);

#define GET_RESOURCE(resourcemgr, type) \
	((type == RESOURCE_TYPE_SENSOR0) ? &resourcemgr->resource_sensor0 : \
	((type == RESOURCE_TYPE_SENSOR1) ? &resourcemgr->resource_sensor1 : \
	((type == RESOURCE_TYPE_SENSOR2) ? &resourcemgr->resource_sensor2 : \
	((type == RESOURCE_TYPE_SENSOR3) ? &resourcemgr->resource_sensor3 : \
	((type == RESOURCE_TYPE_SENSOR4) ? &resourcemgr->resource_sensor4 : \
	((type == RESOURCE_TYPE_SENSOR5) ? &resourcemgr->resource_sensor5 : \
	((type == RESOURCE_TYPE_ISCHAIN) ? &resourcemgr->resource_ischain : \
	NULL)))))))

/* cache maintenance for user buffer */
#define MAX_DBUF_LIST	10

struct is_dbuf_list {
	struct list_head	list;
	struct dma_buf		*dbuf[IS_MAX_PLANES];
};

struct is_dbuf_q {
	struct is_dbuf_list	*dbuf_list[ID_DBUF_MAX];
	struct mutex		lock[ID_DBUF_MAX];

	u32			queu_count[ID_DBUF_MAX];
	struct list_head	queu_list[ID_DBUF_MAX];

	u32			free_count[ID_DBUF_MAX];
	struct list_head	free_list[ID_DBUF_MAX];
};

struct is_dbuf_q *is_init_dbuf_q(void);
void is_deinit_dbuf_q(struct is_dbuf_q *dbuf_q);
void is_q_dbuf_q(struct is_dbuf_q *dbuf_q, struct is_sub_dma_buf *sdbuf, u32 qcnt);
void is_dq_dbuf_q(struct is_dbuf_q *dbuf_q, u32 dma_id, enum dma_data_direction dir);

u32 is_get_shot_timeout(void);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_resourcemgr_init_dynamic_mem(struct is_resourcemgr *resourcemgr);
int pablo_kunit_resourcemgr_deinit_dynamic_mem(struct is_resourcemgr *resourcemgr);
#endif

#endif
