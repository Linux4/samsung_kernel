/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 */

#ifndef MTK_DEFERRED_FREE_HELPER_H
#define MTK_DEFERRED_FREE_HELPER_H

/**
 * df_reason - enum for reason why item was freed
 *
 * This provides a reason for why the free function was called
 * on the item. This is useful when deferred_free is used in
 * combination with a pagepool, so under pressure the page can
 * be immediately freed.
 *
 * MTK_DF_NORMAL:         Normal deferred free
 *
 * MTK_DF_UNDER_PRESSURE: Free was called because the system
 *                        is under memory pressure. Usually
 *                        from a shrinker. Avoid allocating
 *                        memory in the free call, as it may
 *                        fail.
 */
enum mtk_df_reason {
	MTK_DF_NORMAL,
	MTK_DF_UNDER_PRESSURE,
};

/**
 * mtk_deferred_freelist_item - item structure for deferred freelist
 *
 * This is to be added to the structure for whatever you want to
 * defer freeing on.
 *
 * @nr_pages: number of pages used by item to be freed
 * @free: function pointer to be called when freeing the item
 * @list: list entry for the deferred list
 */
struct mtk_deferred_freelist_item {
	size_t nr_pages;
	void (*free)(struct mtk_deferred_freelist_item *i,
		     enum mtk_df_reason reason);
	struct list_head list;
};

/**
 * mtk_deferred_free - call to add item to the deferred free list
 *
 * @item: Pointer to deferred_freelist_item field of a structure
 * @free: Function pointer to the free call
 * @nr_pages: number of pages to be freed
 */
void mtk_deferred_free(struct mtk_deferred_freelist_item *item,
		   void (*free)(struct mtk_deferred_freelist_item *i,
				enum mtk_df_reason reason),
		   size_t nr_pages);

int mtk_deferred_freelist_init(void);

#endif
