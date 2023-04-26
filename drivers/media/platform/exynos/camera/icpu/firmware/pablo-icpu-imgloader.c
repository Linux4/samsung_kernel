// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/exynos/imgloader.h>
#include <linux/slab.h>
#endif

#include "pablo-icpu.h"
#include "pablo-icpu-imgloader.h"
#include "pablo-icpu-mem.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-IMGLOADER]",
};

struct icpu_logger *get_icpu_imgloader_log(void)
{
	return &_log;
}

enum icpu_imgloader_state {
	ICPU_IMGLOADER_DEINIT = 0,
	ICPU_IMGLOADER_INIT = 1,
	ICPU_IMGLOADER_INVALID = 2,
};

static struct icpu_imgloader {
	void *dev;
	struct pablo_icpu_imgloader_ops *ops;
	enum icpu_imgloader_state state;
	void *desc; /* for exynos imgloader */

	void *icpubuf;
	ulong fw_kva;
	phys_addr_t fw_phys_base;
	size_t fw_bin_size;
	size_t fw_mem_size;
} _imgloader;

static int __copy_firmware(const u8 *metadata, size_t size)
{
	int ret;

	if (_imgloader.ops->verify) {
		ret = _imgloader.ops->verify(metadata, size);
		if (ret) {
			ICPU_ERR("firmware verify fail, ret(%d)", ret);
			return ret;
		}
	}

	memcpy((void*)_imgloader.fw_kva, metadata, size);

	/* sync for verification */
	pablo_icpu_mem_sync_for_device(_imgloader.icpubuf);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static int __mem_setup(struct imgloader_desc *imgloader,
		const u8 *metadata, size_t size,
		phys_addr_t *fw_phys_base,
		size_t *fw_bin_size,
		size_t *fw_mem_size)
{
	int ret;

	ret = __copy_firmware(metadata, size);
	if (ret)
		return ret;

	*fw_phys_base = _imgloader.fw_phys_base;

	_imgloader.fw_bin_size = size;
	*fw_bin_size = size;

	*fw_mem_size = _imgloader.fw_mem_size;

	return 0;
}

static struct imgloader_ops exynos_imgloader_ops = {
	.mem_setup = __mem_setup,
};
#endif

#define ICPU_FIRMWARE_BINARY_NAME "pablo_icpufw.bin"
static int __init_imgloader_desc(void *dev, void **desc)
{
	int ret = 0;
	void *_desc = NULL;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc *loader = kzalloc(sizeof(struct imgloader_desc), GFP_KERNEL);

	loader->dev = dev;
	loader->owner = THIS_MODULE;
	loader->ops = &exynos_imgloader_ops;
	loader->name = "ISP_ICPU";
	loader->fw_name = ICPU_FIRMWARE_BINARY_NAME;
	loader->fw_id = 0;

	ret = imgloader_desc_init(loader);
	if (ret) {
		ICPU_ERR("exynos_imgloader_desc_init fail, ret(%d)", ret);
		kfree(loader);
		loader = NULL;
	}

	_desc = loader;
#endif

	*desc = _desc;

	return ret;
}

int pablo_icpu_imgloader_init(void *dev, bool signature, struct pablo_icpu_imgloader_ops *ops)
{
	int ret = 0;

	_imgloader.dev = dev;

	_imgloader.desc = NULL;
	if (signature) {
		ret = __init_imgloader_desc(dev, &_imgloader.desc);
		if (ret) {
			ICPU_ERR("Fail to init imgloader desc, ret(%d)", ret);
			return ret;
		}
	}

	_imgloader.ops = ops;
	_imgloader.state = ICPU_IMGLOADER_INIT;

	return ret;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_imgloader_init);

static int __update_buf_info(void *icpubuf)
{
	struct pablo_icpu_buf_info fw_info = pablo_icpu_mem_get_buf_info(icpubuf);

	if (!fw_info.kva) {
		ICPU_ERR("invalid fw mem");
		return -EINVAL;
	}

	_imgloader.icpubuf = icpubuf;
	_imgloader.fw_kva = fw_info.kva;
	_imgloader.fw_phys_base = fw_info.pa;
	_imgloader.fw_mem_size = fw_info.size;

	return 0;
}

static int __load_firmware(void)
{
	int ret;
	const struct firmware *fw_entry;

	ret = request_firmware(&fw_entry, ICPU_FIRMWARE_BINARY_NAME, _imgloader.dev);
	if (ret)
		return ret;

	ret = __copy_firmware(fw_entry->data, fw_entry->size);
	if (ret)
		goto release_fw;

	_imgloader.fw_bin_size = fw_entry->size;

release_fw:
	release_firmware(fw_entry);

	return ret;
}

int pablo_icpu_imgloader_load(void *icpubuf)
{
	int ret;

	ret = __update_buf_info(icpubuf);
	if (ret)
		return ret;

	if (_imgloader.desc) {
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
		ret = imgloader_boot(_imgloader.desc);
		if (ret) {
			ICPU_ERR("exynos imgloader_boot fail, ret(%d)", ret);
			return -EINVAL;
		}
#endif
	} else {
		ret = __load_firmware();
		if (ret)
			return ret;
	}

	return ret;
}

void pablo_icpu_imgloader_release(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	if (_imgloader.desc) {
		imgloader_shutdown(_imgloader.desc);
		imgloader_desc_release(_imgloader.desc);
		kfree(_imgloader.desc);
	}
#endif

	memset(&_imgloader, 0, sizeof(_imgloader));
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_imgloader_release);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct imgloader_ops *pablo_kunit_get_imgloader_ops(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	return &exynos_imgloader_ops;
#else
	return NULL;
#endif
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_imgloader_ops);

int pablo_kunit_update_bufinfo(void *icpubuf)
{
	return __update_buf_info(icpubuf);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_update_bufinfo);
#endif
