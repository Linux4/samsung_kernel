// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series uNPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "npu-clock.h"
#include "npu-config.h"
#include "npu-util-regs.h"
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pmu-if.h>

#define DEBUG_FS_UNPU_ROOT_NAME "unpu"

#define DEBUG_FS_UNPU_FW_UT_WRITE_NAME "load_fw_ut"
#define DEBUG_FS_UNPU_FW_WRITE_NAME "load_fw"
#define DEBUG_FS_UNPU_LOG_NAME "fw-report"
#define DEBUG_FS_UNPU_BOOT_NAME "boot_fw"
#define DEBUG_FS_UNPU_SREG "access_sram"

#define UNPU_REMAP_DEST_RESET 0x20200000
#define UNPU_REMAP_SRC_RESET 0x00080000
#define UNPU_FW_LOG_OFFSET 0x80000
#define UNPU_FW_UT_OFFSET 0x40000
#define UNPU_REMAP_DEST_START 0x8
#define UNPU_REMAP_SRC_END 0x4
#define UNPU_CPU_PC 0x4c

#define UNPU_FW_UT_SIZE 0x40000
#define UNPU_FW_LOG_SIZE 0x4000
#define UNPU_MAX_PATH_SIZE 128
#define UNPU_FW_SIZE 0x40000
#define ACCESS_4BYTE 0x4

const char * const reg_names[UNPU_MAX_IOMEM] = {"unpu_ctrl", "yamin_ctrl", "unpu_sram"};

/* Singleton reference to debug object */
static struct unpu_debug *unpu_debug_ref;

#define UNPU_FW_UT_NAME "uNPU_UT.bin"
#define UNPU_FW_NAME "uNPU.bin"
typedef enum {
	FS_READY = 0,
} unpu_debug_state_bits_e;

static int __validate_user_file(const struct firmware *blob, uint32_t size)
{
	int ret = 0;

	if (unlikely(!blob))
	{
		npu_err("NULL in blob\n");
		return -EINVAL;
	}

	if (unlikely(!blob->data))
	{
		npu_err("NULL in blob->data\n");
		return -EINVAL;
	}

	if (blob->size > size)
	{
		npu_err("blob is too big (%ld > %u)\n", blob->size, size);
		return -EIO;
	}

	return ret;
}

static int __load_user_file(const struct firmware **blob, const char *path, struct device *dev, uint32_t size)
{
	int ret = 0;

	// get binary from userspace
	ret = request_firmware(blob, path, dev);
	if (ret)
	{
		npu_err("fail(%d) in request_firmware_direct: %s\n", ret, path);
		return ret;
	}

	ret = __validate_user_file(*blob, size);
	if (ret)
	{
		npu_err("fail(%d) to validate user file: %s\n", ret, path);
		release_firmware(*blob);
		*blob = NULL;
		return ret;
	}

	return ret;
}

static int __load_image_to_io(struct device *dev, struct unpu_iomem_data *mem,
			      char *name, unsigned int offset, size_t size)
{
	const struct firmware *blob = NULL;
	int ret = 0, iter = 0, i;
	phys_addr_t sram_offset;
	char path[100] = "";
	volatile u32 v;

	strcat(path, name);
	sram_offset = mem->paddr + offset;

	ret = __load_user_file(&blob, path, dev, size);
	if (ret)
	{
		npu_err("fail(%d) to load user file: %s\n", ret, path);
		goto exit;
	}

	iter = (blob->size) / ACCESS_4BYTE;
	for (i = 0; i < iter; i++) {
		v = readl((volatile void *)(blob->data + i * ACCESS_4BYTE));
		ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)(sram_offset + i * ACCESS_4BYTE)), v, 0x0);
	}
	mb();
	if (blob)
		release_firmware(blob);

exit:
	return ret;
}

static inline void set_state_bit(unpu_debug_state_bits_e state_bit)
{
	int old = test_and_set_bit(state_bit, &(unpu_debug_ref->state));

	if (old)
		npu_warn("state(%d): state set requested but already set.", state_bit);
}

