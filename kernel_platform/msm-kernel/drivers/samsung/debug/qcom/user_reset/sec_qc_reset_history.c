// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2006-2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/samsung/debug/qcom/sec_qc_dbg_partition.h>
#include <linux/samsung/debug/qcom/sec_qc_rbcmd.h>

#include "sec_qc_user_reset.h"

static int __reset_history_prepare_reset_header(
		struct qc_user_reset_proc *reset_history)
{
	struct debug_reset_header *reset_header;

	reset_header = sec_qc_user_reset_get_reset_header();
	if (IS_ERR_OR_NULL(reset_header))
		return -ENOENT;

	if (reset_header->reset_history_valid != RESTART_REASON_SEC_DEBUG_MODE ||
	    reset_header->reset_history_cnt == 0) {
		pr_warn("reset_history no data.\n");
		kvfree(reset_header);
		return -ENODATA;
	}

	reset_history->reset_header = reset_header;

	return 0;
}

static void __reset_history_release_reset_header(
		struct qc_user_reset_proc *reset_history)
{
	struct debug_reset_header *reset_header = reset_history->reset_header;

	__qc_user_reset_set_reset_header(reset_header);
	kvfree(reset_header);

	reset_history->reset_header = NULL;
}

struct history_data {
	char context[SEC_DEBUG_AUTO_COMMENT_SIZE];
};

static inline void __reset_history_trim_context(struct history_data *history)
{
	size_t i;

	for (i = 0; i < sizeof(history->context); i++) {
		char *ch = &history->context[i];

		if (!isprint(*ch) && !isspace(*ch)) {
			*ch = '\0';
			return;
		}
	}
}

static size_t __reset_history_copy(char *dst, char *src,
		size_t history_count)
{
	size_t nr_history = history_count >= SEC_DEBUG_RESET_HISTORY_MAX_CNT ?
			SEC_DEBUG_RESET_HISTORY_MAX_CNT : history_count;
	struct history_data *history = (void *)src;
	const size_t sz_buf = SEC_DEBUG_RESET_HISTORY_SIZE;
	size_t written = 0;
	size_t i;

	for (i = 0; i < nr_history; i++) {
		size_t idx = (history_count - 1 - i) %
				SEC_DEBUG_RESET_HISTORY_MAX_CNT;

		__reset_history_trim_context(&history[idx]);
		written += scnprintf(&dst[written], sz_buf - written, "%s\n\n\n",
				history[idx].context);
	}

	return written > SEC_DEBUG_RESET_HISTORY_SIZE ?
			SEC_DEBUG_RESET_HISTORY_SIZE : written;
}

static int __reset_history_prepare_buf(struct qc_user_reset_proc *reset_history)
{
	const struct debug_reset_header *reset_header =
			reset_history->reset_header;
	char *buf_raw;
	char *buf;
	int ret = 0;

	buf_raw = vmalloc(SEC_DEBUG_RESET_HISTORY_SIZE);
	buf = vmalloc(SEC_DEBUG_RESET_HISTORY_SIZE);
	if (!buf_raw || !buf) {
		ret = -ENOMEM;
		goto err_nomem;
	}

	if (!sec_qc_dbg_part_read(debug_index_reset_history, buf_raw)) {
		ret = -ENXIO;
		goto failed_to_read;
	}

	reset_history->len = __reset_history_copy(buf, buf_raw,
			reset_header->reset_history_cnt);
	reset_history->buf = buf;

	vfree(buf_raw);

	return 0;

failed_to_read:
err_nomem:
	vfree(buf_raw);
	vfree(buf);
	return ret;
}

static void __reset_history_release_buf(
		struct qc_user_reset_proc *reset_history)
{
	vfree(reset_history->buf);
	reset_history->buf = NULL;
}

static int sec_qc_reset_history_proc_open(struct inode *inode,
		struct file *file)
{
	struct qc_user_reset_proc *reset_history = PDE_DATA(inode);
	int err = 0;

	mutex_lock(&reset_history->lock);

	if (reset_history->ref) {
		reset_history->ref++;
		goto already_cached;
	}

	err = __reset_history_prepare_reset_header(reset_history);
	if (err) {
		pr_warn("failed to load reset_header (%d)\n", err);
		goto err_reset_header;
	}

	err = __reset_history_prepare_buf(reset_history);
	if (err) {
		pr_warn("failed to load to buffer (%d)\n", err);
		goto err_buf;
	}

	reset_history->ref++;

	mutex_unlock(&reset_history->lock);

	return 0;

err_buf:
	__reset_history_release_reset_header(reset_history);
err_reset_header:
already_cached:
	mutex_unlock(&reset_history->lock);
	return err;
}

static ssize_t sec_qc_reset_history_proc_read(struct file *file,
		char __user *buf, size_t nbytes, loff_t *ppos)
{
	struct qc_user_reset_proc *reset_history = PDE_DATA(file_inode(file));
	loff_t pos = *ppos;

	nbytes = min_t(size_t, nbytes, reset_history->len - pos);
	if (copy_to_user(buf, &reset_history->buf[pos], nbytes))
		return -EFAULT;

	*ppos += nbytes;

	return nbytes;
}

static loff_t sec_qc_reset_history_proc_lseek(struct file *file, loff_t off,
		int whence)
{
	struct qc_user_reset_proc *reset_history = PDE_DATA(file_inode(file));

	return fixed_size_llseek(file, off, whence, reset_history->len);
}

static int sec_qc_reset_history_proc_release(struct inode *inode,
		struct file *file)
{
	struct qc_user_reset_proc *reset_history = PDE_DATA(inode);

	mutex_lock(&reset_history->lock);

	reset_history->ref--;
	if (reset_history->ref)
		goto still_used;

	reset_history->len = 0;
	__reset_history_release_buf(reset_history);
	__reset_history_release_reset_header(reset_history);

still_used:
	mutex_unlock(&reset_history->lock);

	return 0;
}

static const struct proc_ops reset_history_pops = {
	.proc_open = sec_qc_reset_history_proc_open,
	.proc_read = sec_qc_reset_history_proc_read,
	.proc_lseek = sec_qc_reset_history_proc_lseek,
	.proc_release = sec_qc_reset_history_proc_release,
};

int __qc_reset_history_init(struct builder *bd)
{
	struct qc_user_reset_drvdata *drvdata =
			container_of(bd, struct qc_user_reset_drvdata, bd);
	struct qc_user_reset_proc *reset_history = &drvdata->reset_history;
	const char *node_name = "reset_history";

	mutex_init(&reset_history->lock);

	reset_history->proc = proc_create_data(node_name, 0440, NULL,
			&reset_history_pops, reset_history);
	if (!reset_history->proc) {
		dev_err(drvdata->bd.dev, "failed create procfs node (%s)\n",
				node_name);
		return -ENODEV;
	}

	return 0;
}

void __qc_reset_history_exit(struct builder *bd)
{
	struct qc_user_reset_drvdata *drvdata =
			container_of(bd, struct qc_user_reset_drvdata, bd);
	struct qc_user_reset_proc *reset_history = &drvdata->reset_history;

	proc_remove(reset_history->proc);
	mutex_destroy(&reset_history->lock);
}

