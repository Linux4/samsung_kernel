/* sound/soc/samsung/abox/abox_dbg.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Debug driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/mm_types.h>
#include <asm/cacheflush.h>

#include "abox_util.h"
#include "abox_gic.h"
#include "abox_core.h"
#include "abox_dbg.h"

#define ABOX_DBG_DUMP_MAGIC_SRAM	0x3935303030504D44ull /* DMP00059 */
#define ABOX_DBG_DUMP_MAGIC_DRAM	0x3231303038504D44ull /* DMP80012 */
#define ABOX_DBG_DUMP_MAGIC_LOG		0x3142303038504D44ull /* DMP800B1 */
#define ABOX_DBG_DUMP_MAGIC_SFR		0x5246533030504D44ull /* DMP00SFR */
#define ABOX_DBG_DUMP_LIMIT_NS		(5 * NSEC_PER_SEC)

static struct dentry *abox_dbg_root_dir __read_mostly;

struct dentry *abox_dbg_get_root_dir(void)
{
	pr_debug("%s\n", __func__);

	if (abox_dbg_root_dir == NULL)
		abox_dbg_root_dir = debugfs_create_dir("abox", NULL);

	return abox_dbg_root_dir;
}

static void abox_dbg_remove_root_dir(void)
{
	debugfs_remove_recursive(abox_dbg_root_dir);
}

void abox_dbg_print_gpr_from_addr(struct device *dev, struct abox_data *data,
		unsigned int *addr)
{
	abox_core_print_gpr_dump(addr);
}

void abox_dbg_print_gpr(struct device *dev, struct abox_data *data)
{
	abox_core_print_gpr();
}

struct abox_dbg_dump_sram {
	unsigned long long magic;
	char dump[SZ_512K];
} __packed;

struct abox_dbg_dump_dram {
	unsigned long long magic;
	char dump[DRAM_FIRMWARE_SIZE];
} __packed;

struct abox_dbg_dump_log {
	unsigned long long magic;
	char dump[ABOX_LOG_SIZE];
} __packed;

struct abox_dbg_dump_sfr {
	unsigned long long magic;
	u32 dump[SZ_64K / sizeof(u32)];
} __packed;

struct abox_dbg_dump {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_dram dram;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[SZ_128];
	long long time;
	char reason[SZ_32];
	unsigned int previous;
} __packed;

struct abox_dbg_dump_min {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_dram *dram;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[SZ_128];
	long long time;
	char reason[SZ_32];
	unsigned int previous;
} __packed;

struct abox_dbg_dump_info {
	struct debugfs_blob_wrapper sram;
	struct debugfs_blob_wrapper dram;
	struct debugfs_blob_wrapper log;
	struct debugfs_blob_wrapper sfr;
	struct debugfs_blob_wrapper gicd;
	struct debugfs_blob_wrapper gpr;
	struct debugfs_blob_wrapper reason;
};

static struct abox_dbg_dump (*p_abox_dbg_dump)[ABOX_DBG_DUMP_COUNT];
static struct abox_dbg_dump_min (*p_abox_dbg_dump_min)[ABOX_DBG_DUMP_COUNT];
static struct abox_dbg_dump_info abox_dbg_dump_info[ABOX_DBG_DUMP_COUNT];

static struct reserved_mem *abox_dbg_slog;

static int __init abox_dbg_slog_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	abox_dbg_slog = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(abox_dbg_slog, "exynos,abox_slog", abox_dbg_slog_setup);

static void abox_dbg_slog_init(struct abox_data *data)
{
	struct device *dev_abox = data->dev;

	dev_info(dev_abox, "%s: base=%pa, size=%pa\n", __func__,
			&abox_dbg_slog->base, &abox_dbg_slog->size);

	data->slog_base_phys = abox_dbg_slog->base;
	data->slog_size = abox_dbg_slog->size;
	data->slog_base = rmem_vmap(abox_dbg_slog);
	abox_iommu_map(dev_abox, IOVA_SILENT_LOG, data->slog_base_phys,
			data->slog_size, data->slog_base);
}

static struct reserved_mem *abox_dbg_rmem;

static int __init abox_dbg_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	abox_dbg_rmem = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(abox_dbg_rmem, "exynos,abox_dbg", abox_dbg_rmem_setup);

