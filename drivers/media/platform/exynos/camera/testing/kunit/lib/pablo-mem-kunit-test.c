// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-memops.h>

#include "pablo-kunit-test.h"

#include "is-core.h"
#include "pablo-mem.h"

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

	struct vb2_buffer		*vb;
};

static struct pablo_kunit_test_ctx {
	struct is_mem *mem;
	struct vb2_queue *queue;
	struct is_vb2_buf *vbuf;
	struct is_sub_dma_buf *sdbuf;
	struct is_priv_buf *pbuf;
	struct dma_buf *dbuf;
	struct device *ext_dev;
} pkt_ctx;

#if (KERNEL_VERSION(5, 15, 3) <= LINUX_VERSION_CODE)
static void *pkt_vb2_vaddr(struct vb2_buffer *vb, void *buf_priv)
#else
static void *pkt_vb2_vaddr(void *buf_priv)
#endif
{
	return buf_priv;
}

#if (KERNEL_VERSION(5, 15, 3) <= LINUX_VERSION_CODE)
static void *pkt_vb2_cookie(struct vb2_buffer *vb, void *buf_priv)
#else
static void *pkt_vb2_cookie(void *buf_priv)
#endif
{
	struct vb2_dma_sg_buf *buf = buf_priv;

	return (void *)&buf->sg_table;
}

const struct vb2_mem_ops pkt_vb2_mem_ops = {
	.vaddr = pkt_vb2_vaddr,
	.cookie = pkt_vb2_cookie,
};

static struct dma_buf_attachment *pkt_dbuf_attach(struct dma_buf *dbuf, struct device *dev)
{
	struct vb2_dma_sg_buf *buf;

	if (!dev->parent)
		return ERR_PTR(-ENODEV);

	buf = (struct vb2_dma_sg_buf *)pkt_ctx.vbuf->vb.vb2_buf.planes[0].mem_priv;

	return buf->db_attach;
}

static void pkt_dbuf_detach(struct dma_buf *dbuf, struct dma_buf_attachment *attach)
{
}

static struct dma_buf *pkt_dbuf_get(int fd)
{
	if (!fd)
		return NULL;

	return pkt_ctx.dbuf;
}

static void pkt_dbuf_put(struct dma_buf *dbuf)
{
}

static struct sg_table *pkt_dbuf_map_attachment(struct dma_buf_attachment *attach,
		enum dma_data_direction dir)
{
	struct vb2_dma_sg_buf *buf;

	if (!attach->dmabuf)
		return ERR_PTR(-EINVAL);

	buf = (struct vb2_dma_sg_buf *)pkt_ctx.vbuf->vb.vb2_buf.planes[0].mem_priv;

	return &buf->sg_table;
}

static void pkt_dbuf_unmap_attachment(struct dma_buf_attachment *attach,
		struct sg_table *sgt, enum dma_data_direction dir)
{
}

static int pkt_dbuf_begin_cpu_access(struct dma_buf *dbuf, enum dma_data_direction dir)
{
	if (dir == DMA_NONE)
		return -EINVAL;

	return 0;
}

static int pkt_dbuf_end_cpu_access(struct dma_buf *dbuf, enum dma_data_direction dir)
{
	return 0;
}

static void *pkt_dbuf_vmap(struct dma_buf *dbuf)
{
	if (!dbuf->size)
		return NULL;

	return (void *)&dbuf->vmap_ptr;
}

static void pkt_dbuf_vunmap(struct dma_buf *dbuf, void *kva)
{
}

static void pkt_get_dma_buf(struct dma_buf *dbuf)
{
}

const struct pablo_dma_buf_operations pkt_pdb_ops = {
	.attach = pkt_dbuf_attach,
	.detach = pkt_dbuf_detach,
	.get = pkt_dbuf_get,
	.put = pkt_dbuf_put,
	.map_attachment = pkt_dbuf_map_attachment,
	.unmap_attachment = pkt_dbuf_unmap_attachment,
	.begin_cpu_access = pkt_dbuf_begin_cpu_access,
	.end_cpu_access = pkt_dbuf_end_cpu_access,
	.vmap = pkt_dbuf_vmap,
	.vunmap = pkt_dbuf_vunmap,
	.get_dma_buf = pkt_get_dma_buf,
};

static int pkt_dbuf_con_get_count(struct dma_buf *dbuf)
{
	if (!dbuf->size)
		return -EINVAL;
	else if (dbuf->size > IS_MAX_PLANES)
		return 0;

	return dbuf->size;
}

