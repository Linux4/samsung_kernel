/*
 *  linux/mm/page_steal.c
 *
 *  Copyright (C)
 */

#define pr_fmt(fmt) "page_steal: " fmt

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/page-isolation.h>
#include <linux/hugetlb.h>
#include <linux/migrate.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/freezer.h>
#include <linux/rmap.h>
#include <linux/huge_mm.h>
#include <linux/mmu_notifier.h>
#include <linux/page_owner.h>

#include <asm/tlbflush.h>

#include "internal.h"

/* copied from mm/rmap. */
#ifdef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH
static void __set_tlb_ubc_flush_pending(struct mm_struct *mm, bool writable)
{
	struct tlbflush_unmap_batch *tlb_ubc = &current->tlb_ubc;

	arch_tlbbatch_add_mm(&tlb_ubc->arch, mm);
	tlb_ubc->flush_required = true;

	/*
	 * Ensure compiler does not re-order the setting of tlb_flush_batched
	 * before the PTE is cleared.
	 */
	barrier();
	mm->tlb_flush_batched = true;

	/*
	 * If the PTE was dirty then it's best to assume it's writable. The
	 * caller must use try_to_unmap_flush_dirty() or try_to_unmap_flush()
	 * before the page is queued for IO.
	 */
	if (writable)
		tlb_ubc->writable = true;
}
#else
static void __set_tlb_ubc_flush_pending(struct mm_struct *mm, bool writable)
{
}
#endif

static bool migrate_anon_page_one(struct page *page, struct vm_area_struct *vma,
				  unsigned long address, void *arg)
{
	struct mm_struct *mm = vma->vm_mm;
	struct page_vma_mapped_walk pvmw = {
		.page = page,
		.vma = vma,
		.address = address,
	};
	pte_t pteval, newpte;
	struct page *newpage = arg;
	unsigned long start = address, end;

	mmu_notifier_invalidate_range_start(vma->vm_mm, start, end);

	while (page_vma_mapped_walk(&pvmw)) {
		/* Nuke the page table entry. */
		flush_cache_page(vma, address, pte_pfn(*pvmw.pte));

		if (IS_ENABLED(CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH)) {
			pteval = ptep_get_and_clear(mm, address, pvmw.pte);
			__set_tlb_ubc_flush_pending(mm, pte_dirty(pteval));
		} else {
			pteval = ptep_clear_flush(vma, address, pvmw.pte);
		}
		/* Move the dirty bit to the page. Now the pte is gone. */
		if (pte_dirty(pteval))
			set_page_dirty(page);

		page_remove_rmap(page, false);
		put_page(page); /* remove page count along with mapcount */

		mmu_notifier_invalidate_range(mm, address,
					      address + PAGE_SIZE);
		/* remove_migration_pte */
		get_page(newpage);
		newpte = pte_mkold(mk_pte(newpage,
				   READ_ONCE(vma->vm_page_prot)));
		if (pte_soft_dirty(pteval))
			newpte = pte_mksoft_dirty(newpte);
		if (pte_write(pteval))
			newpte = maybe_mkwrite(newpte, vma->vm_flags);

		flush_dcache_page(newpage);

		set_pte_at(vma->vm_mm, pvmw.address, pvmw.pte, newpte);

		page_add_anon_rmap(newpage, vma, pvmw.address, false);
		if (vma->vm_flags & VM_LOCKED)
			mlock_vma_page(newpage);

		update_mmu_cache(vma, pvmw.address, pvmw.pte);
	}

	mmu_notifier_invalidate_range_end(vma->vm_mm, start, end);

	return true;
}

static int is_zero_page_mapcount(struct page *page)
{
	return !total_mapcount(page);
}

/* copied from is_vma_temporary_stack() in mm/rmap.c */
static bool is_invalid_migration_vma(struct vm_area_struct *vma, void *arg)
{
	int maybe_stack = vma->vm_flags & (VM_GROWSDOWN | VM_GROWSUP);

	if (!maybe_stack)
		return false;

	if ((vma->vm_flags & VM_STACK_INCOMPLETE_SETUP) ==
						VM_STACK_INCOMPLETE_SETUP)
		return true;

	return false;
}

