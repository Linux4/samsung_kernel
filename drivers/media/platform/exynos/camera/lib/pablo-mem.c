// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#if IS_ENABLED(CONFIG_DMA_SHARED_BUFFER)
#include <linux/dma-buf.h>
#if IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) || IS_ENABLED(CONFIG_DMABUF_CONTAINER)
#include <linux/dma-buf-container.h>
#endif
#endif
#if IS_ENABLED(CONFIG_ION)
#include <linux/ion.h>
#endif
#include <linux/kmemleak.h>
#include <linux/swiotlb.h>
#include <asm/cacheflush.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-dma-sg.h>
#if IS_ENABLED(CONFIG_DMABUF_HEAPS)
#include <linux/dma-heap.h>
#endif
#include <linux/iommu.h>

#include "pablo-mem.h"
#include "pablo-debug.h"
#include "pablo-fpsimd.h"
#include "is-video.h"

static struct is_minfo minfo;

struct is_vfree_deferred {
	struct llist_head list;
	struct work_struct wq;
};
static struct is_vfree_deferred __percpu *is_vfree_deferred;

#if defined(USE_CAMERA_HEAP)
static int rmem_allocated = 0;
#endif

#define CALL_DMABUF_OPS(buf, op, args...) ((buf)->dbuf_ops->op ? (buf)->dbuf_ops->op(args) : 0)
#define CALL_DMABUF_CON_OPS(buf, op, args...) ((buf)->dbuf_con_ops->op ? (buf)->dbuf_con_ops->op(args) : 0)

const struct pablo_dma_buf_operations pdb_ops = {
	.attach = dma_buf_attach,
	.detach = dma_buf_detach,
	.get = dma_buf_get,
	.put = dma_buf_put,
	.map_attachment = dma_buf_map_attachment,
	.unmap_attachment = dma_buf_unmap_attachment,
	.begin_cpu_access = dma_buf_begin_cpu_access,
	.end_cpu_access = dma_buf_end_cpu_access,
	.vmap = pkv_dma_buf_vmap,
	.vunmap = pkv_dma_buf_vunmap,
	.get_dma_buf = get_dma_buf,
};

#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#define IS_IOMMU_PROP	(IOMMU_READ | IOMMU_WRITE)

struct vb2_dma_sg_buf {
	struct device			*dev;
	void				*vaddr;
	struct page			**pages;
	struct frame_vector		*vec;
	int				offset;
	enum dma_data_direction		dma_dir;
	struct sg_table			sg_table;
	/*
	 * This will point to sg_table when used with the MMAP or USERPTR
	 * memory model, and to the dma_buf sglist when used with the
	 * DMABUF memory model.
	 */
	struct sg_table			*dma_sgt;
	size_t				size;
	unsigned int			num_pages;
	refcount_t			refcount;
	struct vb2_vmarea_handler	handler;

	struct dma_buf_attachment	*db_attach;
	/*
	 * Our IO address space is not managed by dma-mapping. Therefore
	 * scatterlist.dma_address should not be corrupted by the IO address
	 * returned by iovmm_map() because it is used by cache maintenance.
	 */
	dma_addr_t			iova;
};

/* IS memory statistics */
static struct is_mem_stats stats;
static atomic_t mem_stats_sz_active = ATOMIC_INIT(0);

static int param_set_mem_stats_active(const char *val, const struct kernel_param *kp)
{
	int res;

	if (!kstrtoint(val, 0, &res))
		atomic_set(&mem_stats_sz_active, res);

	return 0;
}

static int param_get_mem_stats_active(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%d\n", atomic_read(&mem_stats_sz_active));
}

static const struct kernel_param_ops param_ops_mem_stats_active = {
	.set = param_set_mem_stats_active,
	.get = param_get_mem_stats_active,
};

module_param_cb(mem_stats_active, &param_ops_mem_stats_active, NULL, 0644);

static void is_free_work(struct work_struct *w)
{
	struct is_vfree_deferred *p = container_of(w, struct is_vfree_deferred, wq);
	struct llist_node *t, *llnode;

	llist_for_each_safe(llnode, t, llist_del_all(&p->list))
		vfree((void *)llnode);
}

/* is vb2 buffer operations */
static inline ulong is_vb2_dma_sg_plane_kvaddr(
		struct is_vb2_buf *vbuf, u32 plane)

{
	return (ulong)vb2_plane_vaddr(&vbuf->vb.vb2_buf, plane);
}

static inline dma_addr_t is_vb2_dma_sg_plane_dvaddr(
		struct is_vb2_buf *vbuf, u32 plane)

{
	struct sg_table *sgt = vb2_dma_sg_plane_desc(&vbuf->vb.vb2_buf, plane);
	return sg_dma_address(sgt->sgl);
}

static inline ulong is_vb2_dma_sg_plane_kmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = vb->planes[plane].mem_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* Is a non-image plane? */
		if (plane >= num_i_planes)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (likely(vbuf->kva[adj_plane]))
		return vbuf->kva[adj_plane];

	/* Use a dma_buf extracted from the DMA buffer container or original one. */
	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs && (plane < num_i_planes))
		dbuf = vbuf->dbuf[adj_plane];
	else
		dbuf = CALL_DMABUF_OPS(vbuf, get, vb->planes[plane].m.fd);

	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("failed to get dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	if (CALL_DMABUF_OPS(vbuf, begin_cpu_access, dbuf, buf->dma_dir)) {
		CALL_DMABUF_OPS(vbuf, put, dbuf);
		pr_err("failed to access dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	vbuf->kva[adj_plane] = (ulong)CALL_DMABUF_OPS(vbuf, vmap, dbuf) + buf->offset;
	if (!vbuf->kva[adj_plane]) {
		CALL_DMABUF_OPS(vbuf, end_cpu_access, dbuf, buf->dma_dir);
		CALL_DMABUF_OPS(vbuf, put, dbuf);
		pr_err("failed to kmapping dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	/* Save the dma_buf if there is no saved one. */
	if (!vbuf->dbuf[adj_plane])
		vbuf->dbuf[adj_plane] = dbuf;
	vbuf->kva[adj_plane] += buf->offset & ~PAGE_MASK;

	atomic_inc(&stats.cnt_plane_kmap);

	return vbuf->kva[adj_plane];
}

static inline void is_vb2_dma_sg_plane_kunmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		if (plane >= num_i_planes)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (vbuf->kva[adj_plane]) {
		dbuf = vbuf->dbuf[adj_plane];

		CALL_DMABUF_OPS(vbuf, vunmap, dbuf, (void *)vbuf->kva[adj_plane]);
		CALL_DMABUF_OPS(vbuf, end_cpu_access, dbuf, 0);

		CALL_DMABUF_OPS(vbuf, put, dbuf);
		vbuf->dbuf[adj_plane] = NULL;
		vbuf->kva[adj_plane] = 0;

		atomic_inc(&stats.cnt_plane_kunmap);
	}
}

static inline dma_addr_t is_vb2_dma_sg_plane_dmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx, struct device *dev_ext)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (!dev_ext)
		return 0;

	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* Is a non-image plane? */
		if (plane >= num_i_planes)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (vbuf->dva_ext[adj_plane])
		return vbuf->dva_ext[adj_plane];

	dbuf = CALL_DMABUF_OPS(vbuf, get, vb->planes[plane].m.fd);

	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("failed to get dmabuf of fd: %d, plane: %d\n",
				vb->planes[plane].m.fd, plane);
		return 0;
	}

	/* attach dma for extra dva */
	vbuf->atch_ext[adj_plane] = CALL_DMABUF_OPS(vbuf, attach, dbuf, dev_ext);
	if (IS_ERR(vbuf->atch_ext[adj_plane])) {
		pr_err("failed to dma_buf_attach.\n");
		goto err_attach;
	}

	vbuf->sgt_ext[adj_plane] = CALL_DMABUF_OPS(vbuf, map_attachment, vbuf->atch_ext[adj_plane], DMA_BIDIRECTIONAL);
	if (IS_ERR(vbuf->sgt_ext[adj_plane])) {
		pr_err("failed to dma_buf_map_attachment.\n");
		goto err_map_dmabuf;
	}

	vbuf->dva_ext[adj_plane] = sg_dma_address(vbuf->sgt_ext[adj_plane]->sgl);
	if (IS_ERR_VALUE(vbuf->dva_ext[adj_plane])) {
		pr_err("failed to sg_dma_address.\n");
		goto err_ion_map_io;
	}

	vbuf->dbuf_ext[adj_plane] = dbuf;

	atomic_inc(&stats.cnt_plane_dmap);

	return vbuf->dva_ext[adj_plane];

