/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SYSTEM_H_
#define _NPU_SYSTEM_H_

#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/hashtable.h>
#include "npu-scheduler.h"
#include "npu-qos.h"
#include "npu-clock.h"
#include "npu-wakeup.h"
#include "npu-interface.h"
#include "mailbox.h"
#include "npu-exynos.h"
#include "npu-memory.h"
#include "npu-binary.h"
#include "npu-precision.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include "npu-fw-test-handler.h"
#endif

#define NPU_SYSTEM_DEFAULT_CORENUM	1
#define NPU_SYSTEM_IRQ_MAX		7
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
#define NPU_AFM_IRQ_CNT			2
#else
#define NPU_AFM_IRQ_CNT			1
#endif

#if IS_ENABLED(CONFIG_NPU_AFM)
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
#define BUCK_CNT		2
#else
#define BUCK_CNT		1
#endif
#endif

#define NPU_SET_DEFAULT_LAYER   (0xffffffff)

#define NPU_FW_LOAD_SUCCESS	(0x5055E557)

#define NPU_S2D_MODE_ON		(0x20120102)
#define NPU_S2D_MODE_OFF	(0x20130201)

#define FW_32_BIT		"cpuon"
#define FW_64_BIT		"cpuon64"
#define FW_64_SYMBOL		"AIE64"
#define FW_64_SYMBOL_S		(0x5)
#define FW_SYMBOL_OFFSET	(0xF009)
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
struct fw_message;
#endif

struct iomem_reg_t {
	u32 vaddr;
	u32 paddr;
	u32 size;
};

struct npu_iomem_init_data {
	const char	*heapname;
	const char	*name;
	void	*area_info;       /* Save iomem result */
};

#define NPU_MAX_IO_DATA		50
struct npu_io_data {
	const char	*heapname;
	const char	*name;
	struct npu_iomem_area	*area_info;
};

#define NPU_MAX_MEM_DATA	32
struct npu_mem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
};

struct npu_rmem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
	struct reserved_mem	*rmem;
};

struct reg_cmd_list {
	char			*name;
	struct reg_cmd_map	*list;
	int			count;
};

#if (IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR) || IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2))
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
enum align {
	INCREASE_UPWARD,
	INCREASE_DOWNWARD,
};
#endif

struct imb_alloc_info {
	struct npu_memory_buffer	chunk[NPU_IMB_CHUNK_MAX_NUM];
	u32				alloc_chunk_cnt;
	struct npu_iomem_area		*chunk_imb;
	struct mutex			imb_alloc_lock;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	enum align alignment;
#endif
};

struct imb_size_control {
	int				result_code;
	u32				fw_size;
	wait_queue_head_t		waitq;
};
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
struct imb_memory_buffer {
	size_t IMB_size;
	struct npu_memory_buffer *IMB_mem_buf;
	unsigned long ss_state;
};
#endif

struct dsp_dhcp;
struct npu_interface_ops {
	int (*interface_probe)(struct device *dev, void *regs1, void *regs2, void *regs3);
	int (*interface_open)(struct npu_system *system);
	int (*interface_close)(struct npu_system *system);
};
extern const struct npu_interface_ops n_npu_interface_ops;

struct npu_system {
	struct platform_device	*pdev;
	struct npu_hw_device **hwdev_list;
	u32 hwdev_num;
	struct dsp_dhcp *dhcp;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	wait_queue_head_t dsp_wq;
	unsigned int dsp_system_flag;
#endif

	struct npu_io_data	io_area[NPU_MAX_IO_DATA];
	struct npu_mem_data	mem_area[NPU_MAX_MEM_DATA];
	struct npu_rmem_data	rmem_area[NPU_MAX_MEM_DATA];
	struct iommu_domain *domain;

	struct reg_cmd_list	*cmd_list;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct npu_fw_test_handler	npu_fw_test_handler;
#endif

	int			irq_num;
	int			afm_irq_num;
	int			irq[NPU_SYSTEM_IRQ_MAX];

	struct npu_qos_setting	qos_setting;

	u32			max_npu_core;
	struct npu_clocks	clks;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* maintain to be awake */
	struct wakeup_source	*ws;
#endif

	struct npu_exynos exynos;
	struct npu_memory memory;
	struct npu_binary binary;

	volatile struct mailbox_hdr	*mbox_hdr;
	volatile struct npu_interface	*interface;

#if (IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR) || IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2))
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	struct imb_alloc_info		imb_alloc_data;
#elif IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	struct imb_alloc_info		imb_alloc_data[2];
#endif
	struct imb_size_control		imb_size_ctl;
#endif

#if IS_ENABLED(CONFIG_NPU_STM)
	unsigned int fw_load_success;
#endif
	/* Open status (Bitfield of npu_system_resume_steps) */
	unsigned long			resume_steps;
	unsigned long			resume_soc_steps;
	unsigned long			saved_warm_boot_flag;