int migrate_anon_page(struct page *page, struct page *newpage)
{
	struct rmap_walk_control rwc = {
		.rmap_one = migrate_anon_page_one,
		.arg = newpage,
		.done = is_zero_page_mapcount,
		.anon_lock = page_lock_anon_vma_read,
		/*
		 * During exec, a temporary VMA is setup and later moved.
		 * The VMA is moved under the anon_vma lock but not the
		 * page tables leading to a race where migration cannot
		 * find the migration ptes. Rather than increasing the
		 * locking requirements of exec(), migration skips
		 * temporary VMAs until after exec() completes.
		 */
		.invalid_vma = is_invalid_migration_vma,
	};
	int rc;
	int refcount, mapcount;

	/*
	 * We are doing irreversible anonymous page migration because we
	 * abandoned migration pte. Therefore we should delegate migration
	 * of pinned anonymous pages to try_to_unmap() that handles all
	 * exceptional cases.
	 */
	if (page_mapcount(page) != page_count(page) - 1)
		return -EBUSY;

	if (PageHWPoison(page) || !PageAnon(page) || PageSwapCache(page) ||
	    PageCompound(page) || PageHuge(page) || PageKsm(page))
		return -EAGAIN;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(!PageLocked(newpage), newpage);

	refcount = page_count(page);
	mapcount = page_mapcount(page);

	rmap_walk(page, &rwc);

	if ((refcount != page_count(newpage)) ||
	    (mapcount != page_mapcount(newpage))) {
		pr_err("unbalanced counts(%pGp): mapcnt %d->%d, cnt %d->%d\n",
		       &newpage->flags, mapcount, page_mapcount(newpage),
		       refcount, page_count(newpage));
		rc = -EBUSY;
		goto revert;
	}
	/*
	 * We migrate reverse mapping after completing pte migration.
	 * It is safe for now iff the page is anonymous single page.
	 */
	rc = migrate_page(NULL, newpage, page, MIGRATE_SYNC);
	if (rc != MIGRATEPAGE_SUCCESS) {
		pr_err("Failed to migrate page\n");
		goto revert;
	}

	return rc;
revert:
	rwc.arg = page;
	/* rervert migration */
	rmap_walk(newpage, &rwc);
	return rc;
}

static int isolate_movable_pages(unsigned long start_pfn, unsigned int nr_pages,
				 struct list_head *nonfile, struct list_head *nonlru,
				 struct list_head *pagecache)
{
	unsigned long pfn;
	unsigned long pfn_end = start_pfn + nr_pages;
	LIST_HEAD(nonfiles);
	LIST_HEAD(pagecaches);
	int ret = 0;
	int count = 0;

	for (pfn = start_pfn; pfn < pfn_end; pfn++) {
		struct page *page;

		if (!pfn_valid(pfn))
			continue;

		page = pfn_to_page(pfn);

		if (PageHuge(page)) {
			struct page *head = compound_head(page);

			pfn = page_to_pfn(head) + (1 << compound_order(head)) - 1;
			if (compound_order(head) > PFN_SECTION_SHIFT) {
				pr_err("too large huge page %#lx (>section)\n",
				       page_to_pfn(head));
				ret = -EBUSY;
				goto fail;
			}

			if (!isolate_huge_page(page, nonfile)) {
				pr_err("failed to isolate hpage %#lx(odr %d)\n",
				       page_to_pfn(head), compound_order(head));
				ret = -EBUSY;
				goto fail;
			}

			count += 1 << compound_order(head);

			continue;
		} else if (thp_migration_supported() && PageTransHuge(page))
			pfn = page_to_pfn(compound_head(page))
				+ hpage_nr_pages(page) - 1;
		/*
		 * HWPoison pages have elevated reference counts so the
		 * migration would fail on them. It also doesn't make any sense
		 * to migrate them in the first place. Still try to unmap such a
		 * page in case it is still mapped (e.g. current hwpoison
		 * implementation doesn't unmap KSM pages but keep the unmap as
		 * the catch all safety net).
		 */
		if (PageHWPoison(page)) {
			if (WARN_ON(PageLRU(page)))
				isolate_lru_page(page);
			if (page_mapped(page))
				try_to_unmap(page,
					TTU_IGNORE_MLOCK | TTU_IGNORE_ACCESS, NULL);
			continue;
		}
		/*
		 * We can skip free pages because the free pages are in the
		 * freelist of MIGRATE_ISOLATE.
		 */
		if (!get_page_unless_zero(page))
			continue;

		ret = PageLRU(page) ? isolate_lru_page(page)
				    : isolate_movable_page(page,
							   ISOLATE_UNEVICTABLE);
		if (!ret) { /* Success */
			put_page(page);

			if (__PageMovable(page) || PageUnevictable(page)) {
				list_add_tail(&page->lru, nonlru);
			} else if (page_is_file_cache(page) &&
				   !PageDirty(page) && !PageUnevictable(page)) {
				list_add_tail(&page->lru, pagecache);
			} else {
				list_add_tail(&page->lru, nonfile);
			}

			if (!__PageMovable(page))
				inc_node_page_state(page, NR_ISOLATED_ANON +
						    page_is_file_cache(page));
			count++;
		} else {
			pr_alert_ratelimited("isolation failed: "
					     "pfn %#lx(%pGp,cnt%d,mapcnt%d)\n",
					     pfn, &page->flags,
					     page_count(page),
					     page_mapcount(page));
			if (IS_ENABLED(CONFIG_DEBUG_VM))
				dump_page(page, "isolation failed");
			put_page(page);
			/* Because we don't have big zone->lock. we should
			   check this again here. */
			if (page_count(page))
				goto fail;
		}
	}

