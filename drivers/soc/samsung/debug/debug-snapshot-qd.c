/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/sched/clock.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <asm-generic/sections.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/debug-snapshot-qd.h>
#include "debug-snapshot-local.h"

#define DSS_QD_MAX_NUM_ENTRIES		(128)

struct dbg_snapshot_qd_table {
	struct dbg_snapshot_qd_region entry[DSS_QD_MAX_NUM_ENTRIES];
	u32 num_regions;
} __packed;

struct dbg_snapshot_qd_info {
	u64 magic;
	struct dbg_snapshot_qd_table table;
	u32 checksum;
} __packed;

static struct dbg_snapshot_qd_table *dss_qd_table;
static struct device *qdump_dev;
static DEFINE_SPINLOCK(qd_lock);

static void make_checksum_of_qd_table(struct dbg_snapshot_qd_info *info)
{
	int i;
	unsigned int *data = (unsigned int *)&info->table;

	info->checksum = 0;
	for (i = 0; i < (int)sizeof(info->table) / (int)sizeof(unsigned int); i++)
		info->checksum ^= data[i];
}

int dbg_snapshot_qd_add_region(void *v_entry, u32 attr)
{
	struct dbg_snapshot_qd_region *entry =
			(struct dbg_snapshot_qd_region *)v_entry;
	struct dbg_snapshot_qd_region *qd_entry;
	u32 entries, i;
	int ret = 0;

	if (!qdump_dev || !dss_qd_table) {
		pr_err("%s: qd is not suppoerted\n", __func__);
		return -ENODEV;
	}

	if (!entry || !entry->virt_addr ||
			!entry->phys_addr || !strlen(entry->name)) {
		dev_err(qdump_dev, "Invalid entry details\n");
		return -EINVAL;
	}

	if ((strlen(entry->name) > sizeof(entry->name)) ||
		(strlen(entry->struct_name) > sizeof(entry->struct_name))) {
		dev_err(qdump_dev, "over string names\n");
		return -EINVAL;
	}

	spin_lock(&qd_lock);

	entries = dss_qd_table->num_regions;
	if (entries >= DSS_QD_MAX_NUM_ENTRIES) {
		dev_err(qdump_dev, "entries is full, No allowed more\n");
		spin_unlock(&qd_lock);
		return -ENOMEM;
	}

	for (i = 0; i < entries; i++) {
		qd_entry = &dss_qd_table->entry[i];
		if (!strncmp(qd_entry->name, entry->name,
					strlen(entry->name))) {
			dev_err(qdump_dev, "entry name is exist in array : %s",
				entry->name);
			spin_unlock(&qd_lock);
			return -EINVAL;
		}
	}

	qd_entry = &dss_qd_table->entry[entries];
	strlcpy(qd_entry->name, entry->name, sizeof(qd_entry->name));
	strlcpy(qd_entry->struct_name, entry->struct_name, sizeof(qd_entry->struct_name));
	qd_entry->virt_addr = entry->virt_addr;
	qd_entry->phys_addr = entry->phys_addr;
	qd_entry->size = entry->size;
	qd_entry->magic = DSS_SIGN_MAGIC | entries;
	qd_entry->attr = attr;
	dss_qd_table->num_regions = entries + 1;

	make_checksum_of_qd_table((struct dbg_snapshot_qd_info *)dev_get_drvdata(qdump_dev));
	spin_unlock(&qd_lock);

	dev_info(qdump_dev, "Success to add (0x%x: %s)\n",
			entries, qd_entry->name);

	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_qd_add_region);

bool dbg_snapshot_qd_enable(void)
{
	bool ret = false;

	if (dss_dpm.dump_mode == QUICK_DUMP)
		ret = true;

	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_qd_enable);

static void register_dss_log_item(void)
{
	struct dbg_snapshot_qd_region qd_entry;
	int item_num = dbg_snapshot_log_get_num_items();
	struct dbg_snapshot_log_item *item;
	int i;

	for (i = 0; i < item_num; i++){
		item = (struct dbg_snapshot_log_item *)
			dbg_snapshot_log_get_item_by_index(i);
		if (item->entry.enabled) {
			strlcpy(qd_entry.name, item->name,
					sizeof(qd_entry.name));
			strlcpy(qd_entry.struct_name, item->name,
					sizeof(qd_entry.struct_name));
			qd_entry.virt_addr = item->entry.vaddr;
			qd_entry.phys_addr = item->entry.paddr;
			qd_entry.size = item->entry.size;
			if (dbg_snapshot_qd_add_region(&qd_entry,
						DSS_QD_ATTR_STRUCT))
				dev_err(qdump_dev, "Failed to add "
						"dss_log_item : %s\n",
						item->name);
		}
	}
}

