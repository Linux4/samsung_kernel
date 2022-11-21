/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "vipx-log.h"
#include "vipx-core.h"
#include "vipx-context.h"
#include "vipx-ioctl.h"

struct vs4l_graph32 {
	__u32			id;
	__u32			priority;
	__u32			time;
	__u32			flags;
	__u32			size;
	compat_caddr_t		addr;
};

struct vs4l_format32 {
	__u32			target;
	__u32			format;
	__u32			plane;
	__u32			width;
	__u32			height;
};

struct vs4l_format_list32 {
	__u32			direction;
	__u32			count;
	compat_caddr_t		formats;
};

struct vs4l_param32 {
	__u32			target;
	compat_caddr_t		addr;
	__u32			offset;
	__u32			size;
};

struct vs4l_param_list32 {
	__u32			count;
	compat_caddr_t		params;
};

struct vs4l_ctrl32 {
	__u32			ctrl;
	__u32			value;
};

struct vs4l_roi32 {
	__u32			x;
	__u32			y;
	__u32			w;
	__u32			h;
};

struct vs4l_buffer32 {
	struct vs4l_roi		roi;
	union {
		compat_caddr_t	userptr;
		__s32		fd;
	} m;
	compat_caddr_t		reserved;
};

struct vs4l_container32 {
	__u32			type;
	__u32			target;
	__u32			memory;
	__u32			reserved[4];
	__u32			count;
	compat_caddr_t		buffers;
};

struct vs4l_container_list32 {
	__u32			direction;
	__u32			id;
	__u32			index;
	__u32			flags;
	struct compat_timeval	timestamp[6];
	__u32			count;
	__u32			user_params[MAX_NUM_OF_USER_PARAMS];
	compat_caddr_t		containers;
};

#define VS4L_VERTEXIOC_S_GRAPH32	\
	_IOW('V', 0, struct vs4l_graph32)
#define VS4L_VERTEXIOC_S_FORMAT32	\
	_IOW('V', 1, struct vs4l_format_list32)
#define VS4L_VERTEXIOC_S_PARAM32	\
	_IOW('V', 2, struct vs4l_param_list32)
#define VS4L_VERTEXIOC_S_CTRL32		\
	_IOW('V', 3, struct vs4l_ctrl32)
#define VS4L_VERTEXIOC_QBUF32		\
	_IOW('V', 6, struct vs4l_container_list32)
#define VS4L_VERTEXIOC_DQBUF32		\
	_IOW('V', 7, struct vs4l_container_list32)

