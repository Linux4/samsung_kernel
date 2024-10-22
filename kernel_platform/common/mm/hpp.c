// SPDX-License-Identifier: GPL-2.0
/*
 * linux/mm/hpp.c
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 */
#include <uapi/linux/sched/types.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/memblock.h>
#include "internal.h"

extern void ___free_pages_ok(struct page *page, unsigned int order,
		int __bitwise fpi_flags, bool skip_hugepage_pool);
extern void prep_new_page(struct page *page, unsigned int order,
		gfp_t gfp_flags, unsigned int alloc_flags);
extern void compact_node_async(void);

struct task_struct *khppd_task;

enum hpp_state_enum {
	HPP_OFF,
	HPP_ON,
	HPP_ACTIVATED,
	HPP_STATE_MAX
};

static unsigned int hpp_state;
static bool hpp_debug = false;
static int khppd_wakeup = 1;
static bool app_launch;

#define K(x) ((x) << (PAGE_SHIFT-10))
#define GB_TO_PAGES(x) ((x) << (30 - PAGE_SHIFT))
#define MB_TO_PAGES(x) ((x) << (20 - PAGE_SHIFT))

#define HUGEPAGE_ORDER HPAGE_PMD_ORDER

static void try_to_wake_up_khppd(void);

DECLARE_WAIT_QUEUE_HEAD(khppd_wait);
static struct list_head hugepage_list[MAX_NR_ZONES];
static struct list_head hugepage_nonzero_list[MAX_NR_ZONES];
int nr_hugepages_quota[MAX_NR_ZONES];
int nr_hugepages_limit[MAX_NR_ZONES];
int nr_hugepages_to_fill[MAX_NR_ZONES];
int nr_hugepages[MAX_NR_ZONES];
int nr_hugepages_nonzero[MAX_NR_ZONES];
int nr_hugepages_tried[MAX_NR_ZONES];
int nr_hugepages_alloced[MAX_NR_ZONES];
int nr_hugepages_fill_tried[MAX_NR_ZONES];
int nr_hugepages_fill_done[MAX_NR_ZONES];
static spinlock_t hugepage_list_lock[MAX_NR_ZONES];
static spinlock_t hugepage_nonzero_list_lock[MAX_NR_ZONES];

/* free pool if available memory is below this value */
static unsigned long hugepage_avail_low[MAX_NR_ZONES];
/* fill pool if available memory is above this value */
static unsigned long hugepage_avail_high[MAX_NR_ZONES];

static unsigned long __init get_hugepage_quota(void)
{
	unsigned long memblock_memory_size;
	unsigned long totalram;

	memblock_memory_size = (unsigned long)memblock_phys_mem_size();
	totalram = memblock_memory_size >> PAGE_SHIFT;

	if (totalram > GB_TO_PAGES(10))
		return GB_TO_PAGES(1);
	else if (totalram > GB_TO_PAGES(6))
		return GB_TO_PAGES(1);
	else
		return GB_TO_PAGES(0);
}

static unsigned long __init get_available_low(void)
{
	unsigned long memblock_memory_size;
	unsigned long totalram;

	memblock_memory_size = (unsigned long)memblock_phys_mem_size();
	totalram = memblock_memory_size >> PAGE_SHIFT;

	if (totalram > GB_TO_PAGES(10))
		return MB_TO_PAGES(2560);
	else if (totalram > GB_TO_PAGES(6))
		return MB_TO_PAGES(1100);
	else
		return GB_TO_PAGES(0);
}


static void __init init_hugepage_pool(void)
{
	struct zone *zone;

	long hugepage_quota = get_hugepage_quota();
	long avail_low = get_available_low();
	long avail_high = avail_low + (avail_low >> 2);
	uint32_t totalram_pages_uint = totalram_pages();

	for_each_zone(zone) {
		u64 num_pages;
		int zidx = zone_idx(zone);
		unsigned long managed_pages = zone_managed_pages(zone);

		/*
		 * calculate without zone lock as we assume managed_pages of
		 * zones do not change at runtime
		 */
		num_pages = (u64)hugepage_quota * managed_pages;
		do_div(num_pages, totalram_pages_uint);
		nr_hugepages_quota[zidx] = (num_pages >> HUGEPAGE_ORDER);
		nr_hugepages_limit[zidx] = nr_hugepages_quota[zidx];

		hugepage_avail_low[zidx] = (u64)avail_low * managed_pages;
		do_div(hugepage_avail_low[zidx], totalram_pages_uint);

		hugepage_avail_high[zidx] = (u64)avail_high * managed_pages;
		do_div(hugepage_avail_high[zidx], totalram_pages_uint);

		spin_lock_init(&hugepage_list_lock[zidx]);
		spin_lock_init(&hugepage_nonzero_list_lock[zidx]);
		INIT_LIST_HEAD(&hugepage_list[zidx]);
		INIT_LIST_HEAD(&hugepage_nonzero_list[zidx]);
	}
}

