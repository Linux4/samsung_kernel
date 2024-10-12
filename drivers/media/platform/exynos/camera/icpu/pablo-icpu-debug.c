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
static struct icpu_debug {
	struct dentry *root_dir;
	struct dentry *logfile;
	u32 *log_offset;
	void *log_base_kva;
	size_t log_size;
	size_t overlap_margin;
	copy_to_buf_func_t copy_to_buf;
	int state;
	char *priv_buf;
	u32 read_flag;
} debug;

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

static int __debug_read_overlapped(size_t offset,
		char __user *user_buf, size_t buf_len, loff_t *ppos, ssize_t *pcnt)
{
	int ret;
	size_t read_offset;
	loff_t _pos = *ppos;
	ssize_t read_cnt = *pcnt;

	if (_pos > 0 && _pos <= offset)
		return 0;

	read_offset = _pos ? _pos : offset + debug.overlap_margin;

	if (read_offset < debug.log_size)
		read_cnt = debug.log_size - read_offset;

	if (read_cnt > buf_len)
		read_cnt = buf_len;

	_pos = read_offset;
	if (_pos > debug.log_size)
		_pos -= debug.log_size;

	_pos += read_cnt;
	if (_pos == debug.log_size)
		_pos = 0;

	ICPU_DEBUG("read_offset(%d) _pos(%d), read_cnt(%d)", read_offset, _pos, read_cnt);

	if (read_cnt) {
		ret = debug.copy_to_buf(user_buf, debug.log_base_kva + read_offset, read_cnt);
		if (ret) {
			ICPU_ERR("failed copying %d bytes of debug log\n", ret);
			return -1;
		}
	}

	*ppos = _pos;
	*pcnt = read_cnt;

	return 0;
}

static u32 __print_buf(u32 src_idx, char *buf, size_t st, size_t ed, size_t st_pr)
{
	const u32 max_src_idx = DEBUG_BUF_LEN - 1;

	while (st < ed) {
		if (debug.state != ICPU_DEBUG_STATUS_RUN)
			break;

		debug.priv_buf[src_idx] = buf[st++];

		if (debug.priv_buf[src_idx] == '\n' || src_idx == max_src_idx) {
			if (src_idx == max_src_idx) {
				ICPU_WARN("string exceed buf len(%d)", DEBUG_BUF_LEN);
				debug.priv_buf[src_idx] = '\0';
				st--;
			}
			if (_log.level == LOGLEVEL_CUSTOM1) {
				pr_warn("[ICPU-FW] %s", debug.priv_buf);
			} else {
				is_memlog_ddk("[ICPU-FW] %s", debug.priv_buf);
				if (st_pr < st)
					pr_warn("[ICPU-FW] %s", debug.priv_buf);
			}
			memset(debug.priv_buf, 0, DEBUG_BUF_LEN);
			src_idx = 0;
			continue;
		}
		src_idx++;
	}

	return src_idx;
}

static ssize_t __log_dump(loff_t *ppos, size_t pr_size)
{
	bool overflow_bit;
	size_t offset;
	size_t st, ed, st_pr;
	u32 index = 0;
	char *log_buf = debug.log_base_kva;

	if (!debug.priv_buf) {
		ICPU_INFO("no buf to dump");
		return 0;
	}

	if (debug.state != ICPU_DEBUG_STATUS_RUN)
		return 0;

	overflow_bit = (*debug.log_offset) >> 31;
	offset = (*debug.log_offset) & ~(1 << 31);

	if (_log.level != LOGLEVEL_CUSTOM1)
		is_memlog_ddk("[ICPU-FW] log offset: 0x%x", *debug.log_offset);

	if (!overflow_bit) {
		st = *ppos;
		ed = offset;
		st_pr = (ed > pr_size) ? ed - pr_size : st;
		index = __print_buf(index, log_buf, st, ed, st_pr);
	} else {
		st = *ppos ? *ppos : offset + 1;
		ed = debug.log_size;
		st_pr = (offset < pr_size) ? ed - (pr_size - offset) : ed;
		index = __print_buf(index, log_buf, st, ed, st_pr);

		st = 0;
		ed = offset;
		st_pr = (offset >= pr_size) ? ed - pr_size : st;
		index = __print_buf(index, log_buf, st, ed, st_pr);
	}

	*ppos = ed - 1;

	return 1;
}

static ssize_t __printk_fw_log(char __user *user_buf, size_t buf_len, loff_t *ppos)
{
	ssize_t ret;

	ret = __log_dump(ppos, 0);
	if (ret) {
		debug.copy_to_buf(user_buf, "\r", 1);
		fsleep(WAIT_TIME_US);
	}

	return ret;
}