static struct dma_buf *pkt_dbuf_con_get_buffer(struct dma_buf *dbuf, int index)
{
	if (!dbuf->vmapping_counter)
		return NULL;

	return pkt_ctx.dbuf;
}

const struct pablo_dmabuf_container_operations pkt_pdbc_ops = {
	.get_count = pkt_dbuf_con_get_count,
	.get_buffer = pkt_dbuf_con_get_buffer,
};

/* Define testcases */
static void pablo_mem_stats_active_kunit_test(struct kunit *test)
{
	const struct kernel_param *kp = pablo_mem_get_kernel_param();
	char *buf = (char *)kunit_kzalloc(test, NAME_MAX, 0);
	int val, ret;

	/* TC #1. Set/Get mem_state_active */
	kp->ops->set("3", kp);
	kp->ops->get(buf, kp);

	ret = kstrtoint(buf, 0, &val);

	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 3);

	kp->ops->set("0", kp);

	kunit_kfree(test, buf);
}

static void pablo_mem_vb2_dma_sg_vaddr_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_dma_sg_buf *buf;
	struct scatterlist *sgl;
	ulong kva;
	dma_addr_t dva;

	/* TC #1. Get kva of DMA buffer */
	kva = mem->is_vb2_buf_ops->plane_kvaddr(vbuf, 0);

	buf = (struct vb2_dma_sg_buf *)vbuf->vb.vb2_buf.planes[0].mem_priv;
	KUNIT_EXPECT_PTR_EQ(test, (void *)kva, (void *)buf);

	/* TC #2. Get dva of DMA buffer */
	dva = mem->is_vb2_buf_ops->plane_dvaddr(vbuf, 0);

	sgl = buf->sg_table.sgl;
	KUNIT_EXPECT_EQ(test, dva, sgl->dma_address);
}

static void pablo_mem_vb2_dma_sg_kmap_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	ulong kva, prev_kva;

	/* TC #1. Invalid fd of dma_buf */
	vb->planes[0].m.fd = 0;
	kva = mem->is_vb2_buf_ops->plane_kmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, kva, (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kmap), 0);

	/* TC #2. Error return from 'begin_cpu_access' */
	vb->planes[0].m.fd = __LINE__;
	buf->dma_dir = DMA_NONE;
	kva = mem->is_vb2_buf_ops->plane_kmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, kva, (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kmap), 0);

	/* TC #3. Fail to do 'vmap' of dma_buf */
	buf->dma_dir = DMA_BIDIRECTIONAL;
	dbuf->size = 0;
	kva = mem->is_vb2_buf_ops->plane_kmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, kva, (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kmap), 0);

	/* TC #4. Get valid kva of dma_buf */
	dbuf->size = __LINE__;
	pkt_ctx.ext_dev->parent = mem->dev;
	vbuf->kva[0] = 0UL;
	db_attach->dmabuf = dbuf;
	kva = mem->is_vb2_buf_ops->plane_kmap(vbuf, 0, 0);
	KUNIT_EXPECT_NE(test, kva, (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kmap), 1);

	/* TC #5. Get kva of dma_buf, again */
	prev_kva = kva;
	kva = mem->is_vb2_buf_ops->plane_kmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, kva, prev_kva);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kmap), 1);

	/* TC #6. Unmap kva of dma_buf */
	mem->is_vb2_buf_ops->plane_kunmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kunmap), 1);

	/* TC #7. Unmap kva_of dma_buf, again */
	mem->is_vb2_buf_ops->plane_kunmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_kunmap), 1);
}

