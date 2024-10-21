// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * DMABUF System heap exporter
 *
 */

#define pr_fmt(fmt) "dma_heap: system "fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <trace/hooks/mm.h>

#include "mtk_page_pool.h"
#include "mtk-deferred-free-helper.h"
#include <uapi/linux/dma-buf.h>

#include <linux/iommu.h>
#include "mtk_heap_priv.h"
#include "mtk_heap.h"
#include "mtk_page_pool.h"
#include "mtk-smmu-v3.h"

atomic64_t dma_heap_normal_total = ATOMIC64_INIT(0);
EXPORT_SYMBOL(dma_heap_normal_total);

atomic64_t sys_heap_total_bytes = ATOMIC64_INIT(0);
atomic64_t sys_uncached_heap_total_bytes = ATOMIC64_INIT(0);
atomic64_t mtk_mm_heap_total_bytes = ATOMIC64_INIT(0);
atomic64_t mtk_mm_uncached_heap_total_bytes = ATOMIC64_INIT(0);

static dmaheap_slc_callback slc_callback;

dmabuf_rbtree_dump_cb dmabuf_rbtree_dump_by_domain;
EXPORT_SYMBOL_GPL(dmabuf_rbtree_dump_by_domain);

static bool smmu_v3_enable;

/* vendor hook functions */
static atomic64_t *get_total_bytes_ptr(struct dma_heap *heap)
{
	if (!strncmp(dma_heap_get_name(heap), "system", 6))
		return &sys_heap_total_bytes;
	else if (!strncmp(dma_heap_get_name(heap), "system-uncached", 15))
		return &sys_uncached_heap_total_bytes;
	else if (!strncmp(dma_heap_get_name(heap), "mtk_mm", 6))
		return &mtk_mm_heap_total_bytes;
	else if (!strncmp(dma_heap_get_name(heap), "mtk_mm-uncached", 15))
		return &mtk_mm_uncached_heap_total_bytes;
	return NULL;
}

static long get_total_bytes(void)
{
	return atomic64_read(&sys_heap_total_bytes) +
		atomic64_read(&mtk_mm_heap_total_bytes);
}

static long get_total_bytes_uncached(void)
{
	return atomic64_read(&sys_uncached_heap_total_bytes) +
		atomic64_read(&mtk_mm_uncached_heap_total_bytes);
}

static void mtk_system_heap_show_mem(void *data, unsigned int filter, nodemask_t *nodemask)
{
	pr_info("%s: %ld kB\n", "system", get_total_bytes() >> 10);
	pr_info("%s: %ld kB\n", "system-uncached", get_total_bytes_uncached() >> 10);
}

static void mtk_system_heap_meminfo(void *data, struct seq_file *m)
{
	show_val_meminfo(m, "system", get_total_bytes() >> 10);
	show_val_meminfo(m, "system-uncached", get_total_bytes_uncached() >> 10);
}

static struct iova_cache_data *get_iova_cache(struct system_heap_buffer *buffer, u64 tab_id)
{
	struct iova_cache_data *cache_data;

	list_for_each_entry(cache_data, &buffer->iova_caches, iova_caches) {
		if (cache_data->tab_id == tab_id)
			return cache_data;
	}
	return NULL;
}


/* function declare */
static int system_buf_priv_dump(const struct dma_buf *dmabuf,
				struct seq_file *s);

static struct sg_table *dup_sg_table(struct sg_table *table)
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

static struct sg_table *dup_sg_table_by_range(struct sg_table *table,
					unsigned int offset, unsigned int len)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;
	unsigned int sg_offset = 0, sg_len;
	unsigned int contig_size = 0, found_start = 0;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	new_table->nents = 0;

	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		if (!found_start) {
			sg_offset += sg->length;
			if (sg_offset <= offset)
				continue;
			found_start = 1;
			sg_len = len + sg->length - (sg_offset - offset);
		}

		if (contig_size >= sg_len)
			break;

		memcpy(new_sg, sg, sizeof(*sg));
		contig_size += sg->length;
		new_sg = sg_next(new_sg);
		new_table->nents++;
	}

	return new_table;
}

/* source copy to dest, no memory alloc */
static int copy_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sgl, *dest_sgl;

	if (source->orig_nents != dest->orig_nents) {
		pr_info("nents not match %d-%d\n",
			source->orig_nents, dest->orig_nents);

		return -EINVAL;
	}

	/* copy mapped nents */
	dest->nents = source->nents;

	dest_sgl = dest->sgl;
	for_each_sg(source->sgl, sgl, source->orig_nents, i) {
		if (!dest_sgl || !sgl) {
			pr_info("%s error: dest_sgl or sgl is null\n", __func__);
			break;
		}

		memcpy(dest_sgl, sgl, sizeof(*sgl));
		dest_sgl = sg_next(dest_sgl);
	}

	return 0;
};

/*
 * must check domain info before call fill_buffer_info
 * @Return 0: pass
 */
