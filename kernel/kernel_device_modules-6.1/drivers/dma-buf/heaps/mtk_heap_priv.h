/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#ifndef _MTK_DMABUFHEAP_PRIV_H
#define _MTK_DMABUFHEAP_PRIV_H

#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/iommu.h>

#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include <uapi/linux/dma-buf.h>
#include "mtk-deferred-free-helper.h"

#define P2K(x) ((x) << (PAGE_SHIFT - 10))	/* Converts #Pages to KB */
#define P2M(x) ((x) >> (20 - PAGE_SHIFT))	/* Converts #Pages to MB */

#define DUMP_INFO_LEN_MAX    (400)

/* Bit map */
#define HEAP_DUMP_SKIP_ATTACH     (1 << 0)
#define HEAP_DUMP_SKIP_RB_DUMP    (1 << 1)
#define HEAP_DUMP_HEAP_SKIP_POOL  (1 << 2)
#define HEAP_DUMP_STATS           (1 << 3)
#define HEAP_DUMP_DEC_1_REF       (1 << 4)
#define HEAP_DUMP_OOM             (1 << 5)
#define HEAP_DUMP_PSS_BY_FD	  (1 << 6)
#define HEAP_DUMP_EGL		  (1 << 7)
#define HEAP_DUMP_STATISTIC	  (1 << 8)

#define HANG_DMABUF_FILE_TAG	((void *)0x1)
typedef void (*hang_dump_cb)(const char *fmt, ...);
extern hang_dump_cb hang_dump_proc;

typedef int (*mtk_refill_order_cb)(unsigned int order, int value);

typedef void (*dmabuf_rbtree_dump_cb)(u64 tab_id, u32 dom_id);
extern dmabuf_rbtree_dump_cb dmabuf_rbtree_dump_by_domain;

