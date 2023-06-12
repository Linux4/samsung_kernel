/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/errno.h>	/* cma.h needs this */
#if defined(CONFIG_TZDEV_CMA)
#if defined(CONFIG_DMA_CMA)
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#elif !defined(CONFIG_ARCH_MSM)
#include <linux/cma.h>
#endif
#endif
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/migrate.h>
#include <linux/miscdevice.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include "sysdep.h"
#include "tzdev.h"
#include "tzlog.h"
#include "tzpm.h"
#include "tzprofiler.h"
#include "tz_cdev.h"
#include "tz_core_migration.h"
#include "tz_boost.h"
#include "tz_iwlog.h"
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

MODULE_AUTHOR("Jaemin Ryu <jm77.ryu@samsung.com>");
MODULE_AUTHOR("Vasily Leonenko <v.leonenko@samsung.com>");
MODULE_AUTHOR("Alex Matveev <alex.matveev@samsung.com>");
MODULE_DESCRIPTION("TZDEV driver");
MODULE_LICENSE("GPL");

int tzdev_verbosity = 0;

struct hrtimer tzdev_get_event_timer;

module_param(tzdev_verbosity, int, 0644);
MODULE_PARM_DESC(tzdev_verbosity, "0: normal, 1: verbose, 2: debug");

static DEFINE_MUTEX(tzdev_fd_mutex);
static int tzdev_sw_init_done = 0;
static int tzdev_fd_open;

static DEFINE_IDR(tzdev_mem_map);
static DEFINE_MUTEX(tzdev_mem_mutex);
static DECLARE_COMPLETION(tzdev_ow_comp);

#if defined(CONFIG_QCOM_SCM_ARMV8)
DEFINE_MUTEX(tzdev_smc_lock);
#endif

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
unsigned long tzdev_qc_clk = QSEE_CLK_OFF;
static DEFINE_MUTEX(tzdev_qc_clk_mutex);
#else
unsigned long tzdev_qc_clk = QSEE_CLK_ON;
#endif

struct tzdev_mem_reg {
	struct pid *pid;
	unsigned long nr_pages;
	struct page **pages;
};

#if defined(CONFIG_TZDEV_CMA)
#if defined(CONFIG_CMA) && !defined(CONFIG_DMA_CMA)
static struct cma_info tzdev_cma_info;
#elif defined(CONFIG_DMA_CMA)
struct page *tzdev_page = NULL;
#endif
static dma_addr_t tzdev_cma_addr = 0;
#endif

static struct tzio_sysconf tz_sysconf;

#ifdef CONFIG_TZDEV_SK_MULTICORE

static DEFINE_PER_CPU(struct tzio_aux_channel *, tzdev_aux_channel);

#define aux_channel_get(ch)		get_cpu_var(ch)
#define aux_channel_put(ch)		put_cpu_var(ch)
#define aux_channel_init(ch, cpu)	per_cpu(ch, cpu)

#else /* CONFIG_TZDEV_SK_MULTICORE */

static DEFINE_MUTEX(tzdev_aux_channel_lock);
static struct tzio_aux_channel *tzdev_aux_channel[NR_CPUS];

static struct tzio_aux_channel *aux_channel_get(struct tzio_aux_channel *ch[])
{
	mutex_lock(&tzdev_aux_channel_lock);
	BUG_ON(smp_processor_id() != 0);
	return ch[0];
}

#define aux_channel_put(ch)		mutex_unlock(&tzdev_aux_channel_lock)
#define aux_channel_init(ch, cpu)	ch[cpu]

#endif /* CONFIG_TZDEV_SK_MULTICORE */

struct tzio_aux_channel *tzdev_get_aux_channel(void)
{
	return aux_channel_get(tzdev_aux_channel);
}

void tzdev_put_aux_channel(void)
{
	aux_channel_put(tzdev_aux_channel);
}

static void *tzdev_mem_release_buf;

static int tzdev_alloc_aux_channel(int cpu)
{
	struct tzio_aux_channel *channel;
	struct page *page;
	int ret;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	channel = page_address(page);

	tzdev_print(0, "AUX Channel[%d] = 0x%p\n", cpu, channel);

	ret = tzdev_smc_connect(TZDEV_CONNECT_AUX, page_to_pfn(page), 1);
	if (ret) {
		__free_page(page);
		return ret;
	}

	aux_channel_init(tzdev_aux_channel, cpu) = channel;

	return 0;
}