static void pablo_mem_vb2_dma_sg_dmap_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	dma_addr_t dva, prev_dva;

	/* TC #1. Invalid fd of dma_buf */
	vb->planes[0].m.fd = 0;
	dva = mem->is_vb2_buf_ops->plane_dmap(vbuf, 0, 0, NULL);
	KUNIT_EXPECT_EQ(test, dva, (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dmap), 0);

	/* TC #2. Fail to attach dma_buf with extra device */
	dbuf->size = __LINE__;
	pkt_ctx.ext_dev->parent = NULL;
	dva = mem->is_vb2_buf_ops->plane_dmap(vbuf, 0, 0, pkt_ctx.ext_dev);
	KUNIT_EXPECT_EQ(test, dva, (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dmap), 0);

	/* TC #3. Fail to map dma_buf attachment with extra device */
	pkt_ctx.ext_dev->parent = mem->dev;
	db_attach->dmabuf = NULL;
	dva = mem->is_vb2_buf_ops->plane_dmap(vbuf, 0, 0, pkt_ctx.ext_dev);
	KUNIT_EXPECT_EQ(test, dva, (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dmap), 0);

	/* TC #4. Get valid dva of dma_buf */
	vb->planes[0].m.fd = __LINE__;
	buf->dma_dir = DMA_BIDIRECTIONAL;
	dbuf->size = __LINE__;
	pkt_ctx.ext_dev->parent = mem->dev;
	db_attach->dmabuf = dbuf;
	dva = mem->is_vb2_buf_ops->plane_dmap(vbuf, 0, 0, pkt_ctx.ext_dev);
	KUNIT_EXPECT_NE(test, dva, (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dmap), 1);

	/* TC #5. Get dva of dma_buf, again */
	prev_dva = dva;
	dva = mem->is_vb2_buf_ops->plane_dmap(vbuf, 0, 0, pkt_ctx.ext_dev);
	KUNIT_EXPECT_EQ(test, dva, prev_dva);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dmap), 1);

	/* TC #6. Unmap dva of dma_buf */
	mem->is_vb2_buf_ops->plane_dunmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, vbuf->dva_ext[0], (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dunmap), 1);

	/* TC #7. Unmap dva of dma_buf, again */
	mem->is_vb2_buf_ops->plane_dunmap(vbuf, 0, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_plane_dunmap), 1);
}

static void pablo_mem_vb2_dma_sg_remap_attr_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vbuf->vb.vb2_buf.planes[0].mem_priv;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct scatterlist *sgl = buf->sg_table.sgl;
	long ret;

	/* TC #1. Fail to map dma_buf attachment */
	db_attach->dmabuf = NULL;
	ret = mem->is_vb2_buf_ops->remap_attr(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_buf_remap), 0);

	/* TC #2. Fail to get valid dva from dma_buf */
	db_attach->dmabuf = dbuf;
	sgl->dma_address = (dma_addr_t)ERR_PTR(-EINVAL);
	ret = mem->is_vb2_buf_ops->remap_attr(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_buf_remap), 0);

	/* TC #3. Get dva of dma_buf */
	sgl->dma_address = (dma_addr_t)__LINE__;
	ret = mem->is_vb2_buf_ops->remap_attr(vbuf, 0);
	KUNIT_EXPECT_EQ(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_buf_remap), 1);

	/* TC #4. Unmap dva of dma_buf */
	mem->is_vb2_buf_ops->unremap_attr(vbuf, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_buf_unremap), 1);
}

static void pablo_mem_dbufcon_prepare_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	long ret;

	vb->num_planes = 2;

	/* TC #1. Invalid fd of dmabuf_container */
	vb->planes[0].m.fd = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 0);

	/* TC #2. Valid fd, but it's not one of dmabuf_container */
	vb->planes[0].m.fd = __LINE__;
	dbuf->size = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_EQ(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 0);

	/* TC #3. Empty dmabuf_container */
	dbuf->size = IS_MAX_PLANES + 1;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 0);

	/* TC #4. dmabuf_container has too many dma_buf */
	dbuf->size = IS_MAX_PLANES;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 0);

	/* TC #5. Valid dmabuf_container, but fail to get valid fd of single dma_buf */
	dbuf->size = 1;
	dbuf->vmapping_counter = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 0);

	/* TC #5. Valid dmabuf_container */
	dbuf->vmapping_counter = 1;
	db_attach->dmabuf = dbuf;
	ret = mem->is_vb2_buf_ops->dbufcon_prepare(vbuf, mem->dev);
	KUNIT_EXPECT_EQ(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->num_merged_dbufs, (unsigned int)1);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_prepare), 1);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)vbuf->dbuf[0]);

	/* TC #6. Cleanup dmabuf_container */
	mem->is_vb2_buf_ops->dbufcon_finish(vbuf);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_finish), 1);
	KUNIT_EXPECT_PTR_EQ(test, (void *)vbuf->dbuf[0], NULL);
}

static void pablo_mem_dbufcon_map_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	struct scatterlist *sgl = buf->sg_table.sgl;
	long ret;

	vbuf->num_merged_dbufs = 1;
	vb->num_planes = 2;

	/* TC #1. Fail to map dma_buf attachment */
	db_attach->dmabuf = NULL;
	vbuf->atch[0] = db_attach;
	ret = mem->is_vb2_buf_ops->dbufcon_map(vbuf);
	KUNIT_EXPECT_NE(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->dva[0], (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_map), 0);

	/* TC #2. Fail to get dva of dma_buf */
	db_attach->dmabuf = dbuf;
	sgl->dma_address = (dma_addr_t)-EINVAL;
	ret = mem->is_vb2_buf_ops->dbufcon_map(vbuf);
	KUNIT_EXPECT_NE(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, vbuf->dva[0], (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_map), 0);

	/* TC #3. Succeed to map dma_buf */
	sgl->dma_address = (dma_addr_t)__LINE__;
	ret = mem->is_vb2_buf_ops->dbufcon_map(vbuf);
	KUNIT_EXPECT_EQ(test, ret, (long)0);
	KUNIT_EXPECT_NE(test, vbuf->dva[0], (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_map), 1);

	/* TC #4. Unmap dma_buf */
	mem->is_vb2_buf_ops->dbufcon_unmap(vbuf);
	KUNIT_EXPECT_EQ(test, vbuf->dva[0], (dma_addr_t)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_unmap), 1);
}