	return count;
fail:
	putback_movable_pages(nonfile);
	putback_movable_pages(pagecache);
	putback_movable_pages(nonlru);
	return ret;
}

static struct page *get_migrate_target(struct page *page, unsigned long private)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE | __GFP_RETRY_MAYFAIL;
	unsigned int order = 0;
	struct page *new_page = NULL;

	if (PageHuge(page))
		return alloc_huge_page_nodemask(page_hstate(compound_head(page)),
				preferred_nid, nodemask);

	if (thp_migration_supported() && PageTransHuge(page)) {
		order = HPAGE_PMD_ORDER;
		gfp_mask |= GFP_TRANSHUGE;
	}

	if (PageHighMem(page))
		gfp_mask |= __GFP_HIGHMEM;

	new_page = alloc_pages(gfp_mask, order);

	if (new_page && PageTransHuge(new_page))
		prep_transhuge_page(new_page);

	return new_page;
}

static int reclaim_pages_list(unsigned long start_pfn, unsigned int nr_pages,
			      struct list_head *nonfile,
			      struct list_head *nonlru,
			      struct list_head *pagecache,
			      enum migrate_reason reason)
{
	if (!list_empty(pagecache)) {
		struct zone *zone = page_zone(pfn_to_page(start_pfn));
		enum ttu_flags flags = 0;
		unsigned long freed;

		if (reason == MR_MEMORY_HOTPLUG)
			flags |= TTU_FORCE_BATCH_FLUSH;

		freed = reclaim_clean_pages_from_list(zone, pagecache, flags);
		if (!list_empty(pagecache))
			list_splice(pagecache, nonfile);
		mod_node_page_state(zone->zone_pgdat, NR_ISOLATED_FILE, -freed);
	}
	/*
	 * Let's migrate non-lru movable pages before migrating anonymous pages.
	 * It's because non-lru movable pages are more easily migrated than anon
	 * pages. Memory hot-plug will soon retry page migration on the same
	 * page block because it should migrate all pages in the victim section.
	 * We want to migrate more pages in the first try.
	 */
	if (!list_empty(nonlru)) {
		migrate_pages(nonlru, get_migrate_target, NULL, 0,
			      MIGRATE_SYNC, reason);
		/*
		 * give second chance to non lru/unevictable pages
		 * nonlru list may have freed pages but it is okay because
		 * migrate_pages() skips such pages.
		 */
		list_splice(nonlru, nonfile);
	}

	if (!list_empty(nonfile)) {
		int ret = migrate_pages(nonfile, get_migrate_target, NULL, 0,
					MIGRATE_SYNC, reason);

		if (ret) {
			pr_err("failed to migrate pages in [%#lx, %#lx) (%d)\n",
			       start_pfn, start_pfn + nr_pages, ret);
			putback_movable_pages(nonfile);
			return -EAGAIN;
		}
	}

	return 0;
}

