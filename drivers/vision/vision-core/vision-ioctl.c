/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/compat.h>

#include "vision-config.h"
#include "vision-dev.h"
#include "vision-ioctl.h"
#include "common/npu-log.h"
#include "npu-vertex.h"
#include "common/npu-profile-v2.h"

static unsigned long __npu_access_ok(const void __user *addr, unsigned long size)
{
	return access_ok(addr, size);
}

#define MASK_COMPAT_LONG(u) ((u) &= 0xFFFFFFFFUL & (u))
#define MASK_COMPAT_UPTR(p) (p = ((void *) (0xFFFFFFFFUL & ((ulong) (p)))))

static int get_vs4l_ctrl64(struct vs4l_ctrl *kp, struct vs4l_ctrl __user *up)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_ctrl))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_ctrl;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_ctrl));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_ctrl;
	}

p_err_ctrl:
	return ret;
}

static void put_vs4l_ctrl64(struct vs4l_ctrl *kp, struct vs4l_ctrl __user *up)
{
	int ret;

	ret = copy_to_user(up, kp, sizeof(struct vs4l_ctrl));
	if (ret)
		vision_err("Failed to copy to user at ctrl.\n", ret);
}


static int get_vs4l_graph64(struct vs4l_graph *kp,
			    struct vs4l_graph __user *up, bool chk_compat)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_graph))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_graph;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_graph));

	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_graph;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_LONG(kp->addr);

p_err_graph:
	return ret;
}

static int get_vs4l_sched_param64(struct vs4l_sched_param *kp, struct vs4l_sched_param __user *up)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_sched_param))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_sched_param));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

p_err:
	return ret;
}

static void put_vs4l_sched_param64(struct vs4l_sched_param *kp,
				   struct vs4l_sched_param __user *up)
{
}

static int put_vs4l_max_freq(struct vs4l_freq_param *kp,
			     struct vs4l_freq_param __user *up)
{
	int ret = 0;
	if (!__npu_access_ok(up, sizeof(struct vs4l_freq_param))) {
		vision_err("Cannot access to user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_freq;
	}

	if (copy_to_user((void *)up, (void *)kp, sizeof(struct vs4l_freq_param))) {
		vision_err("Copy_to_user failed (%pK -> %pK)\n", kp, up);
		ret = -EFAULT;
		goto p_err_freq;
	}

	return ret;

p_err_freq:
	vision_err("return with fail... (%d)\n", ret);
	return ret;
}

static int get_vs4l_format64(struct vs4l_format_list *kp,
			     struct vs4l_format_list __user *up,
			     bool chk_compat)
{
	int ret = 0;
	size_t size = 0;
	struct vs4l_format *kformats_ptr;

	if (!__npu_access_ok(up, sizeof(struct vs4l_format_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_format;
	}

	ret = copy_from_user((void *)kp, (void __user *)up,
				sizeof(struct vs4l_format_list));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_format;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->formats);

	if (kp->count > VISION_MAX_BUFFER) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_BUFFER(%d)\n", kp->count, VISION_MAX_BUFFER);
		ret = -EINVAL;
		goto p_err_format;
	}

	size = kp->count * sizeof(struct vs4l_format);
	if (!__npu_access_ok((void __user *)kp->formats, size)) {
		vision_err("acesss is fail\n");
		ret = -EFAULT;
		goto p_err_format;
	}

	kformats_ptr = kmalloc(size, GFP_KERNEL);
	if (!kformats_ptr) {
		ret = -ENOMEM;
		goto p_err_format;
	}

	ret = copy_from_user(kformats_ptr, (void __user *)kp->formats, size);
	if (ret) {
		vision_err("copy_from_user failed(%d)\n", ret);
		kfree(kformats_ptr);
		ret = -EFAULT;
		goto p_err_format;
	}

	kp->formats = kformats_ptr;

p_err_format:
	return ret;
}

static void put_vs4l_format64(struct vs4l_format_list *kp, struct vs4l_format_list __user *up)
{
	kfree(kp->formats);
}

static int get_vs4l_param64(struct vs4l_param_list *kp,
			    struct vs4l_param_list __user *up, bool chk_compat)
{
	int i, ret;
	size_t size = 0;
	struct vs4l_param *kparams_ptr;

	if (!__npu_access_ok(up, sizeof(struct vs4l_param_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_param;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_param_list));

	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_param;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->params);

