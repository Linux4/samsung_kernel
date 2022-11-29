// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/blkdev.h>

#include "smcdsd_notify.h"

#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define NSEC_TO_MSEC(ns)		(div_u64(ns, NSEC_PER_MSEC))

#define __XX(a)	#a,
const char *EVENT_NAME[] = { EVENT_LIST };
const char *STATE_NAME[] = { STATE_LIST };
#undef __XX

enum {
	CHAIN_START,
	CHAIN_END,
	CHAIN_MAX,
};

u32 EVENT_NAME_LEN;
u32 STATE_NAME_LEN;
u64 STAMP_TIME[STAMP_MAX][CHAIN_MAX];

static u32 EVENT_TO_STAMP[EVENT_MAX] = {
	[FB_EVENT_BLANK] =		SMCDSD_STAMP_AFTER,
	[FB_EARLY_EVENT_BLANK] =	SMCDSD_STAMP_EARLY,
	[SMCDSD_EVENT_DOZE] =		SMCDSD_STAMP_AFTER,
	[SMCDSD_EARLY_EVENT_DOZE] =	SMCDSD_STAMP_EARLY,
	[SMCDSD_EVENT_FRAME] =		SMCDSD_STAMP_FRAME,
	[SMCDSD_EVENT_FRAME_SEND] =	SMCDSD_STAMP_FRAME_SEND,
	[SMCDSD_EVENT_FRAME_DONE] =	SMCDSD_STAMP_FRAME_DONE,
};

static BLOCKING_NOTIFIER_HEAD(smcdsd_notifier_list);

int smcdsd_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&smcdsd_notifier_list, nb);
}
EXPORT_SYMBOL(smcdsd_register_notifier);

int smcdsd_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&smcdsd_notifier_list, nb);
}
EXPORT_SYMBOL(smcdsd_unregister_notifier);

int smcdsd_notifier_call_chain(unsigned long val, void *v)
{
	int state = 0, ret = 0;
	struct fb_event *evdata = NULL;
	u64 blank_delta = 0, extra_delta = 0, total_delta = 0, frame_delta = 0;
	u64 current_clock = 0;
	u32 current_index = 0, current_first = 0;

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	current_index = EVENT_TO_STAMP[val] ? EVENT_TO_STAMP[val] : SMCDSD_STAMP_UNKNOWN;

	WARN_ON(current_index == SMCDSD_STAMP_UNKNOWN);

	STAMP_TIME[current_index][CHAIN_START] = current_clock = local_clock();
	STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_END] = (val == SMCDSD_EVENT_DOZE) ? current_clock : STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_END];

	if (IS_EARLY(val)) {
		dbg_info("smcdsd_notifier: blank_mode: %d, %02lx, + %-*s, %-*s\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val]);
	}

	ret = blocking_notifier_call_chain(&smcdsd_notifier_list, val, v);

	current_first = (STAMP_TIME[SMCDSD_STAMP_AFTER][CHAIN_END] > STAMP_TIME[current_index][CHAIN_END]) ? 1 : 0;

	STAMP_TIME[current_index][CHAIN_END] = current_clock = local_clock();
	STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_START] = (val == SMCDSD_EARLY_EVENT_DOZE) ? current_clock : STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_START];

	blank_delta += STAMP_TIME[SMCDSD_STAMP_EARLY][CHAIN_END] - STAMP_TIME[SMCDSD_STAMP_EARLY][CHAIN_START];
	blank_delta += STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_END] - STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_START];
	blank_delta += STAMP_TIME[SMCDSD_STAMP_AFTER][CHAIN_END] - STAMP_TIME[SMCDSD_STAMP_AFTER][CHAIN_START];

	extra_delta = current_clock - STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_END];
	frame_delta = current_clock - STAMP_TIME[current_index - 1][CHAIN_END];

	total_delta = blank_delta + extra_delta;

	if (IS_AFTER(val)) {
		dbg_info("smcdsd_notifier: blank_mode: %d, %02lx, - %-*s, %-*s, %lld\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val], NSEC_TO_MSEC(blank_delta));
	} else if (current_first && IS_FRAME(val)) {
		dbg_info("smcdsd_notifier: blank_mode: %d, %02lx, * %-*s, %-*s, %lld(%lld)\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val], NSEC_TO_MSEC(frame_delta), NSEC_TO_MSEC(total_delta));
	}

	return ret;
}
EXPORT_SYMBOL(smcdsd_notifier_call_chain);

int smcdsd_simple_notifier_call_chain(unsigned long val, int blank)
{
	struct fb_info *fbinfo = registered_fb[0];
	struct fb_event v = {0, };
	int fb_blank = blank;

	v.info = fbinfo;
	v.data = &fb_blank;

	return smcdsd_notifier_call_chain(val, &v);
}
EXPORT_SYMBOL(smcdsd_simple_notifier_call_chain);

struct notifier_block smcdsd_nb_priority_max = {
	.priority = INT_MAX,
};

struct notifier_block smcdsd_nb_priority_min = {
	.priority = INT_MIN,
};

static int smcdsd_fb_notifier_event(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EARLY_EVENT_BLANK:
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	smcdsd_notifier_call_chain(val, v);

	return NOTIFY_DONE;
}

static struct notifier_block smcdsd_fb_notifier = {
	.notifier_call = smcdsd_fb_notifier_event,
};

static int smcdsd_fb_notifier_blank_early(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_START] = local_clock();

	return NOTIFY_DONE;
}

static int smcdsd_fb_notifier_blank_after(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	STAMP_TIME[SMCDSD_STAMP_BLANK][CHAIN_END] = local_clock();

	return NOTIFY_DONE;
}

static struct notifier_block smcdsd_fb_notifier_blank_min = {
	.notifier_call = smcdsd_fb_notifier_blank_early,
	.priority = INT_MIN,
};

static struct notifier_block smcdsd_fb_notifier_blank_max = {
	.notifier_call = smcdsd_fb_notifier_blank_after,
	.priority = INT_MAX,
};

static int __init smcdsd_notifier_init(void)
{
	EVENT_NAME_LEN = EVENT_NAME[FB_EARLY_EVENT_BLANK] ? min_t(size_t, MAX_INPUT, strlen(EVENT_NAME[FB_EARLY_EVENT_BLANK])) : EVENT_NAME_LEN;
	STATE_NAME_LEN = STATE_NAME[FB_BLANK_POWERDOWN] ? min_t(size_t, MAX_INPUT, strlen(STATE_NAME[FB_BLANK_POWERDOWN])) : STATE_NAME_LEN;

	fb_register_client(&smcdsd_fb_notifier_blank_min);
	fb_register_client(&smcdsd_fb_notifier);
	fb_register_client(&smcdsd_fb_notifier_blank_max);

	return 0;
}

core_initcall(smcdsd_notifier_init);