static int tzdev_alloc_aux_channels(void)
{
	int ret;
	unsigned int i;

	for (i = 0; i < NR_SW_CPU_IDS; ++i) {
		ret = tzdev_alloc_aux_channel(i);
		if (ret)
			return ret;
	}

	return 0;
}

static int tzdev_alloc_mem_release_buffer(void)
{
	struct page *page;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	tzdev_mem_release_buf = page_address(page);

	tzdev_print(0, "AUX channels mem release buffer allocated\n");

	return 0;
}

static unsigned long __tzdev_get_user_pages(struct task_struct *task,
		struct mm_struct *mm, unsigned long start, unsigned long nr_pages,
		int write, int force, struct page **pages,
		struct vm_area_struct **vmas)
{
	struct page **cur_pages = pages;
	unsigned long nr_pinned = 0;
	int res;

	while (nr_pinned < nr_pages) {
		res = get_user_pages(task, mm, start, nr_pages - nr_pinned, write,
				force, cur_pages, vmas);
		if (res < 0)
			return nr_pinned;

		start += res * PAGE_SIZE;
		nr_pinned += res;
		cur_pages += res;
	}

	return nr_pinned;
}

/* This is the same approach to pinning user memory
 * as used in Infiniband drivers.
 * Refer to drivers/inifiniband/core/umem.c */
static int tzdev_get_user_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages, struct vm_area_struct **vmas)
{
	unsigned long i, locked, lock_limit, nr_pinned;

	if (!can_do_mlock())
		return -EPERM;

	down_write(&mm->mmap_sem);

	locked = nr_pages + mm->pinned_vm;
	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;

	if ((locked > lock_limit) && !capable(CAP_IPC_LOCK)) {
		up_write(&mm->mmap_sem);
		return -ENOMEM;
	}

	nr_pinned = __tzdev_get_user_pages(task, mm, start, nr_pages, write,
						force, pages, vmas);
	if (nr_pinned != nr_pages)
		goto fail;

	mm->pinned_vm = locked;
	up_write(&mm->mmap_sem);

	return 0;

fail:
	for (i = 0; i < nr_pinned; i++)
		put_page(pages[i]);

	up_write(&mm->mmap_sem);

	return -EFAULT;
}

static void tzdev_put_user_pages(struct page **pages, unsigned long nr_pages)
{
	unsigned long i;

	for (i = 0; i < nr_pages; i++) {
		/* NULL pointers may appear here due to unsuccessful migration */
		if (pages[i])
			put_page(pages[i]);
	}
}

static void tzdev_decrease_pinned_vm(struct mm_struct *mm, unsigned long nr_pages)
{
	down_write(&mm->mmap_sem);
	mm->pinned_vm -= nr_pages;
	up_write(&mm->mmap_sem);
}

#if defined(CONFIG_TZDEV_PAGE_MIGRATION)
static int tzdev_migrate_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages);

static int tzdev_verify_migration(struct page **pages, unsigned long nr_pages);

int isolate_lru_page(struct page *page);

static struct page *tzdev_alloc_kernel_page(struct page *page, unsigned long private, int **x)
{
	return alloc_page(GFP_KERNEL);
}

static void tzdev_free_kernel_page(struct page *page, unsigned long private)
{
	__free_page(page);
}

static unsigned long tzdev_get_migratetype(struct page *page)
{
	struct zone *zone;
	unsigned long flags;
	unsigned long migrate_type;

	/* Zone lock must be held to avoid race with
	 * set_pageblock_migratetype() */
	zone = page_zone(page);
	spin_lock_irqsave(&zone->lock, flags);
	migrate_type = get_pageblock_migratetype(page);
	spin_unlock_irqrestore(&zone->lock, flags);

	return migrate_type;
}

