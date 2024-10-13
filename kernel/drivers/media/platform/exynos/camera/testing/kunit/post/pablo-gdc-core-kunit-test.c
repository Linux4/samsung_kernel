// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "gdc/pablo-gdc.h"
#include "votf/pablo-votf.h"
#include "gdc/pablo-hw-api-gdc.h"
#include "pmio.h"
#include "pablo-mem.h"

struct pablo_vb2_fileio_buf {
	void *vaddr;
	unsigned int size;
	unsigned int pos;
	unsigned int queued : 1;
};

struct pablo_vb2_fileio_data {
	unsigned int count;
	unsigned int type;
	unsigned int memory;
	struct pablo_vb2_fileio_buf bufs[VB2_MAX_FRAME];
	unsigned int cur_index;
	unsigned int initial_index;
	unsigned int q_count;
	unsigned int dq_count;
	unsigned read_once : 1;
	unsigned write_immediately : 1;
};

static struct pablo_gdc_test_ctx {
	struct gdc_dev gdc;
	struct gdc_ctx ctx;
	struct device dev;
	struct gdc_fmt g_fmt;
	struct v4l2_m2m_ctx m2m_ctx;
	struct vb2_queue vq;
	struct file file;
	struct inode f_inode;
	struct vb2_buffer vb;
	struct is_priv_buf payload;
	struct dma_buf dbuf;
	struct platform_device pdev;
	struct gdc_frame frame;
} pkt_ctx;

static struct is_mem_ops pkt_mem_ops_ion = {
	.init = NULL,
	.cleanup = NULL,
	.alloc = NULL,
};

static struct is_priv_buf_ops pkt_buf_ops = {
	.kvaddr = NULL,
	.dvaddr = NULL,
};

static struct is_priv_buf *pkt_ion_alloc(
	void *ctx, size_t size, const char *heapname, unsigned int flags)
{
	struct is_priv_buf *payload = &pkt_ctx.payload;

	payload->ops = &pkt_buf_ops;

	return payload;
}

static struct pkt_gdc_ops *pkt_ops;

int pkt_v4l2_m2m_stream_onoff(struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type)
{
	if (!V4L2_TYPE_IS_OUTPUT(type))
		return -EINVAL;

	return 0;
}

static struct pablo_gdc_v4l2_ops pkt_v4l2_ops = {
	.m2m_streamon = pkt_v4l2_m2m_stream_onoff,
	.m2m_streamoff = pkt_v4l2_m2m_stream_onoff,
};

static unsigned long pkt_copy_from_user(void *dst, const void *src, unsigned long size)
{
	return 0;
}

static struct pablo_gdc_sys_ops pkt_gdc_sys_ops = {
	.copy_from_user = pkt_copy_from_user,
};

static u32 pkt_hw_gdc_sw_reset(struct pablo_mmio *pmio)
{
	return 0;
}

static void pkt_hw_gdc_set_initialization(struct pablo_mmio *pmio)
{
}

int pkt_hw_gdc_update_param(struct pablo_mmio *pmio, struct gdc_dev *gdc)
{
	if (!pmio)
		return -EINVAL;

	return 0;
}

void pkt_hw_gdc_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb)
{
}

void pkt_gdc_sfr_dump(void __iomem *base_addr)
{
}

static struct pablo_camerapp_hw_gdc pkt_hw_gdc_ops = {
	.sw_reset = pkt_hw_gdc_sw_reset,
	.set_initialization = pkt_hw_gdc_set_initialization,
	.update_param = pkt_hw_gdc_update_param,
	.start = pkt_hw_gdc_start,
	.sfr_dump = pkt_gdc_sfr_dump,
};

static void pablo_gdc_get_debug_gdc_kunit_test(struct kunit *test)
{
	char *buffer;
	int test_result;

	buffer = __getname();

	test_result = pkt_ops->get_debug(buffer, NULL);
	KUNIT_EXPECT_GT(test, test_result, 0);

	__putname(buffer);
}

static void pablo_gdc_votf_set_service_cfg_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	int test_result;

	test_result = pkt_ops->votf_set_service_cfg(TWS, gdc);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	test_result = pkt_ops->votf_set_service_cfg(TRS, gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_votfitf_set_flush_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	int test_result;

	test_result = pkt_ops->votfitf_set_flush(gdc);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_find_format_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	const struct gdc_fmt *fmt;
	u32 pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_32_8B;

	fmt = pkt_ops->find_format(gdc, pixelformat, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fmt);
	KUNIT_EXPECT_EQ(test, fmt->bitperpixel[0], (u8)8);
	KUNIT_EXPECT_EQ(test, fmt->bitperpixel[1], (u8)4);
	KUNIT_EXPECT_EQ(test, fmt->pixelformat, pixelformat);
}

