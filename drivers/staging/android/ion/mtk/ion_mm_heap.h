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
//#include <mmprofile.h>
//#include <mmprofile_function.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/fdtable.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include "mtk/ion_drv.h"

struct ion_mm_buffer_info {
	int module_id;
	int fix_module_id;
	unsigned int security;
	unsigned int coherent;
	unsigned int mva_cnt;
	void *VA;
	unsigned int MVA[DOMAIN_NUM];
	unsigned int FIXED_MVA[DOMAIN_NUM];
	unsigned int iova_start[DOMAIN_NUM];
	unsigned int iova_end[DOMAIN_NUM];
	/*  same as module_id for 1 domain */
	int port[DOMAIN_NUM];
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	struct sg_table table[DOMAIN_NUM];
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
#endif /* ION_MM_HEAP_H */