err_ion_map_io:
	CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch_ext[adj_plane], vbuf->sgt_ext[adj_plane], DMA_BIDIRECTIONAL);
	vbuf->sgt_ext[adj_plane] = NULL;
err_map_dmabuf:
	CALL_DMABUF_OPS(vbuf, detach, dbuf, vbuf->atch_ext[adj_plane]);
	vbuf->atch_ext[adj_plane] = NULL;
err_attach:
	CALL_DMABUF_OPS(vbuf, put, dbuf);
	pr_err("%s: Error occurred while attach.\n", __func__);

	return 0;
}

static inline void is_vb2_dma_sg_plane_dunmap(
		struct is_vb2_buf *vbuf, u32 plane, u32 buf_idx)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbuf;
	u32 adj_plane = plane;

	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		if (plane >= num_i_planes)
			adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
				+ (plane - num_i_planes);
		else
			adj_plane = (buf_idx * num_i_planes) + plane;
	}

	if (vbuf->dva_ext[adj_plane]) {
		dbuf = vbuf->dbuf_ext[adj_plane];

		CALL_DMABUF_OPS(vbuf, unmap_attachment,
				vbuf->atch_ext[adj_plane], vbuf->sgt_ext[adj_plane], DMA_BIDIRECTIONAL);
		vbuf->sgt_ext[adj_plane] = NULL;
		CALL_DMABUF_OPS(vbuf, detach, dbuf, vbuf->atch_ext[adj_plane]);
		vbuf->atch_ext[adj_plane] = NULL;
		vbuf->dva_ext[adj_plane] = 0;

		CALL_DMABUF_OPS(vbuf, put, dbuf);

		atomic_inc(&stats.cnt_plane_dunmap);
	}
}

static inline void is_vb2_dma_sg_plane_sync_for_device(struct is_vb2_buf *vbuf)
{
	struct dma_buf *dbuf;
	u32 i, max_plane;

	max_plane = ARRAY_SIZE(vbuf->dva_ext);
	for (i = 0; i < max_plane; i++) {
		if (vbuf->dva_ext[i]) {
			dbuf = vbuf->dbuf_ext[i];
			CALL_DMABUF_OPS(vbuf, end_cpu_access, dbuf, DMA_BIDIRECTIONAL);
			CALL_DMABUF_OPS(vbuf, begin_cpu_access, dbuf, DMA_FROM_DEVICE);
		}
	}
}

static long is_vb2_dma_sg_remap_attr(struct is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf;
	unsigned int plane;
	long ret;
	int ioprot = IS_IOMMU_PROP;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;
		vbuf->sgt[plane] = CALL_DMABUF_OPS(vbuf, map_attachment, buf->db_attach, buf->dma_dir);

		if (IS_ERR(vbuf->sgt[plane])) {
			ret = PTR_ERR(vbuf->sgt[plane]);
			pr_err("Error getting dmabuf scatterlist\n");
			goto err_get_sgt;
		}

		if ((vbuf->dva[plane] == 0) || IS_ERR_VALUE(vbuf->dva[plane])) {
			if (device_get_dma_attr(buf->dev) == DEV_DMA_COHERENT)
				ioprot |= IOMMU_CACHE;

			vbuf->dva[plane] = sg_dma_address(vbuf->sgt[plane]->sgl);
			if (IS_ERR_VALUE(vbuf->dva[plane])) {
				ret = vbuf->dva[plane];
				pr_err("Error from sg_dma_address(): %ld\n", ret);
				goto err_map_remap;
			}
		}
	}

	atomic_inc(&stats.cnt_buf_remap);

	return 0;

err_map_remap:
	CALL_DMABUF_OPS(vbuf, unmap_attachment, buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
	vbuf->dva[plane] = 0;

err_get_sgt:
	while (plane-- > 0) {
		buf = vb->planes[plane].mem_priv;

		CALL_DMABUF_OPS(vbuf, unmap_attachment, buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
		vbuf->dva[plane] = 0;
	}

	return ret;
}

void is_vb2_dma_sg_unremap_attr(struct is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf;
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;

		CALL_DMABUF_OPS(vbuf, unmap_attachment, buf->db_attach, vbuf->sgt[plane], buf->dma_dir);
		vbuf->dva[plane] = 0;
	}

	atomic_inc(&stats.cnt_buf_unremap);
}

#if IS_ENABLED(DMABUF_CONTAINER)
static long is_dbufcon_prepare(struct is_vb2_buf *vbuf, struct device *dev)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbufcon;
	int count;
	int p, b, i;
	long ret;

	for (p = 0; p < num_i_planes; p++) {
		dbufcon = CALL_DMABUF_OPS(vbuf, get, vb->planes[p].m.fd);
		if (IS_ERR_OR_NULL(dbufcon)) {
			err("failed to get dmabuf of fd: %d, plane: %d",
				vb->planes[p].m.fd, p);
			ret = -EINVAL;
			goto err_get_dbufcon;
		}

		count = CALL_DMABUF_CON_OPS(vbuf, get_count, dbufcon);
		if (count < 0) {
			ret = 0;
			goto err_get_count;
		} else if (count == 0) {
			err("empty dmabuf-container of fd: %d", vb->planes[p].m.fd);
			ret = -EINVAL;
			goto err_empty_dbufcon;
		}

		/* IS_MAX_PLANES includes meta-planes */
		if (((count * num_i_planes) + NUM_OF_META_PLANE)
			> IS_MAX_PLANES) {
			err("out of range plane. require: %d > supported: %d",
				(count * num_i_planes) + NUM_OF_META_PLANE,
				IS_MAX_PLANES);
			ret = -EINVAL;
			goto err_too_many_planes;

		}

		/* change the order of each plane by the order of buffer */
		for (b = 0, i = p; b < count; b++, i = (b * num_i_planes) + p) {
			vbuf->dbuf[i] = CALL_DMABUF_CON_OPS(vbuf, get_buffer, dbufcon, b);
			if (IS_ERR_OR_NULL(vbuf->dbuf[i])) {
				err("failed to get dmabuf-container's dmabuf: %d", b);
				ret = -EINVAL;
				goto err_get_dbuf;
			}

			vbuf->atch[i] = CALL_DMABUF_OPS(vbuf, attach, vbuf->dbuf[i], dev);
			if (IS_ERR(vbuf->atch[i])) {
				err("failed to attach dmabuf: %d", b);
				ret = PTR_ERR(vbuf->atch[i]);
				goto err_attach_dbuf;
			}

			vbuf->sgt[i] = CALL_DMABUF_OPS(vbuf, map_attachment, vbuf->atch[i], DMA_BIDIRECTIONAL);
			if (IS_ERR(vbuf->sgt[i])) {
				err("failed to get dmabuf's sgt: %d", b);
				ret = PTR_ERR(vbuf->sgt[i]);
				goto err_get_sgt;
			}
		}

		CALL_DMABUF_OPS(vbuf, put, dbufcon);
	}

	vbuf->num_merged_dbufs = count;

	atomic_inc(&stats.cnt_dbuf_prepare);

	return 0;

err_get_sgt:
	CALL_DMABUF_OPS(vbuf, detach, vbuf->dbuf[i], vbuf->atch[i]);
	vbuf->sgt[i] = NULL;

err_attach_dbuf:
	CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[i]);
	vbuf->atch[i] = NULL;
	vbuf->dbuf[i] = NULL;

