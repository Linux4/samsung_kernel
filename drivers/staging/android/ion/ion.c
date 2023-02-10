// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/ion/ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/ion_exynos.h>

#include <trace/systrace_mark.h>

#include "ion.h"
#include "ion_exynos.h"
#include "ion_debug.h"

static struct ion_device *internal_dev;
static int heap_id;
static atomic_long_t total_heap_bytes;

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			perrfn("buffer already found.");
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);

	get_task_comm(buffer->task_comm, current->group_leader);
	get_task_comm(buffer->thread_comm, current);
	buffer->pid = current->group_leader->pid;
	buffer->tid = current->pid;
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;
	long nr_alloc_cur, nr_alloc_peak;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;
	buffer->dev = dev;
	buffer->size = len;

	ret = heap->ops->allocate(heap, buffer, len, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, flags);
		if (ret)
			goto err2;
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	INIT_LIST_HEAD(&buffer->iovas);
	mutex_init(&buffer->lock);
	mutex_lock(&dev->buffer_lock);
	ret = exynos_ion_alloc_fixup(dev, buffer);
	if (ret < 0) {
		mutex_unlock(&dev->buffer_lock);
		goto err1;
	}

	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	nr_alloc_cur = atomic_long_add_return(len, &heap->total_allocated);
	nr_alloc_peak = atomic_long_read(&heap->total_allocated_peak);
	if (nr_alloc_cur > nr_alloc_peak)
		atomic_long_set(&heap->total_allocated_peak, nr_alloc_cur);

	atomic_long_add(len, &total_heap_bytes);
	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	perrfn("failed to alloc (len %zu, flag %#lx) buffer from %s heap",
	       len, flags, heap->name);
	return ERR_PTR(ret);
}

void ion_buffer_destroy(struct ion_buffer *buffer)
{
	ion_event_begin();

	exynos_ion_free_fixup(buffer);
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
	}
	atomic_long_sub(buffer->size, &buffer->heap->total_allocated);
	buffer->heap->ops->free(buffer);

	ion_event_end(ION_EVENT_TYPE_FREE, buffer);

	kfree(buffer);
}

static void _ion_buffer_destroy(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
	atomic_long_sub(buffer->size, &total_heap_bytes);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_destroy(buffer);
}

void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	ion_event_begin();

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr)) {
		perrfn("failed to alloc kernel address of %zu buffer",
		       buffer->size);
		return vaddr;
	}
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;

	ion_event_end(ION_EVENT_TYPE_KMAP, buffer);

	return vaddr;
}

void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

#ifndef CONFIG_ION_EXYNOS
static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg->dma_address = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

static int ion_dma_buf_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct sg_table *table;
	struct ion_buffer *buffer = dmabuf->priv;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table))
		return -ENOMEM;

	attachment->priv = table;

	return 0;
}

static void ion_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	free_duped_table(attachment->priv);
}

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct sg_table *table = attachment->priv;

	if (!dma_map_sg(attachment->dev, table->sgl, table->nents,
			direction))
		return ERR_PTR(-ENOMEM);

	return table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	dma_unmap_sg(attachment->dev, table->sgl, table->nents, direction);
}
#endif /* !CONFIG_ION_EXYNOS */

static int ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret = 0;

	ion_event_begin();

	if (!buffer->heap->ops->map_user) {
		perrfn("this heap does not define a method for mapping to userspace");
		return -EINVAL;
	}

	if ((buffer->flags & ION_FLAG_NOZEROED) != 0) {
		perrfn("mmap() to nozeroed buffer is not allowed");
		return -EACCES;
	}

	if ((buffer->flags & ION_FLAG_PROTECTED) != 0) {
		perrfn("mmap() to protected buffer is not allowed");
		return -EACCES;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		perrfn("failure mapping buffer to userspace");

	ion_event_end(ION_EVENT_TYPE_MMAP, buffer);

	return ret;
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	_ion_buffer_destroy(buffer);
}

static void *ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr;

	if (!buffer->heap->ops->map_kernel) {
		pr_err("%s: map kernel is not implemented by this heap.\n",
		       __func__);
		return ERR_PTR(-ENOTTY);
	}
	mutex_lock(&buffer->lock);
	vaddr = ion_buffer_kmap_get(buffer);
	mutex_unlock(&buffer->lock);

	if (IS_ERR(vaddr))
		return vaddr;

	return vaddr + offset * PAGE_SIZE;
}

static void ion_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
			       void *ptr)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}

}

static void *ion_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_get(buffer);
		mutex_unlock(&buffer->lock);
	}

	return buffer->vaddr;
}

