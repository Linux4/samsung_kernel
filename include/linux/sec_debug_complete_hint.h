/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2019 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_COMPLETE_HINT_H
#define SEC_DEBUG_COMPLETE_HINT_H

#include <linux/sched.h>

#define MAX_HINT 3
#define CALLSTACK_MAX_NUM 5
#define HINT_MAGIC 0xADDB100D

struct complete_hint {
	void * addrs[CALLSTACK_MAX_NUM];
	union {
		struct task_struct *p;
		void *fn;
	};
	int magic;
	int type;
};

struct secdbg_hint {
	struct complete_hint hint[MAX_HINT];	
	int hint_magic;
	unsigned int hint_idx;
};

static inline void secdbg_hint_init(struct secdbg_hint *hint);

#ifdef CONFIG_SEC_DEBUG_COMPLETE_HINT
#include <linux/completion.h>

static inline void secdbg_hint_add_completion_to_task(struct completion *x)
{
	current->x = x;
}

static inline void secdbg_hint_del_completion_to_task(struct completion *x)
{
	current->x = 0;
}
#endif

static inline void secdbg_hint_init(struct secdbg_hint *hint)
{
	if (hint->hint_magic != HINT_MAGIC) {
		hint->hint_idx = 0;
		hint->hint_magic = HINT_MAGIC;
	}
}

extern void secdbg_hint_save_complete_hint(struct secdbg_hint *hint);

extern void secdbg_hint_display_complete_hint(void);

#endif /* SEC_DEBUG_COMPLETE_HINT_H */
