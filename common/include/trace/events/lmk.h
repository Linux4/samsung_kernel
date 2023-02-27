#undef TRACE_SYSTEM
#define TRACE_SYSTEM lmk

#if !defined(_TRACE_LMK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_LMK_H
#include <linux/tracepoint.h>

TRACE_EVENT(lmk_s,
        TP_PROTO(int seq),

        TP_ARGS(seq),

	TP_STRUCT__entry(
		__field(	int,	seq)
        ),

	TP_fast_assign(
		__entry->seq = seq;
        ),

	TP_printk("start shrink seq=%d", __entry->seq)
);

TRACE_EVENT(lmk_e,
        TP_PROTO(int seq),

        TP_ARGS(seq),

	TP_STRUCT__entry(
		__field(	int,	seq)
        ),

	TP_fast_assign(
		__entry->seq = seq;
        ),

	TP_printk("end shrink seq=%d", __entry->seq)
);


TRACE_EVENT(lmk_kill_s,

	TP_PROTO(struct task_struct *task, int adj),

	TP_ARGS(task, adj),

	TP_STRUCT__entry(
		__field(	pid_t,	pid)
		__array(	char,	comm,	TASK_COMM_LEN )
		__field(	int,	adj)
	),

	TP_fast_assign(
		__entry->pid = task->pid;
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->adj = adj;
	),

	TP_printk("kill pid=%d comm=%s adj=%d",
		__entry->pid, __entry->comm, __entry->adj)
);

TRACE_EVENT(lmk_kill_e,
        TP_PROTO(int seq),

        TP_ARGS(seq),

	TP_STRUCT__entry(
		__field(	int,	seq)
        ),

	TP_fast_assign(
		__entry->seq = seq;
        ),
	TP_printk("kill signaled seq=%d", __entry->seq)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