static void pablo_mem_dbufcon_kmap_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	struct scatterlist *sgl = buf->sg_table.sgl;
	int ret;

	vbuf->num_merged_dbufs = 1;
	vb->num_planes = 2;

	/* TC #1. Invalid fd of dma_buf, DMA buffer */
	vb->planes[0].m.fd = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 0);

	/* TC #2. Fail to get valid fd of single dma_buf */
	vb->planes[0].m.fd = __LINE__;
	dbuf->vmapping_counter = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 0);

	/* TC #3. Fail to do vmap for kva */
	dbuf->vmapping_counter = 1;
	dbuf->size = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 0);

	/* TC #4. Get valid kva from dma_buf */
	dbuf->size = 1;
	db_attach->dmabuf = dbuf;
	sgl->dma_address = (dma_addr_t)__LINE__;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 1);

	/* TC #5. Unmap kva of dma_buf */
	mem->is_vb2_buf_ops->dbufcon_kunmap(vbuf, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kunmap), 1);

	/* TC #6. There is no pre-merged dma_buf, and it's not dmabuf_container */
	vbuf->num_merged_dbufs = 0;
	dbuf->size = 0;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 1);

	/* TC #7. Empty dmabuf_container */
	dbuf->size = IS_MAX_PLANES + 1;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_NE(test, ret, 0);
	KUNIT_EXPECT_EQ(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 1);

	/* TC #8. Normal dmabuf_container */
	dbuf->size = 1;
	ret = mem->is_vb2_buf_ops->dbufcon_kmap(vbuf, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, vbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_dbuf_kmap), 2);
}

static void pablo_mem_subbuf_prepare_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_sub_dma_buf *sdbuf = pkt_ctx.sdbuf;
	struct v4l2_plane plane;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)pkt_ctx.vbuf->vb.vb2_buf.planes[0].mem_priv;
	struct dma_buf_attachment *db_attach = buf->db_attach;

	/* TC #1. Invalid fd of v4l2_plane */
	plane.m.fd = 0;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)sdbuf->dbuf[0], NULL);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #2. Empty dmabuf_container fd */
	plane.m.fd = __LINE__;
	dbuf->size = IS_MAX_PLANES + 1;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)sdbuf->dbuf[0], NULL);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #3. dmabuf_container fd, but it has too many buffers */
	sdbuf->num_plane = 2;
	dbuf->size = IS_MAX_PLANES;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)sdbuf->dbuf[0], NULL);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #4. dmabuf_container fd, but it fails to get single dma_buf */
	sdbuf->num_plane = 1;
	dbuf->size = 1;
	dbuf->vmapping_counter = 0;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)sdbuf->dbuf[0], NULL);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #5. Single dmabuf fd, fail to attach with device */
	dbuf->size = 0;
	pkt_ctx.ext_dev->parent = NULL;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, pkt_ctx.ext_dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #6. Single dmabuf fd, fail to map attachment */
	db_attach->dmabuf = NULL;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 0);

	/* TC #6. Single dmabuf fd */
	db_attach->dmabuf = dbuf;
	mem->is_vb2_buf_ops->subbuf_prepare(sdbuf, &plane, mem->dev);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)1);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_prepare), 1);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)sdbuf->dbuf[0]);

	/* TC #7. Finish sub dma buffer */
	mem->is_vb2_buf_ops->subbuf_finish(sdbuf);
	KUNIT_EXPECT_EQ(test, sdbuf->num_buffer, (u32)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_finish), 1);
	KUNIT_EXPECT_PTR_EQ(test, (void *)sdbuf->dbuf[0], NULL);
}