static int reclaim_page_block(unsigned long blk_start_pfn, int mode, int reason)
{
	LIST_HEAD(nonfile);
	LIST_HEAD(nonlru);
	LIST_HEAD(pagecache);
	int ret;

	ret = isolate_movable_pages(blk_start_pfn, pageblock_nr_pages,
				    &nonfile, &nonlru, &pagecache);
	if (ret < 0)
		return ret;

	ret = reclaim_pages_list(blk_start_pfn, pageblock_nr_pages,
				 &nonfile, &nonlru, &pagecache, reason);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * reclaim_pages_range - reclaim/migrate all lru/movable pages in given range
 * @start_pfn: pfn of the first page to reclaim.
 *             should be aligned by pageblock_nr_pages.
 * @count: number of pages from @start_pfn to reclaim.
 *         should be aligned by pageblock_nr_pages.
 * @mode: options to control migration/reclamation logic.
 * @reason: information for debugging purpose
 *
 * Return 0 if all @count pages from @start_pfn are free or reclaimed.
 * -error otherwise.
 *
 * On success, PageBuddy() of all pages in the given range is true unless a page
 * is in per-cpu freelist. If a user wants to get guaranteed the pages in the
 * range free, he/she must turn the migratetype of page blocks of the pages to
 * MIGRATE_ISOLATE with start_isolate_page_range().
 */
int reclaim_pages_range(unsigned long start_pfn, unsigned long count,
			int mode, int reason)
{
	unsigned long pfn = start_pfn;
	unsigned long end_pfn = start_pfn + count;
	int ret = 0;

	BUG_ON(!IS_ALIGNED(start_pfn, pageblock_nr_pages));
	BUG_ON(!IS_ALIGNED(end_pfn, pageblock_nr_pages));

	while (pfn < end_pfn) {
		ret = reclaim_page_block(pfn, mode, reason);
		if (ret < 0) {
			pr_err("failed to reclaim page block at %#lx (%d)\n",
			       pfn, ret);
			goto err;
		}

		pfn += pageblock_nr_pages;
	}

err:
	try_to_unmap_flush();

	return ret;
}

static bool page_steal_available(unsigned long pfn, unsigned int order)
{
	unsigned long pfn_end = pfn + (1 << order);

	while (pfn < pfn_end) {
		struct page *page = pfn_to_page(pfn);

		if (!pfn_valid_within(pfn))
			return false;
		if (PageBuddy(page)) {
			pfn += 1 << page_order(page);
			continue;
		}
		if (PageCompound(page) || PageReserved(page))
			return false;
		if (!PageLRU(page) && !__PageMovable(page))
			return false;
		pfn++;
	}

	return true;
}

int steal_pages(unsigned long pfn, unsigned int order)
{
	struct page *page = pfn_to_page(pfn);
	struct zone *zone = page_zone(page);
	LIST_HEAD(nonfile);
	LIST_HEAD(nonlru);
	LIST_HEAD(file);
	unsigned long flags;
	int ret;

	if (!IS_ALIGNED(pfn, 1 << order) || (order > pageblock_order)) {
		pr_err("invalid order %d with pfn %#lx\n", order, pfn);
		return -EINVAL;
	}

	if (!page_steal_available(pfn, order))
		return -EBUSY;

	ret = isolate_movable_pages(pfn, 1 << order, &nonfile, &nonlru, &file);
	if (ret < 0)
		return ret;

	ret = reclaim_pages_list(pfn, 1 << order, &nonfile, &nonlru, &file,
				 MR_CONTIG_RANGE);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&zone->lock, flags);

	if (!PageBuddy(page) ||
	    (page_order(page) != order) || !__isolate_free_page(page, order)) {
		spin_unlock_irqrestore(&zone->lock, flags);
		return -EAGAIN;
	}

	spin_unlock_irqrestore(&zone->lock, flags);

	post_alloc_hook(page, order, __GFP_MOVABLE);

	return 0;
}

/*
 * test procedure:
 * 1. write the base pfn to debugfs/base_pfn
 * 2. write the page count to debugfs/steal
 * 3. notice the result
 * 4. write 0 to debugfs/steal to return the stolen pages to the system.
 */
/* updated by writing to debugfs/base_pfn */
int test_reclaim_pages_range(unsigned long start_pfn, int count)
{
	int ret;
	unsigned long nr_pages = ALIGN(count, pageblock_nr_pages);
	unsigned long base_pfn = start_pfn;
	unsigned long end_pfn = start_pfn + count;
	ktime_t begin, isolate, steal, end;

	if (!pfn_valid(start_pfn) || !pfn_valid(end_pfn - 1)) {
		pr_err("start/end [%#lx, %#lx] is not valid\n",
		       start_pfn, end_pfn - 1);
		return -EINVAL;
	}

	if (page_zone(pfn_to_page(start_pfn)) !=
	    page_zone(pfn_to_page(end_pfn - 1))) {
		pr_err("stealing pages in different zones is not allowed\n");
		return -EINVAL;
	}

	for (base_pfn = start_pfn; base_pfn < end_pfn;
	     base_pfn += pageblock_nr_pages) {
		if (MIGRATE_ISOLATE ==
		    get_pageblock_migratetype(pfn_to_page(base_pfn))) {
			pr_err("found isolated pageblock at pfn %#lx\n",
			       base_pfn);
			return -EBUSY;
		}
	}

	freeze_processes();

	begin = ktime_get();
	/*
	 * we make all page blocks in [start_pfn, start_pfn + count)
	 * MIGRATE_MOVABLE when this function ends. This is okay due to the
	 * following reasons:
	 * 1. If start_isolated_pages_range() succeeds, no page in the given
	 *    range is unmovable.
	 * 2. If stealing succeeds, the proper migratetype of the page blocks in
	 *    the range is MIGRATE_MOVABLE because all pages in the page blocks
	 *    are free.
	 */
	ret = start_isolate_page_range(start_pfn, end_pfn,
				       MIGRATE_MOVABLE, false);
	if (ret < 0) {
		pr_err("failed isolating pages in [%#lx, %#lx)\n",
		       start_pfn, end_pfn);
		goto err;
	}

	isolate = ktime_get();

	lru_add_drain_all();
	drain_all_pages(page_zone(pfn_to_page(start_pfn)));

	steal = ktime_get();

	ret = reclaim_pages_range(start_pfn, nr_pages,
				  MIGRATE_SYNC, MR_MEMORY_HOTPLUG);
	if (ret < 0) {
		pr_err("failed stealing pages in [%#lx, %#lx)\n",
		       start_pfn, end_pfn);
		undo_isolate_page_range(start_pfn, end_pfn, MIGRATE_MOVABLE);
	}

	end = ktime_get();
err:
	thaw_processes();

	if (ret < 0)
		return ret;

	pr_info("stolen %lu pages in %llu msec.(drn,islt,stel %llu %llu %llu)\n",
		nr_pages, ktime_ms_delta(end, begin),
		ktime_ms_delta(steal, isolate),
		ktime_ms_delta(isolate, begin),
		ktime_ms_delta(end, steal));

	return (int)nr_pages;
}

