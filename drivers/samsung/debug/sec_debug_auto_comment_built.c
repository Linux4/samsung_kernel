// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_auto_comment.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "sec_debug_internal.h"
#include "../../../kernel/printk/printk_ringbuffer.h"

#define AC_SIZE 0xf3c
#define AC_MAGIC 0xcafecafe
#define AC_TAIL_MAGIC 0x00c0ffee
#define AC_EDATA_MAGIC 0x43218765

enum {
	PRIO_LV0 = 0,
	PRIO_LV1,
	PRIO_LV2,
	PRIO_LV3,
	PRIO_LV4,
	PRIO_LV5,
	PRIO_LV6,
	PRIO_LV7,
	PRIO_LV8,
	PRIO_LV9
};

struct sec_debug_auto_comm_log_idx {
	atomic_t logging_entry;
	atomic_t logging_disable;
	atomic_t logging_count;
};

struct auto_comment_log_map {
	char prio_level;
	char max_count;
};

static struct sec_debug_auto_comm_log_idx ac_idx[SEC_DEBUG_AUTO_COMM_BUF_SIZE];
static struct sec_debug_auto_comment *auto_comment_info;
static char *auto_comment_buf;

static const struct auto_comment_log_map init_data[SEC_DEBUG_AUTO_COMM_BUF_SIZE] = {
	{PRIO_LV0, 0},
	{PRIO_LV5, 8},
	{PRIO_LV9, 0},
	{PRIO_LV5, 0},
	{PRIO_LV5, 0},
	{PRIO_LV1, 7},
	{PRIO_LV2, 8},
	{PRIO_LV5, 0},
	{PRIO_LV5, 8},
	{PRIO_LV0, 0},
};

extern void register_set_auto_comm_buf(void (*func)(int type,
					struct printk_ringbuffer *rb,
					struct printk_record *r));

void secdbg_comm_log_disable(int type)
{
	atomic_inc(&(ac_idx[type].logging_disable));
}

void secdbg_comm_log_once(int type)
{
	if (atomic_read(&(ac_idx[type].logging_entry)))
		secdbg_comm_log_disable(type);
	else
		atomic_inc(&(ac_idx[type].logging_entry));
}

static void secdbg_comm_print_log(int type, const char *buf, size_t size)
{
	struct sec_debug_auto_comm_buf *p = &auto_comment_info->auto_comm_buf[type];
	unsigned int offset = p->offset;

	if (atomic_read(&(ac_idx[type].logging_disable)))
		return;

	if (offset + size > SZ_4K)
		return;

	if (init_data[type].max_count &&
	    (atomic_read(&(ac_idx[type].logging_count)) > init_data[type].max_count))
		return;

	if (!(auto_comment_info->fault_flag & (1 << type))) {
		auto_comment_info->fault_flag |= (1 << type);
		if (init_data[type].prio_level == PRIO_LV5) {
			auto_comment_info->lv5_log_order |= type << (auto_comment_info->lv5_log_cnt * 4);
			auto_comment_info->lv5_log_cnt++;
		}
		auto_comment_info->order_map[auto_comment_info->order_map_cnt++] = type;
	}

	atomic_inc(&(ac_idx[type].logging_count));

	memcpy(p->buf + offset, buf, size);
	p->offset += (unsigned int)size;
}

static size_t print_time(u64 ts, char *buf, size_t buf_size)
{
	unsigned long rem_nsec = do_div(ts, 1000000000);

	return snprintf(buf, buf_size, "[%5lu.%06lu]", (unsigned long)ts, rem_nsec / 1000);
}

static size_t print_caller(u32 id, char *buf, size_t buf_size)
{
	char caller[12];

	snprintf(caller, sizeof(caller), "%c%u",
		 id & 0x80000000 ? 'C' : 'T', id & ~0x80000000);
	return snprintf(buf, buf_size, "[%6s] ", caller);
}

static size_t print_fixed_caller(char *buf, size_t buf_size)
{
	size_t len = 0;

	len = snprintf(buf, buf_size, "%c[%1d:%15s:%5d] ", in_interrupt() ? 'I' : ' ',
			smp_processor_id(), current->comm, task_pid_nr(current));
	return len;
}

static size_t info_print_prefix(const struct printk_info *info, char *buf,
				size_t buf_size, bool init)
{
	size_t len = 0;

	len += print_time(info->ts_nsec, buf + len, buf_size - len);

	if (init)
		len += print_fixed_caller(buf + len, buf_size - len);
	else
		len += print_caller(info->caller_id, buf + len, buf_size - len);

	return len;
}

