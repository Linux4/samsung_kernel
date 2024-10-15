// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include "pablo-icpu.h"
#include "pablo-icpu-firmware.h"
#include "pablo-icpu-mem.h"
#include "pablo-icpu-imgloader.h"
#include "pablo-icpu-enum.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-FIRMWARE]",
};

struct icpu_logger *get_icpu_firmware_log(void)
{
	return &_log;
}

enum icpu_firmware_state {
	ICPU_FW_STATE_INIT,
	ICPU_FW_STATE_LOADED,
	ICPU_FW_STATE_INVALID,
};

enum icpu_firmware_mem_id {
	ICPU_FW_MEM_BIN = 0,
	ICPU_FW_MEM_HEAP = 1,
	ICPU_FW_MEM_LOG = 2,
	ICPU_FW_MEM_SECURE_HEAP = 3,
	ICPU_FW_MEM_MAX = 4,
};

struct icpu_firmware_mem_desc {
	enum icpu_firmware_mem_id id;
	enum icpu_mem_type type;
	const char *mem_name;
	const char *heap_name;
	u32 size;
	u32 align;
	bool permanent;
	u32 sign_size;
	bool sign_check;
	void *icpubuf;
	bool no_optimization;
};

static struct icpu_firmware {
	u32 num_mem;
	u32 bin_size_max;
	bool aarch64;
	struct icpu_firmware_mem_desc *mem_desc;
	enum icpu_firmware_state state;
} _icpu_fw;
static DEFINE_MUTEX(fw_load_mutex);

static inline bool __is_fw_bin_permanent(void)
{
	return (_icpu_fw.mem_desc[ICPU_FW_MEM_BIN].permanent &&
		!is_get_debug_param(IS_DEBUG_PARAM_ICPU_RELOAD));
}

#define ICPU_FIRMWARE_VERSION_OFFSET 80
#define ICPU_FIRMWARE_MEM_SIZE_OFFSET (ICPU_FIRMWARE_VERSION_OFFSET + 4)
static void __free_firmware_mem(void)
{
	int i;
	struct icpu_firmware_mem_desc *desc;

	for (i = 0; i < _icpu_fw.num_mem; i++) {
		desc = &_icpu_fw.mem_desc[i];
		if ((desc->permanent == false || is_get_debug_param(IS_DEBUG_PARAM_ICPU_RELOAD)) &&
			desc->icpubuf) {
			ICPU_INFO("free firmware mem name(%s), size(0x%x)",
					desc->mem_name, desc->size);
			pablo_icpu_mem_free(desc->icpubuf);
			desc->icpubuf = NULL;
		}
	}
}

static int __alloc_firmware_mem(enum icpu_firmware_mem_id id)
{
	struct icpu_firmware_mem_desc *desc;

	if (id >= _icpu_fw.num_mem) {
		ICPU_WARN("id(%d) is not valid, skip to alloc", id);
		return 0;
	}

	desc = &_icpu_fw.mem_desc[id];
	if (!desc->icpubuf) {
		desc->icpubuf = pablo_icpu_mem_alloc(desc->type, desc->size,
				desc->heap_name, desc->align);
		if (IS_ERR_OR_NULL(desc->icpubuf))
			return -ENOMEM;

		ICPU_INFO("alloc firmware mem name(%s), size(0x%x/%uMB)",
				desc->mem_name, desc->size, desc->size >> 20);
	}

	return 0;
}

static inline void __print_firmware_version(const void *kva, size_t size)
{
	u32 offset = _icpu_fw.mem_desc[ICPU_FW_MEM_BIN].sign_size + ICPU_FIRMWARE_VERSION_OFFSET;

	ICPU_INFO("%s", (char *)(kva + size - offset));
}

#define ICPU_FIRMWARE_SIZE_LEN (4) /* 4 character for firmware size */
static size_t __get_heap_size_firmware(const void *kva, size_t size)
{
	int ret;
	char carray[5] = { 0, };
	size_t heap_size = 0;
	u32 offset = _icpu_fw.mem_desc[ICPU_FW_MEM_BIN].sign_size + ICPU_FIRMWARE_MEM_SIZE_OFFSET;

	memcpy(carray, kva + size - offset, ICPU_FIRMWARE_SIZE_LEN);
	ret = kstrtol(carray, 16, &heap_size);
	if (ret) {
		ICPU_ERR("Get fw mem heap_size fail, str:%s, ret(%d)", carray, ret);
		return 0;
	}

	/* Size in the binary is upper 16bit */
	heap_size = heap_size << 16;

	ICPU_INFO("Firmware mem heap_size str:%s, 0x%zx, %zuMB", carray, heap_size, heap_size >> 20);

	return heap_size;
}

static void *__get_buf(enum icpu_firmware_mem_id id)
{
	int i;
	struct icpu_firmware_mem_desc *desc;

	for (i = 0; i < _icpu_fw.num_mem; i++) {
		desc = &_icpu_fw.mem_desc[i];
		if (desc->id == id)
			return desc->icpubuf;
	}

	return NULL;
}