static void pablo_gdc_v4l2_querycap_kunit_test(struct kunit *test)
{
	struct v4l2_capability cap;
	u32 capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			   V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_DEVICE_CAPS;
	u32 device_caps = V4L2_CAP_VIDEO_M2M_MPLANE;
	int test_result;

	test_result = pkt_ops->v4l2_querycap(NULL, NULL, &cap);

	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, cap.capabilities, capabilities);
	KUNIT_EXPECT_EQ(test, cap.device_caps, device_caps);
	KUNIT_EXPECT_EQ(test, strcmp(cap.driver, GDC_MODULE_NAME), 0);
	KUNIT_EXPECT_EQ(test, strcmp(cap.card, GDC_MODULE_NAME), 0);
}

static void pablo_gdc_v4l2_g_fmt_mplane_kunit_test(struct kunit *test)
{
	struct v4l2_format v_fmt;
	int test_result;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct gdc_fmt *g_fmt = &pkt_ctx.g_fmt;

	/* TC : Check frame error */
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : pixelformat is not V4L2_PIX_FMT_YVU420 */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	pkt_ctx.ctx.d_frame.gdc_fmt = g_fmt;
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : pixelformat is V4L2_PIX_FMT_YVU420 */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	g_fmt->pixelformat = V4L2_PIX_FMT_YVU420;
	pkt_ctx.ctx.s_frame.gdc_fmt = g_fmt;
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_v4l2_try_fmt_mplane_kunit_test(struct kunit *test)
{
	struct v4l2_format v_fmt;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	u32 pixelformat[] = { V4L2_PIX_FMT_NV12M_S10B, V4L2_PIX_FMT_NV16M_S10B,
			      V4L2_PIX_FMT_NV12M_P010, V4L2_PIX_FMT_NV16M_P210,
			      V4L2_PIX_FMT_NV12M_SBWCL_32_8B };
	int i;
	int test_result;

	/* TC : Check v4l2 fmt error */
	test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check not support gdc_fmt */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check not gdc_fmt */
	pkt_ctx.gdc.variant = camerapp_hw_gdc_get_size_constraints(pkt_ctx.gdc.pmio);
	for (i = 0; i < ARRAY_SIZE(pixelformat); ++i) {
		v_fmt.fmt.pix_mp.pixelformat = pixelformat[i];
		test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
		KUNIT_EXPECT_EQ(test, test_result, 0);
	}
}

static void pablo_gdc_image_bound_check_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct v4l2_format v_fmt;
	struct v4l2_pix_format_mplane *pixm = &v_fmt.fmt.pix_mp;
	const struct gdc_variant *variant = camerapp_hw_gdc_get_size_constraints(pkt_ctx.gdc.pmio);
	int test_result;

	ctx->gdc_dev->variant = variant;
	v_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M_S10B;

	v_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	v_fmt.fmt.pix_mp.width = variant->limit_output.max_w + 1;
	test_result = pkt_ops->image_bound_check(ctx, v_fmt.type, pixm);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	v_fmt.fmt.pix_mp.width = variant->limit_output.min_w - 1;
	test_result = pkt_ops->image_bound_check(ctx, v_fmt.type, pixm);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	v_fmt.fmt.pix_mp.width = variant->limit_output.max_w + 1;
	test_result = pkt_ops->image_bound_check(ctx, v_fmt.type, pixm);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	v_fmt.fmt.pix_mp.width = variant->limit_output.min_w - 1;
	test_result = pkt_ops->image_bound_check(ctx, v_fmt.type, pixm);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_v4l2_s_fmt_mplane_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct v4l2_format v_fmt;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	v_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	/* TC : Check device is busy line:624*/
	pkt_ctx.m2m_ctx.out_q_ctx.q.streaming = true;
	test_result = pkt_ops->v4l2_s_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EBUSY);

	/* TC : Check normal process */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	pkt_ctx.m2m_ctx.out_q_ctx.q.streaming = false;
	gdc->variant = camerapp_hw_gdc_get_size_constraints(pkt_ctx.gdc.pmio);
	v_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M_S10B;
	test_result = pkt_ops->v4l2_s_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_v4l2_reqbufs_kunit_test(struct kunit *test)
{
	struct v4l2_requestbuffers reqbufs;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_reqbufs(NULL, fh, &reqbufs);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_v4l2_querybuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_querybuf(NULL, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_vb2_qbuf_kunit_test(struct kunit *test)
{
	struct vb2_queue *q = &pkt_ctx.vq;
	struct v4l2_buffer buf;
	int test_result;
	int i;

	/* TC : file io in progress */
	q->fileio = kunit_kzalloc(test, sizeof(struct pablo_vb2_fileio_data), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, q->fileio);
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EBUSY);

	/* TC : buf type is invalid */
	kunit_kfree(test, q->fileio);
	q->fileio = NULL;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : buffer index out of range */
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.index = 2;
	q->num_buffers = 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : invalid memory type */
	buf.index = 1;
	q->num_buffers = 2;
	buf.memory = VB2_MEMORY_DMABUF;
	q->memory = VB2_MEMORY_UNKNOWN;
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		q->bufs[i] = kunit_kzalloc(test, sizeof(struct vb2_buffer), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, q->bufs[i]);
	}
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : planes array not provided */
	q->memory = VB2_MEMORY_DMABUF;
	buf.m.planes = NULL;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : incorrect planes array length */
	buf.m.planes = kunit_kzalloc(test, sizeof(struct v4l2_plane), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf.m.planes);
	buf.length = VB2_MAX_PLANES + 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : buffer is not in dequeued state */
	buf.length = VB2_MAX_PLANES - 2;
	buf.flags = V4L2_BUF_FLAG_REQUEST_FD;
	q->bufs[buf.index]->state = VB2_BUF_STATE_IN_REQUEST;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check normal process */
	buf.flags = VB2_BUF_STATE_IN_REQUEST;
	q->bufs[buf.index]->num_planes = 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, buf.m.planes);
	for (i = 0; i < VIDEO_MAX_FRAME; i++)
		kunit_kfree(test, q->bufs[i]);
}