static void ion_dma_buf_vunmap(struct dma_buf *dmabuf, void *ptr)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}
}

#ifndef CONFIG_ION_EXYNOS
static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct dma_buf_attachment *att;


	mutex_lock(&dmabuf->lock);
	list_for_each_entry(att, &dmabuf->attachments, node) {
		struct sg_table *table = att->priv;

		dma_sync_sg_for_cpu(att->dev, table->sgl, table->nents,
				    direction);
	}
	mutex_unlock(&dmabuf->lock);
	
	return 0;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct dma_buf_attachment *att;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry(att, &dmabuf->attachments, node) {
		struct sg_table *table = att->priv;

		dma_sync_sg_for_device(att->dev, table->sgl, table->nents,
				       direction);
	}
	mutex_unlock(&dmabuf->lock);

	return 0;
}
#endif

static int ion_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct ion_buffer *buffer = dmabuf->priv;

	*flags = (unsigned long)ION_HEAP_MASK(buffer->heap->type) << ION_HEAP_SHIFT;
	*flags |= ION_BUFFER_MASK(buffer->flags);

	return 0;
}

const struct dma_buf_ops ion_dma_buf_ops = {
#ifdef CONFIG_ION_EXYNOS
	.map_dma_buf = ion_exynos_map_dma_buf,
	.unmap_dma_buf = ion_exynos_unmap_dma_buf,
	.map_dma_buf_area = ion_exynos_map_dma_buf_area,
	.unmap_dma_buf_area = ion_exynos_unmap_dma_buf_area,
	.begin_cpu_access = ion_exynos_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_exynos_dma_buf_end_cpu_access,
#else
	.attach = ion_dma_buf_attach,
	.detach = ion_dma_buf_detatch,
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
#endif
	.mmap = ion_mmap,
	.release = ion_dma_buf_release,
	.map = ion_dma_buf_kmap,
	.unmap = ion_dma_buf_kunmap,
	.vmap = ion_dma_buf_vmap,
	.vunmap = ion_dma_buf_vunmap,
	.get_flags = ion_dma_buf_get_flags,
};

#define ION_EXPNAME_LEN (4 + 4 + 1) /* strlen("ion-") + strlen("2048") + '\0' */

struct dma_buf *__ion_alloc(size_t len, unsigned int heap_id_mask,
			    unsigned int flags)
{
	struct ion_device *dev = internal_dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	char expname[ION_EXPNAME_LEN];
	struct dma_buf *dmabuf;

	ion_event_begin();

	pr_debug("%s: len %zu heap_id_mask %u flags %x\n", __func__,
		 len, heap_id_mask, flags);
	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);

	if (!len) {
		perrfn("zero size allocation - heapmask %#x, flags %#x",
		       heap_id_mask, flags);
		return ERR_PTR(-EINVAL);
	}

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		systrace_mark_begin("%s(%s, %zu, 0x%x, 0x%x)\n",
			__func__, heap->name, len, heap_id_mask, flags);
		buffer = ion_buffer_create(heap, dev, len, flags);
		systrace_mark_end();
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer) {
		perrfn("no matching heap found against heapmaks %#x", heap_id_mask);
		return ERR_PTR(-ENODEV);
	}

	if (IS_ERR(buffer))
		return ERR_CAST(buffer);

	snprintf(expname, ION_EXPNAME_LEN, "ion-%d", buffer->id);
	exp_info.exp_name = expname;

	exp_info.ops = &ion_dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		perrfn("failed to export dmabuf (err %ld)", -PTR_ERR(dmabuf));
		_ion_buffer_destroy(buffer);
	}

	ion_event_end(ION_EVENT_TYPE_ALLOC, buffer);

	return dmabuf;
}

int ion_alloc(size_t len, unsigned int heap_id_mask, unsigned int flags)
{
	struct dma_buf *dmabuf = __ion_alloc(len, heap_id_mask, flags);
	int fd;

	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		perrfn("failed to get dmabuf fd (err %d)",  -fd);
		dma_buf_put(dmabuf);
	}

	return fd;
}

#define MAX_ION_ACC_PROCESS	16	/* should be smaller than bitmap */
struct ion_size_account {
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
	size_t size;
};
static struct ion_size_account ion_size_acc[MAX_ION_ACC_PROCESS];
static int ion_dbg_idx_new;
static int ion_dbg_idx_last;