static unsigned long get_zone_pool_pages(enum zone_type zidx)
{
	unsigned long total_nr_hugepages = 0;

	spin_lock(&hugepage_list_lock[zidx]);
	total_nr_hugepages += nr_hugepages[zidx];
	spin_unlock(&hugepage_list_lock[zidx]);

	spin_lock(&hugepage_nonzero_list_lock[zidx]);
	total_nr_hugepages += nr_hugepages_nonzero[zidx];
	spin_unlock(&hugepage_nonzero_list_lock[zidx]);

	return total_nr_hugepages << HUGEPAGE_ORDER;
}

static unsigned long get_zone_pool_pages_unsafe(enum zone_type zidx)
{
	return (nr_hugepages[zidx] + nr_hugepages_nonzero[zidx]) << HUGEPAGE_ORDER;
}

static unsigned long get_pool_pages_under_zone(enum zone_type zidx, bool accurate)
{
	unsigned long total_pool_pages = 0;
	int i;

	for (i = zidx; i >= 0; i--)
		total_pool_pages += (accurate ? get_zone_pool_pages(i)
				: get_zone_pool_pages_unsafe(i));

	return total_pool_pages;
}

unsigned long total_hugepage_pool_pages(void)
{
	if (hpp_state == HPP_OFF)
		return 0;

	return get_pool_pages_under_zone(MAX_NR_ZONES - 1, true);
}

static inline unsigned long zone_available_simple(struct zone *zone)
{
	return zone_page_state(zone, NR_FREE_PAGES) +
		zone_page_state(zone, NR_ZONE_INACTIVE_FILE) +
		zone_page_state(zone, NR_ZONE_ACTIVE_FILE);
}

static DEFINE_RATELIMIT_STATE(hugepage_calc_log_rs, HZ, 1);

/*
 * adjust limits depending on available memory
 * then return total limits in #pages under the specified zone.
 * If ratelimited, it returns -1. Caller should check returned value.
 */
static void hugepage_calculate_limits_under_zone(
		enum zone_type high_zoneidx, bool accurate)
{
	struct zone *zone;
	int prev_limit;
	bool print_debug_log = false;

	/* calculate only after 100ms passed */
	static DEFINE_RATELIMIT_STATE(hugepage_calc_rs, HZ/10, 1);
	ratelimit_set_flags(&hugepage_calc_rs, RATELIMIT_MSG_ON_RELEASE);

	if (!__ratelimit(&hugepage_calc_rs))
		return;

	if (unlikely(hpp_debug)) {
		if (__ratelimit(&hugepage_calc_log_rs)) {
			print_debug_log = true;
			pr_err("%s(high_zoneidx=%d, accurate=%d): ", __func__,
					high_zoneidx, accurate);
			pr_err("%s: zidx curavail d_avail "
					" curpool curlimit newlimit\n", __func__);
		}
	}

	for_each_zone(zone) {
		int zidx = zone_idx(zone);
		long avail_pages = zone_available_simple(zone);
		long delta_avail = 0;
		long current_pool_pages = accurate ?
			get_zone_pool_pages(zidx) : get_zone_pool_pages_unsafe(zidx);

		prev_limit = nr_hugepages_limit[zidx];
		if (zidx <= high_zoneidx) {
			if (avail_pages < hugepage_avail_low[zidx]) {
				delta_avail = hugepage_avail_low[zidx] - avail_pages;
				if (current_pool_pages - delta_avail < 0)
					delta_avail = current_pool_pages;
				nr_hugepages_limit[zidx] = (current_pool_pages - delta_avail) >> HUGEPAGE_ORDER;
			} else {
				nr_hugepages_limit[zidx] = nr_hugepages_quota[zidx];
			}
		}

		if (unlikely(hpp_debug)) {
			if (print_debug_log) {
				pr_err("%s: %4d %8ld %8ld %8ld %8d %8d\n",
						__func__, zidx,
						avail_pages, delta_avail,
						current_pool_pages, prev_limit,
						nr_hugepages_limit[zidx]);
			}
		}
	}
}

