// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>

#include "common/dsp-log.h"
#include "dsp-binary.h"

int dsp_binary_alloc_load(struct device *dev, const char *name, char *postfix,
		const char *extension, void **target, size_t *loaded_size)
{
	int ret;
	char full_name[DSP_BINARY_NAME_SIZE];
	const struct firmware *fw_blob;

	dsp_enter();
	if (postfix && (postfix[0] != '\0'))
		snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s_%s.%s",
				name, postfix, extension);
	else if (extension)
		snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s.%s",
				name, extension);
	else
		snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s", name);

	if (!target) {
		ret = -EINVAL;
		dsp_err("dest address must be not NULL[%s]\n", full_name);
		goto p_err_target;
	}

	ret = request_firmware(&fw_blob, full_name, dev);
	if (ret < 0) {
		dsp_err("Failed to request binary[%s](%d)\n", full_name, ret);
		goto p_err_req;
	}

	*target = vmalloc(fw_blob->size);
	if (!(*target)) {
		ret = -ENOMEM;
		dsp_err("Failed to allocate target for binary[%s](%zu)\n",
				full_name, fw_blob->size);
		goto p_err_alloc;
	}

	memcpy(*target, fw_blob->data, fw_blob->size);
	if (loaded_size)
		*loaded_size = fw_blob->size;
	dsp_info("binary[%s/%zu] is loaded\n", full_name, fw_blob->size);
	release_firmware(fw_blob);
	dsp_leave();
	return 0;
p_err_alloc:
	release_firmware(fw_blob);
p_err_req:
p_err_target:
	return ret;
}
