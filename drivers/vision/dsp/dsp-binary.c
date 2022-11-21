// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>

#include "dsp-log.h"
#include "dsp-binary.h"

int dsp_binary_load(const char *name, void *target, size_t size)
{
	int ret;
	const struct firmware *fw_blob;

	dsp_enter();
	if (!target) {
		ret = -EINVAL;
		dsp_err("dest address must be not NULL[%s]\n", name);
		goto p_err_target;
	}

	ret = request_firmware(&fw_blob, name, dsp_global_dev);
	if (ret < 0) {
		dsp_err("Failed to request binary[%s](%d)\n", name, ret);
		goto p_err_req;
	}

	if (fw_blob->size > size) {
		ret = -EIO;
		dsp_err("binary(%s) size is over(%zu/%zu)\n",
				name, fw_blob->size, size);
		goto p_err_size;
	}

	memcpy(target, fw_blob->data, fw_blob->size);
	ret = fw_blob->size;
	release_firmware(fw_blob);

	dsp_leave();
	return ret;
p_err_size:
	release_firmware(fw_blob);
p_err_req:
p_err_target:
	return ret;
}

int dsp_binary_master_load(const char *name, void __iomem *target, size_t size)
{
	int ret;
	const struct firmware *fw_blob;
	unsigned int remain = 0;

	dsp_enter();
	if (!target) {
		ret = -EINVAL;
		dsp_err("dest address must be not NULL[%s]\n", name);
		goto p_err_target;
	}

	ret = request_firmware(&fw_blob, name, dsp_global_dev);
	if (ret < 0) {
		dsp_err("Failed to request binary[%s](%d)\n", name, ret);
		goto p_err_req;
	}

	if (fw_blob->size > size) {
		ret = -EIO;
		dsp_err("binary(%s) size is over(%zu/%zu)\n",
				name, fw_blob->size, size);
		goto p_err_size;
	}

	__iowrite32_copy(target, fw_blob->data,	fw_blob->size >> 2);
	if (fw_blob->size & 0x3) {
		memcpy(&remain, fw_blob->data + (fw_blob->size & ~0x3),
				fw_blob->size & 0x3);
		writel(remain, target + (fw_blob->size & ~0x3));
	}
	ret = fw_blob->size;
	release_firmware(fw_blob);

	dsp_leave();
	return ret;
p_err_size:
	release_firmware(fw_blob);
p_err_req:
p_err_target:
	return ret;
}

int dsp_binary_alloc_load(const char *name, void **target)
{
	int ret;
	const struct firmware *fw_blob;

	dsp_enter();
	if (!target) {
		ret = -EINVAL;
		dsp_err("dest address must be not NULL[%s]\n", name);
		goto p_err_target;
	}

	ret = request_firmware(&fw_blob, name, dsp_global_dev);
	if (ret < 0) {
		dsp_err("Failed to request binary[%s](%d)\n", name, ret);
		goto p_err_req;
	}

	*target = vzalloc(fw_blob->size);
	if (!(*target)) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate target for binary[%s](%zu)\n",
				name, fw_blob->size);
		goto p_err_alloc;
	}

	memcpy(*target, fw_blob->data, fw_blob->size);
	ret = fw_blob->size;
	release_firmware(fw_blob);

	dsp_leave();
	return ret;
p_err_alloc:
	release_firmware(fw_blob);
p_err_req:
p_err_target:
	return ret;
}

int dsp_binary_load_async(const char *name, void *context,
		void (*cont)(const struct firmware *fw, void *context))
{
	int ret;

	dsp_enter();
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, name,
			dsp_global_dev, GFP_KERNEL, context, cont);
	if (ret < 0) {
		dsp_err("Failed to request binary asynchronously[%s](%d)\n",
				name, ret);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}
