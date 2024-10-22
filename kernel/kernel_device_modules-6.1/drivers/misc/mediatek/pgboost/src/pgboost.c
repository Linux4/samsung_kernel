// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/cma.h>
#include <cma.h>
#include <linux/huge_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/sched/clock.h>
#include <soc/mediatek/emi.h>
#include <inc/pgboost.h>
#include <inc/ksym.h>

/*
 * It only works for systems with the following conditions -
 * 1. DRAM with dual ranks
 * 2. The system is single memory zone
 */

#define RANKS_COUNT	(2)

/* Ready status */
static bool pgboost_ready __ro_after_init;

/* Rank information */
struct rank_info {
	unsigned long start_pfn;
	unsigned long end_pfn;		/* The one after its last pfn */
	unsigned long free;		/* number of free pages */
};

/* Frequently-used data */
static struct zone *zone __ro_after_init;
static unsigned long zone_start_pfn __ro_after_init;
static unsigned long rank_boundary_pfn __ro_after_init;
static unsigned long rank_boundary_phys __ro_after_init;
static int zone_batch __ro_after_init;
static struct rank_info rank_info[RANKS_COUNT];

#ifdef PGBOOST_STRATEGY_HOLD_SPG
/* Private structures for small pages with MIGRATE_MOVABLE */
static struct list_head spg_list_0[HPAGE_PMD_ORDER];
static struct list_head spg_list_1[HPAGE_PMD_ORDER];
static unsigned long spg_nr_0[HPAGE_PMD_ORDER];
static unsigned long spg_nr_1[HPAGE_PMD_ORDER];
#ifdef DEBUG_PGBOOST_ENABLED
module_param_array_named(status_hold_small_pages_rank0, spg_nr_0, ulong, NULL, 0644);
module_param_array_named(status_hold_small_pages_rank1, spg_nr_1, ulong, NULL, 0644);
#endif
static DEFINE_SPINLOCK(spg_lock);
#endif

/* Delayed work with periodicity, the default kick interval is 10s */
static struct delayed_work pgboost_periodic_work;
static unsigned int period_intval = 10;	/* Kick interval, in HZ */
module_param_named(period_intval_hz, period_intval, uint, 0644);
static unsigned int timer_intval = 10;	/* Default CD time for periodic work, in HZ */
module_param_named(cd_timer_intval_hz, timer_intval, uint, 0644);

/* Delayed event work for hooked tracepoints */
static struct delayed_work pgboost_work;
static unsigned int etrig_intval = 1;	/* Default CD time for event work, in HZ */
module_param_named(cd_etrig_intval_hz, etrig_intval, uint, 0644);

/* For probing interesting tracepoints */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
};

/* Controls */
static bool force_drain;
static bool curr_favor_rank1;
static bool pgboost_in_progres;
static atomic_long_t pgs_violate_ranks;
#define LEAST_TOTAL_PGS	(0x40000)

/* Helpers */
static inline bool phys_in_interesting_rank(unsigned long phys, bool favor_rank1)
{
	return favor_rank1 ? (phys >= rank_boundary_phys) : (phys < rank_boundary_phys);
}

static inline bool phys_not_in_interesting_rank(unsigned long phys, bool favor_rank1)
{
	return favor_rank1 ? (phys < rank_boundary_phys) : (phys >= rank_boundary_phys);
}

static inline bool pfn_in_interesting_rank(unsigned long pfn, bool favor_rank1)
{
	return favor_rank1 ? (pfn >= rank_boundary_pfn) : (pfn < rank_boundary_pfn);
}

static inline bool pfn_not_in_interesting_rank(unsigned long pfn, bool favor_rank1)
{
	return favor_rank1 ? (pfn < rank_boundary_pfn) : (pfn >= rank_boundary_pfn);
}

/* Show the status of list ordering */
static int dump_list_rank_layout(int mt, char *buffer)
{
	unsigned long flags;
	struct page *page;
	unsigned long pfn;
	unsigned long cnt = 0;
	int order;
	int prev_rank = -1, curr_rank;
	int result = 0, ret;
	char layout[1024];
	char *output = (buffer != NULL) ? buffer : layout;

	ret = sprintf(output, "[migrate_type:%d]\n", mt);
	if (ret < 0)
		goto exit;
	result += ret;

	spin_lock_irqsave(&zone->lock, flags);

	for (order = MAX_ORDER - 1; order >= 0; order--) {

		ret = sprintf(output + result, "[order-%2d]:", order);
		if (ret < 0)
			goto exit;
		result += ret;

		list_for_each_entry(page, &zone->free_area[order].free_list[mt],
				buddy_list) {
			pfn = page_to_pfn(page);

			if (pfn_in_interesting_rank(pfn, false))
				curr_rank = 0;
			else
				curr_rank = 1;

			/* update cnt, rank info and write result */
			if (curr_rank == prev_rank) {
				cnt++;
			} else {
				if (cnt != 0 && result < 1000) {
					ret = sprintf(output + result, "(%d)%lu-",
							prev_rank, cnt);
					if (ret < 0)
						goto exit;
					result += ret;
				}

				/* the new rank's 1st */
				cnt = 1;
				prev_rank = curr_rank;
			}
		}

		/* write out the last result */
		if (cnt != 0 && result < 1000) {
			ret = sprintf(output + result, "(%d)%lu", prev_rank, cnt);
			if (ret < 0)
				goto exit;
			result += ret;
		}

		/* add newline */
		ret = sprintf(output + result, "\n");
		if (ret < 0)
			goto exit;
		result += ret;

		/* reset */
		cnt = 0;
		prev_rank = -1;
	}

	spin_unlock_irqrestore(&zone->lock, flags);

	/* output trailing character */
	output[result] = '\0';

	if (buffer == NULL)
		pr_info("%s: %s\n", __func__, output);

exit:
	return result;
}

