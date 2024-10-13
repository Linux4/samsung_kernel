/*
 * Samsung Mobile VE Group.
 *
 * drivers/battery/sec_batt_selfdchg_common.c
 *
 * Common functions for samsung batter self discharging.
 *
 * Copyright (C) 2015, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/sec_batt_selfdchg_common.h>

#include <linux/proc_fs.h>

struct list_head sdchg_info_head = LIST_HEAD_INIT(sdchg_info_head);
bool sdchg_nochip_support;

/****************************************************************/
static __kernel_time_t start_sec;
static __kernel_time_t curr_sec;
static __kernel_time_t target_sec;
static unsigned long target_time;
static bool sdchg_set_polling;


// true : runned sdchg polling time, false : other polling time at monitor work
bool sdchg_check_polling_time(__kernel_time_t curr)
{
	curr_sec = curr;
	if (sdchg_set_polling)
		if ((unsigned long)curr_sec >= (unsigned long)(target_sec - 10)) {
			sdchg_set_polling = false;
			return true;
		}
	return false;
}


void sdchg_set_polling_time(int polling_time)
{
	if (polling_time == 0) {
		sdchg_set_polling = false;
		goto out;
	}
		
	start_sec = curr_sec;
	target_time = polling_time;
	target_sec = start_sec + polling_time;

	sdchg_set_polling = true;
out:
	return;
}


unsigned int sdchg_get_polling_time(unsigned int cur_polling_time)
{
	unsigned int new_polling_time = cur_polling_time;
	unsigned long elapse_time, remain_time;

	if (!sdchg_set_polling || target_time == 1 )
		goto out;

	elapse_time = (unsigned long)(curr_sec - start_sec);
	remain_time = target_time - elapse_time;

	if (remain_time < new_polling_time)
		new_polling_time = remain_time;
out:
	//pr_info("[SDCHG][%s] new polling time = %u\n", __func__, new_polling_time);
	return new_polling_time;
}


/****************************************************************/
