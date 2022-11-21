/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vertex-log.h"
#include "vertex-util.h"
#include "vertex-ioctl.h"
#include "vertex-device.h"
#include "vertex-graph.h"
#include "vertex-context.h"
#include "vertex-core.h"

static inline int __vref_get(struct vertex_core_refcount *vref)
{
	int ret;

	vertex_enter();
	ret = (atomic_inc_return(&vref->refcount) == 1) ?
		vref->first(vref->core) : 0;
	if (ret)
		atomic_dec(&vref->refcount);
	vertex_leave();
	return ret;
}

static inline int __vref_put(struct vertex_core_refcount *vref)
{
	int ret;

	vertex_enter();
	ret = (atomic_dec_return(&vref->refcount) == 0) ?
		vref->final(vref->core) : 0;
	if (ret)
		atomic_inc(&vref->refcount);
	vertex_leave();
	return ret;
}

static int vertex_core_set_graph(struct vertex_context *vctx,
		struct vs4l_graph *ginfo)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for set_graph (%d)\n",
				ret);
		goto p_err_lock;
	}

	if (!(vctx->state & BIT(VERTEX_CONTEXT_OPEN))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not open\n", vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->set_graph(&vctx->queue_list, ginfo);
	if (ret)
		goto p_err_queue_ops;

	vctx->state = BIT(VERTEX_CONTEXT_GRAPH);
	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_set_format(struct vertex_context *vctx,
		struct vs4l_format_list *flist)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for set_format (%d)\n",
				ret);
		goto p_err_lock;
	}

	if (!(vctx->state & (BIT(VERTEX_CONTEXT_GRAPH) |
					BIT(VERTEX_CONTEXT_FORMAT)))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not graph/format\n",
				vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->set_format(&vctx->queue_list, flist);
	if (ret)
		goto p_err_queue_ops;

	vctx->state = BIT(VERTEX_CONTEXT_FORMAT);
	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_set_param(struct vertex_context *vctx,
		struct vs4l_param_list *plist)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for set_param (%d)\n",
				ret);
		goto p_err_lock;
	}

	if (!(vctx->state & BIT(VERTEX_CONTEXT_START))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not start\n", vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->set_param(&vctx->queue_list, plist);
	if (ret)
		goto p_err_queue_ops;

	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_set_ctrl(struct vertex_context *vctx,
		struct vs4l_ctrl *ctrl)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for set_ctrl (%d)\n",
				ret);
		goto p_err_lock;
	}

	ret = vctx->queue_ops->set_ctrl(&vctx->queue_list, ctrl);
	if (ret)
		goto p_err_queue_ops;

	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_qbuf(struct vertex_context *vctx,
		struct vs4l_container_list *clist)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for qbuf (%d)\n", ret);
		goto p_err_lock;
	}

	if (!(vctx->state & BIT(VERTEX_CONTEXT_START))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not start\n", vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->qbuf(&vctx->queue_list, clist);
	if (ret)
		goto p_err_queue_ops;

	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_dqbuf(struct vertex_context *vctx,
		struct vs4l_container_list *clist)
{
	int ret;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for dqbuf (%d)\n", ret);
		goto p_err_lock;
	}

	if (!(vctx->state & BIT(VERTEX_CONTEXT_START))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not start\n", vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->dqbuf(&vctx->queue_list, clist);
	if (ret)
		goto p_err_queue_ops;

	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_streamon(struct vertex_context *vctx)
{
	int ret;
	struct vertex_core *core;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for stream-on (%d)\n",
				ret);
		goto p_err_lock;
	}

	if (!(vctx->state & (BIT(VERTEX_CONTEXT_FORMAT) |
					BIT(VERTEX_CONTEXT_STOP)))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not format/stop\n",
				vctx->state);
		goto p_err_state;
	}

	core = vctx->core;
	ret = __vref_get(&core->start_cnt);
	if (ret) {
		vertex_err("vref_get(start) is fail(%d)\n", ret);
		goto p_err_vref;
	}

	ret = vctx->queue_ops->streamon(&vctx->queue_list);
	if (ret)
		goto p_err_queue_ops;

	vctx->state = BIT(VERTEX_CONTEXT_START);
	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_queue_ops:
p_err_vref:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

static int vertex_core_streamoff(struct vertex_context *vctx)
{
	int ret;
	struct vertex_core *core;

	vertex_enter();
	if (mutex_lock_interruptible(&vctx->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock context lock for stream-off (%d)\n",
				ret);
		goto p_err_lock;
	}

	if (!(vctx->state & BIT(VERTEX_CONTEXT_START))) {
		ret = -EINVAL;
		vertex_err("Context state(%x) is not start\n", vctx->state);
		goto p_err_state;
	}

	ret = vctx->queue_ops->streamoff(&vctx->queue_list);
	if (ret)
		goto p_err_queue_ops;

	core = vctx->core;
	ret = __vref_put(&core->start_cnt);
	if (ret) {
		vertex_err("vref_put(start) is fail(%d)\n", ret);
		goto p_err_vref;
	}

	vctx->state = BIT(VERTEX_CONTEXT_STOP);
	mutex_unlock(&vctx->lock);
	vertex_leave();
	return 0;
p_err_vref:
p_err_queue_ops:
p_err_state:
	mutex_unlock(&vctx->lock);
p_err_lock:
	return ret;
}

