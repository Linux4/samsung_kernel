/*
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * sec_debug_wdd_info.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched/clock.h>
#include <linux/rtc.h>
#include "sec_debug_internal.h"

extern struct watchdogd_info *secdbg_base_get_wdd_info(void);
static struct watchdogd_info *wdd_info;
static struct rtc_time wdd_info_tm;

void secdbg_wdd_set_keepalive(void)
{
	time64_t sec;

	if (!wdd_info) {
		pr_info("%s: wdd_info not initialized\n", __func__);

		return;
	}

	printk("%s: wdd_info: 0x%p\n", __func__, wdd_info);
	wdd_info->last_ping_cpu = raw_smp_processor_id();
	wdd_info->last_ping_time = sched_clock();

	sec = ktime_get_real_seconds();
	rtc_time_to_tm(sec, wdd_info->tm);
	pr_info("Watchdog: %s RTC %d-%02d-%02d %02d:%02d:%02d UTC\n",
			__func__,
			wdd_info->tm->tm_year + 1900, wdd_info->tm->tm_mon + 1,
			wdd_info->tm->tm_mday, wdd_info->tm->tm_hour,
			wdd_info->tm->tm_min, wdd_info->tm->tm_sec);
}

void secdbg_wdd_set_start(void)
{
	if (!wdd_info) {
		pr_info("%s: wdd_info not initialized\n", __func__);

		return;
	}

	printk("%s: wdd_info->init_done: %s\n", __func__, wdd_info->init_done ? "true" : "false");
	if (wdd_info->init_done == false) {
		wdd_info->tsk = current;
		wdd_info->thr = current_thread_info();
		wdd_info->init_done = true;
	}
}

void secdbg_wdd_set_emerg_addr(unsigned long addr)
{
	if (!wdd_info) {
		pr_info("%s: wdd_info not initialized\n", __func__);

		return;
	}

	wdd_info->emerg_addr = addr;
}

static int __init secdbg_wdd_probe(void)
{
	wdd_info = secdbg_base_get_wdd_info();
	if (wdd_info) {
		wdd_info->init_done = false;
		wdd_info->tm = &wdd_info_tm;
		wdd_info->emerg_addr = 0;
	}

	return 0;
}
subsys_initcall(secdbg_wdd_probe);