static bool abox_dbg_dump_valid(int idx)
{
	bool ret = false;

	if (idx >= ABOX_DBG_DUMP_COUNT)
		return false;

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[idx];

		ret = (p_dump->sfr.magic == ABOX_DBG_DUMP_MAGIC_SFR);
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[idx];

		ret = (p_dump->sfr.magic == ABOX_DBG_DUMP_MAGIC_SFR);
	}

	return ret;
}

static void abox_dbg_clear_valid(int idx)
{
	if (idx >= ABOX_DBG_DUMP_COUNT)
		return;

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[idx];

		p_dump->sfr.magic = 0;
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[idx];

		p_dump->sfr.magic = 0;
	}
}

static ssize_t abox_dbg_read_valid(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	int idx = (int)file->private_data;
	bool valid = abox_dbg_dump_valid(idx);
	char buf_val[4] = {0, }; /* enough to store a bool and "\n\0" */

	if (valid)
		buf_val[0] = 'Y';
	else
		buf_val[0] = 'N';
	buf_val[1] = '\n';
	buf_val[2] = 0x00;
	return simple_read_from_buffer(user_buf, count, ppos, buf_val, 2);
}

static const struct file_operations abox_dbg_fops_valid = {
	.open = simple_open,
	.read = abox_dbg_read_valid,
	.llseek = default_llseek,
};

static ssize_t abox_dbg_read_clear(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	int idx = (int)file->private_data;

	abox_dbg_clear_valid(idx);

	return 0;
}

static ssize_t abox_dbg_write_clear(struct file *file,
				    const char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	int idx = (int)file->private_data;

	abox_dbg_clear_valid(idx);

	return 0;
}

static const struct file_operations abox_dbg_fops_clear = {
	.open = simple_open,
	.read = abox_dbg_read_clear,
	.write = abox_dbg_write_clear,
	.llseek = no_llseek,
};

static int abox_dbg_dump_create_file(struct abox_data *data)
{
	const char *dir_fmt = "snapshot_%d";
	struct dentry *root_dir = abox_dbg_get_root_dir();
	struct dentry *dir;
	struct abox_dbg_dump_info *info;
	char *dir_name;
	int i;

	dev_dbg(data->dev, "%s\n", __func__);

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump;

		for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
			dir_name = kasprintf(GFP_KERNEL, dir_fmt, i);
			dir = debugfs_create_dir(dir_name, root_dir);
			p_dump = &(*p_abox_dbg_dump)[i];
			info = &abox_dbg_dump_info[i];

			info->sram.data = p_dump->sram.dump;
			info->sram.size = sizeof(p_dump->sram.dump);
			debugfs_create_blob("sram", 0440, dir, &info->sram);

			info->dram.data = p_dump->dram.dump;
			info->dram.size = sizeof(p_dump->dram.dump);
			debugfs_create_blob("dram", 0440, dir, &info->dram);

			info->log.data = p_dump->dram.dump + ABOX_LOG_OFFSET;
			info->log.size = ABOX_LOG_SIZE;
			debugfs_create_blob("log", 0444, dir, &info->log);

			info->sfr.data = p_dump->sfr.dump;
			info->sfr.size = sizeof(p_dump->sfr.dump);
			debugfs_create_blob("sfr", 0440, dir, &info->sfr);

			info->gicd.data = p_dump->sfr_gic_gicd;
			info->gicd.size = sizeof(p_dump->sfr_gic_gicd);
			debugfs_create_blob("gicd", 0440, dir, &info->gicd);

			info->gpr.data = p_dump->gpr;
			info->gpr.size = sizeof(p_dump->gpr);
			debugfs_create_blob("gpr", 0440, dir, &info->gpr);

			debugfs_create_u64("time", 0440, dir, &p_dump->time);

			info->reason.data = p_dump->reason;
			info->reason.size = sizeof(p_dump->reason);
			debugfs_create_blob("reason", 0440, dir, &info->reason);

			debugfs_create_u32("previous", 0440, dir,
					&p_dump->previous);

			debugfs_create_file("valid", 0440, dir,
					(void *)(long)i,
					&abox_dbg_fops_valid);

			debugfs_create_file("clear", 0440, dir,
					(void *)(long)i,
					&abox_dbg_fops_clear);

			kfree(dir_name);
		}
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump;

		for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
			dir_name = kasprintf(GFP_KERNEL, dir_fmt, i);
			dir = debugfs_create_dir(dir_name, root_dir);
			p_dump = &(*p_abox_dbg_dump_min)[i];
			info = &abox_dbg_dump_info[i];

			info->sram.data = p_dump->sram.dump;
			info->sram.size = sizeof(p_dump->sram.dump);
			debugfs_create_blob("sram", 0440, dir, &info->sram);

			info->sfr.data = p_dump->sfr.dump;
			info->sfr.size = sizeof(p_dump->sfr.dump);
			debugfs_create_blob("sfr", 0440, dir, &info->sfr);

			info->gicd.data = p_dump->sfr_gic_gicd;
			info->gicd.size = sizeof(p_dump->sfr_gic_gicd);
			debugfs_create_blob("gicd", 0440, dir, &info->gicd);

			info->gpr.data = p_dump->gpr;
			info->gpr.size = sizeof(p_dump->gpr);
			debugfs_create_blob("gpr", 0440, dir, &info->gpr);

			debugfs_create_u64("time", 0440, dir, &p_dump->time);

			info->reason.data = p_dump->reason;
			info->reason.size = sizeof(p_dump->reason);
			debugfs_create_blob("reason", 0440, dir, &info->reason);

			debugfs_create_u32("previous", 0440, dir,
					&p_dump->previous);

			debugfs_create_file("valid", 0440, dir,
					(void *)(long)i,
					&abox_dbg_fops_valid);

			debugfs_create_file("clear", 0440, dir,
					(void *)(long)i,
					&abox_dbg_fops_clear);

			kfree(dir_name);
		}
	} else {
		return -ENOMEM;
	}

	return 0;
}

