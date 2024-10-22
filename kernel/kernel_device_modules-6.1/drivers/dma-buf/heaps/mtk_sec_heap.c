// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_sec heap exporter
 *
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt) "dma_heap: mtk_sec " fmt

#include <linux/arm-smccc.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/vmalloc.h>

#include <public/trusted_mem_api.h>
#include "page_pool.h"
#include "mtk_heap_priv.h"
#include "mtk_heap.h"
#include "mtk_sec_heap.h"
#include "mtk_iommu.h"
#include "mtk-smmu-v3.h"

#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
#define MID_ORDER_GFP (LOW_ORDER_GFP | __GFP_NOWARN)
#define HIGH_ORDER_GFP                                                         \
	(((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN | __GFP_NORETRY) &         \
	  ~__GFP_RECLAIM) |                                                    \
	 __GFP_COMP)

// static gfp_t order_flags[] = { HIGH_ORDER_GFP, MID_ORDER_GFP, LOW_ORDER_GFP };
static gfp_t order_flags[] = { HIGH_ORDER_GFP, LOW_ORDER_GFP, LOW_ORDER_GFP };

// int orders[3] = { 9, 4, 0 };
int orders[3] = { 9, 4, 3 };
#define NUM_ORDERS ARRAY_SIZE(orders)
struct dmabuf_page_pool *pools[NUM_ORDERS];

static bool smmu_v3_enable;
int tmem_api_ver;

enum sec_heap_region_type {
	/* MM heap */
	MM_HEAP_START,
	SVP_REGION,
	PROT_REGION,
	PROT_2D_FR_REGION,
	WFD_REGION,
	TUI_REGION,
	MM_HEAP_END,

	/* APU heap */
	APU_HEAP_START,
	SAPU_DATA_SHM_REGION,
	SAPU_ENGINE_SHM_REGION,
	APU_HEAP_END,

	REGION_HEAPS_NUM,
};

enum sec_heap_page_type {
	SVP_PAGE,
	PROT_PAGE,
	WFD_PAGE,
	SAPU_PAGE,
	TEE_PAGE,
	PAGE_HEAPS_NUM,
};

enum HEAP_BASE_TYPE {
	HEAP_TYPE_INVALID = 0,
	REGION_BASE,
	PAGE_BASE,
	HEAP_BASE_NUM
};

#define NAME_LEN 32

enum REGION_TYPE {
	REGION_HEAP_NORMAL,
	REGION_HEAP_ALIGN,
	REGION_TYPE_NUM,
};

struct secure_heap_region {
	bool heap_mapped; /* indicate whole region if it is mapped */
	bool heap_filled; /* indicate whole region if it is filled */
	struct mutex heap_lock;
	const char *heap_name[REGION_TYPE_NUM];
	atomic64_t total_size;
	phys_addr_t region_pa;
	u32 region_size;
	struct sg_table *region_table;
	struct dma_heap *heap[REGION_TYPE_NUM];
	struct device *heap_dev;
	struct device *iommu_dev; /* smmu uses shared dev */
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

struct secure_heap_page {
	const char *heap_name;
	atomic64_t total_size;
	struct dma_heap *heap;
	struct device *heap_dev;
	struct page *bitmap;
	enum TRUSTED_MEM_REQ_TYPE tmem_type;
	enum HEAP_BASE_TYPE heap_type;
};

struct mtk_sec_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	bool uncached;
	/* helper function */
	int (*show)(const struct dma_buf *dmabuf, struct seq_file *s);

	/* secure heap will not strore sgtable here */
	struct list_head iova_caches;
	struct mutex map_lock; /* map iova lock */
	pid_t pid;
	pid_t tid;
	char pid_name[TASK_COMM_LEN];
	char tid_name[TASK_COMM_LEN];
	unsigned long long ts; /* us */

	int gid; /* slc */

	/* private part for secure heap */
	u64 sec_handle; /* keep same type with tmem */
	struct ssheap_buf_info *ssheap; /* for page base */
};

static struct secure_heap_page *mtk_sec_heap_page;
static u32 mtk_sec_page_count;

static struct secure_heap_region *mtk_sec_heap_region;
static u32 mtk_sec_region_count;

/* function declare */
static int is_region_base_dmabuf(const struct dma_buf *dmabuf);
static int is_page_base_dmabuf(const struct dma_buf *dmabuf);
static int sec_buf_priv_dump(const struct dma_buf *dmabuf, struct seq_file *s);

static int mtee_assign_buffer_v2(struct ssheap_buf_info *ssheap, u8 pmm_attr);
static int mtee_unassign_buffer_v2(struct ssheap_buf_info *ssheap, u8 pmm_attr);

static struct iova_cache_data *get_iova_cache(struct mtk_sec_heap_buffer *buffer, u64 tab_id)
{
	struct iova_cache_data *cache_data;

	list_for_each_entry(cache_data, &buffer->iova_caches, iova_caches) {
		if (cache_data->tab_id == tab_id)
			return cache_data;
	}
	return NULL;
}

static bool region_heap_is_aligned(struct dma_heap *heap)
{
	if (strstr(dma_heap_get_name(heap), "aligned"))
		return true;

	return false;
}

static int get_heap_base_type(const struct dma_heap *heap)
{
	int i, j, k;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return REGION_BASE;
		}
	}
	for (k = 0; k < mtk_sec_page_count; k++) {
		if (mtk_sec_heap_page[k].heap == heap)
			return PAGE_BASE;
	}
	return HEAP_TYPE_INVALID;
}

static struct secure_heap_region *
sec_heap_region_get(const struct dma_heap *heap)
{
	int i, j;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (mtk_sec_heap_region[i].heap[j] == heap)
				return &mtk_sec_heap_region[i];
		}
	}
	return NULL;
}

static struct secure_heap_page *sec_heap_page_get(const struct dma_heap *heap)
{
	int i = 0;

	for (i = 0; i < mtk_sec_page_count; i++) {
		if (mtk_sec_heap_page[i].heap == heap)
			return &mtk_sec_heap_page[i];
	}
	return NULL;
}

static struct sg_table *dup_sg_table_sec(struct sg_table *table)
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
		/* skip copy dma_address, need get via map_attachment */
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}
//TODO rename
static int system_heap_zero_buffer(struct mtk_sec_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}

	return ret;
}

static int region_base_free(struct secure_heap_region *sec_heap,
			    struct mtk_sec_heap_buffer *buffer)
{
	struct iova_cache_data *cache_data, *temp_data;
	struct device *iommu_dev;
	int j, ret = 0;
	u64 sec_handle = 0;

	iommu_dev = sec_heap->iommu_dev;
	/* remove all domains' sgtable */
	list_for_each_entry_safe(cache_data, temp_data, &buffer->iova_caches, iova_caches) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			struct sg_table *table = cache_data->mapped_table[j];
			struct mtk_heap_dev_info dev_info =
				cache_data->dev_info[j];
			unsigned long attrs =
				dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

			if (!cache_data->mapped[j] ||
			    (!smmu_v3_enable && dev_is_normal_region(dev_info.dev)) ||
			    (smmu_v3_enable &&
			    (get_smmu_tab_id(dev_info.dev) == get_smmu_tab_id(iommu_dev))))
				continue;
			pr_debug("%s: free tab:%llu, dom:%d iova:0x%lx, dev:%s\n",
				 __func__, cache_data->tab_id, j,
				 (unsigned long)sg_dma_address(table->sgl),
				 dev_name(dev_info.dev));
			dma_unmap_sgtable(dev_info.dev, table,
					  dev_info.direction, attrs);
			cache_data->mapped[j] = false;
			sg_free_table(table);
			kfree(table);
		}
		list_del(&cache_data->iova_caches);
		kfree(cache_data);
	}

	sec_handle = buffer->sec_handle;
	ret = trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, NULL);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s warn!, total memory overflow, 0x%llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	if (!atomic64_read(&sec_heap->total_size)) {
		mutex_lock(&sec_heap->heap_lock);
		if (sec_heap->heap_filled && sec_heap->region_table) {
			if (sec_heap->heap_mapped) {
				dma_unmap_sgtable(iommu_dev,
						  sec_heap->region_table,
						  DMA_BIDIRECTIONAL,
						  DMA_ATTR_SKIP_CPU_SYNC);
				sec_heap->heap_mapped = false;
			}
			sg_free_table(sec_heap->region_table);
			kfree(sec_heap->region_table);
			sec_heap->region_table = NULL;
			sec_heap->heap_filled = false;
			pr_debug(
				"%s: all secure memory already free, unmap heap_region iova\n",
				__func__);
		}
		mutex_unlock(&sec_heap->heap_lock);
	}

	pr_debug("%s done, [%s] size:%#lx, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));
	return ret;
}