#ifdef DEBUG_PGBOOST_ENABLED
static void show_zone_pcp_status(void)
{
	int i;

	for_each_online_cpu(i) {
		struct per_cpu_pages *pcp;

		pcp = per_cpu_ptr(zone->per_cpu_pageset, i);
		PGBOOST_DEBUG("cpu(%i): count(%i) high(%i) batch(%i)",
				i, pcp->count, pcp->high, pcp->batch);
	}
}
#else
static void show_zone_pcp_status(void) {; }
#endif

/* Drain pcp pages through cma_alloc */
static void try_to_drain_pcp_pages(void)
{
	struct page *page = NULL;

	PGBOOST_DEBUG("%s:+++\n", __func__);
	show_zone_pcp_status();

	page = cma_alloc(dev_get_cma_area(NULL), 1, 0, true);
	if (!page)
		return;

	if (!cma_release(dev_get_cma_area(NULL), page, 1))
		pr_info("%s: failed to release cma page\n", __func__);

	show_zone_pcp_status();
	PGBOOST_DEBUG("%s:---\n", __func__);
}

/* Imitate set_pfnblock_flags_mask */
static void change_pageblock_migrate_type(struct page *page, unsigned long mt,
					unsigned long pfn, unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	bitmap = section_to_usemap(__pfn_to_section(pfn));/* CONFIG_SPARSEMEM */

	pfn &= (PAGES_PER_SECTION-1);/* pfn_to_bitidx, CONFIG_SPARSEMEM */
	bitidx = (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;

	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG-1);

	mask <<= bitidx;
	mt <<= bitidx;

	word = READ_ONCE(bitmap[word_bitidx]);
	do {
	} while (!try_cmpxchg(&bitmap[word_bitidx], &word, (word & ~mask) | mt));
}

static void change_migrate_type(int mt, bool favor_rank1)
{
#define MAX_TRIES	(9)
#define EB_TRIES	(2)
#define MT_BATCH	(64)

	unsigned long flags;
	struct page *page, *tmp;
	unsigned long pfn;
	int order = MAX_ORDER - 1;
	int tried = 0, changed = 0, slot = 0;
	bool positive_seq = false; /* works on <= HPAGE_PMD_ORDER */
	LIST_HEAD(list);

	/* force drain all pages for every 1st mt change at specific order */
	WRITE_ONCE(force_drain, true);

	do {
		spin_lock_irqsave(&zone->lock, flags);

		/* Check pages NOT in interesting rank */
		list_for_each_entry_safe(page, tmp, &zone->free_area[order].free_list[mt],
				buddy_list) {
			pfn = page_to_pfn(page);

			if (pfn_not_in_interesting_rank(pfn, favor_rank1)) {
				/* Change migrate type and isolate it into list */
				change_pageblock_migrate_type(page, MIGRATE_MOVABLE, pfn,
						MIGRATETYPE_MASK);
				list_move_tail(&page->buddy_list, &list);
				if (++slot == MT_BATCH)
					break;
			}
		}

		/*
		 * Add the isolated pages to the movable list head to
		 * make uninteresting ones be allocated firstly before
		 * the completion of this pgboost.
		 * The remaining uninteresting ones will be reordered to tail when
		 * reordering MIGRATE_MOVABLE list.
		 */
		list_splice_init(&list, &zone->free_area[order].free_list[MIGRATE_MOVABLE]);

		spin_unlock_irqrestore(&zone->lock, flags);

		/* Early termination */
		if (slot == 0)
			break;

		changed += slot;

		/* Early break if no progress */
		if (tried >= EB_TRIES && changed == 0)
			break;

		/* Reset slot */
		slot = 0;

		/* Next trial */
	} while (++tried < MAX_TRIES);

	PGBOOST_DEBUG("%s: order(%d), migrate_type(%d), tried(%d), changed(%d)\n", __func__,
			order, mt, tried, changed);

	/* Try orders <= HPAGE_PMD_ORDER */
	order = HPAGE_PMD_ORDER;
	do {
		/* Sanity check */
		if (!list_empty(&list)) {
			pr_info("%s: the list should be empty here\n", __func__);
			break;
		}

		/* Reset pfn */
		pfn = 0;

		spin_lock_irqsave(&zone->lock, flags);

		/* Find pages in the interesting rank */
		list_for_each_entry_safe(page, tmp, &zone->free_area[order].free_list[mt],
				buddy_list) {
			pfn = page_to_pfn(page);

			if (pfn_in_interesting_rank(pfn, favor_rank1))
				list_move_tail(&page->buddy_list, &list);
		}

		/*
		 * Move list -
		 * Moving to the list head if positive_seq is true,
		 * else moving to the tail.
		 */
		if (positive_seq)
			list_splice_init(&list, &zone->free_area[order].free_list[mt]);
		else
			list_splice_tail_init(&list, &zone->free_area[order].free_list[mt]);

		spin_unlock_irqrestore(&zone->lock, flags);

		/* Flip positive_seq if pfn is not zero */
		if (pfn != 0)
			positive_seq = !positive_seq;

	} while (--order >= 0);

#ifdef DEBUG_PGBOOST_ENABLED
	dump_list_rank_layout(mt, NULL);
#endif

#undef MT_BATCH
#undef EB_TRIES
#undef MAX_TRIES
}

#ifdef PGBOOST_STRATEGY_HOLD_SPG
static unsigned long release_small_pages(struct list_head *spg_list,
			unsigned long *spg_nr, unsigned long total_freed,
			unsigned long nr_to_scan)
{
	unsigned long order, freed = 0;
	struct page *page, *tmp;
	LIST_HEAD(list);

	/* Try to release cached objects */
	for (order = 0; order < HPAGE_PMD_ORDER; order++) {

		spin_lock(&spg_lock);

		/* isolate them */
		list_for_each_entry_safe(page, tmp, &spg_list[order], lru) {
			list_move_tail(&page->lru, &list);
			spg_nr[order]--;
			freed++;
		}

		spin_unlock(&spg_lock);

		/* free them */
		list_for_each_entry_safe(page, tmp, &list, lru) {
			list_del(&page->lru);
			__free_pages(page, order);
		}

		/* update NR_KERNEL_MISC_RECLAIMABLE & total_freed and reset freed */
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
				-(long)freed << order);
		total_freed += (freed << order);
		freed = 0;

		/* early break */
		if (total_freed >= nr_to_scan)
			break;
	}

	return total_freed;
}
#endif

