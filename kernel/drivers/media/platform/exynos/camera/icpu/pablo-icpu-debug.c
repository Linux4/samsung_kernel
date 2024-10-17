// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include "pablo-icpu.h"
#include "pablo-icpu-debug.h"

#include "pablo-debug.h"

enum icpu_debug_status {
	ICPU_DEBUG_STATUS_INIT,
	ICPU_DEBUG_STATUS_RUN,
	ICPU_DEBUG_STATUS_INVALID,
};

#define WAIT_TIME_US (200 * 1000)

#define DEBUG_LOG_SIZE SZ_1M
#define DEBUG_BUF_LEN 256
#define ICPU_KERNEL_DUMP_SIZE 3000
#define ICPU_LOG_TO_KERNEL_DELAY 16

enum icpu_debug_type { ICPU_DEBUG_MEM, ICPU_DEBUG_KERNEL, ICPU_DEBUG_MAX };

static struct icpu_debug {
	enum icpu_debug_type type;
	struct dentry *root_dir;
	struct dentry *logfile;
	u32 *log_offset;
	void *log_base_kva;
	size_t log_size;
	size_t overlap_margin;
	copy_to_buf_func_t copy_to_buf;
	atomic_t state;
	char *priv_buf;
	u32 read_flag;
	struct kthread_worker *worker;
	struct kthread_work work;
	wait_queue_head_t wait_q;
} __debug[ICPU_DEBUG_MAX];

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-DEBUG]",
};

struct icpu_logger *get_icpu_debug_log(void)
{
	return &_log;
}

static int __debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static int __debug_read_overlapped(struct icpu_debug *debug, size_t offset, char __user *user_buf,
				   size_t buf_len, loff_t *ppos, ssize_t *pcnt)
{
	int ret;
	size_t read_offset;
	loff_t _pos = *ppos;
	ssize_t read_cnt = *pcnt;

	if (_pos > 0 && _pos <= offset)
		return 0;

	read_offset = _pos ? _pos : offset + debug->overlap_margin;

	if (read_offset < debug->log_size)
		read_cnt = debug->log_size - read_offset;

	if (read_cnt > buf_len)
		read_cnt = buf_len;

	_pos = read_offset;
	if (_pos > debug->log_size)
		_pos -= debug->log_size;

	_pos += read_cnt;
	if (_pos == debug->log_size)
		_pos = 0;

	ICPU_DEBUG("read_offset(%zu) _pos(%lld), read_cnt(%zd)", read_offset, _pos, read_cnt);

	if (read_cnt) {
		ret = debug->copy_to_buf(user_buf, debug->log_base_kva + read_offset, read_cnt);
		if (ret) {
			ICPU_ERR("failed copying %d bytes of debug log\n", ret);
			return -1;
		}
	}

	*ppos = _pos;
	*pcnt = read_cnt;

	return 0;
}

static u32 __print_buf(struct icpu_debug *debug, u32 src_idx, size_t st, size_t ed, size_t st_pr)
{
	const u32 max_src_idx = DEBUG_BUF_LEN - 1;
	char *buf = debug->log_base_kva;

	while (st < ed) {
		if (atomic_read(&debug->state) != ICPU_DEBUG_STATUS_RUN)
			break;

		debug->priv_buf[src_idx] = buf[st++];

		if (debug->priv_buf[src_idx] == '\n' || src_idx == max_src_idx) {
			if (src_idx == max_src_idx) {
				ICPU_DEBUG("string exceed buf len(%d)", DEBUG_BUF_LEN);
				debug->priv_buf[src_idx] = '\0';
				st--;
			}
			if (debug->type == ICPU_DEBUG_KERNEL) {
				pr_warn("[ICPU-FW] %s", debug->priv_buf);
			} else {
				is_memlog_ddk("[ICPU-FW] %s", debug->priv_buf);
				if (st_pr < st)
					pr_warn("[ICPU-FW] %s", debug->priv_buf);
			}
			memset(debug->priv_buf, 0, DEBUG_BUF_LEN);
			src_idx = 0;
			continue;
		}
		src_idx++;
	}

	return src_idx;
}