static int page_base_free_v2(struct secure_heap_page *sec_heap,
			     struct mtk_sec_heap_buffer *buffer)
{
	struct page *pmm_page, *tmp_page;
	struct list_head *pmm_msg_list;
	struct sg_table *table = NULL;
	struct scatterlist *sg = NULL;
	int smc_ret, i, j, page_count = 0;
	uint32_t *bitmap = page_address(sec_heap->bitmap);

	smc_ret = mtee_unassign_buffer_v2(buffer->ssheap, sec_heap->tmem_type);
	if (smc_ret) {
		pr_err("%s: error, unassign buffer smc_ret=%d\n", __func__,
		       smc_ret);
		return -EINVAL;
	}

	/* free the pmm_msg_pages */
	pmm_page = buffer->ssheap->pmm_page;
	pmm_msg_list = &buffer->ssheap->pmm_msg_list;
	list_for_each_entry_safe(pmm_page, tmp_page, pmm_msg_list, lru) {
		uint32_t *pmm_msg = page_address(pmm_page);
		uint32_t idx_2m, idx, offset;
		uint32_t entry_num = page_size(pmm_page)/sizeof(uint32_t);

		// pr_debug("list_for_each_entry: entry_num%#x\n", entry_num);
		for (i = 0; i < entry_num && pmm_msg[i]; i++) {
			idx_2m = (pmm_msg[i] & 0xffffff) >> 9; // 2MB -> 21bit = 12bit(page) + 9bit
			idx = idx_2m / 32; // bitmap layout: 0 ~ 31bit, one bit -> one 2MB block
			offset = idx_2m % 32; // offset is the 2MB block which this page included.
			bitmap[idx] |= (1 << offset);
			// pr_debug("bitmap[%#x]:%#x, offset:%#x\n", idx, bitmap[idx], offset);
			++page_count;
		}
		memset(page_address(pmm_page), 0, page_size(pmm_page));
		__free_pages(pmm_page, get_order(PAGE_SIZE));
	}
	kfree(buffer->ssheap);

	/* Zero the buffer pages before adding back to the pool */
	system_heap_zero_buffer(buffer);

	table = &buffer->sg_table;
	for_each_sgtable_sg(table, sg, i) {
		struct page *page;

		if (!sg) {
			pr_info("%s err, sg null at %d\n", __func__, i);
			return -EINVAL;
		}
		page = sg_page(sg);

		for (j = 0; j < NUM_ORDERS; j++) {
			if (compound_order(page) == orders[j])
				break;
		}
		if (j < NUM_ORDERS)
			dmabuf_page_pool_free(pools[j], page);
		else
			pr_err("%s error: order %u\n", __func__,
			       compound_order(page));
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, %#llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	/* check need infra MPU lv1 bypass */
	if (atomic64_read(&sec_heap->total_size) == 0) {
		struct arm_smccc_res smc_res;
		int i;

		pr_info("%s: page count:%d, merge the cpu and infra-mpu pgtbl\n",
				__func__, page_count);
		pr_debug("bitmap:\n");
		for (i = 0; i < 1024; ++i) {
			if (bitmap[i] != 0)
				pr_debug("%#4x: %#32x ", i, bitmap[i]);
		}
		arm_smccc_smc(HYP_PMM_MERGED_TABLE, page_to_pfn(sec_heap->bitmap),
				0, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0 != 0) {
			pr_err("smc_res.a0=%#lx\n", smc_res.a0);
			return -EINVAL;
		}
		memset(page_address(sec_heap->bitmap), 0, PAGE_SIZE);
	}

	pr_debug("%s done, [%s] size:%#lx, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));
	return 0;
}

static int page_base_free(struct secure_heap_page *sec_heap,
			  struct mtk_sec_heap_buffer *buffer)
{
	int ret;

	ret = trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, buffer->ssheap);
	if (ret) {
		pr_err("%s error, trusted_mem_api_unref failed, heap:%s, ret:%d\n",
		       __func__, dma_heap_get_name(buffer->heap), ret);
		return ret;
	}

	if (atomic64_sub_return(buffer->len, &sec_heap->total_size) < 0)
		pr_warn("%s, total memory overflow, 0x%llx!!\n", __func__,
			atomic64_read(&sec_heap->total_size));

	pr_debug("%s done, [%s] size:%#lx, total_size:%#llx\n", __func__,
		 dma_heap_get_name(buffer->heap), buffer->len,
		 atomic64_read(&sec_heap->total_size));

	return ret;
}

static void tmem_region_free(struct dma_buf *dmabuf)
{
	int ret = -EINVAL;
	struct secure_heap_region *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
				      "null ptr");
		return;
	}

	ret = region_base_free(sec_heap, buffer);
	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

static void tmem_page_free(struct dma_buf *dmabuf)
{
	struct iova_cache_data *cache_data, *temp_data;
	int j, ret = -EINVAL;
	struct secure_heap_page *sec_heap;
	struct mtk_sec_heap_buffer *buffer = NULL;

	dmabuf_release_check(dmabuf);

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       buffer->heap ? dma_heap_get_name(buffer->heap) :
				      "null ptr");
		return;
	}

	/* remove all domains' sgtable */
	list_for_each_entry_safe(cache_data, temp_data, &buffer->iova_caches, iova_caches) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			struct sg_table *table = cache_data->mapped_table[j];
			struct mtk_heap_dev_info dev_info = cache_data->dev_info[j];
			unsigned long attrs =
				dev_info.map_attrs | DMA_ATTR_SKIP_CPU_SYNC;

			if (!cache_data->mapped[j])
				continue;
			pr_debug(
				"%s: free tab:%llu, region:%d iova:0x%lx, dev:%s\n",
				__func__, cache_data->tab_id, j,
				(unsigned long)sg_dma_address(table->sgl),
				dev_name(dev_info.dev));
			dma_unmap_sgtable(dev_info.dev, table,
					  dev_info.direction, attrs);
			cache_data->mapped[j] = false;
			sg_free_table(table);
			kfree(table);
		}
		list_del(&cache_data->iova_caches);
		kfree(cache_data);
	}

	trusted_mem_page_based_free(sec_heap->tmem_type, buffer->sec_handle);

	if (tmem_api_ver == 2)
		ret = page_base_free_v2(sec_heap, buffer);
	else
		ret = page_base_free(sec_heap, buffer);

	if (ret) {
		pr_err("%s fail, heap:%u\n", __func__, sec_heap->heap_type);
		return;
	}

	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

static int mtk_sec_heap_attach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table_sec(&buffer->sg_table);
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

static void mtk_sec_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