	if (kp->count > VISION_MAX_BUFFER) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_BUFFER(%d)\n", kp->count, VISION_MAX_BUFFER);
		ret = -EINVAL;
		goto p_err_param;
	}

	size = kp->count * sizeof(struct vs4l_param);
	if (!__npu_access_ok((void __user *)kp->params, size)) {
		vision_err("access failed\n");
		ret = -EFAULT;
		goto p_err_param;
	}

	kparams_ptr = kmalloc(size, GFP_KERNEL);
	if (!kparams_ptr) {
		ret = -ENOMEM;
		goto p_err_param;
	}

	ret = copy_from_user(kparams_ptr, (void __user *)kp->params, size);
	if (ret) {
		vision_err("copy_from_user failed(%d)\n", ret);
		kfree(kparams_ptr);
		ret = -EFAULT;
		goto p_err_param;
	}

	if (unlikely(chk_compat)) {
		for (i = 0; i < kp->count; i++) {
			MASK_COMPAT_LONG(kparams_ptr[i].addr);
		}
	}

	kp->params = kparams_ptr;

p_err_param:
	return ret;
}

static void put_vs4l_param64(struct vs4l_param_list *kp, struct vs4l_param_list __user *up)
{
	kfree(kp->params);
}

static int get_vs4l_containers(struct vs4l_container_list *kp,
				struct vs4l_container_list __user *up,
				bool chk_compat)
{
	int ret, i;
	size_t size = 0;
	struct vs4l_container *kcontainer_ptr;

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_container_list));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		return ret;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->containers);

	/* container_list -> (vs4l_container)containers[count] -> (vs4l_buffer)buffers[count] */
	if (kp->count > VISION_MAX_CONTAINERLIST) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_CONTAINERLIST(%d)\n", kp->count, VISION_MAX_CONTAINERLIST);
		ret = -EINVAL;
		return ret;
	}

	size = kp->count * sizeof(struct vs4l_container);
	if (!__npu_access_ok((void __user *)kp->containers, size)) {
		vision_err("access to containers ptr failed (%pK)\n",
				kp->containers);
		ret = -EFAULT;
		return ret;
	}

	/* NOTE: hoon98.choi: check if devm_kzalloc() for memory management */
	kcontainer_ptr = kzalloc(size, GFP_KERNEL);
	if (!kcontainer_ptr) {
		ret = -ENOMEM;
		return ret;
	}

	ret = copy_from_user(kcontainer_ptr, (void __user *)kp->containers, size);
	if (ret) {
		vision_err("error from copy_from_user(%d), size(%zu)\n", ret, size);
		ret = -EFAULT;
		goto p_err_container;
	}

	if (unlikely(chk_compat)) {
		for (i = 0; i < kp->count; i++) {
			MASK_COMPAT_UPTR(kcontainer_ptr[i].buffers);
		}
	}

	kp->containers = kcontainer_ptr;

	return ret;

p_err_container:
	kfree(kcontainer_ptr);
	kp->containers = NULL;
	return ret;
}

