// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF HPA heap exporter for Samsung
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/anon_inodes.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direct.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/genalloc.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/shrinker.h>
#include <trace/hooks/mm.h>

#include "secure_buffer.h"

struct hpa_dma_heap {
	struct dma_heap *dma_heap;
	const char *name;
	unsigned char protection_id;
	atomic_long_t total_bytes;
};

struct hpa_dma_buffer {
	struct hpa_dma_heap *heap;
	struct sg_table sg_table;
	void *priv;
	unsigned long i_node;
};

static struct hpa_dma_heap hpa_heaps[] = {
	{
		.name = "vframe-secure",
		.protection_id = 5,
	},
	{
		.name = "system-secure-vframe-secure",
		.protection_id = 5,
	},
	{
		.name = "vscaler-secure",
		.protection_id = 6,
	},
	{
		.name = "system-secure-gpu_buffer-secure",
		.protection_id = 9,
	},
};

#define HPA_EVENT_LOG	256
#define HPA_EVENT_CLAMP_ID(id) ((id) & (HPA_EVENT_LOG - 1))

static atomic_t hpa_eventid;

enum hpa_event_type {
	HPA_EVENT_ALLOC = 0,
	HPA_EVENT_ALLOC_POOL,
	HPA_EVENT_ALLOC_PAGE,
	HPA_EVENT_FLUSH,
	HPA_EVENT_PROT,
	HPA_EVENT_UNPROT,
	HPA_EVENT_FREE,
};

static char * const hpa_event_name[] = {
	"alloc",
	"alloc_pool",
	"alloc_page",
	"flush",
	"prot",
	"unprot",
	"free",
};

static struct hpa_event {
	ktime_t timestamp;
	unsigned long elapsed;
	size_t size;
	unsigned char protid;
	enum hpa_event_type type;
} hpa_events[HPA_EVENT_LOG];

void hpa_event_record(enum hpa_event_type type, size_t size, unsigned char protid, ktime_t begin)
{
	int idx = HPA_EVENT_CLAMP_ID(atomic_inc_return(&hpa_eventid));
	struct hpa_event *event = &hpa_events[idx];

	event->timestamp = begin;
	event->elapsed = ktime_us_delta(ktime_get(), begin);
	event->type = type;
	event->size = size;
	event->protid = protid;
}