err_get_dbuf:
	while (b-- > 0) {
		i = (b * num_i_planes) + p;
		CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch[i], vbuf->sgt[i], DMA_BIDIRECTIONAL);
		CALL_DMABUF_OPS(vbuf, detach, vbuf->dbuf[i], vbuf->atch[i]);
		CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[i]);
		vbuf->sgt[i] = NULL;
		vbuf->atch[i] = NULL;
		vbuf->dbuf[i] = NULL;
	}

err_too_many_planes:
err_empty_dbufcon:
err_get_count:
	CALL_DMABUF_OPS(vbuf, put, dbufcon);

err_get_dbufcon:
	vbuf->num_merged_dbufs = 0;

	while (p-- > 0) {
		dbufcon = CALL_DMABUF_OPS(vbuf, get, vb->planes[p].m.fd);
		count = CALL_DMABUF_CON_OPS(vbuf, get_count, dbufcon);
		CALL_DMABUF_OPS(vbuf, put, dbufcon);

		for (b = 0, i = p; b < count; b++, i = (b * num_i_planes) + p) {
			CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch[i], vbuf->sgt[i], DMA_BIDIRECTIONAL);
			CALL_DMABUF_OPS(vbuf, detach, vbuf->dbuf[i], vbuf->atch[i]);
			CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[i]);
			vbuf->sgt[i] = NULL;
			vbuf->atch[i] = NULL;
			vbuf->dbuf[i] = NULL;
		}
	}

	return ret;
}

static void is_dbufcon_finish(struct is_vb2_buf *vbuf)
{
	int i;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch[i], vbuf->sgt[i], DMA_BIDIRECTIONAL);
		CALL_DMABUF_OPS(vbuf, detach, vbuf->dbuf[i], vbuf->atch[i]);
		CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[i]);
		vbuf->sgt[i] = NULL;
		vbuf->atch[i] = NULL;
		vbuf->dbuf[i] = NULL;

	}

	atomic_inc(&stats.cnt_dbuf_finish);
}

static long is_dbufcon_map(struct is_vb2_buf *vbuf)
{
	int i;
	long ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		vbuf->sgt[i] = CALL_DMABUF_OPS(vbuf, map_attachment, vbuf->atch[i], DMA_BIDIRECTIONAL);
		if (IS_ERR(vbuf->sgt[i])) {
			ret = PTR_ERR(vbuf->atch[i]);
			pr_err("Error getting dmabuf scatterlist\n");
			goto err_map_dbuf;
		}
		vbuf->dva[i] = sg_dma_address(vbuf->sgt[i]->sgl);
		if (IS_ERR_VALUE(vbuf->dva[i])) {
			err("failed to map dmabuf: %d", i);
			ret = vbuf->dva[i];
			goto err_map_dbuf;
		}
	}

	atomic_inc(&stats.cnt_dbuf_map);

	return 0;

err_map_dbuf:
	vbuf->dva[i] = 0;

	while (i-- > 0) {
		CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch[i], vbuf->sgt[i], DMA_BIDIRECTIONAL);
		vbuf->dva[i] = 0;
	}

	return ret;
}

static void is_dbufcon_unmap(struct is_vb2_buf *vbuf)
{
	int i;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vbuf->num_merged_dbufs
		* (vb->num_planes - NUM_OF_META_PLANE);

	for (i = 0; i < num_i_planes; i++) {
		CALL_DMABUF_OPS(vbuf, unmap_attachment, vbuf->atch[i], vbuf->sgt[i], DMA_BIDIRECTIONAL);
		vbuf->dva[i] = 0;
	}

	atomic_inc(&stats.cnt_dbuf_unmap);
}

static int is_dbufcon_kmap(struct is_vb2_buf *vbuf, u32 plane)
{
	int ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	u32 num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct dma_buf *dbufcon = NULL;
	u32 adj_plane = plane;
	u32 num_planes, b, p;
	int count = 0, total_planes;
	bool is_dbuf = (plane < num_i_planes);

	if (is_dbuf) {
		/* DMA buffers */
		num_planes = num_i_planes;
		count = vbuf->num_merged_dbufs;
	} else {
		/* Shot buffers */
		adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
			+ (plane - num_i_planes);
		num_planes = 1;
		count = vbuf->num_merged_sbufs;
	}

	dbufcon = CALL_DMABUF_OPS(vbuf, get, vb->planes[plane].m.fd);
	if (IS_ERR_OR_NULL(dbufcon)) {
		err("failed to get dmabuf of fd: %d, plane: %d",
			vb->planes[plane].m.fd, plane);
		ret = -EINVAL;
		goto err_get_dbufcon;
	}

	if (count) {
		if (is_dbuf)
			goto skip_dbufcon_release;
		else
			return 0;
	}

	count = CALL_DMABUF_CON_OPS(vbuf, get_count, dbufcon);
	if (count < 0) {
		ret = 0;
		goto err_get_count;
	} else if (count == 0) {
		err("empty dmabuf-container of fd: %d",
			vb->planes[plane].m.fd);
		ret = -EINVAL;
		goto err_empty_dbufcon;
	}

	/* Check the plane count limitation. */
	if (!is_dbuf) {
		total_planes = (num_i_planes * vbuf->num_merged_dbufs) + count;
		if (total_planes > IS_MAX_PLANES) {
			err("out of range plane. require: %d > supported: %d",
					total_planes, IS_MAX_PLANES);
			ret = -EINVAL;
			goto err_too_many_planes;

		}
	}

skip_dbufcon_release:
	for (b = 0, p = adj_plane; b < count; b++, p += num_planes) {
		/* Increase dma_buf reference count */
		if (vbuf->dbuf[p])
			CALL_DMABUF_OPS(vbuf, get_dma_buf, vbuf->dbuf[p]);
		else
			vbuf->dbuf[p] = CALL_DMABUF_CON_OPS(vbuf, get_buffer, dbufcon, b);
		if (IS_ERR_OR_NULL(vbuf->dbuf[p])) {
			err("failed to get dmabuf-container's dmabuf: %d", b);
			ret = -EINVAL;
			goto err_get_dbuf;
		}


		vbuf->kva[p] = (ulong)CALL_DMABUF_OPS(vbuf, vmap, vbuf->dbuf[p]);
		if (IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			err("[P%d]Failed to get kva %ld",
				p, vbuf->kva[p]);
			ret = -ENOMEM;
			goto err_kmap;
		}
	}

	if (dbufcon)
		CALL_DMABUF_OPS(vbuf, put, dbufcon);

	if (!is_dbuf)
		vbuf->num_merged_sbufs = count;

	atomic_inc(&stats.cnt_dbuf_kmap);

	return 0;

err_kmap:
err_get_dbuf:
	for (b = 0, p = adj_plane; b < count; b++, p += num_planes) {
		if (!IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			CALL_DMABUF_OPS(vbuf, vunmap, vbuf->dbuf[p], (void *)vbuf->kva[p]);
			vbuf->kva[p] = 0;
		}

		if (!IS_ERR_OR_NULL(vbuf->dbuf[p]))
			CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[p]);
	}

err_too_many_planes:
err_empty_dbufcon:
err_get_count:
	if (dbufcon)
		CALL_DMABUF_OPS(vbuf, put, dbufcon);

err_get_dbufcon:
	if (!is_dbuf)
		vbuf->num_merged_sbufs = 0;

	return ret;
}

static void is_dbufcon_kunmap(struct is_vb2_buf *vbuf, u32 plane)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	u32 adj_plane = plane;
	u32 num_planes, num_merged, b, p;
	bool is_dbuf = (plane < num_i_planes);

	if (is_dbuf) {
		/* DMA buffers */
		num_planes = num_i_planes;
		num_merged = vbuf->num_merged_dbufs;
	} else {
		/* Shot buffers */
		adj_plane = (vbuf->num_merged_dbufs * num_i_planes)
			+ (plane - num_i_planes);
		num_planes = 1;
		num_merged = vbuf->num_merged_sbufs;
	}

	for (b = 0, p = adj_plane; b < num_merged; b++, p += num_planes) {
		if (!IS_ERR_OR_NULL((void *) vbuf->kva[p])) {
			CALL_DMABUF_OPS(vbuf, vunmap, vbuf->dbuf[p], (void *)vbuf->kva[p]);
			vbuf->kva[p] = 0;
		}

		if (!IS_ERR_OR_NULL(vbuf->dbuf[p]))
			CALL_DMABUF_OPS(vbuf, put, vbuf->dbuf[p]);
	}

	if (!is_dbuf)
		vbuf->num_merged_sbufs = 0;

	atomic_inc(&stats.cnt_dbuf_kunmap);
}

