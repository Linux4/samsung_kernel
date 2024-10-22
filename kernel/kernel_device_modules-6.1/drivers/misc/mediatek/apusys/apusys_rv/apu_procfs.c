// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif

#include "apu.h"
#include "apu_regdump.h"
#include "apu_ce_excep.h"

#define PT_MAGIC (0x58901690)
#define PROC_WRITE_TEMP_BUFF_SIZE (16)

static struct platform_device *g_apu_pdev;
static struct proc_dir_entry *procfs_root;
static size_t coredump_len, xfile_len;
static void *coredump_base, *xfile_base;

static int coredump_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, coredump_base, coredump_len);

	return 0;
}

static int xfile_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, xfile_base, xfile_len);

	return 0;
}

static int regdump_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = NULL;
	const struct apusys_regdump_info *info = NULL;
	const struct apusys_regdump_region_info *region_info = NULL;
	void *base_va = NULL;
	uint32_t region_info_num = 0;
	unsigned int region_offset = 0;
	int i;

	apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);

	base_va = apu->apu_aee_coredump_mem_base +
		apu->apusys_aee_coredump_info->regdump_ofs;
	info = (struct apusys_regdump_info *) base_va;

	if (info == NULL) {
		dev_info(&g_apu_pdev->dev, "%s: apusys_regdump_info == NULL\n",
			__func__);
		return 0;
	}

	if (info->region_info_num == 0) {
		dev_info(&g_apu_pdev->dev, "%s: region_info_num == 0\n",
			__func__);
		return 0;
	}

	region_info = info->region_info;
	region_info_num = info->region_info_num;

	for (i = 0; i < region_info_num; i++) {
		seq_printf(s, "---- dump %s from 0x%x to 0x%x ----\n",
			region_info[i].name, region_info[i].start,
			region_info[i].start + region_info[i].size);
		seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4,
			(void *)(base_va + info->size + region_offset),
			region_info[i].size, false);
		region_offset += region_info[i].size;
	}

	return 0;
}

static ssize_t dump_ce_fw_sram_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char tmp[PROC_WRITE_TEMP_BUFF_SIZE] = {0};
	int ret;
	unsigned int input = 0;
	struct mtk_apu *apu = (struct mtk_apu *)platform_get_drvdata(g_apu_pdev);

	if (count >= PROC_WRITE_TEMP_BUFF_SIZE - 1)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: copy_from_user failed (%d)\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, PROC_WRITE_TEMP_BUFF_SIZE, &input);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: kstrtouint failed (%d)\n", __func__, ret);
		goto out;
	}

	dev_info(&g_apu_pdev->dev, "%s: dump ops (0x%x)\n", __func__, input);

	if (input & (0x1)) {
		if (apu_ce_reg_dump(apu) == 0)
			dev_info(&g_apu_pdev->dev, "%s: dump ce register success\n", __func__);
		else
			dev_info(&g_apu_pdev->dev, "%s: dump ce register smc call fail\n", __func__);
	}
	if (input & (0x2)) {
		if (apu_ce_sram_dump(apu) == 0)
			dev_info(&g_apu_pdev->dev, "%s: dump are sram success\n", __func__);
		else
			dev_info(&g_apu_pdev->dev, "%s: dump are sram call fail\n", __func__);
	}
out:
	return count;
}

static int ce_fw_sram_show(struct seq_file *s, void *v)
{
	uint32_t *ce_reg_addr = NULL;
	uint32_t *ce_sram_addr = NULL;
	uint32_t start, end, size, offset = 0;
	struct mtk_apu *apu = (struct mtk_apu *)platform_get_drvdata(g_apu_pdev);

	ce_reg_addr = (uint32_t *)((unsigned long)apu->apu_aee_coredump_mem_base +
		(unsigned long)apu->apusys_aee_coredump_info->ce_bin_ofs);

	while (ce_reg_addr[offset++] == CE_REG_DUMP_MAGIC_NUM) {
		start = ce_reg_addr[offset++];
		end = ce_reg_addr[offset++];
		size = end - start;

		seq_printf(s, "---- dump ce register from 0x%08x to 0x%08x ----\n",
			start, end - 4);

		seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4, ce_reg_addr + offset, size, false);
		offset += size / 4;
	}

	seq_printf(s, "---- dump ce sram start from 0x%08x ----\n", APU_ARE_SRAMBASE);
	ce_sram_addr = (uint32_t *)((unsigned long)apu->apu_aee_coredump_mem_base +
		(unsigned long)apu->apusys_aee_coredump_info->are_sram_ofs);
	size = apu->apusys_aee_coredump_info->are_sram_sz;

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 16, 4, ce_sram_addr, size, false);

	return 0;
}

static int coredump_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, coredump_seq_show, NULL);
}

static int xfile_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, xfile_seq_show, NULL);
}

static int regdump_sqopen(struct inode *inode, struct file *file)
{
	bool is_do_regdump = false;

	if (is_do_regdump)
		apu_regdump();

	return single_open(file, regdump_seq_show, NULL);
}

