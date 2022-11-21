/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _SEH_LINUX_H
#define _SEH_LINUX_H

#include "eeh_ioctl.h"

struct seh_dev {
	EehMsgStruct msg;
	struct device *dev;
	struct semaphore read_sem;                              /* mutual exclusion semaphore */
	wait_queue_head_t readq;                                /* read queue */
};


#endif

