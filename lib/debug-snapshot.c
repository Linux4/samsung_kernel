/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 * Author: Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched/clock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>

#include "debug-snapshot-local.h"

/* To Support Samsung SoC */
#include <soc/samsung/cal-if.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#endif

/* 1. last kmsg hooking */
#include <linux/sec_debug.h>

#ifdef CONFIG_SEC_PM_DEBUG
static bool sec_log_full;
#endif

extern void register_hook_logbuf(void (*)(const char *, size_t, int fatal));
extern void register_hook_logger(void (*)(const char *, const char *, size_t));

struct dbg_snapshot_interface {
	struct dbg_snapshot_log *info_event;
	struct dbg_snapshot_item info_log[DSS_ITEM_MAX_NUM];
};

struct dbg_snapshot_ops {
        int (*pd_status)(unsigned int id);
};

struct dbg_snapshot_ops dss_ops = {
	.pd_status = cal_pd_status,
};

struct dbg_snapshot_bl *dss_bl;
struct dbg_snapshot_item dss_items[] = {
	{DSS_ITEM_HEADER_ID,	DSS_ITEM_HEADER,	{0, 0, 0, false, true, true}, NULL ,NULL, 0, },
	{DSS_ITEM_KERNEL_ID,	DSS_ITEM_KERNEL,	{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_PLATFORM_ID,	DSS_ITEM_PLATFORM,	{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_FATAL_ID,	DSS_ITEM_FATAL,		{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_KEVENTS_ID,	DSS_ITEM_KEVENTS,	{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_PSTORE_ID,	DSS_ITEM_PSTORE,	{0, 0, 0, true,  false, true}, NULL ,NULL, 0, },
	{DSS_ITEM_SFR_ID,	DSS_ITEM_SFR,		{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_S2D_ID,	DSS_ITEM_S2D,		{0, 0, 0, false, false, true}, NULL, NULL, 0, },
	{DSS_ITEM_ARRDUMP_RESET_ID, DSS_ITEM_ARRDUMP_RESET,{0, 0, 0, false, false, false}, NULL, NULL, 0, },
	{DSS_ITEM_ARRDUMP_PANIC_ID, DSS_ITEM_ARRDUMP_PANIC,{0, 0, 0, false, false, false}, NULL, NULL, 0, },
	{DSS_ITEM_ETM_ID,	DSS_ITEM_ETM,		{0, 0, 0, true, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_BCM_ID,	DSS_ITEM_BCM,		{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_LLC_ID,	DSS_ITEM_LLC,		{0, 0, 0, false, false, false}, NULL ,NULL, 0, },
	{DSS_ITEM_DBGC_ID,	DSS_ITEM_DBGC,		{0, 0, 0, false, false, true}, NULL ,NULL, 0, },
};

/*  External interface variable for trace debugging */
static struct dbg_snapshot_interface dss_info __attribute__ ((used));
static struct dbg_snapshot_interface *ess_info __attribute__ ((used));

struct dbg_snapshot_base dss_base;
struct dbg_snapshot_base ess_base;
struct dbg_snapshot_log *dss_log = NULL;
struct dbg_snapshot_desc dss_desc;

void secdbg_base_get_kevent_info(struct ess_info_offset *p, int type)
{
	unsigned long kevent_base_va = (unsigned long)(dss_log->task);
	unsigned long kevent_base_pa = dss_items[DSS_ITEM_KEVENTS_ID].entry.paddr;

	switch (type) {
	case DSS_KEVENT_TASK:
		p->base = kevent_base_pa + (unsigned long)(dss_log->task) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __task_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_WORK:
		p->base = kevent_base_pa + (unsigned long)(dss_log->work) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __work_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_IRQ:
		p->base = kevent_base_pa + (unsigned long)(dss_log->irq) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM * 2;
		p->size = sizeof(struct __irq_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_FREQ:
		p->base = kevent_base_pa + (unsigned long)(dss_log->freq) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __freq_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_IDLE:
		p->base = kevent_base_pa + (unsigned long)(dss_log->cpuidle) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __cpuidle_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_THRM:
		p->base = kevent_base_pa + (unsigned long)(dss_log->thermal) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __thermal_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_ACPM:
		p->base = kevent_base_pa + (unsigned long)(dss_log->acpm) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __acpm_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_MFRQ:
		p->base = kevent_base_pa + (unsigned long)(dss_log->freq_misc) - kevent_base_va;
		p->nr = DSS_LOG_MAX_NUM;
		p->size = sizeof(struct __freq_misc_log);
		p->per_core = 0;
		break;

	default:
		p->base = 0;
		p->nr = 0;
		p->size = 0;
		p->per_core = 0;
		break;
	}

	p->last = secdbg_base_get_kevent_index_addr(type);
}

int dbg_snapshot_get_debug_level(void)
{
	return dss_desc.debug_level;
}

int dbg_snapshot_add_bl_item_info(const char *name, unsigned int paddr, unsigned int size)
{
	if (!dbg_snapshot_get_enable())
		return -ENODEV;

	if (dss_bl->item_count >= DSS_MAX_BL_SIZE)
		return -ENOMEM;

	memcpy(dss_bl->item[dss_bl->item_count].name, name, strlen(name) + 1);
	dss_bl->item[dss_bl->item_count].paddr = paddr;
	dss_bl->item[dss_bl->item_count].size = size;
	dss_bl->item[dss_bl->item_count].enabled = 1;
	dss_bl->item_count++;

	return 0;
}

int dbg_snapshot_set_enable_item(const char *name, int en)
{
	struct dbg_snapshot_item *item = NULL;
	unsigned long i;

	if (!name)
		return -ENODEV;

	if (!dss_dpm.enabled || !dss_dpm.enabled_debug)
		return -EACCES;

	if (dss_dpm.enabled_dump_mode) {
		/* This is default for debug-mode */
		for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name))) {
				item = &dss_items[i];
				item->entry.enabled = en;
				pr_info("debug-snapshot: item - %s is %sabled\n",
						name, en ? "en" : "dis");

				break;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name))) {
				item = &dss_items[i];
				if (item->entry.enabled_no_dump) {
					item->entry.enabled = en;
					pr_info("debug-snapshot: item - %s is %sabled\n",
							name, en ? "en" : "dis");
				}
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_set_enable_item);

int __init dbg_snapshot_set_enable(int en)
{
	dss_base.enabled = en;
	dev_info(dss_desc.dev, "debug-snapshot: %sabled\n", en ? "en" : "dis");

	return 0;
}

int dbg_snapshot_try_enable(const char *name, unsigned long long duration)
{
	struct dbg_snapshot_item *item = NULL;
	unsigned long long time;
	unsigned long i;
	int ret = -1;

	/* If DSS was disabled, just return */
	if (!dbg_snapshot_get_enable())
		return ret;

	for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
		if (!strncmp(dss_items[i].name, name, strlen(name))) {
			item = &dss_items[i];

			/* We only interest in disabled */
			if (!item->entry.enabled && item->time) {
				time = local_clock() - item->time;
				if (time > duration) {
					item->entry.enabled = true;
					ret = 1;
				} else
					ret = 0;
			}
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_try_enable);

int dbg_snapshot_get_enable(void)
{
	return dss_base.enabled;
}
EXPORT_SYMBOL(dbg_snapshot_get_enable);

int dbg_snapshot_get_enable_item(const char *name)
{
	struct dbg_snapshot_item *item = NULL;
	unsigned long i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
		if (!strncmp(dss_items[i].name, name, strlen(name))) {
			item = &dss_items[i];
			ret = item->entry.enabled;
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_get_enable_item);

static inline int dbg_snapshot_check_eob(struct dbg_snapshot_item *item,
						size_t size)
{
	size_t max, cur;

	max = (size_t)(item->head_ptr + item->entry.size);
	cur = (size_t)(item->curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}

static inline void dbg_snapshot_hook_logger(const char *name,
					 const char *buf, size_t size)
{
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_PLATFORM_ID];

	if (likely(dbg_snapshot_get_enable() && item->entry.enabled)) {
		size_t last_buf;

		if (unlikely((dbg_snapshot_check_eob(item, size))))
			item->curr_ptr = item->head_ptr;

		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
		/*  save the address of last_buf to physical address */
		last_buf = (size_t)item->curr_ptr;

		__raw_writel_no_log(item->entry.paddr + (last_buf - item->entry.vaddr),
			dbg_snapshot_get_base_vaddr() + DSS_OFFSET_LAST_PLATFORM_LOGBUF);
	}
}

size_t dbg_snapshot_get_curr_ptr_for_sysrq(void)
{
#ifdef CONFIG_SEC_DEBUG_SYSRQ_KMSG
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_KERNEL_ID];

	return (size_t)item->curr_ptr;
#else
	return 0;
#endif
}

static inline void dbg_snapshot_hook_logbuf(const char *buf, size_t size, int fatal)
{
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_KERNEL_ID];

	do {
		if (likely(dbg_snapshot_get_enable() && item->entry.enabled)) {
			size_t last_buf;

			if (dbg_snapshot_check_eob(item, size)) {
				item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
				*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
#ifdef CONFIG_SEC_PM_DEBUG
				if (unlikely(!sec_log_full))
					sec_log_full = true;
#endif
			}

			memcpy(item->curr_ptr, buf, size);
			item->curr_ptr += size;
			/*  save the address of last_buf to physical address */
			last_buf = (size_t)item->curr_ptr;

			if (item == (struct dbg_snapshot_item *)&dss_items[DSS_ITEM_KERNEL_ID])
				__raw_writel_no_log(item->entry.paddr + (last_buf - item->entry.vaddr),
					dbg_snapshot_get_header_vaddr() + DSS_OFFSET_LAST_LOGBUF);

			if (fatal == 1)
				item = &dss_items[DSS_ITEM_FATAL_ID];
		}
	} while(fatal-- > 0);
}

static bool dbg_snapshot_check_pmu(struct dbg_snapshot_sfrdump *sfrdump,
						const struct device_node *np)
{
	int ret = 0, count, i;
	unsigned int val;

	if (!sfrdump->pwr_mode)
		return true;

	count = of_property_count_u32_elems(np, "cal-pd-id");
	for (i = 0; i < count; i++) {
		ret = of_property_read_u32_index(np, "cal-pd-id", i, &val);
		if (ret < 0) {
			dev_err(dss_desc.dev, "failed to get pd-id - %s\n", sfrdump->name);
			return false;
		}
		ret = dss_ops.pd_status(val);
		if (ret < 0) {
			dev_err(dss_desc.dev, "not powered - %s (pd-id: %d)\n", sfrdump->name, i);
			return false;
		}
	}
	return true;
}

static int dbg_snapshot_output(void)
{
	unsigned long i, size = 0;

	dev_info(dss_desc.dev, "debug-snapshot physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
		if (dss_items[i].entry.enabled)
			dev_info(dss_desc.dev, "%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx / en:%d\n",
				dss_items[i].name,
				dss_items[i].entry.paddr,
				dss_items[i].entry.vaddr,
				dss_items[i].entry.size,
				dss_items[i].entry.enabled);
		size += dss_items[i].entry.size;
	}

	dev_info(dss_desc.dev, "total_item_size: %ldKB, dbg_snapshot_log struct size: %dKB\n",
			size / SZ_1K, dbg_snapshot_log_size / SZ_1K);

	return 0;
}

void dbg_snapshot_dump_sfr(void)
{
	struct dbg_snapshot_sfrdump *sfrdump;
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_SFR_ID];
	struct list_head *entry;
	struct device_node *np;
	unsigned int reg, offset, val, size;
	int i, ret;
	static char buf[SZ_64];

	dbg_snapshot_output();

	if (unlikely(!dbg_snapshot_get_enable() || !item->entry.enabled)) {
		dev_emerg(dss_desc.dev, "debug-snapshot: %s is disabled, %d\n", item->name, item->entry.enabled);
		return;
	} else {
		dev_emerg(dss_desc.dev, "debug-snapshot: %s is enabled, %d\n", item->name, item->entry.enabled);
	}

	if (list_empty(&dss_desc.sfrdump_list)) {
		dev_emerg(dss_desc.dev, "debug-snapshot: %s: No information\n", __func__);
		return;
	}

	list_for_each(entry, &dss_desc.sfrdump_list) {
		sfrdump = list_entry(entry, struct dbg_snapshot_sfrdump, list);
		np = of_node_get(sfrdump->node);
		ret = dbg_snapshot_check_pmu(sfrdump, np);
		if (!ret)
			/* may off */
			continue;

		for (i = 0; i < (int)(sfrdump->size >> 2); i++) {
			offset = i * 4;
			reg = sfrdump->phy_reg + offset;

			val = __raw_readl_no_log(sfrdump->reg + offset);
			snprintf(buf, SZ_64, "0x%X = 0x%0X\n",reg, val);
			size = (unsigned int)strlen(buf);
			if (unlikely((dbg_snapshot_check_eob(item, size))))
				item->curr_ptr = item->head_ptr;
			memcpy(item->curr_ptr, buf, strlen(buf));
			item->curr_ptr += strlen(buf);
		}
		of_node_put(np);
		dev_info(dss_desc.dev, "debug-snapshot: complete to dump %s\n", sfrdump->name);
	}

}

static int dbg_snapshot_sfr_dump_init(struct device_node *np)
{
	struct device_node *dump_np;
	struct dbg_snapshot_sfrdump *sfrdump;
	u32 phy_regs[2];
	int ret = 0;

	INIT_LIST_HEAD(&dss_desc.sfrdump_list);
	for_each_child_of_node(np, dump_np) {

		sfrdump = kzalloc(sizeof(struct dbg_snapshot_sfrdump), GFP_KERNEL);
		if (!sfrdump) {
			dev_err(dss_desc.dev, "failed to get memory region of dbg_snapshot_sfrdump\n");
			of_node_put(dump_np);
			ret = -ENOMEM;
			break;
		}

		ret = of_property_read_u32_array(dump_np, "reg", phy_regs, 2);
		if (ret < 0) {
			dev_err(dss_desc.dev, "failed to get register information\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}

		sfrdump->reg = ioremap(phy_regs[0], phy_regs[1]);
		if (!sfrdump->reg) {
			dev_err(dss_desc.dev, "failed to get i/o address %s node\n", dump_np->name);
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}

		/* check 4 bytes alignment */
		if ((phy_regs[0] & 0x3) ||(phy_regs[1] & 0x3)) {
			dev_err(dss_desc.dev, "(%s) Invalid alignments (4 bytes)\n", dump_np->name);
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}

		sfrdump->phy_reg = phy_regs[0];
		sfrdump->size = phy_regs[1];
		sfrdump->name = dump_np->name;
		sfrdump->node = dump_np;

		ret = of_property_count_u32_elems(dump_np, "cal-pd-id");
		if (ret < 0)
			sfrdump->pwr_mode = false;
		else
			sfrdump->pwr_mode = true;

		list_add(&sfrdump->list, &dss_desc.sfrdump_list);

		dev_info(dss_desc.dev, "success to regsiter %s\n", sfrdump->name);
		of_node_put(dump_np);
		ret = 0;
	}

	if (!ret && list_empty(&dss_desc.sfrdump_list))
		dev_info(dss_desc.dev, "There is no sfr dump list\n");

	return ret;
}

static int __init dbg_snapshot_remap(void)
{
	unsigned long i, j;
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	unsigned int enabled_count = 0;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size;
	struct page *page;
	struct page **pages;
	void *vaddr;

	for (i = 0; i < ARRAY_SIZE(dss_items); i++) {
		if (dss_items[i].entry.enabled && dss_items[i].entry.paddr && dss_items[i].entry.size) {
			enabled_count++;
			page_size = dss_items[i].entry.size / PAGE_SIZE;
			pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
			page = phys_to_page(dss_items[i].entry.paddr);

			for (j = 0; j < page_size; j++)
				pages[j] = page++;

			vaddr = vmap(pages, page_size, flags, prot);
			kfree(pages);
			if (!vaddr) {
				dev_err(dss_desc.dev,
				"debug-snapshot: %s: paddr:%lx page_size:%lx  failed to mapping between virt and phys\n",
				dss_items[i].name,
				(unsigned long)dss_items[i].entry.paddr,
				(unsigned long)dss_items[i].entry.size);
				return -ENOMEM;
			}

			dss_items[i].entry.vaddr = (size_t)vaddr;
			dss_items[i].head_ptr = (unsigned char *)dss_items[i].entry.vaddr;
			dss_items[i].curr_ptr = (unsigned char *)dss_items[i].entry.vaddr;

			if (strnstr(dss_items[i].name, "header", strlen("header")))
				dss_base.vaddr = dss_items[i].entry.vaddr;
		}
	}
	return enabled_count;
}

static int __init dbg_snapshot_init_desc(void)
{
	/* initialize dss_desc */
	memset((struct dbg_snapshot_desc *)&dss_desc, 0, sizeof(struct dbg_snapshot_desc));
	dss_desc.callstack = CONFIG_DEBUG_SNAPSHOT_CALLSTACK;
	raw_spin_lock_init(&dss_desc.ctrl_lock);
	raw_spin_lock_init(&dss_desc.nmi_lock);
	dss_desc.dev = create_empty_device();

	if (!dss_desc.dev)
		panic("Exynos: create empty device fail");
	dev_set_socdata(dss_desc.dev, "Exynos", "DSS");

	dss_desc.log_cnt = ARRAY_SIZE(dss_items);
#ifdef CONFIG_S3C2410_WATCHDOG
	dss_desc.no_wdt_dev = false;
#else
	dss_desc.no_wdt_dev = true;
#endif
	return 0;
}

#ifdef CONFIG_OF_RESERVED_MEM
int __init dbg_snapshot_reserved_mem_check(unsigned long node, unsigned long size)
{
	const char *name;
	unsigned int i;
	int ret = 0;

	name = of_get_flat_dt_prop(node, "compatible", NULL);
	if (!name)
		goto out;

	if (!strstr(name, "debug-snapshot"))
		goto out;

	if (size == 0) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < (unsigned int)ARRAY_SIZE(dss_items); i++) {
		if (strnstr(name, dss_items[i].name, strlen(name)))
			break;
	}

	if (i == ARRAY_SIZE(dss_items) || !dss_items[i].entry.enabled)
		ret = -EINVAL;
out:
	return ret;
}

static int __init dbg_snapshot_item_reserved_mem_setup(struct reserved_mem *remem)
{
	unsigned int i;

	for (i = 0; i < (unsigned int)ARRAY_SIZE(dss_items); i++) {
		if (strnstr(remem->name, dss_items[i].name, strlen(remem->name)))
			break;
	}

	if (i == ARRAY_SIZE(dss_items))
		return -ENODEV;

	if (!dss_items[i].entry.enabled)
		return -ENODEV;

	dss_items[i].entry.paddr = remem->base;
	dss_items[i].entry.size = remem->size;

	if (strnstr(remem->name, "header", strlen(remem->name))) {
		dss_base.paddr = remem->base;
		ess_base = dss_base;
	}
	dss_base.size += remem->size;
	return 0;
}

#define DECLARE_DBG_SNAPSHOT_RESERVED_REGION(compat, name) \
RESERVEDMEM_OF_DECLARE(name, compat#name, dbg_snapshot_item_reserved_mem_setup)

DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", header);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_kernel);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_platform);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_sfr);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_s2d);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_arrdumpreset);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_arrdumppanic);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_etm);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_bcm);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_llc);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_dbgc);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_pstore);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_kevents);
DECLARE_DBG_SNAPSHOT_RESERVED_REGION("debug-snapshot,", log_fatal);
#endif

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	vaddr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/

static void __init dbg_snapshot_fixmap_header(void)
{
	/*  fill 0 to next to header */
	size_t vaddr, paddr, size;
	size_t *addr;

	vaddr = dss_items[DSS_ITEM_HEADER_ID].entry.vaddr;
	paddr = dss_items[DSS_ITEM_HEADER_ID].entry.paddr;
	size = dss_items[DSS_ITEM_HEADER_ID].entry.size;

	/*  set to confirm debug-snapshot */
	addr = (size_t *)vaddr;
	memcpy(addr, &dss_base, sizeof(struct dbg_snapshot_base));

	if (!dbg_snapshot_get_enable_item("header"))
		return;

	/*  initialize kernel event to 0 except only header */
	memset((size_t *)(vaddr + DSS_KEEP_HEADER_SZ), 0, size - DSS_KEEP_HEADER_SZ);

	dss_bl = dbg_snapshot_get_header_vaddr() + DSS_OFFSET_ITEM_INFO;
	memset(dss_bl, 0, sizeof(struct dbg_snapshot_bl));

	dss_bl->magic1 = 0x01234567;
	dss_bl->magic2 = 0x89ABCDEF;
	dss_bl->item_count = ARRAY_SIZE(dss_items);
	memcpy(dss_bl->item[DSS_ITEM_HEADER_ID].name,
			dss_items[DSS_ITEM_HEADER_ID].name,
			strlen(dss_items[DSS_ITEM_HEADER_ID].name) + 1);
	dss_bl->item[DSS_ITEM_HEADER_ID].paddr = paddr;
	dss_bl->item[DSS_ITEM_HEADER_ID].size = size;
	dss_bl->item[DSS_ITEM_HEADER_ID].enabled =
		dss_items[DSS_ITEM_HEADER_ID].entry.enabled;
}

static void __init dbg_snapshot_fixmap(void)
{
	size_t last_buf;
	size_t vaddr, paddr, size;
	unsigned long i;

	/*  fixmap to header first */
	dbg_snapshot_fixmap_header();

	for (i = 1; i < ARRAY_SIZE(dss_items); i++) {
		memcpy(dss_bl->item[i].name, dss_items[i].name,	strlen(dss_items[i].name) + 1);
		dss_bl->item[i].enabled = dss_items[i].entry.enabled;

		if (!dss_items[i].entry.enabled)
			continue;

		/*  assign dss_item information */
		paddr = dss_items[i].entry.paddr;
		vaddr = dss_items[i].entry.vaddr;
		size = dss_items[i].entry.size;

		if (i == DSS_ITEM_KERNEL_ID) {
			/*  load last_buf address value(phy) by virt address */
			last_buf = (size_t)__raw_readl(dbg_snapshot_get_header_vaddr() +
							DSS_OFFSET_LAST_LOGBUF);
			/*  check physical address offset of kernel logbuf */
			if (last_buf >= dss_items[i].entry.paddr &&
				(last_buf) <= (dss_items[i].entry.paddr + dss_items[i].entry.size)) {
				/*  assumed valid address, conversion to virt */
				dss_items[i].curr_ptr = (unsigned char *)(dss_items[i].entry.vaddr +
							(last_buf - dss_items[i].entry.paddr));
			} else {
				/*  invalid address, set to first line */
				dss_items[i].curr_ptr = (unsigned char *)vaddr;
				/*  initialize logbuf to 0 */
				memset((size_t *)vaddr, 0, size);
			}
		} else if (i == DSS_ITEM_PLATFORM_ID) {
				last_buf = (size_t)__raw_readl(dbg_snapshot_get_header_vaddr() +
							DSS_OFFSET_LAST_PLATFORM_LOGBUF);
			if (last_buf >= dss_items[i].entry.vaddr &&
				(last_buf) <= (dss_items[i].entry.vaddr + dss_items[i].entry.size)) {
				dss_items[i].curr_ptr = (unsigned char *)(last_buf);
			} else {
				dss_items[i].curr_ptr = (unsigned char *)vaddr;
				memset((size_t *)vaddr, 0, size);
			}
		} else {
			/*  initialized log to 0 if persist == false */
			if (!dss_items[i].entry.persist) {
				memset((size_t *)vaddr, 0, size);
			}
		}
		dss_info.info_log[i - 1].name = kstrdup(dss_items[i].name, GFP_KERNEL);
		dss_info.info_log[i - 1].head_ptr = (unsigned char *)dss_items[i].entry.vaddr;
		dss_info.info_log[i - 1].curr_ptr = NULL;
		dss_info.info_log[i - 1].entry.size = size;

		memcpy(dss_bl->item[i].name,  dss_items[i].name, strlen(dss_items[i].name) + 1);
		dss_bl->item[i].paddr = paddr;
		dss_bl->item[i].size = size;
	}

	dss_log = (struct dbg_snapshot_log *)(dss_items[DSS_ITEM_KEVENTS_ID].entry.vaddr);

	/*  set fake translation to virtual address to debug trace */
	dss_info.info_event = dss_log;
	ess_info = &dss_info;

	/* output the information of debug-snapshot */
	dbg_snapshot_output();

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
	secdbg_lkmg_store(dss_items[DSS_ITEM_KERNEL_ID].head_ptr,
			dss_items[DSS_ITEM_KERNEL_ID].curr_ptr,
			dss_items[DSS_ITEM_KERNEL_ID].entry.size);
#endif
}

static int dbg_snapshot_init_dt_parse(struct device_node *np)
{
	int ret = 0;
	struct device_node *sfr_dump_np;

	if (of_property_read_u32(np, "use_multistage_wdt_irq",
				&dss_desc.multistage_wdt_irq)) {
		dss_desc.multistage_wdt_irq = 0;
		dev_err(dss_desc.dev, "debug-snapshot: no support multistage_wdt\n");
	}

	if (dbg_snapshot_get_enable_item(DSS_ITEM_SFR)) {
		sfr_dump_np = of_get_child_by_name(np, "dump-info");
		if (!sfr_dump_np) {
			dev_info(dss_desc.dev, "debug-snapshot: failed to get dump-info node\n");
			ret = -ENODEV;
		} else {
			ret = dbg_snapshot_sfr_dump_init(sfr_dump_np);
			if (ret < 0) {
				dev_err(dss_desc.dev, "debug-snapshot: failed to register sfr dump node\n");
				ret = -ENODEV;
				of_node_put(sfr_dump_np);
			}
		}
		if (ret < 0)
			dbg_snapshot_set_enable_item(DSS_ITEM_SFR, false);
	}

	of_node_put(np);
	return ret;
}

static const struct of_device_id dss_of_match[] __initconst = {
	{ .compatible	= "debug-snapshot-soc",
	  .data		= dbg_snapshot_init_dt_parse},
	{},
};

static int __init dbg_snapshot_init_dt(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	dss_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, dss_of_match, &matched_np);

	if (!np) {
		dev_info(dss_desc.dev, "debug-snapshot: couldn't find device tree file of debug-snapshot\n");
		return -ENODEV;
	}

	init_fn = (dss_initcall_t)matched_np->data;
	return init_fn(np);
}

static int __init dbg_snapshot_init_value(void)
{
	dss_desc.debug_level = dbg_snapshot_get_debug_level_reg();

	if (dss_dpm.enabled_dump_mode && dss_desc.debug_level)
		dbg_snapshot_scratch_reg(DSS_SIGN_SCRATCH);
	else
		dbg_snapshot_scratch_reg(DSS_SIGN_RESET);

	dbg_snapshot_set_sjtag_status();

	/* copy linux_banner, physical address of
	 * kernel log / platform log / kevents to DSS header */
	strncpy(dbg_snapshot_get_header_vaddr() + DSS_OFFSET_LINUX_BANNER,
		linux_banner, strlen(linux_banner));

	return 0;
}

static void __init dbg_snapshot_boot_cnt(void)
{
	unsigned int reg;

	reg = __raw_readl(dbg_snapshot_get_header_vaddr() +
				DSS_OFFSET_KERNEL_BOOT_CNT_MAGIC);
	if (reg == DSS_BOOT_CNT_MAGIC) {
		reg = __raw_readl(dbg_snapshot_get_header_vaddr() +
					DSS_OFFSET_KERNEL_BOOT_CNT);
		reg += 1;
		writel(reg, dbg_snapshot_get_header_vaddr() +
					DSS_OFFSET_KERNEL_BOOT_CNT);
	} else {
		reg = 1;
		writel(reg, dbg_snapshot_get_header_vaddr() +
					DSS_OFFSET_KERNEL_BOOT_CNT);
		writel(DSS_BOOT_CNT_MAGIC, dbg_snapshot_get_header_vaddr() +
						DSS_OFFSET_KERNEL_BOOT_CNT_MAGIC);

	}

	dev_info(dss_desc.dev, "Kernel Booting SEQ #%u\n", reg);
}

static int __init dbg_snapshot_init(void)
{
	if (!dbg_snapshot_get_enable_item(DSS_ITEM_HEADER))
		return 0;

	dbg_snapshot_init_desc();

	if (dbg_snapshot_remap() > 0) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		dbg_snapshot_init_log_idx();
		dbg_snapshot_fixmap();

		dbg_snapshot_set_enable(true);

		dbg_snapshot_boot_cnt();
		dbg_snapshot_init_dt();
		dbg_snapshot_init_helper();
		dbg_snapshot_init_utils();
		dbg_snapshot_init_value();

		register_hook_logbuf(dbg_snapshot_hook_logbuf);
		register_hook_logger(dbg_snapshot_hook_logger);
	} else
		dev_err(dss_desc.dev, "debug-snapshot: %s failed\n", __func__);

	return 0;
}
early_initcall(dbg_snapshot_init);

#ifdef CONFIG_SEC_PM_DEBUG
static ssize_t sec_log_read_all(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
	size_t size;
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_KERNEL_ID];

	if (sec_log_full)
		size = item->entry.size;
	else
		size = (size_t)(item->curr_ptr - item->head_ptr);

	if (pos >= size)
		return 0;

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, item->head_ptr + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_log_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_all,
};

static int __init sec_log_late_init(void)
{
	struct proc_dir_entry *entry;
	struct dbg_snapshot_item *item = &dss_items[DSS_ITEM_KERNEL_ID];

	if (!item->head_ptr)
		return 0;

	entry = proc_create("sec_log", 0440, NULL, &sec_log_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, item->entry.size);

	return 0;
}

late_initcall(sec_log_late_init);
#endif /* CONFIG_SEC_PM_DEBUG */
