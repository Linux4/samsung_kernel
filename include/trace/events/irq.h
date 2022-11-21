#undef TRACE_SYSTEM
#define TRACE_SYSTEM irq

#if !defined(_TRACE_IRQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IRQ_H

#include <linux/tracepoint.h>

struct irqaction;
struct softirq_action;

#define softirq_name(sirq) { sirq##_SOFTIRQ, #sirq }
#define show_softirq_name(val)				\
	__print_symbolic(val,				\
			 softirq_name(HI),		\
			 softirq_name(TIMER),		\
			 softirq_name(NET_TX),		\
			 softirq_name(NET_RX),		\
			 softirq_name(BLOCK),		\
			 softirq_name(BLOCK_IOPOLL),	\
			 softirq_name(TASKLET),		\
			 softirq_name(SCHED),		\
			 softirq_name(HRTIMER),		\
			 softirq_name(RCU))

/**
 * irq_handler_entry - called immediately before the irq action handler
 * @irq: irq number
 * @action: pointer to struct irqaction
 *
 * The struct irqaction pointed to by @action contains various
 * information about the handler, including the device name,
 * @action->name, and the device id, @action->dev_id. When used in
 * conjunction with the irq_handler_exit tracepoint, we can figure
 * out irq handler latencies.
 */
TRACE_EVENT(irq_handler_entry,

	TP_PROTO(int irq, struct irqaction *action),

	TP_ARGS(irq, action),

	TP_STRUCT__entry(
		__field(	int,	irq		)
		__string(	name,	action->name	)
		__field(void*,	handler)
	),

	TP_fast_assign(
		__entry->irq = irq;
		__assign_str(name, action->name);
		__entry->handler = action->handler;
	),

	TP_printk("irq=%d name=%s handler=%pf",
		 __entry->irq, __get_str(name), __entry->handler)
);

TRACE_EVENT(mdss_dsi_cmd_mdp_busy_start,
	TP_PROTO(unsigned long ctrl),
	TP_ARGS(ctrl),
	TP_STRUCT__entry(
		__field(	unsigned long, ctrl)
	),
	TP_fast_assign(
		__entry->ctrl = ctrl;
	),
	TP_printk("ctrl=%lx",__entry->ctrl)
);

TRACE_EVENT(mdss_dsi_cmd_mdp_busy_end,
	TP_PROTO(unsigned int timeout),
	TP_ARGS(timeout),
	TP_STRUCT__entry(
		__field(	unsigned int, timeout)
	),
	TP_fast_assign(
		__entry->timeout = timeout;
	),
	TP_printk("timeout=%d",__entry->timeout)
);

TRACE_EVENT(mdss_mdp_isr_start,
	TP_PROTO(int irq, unsigned int isr, unsigned int mask),
	TP_ARGS(irq, isr, mask),
	TP_STRUCT__entry(
		__field(int, irq)
		__field(unsigned int, isr)
		__field(unsigned int, mask)
	),
	TP_fast_assign(
		__entry->irq = irq;
		__entry->isr = isr;
		__entry->mask = mask;
	),

	TP_printk("irq=%d isr=%x mask=%x",
		__entry->irq, __entry->isr, __entry->mask)
);

TRACE_EVENT(mdss_mdp_isr_end,
	TP_PROTO(int irq, unsigned int hist_isr, unsigned int hist_mask),
	TP_ARGS(irq, hist_isr, hist_mask),
	TP_STRUCT__entry(
		__field(int, irq)
		__field(unsigned int, hist_isr)
		__field(unsigned int, hist_mask)
	),
	TP_fast_assign(
		__entry->irq = irq;
		__entry->hist_isr = hist_isr;
		__entry->hist_mask = hist_mask;
	),

	TP_printk("irq=%d hist_isr=%x hist_mask=%x",
		__entry->irq, __entry->hist_isr, __entry->hist_mask)
);

TRACE_EVENT(mdss_dsi_isr_start,
	TP_PROTO(int irq, unsigned int isr, unsigned long ctrl),
	TP_ARGS(irq, isr, ctrl),
	TP_STRUCT__entry(
		__field(int, irq)
		__field(unsigned int, isr)
		__field(unsigned long, ctrl)
	),
	TP_fast_assign(
		__entry->irq = irq;
		__entry->isr = isr;
		__entry->ctrl = ctrl;
	),

	TP_printk("irq=%d isr=%x ctrl=%lx",
		__entry->irq, __entry->isr, __entry->ctrl)
);

TRACE_EVENT(mdss_dsi_isr_end,
	TP_PROTO(int irq),
	TP_ARGS(irq),
	TP_STRUCT__entry(
		__field(	int, irq)
	),
	TP_fast_assign(
		__entry->irq = irq;
	),
	TP_printk("irq=%d",__entry->irq)
);
/**
 * irq_handler_exit - called immediately after the irq action handler returns
 * @irq: irq number
 * @action: pointer to struct irqaction
 * @ret: return value
 *
 * If the @ret value is set to IRQ_HANDLED, then we know that the corresponding
 * @action->handler scuccessully handled this irq. Otherwise, the irq might be
 * a shared irq line, or the irq was not handled successfully. Can be used in
 * conjunction with the irq_handler_entry to understand irq handler latencies.
 */
TRACE_EVENT(irq_handler_exit,

	TP_PROTO(int irq, struct irqaction *action, int ret),

	TP_ARGS(irq, action, ret),

	TP_STRUCT__entry(
		__field(	int,	irq	)
		__field(	int,	ret	)
	),

	TP_fast_assign(
		__entry->irq	= irq;
		__entry->ret	= ret;
	),

	TP_printk("irq=%d ret=%s",
		  __entry->irq, __entry->ret ? "handled" : "unhandled")
);

DECLARE_EVENT_CLASS(softirq,

	TP_PROTO(unsigned int vec_nr),

	TP_ARGS(vec_nr),

	TP_STRUCT__entry(
		__field(	unsigned int,	vec	)
	),

	TP_fast_assign(
		__entry->vec = vec_nr;
	),

	TP_printk("vec=%u [action=%s]", __entry->vec,
		  show_softirq_name(__entry->vec))
);

/**
 * softirq_entry - called immediately before the softirq handler
 * @vec_nr:  softirq vector number
 *
 * When used in combination with the softirq_exit tracepoint
 * we can determine the softirq handler runtine.
 */
DEFINE_EVENT(softirq, softirq_entry,

	TP_PROTO(unsigned int vec_nr),

	TP_ARGS(vec_nr)
);

/**
 * softirq_exit - called immediately after the softirq handler returns
 * @vec_nr:  softirq vector number
 *
 * When used in combination with the softirq_entry tracepoint
 * we can determine the softirq handler runtine.
 */
DEFINE_EVENT(softirq, softirq_exit,

	TP_PROTO(unsigned int vec_nr),

	TP_ARGS(vec_nr)
);

/**
 * softirq_raise - called immediately when a softirq is raised
 * @vec_nr:  softirq vector number
 *
 * When used in combination with the softirq_entry tracepoint
 * we can determine the softirq raise to run latency.
 */
DEFINE_EVENT(softirq, softirq_raise,

	TP_PROTO(unsigned int vec_nr),

	TP_ARGS(vec_nr)
);

#endif /*  _TRACE_IRQ_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
