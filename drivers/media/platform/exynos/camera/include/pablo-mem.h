/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_MEM_H
#define IS_MEM_H

#include <linux/platform_device.h>
#include <media/videobuf2-v4l2.h>
#if defined(CONFIG_VIDEOBUF2_DMA_SG)
#include <media/videobuf2-dma-sg.h>
#endif
#include "pablo-framemgr.h"
#include "exynos-is-sensor.h"

#define SECURE_HEAPNAME "secure_camera-secure"

struct is_sub_dma_buf {
	u32				vid;
	u32				num_plane;
	u32				num_buffer;
	__s32				buf_fd[IS_MAX_PLANES];
	u32				size[IS_MAX_PLANES];

	struct dma_buf			*dbuf[IS_MAX_PLANES];
	struct dma_buf_attachment	*atch[IS_MAX_PLANES];
	struct sg_table			*sgt[IS_MAX_PLANES];

	ulong				kva[IS_MAX_PLANES];
	dma_addr_t			dva[IS_MAX_PLANES];

	const struct pablo_dma_buf_operations		*dbuf_ops;
	const struct pablo_dmabuf_container_operations	*dbuf_con_ops;
};

struct is_sub_buf {
	u32				ldr_vid;
	struct is_sub_dma_buf		sub[CAPTURE_NODE_MAX];
};

struct is_vb2_buf;
struct is_vb2_buf_ops {
	ulong (*plane_kvaddr)(struct is_vb2_buf *vbuf, u32 plane);
	dma_addr_t (*plane_dvaddr)(struct is_vb2_buf *vbuf, u32 plane);
	ulong (*plane_kmap)(struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx);
	void (*plane_kunmap)(struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx);
	dma_addr_t (*plane_dmap)(struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx, struct device *dev_ext);
	void (*plane_dunmap)(struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx);
	void (*plane_sync_for_device)(struct is_vb2_buf *vbuf);
	long (*remap_attr)(struct is_vb2_buf *vbuf, int attr);
	void (*unremap_attr)(struct is_vb2_buf *vbuf, int attr);
	long (*dbufcon_prepare)(struct is_vb2_buf *vbuf, struct device *dev);
	void (*dbufcon_finish)(struct is_vb2_buf *vbuf);
	long (*dbufcon_map)(struct is_vb2_buf *vbuf);
	void (*dbufcon_unmap)(struct is_vb2_buf *vbuf);
	int (*dbufcon_kmap)(struct is_vb2_buf *vbuf, u32 plane);
	void (*dbufcon_kunmap)(struct is_vb2_buf *vbuf, u32 plane);
	void (*subbuf_prepare)(struct is_sub_dma_buf *buf,
				struct v4l2_plane *plane,
				struct device *dev);
	long (*subbuf_dvmap)(struct is_sub_dma_buf *buf);
	ulong (*subbuf_kvmap)(struct is_sub_dma_buf *buf);
	void (*subbuf_kunmap)(struct is_sub_dma_buf *buf);
	void (*subbuf_finish)(struct is_sub_dma_buf *buf);
};

struct pablo_dma_buf_operations {
	struct dma_buf_attachment *(*attach)(struct dma_buf *dma_buf, struct device *dev);
	void (*detach)(struct dma_buf *dma_buf, struct dma_buf_attachment *db_attach);
	struct dma_buf *(*get)(int fd);
	void (*put)(struct dma_buf *dma_buf);
	struct sg_table *(*map_attachment)(struct dma_buf_attachment *db_attach, enum dma_data_direction dir);
	void (*unmap_attachment)(struct dma_buf_attachment *db_attach, struct sg_table *sgt, enum dma_data_direction dir);
	int (*begin_cpu_access)(struct dma_buf *dma_buf, enum dma_data_direction dir);
	int (*end_cpu_access)(struct dma_buf *dma_buf, enum dma_data_direction dir);
	void *(*vmap)(struct dma_buf *dma_buf);
	void (*vunmap)(struct dma_buf *dbuf, void *kva);
	void (*get_dma_buf)(struct dma_buf *dma_buf);
};

struct pablo_dmabuf_container_operations {
	int (*get_count)(struct dma_buf *dma_buf);
	struct dma_buf *(*get_buffer)(struct dma_buf *dma_buf, int index);
};