const struct vertex_ioctl_ops vertex_core_ioctl_ops = {
	.set_graph		= vertex_core_set_graph,
	.set_format		= vertex_core_set_format,
	.set_param		= vertex_core_set_param,
	.set_ctrl		= vertex_core_set_ctrl,
	.qbuf			= vertex_core_qbuf,
	.dqbuf			= vertex_core_dqbuf,
	.streamon		= vertex_core_streamon,
	.streamoff		= vertex_core_streamoff,
};

static int vertex_open(struct inode *inode, struct file *file)
{
	int ret;
	struct miscdevice *miscdev;
	struct vertex_device *device;
	struct vertex_core *core;
	struct vertex_context *vctx;
	struct vertex_graph *graph;

	vertex_enter();
	miscdev = file->private_data;
	device = dev_get_drvdata(miscdev->parent);
	core = &device->core;

	if (mutex_lock_interruptible(&core->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock device lock for open (%d)\n", ret);
		goto p_err_lock;
	}

	ret = __vref_get(&core->open_cnt);
	if (ret) {
		vertex_err("vref_get(open) is fail(%d)", ret);
		goto p_err_vref;
	}

	vctx = vertex_context_create(core);
	if (IS_ERR(vctx)) {
		ret = PTR_ERR(vctx);
		goto p_err_vctx;
	}

	graph = vertex_graph_create(vctx, &core->system->graphmgr);
	if (IS_ERR(graph)) {
		ret = PTR_ERR(graph);
		goto p_err_graph;
	}
	vctx->graph = graph;

	file->private_data = vctx;

	mutex_unlock(&core->lock);
	vertex_leave();
	return 0;
p_err_graph:
p_err_vctx:
	__vref_put(&core->open_cnt);
p_err_vref:
	mutex_unlock(&core->lock);
p_err_lock:
	return ret;
}

static int vertex_release(struct inode *inode, struct file *file)
{
	int ret;
	struct vertex_context *vctx;
	struct vertex_core *core;

	vertex_enter();
	vctx = file->private_data;
	core = vctx->core;

	if (mutex_lock_interruptible(&core->lock)) {
		ret = -ERESTARTSYS;
		vertex_err("Failed to lock device lock for release (%d)\n",
				ret);
		return ret;
	}

	vertex_graph_destroy(vctx->graph);
	vertex_context_destroy(vctx);
	__vref_put(&core->open_cnt);

	mutex_unlock(&core->lock);
	vertex_leave();
	return 0;
}

static unsigned int vertex_poll(struct file *file,
		struct poll_table_struct *poll)
{
	int ret;
	struct vertex_context *vctx;

	vertex_enter();
	vctx = file->private_data;
	if (!(vctx->state & BIT(VERTEX_CONTEXT_START))) {
		ret = POLLERR;
		vertex_err("Context state(%x) is not start (%d)\n",
				vctx->state, ret);
		goto p_err;
	}

	ret = vctx->queue_ops->poll(&vctx->queue_list, file, poll);
	if (ret)
		goto p_err;

	vertex_leave();
	return 0;
p_err:
	return ret;
}

const struct file_operations vertex_file_ops = {
	.owner		= THIS_MODULE,
	.open		= vertex_open,
	.release	= vertex_release,
	.poll		= vertex_poll,
	.unlocked_ioctl	= vertex_ioctl,
	.compat_ioctl	= vertex_compat_ioctl
};

static int __vref_open(struct vertex_core *core)
{
	vertex_check();
	atomic_set(&core->start_cnt.refcount, 0);
	return vertex_device_open(core->device);
}

static int __vref_close(struct vertex_core *core)
{
	vertex_check();
	return vertex_device_close(core->device);
}

static int __vref_start(struct vertex_core *core)
{
	vertex_check();
	return vertex_device_start(core->device);
}

static int __vref_stop(struct vertex_core *core)
{
	vertex_check();
	return vertex_device_stop(core->device);
}

static inline void __vref_init(struct vertex_core_refcount *vref,
	struct vertex_core *core,
	int (*first)(struct vertex_core *core),
	int (*final)(struct vertex_core *core))
{
	vertex_enter();
	vref->core = core;
	vref->first = first;
	vref->final = final;
	atomic_set(&vref->refcount, 0);
	vertex_leave();
}

/* Top-level data for debugging */
static struct vertex_dev *vdev;

int vertex_core_probe(struct vertex_device *device)
{
	int ret;
	struct vertex_core *core;

	vertex_enter();
	core = &device->core;
	core->device = device;
	core->system = &device->system;
	vdev = &core->vdev;

	mutex_init(&core->lock);
	__vref_init(&core->open_cnt, core, __vref_open, __vref_close);
	__vref_init(&core->start_cnt, core, __vref_start, __vref_stop);
	core->ioc_ops = &vertex_core_ioctl_ops;

	vertex_util_bitmap_init(core->vctx_map, VERTEX_MAX_CONTEXT);
	INIT_LIST_HEAD(&core->vctx_list);
	core->vctx_count = 0;

	vdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	vdev->miscdev.name = VERTEX_DEV_NAME;
	vdev->miscdev.fops = &vertex_file_ops;
	vdev->miscdev.parent = device->dev;

	ret = misc_register(&vdev->miscdev);
	if (ret) {
		vertex_err("miscdevice is not registered (%d)\n", ret);
		goto p_err_misc;
	}

	vertex_leave();
	return 0;
p_err_misc:
	mutex_destroy(&core->lock);
	return ret;
}

void vertex_core_remove(struct vertex_core *core)
{
	vertex_enter();
	misc_deregister(&core->vdev.miscdev);
	mutex_destroy(&core->lock);
	vertex_leave();
}