static int fill_buffer_info(struct system_heap_buffer *buffer,
			    struct sg_table *table,
			    struct dma_buf_attachment *a,
			    enum dma_data_direction dir,
			    u64 tab_id, unsigned int dom_id)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(a->dev);
	struct sg_table *new_table = NULL;
	struct iova_cache_data *cache_data;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (!smmu_v3_enable &&
	   (!fwspec || tab_id >= MTK_M4U_TAB_NR_MAX ||
	    dom_id >= MTK_M4U_DOM_NR_MAX))
		return 0;

	/* smmu only suuport one domain */
	if (smmu_v3_enable && !fwspec)
		return 0;

	cache_data = get_iova_cache(buffer, tab_id);
	if (cache_data != NULL && cache_data->mapped[dom_id]) {
		struct device *dev = cache_data->dev_info[dom_id].dev;

		pr_info("%s already mapped for dev:%s, tab_id:%llu, dom_id:%u",
			__func__, dev_name(dev), tab_id, dom_id);
		return -EINVAL;
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return -ENOMEM;

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_info("%s err: sg_alloc_table failed\n", __func__);
		kfree(new_table);
		return -ENOMEM;
	}

	ret = copy_sg_table(table, new_table);
	if (ret) {
		kfree(new_table);
		return ret;
	}

	if (!cache_data) {
		cache_data = kzalloc(sizeof(*cache_data), GFP_KERNEL);
		if (!cache_data) {
			kfree(new_table);
			return -ENOMEM;
		}
		cache_data->tab_id = tab_id;
		list_add(&cache_data->iova_caches, &buffer->iova_caches);
	}

	cache_data->mapped_table[dom_id] = new_table;
	cache_data->mapped[dom_id] = true;
	cache_data->dev_info[dom_id].dev = a->dev;
	cache_data->dev_info[dom_id].direction = dir;
	/* TODO: check map_attrs affect??? */
	cache_data->dev_info[dom_id].map_attrs = a->dma_map_attrs;

	return 0;
}

int system_heap_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	dmabuf_name_check(dmabuf, attachment->dev);

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

void system_heap_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

struct sg_table *mtk_mm_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = attachment->dma_map_attrs;
	struct iova_cache_data *cache_data;
	int ret;

	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int dom_id = 0;
	u64 tab_id = 0;
	struct system_heap_buffer *buffer = attachment->dmabuf->priv;
	int larb_id, port_id;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->map_lock);

	if (fwspec && !smmu_v3_enable) {
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		cache_data = get_iova_cache(buffer, tab_id);
		larb_id = MTK_M4U_TO_LARB(fwspec->ids[0]);
		port_id = MTK_M4U_TO_PORT(fwspec->ids[0]);
	} else if (fwspec && smmu_v3_enable) {
		tab_id = get_smmu_tab_id(attachment->dev);
		cache_data = get_iova_cache(buffer, tab_id);
		dom_id = 0;
		larb_id = 0;
		port_id = 0;
	}

	/* device with iommus attribute AND mapped before */
	if (fwspec && cache_data != NULL && cache_data->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sg_table(cache_data->mapped_table[dom_id], table);

		mutex_unlock(&buffer->map_lock);
		if (ret)
			return ERR_PTR(-EINVAL);

		a->mapped = true;

		if (!(attr & DMA_ATTR_SKIP_CPU_SYNC))
			dma_sync_sgtable_for_device(attachment->dev, table, direction);

		return table;
	}

	/* first map OR device without iommus attribute */
	if (dma_map_sgtable(attachment->dev, table, direction, attr)) {
		pr_info("%s map fail tab:%llu, dom:%d, dev:%s\n",
			__func__, tab_id, dom_id, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);

		if (dmabuf_rbtree_dump_by_domain)
			dmabuf_rbtree_dump_by_domain(tab_id, dom_id);

		return ERR_PTR(-ENOMEM);
	}

	ret = fill_buffer_info(buffer, table,
			       attachment, direction, tab_id, dom_id);
	if (ret) {
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(ret);
	}
	mutex_unlock(&buffer->map_lock);
	a->mapped = true;

	return table;
}

static struct sg_table *system_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = attachment->dma_map_attrs;
	int ret;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

void system_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	struct dma_buf *buf = attachment->dmabuf;

	if (a->uncached)
		attr |= DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;

	/*
	 * mtk_mm heap: for devices with iommus attribute,
	 * unmap iova when release dma-buf.
	 * system heap: unmap it every time
	 */
	if ((is_mtk_mm_heap_dmabuf(buf) || is_rbin_heap_dmabuf(buf)) && fwspec) {
		if (!(attr & DMA_ATTR_SKIP_CPU_SYNC))
			dma_sync_sgtable_for_cpu(attachment->dev, table, direction);
		return;
	}

	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

int system_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

int system_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, a->table, direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int mtk_slc_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	//pr_info("%s slc_callback 0x%lx\n", __func__, (unsigned long)slc_callback);
	if (slc_callback)
		slc_callback(dmabuf);

	return 0;
}

