/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 */

#ifndef __CONFIG_LINUX_KERNEL_INC__
#define __CONFIG_LINUX_KERNEL_INC__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/ftrace.h>
#include <sound/pcm.h>

#define _ASSERT(e)
#define PRINT_ASSERT(e) {if ((e))\
	pr_err("PrintAssert:%s (%s:%d) error code:%d\n",\
	__func__, __FILE__, __LINE__, e); }

#endif /* __CONFIG_LINUX_KERNEL_INC__ */

