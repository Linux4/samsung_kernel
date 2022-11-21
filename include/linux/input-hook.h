/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __INPUT_HOOK_H
#define __INPUT_HOOK_H
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/sysrq.h>
#include <linux/sysctl.h>

//#define HOOK_POWER_KEY

struct trigger_watch_event {
	bool initialized;
	bool disable_timer;
	bool disable_clk_source;
	bool repeated;
#define TRIGGER_WATCH_EVENT_REPEATED	(1 << 31)
	u64 last_ts; /* last start timestamp */
	char *name;
	int period; /* ms */
	struct hrtimer timer;
	void (*trigger_watch_event_cb)(void *private);
	void *private;
};

void trigger_watch_event_start(struct trigger_watch_event *event, int ms);
int trigger_watch_event_stop(struct trigger_watch_event *event);
void input_report_key_hook(struct input_dev *dev, unsigned int code, int value);
void input_hook_init(void);
void input_hook_exit(void);
#endif