int is_dmabuf_container_get_count(struct dma_buf *dmabuf)
{
	return dmabuf_container_get_count(dmabuf);
}

struct dma_buf *is_dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index)
{
	return dmabuf_container_get_buffer(dmabuf, index);
}
#else
static long is_dbufcon_prepare(struct is_vb2_buf *vbuf, struct device *dev)
{
	return 0;
}

static void is_dbufcon_finish(struct is_vb2_buf *vbuf)
{
}

static long is_dbufcon_map(struct is_vb2_buf *vbuf)
{
	return 0;
}

static void is_dbufcon_unmap(struct is_vb2_buf *vbuf)
{
}

static int is_dbufcon_kmap(struct is_vb2_buf *vbuf, u32 plane)
{
	return 0;
}

static void is_dbufcon_kunmap(struct is_vb2_buf *vbuf, u32 plane)
{
}

int is_dmabuf_container_get_count(struct dma_buf *dmabuf)
{
	return -1;
}

struct dma_buf *is_dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index)
{
	return NULL;
}
#endif /* DMABUF_CONTAINER */

void is_subbuf_prepare(struct is_sub_dma_buf *buf, struct v4l2_plane *plane,
		struct device *dev)
{
	struct dma_buf *dbuf;
	int cnt = 0;
	u32 num_p = buf->num_plane;
	u32 p, b, i;
	bool is_dbufcon = true;
	u32 retry = 0, retry_max = 3;

	for (p = 0; p < num_p; p++) {
		dbuf = CALL_DMABUF_OPS(buf, get, plane[p].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			err("[V%d][P%d]Failed to get dmabuf. fd %d ret %ld",
				buf->vid, p, plane[p].m.fd, PTR_ERR(dbuf));
			goto err_get_dbufcon;
		}

		cnt = CALL_DMABUF_CON_OPS(buf, get_count, dbuf);
		if (cnt < 0) {
			is_dbufcon = false;
			cnt = 1;
		} else if (cnt == 0) {
			err("[V%d][P%d]Empty dma_buf_container. fd %d",
				buf->vid, p, plane[p].m.fd);
			goto err_get_dbuf;
		}

		if ((cnt * num_p) > IS_MAX_PLANES) {
			err("[V%d][P%d]Out of range plane. required %d > supported %d",
				buf->vid, p,
				(cnt * num_p), IS_MAX_PLANES);
			goto err_get_dbuf;
		}

		for (b = 0, i = p; b < cnt; b++, i = (num_p * b) + p) {
			if (IS_ENABLED(DMABUF_CONTAINER) && is_dbufcon)
				buf->dbuf[i] = CALL_DMABUF_CON_OPS(buf, get_buffer, dbuf, b);
			else
				buf->dbuf[i] = CALL_DMABUF_OPS(buf, get, plane[i].m.fd);

			if (IS_ERR_OR_NULL(buf->dbuf[i])) {
				err("[V%d][P%d]Failed to get dmabuf. fd %d ret %ld",
					buf->vid, i,
					plane[i].m.fd, PTR_ERR(buf->dbuf[i]));
				goto err_get_dbuf;
			}

			buf->atch[i] = CALL_DMABUF_OPS(buf, attach, buf->dbuf[i], dev);
			if (IS_ERR(buf->atch[i])) {
				err("[V%d][P%d]Failed to attach dmabuf. ret %ld",
					buf->vid, i, PTR_ERR(buf->atch[i]));
				goto err_get_dbuf;
			}

			do {
				buf->sgt[i] = CALL_DMABUF_OPS(buf, map_attachment,buf->atch[i], DMA_BIDIRECTIONAL);
				if (IS_ERR(buf->sgt[i])) {
					err("[V%d][P%d]Failed to get sgt. ret %ld (retry:%d)",
						buf->vid, i,  PTR_ERR(buf->sgt[i]), retry);
					retry++;
					if (retry >= retry_max)
						goto err_get_dbuf;

					usleep_range(2000, 2001);
				}

			} while (IS_ERR(buf->sgt[i]));

			buf->size[i] = plane[p].length;
			buf->dva[i] = 0;
			buf->kva[i] = 0;
		}

		CALL_DMABUF_OPS(buf, put, dbuf);
	}

	buf->num_buffer = cnt;

	atomic_inc(&stats.cnt_subbuf_prepare);

	return;

err_get_dbuf:
	CALL_DMABUF_OPS(buf, put, dbuf);

err_get_dbufcon:
	buf->num_buffer = 0;

	while (p-- > 0) {
		for (b = 0, i = p; b < cnt; b++, i = (b * num_p) + p) {
			CALL_DMABUF_OPS(buf, unmap_attachment, buf->atch[i], buf->sgt[i], DMA_BIDIRECTIONAL);
			CALL_DMABUF_OPS(buf, detach, buf->dbuf[i], buf->atch[i]);
			CALL_DMABUF_OPS(buf, put, buf->dbuf[i]);
			buf->sgt[i] = NULL;
			buf->atch[i] = NULL;
			buf->dbuf[i] = NULL;
		}
	}
}

long is_subbuf_dvmap(struct is_sub_dma_buf *buf)
{
	long ret;
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes && p < IS_MAX_PLANES; p++) {
		if (!buf->sgt[p]) {
			err("[N%d][P%d]sgt is NULL. fd %d",
				buf->vid, p,
				buf->buf_fd[p]);
			/* critical error!! will be die */
			return -ENOMEM;
		}

		buf->dva[p] = sg_dma_address(buf->sgt[p]->sgl);
		if (IS_ERR_VALUE(buf->dva[p])) {
			err("[N%d][P%d]Failed to get dva",
				buf->vid, p);
			ret = buf->dva[p];
			goto err_map_dbuf;
		}
	}

	return 0;

err_map_dbuf:
	while (p-- > 0) {
		CALL_DMABUF_OPS(buf, unmap_attachment, buf->atch[p], buf->sgt[p], DMA_BIDIRECTIONAL);
		buf->dva[p] = 0;
	}

	return ret;
}

ulong is_subbuf_kvmap(struct is_sub_dma_buf *buf)
{
	long ret;
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes && p < IS_MAX_PLANES; p++) {
		buf->kva[p] = (ulong)CALL_DMABUF_OPS(buf, vmap, buf->dbuf[p]);
		if (IS_ERR_OR_NULL((void *)buf->kva[p])) {
			err("[N%d][P%d]Failed to get kva",
				buf->vid, p);
			ret = buf->kva[p];
			goto err_vmap;
		}
	}

	atomic_inc(&stats.cnt_subbuf_kvmap);

	return 0;

err_vmap:
	while (p-- > 0) {
		CALL_DMABUF_OPS(buf, vunmap, buf->dbuf[p], (void *)buf->kva[p]);
		buf->kva[p] = 0;
	}

	return ret;
}

void is_subbuf_kunmap(struct is_sub_dma_buf *buf)
{
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes && p < IS_MAX_PLANES; p++) {
		if (buf->kva[p]) {
			CALL_DMABUF_OPS(buf, vunmap, buf->dbuf[p], (void *)buf->kva[p]);
			buf->kva[p] = 0;
		}
	}

	atomic_inc(&stats.cnt_subbuf_kunmap);
}

void is_subbuf_finish(struct is_sub_dma_buf *buf)
{
	u32 num_planes = buf->num_plane * buf->num_buffer;
	u32 p;

	for (p = 0; p < num_planes && p < IS_MAX_PLANES; p++) {
		CALL_DMABUF_OPS(buf, unmap_attachment, buf->atch[p], buf->sgt[p], DMA_BIDIRECTIONAL);
		CALL_DMABUF_OPS(buf, detach, buf->dbuf[p], buf->atch[p]);
		CALL_DMABUF_OPS(buf, put, buf->dbuf[p]);

		/* clear */
		buf->sgt[p] = NULL;
		buf->atch[p] = NULL;
		buf->dbuf[p] = NULL;
		buf->dva[p] = 0;
		buf->buf_fd[p] = 0;
	}

	buf->num_plane = 0;
	buf->num_buffer = 0;
	buf->vid = 0;

	atomic_inc(&stats.cnt_subbuf_finish);
}