static int system_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

int mtk_mm_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	unsigned int i;
	int ret;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page;
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len;

		if (!sg) {
			pr_info("%s error: sg is null\n", __func__);
			break;
		}
		page = sg_page(sg);
		len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static void *system_heap_do_vmap(struct system_heap_buffer *buffer)
{
	struct sg_table *table = &buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	pgprot_t pgprot = PAGE_KERNEL;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

int system_heap_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;
	int ret = 0;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		iosys_map_set_vaddr(map, buffer->vaddr);
		goto out;
	}

	vaddr = system_heap_do_vmap(buffer);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		goto out;
	}

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
	iosys_map_set_vaddr(map, buffer->vaddr);
out:
	mutex_unlock(&buffer->lock);

	return ret;
}

void system_heap_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct system_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	iosys_map_clear(map);
}

static int system_heap_zero_buffer(struct system_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_local_page(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_local(vaddr);
	}

	return ret;
}

static void system_heap_buf_free(struct mtk_deferred_freelist_item *item,
				 enum mtk_df_reason reason)
{
	struct mtk_dmabuf_page_pool **page_pools;
	struct mtk_heap_priv_info *heap_priv;
	struct system_heap_buffer *buffer;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;

	buffer = container_of(item, struct system_heap_buffer, deferred_free);
	heap_priv = dma_heap_get_drvdata(buffer->heap);
	page_pools = heap_priv ? heap_priv->page_pools : NULL;

	/* Zero the buffer pages before adding back to the pool */
	if (reason == MTK_DF_NORMAL)
		if (system_heap_zero_buffer(buffer))
			reason = MTK_DF_UNDER_PRESSURE; // On failure, just free

	table = &buffer->sg_table;
	for_each_sgtable_sg(table, sg, i) {
		struct page *page;

		if (!sg) {
			pr_info("%s error: sg is null\n", __func__);
			break;
		}
		page = sg_page(sg);

		if (reason == MTK_DF_UNDER_PRESSURE || !page_pools) {
			__free_pages(page, compound_order(page));
		} else {
			for (j = 0; j < NUM_ORDERS; j++) {
				if (compound_order(page) == orders[j])
					break;
			}

			if (j < NUM_ORDERS)
				mtk_dmabuf_page_pool_free(page_pools[j], page);
			else
				pr_info("%s error order:%u\n",
					__func__, compound_order(page));
		}
	}

	if (page_pools) {
		struct mtk_dmabuf_page_pool *pool = page_pools[0];

		dmabuf_log_pool_size(buffer->heap);
		if (pool->refill_kthread && !pool->recycling &&
		    pool->need_recycle && pool->need_recycle(pool))
			wake_up_process(pool->refill_kthread);
	}

	sg_free_table(table);
	kfree(buffer);
}

void mtk_mm_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	unsigned long buf_len = buffer->len;
	struct iova_cache_data *cache_data, *temp_data;
	struct mtk_heap_dev_info dev_info;
	struct sg_table *table;
	unsigned long attrs;
	int k;
	atomic64_t *total_bytes_ptr = get_total_bytes_ptr(buffer->heap);

	spin_lock(&dmabuf->name_lock);
	pr_debug("%s: inode:%lu, size:%lu, name:%s\n", __func__,
		 file_inode(dmabuf->file)->i_ino, buffer->len,
		 dmabuf->name?:"NULL");
	spin_unlock(&dmabuf->name_lock);

	dmabuf_release_check(dmabuf);

	/* unmap all domains' iova */
	list_for_each_entry_safe(cache_data, temp_data, &buffer->iova_caches, iova_caches) {
		for (k = 0; k < MTK_M4U_DOM_NR_MAX; k++) {
			table = cache_data->mapped_table[k];
			dev_info = cache_data->dev_info[k];
			attrs = dev_info.map_attrs;

			if (buffer->uncached)
				attrs |= DMA_ATTR_SKIP_CPU_SYNC;

			if (!cache_data->mapped[k])
				continue;

			dma_unmap_sgtable(dev_info.dev, table, dev_info.direction, attrs);
			cache_data->mapped[k] = false;
			sg_free_table(table);
			kfree(table);
		}
		list_del(&cache_data->iova_caches);
		kfree(cache_data);
	}

	/* free buffer memory */
	if (is_rbin_heap_dmabuf(dmabuf))
		rbin_heap_release(dmabuf);
	else
		mtk_deferred_free(&buffer->deferred_free, system_heap_buf_free, npages);

	if (atomic64_sub_return(buf_len, &dma_heap_normal_total) < 0) {
		pr_info("warn: %s, total memory underflow, 0x%llx!!, reset as 0\n",
			__func__, atomic64_read(&dma_heap_normal_total));
		atomic64_set(&dma_heap_normal_total, 0);
	}

	if (total_bytes_ptr && atomic64_sub_return(buf_len, total_bytes_ptr) < 0) {
		pr_info("warn: %s, total memory underflow, 0x%llx!!, reset as 0\n",
			__func__, atomic64_read(total_bytes_ptr));
		atomic64_set(total_bytes_ptr, 0);
	}
}