static int __tzdev_migrate_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages, unsigned long *verified_bitmap)
{
	unsigned long i = 0, migrate_nr = 0, nr_pin = 0;
	unsigned long cur_pages_index, cur_start, pinned, migrate_type;
	int res;
	struct page **cur_pages;
	LIST_HEAD(pages_list);
	int ret = 0;

	/* Add migrating pages to the list */
	while ((i = find_next_zero_bit(verified_bitmap, nr_pages, i)) < nr_pages) {
		migrate_type = tzdev_get_migratetype(pages[i]);
		/* Skip pages that is currently isolated by somebody.
		 * Isolated page may originally have MIGRATE_CMA type,
		 * so caller should repeat migration for such pages */
		if (migrate_type == MIGRATE_ISOLATE) {
			ret = -EAGAIN;
			i++;
			continue;
		}

		/* Mark non-CMA pages as verified and skip them */
		if (migrate_type != MIGRATE_CMA) {
			bitmap_set(verified_bitmap, i, 1);
			i++;
			continue;
		}

		/* Call migrate_prep() once if migration necessary */
		if (migrate_nr == 0)
			migrate_prep();

		/* Pages should be isolated from an LRU list before migration.
		 * If isolation failed skip this page and inform caller to
		 * repeat migrate operation */
		res = isolate_lru_page(pages[i]);
		if (res < 0) {
			ret = -EAGAIN;
			i++;
			continue;
		}

		list_add_tail(&pages[i]->lru, &pages_list);
		put_page(pages[i]);
		/* pages array will be refilled with migrated pages later */
		pages[i] = NULL;
		migrate_nr++;
		i++;
	}

	if (!migrate_nr)
		return ret;

	/* make migration */
	res = MIGRATE_PAGES(&pages_list, tzdev_alloc_kernel_page, tzdev_free_kernel_page);
	if (res) {
		PUTBACK_ISOLATED_PAGES(&pages_list);
		return -EFAULT;
	}

	/* pin migrated pages */
	i = 0;
	do {
		nr_pin = 0;

		/* find index of the next migrated page */
		while (i < nr_pages && pages[i])
			i++;

		cur_pages = &pages[i];
		cur_pages_index = i;
		cur_start = start + i * PAGE_SIZE;

		/* find continuous migrated pages range */
		while (i < nr_pages && !pages[i]) {
			nr_pin++;
			i++;
		}

		/* and pin it */
		down_write(&mm->mmap_sem);
		pinned = __tzdev_get_user_pages(task, mm, cur_start, nr_pin,
						write, force, cur_pages, NULL);
		up_write(&mm->mmap_sem);
		if (pinned != nr_pin)
			return -EFAULT;

		/* Check that migrated pages are not MIGRATE_CMA or MIGRATE_ISOLATE
		 * and mark them as verified. If it is not true inform caller
		 * to repeat migrate operation */
		if (tzdev_verify_migration(cur_pages, nr_pin) == 0)
			bitmap_set(verified_bitmap, cur_pages_index, nr_pin);
		else
			ret = -EAGAIN;

		migrate_nr -= nr_pin;
	} while (migrate_nr);

	return ret;
}

#define TZDEV_MIGRATION_MAX_RETRIES 20

static int tzdev_migrate_pages(struct task_struct *task, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages, int write,
		int force, struct page **pages)
{
	int ret;
	unsigned int retries = 0;
	unsigned long *verified_bitmap;
	size_t bitmap_size = DIV_ROUND_UP(nr_pages, BITS_PER_LONG);

	verified_bitmap = kcalloc(bitmap_size, sizeof(unsigned long), GFP_KERNEL);
	if (!verified_bitmap)
		return -ENOMEM;

	do {
		ret = __tzdev_migrate_pages(task, mm, start, nr_pages, write,
				force, pages, verified_bitmap);

		if (ret != -EAGAIN || (retries++ >= TZDEV_MIGRATION_MAX_RETRIES))
			break;
		msleep(1);
	} while (1);

	kfree(verified_bitmap);

	return ret;
}

static int tzdev_verify_migration(struct page **pages, unsigned long nr_pages)
{
	unsigned long migrate_type;
	int i;

	for (i = 0; i < nr_pages; i++) {
		migrate_type = tzdev_get_migratetype(pages[i]);
		if (migrate_type == MIGRATE_CMA || migrate_type == MIGRATE_ISOLATE)
			return -EFAULT;
	}

	return 0;
}
#endif

static int __tzdev_mem_free(int id, void *p, void *data)
{
	struct tzdev_mem_reg *mem = p;
	struct task_struct *task;
	struct mm_struct *mm;

	tzdev_put_user_pages(mem->pages, mem->nr_pages);

	task = get_pid_task(mem->pid, PIDTYPE_PID);
	put_pid(mem->pid);
	if (!task)
		goto out;

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		goto out;

	tzdev_decrease_pinned_vm(mm, mem->nr_pages);
	mmput(mm);

out:
	kfree(mem->pages);
	kfree(mem);

	return 0;
}

static void tzdev_get_nwd_sysconf(struct tzio_sysconf *s)
{
	s->flags = 0;
#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	s->flags |= SYSCONF_CRYPTO_CLOCK_MANAGEMENT;
#endif
}

