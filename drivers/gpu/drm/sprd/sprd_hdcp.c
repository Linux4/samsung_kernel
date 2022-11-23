// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */


#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "sprd_hdcp.h"

#define MAX_HL_DEVICES    1

#define HDCP_REG_RD(reg) 		readl_relaxed(reg)
#define HDCP_REG_WR(reg, mask) 		writel_relaxed(mask, reg)
#define HDCP_REG_SET(reg, mask) 	writel_relaxed(readl_relaxed(reg) | mask, reg)

bool randomize_mem;
int hdcp13_enabled;
struct sprd_hdcp hdcps[MAX_HL_DEVICES];
struct hdcp_mem *mem;

/* sprd_hdcp_ioc_meminfo implementation */
static long get_meminfo(struct sprd_hdcp *hl_dev, void __user *arg)
{
	struct sprd_hdcp_ioc_meminfo info;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0))
		return -EFAULT;

	info.hpi_base  = hl_dev->hpi_resource->start;
	info.code_base = 0;
	info.code_size = hl_dev->code_size;
	info.data_base = hl_dev->data_base - hl_dev->code_base;
	info.data_size = hl_dev->data_size;

	if (copy_to_user(arg, &info, sizeof info) != 0)
		return -EFAULT;

	DRM_INFO("%s SUCCESS()\n", __func__);

	return 0;
}

/* HL_DRV_IOC_LOAD_CODE implementation */
static long load_code(struct sprd_hdcp *hl_dev, struct hl_drv_ioc_code __user *arg)
{
	struct hl_drv_ioc_code head;
	uint32_t k;

	if (!hl_dev || !arg == 0 || !hl_dev->code || !hl_dev->data)
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (head.len > hl_dev->code_size)
		return -ENOSPC;

	if (hl_dev->code_loaded)
		return -EBUSY;

	if (randomize_mem) {
		prandom_bytes(hl_dev->code_offset, hl_dev->code_size);
		prandom_bytes(hl_dev->data_offset, hl_dev->data_size);
	}

	for (k = 0; k < 16; k = k + 1)
		*(hl_dev->code_offset + k) = k | 0xDEADBE00;

	DRM_INFO("Randomization passed \n");
	DRM_INFO("firmware length %d \n", head.len);

	if (copy_from_user(hl_dev->code_offset, &arg->data, head.len) != 0)
		return -EFAULT;

	for (k = 0; k < 16; k = k + 1) {
		DRM_INFO("ramdomized values read back %08x \n", *(hl_dev->code_offset + k));
	}

	hl_dev->code_loaded = 1;

	DRM_INFO("%s() SUCCESS\n", __func__);

	return 0;
}

static long sprd_enable_hdcp13(void)
{
	int i = 0;

	hdcp13_enabled = 0;
	dptx_configure_hdcp13();

	while (i < 10) {
		if (hdcp13_enabled) {
			DRM_INFO("%s() SUCCESS\n", __func__);
			return 0;
		}
		msleep(500);
		i++;
	}

	DRM_ERROR("%s() failed\n", __func__);

	return -EFAULT;
}

/* HL_DRV_IOC_WRITE_DATA implementation */
static long write_data(struct sprd_hdcp *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;
	int i;
	uint8_t *data_buf;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0))
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;

	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (head.offset & 0x3) {
		DRM_ERROR("%s cmd_data_write: Offset is non DWORD alligned 0x%x\n",
				MY_TAG, head.offset);
		return -EFAULT;
	}

	if (head.len & 0x3) {
		DRM_ERROR("%s cmd_data_write: Number of bytes is non DWORD alligned 0x%x\n",
				MY_TAG, head.len);
		return -EFAULT;
	}

	// copy data from dynamic array;
	data_buf = kzalloc((head.len + 8), GFP_KERNEL);
	for (i = 0; i < head.len; i++) {
		*(data_buf + i) = arg->data[i];
	}

	for (i = 0; i < head.len / 4; i++) {
		*(u32 *)(hl_dev->data_offset + head.offset + i * 4) = *(u32 *) (/*&arg->data*/data_buf + i * 4);
	}

	kfree(data_buf);
	DRM_INFO("%s() SUCCESS\n", __func__);

   return 0;
}

