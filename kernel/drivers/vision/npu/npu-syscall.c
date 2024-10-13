/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
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

#include "npu-syscall.h"
#include "npu-vertex.h"
#include "npu-log.h"
#include "npu-profile-v2.h"

static struct vision_device *vision_device[VISION_NUM_DEVICES];

static unsigned long __npu_access_ok(const void __user *addr, unsigned long size)
{
	return access_ok(addr, size);
}

#define MASK_COMPAT_LONG(u) ((u) &= 0xFFFFFFFFUL & (u))
#define MASK_COMPAT_UPTR(p) (p = ((void *) (0xFFFFFFFFUL & ((ulong) (p)))))


static inline int __copy_from_usr_vs4l(void *kp, void __user *up, size_t len, const char *msg)
{
	int ret = 0;

	if (!__npu_access_ok(up, len)) {
		npu_err("%s: access_ok fail uptr(%pK) size(%zu)\n",
			   msg, up, len);
		return -EFAULT;
	}

	ret = copy_from_user(kp, up, len);
	if (ret) {
		npu_err("%s: copy_from_user fail ret(%d) size(%zu)\n",
			   msg, ret, len);
		return -EFAULT;
	}
	return 0;
}

static inline void* copy_from_usr_vs4l(void __user *up, size_t len, const char *msg)
{
	int ret = 0;
	void *kp = NULL;

	kp = kzalloc(len, GFP_KERNEL);
	if (!kp) {
		npu_err("%s: kzalloc fail\n", msg);
		return ERR_PTR(-ENOMEM);
	}

	ret = __copy_from_usr_vs4l(kp, up, len, msg);
	if (ret) {
		kfree(kp);
		return ERR_PTR(ret);
	}
	return kp;
}

static int get_vs4l_ctrl64(struct vs4l_ctrl *kp, struct vs4l_ctrl __user *up)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_ctrl))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_ctrl;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_ctrl));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
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
		npu_err("Failed(%d) to copy to user at ctrl.\n", ret);
}


static int get_vs4l_graph64(struct vs4l_graph *kp,
			    struct vs4l_graph __user *up, bool chk_compat)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_graph))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_graph;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_graph));

	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_graph;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_LONG(kp->addr);

p_err_graph:
	return ret;
}

static void put_vs4l_graph64(struct vs4l_graph *kp,
			    struct vs4l_graph __user *up, bool chk_compat)
{
	int ret;

	ret = copy_to_user(up, kp, sizeof(struct vs4l_graph));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
	}
}

static int get_vs4l_sched_param64(struct vs4l_sched_param *kp, struct vs4l_sched_param __user *up)
{
	int ret;

	if (!__npu_access_ok(up, sizeof(struct vs4l_sched_param))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_sched_param));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

p_err:
	return ret;
}

static int put_vs4l_max_freq(struct vs4l_freq_param *kp,
			     struct vs4l_freq_param __user *up)
{
	int ret = 0;
	if (!__npu_access_ok(up, sizeof(struct vs4l_freq_param))) {
		npu_err("Cannot access to user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_freq;
	}

	if (copy_to_user((void *)up, (void *)kp, sizeof(struct vs4l_freq_param))) {
		npu_err("Copy_to_user failed (%pK -> %pK)\n", kp, up);
		ret = -EFAULT;
		goto p_err_freq;
	}

	return ret;

p_err_freq:
	npu_err("return with fail... (%d)\n", ret);
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
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_format;
	}

