// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2016-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/percpu.h>

#include <debug_kinfo.h>
#include <linux/samsung/debug/qcom/sec_qc_summary.h>

#include "sec_qc_summary.h"

static void __summary_kconst_init_common(
		struct qc_summary_drvdata *drvdata)
{
	struct sec_qc_summary_kconst *kconst =
			&(secdbg_apss(drvdata)->kconst);

	kconst->nr_cpus = num_possible_cpus();
	kconst->page_offset = PAGE_OFFSET;
	kconst->vmap_stack = !!IS_ENABLED(CONFIG_VMAP_STACK);

#if IS_ENABLED(CONFIG_ARM64)
	kconst->phys_offset = PHYS_OFFSET;
	kconst->va_bits = VA_BITS;
	kconst->kimage_vaddr = kimage_vaddr;
	kconst->kimage_voffset = kimage_voffset;
#endif

#if IS_ENABLED(CONFIG_SMP)
	kconst->per_cpu_offset.pa = virt_to_phys(__per_cpu_offset);
	kconst->per_cpu_offset.size = sizeof(__per_cpu_offset[0]);
	kconst->per_cpu_offset.count = ARRAY_SIZE(__per_cpu_offset);
#endif
}

static void __summary_kconst_init_builtin(
		struct qc_summary_drvdata *drvdata)
{
	struct sec_qc_summary_kconst *kconst =
			&(secdbg_apss(drvdata)->kconst);

	kconst->swapper_pg_dir_paddr = __pa_symbol(swapper_pg_dir);
}

static inline void __summary_kconst_init_module(
		struct qc_summary_drvdata *drvdata)
{
	struct sec_qc_summary_kconst *kconst =
			&(secdbg_apss(drvdata)->kconst);
	const struct kernel_all_info *all_kinfo =
			drvdata->debug_kinfo_rmem->priv;
	const struct kernel_info *kinfo = &(all_kinfo->info);

	kconst->swapper_pg_dir_paddr = kinfo->swapper_pg_dir_pa;
}

int notrace __qc_summary_kconst_init(struct builder *bd)
{
	struct qc_summary_drvdata *drvdata =
			container_of(bd, struct qc_summary_drvdata, bd);

	__summary_kconst_init_common(drvdata);

	if (IS_BUILTIN(CONFIG_SEC_QC_SUMMARY))
		__summary_kconst_init_builtin(drvdata);
	else
		__summary_kconst_init_module(drvdata);

	return 0;
}