static unsigned long last_wakeup_stamp;
static inline void __try_to_wake_up_khppd(enum zone_type ht)
{
	bool do_wakeup = false;

	if (app_launch || need_memory_boosting())
		return;

	if (time_is_after_jiffies(last_wakeup_stamp + 10 * HZ))
		return;

	if (khppd_wakeup)
		return;

	if (nr_hugepages_limit[ht]) {
		if (nr_hugepages[ht] * 2 < nr_hugepages_limit[ht] ||
				nr_hugepages_nonzero[ht])
			do_wakeup = true;
	} else {
		struct pglist_data *pgdat = first_online_pgdat();
		struct zone *zone = &pgdat->node_zones[ht];

		if (zone_available_simple(zone) > hugepage_avail_high[ht])
			do_wakeup = true;
	}

	if (do_wakeup) {
		khppd_wakeup = 1;
		if (unlikely(hpp_debug))
			pr_info("khppd: woken up\n");
		wake_up(&khppd_wait);
	}
}

static void try_to_wake_up_khppd(void)
{
	int i;
	enum zone_type high_zoneidx;

	high_zoneidx = gfp_zone(GFP_HIGHUSER_MOVABLE);

	for (i = high_zoneidx; i >= 0; i--)
		__try_to_wake_up_khppd(i);
}

static inline gfp_t get_gfp(enum zone_type ht)
{
	gfp_t ret;

	if (ht == ZONE_MOVABLE)
		ret = __GFP_MOVABLE | __GFP_HIGHMEM;
#ifdef CONFIG_ZONE_DMA
	else if (ht == ZONE_DMA)
		ret = __GFP_DMA;
#elif defined(CONFIG_ZONE_DMA32)
	else if (ht == ZONE_DMA32)
		ret = __GFP_DMA32;
#endif
	else
		ret = 0;
	return ret & ~__GFP_RECLAIM;
}

bool insert_hugepage_pool(struct page *page)
{
	enum zone_type ht = page_zonenum(page);

	if (hpp_state == HPP_OFF)
		return false;

	if (nr_hugepages[ht] + nr_hugepages_nonzero[ht] >= nr_hugepages_quota[ht])
		return false;

	/*
	 * note that, at this point, the page is in the free page state except
	 * it is not in buddy. need prep_new_page before going to hugepage list.
	 */
	spin_lock(&hugepage_nonzero_list_lock[ht]);
	list_add(&page->lru, &hugepage_nonzero_list[ht]);
	nr_hugepages_nonzero[ht]++;
	spin_unlock(&hugepage_nonzero_list_lock[ht]);

	return true;
}

static void zeroing_nonzero_list(enum zone_type ht)
{
	if (!nr_hugepages_nonzero[ht])
		return;

	spin_lock(&hugepage_nonzero_list_lock[ht]);
	while (!list_empty(&hugepage_nonzero_list[ht])) {
		struct page *page = list_first_entry(&hugepage_nonzero_list[ht],
						     struct page, lru);
		list_del(&page->lru);
		nr_hugepages_nonzero[ht]--;
		spin_unlock(&hugepage_nonzero_list_lock[ht]);

		if (nr_hugepages[ht] < nr_hugepages_quota[ht]) {
			prep_new_page(page, HUGEPAGE_ORDER, __GFP_ZERO, 0);
			spin_lock(&hugepage_list_lock[ht]);
			list_add(&page->lru, &hugepage_list[ht]);
			nr_hugepages[ht]++;
			spin_unlock(&hugepage_list_lock[ht]);
		} else {
			___free_pages_ok(page, HUGEPAGE_ORDER,
					(__force int __bitwise)0, true);
		}

		spin_lock(&hugepage_nonzero_list_lock[ht]);
	}
	spin_unlock(&hugepage_nonzero_list_lock[ht]);

}

static void prepare_hugepage_alloc(void);