static void register_dss_item(void)
{
	struct dbg_snapshot_qd_region qd_entry;
	int item_num = dbg_snapshot_get_num_items();
	struct dbg_snapshot_item *item;
	int i;

	for (i = 0; i < item_num; i++){
		if (i == DSS_ITEM_KEVENTS_ID)
			continue;
		if (i == DSS_ITEM_PLATFORM_ID)
			continue;
		if (i == DSS_ITEM_S2D_ID)
			continue;
		item = (struct dbg_snapshot_item *)
			dbg_snapshot_get_item_by_index(i);
		if (item->entry.enabled) {
			u32 attr = DSS_QD_ATTR_BINARY;

			if (i == DSS_ITEM_BACKTRACE_ID)
				attr |= DSS_QD_ATTR_SKIP_ENCRYPT;
			strlcpy(qd_entry.name, item->name,
					sizeof(qd_entry.name));
			qd_entry.struct_name[0] = '\0';
			qd_entry.virt_addr = item->entry.vaddr;
			qd_entry.phys_addr = item->entry.paddr;
			qd_entry.size = item->entry.size;
			if (dbg_snapshot_qd_add_region(&qd_entry, attr))
				dev_err(qdump_dev, "Failed to add dss_item : %s\n", item->name);
		}
	}
}

static void dbg_snapshot_qd_list_dump(void)
{
	struct dbg_snapshot_qd_region *entry;
	unsigned long i, size = 0;

	if (!qdump_dev)
		return;

	dev_info(qdump_dev, "quickdump physical / virtual memory layout:\n");
	for (i = 0; i < dss_qd_table->num_regions; i++) {
		entry = &dss_qd_table->entry[i];
		dev_info(qdump_dev, "%-16s: -%16s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
			entry->name,
			entry->struct_name,
			entry->phys_addr,
			entry->virt_addr,
			entry->size);

		size += entry->size;
	}

	dev_info(qdump_dev, "total_quick_dump_size: %ldKB, quick_dump_entry: %#lx\n",
			size / SZ_1K, __raw_readq(dbg_snapshot_get_header_vaddr() + DSS_OFFSET_QD_ENTRY));
}

static void *qd_rmem_set_vmap(struct reserved_mem *rmem)
{
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	int i, page_size;
	struct page *page, **pages;
	void *vaddr;

	page_size = rmem->size / PAGE_SIZE;
	pages = kcalloc(page_size, sizeof(struct page *), GFP_KERNEL);
	page = phys_to_page(rmem->base);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	vaddr = vmap(pages, page_size, VM_NO_GUARD | VM_DMA_COHERENT, prot);
	kfree(pages);

	return vaddr;
}

static int dbg_snapshot_qd_probe(struct platform_device *pdev)
{
	struct dbg_snapshot_qd_info *qd_info;
	struct reserved_mem *rmem;
	struct device_node *np;

	dbg_snapshot_set_qd_entry(0);
	/* Need to wait for checking whether quick-dump is enabled or not */
	if (!dbg_snapshot_get_enable())
		return -EPROBE_DEFER;

	if (dss_dpm.dump_mode != QUICK_DUMP) {
		dev_err(&pdev->dev, "No Quick dump mode\n");
		return -ENODEV;
	}

	qdump_dev = &pdev->dev;
	np = of_parse_phandle(qdump_dev->of_node, "memory-region", 0);
	if (!np) {
		dev_err(qdump_dev, "%s: there is no memory-region node\n", __func__);
		return -ENOMEM;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		dev_err(qdump_dev, "%s: no such rmem node\n", __func__);
		return -ENOMEM;
	}

	if (rmem->size < sizeof(*qd_info)) {
		dev_err(qdump_dev, "%s: rmem size is too small to use qd(%#lx/%#lx)\n",
						__func__, rmem->size, sizeof(*qd_info));
		return -ENOMEM;
	}

	qd_info = qd_rmem_set_vmap(rmem);
	qd_info->magic = 0xDB909090;
	dev_set_drvdata(qdump_dev, qd_info);
	dbg_snapshot_set_qd_entry((unsigned long)rmem->base);
	dss_qd_table = &qd_info->table;
	dss_qd_table->num_regions = 0;

	dev_info(&pdev->dev, "Success to initialization\n");

	register_dss_item();
	register_dss_log_item();

	dbg_snapshot_qd_list_dump();

	dev_info(&pdev->dev, "%s successful.\n", __func__);
	return 0;
}

static const struct of_device_id dbg_snapshot_qd_matches[] = {
	{ .compatible = "debug-snapshot,qdump", },
	{},
};
MODULE_DEVICE_TABLE(of, dbg_snapshot_qd_matches);

static struct platform_driver dbg_snapshot_qd_driver = {
	.probe		= dbg_snapshot_qd_probe,
	.driver		= {
		.name	= "debug-snapshot-qdump",
		.of_match_table	= of_match_ptr(dbg_snapshot_qd_matches),
	},
};
module_platform_driver(dbg_snapshot_qd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Debug-Snapshot-qdump");