static void system_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct mtk_dmabuf_page_pool **page_pools;
	struct mtk_heap_priv_info *heap_priv;
	unsigned long buf_len = buffer->len;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;
	atomic64_t *total_bytes_ptr = get_total_bytes_ptr(buffer->heap);

	dmabuf_release_check(dmabuf);

	heap_priv = dma_heap_get_drvdata(buffer->heap);
	page_pools = heap_priv ? heap_priv->page_pools : NULL;

	/* Zero the buffer pages before adding back to the pool */
	system_heap_zero_buffer(buffer);

	table = &buffer->sg_table;
	for_each_sgtable_sg(table, sg, i) {
		struct page *page;

		if (!sg) {
			pr_info("%s error: sg is null\n", __func__);
			break;
		}
		page = sg_page(sg);

		for (j = 0; j < NUM_ORDERS; j++) {
			if (compound_order(page) == orders[j])
				break;
		}
		if (j < NUM_ORDERS) {
			if (page_pools)
				mtk_dmabuf_page_pool_free(page_pools[j], page);
			else
				__free_pages(page, compound_order(page));
		} else {
			pr_info("%s error: order %u\n", __func__, compound_order(page));
		}
	}
	sg_free_table(table);
	kfree(buffer);

	if (atomic64_sub_return(buf_len, &dma_heap_normal_total) < 0) {
		pr_info("warn: %s, total memory underflow, 0x%llx!!, reset as 0\n",
			__func__, atomic64_read(&dma_heap_normal_total));
		atomic64_set(&dma_heap_normal_total, 0);
	}

	if (total_bytes_ptr && atomic64_sub_return(buf_len, total_bytes_ptr) < 0) {
		pr_info("warn: %s, total memory underflow, 0x%llx!!, reset as 0\n",
			__func__, atomic64_read(total_bytes_ptr));
		atomic64_set(total_bytes_ptr, 0);
	}
}

int system_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	struct system_heap_buffer *buffer = dmabuf->priv;

	*flags = buffer->uncached;

	return 0;
}

static int dmabuf_check_user_args(unsigned int offset, unsigned int len,
					unsigned long buf_len)
{
	if (!len || len > buf_len || offset >= buf_len ||
	    offset + len > buf_len)
		return -EINVAL;

	return 0;
}

int mtk_mm_heap_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
					enum dma_data_direction direction,
					unsigned int offset, unsigned int len)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *sgt_tmp;
	int ret;

	mutex_lock(&buffer->lock);

	ret = dmabuf_check_user_args(offset, len, buffer->len);
	if (ret) {
		pr_err("%s: invalid args, off:0x%x, len:0x%x, buf_len:0x%lx\n",
		       __func__, offset, len, buffer->len);
		mutex_unlock(&buffer->lock);
		return ret;
	}

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr + offset, len);


	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;

			sgt_tmp = dup_sg_table_by_range(a->table, offset, len);
			if (IS_ERR(sgt_tmp)) {
				pr_err("%s: dup sg_table failed!\n", __func__);
				mutex_unlock(&buffer->lock);
				return -ENOMEM;
			}

			dma_sync_sg_for_cpu(a->dev, sgt_tmp->sgl,
					    sgt_tmp->nents, direction);

			sg_free_table(sgt_tmp);
			kfree(sgt_tmp);
		}
	}

	mutex_unlock(&buffer->lock);

	return 0;
}

int mtk_mm_heap_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					enum dma_data_direction direction,
					unsigned int offset, unsigned int len)
{
	struct system_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *sgt_tmp;
	int ret;

	mutex_lock(&buffer->lock);

	ret = dmabuf_check_user_args(offset, len, buffer->len);
	if (ret) {
		pr_err("%s: invalid args, off:0x%x, len:0x%x, buf_len:0x%lx\n",
		       __func__, offset, len, buffer->len);
		mutex_unlock(&buffer->lock);
		return ret;
	}

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr + offset, len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;

			sgt_tmp = dup_sg_table_by_range(a->table, offset, len);
			if (IS_ERR(sgt_tmp)) {
				pr_err("%s: dup sg_table failed!\n", __func__);
				mutex_unlock(&buffer->lock);
				return -ENOMEM;
			}

			dma_sync_sg_for_device(a->dev, sgt_tmp->sgl,
					       sgt_tmp->nents, direction);

			sg_free_table(sgt_tmp);
			kfree(sgt_tmp);
		}
	}

	mutex_unlock(&buffer->lock);

	return 0;
}