	ret = copy_from_user((void *)kp, (void __user *)up,
				sizeof(struct vs4l_format_list));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_format;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->formats);

	if (kp->count > NPU_FRAME_MAX_CONTAINERLIST) {
		npu_err("kp->count(%u) cannot be greater to NPU_FRAME_MAX_CONTAINERLIST(%d)\n", kp->count, NPU_FRAME_MAX_CONTAINERLIST);
		ret = -EINVAL;
		goto p_err_format;
	}

	size = kp->count * sizeof(struct vs4l_format);
	if (!__npu_access_ok((void __user *)kp->formats, size)) {
		npu_err("acesss is fail\n");
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
		npu_err("copy_from_user failed(%d)\n", ret);
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

static int get_vs4l_format_bundle(struct vs4l_format_bundle *kp,
			     struct vs4l_format_bundle __user *up, bool chk_compat)
{
	int ret = 0, i = 0;
	struct vs4l_format_bundle cf;

	ret = __copy_from_usr_vs4l(&cf, (void __user *)up,
				sizeof(struct vs4l_format_bundle), "vs4l_format_bundle");
	if (ret)
		return ret;

	for (i = 0; i < 2; i++) {
		kp->m[i].flist = kzalloc(sizeof(struct vs4l_format_list), GFP_KERNEL);
		if (!kp->m[i].flist)
			return -ENOMEM;
	}

	for (i = 0; i < 2; i++) {
		if (unlikely(chk_compat))
			MASK_COMPAT_UPTR(cf.m[i].flist);
		ret = get_vs4l_format64(kp->m[i].flist, cf.m[i].flist, chk_compat);
		if (ret)
			break;
	}

	return ret;
}

static void put_vs4l_format_bundle(struct vs4l_format_bundle *kp,
			     struct vs4l_format_bundle __user *up)
{
	int i = 0;
	struct vs4l_format_bundle cf;

	__copy_from_usr_vs4l(&cf, (void __user *)up,
				sizeof(struct vs4l_format_bundle), "vs4l_format_bundle");

	for (i = 0; i < 2; i++)
		put_vs4l_format64(kp->m[i].flist, cf.m[i].flist);

	for (i = 0; i < 2; i++) {
		if (kp->m[i].flist)
			kfree(kp->m[i].flist);
	}
}

static int get_vs4l_param64(struct vs4l_param_list *kp,
			    struct vs4l_param_list __user *up, bool chk_compat)
{
	int i, ret;
	size_t size = 0;
	struct vs4l_param *kparams_ptr;

	if (!__npu_access_ok(up, sizeof(struct vs4l_param_list))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_param;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_param_list));

	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_param;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->params);

	size = kp->count * sizeof(struct vs4l_param);
	if (!__npu_access_ok((void __user *)kp->params, size)) {
		npu_err("access failed\n");
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
		npu_err("copy_from_user failed(%d)\n", ret);
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

static int get_vs4l_profile_ready(struct vs4l_profile_ready *kp,
			     struct vs4l_profile_ready __user *up)
{
	int ret = 0;

	if (!__npu_access_ok(up, sizeof(struct vs4l_profile_ready))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_ctrl;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_profile_ready));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_ctrl;
	}

