/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_LOG_H__
#define __CAMELLIA_LOG_H__

#include <linux/device.h>

#define LOG_TAG "[camellia]"
#define LOG_FMT " %s:%s:%d: "
#define LOG_CAMELLIA "log_camellia"
#define CAMELLIA_TAG "[CAMELLIA_SSP]"

#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#ifdef __FILE_NAME__
#define FILE_NAME __FILE_NAME__
#else
#define FILE_NAME __FILE__
#endif

#define LOG_DEV_DBG(dev, fmt, ...)                                            \
	dev_dbg(dev, LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		##__VA_ARGS__)
#define LOG_DEV_INFO(dev, fmt, ...)                                            \
	dev_info(dev, LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		 ##__VA_ARGS__)
#define LOG_DEV_NOTICE(dev, fmt, ...)                                  \
	dev_notice(dev, LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, \
		   __LINE__, ##__VA_ARGS__)
#define LOG_DEV_WARN(dev, fmt, ...)                                            \
	dev_warn(dev, LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		 ##__VA_ARGS__)
#define LOG_DEV_ERR(dev, fmt, ...)                                            \
	dev_err(dev, LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		##__VA_ARGS__)

#define LOG_DEV_ENTRY(dev) \
	dev_err(dev, LOG_TAG LOG_FMT "ENTRY\n", FILE_NAME, __func__, __LINE__)
#define LOG_DEV_EXIT(dev) \
	dev_err(dev, LOG_TAG LOG_FMT "EXIT\n", FILE_NAME, __func__, __LINE__)

#define LOG_DBG(fmt, ...)                                                 \
	pr_debug(LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		 ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                               \
	pr_info(LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		##__VA_ARGS__)
#define LOG_NOTICE(fmt, ...)                                               \
	pr_notice(LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		  ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                               \
	pr_warn(LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
		##__VA_ARGS__)
#define LOG_ERR(fmt, ...)                                               \
	pr_err(LOG_TAG LOG_FMT fmt "\n", FILE_NAME, __func__, __LINE__, \
	       ##__VA_ARGS__)

#define LOG_ENTRY \
	pr_debug(LOG_TAG LOG_FMT "ENTRY\n", FILE_NAME, __func__, __LINE__)
#define LOG_EXIT \
	pr_debug(LOG_TAG LOG_FMT "EXIT\n", FILE_NAME, __func__, __LINE__)

#define CAMELLIA_PRINT(fmt, ...)                                               \
	pr_info(CAMELLIA_TAG fmt "\n", \
		##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DUMP_BUF(buf, len)                                               \
	print_hex_dump(KERN_DEBUG, "[camellia] ", DUMP_PREFIX_OFFSET, 16, 1, \
		       buf, len, true)
#else
#define LOG_DUMP_BUF(buf, len) \
	do {                   \
	} while (0)
#endif

#endif /* __CAMELLIA_LOG_H__ */
