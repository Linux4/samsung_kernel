/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DP logger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DP_LOGGER_H_
#define _DP_LOGGER_H_

#define MAX_DPLOG_STR_LEN	160

#define SEND_OPT_TOAST	0
#define SEND_OPT_MESSAGE 1

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
extern void dp_logger_set_max_count(int count);
extern void dp_logger_print(const char *fmt, ...);
extern void dp_logger_hex_dump(void *buf, void *pref, size_t len);
extern int dp_logger_init(void);
extern void dp_enable_uevent(int enable);
extern void dp_print_log_to_uevent(char *str, int size, int option);
extern void dp_logger_print_to_toast(const char *fmt, ...);
#else
#define dp_logger_set_max_count(x)		do { } while (0)
#define dp_logger_print(fmt, ...)		do { } while (0)
#define dp_logger_hex_dump(buf, pref, len)	do { } while (0)
#define dp_logger_init()			do { } while (0)
#define dp_enable_uevent(enable)			do { } while (0)
#define dp_print_log_to_uevent(str, size, option)	do { } while (0)
#define dp_logger_print_to_toast(fmt, ...)		do { } while (0)
#endif

#endif

