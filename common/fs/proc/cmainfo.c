/*
 * =============================================================================
 *
 *       Filename:  cmainfo.c
 *
 *      description: displays the cma memory information
 *
 *        Version:  1.0
 *        Created:  17/7/2014
 *       Revision:  none
 *       Compiler:  arm-gcc
 *
 *   Organization:  AP Systems R&D 2 , SRIB
 *
 *Copyright (C) 2014, Samsung Electronics Co., Ltd. All Rights Reserved.
 * =============================================================================
 */

#include <linux/fs.h>
#include <linux/hugetlb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/quicklist.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include "internal.h"

#include <linux/cmainfo.h>

void __attribute__((weak)) arch_report_cmainfo(struct seq_file *m)
{
}

static int cmainfo_proc_show(struct seq_file *m, void *v)
{
	cmainfo_t cmainfo;
	unsigned int i;
	unsigned int midr = 0;

	/*
	 * display in kilobytes.
	*/
	#define K(x) ((x) << (PAGE_SHIFT - 10))

	memset(&cmainfo, 0, sizeof(cmainfo_t));

	/*
	 * It updates value of cma_active_anon, cma_inactive_anon,
	 * cma_active_file, cma_inactive_file counter.
	 */
	cma_mem_info(&cmainfo);
	cma_page_type_det(&cmainfo);

	 __asm__ __volatile__("mrc p15, 0, %0, c2, c0, 0" : "=r"(midr));
	seq_printf(m,
		"MemTotal :					%8lu kB \n"
		"MemFree:					%8lu kB \n"
		"Active						%8lu kB \n"
		"Inactive					%8lu kB \n"
		"Active(anon)				%8lu kB \n"
		"Inactive(anon)				%8lu kB \n"
		"Active(file)				%8lu kB \n"
		"Inactive(file)				%8lu kB \n"
		,
		cmainfo.totalcma,
		K(global_page_state(NR_FREE_CMA_PAGES)),
		(cmainfo.cma_active_anon + cmainfo.cma_active_file) * 4,
		(cmainfo.cma_inactive_anon + cmainfo.cma_inactive_file) * 4,
		(cmainfo.cma_active_anon * 4),
		(cmainfo.cma_inactive_anon * 4),
		(cmainfo.cma_active_file * 4),
		(cmainfo.cma_inactive_file * 4)
		);

	for(i = 0; i < cmainfo.nr_cma_areas; i++) {
		seq_printf(m,
		"\nphys_addr_range(%s):			0x%08lx - 0x%08lx  \n"
		"virt_addr_range(%s):			0x%08lx - 0x%08lx  \n"
		,
		(cmainfo.cma_areas[i].flag == CMA_AREA_DEVICE)?"dev":"gbl",
		cmainfo.cma_areas[i].cma_phy_start,
		cmainfo.cma_areas[i].cma_phy_end,
		(cmainfo.cma_areas[i].flag == CMA_AREA_DEVICE)?"dev":"gbl",
		phys_to_virt(cmainfo.cma_areas[i].cma_phy_start),
		phys_to_virt(cmainfo.cma_areas[i].cma_phy_end)
		);

		cma_range_populate(
			phys_to_virt(cmainfo.cma_areas[i].cma_phy_start),
			phys_to_virt(cmainfo.cma_areas[i].cma_phy_end));

		cma_walk_pgd(m);
	}

	arch_report_cmainfo(m);

	return 0;

#undef K
}


static int cmainfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmainfo_proc_show, NULL);
}

static const struct file_operations cmainfo_proc_fops = {
	.open		= cmainfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmainfo_init(void)
{
	proc_create("cmainfo", 0, NULL, &cmainfo_proc_fops);
	return 0;
}

module_init(proc_cmainfo_init);
