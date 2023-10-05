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

#include <linux/vmalloc.h>

#include "pablo-kunit-test.h"
#include "pablo-binary.h"
#include "pablo-debug.h"

static struct pablo_kunit_test_ctx {
	struct is_debug *debug;
	struct is_debug_event *event;

	struct memlog_obj mobj;
	const struct pablo_memlog_operations *pml_ops_b;

	struct is_minfo minfo;

	int event_index;
	int critical_log_tail;
	int normal_log_tail;
	int overflow_csi;
	int overflow_3aa;

	const struct pablo_bcm_operations *pbcm_ops_b;
	bool bcm_flag;

	const struct pablo_dss_operations *pdss_ops_b;
	bool dss_flag;
} pkt_ctx;

#define DOT_INFO_LINE_OFS	4
static inline char *pkt_debug_get_dot_infos(char *buf, u32 index)
{
	int line_i;

	for (line_i = 0; line_i < DOT_INFO_LINE_OFS + index; line_i++)
		strsep(&buf, "\n");

	return strim(strsep(&buf, "\n"));
}

static inline bool pkt_debug_cmp_dot_infos(char *left, char *right)
{
	unsigned char c1, c2;

	while (*left != '\0' && *right != '\0') {
		left = skip_spaces(left);
		right = skip_spaces(right);
		c1 = *left++;
		c2 = *right++;
		if (c1 != c2)
			return false;
		else if (c1 == '\0')
			break;
	}

	return true;
}

static inline bool pkt_debug_is_empty_buf(u32 *buf, size_t size)
{
	int i;

	for (i = 0; i < (size / sizeof(*buf)); i++) {
		if (buf[i])
			return false;
	}

	return true;
}

static struct memlog_obj *pkt_debug_memlog_alloc_printf(struct memlog *desc, size_t size, struct memlog_obj *file_obj,
		const char *name, u32 uflag)
{
	if (!size)
		return NULL;

	return &pkt_ctx.mobj;
}

static struct memlog_obj *pkt_debug_memlog_alloc_dump(struct memlog *desc, size_t size, phys_addr_t paddr, bool is_sfr,
		struct memlog_obj *file_objs, const char *name)
{
	if (!size)
		return NULL;

	return &pkt_ctx.mobj;
}

static int pkt_debug_memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	if (!obj->enabled)
		return -EPERM;

	return 0;
}

const static struct pablo_memlog_operations pml_ops_mock = {
	.alloc_printf = pkt_debug_memlog_alloc_printf,
	.alloc_dump = pkt_debug_memlog_alloc_dump,
	.do_dump = pkt_debug_memlog_do_dump,
};

static void pkt_bcm_dbg_start(void)
{
	pkt_ctx.bcm_flag = true;
}

static void pkt_bcm_dbg_stop(unsigned int bcm_stop_owner)
{
	pkt_ctx.bcm_flag = false;
}

const static struct pablo_bcm_operations pbcm_ops_mock = {
	.start = pkt_bcm_dbg_start,
	.stop = pkt_bcm_dbg_stop,
};

static int pkt_dss_expire_watchdog(void)
{
	pkt_ctx.dss_flag = true;

	return 0;
}

const static struct pablo_dss_operations pdss_ops_mock = {
	.expire_watchdog = pkt_dss_expire_watchdog,
};

/* Define the test cases. */

