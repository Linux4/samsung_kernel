/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TBASE_FASTCALL_H
#define TBASE_FASTCALL_H

#include "mmu.h"

struct fc_s_yield {
	u32	resp;
	u32	ret;
	u32	code;
};

/* Base for all fastcalls, do not use outside of other structs */
union fc_common {
	struct {
		u32 cmd;
		u32 param[3];
	} in;

	struct {
		u32 resp;
		u32 ret;
		u32 param[2];
	} out;
};

int fc_sched_init(phys_addr_t buffer, u32 size);
int fc_mci_init(uintptr_t phys_addr, u32 page_count);
int fc_info(u32 ext_info_id, u32 *state, u32 *ext_info);
int fc_trace_init(phys_addr_t buffer, u32 size);
int fc_trace_set_level(u32 level);
int fc_trace_deinit(void);
int fc_vm_destroy(void);
int fc_nsiq(u32 session_id, u32 payload);
int fc_yield(u32 session_id, u32 payload, struct fc_s_yield *resp);
int fc_cpu_off(void);

int fc_register_buffer(struct page **pages, struct tee_mmu *mmu, u64 tag);
void fc_reclaim_buffer(struct tee_mmu *mmu);

int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf);

#endif /* TBASE_FASTCALL_H */