static void calculate_nr_hugepages_to_fill(enum zone_type high_zoneidx)
{
	struct zone *zone;

	if (unlikely(hpp_debug)) {
		pr_err("%s(high_zoneidx=%d): ", __func__, high_zoneidx );
		pr_err("%s: zidx curavail d_avail curpool tofill\n", __func__);
	}

	for_each_zone(zone) {
		int zidx = zone_idx(zone);
		long avail_pages = zone_available_simple(zone);
		long delta_avail = 0;
		long current_pool_pages = get_zone_pool_pages(zidx);
		long quota_pages = ((long)nr_hugepages_quota[zidx]) << HUGEPAGE_ORDER;

		if (zidx <= high_zoneidx) {
			if (avail_pages > hugepage_avail_high[zidx]) {
				delta_avail = avail_pages - hugepage_avail_high[zidx];
				if (current_pool_pages + delta_avail > quota_pages)
					delta_avail = quota_pages - current_pool_pages;
				nr_hugepages_to_fill[zidx] = delta_avail >> HUGEPAGE_ORDER;
			} else {
				nr_hugepages_to_fill[zidx] = 0;
			}
		}

		if (unlikely(hpp_debug)) {
			pr_err("%s: %4d %8ld %8ld %8ld %8d\n",
					__func__, zidx, avail_pages,
					delta_avail, current_pool_pages,
					nr_hugepages_to_fill[zidx]);
		}
	}
}

static void fill_hugepage_pool(enum zone_type ht)
{
	int trial = nr_hugepages_to_fill[ht];

	prepare_hugepage_alloc();

	nr_hugepages_fill_tried[ht] += trial;
	while (trial--) {
		struct page *page;

		if (nr_hugepages[ht] >= nr_hugepages_quota[ht])
			break;

		page = alloc_pages(get_gfp(ht) | __GFP_ZERO |
						__GFP_NOWARN, HUGEPAGE_ORDER);

		/* if alloc fails, future requests may fail also. stop here. */
		if (!page)
			break;

		if (page_zonenum(page) != ht) {
			/* Note that we should use __free_pages to call to free_pages_prepare */
			__free_pages(page, HUGEPAGE_ORDER);

			/*
			 * if page is from the lower zone, future requests may
			 * also get the lower zone pages. stop here.
			 */
			break;
		}
		nr_hugepages_fill_done[ht]++;
		spin_lock(&hugepage_list_lock[ht]);
		list_add(&page->lru, &hugepage_list[ht]);
		nr_hugepages[ht]++;
		spin_unlock(&hugepage_list_lock[ht]);
	}
}

struct page *alloc_zeroed_hugepage(gfp_t gfp_mask)
{
	int zidx;
	enum zone_type high_zoneidx;
	struct page *page = NULL;

	if (hpp_state != HPP_ACTIVATED)
		return NULL;

	if (current == khppd_task)
		return NULL;

	high_zoneidx = gfp_zone(gfp_mask);
	nr_hugepages_tried[high_zoneidx]++;
	for (zidx = high_zoneidx; zidx >= 0; zidx--) {
		__try_to_wake_up_khppd(zidx);
		if (!nr_hugepages[zidx])
			continue;
		if (unlikely(!spin_trylock(&hugepage_list_lock[zidx])))
			continue;

		if (!list_empty(&hugepage_list[zidx])) {
			page = list_first_entry(&hugepage_list[zidx],
					struct page, lru);
			list_del(&page->lru);
			nr_hugepages[zidx]--;
		}
		spin_unlock(&hugepage_list_lock[zidx]);

		if (page)
			goto got_page;
	}

	for (zidx = high_zoneidx; zidx >= 0; zidx--) {
		if (!nr_hugepages_nonzero[zidx])
			continue;
		if (unlikely(!spin_trylock(&hugepage_nonzero_list_lock[zidx])))
			continue;

		if (!list_empty(&hugepage_nonzero_list[zidx])) {
			page = list_first_entry(&hugepage_nonzero_list[zidx],
					struct page, lru);
			list_del(&page->lru);
			nr_hugepages_nonzero[zidx]--;
		}
		spin_unlock(&hugepage_nonzero_list_lock[zidx]);

		if (page) {
			prep_new_page(page, HUGEPAGE_ORDER, __GFP_ZERO, 0);
			goto got_page;
		}
	}

	return NULL;

got_page:
	nr_hugepages_alloced[zidx]++;
	if (gfp_mask & __GFP_COMP)
		prep_compound_page(page, HUGEPAGE_ORDER);
	return page;
}