/* source copy to dest */
static int copy_sec_sg_table(struct sg_table *source, struct sg_table *dest)
{
	int i;
	struct scatterlist *sgl, *dest_sgl;

	if (source->orig_nents != dest->orig_nents) {
		pr_err("%s err, nents not match %d-%d\n", __func__,
		       source->orig_nents, dest->orig_nents);

		return -EINVAL;
	}

	/* copy mapped nents */
	dest->nents = source->nents;

	dest_sgl = dest->sgl;
	for_each_sg(source->sgl, sgl, source->orig_nents, i) {
		if (!sgl || !dest_sgl) {
			pr_info("%s err, sgl or dest_sgl null at %d\n",
				__func__, i);
			return -EINVAL;
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
static int fill_sec_buffer_info(struct mtk_sec_heap_buffer *buf,
				struct sg_table *table,
				struct dma_buf_attachment *a,
				enum dma_data_direction dir,
				unsigned int tab_id, unsigned int dom_id)
{
	struct iova_cache_data *cache_data;
	struct sg_table *new_table = NULL;
	int ret = 0;

	/*
	 * devices without iommus attribute,
	 * use common flow, skip set buf_info
	 */
	if (!smmu_v3_enable &&
	   (tab_id >= MTK_M4U_TAB_NR_MAX ||
	    dom_id >= MTK_M4U_DOM_NR_MAX))
		return 0;

	cache_data = get_iova_cache(buf, tab_id);
	if (cache_data != NULL && cache_data->mapped[dom_id]) {
		pr_info("%s err: already mapped, no need fill again\n",
			__func__);
		return -EINVAL;
	}

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return -ENOMEM;

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return -ENOMEM;
	}

	ret = copy_sec_sg_table(table, new_table);
	if (ret)
		return ret;

	if (!cache_data) {
		cache_data = kzalloc(sizeof(*cache_data), GFP_KERNEL);
		if (!cache_data) {
			kfree(new_table);
			return -ENOMEM;
		}
		cache_data->tab_id = tab_id;
		list_add(&cache_data->iova_caches, &buf->iova_caches);
	}

	cache_data->mapped_table[dom_id] = new_table;
	cache_data->mapped[dom_id] = true;
	cache_data->dev_info[dom_id].dev = a->dev;
	cache_data->dev_info[dom_id].direction = dir;
	cache_data->dev_info[dom_id].map_attrs = a->dma_map_attrs;

	return 0;
}

static int check_map_alignment(struct sg_table *table)
{
	if (!trusted_mem_is_page_v2_enabled()) {
		int i;
		struct scatterlist *sgl;

		for_each_sg(table->sgl, sgl, table->orig_nents, i) {
			unsigned int len;
			phys_addr_t s_phys;

			if (!sgl) {
				pr_info("%s err, sgl null at %d\n", __func__, i);
				return -EINVAL;
			}

			len = sgl->length;
			s_phys = sg_phys(sgl);

			if (!IS_ALIGNED(len, SZ_1M)) {
				pr_info("%s err, size(0x%x) is not 1MB alignment\n",
				       __func__, len);
				return -EINVAL;
			}
			if (!IS_ALIGNED(s_phys, SZ_1M)) {
				pr_info("%s err, s_phys(%pa) is not 1MB alignment\n",
				       __func__, &s_phys);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static struct sg_table *
mtk_sec_heap_page_map_dma_buf(struct dma_buf_attachment *attachment,
			      enum dma_data_direction direction)
{
	int ret;
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct iova_cache_data *cache_data = NULL;

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_debug("%s done, non-iommu-dev(%s)\n", __func__,
			 dev_name(attachment->dev));
		return table;
	}

	buffer = dmabuf->priv;
	sec_heap = sec_heap_page_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, dma_sec_heap_get failed\n", __func__);
		return NULL;
	}

	mutex_lock(&buffer->map_lock);
	if (!smmu_v3_enable) {
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		cache_data = get_iova_cache(buffer, tab_id);
	} else {
		tab_id = get_smmu_tab_id(attachment->dev);
		cache_data = get_iova_cache(buffer, tab_id);
		dom_id = 0;
	}

	/* device with iommus attribute and mapped before */
	if (fwspec && cache_data != NULL && cache_data->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(cache_data->mapped_table[dom_id],
					table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_debug(
			"%s done(has mapped), dev:%s(%s), sec_handle:%llu, len:%#lx, iova:%#lx, id:(%d,%d)\n",
			__func__,
			dev_name(cache_data->dev_info[dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle,
			buffer->len, (unsigned long)sg_dma_address(table->sgl),
			tab_id, dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}

	ret = check_map_alignment(table);
	if (ret) {
		pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(ret);
	}
	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret) {
		pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);

		if (dmabuf_rbtree_dump_by_domain)
			dmabuf_rbtree_dump_by_domain(tab_id, dom_id);
		return ERR_PTR(ret);
	}
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id,
				   dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_debug(
		"%s done, dev:%s, sec_handle:%llu, len:%#lx, iova:%#lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle,
		buffer->len, (unsigned long)sg_dma_address(table->sgl), tab_id,
		dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static struct sg_table *
mtk_sec_heap_region_map_dma_buf(struct dma_buf_attachment *attachment,
				enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct dma_buf *dmabuf = attachment->dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_region *sec_heap;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);
	unsigned int tab_id = MTK_M4U_TAB_NR_MAX, dom_id = MTK_M4U_DOM_NR_MAX;
	int ret;
	dma_addr_t dma_address;
	uint64_t phy_addr = 0;
	/* for iommu mapping, should be skip cache sync */
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct device *iommu_dev; /* The dev to map iova */
	struct iova_cache_data *cache_data = NULL;
	unsigned int region_tab_id; /* The region heap's smmu tab id */

	/* non-iommu master */
	if (!fwspec) {
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, non-iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			return ERR_PTR(ret);
		}
		a->mapped = true;
		pr_debug("%s done, non-iommu-dev(%s), pa:0x%lx\n", __func__,
			 dev_name(attachment->dev),
			 (unsigned long)sg_dma_address(table->sgl));
		return table;
	}

	buffer = dmabuf->priv;
	sec_heap = sec_heap_region_get(buffer->heap);
	if (!sec_heap) {
		pr_err("%s, sec_heap_region_get failed\n", __func__);
		return NULL;
	}
	iommu_dev = sec_heap->iommu_dev;

	mutex_lock(&sec_heap->heap_lock);
	if (!sec_heap->heap_mapped) {
		ret = check_map_alignment(sec_heap->region_table);
		if (ret) {
			pr_err("%s err, heap_region size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(iommu_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(ret);
		}
		if (dma_map_sgtable(iommu_dev, sec_heap->region_table,
				    DMA_BIDIRECTIONAL,
				    DMA_ATTR_SKIP_CPU_SYNC)) {
			pr_err("%s err, heap_region(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(iommu_dev));
			mutex_unlock(&sec_heap->heap_lock);
			return ERR_PTR(-ENOMEM);
		}
		sec_heap->heap_mapped = true;
		pr_debug(
			"%s heap_region map success, heap:%s, iova:0x%lx, sz:0x%lx\n",
			__func__, dma_heap_get_name(buffer->heap),
			(unsigned long)sg_dma_address(
				sec_heap->region_table->sgl),
			(unsigned long)sg_dma_len(sec_heap->region_table->sgl));
	}
	mutex_unlock(&sec_heap->heap_lock);

	mutex_lock(&buffer->map_lock);
	if (!smmu_v3_enable) {
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		cache_data = get_iova_cache(buffer, tab_id);
	} else {
		tab_id = get_smmu_tab_id(attachment->dev);
		cache_data = get_iova_cache(buffer, tab_id);
		dom_id = 0;
		region_tab_id = get_smmu_tab_id(iommu_dev);
	}
	phy_addr = (uint64_t)sg_phys(table->sgl);
	/* device with iommus attribute and mapped before */
	if (cache_data && cache_data->mapped[dom_id]) {
		/* mapped before, return saved table */
		ret = copy_sec_sg_table(cache_data->mapped_table[dom_id],
					table);
		if (ret) {
			pr_err("%s err, copy_sec_sg_table failed, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(-EINVAL);
		}
		a->mapped = true;
		pr_debug(
			"%s done(has mapped), dev:%s(%s), sec_handle:%llu, len:%#lx, pa:%#llx, iova:%#lx, id:(%d,%d)\n",
			__func__,
			dev_name(cache_data->dev_info[dom_id].dev),
			dev_name(attachment->dev), buffer->sec_handle,
			buffer->len, phy_addr,
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		mutex_unlock(&buffer->map_lock);
		return table;
	}

	/* For iommu, remap to normal iova domain if necessary */
	if (!smmu_v3_enable && !dev_is_normal_region(attachment->dev)) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);

			if (dmabuf_rbtree_dump_by_domain)
				dmabuf_rbtree_dump_by_domain(tab_id, dom_id);
			return ERR_PTR(ret);
		}
		pr_debug(
			"%s reserve_iommu-dev(%s) dma_map_sgtable done, iova:%#lx, id:(%d,%d)\n",
			__func__, dev_name(attachment->dev),
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		goto map_done;
	}

	/* For smmu, remap to target pgtable if necessary */
	if (smmu_v3_enable && tab_id != region_tab_id) {
		ret = check_map_alignment(table);
		if (ret) {
			pr_err("%s err, size or PA is not 1MB alignment, dev:%s\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		ret = dma_map_sgtable(attachment->dev, table, direction, attr);
		if (ret) {
			pr_err("%s err, iommu-dev(%s) dma_map_sgtable failed\n",
			       __func__, dev_name(attachment->dev));
			mutex_unlock(&buffer->map_lock);
			return ERR_PTR(ret);
		}
		pr_debug(
			"%s reserve_iommu-dev(%s) dma_map_sgtable done, iova:%#lx, id:(%d,%d)\n",
			__func__, dev_name(attachment->dev),
			(unsigned long)sg_dma_address(table->sgl), tab_id,
			dom_id);
		goto map_done;
	}

	if (buffer->len <= 0 || buffer->len > sec_heap->region_size ||
	    phy_addr < sec_heap->region_pa ||
	    phy_addr > sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len >
		    sec_heap->region_pa + sec_heap->region_size ||
	    phy_addr + buffer->len <= sec_heap->region_pa) {
		pr_err("%s err. req size/pa is invalid! heap:%s\n", __func__,
		       dma_heap_get_name(buffer->heap));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	dma_address = phy_addr - sec_heap->region_pa +
		      sg_dma_address(sec_heap->region_table->sgl);
	sg_dma_address(table->sgl) = dma_address;
	sg_dma_len(table->sgl) = buffer->len;

map_done:
	ret = fill_sec_buffer_info(buffer, table, attachment, direction, tab_id,
				   dom_id);
	if (ret) {
		pr_err("%s failed, fill_sec_buffer_info failed, dev:%s\n",
		       __func__, dev_name(attachment->dev));
		mutex_unlock(&buffer->map_lock);
		return ERR_PTR(-ENOMEM);
	}
	a->mapped = true;

	pr_debug(
		"%s done, dev:%s, sec_handle:%llu, len:%#lx(%#x), pa:%#llx, iova:%#lx, id:(%d,%d)\n",
		__func__, dev_name(attachment->dev), buffer->sec_handle,
		buffer->len, sg_dma_len(table->sgl), phy_addr,
		(unsigned long)sg_dma_address(table->sgl), tab_id, dom_id);
	mutex_unlock(&buffer->map_lock);

	return table;
}

static void mtk_sec_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				       struct sg_table *table,
				       enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = attachment->dma_map_attrs | DMA_ATTR_SKIP_CPU_SYNC;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(attachment->dev);

	if (!fwspec) {
		pr_debug("%s, non-iommu-dev unmap, dev:%s\n", __func__,
			 dev_name(attachment->dev));
		dma_unmap_sgtable(attachment->dev, table, direction, attr);
	}
	a->mapped = false;
}

static bool heap_is_region_base(enum HEAP_BASE_TYPE heap_type)
{
	if (heap_type == REGION_BASE)
		return true;

	return false;
}

static int fill_heap_sgtable(struct secure_heap_region *sec_heap,
			     struct mtk_sec_heap_buffer *buffer)
{
	int ret;

	if (!heap_is_region_base(sec_heap->heap_type)) {
		pr_info("%s skip page base filled\n", __func__);
		return 0;
	}

	mutex_lock(&sec_heap->heap_lock);
	if (sec_heap->heap_filled) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_debug("%s, %s already filled\n", __func__,
			 dma_heap_get_name(buffer->heap));
		return 0;
	}

	ret = trusted_mem_api_get_region_info(sec_heap->tmem_type,
					      &sec_heap->region_pa,
					      &sec_heap->region_size);
	if (!ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s],get_region_info failed!\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return -EINVAL;
	}

	sec_heap->region_table =
		kzalloc(sizeof(*sec_heap->region_table), GFP_KERNEL);
	if (!sec_heap->region_table) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] kzalloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	ret = sg_alloc_table(sec_heap->region_table, 1, GFP_KERNEL);
	if (ret) {
		mutex_unlock(&sec_heap->heap_lock);
		pr_err("%s, [%s] alloc_sgtable failed\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return ret;
	}
	sg_set_page(sec_heap->region_table->sgl,
		    phys_to_page(sec_heap->region_pa), sec_heap->region_size,
		    0);
	sec_heap->heap_filled = true;
	mutex_unlock(&sec_heap->heap_lock);
	pr_debug("%s [%s] fill done, region_pa:%pa, region_size:0x%x\n",
		 __func__, dma_heap_get_name(buffer->heap),
		 &sec_heap->region_pa, sec_heap->region_size);
	return 0;
}

static int mtk_sec_heap_dma_buf_get_flags(struct dma_buf *dmabuf,
					  unsigned long *flags)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;

	*flags = buffer->uncached;

	return 0;
}

static int mtk_sec_heap_begin_cpu_access(struct dma_buf *dmabuf,
					 enum dma_data_direction direction)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
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

static int mtk_sec_heap_end_cpu_access(struct dma_buf *dmabuf,
				       enum dma_data_direction direction)
{
	struct mtk_sec_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	if (!buffer->uncached) {
		list_for_each_entry(a, &buffer->attachments, list) {
			if (!a->mapped)
				continue;
			dma_sync_sgtable_for_device(a->dev, a->table,
						    direction);
		}
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static const struct dma_buf_ops sec_buf_region_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_region_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.release = tmem_region_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

static const struct dma_buf_ops sec_buf_page_ops = {
	/* one attachment can only map once */
	.cache_sgt_mapping = 1,
	.attach = mtk_sec_heap_attach,
	.detach = mtk_sec_heap_detach,
	.map_dma_buf = mtk_sec_heap_page_map_dma_buf,
	.unmap_dma_buf = mtk_sec_heap_unmap_dma_buf,
	.begin_cpu_access = mtk_sec_heap_begin_cpu_access,
	.end_cpu_access = mtk_sec_heap_end_cpu_access,
	.release = tmem_page_free,
	.get_flags = mtk_sec_heap_dma_buf_get_flags,
};

static int is_region_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_region_ops;
}

static int is_page_base_dmabuf(const struct dma_buf *dmabuf)
{
	return dmabuf && dmabuf->ops == &sec_buf_page_ops;
}

/* region base size is 4K alignment */
static int region_base_alloc(struct secure_heap_region *sec_heap,
			     struct mtk_sec_heap_buffer *buffer,
			     unsigned long req_sz, bool aligned)
{
	int ret;
	u64 sec_handle = 0;
	u32 refcount = 0; /* tmem refcount */
	u32 alignment = aligned ? SZ_1M : 0;
	uint64_t phy_addr = 0;
	struct sg_table *table;

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(sec_heap->tmem_type, alignment,
				    (unsigned int *)&buffer->len, &refcount,
				    &sec_handle,
				    (uint8_t *)dma_heap_get_name(buffer->heap),
				    0, NULL);
	if (ret == -ENOMEM) {
		pr_err("%s error: security out of memory!! heap:%s\n", __func__,
		       dma_heap_get_name(buffer->heap));
		return -ENOMEM;
	}
	if (!sec_handle) {
		pr_err("%s alloc security memory failed, req_size:%#lx, total_size %#llx\n",
		       __func__, req_sz, atomic64_read(&sec_heap->total_size));
		return -ENOMEM;
	}

	table = &buffer->sg_table;
	/* region base PA is continuous, so nent = 1 */
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. Allocate mem failed.\n", __func__,
		       __LINE__);
		goto free_buffer;
	}
#if IS_ENABLED(CONFIG_ARM_FFA_TRANSPORT)
	if (trusted_mem_is_ffa_enabled()) {
		ret = trusted_mem_ffa_query_pa(&sec_handle, &phy_addr);
	} else {
		ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len,
				       &refcount, &sec_handle,
				       (u8 *)dma_heap_get_name(buffer->heap), 0,
				       0, &phy_addr);
	}
#else
	ret = trusted_mem_api_query_pa(sec_heap->tmem_type, 0, buffer->len,
				       &refcount, &sec_handle,
				       (u8 *)dma_heap_get_name(buffer->heap), 0,
				       0, &phy_addr);
#endif
	if (ret) {
		/* free buffer */
		pr_err("%s#%d Error. query pa failed.\n", __func__, __LINE__);
		goto free_buffer_struct;
	}
	sg_set_page(table->sgl, phys_to_page(phy_addr), buffer->len, 0);
	/* store seucre handle */
	buffer->sec_handle = sec_handle;

	ret = fill_heap_sgtable(sec_heap, buffer);
	if (ret) {
		pr_err("%s#%d Error. fill_heap_sgtable failed.\n", __func__,
		       __LINE__);
		goto free_buffer_struct;
	}

	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_debug(
		"%s done: [%s], req_size:%#lx, align_sz:%#lx, handle:%#llx, pa:%#llx, total_sz:%#llx\n",
		__func__, dma_heap_get_name(buffer->heap), req_sz, buffer->len,
		buffer->sec_handle, phy_addr,
		atomic64_read(&sec_heap->total_size));

	return 0;

free_buffer_struct:
	sg_free_table(table);
free_buffer:
	/* free secure handle */
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      NULL);

	return ret;
}

static struct page *alloc_largest_available(unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < (PAGE_SIZE << orders[i]) && i < NUM_ORDERS - 1)
			continue;
		if (max_order < orders[i])
			continue;
		page = dmabuf_page_pool_alloc(pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

static int mtee_common_buffer_v2(struct ssheap_buf_info *ssheap, u8 pmm_attr,
				 int lock)
{
	struct arm_smccc_res smc_res;
	struct page *pmm_page, *tmp_page;
	uint32_t smc_id = lock == 1 ? HYP_PMM_ASSIGN_BUFFER_V2 :
				      HYP_PMM_UNASSIGN_BUFFER_V2;
	uint32_t tmp_count = 0;
	int count = 0;

	if (!ssheap || !ssheap->pmm_page) {
		pr_err("ssheap info not ready!\n");
		return -EINVAL;
	}

	count = ssheap->elems;
	pmm_page = ssheap->pmm_page;
	list_for_each_entry_safe(pmm_page, tmp_page, &ssheap->pmm_msg_list,
				  lru) {
		tmp_count =
			count >= PMM_MSG_ENTRIES_PER_PAGE ?
				(uint32_t)PMM_MSG_ENTRIES_PER_PAGE :
				(uint32_t)(count % PMM_MSG_ENTRIES_PER_PAGE);
		arm_smccc_smc(smc_id, page_to_pfn(pmm_page), pmm_attr,
			      tmp_count, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0 != 0) {
			pr_err("smc_id=%#x smc_res.a0=%#lx\n", smc_id,
			       smc_res.a0);
			return -EINVAL;
		}
		count -= PMM_MSG_ENTRIES_PER_PAGE;
	}
	return 0;
}

static int mtee_assign_buffer_v2(struct ssheap_buf_info *ssheap, u8 pmm_attr)
{
	return mtee_common_buffer_v2(ssheap, pmm_attr, 1);
}

static int mtee_unassign_buffer_v2(struct ssheap_buf_info *ssheap, u8 pmm_attr)
{
	return mtee_common_buffer_v2(ssheap, pmm_attr, 0);
}

/*
 *	PMM_MSG_ENTRY format
 *	page number = PA >> PAGE_SHIFT
 *	 _______________________________________
 *	|  reserved  | page order | page number |
 *	|____________|____________|_____________|
 *	31         28 27        24 23          0
 */
static inline void set_pmm_msg_entry(uint32_t *pmm_msg, uint32_t index,
				     struct page *page)
{
	int size, order;
	phys_addr_t pa;

	size = page_size(page);
	order = compound_order(page);
	pa = page_to_phys(page);

	pmm_msg[index] = PMM_MSG_ENTRY(pa, order);
//	pr_info("%s: pmm_msg[%d]=%#x (size=%d order=%d pa=0x%llx)\n",
//		__func__, index, pmm_msg[index], size, order, pa);
}

struct page *alloc_pmm_msg_v2(struct sg_table *table,
			      struct ssheap_buf_info *ssheap,
			      unsigned int max_order)
{
	struct page *pmm_page, *tmp_page, *head_page;
	struct scatterlist *sg;
	struct list_head *pmm_msg_list;
	unsigned long size_remaining = 0;
	void *pmm_msg;
	int i = 0, pmepp = PMM_MSG_ENTRIES_PER_PAGE;

	pmm_msg_list = &ssheap->pmm_msg_list;
	INIT_LIST_HEAD(pmm_msg_list);

	size_remaining =
		ROUNDUP(table->orig_nents * sizeof(uint32_t), PAGE_SIZE);
	pr_debug("%s: size_remaining=%#lx\n", __func__, size_remaining);

	while (size_remaining > 0) {
		pmm_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, max_order);
		if (!pmm_page)
			goto free_buffer;
		list_add_tail(&pmm_page->lru, pmm_msg_list);
		size_remaining -= page_size(pmm_page);
		if (i == 0)
			head_page = pmm_page;
		i++;
	}
	pr_debug("%s: pmm_msg alloc %d pages\n", __func__, i);
	pmm_msg = page_address(head_page); // first page's addr
	tmp_page = head_page; // FOR TRIVERS

	sg = table->sgl;
	for (i = 0; i < table->orig_nents; i++) {
		if (i != 0 && (i % pmepp == 0)) {
			tmp_page = list_next_entry(tmp_page, lru);
			pmm_msg = page_address(tmp_page);
		}
		if (!sg) {
			pr_info("%s err, sg null at %d\n", __func__, i);
			goto free_buffer;
		}
		set_pmm_msg_entry(pmm_msg, i % pmepp, sg_page(sg));
		sg = sg_next(sg);
	}

	return head_page;

free_buffer:
	list_for_each_entry_safe(pmm_page, tmp_page, pmm_msg_list, lru)
		__free_pages(pmm_page, compound_order(pmm_page));
	return NULL;
}

struct ssheap_buf_info *create_ssheap_by_sgtable(struct sg_table *table,
						 unsigned long req_sz)
{
	struct ssheap_buf_info *buf = NULL;

	buf = kzalloc(sizeof(struct ssheap_buf_info), GFP_KERNEL);
	if (buf == NULL)
		return NULL;

	buf->table = table;
	buf->aligned_req_size = req_sz;
	buf->alignment = PAGE_SIZE;
	return buf;
}

static int pa_cmp(void *priv, const struct list_head *a,
		  const struct list_head *b)
{
	struct page *ia, *ib;

	ia = list_entry(a, struct page, lru);
	ib = list_entry(b, struct page, lru);

	if (page_to_phys(ia) > page_to_phys(ib))
		return 1;
	if (page_to_phys(ia) < page_to_phys(ib))
		return -1;

	return 0;
}

static int page_base_alloc_v2(struct secure_heap_page *sec_heap,
			      struct mtk_sec_heap_buffer *buffer,
			      unsigned long req_sz)
{
	// check input parameters
	// if (!is_page_based_heap_type(sec_heap->tmem_type))
	//	return -EINVAL;
	unsigned long size_remaining = 0;
	unsigned int max_order = orders[0];
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page, *pmm_page;
	int i = 0, smc_ret = 0;

	size_remaining = ROUNDUP(req_sz, SZ_64K);
	pr_debug("%s: req_sz=%#lx, size_remaining=%#lx\n", __func__, req_sz,
		 size_remaining);
	// allocage pages by dma-poll
	INIT_LIST_HEAD(&pages);

	while (size_remaining > 0) {
		page = alloc_largest_available(size_remaining, max_order);
		if (!page) {
			pr_err("%s: Failed to alloc pages!!\n", __func__);
			goto free_pages;
		}
		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}
	// sort pages by pa
	list_sort(NULL, &pages, pa_cmp);

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL)) {
		pr_err("%s: sg_alloc_table failed\n", __func__);
		goto free_pages;
	}

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
	}

	/*
	 * For uncached buffers, we need to initially invalid cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	dma_map_sgtable(dma_heap_get_dev(buffer->heap), table,
			DMA_BIDIRECTIONAL, 0);
	dma_unmap_sgtable(dma_heap_get_dev(buffer->heap), table,
			  DMA_BIDIRECTIONAL, 0);

	//TODO naming change to assign sg to ssheap
	buffer->ssheap = create_ssheap_by_sgtable(table, req_sz);
	if (buffer->ssheap == NULL) {
		pr_err("%s: create_ssheap_by_sgtable failed\n", __func__);
		goto free_sg_table;
	}

	pmm_page =
		alloc_pmm_msg_v2(table, buffer->ssheap, get_order(PAGE_SIZE));
	if (!pmm_page) {
		pr_err("%s: alloc_pmm_msg_v2 failed\n", __func__);
		kfree(buffer->ssheap);
		goto free_sg_table;
	}

	buffer->ssheap->pmm_page = pmm_page;
	buffer->ssheap->elems = table->orig_nents;
	buffer->len = buffer->ssheap->aligned_req_size;
	atomic64_add(buffer->len, &sec_heap->total_size);

	trusted_mem_enable_high_freq();
	// mtee_assign assign pa to protected pa
	smc_ret = mtee_assign_buffer_v2(buffer->ssheap, sec_heap->tmem_type);
	trusted_mem_disable_high_freq();

	if (smc_ret != 0) {
		pr_err("%s:mtee_assign_buffer_v2 smc_ret=%d\n", __func__,
		       smc_ret);
		kfree(buffer->ssheap);
		goto free_pmm_page;
	}
	return 0;

free_pmm_page:
	list_for_each_entry_safe(pmm_page, tmp_page,
				  &buffer->ssheap->pmm_msg_list, lru)
		__free_pages(pmm_page, compound_order(pmm_page));
free_sg_table:
	sg_free_table(table);
free_pages:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		__free_pages(page, compound_order(page));

	return -ENOMEM;
}

static int page_base_alloc(struct secure_heap_page *sec_heap,
			   struct mtk_sec_heap_buffer *buffer,
			   unsigned long req_sz)
{
	int ret;
	u64 sec_handle = 0;
	u32 refcount = 0; /* tmem refcount */
	struct ssheap_buf_info *ssheap = NULL;
	struct sg_table *table = &buffer->sg_table;

	if (buffer->len > UINT_MAX) {
		pr_err("%s error. len more than UINT_MAX\n", __func__);
		return -EINVAL;
	}
	ret = trusted_mem_api_alloc(
		sec_heap->tmem_type, 0, (unsigned int *)&buffer->len, &refcount,
		&sec_handle, (uint8_t *)dma_heap_get_name(sec_heap->heap), 0,
		&ssheap);
	if (!ssheap) {
		pr_err("%s error, alloc page base failed\n", __func__);
		return -ENOMEM;
	}

	ret = sg_alloc_table(table, ssheap->table->orig_nents, GFP_KERNEL);
	if (ret) {
		pr_err("%s error. sg_alloc_table failed\n", __func__);
		goto free_tmem;
	}

	ret = copy_sec_sg_table(ssheap->table, table);
	if (ret) {
		pr_err("%s error. copy_sec_sg_table failed\n", __func__);
		goto free_table;
	}
	buffer->len = ssheap->aligned_req_size;
	buffer->ssheap = ssheap;
	atomic64_add(buffer->len, &sec_heap->total_size);

	pr_debug(
		"%s done: [%s], req_size:%#lx(%#lx), align_sz:%#lx, nent:%u--%lu, align:%#lx, total_sz:%#llx\n",
		__func__, dma_heap_get_name(sec_heap->heap),
		buffer->ssheap->req_size, req_sz, buffer->len,
		buffer->ssheap->table->orig_nents, buffer->ssheap->elems,
		buffer->ssheap->alignment,
		atomic64_read(&sec_heap->total_size));

	return 0;

free_table:
	sg_free_table(table);
free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      ssheap);

	return ret;
}