static void pablo_mem_subbuf_dvmap_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_sub_dma_buf *sdbuf = pkt_ctx.sdbuf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)pkt_ctx.vbuf->vb.vb2_buf.planes[0].mem_priv;
	struct scatterlist *sgl = buf->sg_table.sgl;
	long ret;

	sdbuf->num_buffer = 1;

	/* TC #1. SGT pointer is NULL */
	sdbuf->sgt[0] = NULL;
	ret = mem->is_vb2_buf_ops->subbuf_dvmap(sdbuf);
	KUNIT_EXPECT_LT(test, ret, (long)0);
	KUNIT_EXPECT_EQ(test, sdbuf->dva[0], (dma_addr_t)0);

	/* TC #2. Fail to get dva */
	sdbuf->sgt[0] = &buf->sg_table;
	sgl->dma_address = (dma_addr_t)ERR_PTR(-EINVAL);
	ret = mem->is_vb2_buf_ops->subbuf_dvmap(sdbuf);
	KUNIT_EXPECT_LT(test, ret, (long)0);

	/* TC #3. Get valid dva */
	sgl->dma_address = (dma_addr_t)__LINE__;
	ret = mem->is_vb2_buf_ops->subbuf_dvmap(sdbuf);
	KUNIT_EXPECT_EQ(test, ret, (long)0);
	KUNIT_EXPECT_NE(test, sdbuf->dva[0], (dma_addr_t)0);
}

static void pablo_mem_subbuf_kvmap_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;
	struct is_sub_dma_buf *sdbuf = pkt_ctx.sdbuf;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	ulong ret;

	sdbuf->num_buffer = 1;

	/* TC #1. Fail to map kva on dma_buf */
	dbuf->size = 0;
	sdbuf->dbuf[0] = dbuf;
	ret = mem->is_vb2_buf_ops->subbuf_kvmap(sdbuf);
	KUNIT_EXPECT_EQ(test, sdbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_kvmap), 0);

	/* TC #2. Map kva */
	dbuf->size = __LINE__;
	ret = mem->is_vb2_buf_ops->subbuf_kvmap(sdbuf);
	KUNIT_EXPECT_EQ(test, ret, (ulong)0);
	KUNIT_EXPECT_NE(test, sdbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_kvmap), 1);

	/* TC #3. Unmap kva */
	mem->is_vb2_buf_ops->subbuf_kunmap(sdbuf);
	KUNIT_EXPECT_EQ(test, sdbuf->kva[0], (ulong)0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_kunmap), 1);
}

static void pablo_mem_dmabuf_heap_kunit_test(struct kunit *test)
{
	struct platform_device *pdev;
	struct is_mem *mem = pkt_ctx.mem;
	struct pablo_dmabuf_heap_ctx *pdhc;
	struct is_priv_buf *pbuf;
	size_t size = 0x10;

	pdev = container_of(mem->dev, struct platform_device, dev);

	/* TC #1. Init dmabuf_heap context */
	pdhc = mem->is_mem_ops->init(pdev);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pdhc);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pdhc->dh_system);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pdhc->dh_system_uncached);

	/* TC #2. Alloc dmabuf_heap with invalid heap name */
	pbuf = mem->is_mem_ops->alloc((void *)pdhc, size, "invalid-system", 0);
	KUNIT_EXPECT_LT(test, (long)pbuf, (long)0);

	/* TC #3. Alloc dmabuf_heap from system-heap */
	pbuf = mem->is_mem_ops->alloc((void*)pdhc, size, NULL, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf->dma_buf);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf->attachment);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf->sgt);
	KUNIT_EXPECT_EQ(test, pbuf->size, size);
	KUNIT_EXPECT_EQ(test, pbuf->aligned_size, PAGE_ALIGN(size));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf->ops);
	KUNIT_EXPECT_NE(test, pbuf->iova, (dma_addr_t)0);

	/* TC #4. Free dmabuf_heap */
	pbuf->ops->free(pbuf);

	/* TC #5. Cleanup dmabuf_heap context*/
	mem->is_mem_ops->cleanup((void *)pdhc);
}

static void pablo_mem_priv_buf_kva_kunit_test(struct kunit *test)
{
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;
	ulong kva, prev_kva;

	/* TC #1. Get kva with NULL pointer of priv_buf */
	kva = pbuf->ops->kvaddr(NULL);
	KUNIT_EXPECT_EQ(test, kva, (ulong)0);
	KUNIT_EXPECT_EQ(test, (ulong)pbuf->kva, (ulong)0);

	/* TC #2. Get kva with valid pointer */
	kva = pbuf->ops->kvaddr(pbuf);
	KUNIT_EXPECT_NE(test, kva, (ulong)0);
	KUNIT_EXPECT_NE(test, (ulong)pbuf->kva, (ulong)0);

	/* TC #3. Get kva, again */
	prev_kva = kva;
	kva = pbuf->ops->kvaddr(pbuf);
	KUNIT_EXPECT_EQ(test, kva, prev_kva);
	KUNIT_EXPECT_EQ(test, (ulong)pbuf->kva, prev_kva);
}