static inline void clear_state_bit(unpu_debug_state_bits_e state_bit)
{
	int old = test_and_clear_bit(state_bit, &(unpu_debug_ref->state));

	if (!old)
		npu_warn("state(%d): state clear requested but already cleared.", state_bit);
}

static inline int check_state_bit(unpu_debug_state_bits_e state_bit)
{
	return test_bit(state_bit, &(unpu_debug_ref->state));
}

static int unpu_fw_load_show(struct seq_file *file, void *unused)
{
	seq_printf(file, "echo 1 > /d/unpu/load_fw to load firmware\n");

	return 0;
}

static int unpu_fw_load_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_fw_load_show,
			inode->i_private);
}

static ssize_t unpu_fw_load_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *mem;
	int ret = 0, fw_load;
	char buf[30];
	ssize_t size;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	mem = &unpu_device->iomem_data[UNPU_SRAM];

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &fw_load);
	if (ret) {
		npu_err("Failed to get fw_load parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	isb();
	msleep(2);
	if (fw_load) {
		npu_dbg("firmware load: vaddr: 0x%p paddr: 0x%x size: 0x%x\n",
			(mem)->vaddr, (mem)->paddr, (mem)->size);
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
		ret = unpu_firmware_file_read_signature(unpu_device);
		if (ret) {
			npu_err("error(%d) in npu_firmware_file_read_signature\n", ret);
			goto p_err;
		}
#else
		ret = __load_image_to_io(unpu_device->dev, mem, UNPU_FW_NAME, 0,
					 UNPU_FW_SIZE);
		if (ret) {
			npu_err("error(%d) in loading firmware.\n", ret);
			goto p_err;
		}
#endif
	}
	mb();

	npu_dbg("input value is %d\n", fw_load);
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	imgloader_shutdown(&unpu_device->imgloader);
#endif
p_err:
	return ret;
}

static const struct file_operations unpu_fw_load_fops = {
	.open		= unpu_fw_load_open,
	.read		= seq_read,
	.write		= unpu_fw_load_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int unpu_fw_ut_load_show(struct seq_file *file, void *unused)
{
	seq_printf(file, "echo 1 > /d/unpu/load_fw_ut to load UT\n");

	return 0;
}

static int unpu_fw_ut_load_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_fw_ut_load_show,
			inode->i_private);
}

static ssize_t unpu_fw_ut_load_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *mem;
	int load_fw_ut;
	ssize_t size;
	char buf[30];
	int ret = 0;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	mem = &unpu_device->iomem_data[UNPU_SRAM];

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &load_fw_ut);
	if (ret) {
		npu_err("Failed to get fw_ut parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}


	isb();
	msleep(2);
	if (load_fw_ut) {
		ret = __load_image_to_io(unpu_device->dev, mem, UNPU_FW_UT_NAME,
					 UNPU_FW_UT_OFFSET, UNPU_FW_UT_SIZE);
		if (ret) {
			npu_err("loading firmware ut failed(%d)\n", ret);
			goto p_err;
		}
	}

	mb();
	npu_dbg("load firmware ut: vaddr: 0x%p paddr: 0x%#llx size: 0x%x\n", (mem)->vaddr,
		((mem)->paddr + UNPU_FW_UT_OFFSET), UNPU_FW_UT_SIZE);
	npu_dbg("value is %d\n", load_fw_ut);
p_err:
	return ret;
}

static const struct file_operations unpu_fw_ut_load_fops = {
	.open		= unpu_fw_ut_load_open,
	.read		= seq_read,
	.write		= unpu_fw_ut_load_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int unpu_boot_show(struct seq_file *file, void *unused)
{
	seq_printf(file, "echo 1 > /d/unpu/boot_fw to boot fw\n");

	return 0;
}

static int unpu_boot_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_boot_show,
			inode->i_private);
}