const struct is_vb2_buf_ops is_vb2_buf_ops_dma_sg = {
	.plane_kvaddr		= is_vb2_dma_sg_plane_kvaddr,
	.plane_dvaddr		= is_vb2_dma_sg_plane_dvaddr,
	.plane_kmap		= is_vb2_dma_sg_plane_kmap,
	.plane_kunmap		= is_vb2_dma_sg_plane_kunmap,
	.plane_dmap		= is_vb2_dma_sg_plane_dmap,
	.plane_dunmap		= is_vb2_dma_sg_plane_dunmap,
	.plane_sync_for_device	= is_vb2_dma_sg_plane_sync_for_device,
	.remap_attr		= is_vb2_dma_sg_remap_attr,
	.unremap_attr		= is_vb2_dma_sg_unremap_attr,
	.dbufcon_prepare	= is_dbufcon_prepare,
	.dbufcon_finish		= is_dbufcon_finish,
	.dbufcon_map		= is_dbufcon_map,
	.dbufcon_unmap		= is_dbufcon_unmap,
	.dbufcon_kmap		= is_dbufcon_kmap,
	.dbufcon_kunmap		= is_dbufcon_kunmap,
	.subbuf_prepare		= is_subbuf_prepare,
	.subbuf_dvmap		= is_subbuf_dvmap,
	.subbuf_kvmap		= is_subbuf_kvmap,
	.subbuf_kunmap		= is_subbuf_kunmap,
	.subbuf_finish		= is_subbuf_finish
};
#endif /* CONFIG_VIDEOBUF2_DMA_SG */

#if IS_ENABLED(CONFIG_DMA_SHARED_BUFFER)
static int pablo_dmabuf_heap_attach_ext(struct is_priv_buf *pbuf, struct device *dev)
{
	int ret;

	pbuf->attachment_ext = CALL_DMABUF_OPS(pbuf, attach, pbuf->dma_buf, dev);
	if (IS_ERR(pbuf->attachment_ext)) {
		ret = PTR_ERR(pbuf->attachment_ext);
		goto err_attach;
	}

	pbuf->sgt_ext = CALL_DMABUF_OPS(pbuf, map_attachment, pbuf->attachment_ext, DMA_BIDIRECTIONAL);
	if (IS_ERR(pbuf->sgt_ext)) {
		ret = PTR_ERR(pbuf->sgt_ext);
		goto err_map_dmabuf;
	}

	pbuf->iova_ext = sg_dma_address(pbuf->sgt_ext->sgl);
	if (IS_ERR_VALUE(pbuf->iova_ext)) {
		ret = (int)pbuf->iova_ext;
		goto err_ion_map_io;
	}

	dbg_mem(1, "%s: dev(%p)\n", __func__, dev);

	return 0;

err_ion_map_io:
	CALL_DMABUF_OPS(pbuf, unmap_attachment, pbuf->attachment_ext, pbuf->sgt_ext, DMA_BIDIRECTIONAL);
	pbuf->sgt_ext = NULL;
err_map_dmabuf:
	CALL_DMABUF_OPS(pbuf, detach, pbuf->dma_buf, pbuf->attachment_ext);
	pbuf->attachment_ext = NULL;
err_attach:
	pr_err("%s: Error occurred while attach\n", __func__);
	return ret;
}

static void pablo_dmabuf_heap_detach_ext(struct is_priv_buf *pbuf)
{
	if (!pbuf->attachment_ext)
		return;

	CALL_DMABUF_OPS(pbuf, unmap_attachment, pbuf->attachment_ext, pbuf->sgt_ext, DMA_BIDIRECTIONAL);
	pbuf->sgt_ext = NULL;
	CALL_DMABUF_OPS(pbuf, detach, pbuf->dma_buf, pbuf->attachment_ext);
	pbuf->attachment_ext = NULL;

	dbg_mem(1, "%s\n", __func__);
}

static dma_addr_t pablo_mem_dvaddr_ext(struct is_priv_buf *pbuf)
{
	return (dma_addr_t)pbuf->iova_ext;
}

static void pablo_dma_buf_free(struct is_priv_buf *pbuf)
{
#if defined(USE_CAMERA_HEAP)
	if (!strncmp(pbuf->dma_buf->exp_name, CAMERA_HEAP_NAME, CAMERA_HEAP_NAME_LEN)
		|| !strncmp(pbuf->dma_buf->exp_name, CAMERA_HEAP_UNCACHED_NAME, CAMERA_HEAP_UNCACHED_NAME_LEN)) {
		rmem_allocated -= pbuf->size;
		if (rmem_allocated < 0) {
			rmem_allocated = 0;
			warn("Camera Heap: dealloc size higher than alloc size!");
		}
	}
#endif
	if (pbuf->kva)
		CALL_DMABUF_OPS(pbuf, vunmap, pbuf->dma_buf, pbuf->kva);

	CALL_DMABUF_OPS(pbuf, unmap_attachment, pbuf->attachment, pbuf->sgt, DMA_BIDIRECTIONAL);
	CALL_DMABUF_OPS(pbuf, detach, pbuf->dma_buf, pbuf->attachment);
	pablo_dmabuf_heap_detach_ext(pbuf);
	CALL_DMABUF_OPS(pbuf, put, pbuf->dma_buf);

#if defined(USE_CAMERA_HEAP)
	dbg_mem(1, "%s: heap[%d] size(%zu) total[%d]\n",
		__func__, pbuf->size, pbuf->dma_buf->exp_name, rmem_allocated);
#endif
	atomic_sub(pbuf->aligned_size, &mem_stats_sz_active);

	vfree(pbuf);
}

static ulong pablo_dma_buf_kvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	if (!pbuf->kva)
		pbuf->kva = CALL_DMABUF_OPS(pbuf, vmap, pbuf->dma_buf);

	return (ulong)pbuf->kva;
}
#endif /* CONFIG_DMA_SHARED_BUFFER */

static dma_addr_t pablo_mem_dvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return (dma_addr_t)pbuf->iova;
}

/* The physical address can only be supported when using reserved memory. */
static phys_addr_t pablo_mem_phaddr(struct is_priv_buf *pbuf)
{
	return (phys_addr_t)sg_phys(pbuf->sgt->sgl);
}

static void pablo_mem_sync(struct is_priv_buf *pbuf, off_t offset, size_t size,
		struct device *dev, enum dma_data_direction dir, enum dma_sync_target target)
{
	int i;
	struct scatterlist *sg;
	ulong size_offset = 0;
	size_t remained_size = size;
	ulong actual_offset;
	size_t actual_size;

	if (!pbuf->kva)
		return;

	if ((offset < 0) || (offset > pbuf->size)) {
		warn("%s, invalid offset: %ld", __func__, offset);
		return;
	}

	if ((size > pbuf->size) || ((offset + size) > pbuf->size)) {
		warn("%s, invalid size: %zu or offset: %ld", __func__, size, offset);
		return;
	}

	dbg_mem(1, "%s_for_%s begin (dva: %pad, offset: %ld, size: %zu, sg_dma_len:%d)",
			__func__, target == SYNC_FOR_DEVICE ? "device" : "cpu",
			&pbuf->iova, offset, size, sg_dma_len(pbuf->sgt->sgl));

