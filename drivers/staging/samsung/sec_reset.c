/*
 * Copyright (C) 2016 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/trace_seq.h>

enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,
	RR_N = 9,
	RR_T = 10,
};

static unsigned int reset_reason = RR_N;
module_param_named(reset_reason, reset_reason, uint, 0644);

static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	switch (reset_reason) {
	case RR_S:
		seq_puts(m, "SPON\n");
		break;
	case RR_W:
		seq_puts(m, "WPON\n");
		break;
	case RR_D:
		seq_puts(m, "DPON\n");
		break;
	case RR_K:
		seq_puts(m, "KPON\n");
		break;
	case RR_M:
		seq_puts(m, "MPON\n");
		break;
	case RR_P:
		seq_puts(m, "PPON\n");
		break;
	case RR_R:
		seq_puts(m, "RPON\n");
		break;
	case RR_B:
		seq_puts(m, "BPON\n");
		break;
	case RR_N:
		seq_puts(m, "NPON\n");
		break;
	case RR_T:
		seq_puts(m, "TPON\n");
		break;
	default:
		seq_puts(m, "NPON\n");
		break;
	}
	return 0;
}

static int
sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open = sec_reset_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", 0444, NULL,
			    &sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_reset_reason_init);