static void pablo_mem_priv_buf_dva_kunit_test(struct kunit *test)
{
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;
	dma_addr_t dva;

	/* TC #1. Get dva with NULL pointer of priv_buf */
	dva = pbuf->ops->dvaddr(NULL);
	KUNIT_EXPECT_EQ(test, dva, (dma_addr_t)0);

	/* TC #2. Get dva with valid pointer */
	dva = pbuf->ops->dvaddr(pbuf);
	KUNIT_EXPECT_EQ(test, dva, (dma_addr_t)pbuf->iova);
}

static void pablo_mem_priv_buf_pa_kunit_test(struct kunit *test)
{
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;
	phys_addr_t pa;

	/* TC #1. Get pa from priv_buf */
	pa = pbuf->ops->phaddr(pbuf);
	KUNIT_EXPECT_NE(test, pa, (phys_addr_t)0);
}

static void pablo_mem_priv_buf_sync_kunit_test(struct kunit *test)
{
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;

	/* Map kva */
	pbuf->ops->kvaddr(pbuf);
	KUNIT_ASSERT_NE(test, (ulong)pbuf->kva, (ulong)0);

	/* TC #1. Sync with invalid offset */
	pbuf->ops->sync_for_device(pbuf, pbuf->size + 1, pbuf->size, DMA_TO_DEVICE);

	/* TC #2. Sync with invalid size */
	pbuf->ops->sync_for_device(pbuf, 0, pbuf->size + 1, DMA_TO_DEVICE);

	/* TC #3. Sync for device */
	pbuf->ops->sync_for_device(pbuf, 0, pbuf->size, DMA_TO_DEVICE);

	/* TC #4. Sync for device with specific offset */
	pbuf->ops->sync_for_device(pbuf, PAGE_SIZE / 2, PAGE_SIZE, DMA_TO_DEVICE);

	/* TC #5. Sync for CPU */
	pbuf->ops->sync_for_cpu(pbuf, PAGE_SIZE / 2, PAGE_SIZE, DMA_FROM_DEVICE);
}

static void pablo_mem_priv_buf_ext_dev_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;
	struct dma_buf *dbuf = pkt_ctx.dbuf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)pkt_ctx.vbuf->vb.vb2_buf.planes[0].mem_priv;
	struct dma_buf_attachment *db_attach = buf->db_attach;
	struct scatterlist *sgl = buf->sg_table.sgl;
	const struct pablo_dma_buf_operations *b_dbuf_ops;
	struct dma_buf *b_dbuf;
	int ret;
	dma_addr_t dva;

	b_dbuf_ops = pbuf->dbuf_ops;
	b_dbuf = pbuf->dma_buf;
	pbuf->dbuf_ops = mem->dbuf_ops;
	pbuf->dma_buf = dbuf;

	/* TC #1. Fail to attach extra device */
	pkt_ctx.ext_dev->parent = NULL;
	ret = pbuf->ops->attach_ext(pbuf, pkt_ctx.ext_dev);
	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, pbuf->iova_ext, (dma_addr_t)0);

	/* TC #2. Fail to map attachment */
	db_attach->dmabuf = NULL;
	ret = pbuf->ops->attach_ext(pbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, pbuf->iova_ext, (dma_addr_t)0);

	/* TC #3. Fail to get dva */
	db_attach->dmabuf = dbuf;
	sgl->dma_address = (dma_addr_t)ERR_PTR(-EINVAL);
	ret = pbuf->ops->attach_ext(pbuf, mem->dev);
	KUNIT_EXPECT_LT(test, ret, 0);

	/* TC #4. Success to attach with extra device */
	sgl->dma_address = (dma_addr_t)__LINE__;
	ret = pbuf->ops->attach_ext(pbuf, mem->dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, pbuf->iova_ext, (dma_addr_t)0);

	/* TC #5. Get dva for extra device */
	dva = pbuf->ops->dvaddr_ext(pbuf);
	KUNIT_EXPECT_EQ(test, dva, sgl->dma_address);

	/* TC #6. Detach device */
	pbuf->ops->detach_ext(pbuf);
	KUNIT_EXPECT_PTR_EQ(test, (void *)pbuf->sgt_ext, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)pbuf->attachment_ext, NULL);

	pbuf->dbuf_ops = b_dbuf_ops;
	pbuf->dma_buf = b_dbuf;
}

