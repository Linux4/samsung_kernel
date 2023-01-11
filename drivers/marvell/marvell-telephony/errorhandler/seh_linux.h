/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _SEH_LINUX_H_
#define _SEH_LINUX_H_

#include "eeh_ioctl.h"

extern spinlock_t lock_for_ripc;
extern bool mmp_hwlock_get_status(void);

struct eeh_msg_list_struct {
	struct list_head msg_list;
	EehMsgStruct msg;
};

struct eeh_ast_list_struct {
	struct list_head ast_list;
	EehAppAssertParam param;
};

struct seh_dev {
	struct list_head msg_head;
	struct list_head ast_head;
	struct device *dev;
	struct semaphore read_sem; /* mutual exclusion semaphore for read */
	struct semaphore ast_sem;  /* mutual exclusion semaphore for assert */
	wait_queue_head_t readq;   /* read queue */
};
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
struct EehApiParams32 {
	int eehApiId;
	unsigned int status;
	compat_uptr_t params;
};

/* Communicate the error info to SEH (for RAMDUMP and m.b. more) */
struct EehErrorInfo32 {
	unsigned err;
	compat_uptr_t str;
	compat_uptr_t regs;
};
#endif

int read_ee_config_b_cp_reset(void);
int trigger_modem_crash(int force_reset, const char *disc);
#endif
