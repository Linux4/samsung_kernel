#undef TRACE_SYSTEM
#define TRACE_SYSTEM pxa_trace

#if !defined(_TRACE_PXA_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PXA_TRACE_H

#include <linux/tracepoint.h>

#define LPM_ENTRY(index)	((index << 16) | 0)
#define LPM_EXIT(index)		((index << 16) | 1)

#define CLK_CHG_ENTRY	1
#define CLK_CHG_EXIT	2

#define CLK_ENABLE	1
#define CLK_DISABLE	2

#define HOTPLUG_ENTRY	1
#define HOTPLUG_EXIT	2


TRACE_EVENT(pxa_cpu_idle,
	TP_PROTO(u32 state, u32 cpu_id),

	TP_ARGS(state, cpu_id),

	TP_STRUCT__entry(
		__field(u32, state)
		__field(u32, cpu_id)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("state: 0x%x cpu_id: %u", __entry->state, __entry->cpu_id)
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

TRACE_EVENT(pxa_gc_clk_chg,
	TP_PROTO(u32 val1, u32 val2),

	TP_ARGS(val1, val2),

	TP_STRUCT__entry(
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("freq: %u -> %u", __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_vpu_clk,
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

TRACE_EVENT(pxa_vpu_clk_chg,
	TP_PROTO(u32 val1, u32 val2),

	TP_ARGS(val1, val2),

	TP_STRUCT__entry(
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("freq: %u -> %u", __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_pll_vco_enable,
	TP_PROTO(u32 num, u32 val1, u32 val2),

	TP_ARGS(num, val1, val2),

	TP_STRUCT__entry(
		__field(u32, num)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->num = num;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("pll%u, swcr: 0x%x, pllcr: 0x%x",
		__entry->num, __entry->val1, __entry->val2)
);

TRACE_EVENT(pxa_pll_vco_disable,
	TP_PROTO(u32 num),

	TP_ARGS(num),

	TP_STRUCT__entry(
		__field(u32, num)
	),

	TP_fast_assign(
		__entry->num = num;
	),

	TP_printk("pll%u", __entry->num)
);

TRACE_EVENT(pxa_isp_dxo_clk,
	TP_PROTO(u32 state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->state = state;
	),

	TP_printk("state: %u", __entry->state)
);

TRACE_EVENT(pxa_isp_dxo_clk_chg,
	TP_PROTO(u32 val),

	TP_ARGS(val),

	TP_STRUCT__entry(
		__field(u32, val)
	),

	TP_fast_assign(
		__entry->val = val;
	),

	TP_printk("freq: %u", __entry->val)
);

TRACE_EVENT(pxa_set_voltage,
	TP_PROTO(u32 val0, u32 val1, u32 val2),

	TP_ARGS(val0, val1, val2),

	TP_STRUCT__entry(
		__field(u32, val0)
		__field(u32, val1)
		__field(u32, val2)
	),

	TP_fast_assign(
		__entry->val0 = val0;
		__entry->val1 = val1;
		__entry->val2 = val2;
	),

	TP_printk("rail %x set voltage: %umV -> %umV",
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

TRACE_EVENT(pxa_ddr_workload,
	TP_PROTO(u32 workload, u32 freq),

	TP_ARGS(workload, freq),

	TP_STRUCT__entry(
		__field(u32, workload)
		__field(u32, freq)
	),

	TP_fast_assign(
		__entry->workload = workload;
		__entry->freq = freq;
	),

	TP_printk("workload: %u freq: %u", __entry->workload, __entry->freq)
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

TRACE_EVENT(pxa_ddr_lpm,
	TP_PROTO(u32 value),

	TP_ARGS(value),

	TP_STRUCT__entry(
		__field(u32, value)
	),

	TP_fast_assign(
		__entry->value = value;
	),

	TP_printk("dll val: 0x%x", __entry->value)
);

TRACE_EVENT(pxa_ddr_lpm_tbl_update,
	TP_PROTO(u32 value),

	TP_ARGS(value),

	TP_STRUCT__entry(
		__field(u32, value)
	),

	TP_fast_assign(
		__entry->value = value;
	),

	TP_printk("lpm_tbl: 0x%x", __entry->value)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */

#endif /* _TRACE_PXA_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH plat

#define TRACE_INCLUDE_FILE pxa_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