static int tzdev_get_sysconf(struct tzio_sysconf *s)
{
	struct tzio_sysconf nwd_sysconf;
	struct tzio_aux_channel *ch;
	int ret = 0;

	/* Get sysconf from SWd */
	ch = aux_channel_get(tzdev_aux_channel);
	ret = tzdev_smc_get_swd_sysconf();
	aux_channel_put(tzdev_aux_channel);

	if (ret) {
		tzdev_print(0, "tzdev_smc_get_swd_sysconf() failed with %d\n", ret);
		return ret;
	}
	memcpy(s, ch->buffer, sizeof(struct tzio_sysconf));

	tzdev_get_nwd_sysconf(&nwd_sysconf);

	/* Merge NWd and SWd sysconf structures */
	s->flags |= nwd_sysconf.flags;

	return ret;
}

unsigned int tzdev_is_initialized(void)
{
	return tzdev_sw_init_done;
}

static int tzdev_open(struct inode *inode, struct file *filp)
{
#if defined(CONFIG_TZDEV_CMA)
	struct page *p;
	void *ptr;
	unsigned long pfn;
#endif
	int ret = 0;

	tzdev_migrate();

	mutex_lock(&tzdev_fd_mutex);
	if (tzdev_fd_open != 0) {
		ret = -EBUSY;
		goto out;
	}

	if (!tzdev_sw_init_done) {
		/* check kernel and driver version compatibility with Blowfish */
		ret = tzdev_smc_check_version();
		if (ret == -ENOSYS) {
			tzdev_print(0, "Minor version of TZDev driver is newer than version of"
				"Blowfish secure kernel.\nNot critical, continue...\n");
			ret = 0;
		} else if (ret) {
			tzdev_print(0, "The version of the Linux kernel or "
				"TZDev driver is not compatible with Blowfish "
				"secure kernel\n");
			goto out;
		}

#if defined(CONFIG_TZDEV_CMA)
		if (tzdev_cma_addr != 0) {
			for (pfn = __phys_to_pfn(tzdev_cma_addr);
				pfn < __phys_to_pfn(tzdev_cma_addr +
					CONFIG_TZDEV_MEMRESSZ - CONFIG_TZDEV_MEMRESSZPROT);
				pfn++) {
				p = pfn_to_page(pfn);
				ptr = kmap(p);
				/*
				 * We can just invalidate here, but kernel doesn't
				 * export cache invalidation functions.
				 */
				__flush_dcache_area(ptr, PAGE_SIZE);
				kunmap(p);
			}
			outer_inv_range(tzdev_cma_addr, tzdev_cma_addr +
				CONFIG_TZDEV_MEMRESSZ - CONFIG_TZDEV_MEMRESSZPROT);

			ret = tzdev_smc_mem_reg((unsigned long)__phys_to_pfn(tzdev_cma_addr),
				(unsigned long)(get_order(CONFIG_TZDEV_MEMRESSZ) + PAGE_SHIFT));
			if (ret) {
				tzdev_print(0, "Registration of CMA region in SW failed;\n");
				goto out;
			}
		}
#endif

		BUG_ON(tzdev_alloc_aux_channels());

		BUG_ON(tzdev_alloc_mem_release_buffer());

		BUG_ON(tz_iwlog_alloc_channels());

		BUG_ON(tzprofiler_initialize());

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT)
		BUG_ON(tzdev_qc_pm_clock_initialize());
#endif
		BUG_ON(tzdev_get_sysconf(&tz_sysconf));

		tzdev_sw_init_done = 1;
	}

	tzdev_fd_open++;
	tzdev_smc_nwd_alive();

out:
	mutex_unlock(&tzdev_fd_mutex);
	return ret;
}

static int tzdev_release(struct inode *inode, struct file *filp)
{
#ifdef CONFIG_TZDEV_NWD_PANIC_ON_CLOSE
	panic("tzdev invalid close\n");
#endif

	mutex_lock(&tzdev_fd_mutex);
	tz_boost_disable();

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	if (tzdev_qc_clk == QSEE_CLK_ON) {
		tzdev_qc_pm_clock_disable();
		tzdev_qc_clk = QSEE_CLK_OFF;
	}
#endif

	tzdev_fd_open--;
	BUG_ON(tzdev_fd_open);

	/* FIXME Ugly, but works.
	 * Will be corrected in the course of SMC handler work */
	while (tzdev_smc_nwd_dead() != 1)
		;

	mutex_lock(&tzdev_mem_mutex);
	idr_for_each(&tzdev_mem_map, __tzdev_mem_free, NULL);

	IDR_REMOVE_ALL(&tzdev_mem_map);

	idr_destroy(&tzdev_mem_map);
	mutex_unlock(&tzdev_mem_mutex);

	mutex_unlock(&tzdev_fd_mutex);
	return 0;
}