static enum icpu_firmware_mem_id __get_secure_mem_id(void)
{
	enum icpu_firmware_mem_id mem_id;

	if (_icpu_fw.num_mem > ICPU_FW_MEM_SECURE_HEAP)
		mem_id = ICPU_FW_MEM_SECURE_HEAP;
	else
		mem_id = ICPU_FW_MEM_HEAP;

	ICPU_INFO("Secure heap mem_id(%d)", mem_id);

	return mem_id;
}

void *icpu_firmware_get_buf_bin(void)
{
	return __get_buf(ICPU_FW_MEM_BIN);
}

void *icpu_firmware_get_buf_heap(bool secure_mode)
{
	enum icpu_firmware_mem_id mem_id;

	if (secure_mode)
		mem_id = __get_secure_mem_id();
	else
		mem_id = ICPU_FW_MEM_HEAP;

	return __get_buf(mem_id);
}

void *icpu_firmware_get_buf_log(void)
{
	return __get_buf(ICPU_FW_MEM_LOG);
}

bool icpu_firmware_get_aarch64(void)
{
	return _icpu_fw.aarch64;
}

static int __update_fw_size(size_t size)
{
	struct icpu_firmware_mem_desc *mem_desc;

	if (size > _icpu_fw.bin_size_max) {
		ICPU_ERR("Requested FW size is too big, req(0x%zx), max(0x%x)",
				size, _icpu_fw.bin_size_max);
		return -EINVAL;
	}

	mem_desc = &_icpu_fw.mem_desc[ICPU_FW_MEM_BIN];

	/* for backward compatibility. will be removed */
	if (mem_desc->no_optimization)
		return 0;

	if (mem_desc->size != size) {
		ICPU_INFO("FW size changed: %d to 0x%zx/%zuMB", mem_desc->size, size, size >> 20);
		mem_desc->size = size;
	}

	return 0;
}

static void __update_heap_size(size_t size)
{
	struct icpu_firmware_mem_desc *mem_desc;

	/* for backward compatibility. will be removed */
	if (_icpu_fw.num_mem == 1)
		mem_desc = &_icpu_fw.mem_desc[ICPU_FW_MEM_BIN];
	else
		mem_desc = &_icpu_fw.mem_desc[ICPU_FW_MEM_HEAP];

	if (mem_desc->size != size) {
		ICPU_INFO("heap size changed: %d to 0x%zx/%zu", mem_desc->size, size, size >> 20);
		mem_desc->size = size;
	}
}

static void *__allocate_fw_buf(size_t size)
{
	int ret;

	ret = __update_fw_size(size);
	if (ret)
		return NULL;

	ret = __alloc_firmware_mem(ICPU_FW_MEM_BIN);
	if (ret) {
		ICPU_ERR("Fail to alloc bin mem, ret(%d)", ret);
		return NULL;
	}

	return __get_buf(ICPU_FW_MEM_BIN);
}

static int __check_firmware(const void *kva, size_t size)
{
	size_t heap_size = 0;

	heap_size = __get_heap_size_firmware(kva, size);
	if (heap_size) {
		__print_firmware_version(kva, size);

		/* check & update heap size */
		__update_heap_size(heap_size);
	} else {
		/* something wrong */
		ICPU_ERR("firmware binary is not valid");
		return -EINVAL;
	}

	return 0;
}

static struct pablo_icpu_imgloader_ops imgloader_ops = {
	.req_buf = __allocate_fw_buf,
	.verify = __check_firmware,
};

static int __load_firmware_bin(void *dev)
{
	int ret;
	bool fw_signed = _icpu_fw.mem_desc[ICPU_FW_MEM_BIN].sign_check;

	if (_icpu_fw.state == ICPU_FW_STATE_LOADED) {
		ICPU_INFO("Firmware is loaded. skip load firmware");
		return 0;
	}

	ret = pablo_icpu_imgloader_init(dev, fw_signed, &imgloader_ops);
	if (ret)
		return ret;

	ret = pablo_icpu_imgloader_load();
	if (ret)
		goto imgload_fail;

	_icpu_fw.state = ICPU_FW_STATE_LOADED;

	return ret;

imgload_fail:
	pablo_icpu_imgloader_release();

	return ret;
}

static int __load_firmware_heap(bool secure_mode)
{
	int ret;
	enum icpu_firmware_mem_id mem_id;

	if (secure_mode)
		mem_id = __get_secure_mem_id();
	else
		mem_id = ICPU_FW_MEM_HEAP;

	ret = __alloc_firmware_mem(mem_id);
	if (ret) {
		ICPU_ERR("Fail to alloc heap mem id(%d), ret(%d)", mem_id, ret);
		return ret;
	}

	ret = __alloc_firmware_mem(ICPU_FW_MEM_LOG);
	if (ret) {
		ICPU_ERR("Fail to alloc log mem, ret(%d)", ret);
		return ret;
	}

	return ret;
}