#define dmabuf_dump(file, fmt, args...)                         \
	do {                                                    \
		if (file == HANG_DMABUF_FILE_TAG) {             \
			if (hang_dump_proc != NULL)             \
				hang_dump_proc(fmt, ##args);    \
		}                                               \
		else if (file)                                  \
			seq_printf(file, fmt, ##args);          \
		else                                            \
			pr_info(fmt, ##args);                   \
	} while (0)

enum mtk_dmaheap_type {
	DMA_HEAP_INVALID,
	DMA_HEAP_SYSTEM,
	DMA_HEAP_MTK_MM,
	DMA_HEAP_MTK_SEC_REGION,
	DMA_HEAP_MTK_SEC_PAGE,
	DMA_HEAP_MTK_SLC,
	DMA_HEAP_MTK_CMA,
};

/* mtk_heap private info, used for dump */
struct mtk_heap_priv_info {
	int uncached;

	/* heap specific page pool */
	struct mtk_dmabuf_page_pool **page_pools;

	/* used for heap dump */
	void (*show)(struct dma_heap *heap, void *seq_file, int flag);

	/* used for buffer dump */
	int (*buf_priv_dump)(const struct dma_buf *dmabuf,
			     struct dma_heap *heap,
			     void *seq_file);
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;

	bool uncached;
};

struct mtk_heap_dev_info {
	struct device           *dev;
	enum dma_data_direction direction;
	unsigned long           map_attrs;
};

struct iova_cache_data {
	struct mtk_heap_dev_info dev_info[MTK_M4U_DOM_NR_MAX];
	struct sg_table          *mapped_table[MTK_M4U_DOM_NR_MAX];
	bool			 mapped[MTK_M4U_DOM_NR_MAX];
	struct list_head	 iova_caches;
	u64			 tab_id;
};

struct mtk_dma_heap_config {
	u32			heap_type;
	const char		*heap_name;
	struct device		*dev;
	bool			heap_uncached;
	u32			trusted_mem_type;
	const char		*region_heap_align_name;
	u32			max_align;
	struct list_head	list_node;
};

struct mtk_dma_heap_match_data {
	u32	dmaheap_type;
};

int mtk_dma_heap_config_parse(struct device *dev, struct mtk_dma_heap_config *heap_config);

const char **mtk_refill_heap_names(void);

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void heap_monitor_init(void);
void heap_monitor_exit(void);
ssize_t heap_monitor_proc_write(struct file *file, const char *buf,
				size_t count, loff_t *data);
int heap_monitor_proc_open(struct inode *inode, struct file *file);
int mtk_register_refill_order_callback(mtk_refill_order_cb cb);
#endif

void dmabuf_log_alloc_time(struct mtk_dmabuf_page_pool *pool, u64 tm, u64 tm_alloc,
			   bool success, bool from_pool);
void dmabuf_log_allocate(struct dma_heap *heap, u64 tm, u32 nent, u32 pages, u32 inode);
void dmabuf_log_refill(int heap_tag, u64 tm, int ret, u32 order, u32 pages_refill, u32 pages_new);
void dmabuf_log_recycle(int heap_tag, u64 tm, u32 order, u32 pages_remove, u32 pages_new);
void dmabuf_log_shrink(struct mtk_dmabuf_page_pool *pool, u64 tm, int freed, int scan);
void dmabuf_log_pool_size(struct dma_heap *heap);
void dmabuf_log_prefill(struct dma_heap *heap, u64 tm, int ret, u32 size);

/* common function */
static void __maybe_unused dmabuf_release_check(const struct dma_buf *dmabuf)
{
	dma_addr_t iova = 0x0;
	const char *device_name = NULL;
	int attach_cnt = 0;
	struct dma_buf_attachment *attach_obj;

	if (!dma_resv_trylock(dmabuf->resv)) {
		/* get lock fail, maybe is using, skip check */
		return;
	}

	/* Don't dump inode number here, it will cause KASAN issue !! */
	if (WARN(!list_empty(&dmabuf->attachments),
		 "%s: size:%-8ld dbg_name:%s exp:%s, %s\n", __func__,
		 dmabuf->size,
		 dmabuf->name,
		 dmabuf->exp_name,
		 "Release dmabuf before detach all attachments, dump attach below:")) {

		/* dump all attachment info */
		list_for_each_entry(attach_obj, &dmabuf->attachments, node) {
			iova = (dma_addr_t)0;

			attach_cnt++;
			if (attach_obj->sgt && dev_iommu_fwspec_get(attach_obj->dev))
				iova = sg_dma_address(attach_obj->sgt->sgl);

			device_name = dev_name(attach_obj->dev);
			dmabuf_dump(NULL,
				    "attach[%d]: iova:0x%-12lx attr:%-4lx dir:%-2d dev:%s\n",
				    attach_cnt, (unsigned long)iova,
				    attach_obj->dma_map_attrs,
				    attach_obj->dir,
				    device_name);
		}
		dmabuf_dump(NULL, "Total %d devices attached\n\n", attach_cnt);
	}
	dma_resv_unlock(dmabuf->resv);
}

static inline long dmabuf_name_check(struct dma_buf *dmabuf, struct device *dev)
{
	char *name = NULL;

	if (IS_ERR_OR_NULL(dmabuf) || IS_ERR_OR_NULL(dev))
		return -EINVAL;

	spin_lock(&dmabuf->name_lock);
	if (dmabuf->name) {
		spin_unlock(&dmabuf->name_lock);
		return 0;
	}

	/* dma_buf set default name is device name if not set name */
	name = kstrndup(dev_name(dev), DMA_BUF_NAME_LEN - 1, GFP_ATOMIC);
	if (IS_ERR_OR_NULL(name)) {
		spin_unlock(&dmabuf->name_lock);
		return -ENOMEM;
	}

	dmabuf->name = name;
	spin_unlock(&dmabuf->name_lock);

	return 0;
}

#if defined(CONFIG_RBIN)
int rbin_dma_heap_init(void);
void rbin_dma_heap_exit(void);
#else
static inline int rbin_dma_heap_init(void)
{
	return 0;
}

#define rbin_dma_heap_exit() do { } while (0)
#endif

struct system_heap_buffer {
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

	/* system heap will not strore sgtable here */
	struct list_head	 iova_caches;
	struct mutex             map_lock; /* map iova lock */
	pid_t                    pid;
	pid_t                    tid;
	char                     pid_name[TASK_COMM_LEN];
	char                     tid_name[TASK_COMM_LEN];
	unsigned long long       ts; /* us */

	int gid; /* slc */

	/* private part for system heap */
	struct mtk_deferred_freelist_item deferred_free;
};

extern int system_heap_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment);
extern void system_heap_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment);
extern void system_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction);
extern int system_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction);
extern int system_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction);
extern int system_heap_vmap(struct dma_buf *dmabuf, struct iosys_map *map);
extern void system_heap_vunmap(struct dma_buf *dmabuf, struct iosys_map *map);
extern int system_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags);
extern struct sg_table *mtk_mm_heap_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction);
extern int mtk_mm_heap_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
					enum dma_data_direction direction,
					unsigned int offset, unsigned int len);
extern int mtk_mm_heap_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					enum dma_data_direction direction,
					unsigned int offset, unsigned int len);
extern int mtk_mm_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma);
extern void mtk_mm_heap_dma_buf_release(struct dma_buf *dmabuf);
extern int is_rbin_heap_dmabuf(const struct dma_buf *dmabuf);
extern void rbin_heap_release(struct dma_buf *dmabuf);

#endif /* _MTK_DMABUFHEAP_PRIV_H */