static ssize_t unpu_boot_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct unpu_iomem_data *unpu_ctrl, *yamin_ctrl;
	struct unpu_device *unpu;
	int ret = 0, i = 0, val;
	unsigned long v_tmp;
	char buf[30];
	ssize_t size;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);
	unpu_ctrl = &unpu->iomem_data[UNPU_CTRL];
	yamin_ctrl = &unpu->iomem_data[YAMIN_CTRL];

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		npu_err("Failed to get unpu_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr + UNPU_REMAP_DEST_START),
		   UNPU_REMAP_DEST_RESET, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr + UNPU_REMAP_SRC_END),
		   UNPU_REMAP_SRC_RESET, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr), 0x1, 0x0);

	exynos_smc_readsfr(yamin_ctrl->paddr + UNPU_CPU_PC, &v_tmp);
	npu_dbg("cpupc val is 0x%#x\n", (unsigned int)v_tmp);

	if (val) {
		ret = exynos_pmu_write(unpu->pmu_offset, 0x1);
		if (ret) {
			npu_err("pmu write is not succesful.\n");
			goto p_err;
		}
	}

	for (i = 0; i < 300; i++) {
		mdelay(1);
		if (!(i % 100)) {
			exynos_smc_readsfr(yamin_ctrl->paddr + UNPU_CPU_PC, &v_tmp);
			npu_dbg("cpupc val is %#x\n", (unsigned int)v_tmp);
		}
	}
p_err:
	return ret;
}

static const struct file_operations unpu_boot_fops = {
	.open		= unpu_boot_open,
	.read		= seq_read,
	.write		= unpu_boot_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int unpu_fw_report_show(struct seq_file *file, void *unused)
{

	seq_printf(file, "unpu_fw_report.\n");

	return 0;
}

static int unpu_fw_report_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_fw_report_show,
			inode->i_private);
}

static ssize_t unpu_fw_report_read(struct file *file, char __user *outbuf,
				       size_t outlen, loff_t *loff)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *mem;
	u32 log_size, iter, ret = 0;
	phys_addr_t sram_offset;
	char *tmp_buf;
	int i = 0;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	mem = &unpu_device->iomem_data[UNPU_SRAM];
	if (!mem) {
		pr_info("Could not find the mem for uNPU SRAM log area\n");
		return 0;
	}
	log_size = UNPU_FW_LOG_SIZE;
	sram_offset = mem->paddr + UNPU_FW_LOG_OFFSET;

	tmp_buf = kzalloc(log_size, GFP_KERNEL);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto err_exit;
	}

	mb();
	npu_dbg("log_size = 0x%x\n", log_size);

	iter = log_size/ACCESS_4BYTE;
	for (i = 0; i < (iter-1); i++)
		ret = exynos_smc_readsfr((int)(sram_offset + (i * ACCESS_4BYTE)),
					 (unsigned long *)(tmp_buf + (i * ACCESS_4BYTE)));

        mb();

	ret = copy_to_user(outbuf, tmp_buf, outlen);
	if (ret) {
		npu_err("copy_to_user failed : 0x%x\n", ret);
		ret = -EFAULT;
		goto err_exit;
	}

	ret = outlen;
err_exit:
	if (tmp_buf)
		kfree(tmp_buf);
	return ret;
}

static const struct file_operations unpu_fw_report_fops = {
	.open		= unpu_fw_report_open,
	.read		= unpu_fw_report_read,
	.release	= single_release
};

static u32 tmp_reg_addr = 0x151104C;
static int sreg_show(struct seq_file *file, void *unused)
{
	unsigned long value;

	seq_printf(file, "echo 0x<address> > /d/unpu/access_sram\n");
	exynos_smc_readsfr((unsigned long)tmp_reg_addr, &value);
	seq_printf(file, "address = %08x, value: %#lx\n", tmp_reg_addr, value);

	return 0;
}

static int unpu_sreg_read_open(struct inode *inode, struct file *file)
{
	return single_open(file, sreg_show,
			inode->i_private);
}
static ssize_t unpu_sreg_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[30];
	ssize_t size;
	u32 address;
	int ret;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "0x%xd", &address);
	if (ret != 1)
	{
		pr_err("fail(%d) in parsing sreg address & value\n", ret);
		return -EINVAL;
	}
	tmp_reg_addr = address;

	npu_dbg("address = %08x\n", address);