#ifdef CONFIG_DEBUG_FS
static int hpa_event_show(struct seq_file *s, void *unused)
{
	int index = HPA_EVENT_CLAMP_ID(atomic_read(&hpa_eventid) + 1);
	int i = index;

	seq_printf(s, "%14s %18s %16s %16s %10s\n",
		   "timestamp", "event", "protection id", "size(kb)", "elapsed(us)");

	do {
		struct hpa_event *event = &hpa_events[HPA_EVENT_CLAMP_ID(i)];
		struct timespec64 ts = ktime_to_timespec64(event->timestamp);

		if (event->elapsed == 0)
			continue;

		seq_printf(s, "[%06lld.%06ld]", ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
		seq_printf(s, "%18s %16d %16zd %10ld", hpa_event_name[event->type],
			   event->protid, event->size >> 10, event->elapsed);

		if (event->elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		seq_puts(s, "\n");
	} while (HPA_EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int hpa_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, hpa_event_show, inode->i_private);
}

static const struct file_operations hpa_event_fops = {
	.open = hpa_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void hpa_debug_init(void)
{
	struct dentry *root = debugfs_create_dir("hpa", NULL);

	if (IS_ERR(root)) {
		pr_err("Failed to create debug directory for dma_heap");
		return;
	}

	if (IS_ERR(debugfs_create_file("event", 0444, root, NULL, &hpa_event_fops)))
		pr_err("Failed to create event file for dma_heap\n");
}
#else
void hpa_debug_init(void)
{
}
#endif

#define MAX_EXCEPTION_AREAS 4
static phys_addr_t hpa_exception_areas[MAX_EXCEPTION_AREAS][2];
static int nr_hpa_exception;

#define HPA_DEFAULT_ORDER 4
#define HPA_CHUNK_SIZE  (PAGE_SIZE << HPA_DEFAULT_ORDER)
#define HPA_PAGE_COUNT(len) (ALIGN(len, HPA_CHUNK_SIZE) / HPA_CHUNK_SIZE)
#define HPA_MAX_CHUNK_COUNT ((PAGE_SIZE * 4) / sizeof(struct page *))

static const unsigned int prot_ids[] = { 5, 6, 9 };
#define NR_PROT_IDS ARRAY_SIZE(prot_ids)

/**
 * struct hpa_page_pool - hpa page pool struct
 * @count:		array of number of pages in the pool
 * @items:		array of list of pages
 * @lock:		lock protecting this struct and especially the count item list
 *
 * Allows you to keep a pool of pre allocated pages to use
 */
struct hpa_page_pool {
	int count;
	struct list_head items;
	spinlock_t lock;
	unsigned int protid;
} hpa_pools[NR_PROT_IDS];

static unsigned int get_protindex(unsigned int protid)
{
	unsigned int i;

	for (i = 0; i < NR_PROT_IDS; i++) {
		if (prot_ids[i] == protid)
			return i;
	}
	BUG_ON(1);
}

static struct page *hpa_page_pool_remove(unsigned int protid)
{
	struct hpa_page_pool *pool = &hpa_pools[get_protindex(protid)];
	struct page *page;

	spin_lock(&pool->lock);
	page = list_first_entry_or_null(&pool->items, struct page, lru);
	if (page) {
		pool->count--;
		list_del_init(&page->lru);
		spin_unlock(&pool->lock);
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
				    -(1 << HPA_DEFAULT_ORDER));
		return page;
	}
	spin_unlock(&pool->lock);

	return NULL;
}

static void hpa_page_pool_add(struct page *page, unsigned int protid)
{
	struct hpa_page_pool *pool = &hpa_pools[get_protindex(protid)];

	spin_lock(&pool->lock);
	list_add_tail(&page->lru, &pool->items);
	pool->count++;
	spin_unlock(&pool->lock);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << HPA_DEFAULT_ORDER);
}

static unsigned long alloc_pages_from_pool(struct page **pages, int required, unsigned int protid)
{
	int alloced = 0;
	ktime_t begin = ktime_get();

	while (alloced < required) {
		pages[alloced] = hpa_page_pool_remove(protid);

		if (!pages[alloced])
			break;

		alloced++;
	}
	hpa_event_record(HPA_EVENT_ALLOC_POOL, alloced * HPA_CHUNK_SIZE, protid, begin);

	return alloced;
}

static unsigned long hpa_buffer_size(unsigned int nr_pages);
static int hpa_compare_pages(const void *p1, const void *p2);
static int hpa_heap_hvc(struct page **pages, unsigned int nr_pages,
			unsigned int hvc_fid, unsigned int protid);

static int alloc_pages_from_core(struct dma_heap *heap, struct page **pages, unsigned long nr_pages)
{
	struct hpa_dma_heap *hpa_heap = dma_heap_get_drvdata(heap);
	struct device *dev = dma_heap_get_dev(heap);
	ktime_t begin = ktime_get();
	unsigned long size = hpa_buffer_size(nr_pages);
	pgoff_t pg;
	int ret;
	unsigned int protid = hpa_heap->protection_id;

	ret = alloc_pages_highorder_except(HPA_DEFAULT_ORDER, pages, nr_pages,
					   hpa_exception_areas, nr_hpa_exception);
	hpa_event_record(HPA_EVENT_ALLOC_PAGE, size, protid, begin);
	if (ret)
		return ret;

	begin = ktime_get();
	for (pg = 0; pg < nr_pages; pg++) {
		dma_addr_t dma_addr;

		dma_addr = dma_map_page(dev, pages[pg], 0, HPA_CHUNK_SIZE, DMA_TO_DEVICE);
		dma_unmap_page(dev, dma_addr, HPA_CHUNK_SIZE, DMA_FROM_DEVICE);
	}
	hpa_event_record(HPA_EVENT_FLUSH, size, protid, begin);

	sort(pages, nr_pages, sizeof(*pages), hpa_compare_pages, NULL);
	ret = hpa_heap_hvc(pages, nr_pages, HVC_DRM_TZMP2_PROT, protid);
	if (ret) {
		for (pg = 0; pg < nr_pages; pg++)
			__free_pages(pages[pg], HPA_DEFAULT_ORDER);
	}

	return ret;
}

static unsigned long hpa_page_pool_get_count(void)
{
	unsigned int i, count = 0;

	/*
	 * No lock is required to access hpa_pool.count because it is not necessary
	 * to obtail the exact count. That function is only supported for debugging purposes.
	 */
	for (i = 0; i < NR_PROT_IDS; i++)
		count += hpa_pools[i].count;

	return count << HPA_DEFAULT_ORDER;
}

static int hpa_page_pool_do_shrink(struct hpa_page_pool *pool, int nr_to_scan)
{
	struct page **pages;
	int i = 0, freed = 0, nr_pages = 0;

	/*
	 * Allocate order-0 page to reduce allocation failure.
	 */
	nr_to_scan = min_t(int, nr_to_scan, (PAGE_SIZE / sizeof(*pages)) << HPA_DEFAULT_ORDER);
	pages = kmalloc_array(nr_to_scan >> HPA_DEFAULT_ORDER, sizeof(*pages), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(pages))
		return 0;

	while (freed < nr_to_scan) {
		struct page *page;

		page = hpa_page_pool_remove(pool->protid);
		if (!page)
			break;

		pages[nr_pages++] = page;
		freed += (1 << HPA_DEFAULT_ORDER);
	}

	if (nr_pages && !hpa_heap_hvc(pages, nr_pages, HVC_DRM_TZMP2_UNPROT, pool->protid)) {
		for (i = 0; i < nr_pages; i++)
			__free_pages(pages[i], HPA_DEFAULT_ORDER);
	}

	kfree(pages);

	return freed;
}

static unsigned long hpa_page_pool_shrink(int nr_to_scan)
{
	unsigned long nr_total = 0;
	int i, nr_freed;

	if (nr_to_scan == 0)
		return hpa_page_pool_get_count();

	for (i = 0; i < NR_PROT_IDS; i++) {
		nr_freed = hpa_page_pool_do_shrink(&hpa_pools[i], nr_to_scan);
		nr_to_scan -= nr_freed;
		nr_total += nr_freed;

		if (nr_to_scan <= 0)
			break;
	}

	return (nr_total) ? nr_total : SHRINK_STOP;
}

static unsigned long hpa_page_pool_shrink_count(struct shrinker *shrinker,
						struct shrink_control *sc)
{
	return hpa_page_pool_shrink(0);
}

static unsigned long hpa_page_pool_shrink_scan(struct shrinker *shrinker,
					       struct shrink_control *sc)
{
	if (sc->nr_to_scan == 0)
		return 0;
	return hpa_page_pool_shrink(sc->nr_to_scan);
}

struct shrinker hpa_pool_shrinker = {
	.count_objects = hpa_page_pool_shrink_count,
	.scan_objects = hpa_page_pool_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

static long hpa_heap_get_pool_size(struct dma_heap *heap)
{
	const char *name = dma_heap_get_name(heap);

	/*
	 * All HPA heaps share the page pool. We only calculate
	 * the pool for representative HPA heap. Otherwise, it is
	 * overcalculated by the number of registered HPA heaps.
	 */
	if (strcmp(name, hpa_heaps[0].name))
		return 0;

	return hpa_page_pool_get_count() << PAGE_SHIFT;
}

static unsigned long *hpa_create_page_list(struct page **pages, unsigned int nr_pages)
{
	unsigned long *page_list;
	unsigned int i;

	/*
	 * No need to allocate chunk array for one physically linear page.
	 */
	if (nr_pages == 1)
		return (unsigned long *)page_to_virt(pages[0]);

	page_list = kmalloc_array(nr_pages, sizeof(*page_list), GFP_KERNEL);
	if (!page_list)
		return NULL;

	for (i = 0; i < nr_pages; i++)
		page_list[i] = page_to_phys(pages[i]);

	return page_list;
}

static void hpa_free_page_list(unsigned long *pagelist, unsigned int nr_pages)
{
	if (nr_pages == 1)
		return;

	kfree(pagelist);
}

static unsigned long hpa_buffer_size(unsigned int nr_pages)
{
	return nr_pages << (PAGE_SHIFT + HPA_DEFAULT_ORDER);
}

static int hpa_heap_hvc(struct page **pages, unsigned int nr_pages,
			unsigned int hvc_fid, unsigned int protid)
{
	unsigned long *pagelist;
	ktime_t begin = ktime_get();
	int ret, event_id;

	pagelist = hpa_create_page_list(pages, nr_pages);
	if (!pagelist)
		return -ENOMEM;

	ret = ppmp_hvc(hvc_fid, nr_pages, protid, HPA_CHUNK_SIZE, virt_to_phys(pagelist));
	if (ret) {
		pr_err("%s: %#x (err=%d,size=%#lx,cnt=%u,protid=%u)\n",
		       __func__, hvc_fid, ret, hpa_buffer_size(nr_pages),
		       nr_pages, protid);

		ret = E_DRMPLUGIN_BUFFER_LIST_FULL ? -ENOSPC : -EACCES;
	}

	hpa_free_page_list(pagelist, nr_pages);

	event_id = (hvc_fid == HVC_DRM_TZMP2_PROT) ? HPA_EVENT_PROT : HPA_EVENT_UNPROT;
	hpa_event_record(event_id, hpa_buffer_size(nr_pages), protid, begin);

	return ret;
}

static int hpa_compare_pages(const void *p1, const void *p2)
{
	if (*((unsigned long *)p1) > (*((unsigned long *)p2)))
		return 1;
	else if (*((unsigned long *)p1) < (*((unsigned long *)p2)))
		return -1;
	return 0;
}

static struct hpa_dma_buffer* hpa_dma_buffer_alloc(struct hpa_dma_heap *hpa_heap,
						   size_t size, unsigned long nents)
{
	struct hpa_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	if (sg_alloc_table(&buffer->sg_table, nents, GFP_KERNEL)) {
		pr_err("failed to allocate sgtable with %lu entry\n", nents);
		kfree(buffer);
		return ERR_PTR(-ENOMEM);
	}

	buffer->heap = hpa_heap;

	return buffer;
}

void hpa_dma_buffer_free(struct hpa_dma_buffer *buffer)
{
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

static struct sg_table *hpa_dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static DEFINE_MUTEX(hpa_trace_lock);

struct hpa_trace_task {
	struct list_head node;
	struct task_struct *task;
	struct file *file;
};
static struct list_head hpa_task_list = LIST_HEAD_INIT(hpa_task_list);

#define MAX_HPA_MAP_DEVICE 64
static struct hpa_map_device {
	struct device *dev;
	size_t size;
	unsigned long mapcnt;
} hpa_map_lists[MAX_HPA_MAP_DEVICE];
static unsigned int hpa_map_devices;

static struct hpa_map_device *hpa_trace_get_device(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < hpa_map_devices; i++) {
		if (hpa_map_lists[i].dev == dev)
			return &hpa_map_lists[i];
	}

	BUG_ON(hpa_map_devices == MAX_HPA_MAP_DEVICE);
	hpa_map_lists[hpa_map_devices].dev = dev;

	return &hpa_map_lists[hpa_map_devices++];
}

void hpa_trace_map(struct dma_buf *dmabuf, struct device *dev)
{
	struct hpa_map_device *trace_device;

	mutex_lock(&hpa_trace_lock);
	trace_device = hpa_trace_get_device(dev);

	trace_device->size += dmabuf->size;
	trace_device->mapcnt++;

	mutex_unlock(&hpa_trace_lock);
}

void hpa_trace_unmap(struct dma_buf *dmabuf, struct device *dev)
{
	struct hpa_map_device *trace_device;

	mutex_lock(&hpa_trace_lock);
	trace_device = hpa_trace_get_device(dev);

	trace_device->size -= dmabuf->size;
	trace_device->mapcnt--;

	mutex_unlock(&hpa_trace_lock);
}

static int hpa_trace_task_release(struct inode *inode, struct file *file)
{
	struct hpa_trace_task *task = file->private_data;

	if (!(task->task->flags & PF_EXITING)) {
		pr_err("%s: Invalid to close '%d' on process '%s'(%x, %x)\n",
		       __func__, task->task->pid, task->task->comm,
		       task->task->flags, task->task->__state);
		dump_stack();
	}
	put_task_struct(task->task);

	mutex_lock(&hpa_trace_lock);
	list_del(&task->node);
	mutex_unlock(&hpa_trace_lock);

	kfree(task);

	return 0;
}

static const struct file_operations hpa_trace_task_fops = {
	.release = hpa_trace_task_release,
};

void hpa_trace_add_task(void)
{
	struct hpa_trace_task *task;
	unsigned char name[10];
	int fd;

	if (!current->mm && (current->flags & PF_KTHREAD))
		return;

	if (current->group_leader->pid == 1)
		return;

	mutex_lock(&hpa_trace_lock);

	list_for_each_entry(task, &hpa_task_list, node) {
		if (task->task == current->group_leader) {
			mutex_unlock(&hpa_trace_lock);
			return;
		}
	}

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		goto err;

	get_task_struct(current->group_leader);
	task->task = current->group_leader;

	fd = get_unused_fd_flags(O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto err_fd;

	scnprintf(name, 10, "%d", current->group_leader->pid);
	task->file = anon_inode_getfile(name, &hpa_trace_task_fops, task, O_RDWR);
	if (IS_ERR(task->file))
		goto err_inode;

	fd_install(fd, task->file);
	list_add_tail(&task->node, &hpa_task_list);

	mutex_unlock(&hpa_trace_lock);

	return;

err_inode:
	put_unused_fd(fd);
err_fd:
	put_task_struct(current->group_leader);
	kfree(task);
err:
	mutex_unlock(&hpa_trace_lock);
}

static struct sg_table *hpa_heap_map_dma_buf(struct dma_buf_attachment *a,
					     enum dma_data_direction direction)
{
	struct hpa_dma_buffer *buffer = a->dmabuf->priv;
	struct sg_table *table;
	int ret = 0;

	table = hpa_dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table))
		return table;

	a->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	ret = dma_map_sgtable(a->dev, table, direction, a->dma_map_attrs);
	if (ret) {
		sg_free_table(table);
		kfree(table);

		return ERR_PTR(ret);
	}

	hpa_trace_map(a->dmabuf, a->dev);

	return table;
}

static void hpa_heap_unmap_dma_buf(struct dma_buf_attachment *a,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	hpa_trace_unmap(a->dmabuf, a->dev);

	dma_unmap_sgtable(a->dev, table, direction, a->dma_map_attrs);

	sg_free_table(table);
	kfree(table);
}

static int hpa_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	pr_err("mmap() to protected buffer is not allowed\n");
	return -EACCES;
}