static void abox_dbg_rmem_init(struct abox_data *data)
{
	struct device *dev_abox = data->dev;
	int i;

	dev_info(dev_abox, "%s: base=%pa, size=%pa\n", __func__,
			&abox_dbg_rmem->base, &abox_dbg_rmem->size);

	if (sizeof(*p_abox_dbg_dump) <= abox_dbg_rmem->size) {
		struct abox_dbg_dump *p_dump;

		p_abox_dbg_dump = rmem_vmap(abox_dbg_rmem);
		data->dump_base = p_abox_dbg_dump;
		for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
			p_dump = &(*p_abox_dbg_dump)[i];
			if (p_dump->sfr.magic == ABOX_DBG_DUMP_MAGIC_SFR)
				p_dump->previous++;
			else
				p_dump->previous = 0;
		}
	} else if (sizeof(*p_abox_dbg_dump_min) <= abox_dbg_rmem->size) {
		struct abox_dbg_dump_min *p_dump;

		p_abox_dbg_dump_min = rmem_vmap(abox_dbg_rmem);
		data->dump_base = p_abox_dbg_dump_min;
		for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
			p_dump = &(*p_abox_dbg_dump_min)[i];
			if (p_dump->sfr.magic == ABOX_DBG_DUMP_MAGIC_SFR)
				p_dump->previous++;
			else
				p_dump->previous = 0;
			p_dump->dram = NULL;
		}
	}

	abox_dbg_dump_create_file(data);

	data->dump_base_phys = abox_dbg_rmem->base;
	abox_iommu_map(dev_abox, IOVA_DUMP_BUFFER, abox_dbg_rmem->base,
			abox_dbg_rmem->size, data->dump_base);
}

void abox_dbg_dump_gpr_from_addr(struct device *dev, unsigned int *addr,
		enum abox_dbg_dump_src src, const char *reason)
{
	static unsigned long long called[ABOX_DBG_DUMP_COUNT];
	unsigned long long time = sched_clock();

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (called[src] && time - called[src] < ABOX_DBG_DUMP_LIMIT_NS) {
		dev_dbg_ratelimited(dev, "%s(%d): skipped\n", __func__, src);
		called[src] = time;
		return;
	}
	called[src] = time;

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		abox_core_dump_gpr_dump(p_dump->gpr, addr);
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		abox_core_dump_gpr_dump(p_dump->gpr, addr);
	}
}

void abox_dbg_dump_gpr(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	static unsigned long long called[ABOX_DBG_DUMP_COUNT];
	unsigned long long time = sched_clock();

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (called[src] && time - called[src] < ABOX_DBG_DUMP_LIMIT_NS) {
		dev_dbg_ratelimited(dev, "%s(%d): skipped\n", __func__, src);
		called[src] = time;
		return;
	}
	called[src] = time;

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		abox_core_dump_gpr(p_dump->gpr);
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		abox_core_dump_gpr(p_dump->gpr);
	}
}