static void pablo_gdc_check_qbuf_kunit_test(struct kunit *test)
{
	struct v4l2_m2m_ctx *m2m_ctx = &pkt_ctx.m2m_ctx;
	struct v4l2_buffer buf;
	int test_result;

	buf.flags = V4L2_BUF_FLAG_REQUEST_FD;
	test_result = pkt_ops->check_qbuf(NULL, m2m_ctx, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EPERM);

	/* TODO : add successful TC */
}

static void pablo_gdc_v4l2_qbuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_qbuf(file, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_v4l2_dqbuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_dqbuf(file, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_power_clk_enable_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	int test_result;

	test_result = pkt_ops->power_clk_enable(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_power_clk_disable_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;

	gdc->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->pmio);

	gdc->pmio->mmio_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->pmio->mmio_base);

	pkt_ops->power_clk_disable(gdc);

	kunit_kfree(test, gdc->pmio->mmio_base);
	kunit_kfree(test, gdc->pmio);
}

static void pablo_gdc_v4l2_stream_on_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	int test_result;

	gdc->v4l2_ops = &pkt_v4l2_ops;

	/* TC : type is not output */
	gdc->stalled = 1; /* stall state */
	test_result = pkt_ops->v4l2_streamon(file, fh, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
	KUNIT_EXPECT_EQ(test, gdc->stalled, 0);

	/* TC : add successful TC */
	gdc->stalled = 0;
	test_result = pkt_ops->v4l2_streamon(file, fh, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_v4l2_stream_off_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	int test_result;

	gdc->v4l2_ops = &pkt_v4l2_ops;
	gdc->hw_gdc_ops = &pkt_hw_gdc_ops;

	/* TC : type is not V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE */
	atomic_set(&gdc->m2m.in_use, 1);
	test_result = pkt_ops->v4l2_streamoff(file, fh, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : add successful TC */
	test_result = pkt_ops->v4l2_streamoff(file, fh, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_v4l2_s_ctrl_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct file *file = &pkt_ctx.file;
	struct v4l2_control ctrl;
	struct v4l2_fh *fh = &ctx->fh;
	int test_result;

	file->private_data = fh;

	/* TC : successful TC */
	ctrl.value = 0x8004;
	ctrl.id = V4L2_CID_CAMERAPP_GDC_GRID_CROP_START;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->crop_start_x, (ctrl.value & 0xFFFF0000) >> 16);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->crop_start_y, (ctrl.value & 0x0000FFFF));

	ctrl.value = 0x14000F0;
	ctrl.id = V4L2_CID_CAMERAPP_GDC_GRID_CROP_SIZE;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->crop_width, (ctrl.value & 0xFFFF0000) >> 16);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->crop_height, (ctrl.value & 0x0000FFFF));

	ctrl.id = V4L2_CID_CAMERAPP_GDC_GRID_SENSOR_SIZE;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->sensor_width, (ctrl.value & 0xFFFF0000) >> 16);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->sensor_height, (ctrl.value & 0x0000FFFF));

	ctrl.value = 2;
	ctrl.id = V4L2_CID_CAMERAPP_SENSOR_NUM;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, ctx->crop_param->sensor_num, ctrl.value);

	/* TC : default case */
	ctrl.id = V4L2_CID_CAMERAPP_GDC_GRID_CONTROL;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_v4l2_s_ext_ctrls_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct file *file = &pkt_ctx.file;
	struct v4l2_ext_controls *ctrls;
	int test_result;

	ctx->gdc_dev->sys_ops = &pkt_gdc_sys_ops;

	file->private_data = &ctx->fh;

	ctrls = kunit_kzalloc(test, sizeof(struct v4l2_ext_controls), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctrls);
	ctrls->count = 1;

	ctrls->controls = kunit_kzalloc(test, sizeof(struct v4l2_ext_control), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctrls->controls);

	/* TC : case V4L2_CID_CAMERAPP_GDC_GRID_CONTROL */
	ctrls->controls->id = V4L2_CID_CAMERAPP_GDC_GRID_CONTROL;
	ctrls->count = 1;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : case V4L2_CID_CAMERAPP_GDC_GRID_CROP_START */
	ctrls->controls->id = V4L2_CID_CAMERAPP_GDC_GRID_CROP_START;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : case default : gdc_v4l2_s_ext_ctrls is fail*/
	ctrls->controls->id = 0;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	kunit_kfree(test, ctrls->controls);
	kunit_kfree(test, ctrls);
}