#define SPG_GFP_FLAGS	((GFP_KERNEL | __GFP_MOVABLE) & ~__GFP_RECLAIM)
/*
 * This function will try to move huge pages or above
 * in the interesting rank to the list head.
 * If pick_small is true, it will move small pages in
 * the interesting rank to the list tail, and try to
 * do allocation(of uninteresting one).
 */
static void do_pgboost_list_reordering(bool pick_small, bool favor_rank1)
{
	unsigned long flags;
	struct page *page, *tmp;
	unsigned long pfn;
	int order = MAX_ORDER - 1;
	int end_order = pick_small ? 0 : HPAGE_PMD_ORDER;

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	int changed = 0, count = 0;
	int movable_free[HPAGE_PMD_ORDER];
	unsigned long spg_size = 0;
	struct list_head *spg_list = NULL;
	unsigned long *spg_nr = NULL;
#else
	bool positive_seq = false; /* works on < HPAGE_PMD_ORDER */
#endif
	LIST_HEAD(list);

	WRITE_ONCE(pgboost_in_progres, true);

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	/* Putback interesting rank's small pages firstly */
	if (favor_rank1)
		release_small_pages(spg_list_1, spg_nr_1, 0, ULONG_MAX);
	else
		release_small_pages(spg_list_0, spg_nr_0, 0, ULONG_MAX);
#endif

	/* Drain pcp pages before change migrate type */
	try_to_drain_pcp_pages();

	/* Try to change MIGRATE_UNMOVABLE to MIGRATE_MOVABLE */
	change_migrate_type(MIGRATE_UNMOVABLE, favor_rank1);

	/* Try to change MIGRATE_RECLAIMABLE to MIGRATE_MOVABLE */
	change_migrate_type(MIGRATE_RECLAIMABLE, favor_rank1);

	/* Drain pcp pages before refordering */
	try_to_drain_pcp_pages();

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	/* Reordering the lists of MIGRATE_MOVABLE */
	do {
		spin_lock_irqsave(&zone->lock, flags);

		/* Find pages in the interesting rank */
		list_for_each_entry_safe(page, tmp,
				&zone->free_area[order].free_list[MIGRATE_MOVABLE], buddy_list) {
			pfn = page_to_pfn(page);

			if (pfn_in_interesting_rank(pfn, favor_rank1)) {
				list_move_tail(&page->buddy_list, &list);
				changed++;
			}

			/* Count for free */
			count++;
		}

		/*
		 * Move list -
		 * order >= HPAGE_PMD_ORDER: move to the list head
		 * others: move to the list tail
		 */
		if (order >= HPAGE_PMD_ORDER) {
			list_splice_init(&list, &zone->free_area[order].free_list[MIGRATE_MOVABLE]);
		} else {
			list_splice_tail_init(&list,
					&zone->free_area[order].free_list[MIGRATE_MOVABLE]);
			movable_free[order] = count - changed;
			spg_size += (movable_free[order] << order);
		}

		spin_unlock_irqrestore(&zone->lock, flags);

		PGBOOST_DEBUG("%s: changed(%d) for order(%d)-nrfree(%d)\n",
				__func__, changed, order, count);

		/* Sanity check */
		if (!list_empty(&list)) {
			pr_info("%s: the list should be empty here\n", __func__);
			goto exit;
		}

		/* Reset changed & count */
		changed = 0;
		count = 0;

	} while (--order >= end_order);

	/* Update what the current rank is */
	WRITE_ONCE(curr_favor_rank1, favor_rank1);

	/* Reset page violation status (how many pages are polluting reordered lists) */
	atomic_long_set(&pgs_violate_ranks, 0);

	if (!pick_small)
		goto exit;

	/* Collect small pages if the total size is small (<= 1024MB) */
	if (spg_size > LEAST_TOTAL_PGS) {
		pr_info("%s: don't collect small pages, total size(%lu)\n",
				__func__, spg_size * PAGE_SIZE);
		goto exit;
	}

	/* Decide where we can put small pages */
	if (favor_rank1) {
		/* interesting rank is rank#1, so pick rank#0 small pages */
		spg_list = spg_list_0;
		spg_nr = spg_nr_0;
	} else {
		/* interesting rank is rank#0, so pick rank#1 small pages */
		spg_list = spg_list_1;
		spg_nr = spg_nr_1;
	}

	/* Pick them through alloc_pages */
	for (order = 0; order < HPAGE_PMD_ORDER; order++) {
		for (count = 0; count < movable_free[order]; count++) {
			page = alloc_pages(SPG_GFP_FLAGS, order);
			if (!page)
				goto abort;

			/*
			 * If the page allocated is not the interesting one,
			 * free it and break the internal loop
			 */
			pfn = page_to_pfn(page);
			if (pfn_in_interesting_rank(pfn, favor_rank1)) {
				__free_pages(page, order);
				break;
			}

			/* Add to spg list */
			spin_lock(&spg_lock);
			list_add_tail(&page->lru, &spg_list[order]);
			spg_nr[order]++;
			spin_unlock(&spg_lock);

			/* update NR_KERNEL_MISC_RECLAIMABLE */
			mod_node_page_state(page_pgdat(page),
					NR_KERNEL_MISC_RECLAIMABLE, 1 << order);
		}

		PGBOOST_DEBUG("%s: small pages @order(%d)-nr(%lu) collected\n",
				__func__, order, spg_nr[order]);
	}

	goto exit;

abort:
	pr_info("%s: abort collecting small pages @order(%d)-count(%d)\n",
			__func__, order, count);

#else	/* !defined(PGBOOST_STRATEGY_HOLD_SPG) */

	/* Reordering the lists of MIGRATE_MOVABLE */
	do {
		/* Reset pfn */
		pfn = 0;

		spin_lock_irqsave(&zone->lock, flags);

		/* Find pages in the interesting rank */
		list_for_each_entry_safe(page, tmp,
				&zone->free_area[order].free_list[MIGRATE_MOVABLE], buddy_list) {
			pfn = page_to_pfn(page);

			if (pfn_in_interesting_rank(pfn, favor_rank1))
				list_move_tail(&page->buddy_list, &list);
		}

		/*
		 * Move list -
		 * order >= HPAGE_PMD_ORDER: move to the list head
		 * others: move to position according positive_seq
		 */
		if (order >= HPAGE_PMD_ORDER) {
			list_splice_init(&list, &zone->free_area[order].free_list[MIGRATE_MOVABLE]);
		} else {
			/*
			 * Moving to the list head if positive_seq is true,
			 * else moving to the tail.
			 */
			if (positive_seq)
				list_splice_init(&list,
						&zone->free_area[order].free_list[MIGRATE_MOVABLE]);
			else
				list_splice_tail_init(&list,
						&zone->free_area[order].free_list[MIGRATE_MOVABLE]);

			/* Flip positive_seq if pfn is not zero */
			if (pfn != 0)
				positive_seq = !positive_seq;
		}

		spin_unlock_irqrestore(&zone->lock, flags);

		/* Sanity check */
		if (!list_empty(&list)) {
			pr_info("%s: the list should be empty here\n", __func__);
			goto exit;
		}

	} while (--order >= end_order);

	/* Update what the current rank is */
	WRITE_ONCE(curr_favor_rank1, favor_rank1);

	/* Reset page violation status (how many pages are polluting reordered lists) */
	atomic_long_set(&pgs_violate_ranks, 0);
#endif

exit:
	WRITE_ONCE(pgboost_in_progres, false);
}

