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

#include "vertex-log.h"
#include "vertex-core.h"
#include "vertex-context.h"
#include "vertex-ioctl.h"

static int __vertex_ioctl_get_graph(struct vs4l_graph *karg,
		struct vs4l_graph __user *uarg)
{
	int ret;

	vertex_enter();
	ret = copy_from_user(karg, uarg, sizeof(*karg));
	if (ret) {
		vertex_err("Copy failed [S_GRAPH] (%d)\n", ret);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static void __vertex_ioctl_put_graph(struct vs4l_graph *karg,
		struct vs4l_graph __user *uarg)
{
	vertex_enter();
	vertex_leave();
}

static int __vertex_ioctl_get_format(struct vs4l_format_list *karg,
		struct vs4l_format_list __user *uarg)
{
	int ret;
	size_t size;
	struct vs4l_format __user *uformat;
	struct vs4l_format *kformat;

	vertex_enter();
	ret = copy_from_user(karg, uarg, sizeof(*karg));
	if (ret) {
		vertex_err("Copy failed [S_FORMAT] (%d)\n", ret);
		goto p_err;
	}

	uformat = karg->formats;

	size = karg->count * sizeof(*kformat);
	kformat = kzalloc(size, GFP_KERNEL);
	if (!kformat) {
		ret = -ENOMEM;
		vertex_err("Failed to alloc kformat (%zu)\n", size);
		goto p_err;
	}

	ret = copy_from_user(kformat, uformat, size);
	if (ret) {
		vertex_err("Copy failed [S_FORMAT] (%d)\n", ret);
		goto p_err_free;
	}

	karg->formats = kformat;

	vertex_leave();
	return 0;
p_err_free:
	kfree(kformat);
p_err:
	return ret;
}

static void __vertex_ioctl_put_format(struct vs4l_format_list *karg,
		struct vs4l_format_list __user *uarg)
{
	vertex_enter();
	kfree(karg->formats);
	vertex_leave();
}

static int __vertex_ioctl_get_param(struct vs4l_param_list *karg,
		struct vs4l_param_list __user *uarg)
{
	int ret;
	size_t size;
	struct vs4l_param __user *uparam;
	struct vs4l_param *kparam;

	vertex_enter();
	ret = copy_from_user(karg, uarg, sizeof(*karg));
	if (ret) {
		vertex_err("Copy failed [S_PARAM] (%d)\n", ret);
		goto p_err;
	}

	uparam = karg->params;

	size = karg->count * sizeof(*kparam);
	kparam = kzalloc(size, GFP_KERNEL);
	if (!kparam) {
		ret = -ENOMEM;
		vertex_err("Failed to alloc kparam (%zu)\n", size);
		goto p_err;
	}

	ret = copy_from_user(kparam, uparam, size);
	if (ret) {
		vertex_err("Copy failed [S_PARAM] (%d)\n", ret);
		goto p_err_free;
	}

	karg->params = kparam;
	vertex_leave();
	return 0;
p_err_free:
	kfree(kparam);
p_err:
	return ret;
}

static void __vertex_ioctl_put_param(struct vs4l_param_list *karg,
		struct vs4l_param_list __user *uarg)
{
	vertex_enter();
	kfree(karg->params);
	vertex_leave();
}

static int __vertex_ioctl_get_ctrl(struct vs4l_ctrl *karg,
		struct vs4l_ctrl __user *uarg)
{
	int ret;

	vertex_enter();
	ret = copy_from_user(karg, uarg, sizeof(*karg));
	if (ret) {
		vertex_err("Copy failed [S_CTRL] (%d)\n", ret);
		goto p_err;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static void __vertex_ioctl_put_ctrl(struct vs4l_ctrl *karg,
		struct vs4l_ctrl __user *uarg)
{
	vertex_enter();
	vertex_leave();
}

static int __vertex_ioctl_get_container(struct vs4l_container_list *karg,
		struct vs4l_container_list __user *uarg)
{
	int ret;
	size_t size;
	unsigned int idx;
	struct vs4l_container *ucon;
	struct vs4l_container *kcon;
	struct vs4l_buffer *ubuf;
	struct vs4l_buffer *kbuf;

	vertex_enter();
	ret = copy_from_user(karg, uarg, sizeof(*karg));
	if (ret) {
		vertex_err("Copy failed [CONTAINER] (%d)\n", ret);
		goto p_err;
	}

	ucon = karg->containers;

	size = karg->count * sizeof(*kcon);
	kcon = kzalloc(size, GFP_KERNEL);
	if (!kcon) {
		ret = -ENOMEM;
		vertex_err("Failed to alloc kcon (%zu)\n", size);
		goto p_err;
	}

	karg->containers = kcon;

	ret = copy_from_user(kcon, ucon, size);
	if (ret) {
		vertex_err("Copy failed [CONTAINER] (%d)\n", ret);
		goto p_err_free;
	}

	for (idx = 0; idx < karg->count; ++idx) {
		ubuf = kcon[idx].buffers;

		size = kcon[idx].count * sizeof(*kbuf);
		kbuf = kzalloc(size, GFP_KERNEL);
		if (!kbuf) {
			ret = -ENOMEM;
			vertex_err("Failed to alloc kbuf (%zu)\n", size);
			goto p_err_free;
		}

		kcon[idx].buffers = kbuf;

		ret = copy_from_user(kbuf, ubuf, size);
		if (ret) {
			vertex_err("Copy failed [CONTAINER] (%d)\n", ret);
			goto p_err_free;
		}
	}

	vertex_leave();
	return 0;
p_err_free:
	for (idx = 0; idx < karg->count; ++idx)
		kfree(kcon[idx].buffers);

	kfree(kcon);
p_err:
	return ret;
}

static void __vertex_ioctl_put_container(struct vs4l_container_list *karg,
		struct vs4l_container_list __user *uarg)
{
	unsigned int idx;

	vertex_enter();
	if (put_user(karg->id, &uarg->id) ||
			put_user(karg->index, &uarg->index) ||
			put_user(karg->flags, &uarg->flags) ||
			copy_to_user(uarg->timestamp, karg->timestamp,
				sizeof(karg->timestamp)) ||
			copy_to_user(uarg->user_params, karg->user_params,
				sizeof(karg->user_params))) {
		vertex_err("Copy failed to user [CONTAINER]\n");
	}

	for (idx = 0; idx < karg->count; ++idx)
		kfree(karg->containers[idx].buffers);

	kfree(karg->containers);
	vertex_leave();
}

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vertex_context *vctx;
	const struct vertex_ioctl_ops *ops;
	union vertex_ioc_arg karg;
	void __user *uarg;

	vertex_enter();
	vctx = file->private_data;
	ops = vctx->core->ioc_ops;
	uarg = (void __user *)arg;

	switch (cmd) {
	case VS4L_VERTEXIOC_S_GRAPH:
		ret = __vertex_ioctl_get_graph(&karg.graph, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_graph(vctx, &karg.graph);
		__vertex_ioctl_put_graph(&karg.graph, uarg);
		break;
	case VS4L_VERTEXIOC_S_FORMAT:
		ret = __vertex_ioctl_get_format(&karg.flist, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_format(vctx, &karg.flist);
		__vertex_ioctl_put_format(&karg.flist, uarg);
		break;
	case VS4L_VERTEXIOC_S_PARAM:
		ret = __vertex_ioctl_get_param(&karg.plist, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_param(vctx, &karg.plist);
		__vertex_ioctl_put_param(&karg.plist, uarg);
		break;
	case VS4L_VERTEXIOC_S_CTRL:
		ret = __vertex_ioctl_get_ctrl(&karg.ctrl, uarg);
		if (ret)
			goto p_err;

		ret = ops->set_ctrl(vctx, &karg.ctrl);
		__vertex_ioctl_put_ctrl(&karg.ctrl, uarg);
		break;
	case VS4L_VERTEXIOC_STREAM_ON:
		ret = ops->streamon(vctx);
		break;
	case VS4L_VERTEXIOC_STREAM_OFF:
		ret = ops->streamoff(vctx);
		break;
	case VS4L_VERTEXIOC_QBUF:
		ret = __vertex_ioctl_get_container(&karg.clist, uarg);
		if (ret)
			goto p_err;

		ret = ops->qbuf(vctx, &karg.clist);
		__vertex_ioctl_put_container(&karg.clist, uarg);
		break;
	case VS4L_VERTEXIOC_DQBUF:
		ret = __vertex_ioctl_get_container(&karg.clist, uarg);
		if (ret)
			goto p_err;

		ret = ops->dqbuf(vctx, &karg.clist);
		__vertex_ioctl_put_container(&karg.clist, uarg);
		break;
	default:
		ret = -EINVAL;
		vertex_err("ioc command(%x) is not supported\n", cmd);
		goto p_err;
	}

	vertex_leave();
p_err:
	return ret;
}
