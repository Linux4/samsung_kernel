#ifndef _NPU_MEMORY_H_
#define _NPU_MEMORY_H_

#include <linux/platform_device.h>
#include <media/videobuf2-core.h>
#if IS_ENABLED(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif IS_ENABLED(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif

#include "vs4l.h"
#include "npu-common.h"

#define NAME_LEN	50

struct npu_memory_buffer {
	struct list_head	list;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	struct list_head	dsp_list;
#endif
	struct dma_buf	*dma_buf;
	struct sg_table	*sgt;
	struct dma_buf_attachment	*attachment;
	dma_addr_t	daddr;
	phys_addr_t	paddr;
	void	*vaddr;
	size_t	size;
	size_t	used_size;
#if (IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR) || IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2))
	size_t	ncp_max_size;
#endif
	struct vs4l_roi			roi;
	union {
		unsigned long		userptr;
		__s32			fd;
	} m;
	ulong	reserved;
	char name[NAME_LEN];
	bool	ncp;
	bool	npu_sgt;
};

struct npu_memory_v_buf {
	struct list_head		list;
	u8				*v_buf;
	size_t				size;
	char name[NAME_LEN];
};

struct npu_memory_debug_info {
	u64	dma_buf;
	size_t	size;
	pid_t	pid;
	char	comm[TASK_COMM_LEN];
	atomic_long_t	ref_count;
	struct list_head		list;
};

struct npu_memory;
struct npu_mem_ops {
	int (*resume)(struct npu_memory *memory);
	int (*suspend)(struct npu_memory *memory);
};

struct npu_memory {
	struct device			*dev;

	spinlock_t			map_lock;
	struct list_head		map_list;
	u32				map_count;

	spinlock_t			alloc_lock;
	struct list_head		alloc_list;
	u32				alloc_count;

	spinlock_t			valloc_lock;
	struct list_head		valloc_list;
	u32				valloc_count;

	spinlock_t			info_lock;
	struct list_head		info_list;
	u32				info_count;
};

int npu_memory_probe(struct npu_memory *memory, struct device *dev);
void npu_memory_release(struct npu_memory *memory);
int npu_memory_open(struct npu_memory *memory);
int npu_memory_close(struct npu_memory *memory);
void npu_memory_alloc_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf, size_t size);
void npu_memory_update_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf);
int npu_memory_map(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap);
void npu_memory_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer);
struct npu_memory_buffer *npu_memory_copy(struct npu_memory *memory,
					struct npu_memory_buffer *buffer, size_t offset, size_t size);
struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag);
int npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot);
int npu_memory_alloc_cached(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot);
int npu_memory_alloc_secure(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot);
int npu_memory_free(struct npu_memory *memory, struct npu_memory_buffer *buffer);
int npu_memory_v_alloc(struct npu_memory *memory, struct npu_memory_v_buf *buffer);
void npu_memory_v_free(struct npu_memory *memory, struct npu_memory_v_buf *buffer);
int npu_memory_secure_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot);
int npu_memory_secure_free(struct npu_memory *memory, struct npu_memory_buffer *buffer);
void * npu_dma_buf_vmap(struct dma_buf *dmabuf);
void npu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr);
dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer);
void npu_memory_dma_buf_dva_unmap(__attribute__((unused))struct npu_memory_buffer *buffer, bool mapping);
unsigned long npu_memory_set_prot(int prot, struct dma_buf_attachment *attachment);
int npu_memory_alloc_from_heap(struct platform_device *pdev,
		struct npu_memory_buffer *buffer, dma_addr_t daddr,
		struct npu_memory *memory, const char *heapname, int prot);
void npu_memory_free_from_heap(struct device *dev, struct npu_memory *memory, struct npu_memory_buffer *buffer);
void npu_dma_buf_add_attr(struct dma_buf_attachment *attachment);
#ifdef CONFIG_NPU_KUNIT_TEST
#define npu_memory_dump(x) do {} while(0)
#define npu_memory_buffer_dump(x) do {} while(0)
#define npu_memory_buffer_health_dump(x) do {} while(0)
#define npu_memory_health(x) do {} while(0)
#else
void npu_memory_dump(struct npu_memory *memory);
void npu_memory_buffer_dump(struct npu_memory_buffer *buffer);
void npu_memory_buffer_health_dump(struct npu_memory_buffer *buffer);
void npu_memory_health(struct npu_memory *memory);
#endif

#endif