static const struct dma_buf_ops mtk_mm_heap_buf_ops = {
	/* 1 attachment can only map 1 iova */
	.cache_sgt_mapping = 1,
	.attach = system_heap_attach,
	.detach = system_heap_detach,
	.map_dma_buf = mtk_mm_heap_map_dma_buf,
	.unmap_dma_buf = system_heap_unmap_dma_buf,
	.begin_cpu_access = system_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = system_heap_dma_buf_end_cpu_access,
	.begin_cpu_access_partial =
			mtk_mm_heap_dma_buf_begin_cpu_access_partial,
	.end_cpu_access_partial = mtk_mm_heap_dma_buf_end_cpu_access_partial,
	.mmap = mtk_mm_heap_mmap,
	.vmap = system_heap_vmap,
	.vunmap = system_heap_vunmap,
	.release = mtk_mm_heap_dma_buf_release,
	.get_flags = system_heap_dma_buf_get_flags,
};

static const struct dma_buf_ops system_heap_buf_ops = {
	/* 1 attachment can only map 1 iova */
	.cache_sgt_mapping = 1,
	.attach = system_heap_attach,
	.detach = system_heap_detach,
	.map_dma_buf = system_heap_map_dma_buf,
	.unmap_dma_buf = system_heap_unmap_dma_buf,
	.begin_cpu_access = system_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = system_heap_dma_buf_end_cpu_access,
	.mmap = system_heap_mmap,
	.vmap = system_heap_vmap,
	.vunmap = system_heap_vunmap,
	.release = system_heap_dma_buf_release,
	.get_flags = system_heap_dma_buf_get_flags,
};

static const struct dma_buf_ops mtk_slc_heap_buf_ops = {
	/* 1 attachment can only map 1 iova */
	.cache_sgt_mapping = 1,
	.attach = system_heap_attach,
	.detach = system_heap_detach,
	.map_dma_buf = system_heap_map_dma_buf,
	.unmap_dma_buf = system_heap_unmap_dma_buf,
	.begin_cpu_access = system_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = mtk_slc_dma_buf_end_cpu_access,
	.mmap = system_heap_mmap,
	.vmap = system_heap_vmap,
	.vunmap = system_heap_vunmap,
	.release = system_heap_dma_buf_release,
	.get_flags = system_heap_dma_buf_get_flags,
};

static struct page *alloc_largest_available(unsigned long size,
					    unsigned int max_order,
					    struct mtk_dmabuf_page_pool **page_pools)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		if (likely(page_pools)) {
			page = mtk_dmabuf_page_pool_alloc(page_pools[i]);
			if (!page)
				continue;

			if (page_pools && page_pools[i]->refill_kthread &&
				!page_pools[i]->refilling && page_pools[i]->need_refill &&
				page_pools[i]->need_refill(page_pools[i]))
				wake_up_process(page_pools[i]->refill_kthread);
		} else {
			page = alloc_pages(order_flags[i], orders[i]);
		}

		return page;
	}
	return NULL;
}

static struct dma_buf *system_heap_do_allocate(struct dma_heap *heap,
					       unsigned long len,
					       unsigned long fd_flags,
					       unsigned long heap_flags,
					       bool uncached,
					       const struct dma_buf_ops *ops)
{
	struct system_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size_remaining = len;
	unsigned int max_order = orders[0];
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i, ret = -ENOMEM;
	struct task_struct *task = current->group_leader;
	struct mtk_dmabuf_page_pool **page_pools;
	struct mtk_heap_priv_info *heap_priv = dma_heap_get_drvdata(heap);
	u64 tm1, tm2;
	atomic64_t *total_bytes_ptr = get_total_bytes_ptr(heap);

	page_pools = heap_priv ? heap_priv->page_pools : NULL;
	if (len / PAGE_SIZE > totalram_pages()) {
		pr_info("%s error: len %ld is more than %ld\n",
			__func__, len, totalram_pages() * PAGE_SIZE);
		return ERR_PTR(-ENOMEM);
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	INIT_LIST_HEAD(&buffer->iova_caches);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = uncached;

	INIT_LIST_HEAD(&pages);
	i = 0;
	tm1 = sched_clock();
	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			pr_info("%s, %d, %s, current process fatal_signal_pending\n",
				__func__, __LINE__, dma_heap_get_name(heap));
			goto free_buffer;
		}

		page = alloc_largest_available(size_remaining, max_order, page_pools);
		if (!page) {
			if (fatal_signal_pending(current)) {
				ret = -EINTR;
				pr_info("%s, %d, %s, current process fatal_signal_pending\n",
					__func__, __LINE__, dma_heap_get_name(heap));
			}
			goto free_buffer;
		}

		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}
	tm2 = sched_clock();
	dmabuf_log_allocate(heap, tm2 - tm1, i, len>>PAGE_SHIFT, 0);
	if (page_pools)
		dmabuf_log_pool_size(heap);

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_buffer;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	mutex_init(&buffer->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(buffer->pid_name, task);
	get_task_comm(buffer->tid_name, current);
	buffer->pid = task_pid_nr(task);
	buffer->tid = task_pid_nr(current);
	buffer->ts  = sched_clock() / 1000;
	buffer->show = system_buf_priv_dump;
	buffer->gid = -1;

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	if (buffer->uncached) {
		dma_map_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
		dma_unmap_sgtable(dma_heap_get_dev(heap), table, DMA_BIDIRECTIONAL, 0);
	}

	atomic64_add(dmabuf->size, &dma_heap_normal_total);
	if (total_bytes_ptr)
		atomic64_add(dmabuf->size, total_bytes_ptr);

	return dmabuf;

free_pages:
	for_each_sgtable_sg(table, sg, i) {
		struct page *p = sg_page(sg);

		__free_pages(p, compound_order(p));
	}
	sg_free_table(table);
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		__free_pages(page, compound_order(page));
	kfree(buffer);

	return ERR_PTR(ret);
}

