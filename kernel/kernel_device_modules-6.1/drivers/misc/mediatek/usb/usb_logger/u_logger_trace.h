/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Dumper Driver
 * *
 * Copyright (c) 2023 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM usb_logger

#if !defined(__U_LOGGER_TRACE_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __U_LOGGER_TRACE_H__

#include <linux/types.h>
#include <linux/tracepoint.h>
#include "u_logger.h"

#define U_LOGGER_MSG_MAX	256

#if IS_ENABLED(CONFIG_TRACING)
void u_logger_dbg_trace(struct device *dev, const char *fmt, ...);
#else
static inline void u_logger_dbg_trace(struct device *dev, const char *fmt, ...) {}
#endif /* CONFIG_TRACING */

TRACE_EVENT(u_logger_log,
	TP_PROTO(struct device *dev, struct va_format *vaf),
	TP_ARGS(dev, vaf),
	TP_STRUCT__entry(
		__string(name, dev_name(dev))
		__dynamic_array(char, msg, U_LOGGER_MSG_MAX)
	),
	TP_fast_assign(
		int n;

		__assign_str(name, dev_name(dev));
		n = vsnprintf(__get_str(msg), U_LOGGER_MSG_MAX, vaf->fmt, *vaf->va);
	),
	TP_printk("%s: %s", __get_str(name), __get_str(msg))
);

DECLARE_EVENT_CLASS(log_urb_info,
	TP_PROTO(struct u_logger *logger, struct mtk_urb *urb),
	TP_ARGS(logger, urb),
	TP_STRUCT__entry(
		__field(void *, urb)
		__field(int, dir)
		__field(int, ep_num)
		__field(int, length)
		__field(int, type)
		__dynamic_array(char, info, info_str_length(logger, urb))
	),
	TP_fast_assign(
		__entry->urb = urb->addr;
		__entry->dir = usb_endpoint_dir_in(urb->desc);
		__entry->ep_num = usb_endpoint_num(urb->desc);
		__entry->length = urb->transfer_buffer_length;
		__entry->type = usb_endpoint_type(urb->desc);

		info_str_fill(logger, __get_str(info), urb);
	),
	TP_printk("[%s %s ep%d][%p %d] %s",
		xfer_type_name(__entry->type),
		__entry->dir ? "in" : "out",
		__entry->ep_num,
		__entry->urb,
		__entry->length,
		__get_str(info))
);

DEFINE_EVENT(log_urb_info, urb_enqueue,
	TP_PROTO(struct u_logger *logger, struct mtk_urb *urb),
	TP_ARGS(logger, urb)
);

DECLARE_EVENT_CLASS(log_urb_data,
	TP_PROTO(struct u_logger *logger, struct mtk_urb *urb),
	TP_ARGS(logger, urb),
	TP_STRUCT__entry(
		__field(void *, urb)
		__field(int, dir)
		__field(int, ep_num)
		__field(int, length)
		__field(int, type)
		__dynamic_array(char, raw, raw_str_length(logger, urb))
	),

	TP_fast_assign(
		__entry->urb = urb->addr;
		__entry->dir = usb_endpoint_dir_in(urb->desc);
		__entry->ep_num = usb_endpoint_num(urb->desc);
		__entry->length = urb->actual_length;
		__entry->type = usb_endpoint_type(urb->desc);

		raw_str_fill(logger, __get_str(raw), urb);
	),

	TP_printk("[%s %s ep%d][%p %d] %s",
		xfer_type_name(__entry->type),
		__entry->dir ? "in" : "out",
		__entry->ep_num,
		__entry->urb,
		__entry->length,
		__get_str(raw))
);

DEFINE_EVENT(log_urb_data, urb_giveback,
	TP_PROTO(struct u_logger *logger, struct mtk_urb *urb),
	TP_ARGS(logger, urb)
);

#endif /* __U_LOGGER_TRACE_H__ */
/* this part has to be here */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE u_logger_trace

#include <trace/define_trace.h>