	for_each_sg(pbuf->sgt->sgl, sg, pbuf->sgt->orig_nents, i) {
		dbg_mem(2, "[I:%d] size_offset: %lu, sg->length: %d",
					i, size_offset, sg->length);

		if (size_offset + sg->length >= offset) {
			/* If offset starts inside the sg region,
			 * start region of cache process is offset.
			 * Else, start region of cache process is sg start point
			 */
			actual_offset = (size_offset <= offset) ? offset : size_offset;

			/* Case 1: cache region is in sg region
			 * Case 2: cache region start in sg region, but size is out of region
			 * Case 3: cache region is larger than sg region or same
			 */
			if (offset + size < size_offset + sg->length)
				actual_size = remained_size;
			else if (offset + size >= size_offset + sg->length
					&& actual_offset == offset)
				actual_size = sg->length - (offset - size_offset);
			else
				actual_size = sg->length;

			remained_size -= actual_size;

			if (actual_size > sg->length) {
				warn("%s, actual_size: %zu > sg->length: %d", __func__,
								actual_size, sg->length);
				actual_offset = size_offset;
				actual_size = sg->length;
			}

			dbg_mem(2, "[I:%d] do cache ops(actual_offset: %lu, actual_size :%zu)",
					i, actual_offset, actual_size);

			if (target == SYNC_FOR_DEVICE)
				dma_sync_single_for_device(dev, pbuf->iova + actual_offset,
									actual_size, dir);
			else
				dma_sync_single_for_cpu(dev, pbuf->iova + actual_offset,
									actual_size, dir);
		}

		size_offset += sg->length;
		if (size_offset >= offset + size)
			break;
	}

	dbg_mem(1, "%s_for_%s end", __func__, target == SYNC_FOR_DEVICE ? "device" : "cpu");
}


#if IS_ENABLED(CONFIG_ION)
/* is private buffer operations for 'ION' */
static void is_ion_sync_for_device(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct is_ion_ctx *iic;

	if (pbuf) {
		iic = (struct is_ion_ctx *)(pbuf->ctx);
		pablo_mem_sync(pbuf, offset, size, iic->dev, dir, SYNC_FOR_DEVICE);
	}
}

static void is_ion_sync_for_cpu(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct is_ion_ctx *iic;

	if (pbuf) {
		iic = (struct is_ion_ctx *)(pbuf->ctx);
		pablo_mem_sync(pbuf, offset, size, iic->dev, dir, SYNC_FOR_CPU);
	}
}

const struct is_priv_buf_ops is_priv_buf_ops_ion = {
	.free			= pablo_dma_buf_free,
	.kvaddr			= pablo_dma_buf_kvaddr,
	.dvaddr			= pablo_mem_dvaddr,
	.phaddr			= pablo_mem_phaddr,
	.sync_for_device	= is_ion_sync_for_device,
	.sync_for_cpu		= is_ion_sync_for_cpu,
};

/* is memory operations for 'ION' */
unsigned int is_ion_query_heapmask(const char *heap_name)
{
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];
	int i;
	int cnt = ion_query_heaps_kernel(NULL, 0);

	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);

	for (i = 0; i < cnt; i++) {
		 if (!strncmp(data[i].name, heap_name, MAX_HEAP_NAME))
			 break;
	}

	if (i == cnt)
		return 0;

	return 1 << data[i].heap_id;
}

static void *is_ion_init(struct platform_device *pdev)
{
	struct is_ion_ctx *ctx;
	const char *heapname =
		IS_ENABLED(CONFIG_ION_EXYNOS) ? "vendor_system_heap" : "ion_system_heap";
	ctx = vzalloc(sizeof(*ctx));
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->dev = &pdev->dev;
	ctx->alignment = SZ_4K;
	ctx->flags = ION_FLAG_CACHED;
	mutex_init(&ctx->lock);
	ctx->heapmask = is_ion_query_heapmask(heapname);
	if (!ctx->heapmask)
		ctx->heapmask = is_ion_query_heapmask("ion_system_heap");

	ctx->heapmask_s = is_ion_query_heapmask("secure_camera_heap");

	return ctx;
}

static void is_ion_deinit(void *ctx)
{
	struct is_ion_ctx *alloc_ctx = ctx;

	mutex_destroy(&alloc_ctx->lock);
	vfree(alloc_ctx);
}

static struct is_priv_buf *is_ion_alloc(void *ctx,
		size_t size, const char *heapname, unsigned int flags)
{
	struct is_ion_ctx *alloc_ctx = ctx;
	struct is_priv_buf *buf;
	unsigned int heapmask;
	int ret = 0;
	size_t aligned_size;

	buf = vzalloc(sizeof(*buf));
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dbuf_ops = &pdb_ops;
	aligned_size = PAGE_ALIGN(size);
	heapmask = (flags & ION_EXYNOS_FLAG_PROTECTED) ? alloc_ctx->heapmask_s : alloc_ctx->heapmask;

	buf->dma_buf = ion_alloc(aligned_size, heapmask, flags ? flags : alloc_ctx->flags);
	if (IS_ERR(buf->dma_buf)) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	buf->attachment = CALL_DMABUF_OPS(buf, attach, buf->dma_buf, alloc_ctx->dev);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		goto err_attach;
	}

	buf->sgt = CALL_DMABUF_OPS(buf, map_attachment, buf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->sgt)) {
		ret = PTR_ERR(buf->sgt);
		goto err_map_dmabuf;
	}

	buf->ctx = alloc_ctx;
	buf->size = size;
	buf->aligned_size = aligned_size;
	buf->direction = DMA_BIDIRECTIONAL;
	buf->ops = &is_priv_buf_ops_ion;

	mutex_lock(&alloc_ctx->lock);
	buf->iova = sg_dma_address(buf->sgt->sgl);
	if (IS_ERR_VALUE(buf->iova)) {
		ret = (int)buf->iova;
		mutex_unlock(&alloc_ctx->lock);
		goto err_ion_map_io;
	}
	mutex_unlock(&alloc_ctx->lock);

	dbg_mem(1, "%s: size(%zu), flag(%x)\n", __func__, size, flags);
	atomic_add(aligned_size, &mem_stats_sz_active);

	return buf;

err_ion_map_io:
	CALL_DMABUF_OPS(buf, unmap_attachment, buf->attachment, buf->sgt, DMA_BIDIRECTIONAL);
err_map_dmabuf:
	CALL_DMABUF_OPS(buf, detach, buf->dma_buf, buf->attachment);
err_attach:
	CALL_DMABUF_OPS(buf, put, buf->dma_buf);
err_alloc:
	vfree(buf);

	pr_err("%s: Error occurred while allocating\n", __func__);
	return ERR_PTR(ret);
}

const struct is_mem_ops is_mem_ops_ion = {
	.init		= is_ion_init,
	.cleanup	= is_ion_deinit,
	.alloc		= is_ion_alloc,
};
#else
const struct is_mem_ops is_mem_ops_ion = { NULL, };
#endif /* CONFIG_ION */

#if IS_ENABLED(CONFIG_DMABUF_HEAPS)
/* is private buffer operations for 'dmabuf heaps' */
static void pablo_dmabuf_heap_sync_for_device(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct pablo_dmabuf_heap_ctx *pdhc;

	if (pbuf) {
		pdhc = (struct pablo_dmabuf_heap_ctx *)(pbuf->ctx);
		pablo_mem_sync(pbuf, offset, size, pdhc->dev, dir, SYNC_FOR_DEVICE);
	}
}

static void pablo_dmabuf_heap_sync_for_cpu(struct is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct pablo_dmabuf_heap_ctx *pdhc;

	if (pbuf) {
		pdhc = (struct pablo_dmabuf_heap_ctx *)(pbuf->ctx);
		pablo_mem_sync(pbuf, offset, size, pdhc->dev, dir, SYNC_FOR_CPU);
	}
}

const struct is_priv_buf_ops pablo_priv_buf_ops_dmabuf_heap = {
	.free			= pablo_dma_buf_free,
	.kvaddr			= pablo_dma_buf_kvaddr,
	.dvaddr			= pablo_mem_dvaddr,
	.phaddr			= pablo_mem_phaddr,
	.sync_for_device	= pablo_dmabuf_heap_sync_for_device,
	.sync_for_cpu		= pablo_dmabuf_heap_sync_for_cpu,
	.attach_ext		= pablo_dmabuf_heap_attach_ext,
	.detach_ext		= pablo_dmabuf_heap_detach_ext,
	.dvaddr_ext		= pablo_mem_dvaddr_ext,
};