p_err:
	return ret;
}

static const struct file_operations unpu_sreg_fops = {
	.open		= unpu_sreg_read_open,
	.read		= seq_read,
	.write		= unpu_sreg_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

int unpu_debug_register_arg(const char *name, void *private_arg, const struct file_operations *ops)
{
	struct unpu_device	*unpu_device;
	struct dentry *dbgfs_entry;
	struct device *dev;
	mode_t mode = 0;
	int ret	= 0;

	/* Check whether the debugfs is properly initialized */
	if (!check_state_bit(FS_READY)) {
		probe_warn("DebugFS not initialized or disabled. Skip creation [%s]\n", name);
		ret = 0;
		goto err_exit;
	}

	/* Default parameter is npu_debug object */
	if (private_arg == NULL)
		private_arg = (void *)unpu_debug_ref;

	/* Retrieve struct device to use devm_XX api */
	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	BUG_ON(!unpu_device);
	dev = unpu_device->dev;
	BUG_ON(!dev);

	if (name == NULL) {
		npu_err("name is null\n");
		ret = -EFAULT;
		goto err_exit;
	}
	if (ops == NULL) {
		npu_err("ops is null\n");
		ret = -EFAULT;
		goto err_exit;
	}

	/* Setting file permission based on file_operation member */
	if (ops->read || ops->compat_ioctl || ops->unlocked_ioctl)
		mode |= 0400;	/* Read permission to owner */

	if (ops->write)
		mode |= 0200;	/* Write permission to owner */

	/* Register */
	dbgfs_entry = debugfs_create_file(name, mode, unpu_debug_ref->dfile_root, private_arg, ops);
	if (IS_ERR(dbgfs_entry)) {
		probe_err("fail in unpu DebugFS registration (%s)\n", name);
		ret = PTR_ERR(dbgfs_entry);
		goto err_exit;
	}
	probe_info("success in unpu DebugFS registration (%s) : Mode %04o\n", name, mode);
	return 0;

err_exit:
	return ret;
}

int unpu_debug_register(const char *name, const struct file_operations *ops)
{
	int ret = 0, debugfs_enable = 0;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_enable = 1;
	ret = unpu_debug_register_arg(name, NULL, ops);
	if (ret)
		probe_err("failed (%d)", ret);
#endif

	probe_info("register %s debugfs_state in unpu %s", name,
			   debugfs_enable ? "enabled" : "disabled");
	return ret;
}

int unpu_debug_probe(struct unpu_device *unpu)
{
	int ret = 0;

	BUG_ON(!unpu);

	/* Save reference */
	unpu_debug_ref = &unpu->debug;
	memset(unpu_debug_ref, 0, sizeof(*unpu_debug_ref));

	unpu_debug_ref->dfile_root = debugfs_create_dir(DEBUG_FS_UNPU_ROOT_NAME, NULL);
	if (!unpu_debug_ref->dfile_root) {
		dev_err(unpu->dev, "Loading npu_debug : debugfs root [%s] can not be created\n",
				DEBUG_FS_UNPU_ROOT_NAME);

		unpu_debug_ref->dfile_root = NULL;
		ret = -ENOENT;
		goto err_exit;
	}

	dev_dbg(unpu->dev, "Loading unpu_debug : debugfs root [%s] created\n" ,
			DEBUG_FS_UNPU_ROOT_NAME);
	set_state_bit(FS_READY);

	ret = unpu_debug_register(DEBUG_FS_UNPU_FW_WRITE_NAME, &unpu_fw_load_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs for loading fw can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_FW_UT_WRITE_NAME, &unpu_fw_ut_load_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs for loading fw ut can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_BOOT_NAME, &unpu_boot_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs for booting fw can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_LOG_NAME, &unpu_fw_report_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs for getting unpu fw-report can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_SREG, &unpu_sreg_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for sreg access can not be created(%d)\n", ret);
		goto err_exit;
	}

	return 0;

err_exit:
	return ret;
}

static int unpu_get_iomem_from_dt(struct platform_device *pdev, const char *name,
								  struct unpu_iomem_data *data)
{
	struct resource *res;

	if (!pdev || !data | !name)
		return -EINVAL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		dev_err(&pdev->dev, "REG name %s not defined for UNPU\n", name);
		return 0;
	}

	data->vaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->vaddr)) {
		dev_err(&pdev->dev, "IO remap failed for region %s\n", name);
		return -EINVAL;
	}
	data->paddr = res->start;
	data->size = resource_size(res);

	dev_info(&pdev->dev, "UNPU IOMEM: name: %s vaddr: 0x%p paddr: 0x%#llx size: 0x%x\n",
			 name, data->vaddr, data->paddr, (unsigned int)data->size);

	return 0;
}

