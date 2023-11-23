// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/vmalloc.h>

#include "dsp-log.h"
#include "dsp-util.h"
#include "dsp-binary.h"
#include "dsp-device.h"
#include "dsp-graph.h"
#include "dsp-kernel.h"
#include "hardware/dsp-mailbox.h"

#include "dl/dsp-dl-engine.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-hash.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-xml-parser.h"
#include "dl/dsp-string-tree.h"
#include "dl/dsp-rule-reader.h"
#include "dl/dsp-lib-manager.h"

#define DSP_DL_GKT_NAME			"dsp_gkt.xml"
#define DSP_DL_RULE_NAME		"dsp_reloc_rules.bin"
#define DSP_DL_GPT_OFFSET		(SZ_1K * 30)
#define DSP_DL_GPT_SIZE			(SZ_1K * 2)

struct dsp_kernel *dsp_kernel_alloc(struct dsp_kernel_manager *kmgr,
		unsigned int name_length, struct dsp_dl_lib_info *dl_lib)

{
	int ret;
	struct dsp_kernel *new, *list, *temp;
	bool checked = false;

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
				!strncmp(list->name, dl_lib->name,
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
		new->ref_count = 1;
		ret = dsp_binary_alloc_load(new->name, &new->elf);
		if (ret < 0) {
			mutex_unlock(&kmgr->lock);
			goto p_err_load;
		}
		new->elf_size = ret;
	}

	list_add_tail(&new->list, &kmgr->kernel_list);
	kmgr->kernel_count++;
	mutex_unlock(&kmgr->lock);

	dl_lib->file.mem = new->elf;
	dl_lib->file.size = new->elf_size;

	dsp_info("loaded kernel name : [%s](%#lx/%zu)\n",
			new->name, (long)new->elf, new->elf_size);
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

void dsp_kernel_free(struct dsp_kernel *kernel)
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

int dsp_kernel_load(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count)
{
	struct dsp_dl_load_status ret;
	struct dsp_mailbox *mbox;

	dsp_enter();
	mbox = &kmgr->gmgr->core->dspdev->system.mailbox;

	ret = dsp_dl_load_libraries(dl_libs, kernel_count);
	if (ret.status) {
		dsp_err("Failed to load kernel libraries(%u/%d)\n",
				kernel_count, ret.status);
		dsp_kernel_dump(kmgr);
		goto p_err;
	}

	if (ret.pm_inv)
		dsp_mailbox_send_message(mbox, DSP_COMMON_IVP_CACHE_INVALIDATE);

	dsp_leave();
	return 0;
p_err:
	return ret.status;
}

int dsp_kernel_unload(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count)
{
	int ret;

	dsp_enter();
	ret = dsp_dl_unload_libraries(dl_libs, kernel_count);
	if (ret) {
		dsp_err("Failed to unload kernel libraries(%u/%d)\n",
				kernel_count, ret);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_kernel_dump(struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	if (kmgr->dl_init)
		dsp_dl_print_status();
	dsp_leave();
}

static int __dsp_kernel_manager_dl_init(struct dsp_kernel_manager *kmgr)
{
	int ret;
	struct dsp_priv_mem *pmem;
	struct dsp_dl_param *dl_param;
	void *load;

	dsp_enter();
	pmem = kmgr->gmgr->core->dspdev->system.memory.priv_mem;
	dl_param = &kmgr->dl_param;

	ret = dsp_binary_alloc_load(DSP_DL_GKT_NAME, &load);
	if (ret < 0)
		goto p_err;

	dl_param->gkt.mem = load;
	dl_param->gkt.size = ret;

	ret = dsp_binary_alloc_load(DSP_DL_RULE_NAME, &load);
	if (ret < 0)
		goto p_err_rule;

	dl_param->rule.mem = load;
	dl_param->rule.size = ret;

	dl_param->pm.addr = (unsigned long)pmem[DSP_PRIV_MEM_IVP_PM].kvaddr;
	dl_param->pm.size = pmem[DSP_PRIV_MEM_IVP_PM].size;
	dl_param->pm_offset = pmem[DSP_PRIV_MEM_IVP_PM].used_size;
	dl_param->gpt.addr = DSP_DL_GPT_OFFSET;
	dl_param->gpt.size = DSP_DL_GPT_SIZE;
	dl_param->dl_out.addr = (unsigned long)pmem[DSP_PRIV_MEM_DL_OUT].kvaddr;
	dl_param->dl_out.size = pmem[DSP_PRIV_MEM_DL_OUT].size;

	ret = dsp_dl_init(&kmgr->dl_param);
	if (ret) {
		dsp_err("Failed to init DL(%d)\n", ret);
		goto p_err_init;
	}

	dsp_info("DL is initilized(%s)\n", DL_COMMIT_HASH);
	dsp_leave();
	return 0;
p_err_init:
	vfree(dl_param->rule.mem);
p_err_rule:
	vfree(dl_param->gkt.mem);
p_err:
	return ret;
}

int dsp_kernel_manager_open(struct dsp_kernel_manager *kmgr)
{
	int ret;

	dsp_enter();
	mutex_lock(&kmgr->lock);
	if (kmgr->dl_init) {
		kmgr->dl_init++;
		mutex_unlock(&kmgr->lock);
	} else {
		ret = __dsp_kernel_manager_dl_init(kmgr);
		if (ret) {
			mutex_unlock(&kmgr->lock);
			goto p_err;
		}
		kmgr->dl_init = 1;
		mutex_unlock(&kmgr->lock);
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_kernel_manager_dl_deinit(struct dsp_kernel_manager *kmgr)
{
	int ret;

	dsp_enter();
	ret = dsp_dl_close();
	if (ret)
		dsp_err("Failed to close DL(%d)\n", ret);

	vfree(kmgr->dl_param.rule.mem);
	vfree(kmgr->dl_param.gkt.mem);
	dsp_leave();
}

void dsp_kernel_manager_close(struct dsp_kernel_manager *kmgr)
{
	dsp_enter();
	mutex_lock(&kmgr->lock);
	if (kmgr->dl_init == 1) {
		__dsp_kernel_manager_dl_deinit(kmgr);
		kmgr->dl_init = 0;
	} else {
		kmgr->dl_init--;
	}
	mutex_unlock(&kmgr->lock);
	dsp_leave();
}

int dsp_kernel_manager_probe(struct dsp_graph_manager *gmgr)
{
	int ret;
	struct dsp_kernel_manager *kmgr;

	dsp_enter();
	kmgr = &gmgr->kernel_manager;
	kmgr->gmgr = gmgr;

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
		dsp_kernel_free(kernel);
	}
	dsp_util_bitmap_deinit(&kmgr->kernel_map);
	dsp_leave();
}
