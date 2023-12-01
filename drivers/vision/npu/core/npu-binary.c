/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#include "npu-binary.h"
#include "common/npu-log.h"
#include "npu-device.h"
#include "npu-scheduler.h"
#include "npu-common.h"
#include "npu-ver.h"
#if IS_ENABLED(CONFIG_NPU_FW_HEADER_FILE)
#include "npu-firmware.h"
#endif

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#if IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#define NPU_S2MPU_NAME	"NPUS"
#else
#define NPU_S2MPU_NAME	"DNC"
#endif

int npu_firmware_file_read_signature(struct npu_binary *binary,
				__attribute__((unused))void *target,
				__attribute__((unused))size_t target_size,
				__attribute__((unused))int mode)
{
	int ret = 0;

	ret = imgloader_boot(&binary->imgloader);
	if (ret)
		npu_err("fail(%d) in imgloader_boot\n", ret);

	return ret;
}

int npu_imgloader_probe(struct npu_binary *binary, struct device *dev)
{
	int ret = 0;

	binary->imgloader.dev = dev;
	binary->imgloader.owner = THIS_MODULE;
	binary->imgloader.ops = &npu_imgloader_ops;
	binary->imgloader.fw_name = NPU_FW_NAME;
	binary->imgloader.name = NPU_S2MPU_NAME;
	binary->imgloader.fw_id = 0;

	ret = imgloader_desc_init(&binary->imgloader);
	if (ret)
		probe_err("imgloader_desc_init is fail(%d)\n", ret);

	return ret;
}

#define FW_SIGNATURE_LEN        528
static int npu_imgloader_memcpy(struct imgloader_desc *imgloader, const u8 *metadata, size_t size,
									phys_addr_t *fw_phys_base, size_t *fw_bin_size, size_t *fw_mem_size)
{
	int ret = 0;
	struct npu_binary *binary;
	struct npu_system *system;
	struct npu_memory_buffer *fwmem;

	binary = container_of(imgloader, struct npu_binary, imgloader);
	system = container_of(binary, struct npu_system, binary);

	fwmem = npu_get_mem_area(system, "fwmem");
	memcpy(fwmem->vaddr, metadata, size);

	npu_dbg("checking firmware phys(%#llx), bin_size(%#zx), daddr(%#llx++%#zx)\n",
						fwmem->paddr, size, fwmem->daddr, fwmem->size);

	*fw_phys_base = fwmem->paddr;
	*fw_bin_size = size;
	*fw_mem_size = fwmem->size;

	return ret;
}

static struct imgloader_ops npu_imgloader_ops = {
	.mem_setup = npu_imgloader_memcpy,
	.verify_fw = NULL,
	.blk_pwron = NULL,
	.init_image = NULL,
	.deinit_image = NULL,
	.shutdown = NULL,
};
#else
#define FW_SIGNATURE_LEN        528
int npu_firmware_file_read_signature(struct npu_binary *binary,
				void *target, size_t target_size, int mode)
{
	int ret = 0;
	const struct firmware *fw_blob;
	char *npu_fw_path = NULL;

#if IS_ENABLED(CONFIG_NPU_FW_HEADER_FILE)
	binary->image_size = NPU_bin_len;
	memcpy(target, (void *)NPU_bin, NPU_bin_len);
	npu_info("success of fw_header_file(NPU.bin, %u) apply.\n", NPU_bin_len);
	ret = 0;
#else
	switch (mode) {
	case NPU_PERF_MODE_NPU_BOOST:
	case NPU_PERF_MODE_NPU_BOOST_BLOCKING:
		npu_fw_path = NPU_FW_PATH4 NPU_PERF_FW_NAME;
		break;
	case NPU_PERF_MODE_NPU_DN:
		npu_fw_path = NPU_FW_PATH5 NPU_DN_FW_NAME;
		break;
	default:
		npu_fw_path = NPU_FW_PATH2 NPU_FW_NAME;
		break;
	}

	ret = request_firmware(&fw_blob, NPU_FW_NAME, binary->dev);
	if (ret) {
		npu_err("fail(%d) in request_firmware(%s)", ret, npu_fw_path);
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		npu_err("null in fw_blob\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		npu_err("null in fw_blob->data\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (fw_blob->size > target_size) {
		npu_err("image size over(%ld > %ld)\n", fw_blob->size, target_size);
		ret = -EIO;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%s, %ld) apply.\n", npu_fw_path, fw_blob->size);

request_err:
	release_firmware(fw_blob);
#endif
	return ret;
}

int npu_imgloader_probe(__attribute__((unused))struct npu_binary *binary,
				__attribute__((unused))struct device *dev)
{
	return 0;
}
#endif

int npu_fw_vector_load(struct npu_binary *binary,
			void *target, size_t target_size)
{
	int ret = 0;
	const struct firmware *fw_blob;

	ret = request_firmware(&fw_blob, NPU_FW_VECTOR_NAME, binary->dev);
	if (ret) {
		npu_err("fail(%d) in request_firmware(%s)", ret, NPU_FW_VECTOR_NAME);
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		npu_err("null in fw_blob\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		npu_err("null in fw_blob->data\n");
		ret = -EINVAL;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%s, %ld) apply.\n", NPU_FW_VECTOR_NAME, fw_blob->size);

request_err:
	release_firmware(fw_blob);
	return ret;
}

int npu_fw_slave_load(struct npu_binary *binary, void *target)
{
	int ret = 0;
	const struct firmware *fw_blob;

	ret = firmware_request_nowarn(&fw_blob, NPU_FW_SLAVE_NAME, binary->dev);
	if (ret) {
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		ret = -EINVAL;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%s, %ld) apply.\n", NPU_FW_SLAVE_NAME, fw_blob->size);

request_err:
	release_firmware(fw_blob);
	return ret;
}

int npu_binary_init(struct npu_binary *binary,
	struct device *dev,
	char *fpath1,
	char *fpath2,
	char *fname)
{
	BUG_ON(!binary);
	BUG_ON(!dev);

	binary->dev = dev;
	binary->image_size = 0;
	snprintf(binary->fpath1, sizeof(binary->fpath1), "%s%s", fpath1, fname);
	snprintf(binary->fpath2, sizeof(binary->fpath2), "%s%s", fpath2, fname);

	return 0;
}

#define MAX_SIGNATURE_LEN       128
#define SIGNATURE_HEADING       "NPU Firmware signature : "
void print_ufw_signature(struct npu_memory_buffer *fwmem)
{
	int i = 0;
	char version_buf[MAX_FW_VERSION_LEN];
	char *fw_version_addr;
	char *p_fw_version;

	fw_version_addr = (char *)(fwmem->vaddr + 0xF000);

	version_buf[MAX_FW_VERSION_LEN - 1] = '\0';

	p_fw_version = version_buf;

	for (i = 0; i < MAX_FW_VERSION_LEN - 1; i++, p_fw_version++, fw_version_addr++)
		*p_fw_version = *fw_version_addr;

	npu_info("NPU Firmware version : %s\n", version_buf);
}
