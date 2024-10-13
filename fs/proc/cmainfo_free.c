/*
 * =============================================================================
 *
 *       Filename:  cmainfo_free.c
 *
 *      description: displays the cma free memory information
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

void __attribute__((weak)) arch_report_cmainfo_free(struct seq_file *m)
{
}

static int cmainfo_free_proc_show(struct seq_file *m, void *v)
{
	cmainfo_t cmainfo;
	unsigned int i;

	/*
	 * display in kilobytes.
	 */
	#define K(x) ((x) << (PAGE_SHIFT - 10))

	memset(&cmainfo, 0, sizeof(cmainfo_t));

	cma_mem_info(&cmainfo);

	seq_printf(m,
		"CMA_free:				%8lu kB  \n",
		K(global_page_state(NR_FREE_CMA_PAGES))
		);

	for(i = 0; i < cmainfo.nr_cma_areas; i++) {
		seq_printf(m,
		"phys_addr_range(%s):		0x%08lx - 0x%08lx  \n"
		,
		(cmainfo.cma_areas[i].flag == CMA_AREA_DEVICE) ? "dev" : "gbl",
		cmainfo.cma_areas[i].cma_phy_start,
		cmainfo.cma_areas[i].cma_phy_end
		);
	}

	cmatypeinfo_showfree_print(m, cmainfo.totalcma);
	arch_report_cmainfo_free(m);

	return 0;

#undef K
}

static int cmainfo_free_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmainfo_free_proc_show, NULL);
}

static const struct file_operations cmainfo_free_proc_fops = {
	.open		= cmainfo_free_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmainfo_free_init(void)
{
	proc_create("cmainfo_free", 0, NULL, &cmainfo_free_proc_fops);
	return 0;
}

module_init(proc_cmainfo_free_init);