static struct dma_buf *system_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	struct mtk_heap_priv_info *heap_priv = dma_heap_get_drvdata(heap);

	return system_heap_do_allocate(heap, len, fd_flags, heap_flags,
				       heap_priv ? heap_priv->uncached : false,
				       &system_heap_buf_ops);
}

static struct dma_buf *mtk_mm_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	struct mtk_heap_priv_info *heap_priv = dma_heap_get_drvdata(heap);

	return system_heap_do_allocate(heap, len, fd_flags, heap_flags,
				       heap_priv ? heap_priv->uncached : false,
				       &mtk_mm_heap_buf_ops);
}

static struct dma_buf *mtk_slc_heap_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	return system_heap_do_allocate(heap, len, fd_flags, heap_flags, false,
				       &mtk_slc_heap_buf_ops);
}

static long mtk_get_pool_size(struct dma_heap *heap)
{
	struct mtk_heap_priv_info *heap_priv;

	heap_priv = heap ? dma_heap_get_drvdata(heap) : NULL;
	if (heap_priv && !heap_priv->uncached)
		return mtk_dmabuf_page_pool_size(heap);

	return 0;
}

static const struct dma_heap_ops system_heap_ops = {
	.allocate = system_heap_allocate,
	.get_pool_size = mtk_get_pool_size,
};

static const struct dma_heap_ops mtk_mm_heap_ops = {
	.allocate = mtk_mm_heap_allocate,
	.get_pool_size = mtk_get_pool_size,
};

static const struct dma_heap_ops mtk_slc_heap_ops = {
	.allocate = mtk_slc_heap_allocate,
};

static int system_buf_priv_dump(const struct dma_buf *dmabuf,
				struct seq_file *s)
{
	int k = 0;
	struct system_heap_buffer *buf = dmabuf->priv;
	struct list_head *cache_node;
	struct iova_cache_data *cache_data;

	dmabuf_dump(s, "\tbuf_priv: uncached:%d alloc_pid:%d(%s)tid:%d(%s) alloc_time:%lluus\n",
		    !!buf->uncached,
		    buf->pid, buf->pid_name,
		    buf->tid, buf->tid_name,
		    buf->ts);

	if (is_system_heap_dmabuf(dmabuf))
		return 0;

	mutex_lock(&buf->map_lock);
	list_for_each(cache_node, &buf->iova_caches) {
		cache_data = list_entry(cache_node, struct iova_cache_data, iova_caches);
		for (k = 0; k < MTK_M4U_DOM_NR_MAX; k++) {
			bool mapped = cache_data->mapped[k];
			struct device *dev = cache_data->dev_info[k].dev;
			struct sg_table *sgt = cache_data->mapped_table[k];

			if (!sgt || !dev || !dev_iommu_fwspec_get(dev))
				continue;

			dmabuf_dump(s,
				    "\tbuf_priv: tab:%-2llu dom:%-2d map:%d iova:0x%-12lx attr:0x%-4lx dir:%-2d dev:%s\n",
				    cache_data->tab_id, k, mapped,
				    (unsigned long)sg_dma_address(sgt->sgl),
				    cache_data->dev_info[k].map_attrs,
				    cache_data->dev_info[k].direction,
				    dev_name(dev));
		}
	}
	mutex_unlock(&buf->map_lock);

	return 0;
}

/**
 * return none-zero value means dump fail.
 *       maybe the input dmabuf isn't this heap buffer, no need dump
 *
 * return 0 means dump pass
 */
static int system_heap_buf_priv_dump(const struct dma_buf *dmabuf,
				     struct dma_heap *heap,
				     void *priv)
{
	struct seq_file *s = priv;
	struct system_heap_buffer *buf = dmabuf->priv;

	if (!is_system_heap_dmabuf(dmabuf) && !is_mtk_mm_heap_dmabuf(dmabuf))
		return -EINVAL;

	if (heap != buf->heap)
		return -EINVAL;

	if (buf->show)
		return buf->show(dmabuf, s);

	return -EINVAL;
}