void abox_dbg_dump_mem(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	static unsigned long long called[ABOX_DBG_DUMP_COUNT];
	unsigned long long time = sched_clock();

	dev_dbg(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (called[src] && time - called[src] < ABOX_DBG_DUMP_LIMIT_NS) {
		dev_dbg_ratelimited(dev, "%s(%d): skipped\n", __func__, src);
		called[src] = time;
		return;
	}
	called[src] = time;

	if (p_abox_dbg_dump) {
		struct abox_dbg_dump *p_dump = &(*p_abox_dbg_dump)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		memcpy_fromio(p_dump->sram.dump, data->sram_base,
				data->sram_size);
		p_dump->sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
		memcpy(p_dump->dram.dump, data->dram_base, DRAM_FIRMWARE_SIZE);
		p_dump->dram.magic = ABOX_DBG_DUMP_MAGIC_DRAM;
		memcpy_fromio(p_dump->sfr.dump, data->sfr_base,
				sizeof(p_dump->sfr.dump));
		p_dump->sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
		abox_gicd_dump(data->dev_gic, (char *)p_dump->sfr_gic_gicd, 0,
				sizeof(p_dump->sfr_gic_gicd));
	} else if (p_abox_dbg_dump_min) {
		struct abox_dbg_dump_min *p_dump = &(*p_abox_dbg_dump_min)[src];

		p_dump->time = time;
		p_dump->previous = 0;
		strncpy(p_dump->reason, reason, sizeof(p_dump->reason) - 1);
		memcpy_fromio(p_dump->sram.dump, data->sram_base,
				data->sram_size);
		p_dump->sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
		memcpy_fromio(p_dump->sfr.dump, data->sfr_base,
				sizeof(p_dump->sfr.dump));
		p_dump->sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
		abox_gicd_dump(data->dev_gic, (char *)p_dump->sfr_gic_gicd, 0,
				sizeof(p_dump->sfr_gic_gicd));

		if (p_dump->dram) {
			memcpy(p_dump->dram->dump, data->dram_base,
					DRAM_FIRMWARE_SIZE);
			p_dump->dram->magic = ABOX_DBG_DUMP_MAGIC_DRAM;
			flush_cache_all();
		} else {
			dev_info(dev, "Failed to save ABOX dram\n");
		}
	}
}

void abox_dbg_dump_gpr_mem(struct device *dev, struct abox_data *data,
		enum abox_dbg_dump_src src, const char *reason)
{
	abox_dbg_dump_gpr(dev, data, src, reason);
	abox_dbg_dump_mem(dev, data, src, reason);
}

struct abox_dbg_dump_simple {
	struct abox_dbg_dump_sram sram;
	struct abox_dbg_dump_log log;
	struct abox_dbg_dump_sfr sfr;
	u32 sfr_gic_gicd[SZ_4K / sizeof(u32)];
	unsigned int gpr[SZ_128];
	long long time;
	char reason[SZ_32];
} __packed;

static struct abox_dbg_dump_simple abox_dump_simple;

void abox_dbg_dump_simple(struct device *dev, struct abox_data *data,
		const char *reason)
{
	static unsigned long long called;
	unsigned long long time = sched_clock();

	dev_info(dev, "%s\n", __func__);

	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return;
	}

	if (called && time - called < ABOX_DBG_DUMP_LIMIT_NS) {
		dev_dbg_ratelimited(dev, "%s: skipped\n", __func__);
		called = time;
		return;
	}
	called = time;

	abox_dump_simple.time = time;
	strncpy(abox_dump_simple.reason, reason,
			sizeof(abox_dump_simple.reason) - 1);
	abox_core_dump_gpr(abox_dump_simple.gpr);
	memcpy_fromio(abox_dump_simple.sram.dump, data->sram_base,
			data->sram_size);
	abox_dump_simple.sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
	memcpy(abox_dump_simple.log.dump, data->dram_base + ABOX_LOG_OFFSET,
			ABOX_LOG_SIZE);
	abox_dump_simple.log.magic = ABOX_DBG_DUMP_MAGIC_LOG;
	memcpy_fromio(abox_dump_simple.sfr.dump, data->sfr_base,
			sizeof(abox_dump_simple.sfr.dump));
	abox_dump_simple.sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
	abox_gicd_dump(data->dev_gic, (char *)abox_dump_simple.sfr_gic_gicd, 0,
			sizeof(abox_dump_simple.sfr_gic_gicd));
}