static int unpu_mem_probe(struct platform_device *pdev, struct unpu_device *unpu)
{
	struct device *dev = &pdev->dev;
	u32 pmu_offset;
	int i, ret;

	/* getting offset from  dt */
	ret = of_property_read_u32(dev->of_node, "samsung,unpu-pmu-offset", &pmu_offset);
	if (ret) {
		npu_err("parsing pmu-offset from dt failed.\n");
		return ret;
	}
	unpu->pmu_offset = pmu_offset;

	for (i = 0 ; i < UNPU_MAX_IOMEM; i++) {
		ret = unpu_get_iomem_from_dt(pdev, reg_names[i], &unpu->iomem_data[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int unpu_debug_release(void)
{
	struct unpu_device *unpu_device;
	struct device *dev;
	int ret = 0;

	if (!check_state_bit(FS_READY)) {
		/* No need to clean-up */
		probe_err("not ready in npu_debug\n");
		return -1;
	}

	/* Retrieve struct device to use devm_XX api */
	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	dev = unpu_device->dev;

	/* Remove debug fs entries */
	debugfs_remove_recursive(unpu_debug_ref->dfile_root);
	probe_info("unloading unpu_debug: root node is removed.\n");

	clear_state_bit(FS_READY);
	return ret;
}


static int unpu_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct unpu_device *unpu;
	int ret;

	unpu = devm_kzalloc(dev, sizeof(*unpu), GFP_KERNEL);
	if (!unpu) {
		dev_err(dev, "fail in devm_kzalloc\n");
		return -ENOMEM;
	}
	unpu->dev = dev;

	ret = unpu_mem_probe(pdev, unpu);
	if (ret) {
		dev_err(dev, "unpu_mem_probe failed. ret = %d\n", ret);
		return ret;
	}

	ret = unpu_debug_probe(unpu);
	if (ret) {
		dev_err(dev, "fail(%d) in unpu_debug_probe\n", ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	ret = unpu_imgloader_probe(unpu, dev);
	if (ret) {
		dev_err(dev, "unpu_imgloader_probe is fail(%d)\n", ret);
		goto clean_debug_probe;
	}
#endif
	dev_set_drvdata(dev, unpu);

	ret = 0;
	dev_info(dev, "unpu probe is successful \n");

	return ret;

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
clean_debug_probe:
	unpu_debug_release();
#endif

	return ret;
}

static int unpu_device_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = unpu_debug_release();
	if (ret)
		probe_err("fail(%d) in unpu_debug_release\n", ret);

	probe_info("completed in %s\n", __func__);
	return ret;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos_unpu_match[] = {
	{
		.compatible = "samsung,exynos-unpu"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_unpu_match);
#endif

static struct platform_driver unpu_driver = {
	.probe	= unpu_device_probe,
	.remove = unpu_device_remove,
	.driver = {
		.name	= "exynos-unpu",
		.owner	= THIS_MODULE,
//		.pm	= &unpu_pm_ops,
		.of_match_table = of_match_ptr(exynos_unpu_match),
	},
}
;

int __init unpu_register(void)
{
	int ret = 0;

	ret = platform_driver_register(&unpu_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		return ret;
	}

	return ret;
}

