/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * DMA BUF PagePool implementation
 * Based on earlier ION code by Google
 *
 */

#ifndef _MTK_SEC_HEAP_H
#define _MTK_SEC_HEAP_H

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

#define PMM_MSG_ORDER_SHIFT (24UL)
#define PMM_MSG_ENTRY(pa, page_order)                                          \
	((pa >> PAGE_SHIFT) | (page_order << PMM_MSG_ORDER_SHIFT))
#define GET_PMM_ENTRY_PA(entry)                                                \
	((entry & ((1UL << PMM_MSG_ORDER_SHIFT) - 1)) << PAGE_SHIFT)
#define PMM_MSG_ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(uint32_t))

#define HYP_PMM_ASSIGN_BUFFER_V2 (0XBB00FFAB)
#define HYP_PMM_UNASSIGN_BUFFER_V2 (0XBB00FFAC)
#define HYP_PMM_MERGED_TABLE (0XBB00FFAD)

#endif /* _MTK_SEC_HEAP_H */
