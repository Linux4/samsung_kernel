#undef TRACE_SYSTEM
#define TRACE_SYSTEM tel

#if !defined(_TEL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TEL_TRACE_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/tracepoint.h>

TRACE_EVENT(psd_xmit_skb_realloc,

	TP_PROTO(struct sk_buff *old, struct sk_buff *new),

	TP_ARGS(old, new),

	TP_STRUCT__entry(
		__field(void *, old)
		__field(void *, new)
		__string(name, old->dev->name)
	),

	TP_fast_assign(
		__entry->old = old;
		__entry->new = new;
		__assign_str(name, old->dev->name);
	),

	TP_printk("dev=%s old=%p new=%p",
		__get_str(name), __entry->old, __entry->new)
);

TRACE_EVENT(psd_xmit,

	TP_PROTO(struct sk_buff *skb, int slot),

	TP_ARGS(skb, slot),

	TP_STRUCT__entry(
		__field(void *, skbaddr)
		__field(int, len)
		__field(int, slot)
		__string(name, skb->dev->name)
	),

	TP_fast_assign(
		__entry->skbaddr = skb;
		__entry->len = skb->len;
		__entry->slot = slot;
		__assign_str(name, skb->dev->name);
	),

	TP_printk("dev=%s skbaddr=%p len=%d slot=%d",
		__get_str(name), __entry->skbaddr,
		__entry->len, __entry->slot)
);

TRACE_EVENT(psd_xmit_irq,

	TP_PROTO(int cnt),

	TP_ARGS(cnt),

	TP_STRUCT__entry(
		__field(int, cnt)
	),

	TP_fast_assign(
		__entry->cnt = cnt;
	),

	TP_printk("cnt=%d", __entry->cnt)
);

TRACE_EVENT(psd_recv_irq,

	TP_PROTO(int cp_wptr),

	TP_ARGS(cp_wptr),

	TP_STRUCT__entry(
		__field(int, cp_wptr)
	),

	TP_fast_assign(
		__entry->cp_wptr = cp_wptr;
	),

	TP_printk("cp_wptr=%d", __entry->cp_wptr)
);

TRACE_EVENT(psd_recv,

	TP_PROTO(int slot),

	TP_ARGS(slot),

	TP_STRUCT__entry(
		__field(int, slot)
	),

	TP_fast_assign(
		__entry->slot = slot;
	),

	TP_printk("slot=%d", __entry->slot)
);

DECLARE_EVENT_CLASS(portq_xfer_template,

	TP_PROTO(int group, int port, int len),

	TP_ARGS(group, port, len),

	TP_STRUCT__entry(
		__field(int, group)
		__field(int, port)
		__field(int, len)
	),

	TP_fast_assign(
		__entry->group = group;
		__entry->port = port;
		__entry->len = len;
	),

	TP_printk("group=%d port=%d len=%d",
		__entry->group, __entry->port, __entry->len)
);

DEFINE_EVENT(portq_xfer_template, portq_xmit,
	TP_PROTO(int group, int port, int len),
	TP_ARGS(group, port, len));

DEFINE_EVENT(portq_xfer_template, portq_recv,
	TP_PROTO(int group, int port, int len),
	TP_ARGS(group, port, len));

DECLARE_EVENT_CLASS(drb_xfer_template,

	TP_PROTO(int type, int len),

	TP_ARGS(type, len),

	TP_STRUCT__entry(
		__field(int, type)
		__field(int, len)
	),

	TP_fast_assign(
		__entry->type = type;
		__entry->len = len;
	),

	TP_printk("type=%d len=%d", __entry->type, __entry->len)
);

DEFINE_EVENT(drb_xfer_template, drb_xmit,
	TP_PROTO(int type, int len),
	TP_ARGS(type, len));

DEFINE_EVENT(drb_xfer_template, drb_recv,
	TP_PROTO(int type, int len),
	TP_ARGS(type, len));

#endif /* _TEL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ../include
#define TRACE_INCLUDE_FILE tel_trace
#include <trace/define_trace.h>
