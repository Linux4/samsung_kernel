#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/page-debug-flags.h>
#include <linux/poison.h>
#include <linux/ratelimit.h>
#include <linux/stacktrace.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <asm/current.h>

#define TRACK_ADDRS_COUNT 8

static void trace_page_track(struct page *page, int enable)
{
	struct tracker *p = &page->tracker[enable];

#ifdef CONFIG_STACKTRACE
		struct stack_trace trace;
		int i;

		trace.nr_entries = 0;
		trace.max_entries = TRACK_ADDRS_COUNT;
		trace.entries = p->addrs;
		trace.skip = 4;
		save_stack_trace(&trace);

		/* See rant in lockdep.c */
		if (trace.nr_entries != 0 &&
		    trace.entries[trace.nr_entries - 1] == ULONG_MAX)
			trace.nr_entries--;

		for (i = trace.nr_entries; i < TRACK_ADDRS_COUNT; i++)
			p->addrs[i] = 0;
#endif
	p->pid = current->pid;
	p->when = jiffies;
}

static inline void set_page_poison(struct page *page)
{
	__set_bit(PAGE_DEBUG_FLAG_POISON, &page->debug_flags);
}

static inline void clear_page_poison(struct page *page)
{
	__clear_bit(PAGE_DEBUG_FLAG_POISON, &page->debug_flags);
}

static inline bool page_poison(struct page *page)
{
	return test_bit(PAGE_DEBUG_FLAG_POISON, &page->debug_flags);
}

static void poison_page(struct page *page)
{
	void *addr = kmap_atomic(page);

	/* the 1st tracker will record the latest page free stack etc */
	trace_page_track(page, 0);
	set_page_poison(page);
	memset(addr, PAGE_POISON, PAGE_SIZE);
	kunmap_atomic(addr);
}

static void poison_pages(struct page *page, int n)
{
	int i;

	for (i = 0; i < n; i++)
		poison_page(page + i);
}

static bool single_bit_flip(unsigned char a, unsigned char b)
{
	unsigned char error = a ^ b;

	return error && !(error & (error - 1));
}

static void dump_page_tracker(struct page *page)
{
	int i, j;
	struct tracker *p;
	unsigned long where, from;
	phys_addr_t phys = page_to_phys(page);

	for (i = 0; i < 2; i++) {
		p = &page->tracker[i];
		pr_info("page %p phys %pa last %s by process %d @ %lu(jiffies) stack:\n",
			 page, &phys, i ? "alloc" : "free", p->pid, p->when);
		for (j = 0; j < TRACK_ADDRS_COUNT; j += 2) {
			where = p->addrs[j];
			from = p->addrs[j + 1];
			if (!where && !from)
				break;
#ifdef CONFIG_KALLSYMS
			pr_info("[<%08lx>] (%pS) from [<%08lx>] (%pS)\n",
					where, (void *)where, from, (void *)from);
#else
			pr_info("Function entered at [<%08lx>] from [<%08lx>]\n",
					where, from);
#endif
		}
		pr_info("----------------------------------------\n");
	}
}

static void check_poison_mem(struct page *page, size_t bytes)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 10);
	unsigned char *start;
	unsigned char *end;
	unsigned char *mem = kmap_atomic(page);

	start = memchr_inv(mem, PAGE_POISON, bytes);
	if (!start) {
		kunmap_atomic(mem);
		return;
	}

	for (end = mem + bytes - 1; end > start; end--) {
		if (*end != PAGE_POISON)
			break;
	}

	if (!__ratelimit(&ratelimit)) {
		kunmap_atomic(mem);
		return;
	} else if (start == end && single_bit_flip(*start, PAGE_POISON))
		printk(KERN_ERR "pagealloc: single bit error\n");
	else
		printk(KERN_ERR "pagealloc: memory corruption\n");

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 1, start,
			end - start + 1, 1);
	kunmap_atomic(mem);
	/* if found page was posion, dump the content of page tracker */
	dump_page_tracker(page);
	dump_stack();
	BUG_ON(1);
}

static void unpoison_page(struct page *page)
{
	if (!page_poison(page))
		return;

	check_poison_mem(page, PAGE_SIZE);
	clear_page_poison(page);

	/*the 2nd tracker will record the latest page allocated stack etc */
	trace_page_track(page, 1);
}

static void unpoison_pages(struct page *page, int n)
{
	int i;

	for (i = 0; i < n; i++)
		unpoison_page(page + i);
}

void kernel_map_pages(struct page *page, int numpages, int enable)
{
	if (enable)
		unpoison_pages(page, numpages);
	else
		poison_pages(page, numpages);
}