static int ce_fw_sram_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, ce_fw_sram_show, NULL);
}

static ssize_t debug_ctrl_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char tmp[PROC_WRITE_TEMP_BUFF_SIZE] = {0};
	int ret;
	unsigned int input = 0;
	struct mtk_apu *apu = (struct mtk_apu *)platform_get_drvdata(g_apu_pdev);

	if (count >= PROC_WRITE_TEMP_BUFF_SIZE - 1)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: copy_from_user failed (%d)\n", __func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, PROC_WRITE_TEMP_BUFF_SIZE, &input);
	if (ret) {
		dev_info(&g_apu_pdev->dev, "%s: kstrtouint failed (%d)\n", __func__, ret);
		goto out;
	}

	dev_info(&g_apu_pdev->dev, "%s: user input (0x%x)\n", __func__, input);

	if (input & 0x1)
		apu->disable_ke = true;
	else
		apu->disable_ke = false;
out:
	return count;
}

static int debug_ctrl_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = (struct mtk_apu *)platform_get_drvdata(g_apu_pdev);

	seq_printf(s, "disable_ke: 0x%x\n", apu->disable_ke);
	return 0;
}

static int debug_ctrl_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, debug_ctrl_seq_show, NULL);
}

static int debug_info_dump_seq_show(struct seq_file *s, void *v)
{
	struct mtk_apu *apu = NULL;
	struct mtk_apu_hw_ops *hw_ops;

	apu = (struct mtk_apu *) platform_get_drvdata(g_apu_pdev);
	if (!apu)
		return -EINVAL;
	hw_ops = &apu->platdata->ops;

	if (hw_ops->debug_info_dump)
		hw_ops->debug_info_dump(apu, s);
	else
		seq_puts(s, "debug_info_dump not support\n");

	return 0;
}

static int debug_info_dump_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, debug_info_dump_seq_show, NULL);
}