static void pablo_gdc_ctx_stop_req_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	ctx->gdc_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx->gdc_dev->m2m.m2m_dev);
	test_result = pkt_ops->ctx_stop_req(ctx);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	pkt_ops->v4l2_m2m_release(ctx->gdc_dev->m2m.m2m_dev);
}

static void pablo_gdc_vb2_queue_setup_kunit_test(struct kunit *test)
{
#define NUM_PLANES 2
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *q = &pkt_ctx.vq;
	unsigned int num_buffers;
	unsigned int num_planes = NUM_PLANES;
	unsigned int sizes[NUM_PLANES];
	struct device *alloc_devs[NUM_PLANES];
	int i;
	int test_result;

	for (i = 0; i < num_planes; i++)
		alloc_devs[i] = &pkt_ctx.dev;
	q->drv_priv = ctx;

	/* TC : frame error */
	test_result = pkt_ops->vb2_queue_setup(q, &num_buffers, &num_planes, sizes, alloc_devs);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : check normal process */
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	test_result = pkt_ops->vb2_queue_setup(q, &num_buffers, &num_planes, sizes, alloc_devs);
	KUNIT_EXPECT_EQ(test, num_planes, ctx->s_frame.num_planes);
	KUNIT_EXPECT_EQ(test, sizes[0], ctx->s_frame.bytesused[0]);
	KUNIT_EXPECT_EQ(test, (u64)alloc_devs[0], (u64)ctx->gdc_dev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_vb2_buf_prepare_kunit_test(struct kunit *test)
{
	struct vb2_buffer *vb = &pkt_ctx.vb;
	int test_result;

	vb->vb2_queue = &pkt_ctx.vq;
	vb->vb2_queue->drv_priv = &pkt_ctx.ctx;

	/* TC : frame is NULL */
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : add successful TC */
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_vb2_buf_finish_kunit_test(struct kunit *test)
{
	struct vb2_buffer *vb = &pkt_ctx.vb;

	vb->vb2_queue = &pkt_ctx.vq;
	vb->vb2_queue->drv_priv = &pkt_ctx.ctx;
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	pkt_ops->vb2_buf_finish(vb);
}

static void pablo_gdc_vb2_buf_queue_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct v4l2_m2m_buffer m2m_buffer;
	struct vb2_v4l2_buffer *v4l2_buf = &m2m_buffer.vb;
	struct vb2_buffer *vb = &v4l2_buf->vb2_buf;
	struct list_head head;

	vb->vb2_queue = &pkt_ctx.vq;
	ctx->m2m_ctx->out_q_ctx.rdy_queue.prev = &head;
	ctx->m2m_ctx->out_q_ctx.rdy_queue.prev->next = &ctx->m2m_ctx->out_q_ctx.rdy_queue;
	vb->vb2_queue->drv_priv = ctx;
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	pkt_ops->vb2_buf_queue(vb);
}

static void pablo_gdc_vb2_lock_unlock_kunit_test(struct kunit *test)
{
	struct vb2_queue *q = &pkt_ctx.vq;
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;

	mutex_init(&gdc->lock);

	q->drv_priv = &pkt_ctx.ctx;

	pkt_ops->vb2_lock(q);
	pkt_ops->vb2_unlock(q);
}

static void pablo_gdc_cleanup_queue_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;

	pkt_ops->cleanup_queue(ctx);
}

static void pablo_gdc_vb2_streaming_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *q = &pkt_ctx.vq;
	int test_result;

	q->drv_priv = ctx;

	clear_bit(CTX_STREAMING, &ctx->flags);
	test_result = pkt_ops->vb2_start_streaming(q, 0);
	KUNIT_EXPECT_EQ(test, test_bit(CTX_STREAMING, &ctx->flags), 1);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	ctx->gdc_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	pkt_ops->vb2_stop_streaming(q);
	KUNIT_EXPECT_EQ(test, test_bit(CTX_STREAMING, &ctx->flags), 0);
	pkt_ops->v4l2_m2m_release(ctx->gdc_dev->m2m.m2m_dev);
}

static void pablo_gdc_queue_init_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *src, *dst;
	int test_result;

	src = kunit_kzalloc(test, sizeof(struct vb2_queue), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, src);
	dst = kunit_kzalloc(test, sizeof(struct vb2_queue), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dst);

	test_result = pkt_ops->queue_init(ctx, src, dst);
	KUNIT_EXPECT_EQ(test, src->timestamp_flags, V4L2_BUF_FLAG_TIMESTAMP_COPY);
	KUNIT_EXPECT_EQ(test, src->type, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, dst->timestamp_flags, V4L2_BUF_FLAG_TIMESTAMP_COPY);
	KUNIT_EXPECT_EQ(test, dst->type, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, src);
	kunit_kfree(test, dst);
}

