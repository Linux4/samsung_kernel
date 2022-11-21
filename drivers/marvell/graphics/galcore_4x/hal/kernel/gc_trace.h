#if MRVL_CONFIG_ENABLE_GC_TRACE
#undef TRACE_SYSTEM
#define TRACE_SYSTEM gc_trace

#if !defined(_TRACE_GC_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GC_TRACE_H

#include <linux/tracepoint.h>

#define TRACE_LOG_ENTRY     0
#define TRACE_LOG_EXIT      1

#define TRACE_REG_READ      0
#define TRACE_REG_WRITE     1

TRACE_EVENT(gc_reg_acc,
       TP_PROTO(u32 done, u32 access, u32 reg, u32 data),

       TP_ARGS(done, access, reg, data),

       TP_STRUCT__entry(
               __field(u32, done)
               __field(u32, access)
               __field(u32, reg)
               __field(u32, data)
       ),

       TP_fast_assign(
               __entry->done   = done;
               __entry->access = access;
               __entry->reg    = reg;
               __entry->data   = data;
       ),

       TP_printk("%d:%5s reg: 0x%04x value: 0x%8x",
                        __entry->done,
                        (TRACE_REG_READ == __entry->access)?"read":"write",
                        __entry->reg, __entry->data)
);

TRACE_EVENT(gc_workload,
    TP_PROTO(u32 core, u32 freq, u32 workload),

    TP_ARGS(core, freq, workload),

    TP_STRUCT__entry(
        __field(u32, core)
        __field(u32, freq)
        __field(u32, workload)
    ),

    TP_fast_assign(
        __entry->core = core;
        __entry->freq = freq;
        __entry->workload = workload;
    ),

    TP_printk("core: %d freq: %u workload: %2u", __entry->core, __entry->freq, __entry->workload)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */

#endif /* _TRACE_GC_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE

/* use absolate include path for this header file in order to be found in define_trace.h */
#define TRACE_INCLUDE_PATH GPU_DRV_SRC_ROOT/hal/kernel

#define TRACE_INCLUDE_FILE gc_trace

/* This part must be outside protection */
#include <trace/define_trace.h>

# else /* MRVL_CONFIG_ENABLE_GC_TRACE == 0 */

#ifndef _TRACE_GC_TRACE_H
#define _TRACE_GC_TRACE_H

#define TRACE_LOG_ENTRY     0
#define TRACE_LOG_EXIT      1

#define TRACE_REG_READ      0
#define TRACE_REG_WRITE     1

#define trace_gc_reg_acc(...)
#define trace_gc_workload(...)

#endif /* _TRACE_GC_TRACE_H */

# endif /* MRVL_CONFIG_ENABLE_GC_TRACE */
