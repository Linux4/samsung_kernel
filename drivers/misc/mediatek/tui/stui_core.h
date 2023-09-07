/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __STUI_CORE_H_
#define __STUI_CORE_H_

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/wakelock.h>

#define STUI_ALIGN_1MB_SZ	0x100000		/* 1MB */
#define STUI_ALIGN_16MB_SZ	0x1000000		/* 16MB */
#define STUI_ALIGN_32MB_SZ	0x2000000		/* 32MB */

#define STUI_ALIGN_UP(size, block)	((((size) + (block) - 1) / (block)) * (block))

/* TODO:
 * Set correct framebuffer size according to the device specifications
 */
#define STUI_BUFFER_SIZE	((1920*1080*4) + STUI_ALIGN_1MB_SZ)	/* Frame size for one buffer */
#define STUI_BUFFER_NUM		1				/* Framebuffer only */
/*#define STUI_BUFFER_NUM	2*/			/* Framebuffer and Working buffer */


#define STUI_DEV_NAME		"tuihw"
#define STUI_BUF_MAX		4


extern struct wake_lock tui_wakelock;

long stui_process_cmd(struct file *f, unsigned int cmd, unsigned long arg);

struct stui_buf_info {
	unsigned long pa[STUI_BUF_MAX];
	size_t size[STUI_BUF_MAX];
};

#endif /* __STUI_CORE_H_ */
