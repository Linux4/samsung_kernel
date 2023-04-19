/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Samsung Abox Logging API with memlog
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SND_SOC_ABOX_MEMLOG_H
#define __SND_SOC_ABOX_MEMLOG_H

#include <linux/device.h>
#include <soc/samsung/exynos/memlogger.h>
#include <soc/samsung/exynos/sysevent.h>
#include "abox.h"

/**
 * Print a log through the memlog module.
 * @param[in]	data		abox data
 * @param[in]	level		log level
 * @param[in]	fmt		print format
 * @param[in]	...		additional arguments
 * @return	0 or error code
 */
extern int abox_memlog_printf(struct abox_data *data, int level, const char *fmt, ...);

#define abox_dev_printf(dev, level, fmt, args...)			\
	do {								\
		if (dev)						\
			abox_memlog_printf(abox_get_data(dev), level,	\
					"[%s] " fmt, dev_name(dev),	\
					##args);			\
	} while (0)

#if defined(VERBOSE_DEBUG)
#define abox_vdbg(dev, fmt, args...)					\
	do {								\
		dev_vdbg(dev, fmt, ##args);				\
		abox_dev_printf(dev, MEMLOG_LEVEL_DEBUG, fmt, ##args);	\
	} while (0)
#else
#define abox_vdbg(dev, fmt, args...)					\
	do {								\
		dev_vdbg(dev, fmt, ##args);				\
	} while (0)
#endif

#if defined(DEBUG)
#define abox_dbg(dev, fmt, args...)					\
	do {								\
		dev_dbg(dev, fmt, ##args);				\
		abox_dev_printf(dev, MEMLOG_LEVEL_DEBUG, fmt, ##args);	\
	} while (0)
#else
#define abox_dbg(dev, fmt, args...)					\
	do {								\
		dev_dbg(dev, fmt, ##args);				\
	} while (0)
#endif

#define abox_info(dev, fmt, args...)					\
	do {								\
		dev_info(dev, fmt, ##args);				\
		abox_dev_printf(dev, MEMLOG_LEVEL_INFO, fmt, ##args);	\
	} while (0)

#define abox_warn(dev, fmt, args...)					\
	do {								\
		dev_warn(dev, fmt, ##args);				\
		abox_dev_printf(dev, MEMLOG_LEVEL_CAUTION, fmt, ##args);\
	} while (0)

#define abox_err(dev, fmt, args...)					\
	do {								\
		dev_err(dev, fmt, ##args);				\
		abox_dev_printf(dev, MEMLOG_LEVEL_ERR, fmt, ##args);	\
	} while (0)

#define abox_sysevent_get(dev)						\
	do {								\
		struct abox_data *abox_data = abox_get_data(dev);	\
		if (abox_data->sysevent_dev)				\
			sysevent_get(abox_data->sysevent_desc.name);	\
	} while (0)

#define abox_sysevent_put(dev)						\
	do {								\
		struct abox_data *abox_data = abox_get_data(dev);	\
		if (abox_data->sysevent_dev)				\
			sysevent_put(abox_data->sysevent_dev);		\
	} while (0)

#endif /* __SND_SOC_ABOX_MEMLOG_H */