/* is memory operations for 'dmabuf heaps' */
static void *pablo_dmabuf_heap_init(struct platform_device *pdev)
{
	struct pablo_dmabuf_heap_ctx *pdhc;

	pdhc = vzalloc(sizeof(*pdhc));
	if (!pdhc)
		return ERR_PTR(-ENOMEM);

	pdhc->dev = &pdev->dev;
	mutex_init(&pdhc->lock);

	pdhc->dh_system = dma_heap_find("system");
	pdhc->dh_system_uncached = dma_heap_find("system-uncached");
	pdhc->dh_secure_camera = dma_heap_find(SECURE_HEAPNAME);
	if (!pdhc->dh_secure_camera)
		probe_warn("There's no secure camera reserved memory");

#if defined(USE_CAMERA_HEAP)
	pdhc->dh_camera = dma_heap_find(CAMERA_HEAP_NAME);
	if (!pdhc->dh_camera)
		probe_warn("There's no camera heap");
	pdhc->dh_camera_uncached = dma_heap_find(CAMERA_HEAP_UNCACHED_NAME);
	if (!pdhc->dh_camera_uncached)
		probe_warn("There's no camera heap uncached");
#endif

	return pdhc;
}

static void pablo_dmabuf_heap_cleanup(void *ctx)
{
	struct pablo_dmabuf_heap_ctx *pdhc =
		(struct pablo_dmabuf_heap_ctx *)ctx;

	mutex_destroy(&pdhc->lock);

	if (pdhc->dh_system)
		dma_heap_put(pdhc->dh_system);
	if (pdhc->dh_system_uncached)
		dma_heap_put(pdhc->dh_system_uncached);
	if (pdhc->dh_secure_camera)
		dma_heap_put(pdhc->dh_secure_camera);
#if defined(USE_CAMERA_HEAP)
	if (pdhc->dh_camera)
		dma_heap_put(pdhc->dh_camera);
	if (pdhc->dh_camera_uncached)
		dma_heap_put(pdhc->dh_camera_uncached);
#endif

	vfree(pdhc);
}

static struct dma_heap *pablo_dmabuf_heap_find(void *ctx, const char *heapname)
{
	struct pablo_dmabuf_heap_ctx *pdhc = (struct pablo_dmabuf_heap_ctx *)ctx;

	if (!strncmp(heapname, "system", 6))
		return pdhc->dh_system;
	else if (!strncmp(heapname, "system-uncached", 15))
		return pdhc->dh_system_uncached;
	else if (!strncmp(heapname, SECURE_HEAPNAME, 20))
		return pdhc->dh_secure_camera;
#if defined(USE_CAMERA_HEAP)
	else if (!strncmp(heapname, CAMERA_HEAP_NAME, CAMERA_HEAP_NAME_LEN))
		return pdhc->dh_camera ? pdhc->dh_camera : pdhc->dh_system;
	else if (!strncmp(heapname, CAMERA_HEAP_UNCACHED_NAME, CAMERA_HEAP_UNCACHED_NAME_LEN))
		return pdhc->dh_camera_uncached ? pdhc->dh_camera_uncached : pdhc->dh_system_uncached;
#endif
	else
		return NULL;
}

static struct is_priv_buf *pablo_dmabuf_heap_alloc(void *ctx,
		size_t size, const char *heapname, unsigned int flags)
{
	struct pablo_dmabuf_heap_ctx *pdhc = (struct pablo_dmabuf_heap_ctx *)ctx;
	struct dma_heap	*dh;
	struct is_priv_buf *buf;
	int ret;
	size_t aligned_size;

	buf = vzalloc(sizeof(*buf));
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dbuf_ops = &pdb_ops;
	aligned_size = PAGE_ALIGN(size);

	dh = heapname && strlen(heapname) ?
		pablo_dmabuf_heap_find(ctx, heapname) : pdhc->dh_system;
	if (!dh)
		return ERR_PTR(-EINVAL);
#ifdef USE_CAMERA_HEAP
	if ((pdhc->dh_camera && dh == pdhc->dh_camera) ||
		(pdhc->dh_camera_uncached && dh == pdhc->dh_camera_uncached)) {
		if (rmem_allocated + size < CAMERA_HEAP_RESERVE_MEM_SIZE) {
			buf->dma_buf = dma_heap_buffer_alloc(dh, size, 0, 0);
			if (IS_ERR(buf->dma_buf)) {
				info("Camera Heap: alloc error, try system heap, size = %d\n", size);
				if (dh == pdhc->dh_camera_uncached)
					dh = pdhc->dh_system_uncached;
				else
					dh = pdhc->dh_system;
			} else {
				rmem_allocated += size;
				info("Camera Heap: alloc success, size = %d\n total = %d", size, rmem_allocated);
				goto camera_heap_alloc_success;
			}
		} else {
			info("Camera Heap: Kernel Reserve mem limit reached! try system heap, size = %d\n", size);
			if (dh == pdhc->dh_camera_uncached)
				dh = pdhc->dh_system_uncached;
			else
				dh = pdhc->dh_system;
		}
	}
#endif
	buf->dma_buf = dma_heap_buffer_alloc(dh, aligned_size, 0, 0);
	if (IS_ERR(buf->dma_buf)) {
		ret = -ENOMEM;
		goto err_alloc;
	}
#ifdef USE_CAMERA_HEAP
camera_heap_alloc_success:
#endif
	buf->attachment = CALL_DMABUF_OPS(buf, attach, buf->dma_buf, pdhc->dev);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		goto err_attach;
	}

	buf->sgt = CALL_DMABUF_OPS(buf, map_attachment, buf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->sgt)) {
		ret = PTR_ERR(buf->sgt);
		goto err_map_dmabuf;
	}

	buf->ctx = (void *)pdhc;
	buf->size = size;
	buf->aligned_size = aligned_size;
	buf->direction = DMA_BIDIRECTIONAL;
	buf->ops = &pablo_priv_buf_ops_dmabuf_heap;

	mutex_lock(&pdhc->lock);
	buf->iova = sg_dma_address(buf->sgt->sgl);
	if (IS_ERR_VALUE(buf->iova)) {
		ret = (int)buf->iova;
		mutex_unlock(&pdhc->lock);
		goto err_ion_map_io;
	}
	mutex_unlock(&pdhc->lock);

	dbg_mem(1, "%s: size(%zu), flag(%x)\n", __func__, size, flags);
	atomic_add(aligned_size, &mem_stats_sz_active);

	return buf;

err_ion_map_io:
	CALL_DMABUF_OPS(buf, unmap_attachment, buf->attachment, buf->sgt, DMA_BIDIRECTIONAL);
err_map_dmabuf:
	CALL_DMABUF_OPS(buf, detach, buf->dma_buf, buf->attachment);
err_attach:
	CALL_DMABUF_OPS(buf, put, buf->dma_buf);
err_alloc:
	vfree(buf);

	pr_err("%s: Error occurred while allocating\n", __func__);
	return ERR_PTR(ret);
}

const struct is_mem_ops pablo_mem_ops_dmabuf_heap = {
	.init		= pablo_dmabuf_heap_init,
	.cleanup	= pablo_dmabuf_heap_cleanup,
	.alloc		= pablo_dmabuf_heap_alloc,
};
#else
const struct is_mem_ops pablo_mem_ops_dmabuf_heap = { NULL, };
#endif /* CONFIG_DMABUF_HEAPS */

/* pablo private buffer operations for 'contig mem' */
static void pablo_contig_free(struct is_priv_buf *pbuf)
{
	kfree(pbuf->kvaddr);
	pbuf->kvaddr = NULL;

	kfree(pbuf);
	pbuf = NULL;
}

static ulong pablo_contig_kvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return (ulong)pbuf->kvaddr;
}

static phys_addr_t pablo_contig_phaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return virt_to_phys(pbuf->kvaddr);
}

const struct is_priv_buf_ops is_priv_buf_ops_contig = {
	.free	= pablo_contig_free,
	.kvaddr	= pablo_contig_kvaddr,
	.phaddr	= pablo_contig_phaddr,
};