static void pablo_gdc_pmio_init_exit_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	int test_result;

	gdc->regs_base = kunit_kzalloc(test, 0x10000, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->regs_base);

	gdc->regs_rsc = kunit_kzalloc(test, sizeof(struct resource), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->regs_rsc);

	test_result = pkt_ops->pmio_init(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->pmio_exit(gdc);

	kunit_kfree(test, gdc->regs_rsc);
	kunit_kfree(test, gdc->regs_base);
}

static void pablo_gdc_pmio_config_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	struct c_loader_buffer clb;
	u32 num_of_headers;
	struct c_loader_header clh;
	ulong tmp_dbg_gdc = pablo_get_dbg_gdc();
	ulong set_dbg_gdc = tmp_dbg_gdc;

	gdc->pb_c_loader_payload = &pkt_ctx.payload;
	gdc->pb_c_loader_header = &pkt_ctx.payload;
	gdc->pb_c_loader_payload->ops = &pkt_buf_ops;
	gdc->pb_c_loader_header->ops = &pkt_buf_ops;
	gdc->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->pmio);

	/* TC : normal process */
	clb.num_of_pairs = 1;
	num_of_headers = clb.num_of_headers;
	pkt_ops->pmio_config(gdc, &clb);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, num_of_headers++);

	/* TC : not GDC_DBG_PMIO_MODE */
	set_bit(GDC_DBG_PMIO_MODE, &set_dbg_gdc);
	pablo_set_dbg_gdc(set_dbg_gdc);
	clb.clh = &clh;
	pkt_ops->pmio_config(gdc, &clb);
	KUNIT_EXPECT_NULL(test, clb.clh);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, (u32)0);

	/* TC : not GDC_DBG_DUMP_PMIO_CACHE */
	clear_bit(GDC_DBG_PMIO_MODE, &set_dbg_gdc);
	set_bit(GDC_DBG_DUMP_PMIO_CACHE, &set_dbg_gdc);
	pablo_set_dbg_gdc(set_dbg_gdc);
	num_of_headers = clb.num_of_headers;
	pkt_ops->pmio_config(gdc, &clb);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, num_of_headers++);

	pablo_set_dbg_gdc(tmp_dbg_gdc);
	kunit_kfree(test, gdc->pmio);
}

static void gdc_watchdog(struct timer_list *t)
{
}

static void pablo_gdc_run_next_job_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	INIT_LIST_HEAD(&gdc->context_list);
	list_add_tail(&ctx->node, &gdc->context_list);

	ctx->gdc_dev->sys_ops = &pkt_gdc_sys_ops;
	ctx->gdc_dev->hw_gdc_ops = &pkt_hw_gdc_ops;

	timer_setup(&gdc->wdt.timer, gdc_watchdog, 0);

	gdc->context_list.next = (struct list_head *)ctx;
	gdc->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();

	/* TC : gdc param update done */
	gdc->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->pmio);
	test_result = pkt_ops->run_next_job(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : a job is currently being processed or no job is to run */
	test_result = pkt_ops->run_next_job(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->v4l2_m2m_release(gdc->m2m.m2m_dev);
	kunit_kfree(test, gdc->pmio);
	del_timer(&gdc->wdt.timer);
}

static void pablo_gdc_add_context_and_run_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	gdc->current_ctx = &pkt_ctx.ctx;
	INIT_LIST_HEAD(&gdc->context_list);
	test_result = pkt_ops->add_context_and_run(gdc, ctx);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_out_votf_otf_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct gdc_ctx *ctx = &pkt_ctx.ctx;

	pkt_ops->out_votf_otf(gdc, ctx);
	KUNIT_EXPECT_EQ(test, (int)atomic_read(&gdc->wait_mfc), (int)1);

	/* TODO : gdc->gdc_ready_frame_cb is not NULL */
	/* need to worry for v4l2_m2m_next_src_buf, v4l2_m2m_next_dst_buf */
}

static void pablo_gdc_m2m_device_run_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct v4l2_m2m_buffer m2m_buffer;

	/* TC : GDC is in suspend state */
	set_bit(DEV_SUSPEND, &ctx->gdc_dev->state);
	pkt_ops->m2m_device_run(ctx);

	/* TC : aborted GDC device run */
	clear_bit(DEV_SUSPEND, &ctx->gdc_dev->state);
	set_bit(CTX_ABORT, &ctx->flags);
	pkt_ops->m2m_device_run(ctx);

	/* TC : out_mode == GDC_OUT_VOTF */
	clear_bit(CTX_ABORT, &ctx->flags);
	INIT_LIST_HEAD(&ctx->m2m_ctx->out_q_ctx.rdy_queue);
	list_add_tail(&m2m_buffer.list, &ctx->m2m_ctx->out_q_ctx.rdy_queue);
	ctx->crop_param[1].votf_en = true;
	pkt_ops->m2m_device_run(ctx);
}

static void pablo_gdc_poll_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct poll_table_struct wait;
	int test_result;

	file->private_data = &pkt_ctx.ctx.fh;

	test_result = pkt_ops->poll(file, &wait);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_mmap_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct vm_area_struct vma;
	int test_result;

	file->private_data = &pkt_ctx.ctx.fh;

	test_result = pkt_ops->mmap(file, &vma);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_gdc_m2m_job_abort_kunit_test(struct kunit *test)
{
	struct gdc_ctx *ctx = &pkt_ctx.ctx;

	ctx->gdc_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	pkt_ops->m2m_job_abort(ctx);
	pkt_ops->v4l2_m2m_release(ctx->gdc_dev->m2m.m2m_dev);
}