static void pablo_mem_priv_buf_contig_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_priv_buf *pbuf;
	size_t size = PAGE_SIZE;
	ulong kva;
	phys_addr_t pa;

	/* TC #1. Alloc kernel memory */
	pbuf = mem->contig_alloc(size);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pbuf->kvaddr);
	KUNIT_EXPECT_EQ(test, pbuf->size, size);

	/* TC #2. Get kva without priv_buf */
	kva = pbuf->ops->kvaddr(NULL);
	KUNIT_EXPECT_EQ(test, kva, (ulong)0);

	/* TC #3. Get kva */
	kva = pbuf->ops->kvaddr(pbuf);
	KUNIT_EXPECT_EQ(test, kva, (ulong)pbuf->kvaddr);

	/* TC #4. Get pa without priv_buf */
	pa = pbuf->ops->phaddr(NULL);
	KUNIT_EXPECT_EQ(test, pa, (phys_addr_t)0);

	/* TC #5. Get pav */
	pa = pbuf->ops->phaddr(pbuf);
	KUNIT_EXPECT_NE(test, pa, (phys_addr_t)0);

	/* TC #6. Free priv_buf */
	pbuf->ops->free(pbuf);
}

static void pablo_mem_check_stats_kunit_test(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_mem_stats *stats = mem->stats;

	/* TC #1. Make unbalanced state */
	atomic_set(&stats->cnt_subbuf_kvmap, 2);
	atomic_set(&stats->cnt_subbuf_kunmap, 1);

	is_mem_check_stats(mem);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_kvmap), 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&stats->cnt_subbuf_kunmap), 0);
}

static void pablo_mem_clean_dcache_area_kunit_test(struct kunit *test)
{
	void *kva = kunit_kmalloc(test, PAGE_SIZE, GFP_KERNEL);

	is_clean_dcache_area(kva, PAGE_SIZE);

	kunit_kfree(test, kva);
}
/* End of TC definition */

static void pablo_mem_kunit_test_init_vb2_buf(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct vb2_queue *queue = pkt_ctx.queue;
	struct is_vb2_buf *vbuf;
	struct vb2_buffer *vb;
	struct vb2_dma_sg_buf *buf;
	struct scatterlist *sgl;
	struct dma_buf_attachment *db_attach;

	/* Setup dummy videobuf2 buffer */
	vbuf = (struct is_vb2_buf *)kunit_kzalloc(test, sizeof(struct is_vb2_buf), 0);
	vbuf->dbuf_ops = mem->dbuf_ops;
	vbuf->dbuf_con_ops = mem->dbuf_con_ops;

	vb = &vbuf->vb.vb2_buf;
	vb->vb2_queue = queue;
	vb->num_planes = 1;
	pkt_ctx.vbuf = vbuf;

	/* Setup dummy vb2_dma_sg buffer */
	buf = kunit_kzalloc(test, queue->buf_struct_size, 0);
	buf->dev = mem->dev;
	vb->planes[0].mem_priv = (void *)buf;
	vb->planes[0].length = queue->buf_struct_size;

	/* Setup dummy scatterlist */
	sgl = kunit_kzalloc(test, sizeof(struct scatterlist), 0);
	sgl->dma_address = (dma_addr_t)__LINE__;
	buf->sg_table.sgl = sgl;

	/* Setup dummy dma_buf_attachment */
	db_attach = kunit_kzalloc(test, sizeof(struct dma_buf_attachment), 0);
	buf->db_attach = db_attach;
}

static void pablo_mem_kunit_deinit_vb2_buf(struct kunit *test)
{
	struct is_vb2_buf *vbuf = pkt_ctx.vbuf;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_dma_sg_buf *buf = (struct vb2_dma_sg_buf *)vb->planes[0].mem_priv;
	struct scatterlist *sgl = buf->sg_table.sgl;
	struct dma_buf_attachment *db_attach = buf->db_attach;

	kunit_kfree(test, db_attach);
	kunit_kfree(test, sgl);
	kunit_kfree(test, buf);
	kunit_kfree(test, vbuf);
}

static void pablo_mem_kunit_test_init_sub_dma_buf(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_sub_dma_buf *sdbuf;

	/* Setup dummy sub dma buffer */
	sdbuf = (struct is_sub_dma_buf *)kunit_kzalloc(test, sizeof(struct is_sub_dma_buf), 0);
	sdbuf->dbuf_ops = mem->dbuf_ops;
	sdbuf->dbuf_con_ops = mem->dbuf_con_ops;
	sdbuf->num_plane = 1;
	pkt_ctx.sdbuf = sdbuf;
}