/*
 * Profile current "HUGE" page distribution over ranks to determine the next action.
 * When the size of HUGE pages is greater than 1GB(precondition) and one of the
 * following conditions is matched, then return FAVOR_RANK_#. Others return NO_FAVOR,
 * 1. rank# has 1GB more page than the other rank.
 * 2. rank# occupies the most part (>75%).
 * 3. A lot of other rank pages pollute in the list head (>25%, iterating the first
 *    zone_batch pages).
 *
 * force: just consider the comparison result of ranks' sizes
 */
static enum pgboost_action next_pgboost_action(bool force)
{
	unsigned long current_free = global_zone_page_state(NR_FREE_PAGES) -
					global_zone_page_state(NR_FREE_CMA_PAGES);
	unsigned long flags;
	struct page *page;
	int mt;
	int order = MAX_ORDER - 1;
	int end_order = HPAGE_PMD_ORDER;
	unsigned long stride = MAX_ORDER_NR_PAGES;

	/* For condition 1 & 2 */
	unsigned long pgcnt_in_rank1 = 0, pgcnt_total = 0;

	/* For condition 3 */
	int cnt, lh_rank1_pgcnt;
	bool lh_rank0 = false, lh_rank1 = false, favor_rank1;

	/* Early break if we don't have enough free pages */
	if (current_free <= LEAST_TOTAL_PGS)
		return NO_FAVOR;

	spin_lock_irqsave(&zone->lock, flags);

	do {
		/* Iterating free lists to count the number of HUGE pages */
		for (mt = MIGRATE_UNMOVABLE; mt < MIGRATE_PCPTYPES; mt++) {

			/* Reset list head status for next iterating */
			cnt = 0;
			lh_rank1_pgcnt = 0;

			/* Iterating at most zone_batch pages */
			list_for_each_entry(page, &zone->free_area[order].free_list[mt],
					buddy_list) {

				if (page_to_pfn(page) >= rank_boundary_pfn) {
					pgcnt_in_rank1 += stride;
					lh_rank1_pgcnt++;
				}

				pgcnt_total += stride;

				/* Break if cnt hits the batch */
				if (++cnt == zone_batch)
					break;
			}

			/* Identify the list head status */
			if (lh_rank1_pgcnt > (cnt >> 2))
				lh_rank1 = true;
			else if (lh_rank1_pgcnt < (cnt >> 2) * 3)
				lh_rank0 = true;

			/* Iterating is finished*/
			if (list_entry_is_head(page,
						&zone->free_area[order].free_list[mt], buddy_list))
				continue;

			/* Iterating remaining pages */
			list_for_each_entry_continue(page,
					&zone->free_area[order].free_list[mt], buddy_list) {

				if (page_to_pfn(page) >= rank_boundary_pfn)
					pgcnt_in_rank1 += stride;

				pgcnt_total += stride;
			}
		}

		/* Update stride for the next order */
		stride >>= 1;

	} while (--order >= end_order);

	spin_unlock_irqrestore(&zone->lock, flags);

	PGBOOST_DEBUG("%s: pgcnt_rank1(%lu) pgcnt_total(%lu) rank(%d)(%d) curr_favor_rank1(%d)\n",
			__func__, pgcnt_in_rank1, pgcnt_total, lh_rank0, lh_rank1,
			READ_ONCE(curr_favor_rank1));

	/* Record rank page counts */
	rank_info[0].free = pgcnt_total - pgcnt_in_rank1;
	rank_info[1].free = pgcnt_in_rank1;