static struct abox_dbg_dump_simple abox_dump_suspend;

void abox_dbg_dump_suspend(struct device *dev, struct abox_data *data)
{
	dev_dbg(dev, "%s\n", __func__);

	abox_dump_suspend.time = sched_clock();
	strncpy(abox_dump_suspend.reason, "suspend",
			sizeof(abox_dump_suspend.reason) - 1);
	abox_core_dump_gpr(abox_dump_suspend.gpr);
	memcpy_fromio(abox_dump_suspend.sram.dump, data->sram_base,
			data->sram_size);
	abox_dump_suspend.sram.magic = ABOX_DBG_DUMP_MAGIC_SRAM;
	memcpy(abox_dump_suspend.log.dump, data->dram_base + ABOX_LOG_OFFSET,
			ABOX_LOG_SIZE);
	abox_dump_suspend.log.magic = ABOX_DBG_DUMP_MAGIC_LOG;
	memcpy_fromio(abox_dump_suspend.sfr.dump, data->sfr_base,
			sizeof(abox_dump_suspend.sfr.dump));
	abox_dump_suspend.sfr.magic = ABOX_DBG_DUMP_MAGIC_SFR;
	abox_gicd_dump(data->dev_gic, (char *)abox_dump_suspend.sfr_gic_gicd, 0,
			sizeof(abox_dump_suspend.sfr_gic_gicd));
}

static atomic_t abox_error_count = ATOMIC_INIT(0);

void abox_dbg_report_status(struct device *dev, bool ok)
{
	char env[32] = {0,};
	char *envp[2] = {env, NULL};

	dev_info(dev, "%s\n", __func__);

	if (ok)
		atomic_set(&abox_error_count, 0);
	else
		atomic_inc(&abox_error_count);

	snprintf(env, sizeof(env), "ERR_CNT=%d",
			atomic_read(&abox_error_count));
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
}

int abox_dbg_get_error_count(struct device *dev)
{
	int count = atomic_read(&abox_error_count);

	dev_dbg(dev, "%s: %d\n", __func__, count);

	return count;
}

static ssize_t calliope_sram_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *dev_abox = dev->parent;

	dev_dbg(dev, "%s(%lld, %zu)\n", __func__, off, size);

	if (pm_runtime_get_if_in_use(dev_abox) > 0) {
		if (off == 0)
			abox_core_flush();
		memcpy_fromio(buf, battr->private + off, size);
		pm_runtime_put(dev_abox);
	} else {
		memset(buf, 0x0, size);
	}

	return size;
}

static ssize_t calliope_dram_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *dev_abox = dev->parent;

	dev_dbg(dev, "%s(%lld, %zu)\n", __func__, off, size);

	if (pm_runtime_get_if_in_use(dev_abox) > 0) {
		if (off == 0)
			abox_core_flush();
		pm_runtime_put(dev_abox);
	}
	memcpy(buf, battr->private + off, size);
	return size;
}

static ssize_t calliope_log_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	return calliope_dram_read(file, kobj, battr, buf, off, size);
}

static ssize_t calliope_slog_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	return calliope_dram_read(file, kobj, battr, buf, off, size);
}

static ssize_t gicd_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *battr, char *buf,
		loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device *dev_abox = dev->parent;
	struct abox_data *data = dev_get_drvdata(dev_abox);

	dev_dbg(dev, "%s(%lld, %zu)\n", __func__, off, size);

	pm_runtime_get(dev_abox);
	abox_gicd_dump(data->dev_gic, buf, off, size);
	pm_runtime_put(dev_abox);

	return size;
}

/* size will be updated later */
static BIN_ATTR(calliope_sram, 0440, calliope_sram_read, NULL, 0);
static BIN_ATTR(calliope_dram, 0440, calliope_dram_read, NULL,
		DRAM_FIRMWARE_SIZE);
static BIN_ATTR_RO(calliope_log, ABOX_LOG_SIZE);
static BIN_ATTR(calliope_slog, 0440, calliope_slog_read, NULL, 0);
static BIN_ATTR(gicd, 0440, gicd_read, NULL, SZ_4K);
static struct bin_attribute *calliope_bin_attrs[] = {
	&bin_attr_calliope_sram,
	&bin_attr_calliope_dram,
	&bin_attr_calliope_log,
	&bin_attr_calliope_slog,
	&bin_attr_gicd,
};