static void __print_log_buf(struct icpu_debug *debug, loff_t *ppos, size_t pr_size,
			    bool overflow_bit, size_t offset)
{
	u32 index = 0;
	size_t st, ed, st_pr;

	if (!overflow_bit) {
		st = *ppos;
		ed = offset;
		st_pr = (ed > pr_size) ? ed - pr_size : st;
		index = __print_buf(debug, index, st, ed, st_pr);
	} else {
		st = *ppos ? *ppos : offset + 1;
		ed = debug->log_size;
		st_pr = (offset < pr_size) ? ed - (pr_size - offset) : ed;
		index = __print_buf(debug, index, st, ed, st_pr);

		st = 0;
		ed = offset;
		st_pr = (offset >= pr_size) ? ed - pr_size : st;
		index = __print_buf(debug, index, st, ed, st_pr);
	}

	*ppos = ed;
}

static ssize_t __log_dump(struct icpu_debug *debug, size_t pr_size)
{
	loff_t ppos = 0;
	bool overflow_bit;
	size_t offset;

	if (!debug->priv_buf) {
		ICPU_INFO("no buf to dump");
		return 0;
	}

	if (atomic_read(&debug->state) != ICPU_DEBUG_STATUS_RUN)
		return 0;

	offset = (*debug->log_offset) & ~(1 << 31);
	overflow_bit = (*debug->log_offset) >> 31;

	is_memlog_ddk("[ICPU-FW] log offset: 0x%x", *debug->log_offset);

	__print_log_buf(debug, &ppos, pr_size, overflow_bit, offset);

	return 1;
}

static ssize_t __log_printk(struct icpu_debug *debug, loff_t *ppos)
{
	bool overflow_bit = 0;
	size_t offset;

	if (!debug->priv_buf) {
		ICPU_INFO("no buf to dump");
		return 0;
	}

	if (atomic_read(&debug->state) != ICPU_DEBUG_STATUS_RUN)
		return 0;

	offset = (*debug->log_offset) & ~(1 << 31);
	if (offset < *ppos)
		overflow_bit = 1;

	__print_log_buf(debug, ppos, 0, overflow_bit, offset);

	return 1;
}

static ssize_t __printk_fw_log(struct icpu_debug *debug, char __user *user_buf, size_t buf_len,
			       loff_t *ppos)
{
	ssize_t ret;

	debug->type = ICPU_DEBUG_KERNEL;

	ret = __log_printk(debug, ppos);
	if (ret) {
		debug->copy_to_buf(user_buf, "\r", 1);
		fsleep(WAIT_TIME_US);
	}

	debug->type = ICPU_DEBUG_MEM;

	return ret;
}

static ssize_t __debug_read(struct file *file, char __user *user_buf, size_t buf_len, loff_t *ppos)
{
	int ret = 0;
	bool overflow_bit;
	size_t read_cnt_pre = 0;
	size_t read_cnt = 0;
	size_t offset;
	loff_t read_offset = *ppos;
	struct icpu_debug *debug = &__debug[ICPU_DEBUG_MEM];

	if (atomic_read(&debug->state) != ICPU_DEBUG_STATUS_RUN)
		return 0;

	if (!debug->log_offset || !debug->log_base_kva)
		return 0;

	debug->read_flag = 1;

	if (_log.level == LOGLEVEL_CUSTOM1)
		return __printk_fw_log(debug, user_buf, buf_len, ppos);

	overflow_bit = (*debug->log_offset) >> 31;
	offset = (*debug->log_offset) & ~(1 << 31);

	ICPU_DEBUG("overflow(%d), offset(%zu), read_offset(%lld)", overflow_bit, offset,
		   read_offset);

	if (offset > debug->log_size) {
		ICPU_ERR("firmware log offset is invalid - 0x%zx (dec. %zu)", offset, offset);
		return 0;
	}

	if (read_offset == offset) {
		/* no new log: but we keep wait */
		debug->copy_to_buf(user_buf, "\r", 1);
		fsleep(WAIT_TIME_US);
		return 1;
	}

	if (overflow_bit)
		ret = __debug_read_overlapped(debug, offset, user_buf, buf_len, &read_offset,
					      &read_cnt_pre);

	buf_len -= read_cnt_pre;

	if (ret || (offset < read_offset && buf_len)) {
		ICPU_ERR("unknown err(ret=%d), offset(%zu), read_offset(%lld), overflow(%d)", ret,
			 offset, read_offset, overflow_bit);
		goto unknown_err;
	}

	if (buf_len) {
		read_cnt = offset - read_offset;
		if (read_cnt > buf_len)
			read_cnt = buf_len;

		ret = debug->copy_to_buf(user_buf + read_cnt_pre, debug->log_base_kva + read_offset,
					 read_cnt);
		if (ret) {
			ICPU_ERR("failed copying %d bytes of debug log\n", ret);
			goto unknown_err;
		}

		read_offset += read_cnt;
	}

	*ppos = read_offset;

	ICPU_DEBUG("ppos(%lld), ret(%zu)", *ppos, read_cnt_pre + read_cnt);
	return read_cnt_pre + read_cnt;

unknown_err:
	*ppos = 0;
	return 0;
}