	/* Check conditions */
	if (!force) {
		/* Precondition - The size of huge pages should be large enough */
		if (pgcnt_total <= LEAST_TOTAL_PGS)
			return NO_FAVOR;

		/* Condition 1 - Identify interesting rank by size */
		if (pgcnt_in_rank1 >= (pgcnt_total - pgcnt_in_rank1 + LEAST_TOTAL_PGS))
			return FAVOR_RANK_1;
		else if ((pgcnt_in_rank1 + LEAST_TOTAL_PGS) <= (pgcnt_total - pgcnt_in_rank1))
			return FAVOR_RANK_0;

		/* Condition 2 - Identify interesting rank by percentage */
		if (pgcnt_in_rank1 > (pgcnt_total >> 2) * 3)
			return FAVOR_RANK_1;
		else if (pgcnt_in_rank1 < (pgcnt_total >> 2))
			return FAVOR_RANK_0;

		/* Condition 3 - Identify the pollution status */
		favor_rank1 = READ_ONCE(curr_favor_rank1);
		if (lh_rank0 && favor_rank1)
			return FAVOR_RANK_1 + FAVOR_STRICT;
		else if (lh_rank1 && !favor_rank1)
			return FAVOR_RANK_0 + FAVOR_STRICT;
	} else {
		if (pgcnt_in_rank1 >= (pgcnt_total - pgcnt_in_rank1))
			return FAVOR_RANK_1;
		else
			return FAVOR_RANK_0;
	}

	/* No interesting ranks */
	return NO_FAVOR;
}

static unsigned long get_vm_event(enum vm_event_item event)
{
	unsigned long result = 0;
	int cpu;

	for_each_online_cpu(cpu) {
		struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

		result += this->event[event];
	}

	return result;
}

/* Main entry of gathering pages */
static void __do_pgboost(bool force, bool light)
{
	bool favor_rank1 = READ_ONCE(curr_favor_rank1);
	enum pgboost_action next_action;
	unsigned long long t0, t1, t2;

	t0 = sched_clock();

	next_action = next_pgboost_action(force);

	/* Check action level firstly */
	if (next_action & FAVOR_STRICT) {
		next_action &= FAVOR_MASK;
	} else if (!force) {
		if (favor_rank1 && (next_action == FAVOR_RANK_1))
			next_action = NO_FAVOR;
		else if (!favor_rank1 && (next_action == FAVOR_RANK_0))
			next_action = NO_FAVOR;
	}

	t1 = sched_clock();

	switch (next_action) {
	case FAVOR_RANK_0:
		do_pgboost_list_reordering(!light, false);
		break;
	case FAVOR_RANK_1:
		do_pgboost_list_reordering(!light, true);
		break;
	case NO_FAVOR:
		break;
	default:
		pr_info("%s: unexpected action!\n", __func__);
		break;
	}

	t2 = sched_clock();

	PGBOOST_DEBUG("%s: action(%d) done, (%llu:%llu) nsec elapsed!\n",
			__func__, next_action, t1 - t0, t2 - t1);
}

/*
 * Entry to pgboost with the limitation of triggering frequency.
 *
 * cd_intval: minimum CD time since the last triggering
 * force: tell pgboost to only depend on the comparision of ranks
 * light: a hint to tell pgboost paying less effort, which affects
 *        some underlying behaviors in __do_pgboost.
 *        If true, it will not check pcp free lists.
 */
static void do_pgboost(unsigned long long cd_intval, bool force, bool light)
{
	static unsigned long long prev_pgboost_time;
	unsigned long long t0, t1;

	if (!pgboost_ready)
		return;

	/* Add the COOL-DOWN time to limit the calling frequency */
	t0 = sched_clock();
	t1 = READ_ONCE(prev_pgboost_time) + cd_intval;
	if (t0 < t1)
		return;

	/* Start pgboost */
	__do_pgboost(force, light);
	WRITE_ONCE(prev_pgboost_time, sched_clock());
}

void trigger_pgboost(void)
{
	do_pgboost(jiffies64_to_nsecs((unsigned long)etrig_intval * HZ), true, false);
}
EXPORT_SYMBOL(trigger_pgboost);

void trigger_pgboost_light(void)
{
	do_pgboost(jiffies64_to_nsecs((unsigned long)etrig_intval * HZ), true, true);
}
EXPORT_SYMBOL(trigger_pgboost_light);

static void periodic_pgboost(void)
{
	static unsigned long prev_pgfree;

	/* Large variance in PGFREE is observed */
	if (get_vm_event(PGFREE) > (READ_ONCE(prev_pgfree) + LEAST_TOTAL_PGS))
		trigger_pgboost();
	else
		do_pgboost(jiffies64_to_nsecs((unsigned long)timer_intval * HZ), false, false);

	/* Update prev_pgfree at the end of do_pgboost */
	WRITE_ONCE(prev_pgfree, get_vm_event(PGFREE));
}

/* Delayed periodic work handler */
static unsigned int allow_periodicity;	/* 0: no periodic pgboost, !0: allow periodic pgboost */
module_param(allow_periodicity, uint, 0600);
static void pgboost_periodic_work_handler(struct work_struct *work)
{
	if (allow_periodicity != 0)
		periodic_pgboost();

	queue_delayed_work(system_unbound_wq, &pgboost_periodic_work,
			(unsigned long)period_intval * HZ);
}

/* Delayed event work handler */
static unsigned int allow_event_trigger;	/* 0: not allowed, !0: allowed */
module_param(allow_event_trigger, uint, 0600);
static void pgboost_work_handler(struct work_struct *work)
{
	if (allow_event_trigger == 0)
		return;

	trigger_pgboost_light();
}

static int kick_pgboost(const char *val, const struct kernel_param *kp)
{
	int retval;
	unsigned long tmp;

	retval = kstrtoul(val, 0, &tmp);
	if (retval != 0) {
		pr_info("%s: failed to do operation!\n", __func__);
		return retval;
	}

	switch (tmp) {
	case 1:
		trigger_pgboost();
		break;
	case 2:
		trigger_pgboost_light();
		break;
	default:
		pr_info("%s invalid ops!\n", __func__);
		break;
	}

	return retval;
}

static const struct kernel_param_ops pgboost_param_ops = {
	.set = &kick_pgboost,
};
module_param_cb(kick_pgboost, &pgboost_param_ops, NULL, 0600);