static const struct proc_ops coredump_file_ops = {
	.proc_open		= coredump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops xfile_file_ops = {
	.proc_open		= xfile_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops regdump_file_ops = {
	.proc_open		= regdump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops ce_fw_sram_file_ops = {
	.proc_open		= ce_fw_sram_sqopen,
	.proc_write     = dump_ce_fw_sram_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops debug_info_dump_file_ops = {
	.proc_open		= debug_info_dump_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static const struct proc_ops debug_ctrl_file_ops = {
	.proc_open		= debug_ctrl_sqopen,
	.proc_write		= debug_ctrl_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
static void apu_mrdump_register(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;
	const struct apusys_regdump_info *info = NULL;
	unsigned long base_va = 0;
	unsigned long base_pa = 0;
	unsigned long size = 0;
	uint32_t coredump_size = apu->up_code_buf_sz + REG_SIZE +
		TBUF_SIZE + CACHE_DUMP_SIZE;

	if (apu->platdata->flags & F_SECURE_COREDUMP) {
		base_pa = apu->apusys_aee_coredump_mem_start +
			apu->apusys_aee_coredump_info->up_coredump_ofs;
		base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_coredump_ofs;
		size = coredump_size;
	} else {
		base_pa = apu->coredump_buf_pa;
		base_va = (unsigned long) apu->coredump_buf;
		size = coredump_size;

		dev_info(dev, "%s: base_va = 0x%lx, base_pa = 0x%lx\n",
			__func__, base_va, base_pa);
	}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
		"APUSYS_COREDUMP");
#endif
	if (ret)
		dev_info(dev, "%s: APUSYS_COREDUMP add fail(%d)\n",
			__func__, ret);
	coredump_len = (size_t) size;
	coredump_base = (void *) base_va;

	if (apu->platdata->flags & F_PRELOAD_FIRMWARE) {
		base_pa = apu->apusys_aee_coredump_mem_start +
			apu->apusys_aee_coredump_info->up_xfile_ofs;
		base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
			apu->apusys_aee_coredump_info->up_xfile_ofs;
		if (ioread32((void *) base_va) != PT_MAGIC) {
			dev_info(dev, "%s: reserve memory corrupted!\n", __func__);
			size = 0;
		} else {
			size = apu->apusys_aee_coredump_info->up_xfile_sz;
			dev_info(dev, "%s: up_xfile_sz = 0x%lx\n", __func__, size);
		}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
			"APUSYS_RV_XFILE");
#endif
		if (ret)
			dev_info(dev, "%s: APUSYS_RV_XFILE add fail(%d)\n",
				__func__, ret);
	} else {
		/* not F_PRELOAD_FIRMWARE just return */
		return;
	}

	xfile_len = (size_t) size;
	xfile_base = (void *) base_va;

	base_pa = apu->apusys_aee_coredump_mem_start +
		apu->apusys_aee_coredump_info->regdump_ofs;
	base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
		apu->apusys_aee_coredump_info->regdump_ofs;
	info = (struct apusys_regdump_info *) base_va;
	size = apu->apusys_aee_coredump_info->regdump_sz;

	if (info != NULL) {
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		ret = mrdump_mini_add_extra_file(base_va, base_pa, size,
			"APUSYS_REGDUMP");
#endif
		if (ret)
			dev_info(dev, "%s: APUSYS_REGDUMP add fail(%d)\n",
				__func__, ret);
	}

	if (apu->platdata->flags & F_CE_EXCEPTION_ON) {

		//CE FW + CE sram start addr & total size
		base_pa = apu->apusys_aee_coredump_mem_start +
				apu->apusys_aee_coredump_info->ce_bin_ofs;
		base_va = (unsigned long) apu->apu_aee_coredump_mem_base +
				apu->apusys_aee_coredump_info->ce_bin_ofs;

		size = apu->apusys_aee_coredump_info->ce_bin_sz +
			apu->apusys_aee_coredump_info->are_sram_sz;
		dev_info(dev, "%s: ce_bin_sz = 0x%x, are_sram_sz = 0x%x\n", __func__,
			apu->apusys_aee_coredump_info->ce_bin_sz,
			apu->apusys_aee_coredump_info->are_sram_sz);
	}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	ret = mrdump_mini_add_extra_file(base_va, base_pa, size, "APUSYS_CE_FW_SRAM");
#endif
	if (ret)
		dev_info(dev, "%s: APUSYS_CE_FW_SRAM add fail(%d)\n",
			__func__, ret);

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	ret = mrdump_mini_add_extra_file((unsigned long)apu, __pa_nodebug(apu),
		sizeof(struct mtk_apu), "APUSYS_RV_INFO");
	if (ret)
		dev_info(dev, "%s: APUSYS_RV_DEBUG_INFO_DUMP add fail(%d)\n",
			__func__, ret);
#endif
}
#else
static void apu_mrdump_register(struct mtk_apu *apu)
{
}
#endif

int apu_procfs_init(struct platform_device *pdev)
{
	int ret;
	struct proc_dir_entry *coredump_seqlog;
	struct proc_dir_entry *xfile_seqlog;
	struct proc_dir_entry *regdump_seqlog;
	struct proc_dir_entry *ce_fw_sram_seqlog;
	struct proc_dir_entry *debug_ctrl_seqlog;
	struct proc_dir_entry *debug_info_dump_seqlog;

	struct mtk_apu *apu = (struct mtk_apu *) platform_get_drvdata(pdev);

	g_apu_pdev = pdev;

	procfs_root = proc_mkdir("apusys_rv", NULL);
	ret = IS_ERR_OR_NULL(procfs_root);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv dir\n", ret);
		goto out;
	}

	coredump_seqlog = proc_create("apusys_rv_coredump", 0440,
		procfs_root, &coredump_file_ops);
	ret = IS_ERR_OR_NULL(coredump_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_rv_coredump)\n",
			ret);
		goto out;
	}

	xfile_seqlog = proc_create("apusys_rv_xfile", 0440,
		procfs_root, &xfile_file_ops);
	ret = IS_ERR_OR_NULL(xfile_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_rv_xfile)\n",
			ret);
		goto out;
	}

	regdump_seqlog = proc_create("apusys_regdump", 0440,
		procfs_root, &regdump_file_ops);
	ret = IS_ERR_OR_NULL(regdump_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_regdump)\n",
			ret);
		goto out;
	}

	if (apu->platdata->flags & F_CE_EXCEPTION_ON) {

		ce_fw_sram_seqlog = proc_create("apusys_ce_fw_sram", 0444,
			procfs_root, &ce_fw_sram_file_ops);
		ret = IS_ERR_OR_NULL(ce_fw_sram_seqlog);
		if (ret) {
			dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_ce_fw_sram)\n",
				ret);
			goto out;
		}
	}

	debug_ctrl_seqlog = proc_create("apusys_debug_ctrl", 0444,
		procfs_root, &debug_ctrl_file_ops);
	ret = IS_ERR_OR_NULL(debug_ctrl_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_debug_ctrl)\n",
			ret);
		goto out;
	}

	apu_regdump_init(pdev);

	debug_info_dump_seqlog = proc_create("apusys_rv_debug_info_dump", 0440,
		procfs_root, &debug_info_dump_file_ops);
	ret = IS_ERR_OR_NULL(debug_info_dump_seqlog);
	if (ret) {
		dev_info(&pdev->dev, "(%d)failed to create apusys_rv node(apusys_rv_debug_info_dump)\n",
			ret);
		goto out;
	}

	apu_mrdump_register(apu);
out:
	return ret;
}

void apu_procfs_remove(struct platform_device *pdev)
{
	remove_proc_entry("apusys_ce_fw_sram", procfs_root);
	remove_proc_entry("apusys_debug_ctrl", procfs_root);
	remove_proc_entry("apusys_rv_debug_info_dump", procfs_root);
	remove_proc_entry("apusys_regdump", procfs_root);
	remove_proc_entry("apusys_rv_xfile", procfs_root);
	remove_proc_entry("apusys_rv_coredump", procfs_root);
	remove_proc_entry("apusys_rv", NULL);
}