static ssize_t gpr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!abox_is_on()) {
		dev_info(dev, "%s is skipped due to no power\n", __func__);
		return -EFAULT;
	}

	return abox_core_show_gpr(buf);
}

static DEVICE_ATTR(gpr, 0440, gpr_show, NULL);

static void abox_dbg_alloc_work_func(struct work_struct *work)
{
	struct abox_dbg_dump_min *p_dump;
	int i;

	if (!p_abox_dbg_dump_min)
		return;


	for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
		p_dump = &(*p_abox_dbg_dump_min)[i];
		if (!p_dump->dram)
			p_dump->dram = vmalloc(sizeof(*p_dump->dram));
	}

}
static DECLARE_WORK(abox_dbg_alloc_work, abox_dbg_alloc_work_func);

static void abox_dbg_free_work_func(struct work_struct *work)
{
	struct abox_dbg_dump_min *p_dump;
	int i;

	if (!p_abox_dbg_dump_min)
		return;

	for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
		p_dump = &(*p_abox_dbg_dump_min)[i];
		if (p_dump->dram)
			vfree(p_dump->dram);
		p_dump->dram = NULL;
	}
}
static DECLARE_DEFERRABLE_WORK(abox_dbg_free_work, abox_dbg_free_work_func);

static int abox_dbg_power_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const unsigned long TIMEOUT = 3 * HZ;
	bool en = !!action;

	if (en) {
		cancel_delayed_work_sync(&abox_dbg_free_work);
		schedule_work(&abox_dbg_alloc_work);
	} else {
		schedule_delayed_work(&abox_dbg_free_work, TIMEOUT);
	}

	return NOTIFY_DONE;
}

static struct notifier_block abox_dbg_power_nb = {
	.notifier_call = abox_dbg_power_notifier,
};

static int samsung_abox_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *dev_abox = dev->parent;
	struct abox_data *data = dev_get_drvdata(dev_abox);
	int i, ret;

	dev_dbg(dev, "%s\n", __func__);

	if (abox_dbg_slog)
		abox_dbg_slog_init(data);

	if (abox_dbg_rmem)
		abox_dbg_rmem_init(data);

	ret = device_create_file(dev, &dev_attr_gpr);
	if (ret < 0)
		dev_warn(dev, "Failed to create file: %s\n",
				dev_attr_gpr.attr.name);
	bin_attr_calliope_sram.size = data->sram_size;
	bin_attr_calliope_sram.private = data->sram_base;
	bin_attr_calliope_dram.private = data->dram_base;
	bin_attr_calliope_log.private = data->dram_base + ABOX_LOG_OFFSET;
	bin_attr_calliope_slog.size = data->slog_size;
	bin_attr_calliope_slog.private = data->slog_base;
	for (i = 0; i < ARRAY_SIZE(calliope_bin_attrs); i++) {
		struct bin_attribute *battr = calliope_bin_attrs[i];

		ret = device_create_bin_file(dev, battr);
		if (ret < 0)
			dev_warn(dev, "Failed to create file: %s\n",
					battr->attr.name);
	}

	if (p_abox_dbg_dump_min)
		abox_power_notifier_register(&abox_dbg_power_nb);

	return ret;
}

static int samsung_abox_debug_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_dbg_dump_min *p_dump;
	int i;

	dev_dbg(dev, "%s\n", __func__);

	abox_dbg_remove_root_dir();

	if (p_abox_dbg_dump_min) {
		abox_power_notifier_unregister(&abox_dbg_power_nb);
		for (i = 0; i < ABOX_DBG_DUMP_COUNT; i++) {
			p_dump = &(*p_abox_dbg_dump_min)[i];
			if (p_dump->dram)
				vfree(p_dump->dram);
			p_dump->dram = NULL;
		}
	}

	return 0;
}

static const struct of_device_id samsung_abox_debug_match[] = {
	{
		.compatible = "samsung,abox-debug",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_debug_match);

static struct platform_driver samsung_abox_debug_driver = {
	.probe  = samsung_abox_debug_probe,
	.remove = samsung_abox_debug_remove,
	.driver = {
		.name = "samsung-abox-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_debug_match),
	},
};

module_platform_driver(samsung_abox_debug_driver);

MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box Debug Driver");
MODULE_ALIAS("platform:samsung-abox-debug");
MODULE_LICENSE("GPL");
