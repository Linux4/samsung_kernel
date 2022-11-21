/*
 * AUTHOR: JK Kim <jk.man.kim@>
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos

#if !defined(_TRACE_EXYNOS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_H

#include <linux/tracepoint.h>
#include <linux/regulator/driver.h>
#include <soc/samsung/exynos-devfreq.h>
#include "../../../drivers/soc/samsung/pwrcal/pwrcal-vclk.h"

#define freq_name(flag) { flag##_EXYNOS_FREQ, #flag }
#define show_freq_name(val)				\
	__print_symbolic(val,				\
			 freq_name(APL),		\
			 freq_name(ATL),		\
			 freq_name(INT),		\
			 freq_name(MIF),		\
			 freq_name(ISP),		\
			 freq_name(DISP))

TRACE_EVENT(exynos_regulator_in,
	TP_PROTO(char *name, struct regulator_dev *rdev, unsigned int voltage),

	TP_ARGS(name, rdev, voltage),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, vsel_reg)
		__field(unsigned int, voltage)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->vsel_reg = rdev->desc->vsel_reg;
		__entry->voltage = voltage;
	),

	TP_printk("name: %s vsel_reg: %u voltage: %u",
		__get_str(name), __entry->vsel_reg, __entry->voltage)
);

TRACE_EVENT(exynos_regulator_on,
	TP_PROTO(char *name, struct regulator_dev *rdev, unsigned int voltage),

	TP_ARGS(name, rdev, voltage),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, vsel_reg)
		__field(unsigned int, voltage)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->vsel_reg = rdev->desc->vsel_reg;
		__entry->voltage = voltage;
	),

	TP_printk("name: %s vsel_reg: %u voltage: %u",
		__get_str(name), __entry->vsel_reg, __entry->voltage)
);

TRACE_EVENT(exynos_regulator_out,
	TP_PROTO(char *name, struct regulator_dev *rdev, unsigned int voltage),

	TP_ARGS(name, rdev, voltage),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, vsel_reg)
		__field(unsigned int, voltage)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->vsel_reg = rdev->desc->vsel_reg;
		__entry->voltage = voltage;
	),

	TP_printk("name: %s vsel_reg: %u voltage: %u",
		__get_str(name), __entry->vsel_reg, __entry->voltage)
);

TRACE_EVENT(exynos_thermal,
	TP_PROTO(void * data, unsigned int temp, char *name, unsigned int maxcooling),

	TP_ARGS(data, temp, name, maxcooling),

	TP_STRUCT__entry(
		__field(void *,data)
		__field(unsigned int, temp)
		__string(name, name)
		__field(unsigned int, maxcooling)
	),

	TP_fast_assign(
		__entry->data = data;
		__entry->temp = temp;
		__assign_str(name, name);
		__entry->maxcooling = maxcooling;
	),

	TP_printk("(struct ...)data: %p temp: %u name: %s maxcooling:%u",
		__entry->data, __entry->temp, __get_str(name), __entry->maxcooling)
);

TRACE_EVENT(exynos_clk_in,
	TP_PROTO(struct vclk *vclk, const char *name),

	TP_ARGS(vclk, name),

	TP_STRUCT__entry(
		__field(void *, vclk)
		__string(name, name)
	),

	TP_fast_assign(
		__entry->vclk = vclk;
		__assign_str(name, name);
	),

	TP_printk("name: %s vclk: %p",
		__get_str(name), __entry->vclk)
);

TRACE_EVENT(exynos_clk_on,
	TP_PROTO(struct vclk *vclk, const char *name),

	TP_ARGS(vclk, name),

	TP_STRUCT__entry(
		__field(void *, vclk)
		__string(name, name)
	),

	TP_fast_assign(
		__entry->vclk = vclk;
		__assign_str(name, name);
	),

	TP_printk("name: %s vclk: %p",
		__get_str(name), __entry->vclk)
);

TRACE_EVENT(exynos_clk_out,
	TP_PROTO(struct vclk *vclk, const char *name),

	TP_ARGS(vclk, name),

	TP_STRUCT__entry(
		__field(void *, vclk)
		__string(name, name)
	),

	TP_fast_assign(
		__entry->vclk = vclk;
		__assign_str(name, name);
	),

	TP_printk("name: %s vclk: %p",
		__get_str(name), __entry->vclk)
);

TRACE_EVENT(exynos_freq_in,
	TP_PROTO(u32 flag, u32 freq),

	TP_ARGS(flag, freq),

	TP_STRUCT__entry(
		__field(u32, flag)
		__field(u32, freq)
	),

	TP_fast_assign(
		__entry->flag = flag;
		__entry->freq = freq;
	),

	TP_printk("name: %s freq: %u",
		show_freq_name(__entry->flag), __entry->freq)

);

TRACE_EVENT(exynos_freq_out,
	TP_PROTO(u32 flag, u32 freq),

	TP_ARGS(flag, freq),

	TP_STRUCT__entry(
		__field(u32, flag)
		__field(u32, freq)
	),

	TP_fast_assign(
		__entry->flag = flag;
		__entry->freq = freq;
	),

	TP_printk("name: %s freq: %u",
		show_freq_name(__entry->flag), __entry->freq)
);

TRACE_EVENT(exynos_clockevent,
	TP_PROTO(long long clc, int64_t delta, void *next_event),

	TP_ARGS(clc, delta, next_event),

	TP_STRUCT__entry(
		__field(long long, clc)
		__field(int64_t, delta)
		__field(long long, next_event)
	),

	TP_fast_assign(
		__entry->clc = clc;
		__entry->delta = delta;
		__entry->next_event = *((long long *)next_event);
	),

	TP_printk("clc: %lld delta: %lld next_event:%lld",
		__entry->clc, __entry->delta, __entry->next_event)
);

TRACE_EVENT(exynos_smc_in,
	TP_PROTO(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3),

	TP_ARGS(cmd, arg1, arg2, arg3),

	TP_STRUCT__entry(
		__field(unsigned long, cmd)
		__field(unsigned long, arg1)
		__field(unsigned long, arg2)
		__field(unsigned long, arg3)
	),

	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->arg1 = arg1;
		__entry->arg2 = arg2;
		__entry->arg3 = arg3;
	),

	TP_printk("cmd: %lx arg1: %lu arg2: %lu arg3: %lu",
		__entry->cmd, __entry->arg1, __entry->arg2, __entry->arg3)
);

TRACE_EVENT(exynos_smc_out,
	TP_PROTO(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3),

	TP_ARGS(cmd, arg1, arg2, arg3),

	TP_STRUCT__entry(
		__field(unsigned long, cmd)
		__field(unsigned long, arg1)
		__field(unsigned long, arg2)
		__field(unsigned long, arg3)
	),

	TP_fast_assign(
		__entry->cmd = cmd;
		__entry->arg1 = arg1;
		__entry->arg2 = arg2;
		__entry->arg3 = arg3;
	),

	TP_printk("cmd: %lx arg1: %lu arg2: %lu arg3: %lu",
		__entry->cmd, __entry->arg1, __entry->arg2, __entry->arg3)
);

TRACE_EVENT(exynos_cpuidle_in,
	TP_PROTO(unsigned int idx),

	TP_ARGS(idx),

	TP_STRUCT__entry(
		__field(unsigned int, idx)
	),

	TP_fast_assign(
		__entry->idx = idx;
	),

	TP_printk("idx: %u", __entry->idx)
);

TRACE_EVENT(exynos_cpuidle_out,
	TP_PROTO(unsigned int idx),

	TP_ARGS(idx),

	TP_STRUCT__entry(
		__field(unsigned int, idx)
	),

	TP_fast_assign(
		__entry->idx = idx;
	),

	TP_printk("idx: %u", __entry->idx)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */

#endif /* _TRACE_EXYNOS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