/*****************************************/
/* -- The impl of tracepoints hook up -- */
/*****************************************/
/*
 * Array index:
 * |------------|-------|-------------------|
 * | rank\order |   0   |  HPAGE_PMD_ORDER  |
 * |------------|-------|-------------------|
 * |     0      |   0   |         1         |
 * |------------|-------|-------------------|
 * |     1      |   2   |         3         |
 * |------------|-------|-------------------|
 */
static atomic_long_t rank_pgcount[4];
static unsigned long ranks[4];
module_param_array_named(status_alloc_in_ranks, ranks, ulong, NULL, 0644);
static void pgboost_mm_page_alloc(void *ignore, struct page *page,
		unsigned int order, gfp_t gfp_flags, int migratetype)
{
	int index;

	/* Only care about 4KB(order:0) & 2MB(order:HPAGE_PMD_ORDER) pages */
	if (!page || (order != 0 && order != HPAGE_PMD_ORDER))
		return;

	/* Calculate index */
	index = (order == 0) ? 0 : 1;
	if (page_to_phys(page) >= rank_boundary_phys)
		index += 2;

	ranks[index] = atomic_long_inc_return(&rank_pgcount[index]);
}

/* Monitor how many pages are polluting the reordered lists and try to trigger reordering */
static void pgboost_mm_page_free(void *ignore, struct page *page, unsigned int order)
{
#ifdef DEBUG_PGBOOST_ENABLED
	static atomic_t triggered;
	int ttr;
#endif
	unsigned long tmp;

	/* Don't monitor when pgboost is in the progress of reordering */
	if (READ_ONCE(pgboost_in_progres))
		return;

	/* Detect how many freeing pages not in the interesting rank. */
	if (pfn_not_in_interesting_rank(page_to_pfn(page), READ_ONCE(curr_favor_rank1))) {
		tmp = atomic_long_add_return(1 << order, &pgs_violate_ranks);
		if (tmp >= LEAST_TOTAL_PGS) {
			atomic_long_set(&pgs_violate_ranks, 0);
			queue_delayed_work(system_unbound_wq, &pgboost_work, 0);
#ifdef DEBUG_PGBOOST_ENABLED
			ttr = atomic_inc_return(&triggered);
			PGBOOST_DEBUG("%s: ...triggered(%d)\n", __func__, ttr);
#endif
		}
	}
}

static void pgboost_bypass_drain_pages(void *ignore, unsigned int mt, bool *skip)
{
	/* Should we force drain pcp pages */
	if (READ_ONCE(force_drain)) {

		*skip = false;

		/* stop force drain all pages for the remaining mt changes */
		WRITE_ONCE(force_drain, false);
		return;
	}

	if (mt == MIGRATE_CMA)
		*skip = false;
	else
		*skip = true;
}

static struct tracepoints_table pgboost_tracepoints[] = {
{.name = "mm_page_alloc_unused", .func = pgboost_mm_page_alloc, .tp = NULL},
{.name = "mm_page_free", .func = pgboost_mm_page_free, .tp = NULL},
{.name = "android_vh_cma_drain_all_pages_bypass", .func = pgboost_bypass_drain_pages, .tp = NULL},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(pgboost_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(pgboost_tracepoints[i].name, tp->name) == 0)
			pgboost_tracepoints[i].tp = tp;
	}
}

/* Find out interesting tracepoints and try to register them. */
static void __init pgboost_hookup_tracepoints(void)
{
	int i;
	int ret;

	/* Find out interesting tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	/* Probing found tracepoints */
	FOR_EACH_INTEREST(i) {
		if (pgboost_tracepoints[i].tp != NULL) {
			ret = tracepoint_probe_register(pgboost_tracepoints[i].tp,
							pgboost_tracepoints[i].func,  NULL);
			if (ret)
				pr_info("Failed to register %s\n", pgboost_tracepoints[i].name);
			else
				pgboost_tracepoints[i].init = true;
		}

		/* Check which one is not probed */
		if (!pgboost_tracepoints[i].init)
			pr_info("%s: %s is not probed!\n", __func__, pgboost_tracepoints[i].name);
	}
}

/* Unregister interesting tracepoints. */
static void __exit pgboost_disconnect_tracepoints(void)
{
	int i;
	int ret;

	/* Unregister found tracepoints */
	FOR_EACH_INTEREST(i) {
		if (pgboost_tracepoints[i].init) {
			ret = tracepoint_probe_unregister(pgboost_tracepoints[i].tp,
							pgboost_tracepoints[i].func,  NULL);
			if (ret)
				pr_info("Failed to unregister %s\n", pgboost_tracepoints[i].name);
		}
	}
}

#ifdef PGBOOST_STRATEGY_HOLD_SPG
/******************************/
/* -- The IMPL of shrinker -- */
/******************************/
static unsigned long pgboost_shrink_count(struct shrinker *shrinker, struct shrink_control *sc)
{
	unsigned long spg_total_nr = 0;
	int order;

	spin_lock(&spg_lock);

	for (order = 0; order < HPAGE_PMD_ORDER; order++) {
		spg_total_nr += spg_nr_0[order] << order;
		spg_total_nr += spg_nr_1[order] << order;
	}

	spin_unlock(&spg_lock);

	return spg_total_nr;
}

static unsigned long release_hold_small_pages(unsigned long nr_to_scan)
{
	unsigned long total_freed = 0;

	total_freed = release_small_pages(spg_list_0, spg_nr_0,
					  total_freed, nr_to_scan);
	total_freed = release_small_pages(spg_list_1, spg_nr_1,
					   total_freed, nr_to_scan);

	PGBOOST_DEBUG("%s: release(%lu) pages\n", __func__, total_freed);
	return total_freed;

}

static unsigned long pgboost_shrink_scan(struct shrinker *shrinker, struct shrink_control *sc)
{
	return release_hold_small_pages(sc->nr_to_scan);
}

