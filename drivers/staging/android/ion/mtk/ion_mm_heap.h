#ifndef ION_MM_HEAP_H
#define ION_MM_HEAP_H

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
//#include <mmprofile.h>
//#include <mmprofile_function.h>
#include <linux/kthread.h>
#include <linux/fdtable.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include "mtk/ion_drv.h"

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#endif

struct ion_mm_buffer_info {
	int module_id;
	int fix_module_id;
	unsigned int security;
	unsigned int coherent;
	unsigned int mva_cnt;
	void *VA;
	unsigned long MVA[DOMAIN_NUM];
	unsigned long FIXED_MVA[DOMAIN_NUM];
	unsigned long iova_start[DOMAIN_NUM];
	unsigned long iova_end[DOMAIN_NUM];
	int port[DOMAIN_NUM];
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	struct sg_table table[DOMAIN_NUM];
	struct sg_table *table_orig;
#endif
	struct ion_mm_buf_debug_info dbg_info;
	ion_mm_buf_destroy_callback_t *destroy_fn;
	pid_t pid;
	struct mutex lock;/* buffer lock */
};

#define ION_DUMP(seq_files, fmt, args...) \
	do {\
		struct seq_file *file = (struct seq_file *)seq_files;\
	    char *fmat = fmt;\
		if (file)\
			seq_printf(file, fmat, ##args);\
		else\
			printk(fmat, ##args);\
	} while (0)

void ion_mm_heap_free_buffer_info(struct ion_buffer *buffer);
int ion_mm_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			    ion_phys_addr_t *addr, size_t *len);
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
void ion_mm_heap_get_table(struct ion_buffer *, struct sg_table *);
#endif
#ifdef MTK_ION_DMABUF_SUPPORT
int ion_mm_heap_dma_buf_config(struct ion_buffer *, struct device *);
#endif

#endif /* ION_MM_HEAP_H */