static const struct file_operations debug_log_fops = {
	.open = __debug_open,
	.read = __debug_read,
};

static void pablo_icpu_debug_print_kernel_work_fn(struct kthread_work *work)
{
	loff_t ppos = 0;
	struct icpu_debug *debug = container_of(work, struct icpu_debug, work);

	while (!wait_event_timeout(debug->wait_q,
				   (atomic_read(&debug->state) != ICPU_DEBUG_STATUS_RUN),
				   msecs_to_jiffies(ICPU_LOG_TO_KERNEL_DELAY))) {
		ICPU_DEBUG("log_dump: %lld", ppos);
		__log_printk(debug, &ppos);
	}
}

static int __pablo_icpu_debug_init_work(struct icpu_debug *debug)
{
	init_waitqueue_head(&debug->wait_q);

	debug->worker = kthread_create_worker(0, "icpu-debug");
	if (IS_ERR(debug->worker)) {
		ICPU_ERR("failed to create icpu debug kthread: %ld", PTR_ERR(debug->worker));
		debug->worker = NULL;
		return PTR_ERR(debug->worker);
	}

	kthread_init_work(&debug->work, pablo_icpu_debug_print_kernel_work_fn);

	kthread_queue_work(debug->worker, &debug->work);

	return 0;
}

static void __pablo_icpu_debug_deinit_work(struct icpu_debug *debug)
{
	if (debug->worker) {
		wake_up(&debug->wait_q);
		kthread_destroy_worker(debug->worker);
		debug->worker = NULL;
	}
}

static void __pablo_icpu_debug_config(struct icpu_debug *debug, void *kva, size_t log_offset,
				      size_t log_size, size_t margin, copy_to_buf_func_t func)
{
	/* actual log size = log_size - size of header(4-bytes) */

	debug->log_offset = kva + log_offset;
	debug->log_base_kva = debug->log_offset + 1;

	debug->log_size = log_size;
	debug->overlap_margin = margin;

	debug->copy_to_buf = func;
}

#define DEBUG_LOG_MARGIN 256 /* TODO: need to fix margin size */

static void __pablo_icpu_debug_config_mem(struct icpu_debug *debug, void *kva, size_t size)
{
	__pablo_icpu_debug_config(debug, kva, 0, DEBUG_LOG_SIZE - 4, DEBUG_LOG_MARGIN,
				  copy_to_user);
	memset(debug->priv_buf, 0, DEBUG_BUF_LEN);

	atomic_set(&debug->state, ICPU_DEBUG_STATUS_RUN);

	ICPU_DEBUG("log to mem offset addr(0x%llx), base(0x%llx)", (u64)debug->log_offset,
		   (u64)debug->log_base_kva);
}

static void __pablo_icpu_debug_config_kernel(struct icpu_debug *debug, void *kva, size_t size)
{
	u32 kernel_log_size = size - DEBUG_LOG_SIZE;

	if (kernel_log_size > 0) {
		__pablo_icpu_debug_config(debug, kva, DEBUG_LOG_SIZE, kernel_log_size - 4,
					  DEBUG_LOG_MARGIN, copy_to_user);

		memset(debug->priv_buf, 0, DEBUG_BUF_LEN);

		atomic_set(&debug->state, ICPU_DEBUG_STATUS_RUN);

		if (__pablo_icpu_debug_init_work(debug)) {
			atomic_set(&debug->state, ICPU_DEBUG_STATUS_INIT);
			__pablo_icpu_debug_config(debug, 0, 0, 0, 0, 0);
			return;
		}

		ICPU_DEBUG("log to kernel offset addr(0x%llx), base(0x%llx)",
			   (u64)debug->log_offset, (u64)debug->log_base_kva);
	}
}