static size_t record_print_text(struct printk_record *r, bool init)
{
	size_t text_len = r->info->text_len;
	size_t buf_size = r->text_buf_size;
	char *text = r->text_buf;
	char prefix[64];
	bool truncated = false;
	size_t prefix_len;
	size_t line_len;
	size_t len = 0;
	char *next;

	/*
	 * If the message was truncated because the buffer was not large
	 * enough, treat the available text as if it were the full text.
	 */
	if (text_len > buf_size)
		text_len = buf_size;

	prefix_len = info_print_prefix(r->info, prefix, sizeof(prefix), init);

	/*
	 * @text_len: bytes of unprocessed text
	 * @line_len: bytes of current line _without_ newline
	 * @text:     pointer to beginning of current line
	 * @len:      number of bytes prepared in r->text_buf
	 */
	for (;;) {
		next = memchr(text, '\n', text_len);
		if (next) {
			line_len = next - text;
		} else {
			/* Drop truncated line(s). */
			if (truncated)
				break;
			line_len = text_len;
		}

		/*
		 * Truncate the text if there is not enough space to add the
		 * prefix and a trailing newline and a terminator.
		 */
		if (len + prefix_len + text_len + 1 + 1 > buf_size) {
			/* Drop even the current line if no space. */
			if (len + prefix_len + line_len + 1 + 1 > buf_size)
				break;

			text_len = buf_size - len - prefix_len - 1 - 1;
			truncated = true;
		}

		memmove(text + prefix_len, text, text_len);
		memcpy(text, prefix, prefix_len);

		/*
		 * Increment the prepared length to include the text and
		 * prefix that were just moved+copied. If this is the last
		 * line, there is no newline.
		 */
		len += prefix_len + line_len + 1;
		if (text_len == line_len) {
			if (!init)
				len--;
			break;
		}

		/*
		 * Advance beyond the added prefix and the related line with
		 * its newline.
		 */
		text += prefix_len + line_len + 1;

		/*
		 * The remaining text has only decreased by the line with its
		 * newline.
		 *
		 * Note that @text_len can become zero. It happens when @text
		 * ended with a newline (either due to truncation or the
		 * original string ending with "\n\n"). The loop is correctly
		 * repeated and (if not truncated) an empty line with a prefix
		 * will be prepared.
		 */
		text_len -= line_len + 1;
	}

	/*
	 * If a buffer was provided, it will be terminated. Space for the
	 * string terminator is guaranteed to be available. The terminator is
	 * not counted in the return value.
	 */
	if (buf_size > 0)
		r->text_buf[len] = 0;

	return len;
}

static void secdbg_comm_hook_printk_record(int type,
					   struct printk_ringbuffer *rb,
					   struct printk_record *r)
{
	static char text_buf[4096];
	char *text;
	struct printk_info info;
	struct printk_record record;
	size_t len = 0;

	text = text_buf;
	text[len++] = '\n';
	prb_rec_init_rd(&record, &info, text + len, sizeof(text_buf) - len);

	if (record.text_buf_size < r->info->text_len + 1 + 1)
		return;

	*record.info = *r->info;
	memcpy(record.text_buf, r->text_buf, r->info->text_len);

	len = record_print_text(&record, true);

	secdbg_comm_print_log(type, text, len);
}

static void secdbg_comm_init_buf(void)
{
	auto_comment_buf = (char *)secdbg_base_built_get_ncva(secdbg_base_built_get_buf_base(SDN_MAP_AUTO_COMMENT));
	auto_comment_info = (struct sec_debug_auto_comment *)secdbg_base_built_get_debug_base(SDN_MAP_AUTO_COMMENT);

	memset(auto_comment_info, 0, sizeof(struct sec_debug_auto_comment));

	register_set_auto_comm_buf(secdbg_comm_hook_printk_record);

	pr_info("%s: done\n", __func__);
}

int secdbg_comm_auto_comment_init(void)
{
	int i;

	pr_info("%s: start\n", __func__);

	secdbg_comm_init_buf();

	if (auto_comment_info) {
		auto_comment_info->header_magic = AC_MAGIC;
		auto_comment_info->tail_magic = AC_TAIL_MAGIC;
	}

	for (i = 0; i < SEC_DEBUG_AUTO_COMM_BUF_SIZE; i++) {
		atomic_set(&(ac_idx[i].logging_entry), 0);
		atomic_set(&(ac_idx[i].logging_disable), 0);
		atomic_set(&(ac_idx[i].logging_count), 0);
	}

	pr_info("%s: done\n", __func__);

	return 0;
}

static ssize_t secdbg_comm_auto_comment_proc_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!auto_comment_buf) {
		pr_err("%s : buffer is null\n", __func__);
		return -ENODEV;
	}

	/* TODO : check reset_reason
	if (reset_reason >= RR_R && reset_reason <= RR_N) {
		pr_err("%s : reset_reason %d\n", __func__, reset_reason);
		return -ENOENT;
	}*/

	if (pos >= AC_SIZE) {
		pr_err("%s : pos 0x%llx\n", __func__, pos);
		return 0;
	}

	if (strncmp(auto_comment_buf, "@ Ramdump", 9)) {
		pr_err("%s : no data in auto_comment\n", __func__);
		return 0;
	}

	count = min(len, (size_t)(AC_SIZE - pos));
	if (copy_to_user(buf, auto_comment_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct proc_ops sec_reset_auto_comment_proc_fops = {
	.proc_lseek = default_llseek,
	.proc_read = secdbg_comm_auto_comment_proc_read,
};

static int __init secdbg_comm_auto_comment_proc_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s: start\n", __func__);

	entry = proc_create("auto_comment", 0400, NULL,
			    &sec_reset_auto_comment_proc_fops);

	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, AC_SIZE);

	pr_info("%s: done\n", __func__);

	return 0;
}
late_initcall(secdbg_comm_auto_comment_proc_init);