int tzdev_get_access_info(struct tzio_access_info *s)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct file *exe_file;

	rcu_read_lock();

	task = find_task_by_vpid(s->pid);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}

	get_task_struct(task);
	rcu_read_unlock();

	s->gid = task->tgid;

	mm = get_task_mm(task);
	put_task_struct(task);
	if (!mm)
		return -ESRCH;

	exe_file = get_mm_exe_file(mm);
	mmput(mm);
	if (!exe_file)
		return -ESRCH;

	strncpy(s->ca_name, exe_file->f_path.dentry->d_name.name, CA_ID_LEN);
	fput(exe_file);

	return 0;
}

static void tzdev_mem_list_release(unsigned char *buf, unsigned int cnt)
{
	uint32_t *ids;
	unsigned int i;
	struct tzdev_mem_reg *mem;

	ids = (uint32_t *)buf;
	for (i = 0; i < cnt; i++) {
		mem = idr_find(&tzdev_mem_map, ids[i]);
		BUG_ON(!mem);
		idr_remove(&tzdev_mem_map, ids[i]);
		__tzdev_mem_free(ids[i], mem, NULL);
	}
}

#define TZDEV_IWSHMEM_IDS_PER_PAGE	(PAGE_SIZE / sizeof(uint32_t))

static int tzdev_mem_release(int id)
{
	struct tzio_aux_channel *ch;
	long cnt;
	int ret = 0;

	mutex_lock(&tzdev_mem_mutex);

	ch = aux_channel_get(tzdev_aux_channel);
	cnt = tzdev_smc_shmem_list_rls(id);
	if (cnt > 0) {
		BUG_ON(cnt > TZDEV_IWSHMEM_IDS_PER_PAGE);

		memcpy(tzdev_mem_release_buf, ch->buffer, cnt * sizeof(uint32_t));
		aux_channel_put(tzdev_aux_channel);

		tzdev_mem_list_release(tzdev_mem_release_buf, cnt);
	} else {
		ret = cnt;
		aux_channel_put(tzdev_aux_channel);
	}

	mutex_unlock(&tzdev_mem_mutex);

	return ret;
}

