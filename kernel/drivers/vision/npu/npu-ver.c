/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/sysfs.h>
#include <linux/string.h>

#include "npu-device.h"
#include "npu-debug.h"
#include "npu-log.h"
#include "npu-interface.h"
#include "npu-ver.h"

#if IS_ENABLED(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER)
static struct npu_ver npu_version;

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t remain = PAGE_SIZE - 1;
	ssize_t ret = 0;

	npu_info("start.\n");

	ret = scnprintf(buf, remain,
		"NCP version: %u\n"
		"Mailbox verison: %d\n"
		"CMD version: %d\n"
		"User API version: %d\n"
		"Device Driver version : %u.%u.%u\n"
		"Device Driver module version : %s\n",
		NCP_VERSION, CONFIG_NPU_MAILBOX_VERSION,
		CONFIG_NPU_COMMAND_VERSION, USER_API_VERSION,
		NPU_DD_MAJOR_VERSION, NPU_DD_MINOR_VERSION, NPU_DD_SOURCE_VERSION, THIS_MODULE->srcversion);

	return ret;
}


static DEVICE_ATTR_RO(version);

/* Exported functions */
int npu_ver_probe(struct npu_device *npu_device)
{
	int ret;
	struct device *dev;

	BUG_ON(!npu_device);
	dev = npu_device->dev;
	BUG_ON(!dev);

	ret = sysfs_create_file(&dev->kobj, &dev_attr_version.attr);
	if (ret) {
		probe_err("sysfs_create_file error : ret = %d\n", ret);
		goto p_err_version;
	}

	npu_version.fw_version = kzalloc(sizeof(char *) * MAX_FW_VERSION_LEN, GFP_KERNEL);
	if (!npu_version.fw_version) {
		ret = -ENOMEM;
		goto p_err_version;
	}

p_err_version:
	return ret;
}
/* Exported functions */
int npu_ver_release(struct npu_device *npu_device)
{
	struct device *dev;

	BUG_ON(!npu_device);
	dev = npu_device->dev;
	BUG_ON(!dev);

	sysfs_remove_file(&dev->kobj, &dev_attr_version.attr);

	return 0;
}

int npu_ver_info(struct npu_device *npu_device, struct vs4l_version *version)
{
	int ret = 0;

	WARN_ON(!version);
	WARN_ON(!npu_device);

	version->build_info = "Not available";
	version->signature = "Not available";
	version->ncp_version = NCP_VERSION;
	version->mailbox_version = CONFIG_NPU_MAILBOX_VERSION;
	version->cmd_version = CONFIG_NPU_COMMAND_VERSION;
	version->api_version = USER_API_VERSION;
	version->driver_version = "Not available";
	strlcpy(version->fw_version, "Not available", strlen("Not available") + 1);

	return ret;
}

int npu_ver_fw_info(struct npu_device *npu_device, struct vs4l_version *version)
{
	int ret = 0;
	int i;
	struct npu_memory_buffer *fwmem;
	char *fw_version_addr;
	char *p_fw_version;

	WARN_ON(!version);
	WARN_ON(!npu_device);

	fwmem = npu_get_mem_area(&npu_device->system, "fwmem");

	fw_version_addr = (char *)(fwmem->vaddr + 0xF000);

	version->fw_version[MAX_FW_VERSION_LEN - 1] = '\0';

	p_fw_version = version->fw_version;

	for (i = 0; i < MAX_FW_VERSION_LEN - 1; i++, p_fw_version++, fw_version_addr++)
		*p_fw_version = *fw_version_addr;

	return ret;
}

void npu_ver_dump(struct npu_device *npu_device)
{
	int i;
	struct npu_memory_buffer *fwmem;
	char *fw_version_addr;
	char *p_fw_version;

	WARN_ON(!npu_device);

	fwmem = npu_get_mem_area(&npu_device->system, "fwmem");

	fw_version_addr = (char *)(fwmem->vaddr + 0xF000);

	npu_version.fw_version[MAX_FW_VERSION_LEN - 1] = '\0';

	p_fw_version = npu_version.fw_version;

	for (i = 0; i < MAX_FW_VERSION_LEN - 1; i++, p_fw_version++, fw_version_addr++)
		*p_fw_version = *fw_version_addr;

	npu_dump("Device Driver version : %u.%u.%u\n", NPU_DD_MAJOR_VERSION, NPU_DD_MINOR_VERSION, NPU_DD_SOURCE_VERSION);
	npu_dump("Device Driver module version : %s\n", THIS_MODULE->srcversion);
	npu_dump("Firmware version : %s\n", npu_version.fw_version);

}
#else	/* Not CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER */

int npu_ver_probe(struct npu_device *npu_device)
{
	/* No OP */
	return 0;
}

int npu_ver_release(struct npu_device *npu_device)
{
	/* No OP */
	return 0;
}
#endif
