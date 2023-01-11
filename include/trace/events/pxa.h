#undef TRACE_SYSTEM
#define TRACE_SYSTEM pxa

#if !defined(_TRACE_PXA_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PXA_H

#include <linux/tracepoint.h>

#define LPM_ENTRY(index)	((index << 16) | 0)
#define LPM_EXIT(index)		((index << 16) | 1)

#define CLK_CHG_ENTRY	1
#define CLK_CHG_EXIT	2

#define CLK_ENABLE	1
#define CLK_DISABLE	2

#define HOTPLUG_ENTRY	1
#define HOTPLUG_EXIT	2

#define PXA_SDHC_DMA_START	0
#define PXA_SDHC_DMA_END	1

TRACE_EVENT(pxa_sdhc_dma,
	TP_PROTO(u32 state, u32 host),

	TP_ARGS(state, host),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, host)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->host = host;
	),

	TP_printk("state: %x host: %x", __entry->state, __entry->host)
);

TRACE_EVENT(pxa_cpu_idle,
	TP_PROTO(u32 state, u32 cpu_id, u32 cluster),

	TP_ARGS(state, cpu_id, cluster),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, cpu_id)
		__field(u32, cluster)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->cpu_id = cpu_id;
		__entry->cluster = cluster;
	),

	TP_printk("state: 0x%x cpu_id: %u cluster %u",
		__entry->state, __entry->cpu_id, __entry->cluster)
);

TRACE_EVENT(pxa_set_voltage,
	TP_PROTO(const char *val0, u32 val1, u32 val2),

	TP_ARGS(val0, val1, val2),

	TP_STRUCT__entry(
		__field(const char *, val0)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->val0 = val0;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("rail %s set voltage: %umV -> %umV",
		__entry->val0, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_core_hotplug,
	TP_PROTO(u32 state, u32 id),

	TP_ARGS(state, id),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, id)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->id = id;
	),

	TP_printk("state: %u cpu_id: %u", __entry->state, __entry->id)
);

/* Show id, task number and mips for each online cpus */
TRACE_EVENT(pxa_hp_single,
	TP_PROTO(u32 id, u32 task, u32 load, u32 mips),

	TP_ARGS(id, task, load, mips),

	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, task)
		__field(u32, load)
		__field(u32, mips)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->task = task;
		__entry->load = load;
		__entry->mips = mips;
	),

	TP_printk("id:%1u task:%3u load:%3u mips:%6u",
		  __entry->id, __entry->task, __entry->load, __entry->mips)
);

/* This trace is for print mips in every hotplug window size */
TRACE_EVENT(pxa_hp_total,
	TP_PROTO(u32 cur_freq, u32 mips, u32 cpu_num),

	TP_ARGS(cur_freq, mips, cpu_num),

	TP_STRUCT__entry(
		__field(u32, cur_freq)
		__field(u32, mips)
		__field(u32, cpu_num)
	),

	TP_fast_assign(
		__entry->cur_freq = cur_freq;
		__entry->mips = mips;
		__entry->cpu_num = cpu_num;
	),

	TP_printk("cur_freq:%4uMHz mips:%6u cpu_num:%1u",
		  __entry->cur_freq, __entry->mips, __entry->cpu_num)
);

TRACE_EVENT(pxa_ddr_workload,
	TP_PROTO(u32 workload, u32 freq, s32 throughput),

	TP_ARGS(workload, freq, throughput),

	TP_STRUCT__entry(
		__field(u32, workload)
		__field(u32, freq)
		__field(s32, throughput)
	),

	TP_fast_assign(
		__entry->workload = workload;
		__entry->freq = freq;
		__entry->throughput = throughput;
	),

	TP_printk("workload: %u freq: %uKHZ throughput: %u", __entry->workload,
					__entry->freq,  __entry->throughput)
);

TRACE_EVENT(pxa_ddr_upthreshold,
	TP_PROTO(u32 upthreshold),

	TP_ARGS(upthreshold),

	TP_STRUCT__entry(
		__field(u32, upthreshold)
	),

	TP_fast_assign(
		__entry->upthreshold = upthreshold;
	),

	TP_printk("ddr devfreq upthreshold: %u", __entry->upthreshold)
);

TRACE_EVENT(pxa_core_clk_chg,
	TP_PROTO(u32 state, u32 val1, u32 val2),

	TP_ARGS(state, val1, val2),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("state: %u freq: %u -> %u",
		__entry->state, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_ddraxi_clk_chg,
	TP_PROTO(u32 state, u32 val1, u32 val2),

	TP_ARGS(state, val1, val2),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("state: %u freq: %u -> %u",
		__entry->state, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_ddr_clk_chg,
	TP_PROTO(u32 state, u32 val1, u32 val2),

	TP_ARGS(state, val1, val2),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("state: %u freq: %u -> %u",
		__entry->state, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_axi_clk_chg,
	TP_PROTO(u32 state, u32 val1, u32 val2),

	TP_ARGS(state, val1, val2),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("state: %u freq: %u -> %u",
		__entry->state, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_gc_clk,
	TP_PROTO(u32 state, u32 val),

	TP_ARGS(state, val),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, val)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->val = val;
	),

	TP_printk("state: %u clk: 0x%x", __entry->state, __entry->val)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */

#endif /* _TRACE_PXA_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
