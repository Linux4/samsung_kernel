/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021-2022 TRUSTONIC LIMITED
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

#ifndef KINIBI_FFA_H
#define KINIBI_FFA_H

#ifdef MC_FFA_FASTCALL

#include <linux/interrupt.h>

#include "mmu.h"
#include "fastcall.h"

inline int ffa_fastcall(union fc_common *fc);

int ffa_register_module(void);
void ffa_unregister_module(void);
int ffa_module_probe(void);

#ifdef MC_FFA_NOTIFICATION
int ffa_register_notif_handler(irq_handler_t handler);
#endif
int ffa_share_trace_buffer(phys_addr_t buffer, u32 size, u64 *ffa_handle);
int ffa_share_mci_buffer(const void *mci, size_t n_cont_pages, u64 *ffa_handle);
int ffa_register_buffer(struct page **pages, struct tee_mmu *mmu, u64 tag);
int ffa_reclaim_buffer(struct tee_mmu *mmu);

#endif /* MC_FFA_FASTCALL */

#endif /* KINIBI_FFA_H */