static int hpa_heap_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	pr_err("vmap() to protected buffer is not allowed\n");
	return -EACCES;
}

static void hpa_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct hpa_dma_buffer *buffer = dmabuf->priv;
	struct sg_table *sgt = &buffer->sg_table;
	struct scatterlist *sg;
	unsigned int i = 0;
	unsigned char protid = buffer->heap->protection_id;
	ktime_t begin = ktime_get();

	for_each_sgtable_sg(sgt, sg, i)
		hpa_page_pool_add(sg_page(sg), protid);

	atomic_long_sub(dmabuf->size, &buffer->heap->total_bytes);
	hpa_dma_buffer_free(buffer);
	hpa_event_record(HPA_EVENT_FREE, dmabuf->size, protid, begin);
}

#define HPA_HEAP_FLAG_PROTECTED BIT(1)

static int hpa_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	*flags = HPA_HEAP_FLAG_PROTECTED;

	return 0;
}

const struct dma_buf_ops hpa_dma_buf_ops = {
	.map_dma_buf = hpa_heap_map_dma_buf,
	.unmap_dma_buf = hpa_heap_unmap_dma_buf,
	.mmap = hpa_heap_mmap,
	.vmap = hpa_heap_vmap,
	.release = hpa_heap_dma_buf_release,
	.get_flags = hpa_heap_dma_buf_get_flags,
};

