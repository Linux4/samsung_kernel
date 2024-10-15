// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "npu-preset.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-session.h"

#include "dsp-log.h"
#include "dsp-util.h"
#include "dsp-binary.h"
#include "dsp-kernel.h"

#include "dl/dsp-dl-engine.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-hash.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-xml-parser.h"
#include "dl/dsp-string-tree.h"
#include "dl/dsp-rule-reader.h"
#include "dl/dsp-lib-manager.h"

#define DSP_ELF_TAG_INFO_SIZE		(103)

int __dsp_kernel_alloc_load_user(struct device *dev, const char *name, void *source,
		size_t source_size, void **target, size_t *loaded_size)
{
	int ret;

	dsp_enter();

	if (!target) {
		ret = -EINVAL;
		dsp_err("dest address must be not NULL[%s]\n", name);
		goto p_err_target;
	}

	*target = vmalloc(source_size);
	if (!(*target)) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate target for binary[%s](%zu)\n",
				name, source_size);
		goto p_err_alloc;
	}

	memcpy(*target, source, source_size);
	if (loaded_size)
		*loaded_size = source_size;
	dsp_info("binary[%s/%zu] is loaded\n", name, source_size);
	dsp_leave();
	return 0;
p_err_alloc:
p_err_target:
	return ret;
}

static struct dsp_kernel *__dsp_kernel_alloc(struct device *dev,
		struct npu_session *session, struct dsp_kernel_manager *kmgr,
		unsigned int name_length, struct dsp_dl_lib_info *dl_lib, int idx)
{
	int ret = 0;
	struct dsp_kernel *new, *list, *temp;
	bool checked = false;
	char *tag_info = NULL;

	dsp_enter();
	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate kernel[%s]\n", dl_lib->name);
		goto p_err;
	}
	new->owner = kmgr;

	new->name_length = name_length + 4;
	new->name = kzalloc(new->name_length, GFP_KERNEL);
	if (!new->name) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate kernel_name[%s]\n", dl_lib->name);
		goto p_err_name;
	}
	snprintf(new->name, new->name_length, "%s.elf", dl_lib->name);

	mutex_lock(&kmgr->lock);
	new->id = dsp_util_bitmap_set_region(&kmgr->kernel_map, 1);
	if (new->id < 0) {
		mutex_unlock(&kmgr->lock);
		ret = new->id;
		dsp_err("Failed to allocate kernel bitmap(%d)\n", ret);
		goto p_err_bitmap;
	}

	list_for_each_entry_safe(list, temp, &kmgr->kernel_list, list) {
		if (list->name_length == new->name_length &&
				!strncmp(list->name, new->name,
					new->name_length)) {
			list->ref_count++;
			if (!checked) {
				new->ref_count = list->ref_count;
				new->elf = list->elf;
				new->elf_size = list->elf_size;
				checked = true;
			}
		}
	}

	if (!checked) {
		if (session->user_kernel_count) {
			if (session->user_kernel_count - 1 < idx) {
				ret = -EINVAL;
				mutex_unlock(&kmgr->lock);
				goto p_err_load;
			}
			ret = __dsp_kernel_alloc_load_user( dev, new->name, session->user_kernel_elf[idx],
				session->user_kernel_elf_size[idx], &new->elf, &new->elf_size);
			new->ref_count = 1;
		} else {
			new->ref_count = 1;
			ret = dsp_binary_alloc_load(dev, new->name, NULL, NULL,
						    &new->elf, &new->elf_size);
			if (ret) {
				mutex_unlock(&kmgr->lock);
				goto p_err_load;
			}
		}
		tag_info = (char *)new->elf + new->elf_size - DSP_ELF_TAG_INFO_SIZE;
	}

	list_add_tail(&new->list, &kmgr->kernel_list);
	kmgr->kernel_count++;
	mutex_unlock(&kmgr->lock);

	dl_lib->file.mem = new->elf;
	dl_lib->file.size = new->elf_size;

	dsp_info("loaded kernel name : [%s](%zu)\n", new->name, new->elf_size);
	if(tag_info){
		dsp_info("loaded kernel tag info : %s\n", tag_info);
	}
	dsp_leave();
	return new;
p_err_load:
p_err_bitmap:
	kfree(new->name);
p_err_name:
	kfree(new);
p_err:
	return ERR_PTR(ret);
}