static int set_heap_dev_dma(struct device *heap_dev)
{
	int err = 0;

	if (!heap_dev)
		return -EINVAL;

	dma_coerce_mask_and_coherent(heap_dev, DMA_BIT_MASK(64));

	if (!heap_dev->dma_parms) {
		heap_dev->dma_parms = devm_kzalloc(heap_dev,
						   sizeof(*heap_dev->dma_parms),
						   GFP_KERNEL);
		if (!heap_dev->dma_parms)
			return -ENOMEM;

		err = dma_set_max_seg_size(heap_dev, (unsigned int)DMA_BIT_MASK(64));
		if (err) {
			devm_kfree(heap_dev, heap_dev->dma_parms);
			dev_err(heap_dev, "Failed to set DMA segment size, err:%d\n", err);
			return err;
		}
	}

	return 0;
}

static int mtk_heap_create(const char *name, const struct dma_heap_ops *ops,
			   bool uncached, bool pool)
{
	struct dma_heap_export_info exp_info;
	struct mtk_heap_priv_info *heap_priv;
	struct mtk_heap_priv_info *heap_priv_uncache;
	struct dma_heap *heap;
	char *name_uncache;
	int ret;

	heap_priv = kzalloc(sizeof(*heap_priv), GFP_KERNEL);
	if (!heap_priv) {
		ret = -ENOMEM;
		goto out;
	}

	exp_info.priv = heap_priv;
	exp_info.name = name;
	exp_info.ops = ops;

	if (pool) {
		heap_priv->page_pools = dynamic_page_pools_create(orders, NUM_ORDERS);
		if (IS_ERR(heap_priv->page_pools)) {
			ret = -ENOMEM;
			goto out_free_heap;
		}
	}
	heap_priv->uncached = false;
	heap_priv->buf_priv_dump = system_heap_buf_priv_dump;

	heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap)) {
		ret = -ENOMEM;
		goto out_free_pools;
	}
	ret = set_heap_dev_dma(dma_heap_get_dev(heap));
	if (ret)
		goto out_free_pools;

	pr_info("%s add heap[%s] success\n", __func__, exp_info.name);
	/* Add uncached heap if need */
	if (uncached) {
		name_uncache = kasprintf(GFP_KERNEL, "%s-uncached", name);
		if (!name_uncache) {
			ret = -ENOMEM;
			goto out_free_pools;
		}
		heap_priv_uncache = kzalloc(sizeof(*heap_priv), GFP_KERNEL);
		if (!heap_priv_uncache) {
			ret = -ENOMEM;
			goto out_free_heap_1;
		}

		exp_info.priv = heap_priv_uncache;
		exp_info.name = name_uncache;
		exp_info.ops = ops;

		heap_priv_uncache->page_pools = heap_priv->page_pools;
		heap_priv_uncache->uncached = true;
		heap_priv_uncache->buf_priv_dump = system_heap_buf_priv_dump;

		heap = dma_heap_add(&exp_info);
		if (IS_ERR(heap)) {
			ret = -ENOMEM;
			goto out_free_heap_2;
		}

		ret = set_heap_dev_dma(dma_heap_get_dev(heap));
		if (ret)
			goto out_free_heap_2;

		pr_info("%s add heap[%s] success\n", __func__, exp_info.name);
	}

	register_trace_android_vh_show_mem(mtk_system_heap_show_mem, NULL);
	register_trace_android_vh_meminfo_proc_show(mtk_system_heap_meminfo, NULL);

	return 0;

out_free_heap_2:
	kfree(heap_priv_uncache);
out_free_heap_1:
	kfree(name_uncache);
out_free_pools:
	dynamic_page_pools_free(heap_priv->page_pools, NUM_ORDERS);
out_free_heap:
	kfree(heap_priv);
out:
	pr_info("%s: failed to create heap %s with error %d\n",
		__func__, name, ret);
	return ret;
}

int mtk_dma_heap_config_parse(struct device *dev, struct mtk_dma_heap_config *heap_config)
{
	const struct mtk_dma_heap_match_data *dmaheap_data = of_device_get_match_data(dev);
	const struct device_node *node = dev->of_node;
	const char *heap_name;
	u32 trusted_mem_type;
	u32 max_align;
	int ret;

	if (!heap_config)
		return -EINVAL;

	ret = of_property_read_string(node, "heap-name", &heap_name);
	if (ret) {
		pr_info("%s has no heap-name node:%s\n", __func__, node->name);
		return -EINVAL;
	}

	heap_config->heap_type = dmaheap_data->dmaheap_type;
	heap_config->heap_name = heap_name;
	heap_config->heap_uncached = of_property_read_bool(node, "heap-uncached");

	ret = of_property_read_string(node, "region-heap-align-name", &heap_name);
	if (!ret)
		heap_config->region_heap_align_name = heap_name;

	ret = of_property_read_u32(node, "trusted-mem-type", &trusted_mem_type);
	if (!ret)
		heap_config->trusted_mem_type = trusted_mem_type;

	ret = of_property_read_u32(node, "heap-max-align", &max_align);
	if (!ret)
		heap_config->max_align = max_align;

	heap_config->dev = dev;
	pr_info("%s node:%s,type:%d,name:%s,uncached:%d,trusted:%d,dev:%s\n",
		__func__, node->name, heap_config->heap_type,
		heap_config->heap_name?:"NULL", heap_config->heap_uncached?1:0,
		heap_config->trusted_mem_type,
		heap_config->dev?dev_name(heap_config->dev):"NULL");

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dma_heap_config_parse);