static struct dma_buf *hpa_heap_allocate(struct dma_heap *heap, unsigned long len,
					 unsigned long fd_flags, unsigned long heap_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct hpa_dma_heap *hpa_heap = dma_heap_get_drvdata(heap);
	struct hpa_dma_buffer *buffer;
	struct scatterlist *sg;
	struct dma_buf *dmabuf;
	struct page **pages;
	unsigned long size, nr_pages, nr_pool_pages;
	int ret, protret = 0;
	pgoff_t pg;
	ktime_t begin = ktime_get();
	unsigned char protid = hpa_heap->protection_id;

	size = ALIGN(len, HPA_CHUNK_SIZE);
	nr_pages = size / HPA_CHUNK_SIZE;

	if (nr_pages > HPA_MAX_CHUNK_COUNT) {
		pr_err("Too big size %lu (HPA limited size %lu)\n", len,
		       HPA_MAX_CHUNK_COUNT * HPA_CHUNK_SIZE);
		return ERR_PTR(-EINVAL);
	}

	pages = kmalloc_array(nr_pages, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return ERR_PTR(-ENOMEM);

	nr_pool_pages = alloc_pages_from_pool(pages, nr_pages, protid);

	if (nr_pool_pages < nr_pages) {
		ret = alloc_pages_from_core(heap, pages + nr_pool_pages, nr_pages - nr_pool_pages);
		if (ret) {
			for (pg = 0; pg < nr_pool_pages; pg++)
				hpa_page_pool_add(pages[pg], protid);

			goto err_alloc;
		}
	}

	buffer = hpa_dma_buffer_alloc(hpa_heap, size, nr_pages);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);
		goto err_buffer;
	}

	sg = buffer->sg_table.sgl;
	for (pg = 0; pg < nr_pages; pg++) {
		sg_set_page(sg, pages[pg], HPA_CHUNK_SIZE, 0);
		sg = sg_next(sg);
	}

	exp_info.ops = &hpa_dma_buf_ops;
	exp_info.exp_name = hpa_heap->name;
	exp_info.size = size;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto err_export;
	}
	buffer->i_node = file_inode(dmabuf->file)->i_ino;

	atomic_long_add(dmabuf->size, &hpa_heap->total_bytes);

	kvfree(pages);

	hpa_trace_add_task();

	hpa_event_record(HPA_EVENT_ALLOC, size, protid, begin);

	return dmabuf;