static int khppd(void *p)
{
	while (!kthread_should_stop()) {
		int i;

		wait_event_freezable(khppd_wait, khppd_wakeup ||
				kthread_should_stop());

		khppd_wakeup = 0;
		last_wakeup_stamp = jiffies;

		calculate_nr_hugepages_to_fill(MAX_NR_ZONES - 1);
		for (i = 0; i < MAX_NR_ZONES; i++) {
			if (app_launch || need_memory_boosting())
				break;

			zeroing_nonzero_list(i);
			fill_hugepage_pool(i);
		}
	}

	return 0;
}

static DEFINE_RATELIMIT_STATE(hugepage_count_log_rs, HZ, 1);

bool is_hugepage_avail_low_ok(void)
{
	struct zone *zone;
	long total_avail_pages = 0;
	long total_avail_low_pages = 0;

	for_each_zone(zone) {
		int zidx = zone_idx(zone);
		total_avail_pages += zone_available_simple(zone);
		total_avail_low_pages += hugepage_avail_low[zidx];
	}

	return total_avail_pages >= total_avail_low_pages ? true : false;
}

static unsigned long hugepage_pool_count(struct shrinker *shrink,
				struct shrink_control *sc)
{
	long count, total_count = 0;
	enum zone_type high_zoneidx;
	int i;

	if (!current_is_kswapd())
		return 0;

	if (is_hugepage_avail_low_ok())
		return 0;

	high_zoneidx = MAX_NR_ZONES - 1;
	hugepage_calculate_limits_under_zone(high_zoneidx, false);
	for (i = high_zoneidx; i >= 0; i--) {
		count = get_zone_pool_pages_unsafe(i) - (nr_hugepages_limit[i] << HUGEPAGE_ORDER);
		if (count > 0)
			total_count += count;
	}

	if (unlikely(hpp_debug)) {
		if (__ratelimit(&hugepage_count_log_rs))
			pr_err("%s returned %ld\n", __func__, total_count);
	}

	return total_count;
}

static DEFINE_RATELIMIT_STATE(hugepage_scan_log_rs, HZ, 1);

static unsigned long hugepage_pool_scan(struct shrinker *shrink,
				struct shrink_control *sc)
{
	unsigned long freed = 0;
	long freed_zone, to_scan_zone; /* freed & to_scan per zone */
	struct zone *zone;
	struct page *page;
	int zidx;
	enum zone_type high_zoneidx;
	bool print_debug_log = false;

	if (!current_is_kswapd())
		return SHRINK_STOP;

	if (unlikely(hpp_debug)) {
		if (__ratelimit(&hugepage_scan_log_rs)) {
			print_debug_log = true;
			pr_err("%s was requested %lu\n", __func__,
					sc->nr_to_scan);
		}
	}

	high_zoneidx = MAX_NR_ZONES - 1;

	hugepage_calculate_limits_under_zone(high_zoneidx, true);
	for_each_zone(zone) {
		zidx = zone_idx(zone);
		to_scan_zone = nr_hugepages[zidx] + nr_hugepages_nonzero[zidx]
					- nr_hugepages_limit[zidx];
		to_scan_zone = (to_scan_zone < 0) ? 0 : to_scan_zone;
		if (zidx > high_zoneidx || !to_scan_zone)
			continue;

		freed_zone = 0;
		spin_lock(&hugepage_nonzero_list_lock[zidx]);
		while (!list_empty(&hugepage_nonzero_list[zidx]) &&
				freed_zone < to_scan_zone) {
			page = list_first_entry(&hugepage_nonzero_list[zidx],
					struct page, lru);
			list_del(&page->lru);
			___free_pages_ok(page, HUGEPAGE_ORDER,
					(__force int __bitwise)0, true);
			nr_hugepages_nonzero[zidx]--;
			freed_zone++;
		}
		spin_unlock(&hugepage_nonzero_list_lock[zidx]);

		spin_lock(&hugepage_list_lock[zidx]);
		while (!list_empty(&hugepage_list[zidx]) &&
				freed_zone < to_scan_zone) {
			page = list_first_entry(&hugepage_list[zidx],
					struct page, lru);
			list_del(&page->lru);
			___free_pages_ok(page, HUGEPAGE_ORDER,
					(__force int __bitwise)0, true);
			nr_hugepages[zidx]--;
			freed_zone++;
		}
		spin_unlock(&hugepage_list_lock[zidx]);

		freed += freed_zone;
	}