static void pablo_gdc_device_run_kunit_test(struct kunit *test)
{
	struct platform_device *pdev = &pkt_ctx.pdev;
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct device *backup = gdc_get_device();
	int test_result;

	/* TC : no gdc_dev */
	test_result = pkt_ops->device_run(0);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	/* TC : no votf_ctx */
	pdev->dev.driver_data = gdc;
	gdc_set_device(&pdev->dev);
	test_result = pkt_ops->device_run(0);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	/* TC : no mfc i_ino */
	gdc->votf_ctx = ctx;
	test_result = pkt_ops->device_run(0);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TODO : add successful TC */

	gdc_set_device(backup);
}

static int kunit_test_gdc_ready_frame_cb(unsigned long i_ino)
{
	return 0;
}

static void pablo_gdc_register_ready_frame_cb_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct platform_device *pdev = &pkt_ctx.pdev;
	struct device *backup = gdc_get_device();
	int test_result;

	/* TC : no gdc_dev */
	test_result = pkt_ops->register_ready_frame_cb(NULL);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	/* TC : no gdc_ready_frame_cb */
	pdev->dev.driver_data = gdc;
	gdc_set_device(&pdev->dev);
	test_result = pkt_ops->register_ready_frame_cb(NULL);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	/* TC : gdc_ready_frame_cb */
	test_result = pkt_ops->register_ready_frame_cb(kunit_test_gdc_ready_frame_cb);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	gdc_set_device(backup);
}

static void pablo_gdc_unregister_ready_frame_cb_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct platform_device *pdev = &pkt_ctx.pdev;
	struct device *backup = gdc_get_device();

	/* TC : no gdc_dev */
	pkt_ops->unregister_ready_frame_cb();

	/* TC : add successful TC */
	pdev->dev.driver_data = gdc;
	gdc_set_device(&pdev->dev);
	gdc->gdc_ready_frame_cb = kunit_test_gdc_ready_frame_cb;
	pkt_ops->unregister_ready_frame_cb();
	KUNIT_EXPECT_NULL(test, gdc->gdc_ready_frame_cb);

	gdc_set_device(backup);
}

static void pablo_gdc_clk_get_put_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	int test_result;

	test_result = pkt_ops->clk_get(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->clk_put(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_sysmmu_fault_handler_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct iommu_fault *fault;
	int test_result;

	fault = kunit_kzalloc(test, sizeof(struct iommu_fault), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fault);

	gdc->dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gdc->dev);

	test_result = pkt_ops->sysmmu_fault_handler(fault, gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, gdc->dev);
	kunit_kfree(test, fault);
}

static void pablo_gdc_shutdown_suspend_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct platform_device *pdev = &pkt_ctx.pdev;

	pdev->dev.driver_data = gdc;
	pkt_ops->shutdown(pdev);
	KUNIT_EXPECT_EQ(test, (int)test_bit(DEV_SUSPEND, &gdc->state), 1);

	pkt_ops->suspend(&pdev->dev);
	KUNIT_EXPECT_EQ(test, (int)test_bit(DEV_SUSPEND, &gdc->state), 1);
}