err_export:
	hpa_dma_buffer_free(buffer);
err_buffer:
	for (pg = 0; !protret && pg < nr_pages; pg++)
		hpa_page_pool_add(pages[pg], protid);
err_alloc:
	kvfree(pages);

	pr_err("failed to alloc (len %zu, %#lx %#lx) from %s heap",
	       len, fd_flags, heap_flags, hpa_heap->name);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops hpa_heap_ops = {
	.allocate = hpa_heap_allocate,
	.get_pool_size = hpa_heap_get_pool_size,
};

static void hpa_add_exception_area(void)
{
	struct device_node *np;

	for_each_node_by_name(np, "dma_heap_exception_area") {
		int naddr = of_n_addr_cells(np);
		int nsize = of_n_size_cells(np);
		phys_addr_t base, size;
		const __be32 *prop;
		int len;
		int i;

		prop = of_get_property(np, "#address-cells", NULL);
		if (prop)
			naddr = be32_to_cpup(prop);

		prop = of_get_property(np, "#size-cells", NULL);
		if (prop)
			nsize = be32_to_cpup(prop);

		prop = of_get_property(np, "ppmpu-limit", &len);
		if (prop && len > 0) {
			base = (phys_addr_t)of_read_number(prop, naddr);

			hpa_exception_areas[0][0] = base;
			hpa_exception_areas[0][1] = -1;

			nr_hpa_exception++;
		}

		prop = of_get_property(np, "exception-range", &len);
		if (prop && len > 0) {
			int n_area = len / (sizeof(*prop) * (nsize + naddr));

			n_area += nr_hpa_exception;
			n_area = min_t(int, n_area, MAX_EXCEPTION_AREAS);

			for (i = nr_hpa_exception; i < n_area ; i++) {
				base = (phys_addr_t)of_read_number(prop, naddr);
				prop += naddr;
				size = (phys_addr_t)of_read_number(prop, nsize);
				prop += nsize;

				hpa_exception_areas[i][0] = base;
				hpa_exception_areas[i][1] = base + size - 1;
			}

			nr_hpa_exception = n_area;
		}
	}
}