struct vipx_ioc_load_kernel_binary32 {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_kernel_binary32 {
	unsigned int			size;
	unsigned int			global_id;
	int				kernel_fd;
	unsigned int			kernel_size;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_load_graph_info32 {
	unsigned int			size;
	struct vipx_common_graph_info	graph_info;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_unload_graph_info32 {
	unsigned int			size;
	unsigned int			graph_id;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

struct vipx_ioc_execute_submodel32 {
	unsigned int			size;
	struct vipx_common_execute_info	execute_info;
	int				ret;
	struct compat_timespec		timestamp[4];
	int				reserved[2];
};

#define VIPX_IOC_LOAD_KERNEL_BINARY32	\
	_IOWR('V', 0, struct vipx_ioc_load_kernel_binary32)
#define VIPX_IOC_UNLOAD_KERNEL_BINARY32	\
	_IOWR('V', 1, struct vipx_ioc_unload_kernel_binary32)
#define VIPX_IOC_LOAD_GRAPH_INFO32	\
	_IOWR('V', 2, struct vipx_ioc_load_graph_info32)
#define VIPX_IOC_UNLOAD_GRAPH_INFO32	\
	_IOWR('V', 3, struct vipx_ioc_unload_graph_info32)
#define VIPX_IOC_EXECUTE_SUBMODEL32	\
	_IOWR('V', 4, struct vipx_ioc_execute_submodel32)

static int __vipx_ioctl_get_graph32(struct vs4l_graph *karg,
		struct vs4l_graph32 __user *uarg)
{
	int ret;
	compat_caddr_t taddr;

	vipx_enter();
	if (get_user(karg->id, &uarg->id) ||
			get_user(karg->priority, &uarg->priority) ||
			get_user(karg->time, &uarg->time) ||
			get_user(karg->flags, &uarg->flags) ||
			get_user(karg->size, &uarg->size) ||
			get_user(taddr, &uarg->addr)) {
		ret = -EFAULT;
		vipx_err("Copy failed [S_GRAPH32]\n");
		goto p_err;
	}

	if (karg->size || taddr) {
		ret = -EINVAL;
		vipx_err("invalid parameters (%u/%u) [S_GRAPH32]\n",
				karg->size, taddr);
		goto p_err;
	} else {
		karg->addr = 0;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_graph32(struct vs4l_graph *karg,
		struct vs4l_graph32 __user *uarg)
{
	vipx_enter();
	vipx_leave();
}

static int __vipx_ioctl_get_format32(struct vs4l_format_list *karg,
		struct vs4l_format_list32 __user *uarg)
{
	int ret;
	unsigned int idx;
	size_t size;
	compat_caddr_t taddr;
	struct vs4l_format32 __user *uformat;
	struct vs4l_format *kformat;

	vipx_enter();
	if (get_user(karg->direction, &uarg->direction) ||
			get_user(karg->count, &uarg->count) ||
			get_user(taddr, &uarg->formats)) {
		ret = -EFAULT;
		vipx_err("Copy failed [S_FORMAT32]\n");
		goto p_err;
	}

	uformat = compat_ptr(taddr);

	size = karg->count * sizeof(*kformat);
	kformat = kzalloc(size, GFP_KERNEL);
	if (!kformat) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc kformat (%zu)\n", size);
		goto p_err;
	}

	for (idx = 0; idx < karg->count; ++idx) {
		if (get_user(kformat[idx].target, &uformat[idx].target) ||
				get_user(kformat[idx].format,
					&uformat[idx].format) ||
				get_user(kformat[idx].plane,
					&uformat[idx].plane) ||
				get_user(kformat[idx].width,
					&uformat[idx].width) ||
				get_user(kformat[idx].height,
					&uformat[idx].height)) {
			ret = -EFAULT;
			vipx_err("Copy failed [S_FORMAT32]\n");
			goto p_err_free;
		}
	}
	karg->formats = kformat;

	vipx_leave();
	return 0;
p_err_free:
	kfree(kformat);
p_err:
	return ret;
}

static void __vipx_ioctl_put_format32(struct vs4l_format_list *karg,
		struct vs4l_format_list32 __user *uarg)
{
	vipx_enter();
	kfree(karg->formats);
	vipx_leave();
}

static int __vipx_ioctl_get_param32(struct vs4l_param_list *karg,
		struct vs4l_param_list32 __user *uarg)
{
	int ret;
	unsigned int idx;
	size_t size;
	compat_caddr_t taddr;
	struct vs4l_param32 __user *uparam;
	struct vs4l_param *kparam;

	vipx_enter();
	if (get_user(karg->count, &uarg->count) ||
			get_user(taddr, &uarg->params)) {
		ret = -EFAULT;
		vipx_err("Copy failed [S_PARAM32]\n");
		goto p_err;
	}

	uparam = compat_ptr(taddr);

	size = karg->count * sizeof(*kparam);
	kparam = kzalloc(size, GFP_KERNEL);
	if (!kparam) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc kparam (%zu)\n", size);
		goto p_err;
	}

	for (idx = 0; idx < karg->count; ++idx) {
		if (get_user(kparam[idx].target, &uparam[idx].target) ||
				get_user(taddr, &uparam[idx].addr) ||
				get_user(kparam[idx].offset,
					&uparam[idx].offset) ||
				get_user(kparam[idx].size,
					&uparam[idx].size)) {
			ret = -EFAULT;
			vipx_err("Copy failed [S_PARAM32]\n");
			goto p_err_free;
		}

		if (kparam[idx].size || taddr) {
			ret = -EINVAL;
			vipx_err("invalid parameters ([%d]%u/%u) [S_PARAM32]\n",
					idx, kparam[idx].size, taddr);
			goto p_err_free;
		} else {
			kparam[idx].addr = 0;
		}
	}

	karg->params = kparam;
	vipx_leave();
	return 0;
p_err_free:
	kfree(kparam);
p_err:
	return ret;
}

static void __vipx_ioctl_put_param32(struct vs4l_param_list *karg,
		struct vs4l_param_list32 __user *uarg)
{
	vipx_enter();
	kfree(karg->params);
	vipx_leave();
}

static int __vipx_ioctl_get_ctrl32(struct vs4l_ctrl *karg,
		struct vs4l_ctrl32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->ctrl, &uarg->ctrl) ||
			get_user(karg->value, &uarg->value)) {
		ret = -EFAULT;
		vipx_err("Copy failed [S_CTRL32]\n");
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_ctrl32(struct vs4l_ctrl *karg,
		struct vs4l_ctrl32 __user *uarg)
{
	vipx_enter();
	vipx_leave();
}

static int __vipx_ioctl_get_container32(struct vs4l_container_list *karg,
		struct vs4l_container_list32 __user *uarg)
{
	int ret;
	size_t size;
	compat_caddr_t taddr;
	unsigned int c_count, b_count;
	struct vs4l_container32 *ucon;
	struct vs4l_container *kcon;
	struct vs4l_buffer32 *ubuf;
	struct vs4l_buffer *kbuf;

	vipx_enter();
	if (get_user(karg->direction, &uarg->direction) ||
			get_user(karg->id, &uarg->id) ||
			get_user(karg->index, &uarg->index) ||
			get_user(karg->flags, &uarg->flags) ||
			get_user(karg->count, &uarg->count) ||
			copy_from_user(karg->user_params, uarg->user_params,
					sizeof(karg->user_params)) ||
			get_user(taddr, &uarg->containers)) {
		ret = -EFAULT;
		vipx_err("Copy failed [CONTAINER32]\n");
		goto p_err;
	}

	ucon = compat_ptr(taddr);

	size = karg->count * sizeof(*kcon);
	kcon = kzalloc(size, GFP_KERNEL);
	if (!kcon) {
		ret = -ENOMEM;
		vipx_err("Failed to alloc kcon (%zu)\n", size);
		goto p_err;
	}

	karg->containers = kcon;

	for (c_count = 0; c_count < karg->count; ++c_count) {
		if (get_user(kcon[c_count].type, &ucon[c_count].type) ||
				get_user(kcon[c_count].target,
					&ucon[c_count].target) ||
				get_user(kcon[c_count].memory,
					&ucon[c_count].memory) ||
				copy_from_user(kcon[c_count].reserved,
					ucon[c_count].reserved,
					sizeof(kcon[c_count].reserved)) ||
				get_user(kcon[c_count].count,
					&ucon[c_count].count) ||
				get_user(taddr, &ucon[c_count].buffers)) {
			ret = -EFAULT;
			vipx_err("Copy failed [CONTAINER32]\n");
			goto p_err_free;
		}

		ubuf = compat_ptr(taddr);

		size = kcon[c_count].count * sizeof(*kbuf);
		kbuf = kzalloc(size, GFP_KERNEL);
		if (!kbuf) {
			ret = -ENOMEM;
			vipx_err("Failed to alloc kbuf (%zu)\n", size);
			goto p_err_free;
		}

		kcon[c_count].buffers = kbuf;

		for (b_count = 0; b_count < kcon[c_count].count;
				++b_count) {
			if (get_user(kbuf[b_count].roi.x,
						&ubuf[b_count].roi.x) ||
					get_user(kbuf[b_count].roi.y,
						&ubuf[b_count].roi.y) ||
					get_user(kbuf[b_count].roi.w,
						&ubuf[b_count].roi.w) ||
					get_user(kbuf[b_count].roi.h,
						&ubuf[b_count].roi.h)) {
				ret = -EFAULT;
				vipx_err("Copy failed [CONTAINER32]\n");
				goto p_err_free;
			}

			if (kcon[c_count].memory == VS4L_MEMORY_DMABUF) {
				if (get_user(kbuf[b_count].m.fd,
						&ubuf[b_count].m.fd)) {
					ret = -EFAULT;
					vipx_err("Copy failed [CONTAINER32]\n");
					goto p_err_free;
				}
			} else {
				if (get_user(kbuf[b_count].m.userptr,
						&ubuf[b_count].m.userptr)) {
					ret = -EFAULT;
					vipx_err("Copy failed [CONTAINER32]\n");
					goto p_err_free;
				}
			}
		}
	}

	vipx_leave();
	return 0;
p_err_free:
	for (c_count = 0; c_count < karg->count; ++c_count)
		kfree(kcon[c_count].buffers);

	kfree(kcon);
p_err:
	return ret;
}

static void __vipx_ioctl_put_container32(struct vs4l_container_list *karg,
		struct vs4l_container_list32 __user *uarg)
{
	unsigned int idx;

	vipx_enter();
	if (put_user(karg->id, &uarg->id) ||
			put_user(karg->index, &uarg->index) ||
			put_user(karg->flags, &uarg->flags) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_usec,
				&uarg->timestamp[0].tv_usec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_usec,
				&uarg->timestamp[1].tv_usec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_usec,
				&uarg->timestamp[2].tv_usec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_usec,
				&uarg->timestamp[3].tv_usec) ||
			put_user(karg->timestamp[4].tv_sec,
				&uarg->timestamp[4].tv_sec) ||
			put_user(karg->timestamp[4].tv_usec,
				&uarg->timestamp[4].tv_usec) ||
			put_user(karg->timestamp[5].tv_sec,
				&uarg->timestamp[5].tv_sec) ||
			put_user(karg->timestamp[5].tv_usec,
				&uarg->timestamp[5].tv_usec) ||
			copy_to_user(uarg->user_params, karg->user_params,
				sizeof(karg->user_params))) {
		vipx_err("Copy failed to user [CONTAINER32]\n");
	}

	for (idx = 0; idx < karg->count; ++idx)
		kfree(karg->containers[idx].buffers);

	kfree(karg->containers);
	vipx_leave();
}

static int __vipx_ioctl_get_load_kernel_binary32(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->global_id, &uarg->global_id) ||
			get_user(karg->kernel_fd, &uarg->kernel_fd) ||
			get_user(karg->kernel_size, &uarg->kernel_size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Load kernel binary(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_kernel_binary32(
		struct vipx_ioc_load_kernel_binary *karg,
		struct vipx_ioc_load_kernel_binary32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_unload_kernel_binary32(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->global_id, &uarg->global_id) ||
			get_user(karg->kernel_fd, &uarg->kernel_fd) ||
			get_user(karg->kernel_size, &uarg->kernel_size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Unload Kernel Binary(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_kernel_binary32(
		struct vipx_ioc_unload_kernel_binary *karg,
		struct vipx_ioc_unload_kernel_binary32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Unload kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_load_graph_info32(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Load graph info(32)]\n");
		goto p_err;
	}

	ret = copy_from_user(&karg->graph_info, &uarg->graph_info,
			sizeof(uarg->graph_info));
	if (ret) {
		vipx_err("Copy failed from user [Load graph info(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_load_graph_info32(
		struct vipx_ioc_load_graph_info *karg,
		struct vipx_ioc_load_graph_info32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_unload_graph_info32(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size) ||
			get_user(karg->graph_id, &uarg->graph_id)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Unload graph info(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_unload_graph_info32(
		struct vipx_ioc_unload_graph_info *karg,
		struct vipx_ioc_unload_graph_info32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Unload graph_info(32)]\n");
	}
	vipx_leave();
}

static int __vipx_ioctl_get_execute_submodel32(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel32 __user *uarg)
{
	int ret;

	vipx_enter();
	if (get_user(karg->size, &uarg->size)) {
		ret = -EFAULT;
		vipx_err("Copy failed [Execute submodel(32)]\n");
		goto p_err;
	}

	ret = copy_from_user(&karg->execute_info, &uarg->execute_info,
			sizeof(uarg->execute_info));
	if (ret) {
		vipx_err("Copy failed from user [Execute submodel(32)]\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));
	memset(karg->reserved, 0, sizeof(karg->reserved));

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static void __vipx_ioctl_put_execute_submodel32(
		struct vipx_ioc_execute_submodel *karg,
		struct vipx_ioc_execute_submodel32 __user *uarg)
{
	vipx_enter();
	if (put_user(karg->ret, &uarg->ret) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		vipx_err("Copy failed to user [Load kernel binary(32)]\n");
	}
	vipx_leave();
}

long vipx_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vipx_context *vctx;
	const struct vipx_ioctl_ops *ops;
	union vipx_ioc_arg karg;
	void __user *uarg;

	vipx_enter();
	vctx = file->private_data;
	ops = vctx->core->ioc_ops;
	uarg = compat_ptr(arg);

	switch (cmd) {
	case VS4L_VERTEXIOC_S_GRAPH32:
		ret = __vipx_ioctl_get_graph32(&karg.graph, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_graph(vctx, &karg.graph);
		__vipx_ioctl_put_graph32(&karg.graph, uarg);
		break;
	case VS4L_VERTEXIOC_S_FORMAT32:
		ret = __vipx_ioctl_get_format32(&karg.flist, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_format(vctx, &karg.flist);
		__vipx_ioctl_put_format32(&karg.flist, uarg);
		break;
	case VS4L_VERTEXIOC_S_PARAM32:
		ret = __vipx_ioctl_get_param32(&karg.plist, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_param(vctx, &karg.plist);
		__vipx_ioctl_put_param32(&karg.plist, uarg);
		break;
	case VS4L_VERTEXIOC_S_CTRL32:
		ret = __vipx_ioctl_get_ctrl32(&karg.ctrl, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_ctrl(vctx, &karg.ctrl);
		__vipx_ioctl_put_ctrl32(&karg.ctrl, uarg);
		break;
	case VS4L_VERTEXIOC_STREAM_ON:
		ret = ops->streamon(vctx);
		break;
	case VS4L_VERTEXIOC_STREAM_OFF:
		ret = ops->streamoff(vctx);
		break;
	case VS4L_VERTEXIOC_QBUF32:
		ret = __vipx_ioctl_get_container32(&karg.clist, uarg);
		if (ret)
			goto p_err;

		ret = ops->qbuf(vctx, &karg.clist);
		__vipx_ioctl_put_container32(&karg.clist, uarg);
		break;
	case VS4L_VERTEXIOC_DQBUF32:
		ret = __vipx_ioctl_get_container32(&karg.clist, uarg);
		if (ret)
			goto p_err;

		ret = ops->dqbuf(vctx, &karg.clist);
		__vipx_ioctl_put_container32(&karg.clist, uarg);
		break;
	case VIPX_IOC_LOAD_KERNEL_BINARY32:
		ret = __vipx_ioctl_get_load_kernel_binary32(&karg.kernel_bin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_kernel_binary(vctx, &karg.kernel_bin);
		__vipx_ioctl_put_load_kernel_binary32(&karg.kernel_bin, uarg);
		break;
	case VIPX_IOC_UNLOAD_KERNEL_BINARY32:
		ret = __vipx_ioctl_get_unload_kernel_binary32(&karg.unload_kbin,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_kernel_binary(vctx, &karg.unload_kbin);
		__vipx_ioctl_put_unload_kernel_binary32(&karg.unload_kbin,
				uarg);
		break;
	case VIPX_IOC_LOAD_GRAPH_INFO32:
		ret = __vipx_ioctl_get_load_graph_info32(&karg.load_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->load_graph_info(vctx, &karg.load_ginfo);
		__vipx_ioctl_put_load_graph_info32(&karg.load_ginfo, uarg);
		break;
	case VIPX_IOC_UNLOAD_GRAPH_INFO32:
		ret = __vipx_ioctl_get_unload_graph_info32(&karg.unload_ginfo,
				uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_graph_info(vctx, &karg.unload_ginfo);
		__vipx_ioctl_put_unload_graph_info32(&karg.unload_ginfo, uarg);
		break;
	case VIPX_IOC_EXECUTE_SUBMODEL32:
		ret = __vipx_ioctl_get_execute_submodel32(&karg.exec, uarg);
		if (ret)
			goto p_err;

		ret = ops->execute_submodel(vctx, &karg.exec);
		__vipx_ioctl_put_execute_submodel32(&karg.exec, uarg);
		break;
	default:
		ret = -EINVAL;
		vipx_err("ioc command(%x) is not supported\n", cmd);
		goto p_err;
	}

	vipx_leave();
p_err:
	return ret;
}