struct is_vb2_buf {
	struct vb2_v4l2_buffer		vb;
	unsigned int			num_merged_dbufs;
	unsigned int			num_merged_sbufs;
	struct dma_buf			*dbuf[IS_MAX_PLANES];
	struct dma_buf_attachment	*atch[IS_MAX_PLANES];
	struct sg_table			*sgt[IS_MAX_PLANES];
	struct dma_buf			*dbuf_ext[IS_MAX_PLANES];
	struct dma_buf_attachment	*atch_ext[IS_MAX_PLANES];
	struct sg_table			*sgt_ext[IS_MAX_PLANES];

#if IS_ENABLED(DMABUF_CONTAINER)
	ulong				kva[IS_MAX_PLANES];
	dma_addr_t			dva[IS_MAX_PLANES];
	dma_addr_t			dva_ext[IS_MAX_PLANES];
#else
	ulong				kva[VIDEO_MAX_PLANES];
	dma_addr_t			dva[VIDEO_MAX_PLANES];
	dma_addr_t			dva_ext[VIDEO_MAX_PLANES];
#endif
	const struct is_vb2_buf_ops			*ops;
	const struct pablo_dma_buf_operations		*dbuf_ops;
	const struct pablo_dmabuf_container_operations	*dbuf_con_ops;
};

struct is_priv_buf;
struct is_priv_buf_ops {
	void (*free)(struct is_priv_buf *pbuf);
	ulong (*kvaddr)(struct is_priv_buf *pbuf);
	dma_addr_t (*dvaddr)(struct is_priv_buf *pbuf);
	phys_addr_t (*phaddr)(struct is_priv_buf *pbuf);
	void (*sync_for_device)(struct is_priv_buf *pbuf,
							off_t offset, size_t size,
							enum dma_data_direction dir);
	void (*sync_for_cpu)(struct is_priv_buf *pbuf,
							off_t offset, size_t size,
							enum dma_data_direction dir);
	int (*attach_ext)(struct is_priv_buf *pbuf, struct device *dev);
	void (*detach_ext)(struct is_priv_buf *pbuf);
	dma_addr_t (*dvaddr_ext)(struct is_priv_buf *pbuf);
};

struct is_priv_buf {
	size_t				size;
	size_t				align;
	size_t				aligned_size;
	void				*ctx;
	void				*kvaddr;

	const struct is_priv_buf_ops	*ops;
	void				*priv;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct dma_buf_attachment	*attachment_ext;
	enum dma_data_direction		direction;
	void				*kva;
	dma_addr_t			iova;
	dma_addr_t			iova_ext;
	struct sg_table			*sgt;
	struct sg_table			*sgt_ext;

	const struct pablo_dma_buf_operations	*dbuf_ops;
};

#define vb_to_is_vb2_buf(x)				\
	container_of(x, struct is_vb2_buf, vb)

#define CALL_BUFOP(buf, op, args...)			\
	((buf)->ops->op ? (buf)->ops->op(args) : 0)

#define CALL_PTR_BUFOP(buf, op, args...)		\
	((buf)->ops->op ? (buf)->ops->op(args) : NULL)

#define CALL_VOID_BUFOP(buf, op, args...)	\
	do {					\
		if ((buf)->ops->op)		\
			(buf)->ops->op(args);	\
	} while (0)

#define call_buf_op(buf, op, args...)			\
	((buf)->ops->op ? (buf)->ops->op((buf), args) : 0)

struct is_mem_ops {
	void *(*init)(struct platform_device *pdev);
	void (*cleanup)(void *ctx);
	void (*set_cached)(void *ctx, bool cacheable);
	int (*set_alignment)(void *ctx, size_t alignment);
	struct is_priv_buf *(*alloc)(void *ctx, size_t size, const char *heapname, unsigned int flags);
};

struct is_ion_ctx {
	struct device	*dev;
	unsigned long	alignment;
	long		flags;
	unsigned int	heapmask;
	unsigned int	heapmask_s;

	/* protects iommu_active_cnt and protected */
	struct mutex	lock;
	int		iommu_active_cnt;
};

struct pablo_dmabuf_heap_ctx {
	struct device	*dev;
	struct dma_heap	*dh_system;
	struct dma_heap	*dh_system_uncached;
	struct dma_heap	*dh_secure_camera;
	struct mutex	lock;
};