	unsigned int layer_start;
	unsigned int layer_end;

#ifdef CONFIG_NPU_AFM
	struct i2c_client               *i2c;
	struct workqueue_struct		*afm_dnc_wq;
	struct delayed_work		afm_dnc_work;
	struct workqueue_struct		*afm_gnpu1_wq;
	struct delayed_work		afm_gnpu1_work;
	struct workqueue_struct		*afm_restore_dnc_wq;
	struct delayed_work		afm_restore_dnc_work;
	struct workqueue_struct		*afm_restore_gnpu1_wq;
	struct delayed_work		afm_restore_gnpu1_work;
	u32 afm_max_lock_level[BUCK_CNT];
	u32 afm_buck_offset[BUCK_CNT];
	u32 afm_buck_level[BUCK_CNT];
	atomic_t ocp_warn_status[BUCK_CNT];
	atomic_t restore_status[BUCK_CNT];
	atomic_t afm_state;
	u32 afm_mode;
#endif
	bool fw_cold_boot;
	u32 s2d_mode;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
		struct imb_memory_buffer imb_bufl;
		struct imb_memory_buffer imb_bufh;
#endif
	unsigned long default_affinity;
	const struct npu_interface_ops *interface_ops;

#if IS_ENABLED(CONFIG_SOC_S5E9945)
	spinlock_t token_lock;
	spinlock_t token_wq_lock;
	bool token;
	bool token_fail;
	atomic_t dvfs_token_cnt;
	u32 backup_token_req_cnt;
	u32 token_req_cnt;
	u32 token_res_cnt;
	u32 token_dvfs_req_cnt;
	u32 token_dvfs_res_cnt;
	u32 token_work_cnt;
	volatile u32 token_intr_cnt;
	volatile u32 token_clear_cnt;
	struct workqueue_struct		*token_wq;
	struct delayed_work		token_work;
#endif
	struct workqueue_struct		*precision_wq;
	struct delayed_work		precision_work;
	DECLARE_HASHTABLE(precision_info_hash_head, 8);
	DECLARE_HASHTABLE(precision_active_hash_head, 8);
	struct npu_precision_model_info	model_info[PRECISION_LEN];
	struct npu_precision_model_info	active_info[PRECISION_LEN];
	struct mutex model_lock;
	u32 precision_index;

#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	struct workqueue_struct *wq;
	struct work_struct work_report;

	u32 enter_suspend;
#endif
};

static inline struct npu_io_data *npu_get_io_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (!strcmp(name, system->io_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_io_data *)NULL;
	else
		return (system->io_area + t);
}
static inline struct npu_iomem_area *npu_get_io_area(struct npu_system *system, const char *name)
{
	struct npu_io_data *t;

	t = npu_get_io_data(system, name);
	return t ? t->area_info : (struct npu_iomem_area *)NULL;
}

static inline struct npu_mem_data *npu_get_mem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->mem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->mem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_mem_data *)NULL;
	else
		return (system->mem_area + t);
}

static inline struct npu_rmem_data *npu_get_rmem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->rmem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->rmem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_rmem_data *)NULL;
	else
		return (system->rmem_area + t);
}

static inline struct npu_memory_buffer *npu_get_mem_area(struct npu_system *system, const char *name)
{
	struct npu_mem_data *t;
	struct npu_rmem_data *tr;

	t = npu_get_mem_data(system, name);
	if (t)
		return t->area_info;
	tr = npu_get_rmem_data(system, name);
	if (tr)
		return tr->area_info;
	return (struct npu_memory_buffer *)NULL;
}

static inline int get_iomem_data_index(const struct npu_iomem_init_data data[], const char *name)
{
	int i;

	for (i = 0; data[i].name != NULL; i++) {
		if (!strcmp(data[i].name, name))
			return i;
	}
	return -1;
}

static inline struct reg_cmd_list *get_npu_cmd_map(struct npu_system *system, const char *cmd_name)

{
	int i;

	for (i = 0; ((system->cmd_list) + i)->name != NULL; i++) {
		if (!strcmp(((system->cmd_list) + i)->name, cmd_name))
			return (system->cmd_list + i);
	}
	return (struct reg_cmd_list *)NULL;
}

int npu_system_probe(struct npu_system *system, struct platform_device *pdev);
int npu_system_release(struct npu_system *system, struct platform_device *pdev);
int npu_system_open(struct npu_system *system);
int npu_system_close(struct npu_system *system);
int npu_system_resume(struct npu_system *system);
int npu_system_suspend(struct npu_system *system);
int npu_system_start(struct npu_system *system);
int npu_system_stop(struct npu_system *system);

void npu_memory_sync_for_cpu(void);
void npu_memory_sync_for_device(void);
void npu_soc_status_report(struct npu_system *system);
void fw_print_log2dram(struct npu_system *system, u32 len);
int __alloc_imb_chunk(struct npu_memory_buffer *IMB_mem_buf, struct npu_system *system,
	struct imb_alloc_info *imb_range, u32 req_chunk_cnt);
void __free_imb_chunk(u32 new_chunk_cnt, struct npu_system *system,
	struct imb_alloc_info *imb_range);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int npu_fw_imb_alloc(struct fw_message *fw_msg, struct npu_system *system);
int npu_fw_imb_dealloc(struct fw_message *fw_msg, struct npu_system *system);
#endif
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int dsp_system_load_binary(struct npu_system *system);
int dsp_system_wait_reset(struct npu_system *system);
#endif
#endif