static void __dsp_kernel_free(struct dsp_kernel *kernel)
{
	struct dsp_kernel_manager *kmgr;
	struct dsp_kernel *list, *temp;

	dsp_enter();
	kmgr = kernel->owner;

	mutex_lock(&kmgr->lock);
	if (kernel->ref_count == 1) {
		vfree(kernel->elf);
		goto free;
	}

	list_for_each_entry_safe(list, temp, &kmgr->kernel_list, list) {
		if (list->name_length == kernel->name_length &&
				!strncmp(list->name, kernel->name,
					list->name_length))
			list->ref_count--;
	}

free:
	kmgr->kernel_count--;
	list_del(&kernel->list);
	dsp_util_bitmap_clear_region(&kmgr->kernel_map, kernel->id, 1);
	mutex_unlock(&kmgr->lock);

	kfree(kernel->name);
	kfree(kernel);
	dsp_leave();
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_kernel_dump(struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	if (kmgr->dl_init)
		dsp_dl_print_status();
	dsp_leave();
}
#endif

static int __dsp_kernel_load(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count)
{
	int ret;
	struct dsp_dl_load_status dl_ret;

	dsp_enter();

	if (kmgr->dl_init) {
		mutex_lock(&kmgr->lock);
		dl_ret = dsp_dl_load_libraries(dl_libs, kernel_count);
		mutex_unlock(&kmgr->lock);
		if (dl_ret.status) {
			ret = dl_ret.status;
			dsp_err("Failed to load kernel(%u/%d)\n",
					kernel_count, ret);
			dsp_kernel_dump(kmgr);
			goto p_err;
		}
	} else {
		ret = -EINVAL;
		dsp_err("Failed to load kernel as DL is not initilized\n");
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_kernel_unload(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count, bool is_user_kernel)
{
	int ret;

	dsp_enter();
	if (kmgr->dl_init) {
		mutex_lock(&kmgr->lock);
		ret = dsp_dl_unload_libraries(dl_libs, kernel_count, is_user_kernel);
		mutex_unlock(&kmgr->lock);
		if (ret) {
			dsp_err("Failed to unload kernel(%u/%d)\n",
					kernel_count, ret);
			goto p_err;
		}
	} else {
		ret = -EINVAL;
		dsp_err("Failed to unload kernel as DL is not initilized\n");
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_graph_remove_kernel(struct npu_device *device, struct npu_session *session)
{
	struct dsp_kernel_manager *kmgr;
	struct dsp_kernel *kernel, *t;
	bool is_user_kernel = (session->str_manager.count > 0);

	dsp_enter();
	kmgr = &device->kmgr;

	if (session->kernel_loaded) {
		__dsp_kernel_unload(kmgr, session->dl_libs, session->kernel_count, is_user_kernel);
		dsp_lib_manager_delete_remain_xml(&session->str_manager);
		kfree(session->dl_libs);
		session->dl_libs = NULL;
	} else {
		kfree(session->dl_libs);
		session->dl_libs = NULL;
	}

	if (session->kernel_name) {
		kfree(session->kernel_name);
		session->kernel_name = NULL;
	}

	list_for_each_entry_safe(kernel, t, &session->kernel_list, graph_list) {
		list_del(&kernel->graph_list);
		__dsp_kernel_free(kernel);
	}
	dsp_leave();
}

char* append_string_number_suffix(const char *str, unsigned int length, unsigned int suffix)
{
	char temp[256] = { 0, };
	char *result;

	strncpy(temp, str, length);
	result = kzalloc(length + 12, GFP_KERNEL);
	sprintf(result, "%s_%u", temp, suffix);

	return result;
}

int dsp_graph_add_kernel(struct npu_device *device,
        struct npu_session *session, void *kernel_name, unsigned int kernel_size, unsigned int unique_id)
{
	int ret;
	struct dsp_kernel_manager *kmgr;
	unsigned int kernel_count;
	unsigned int *length;
	unsigned long offset;
	int idx;
	struct dsp_kernel *kernel;

	dsp_enter();
	kmgr = &device->kmgr;
	kernel_count = session->kernel_count;
	length = kernel_name;
	offset = (unsigned long)&length[kernel_count];
	mutex_lock(session->global_lock);

	session->dl_libs = kcalloc(kernel_count, sizeof(*session->dl_libs),
			GFP_KERNEL);
	if (!session->dl_libs) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dl_libs(%u)\n", kernel_count);
		goto p_err;
	}

	for (idx = 0; idx < kernel_count; ++idx) {
		if (offset > (unsigned long)((char*)kernel_name + kernel_size)) {
			ret = -EINVAL;
			dsp_err("Incorrect offset greater than buffer size (%lu vs %u)", offset - (unsigned long)kernel_name, kernel_size);
			goto p_err_alloc;
		}
		if (unique_id != 0) {
			session->dl_libs[idx].name =
				append_string_number_suffix((const char *)offset,
						length[idx], unique_id);
		} else {
			session->dl_libs[idx].name = (const char *)offset;
		}
		dsp_dbg("Convert lib name[%s]\n", session->dl_libs[idx].name);
		kernel = __dsp_kernel_alloc(device->dev, session, kmgr, length[idx] + 12,
				&session->dl_libs[idx], idx);
		if (IS_ERR(kernel)) {
			ret = PTR_ERR(kernel);
			dsp_err("Failed to alloc kernel(%u/%u)\n",
					idx, kernel_count);
			goto p_err_alloc;
		}

		list_add_tail(&kernel->graph_list, &session->kernel_list);
		offset += length[idx];
	}

	ret = __dsp_kernel_load(kmgr, session->dl_libs, kernel_count);
	if (ret)
		goto p_err_load;

	session->kernel_loaded = true;
	dsp_leave();
	mutex_unlock(session->global_lock);
	return 0;
p_err_load:
p_err_alloc:
	dsp_graph_remove_kernel(device, session);
p_err:
	mutex_unlock(session->global_lock);
	return ret;
}

npu_s_param_ret dsp_kernel_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	void *kernel_name;
	npu_s_param_ret ret = S_PARAM_HANDLED;

	BUG_ON(!sess);
	BUG_ON(!param);

	vctx = &(sess->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	switch (param->target) {
	case NPU_S_PARAM_DSP_KERNEL:
		sess->kernel_count = param->offset;
		sess->kernel_param_size = param->size;
		if (sess->kernel_count > param->size) {
			ret = S_PARAM_ERROR;
			dsp_err("Failed to ensure allowed size\n");
			break;
		}

		kernel_name = kmalloc(param->size + 1, GFP_KERNEL);
		if (!kernel_name) {
			ret = S_PARAM_ERROR;
			dsp_err("Failed to alloc kernel\n");
			break;
		}
		((char*)kernel_name)[param->size] = '\0';
		if (copy_from_user(kernel_name, (void __user *)param->addr, param->size)) {
			ret = S_PARAM_ERROR;
			kfree(kernel_name);
			dsp_err("Failed to copy from user kernel_name\n");
			break;
		}
		ret = dsp_graph_add_kernel(device, sess, kernel_name, param->size, 0);
		if (ret) {
			ret = S_PARAM_ERROR;
			kfree(kernel_name);
			dsp_err("Failed to add kernel\n");
			break;
		}
		sess->kernel_name = kernel_name;
		ret = S_PARAM_HANDLED;
		break;

	case NPU_S_PARAM_DSP_USER_KERNEL:
		sess->kernel_count = param->offset;
		sess->kernel_param_size = param->size;

		kernel_name = kmalloc(param->size, GFP_KERNEL);
		if (!kernel_name) {
			ret = S_PARAM_ERROR;
			dsp_err("Failed to alloc kernel\n");
			break;
		}
		if (copy_from_user(kernel_name, (void __user *)param->addr, param->size)) {
			ret = S_PARAM_ERROR;
			kfree(kernel_name);
			dsp_err("Failed to copy from user kernel_name\n");
			break;
		}
		sess->kernel_name = kernel_name;
		ret = S_PARAM_HANDLED;
		break;

	default:
		ret = S_PARAM_NOMB;
		break;
	}

	return ret;
}

int dsp_kernel_manager_open(struct npu_system *system, struct dsp_kernel_manager *kmgr)
{
	int ret;

	BUG_ON(!system);

	dsp_enter();
	mutex_lock(&kmgr->lock);
	if (kmgr->dl_init) {
		if (kmgr->dl_init + 1 < kmgr->dl_init) {
			ret = -EINVAL;
			dsp_err("dl init count is overflowed\n");
			goto p_err;
		}

		kmgr->dl_init++;
	} else {
		ret = dsp_kernel_manager_dl_init(&system->pdev->dev, &kmgr->dl_param,
							npu_get_mem_area(system, "ivp_pm"), npu_get_mem_area(system, "dl_out"));
		if (ret)
			goto p_err;

		kmgr->dl_init = 1;
	}
	mutex_unlock(&kmgr->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&kmgr->lock);
	return ret;
}

void dsp_kernel_manager_close(struct dsp_kernel_manager *kmgr,
		unsigned int count)
{
	dsp_enter();
	mutex_lock(&kmgr->lock);
	if (!kmgr->dl_init) {
		dsp_warn("dl was not initilized");
		mutex_unlock(&kmgr->lock);
		return;
	}

	if (kmgr->dl_init > count) {
		kmgr->dl_init -= count;
		mutex_unlock(&kmgr->lock);
		return;
	}

	if (kmgr->dl_init < count)
		dsp_warn("dl_init is unstable(%u/%u)", kmgr->dl_init, count);

	dsp_kernel_manager_dl_deinit(&kmgr->dl_param);
	kmgr->dl_init = 0;
	mutex_unlock(&kmgr->lock);
	dsp_leave();
}

int dsp_kernel_manager_probe(struct npu_device *device)
{
	int ret;
	struct dsp_kernel_manager *kmgr;

	dsp_enter();
	kmgr = &device->kmgr;
	kmgr->device = device;

	INIT_LIST_HEAD(&kmgr->kernel_list);

	mutex_init(&kmgr->lock);
	ret = dsp_util_bitmap_init(&kmgr->kernel_map, "kernel_bitmap",
			DSP_KERNEL_MAX_COUNT);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_kernel_manager_remove(struct dsp_kernel_manager *kmgr)
{
	struct dsp_kernel *kernel, *temp;

	dsp_enter();
	list_for_each_entry_safe(kernel, temp, &kmgr->kernel_list, list) {
		dsp_warn("kernel[%u] is destroyed(count:%u)\n",
				kernel->id, kmgr->kernel_count);
		__dsp_kernel_free(kernel);
	}
	dsp_util_bitmap_deinit(&kmgr->kernel_map);
	dsp_leave();
}