struct is_mem_stats {
	/* Counters to check for unbalanced IS vb2 buffer operations */
	atomic_t cnt_plane_kmap;
	atomic_t cnt_plane_kunmap;
	atomic_t cnt_plane_dmap;
	atomic_t cnt_plane_dunmap;
	atomic_t cnt_buf_remap;
	atomic_t cnt_buf_unremap;
	atomic_t cnt_dbuf_prepare;
	atomic_t cnt_dbuf_finish;
	atomic_t cnt_dbuf_map;
	atomic_t cnt_dbuf_unmap;
	atomic_t cnt_dbuf_kmap;
	atomic_t cnt_dbuf_kunmap;
	atomic_t cnt_subbuf_prepare;
	atomic_t cnt_subbuf_finish;
	atomic_t cnt_subbuf_kvmap;
	atomic_t cnt_subbuf_kunmap;
};

struct is_mem {
	struct device			*dev;
	struct is_mem_stats		*stats;

	const struct vb2_mem_ops			*vb2_mem_ops;
	const struct is_vb2_buf_ops			*is_vb2_buf_ops;
	const struct pablo_dma_buf_operations		*dbuf_ops;
	const struct pablo_dmabuf_container_operations	*dbuf_con_ops;

	const struct is_mem_ops		*is_mem_ops;
	void				*priv;

	struct is_priv_buf *(*contig_alloc)(size_t size);
};

#define CALL_MEMOP(mem, op, args...)			\
	((mem)->is_mem_ops->op ?			\
		(mem)->is_mem_ops->op(args) : 0)

#define CALL_PTR_MEMOP(mem, op, args...)		\
	((mem)->is_mem_ops->op ?			\
		(mem)->is_mem_ops->op(args) : NULL)

#define CALL_VOID_MEMOP(mem, op, args...)		\
	do {						\
		if ((mem)->is_mem_ops->op)		\
			(mem)->is_mem_ops->op(args);	\
	} while (0)

struct is_minfo {
	struct is_priv_buf *pb_cal[SENSOR_POSITION_MAX];
	struct is_priv_buf *pb_debug;
	struct is_priv_buf *pb_event;
	struct is_priv_buf *pb_pregion;
	struct is_priv_buf *pb_heap_rta; /* RTA HEAP */
	struct is_priv_buf *pb_heap_ddk; /* DDK HEAP */
	struct is_priv_buf *pb_taaisp;
	struct is_priv_buf *pb_taaisp_s;	/* secure */
	struct is_priv_buf *pb_tnr;
	struct is_priv_buf *pb_tnr_s;	/* secure */
	struct is_priv_buf *pb_orbmch;
	struct is_priv_buf *pb_sfr_dump_addr;
	struct is_priv_buf *pb_sfr_dump_value;

	size_t		total_size;
	ulong		kvaddr_debug_cnt;
	ulong		kvaddr_event_cnt;

	phys_addr_t	phaddr_debug;
	ulong		kvaddr_debug;
	phys_addr_t	phaddr_event;
	ulong		kvaddr_event;
	ulong		kvaddr_sfr_dump_addr;
	ulong		kvaddr_sfr_dump_value;
	ulong		kvaddr_region;

	ulong		kvaddr_cal[SENSOR_POSITION_MAX];

	ulong		kvaddr_heap_ddk;
	ulong		kvaddr_heap_rta;

	/* dummy buffer for internal test */
	struct is_priv_buf *pb_dummy;
	phys_addr_t	phaddr_dummy;
	dma_addr_t	dvaddr_dummy;
	ulong		kvaddr_dummy;

	bool		loaded_setfile[SENSOR_POSITION_MAX];
	bool		pinned_setfile[SENSOR_POSITION_MAX];
	refcount_t	refcount_setfile[SENSOR_POSITION_MAX];
	char		*name_setfile[SENSOR_POSITION_MAX];
};

void is_vfree_atomic(const void *addr);
void is_mem_init_stats(void);
void is_mem_check_stats(struct is_mem *mem);
int is_mem_init(struct is_mem *mem, struct platform_device *pdev);
unsigned int is_ion_query_heapmask(const char *heap_name);
void is_clean_dcache_area(void *kaddr, u32 size);
struct is_minfo *is_get_is_minfo(void);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
const struct kernel_param *pablo_mem_get_kernel_param(void);
#endif

#endif