static void pablo_gdc_runtime_resume_suspend_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = &pkt_ctx.gdc;
	struct platform_device *pdev = &pkt_ctx.pdev;
	int test_result;

	pdev->dev.driver_data = gdc;
	test_result = pkt_ops->runtime_resume(&pdev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	test_result = pkt_ops->runtime_suspend(&pdev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_gdc_alloc_free_pmio_mem_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	int test_result;

	gdc->mem.is_mem_ops = &pkt_mem_ops_ion;

	/* TC : failed to allocate buffer for c-loader payload line1308*/
	test_result = pkt_ops->alloc_pmio_mem(gdc);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	pkt_mem_ops_ion.alloc = pkt_ion_alloc;

	test_result = pkt_ops->alloc_pmio_mem(gdc);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->free_pmio_mem(gdc);
}

static const struct gdc_fmt gdc_formats[] = {
	{
		.name = "YUV422 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV16M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	},
	{
		.name = "YUV420 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV12M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	},
	{
		.name = "YUV 4:2:0 planar, YCbCr",
		.pixelformat = V4L2_PIX_FMT_YVU420,
		.bitperpixel = { 8, 4, 4 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
	},
	{
		.name = "YUV 4:2:0 planar, YCbCr",
		.pixelformat = V4L2_PIX_FMT_YUV420N,
		.bitperpixel = { 8, 4, 4 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
	},
	{
		.name = "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat = V4L2_PIX_FMT_NV21,
		.bitperpixel = { 12 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	},
};

static void pablo_gdc_job_finish_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;

	gdc->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();

	/* TC : ctx is NULL */
	pkt_ops->job_finish(gdc, NULL);
	pkt_ops->v4l2_m2m_release(gdc->m2m.m2m_dev);

	/* TODO : add successful TC */
}

static void pablo_gdc_register_m2m_device_kunit_test(struct kunit *test)
{
	/* TODO : add successful TC */
	/* TODO : add unregister_m2m_device TC */
}

static void pablo_gdc_init_frame_kunit_test(struct kunit *test)
{
	struct gdc_frame *frame = &pkt_ctx.frame;

	frame->addr.cb = 1;
	frame->addr.cr = 1;
	frame->addr.y_2bit = 1;
	frame->addr.cbcr_2bit = 1;
	frame->addr.cbsize = 1;
	frame->addr.crsize = 1;
	frame->addr.ysize_2bit = 1;
	frame->addr.cbcrsize_2bit = 1;

	pkt_ops->init_frame(frame);

	KUNIT_EXPECT_EQ(test, frame->addr.cb, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.cr, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.y_2bit, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.cbcr_2bit, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.cbsize, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.crsize, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize_2bit, 0);
	KUNIT_EXPECT_EQ(test, frame->addr.cbcrsize_2bit, 0);
}

static void pablo_gdc_get_bufaddr_8_plus_2_fmt_kunit_test(struct kunit *test)
{
	struct gdc_frame *frame = &pkt_ctx.frame;
	unsigned int w = 320;
	unsigned int h = 240;

	/* TC : pixelformat == V4L2_PIX_FMT_NV16M_S10B */
	frame->gdc_fmt = &gdc_formats[0];
	pkt_ops->get_bufaddr_8_plus_2_fmt(frame, w, h);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize_2bit, NV16M_Y_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.y_2bit, frame->addr.y + NV16M_Y_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cbcrsize_2bit, NV16M_CBCR_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cbcr_2bit, frame->addr.cb + NV16M_CBCR_SIZE(w, h));

	/* TC : pixelformat == V4L2_PIX_FMT_NV12M_S10B */
	frame->gdc_fmt = &gdc_formats[1];
	pkt_ops->get_bufaddr_8_plus_2_fmt(frame, w, h);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize_2bit, NV12M_Y_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.y_2bit, frame->addr.y + NV12M_Y_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cbcrsize_2bit, NV12M_CBCR_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cbcr_2bit, frame->addr.cb + NV12M_CBCR_SIZE(w, h));
}

static void pablo_gdc_get_bufaddr_frame_fmt_kunit_test(struct kunit *test)
{
	struct gdc_frame *frame = &pkt_ctx.frame;
	struct v4l2_m2m_buffer m2m_buffer;
	struct vb2_v4l2_buffer *v4l2_buf = &m2m_buffer.vb;
	struct vb2_buffer *vb = &v4l2_buf->vb2_buf;
	unsigned int pixsize, bytesize;
	unsigned int w, h;

	frame->width = 320;
	frame->height = 240;
	pixsize = frame->width * frame->height;

	/* TC : gdc_fmt->num_planes is set to 1 */
	frame->gdc_fmt = &gdc_formats[2];
	bytesize = (pixsize * frame->gdc_fmt->bitperpixel[0]) >> 3;
	pkt_ops->get_bufaddr_frame_fmt(frame, pixsize, bytesize, vb);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize, pixsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cbsize, ALIGN(frame->width >> 1, 16) * (frame->height >> 1));
	KUNIT_EXPECT_EQ(test, frame->addr.crsize, frame->addr.cbsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cb, frame->addr.y + pixsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cr, frame->addr.cb + frame->addr.cbsize);

	frame->gdc_fmt = &gdc_formats[3];
	bytesize = (pixsize * frame->gdc_fmt->bitperpixel[0]) >> 3;
	w = frame->width;
	h = frame->height;
	pkt_ops->get_bufaddr_frame_fmt(frame, pixsize, bytesize, vb);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize, YUV420N_Y_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cbsize, YUV420N_CB_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.crsize, YUV420N_CR_SIZE(w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cb, YUV420N_CB_BASE(frame->addr.y, w, h));
	KUNIT_EXPECT_EQ(test, frame->addr.cr, YUV420N_CR_BASE(frame->addr.y, w, h));

	frame->gdc_fmt = &gdc_formats[4];
	pkt_ops->get_bufaddr_frame_fmt(frame, pixsize, bytesize, vb);
	KUNIT_EXPECT_EQ(test, frame->addr.ysize, pixsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cbsize, (bytesize - pixsize) / 2);
	KUNIT_EXPECT_EQ(test, frame->addr.crsize, frame->addr.cbsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cb, frame->addr.y + pixsize);
	KUNIT_EXPECT_EQ(test, frame->addr.cr, frame->addr.cb + frame->addr.cbsize);

	/* TODO : add gdc_fmt->num_planes is set to 3 */
	/* need to confirm gdc_get_dma_address */
}

static void pablo_gdc_get_bufaddr_kunit_test(struct kunit *test)
{
	struct gdc_dev *gdc = pkt_ctx.ctx.gdc_dev;
	struct gdc_ctx *ctx = &pkt_ctx.ctx;
	struct gdc_frame *frame = &pkt_ctx.frame;
	struct v4l2_m2m_buffer m2m_buffer;
	struct vb2_v4l2_buffer *v4l2_buf = &m2m_buffer.vb;
	struct vb2_buffer *vb = &v4l2_buf->vb2_buf;
	int test_result;

	/* TC : failed to get result for gdc_get_dma_address */
	frame->gdc_fmt = &gdc_formats[2];
	test_result = pkt_ops->get_bufaddr(gdc, ctx, vb, frame);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TODO : add successful TC */
}

static int backup_log_level;
static int pablo_gdc_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	pkt_ops = pablo_kunit_get_gdc();

	backup_log_level = pkt_ops->get_log_level();
	pkt_ops->set_log_level(1);

	pkt_ctx.gdc.dev = &pkt_ctx.dev;
	pkt_ctx.ctx.gdc_dev = &pkt_ctx.gdc;
	pkt_ctx.ctx.m2m_ctx = &pkt_ctx.m2m_ctx;
	pkt_ctx.g_fmt.num_planes = 1;
	pkt_ctx.file.f_inode = &pkt_ctx.f_inode;

	return 0;
}

static void pablo_gdc_kunit_test_exit(struct kunit *test)
{
	pkt_ops->set_log_level(backup_log_level);
	pkt_ops = NULL;
}

static struct kunit_case pablo_gdc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_gdc_get_debug_gdc_kunit_test),
	KUNIT_CASE(pablo_gdc_votf_set_service_cfg_kunit_test),
	KUNIT_CASE(pablo_gdc_votfitf_set_flush_kunit_test),
	KUNIT_CASE(pablo_gdc_find_format_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_querycap_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_g_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_try_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_gdc_image_bound_check_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_s_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_reqbufs_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_querybuf_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_qbuf_kunit_test),
	KUNIT_CASE(pablo_gdc_check_qbuf_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_qbuf_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_dqbuf_kunit_test),
	KUNIT_CASE(pablo_gdc_power_clk_enable_kunit_test),
	KUNIT_CASE(pablo_gdc_power_clk_disable_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_stream_on_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_stream_off_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_s_ctrl_kunit_test),
	KUNIT_CASE(pablo_gdc_v4l2_s_ext_ctrls_kunit_test),
	KUNIT_CASE(pablo_gdc_ctx_stop_req_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_queue_setup_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_buf_prepare_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_buf_finish_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_buf_queue_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_lock_unlock_kunit_test),
	KUNIT_CASE(pablo_gdc_cleanup_queue_kunit_test),
	KUNIT_CASE(pablo_gdc_vb2_streaming_kunit_test),
	KUNIT_CASE(pablo_gdc_queue_init_kunit_test),
	KUNIT_CASE(pablo_gdc_pmio_init_exit_kunit_test),
	KUNIT_CASE(pablo_gdc_pmio_config_kunit_test),
	KUNIT_CASE(pablo_gdc_run_next_job_kunit_test),
	KUNIT_CASE(pablo_gdc_add_context_and_run_kunit_test),
	KUNIT_CASE(pablo_gdc_out_votf_otf_kunit_test),
	KUNIT_CASE(pablo_gdc_m2m_device_run_kunit_test),
	KUNIT_CASE(pablo_gdc_poll_kunit_test),
	KUNIT_CASE(pablo_gdc_mmap_kunit_test),
	KUNIT_CASE(pablo_gdc_m2m_job_abort_kunit_test),
	KUNIT_CASE(pablo_gdc_device_run_kunit_test),
	KUNIT_CASE(pablo_gdc_register_ready_frame_cb_kunit_test),
	KUNIT_CASE(pablo_gdc_unregister_ready_frame_cb_kunit_test),
	KUNIT_CASE(pablo_gdc_clk_get_put_kunit_test),
	KUNIT_CASE(pablo_gdc_sysmmu_fault_handler_kunit_test),
	KUNIT_CASE(pablo_gdc_shutdown_suspend_kunit_test),
	KUNIT_CASE(pablo_gdc_runtime_resume_suspend_kunit_test),
	KUNIT_CASE(pablo_gdc_alloc_free_pmio_mem_kunit_test),
	KUNIT_CASE(pablo_gdc_job_finish_kunit_test),
	KUNIT_CASE(pablo_gdc_register_m2m_device_kunit_test),
	KUNIT_CASE(pablo_gdc_init_frame_kunit_test),
	KUNIT_CASE(pablo_gdc_get_bufaddr_8_plus_2_fmt_kunit_test),
	KUNIT_CASE(pablo_gdc_get_bufaddr_frame_fmt_kunit_test),
	KUNIT_CASE(pablo_gdc_get_bufaddr_kunit_test),
	{},
};

struct kunit_suite pablo_gdc_kunit_test_suite = {
	.name = "pablo-gdc-kunit-test",
	.init = pablo_gdc_kunit_test_init,
	.exit = pablo_gdc_kunit_test_exit,
	.test_cases = pablo_gdc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_gdc_kunit_test_suite);

MODULE_LICENSE("GPL");