static void init_buffer_info(struct dma_heap *heap,
			     struct mtk_sec_heap_buffer *buffer)
{
	struct task_struct *task = current->group_leader;

	INIT_LIST_HEAD(&buffer->attachments);
	INIT_LIST_HEAD(&buffer->iova_caches);
	mutex_init(&buffer->lock);
	mutex_init(&buffer->map_lock);
	/* add alloc pid & tid info */
	get_task_comm(buffer->pid_name, task);
	get_task_comm(buffer->tid_name, current);
	buffer->pid = task_pid_nr(task);
	buffer->tid = task_pid_nr(current);
	buffer->ts = sched_clock() / 1000;
}

static struct dma_buf *alloc_dmabuf(struct dma_heap *heap,
				    struct mtk_sec_heap_buffer *buffer,
				    const struct dma_buf_ops *ops,
				    unsigned long fd_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	return dma_buf_export(&exp_info);
}

static struct dma_buf *tmem_page_allocate(struct dma_heap *heap,
					  unsigned long len,
					  unsigned long fd_flags,
					  unsigned long heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	struct secure_heap_page *sec_heap = sec_heap_page_get(heap);
	u64 sec_handle = 0;

	if (!sec_heap) {
		pr_err("%s, %s can not find secure heap!!\n", __func__,
		       heap ? dma_heap_get_name(heap) : "null ptr");
		return ERR_PTR(-EINVAL);
	}

	if (len / PAGE_SIZE > totalram_pages()) {
		pr_err("%s, len %ld is more than %ld\n", __func__, len,
			totalram_pages() * PAGE_SIZE);
		return ERR_PTR(-ENOMEM);
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->len = len;
	buffer->heap = heap;
	/* all page base memory set as noncached buffer */
	buffer->uncached = true;
	buffer->show = sec_buf_priv_dump;
	buffer->gid = -1;

	if (tmem_api_ver == 2)
		ret = page_base_alloc_v2(sec_heap, buffer, len);
	else
		ret = page_base_alloc(sec_heap, buffer, len);

	if (ret) {
		pr_err("%s: page_base_alloc%s ret = %d\n", __func__,
		       tmem_api_ver == 2 ? "_v2" : "", ret);
		goto free_buffer;
	}

	// TODO: need fix cause didn't call free in free
	ret = trusted_mem_page_based_alloc(sec_heap->tmem_type, &buffer->sg_table,
				     &sec_handle, len);
	/* store seucre handle */
	buffer->sec_handle = sec_handle;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_page_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}

	init_buffer_info(heap, buffer);

	return dmabuf;

free_tmem:
	if (tmem_api_ver == 2) {
		page_base_free_v2(sec_heap, buffer);
	} else {
		trusted_mem_api_unref(
			sec_heap->tmem_type, buffer->sec_handle,
			(uint8_t *)dma_heap_get_name(buffer->heap), 0,
			buffer->ssheap);
	}
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

static struct dma_buf *tmem_region_allocate(struct dma_heap *heap,
					    unsigned long len,
					    unsigned long fd_flags,
					    unsigned long heap_flags)
{
	int ret = -ENOMEM;
	struct dma_buf *dmabuf;
	struct mtk_sec_heap_buffer *buffer;
	bool aligned = region_heap_is_aligned(heap);
	struct secure_heap_region *sec_heap = sec_heap_region_get(heap);

	if (!sec_heap) {
		pr_err("%s, can not find secure heap(%s)!!\n", __func__,
		       (heap ? dma_heap_get_name(heap) : "null ptr"));
		return ERR_PTR(-EINVAL);
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->len = len;
	buffer->heap = heap;
	buffer->show = sec_buf_priv_dump;
	buffer->gid = -1;

	ret = region_base_alloc(sec_heap, buffer, len, aligned);
	if (ret)
		goto free_buffer;

	dmabuf = alloc_dmabuf(heap, buffer, &sec_buf_region_ops, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		pr_err("%s alloc_dmabuf fail\n", __func__);
		goto free_tmem;
	}
	init_buffer_info(heap, buffer);

	return dmabuf;

free_tmem:
	trusted_mem_api_unref(sec_heap->tmem_type, buffer->sec_handle,
			      (uint8_t *)dma_heap_get_name(buffer->heap), 0,
			      NULL);
	sg_free_table(&buffer->sg_table);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

static const struct dma_heap_ops sec_heap_page_ops = {
	.allocate = tmem_page_allocate,
};

static const struct dma_heap_ops sec_heap_region_ops = {
	.allocate = tmem_region_allocate,
};

static int sec_buf_priv_dump(const struct dma_buf *dmabuf, struct seq_file *s)
{
	struct iova_cache_data *cache_data, *temp_data;
	unsigned int i = 0, j = 0;
	dma_addr_t iova = 0;
	int region_buf = 0;
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	u64 sec_handle = 0;

	dmabuf_dump(
		s,
		"\t\tbuf_priv: uncached:%d alloc_pid:%d(%s)tid:%d(%s) alloc_time:%lluus\n",
		!!buf->uncached, buf->pid, buf->pid_name, buf->tid,
		buf->tid_name, buf->ts);

	/* region base, only has secure handle */
	if (is_page_base_dmabuf(dmabuf)) {
		region_buf = 0;
	} else if (is_region_base_dmabuf(dmabuf)) {
		region_buf = 1;
	} else {
		WARN_ON(1);
		return 0;
	}

	list_for_each_entry_safe(cache_data, temp_data, &buf->iova_caches, iova_caches) {
		for (j = 0; j < MTK_M4U_DOM_NR_MAX; j++) {
			bool mapped = cache_data->mapped[j];
			struct device *dev = cache_data->dev_info[j].dev;
			struct sg_table *sgt = cache_data->mapped_table[j];
			char tmp_str[40];
			int len = 0;

			if (!sgt || !sgt->sgl || !dev ||
			    !dev_iommu_fwspec_get(dev))
				continue;

			iova = sg_dma_address(sgt->sgl);

			if (region_buf) {
				sec_handle =
					(dma_addr_t)dmabuf_to_secure_handle(
						dmabuf);
				len = scnprintf(tmp_str, 39, "sec_handle:%#llx",
						sec_handle);
				if (len >= 0)
					tmp_str[len] =
						'\0'; /* No need memset */
			}
			dmabuf_dump(
				s,
				"\t\tbuf_priv: tab:%-2u dom:%-2u map:%d iova:%#-12lx %s attr:%#-4lx dir:%-2d dev:%s\n",
				i, j, mapped, (unsigned long)iova,
				region_buf ? tmp_str : "",
				cache_data->dev_info[j].map_attrs,
				cache_data->dev_info[j].direction,
				dev_name(cache_data->dev_info[j].dev));
		}
	}

	return 0;
}

/**
 * return none-zero value means dump fail.
 *       maybe the input dmabuf isn't this heap buffer, no need dump
 *
 * return 0 means dump pass
 */
static int sec_heap_buf_priv_dump(const struct dma_buf *dmabuf,
				  struct dma_heap *heap, void *priv)
{
	struct mtk_sec_heap_buffer *buf = dmabuf->priv;
	struct seq_file *s = priv;

	if (!is_mtk_sec_heap_dmabuf(dmabuf) || heap != buf->heap)
		return -EINVAL;

	if (buf->show)
		return buf->show(dmabuf, s);

	return -EINVAL;
}

static struct mtk_heap_priv_info mtk_sec_heap_priv = {
	.buf_priv_dump = sec_heap_buf_priv_dump,
};

int is_mtk_sec_heap_dmabuf(const struct dma_buf *dmabuf)
{
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	if (dmabuf->ops == &sec_buf_page_ops ||
	    dmabuf->ops == &sec_buf_region_ops)
		return 1;

	return 0;
}
EXPORT_SYMBOL_GPL(is_mtk_sec_heap_dmabuf);

int is_support_secure_handle(const struct dma_buf *dmabuf)
{
	if (IS_ERR_OR_NULL(dmabuf))
		return 0;

	return is_region_base_dmabuf(dmabuf);
}
EXPORT_SYMBOL_GPL(is_support_secure_handle);

/* return 0 means error */
u64 dmabuf_to_secure_handle(const struct dma_buf *dmabuf)
{
	int heap_base;
	struct mtk_sec_heap_buffer *buffer;

	if (!is_mtk_sec_heap_dmabuf(dmabuf)) {
		pr_err("%s err, dmabuf is not secure\n", __func__);
		return 0;
	}
	buffer = dmabuf->priv;
	heap_base = get_heap_base_type(buffer->heap);
	if (heap_base != REGION_BASE && heap_base != PAGE_BASE) {
		pr_warn("%s failed, heap(%s) not support sec_handle\n",
			__func__, dma_heap_get_name(buffer->heap));
		return 0;
	}

	return buffer->sec_handle;
}
EXPORT_SYMBOL_GPL(dmabuf_to_secure_handle);

static int mtk_region_heap_create(void)
{
	struct dma_heap_export_info exp_info = {0};
	int i = 0, j, ret;

	/* region base & page base use same heap show */
	exp_info.priv = (void *)&mtk_sec_heap_priv;

	/* No need pagepool for secure heap */
	exp_info.ops = &sec_heap_region_ops;

	for (i = 0; i < mtk_sec_region_count; i++) {
		for (j = REGION_HEAP_NORMAL; j < REGION_TYPE_NUM; j++) {
			if (!mtk_sec_heap_region[i].heap_name[j])
				continue;
			exp_info.name = mtk_sec_heap_region[i].heap_name[j];
			mtk_sec_heap_region[i].heap[j] = dma_heap_add(&exp_info);
			if (IS_ERR(mtk_sec_heap_region[i].heap[j]))
				return PTR_ERR(mtk_sec_heap_region[i].heap[j]);

			ret = dma_set_mask_and_coherent(
				mtk_sec_heap_region[i].heap_dev,
				DMA_BIT_MASK(34));
			if (ret) {
				dev_info(
					mtk_sec_heap_region[i].heap_dev,
					"dma_set_mask_and_coherent failed: %d\n",
					ret);
				return ret;
			}
			mutex_init(&mtk_sec_heap_region[i].heap_lock);

			pr_info("%s add heap[%s][%d] dev:%s, success\n",
				__func__, exp_info.name,
				mtk_sec_heap_region[i].tmem_type,
				dev_name(mtk_sec_heap_region[i].heap_dev));
		}
	}

	return 0;
}

static struct list_head secure_heap_config_data;
static int mtk_dma_heap_config_probe(struct platform_device *pdev)
{
	struct mtk_dma_heap_config *heap_config;
	int ret;

	heap_config = kzalloc(sizeof(*heap_config), GFP_KERNEL);
	if (!heap_config)
		return -ENOMEM;

	ret = mtk_dma_heap_config_parse(&pdev->dev, heap_config);
	if (ret)
		return ret;

	list_add(&heap_config->list_node, &secure_heap_config_data);
	return 0;
}

static const struct mtk_dma_heap_match_data dmaheap_data_mtk_region = {
	.dmaheap_type = DMA_HEAP_MTK_SEC_REGION,
};

static const struct mtk_dma_heap_match_data dmaheap_data_mtk_page = {
	.dmaheap_type = DMA_HEAP_MTK_SEC_PAGE,
};

static const struct of_device_id mtk_dma_heap_match_table[] = {
	{.compatible = "mediatek,dmaheap-mtk-sec-region", .data = &dmaheap_data_mtk_region},
	{.compatible = "mediatek,dmaheap-mtk-sec-page", .data = &dmaheap_data_mtk_page},
	{},
};

static struct platform_driver mtk_dma_heap_config_driver = {
	.probe = mtk_dma_heap_config_probe,
	.driver = {
		.name = "mtk-dma-heap-secure",
		.of_match_table = mtk_dma_heap_match_table,
	},
};

static int set_heap_dev_dma(struct device *heap_dev)
{
	int err = 0;

	if (!heap_dev)
		return -EINVAL;

	dma_coerce_mask_and_coherent(heap_dev, DMA_BIT_MASK(64));

	if (!heap_dev->dma_parms) {
		heap_dev->dma_parms = devm_kzalloc(
			heap_dev, sizeof(*heap_dev->dma_parms), GFP_KERNEL);
		if (!heap_dev->dma_parms)
			return -ENOMEM;

		err = dma_set_max_seg_size(heap_dev,
					   (unsigned int)DMA_BIT_MASK(64));
		if (err) {
			devm_kfree(heap_dev, heap_dev->dma_parms);
			dev_err(heap_dev,
				"Failed to set DMA segment size, err:%d\n",
				err);
			return err;
		}
	}

	return 0;
}

static int mtk_dma_heap_config_alloc(void)
{
	struct mtk_dma_heap_config *heap_config, *temp_config;
	struct secure_heap_region *heap_region;
	struct secure_heap_page *heap_page;
	int i = 0, j = 0;

	list_for_each_entry(heap_config, &secure_heap_config_data, list_node) {
		if (heap_config->heap_type == DMA_HEAP_MTK_SEC_PAGE)
			mtk_sec_page_count++;
		else if (heap_config->heap_type == DMA_HEAP_MTK_SEC_REGION)
			mtk_sec_region_count++;
	}

	pr_info("%s page count:%d,region count:%d\n", __func__,
		mtk_sec_page_count, mtk_sec_region_count);

	mtk_sec_heap_page = kcalloc(mtk_sec_page_count, sizeof(*mtk_sec_heap_page), GFP_KERNEL);
	if (!mtk_sec_heap_page)
		goto alloc_page_fail;

	mtk_sec_heap_region = kcalloc(mtk_sec_region_count, sizeof(*mtk_sec_heap_region),
				      GFP_KERNEL);
	if (!mtk_sec_heap_region)
		goto alloc_region_fail;

	memset(mtk_sec_heap_page, 0, sizeof(*mtk_sec_heap_page) * mtk_sec_page_count);
	memset(mtk_sec_heap_region, 0, sizeof(*mtk_sec_heap_region) * mtk_sec_region_count);

	list_for_each_entry_safe(heap_config, temp_config, &secure_heap_config_data, list_node) {
		if (heap_config->heap_type == DMA_HEAP_MTK_SEC_PAGE && i < mtk_sec_page_count) {
			heap_page = &mtk_sec_heap_page[i];
			heap_page->heap_type = PAGE_BASE;
			heap_page->tmem_type = heap_config->trusted_mem_type;
			heap_page->heap_dev = heap_config->dev;
			heap_page->heap_name = heap_config->heap_name;
			i++;
		} else if (heap_config->heap_type == DMA_HEAP_MTK_SEC_REGION &&
			   j < mtk_sec_region_count) {
			heap_region = &mtk_sec_heap_region[j];
			heap_region->heap_type = REGION_BASE;
			heap_region->tmem_type = heap_config->trusted_mem_type;
			heap_region->heap_dev = heap_config->dev;
			heap_region->iommu_dev = mtk_smmu_get_shared_device(heap_config->dev);
			heap_region->heap_name[REGION_HEAP_NORMAL] = heap_config->heap_name;
			heap_region->heap_name[REGION_HEAP_ALIGN] =
							heap_config->region_heap_align_name;
			j++;
		}

		list_del(&heap_config->list_node);
		kfree(heap_config);
	}

	return 0;

alloc_region_fail:
	kfree(mtk_sec_heap_page);
	mtk_sec_heap_page = NULL;

alloc_page_fail:
	mtk_sec_page_count = 0;
	mtk_sec_region_count = 0;

	list_for_each_entry_safe(heap_config, temp_config, &secure_heap_config_data, list_node) {
		list_del(&heap_config->list_node);
		kfree(heap_config);
	}
	return -ENOMEM;
}

static int mtk_page_heap_create(void)
{
	struct dma_heap_export_info exp_info = {0};
	int i, j;
	int err = 0;

	/* page pool */
	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i] = dmabuf_page_pool_create(order_flags[i], orders[i]);
		if (IS_ERR(pools[i])) {
			pr_err("%s: page pool creation failed!\n", __func__);
			for (j = 0; j < i; j++)
				dmabuf_page_pool_destroy(pools[j]);
			return PTR_ERR(pools[i]);
		}
	}

	exp_info.ops = &sec_heap_page_ops;
	for (i = 0; i < mtk_sec_page_count; i++) {
		/* param check */
		if (mtk_sec_heap_page[i].heap_type != PAGE_BASE) {
			pr_info("invalid heap param, %s, %d\n",
				mtk_sec_heap_page[i].heap_name,
				mtk_sec_heap_page[i].heap_type);
			continue;
		}

		exp_info.name = mtk_sec_heap_page[i].heap_name;

		mtk_sec_heap_page[i].heap = dma_heap_add(&exp_info);
		if (IS_ERR(mtk_sec_heap_page[i].heap)) {
			pr_err("%s error, dma_heap_add failed, heap:%s\n",
			       __func__, mtk_sec_heap_page[i].heap_name);
			return PTR_ERR(mtk_sec_heap_page[i].heap);
		}
		err = set_heap_dev_dma(
			dma_heap_get_dev(mtk_sec_heap_page[i].heap));
		if (err) {
			pr_err("%s add heap[%s][%d] failed\n", __func__,
			       exp_info.name, mtk_sec_heap_page[i].tmem_type);
			return err;
		}
		mtk_sec_heap_page[i].bitmap = alloc_page(GFP_KERNEL | __GFP_ZERO);
		pr_info("%s add heap[%s][%d] success\n", __func__,
			exp_info.name, mtk_sec_heap_page[i].tmem_type);
	}
	return 0;
}

static int __init mtk_sec_heap_init(void)
{
	int ret;

	pr_info("%s+\n", __func__);

	smmu_v3_enable = smmu_v3_enabled();

	if (trusted_mem_is_page_v2_enabled())
		tmem_api_ver = 2;
	else
		tmem_api_ver = 1;

	INIT_LIST_HEAD(&secure_heap_config_data);
	ret = platform_driver_register(&mtk_dma_heap_config_driver);
	if (ret < 0) {
		pr_info("%s fail to register dma heap: %d\n", __func__, ret);
		return ret;
	}

	ret = mtk_dma_heap_config_alloc();
	if (ret) {
		pr_info("%s secure heap alloc failed\n", __func__);
		goto err;
	}

	ret = mtk_page_heap_create();
	if (ret) {
		pr_info("%s page_base_heap_create failed\n", __func__);
		goto err;
	}

	ret = mtk_region_heap_create();
	if (ret) {
		pr_info("%s region_base_heap_create failed\n", __func__);
		goto err;
	}

	pr_info("%s-\n", __func__);

	return 0;

err:
	platform_driver_unregister(&mtk_dma_heap_config_driver);

	return ret;

}

static void __exit mtk_sec_heap_exit(void)
{
	kfree(mtk_sec_heap_page);
	kfree(mtk_sec_heap_region);

	platform_driver_unregister(&mtk_dma_heap_config_driver);
}

MODULE_SOFTDEP("pre: apusys");
module_init(mtk_sec_heap_init);
module_exit(mtk_sec_heap_exit);
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
