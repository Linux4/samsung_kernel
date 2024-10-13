/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vs4l.h"

#ifndef VISION_IOCTL_H_
#define VISION_IOCTL_H_

#define VISION_NUM_DEVICES	256
#define VISION_MAJOR		82
#define VISION_NAME		"vision4linux"

struct vision_file_ops {
	struct module *owner;	int (*open)(struct file *file);
	int (*release)(struct file *file);
	unsigned int (*poll)(struct file *file, struct poll_table_struct *poll);
	long (*ioctl)(struct file *, unsigned int cmd, unsigned long arg);
	long (*compat_ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
	int (*flush)(struct file *file);
	int (*mmap)(struct file *file, struct vm_area_struct *vm_area);
};

struct vision_device {
	int				minor;
	char				name[32];
	unsigned long			flags;
	int				index;
	u32				type;

	s64				tpf;

	/* device ops */
	const struct vision_file_ops	*fops;

	/* sysfs */
	struct device			dev;
	struct cdev			*cdev;
	struct device			*parent;

	/* vb2_queue associated with this device node. May be NULL. */
	//struct vb2_queue *queue;

	int debug;			/* Activates debug level*/

	/* callbacks */
	void (*release)(struct vision_device *vdev);

	/* serialization lock */
	//DECLARE_BITMAP(disable_locking, BASE_VIDIOC_PRIVATE);
	struct mutex *lock;
};

struct vision_device *vision_devdata(struct file *file);
int vision_register_device(struct vision_device *vdev, int minor, struct module *owner);

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#if IS_ENABLED(CONFIG_COMPAT)
long vertex_compat_ioctl32(struct file *file, unsigned int cmd,
			   unsigned long arg);
#endif

#endif
