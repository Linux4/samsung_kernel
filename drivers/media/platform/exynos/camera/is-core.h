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

#ifndef IS_CORE_H
#define IS_CORE_H

#include <linux/version.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/rt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>

#include "is-interface.h"
#include "is-hw.h"
#include "is-spi.h"
#include "is-vender.h"

#define IS_DRV_NAME			"exynos-is"
#define IS_COMMAND_TIMEOUT			(30*HZ)
#define IS_FLITE_STOP_TIMEOUT		(3*HZ)

#define SIZE_VRA_INTERNEL_BUF		(0x00650000)

enum is_secure_camera_type {
	IS_SECURE_CAMERA_IRIS = 1,
	IS_SECURE_CAMERA_FACE = 2,
};

enum is_secure_state {
	IS_STATE_UNSECURE,
	IS_STATE_SECURING,
	IS_STATE_SECURED,
};

enum is_hal_debug_mode {
	IS_HAL_DEBUG_SUDDEN_DEAD_DETECT,
	IS_HAL_DEBUG_PILE_REQ_BUF,
	IS_HAL_DEBUG_NDONE_REPROCESSING,
};

struct is_core {
	struct platform_device		*pdev;
	atomic_t			rsccount;
	bool				shutdown;
	struct is_sysfs_debug		sysfs_debug;
	struct work_struct		wq_data_print_clk;

	/* depended on isp */
	struct exynos_platform_is	*pdata;

	struct is_resourcemgr		resourcemgr;
	struct is_groupmgr		groupmgr;
	struct is_interface		interface;

	struct mutex			sensor_lock;
	struct is_device_sensor		sensor[IS_SENSOR_COUNT];
	u32				chain_config;
	struct is_device_ischain	ischain[IS_STREAM_COUNT];

	struct is_hardware		hardware;
	struct is_interface_ischain	interface_ischain;

	struct v4l2_device		v4l2_dev;
	struct is_video			video_cmn;

	/* spi */
	struct is_spi			spi0;
	struct is_spi			spi1;

	struct mutex			ixc_lock[SENSOR_CONTROL_I2C_MAX];
	atomic_t			ixc_rsccount[SENSOR_CONTROL_I2C_MAX];

	spinlock_t			shared_rsc_slock[MAX_SENSOR_SHARED_RSC];
	atomic_t			shared_rsc_count[MAX_SENSOR_SHARED_RSC];

	struct is_vender		vender;
	struct mutex			secure_state_lock;
	unsigned long			secure_state;
	ulong				secure_mem_info[2];	/* size, addr */
	ulong				non_secure_mem_info[2];	/* size, addr */
	u32				scenario;

	unsigned long			sensor_map;
	struct ois_mcu_dev		*mcu;
	struct mutex			laser_lock;
	atomic_t			laser_refcount;
	bool noti_register_state;
};

struct device *is_get_is_dev(void);
struct is_core *is_get_is_core(void);
struct is_core *pablo_get_core_async(void);
int is_secure_func(struct is_core *core,
	struct is_device_sensor *device, u32 type, u32 scenario, ulong smc_cmd);
struct is_device_sensor *is_get_sensor_device(u32 device_id);
struct is_device_ischain *is_get_ischain_device(u32 instance);
void is_print_frame_dva(struct is_subdev *subdev);
void is_cleanup(struct is_core *core);
void is_register_iommu_fault_handler(struct device *dev);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct attribute_group *pablo_kunit_get_debug_attr_group(void);
#endif

#define CALL_POPS(s, op, args...) (((s) && (s)->pdata && (s)->pdata->op) ? ((s)->pdata->op(&(s)->pdev->dev)) : -EPERM)

#endif /* IS_CORE_H_ */