#define DEBUG_FS_ROOT_NAME "icpu"
#define DEBUG_FS_LOGFILE_NAME "icpu_fw_msg"
static int __pablo_icpu_debug_probe(void)
{
	u32 type;

	__debug[ICPU_DEBUG_MEM].root_dir = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (__debug[ICPU_DEBUG_MEM].root_dir)
		ICPU_INFO("%s is created\n", DEBUG_FS_ROOT_NAME);

	__debug[ICPU_DEBUG_MEM].logfile =
		debugfs_create_file(DEBUG_FS_LOGFILE_NAME, 00400, __debug[ICPU_DEBUG_MEM].root_dir,
				    &__debug, &debug_log_fops);
	if (__debug[ICPU_DEBUG_MEM].logfile)
		ICPU_INFO("%s is created\n", DEBUG_FS_LOGFILE_NAME);

	for (type = 0; type < ICPU_DEBUG_MAX; type++) {
		__debug[type].type = type;

		__debug[type].priv_buf = vzalloc(DEBUG_BUF_LEN);
		ICPU_ERR_IF(IS_ERR_OR_NULL(__debug[type].priv_buf), "Fail to alloc debug priv_buf");

		atomic_set(&__debug[type].state, ICPU_DEBUG_STATUS_INIT);
		__debug[type].read_flag = 0;
	}

	return 0;
}

int pablo_icpu_debug_probe(void)
{
	return __pablo_icpu_debug_probe();
}

void pablo_icpu_debug_remove(void)
{
	u32 type;

	debugfs_remove(__debug[ICPU_DEBUG_MEM].root_dir);

	for (type = 0; type < ICPU_DEBUG_MAX; type++) {
		vfree(__debug[type].priv_buf);
		__debug[type].priv_buf = NULL;

		memset(&__debug[type], 0, sizeof(struct icpu_debug));
	}
}

size_t pablo_icpu_get_log_size(void)
{
	return DEBUG_LOG_SIZE;
}

int pablo_icpu_debug_config(void *kva, size_t size)
{
	if (!kva || !size)
		return -EINVAL;

	__pablo_icpu_debug_config_mem(&__debug[ICPU_DEBUG_MEM], kva, size);
	__pablo_icpu_debug_config_kernel(&__debug[ICPU_DEBUG_KERNEL], kva, size);

	return 0;
}

void pablo_icpu_debug_fw_log_dump(void)
{
	ICPU_INFO("start firmware log dump");

	__log_dump(&__debug[ICPU_DEBUG_MEM], ICPU_KERNEL_DUMP_SIZE);

	ICPU_INFO("end firmware log dump");
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_debug_fw_log_dump);

void pablo_icpu_debug_reset(void)
{
	int type;

	for (type = 0; type < ICPU_DEBUG_MAX; type++) {
		if (unlikely(__debug[type].read_flag)) {
			ICPU_WARN("wait for fw log print");
			fsleep(WAIT_TIME_US);
		}

		atomic_set(&__debug[type].state, ICPU_DEBUG_STATUS_INIT);
		__pablo_icpu_debug_deinit_work(&__debug[type]);

		__debug[type].read_flag = 0;

		__pablo_icpu_debug_config(&__debug[type], 0, 0, 0, 0, 0);
	}
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_debug_reset);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_test_get_file_ops(struct file_operations *fops)
{
	if (!__debug[ICPU_DEBUG_MEM].log_offset || !__debug[ICPU_DEBUG_MEM].log_base_kva)
		return -ENOMEM;

	*fops = debug_log_fops;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_icpu_test_get_file_ops);

int pablo_icpu_debug_test_config(void *kva, size_t log_size, size_t margin, copy_to_buf_func_t func)
{
	__pablo_icpu_debug_config(&__debug[ICPU_DEBUG_MEM], kva, 0, log_size, margin, func);

	memset(__debug[ICPU_DEBUG_MEM].priv_buf, 0, DEBUG_BUF_LEN);

	atomic_set(&__debug[ICPU_DEBUG_MEM].state, ICPU_DEBUG_STATUS_RUN);

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_icpu_debug_test_config);

int pablo_icpu_debug_test_loglevel(u32 lv)
{
	u32 org = _log.level;

	_log.level = lv;

	return org;
}
EXPORT_SYMBOL_GPL(pablo_icpu_debug_test_loglevel);
#endif
