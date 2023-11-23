// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "dsp-log.h"
#include "dsp-util.h"
#include "dsp-device.h"
#include "hardware/dsp-mailbox.h"
#include "dsp-common-type.h"
#include "dsp-context.h"

static int dsp_context_boot(struct dsp_context *dctx, struct dsp_ioc_boot *args)
{
	int ret;
	struct dsp_core *core;

	dsp_enter();
	core = dctx->core;

	mutex_lock(&dctx->lock);

	ret = dsp_device_start(core->dspdev);
	if (ret) {
		mutex_unlock(&dctx->lock);
		goto p_err_device;
	}

	dctx->boot_count++;
	mutex_unlock(&dctx->lock);

	dsp_leave();
	return 0;
p_err_device:
	return ret;
}

static int __dsp_context_get_graph_info(struct dsp_context *dctx,
		struct dsp_ioc_load_graph *load, void *ginfo, void *kernel_name)
{
	int ret;

	dsp_enter();
	ret = copy_from_user(ginfo, (void *)load->param_addr, load->param_size);
	if (ret) {
		dsp_err("Failed to copy form user param_addr(%d)\n", ret);
		goto p_err_copy;
	}

	ret = copy_from_user(kernel_name, (void *)load->kernel_addr,
			load->kernel_size);
	if (ret) {
		dsp_err("Failed to copy form user kernel_addr(%d)\n", ret);
		goto p_err_copy;
	}

	dsp_leave();
	return 0;
p_err_copy:
	return ret;
}

static void __dsp_context_put_graph_info(struct dsp_context *dctx, void *ginfo)
{
	dsp_enter();
	dsp_leave();
}

static int dsp_context_load_graph(struct dsp_context *dctx,
		struct dsp_ioc_load_graph *args)
{
	int ret;
	struct dsp_system *sys;
	void *kernel_name;
	struct dsp_mailbox_pool *pool;
	struct dsp_common_graph_info_v1 *ginfo1;
	struct dsp_common_graph_info_v2 *ginfo2;
	struct dsp_common_graph_info_v3 *ginfo3;
	struct dsp_graph *graph;
	unsigned int version;

	dsp_enter();
	sys = &dctx->core->dspdev->system;
	version = args->version;