static void pablo_debug_draw_digit_set_kunit_test(struct kunit *test)
{
	const struct kernel_param *kp = pablo_debug_get_kernel_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	int digit_ctrl;
	char *buf = (char *)kunit_kzalloc(test, XATTR_SIZE_MAX, 0);
	char *str;
	bool cmp;

	/* TC #1. Single argument. */
	kp->ops->set("3", kp);

	digit_ctrl = is_get_debug_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	KUNIT_EXPECT_EQ(test, digit_ctrl, 3);

	/* TC #2. Not enough arguments. */
	kp->ops->set("0 1 2 3", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 1 2 3");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #3. Invalid DOT format. */
	kp->ops->set("10 1 2 3 4", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "10: 1 2 3 4");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #4. Invalid DOT scaling ratio. */
	kp->ops->set("0 2.5 2 3 4", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 2.5 2 3 4");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #5. Invalid DOT align. */
	kp->ops->set("0 1 10 3 4", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 1 10 3 4");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #6. Invalid DOT offset_y. */
	kp->ops->set("0 1 2 3.5 4", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 1 2 3.5 4");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #7. Invalid DOT offset_y. */
	kp->ops->set("0 1 2 3 4.5", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 1 2 3 4.5");
	KUNIT_EXPECT_FALSE(test, cmp);

	/* TC #8. Valid arguments. */
	kp->ops->set("0 1 2 3 4", kp);

	kp->ops->get(buf, kp);
	str = pkt_debug_get_dot_infos(buf, 0);
	cmp = pkt_debug_cmp_dot_infos(str, "0: 1 2 3 4");
	KUNIT_EXPECT_TRUE(test, cmp);

	/* Retore draw_digit */
	kp->ops->set("0 16 0 0 0", kp);
	kp->ops->set("0", kp);

	kunit_kfree(test, buf);
}

static void pablo_debug_draw_digit_kunit_test(struct kunit *test)
{
	const struct kernel_param *kp = pablo_debug_get_kernel_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	int digit_ctrl;
	struct is_debug_dma_info dinfo = {100, 100, 0, 0, 0, 0UL}; /* IMG: 100x100 for drawing single digit */
	size_t sz = 2 * dinfo.width * dinfo.height; /* Maximum 2Bytes per pixel */
	char *img_buf = (char *)kunit_kzalloc(test, sz, 0);
	bool is_empty;

	kp->ops->set("2", kp);
	digit_ctrl = is_get_debug_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	KUNIT_ASSERT_EQ(test, digit_ctrl, 2);

	/* TC #1. Invalid arguments */
	dinfo.addr = 0x0UL;

	is_dbg_draw_digit(NULL, 1);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_TRUE(test, is_empty);

	/* TC #2. Invalid format */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_8BIT;
	dinfo.bpp = 8;
	dinfo.pixelformat = v4l2_fourcc('F', 'F', 'F', 'F');
	dinfo.addr = (ulong)img_buf;

	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_TRUE(test, is_empty);

	/* TC #3. YUV422 format, LEFT_TOP */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_8BIT;
	dinfo.bpp = 16;
	dinfo.pixelformat = V4L2_PIX_FMT_YUYV;

	kp->ops->set("6 1 0 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #4. SBGGR10P format, CENTER_TOP */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_PACKED_10BIT;
	dinfo.bpp = 10;
	dinfo.pixelformat = V4L2_PIX_FMT_SBGGR10P;
	kp->ops->set("1 1 1 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #5. Signed SBGGR12P format, RIGHT_TOP */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_13BIT;
	dinfo.bpp = 13;
	dinfo.pixelformat = V4L2_PIX_FMT_SBGGR12P;
	kp->ops->set("3 1 2 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #6. SBGGR12P format, LEFT_CENTER */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_12BIT;
	dinfo.bpp = 12;
	dinfo.pixelformat = V4L2_PIX_FMT_SBGGR12P;
	kp->ops->set("2 1 3 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #7. YUV420 format, CENTER_CENTER */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_8BIT;
	dinfo.bpp = 8;
	dinfo.pixelformat = V4L2_PIX_FMT_NV21;
	kp->ops->set("5 1 4 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #8. GREY format, RIGHT_CENTER */
	memset(img_buf, 0, sz);
	dinfo.pixeltype = CAMERA_PIXEL_SIZE_8BIT;
	dinfo.bpp = 8;
	dinfo.pixelformat = V4L2_PIX_FMT_GREY;
	kp->ops->set("0 1 5 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #9. GREY format, LEFT_BOTTOM */
	memset(img_buf, 0, sz);
	kp->ops->set("0 1 6 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #10. GREY format, CENTER_BOTTOM */
	memset(img_buf, 0, sz);
	kp->ops->set("0 1 7 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* TC #11. GREY format, RIGHT_BOTTOM */
	memset(img_buf, 0, sz);
	kp->ops->set("0 1 8 0 0", kp);
	is_dbg_draw_digit(&dinfo, 1);

	is_empty = pkt_debug_is_empty_buf((u32 *)img_buf, sz);
	KUNIT_EXPECT_FALSE(test, is_empty);

	/* Retore draw_digit */
	kp->ops->set("0 16 0 0 0", kp);
	kp->ops->set("1 2 4 -2 0", kp);
	kp->ops->set("2 4 4 -2 0", kp);
	kp->ops->set("3 1 4 0 0", kp);
	kp->ops->set("5 8 4 2 0", kp);
	kp->ops->set("6 4 4 2 0", kp);
	kp->ops->set("0", kp);

	kunit_kfree(test, img_buf);
}

static void pablo_debug_get_debug_param_kunit_test(struct kunit *test)
{
	int ret;

	/* TC #1. Invalid argument */
	ret = is_get_debug_param(IS_DEBUG_PARAM_MAX);
	KUNIT_EXPECT_LT(test, ret, (int)0);

	/* Normal cases are touched by other TCs. */
}

static void pablo_debug_get_sysfs_kunit_test(struct kunit *test)
{
	ulong val;

	/* TC #1. sysfs_debug */
	val = (ulong)is_get_sysfs_debug();
	KUNIT_EXPECT_NE(test, val, (ulong)0);

	/* TC #2. sysfs_sensor */
	val = (ulong)is_get_sysfs_sensor();
	KUNIT_EXPECT_NE(test, val, (ulong)0);

	/* TC #3. sysfs_actuator */
	val = (ulong)is_get_sysfs_actuator();
	KUNIT_EXPECT_NE(test, val, (ulong)0);

	/* TC #4. sysfs_actuator */
	val = (ulong)is_get_sysfs_eeprom();
	KUNIT_EXPECT_NE(test, val, (ulong)0);
}

static void pablo_debug_sysfs_debug_kunit_test(struct kunit *test)
{
	struct is_sysfs_debug *sysfs_debug;

	sysfs_debug = is_get_sysfs_debug();
	KUNIT_ASSERT_TRUE(test, sysfs_debug != 0);

	sysfs_debug->emulate_start_irq(0);
}

static void pablo_debug_dmsg_concate_kunit_test(struct kunit *test)
{
	char *str;
	u32 i;

	/* TC #1. is_dmsg_init() */
	is_dmsg_init();

	str = is_dmsg_print();
	KUNIT_EXPECT_STREQ(test, str, "");

	/* TC #2. is_dmsg_concate() */
	for (i = 0; i < 5; i++)
		is_dmsg_concate("test%d", i);

	str = is_dmsg_print();
	KUNIT_EXPECT_STREQ(test, str, "test0test1test2test3test4");
}
static void pablo_debug_print_buffer_kunit_test(struct kunit *test)
{
	size_t len = 300;
	char *buf = (char *)kunit_kzalloc(test, len, 0);
	char wd[7] = {0, };
	u32 i;

	/* TC #1. is_print_buffer() */
	for (i = 0; i < (len / 6); i++) {
		snprintf(wd, sizeof(wd), "test%d\n", (i % 10));
		strcat(buf, wd);
	}

	is_print_buffer(buf, len);

	kunit_kfree(test, buf);
}

static void pablo_debug_memlog_alloc_dump_kunit_test(struct kunit *test)
{
	int ret;
	struct is_debug *debug = pkt_ctx.debug;
	struct pablo_mobj_dump *mobj_dump;
	struct memlog_obj *obj_b;
	int idx;

	/* Backup memlog dump object info */
	idx = atomic_read(&debug->memlog.dump_nums);
	mobj_dump = &debug->memlog.dump[idx];
	obj_b = mobj_dump->obj;
	mobj_dump->obj = NULL;

	/* TC #1. Invalid size */
	ret = is_debug_memlog_alloc_dump(0x12345678, 0, "test");
	KUNIT_EXPECT_LT(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)mobj_dump->obj, NULL);

	/* TC #2. Normal case */
	ret = is_debug_memlog_alloc_dump(0x12345678, 10, "test");
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mobj_dump->obj);
	KUNIT_EXPECT_EQ(test, mobj_dump->size, (unsigned long)10);
	KUNIT_EXPECT_STREQ(test, (char *)mobj_dump->name, "test");
	KUNIT_EXPECT_GT(test, atomic_read(&debug->memlog.dump_nums), idx);

	/* Restore memlog dump object info */
	atomic_set(&debug->memlog.dump_nums, idx);
	mobj_dump->obj = obj_b;
}

static void pablo_debug_memlog_dump_cr_all_kunit_test(struct kunit *test)
{
	is_debug_memlog_dump_cr_all(0);
}

static void pablo_debug_open_close_kunit_test(struct kunit *test)
{
	struct is_debug *debug = pkt_ctx.debug;
	bool is_set;

	/* TC #1. open */
	is_debug_open(&pkt_ctx.minfo);
	is_set = test_bit(IS_DEBUG_OPEN, &debug->state);
	KUNIT_EXPECT_EQ(test, *((int *)(pkt_ctx.minfo.kvaddr_debug_cnt)), (int)0);
	KUNIT_EXPECT_EQ(test, debug->read_vptr, (size_t)0);
	KUNIT_EXPECT_PTR_EQ(test, debug->minfo, &pkt_ctx.minfo);
	KUNIT_EXPECT_TRUE(test, is_set);

	/* TC #2. close */
	is_debug_close();
	is_set = test_bit(IS_DEBUG_OPEN, &debug->state);
	KUNIT_EXPECT_FALSE(test, is_set);
}

static void pablo_debug_dma_dump_kunit_test(struct kunit *test)
{
	struct vb2_queue *vbq;
	struct vb2_buffer *buf;
	struct is_queue *queue;
	struct is_frame *frame;
	const size_t sz = 4;
	int plane_i, ret;

	/* Setup required data structures */
	vbq = (struct vb2_queue *)kunit_kzalloc(test, sizeof(struct vb2_queue), 0);
	buf = (struct vb2_buffer *)kunit_kzalloc(test, sizeof(struct vb2_buffer), 0);
	queue = (struct is_queue *)kunit_kzalloc(test, sizeof(struct is_queue), 0);
	frame = (struct is_frame *)kunit_kzalloc(test, sizeof(struct is_frame), 0);

	buf->num_planes = 3; /* IMG plane(2) + META plane(1) */
	vbq->bufs[0] = buf;
	queue->vbq = vbq;
	queue->framemgr.frames = frame;

	/* TC #1. DUMP_IMAGE w/o valid buf_kva */
	ret = is_dbg_dma_dump(queue, 0, 0, 0, DBG_DMA_DUMP_IMAGE);
	KUNIT_EXPECT_LT(test, ret, (int)0);

	/**
	 * TC #2. DUMP_IMAGE w valid buf_kva
	 * - Expectation: Error
	 * - The kernel file write operation is not supported by default.
	 */
	for (plane_i = 0; plane_i < buf->num_planes; plane_i++) {
		queue->framecfg.size[plane_i] = sz;
		queue->buf_kva[0][plane_i] = (ulong)kunit_kmalloc(test, sz, 0);
	}

	ret = is_dbg_dma_dump(queue, 0, 0, 0, DBG_DMA_DUMP_IMAGE);
	KUNIT_EXPECT_LT(test, ret, (int)0);

	/* TC #3. DUMP_META */
	ret = is_dbg_dma_dump(queue, 0, 0, 0, DBG_DMA_DUMP_META);
	KUNIT_EXPECT_LT(test, ret, (int)0);

	for (plane_i -= 1; plane_i >= 0; plane_i--)
		kunit_kfree(test, (void *)queue->buf_kva[0][plane_i]);

	kunit_kfree(test, (void *)frame);
	kunit_kfree(test, (void *)queue);
	kunit_kfree(test, (void *)buf);
	kunit_kfree(test, (void *)vbq);
}

static void pablo_debug_islib_debug_fops_kunit_test(struct kunit *test)
{
	struct is_debug *debug = pkt_ctx.debug;
	struct inode *inode;
	struct file *file;
	char *usr_buf;
	size_t sz = (1 << 8); /* 256B */
	int ret;
	int *dbg_cnt;

	/* Setup required data structures */
	inode = (struct inode *)kunit_kzalloc(test, sizeof(struct inode), 0);
	file = (struct file *)kunit_kzalloc(test, sizeof(struct file), 0);
	usr_buf = (char *)kunit_kzalloc(test, 0x400, 0);
	pkt_ctx.minfo.kvaddr_debug = (ulong)kunit_kzalloc(test, (sz << 2), 0);
	dbg_cnt = (int *)(pkt_ctx.minfo.kvaddr_debug_cnt);

	/* TC #1. File open */
	inode->i_private = (void *)debug;

	ret = debug->dbg_log_fops->open(inode, file);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, file->private_data, inode->i_private);

	/* TC #2. File read w/o open debug */
	ret = debug->dbg_log_fops->read(file, usr_buf, sz, NULL);
	KUNIT_EXPECT_EQ(test, ret, (int)0);

	is_debug_open(&pkt_ctx.minfo);

	/**
	 * TC #3. File read: read_vptr < write_vptr
	 *  Because the 'usr_buf' isn't real user space memory,
	 *  the 'read' ops would return error.
	 */
	debug->read_vptr = 33;
	*dbg_cnt = debug->read_vptr + sz + 1;

	ret = debug->dbg_log_fops->read(file, usr_buf, sz, NULL);
	KUNIT_EXPECT_EQ(test, ret, (int)sz);

	/**
	 * TC #4. File read: read_vptr > write_vptr
	 *  Because the 'usr_buf' isn't real user space memory,
	 *  the 'read' ops would return error.
	 */
	*dbg_cnt = DEBUG_REGION_SIZE;
	debug->read_vptr = sz + 1;

	ret = debug->dbg_log_fops->read(file, usr_buf, sz, NULL);
	KUNIT_EXPECT_EQ(test, ret, (int)sz);

	/* TC #5. File read: user_buf length == 0 */
	ret = debug->dbg_log_fops->read(file, usr_buf, 0, NULL);
	KUNIT_EXPECT_EQ(test, ret, (int)0);

	is_debug_close();

	kunit_kfree(test, (void *)pkt_ctx.minfo.kvaddr_debug);
	kunit_kfree(test, (void *)usr_buf);
	kunit_kfree(test, (void *)file);
	kunit_kfree(test, (void *)inode);
}

static void pablo_debug_event_fops_kunit_test(struct kunit *test)
{
	int ret;
	struct is_debug *debug = pkt_ctx.debug;
	struct inode *inode;
	struct file *file;
	struct seq_file *s_f;
	char *usr_buf;
	size_t sz = (1 << 10); /* 1KB */
	void *p;

	/* Setup required data structures */
	inode = (struct inode *)kunit_kzalloc(test, sizeof(struct inode), 0);
	file = (struct file *)kunit_kzalloc(test, sizeof(struct file), 0);
	usr_buf = (char *)kvmalloc(sz, 0);

	/* TC #1. File open */
	inode->i_private = (void *)debug;

	ret = debug->dbg_event_fops->open(inode, file);
	KUNIT_EXPECT_EQ(test, ret, (int)0);

	/* TC #2. File shot */
	s_f = (struct seq_file *)file->private_data;
	s_f->count = 0;
	s_f->buf = usr_buf;
	s_f->size = sz;
	p = s_f->op->start(s_f, &s_f->index);

	s_f->op->show(s_f, p);
	KUNIT_EXPECT_GT(test, s_f->count, (size_t)0);

	debug->dbg_event_fops->release(inode, file); /* It calls 'kvfree()' for usr_buf. */

	kunit_kfree(test, (void *)file);
	kunit_kfree(test, (void *)inode);
}

static void pablo_debug_event_print_kunit_test(struct kunit *test)
{
	int prev, cur, index;
	struct is_debug_event_log *log;

	/* TC #1. EVENT_CRITICAL overflow */
	prev = atomic_read(&pkt_ctx.event->event_index);
	atomic_set(&pkt_ctx.event->critical_log_tail, IS_EVENT_MAX_NUM);

	is_debug_event_print(IS_EVENT_CRITICAL, NULL, NULL, 0, "%s", __func__);

	cur = atomic_read(&pkt_ctx.event->event_index);
	index = atomic_read(&pkt_ctx.event->critical_log_tail);
	KUNIT_EXPECT_EQ(test, cur, prev);
	KUNIT_EXPECT_GT(test, index, IS_EVENT_MAX_NUM);

	/* TC #2. EVENT_CRITICAL normal */
	prev = atomic_read(&pkt_ctx.event->event_index);
	atomic_set(&pkt_ctx.event->critical_log_tail, 0);

	is_debug_event_print(IS_EVENT_CRITICAL, NULL, NULL, 0, "%s", __func__);

	cur = atomic_read(&pkt_ctx.event->event_index);
	index = atomic_read(&pkt_ctx.event->critical_log_tail);
	log = &pkt_ctx.event->event_log_critical[index];
	KUNIT_EXPECT_GT(test, cur, prev);
	KUNIT_EXPECT_STREQ(test, (char *)log->dbg_msg, (char *)__func__);

	/* TC #3. EVENT_NORMAL overflow */
	prev = atomic_read(&pkt_ctx.event->event_index);
	atomic_set(&pkt_ctx.event->normal_log_tail, IS_EVENT_MAX_NUM - 1);

	is_debug_event_print(IS_EVENT_NORMAL, NULL, NULL, 0, "%s", __func__);

	cur = atomic_read(&pkt_ctx.event->event_index);
	index = atomic_read(&pkt_ctx.event->normal_log_tail) % IS_EVENT_MAX_NUM;
	log = &pkt_ctx.event->event_log_normal[index];
	KUNIT_EXPECT_GT(test, cur, prev);
	KUNIT_EXPECT_LT(test, index, IS_EVENT_MAX_NUM);
	KUNIT_EXPECT_STREQ(test, (char *)log->dbg_msg, (char *)__func__);

	/* TC #4, EVENT_NORMAL with data */
	is_debug_event_print(IS_EVENT_NORMAL, NULL, (void *)&index, sizeof(index), "%s", __func__);
	index = atomic_read(&pkt_ctx.event->normal_log_tail) % IS_EVENT_MAX_NUM;
	log = &pkt_ctx.event->event_log_normal[index];
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, log->ptrdata);
	KUNIT_EXPECT_TRUE(test, memcmp(log->ptrdata, &index, sizeof(index)));
}

static void pablo_debug_event_count_kunit_test(struct kunit *test)
{
	int prev_csi, cur_csi, prev_3aa, cur_3aa;

	/* TC #1. EVENT_OVERFLOW_CSI */
	prev_csi = atomic_read(&pkt_ctx.event->overflow_csi);

	is_debug_event_count(IS_EVENT_OVERFLOW_CSI);

	cur_csi = atomic_read(&pkt_ctx.event->overflow_csi);
	KUNIT_EXPECT_GT(test, cur_csi, prev_csi);

	/* TC #2. EVENT_OVERFLOW_3AA */
	prev_3aa = atomic_read(&pkt_ctx.event->overflow_3aa);

	is_debug_event_count(IS_EVENT_OVERFLOW_3AA);

	cur_3aa = atomic_read(&pkt_ctx.event->overflow_3aa);
	KUNIT_EXPECT_GT(test, cur_3aa, prev_3aa);

	/* TC #3. Invalid EVENT */
	prev_csi = atomic_read(&pkt_ctx.event->overflow_csi);
	prev_3aa = atomic_read(&pkt_ctx.event->overflow_3aa);

	is_debug_event_count(IS_EVENT_ALL);

	cur_csi = atomic_read(&pkt_ctx.event->overflow_csi);
	cur_3aa = atomic_read(&pkt_ctx.event->overflow_3aa);
	KUNIT_EXPECT_EQ(test, prev_csi, cur_csi);
	KUNIT_EXPECT_EQ(test, prev_3aa, cur_3aa);
}

static void pablo_debug_s2d_kunit_test(struct kunit *test)
{
	/* TC #1, Trigger S2D */
	is_debug_s2d(true, __func__);
	KUNIT_EXPECT_TRUE(test, pkt_ctx.dss_flag);
}

static void pablo_debug_bcm_kunit_test(struct kunit *test)
{
	/* TC #1, Start BCM */
	is_debug_bcm(true, 0);
	KUNIT_EXPECT_TRUE(test, pkt_ctx.bcm_flag);

	/* TC #1, Stop BCM */
	is_debug_bcm(false, 0);
	KUNIT_EXPECT_FALSE(test, pkt_ctx.bcm_flag);
}

static void pablo_debug_icpu_ops_kunit_test(struct kunit *test)
{
	is_debug_register_icpu_ops(NULL);

	is_debug_icpu_s2d_handler();
}

static int pablo_debug_kunit_test_init(struct kunit *test)
{
	struct is_debug *debug;

	debug = pkt_ctx.debug = is_debug_get();
	pkt_ctx.event = pablo_debug_get_event();

	/* Save original memlog ops & set dummy ops */
	pkt_ctx.pml_ops_b = debug->memlog.ops;
	debug->memlog.ops = &pml_ops_mock;

	pkt_ctx.minfo.kvaddr_debug_cnt = (ulong)kunit_kmalloc(test, sizeof(int), 0);

	/* Save original event counter */
	pkt_ctx.event_index = atomic_read(&pkt_ctx.event->event_index);
	pkt_ctx.critical_log_tail = atomic_read(&pkt_ctx.event->critical_log_tail);
	pkt_ctx.normal_log_tail = atomic_read(&pkt_ctx.event->normal_log_tail);
	pkt_ctx.overflow_csi = atomic_read(&pkt_ctx.event->overflow_csi);
	pkt_ctx.overflow_3aa = atomic_read(&pkt_ctx.event->overflow_3aa);

	pkt_ctx.pbcm_ops_b = debug->bcm_ops;
	debug->bcm_ops = &pbcm_ops_mock;
	pkt_ctx.pdss_ops_b = debug->dss_ops;
	debug->dss_ops = &pdss_ops_mock;

	return 0;
}

static void pablo_debug_kunit_test_exit(struct kunit *test)
{
	struct is_debug *debug = pkt_ctx.debug;

	/* Restore memlog ops */
	debug->memlog.ops = pkt_ctx.pml_ops_b;

	kunit_kfree(test, (void *)pkt_ctx.minfo.kvaddr_debug_cnt);

	/* Restore event counter */
	atomic_set(&pkt_ctx.event->event_index, pkt_ctx.event_index);
	atomic_set(&pkt_ctx.event->critical_log_tail, pkt_ctx.critical_log_tail);
	atomic_set(&pkt_ctx.event->normal_log_tail, pkt_ctx.normal_log_tail);
	atomic_set(&pkt_ctx.event->overflow_csi, pkt_ctx.overflow_csi);
	atomic_set(&pkt_ctx.event->overflow_3aa, pkt_ctx.overflow_3aa);

	debug->bcm_ops = pkt_ctx.pbcm_ops_b;
	debug->dss_ops = pkt_ctx.pdss_ops_b;
	pkt_ctx.dss_flag = false;
}

static struct kunit_case pablo_debug_kunit_test_cases[] = {
	KUNIT_CASE(pablo_debug_draw_digit_set_kunit_test),
	KUNIT_CASE(pablo_debug_draw_digit_kunit_test),
	KUNIT_CASE(pablo_debug_get_debug_param_kunit_test),
	KUNIT_CASE(pablo_debug_get_sysfs_kunit_test),
	KUNIT_CASE(pablo_debug_sysfs_debug_kunit_test),
	KUNIT_CASE(pablo_debug_dmsg_concate_kunit_test),
	KUNIT_CASE(pablo_debug_print_buffer_kunit_test),
	KUNIT_CASE(pablo_debug_memlog_alloc_dump_kunit_test),
	KUNIT_CASE(pablo_debug_memlog_dump_cr_all_kunit_test),
	KUNIT_CASE(pablo_debug_open_close_kunit_test),
	KUNIT_CASE(pablo_debug_dma_dump_kunit_test),
	KUNIT_CASE(pablo_debug_islib_debug_fops_kunit_test),
	KUNIT_CASE(pablo_debug_event_fops_kunit_test),
	KUNIT_CASE(pablo_debug_event_print_kunit_test),
	KUNIT_CASE(pablo_debug_event_count_kunit_test),
	KUNIT_CASE(pablo_debug_s2d_kunit_test),
	KUNIT_CASE(pablo_debug_bcm_kunit_test),
	KUNIT_CASE(pablo_debug_icpu_ops_kunit_test),
	{},
};

struct kunit_suite pablo_debug_kunit_test_suite = {
	.name = "pablo-debug-kunit-test",
	.init = pablo_debug_kunit_test_init,
	.exit = pablo_debug_kunit_test_exit,
	.test_cases = pablo_debug_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_debug_kunit_test_suite);

MODULE_LICENSE("GPL");
