/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef _SPRDFB_NOTIFIER_H_
#define _SPRDFB_NOTIFIER_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define	DISPC_DBS_FPS	                (0)

struct dispc_dbs {
	struct list_head link;
	int level;
	int type;
	void *data;
	unsigned int (*dispc_notifier)(struct dispc_dbs *h, int state);
};


int dispc_notifier_register(struct dispc_dbs *handler);


#endif 