static struct shrinker pgboost_shrinker = {
	.count_objects = pgboost_shrink_count,
	.scan_objects = pgboost_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};
#endif

/*************************************/
/* -- Initialize fundamental data -- */
/*************************************/
static int __init pgboost_init_data(void)
{
	int i;

	/* Frequently-used data - single zone check */
	zone = page_zone(pfn_to_page(ARCH_PFN_OFFSET));
	if (nr_online_nodes != 1 || zone_idx(zone) != ZONE_NORMAL) {
		pr_info("%s: not single zone. Abort initialization.\n", __func__);
		return -1;
	}

	/* Frequently-used data */
	zone_start_pfn = zone->zone_start_pfn;
	rank_info[0].start_pfn = zone_start_pfn;
	rank_info[0].end_pfn = zone_start_pfn + (mtk_emicen_get_rk_size(0) >> PAGE_SHIFT);
	rank_info[0].free = 0;
	for (i = 1; i < RANKS_COUNT; i++) {
		rank_info[i].start_pfn = rank_info[i - 1].end_pfn;
		rank_info[i].end_pfn = rank_info[i].start_pfn +
			(mtk_emicen_get_rk_size(i) >> PAGE_SHIFT);
		rank_info[i].free = 0;
	}

	rank_boundary_pfn = rank_info[0].end_pfn;
	rank_boundary_phys = rank_boundary_pfn << PAGE_SHIFT;
	zone_batch = zone->pageset_batch;
	if (zone_batch < 0) {
		pr_info("%s: invalid zone_batch. Abort initialization.\n", __func__);
		return -1;
	}

	pr_info("%s: pfns of ranks: (%lu, %lu)\n", __func__,
		rank_info[0].end_pfn - rank_info[0].start_pfn,
		rank_info[1].end_pfn - rank_info[1].start_pfn);

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	/* Initialize lists of small pages */
	for (i = 0; i < HPAGE_PMD_ORDER; i++) {
		INIT_LIST_HEAD(&spg_list_0[i]);
		INIT_LIST_HEAD(&spg_list_1[i]);
		spg_nr_0[i] = 0;
		spg_nr_1[i] = 0;
	}
#endif

	return 0;
}

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
#if !IS_ENABLED(CONFIG_GCOV_KERNEL)
static int __init pgboost_get_ksyms(void)
{
	/* Init ksyms
	if (tools_ka_init()) {
		pr_info("%s:%d pgboost ka is not initialized!\n", __func__, __LINE__);
		return -1;
	}
	*/

	/*
	 * (Example) get interesting ksyms here --
	cma_areas_p = (struct cma *)(tools_addr_find("cma_areas"));
	if (cma_areas_p == NULL) {
		pr_info("%s: Failed to get cma_areas\n", __func__);
		return -ENOENT;
	}

	cma_area_count_p = (unsigned int *)(tools_addr_find("cma_area_count"));
	if (cma_area_count_p == NULL) {
		pr_info("%s: Failed to get cma_area_count\n", __func__);
		return -ENOENT;
	}
	*/

	return 0;
}
#endif
#endif

static void __init pgboost_init(void)
{
	/* Only for DRAM with dual ranks */
	if (mtk_emicen_get_rk_cnt() != RANKS_COUNT)
		return;

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
#if !IS_ENABLED(CONFIG_GCOV_KERNEL)
	/* Query interesting ksyms */
	if (pgboost_get_ksyms() != 0)
		return;
#endif
#endif

	/* Initialize necessary data */
	if (pgboost_init_data() != 0)
		return;

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	/* Register shrinker */
	if (register_shrinker(&pgboost_shrinker, "pgboost") != 0)
		return;
#endif

	/* Initialize & start the delayed work with periodicity */
	INIT_DELAYED_WORK(&pgboost_periodic_work, pgboost_periodic_work_handler);
	queue_delayed_work(system_unbound_wq, &pgboost_periodic_work,
			(unsigned long)period_intval * HZ);

	/* Initialize the delayed work */
	INIT_DELAYED_WORK(&pgboost_work, pgboost_work_handler);

	/* Query interesting TPs */
	pgboost_hookup_tracepoints();

	/* Now it is ready */
	pgboost_ready = true;
}

static int __init init_pgboost(void)
{
	pgboost_init();

	return 0;
}
module_init(init_pgboost);

static void  __exit exit_pgboost(void)
{
	if (!pgboost_ready)
		return;

	/* Cancel pgboost delayed work */
	cancel_delayed_work_sync(&pgboost_work);
	cancel_delayed_work_sync(&pgboost_periodic_work);

	/* Uninitialized other hooks */
	pgboost_disconnect_tracepoints();
#ifdef PGBOOST_STRATEGY_HOLD_SPG
	unregister_shrinker(&pgboost_shrinker);

	/* Release hold small pages */
	release_hold_small_pages(ULONG_MAX);
#endif
}
module_exit(exit_pgboost);