	if (unlikely(hpp_debug) && print_debug_log)
		pr_err("%s freed %ld hugepages(%ldK)\n", __func__, freed,
				K(freed << HUGEPAGE_ORDER));

	if (freed == 0)
		return SHRINK_STOP;

	return freed << HUGEPAGE_ORDER;
}

/*
 * this function should be called within hugepage_poold context, only.
 */
static void prepare_hugepage_alloc(void)
{
	static int compact_count;
	static DEFINE_RATELIMIT_STATE(hugepage_compact_rs, 60 * 60 * HZ, 1);

	if (__ratelimit(&hugepage_compact_rs)) {
		struct sched_param param_normal = { .sched_priority = 0 };
		struct sched_param param_idle = { .sched_priority = 0 };

		if (!sched_setscheduler(current, SCHED_NORMAL,
				   &param_normal)) {
			pr_info("khppd: compact start\n");
			compact_node_async();
			pr_info("khppd: compact end (%d done)\n",
					++compact_count);

			if (sched_setscheduler(current, SCHED_IDLE,
						&param_idle))
				pr_err("khppd: fail to set sched_idle\n");
		}
	}
}

static struct shrinker hugepage_pool_shrinker_info = {
	.scan_objects = hugepage_pool_scan,
	.count_objects = hugepage_pool_count,
	.seeks = DEFAULT_SEEKS,
};

module_param_array(nr_hugepages, int, NULL, 0444);
module_param_array(nr_hugepages_nonzero, int, NULL, 0444);
module_param_array(nr_hugepages_alloced, int, NULL, 0444);
module_param_array(nr_hugepages_tried, int, NULL, 0444);
module_param_array(nr_hugepages_fill_tried, int, NULL, 0444);
module_param_array(nr_hugepages_fill_done, int, NULL, 0444);
module_param_array(nr_hugepages_quota, int, NULL, 0644);
module_param_array(nr_hugepages_limit, int, NULL, 0444);

module_param_array(hugepage_avail_low, ulong, NULL, 0644);
module_param_array(hugepage_avail_high, ulong, NULL, 0644);

static int khppd_app_launch_notifier(struct notifier_block *nb,
					 unsigned long action, void *data)
{
	bool prev_launch;

	if (hpp_state == HPP_OFF)
		return 0;

	prev_launch = app_launch;
	app_launch = action ? true : false;

	if (prev_launch && !app_launch)
		try_to_wake_up_khppd();

	return 0;
}

static struct notifier_block khppd_app_launch_nb = {
	.notifier_call = khppd_app_launch_notifier,
};

static int __init hpp_init(void)
{
	int ret;
	struct sched_param param = { .sched_priority = 0 };

	if (!get_hugepage_quota()) {
		hpp_state = HPP_OFF;
		goto skip_all;
	}

	init_hugepage_pool();
	khppd_task = kthread_run(khppd, NULL, "khppd");
	if (IS_ERR(khppd_task)) {
		pr_err("Failed to start khppduge\n");
		khppd_task = NULL;
		goto skip_all;
	}
	try_to_wake_up_khppd();
	sched_setscheduler(khppd_task, SCHED_IDLE, &param);

	am_app_launch_notifier_register(&khppd_app_launch_nb);

	ret = register_shrinker(&hugepage_pool_shrinker_info, "hugepage_pool");
	if (ret) {
		kthread_stop(khppd_task);
		goto skip_all;
	}

	hpp_state = HPP_ACTIVATED;
skip_all:
	return 0;
}

static void __exit hpp_exit(void)
{
}

static int hpp_debug_param_set(const char *val, const struct kernel_param *kp)
{
	return param_set_bool(val, kp);
}

static struct kernel_param_ops hpp_debug_param_ops = {
	.set =	hpp_debug_param_set,
	.get =	param_get_bool,
};
module_param_cb(debug, &hpp_debug_param_ops, &hpp_debug, 0644);

static int hpp_state_param_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, 0, 2);
}

static struct kernel_param_ops hpp_state_param_ops = {
	.set =	hpp_state_param_set,
	.get =	param_get_uint,
};
module_param_cb(state, &hpp_state_param_ops, &hpp_state, 0644);

#undef K
module_init(hpp_init)
module_exit(hpp_exit);
