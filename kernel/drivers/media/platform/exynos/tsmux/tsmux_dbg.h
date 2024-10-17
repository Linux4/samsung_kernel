/*
 * copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS TSMUX driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TSMUX_DBG_H
#define TSMUX_DBG_H

#define TSMUX_DBG_SFR	3
#define TSMUX_DBG		2
#define TSMUX_INFO		1
#define TSMUX_ERR		0

#define print_tsmux(level, fmt, args...)			\
	do {							\
		if ((g_tsmux_debug_level >= level) || (g_tsmux_log_level >= level))		\
		{									\
			pr_info("%s:%d: " fmt,			\
				__func__, __LINE__, ##args);	\
			wfd_logger_write(g_tsmux_logger, "%s:%d " fmt,	\
				__func__, __LINE__, ##args);	\
		}									\
	} while (0)
#endif
