/*
 *
 * drivers/staging/android/ion/ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/idr.h>
#include <linux/sched/task.h>
#include <linux/sprd_iommu.h>
#include "ion.h"

static struct ion_device *internal_dev;
static atomic_long_t total_heap_bytes;

bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_CACHED);
}

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
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
	struct sg_table *table;
	int i, ret;
	struct scatterlist *sg;
	struct timeval time;
	long nr_alloc_cur, nr_alloc_peak;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;

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

	table = buffer->sg_table;
	buffer->dev = dev;
	buffer->size = len;

	buffer->dev = dev;
	buffer->size = len;
	INIT_LIST_HEAD(&buffer->attachments);
	for_each_sg(buffer->sg_table->sgl, sg, buffer->sg_table->nents, i) {
		sg_dma_address(sg) = sg_phys(sg);
		sg_dma_len(sg) = sg->length;
	}
	mutex_init(&buffer->lock);
	mutex_lock(&dev->buffer_lock);
	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);

	buffer->pid = task_pid_nr(current->group_leader);
	get_task_comm(buffer->task_name, current->group_leader);
	do_gettimeofday(&time);
	time.tv_sec -= sys_tz.tz_minuteswest * 60;
	buffer->alloc_time = time;
	if (heap->type == ION_HEAP_TYPE_SYSTEM)

		atomic_long_add(len, &total_heap_bytes);
	nr_alloc_cur = atomic_long_add_return(len, &heap->total_allocated);
	nr_alloc_peak = atomic_long_read(&heap->total_allocated_peak);
	if (nr_alloc_cur > nr_alloc_peak)
		atomic_long_set(&heap->total_allocated_peak, nr_alloc_cur);
	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

void ion_buffer_destroy(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
	}
	atomic_long_sub(buffer->size, &buffer->heap->total_allocated);
	buffer->heap->ops->free(buffer);
	sprd_iommu_notifier_call_chain((void *)buffer);
	kfree(buffer);
}

static void _ion_buffer_destroy(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
	if (heap->type == ION_HEAP_TYPE_SYSTEM)
		atomic_long_sub(buffer->size, &total_heap_bytes);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_destroy(buffer);
}

static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

static void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

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
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

struct ion_dma_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
};

static int ion_dma_buf_attach(struct dma_buf *dmabuf, struct device *dev,
			      struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	struct ion_buffer *buffer = dmabuf->priv;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = dev;
	INIT_LIST_HEAD(&a->list);

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void ion_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);
	free_duped_table(a->table);

	kfree(a);
}

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table;
	unsigned long attrs = 0;


	table = a->table;

	if (!(buffer->flags & ION_FLAG_CACHED))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	if (!dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
			      direction, attrs))
		return ERR_PTR(-ENOMEM);

	return table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	unsigned long attrs = 0;

	if (!(buffer->flags & ION_FLAG_CACHED))
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents, direction,
			   attrs);
}

static int ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret = 0;

	if (!buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping to userspace\n",
		       __func__);
		return -EINVAL;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	return ret;
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct dma_buf_attachment *attachment =
		(struct dma_buf_attachment *)(buffer->priv_virt);

	ion_dma_buf_detatch(dmabuf, attachment);
	kfree(attachment);

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

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	mutex_lock(&buffer->lock);
	if (!(buffer->flags & ION_FLAG_CACHED))
		goto unlock;

	list_for_each_entry(a, &buffer->attachments, list) {
		dma_sync_sg_for_cpu(a->dev, a->table->sgl, a->table->nents,
				    direction);
	}
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	mutex_lock(&buffer->lock);

	if (!(buffer->flags & ION_FLAG_CACHED))
		goto unlock;

	list_for_each_entry(a, &buffer->attachments, list) {
		dma_sync_sg_for_device(a->dev, a->table->sgl, a->table->nents,
				       direction);
	}
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static const struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_mmap,
	.release = ion_dma_buf_release,
	.attach = ion_dma_buf_attach,
	.detach = ion_dma_buf_detatch,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
	.map_atomic = ion_dma_buf_kmap,
	.unmap_atomic = ion_dma_buf_kunmap,
	.map = ion_dma_buf_kmap,
	.unmap = ion_dma_buf_kunmap,
};

int ion_alloc(size_t len, unsigned int heap_id_mask, unsigned int flags)
{
	struct ion_device *dev = internal_dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int fd;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct ion_dma_buf_attachment *a;

	pr_debug("%s: len %zu heap_id_mask %u flags %x\n", __func__,
		 len, heap_id_mask, flags);
	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);

	if (!len)
		return -EINVAL;

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if ((heap_id_mask & ~0U) == ~0U) {
			if (heap->id != 0)
				continue;
		} else {
			if (!((1 << heap->id) & heap_id_mask))
				continue;
		}
		tracing_mark_begin("%s(%s, %zu, 0x%x, 0x%x)", "ion_alloc",
				   heap->name, len, heap_id_mask, flags);
		buffer = ion_buffer_create(heap, dev, len, flags);
		tracing_mark_end();
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer)
		return -ENODEV;

	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		_ion_buffer_destroy(buffer);
		return PTR_ERR(dmabuf);
	}

	attachment = kzalloc(sizeof(*attachment), GFP_KERNEL);
	if (!attachment) {
		_ion_buffer_destroy(buffer);
		return -ENOMEM;
	}

	buffer->priv_virt = (void *)attachment;
	ion_dma_buf_attach(dmabuf, buffer->dev->dev.this_device, attachment);

	if (!(flags & ION_FLAG_CACHED)) {
		list_for_each_entry(a, &buffer->attachments, list) {
			dma_sync_sg_for_device(a->dev, a->table->sgl,
					    a->table->nents,
					    DMA_BIDIRECTIONAL);
		}
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}

int ion_phys(int fd, unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (!IS_ERR_OR_NULL(dmabuf)) {
		buffer = (struct ion_buffer *)(dmabuf->priv);

		*phys_addr = sg_phys(buffer->sg_table->sgl);
		*size = buffer->size;
	} else {
		return -EPERM;
	}
	dma_buf_put(dmabuf);

	return ret;
}

struct dma_buf *ion_new_alloc(size_t len, unsigned int heap_id_mask,
			      unsigned int flags)
{
	struct ion_device *dev = internal_dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct ion_dma_buf_attachment *a;

	pr_debug("%s: len %zu heap_id_mask %u flags %x\n", __func__,
		 len, heap_id_mask, flags);
	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);

	if (!len)
		return ERR_PTR(-EINVAL);

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, flags);
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer)
		return ERR_PTR(-ENODEV);

	if (IS_ERR(buffer))
		return (struct dma_buf *)buffer;

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		_ion_buffer_destroy(buffer);
		return dmabuf;
	}

	attachment = kzalloc(sizeof(*attachment), GFP_KERNEL);
	if (!attachment) {
		_ion_buffer_destroy(buffer);
		return (struct dma_buf *)-ENOMEM;
	}
	buffer->priv_virt = (void *)attachment;
	ion_dma_buf_attach(dmabuf, buffer->dev->dev.this_device, attachment);

	if (!(flags & ION_FLAG_CACHED)) {
		list_for_each_entry(a, &buffer->attachments, list) {
			dma_sync_sg_for_device(a->dev, a->table->sgl,
				a->table->nents,
				DMA_BIDIRECTIONAL);
		}
	}


	return dmabuf;
}
EXPORT_SYMBOL(ion_new_alloc);

void ion_free(struct dma_buf *dmabuf)
{
	dma_buf_put(dmabuf);
}
EXPORT_SYMBOL(ion_free);

int ion_query_heaps(struct ion_heap_query *query)
{
	struct ion_device *dev = internal_dev;
	struct ion_heap_data __user *buffer = u64_to_user_ptr(query->heaps);
	int ret = -EINVAL, cnt = 0, max_cnt;
	struct ion_heap *heap;
	struct ion_heap_data hdata;

	memset(&hdata, 0, sizeof(hdata));

	down_read(&dev->lock);
	if (!buffer) {
		query->cnt = dev->heap_cnt;
		ret = 0;
		goto out;
	}

	if (query->cnt <= 0)
		goto out;

	max_cnt = query->cnt;

	plist_for_each_entry(heap, &dev->heaps, node) {
		strncpy(hdata.name, heap->name, MAX_HEAP_NAME);
		hdata.name[sizeof(hdata.name) - 1] = '\0';
		hdata.type = heap->type;
		hdata.heap_id = heap->id;

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

#ifdef CONFIG_E_SHOW_MEM
int ion_debug_heap_show_printk(struct ion_heap *heap,
			       enum e_show_mem_type type,
			       void *data)
{
	int i;
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	size_t total_size = 0;
	unsigned long pool_used = 0;
	unsigned long *total_used = data;
	struct tm t;

	pr_info("Heap: %s\n", heap->name);
	pr_info("Detail:\n");
	pr_info("%-10s %-6s %-16s %-10s\n", "size", "pid", "name", "alloc_time");
	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
						     node);
		if (buffer->heap->id != heap->id)
			continue;
		time_to_tm(buffer->alloc_time.tv_sec, 0, &t);
		pr_info("%-10zu %-5d %-16s %ld.%d.%d-%d:%d:%d.%ld\n",
			   buffer->size, buffer->pid, buffer->task_name,
			   t.tm_year + 1900, t.tm_mon + 1,
			   t.tm_mday, t.tm_hour, t.tm_min,
			   t.tm_sec, buffer->alloc_time.tv_usec);
		for (i = 0; i < MAX_MAP_USER; i++) {
			if (buffer->mappers[i].valid) {
				time_to_tm(buffer->mappers[i].map_time.tv_sec, 0, &t);
				pr_info("       |---%-5d  %-5d  %-16s  %ld.%d.%d-%d:%d:%d.%ld\n",
					buffer->mappers[i].pid, buffer->mappers[i].fd, buffer->mappers[i].task_name,
					t.tm_year + 1900, t.tm_mon + 1,
					t.tm_mday, t.tm_hour, t.tm_min,
					t.tm_sec, buffer->mappers[i].map_time.tv_usec);
			}
		}

		total_size += buffer->size;
	}
	mutex_unlock(&dev->buffer_lock);
	pr_info("----------------------------------------------------\n");
	pr_info("%16s %16zu\n", "total ", total_size);
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		pr_info("%16s %16zu\n", "deferred free",	heap->free_list_size);
	pr_info("----------------------------------------------------\n");

	if (heap->debug_show) {
		/* just get pool used */
		heap->debug_show(heap, 0, (void *)(&pool_used));
		/* printk detail infos */
		if (E_SHOW_MEM_BASIC != type)
			heap->debug_show(heap, 0, 0);
	}

	pr_info("%16.s %lu\n", "total pooled", pool_used);
	pr_info("----------------------------------------------------------\n");
	pr_info("Total used: %lu kB\n", (unsigned long)(total_size +
				pool_used + heap->free_list_size) / 1024);
	pr_info("----------------------------------------------------------\n");
	pr_info("\n");

	if (heap->type == ION_HEAP_TYPE_SYSTEM ||
	    heap->type == ION_HEAP_TYPE_SYSTEM_CONTIG) {
		*total_used += (unsigned long)(total_size + pool_used +
					       heap->free_list_size);
	}

	return 0;
}
#endif

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
#ifdef CONFIG_DEBUG_FS
	struct dentry *debug_file;
