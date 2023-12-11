/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __HW_FENCE_DRV_DEBUG
#define __HW_FENCE_DRV_DEBUG

enum hw_fence_drv_prio {
	HW_FENCE_HIGH = 0x000001,	/* High density debug messages (noisy) */
	HW_FENCE_LOW = 0x000002,	/* Low density debug messages */
	HW_FENCE_INFO = 0x000004,	/* Informational prints */
	HW_FENCE_INIT = 0x00008,	/* Initialization logs */
	HW_FENCE_QUEUE = 0x000010,	/* Queue logs */
	HW_FENCE_LUT = 0x000020,	/* Look-up and algorithm logs */
	HW_FENCE_IRQ = 0x000040,	/* Interrupt-related messages */
	HW_FENCE_PRINTK = 0x010000,
};

extern u32 msm_hw_fence_debug_level;

#define dprintk(__level, __fmt, ...) \
	do { \
		if (msm_hw_fence_debug_level & __level) \
			if (msm_hw_fence_debug_level & HW_FENCE_PRINTK) \
				pr_err(__fmt, ##__VA_ARGS__); \
	} while (0)


#define HWFNC_ERR(fmt, ...) \
	pr_err("[hwfence:%s:%d][err][%pS] "fmt, __func__, __LINE__, \
	__builtin_return_address(0), ##__VA_ARGS__)

#define HWFNC_DBG_H(fmt, ...) \
	dprintk(HW_FENCE_HIGH, "[hwfence:%s:%d][dbgh]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_L(fmt, ...) \
	dprintk(HW_FENCE_LOW, "[hwfence:%s:%d][dbgl]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_INFO(fmt, ...) \
	dprintk(HW_FENCE_INFO, "[hwfence:%s:%d][dbgi]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_INIT(fmt, ...) \
	dprintk(HW_FENCE_INIT, "[hwfence:%s:%d][dbg]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_Q(fmt, ...) \
	dprintk(HW_FENCE_QUEUE, "[hwfence:%s:%d][dbgq]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_LUT(fmt, ...) \
	dprintk(HW_FENCE_LUT, "[hwfence:%s:%d][dbglut]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_DBG_IRQ(fmt, ...) \
	dprintk(HW_FENCE_IRQ, "[hwfence:%s:%d][dbgirq]"fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HWFNC_WARN(fmt, ...) \
	pr_warn("[hwfence:%s:%d][warn][%pS] "fmt, __func__, __LINE__, \
	__builtin_return_address(0), ##__VA_ARGS__)

int hw_fence_debug_debugfs_register(struct hw_fence_driver_data *drv_data);

#endif /* __HW_FENCE_DRV_DEBUG */
