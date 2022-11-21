/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_IOCTL_H__
#define __VERTEX_IOCTL_H__

#include <linux/fs.h>
#include <linux/uaccess.h>

#include "vs4l.h"

#define VERTEX_CTRL_VERTEX_BASE		(0x00010000)
#define VERTEX_CTRL_DUMP		(VERTEX_CTRL_VERTEX_BASE + 1)
#define VERTEX_CTRL_MODE		(VERTEX_CTRL_VERTEX_BASE + 2)
#define VERTEX_CTRL_TEST		(VERTEX_CTRL_VERTEX_BASE + 3)

struct vertex_context;

union vertex_ioc_arg {
	struct vs4l_graph		graph;
	struct vs4l_format_list		flist;
	struct vs4l_param_list		plist;
	struct vs4l_ctrl		ctrl;
	struct vs4l_container_list	clist;
};

struct vertex_ioctl_ops {
	int (*set_graph)(struct vertex_context *vctx,
			struct vs4l_graph *graph);
	int (*set_format)(struct vertex_context *vctx,
			struct vs4l_format_list *flist);
	int (*set_param)(struct vertex_context *vctx,
			struct vs4l_param_list *plist);
	int (*set_ctrl)(struct vertex_context *vctx,
			struct vs4l_ctrl *ctrl);
	int (*qbuf)(struct vertex_context *vctx,
			struct vs4l_container_list *clist);
	int (*dqbuf)(struct vertex_context *vctx,
			struct vs4l_container_list *clist);
	int (*streamon)(struct vertex_context *vctx);
	int (*streamoff)(struct vertex_context *vctx);
};

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#if defined(CONFIG_COMPAT)
long vertex_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg);
#else
static inline long vertex_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg) { return 0; };
#endif

#endif
