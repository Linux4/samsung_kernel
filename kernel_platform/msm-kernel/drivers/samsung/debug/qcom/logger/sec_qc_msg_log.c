// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>

#include <trace/events/timer.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/debug/sec_debug_region.h>
#include <linux/samsung/debug/qcom/sec_qc_msg_log.h>

#include "sec_qc_logger.h"

static struct sec_qc_msg_log_data *msg_log_data __read_mostly;

static __always_inline bool __msg_log_is_probed(void)
{
	return !!msg_log_data;
}

static __always_inline int ____msg_log(void *caller, const char *fmt,
		va_list args)
{
	struct sec_qc_msg_buf *qc_msg_buf;
	int cpu = smp_processor_id();
	int r;
	unsigned int i;

	if (!__msg_log_is_probed())
		return 0;

	i = ++(msg_log_data[cpu].idx) & (SEC_QC_MSG_LOG_MAX - 1);
	qc_msg_buf = &msg_log_data[cpu].buf[i];

	qc_msg_buf->time = cpu_clock(cpu);
	r = vsnprintf(qc_msg_buf->msg, sizeof(qc_msg_buf->msg), fmt, args);

	qc_msg_buf->caller0 = __builtin_return_address(0);
	qc_msg_buf->caller1 = caller;
	qc_msg_buf->task = current->comm;

	return r;
}

static int __msg_log_set_msg_log_data(struct builder *bd)
{
	struct qc_logger *logger = container_of(bd, struct qc_logger, bd);
	struct sec_dbg_region_client *client = logger->drvdata->client;

	msg_log_data = (void *)client->virt;
	msg_log_data->idx = -1;

	return 0;
}

static void __msg_log_unset_msg_log_data(struct builder *bd)
{
	msg_log_data = NULL;
}

#if IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML)
int notrace ___sec_debug_msg_log(void *caller, const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = ____msg_log(caller, fmt, args);
	va_end(args);

	return r;
}
#endif

static int notrace __msg_log(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = ____msg_log(__builtin_return_address(1), fmt, args);
	va_end(args);

	return r;
}

static void sec_qc_msg_log_trace_hrtimer_expire_entry(void *unused,
		struct hrtimer *timer, ktime_t *now)
{
	__msg_log("hrtimer %pS entry", timer->function);
}

static int __msg_log_register_trace_hrtimer_expire_entry(struct builder *bd)
{
	return register_trace_hrtimer_expire_entry(
			sec_qc_msg_log_trace_hrtimer_expire_entry, NULL);
}

static void __msg_log_unregister_trace_hrtimer_expire_entry(
		struct builder *bd)
{
	unregister_trace_hrtimer_expire_entry(
			sec_qc_msg_log_trace_hrtimer_expire_entry, NULL);
}

static void sec_qc_msg_log_trace_hrtimer_expire_exit(void *unused,
		struct hrtimer *timer)
{
	__msg_log("hrtimer %pS exit", timer->function);
}

static int __msg_log_register_trace_hrtimer_expire_exit(struct builder *bd)
{
	return register_trace_hrtimer_expire_exit(
			sec_qc_msg_log_trace_hrtimer_expire_exit, NULL);
}

static void __msg_log_unregister_trace_hrtimer_expire_exit(
		struct builder *bd)
{
	unregister_trace_hrtimer_expire_exit(
			sec_qc_msg_log_trace_hrtimer_expire_exit, NULL);
}

#if 0
static void sec_qc_msg_log_trace_timer_expire_entry(void *unused,
		struct timer_list *timer, unsigned long baseclk)
{
	__msg_log("timer %pS entry", timer->function);
}

static int __msg_log_register_trace_timer_expire_entry(struct builder *bd)
{
	return register_trace_timer_expire_entry(
			sec_qc_msg_log_trace_timer_expire_entry, NULL);
}

static void __msg_log_unregister_trace_timer_expire_entry(struct builder *bd)
{
	unregister_trace_timer_expire_entry(
			sec_qc_msg_log_trace_timer_expire_entry, NULL);
}

static void sec_qc_msg_log_trace_timer_expire_exit(void *unused,
		struct timer_list *timer)
{
	__msg_log("timer %pS exit", timer->function);
}

static int __msg_log_register_trace_timer_expire_exit(struct builder *bd)
{
	return register_trace_timer_expire_exit(
			sec_qc_msg_log_trace_timer_expire_exit, NULL);
}

static void __msg_log_unregister_trace_timer_expire_exit(struct builder *bd)
{
	unregister_trace_timer_expire_exit(
			sec_qc_msg_log_trace_timer_expire_exit, NULL);
}
#endif

static size_t sec_qc_msg_log_get_data_size(struct qc_logger *logger)
{
	return sizeof(struct sec_qc_msg_log_data) * num_possible_cpus();
}

static struct dev_builder __msg_logdev_builder[] = {
	DEVICE_BUILDER(__msg_log_set_msg_log_data,
		       __msg_log_unset_msg_log_data),
};

static struct dev_builder __msg_logdev_builder_trace[] = {
	DEVICE_BUILDER(__msg_log_register_trace_hrtimer_expire_entry,
		       __msg_log_unregister_trace_hrtimer_expire_entry),
	DEVICE_BUILDER(__msg_log_register_trace_hrtimer_expire_exit,
		       __msg_log_unregister_trace_hrtimer_expire_exit),
#if 0
	DEVICE_BUILDER(__msg_log_register_trace_timer_expire_entry,
		       __msg_log_unregister_trace_timer_expire_entry),
	DEVICE_BUILDER(__msg_log_register_trace_timer_expire_exit,
		       __msg_log_unregister_trace_timer_expire_exit),
#endif
};

static int sec_qc_msg_log_probe(struct qc_logger *logger)
{
	int err = __qc_logger_sub_module_probe(logger,
			__msg_logdev_builder,
			ARRAY_SIZE(__msg_logdev_builder));
	if (err)
		return err;

	if (IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML))
		return 0;

	return __qc_logger_sub_module_probe(logger,
			__msg_logdev_builder_trace,
			ARRAY_SIZE(__msg_logdev_builder_trace));
}

static void sec_qc_msg_log_remove(struct qc_logger *logger)
{
	__qc_logger_sub_module_remove(logger,
			__msg_logdev_builder,
			ARRAY_SIZE(__msg_logdev_builder));

	if (IS_ENABLED(CONFIG_KUNIT) && IS_ENABLED(CONFIG_UML))
		return;

	__qc_logger_sub_module_remove(logger,
			__msg_logdev_builder_trace,
			ARRAY_SIZE(__msg_logdev_builder_trace));
}

struct qc_logger __qc_logger_msg_log = {
	.get_data_size = sec_qc_msg_log_get_data_size,
	.probe = sec_qc_msg_log_probe,
	.remove = sec_qc_msg_log_remove,
	.unique_id = SEC_QC_MSG_LOG_MAGIC,
};