/* HL_DRV_IOC_READ_DATA implementation */
static long read_data(struct sprd_hdcp *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;
	uint32_t *buf, *ptr;
	int i;

	if (!hl_dev || !arg || !hl_dev->data)
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;

	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (head.offset & 0x3) {
		DRM_ERROR("%scmd_data_read: Offset is non DWORD alligned 0x%x\n",
				__func__, head.offset);
		return -EFAULT;
	}

	if (head.offset + head.len > hl_dev->data_size) {
		DRM_ERROR("%scmd_data_read: Invalid offset and size.\n", MY_TAG);
		return -EFAULT;
	} else {
		buf = kzalloc((head.len + 8), GFP_KERNEL);
		ptr = buf;
		for (i = 0; i < head.len / 4 + 1; i++) {
			*ptr = *(uint32_t *) (hl_dev->data_offset + head.offset + i * 4);
			DRM_INFO("data from booted HDCP %08x \n", *ptr);
			ptr++;
		}

		if (copy_to_user(&arg->data, buf, head.len) != 0)
			return -EFAULT;

		DRM_ERROR("%scmd_data_read: Done reading %u bytes from data memory at offset 0x%x\n",
				MY_TAG, head.len, head.offset);

		kfree(buf);
	}

	DRM_INFO("%s() SUCCESS\n", __func__);

   return 0;
}

/* HL_DRV_IOC_MEMSET_DATA implementation */
static long set_data(struct sprd_hdcp *hl_dev, void __user *arg)
{
	union {
		struct hl_drv_ioc_data data;
		unsigned char buf[sizeof (struct hl_drv_ioc_data) + 1];
	} u;

	if (!hl_dev || !arg || !hl_dev->data_offset)
		return -EFAULT;

	if (copy_from_user(&u.data, arg, sizeof u.buf) != 0)
		return -EFAULT;

	if (hl_dev->data_size < u.data.len)
		return -ENOSPC;

	if (hl_dev->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(hl_dev->data_offset + u.data.offset, u.data.data[0], u.data.len);

	DRM_INFO("%s() SUCCESS\n", __func__);

	return 0;
}

/* HL_DRV_IOC_READ_HPI implementation */
static long hpi_read(struct sprd_hdcp *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0))
		return -EFAULT;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(hl_dev->hpi + reg.offset);

	if (copy_to_user(arg, &reg, sizeof reg) != 0)
		return -EFAULT;

	DRM_INFO("%s() SUCCESS\n", __func__);

	return 0;
}

/* HL_DRV_IOC_WRITE_HPI implementation */
static long hpi_write(struct sprd_hdcp *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0))
		return -EFAULT;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	iowrite32(reg.value, hl_dev->hpi + reg.offset);

#ifdef TROOT_GRIFFIN
	//  If Kill command
	//  (HL_GET_CMD_EVENT(krequest.data) == TROOT_CMD_SYSTEM_ON_EXIT_REQ))
	//
	if ((reg.offset == 0x38) &&
		((reg.value & 0x000000ff) == 0x08)) {
		hl_dev->code_loaded = 0;
	}
#endif
	return 0;
}

static struct sprd_hdcp *alloc_hl_dev_slot(const struct sprd_hdcp_ioc_meminfo *info)
{
	int32_t i;

	if (info == 0)
		return 0;

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		struct sprd_hdcp *slot = &hdcps[i];
		if (slot->allocated && (info->hpi_base == slot->hpi_resource->start))
			return slot;
	}

	/* Find unused slot */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		struct sprd_hdcp *slot = &hdcps[i];
		if (!slot->allocated) {
			slot->allocated = 1;
			return slot;
		}
	}

	return 0;
}