static unsigned long page_steal_debugfs_base_pfn;
/* updated by writing to debugfs/steal, identified by reading debugfs/count */
static int page_steal_debugfs_count;
DEFINE_MUTEX(page_steal_debugfs_lock);

static int page_steal_debugfs_steal_write(void *data, u64 val)
{
	unsigned long base = page_steal_debugfs_base_pfn;
	int count = (int)val;
	int ret;

	if (base + count < base) {
		pr_err("too many page count %d\n", count);
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&page_steal_debugfs_lock);
	if (ret < 0)
		return ret;

	if (count == 0) {
		if (page_steal_debugfs_count != 0) {
			unsigned long end = base + page_steal_debugfs_count;
			/*
			 * pages are not allocated but remained in the free list
			 * of MIGRATE_ISOLATE.
			 */
			undo_isolate_page_range(base, end, MIGRATE_MOVABLE);

			page_steal_debugfs_count = 0;
		}

		mutex_unlock(&page_steal_debugfs_lock);
		return 0;
	}

	ret = test_reclaim_pages_range(base, count);
	if (ret > 0)
		page_steal_debugfs_count = ret;

	mutex_unlock(&page_steal_debugfs_lock);

	if (!ret)
		return -EBUSY;
	if (ret < 0)
		return ret;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(page_steal_debugfs_steal_fops, NULL,
			page_steal_debugfs_steal_write, "%llu\n");

static int page_steal_debugfs_base_pfn_get(void *data, u64 *val)
{
	*val = page_steal_debugfs_base_pfn;
	return 0;
}

static int page_steal_debugfs_base_pfn_set(void *data, u64 val)
{
	unsigned long pfn = ALIGN_DOWN(val, pageblock_nr_pages);
	int ret;

	if (!pfn_valid(pfn)) {
		pr_err("invalid pfn %#lx\n", pfn);
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&page_steal_debugfs_lock);
	if (ret < 0)
		return ret;

	if (page_steal_debugfs_count != 0) {
		/*
		 * changing debugfs/base_pfn is not allowed during a page is stolen
		 * to return the stolen pages correctly.
		 */
		mutex_unlock(&page_steal_debugfs_lock);
		return -EBUSY;
	}

	page_steal_debugfs_base_pfn = pfn;

	mutex_unlock(&page_steal_debugfs_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(page_steal_debugfs_base_pfn_fops,
			page_steal_debugfs_base_pfn_get,
			page_steal_debugfs_base_pfn_set, "0x%016llx\n");


static struct dentry *page_steal_debugfs_root;

static int __init page_steal_debugfs_init(void)
{
	page_steal_debugfs_root = debugfs_create_dir("page_steal", NULL);
	if (!page_steal_debugfs_root)
		return -ENOMEM;

	debugfs_create_file("steal", 0200,
			    page_steal_debugfs_root,
			    NULL,
			    &page_steal_debugfs_steal_fops);
	debugfs_create_file("base_pfn", 0600,
			    page_steal_debugfs_root,
			    NULL,
			    &page_steal_debugfs_base_pfn_fops);
	debugfs_create_u32("count", 0400,
			    page_steal_debugfs_root,
			    &page_steal_debugfs_count);

	return 0;
}
late_initcall(page_steal_debugfs_init);
