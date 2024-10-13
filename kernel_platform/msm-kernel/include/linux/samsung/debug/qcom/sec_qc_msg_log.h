#ifndef __SEC_QC_MSG_LOG_H__
#define __SEC_QC_MSG_LOG_H__

#include <dt-bindings/samsung/debug/qcom/sec_qc_msg_log.h>

#define SEC_QC_MSG_LOG_MAX	1024

struct sec_qc_msg_buf {
	u64 time;
	char msg[64];
	void *caller0;
	void *caller1;
	char *task;
};

struct sec_qc_msg_log_data {
	unsigned int idx;
	struct sec_qc_msg_buf buf[SEC_QC_MSG_LOG_MAX];
} ____cacheline_aligned_in_smp;

#if IS_BUILTIN(CONFIG_SEC_QC_LOGGER)
/* TODO: do not call this function directly.
 * plz use sec_debug_msg_log macro instead.
 */
extern int ___sec_debug_msg_log(void *caller, const char *fmt, ...);

/* called @ kernel/softirq.c */
/* called @ kernel/time/timer.c */
/* called @ kernel/time/hrtimer.c */
#define sec_debug_msg_log(fmt, ...) \
	___sec_debug_msg_log(__builtin_return_address(0), fmt, ##__VA_ARGS__)
#else
#define sec_debug_msg_log(fmt, ...)
#endif

#endif /* __SEC_QC_MSG_LOG_H__ */
