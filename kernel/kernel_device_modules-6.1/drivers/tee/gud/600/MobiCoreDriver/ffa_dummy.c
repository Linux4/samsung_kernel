// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include "platform.h"
#include "main.h"
#include "ffa.h"
#include "mmu.h"
#include "mmu_internal.h"

#ifdef MC_FFA_FASTCALL
#include <linux/arm_ffa.h>
#include "public/trustonic_ffa_api.h"

int ffa_module_probe(void)
{
	mc_dev_info("%s: dummy", __func__);
	return 0;
}

int ffa_share_mci_buffer(const void *mci, size_t n_cont_pages, u64 *ffa_handle)
{
	return 0;
}

int ffa_share_trace_buffer(phys_addr_t buffer_phys, u32 size, u64 *ffa_handle)
{
	return 0;
}

int ffa_register_buffer(struct page **pages, struct tee_mmu *mmu, u64 tag)
{
	return 0;
}

int ffa_reclaim_buffer(struct tee_mmu *mmu)
{
	return 0;
}

inline int ffa_fastcall(union fc_common *fc)
{
	return 0;
}

int ffa_register_module(void)
{
	return 0;
}

void ffa_unregister_module(void)
{
}

int tt_ffa_memory_lend(struct ffa_mem_ops_args *args)
{
	return 0;
}
EXPORT_SYMBOL(tt_ffa_memory_lend);

int tt_ffa_memory_reclaim(u64 handle, u64 tag)
{
	return 0;
}
EXPORT_SYMBOL(tt_ffa_memory_reclaim);

#endif /* MC_FFA_FASTCALL */