static int get_vs4l_buffers(struct vs4l_container_list *kp,
				struct vs4l_container_list __user *up,
				bool chk_compat)
{
	int ret, i, j, free_buf_num;
	size_t size = 0;
	struct vs4l_buffer *kbuffer_ptr = NULL;

	/* fill each container from user's request */
	for (i = 0, free_buf_num = 0; i < kp->count; i++, free_buf_num++) {
		size = kp->containers[i].count * sizeof(struct vs4l_buffer);

		if (!__npu_access_ok((void __user *)
					kp->containers[i].buffers, size)) {
			vision_err("access to containers ptr failed (%pK)\n",
					kp->containers[i].buffers);
			ret = -EFAULT;
			goto p_err_buffer;
		}

		kbuffer_ptr = kmalloc(size, GFP_KERNEL);
		if (!kbuffer_ptr) {
			ret = -ENOMEM;
			goto p_err_buffer;
		}

		ret = copy_from_user(kbuffer_ptr,
				(void __user *)kp->containers[i].buffers, size);
		if (ret) {
			vision_err("error from copy_from_user(idx: %d, ret: %d, size: %zu)\n",
					i, ret, size);
			ret = -EFAULT;
			goto p_err_buffer_malloc;
		}

		if (unlikely(chk_compat)) {
			if (kp->containers[i].memory != VS4L_MEMORY_DMABUF) {
				for (j = 0; j < kp->containers[i].count; j++) {
					MASK_COMPAT_LONG(kbuffer_ptr[j].m.userptr);
				}
			}
		}

		kp->containers[i].buffers = kbuffer_ptr;
	}

	return 0;

p_err_buffer_malloc:
	kfree(kbuffer_ptr);
	kbuffer_ptr = NULL;

p_err_buffer:
	for (i = 0; i < free_buf_num; i++) {
		kfree(kp->containers[i].buffers);
		kp->containers[i].buffers = NULL;
	}
	return ret;
}
static int get_vs4l_container64(struct vs4l_container_list *kp,
				struct vs4l_container_list __user *up,
				bool chk_compat)
{
	int ret = 0;

	if (!__npu_access_ok(up, sizeof(struct vs4l_container_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = get_vs4l_containers(kp, up, chk_compat);
	if (ret)
		goto p_err;

	ret = get_vs4l_buffers(kp, up, chk_compat);
	if (ret) {
		kfree(kp->containers);
		goto p_err;
	}

	return 0;

p_err:
	vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static int put_vs4l_container64(struct vs4l_container_list *kp,
				struct vs4l_container_list __user *up,
				bool chk_compat)
{
	int ret, i;
	size_t ksize;
	struct vs4l_container_list temp;

	if (!__npu_access_ok(up, sizeof(struct vs4l_container_list)))
		vision_err("Cannot access to user ptr(%pK)\n", up);

	ret = get_vs4l_containers(&temp, up, chk_compat);
	if (ret)
		goto p_err;

	for (i = 0; i < kp->count; i++) {
		ksize = kp->containers[i].count * sizeof(struct vs4l_buffer);
		ret = copy_to_user((void *)temp.containers[i].buffers, (void *)kp->containers[i].buffers, ksize);
		if (ret) {
			vision_err("Failed to copy to user at version.\n", ret);
		}
	}

	if (put_user(kp->flags, &up->flags) ||
		put_user(kp->index, &up->index) ||
		put_user(kp->id, &up->id) ||
		put_user(kp->timestamp[0].tv_sec, &up->timestamp[0].tv_sec) ||
		put_user(kp->timestamp[0].tv_usec, &up->timestamp[0].tv_usec) ||
		put_user(kp->timestamp[1].tv_sec, &up->timestamp[1].tv_sec) ||
		put_user(kp->timestamp[1].tv_usec, &up->timestamp[1].tv_usec) ||
		put_user(kp->timestamp[2].tv_sec, &up->timestamp[2].tv_sec) ||
		put_user(kp->timestamp[2].tv_usec, &up->timestamp[2].tv_usec) ||
		put_user(kp->timestamp[3].tv_sec, &up->timestamp[3].tv_sec) ||
		put_user(kp->timestamp[3].tv_usec, &up->timestamp[3].tv_usec) ||
		put_user(kp->timestamp[4].tv_sec, &up->timestamp[4].tv_sec) ||
		put_user(kp->timestamp[4].tv_usec, &up->timestamp[4].tv_usec) ||
		put_user(kp->timestamp[5].tv_sec, &up->timestamp[5].tv_sec) ||
		put_user(kp->timestamp[5].tv_usec, &up->timestamp[5].tv_usec)) {
		vision_err("Copy_to_user failed (%pK -> %pK)\n", kp, up);
	}

	for (i = 0; i < kp->count; ++i)
		kfree(kp->containers[i].buffers);

	kfree(kp->containers);
	kfree(temp.containers);
	kp->containers = NULL;
	temp.containers = NULL;

	return 0;

p_err:
	vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static inline int
__copy_from_usr_vs4l(void *kp, void __user *up, size_t len, const char *msg)
{
	int ret = 0;

	if (!__npu_access_ok(up, len)) {
		vision_err("%s: access_ok fail uptr(%pK) size(%zu)\n",
			   msg, up, len);
		return -EFAULT;
	}

	ret = copy_from_user(kp, up, len);
	if (ret) {
		vision_err("%s: copy_from_user fail ret(%d) size(%zu)\n",
			   msg, ret, len);
		return -EFAULT;
	}
	return 0;
}

static inline void*
copy_from_usr_vs4l(void __user *up, size_t len, const char *msg)
{
	int ret = 0;
	void *kp = NULL;

	kp = kzalloc(len, GFP_KERNEL);
	if (!kp) {
		vision_err("%s: kzalloc fail\n", msg);
		return ERR_PTR(-ENOMEM);
	}

	ret = __copy_from_usr_vs4l(kp, up, len, msg);
	if (ret) {
		kfree(kp);
		return ERR_PTR(ret);
	}
	return kp;
}

static int
get_vs4l_profiler_node_child(struct vs4l_profiler *kp, bool chk_compat)
{
	int i, done_cnt, ret = 0;
	size_t usize, ksize;
	void *tmpkp = NULL;
	struct vs4l_profiler_node **child = NULL;
	compat_uptr_t tmp_child[3];

	ksize = sizeof(struct vs4l_profiler_node *) * 3;
	usize = unlikely(chk_compat) ? sizeof(compat_uptr_t) * 3 : ksize;

	if (!(child = kzalloc(ksize, GFP_KERNEL))) {
		vision_err("profiler_child kzalloc fail\n");
		return -ENOMEM;
	}

	tmpkp = unlikely(chk_compat) ? (void*) tmp_child : (void*) child;
	ret = __copy_from_usr_vs4l(tmpkp, (void __user *)kp->node->child,
				   usize, "profiler_child");

	if (ret) goto p_err_profiler_child1;

	if (unlikely(chk_compat)) {
		for (i = 0; i < 3; i++)
			child[i] = (struct vs4l_profiler_node *) ((ulong) tmp_child[i]);
	}

	// NPU & DSP HW node
	for (done_cnt = 0; done_cnt < 2; done_cnt++) {
		if (!child[done_cnt]) continue;

		tmpkp = copy_from_usr_vs4l((void __user *) child[done_cnt],
					   sizeof(struct vs4l_profiler_node),
					   "profiler_child");
		if (IS_ERR(tmpkp)) {
			ret = (int) PTR_ERR(tmpkp);
			goto p_err_profiler_child2;
		}
		child[done_cnt] = tmpkp;
	}
	kp->node->child = child;
	return ret;
p_err_profiler_child2:
	for (i = 0; i < done_cnt; i++)
		kfree(child[i]);

p_err_profiler_child1:
	kfree(child);
	kp->node->child = NULL;
	return ret;
}

static int
get_vs4l_profiler_node(struct vs4l_profiler *kp, bool chk_compat)
{
	int ret = 0;
	struct vs4l_profiler_node *node = NULL;

	node = copy_from_usr_vs4l((void __user *)kp->node,
				  sizeof(*node), "profiler_node");

	if (IS_ERR(node)) {
		ret = (int) PTR_ERR(node);
		goto p_err_profiler_node;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(node->child);
	kp->node = node;

	if (kp->node->child) {
		ret = get_vs4l_profiler_node_child(kp, chk_compat);
		if (ret) {
			vision_err("get_vs4l_profiler_node_child fail\n");
			goto p_err_profiler_node;
		}
	}
	return ret;
p_err_profiler_node:
	kfree(node);
	kp->node = NULL;
	return ret;
}

static int get_vs4l_profiler(struct vs4l_profiler *kp,
			     struct vs4l_profiler __user *up,
			     bool chk_compat)
{
	int ret = 0;

	if (unlikely(!kp || !up)) return -EFAULT;

	ret = __copy_from_usr_vs4l(kp, up, sizeof(*kp), "profiler");
	if (ret) goto p_err;

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->node);
	if (kp->node) ret = get_vs4l_profiler_node(kp, chk_compat);
p_err:
	if (ret) vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static int
put_vs4l_profiler_node_child(struct vs4l_profiler *kp,
			     struct vs4l_profiler_node __user **uchild,
			     bool chk_compat)
{
	int i, ret;
	size_t usize, ksize;
	void *tmpkp = NULL;
	struct vs4l_profiler_node *kchild[3];
	compat_uptr_t comp_kchild[3];

	ksize = sizeof(struct vs4l_profiler_node *) * 3;
	usize = unlikely(chk_compat) ? sizeof(compat_uptr_t) * 3 : ksize;
	tmpkp = unlikely(chk_compat) ? (void*) comp_kchild : (void*) kchild;

	ret = __copy_from_usr_vs4l(tmpkp, (void __user *)uchild,
				   usize, "profiler_child");

	if (ret) return ret;

	if (unlikely(chk_compat)) {
		for (i = 0; i < 3; i++)
			kchild[i] = (struct vs4l_profiler_node *) ((ulong) comp_kchild[i]);
	}

	// copy to user - NPU, DSP HW duration
	for (i = 0; i < 2; i++) {
		if (!kp->node->child[i] || !kchild[i]) continue;

		ret = put_user(kp->node->child[i]->duration,
			       &kchild[i]->duration);
		if (ret) {
			vision_err("NPU %d-th HW duration update. put_user fail\n", i);
		}

	}
	return ret;
}

static int
put_vs4l_profiler_node(struct vs4l_profiler *kp,
		       struct vs4l_profiler_node __user *node,
		       bool chk_compat)
{
	int ret;
	struct vs4l_profiler_node temp;

	// copy to user - Firmware duration
	if (put_user(kp->node->duration, &node->duration)) {
		vision_err("fw duration put_user fail\n");
		return -EFAULT;
	}

	ret = __copy_from_usr_vs4l(&temp, (void __user *) node,
				   sizeof(temp), "profiler_node");
	if (ret) return ret;

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.child);

	if (temp.child) {
		ret = put_vs4l_profiler_node_child(kp, temp.child,
						   chk_compat);
	}
	return ret;
}

static inline void free_vs4l_profiler_node(struct vs4l_profiler_node *kn)
{
	int i;

	if (!kn) return;
	if (!kn->child) goto free_kn;

	for (i = 0; i < 3; i++) {
		if (kn->child[i]) kfree(kn->child[i]);
	}
	kfree(kn->child);
free_kn:
	kfree(kn);
}

static int put_vs4l_profiler(struct vs4l_profiler *kp,
			     struct vs4l_profiler __user *up,
			     bool chk_compat)
{
	int ret;
	struct vs4l_profiler temp;

	if (unlikely(!kp || !up)) return -EFAULT;

	ret = __copy_from_usr_vs4l(&temp, up, sizeof(temp), "profiler");
	if (ret) goto p_err;

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.node);
	if (kp->node && temp.node) {
		ret = put_vs4l_profiler_node(kp, temp.node, chk_compat);
	}
p_err:
	if (kp->node)
		free_vs4l_profiler_node(kp->node);
	kp->node = NULL;
	if (ret) vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static int get_vs4l_version(struct vs4l_version *kp,
			    struct vs4l_version __user *up, bool chk_compat)
{
	int ret = 0;
	char *fw_version;
	char *fw_hash;

	if (!__npu_access_ok(up, sizeof(*kp))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_version;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(*kp));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_version;
	}

	fw_version = kzalloc(VISION_MAX_FW_VERSION_LEN, GFP_KERNEL);
	if (!fw_version) {
		ret = -ENOMEM;
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->fw_version);
	ret = copy_from_user(fw_version, (void __user *)kp->fw_version,
			     VISION_MAX_FW_VERSION_LEN);
	if (ret) {
		vision_err("error copy_from_user(%d) about fw version\n", ret);
		ret = -EFAULT;
		kfree(fw_version);
		goto p_err_version;
	}
	kp->fw_version = fw_version;

	fw_hash = kzalloc(VISION_MAX_FW_HASH_LEN, GFP_KERNEL);
	if (!fw_hash) {
		ret = -ENOMEM;
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->fw_hash);
	ret = copy_from_user(fw_hash, (void __user *)kp->fw_hash,
			     VISION_MAX_FW_HASH_LEN);
	if (ret) {
		vision_err("error copy_from_user(%d) about fw hash\n", ret);
		ret = -EFAULT;
		kfree(fw_version);
		kfree(fw_hash);
		goto p_err_version;
	}
	kp->fw_hash = fw_hash;
p_err_version:
	return ret;
}

static int put_vs4l_version(struct vs4l_version *kp,
			    struct vs4l_version __user *up, bool chk_compat)
{
	int ret = 0;
	struct vs4l_version temp;

	if (!__npu_access_ok(up, sizeof(*kp)))
		vision_err("Cannot access to user ptr(%pK)\n", up);

	ret = copy_from_user(&temp, (void __user *)up, sizeof(*kp));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.build_info);
	ret = copy_to_user(temp.build_info, kp->build_info,
			   strlen(kp->build_info) + 1);
	if (ret) {
		vision_err("Failed to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.signature);
	ret = copy_to_user(temp.signature, kp->signature,
			   strlen(kp->signature) + 1);
	if (ret) {
		vision_err("Failed to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.driver_version);
	ret = copy_to_user(temp.driver_version, kp->driver_version,
			   strlen(kp->driver_version) + 1);
	if (ret) {
		vision_err("Failed to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.fw_version);
	ret = copy_to_user(temp.fw_version, kp->fw_version,
			   strlen(kp->fw_version));
	if (ret) {
		vision_err("Failed to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.fw_hash);
	ret = copy_to_user(temp.fw_hash, kp->fw_hash, strlen(kp->fw_hash));
	if (ret) {
		vision_err("Failed to copy to user at version.\n", ret);
		goto p_err_version;
	}

	put_user(kp->ncp_version, &up->ncp_version);
	put_user(kp->mailbox_version, &up->mailbox_version);
	put_user(kp->cmd_version, &up->cmd_version);
	put_user(kp->api_version, &up->api_version);

	kfree(kp->fw_version);
	kfree(kp->fw_hash);

p_err_version:
	return ret;
}

static long __vertex_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg, bool chk_compat)
{
	u64 now, temp;
	int ret = 0;
	int type = 0;
	struct vision_device *vdev = vision_devdata(file);
	const struct vertex_ioctl_ops *ops = vdev->ioctl_ops;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_profile *npu_profile = vctx->profile;

	/* temp var to support each ioctl */
	union {
		struct vs4l_graph vsg;
		struct vs4l_format_list vsf;
		struct vs4l_param_list vsp;
		struct vs4l_ctrl vsc;
		struct vs4l_sched_param vsprm;
		struct vs4l_freq_param vfprm;
		struct vs4l_container_list vscl;
		struct vs4l_profiler vspr;
		struct vs4l_version vsvs;
	} vs4l_kvar;

	type = cmd & 0xff;
	now = ktime_to_us(ktime_get_boottime());
	npu_log_ioctl_set_date(cmd, 0);
	switch (cmd) {
	case VS4L_VERTEXIOC_S_GRAPH:
		ret = get_vs4l_graph64(&vs4l_kvar.vsg,
				(struct vs4l_graph __user *)arg, chk_compat);
		if (ret) {
			vision_err("get_vs4l_graph64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_graph(file, &vs4l_kvar.vsg);
		if (ret)
			vision_err("vertexioc_s_graph is fail(%d)\n", ret);

		break;

	case VS4L_VERTEXIOC_S_FORMAT:
		ret = get_vs4l_format64(&vs4l_kvar.vsf,
					(struct vs4l_format_list __user *)arg,
					chk_compat);
		if (ret) {
			vision_err("get_vs4l_format64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_format(file, &vs4l_kvar.vsf);
		if (ret)
			vision_err("vertexioc_s_format (%d)\n", ret);

		put_vs4l_format64(&vs4l_kvar.vsf,
				(struct vs4l_format_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_PARAM:
		ret = get_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg,
				chk_compat);
		if (ret) {
			vision_err("get_vs4l_param64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_param(file, &vs4l_kvar.vsp);
		if (ret)
			vision_err("vertexioc_s_param (%d)\n", ret);

		put_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_CTRL:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		if (ret) {
			vision_err("get_vs4l_ctrl64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_ctrl(file, &vs4l_kvar.vsc);
		if (ret)
			vision_err("vertexioc_s_ctrl is fail(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;

	case VS4L_VERTEXIOC_STREAM_ON:
		ret = ops->vertexioc_streamon(file);
		if (ret)
			vision_err("vertexioc_streamon failed(%d)\n", ret);
		break;

	case VS4L_VERTEXIOC_BOOTUP:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		ret = ops->vertexioc_bootup(file, &vs4l_kvar.vsc);
		if (ret)
			vision_err("vertexioc_bootup failed(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;

	case VS4L_VERTEXIOC_STREAM_OFF:
		ret = ops->vertexioc_streamoff(file);
		if (ret)
			vision_err("vertexioc_streamoff failed(%d)\n", ret);
		break;

	case VS4L_VERTEXIOC_QBUF:
		npu_profile[type].iter++;
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		if (ret)
			break;
		if (npu_profile[PROFILE_ON].flag)
			vs4l_kvar.vscl.timestamp[5].tv_usec = npu_profile[PROFILE_ON].flag;

		ret = ops->vertexioc_qbuf(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_qbuf failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		break;

	case VS4L_VERTEXIOC_DQBUF:
		npu_profile[type].iter++;
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		if (ret)
			break;

		ret = ops->vertexioc_dqbuf(file, &vs4l_kvar.vscl);
		if (ret != 0 && ret != -EWOULDBLOCK)
			vision_err("vertexioc_dqbuf failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);

		break;
	case VS4L_VERTEXIOC_PREPARE:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		if (ret)
			break;

		ret = ops->vertexioc_prepare(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_prepare failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		break;
	case VS4L_VERTEXIOC_UNPREPARE:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		if (ret)
			break;

		ret = ops->vertexioc_unprepare(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_unprepare failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg,
				chk_compat);
		break;
	case VS4L_VERTEXIOC_SCHED_PARAM:
		ret = get_vs4l_sched_param64(&vs4l_kvar.vsprm,
				(struct vs4l_sched_param __user *)arg);
		if (ret)
			break;

		ret = ops->vertexioc_sched_param(file, &vs4l_kvar.vsprm);
		if (ret)
			vision_err("vertexioc_sched_param failed(%d)\n", ret);

		put_vs4l_sched_param64(&vs4l_kvar.vsprm,
				(struct vs4l_sched_param __user *)arg);
		break;
	case VS4L_VERTEXIOC_G_MAXFREQ:
		ret = ops->vertexioc_g_max_freq(file, &vs4l_kvar.vfprm);
		if (ret) {
			vision_err("vertexioc_freq_param failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_max_freq(&vs4l_kvar.vfprm,
					(struct vs4l_freq_param __user *)arg);
		if (ret) {
			vision_err("put_vs4l_max_freq failed(%d)\n", ret);
			break;
		}
		break;
	case VS4L_VERTEXIOC_PROFILE_ON:
		ret = get_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg, chk_compat);

		if (ret) {
			vision_err("get_vs4l_profiler failed(%d)\n", ret);
			break;
		}
		ret = ops->vertexioc_profileon(file, &vs4l_kvar.vspr);
		if (ret) {
			vision_err("vertexioc_profileon failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg, chk_compat);
		if (ret)
			vision_err("put_vs4l_profiler failed(%d)\n", ret);
		break;
	case VS4L_VERTEXIOC_PROFILE_OFF:
		ret = get_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg, chk_compat);
		if (ret) {
			vision_err("get_vs4l_profiler failed(%d)\n", ret);
			break;
		}
		ret = ops->vertexioc_profileoff(file, &vs4l_kvar.vspr);
		if (ret) {
			vision_err("vertexioc_profileoff failed(%d)\n", ret);
			break;
		}
		ret = put_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg, chk_compat);

		if (ret)
			vision_err("put_vs4l_profiler failed(%d)\n", ret);
		break;
	case VS4L_VERTEXIOC_VERSION:
		ret = get_vs4l_version(&vs4l_kvar.vsvs,
				(struct vs4l_version __user *)arg, chk_compat);
		if (ret) {
			vision_err("get_vs4l_version failed(%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_version(file, &vs4l_kvar.vsvs);
		if (ret) {
			vision_err("vertexioc_version failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_version(&vs4l_kvar.vsvs,
				(struct vs4l_version __user *)arg, chk_compat);
		if (ret)
			vision_err("put_vs4l_version failed(%d)\n", ret);

		break;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	case VS4L_VERTEXIOC_SYNC:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		ret = ops->vertexioc_sync(file, &vs4l_kvar.vsc);
		if (ret)
			vision_err("vertexioc_sync failed(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;
#endif
	default:
		vision_err("ioctl(%u) is not supported(usr arg: %lx)\n",
				cmd, arg);
		break;
	}
	temp = ktime_to_us(ktime_get_boottime()) - now;
	npu_profile[type].duration += temp;
	npu_profile[type].type = type;
	npu_log_ioctl_set_date(cmd, 1);
	return ret;
}

static long __vertex_ioctl_w_bundle(struct file *file, unsigned int cmd,
				    unsigned long arg, bool chk_compat)
{
	int i = 0;
	long ret = 0;
	union {
		struct vs4l_format_bundle fb;
		struct vs4l_container_bundle cb;
	} ka;

	switch (cmd) {
	case VS4L_VERTEXIOC_S_FORMAT_BUNDLE:
		ret = __copy_from_usr_vs4l(&ka.fb,
					   (void __user *)arg,
					   sizeof(ka.fb),
					   "vs4l_format_bundle");
		if (ret) break;

		for (i = 0; i < 2; i++) {
			if (unlikely(chk_compat))
				MASK_COMPAT_UPTR(ka.fb.m[i].flist);

			ret = __vertex_ioctl(file,
					     VS4L_VERTEXIOC_S_FORMAT,
					     (ulong) ka.fb.m[i].flist,
					     chk_compat);
			if (ret) {
				vision_err("s_format_bundle(%d) err(%ld)\n",
					   i, ret);
				break;
			}
		}
		break;
	case VS4L_VERTEXIOC_QBUF_BUNDLE:
		ret = __copy_from_usr_vs4l(&ka.cb,
					   (void __user *)arg,
					   sizeof(ka.cb),
					   "vs4l_container_bundle");
		if (ret) break;

		for (i = 0; i < 2; i++) {
			if (unlikely(chk_compat))
				MASK_COMPAT_UPTR(ka.cb.m[i].clist);

			ret = __vertex_ioctl(file,
					     VS4L_VERTEXIOC_QBUF,
					     (ulong) ka.cb.m[i].clist,
					     chk_compat);
			if (ret) {
				vision_err("qbuf_bundle(%d) err(%ld)\n",
					   i, ret);
				break;
			}
		}
		break;
	case VS4L_VERTEXIOC_DQBUF_BUNDLE:
		ret = __copy_from_usr_vs4l(&ka.cb,
					   (void __user *)arg,
					   sizeof(ka.cb),
					   "vs4l_container_bundle");
		if (ret) break;

		for (i = 0; i < 2; i++) {
			if (unlikely(chk_compat))
				MASK_COMPAT_UPTR(ka.cb.m[i].clist);

			ret = __vertex_ioctl(file,
					     VS4L_VERTEXIOC_DQBUF,
					     (ulong) ka.cb.m[i].clist,
					     chk_compat);
			if (ret) {
				vision_err("dqbuf_bundle(%d) err(%ld)\n",
					   i, ret);
				break;
			}
		}
		break;
	case VS4L_VERTEXIOC_PREPARE_BUNDLE:
		ret = __copy_from_usr_vs4l(&ka.cb,
					   (void __user *)arg,
					   sizeof(ka.cb),
					   "vs4l_container_bundle");
		if (ret) break;

		for (i = 0; i < 2; i++) {
			if (unlikely(chk_compat))
				MASK_COMPAT_UPTR(ka.cb.m[i].clist);

			ret = __vertex_ioctl(file,
					     VS4L_VERTEXIOC_PREPARE,
					     (ulong) ka.cb.m[i].clist,
					     chk_compat);
			if (ret) {
				vision_err("prepare_bundle(%d) err(%ld)\n",
					   i, ret);
				break;
			}
		}
		break;
	case VS4L_VERTEXIOC_UNPREPARE_BUNDLE:
		ret = __copy_from_usr_vs4l(&ka.cb,
					   (void __user *)arg,
					   sizeof(ka.cb),
					   "vs4l_container_bundle");
		if (ret) break;

		for (i = 0; i < 2; i++) {
			if (unlikely(chk_compat))
				MASK_COMPAT_UPTR(ka.cb.m[i].clist);

			ret = __vertex_ioctl(file,
					     VS4L_VERTEXIOC_UNPREPARE,
					     (ulong) ka.cb.m[i].clist,
					     chk_compat);
			if (ret) {
				vision_err("unprepare_bundle(%d) err(%ld)\n",
					   i, ret);
				break;
			}
		}
		break;
	default:
		ret = __vertex_ioctl(file, cmd, arg, chk_compat);
		break;
	}
	return ret;
}

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return __vertex_ioctl_w_bundle(file, cmd, arg, false);
}

#if IS_ENABLED(CONFIG_COMPAT)
long vertex_compat_ioctl32(struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	return __vertex_ioctl_w_bundle(file, cmd, arg, true);
}
#endif