#endif
	struct ion_device *dev = internal_dev;

	if (!heap->ops->allocate || !heap->ops->free)
		pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	spin_lock_init(&heap->free_lock);
	heap->free_list_size = 0;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_init_deferred_free(heap);

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink)
		ion_heap_init_shrinker(heap);

	heap->dev = dev;
	down_write(&dev->lock);
	/*
	 * use negative heap->id to reverse the priority -- when traversing
	 * the list later attempt higher id numbers first
	 */
	plist_node_init(&heap->node, -heap->id);
	plist_add(&heap->node, &dev->heaps);

#ifdef CONFIG_DEBUG_FS
	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debug_file = debugfs_create_file(
			debug_name, 0644, dev->debug_root, heap,
			&debug_shrink_fops);
		if (!debug_file) {
			char buf[256], *path;

			path = dentry_path(dev->debug_root, buf, 256);
			pr_err("Failed to create heap shrinker debugfs at %s/%s\n",
			       path, debug_name);
		}
	}
#endif

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
	struct ion_device *dev = internal_dev;
	struct ion_heap *heap;
	u64 total_pages = 0;

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node)
		if (heap->ops->get_pool_size)
			total_pages += heap->ops->get_pool_size(heap);
	up_read(&dev->lock);

	return sprintf(buf, "%llu\n", total_pages * (PAGE_SIZE / 1024));
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
	arch_setup_dma_ops(idev->dev.this_device, 0,
			   DMA_BIT_MASK(64), NULL, false);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		goto err_reg;
	}

	ret = ion_init_sysfs();
	if (ret) {
		pr_err("ion: failed to add sysfs attributes.\n");
		goto err_sysfs;
	}

#ifdef CONFIG_DEBUG_FS
	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (!idev->debug_root) {
		pr_err("ion: failed to create debugfs root directory.\n");
		goto debugfs_done;
	}

debugfs_done:
#endif

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
subsys_initcall(ion_device_create);