static void free_dma_areas(struct sprd_hdcp *hl_dev)
{
	if (hl_dev == 0)
		return;

	if (!hl_dev->code_is_phys_mem && hl_dev->code) {
		dma_free_coherent(0, hl_dev->code_size, hl_dev->code, hl_dev->code_base);
		hl_dev->code = 0;
	}

	if (!hl_dev->data_is_phys_mem && hl_dev->data) {
		dma_free_coherent(0, hl_dev->data_size, hl_dev->data, hl_dev->data_base);
		hl_dev->data = 0;
	}

	DRM_INFO("%s() SUCCESS\n", __func__);
}

static int alloc_dma_areas(struct sprd_hdcp *hl_dev, const struct sprd_hdcp_ioc_meminfo *info)
{
	if ((hl_dev == 0) || (info == 0)) {
		return -EFAULT;
	}

	hl_dev->code_size = info->code_size;
	hl_dev->code_is_phys_mem = (info->code_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->code_is_phys_mem && (hl_dev->code == 0)) {
		/* TODO: support highmem */
		hl_dev->code_base = info->code_base;
		hl_dev->code = phys_to_virt(hl_dev->code_base);
	} else {
		hl_dev->code = dma_alloc_coherent(0, hl_dev->code_size,
										&hl_dev->code_base, GFP_KERNEL);
		if (!hl_dev->code) {
			return -ENOMEM;
		}
	}

	hl_dev->data_size = info->data_size;
	hl_dev->data_is_phys_mem = (info->data_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->data_is_phys_mem && (hl_dev->data == 0)) {
		hl_dev->data_base = info->data_base;
		hl_dev->data = phys_to_virt(hl_dev->data_base);
	} else {
		hl_dev->data = dma_alloc_coherent(0, hl_dev->data_size,
										&hl_dev->data_base, GFP_KERNEL);
		if (!hl_dev->data) {
			free_dma_areas(hl_dev);
			return -ENOMEM;
		}
	}

	DRM_INFO("%s() SUCCESS\n", __func__);

	return 0;
}

