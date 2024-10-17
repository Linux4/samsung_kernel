/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_DEVICE_H_
#define _NPU_DEVICE_H_

#include <linux/notifier.h>
#include <linux/completion.h>

#include "npu-log.h"
#include "npu-debug.h"
#include "npu-vertex.h"
#include "npu-system.h"
#include "npu-syscall.h"
#include "npu-protodrv.h"
#include "npu-sessionmgr.h"

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif

#include "npu-core.h"
#include "npu-scheduler.h"
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "dsp-kernel.h"
#endif
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
#include "npu-fence.h"
#endif
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif

#define NPU_DEVICE_NAME	"npu-turing"

#if IS_ENABLED(CONFIG_SOC_S5E9945)
#define	NPU_DEVICE_PWR_ACTIVE		(0xCAFECAFE)
#define	NPU_DEVICE_PWR_DEACTIVE	(0x0)
#endif

enum npu_device_state {
	NPU_DEVICE_STATE_OPEN,
	NPU_DEVICE_STATE_START
};

enum npu_device_mode {
	NPU_DEVICE_MODE_NORMAL,
	NPU_DEVICE_MODE_TEST
};

enum npu_device_err_state {
	NPU_DEVICE_ERR_STATE_EMERGENCY
};

struct npu_hw_device;

struct npu_device {
	struct device *dev;
	unsigned long state;
	unsigned long err_state;
	struct npu_system system;
	struct npu_hw_device **hdevs;
	struct npu_vertex vertex;
	struct completion my_completion;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	struct npu_fence_device fence;
#endif
	struct npu_debug debug;
	// struct npu_proto_drv *proto_drv;
	struct npu_sessionmgr sessionmgr;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	struct dsp_kernel_manager kmgr;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	struct notifier_block itmon_nb;
#endif
	struct npu_scheduler_info *sched;
	int magic;
	struct mutex start_stop_lock;
	u32 is_secure;
	u32 active_non_secure_sessions;
#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION) && IS_ENABLED(CONFIG_SOC_S5E9945)
	struct notifier_block noti_data;
#endif
	/* For EVT0 , TODO : Sperate EVT0 and EVT1 */
	struct device ncp_sync_dev;
	/* For EVT0 */
	struct workqueue_struct		*npu_log_wq;
	struct delayed_work		npu_log_work;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct cmdq_table_info cmdq_table_info;
#endif
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	atomic_t power_active;
#endif
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	u32 is_first;
	struct npu_session *first_session;
#endif
};

int npu_device_open(struct npu_device *device);
int npu_device_close(struct npu_device *device);
int npu_device_start(struct npu_device *device);
int npu_device_stop(struct npu_device *device);
int npu_device_bootup(struct npu_device *device);
int npu_device_shutdown(struct npu_device *device);
int check_emergency(struct npu_device *dev);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int npu_imb_alloc(struct npu_system *system);
int npu_imb_dealloc(struct npu_system *system);
#endif

//int npu_device_emergency_recover(struct npu_device *device);
void npu_device_set_emergency_err(struct npu_device *device);
int npu_device_is_emergency_err(struct npu_device *device);
int npu_device_recovery_close(struct npu_device *device);
#endif // _NPU_DEVICE_H_
