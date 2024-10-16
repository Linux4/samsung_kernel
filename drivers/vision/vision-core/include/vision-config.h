/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef VISION_CONFIG_H_
#define VISION_CONFIG_H_

#define VISION_MAX_CONTAINERLIST	16
#define VISION_MAX_BUFFER		16
#define VISION_MAX_PLANE		8
#define VISION_MAP_KVADDR
#define VISION_MAX_FW_VERSION_LEN       200
#define VISION_MAX_FW_HASH_LEN       1000

#include "common/npu-log.h"

#define vprobe_info(fmt, ...)		pr_info("VIS:" fmt, ##__VA_ARGS__)
#define vprobe_warn(fmt, args...)	pr_warning("VIS:[WRN]" fmt, ##args)
#define vprobe_err(fmt, args...)	pr_err("VIS:[ERR]%s:%d:" fmt, __func__, __LINE__, ##args)

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define vision_err_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define vision_warn_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define vision_info_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define vision_dbg_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define vision_dump_target(fmt, ...)	npu_dump_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)  // dump

#define vision_err(fmt, args...) \
	vision_err_target("VIS:[" NPU_LOG_TAG "][ERR]%s(%d):" fmt, __func__, __LINE__, ##args)

#define vision_info(fmt, args...) \
	vision_info_target("VIS:[" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

#define vision_dbg(fmt, args...) \
	vision_dbg_target("VIS:[" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

#define vision_dump(fmt, args...) \
	vision_dump_target("VIS:[" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)
#else
#define vision_err_target(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define vision_warn_target(fmt, ...)	pr_warn(fmt, ##__VA_ARGS__)
#define vision_info_target(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define vision_dbg_target(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)

#define vision_err(fmt, args...) \
	vision_err_target("VIS:[ERR]%s:%d:" fmt, __func__, __LINE__, ##args)

#define vision_info(fmt, args...) \
	vision_info_target("VIS:" fmt, ##args)

#define vision_dbg(fmt, args...) \
	vision_dbg_target("VIS:" fmt, ##args)
#endif
#endif