/* HL_DRV_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
	struct resource *hpi_mem;
	struct sprd_hdcp_ioc_meminfo info;
	struct sprd_hdcp *hl_dev;
	int rc;

	if ((f == 0) || (arg == 0))
		return -EFAULT;

	if (copy_from_user(&info, arg, sizeof info) != 0) {
		DRM_INFO("failed to copy data from user \n");
		return -EFAULT;
	}

	DRM_ERROR("allocate slot \n");
	hl_dev = alloc_hl_dev_slot(&info);
	if (!hl_dev)
		return -EMFILE;

	DRM_INFO("allocate dma areas \n");
	if (!hl_dev->initialized) {
		rc = alloc_dma_areas(hl_dev, &info);
		if (rc < 0)
			goto err_free;

		DRM_INFO("Request memory region for info.hpi_base = %x \n", info.hpi_base);
		hpi_mem = request_mem_region(info.hpi_base, 128, "hl_dev_hpi");
		if (!hpi_mem) {
			DRM_INFO("failed memory request fo hpi \n");
			rc = -EADDRNOTAVAIL;
			goto err_free;
		}

		DRM_INFO("allocate hpi memory \n");
		hl_dev->hpi = ioremap_nocache(hpi_mem->start, resource_size(hpi_mem));
		if (!hl_dev->hpi) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		hl_dev->hpi_resource = hpi_mem;
		DRM_INFO("allocate firmware \n");

		hl_dev->code_offset = (uint32_t *)ioremap_nocache(info.code_base, info.code_size);
		if (!hl_dev->code_offset) {
			rc = -ENOMEM;
			goto err_release_region;
		}

		DRM_INFO("allocate data \n");

		hl_dev->data_offset = ioremap_nocache(info.data_base, info.data_size);
		if (!hl_dev->data_offset) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		hl_dev->initialized = 1;
	}

	f->private_data = hl_dev;

	DRM_INFO("%s() success\n", __func__);
	return 0;

err_release_region:
	release_resource(hpi_mem);

err_free:
	free_dma_areas(hl_dev);
	hl_dev->initialized   = 0;
	hl_dev->allocated     = 0;
	hl_dev->hpi_resource  = 0;
	hl_dev->hpi           = 0;

	return rc;
}

static void free_hl_dev_slot(struct sprd_hdcp *slot)
{
	if (slot == 0)
		return;

	if (!slot->allocated)
		return;

	if (slot->initialized) {
		if (slot->hpi) {
			iounmap(slot->hpi);
			slot->hpi = 0;
		}

		if (slot->hpi_resource) {
			release_mem_region(slot->hpi_resource->start, 128);
			slot->hpi_resource = 0;
		}

		free_dma_areas(slot);
	}

	slot->initialized  = 0;
	slot->allocated    = 0;
}

static int sprd_hdcp_power_on(void)
{
	uint32_t reg_val;

	reg_val = HDCP_REG_RD(mem->pmu_ipa_sys_addr);
	reg_val &= ~BIT(25);
	HDCP_REG_WR(mem->pmu_ipa_sys_addr, reg_val);

	reg_val = HDCP_REG_RD(mem->ipa_eb_addr);
	reg_val |= BIT(8);
	HDCP_REG_WR(mem->ipa_eb_addr, reg_val);

	reg_val = HDCP_REG_RD(mem->aon2ipa_en_addr);
	reg_val |= BIT(0);
	HDCP_REG_WR(mem->aon2ipa_en_addr, reg_val);

	reg_val = HDCP_REG_RD(mem->clk_en_addr);
	reg_val |= (BIT(2) | BIT(6));
	HDCP_REG_WR(mem->clk_en_addr, reg_val);

	return 0;
}

static int sprd_hdcp_power_off(void)
{
	uint32_t reg_val;

	reg_val = HDCP_REG_RD(mem->ipa_eb_addr);
	reg_val &= ~BIT(8);
	HDCP_REG_WR(mem->ipa_eb_addr, reg_val);

	reg_val = HDCP_REG_RD(mem->aon2ipa_en_addr);
	reg_val &= ~BIT(0);
	HDCP_REG_WR(mem->aon2ipa_en_addr, reg_val);

	reg_val = HDCP_REG_RD(mem->clk_en_addr);
	reg_val &= ~(BIT(2) | BIT(6));
	HDCP_REG_WR(mem->clk_en_addr, reg_val);

	return 0;
}

static int sprd_hdcp_enable(void)
{
	uint32_t reg_val;

	sprd_hdcp_power_on();

	reg_val = HDCP_REG_RD(mem->dptx_hdcp_cfg_addr);
	reg_val |= BIT(1);
	HDCP_REG_WR(mem->dptx_hdcp_cfg_addr, reg_val);

	return 0;
}

static int sprd_hdcp_disable(void)
{
	uint32_t reg_val;
	int i;

	sprd_hdcp_power_off();

	reg_val = HDCP_REG_RD(mem->dptx_hdcp_cfg_addr);
	reg_val &= ~BIT(1);
	HDCP_REG_WR(mem->dptx_hdcp_cfg_addr, reg_val);

	for (i = 0; i < MAX_HL_DEVICES; i++)
		free_hl_dev_slot(&hdcps[i]);

	return 0;
}

static long sprd_hdcp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct sprd_hdcp *hl_dev;
	void __user *data;

	if (f == 0)
		return -EFAULT;

	hl_dev = f->private_data;
	if (arg)
		data = (void __user *)arg;

	if (cmd == HL_DRV_IOC_INIT) {
		return init(f, data);
	} else if (!hl_dev) {
		return -EAGAIN;
	}

	switch (cmd) {
	case HL_DRV_IOC_HDCP_ENABLE:
		return sprd_enable_hdcp13();
	case HL_DRV_IOC_ENABLE:
		return sprd_hdcp_enable();
	case HL_DRV_IOC_INIT:
		return init(f, data);
	case HL_DRV_IOC_MEMINFO:
		return get_meminfo(hl_dev, data);
	case HL_DRV_IOC_READ_HPI:
		return hpi_read(hl_dev, data);
	case HL_DRV_IOC_WRITE_HPI:
		return hpi_write(hl_dev, data);
	case HL_DRV_IOC_LOAD_CODE:
		return load_code(hl_dev, data);
	case HL_DRV_IOC_WRITE_DATA:
		return write_data(hl_dev, data);
	case HL_DRV_IOC_READ_DATA:
		return read_data(hl_dev, data);
	case HL_DRV_IOC_MEMSET_DATA:
		return set_data(hl_dev, data);
	case HL_DRV_IOC_DISABLE:
		return sprd_hdcp_disable();
	}

	return -ENOTTY;
}

static const struct file_operations sprd_hdcp_file_operations = {
	.unlocked_ioctl = sprd_hdcp_ioctl,
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sprd_hdcp_dev",
	.fops = &sprd_hdcp_file_operations,
};

static int sprd_hdcp_probe(struct platform_device *pdev)
{
	int i;

	mem = kzalloc(sizeof(struct hdcp_mem), GFP_KERNEL);

	for (i = 0; i < MAX_HL_DEVICES; i++) {
		hdcps[i].allocated    = 0;
		hdcps[i].initialized  = 0;
		hdcps[i].code_loaded  = 0;
		hdcps[i].code         = 0;
		hdcps[i].code_offset  = 0;
		hdcps[i].data         = 0;
		hdcps[i].hpi_resource = 0;
		hdcps[i].hpi          = 0;
	}

	mem->pmu_ipa_sys_addr = ioremap_nocache(PMU_IPA_SYS_REG, HDCP_REG_SIZE);
	mem->ipa_eb_addr = ioremap_nocache(IPA_EB_REG, HDCP_REG_SIZE);
	mem->aon2ipa_en_addr = ioremap_nocache(AON2IPA_EN_REG, HDCP_REG_SIZE);
	mem->clk_en_addr = ioremap_nocache(CLOCK_EN_REG, HDCP_REG_SIZE);
	mem->hdcp_base_addr = ioremap_nocache(HDCP_BASE_REG, HDCP_REG_NUM * HDCP_REG_SIZE);
	mem->dptx_hdcp_cfg_addr = ioremap_nocache(DPTX_HDCP_CFG_REG, HDCP_REG_SIZE);

	DRM_INFO("%s() SUCCESS\n", __func__);

	return misc_register(&hld_device);
}

static int sprd_hdcp_remove(struct platform_device *pdev)
{
	sprd_hdcp_disable();

	iounmap(mem->pmu_ipa_sys_addr);
	iounmap(mem->ipa_eb_addr);
	iounmap(mem->aon2ipa_en_addr);
	iounmap(mem->clk_en_addr);
	iounmap(mem->hdcp_base_addr);
	iounmap(mem->dptx_hdcp_cfg_addr);

	randomize_mem = false;
	misc_deregister(&hld_device);
	DRM_INFO("%s() SUCCESS\n", __func__);

	return 0;
}

static const struct of_device_id hdcp_match_table[] = {
	{ .compatible = "sprd,dp-hdcp",},
	{},
};

static struct platform_driver sprd_hdcp_driver = {
	.probe = sprd_hdcp_probe,
	.remove = sprd_hdcp_remove,
	.driver = {
		.name = "sprd-hdcp-drv",
		.of_match_table = hdcp_match_table,
	},
};
module_platform_driver(sprd_hdcp_driver);

MODULE_AUTHOR("Pony Wu <pony.wu@unisoc.com>");
MODULE_DESCRIPTION("Unisoc HDCP Controller Driver");
MODULE_LICENSE("GPL v2");