static inline int __ion_account_add_buf_locked(struct ion_buffer *buffer)
{
	int i;

	if (ion_dbg_idx_new &&
			(ion_size_acc[ion_dbg_idx_last].pid == buffer->pid)) {
		ion_size_acc[ion_dbg_idx_last].size += buffer->size;
		return 0;
	}
	for (i = 0; i < ion_dbg_idx_new; i++) {
		if (ion_size_acc[i].pid == buffer->pid) {
			ion_size_acc[i].size += buffer->size;
			ion_dbg_idx_last = i;
			return 0;
		}
	}
	if (ion_dbg_idx_new == MAX_ION_ACC_PROCESS) {
		pr_warn_once("out of ion_size_account idx\n");
		return -1;
	}
	ion_size_acc[ion_dbg_idx_new].pid = buffer->pid;
	ion_size_acc[ion_dbg_idx_new].size = buffer->size;
	strncpy(ion_size_acc[ion_dbg_idx_new].task_comm, buffer->task_comm,
		TASK_COMM_LEN);
	ion_dbg_idx_last = ion_dbg_idx_new++;
	return 0;
}

static inline void __ion_account_print_locked(void)
{
	int i, heaviest_idx;
	size_t heaviest_size = 0;
	size_t total = 0;

	if (!ion_dbg_idx_new)
		return;
	pr_info("ion_size: accounted by thread group\n");
	pr_info("ion_size: %16s( %3s ) %8s\n", "task_comm", "pid", "size(kb)");
	for (i = 0; i < ion_dbg_idx_new; i++) {
		pr_info("[%d]       %16s(%5u) %8zu\n", i, ion_size_acc[i].task_comm,
			ion_size_acc[i].pid, ion_size_acc[i].size / SZ_1K);
		if (heaviest_size < ion_size_acc[i].size) {
			heaviest_size = ion_size_acc[i].size ;
			heaviest_idx = i;
		}
		total += ion_size_acc[i].size;
	}
	if (heaviest_size)
		pr_info("heaviest_task_ion:%s(%5u) size:%zuKB, total:%zuKB/%luKB\n",
			ion_size_acc[heaviest_idx].task_comm,
			ion_size_acc[heaviest_idx].pid, heaviest_size / SZ_1K,
			total / SZ_1K, totalram_pages << (PAGE_SHIFT - 10));
}

bool ion_account_print_usage(void)
{
	struct rb_node *n;
	struct ion_buffer *buffer;
	unsigned int system_heap_id;
	struct ion_device *dev = internal_dev;
	bool locked;

	if (!dev)
		return false;
	system_heap_id = get_ion_system_heap_id();
	if (IS_ERR(ERR_PTR(system_heap_id)))
		return false;
	locked = mutex_trylock(&dev->buffer_lock);
	if (!locked)
		return false;
	ion_dbg_idx_new = 0;
	ion_dbg_idx_last = -1;
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		buffer = rb_entry(n, struct ion_buffer, node);
		if (buffer->heap->id == system_heap_id)
			__ion_account_add_buf_locked(buffer);
	}
	__ion_account_print_locked();
	mutex_unlock(&dev->buffer_lock);

	return true;
}

int ion_query_heaps(struct ion_heap_query *query)
{
	struct ion_device *dev = internal_dev;
	struct ion_heap_data __user *buffer = u64_to_user_ptr(query->heaps);
	int ret = -EINVAL, cnt = 0, max_cnt;
	struct ion_heap *heap;
	struct ion_heap_data hdata;

	down_read(&dev->lock);
	if (!buffer) {
		query->cnt = dev->heap_cnt;
		ret = 0;
		goto out;
	}

	if (query->cnt <= 0) {
		perrfn("invalid heapdata count %u",  query->cnt);
		goto out;
	}

	max_cnt = query->cnt;

	plist_for_each_entry(heap, &dev->heaps, node) {
		memset(&hdata, 0, sizeof(hdata));

		strncpy(hdata.name, heap->name, MAX_HEAP_NAME);
		hdata.name[sizeof(hdata.name) - 1] = '\0';
		hdata.type = heap->type;
		hdata.heap_id = heap->id;

		if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
			hdata.heap_flags = ION_HEAPDATA_FLAGS_DEFER_FREE;

		if (heap->ops->query_heap)
			heap->ops->query_heap(heap, &hdata);

		if (copy_to_user(&buffer[cnt], &hdata, sizeof(hdata))) {
			ret = -EFAULT;
			goto out;
		}

		cnt++;
		if (cnt >= max_cnt)
			break;
	}

	query->cnt = cnt;
	ret = 0;
out:
	up_read(&dev->lock);
	return ret;
}