int tzdev_mem_register(struct tzio_mem_register *s)
{
#define TZDEV_PFNS_PER_PAGE	(PAGE_SIZE / sizeof(sk_pfn_t))
	struct pid *pid;
	struct task_struct *task;
	struct mm_struct *mm;
	struct page **pages;
	struct tzdev_mem_reg *mem;
	sk_pfn_t *pfns;
	struct tzio_aux_channel *ch;
	unsigned long start, end, nr_pages, offset;
	int ret, res, i, id;

	start = (unsigned long)s->ptr >> PAGE_SHIFT;
	end = ((unsigned long)s->ptr + s->size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	nr_pages = (s->size ? end - start : 0);
	offset = ((unsigned long)s->ptr) & ~PAGE_MASK;

	pid = find_get_pid(s->pid);
	if (!pid)
		return -ESRCH;

	task = get_pid_task(pid, PIDTYPE_PID);
	if (!task) {
		ret = -ESRCH;
		goto out_pid;
	}

	mm = get_task_mm(task);
	if (!mm) {
		ret = -ESRCH;
		goto out_task;
	}

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto out_mm;
	}

	pfns = kmalloc(nr_pages * sizeof(sk_pfn_t), GFP_KERNEL);
	if (!pfns) {
		ret = -ENOMEM;
		goto out_pages;
	}

	mem = kmalloc(sizeof(struct tzdev_mem_reg), GFP_KERNEL);
	if (!mem) {
		ret = -ENOMEM;
		goto out_pfns;
	}

	res = tzdev_get_user_pages(task, mm, (unsigned long)s->ptr,
			nr_pages, 1, !s->write, pages, NULL);
	if (res) {
		tzdev_print(0, "Failed to pin user pages (%d)\n", res);
		ret = res;
		goto out_mem;
	}

#if defined(CONFIG_TZDEV_PAGE_MIGRATION)
	/*
	 * In case of enabled migration it is possible that userspace pages
	 * will be migrated from current physical page to some other
	 * To avoid fails of CMA migrations we have to move pages to other
	 * region which can not be inside any CMA region. This is done by
	 * allocations with GFP_KERNEL flag to point UNMOVABLE memblock
	 * to be used for such allocations.
	 */
	res = tzdev_migrate_pages(task, mm, (unsigned long)s->ptr, nr_pages,
			1, !s->write, pages);
	if (res < 0) {
		tzdev_print(0, "Failed to migrate CMA pages (%d)\n", res);
		ret = res;
		goto out_pin;
	}
#endif

	for (i = 0; i < nr_pages; i++)
		pfns[i] = (sk_pfn_t)page_to_pfn(pages[i]);

	mutex_lock(&tzdev_mem_mutex);
	res = sysdep_idr_alloc(&tzdev_mem_map, mem);
	if (res < 0) {
		ret = res;
		mutex_unlock(&tzdev_mem_mutex);
		goto out_pin;
	} else
		id = res;

	ch = aux_channel_get(tzdev_aux_channel);

	for (i = 0; i < nr_pages; i += TZDEV_PFNS_PER_PAGE) {
		memcpy(ch->buffer, &pfns[i], min(nr_pages - i, TZDEV_PFNS_PER_PAGE) * sizeof(sk_pfn_t));
		if (tzdev_smc_shmem_list_reg(id, nr_pages, s->write)) {
			long cnt;
			cnt = tzdev_smc_shmem_list_rls(id);
			if (cnt > 0) {
				BUG_ON(cnt > TZDEV_IWSHMEM_IDS_PER_PAGE);

				memcpy(tzdev_mem_release_buf, ch->buffer, cnt * sizeof(uint32_t));
				aux_channel_put(tzdev_aux_channel);

				tzdev_mem_list_release(tzdev_mem_release_buf, cnt);
			} else
				aux_channel_put(tzdev_aux_channel);
			idr_remove(&tzdev_mem_map, id);
			mutex_unlock(&tzdev_mem_mutex);
			ret = -EFAULT;
			goto out_pin;
		}
	}

	aux_channel_put(tzdev_aux_channel);

	mem->pid = pid;
	mem->nr_pages = nr_pages;
	mem->pages = pages;

	mutex_unlock(&tzdev_mem_mutex);

	s->id = id;

	kfree(pfns);

	mmput(mm);
	put_task_struct(task);

	return 0;

out_pin:
	tzdev_put_user_pages(pages, nr_pages);
	tzdev_decrease_pinned_vm(mm, nr_pages);
out_mem:
	kfree(mem);
out_pfns:
	kfree(pfns);
out_pages:
	kfree(pages);
out_mm:
	mmput(mm);
out_task:
	put_task_struct(task);
out_pid:
	put_pid(pid);
	return ret;
}