	kernel_name = vzalloc(args->kernel_size);
	if (!kernel_name) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate kernel_name(%d)\n",
				args->kernel_size);
		goto p_err_klist;
	}

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, args->param_size);
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto p_err_pool;
	}

	if (version == DSP_IOC_V1) {
		ginfo1 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo1,
				kernel_name);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&ginfo1->global_id, dctx->id);
	} else if (version == DSP_IOC_V2) {
		ginfo2 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo2,
				kernel_name);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&ginfo2->global_id, dctx->id);
	} else if (version == DSP_IOC_V3) {
		ginfo3 = pool->kva;
		ret = __dsp_context_get_graph_info(dctx, args, ginfo3,
				kernel_name);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&ginfo3->global_id, dctx->id);
	} else {
		ret = -EINVAL;
		dsp_err("Failed to load graph due to invalid version(%u)\n",
				version);
		goto p_err_info;
	}

	graph = dsp_graph_load(&dctx->core->graph_manager, pool,
			kernel_name, version);
	if (IS_ERR(graph)) {
		ret = PTR_ERR(graph);
		goto p_err_graph;
	}

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;

	if (version == DSP_IOC_V1)
		__dsp_context_put_graph_info(dctx, ginfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_graph_info(dctx, ginfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_graph_info(dctx, ginfo3);
	dsp_leave();
	return 0;
p_err_graph:
	if (version == DSP_IOC_V1)
		__dsp_context_put_graph_info(dctx, ginfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_graph_info(dctx, ginfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_graph_info(dctx, ginfo3);
p_err_info:
	dsp_mailbox_free_pool(pool);
p_err_pool:
	vfree(kernel_name);
p_err_klist:
	return ret;

}

static int dsp_context_unload_graph(struct dsp_context *dctx,
		struct dsp_ioc_unload_graph *args)
{
	int ret;
	struct dsp_system *sys;
	struct dsp_graph *graph;
	struct dsp_mailbox_pool *pool;
	unsigned int *global_id;

	dsp_enter();
	sys = &dctx->core->dspdev->system;

	SET_COMMON_CONTEXT_ID(&args->global_id, dctx->id);
	graph = dsp_graph_get(&dctx->core->graph_manager, args->global_id);
	if (!graph) {
		ret = -EINVAL;
		dsp_err("graph is not loaded(%x)\n", args->global_id);
		goto p_err;
	}

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, sizeof(args->global_id));
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto p_err;
	}
	global_id = pool->kva;
	*global_id = args->global_id;

	dsp_graph_unload(graph, pool);

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;

	dsp_mailbox_free_pool(pool);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_context_get_execute_info(struct dsp_context *dctx,
		struct dsp_ioc_execute_msg *execute, void *einfo)
{
	int ret;
	unsigned int size;

	dsp_enter();
	size = execute->size;

	if (!size) {
		ret = -EINVAL;
		dsp_err("size for execute_msg is invalid(%u)\n", size);
		goto p_err;
	}

	ret = copy_from_user(einfo, (void *)execute->addr, size);
	if (ret) {
		dsp_err("Failed to copy form user addr(%d)\n", ret);
		goto p_err_copy;
	}

	dsp_leave();
	return 0;
p_err_copy:
p_err:
	return ret;
}

static void __dsp_context_put_execute_info(struct dsp_context *dctx, void *info)
{
	dsp_enter();
	dsp_leave();
}

static int dsp_context_execute_msg(struct dsp_context *dctx,
		struct dsp_ioc_execute_msg *args)
{
	int ret;
	struct dsp_system *sys;
	struct dsp_mailbox_pool *pool;
	struct dsp_common_execute_info_v1 *einfo1;
	struct dsp_common_execute_info_v2 *einfo2;
	struct dsp_common_execute_info_v3 *einfo3;
	struct dsp_graph *graph;
	unsigned int version;
	unsigned int einfo_global_id;

	dsp_enter();
	sys = &dctx->core->dspdev->system;
	version = args->version;

	pool = dsp_mailbox_alloc_pool(&sys->mailbox, args->size);
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto p_err_pool;
	}

	if (version == DSP_IOC_V1) {
		einfo1 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo1);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo1->global_id, dctx->id);
		einfo_global_id = einfo1->global_id;
	} else if (version == DSP_IOC_V2) {
		einfo2 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo2);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo2->global_id, dctx->id);
		einfo_global_id = einfo2->global_id;
	} else if (version == DSP_IOC_V3) {
		einfo3 = pool->kva;
		ret = __dsp_context_get_execute_info(dctx, args, einfo3);
		if (ret)
			goto p_err_info;
		SET_COMMON_CONTEXT_ID(&einfo3->global_id, dctx->id);
		einfo_global_id = einfo3->global_id;
	} else {
		ret = -EINVAL;
		dsp_err("Failed to execute msg due to invalid version(%u)\n",
				version);
		goto p_err_info;
	}

	graph = dsp_graph_get(&dctx->core->graph_manager, einfo_global_id);
	if (!graph) {
		ret = -EINVAL;
		dsp_err("graph is not loaded(%x)\n", einfo_global_id);
		goto p_err_graph_get;
	}

	if (graph->version != version) {
		ret = -EINVAL;
		dsp_err("graph has different message_version(%u/%u)\n",
				graph->version, version);
		goto p_err_graph_version;
	}

	ret = dsp_graph_execute(graph, pool);
	if (ret)
		goto p_err_graph_execute;

	args->timestamp[0] = pool->time.start;
	args->timestamp[1] = pool->time.end;

	if (version == DSP_IOC_V1)
		__dsp_context_put_execute_info(dctx, einfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_execute_info(dctx, einfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_execute_info(dctx, einfo3);

	dsp_mailbox_free_pool(pool);

	dsp_leave();
	return 0;
p_err_graph_execute:
p_err_graph_version:
p_err_graph_get:
	if (version == DSP_IOC_V1)
		__dsp_context_put_execute_info(dctx, einfo1);
	else if (version == DSP_IOC_V2)
		__dsp_context_put_execute_info(dctx, einfo2);
	else if (version == DSP_IOC_V3)
		__dsp_context_put_execute_info(dctx, einfo3);
p_err_info:
	dsp_mailbox_free_pool(pool);
p_err_pool:
	return ret;
}

const struct dsp_ioctl_ops dsp_ioctl_ops = {
	.boot		= dsp_context_boot,
	.load_graph	= dsp_context_load_graph,
	.unload_graph	= dsp_context_unload_graph,
	.execute_msg	= dsp_context_execute_msg,
};

struct dsp_context *dsp_context_create(struct dsp_core *core)
{
	int ret;
	struct dsp_context *dctx;
	unsigned long flags;

	dsp_enter();
	dctx = kzalloc(sizeof(*dctx), GFP_KERNEL);
	if (!dctx) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc context\n");
		goto p_err;
	}
	dctx->core = core;

	spin_lock_irqsave(&core->dctx_slock, flags);

	dctx->id = dsp_util_bitmap_set_region(&core->context_map, 1);
	if (dctx->id < 0) {
		spin_unlock_irqrestore(&core->dctx_slock, flags);
		ret = dctx->id;
		dsp_err("Failed to allocate context bitmap(%d)\n", ret);
		goto p_err_alloc_id;
	}

	list_add_tail(&dctx->list, &core->dctx_list);
	core->dctx_count++;
	spin_unlock_irqrestore(&core->dctx_slock, flags);

	mutex_init(&dctx->lock);
	dsp_leave();
	return dctx;
p_err_alloc_id:
	kfree(dctx);
p_err:
	return ERR_PTR(ret);
}

void dsp_context_destroy(struct dsp_context *dctx)
{
	struct dsp_core *core;
	unsigned long flags;

	dsp_enter();
	core = dctx->core;

	if (dctx->boot_count)
		dsp_device_stop(core->dspdev, dctx->boot_count);

	mutex_destroy(&dctx->lock);

	spin_lock_irqsave(&core->dctx_slock, flags);
	core->dctx_count--;
	list_del(&dctx->list);
	dsp_util_bitmap_clear_region(&core->context_map, dctx->id, 1);
	spin_unlock_irqrestore(&core->dctx_slock, flags);

	kfree(dctx);
	dsp_leave();
}

const struct dsp_ioctl_ops *dsp_context_get_ops(void)
{
	dsp_check();
	return &dsp_ioctl_ops;
}