static void show_hpa_heap_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	struct hpa_dma_heap *heap = data;
	u64 total_size_kb = div_u64(atomic_long_read(&heap->total_bytes), 1024);

	if (total_size_kb)
		pr_info("%s: %llukb ", heap->name, total_size_kb);
}

struct hpa_fd_iterdata {
	int fd_ref_size;
	int fd_ref_cnt;
};

int hpa_traverse_filetable(const void *t, struct file *file, unsigned fd)
{
	struct hpa_fd_iterdata *iterdata = (struct hpa_fd_iterdata*)t;
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return 0;

	dmabuf = file->private_data;

	iterdata->fd_ref_cnt++;
	iterdata->fd_ref_size += dmabuf->size >> 10;

	return 0;
}

/*
 * The HPA buffers are referenced by attached device, and file descriptor of process.
 * These buffers don't need to consider mmap count because secure buffer can not be mmaped.
 * The file descriptor might be tracked on dma-buf-trace because that interates dma-buf
 * file descriptor when show_mem() calls, so we only show devices to attach the hpa buffers.
 */
static void show_hpa_trace_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	static DEFINE_RATELIMIT_STATE(hpa_map_ratelimit, HZ * 10, 1);
	struct hpa_trace_task *task;
	int i;
	bool show_header = false;

	if (!__ratelimit(&hpa_map_ratelimit))
		return;

	if (in_interrupt()) {
		pr_info("log is skipped in interrupt context\n");
		return;
	}

	if (!mutex_trylock(&hpa_trace_lock))
		return;

	if (!list_empty(&hpa_task_list))
		pr_info("%20s %5s %10s %10s (HPA allocation tasks)\n",
			"comm", "pid", "fdrefcnt", "fdsize(kb)");

	list_for_each_entry(task, &hpa_task_list, node) {
		struct hpa_fd_iterdata iterdata;
		bool locked;

		iterdata.fd_ref_cnt = iterdata.fd_ref_size = 0;

		locked = spin_trylock(&task->task->alloc_lock);
		if (locked) {
			iterate_fd(task->task->files, 0, hpa_traverse_filetable, &iterdata);
			spin_unlock(&task->task->alloc_lock);
		}

		pr_info("%20s %5d %10d %10d %s\n", task->task->comm, task->task->pid,
			iterdata.fd_ref_cnt, iterdata.fd_ref_size,
			locked ? "" : "-> skip by lock contention");
	}

	for (i = 0; i < hpa_map_devices; i++) {
		if (!hpa_map_lists[i].size && !hpa_map_lists[i].mapcnt)
			continue;

		if (!show_header) {
			pr_info("HPA attached device list:\n");
			pr_info("%20s %20s %10s\n", "attached device", "size(kb)", "mapcount");

			show_header = true;
		}

		pr_info("%20s %20zu %10lu\n", dev_name(hpa_map_lists[i].dev),
			hpa_map_lists[i].size >> 10, hpa_map_lists[i].mapcnt);
	}
	mutex_unlock(&hpa_trace_lock);

	for (i = 0; i < NR_PROT_IDS; i++)
		pr_err("HPA pool[%d] %ldKB\n",
		       hpa_pools[i].protid, hpa_buffer_size(hpa_pools[i].count) >> 10);
}

