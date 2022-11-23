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
#include "is-framemgr.h"
#include "exynos-is-sensor.h"

struct is_vb2_buf;
struct is_vb2_buf_ops {
	ulong (*plane_kvaddr)(struct is_vb2_buf *vbuf, u32 plane);
	dma_addr_t (*plane_dvaddr)(struct is_vb2_buf *vbuf, u32 plane);
	ulong (*plane_kmap)(struct is_vb2_buf *vbuf, u32 plane);
	void (*plane_kunmap)(struct is_vb2_buf *vbuf, u32 plane);
	long (*remap_attr)(struct is_vb2_buf *vbuf, int attr);
	void (*unremap_attr)(struct is_vb2_buf *vbuf, int attr);
	long (*dbufcon_prepare)(struct is_vb2_buf *vbuf, u32 num_planes, struct device *dev);
	void (*dbufcon_finish)(struct is_vb2_buf *vbuf);
	long (*dbufcon_map)(struct is_vb2_buf *vbuf);
	void (*dbufcon_unmap)(struct is_vb2_buf *vbuf);
};

struct is_vb2_buf {
	struct vb2_v4l2_buffer		vb;
	unsigned int			num_merged_dbufs;
	struct dma_buf			*dbuf[IS_MAX_PLANES]; /* normal buffer */
	struct dma_buf			*dbuf_bufcon[IS_MAX_PLANES]; /* buffer container */
	struct dma_buf_attachment	*atch[IS_MAX_PLANES];
	struct sg_table			*sgt[IS_MAX_PLANES];

#ifdef CONFIG_DMA_BUF_CONTAINER
	ulong				kva[IS_MAX_PLANES];
	dma_addr_t			dva[IS_MAX_PLANES];
#else
	ulong				kva[VIDEO_MAX_PLANES];
	dma_addr_t			dva[VIDEO_MAX_PLANES];
#endif
	const struct is_vb2_buf_ops *ops;
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
};

struct is_priv_buf {
	size_t					size;
	size_t					align;
	void					*ctx;
	void					*kvaddr;

	const struct is_priv_buf_ops	*ops;
	void					*priv;
	struct dma_buf				*dma_buf;
	struct dma_buf_attachment		*attachment;
	enum dma_data_direction			direction;
	void					*kva;
	dma_addr_t				iova;
	struct sg_table				*sgt;
};

#define vb_to_is_vb2_buf(x)				\
	container_of(x, struct is_vb2_buf, vb)

#define CALL_BUFOP(buf, op, args...)			\
	((buf)->ops->op ? (buf)->ops->op(args) : 0)

#define CALL_PTR_BUFOP(buf, op, args...)		\
	((buf)->ops->op ? (buf)->ops->op(args) : NULL)

#define CALL_VOID_BUFOP(buf, op, args...)	\
	do {									\
		if ((buf)->ops->op)					\
			(buf)->ops->op(args);			\
	} while (0)

#define call_buf_op(buf, op, args...)			\
	((buf)->ops->op ? (buf)->ops->op((buf), args) : 0)

struct is_mem_ops {
	void *(*init)(struct platform_device *pdev);
	void (*cleanup)(void *ctx);
	int (*resume)(void *ctx);
	void (*suspend)(void *ctx);
	void (*set_cached)(void *ctx, bool cacheable);
	int (*set_alignment)(void *ctx, size_t alignment);
	struct is_priv_buf *(*alloc)(void *ctx, size_t size, const char *heapname, unsigned int flags);
};

struct is_ion_ctx {
	struct device		*dev;
	unsigned long		alignment;
	long			flags;

	/* protects iommu_active_cnt and protected */
	struct mutex		lock;
	int			iommu_active_cnt;
};

struct is_mem {
	struct is_ion_ctx				*default_ctx;
	struct is_ion_ctx				*phcontig_ctx;
	const struct is_mem_ops			*is_mem_ops;
	const struct vb2_mem_ops			*vb2_mem_ops;
	const struct is_vb2_buf_ops		*is_vb2_buf_ops;
	void						*priv;
	struct is_priv_buf *(*kmalloc)(size_t size, size_t align);
};

#define CALL_MEMOP(mem, op, args...)			\
	((mem)->is_mem_ops->op ?				\
		(mem)->is_mem_ops->op(args) : 0)

#define CALL_PTR_MEMOP(mem, op, args...)		\
	((mem)->is_mem_ops->op ?				\
		(mem)->is_mem_ops->op(args) : NULL)

#define CALL_VOID_MEMOP(mem, op, args...)		\
	do {										\
		if ((mem)->is_mem_ops->op)			\
			(mem)->is_mem_ops->op(args);	\
	} while (0)

struct is_minfo {
	struct is_priv_buf *pb_fw;
	struct is_priv_buf *pb_setfile;
	struct is_priv_buf *pb_cal[SENSOR_POSITION_MAX];
	struct is_priv_buf *pb_debug;
	struct is_priv_buf *pb_event;
	struct is_priv_buf *pb_fshared;
	struct is_priv_buf *pb_dregion;
	struct is_priv_buf *pb_pregion;
	struct is_priv_buf *pb_heap_rta; /* RTA HEAP */
	struct is_priv_buf *pb_heap_ddk; /* DDK HEAP */
	struct is_priv_buf *pb_taaisp;
	struct is_priv_buf *pb_medrc;
	struct is_priv_buf *pb_taaisp_s;	/* secure */
	struct is_priv_buf *pb_medrc_s;	/* secure */
	struct is_priv_buf *pb_tnr;
	struct is_priv_buf *pb_lhfd;
	struct is_priv_buf *pb_vra;
	struct is_priv_buf *pb_vra_netarray;
	struct is_priv_buf *pb_tpu;
	struct is_priv_buf *pb_mcsc_dnr;
	struct is_priv_buf *pb_orbmch;
	struct is_priv_buf *pb_clahe;

	size_t		total_size;
	ulong		kvaddr_debug_cnt;
	ulong		kvaddr_event_cnt;

	dma_addr_t	dvaddr;
	ulong		kvaddr;
	dma_addr_t	dvaddr_lib;
	ulong		kvaddr_lib;
	phys_addr_t	phaddr_debug;
	dma_addr_t	dvaddr_debug;
	ulong		kvaddr_debug;
	phys_addr_t	phaddr_event;
	dma_addr_t	dvaddr_event;
	ulong		kvaddr_event;
	dma_addr_t	dvaddr_fshared;
	ulong		kvaddr_fshared;
	dma_addr_t	dvaddr_region;
	ulong		kvaddr_region;

	dma_addr_t	dvaddr_tpu;
	ulong		kvaddr_tpu;
	dma_addr_t	dvaddr_lhfd;	/* FD map buffer region */
	ulong		kvaddr_lhfd;	/* NUM_FD_INTERNAL_BUF = 3 */
	dma_addr_t	dvaddr_vra;
	ulong		kvaddr_vra;
	dma_addr_t	dvaddr_vra_netarray;
	ulong		kvaddr_vra_netarray;
	dma_addr_t	dvaddr_mcsc_dnr;
	ulong		kvaddr_mcsc_dnr;

	ulong		kvaddr_setfile;
	ulong		kvaddr_cal[SENSOR_POSITION_MAX];

	/* dummy buffer for internal test */
	struct is_priv_buf *pb_dummy;
	phys_addr_t	phaddr_dummy;
	dma_addr_t	dvaddr_dummy;
	ulong		kvaddr_dummy;
};

int is_mem_init(struct is_mem *mem, struct platform_device *pdev);
#endif
