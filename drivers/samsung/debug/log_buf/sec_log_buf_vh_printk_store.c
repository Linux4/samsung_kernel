// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>

#include <trace/hooks/debug.h>

#include "sec_log_buf.h"

static void __trace_android_vh_printk_store(void *unused, int facility, int level)
{
	__log_buf_store_from_kmsg_dumper();
}

static int log_buf_logger_vh_printk_store_probe(
		struct log_buf_drvdata *drvdata)
{
	return register_trace_android_vh_printk_store(
				__trace_android_vh_printk_store, NULL);
}

static void log_buf_logger_vh_printk_store_remove(
		struct log_buf_drvdata *drvdata)
{
	unregister_trace_android_vh_printk_store(
			__trace_android_vh_printk_store, NULL);
}

static const struct log_buf_logger log_buf_logger_vh_printk_store = {
	.probe = log_buf_logger_vh_printk_store_probe,
	.remove = log_buf_logger_vh_printk_store_remove,
};

const struct log_buf_logger *__log_buf_logger_vh_printk_store_creator(void)
{
	return &log_buf_logger_vh_printk_store;
}