p_err_ctrl:
	return ret;
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
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		return ret;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->containers);

	/* container_list -> (vs4l_container)containers[count] -> (vs4l_buffer)buffers[count] */
	if (kp->count > NPU_FRAME_MAX_CONTAINERLIST) {
		npu_err("kp->count(%u) cannot be greater to NPU_FRAME_MAX_CONTAINERLIST(%d)\n", kp->count, NPU_FRAME_MAX_CONTAINERLIST);
		ret = -EINVAL;
		return ret;
	}

	if (kp->index >= NPU_MAX_QUEUE) {
		npu_err("kp->index(%u) cannot be greater to NPU_MAX_QUEUE(%d)\n", kp->index, NPU_MAX_QUEUE);
		ret = -EINVAL;
		return ret;
	}

	size = kp->count * sizeof(struct vs4l_container);
	if (!__npu_access_ok((void __user *)kp->containers, size)) {
		npu_err("access to containers ptr failed (%pK)\n",
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
		npu_err("error from copy_from_user(%d), size(%zu)\n", ret, size);
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

		if (kp->containers[i].count > NPU_FRAME_MAX_BUFFER) {
			npu_err("kp->count(%u) cannot be greater to NPU_FRAME_MAX_BUFFER(%d)\n", kp->containers[i].count, NPU_FRAME_MAX_BUFFER);
			ret = -EINVAL;
			goto p_err_buffer;
		}

		if (!__npu_access_ok((void __user *)
					kp->containers[i].buffers, size)) {
			npu_err("access to containers ptr failed (%pK)\n",
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
			npu_err("error from copy_from_user(idx: %d, ret: %d, size: %zu)\n",
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
		npu_err("access failed from user ptr(%pK)\n", up);
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
	npu_err("Return with fail... (%d)\n", ret);
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
		npu_err("Cannot access to user ptr(%pK)\n", up);

	ret = get_vs4l_containers(&temp, up, chk_compat);
	if (ret)
		goto p_err;

	for (i = 0; i < kp->count; i++) {
		ksize = kp->containers[i].count * sizeof(struct vs4l_buffer);
		ret = copy_to_user((void *)temp.containers[i].buffers, (void *)kp->containers[i].buffers, ksize);
		if (ret) {
			npu_err("Failed(%d) to copy to user at version.\n", ret);
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
		npu_err("Copy_to_user failed (%pK -> %pK)\n", kp, up);
	}

	for (i = 0; i < kp->count; ++i)
		kfree(kp->containers[i].buffers);

	kfree(kp->containers);
	kfree(temp.containers);
	kp->containers = NULL;
	temp.containers = NULL;

	return 0;

p_err:
	npu_err("Return with fail... (%d)\n", ret);
	return ret;
}

static int get_vs4l_container_bundle(struct vs4l_container_bundle *kp,
				struct vs4l_container_bundle __user *up,
				bool chk_compat)
{
	int ret = 0, i = 0;
	struct vs4l_container_bundle cb;

	ret = __copy_from_usr_vs4l(&cb, (void __user *)up,
				sizeof(struct vs4l_container_bundle), "vs4l_container_bundle");
	if (ret)
		return ret;

	for (i = 0; i < 2; i++) {
		kp->m[i].clist = kzalloc(sizeof(struct vs4l_container_list), GFP_KERNEL);
		if (!kp->m[i].clist)
			return -ENOMEM;
	}

	for (i = 0; i < 2; i++) {
		if (unlikely(chk_compat))
			MASK_COMPAT_UPTR(cb.m[i].clist);
		ret = get_vs4l_container64(kp->m[i].clist, cb.m[i].clist, chk_compat);
		if (ret)
			break;
	}
	return ret;
}

void put_vs4l_container_bundle(struct vs4l_container_bundle *kp,
				struct vs4l_container_bundle __user *up,
				bool chk_compat)
{
	int i = 0;
	struct vs4l_container_bundle cb;

	__copy_from_usr_vs4l(&cb, (void __user *)up,
				sizeof(struct vs4l_container_bundle), "vs4l_container_bundle");

	for (i = 0; i < 2; i++)
		put_vs4l_container64(kp->m[i].clist, cb.m[i].clist, chk_compat);

	for (i = 0; i < 2; i++) {
		if (kp->m[i].clist)
			kfree(kp->m[i].clist);
	}
}

static int get_vs4l_version(struct vs4l_version *kp,
			    struct vs4l_version __user *up, bool chk_compat)
{
	int ret = 0;
	char *fw_version;
	char *fw_hash;

	if (!__npu_access_ok(up, sizeof(*kp))) {
		npu_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_version;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(*kp));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_version;
	}

	fw_version = kzalloc(NPU_MAX_FW_VERSION_LEN, GFP_KERNEL);
	if (!fw_version) {
		ret = -ENOMEM;
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->fw_version);
	ret = copy_from_user(fw_version, (void __user *)kp->fw_version,
			     NPU_MAX_FW_VERSION_LEN);
	if (ret) {
		npu_err("error copy_from_user(%d) about fw version\n", ret);
		ret = -EFAULT;
		kfree(fw_version);
		goto p_err_version;
	}
	kp->fw_version = fw_version;

	fw_hash = kzalloc(NPU_MAX_FW_HASH_LEN, GFP_KERNEL);
	if (!fw_hash) {
		ret = -ENOMEM;
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(kp->fw_hash);
	ret = copy_from_user(fw_hash, (void __user *)kp->fw_hash,
			     NPU_MAX_FW_HASH_LEN);
	if (ret) {
		npu_err("error copy_from_user(%d) about fw hash\n", ret);
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
		npu_err("Cannot access to user ptr(%pK)\n", up);

	ret = copy_from_user(&temp, (void __user *)up, sizeof(*kp));
	if (ret) {
		npu_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.build_info);
	ret = copy_to_user(temp.build_info, kp->build_info,
			   strlen(kp->build_info) + 1);
	if (ret) {
		npu_err("Failed(%d) to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.signature);
	ret = copy_to_user(temp.signature, kp->signature,
			   strlen(kp->signature) + 1);
	if (ret) {
		npu_err("Failed(%d) to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.driver_version);
	ret = copy_to_user(temp.driver_version, kp->driver_version,
			   strlen(kp->driver_version) + 1);
	if (ret) {
		npu_err("Failed(%d) to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.fw_version);
	ret = copy_to_user(temp.fw_version, kp->fw_version,
			   strlen(kp->fw_version));
	if (ret) {
		npu_err("Failed(%d) to copy to user at version.\n", ret);
		goto p_err_version;
	}

	if (unlikely(chk_compat)) MASK_COMPAT_UPTR(temp.fw_hash);
	ret = copy_to_user(temp.fw_hash, kp->fw_hash, strlen(kp->fw_hash));
	if (ret) {
		npu_err("Failed(%d) to copy to user at version.\n", ret);
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
	int ret = 0;
	int type = 0;

	/* temp var to support each ioctl */
	union {
		struct vs4l_graph vsg;
		struct vs4l_param_list vsp;
		struct vs4l_ctrl vsc;
		struct vs4l_sched_param vsprm;
		struct vs4l_freq_param vfprm;
		struct vs4l_container_bundle vscb;
		struct vs4l_format_bundle vsfb;
		struct vs4l_version vsvs;
		struct vs4l_profile_ready vspr;
	} vs4l_kvar;

	type = cmd & 0xff;
	npu_log_ioctl_set_date(cmd, 0);
	switch (cmd) {
	case VS4L_VERTEXIOC_S_GRAPH:
		ret = get_vs4l_graph64(&vs4l_kvar.vsg,
				(struct vs4l_graph __user *)arg, chk_compat);
		if (ret) {
			npu_err("get_vs4l_graph64 (%d)\n", ret);
			break;
		}

		ret = npu_vertex_s_graph(file, &vs4l_kvar.vsg);
		if (ret)
			npu_err("vertexioc_s_graph is fail(%d)\n", ret);

		put_vs4l_graph64(&vs4l_kvar.vsg,
				(struct vs4l_graph __user *)arg, chk_compat);

		break;

	case VS4L_VERTEXIOC_S_FORMAT_BUNDLE:
		ret = get_vs4l_format_bundle(&vs4l_kvar.vsfb,
					(struct vs4l_format_bundle __user *)arg,
					chk_compat);
		if (ret) {
			npu_err("get_vs4l_format_bundle (%d)\n", ret);
			break;
		}

		ret = npu_vertex_s_format(file, &vs4l_kvar.vsfb);
		if (ret)
			npu_err("vertexioc_s_format (%d)\n", ret);

		put_vs4l_format_bundle(&vs4l_kvar.vsfb,
				(struct vs4l_format_bundle __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_PARAM:
		ret = get_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg,
				chk_compat);
		if (ret) {
			npu_err("get_vs4l_param64 (%d)\n", ret);
			break;
		}

		ret = npu_vertex_s_param(file, &vs4l_kvar.vsp);
		if (ret)
			npu_err("vertexioc_s_param (%d)\n", ret);

		put_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_CTRL:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		if (ret) {
			npu_err("get_vs4l_ctrl64 (%d)\n", ret);
			break;
		}

		ret = npu_vertex_s_ctrl(file, &vs4l_kvar.vsc);
		if (ret)
			npu_err("vertexioc_s_ctrl is fail(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;

	case VS4L_VERTEXIOC_STREAM_ON:
		break;

	case VS4L_VERTEXIOC_BOOTUP:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		ret = npu_vertex_bootup(file, &vs4l_kvar.vsc);
		if (ret)
			npu_err("vertexioc_bootup failed(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;

	case VS4L_VERTEXIOC_STREAM_OFF:
		ret = npu_vertex_streamoff(file);
		if (ret)
			npu_err("vertexioc_streamoff failed(%d)\n", ret);
		break;

	case VS4L_VERTEXIOC_QBUF_BUNDLE:
		ret = get_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		if (ret)
			break;

		ret = npu_vertex_qbuf(file, &vs4l_kvar.vscb);
		if (ret)
			npu_err("vertexioc_qbuf failed(%d)\n", ret);

		put_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		break;

	case VS4L_VERTEXIOC_DQBUF_BUNDLE:
		ret = get_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		if (ret)
			break;

		ret = npu_vertex_dqbuf(file, &vs4l_kvar.vscb);
		if (ret != 0 && ret != -EWOULDBLOCK)
			npu_err("vertexioc_dqbuf failed(%d)\n", ret);

		put_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		break;

	case VS4L_VERTEXIOC_PREPARE_BUNDLE:
		ret = get_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		if (ret)
			break;

		ret = npu_vertex_prepare(file, &vs4l_kvar.vscb);
		if (ret)
			npu_err("vertexioc_prepare failed(%d)\n", ret);

		put_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		break;
	case VS4L_VERTEXIOC_UNPREPARE_BUNDLE:
		ret = get_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		if (ret)
			break;

		ret = npu_vertex_unprepare(file, &vs4l_kvar.vscb);
		if (ret)
			npu_err("vertexioc_unprepare failed(%d)\n", ret);

		put_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		break;
	case VS4L_VERTEXIOC_SCHED_PARAM:
		ret = get_vs4l_sched_param64(&vs4l_kvar.vsprm,
				(struct vs4l_sched_param __user *)arg);
		if (ret)
			break;

		ret = npu_vertex_sched_param(file, &vs4l_kvar.vsprm);
		if (ret)
			npu_err("vertexioc_sched_param failed(%d)\n", ret);

		break;
	case VS4L_VERTEXIOC_G_MAXFREQ:
		ret = npu_vertex_g_max_freq(file, &vs4l_kvar.vfprm);
		if (ret) {
			npu_err("vertexioc_freq_param failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_max_freq(&vs4l_kvar.vfprm,
					(struct vs4l_freq_param __user *)arg);
		if (ret) {
			npu_err("put_vs4l_max_freq failed(%d)\n", ret);
			break;
		}
		break;

	case VS4L_VERTEXIOC_PROFILE_READY:
		ret = get_vs4l_profile_ready(&vs4l_kvar.vspr,
				(struct vs4l_profile_ready __user *)arg);
		if (ret)
			break;

		ret = npu_vertex_profile_ready(file, &vs4l_kvar.vspr);
		if (ret)
			npu_err("vertexioc_profile_ready failed(%d)\n", ret);

		break;
	case VS4L_VERTEXIOC_PROFILE_ON:
		break;
	case VS4L_VERTEXIOC_PROFILE_OFF:
		break;
	case VS4L_VERTEXIOC_VERSION:
		ret = get_vs4l_version(&vs4l_kvar.vsvs,
				(struct vs4l_version __user *)arg, chk_compat);
		if (ret) {
			npu_err("get_vs4l_version failed(%d)\n", ret);
			break;
		}

		ret = npu_vertex_version(file, &vs4l_kvar.vsvs);
		if (ret) {
			npu_err("vertexioc_version failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_version(&vs4l_kvar.vsvs,
				(struct vs4l_version __user *)arg, chk_compat);
		if (ret)
			npu_err("put_vs4l_version failed(%d)\n", ret);

		break;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	case VS4L_VERTEXIOC_SYNC:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		ret = npu_vertex_sync(file, &vs4l_kvar.vsc);
		if (ret)
			npu_err("vertexioc_sync failed(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;
#endif
	case VS4L_VERTEXIOC_QBUF_BUNDLE_CANCEL:
		ret = get_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		if (ret)
			break;

		ret = npu_vertex_qbuf_cancel(file, &vs4l_kvar.vscb);
		if (ret)
			npu_err("npu_vertex_qbuf_cancel failed(%d)\n", ret);

		put_vs4l_container_bundle(&vs4l_kvar.vscb,
				(struct vs4l_container_bundle __user *)arg, chk_compat);
		break;
	default:
		npu_err("ioctl(%u) is not supported(usr arg: %lx)\n",
				cmd, arg);
		ret = -EINVAL;
		break;
	}
	npu_log_ioctl_set_date(cmd, 1);
	return ret;
}

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return __vertex_ioctl(file, cmd, arg, false);
}

#if IS_ENABLED(CONFIG_COMPAT)
long vertex_compat_ioctl32(struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	return __vertex_ioctl(file, cmd, arg, true);
}
#endif

static int vision_flush(struct file *file, fl_owner_t id)
{
	return npu_vertex_flush(file);
}

struct vision_device *vision_devdata(struct file *file)
{
	unsigned int minor = 0xFF & iminor(file_inode(file));
	return vision_device[minor];
}

static int vision_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct vision_device *vdev;

	vdev = vision_devdata(file);
	if (!vdev) {
		npu_err("vision device is NULL\n");
		ret = -ENODEV;
		goto p_err;
	}

	get_device(&vdev->dev);

	ret = npu_vertex_open(file);
	if (ret) {
		npu_err("npu_vertex_open ERROR (%d)\n", ret);
		put_device(&vdev->dev);
		goto p_err;
	}

p_err:
	return ret;
}

static int vision_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct vision_device *vdev = vision_devdata(file);

	ret = npu_vertex_close(file);
	if (ret)
		npu_err("npu_vertex_close ERROR (%d)\n", ret);

	put_device(&vdev->dev);

	return ret;
}

static long vision_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct vision_device *vdev = vision_devdata(file);
	struct mutex *lock = vdev->lock;

	if (lock && mutex_lock_interruptible(lock))
		return -ERESTARTSYS;

	ret = vertex_ioctl(file, cmd, arg);
	if (ret)
		npu_err("vertex_ioctl ERROR (%d)\n", ret);

	if (lock)
		mutex_unlock(lock);

	return ret;
}

static long vision_compat_ioctl32(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct vision_device *vdev = vision_devdata(file);
	struct mutex *lock = vdev->lock;

	if (lock && mutex_lock_interruptible(lock))
		return -ERESTARTSYS;

	ret = vertex_compat_ioctl32(file, cmd, arg);
	if (ret)
		npu_err("vertex_compat_ioctl32 ERROR (%d)\n", ret);

	if (lock)
		mutex_unlock(lock);

	return ret;
}

static unsigned int vision_poll(struct file *file, struct poll_table_struct *poll)
{
	return npu_vertex_poll(file, poll);
}

static int vision_mmap(struct file *file, struct vm_area_struct *vm_area)
{
	int ret= 0;
	struct vision_device *vdev = vision_devdata(file);
	struct mutex *lock = vdev->lock;

	if (lock && mutex_lock_interruptible(lock))
		return -ERESTARTSYS;

	ret = npu_vertex_mmap(file, vm_area);
	if (ret)
		npu_err("npu_vertex_mmap ERROR (%d)\n", ret);

	if (lock)
		mutex_unlock(lock);

	return ret;
}

static const struct file_operations vision_fops = {
	.owner = THIS_MODULE,
	.open = vision_open,
	.release = vision_release,
	.unlocked_ioctl = vision_ioctl,
	.compat_ioctl = vision_compat_ioctl32,
	.poll = vision_poll,
	.llseek = no_llseek,
	.flush = vision_flush,
	.mmap = vision_mmap,
};

struct class vision_class = {
	.name = VISION_NAME,
};

static void vision_device_release(struct device *dev)
{
	struct vision_device *vdev = container_of(dev, struct vision_device, dev);

	if (WARN_ON(vision_device[vdev->minor] != vdev)) {
		return;
	}

	vision_device[vdev->minor] = NULL;
	cdev_del(vdev->cdev);
	vdev->cdev = NULL;

	vdev->release(vdev);
}

int vision_register_device(struct vision_device *vdev, int minor, struct module *owner)
{
	int ret = 0;
	const char *name_base;

	BUG_ON(!vdev->parent);
	WARN_ON(vision_device[minor] != NULL);

	vdev->cdev = NULL;
	name_base = "vertex";

	/* Part 3: Initialize the character device */
	vdev->cdev = cdev_alloc();
	if (vdev->cdev == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}
	vdev->cdev->ops = &vision_fops;
	vdev->cdev->owner = THIS_MODULE;
	ret = cdev_add(vdev->cdev, MKDEV(VISION_MAJOR, minor), 1);
	if (ret < 0) {
		probe_err("cdev_add failed (%d)\n", ret);
		kfree(vdev->cdev);
		vdev->cdev = NULL;
		goto cleanup;
	}

	/* Part 4: register the device with sysfs */
	vdev->dev.class = &vision_class;
	vdev->dev.parent = vdev->parent;
	vdev->dev.devt = MKDEV(VISION_MAJOR, minor);
	dev_set_name(&vdev->dev, "%s%d", name_base, minor);
	ret = device_register(&vdev->dev);
	if (ret < 0) {
		probe_err("device_register failed (%d)\n", ret);
		goto cleanup;
	}
	/* Register the release callback that will be called when the last
	 * reference to the device goes away.
	 */
	vdev->dev.release = vision_device_release;

	vdev->minor = minor;
	vision_device[minor] = vdev;

	return 0;

cleanup:
	if (vdev->cdev)
		cdev_del(vdev->cdev);
	/* Mark this video device as never having been registered. */
	vdev->minor = -1;
	return ret;
}
EXPORT_SYMBOL(vision_register_device);
