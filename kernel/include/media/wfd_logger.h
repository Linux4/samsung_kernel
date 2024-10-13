/*
 * copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS WFD logger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TSMUX_LOGGER_H
#define TSMUX_LOGGER_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ktime.h>

struct wfd_logger_info {
	char *log_buf;
	uint32_t size;
	uint32_t offset;
	spinlock_t logger_lock;
	bool rewind;
};

static inline void wfd_rewind_logging_buffer(struct wfd_logger_info *logger_info)
{
	if (logger_info->offset >= logger_info->size) {
		pr_info("%s:%d: rewind logging buffer\n", __func__, __LINE__);
		logger_info->offset = 0;
		logger_info->rewind = true;
	}
}

static inline struct wfd_logger_info* wfd_logger_init(void)
{
    struct wfd_logger_info *logger_info = NULL;

	pr_info("%s++\n", __func__);
	logger_info = kzalloc(sizeof(struct wfd_logger_info), GFP_KERNEL);
	logger_info->log_buf = kzalloc(SZ_1M, GFP_KERNEL);
	logger_info->size = SZ_1M;
	logger_info->rewind = false;
	spin_lock_init(&logger_info->logger_lock);
	pr_info("%s--\n", __func__);

    return logger_info;
}

static inline void wfd_logger_deinit(struct wfd_logger_info *logger_info)
{
	pr_info("%s++\n", __func__);
	if (logger_info) {
		if (logger_info->log_buf) {
			kfree(logger_info->log_buf);
			logger_info->log_buf = NULL;
		}
		kfree(logger_info);
	}
	pr_info("%s--\n", __func__);
}

static inline void wfd_logger_write(struct wfd_logger_info *logger_info, char *format, ...)
{
	va_list args;
	unsigned long flags;
	struct timespec64 tv;

	if (logger_info == NULL || logger_info->log_buf == NULL)
		return;

	tv = ktime_to_timespec64(ktime_get());

	spin_lock_irqsave(&logger_info->logger_lock, flags);
	logger_info->offset += snprintf(logger_info->log_buf + logger_info->offset,
			logger_info->size - logger_info->offset,
			"[%6lld.%06ld] ", tv.tv_sec, (tv.tv_nsec / 1000));
	wfd_rewind_logging_buffer(logger_info);

	va_start(args, format);
	logger_info->offset += vsnprintf(logger_info->log_buf + logger_info->offset,
			logger_info->size - logger_info->offset, format, args);
	va_end(args);
	wfd_rewind_logging_buffer(logger_info);

	spin_unlock_irqrestore(&logger_info->logger_lock, flags);
}

#endif