static long tzdev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	tzdev_migrate();

	switch (cmd) {
	case TZIO_SMC: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;

		if (copy_from_user(&s, argp, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		d = tzdev_smc_command(s.args[0], s.args[1]);
		tzdev_convert_result(&s, &d);

		if (copy_to_user(argp, &s,  sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_GET_ACCESS_INFO: {
		int ret = 0;
		struct tzio_access_info __user *argp = (struct tzio_access_info __user *)arg;
		struct tzio_access_info s;

		if (copy_from_user(&s, argp, sizeof(struct tzio_access_info)))
			return -EFAULT;

		ret = tzdev_get_access_info(&s);
		if (ret)
			return ret;
		if (copy_to_user(argp, &s, sizeof(struct tzio_access_info)))
			return -EFAULT;

		return 0;
	}
	case TZIO_GET_SYSCONF: {
		struct tzio_sysconf __user *argp = (struct tzio_sysconf __user *)arg;

		if (copy_to_user(argp, &tz_sysconf, sizeof(struct tzio_sysconf)))
			return -EFAULT;

		return 0;
	}
	case TZIO_MEM_REGISTER: {
		int ret = 0;
		struct tzio_mem_register __user *argp = (struct tzio_mem_register __user *)arg;
		struct tzio_mem_register s;

		if (copy_from_user(&s, argp, sizeof(struct tzio_mem_register)))
			return -EFAULT;

		ret = tzdev_mem_register(&s);
		if (ret)
			return ret;

		if (copy_to_user(argp, &s, sizeof(struct tzio_mem_register)))
			return -EFAULT;

		return 0;
	}
	case TZIO_MEM_RELEASE: {
		return tzdev_mem_release(arg);
	}
	case TZIO_WAIT_EVT: {
		return wait_for_completion_interruptible(&tzdev_ow_comp);
	}
	case TZIO_GET_PIPE: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;

		if (copy_from_user(&s, argp, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		d = tzdev_smc_get_event();
		tzdev_convert_result(&s, &d);

		if (copy_to_user(argp, &s, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_UPDATE_REE_TIME: {
		struct tzio_smc_data __user *argp = (struct tzio_smc_data __user *)arg;
		struct tzio_smc_data s;
		struct tzdev_smc_data d;
		struct timespec ts;

		if (copy_from_user(&s, argp, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		getnstimeofday(&ts);
#if defined(CONFIG_TZDEV_32BIT_SECURE_KERNEL)
		d = tzdev_smc_update_ree_time((int)ts.tv_sec, (int)ts.tv_nsec);
#else
		d = tzdev_smc_update_ree_time(ts.tv_sec, ts.tv_nsec);
#endif
		tzdev_convert_result(&s, &d);

		if (copy_to_user(argp, &s, sizeof(struct tzio_smc_data)))
			return -EFAULT;

		return 0;
	}
	case TZIO_MIGRATE: {
		tzdev_print(0, "TZIO_MIGRATE: arg = %lu\n", arg);

		return tzdev_migration_request(arg);
	}

	case TZIO_BOOST: {
		tzdev_update_nw_cpu_mask(TZ_BOOST_CPU_BOOST_MASK);
		tz_boost_enable();
		return 0;
	}
	case TZIO_RELAX: {
		tzdev_update_nw_cpu_mask(0);
		tz_boost_disable();
		return 0;
	}

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	case TZIO_SET_QC_CLK: {
		unsigned long clk_cmd;
		unsigned long __user *argp = (unsigned long __user *)arg;

		if (copy_from_user(&clk_cmd, argp, sizeof(clk_cmd)))
			return -EFAULT;

		mutex_lock(&tzdev_qc_clk_mutex);
		if ((clk_cmd == QSEE_CLK_ON) && (tzdev_qc_clk == QSEE_CLK_OFF)) {
			tzdev_qc_pm_clock_enable();
			tzdev_qc_clk = QSEE_CLK_ON;
		} else if ((clk_cmd == QSEE_CLK_OFF) && (tzdev_qc_clk == QSEE_CLK_ON)) {
			tzdev_qc_pm_clock_disable();
			tzdev_qc_clk = QSEE_CLK_OFF;
		}
		mutex_unlock(&tzdev_qc_clk_mutex);

		return 0;
	}
#endif
	default:
		return -ENOTTY;
	}
}

static void tzdev_shutdown(void)
{
	if (tzdev_sw_init_done)
		tzdev_smc_shutdown();
}

static const struct file_operations tzdev_fops = {
	.owner = THIS_MODULE,
	.open = tzdev_open,
	.release = tzdev_release,
	.unlocked_ioctl = tzdev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzdev_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tzdev_cdev = {
	.name = "tzdev",
	.fops = &tzdev_fops,
	.owner = THIS_MODULE,
};

static struct syscore_ops tzdev_syscore_ops = {
	.shutdown = tzdev_shutdown
};

#if defined(CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND)
#include <linux/msm_ion.h>
#include "../qseecom_kernel.h"

void msm_crypto_workaround(void)
{
	enum qseecom_client_handle_type {
		QSEECOM_CLIENT_APP = 1,
		QSEECOM_LISTENER_SERVICE,
		QSEECOM_SECURE_SERVICE,
		QSEECOM_GENERIC,
		QSEECOM_UNAVAILABLE_CLIENT_APP,
	};
	struct qseecom_client_handle {
		u32 app_id;
		u8 *sb_virt;
		s32 sb_phys;
		uint32_t user_virt_sb_base;
		size_t sb_length;
		struct ion_handle *ihandle;		/* Retrieve phy addr */
	};
	struct qseecom_listener_handle {
		u32 id;
	};
	struct qseecom_dev_handle {
		enum qseecom_client_handle_type type;
		union {
			struct qseecom_client_handle client;
			struct qseecom_listener_handle listener;
		};
		bool released;
		int  abort;
		wait_queue_head_t abort_wq;
		atomic_t ioctl_count;
		bool perf_enabled;
		bool fast_load_enabled;
	};

	/* Bogus handles */
	struct qseecom_dev_handle dev_handle;
	struct qseecom_handle handle = {
		.dev = &dev_handle
	};
	int ret;

	ret = qseecom_set_bandwidth(&handle, 1);
	if (ret)
		tzdev_print(0, "qseecom_set_bandwidth failed ret = %d\n", ret);
}
#endif

static irqreturn_t tzdev_event_handler(int irq, void *ptr)
{
	complete(&tzdev_ow_comp);

	return IRQ_HANDLED;
}

#if CONFIG_TZDEV_IWI_PANIC != 0
static void dump_kernel_panic_bh(struct work_struct *work)
{
	tz_iwlog_read_channels();
}

static DECLARE_WORK(dump_kernel_panic, dump_kernel_panic_bh);

static irqreturn_t tzdev_panic_handler(int irq, void *ptr)
{
	schedule_work(&dump_kernel_panic);
	return IRQ_HANDLED;
}
#endif

static enum hrtimer_restart tzdev_get_event_timer_handler(struct hrtimer *timer)
{
	complete(&tzdev_ow_comp);

	return HRTIMER_NORESTART;
}

static int __init init_tzdev(void)
{
	int rc;

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT)
	tzdev_platform_register();
#endif

	rc = tz_cdev_register(&tzdev_cdev);
	if (rc)
		return rc;

	if (request_irq(CONFIG_TZDEV_IWI_EVENT, tzdev_event_handler, 0,
						"tzdev_iwi_event", NULL))
		tzdev_print(0, "TZDEV_IWI_EVENT registration failed\n");

#if CONFIG_TZDEV_IWI_PANIC != 0
	if (request_irq(CONFIG_TZDEV_IWI_PANIC, tzdev_panic_handler, 0,
						"tzdev_iwi_panic", NULL))
		tzdev_print(0, "TZDEV_IWI_PANIC registration failed\n");
#endif

#if defined(CONFIG_TZDEV_CMA)
	tzdev_cma_addr = 0;
#if defined(CONFIG_DMA_CMA)
	tzdev_page = dma_alloc_from_contiguous(tzdev.this_device,
			(CONFIG_TZDEV_MEMRESSZ - CONFIG_TZDEV_MEMRESSZPROT) >> PAGE_SHIFT,
			get_order(CONFIG_TZDEV_MEMRESSZ));
	if (!tzdev_page) {
		tzdev_print(0, "Allocation CMA region failed;"
		" Memory will not be registered in SWd\n");
		goto out;
	}
	tzdev_cma_addr = page_to_phys(tzdev_page);
#elif defined(CONFIG_CMA)
	if (cma_info(&tzdev_cma_info, tzdev.this_device, NULL) < 0) {
		tzdev_print(0, "Getting CMA info failed;"
		" Memory will not be registered in SWd\n");
		goto out;
	}
	tzdev_cma_addr = cma_alloc(tzdev.this_device, NULL,
		tzdev_cma_info.total_size, 0);
#endif
	if (!tzdev_cma_addr || IS_ERR_VALUE(tzdev_cma_addr))
		tzdev_print(0, "Allocation CMA region failed;"
		" Memory will not be registered in SWd\n");
out:
#endif

	tzdev_init_migration();
	register_syscore_ops(&tzdev_syscore_ops);

	hrtimer_init(&tzdev_get_event_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tzdev_get_event_timer.function = tzdev_get_event_timer_handler;
#if defined(CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND)
	/* Force crypto engine clocks on */
	msm_crypto_workaround();
#endif

	rc = tzdev_init_hotplug();

	return rc;
}

static void __exit exit_tzdev(void)
{
	tz_cdev_unregister(&tzdev_cdev);

	idr_for_each(&tzdev_mem_map, __tzdev_mem_free, NULL);

	IDR_REMOVE_ALL(&tzdev_mem_map);

	idr_destroy(&tzdev_mem_map);

#if defined(CONFIG_TZDEV_CMA)
	if (tzdev_cma_addr) {
#if defined(CONFIG_DMA_CMA)
		dma_release_from_contiguous(tzdev.this_device, tzdev_page,
				(CONFIG_TZDEV_MEMRESSZ - CONFIG_TZDEV_MEMRESSZPROT) >> PAGE_SHIFT);
#elif defined(CONFIG_CMA)
		cma_free(tzdev_cma_addr);
#endif
	}
	tzdev_cma_addr = 0;
#endif

	hrtimer_cancel(&tzdev_get_event_timer);

	unregister_syscore_ops(&tzdev_syscore_ops);
	tzdev_fini_migration();
	tzdev_shutdown();

	tzdev_exit_hotplug();

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT)
	tzdev_qc_pm_clock_finalize();
	tzdev_platform_unregister();
#endif
}

module_init(init_tzdev);
module_exit(exit_tzdev);