static int __init hpa_dma_heap_init(void)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *dma_heap;
	int i;

	for (i = 0; i < ARRAY_SIZE(hpa_heaps); i++) {
		exp_info.name = hpa_heaps[i].name;
		exp_info.priv = &hpa_heaps[i];
		exp_info.ops = &hpa_heap_ops;

		dma_heap = dma_heap_add(&exp_info);
		if (IS_ERR(dma_heap))
			return PTR_ERR(dma_heap);

		hpa_heaps[i].dma_heap = dma_heap;
		dma_coerce_mask_and_coherent(dma_heap_get_dev(dma_heap), DMA_BIT_MASK(36));
		register_trace_android_vh_show_mem(show_hpa_heap_handler, &hpa_heaps[i]);

		pr_info("Registered %s dma-heap successfully\n", hpa_heaps[i].name);
	}
	register_trace_android_vh_show_mem(show_hpa_trace_handler, NULL);

	for (i = 0; i < NR_PROT_IDS; i++) {
		INIT_LIST_HEAD(&hpa_pools[i].items);
		spin_lock_init(&hpa_pools[i].lock);
		hpa_pools[i].protid = prot_ids[i];
	}

	register_shrinker(&hpa_pool_shrinker, "hpa-page-pool-shrinker");

	hpa_add_exception_area();
	hpa_debug_init();

	return 0;
}
module_init(hpa_dma_heap_init);
MODULE_LICENSE("GPL v2");