static struct is_priv_buf *pablo_contig_alloc(size_t size)
{
	struct is_priv_buf *buf;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->kvaddr = kzalloc(size, GFP_KERNEL);
	if (!buf->kvaddr) {
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->size = size;
	buf->ops = &is_priv_buf_ops_contig;
	buf->dbuf_ops = &pdb_ops;

	return buf;
}

void is_vfree_atomic(const void *addr)
{
	/*
	 * Use raw_cpu_ptr() because this can be called from preemptible
	 * context. Preemption is absolutely fine here, because the llist_add()
	 * implementation is lockless, so it works even if we are adding to
	 * another cpu's list.  schedule_work() should be fine with this too.
	 */
	struct is_vfree_deferred *p = raw_cpu_ptr(is_vfree_deferred);

	kmemleak_free(addr);

	if (llist_add((struct llist_node *)addr, &p->list))
		schedule_work(&p->wq);
}
EXPORT_SYMBOL_GPL(is_vfree_atomic);

void is_mem_init_stats(void)
{
	atomic_set(&stats.cnt_plane_kmap, 0);
	atomic_set(&stats.cnt_plane_kunmap, 0);
	atomic_set(&stats.cnt_plane_dmap, 0);
	atomic_set(&stats.cnt_plane_dunmap, 0);
	atomic_set(&stats.cnt_buf_remap, 0);
	atomic_set(&stats.cnt_buf_unremap, 0);
	atomic_set(&stats.cnt_dbuf_prepare, 0);
	atomic_set(&stats.cnt_dbuf_finish, 0);
	atomic_set(&stats.cnt_dbuf_map, 0);
	atomic_set(&stats.cnt_dbuf_unmap, 0);
	atomic_set(&stats.cnt_dbuf_kmap, 0);
	atomic_set(&stats.cnt_dbuf_kunmap, 0);
	atomic_set(&stats.cnt_subbuf_prepare, 0);
	atomic_set(&stats.cnt_subbuf_finish, 0);
	atomic_set(&stats.cnt_subbuf_kvmap, 0);
	atomic_set(&stats.cnt_subbuf_kunmap, 0);
}
EXPORT_SYMBOL_GPL(is_mem_init_stats);

void is_mem_check_stats(struct is_mem *mem)
{
	bool unbalanced = atomic_read(&mem->stats->cnt_plane_kmap)
		!= atomic_read(&mem->stats->cnt_plane_kunmap)	||
		atomic_read(&mem->stats->cnt_plane_dmap)
		!= atomic_read(&mem->stats->cnt_plane_dunmap)	||
		atomic_read(&mem->stats->cnt_buf_remap)
		!= atomic_read(&mem->stats->cnt_buf_unremap)	||
		atomic_read(&mem->stats->cnt_dbuf_prepare)
		!= atomic_read(&mem->stats->cnt_dbuf_finish)	||
		atomic_read(&mem->stats->cnt_dbuf_map)
		!= atomic_read(&mem->stats->cnt_dbuf_unmap)	||
		atomic_read(&mem->stats->cnt_dbuf_kmap)
		!= atomic_read(&mem->stats->cnt_dbuf_kunmap)	||
		atomic_read(&mem->stats->cnt_subbuf_prepare)
		!= atomic_read(&mem->stats->cnt_subbuf_finish)	||
		atomic_read(&mem->stats->cnt_subbuf_kvmap)
		!= atomic_read(&mem->stats->cnt_subbuf_kunmap);


	if (unbalanced) {
		dev_err(mem->dev,
			"counters for memory OPs unbalanced!\n");
		dev_err(mem->dev,
			"\tplane_kmap: %d, plane_kunmap: %d\n",
			atomic_read(&mem->stats->cnt_plane_kmap),
			atomic_read(&mem->stats->cnt_plane_kunmap));
		dev_err(mem->dev,
			"\tplane_dmap: %d, plane_dunmap: %d\n",
			atomic_read(&mem->stats->cnt_plane_dmap),
			atomic_read(&mem->stats->cnt_plane_dunmap));
		dev_err(mem->dev,
			"\tbuf_remap: %d, buf_unremap: %d\n",
			atomic_read(&mem->stats->cnt_buf_remap),
			atomic_read(&mem->stats->cnt_buf_unremap));
		dev_err(mem->dev,
			"\tdbuf_prepare: %d, dbuf_finish: %d\n",
			atomic_read(&mem->stats->cnt_dbuf_prepare),
			atomic_read(&mem->stats->cnt_dbuf_finish));
		dev_err(mem->dev,
			"\tdbuf_map: %d, dbuf_unmap: %d\n",
			atomic_read(&mem->stats->cnt_dbuf_map),
			atomic_read(&mem->stats->cnt_dbuf_unmap));
		dev_err(mem->dev,
			"\tdbuf_kmap: %d, dbuf_kunmap: %d\n",
			atomic_read(&mem->stats->cnt_dbuf_kmap),
			atomic_read(&mem->stats->cnt_dbuf_kunmap));
		dev_err(mem->dev,
			"\tsubbuf_prepare: %d, subbuf_finish: %d\n",
			atomic_read(&mem->stats->cnt_subbuf_prepare),
			atomic_read(&mem->stats->cnt_subbuf_finish));
		dev_err(mem->dev,
			"\tsubbuf_kvmap: %d, subbuf_kunmap: %d\n",
			atomic_read(&mem->stats->cnt_subbuf_kvmap),
			atomic_read(&mem->stats->cnt_subbuf_kunmap));
	}

	is_mem_init_stats();
}
EXPORT_SYMBOL_GPL(is_mem_check_stats);

static void __init_percpu_free_work(void)
{
	int i;

	is_vfree_deferred = alloc_percpu(struct is_vfree_deferred);
	if (!is_vfree_deferred) {
		pr_err("alloc_percpu failed!!");
		return;
	}

	for_each_possible_cpu(i) {
		struct is_vfree_deferred *p;

		p = per_cpu_ptr(is_vfree_deferred, i);
		init_llist_head(&p->list);
		INIT_WORK(&p->wq, is_free_work);
	}
}

const struct pablo_dmabuf_container_operations pdbc_ops = {
	.get_count = is_dmabuf_container_get_count,
	.get_buffer = is_dmabuf_container_get_buffer,
};

int is_mem_init(struct is_mem *mem, struct platform_device *pdev)
{
	mem->dev = &pdev->dev;
	mem->stats = &stats;

	if (IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)) {
		mem->vb2_mem_ops = &vb2_dma_sg_memops;
		mem->is_vb2_buf_ops = &is_vb2_buf_ops_dma_sg;
	}

	if (IS_ENABLED(CONFIG_DMABUF_HEAPS)) {
		mem->is_mem_ops = &pablo_mem_ops_dmabuf_heap;
		mem->priv = CALL_PTR_MEMOP(mem, init, pdev);
		if (IS_ERR(mem->priv))
			return PTR_ERR(mem->priv);
	} else if (IS_ENABLED(CONFIG_ION)) {
		mem->is_mem_ops = &is_mem_ops_ion;
		mem->priv = CALL_PTR_MEMOP(mem, init, pdev);
		if (IS_ERR(mem->priv))
			return PTR_ERR(mem->priv);
	}

	mem->dbuf_ops = &pdb_ops;
	mem->dbuf_con_ops = &pdbc_ops;

	mem->contig_alloc = &pablo_contig_alloc;

	if (!is_vfree_deferred)
		__init_percpu_free_work();

	return 0;
}
EXPORT_SYMBOL_GPL(is_mem_init);

void is_clean_dcache_area(void *kaddr, u32 size)
{
	_is_clean_dcache_area(kaddr, size);
}
EXPORT_SYMBOL_GPL(is_clean_dcache_area);

struct is_minfo *is_get_is_minfo(void)
{
	return &minfo;
}
EXPORT_SYMBOL_GPL(is_get_is_minfo);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define G_KERNEL_PARAM(name)	&__param_##name
const struct kernel_param *pablo_mem_get_kernel_param(void)
{
	return G_KERNEL_PARAM(mem_stats_active);
}
KUNIT_EXPORT_SYMBOL(pablo_mem_get_kernel_param);
#endif