int load_firmware(void *dev, bool secure_mode)
{
	int ret;

	mutex_lock(&fw_load_mutex);
	ret = __load_firmware_bin(dev);
	mutex_unlock(&fw_load_mutex);
	if (ret)
		return ret;

	ret = __load_firmware_heap(secure_mode);
	if (ret) {
		pablo_icpu_imgloader_release();
		__free_firmware_mem();

		return ret;
	}

	return ret;
}

int is_firmware_loaded(void)
{
	int ret;

	mutex_lock(&fw_load_mutex);
	if (_icpu_fw.state == ICPU_FW_STATE_LOADED) {
		ICPU_INFO("Firmware is loaded\n");
		ret = 1;
	} else {
		ret = 0;
	}
	mutex_unlock(&fw_load_mutex);

	return ret;
}

int preload_firmware(void *dev, unsigned long flag)
{
	int ret;

	mutex_lock(&fw_load_mutex);
	ret = __load_firmware_bin(dev);
	mutex_unlock(&fw_load_mutex);

	return ret;
}

void teardown_firmware(void)
{
	mutex_lock(&fw_load_mutex);

	if (__is_fw_bin_permanent() == false) {
		pablo_icpu_imgloader_release();
		_icpu_fw.state = ICPU_FW_STATE_INIT;
	}

	__free_firmware_mem();

	mutex_unlock(&fw_load_mutex);
}

static int __parse_dt(struct device *dev)
{
	struct device_node *np;
	struct device_node *desc_np;
	struct icpu_firmware_mem_desc *desc;
	int id;
	int ret;

	_icpu_fw.aarch64 = of_property_read_bool(dev->of_node, "aarch64");
	ICPU_INFO("firmware aarch64: %d", _icpu_fw.aarch64);

	np = of_get_child_by_name(dev->of_node, "firmware");
	if (!np) {
		ICPU_ERR("Could not find firmware in device tree");
		return -ENOENT;
	}

	_icpu_fw.num_mem = of_get_child_count(np);
	ICPU_INFO("firmware num-mem: %d", _icpu_fw.num_mem);

	if (_icpu_fw.num_mem < 1) {
		ICPU_ERR("invalid number of fw mem: %d", _icpu_fw.num_mem);
		return -ENOENT;
	}

	_icpu_fw.mem_desc = kzalloc(sizeof(struct icpu_firmware_mem_desc) * _icpu_fw.num_mem, GFP_KERNEL);
	if (!_icpu_fw.mem_desc)
		return -ENOMEM;

	for_each_child_of_node(np, desc_np) {
		of_property_read_u32(desc_np, "mem-id", &id);
		desc = &_icpu_fw.mem_desc[id];
		desc->id = id;

		desc->mem_name = desc_np->name;

		if (of_property_read_bool(desc_np, "cma-alloc")) {
			ret = of_reserved_mem_device_init(dev);
			if (ret) {
				ICPU_ERR("Failed to get reserved memory region, ret(%d)", ret);
				goto err;
			}
			desc->type = ICPU_MEM_TYPE_CMA;
		}

		if (of_property_read_bool(desc_np, "dma-heap")) {
			of_property_read_string(desc_np, "heap-name", &desc->heap_name);
			if (!desc->heap_name) {
				ICPU_ERR("Failed to get heap-name");
				goto err;
			}
			desc->type = ICPU_MEM_TYPE_PMEM;
		}

		of_property_read_u32(desc_np, "size", &desc->size);
		of_property_read_u32(desc_np, "align", &desc->align);
		desc->permanent = of_property_read_bool(desc_np, "permanent");
		of_property_read_u32(desc_np, "signature-size", &desc->sign_size);
		desc->sign_check = of_property_read_bool(desc_np, "signature-check");
		desc->no_optimization = of_property_read_bool(desc_np, "no-optimization");

		ICPU_INFO("%s: id(%d), heap(%s), size(%d), align(%d), permanent(%d), signature(%dbyte/%d), no-optimization(%d)",
				desc->mem_name, desc->id, desc->heap_name, desc->size,
				desc->align, desc->permanent, desc->sign_size, desc->sign_check,
				desc->no_optimization);
	}

	_icpu_fw.bin_size_max = _icpu_fw.mem_desc[ICPU_FW_MEM_BIN].size;

	return 0;

err:
	kfree(_icpu_fw.mem_desc);
	_icpu_fw.num_mem = 0;

	return -EINVAL;
}

int icpu_firmware_probe(void *dev)
{
	int ret;

	ret = __parse_dt(dev);
	if (ret)
		return ret;

	_icpu_fw.state = ICPU_FW_STATE_INIT;

	return 0;
}

static void reset_fw_mem_desc(void)
{
	int i;

	for (i = 0; i < _icpu_fw.num_mem; i++)
		_icpu_fw.mem_desc[i].permanent = false;

	__free_firmware_mem();
}

void icpu_firmware_remove(void)
{
	reset_fw_mem_desc();

	memset(&_icpu_fw, 0, sizeof(_icpu_fw));
}