static ssize_t __debug_read(struct file *file, char __user *user_buf,
		size_t buf_len, loff_t *ppos)
{
	int ret = 0;
	bool overflow_bit;
	size_t read_cnt_pre = 0;
	size_t read_cnt = 0;
	size_t offset;
	loff_t read_offset = *ppos;

	if (debug.state != ICPU_DEBUG_STATUS_RUN)
		return 0;

	if (!debug.log_offset || !debug.log_base_kva)
		return 0;

	debug.read_flag = 1;

	if (_log.level == LOGLEVEL_CUSTOM1)
		return __printk_fw_log(user_buf, buf_len, ppos);

	overflow_bit = (*debug.log_offset) >> 31;
	offset = (*debug.log_offset) & ~(1 << 31);

	ICPU_DEBUG("overflow(%d), offset(%d), read_offset(%d)", overflow_bit, offset, read_offset);

	if (offset > debug.log_size) {
		ICPU_ERR("firmware log offset is invalid - 0x%x (dec. %d)", offset, offset);
		return 0;
	}

	if (read_offset == offset) {
		/* no new log: but we keep wait */
		debug.copy_to_buf(user_buf, "\r", 1);
		fsleep(WAIT_TIME_US);
		return 1;
	}

	if (overflow_bit)
		ret = __debug_read_overlapped(offset, user_buf, buf_len, &read_offset, &read_cnt_pre);

	buf_len -= read_cnt_pre;

	if (ret || (offset < read_offset && buf_len)) {
		ICPU_ERR("unknown err(ret=%d), offset(%d), read_offset(%d), overflow(%d)",
				ret, offset, read_offset, overflow_bit);
		goto unknown_err;
	}

	if (buf_len) {
		read_cnt = offset - read_offset;
		if (read_cnt > buf_len)
			read_cnt = buf_len;

		ret = debug.copy_to_buf(user_buf + read_cnt_pre, debug.log_base_kva + read_offset, read_cnt);
		if (ret) {
			ICPU_ERR("failed copying %d bytes of debug log\n", ret);
			goto unknown_err;
		}

		read_offset += read_cnt;
	}

	*ppos = read_offset;

	ICPU_DEBUG("ppos(%d), ret(%d)", *ppos, read_cnt_pre + read_cnt);
	return read_cnt_pre + read_cnt;

unknown_err:
	*ppos = 0;
	return 0;
}

static const struct file_operations debug_log_fops = {
	.open	= __debug_open,
	.read	= __debug_read,
	.llseek	= no_llseek
};

#define DEBUG_FS_ROOT_NAME "icpu"
#define DEBUG_FS_LOGFILE_NAME "icpu_fw_msg"
static int __pablo_icpu_debug_probe(void)
{
	debug.root_dir = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (debug.root_dir)
		ICPU_INFO("%s is created\n", DEBUG_FS_ROOT_NAME);

	debug.logfile = debugfs_create_file(DEBUG_FS_LOGFILE_NAME, S_IRUSR,
		debug.root_dir, &debug, &debug_log_fops);
	if (debug.logfile)
		ICPU_INFO("%s is created\n", DEBUG_FS_LOGFILE_NAME);

	debug.priv_buf = vzalloc(DEBUG_BUF_LEN);
	ICPU_ERR_IF(IS_ERR_OR_NULL(debug.priv_buf), "Fail to alloc debug priv_buf");

	debug.state = ICPU_DEBUG_STATUS_INIT;
	debug.read_flag = 0;

	return 0;
}

static void __pablo_icpu_debug_config(void *kva, size_t log_offset, size_t log_size,
		size_t margin, copy_to_buf_func_t func)
{
	debug.log_offset = kva + log_offset;
	debug.log_base_kva = debug.log_offset + 1;

	debug.log_size = log_size;
	debug.overlap_margin = margin;

	debug.copy_to_buf = func;
}

int pablo_icpu_debug_probe(void)
{
	return __pablo_icpu_debug_probe();
}

void pablo_icpu_debug_remove(void)
{
	debugfs_remove(debug.root_dir);

	vfree(debug.priv_buf);
	debug.priv_buf = NULL;

	memset(&debug, 0, sizeof(struct icpu_debug));
}

#define DEBUG_LOG_MARGIN 256 /* TODO: need to fix margin size */
int pablo_icpu_debug_config(void *kva, size_t fw_mem_size)
{
	if (!kva || !fw_mem_size)
		return -EINVAL;

	/* FW debug log area is placed at the end of memory
	 *
	 * |<--------------------- fw_mem_size -------------------->|
	 * |                          |<------ DEBUG_LOG_SIZE ----->|
	 * |                          |<- 4 byte ->                 |
	 * |--------------------------|-log header-|----------------|
	 *
	 * log offset = fw_mem_size - DEBUG_LOG_SIZE
	 * actual log size = DEBUG_LOG_SIZE - size of header(4-bytes)
	 */
	__pablo_icpu_debug_config(kva, fw_mem_size - DEBUG_LOG_SIZE,  DEBUG_LOG_SIZE - 4,
			DEBUG_LOG_MARGIN, copy_to_user);

	memset(debug.priv_buf, 0, DEBUG_BUF_LEN);

	debug.state = ICPU_DEBUG_STATUS_RUN;

	ICPU_DEBUG("log offset addr(0x%x), base(0x%x)", debug.log_offset, debug.log_base_kva);

	return 0;
}

void pablo_icpu_debug_fw_log_dump(void)
{
	loff_t ppos = 0;
	int level = _log.level;

	ICPU_INFO("start firmware log dump");

	_log.level = LOGLEVEL_INFO;
	__log_dump(&ppos, ICPU_KERNEL_DUMP_SIZE);
	_log.level = level;

	ICPU_INFO("end firmware log dump");
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_debug_fw_log_dump);

void pablo_icpu_debug_reset(void)
{
	if (unlikely(debug.read_flag)) {
		ICPU_WARN("wait for fw log print");
		fsleep(WAIT_TIME_US);
	}

	debug.state = ICPU_DEBUG_STATUS_INIT;
	debug.read_flag = 0;

	__pablo_icpu_debug_config(0, 0,  0, 0, 0);
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_debug_reset);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_test_get_file_ops(struct file_operations *fops)
{
	if (!debug.log_offset || !debug.log_base_kva)
		return -ENOMEM;

	*fops = debug_log_fops;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_icpu_test_get_file_ops);

int pablo_icpu_debug_test_config(void *kva, size_t log_size, size_t margin, copy_to_buf_func_t func)
{
	__pablo_icpu_debug_config(kva, 0,  log_size, margin, func);

	memset(debug.priv_buf, 0, DEBUG_BUF_LEN);

	debug.state = ICPU_DEBUG_STATUS_RUN;

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