static void pablo_mem_kunit_test_deinit_sub_dma_buf(struct kunit *test)
{
	kunit_kfree(test, pkt_ctx.sdbuf);
}

static void pablo_mem_kunit_test_init_priv_buf(struct kunit *test)
{
	struct is_mem *mem = pkt_ctx.mem;
	struct is_priv_buf *pbuf;

	pbuf = mem->is_mem_ops->alloc(mem->priv, PAGE_SIZE * 2, NULL, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pbuf);

	pkt_ctx.pbuf = pbuf;
}

static void pablo_mem_kunit_test_deinit_priv_buf(struct kunit *test)
{
	struct is_priv_buf *pbuf = pkt_ctx.pbuf;

	pbuf->ops->free(pbuf);
}

static int pablo_mem_kunit_test_init(struct kunit *test)
{
	struct is_core *core;
	struct is_mem *mem;
	struct vb2_queue *queue;
	struct dma_buf *dbuf;

	core = pablo_get_core_async();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	mem = (struct is_mem *)kunit_kzalloc(test, sizeof(struct is_mem), 0);
	is_mem_init(mem, core->pdev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mem->dev);

	mem->vb2_mem_ops = &pkt_vb2_mem_ops;
	mem->dbuf_ops = &pkt_pdb_ops;
	mem->dbuf_con_ops = &pkt_pdbc_ops;
	pkt_ctx.mem = mem;

	queue = (struct vb2_queue *)kunit_kzalloc(test, sizeof(struct vb2_queue), 0);
	queue->buf_struct_size = sizeof(struct vb2_dma_sg_buf);
	queue->mem_ops = mem->vb2_mem_ops;
	pkt_ctx.queue = queue;

	dbuf = (struct dma_buf *)kunit_kzalloc(test, sizeof(struct dma_buf), 0);

	pkt_ctx.dbuf = dbuf;

	pkt_ctx.ext_dev = (struct device *)kunit_kzalloc(test, sizeof(struct device), 0);

	pablo_mem_kunit_test_init_vb2_buf(test);
	pablo_mem_kunit_test_init_sub_dma_buf(test);
	pablo_mem_kunit_test_init_priv_buf(test);

	return 0;
}

static void pablo_mem_kunit_test_exit(struct kunit *test)
{
	pablo_mem_kunit_test_deinit_priv_buf(test);
	pablo_mem_kunit_test_deinit_sub_dma_buf(test);
	pablo_mem_kunit_deinit_vb2_buf(test);
	is_mem_init_stats();

	kunit_kfree(test, pkt_ctx.ext_dev);
	kunit_kfree(test, pkt_ctx.dbuf);
	kunit_kfree(test, pkt_ctx.queue);
	kunit_kfree(test, pkt_ctx.mem);
}

static struct kunit_case pablo_mem_kunit_test_cases[] = {
	KUNIT_CASE(pablo_mem_stats_active_kunit_test),
	KUNIT_CASE(pablo_mem_vb2_dma_sg_vaddr_kunit_test),
	KUNIT_CASE(pablo_mem_vb2_dma_sg_kmap_kunit_test),
	KUNIT_CASE(pablo_mem_vb2_dma_sg_dmap_kunit_test),
	KUNIT_CASE(pablo_mem_vb2_dma_sg_remap_attr_kunit_test),
	KUNIT_CASE(pablo_mem_dbufcon_prepare_kunit_test),
	KUNIT_CASE(pablo_mem_dbufcon_map_kunit_test),
	KUNIT_CASE(pablo_mem_dbufcon_kmap_kunit_test),
	KUNIT_CASE(pablo_mem_subbuf_prepare_kunit_test),
	KUNIT_CASE(pablo_mem_subbuf_dvmap_kunit_test),
	KUNIT_CASE(pablo_mem_subbuf_kvmap_kunit_test),
	KUNIT_CASE(pablo_mem_dmabuf_heap_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_kva_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_dva_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_pa_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_sync_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_ext_dev_kunit_test),
	KUNIT_CASE(pablo_mem_priv_buf_contig_kunit_test),
	KUNIT_CASE(pablo_mem_check_stats_kunit_test),
	KUNIT_CASE(pablo_mem_clean_dcache_area_kunit_test),
	{},
};

struct kunit_suite pablo_mem_kunit_test_suite = {
	.name = "pablo-mem-kunit-test",
	.init = pablo_mem_kunit_test_init,
	.exit = pablo_mem_kunit_test_exit,
	.test_cases = pablo_mem_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_mem_kunit_test_suite);

MODULE_LICENSE("GPL");
