/*
 * sprd_debuglog.h -- Sprd Debug Power data type description support.
 *
 * Copyright (C) 2020, 2021 unisoc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Debug Power data type description.
 */

#ifndef _SPRD_DEBUGLOG_CORE_H
#define _SPRD_DEBUGLOG_CORE_H

#include <linux/device.h>

/* Debug log event */
struct debug_data {
	void *data;
	int num;
};

struct debug_event {
	struct debug_data data;
	int (*handle)(struct device *dev, struct debug_data *data);
};

/* Debug log all function */
struct debug_log {
	struct device *dev;
	struct debug_event *sleep_condition_check;
	struct debug_event *wakeup_soruce_get;
	struct debug_event *wakeup_soruce_print;
	struct debug_event *subsys_state_monitor;
};

/* Debug log register */
extern int sprd_debug_log_register(struct debug_log *debug_log);

/* Debug log unregister */
extern int sprd_debug_log_unregister(void);

#endif /* _SPRD_DEBUGLOG_CORE_H */