/*****************************************/
/* -- Interface to get pgboost status -- */
/*****************************************/
static int param_get_pgboost_status(char *buffer, const struct kernel_param *kp)
{
	int result = 0, ret;
	int i;

#ifdef PGBOOST_STRATEGY_HOLD_SPG
	unsigned long tmp, size_kb;

	ret = sprintf(buffer, "Hold small pages:\n");
	if (ret < 0)
		goto exit;
	result = ret;

	ret = sprintf(buffer + result, "order:");
	if (ret < 0)
		goto exit;
	result += ret;

	for (i = 0; i < HPAGE_PMD_ORDER; i++) {
		ret = sprintf(buffer + result, "%7d", i);
		if (ret < 0)
			goto exit;
		result += ret;
	}

	/* rank0 */
	size_kb = 0;
	ret = sprintf(buffer + result, "\nrank0:");
	if (ret < 0)
		goto exit;
	result += ret;

	for (i = 0; i < HPAGE_PMD_ORDER; i++) {
		tmp = spg_nr_0[i];
		ret = sprintf(buffer + result, "%7lu", tmp);
		if (ret < 0)
			goto exit;
		result += ret;
		size_kb += (tmp << i);
	}
	ret = sprintf(buffer + result, " (%8lu kB)\n", size_kb << 2);
	if (ret < 0)
		goto exit;
	result += ret;

	/* rank1 */
	size_kb = 0;
	ret = sprintf(buffer + result, "rank1:");
	if (ret < 0)
		goto exit;
	result += ret;

	for (i = 0; i < HPAGE_PMD_ORDER; i++) {
		tmp = spg_nr_1[i];
		ret = sprintf(buffer + result, "%7lu", tmp);
		if (ret < 0)
			goto exit;
		result += ret;
		size_kb += (tmp << i);
	}
	ret = sprintf(buffer + result, " (%8lu kB)\n", size_kb << 2);
	if (ret < 0)
		goto exit;
	result += ret;
#endif

	/* Rank distribution */
	ret = sprintf(buffer + result,
			"Rank huge page distribution(rank0, rank1): (%8lu kB, %8lu kB)\n",
			rank_info[0].free << 2, rank_info[1].free << 2);
	if (ret < 0)
		goto exit;
	result += ret;

	/* Show list ordering information */
	for (i = MIGRATE_UNMOVABLE; i < MIGRATE_PCPTYPES; i++)
		result += dump_list_rank_layout(i, buffer + result);

exit:
	PGBOOST_DEBUG("%s: output size is (%d) bytes\n", __func__, result);

	return result;
}

static const struct kernel_param_ops param_pgboost_status = {
	.get = param_get_pgboost_status,
};
module_param_cb(status, &param_pgboost_status, NULL, 0400);

/****************************************/
/* -- Unit test for profiling result -- */
/****************************************/

#ifdef TEST_PGBOOST_ENABLED
#define MAX_TSZ	(0x80000)
static unsigned long sz_in_pgs = MAX_TSZ;
static unsigned int movable_alloc;	/* 0: GFP_KERNEL, !0: GFP_KERNEL | __GFP_MOVABLE */
module_param_named(test_with_movable_alloc, movable_alloc, uint, 0600);
static int test_allocation_impl(char *buffer, bool favor_rank1)
{
	int result = 0, ret;
	unsigned long full_points, got_points;
	gfp_t gfp_mask = (movable_alloc == 0) ? GFP_KERNEL : (GFP_KERNEL | __GFP_MOVABLE);
	int prev_rank = -1, curr_rank = -1;
	int order, index, meet = 0, no_allocated = 0;
	int hit_favor = -1, hit_not_favor = -1;
	struct page *page;
	LIST_HEAD(list);

	ret = sprintf(buffer, "Test %lu kB for favor rank%s, rank boundary:0x%lx\n",
			sz_in_pgs << 2, favor_rank1 ? "1" : "0", rank_boundary_phys);
	if (ret < 0)
		goto exit;
	result += ret;

	ret = sprintf(buffer + result, "%6s %8s %8s %8s %10s %15s %15s %8s\n",
			"Order", "tried", "meet", "meet_4KB", "hit_favor",
			"hit_not_favor", "no_allocated", "score");
	if (ret < 0)
		goto exit;
	result += ret;

	for (order = pageblock_order; order >= 0; order--) {
		full_points = sz_in_pgs >> order;
		got_points = full_points;

		for (index = 0; index < full_points; index++) {
			page = alloc_pages(gfp_mask, order);
			if (page != NULL) {
				list_add(&page->lru, &list);
				if (phys_in_interesting_rank(page_to_phys(page), favor_rank1)) {
					meet += 1;
					if (hit_favor < 0)
						hit_favor = index;

					curr_rank = favor_rank1 ? 1 : 0;
				} else {
					if (hit_not_favor < 0)
						hit_not_favor = index;

					curr_rank = favor_rank1 ? 0 : 1;
				}

				/* Update score */
				if (prev_rank >= 0 && curr_rank != prev_rank)
					got_points--;

				prev_rank = curr_rank;
			} else {
				no_allocated += 1;
			}
		}

		ret = sprintf(buffer + result, "%6d %8d %8d %8d %10d %15d %15d %8lu\n",
				order, index, meet, meet << order, hit_favor,
				hit_not_favor, no_allocated, got_points);
		if (ret < 0)
			goto exit;
		result += ret;

		/* Put back */
		while (!list_empty(&list)) {
			page = list_first_entry(&list, struct page, lru);
			list_del(&page->lru);
			__free_pages(page, order);
			index -= 1;
		}

		/* Sanity check */
		if (index != no_allocated) {
			ret = sprintf(buffer + result, "*** %s: error remaining (%d)\n",
					__func__, index);
			if (ret < 0)
				goto exit;
			result += ret;
		}

		/* Reset for the next order */
		meet = 0;
		no_allocated = 0;
		hit_favor = -1;
		hit_not_favor = -1;
		prev_rank = curr_rank = -1;
	}

exit:
	return result;
}

static int set_test_allocation(const char *val, const struct kernel_param *kp)
{
	int retval;

	retval = kstrtoul(val, 0, &sz_in_pgs);
	if (retval != 0) {
		pr_info("%s: failed to do test_allocation\n", __func__);
		return retval;
	}

	/* Max check */
	if (sz_in_pgs > MAX_TSZ)
		sz_in_pgs = MAX_TSZ;

	return retval;
}

static int get_test_allocation(char *buffer, const struct kernel_param *kp)
{
	return test_allocation_impl(buffer, READ_ONCE(curr_favor_rank1));
}

static const struct kernel_param_ops test_allocation_ops = {
	.set = &set_test_allocation,
	.get = &get_test_allocation,
};
module_param_cb(test_allocation, &test_allocation_ops, NULL, 0600);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek PGBOOST");
