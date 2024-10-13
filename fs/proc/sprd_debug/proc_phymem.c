#include <linux/module.h>
#include <linux/swap.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/quicklist.h>
#include <linux/vmalloc.h>
#include <asm/setup.h>
#include "../internal.h"

#define K(x) ((x) << (PAGE_SHIFT - 10))

extern struct meminfo meminfo;

static struct proc_dir_entry* phymem;

static int _phymem_map_proc_show(struct seq_file *m, void *v)
{
	int node, i, totalsize = 0;
	struct meminfo *mi = &meminfo;

	for_each_bank (i,mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1, pfn2;
		totalsize += bank->size;
		seq_printf(m, "0x%08lx - 0x%08lx    %lukB\n",
				bank->start, bank->start + bank->size, bank->size/1024);
	}
	seq_printf(m, "linux total memory: %dkB\n", totalsize/1024);
	return 0;
}

static int _phymem_pages_proc_show(struct seq_file *m, void *v)
{
	int free = 0, total = 0, reserved = 0;
	int other = 0, shared = 0, cached = 0, slab = 0, node, i;

	struct meminfo * mi = &meminfo;
	for_each_bank (i,mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1, pfn2;
		struct page *page, *end;
		pfn1 = bank_pfn_start(bank);
		pfn2 = bank_pfn_end(bank);
		page = pfn_to_page(pfn1);
		end  = pfn_to_page(pfn2 - 1) + 1;
		do {
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (PageSlab(page))
				slab++;
			else if (page_count(page) > 1)
				shared++;
			else if (!page_count(page))
				free++;
			else
				other++;
			page++;
		}while (page < end);
	}

	seq_printf(m, "pages of RAM       %d\n", total);
	seq_printf(m, "free pages         %d\n", free);
	seq_printf(m, "reserved pages     %d\n", reserved);
	seq_printf(m, "slab pages         %d\n", slab);
	seq_printf(m, "pages shared       %d\n", shared);
	seq_printf(m, "pages swap cached  %d\n", cached);
	seq_printf(m, "other pages        %d\n", other);
	return 0;
}

#define MLK_ROUNDUP(b, t) b, t, DIV_ROUND_UP(((t) - (b)), SZ_1K)

static int _phymem_dist_proc_show(struct seq_file *m, void *v)
{
	struct sysinfo sys_info;
	int node, i, pfn = 0, linux_total_size = 0;
	struct meminfo *mi = &meminfo;
	long cached;
	long kernelmem;
	struct vmalloc_info vmi;
	unsigned long pages[NR_LRU_LISTS];
	int lru;

	for_each_bank (i,mi) {
		struct membank *bank = &mi->bank[i];
		unsigned int pfn1, pfn2;
		linux_total_size += bank->size;
		pfn = bank_pfn_end(bank);
	}

	si_meminfo(&sys_info);
	si_swapinfo(&sys_info);

	cached = global_page_state(NR_FILE_PAGES) -
			total_swapcache_pages() - sys_info.bufferram;
	if (cached < 0)
		cached = 0;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_page_state(NR_LRU_BASE + lru);

	kernelmem = K(sys_info.totalram - sys_info.freeram - sys_info.bufferram - pages[LRU_ACTIVE_ANON]  \
			- pages[LRU_INACTIVE_ANON] - cached - total_swapcache_pages());
	get_vmalloc_info(&vmi);

	seq_printf(m, "mem: %lukB\n", pfn*4);
	seq_printf(m, "  |--mem_other: %lukB\n", pfn*4 - linux_total_size/1024);
	seq_printf(m, "  |--mem_linux: %lukB\n", linux_total_size/1024);
	seq_printf(m, "       |--reserved: %lukB\n", linux_total_size/1024 - K(sys_info.totalram));
	seq_printf(m, "       |--mem_total: %lukB\n", K(sys_info.totalram));
	seq_printf(m, "            |--free: %lukB\n", K(sys_info.freeram));
	seq_printf(m, "            |--buffer: %lukB\n", K(sys_info.bufferram));
	seq_printf(m, "            |--cache: %lukB\n", K(cached));
	seq_printf(m, "            |--swapcache: %lukB\n", K(total_swapcache_pages()));
	seq_printf(m, "            |--user: %lukB\n", K(pages[LRU_ACTIVE_ANON] + pages[LRU_INACTIVE_ANON]));
	seq_printf(m, "            |    |--active anon: %lukB\n", K(pages[LRU_ACTIVE_ANON]));
	seq_printf(m, "            |    |--inactive anon: %lukB\n", K(pages[LRU_INACTIVE_ANON]));
	seq_printf(m, "            |--kernel: %lukB\n", kernelmem);
	seq_printf(m, "                 |--stack: %lukB\n", global_page_state(NR_KERNEL_STACK) * THREAD_SIZE / 1024);
	seq_printf(m, "                 |--slab: %lukB\n", K(global_page_state(NR_SLAB_RECLAIMABLE) +global_page_state(NR_SLAB_UNRECLAIMABLE)));
	seq_printf(m, "                 |--pagetable: %lukB\n", K(global_page_state(NR_PAGETABLE)));
	seq_printf(m, "                 |--vmalloc: %lukB\n",
			kernelmem - global_page_state(NR_KERNEL_STACK) * THREAD_SIZE / 1024 \
			- K(global_page_state(NR_SLAB_RECLAIMABLE)  - global_page_state(NR_SLAB_UNRECLAIMABLE)) \
			-K(global_page_state(NR_PAGETABLE)));
	return 0;
}


static int _phymem_map_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _phymem_map_proc_show, NULL);
	return 0;
}

static int _phymem_pages_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _phymem_pages_proc_show, NULL);
	return 0;
}

static int _phymem_dist_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, _phymem_dist_proc_show, NULL);
	return 0;
}

static int _phymem_showmem_proc_show(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	/* show_mem(); */
	return 0;
}

struct file_operations phymem_map_proc_fops = {
	.open    = _phymem_map_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

struct file_operations phymem_pages_proc_fops = {
	.open    = _phymem_pages_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

struct file_operations phymem_dist_proc_fops = {
	.open    = _phymem_dist_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

struct file_operations phymem_showmem_proc_fops = {
	.open    = NULL,
	.write   = _phymem_showmem_proc_show,
	.llseek  = NULL,
	.release = NULL,
};