struct ion_heap *ion_get_heap_by_name(const char *heap_name)
{
	struct ion_device *dev = internal_dev;
	struct ion_heap *heap;

	plist_for_each_entry(heap, &dev->heaps, node) {
		if (strlen(heap_name) != strlen(heap->name))
			continue;
		if (strcmp(heap_name, heap->name) == 0)
			return heap;
	}

	return NULL;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = ion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ion_ioctl,
#endif
};

static int debug_shrink_set(void *data, u64 val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = val;

	if (!val) {
		objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
		sc.nr_to_scan = objs;
	}

	heap->shrinker.scan_objects(&heap->shrinker, &sc);
	return 0;
}

static int debug_shrink_get(void *data, u64 *val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = 0;

	objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
	*val = objs;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_shrink_fops, debug_shrink_get,
			debug_shrink_set, "%llu\n");

void ion_device_add_heap(struct ion_heap *heap)
{
	struct ion_device *dev = internal_dev;
	int ret;

	if (!heap->ops->allocate || !heap->ops->free)
		perrfn("can not add heap with invalid ops struct.");

	spin_lock_init(&heap->free_lock);
	heap->free_list_size = 0;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_init_deferred_free(heap);

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink) {
		ret = ion_heap_init_shrinker(heap);
		if (ret)
			pr_err("%s: Failed to register shrinker\n", __func__);
	}

	heap->dev = dev;
	down_write(&dev->lock);
	heap->id = heap_id++;
	/*
	 * use negative heap->id to reverse the priority -- when traversing
	 * the list later attempt higher id numbers first
	 */
	plist_node_init(&heap->node, -heap->id);
	plist_add(&heap->node, &dev->heaps);

	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debugfs_create_file(debug_name, 0644, dev->debug_root,
				    heap, &debug_shrink_fops);
	}

	ion_debug_heap_init(heap);

	dev->heap_cnt++;
	up_write(&dev->lock);
}
EXPORT_SYMBOL(ion_device_add_heap);

static ssize_t
total_heaps_kb_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	u64 size_in_bytes = atomic_long_read(&total_heap_bytes);

	return sprintf(buf, "%llu\n", div_u64(size_in_bytes, 1024));
}

static ssize_t
total_pools_kb_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	u64 size_in_bytes = ion_page_pool_nr_pages() * PAGE_SIZE;

	return sprintf(buf, "%llu\n", div_u64(size_in_bytes, 1024));
}

static struct kobj_attribute total_heaps_kb_attr =
	__ATTR_RO(total_heaps_kb);

static struct kobj_attribute total_pools_kb_attr =
	__ATTR_RO(total_pools_kb);

static struct attribute *ion_device_attrs[] = {
	&total_heaps_kb_attr.attr,
	&total_pools_kb_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ion_device);

static int ion_init_sysfs(void)
{
	struct kobject *ion_kobj;
	int ret;

	ion_kobj = kobject_create_and_add("ion", kernel_kobj);
	if (!ion_kobj)
		return -ENOMEM;

	ret = sysfs_create_groups(ion_kobj, ion_device_groups);
	if (ret) {
		kobject_put(ion_kobj);
		return ret;
	}

	return 0;
}

static int ion_device_create(void)
{
	struct ion_device *idev;
	int ret;

	idev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (!idev)
		return -ENOMEM;

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		goto err_reg;
	}

	ret = ion_init_sysfs();
	if (ret) {
		pr_err("ion: failed to add sysfs attributes.\n");
		goto err_sysfs;
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (idev->debug_root)
		ion_debug_initialize(idev);

	exynos_ion_fixup(idev);
	idev->buffers = RB_ROOT;
	mutex_init(&idev->buffer_lock);
	init_rwsem(&idev->lock);
	plist_head_init(&idev->heaps);
	internal_dev = idev;
	return 0;

err_sysfs:
	misc_deregister(&idev->dev);
err_reg:
	kfree(idev);
	return ret;
}

#ifdef CONFIG_ION_MODULE
int ion_module_init(void)
{
	int ret;

	ret = ion_device_create();
#ifdef CONFIG_ION_SYSTEM_HEAP
	if (ret)
		return ret;

	ret = ion_system_heap_create();
	if (ret)
		return ret;

	ret = ion_system_contig_heap_create();
#endif
#ifdef CONFIG_ION_CMA_HEAP
	if (ret)
		return ret;

	ret = ion_add_cma_heaps();
#endif
	return ret;
}

module_init(ion_module_init);
#else
subsys_initcall(ion_device_create);
#endif

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Ion memory allocator");