static int mtk_dma_heap_config_probe(struct platform_device *pdev)
{
	struct mtk_dma_heap_config heap_config = {0};
	int ret;

	ret = mtk_dma_heap_config_parse(&pdev->dev, &heap_config);
	if (ret)
		return ret;

	if (heap_config.heap_type == DMA_HEAP_MTK_SLC)
		mtk_heap_create(heap_config.heap_name, &mtk_slc_heap_ops, false, false);

	return 0;
}

static const struct mtk_dma_heap_match_data dmaheap_data_mtk_slc = {
	.dmaheap_type = DMA_HEAP_MTK_SLC,
};

static const struct of_device_id mtk_dma_heap_match_table[] = {
	{.compatible = "mediatek,dmaheap-mtk-slc", .data = &dmaheap_data_mtk_slc},
	{},
};

static struct platform_driver mtk_dma_heap_config_driver = {
	.probe = mtk_dma_heap_config_probe,
	.driver = {
		.name = "mtk-dma-heap-slc",
		.of_match_table = mtk_dma_heap_match_table,
	},
};

static int mtk_system_heap_create(void)
{
	int ret;

	// smmu support 4K/2M/1G granule
	smmu_v3_enable = smmu_v3_enabled();
	if (smmu_v3_enable)
		orders[0] = 9;

	mtk_dmabuf_page_pool_init_shrinker();
	mtk_deferred_freelist_init();

	mtk_heap_create("system", &system_heap_ops, true, true);
	mtk_heap_create("mtk_mm", &mtk_mm_heap_ops, true, true);

	ret = platform_driver_register(&mtk_dma_heap_config_driver);
	if (ret < 0) {
		pr_info("%s fail to register mtk_slc heap: %d\n", __func__, ret);
		return ret;
	}

	ret = rbin_dma_heap_init();
	if (ret < 0) {
		pr_info("%s fail to register rbin heap: %d", __func__, ret);
	} else {
		pr_info("%s success to register rbin heap!",__func__);
	}

	mtk_cache_pool_init();
	return 0;
}

static void mtk_system_heap_exit(void)
{
	platform_driver_unregister(&mtk_dma_heap_config_driver);
	rbin_dma_heap_exit();
}

const char *refill_heap_names[] = {
	"mtk_mm",
	NULL,
};

const char **mtk_refill_heap_names(void)
{
	return &refill_heap_names[0];
}
EXPORT_SYMBOL_GPL(mtk_refill_heap_names);

/* ref code: dma_buf.c, dma_buf_set_name */
long mtk_dma_buf_set_name(struct dma_buf *dmabuf, const char *buf)
{
	char *name = kstrndup(buf, DMA_BUF_NAME_LEN, GFP_KERNEL);
	long ret = 0;

	if (IS_ERR(name))
		return PTR_ERR(name);

	/* different with dma_buf.c, always enable setting name */
	spin_lock(&dmabuf->name_lock);
	kfree(dmabuf->name);
	dmabuf->name = name;
	spin_unlock(&dmabuf->name_lock);

	return ret;
} EXPORT_SYMBOL_GPL(mtk_dma_buf_set_name);

int is_mtk_mm_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->ops == &mtk_mm_heap_buf_ops)
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(is_mtk_mm_heap_dmabuf);

int is_system_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->ops == &system_heap_buf_ops)
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(is_system_heap_dmabuf);

hang_dump_cb hang_dump_proc;
EXPORT_SYMBOL_GPL(hang_dump_proc);

int mtk_dmaheap_register_slc_callback(dmaheap_slc_callback cb)
{
	//pr_info("%s slc_callback:0x%lx cb:0x%lx\n", __func__,
	//	(unsigned long)slc_callback, (unsigned long)cb);
	if (slc_callback)
		return -EPERM;

	slc_callback = cb;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dmaheap_register_slc_callback);

int mtk_dmaheap_unregister_slc_callback(void)
{
	slc_callback = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dmaheap_unregister_slc_callback);

int dma_buf_set_gid(struct dma_buf *dmabuf, int gid)
{
	struct system_heap_buffer *buffer = dmabuf->priv;

	if (IS_ERR_OR_NULL(buffer))
		return -EINVAL;

	buffer->gid = gid;
	return 0;
}
EXPORT_SYMBOL_GPL(dma_buf_set_gid);

int dma_buf_get_gid(struct dma_buf *dmabuf)
{
	struct system_heap_buffer *buffer = dmabuf->priv;

	if (IS_ERR_OR_NULL(buffer))
		return -EINVAL;

	return buffer->gid;
}
EXPORT_SYMBOL_GPL(dma_buf_get_gid);

module_init(mtk_system_heap_create);
module_exit(mtk_system_heap_exit);
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
