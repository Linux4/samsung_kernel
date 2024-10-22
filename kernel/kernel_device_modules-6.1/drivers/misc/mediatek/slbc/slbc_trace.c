// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <slbc.h>
#include <slbc_ops.h>
#include <slbc_ipi.h>
#include <mtk_slbc_sram.h>

#include <slbc_trace.h>

/* mask */
#define SLBC_TRACE_LOG_LVL(x) (!((1UL << ((x) - 1)) & slbc_trace->log_mask))
#define SLBC_TRACE_REC_LVL(x) (!((1UL << ((x) - 1)) & slbc_trace->rec_mask))
#define SLBC_TRACE_DUMP_LVL(x) (!((1UL << ((x) - 1)) & slbc_trace->dump_mask))
/* focus */
#define SLBC_TRACE_FOCUS_TYPE(x) (((1UL << (x)) & slbc_trace->focus_type))
#define SLBC_TRACE_FOCUS_ID(x) (((1UL << (x)) & slbc_trace->focus_id))

struct slbc_trace *slbc_trace;
EXPORT_SYMBOL_GPL(slbc_trace);

DEFINE_MUTEX(slbc_trace_lock);

static u64 slbc_trace_dram_base_va;
static u32 slbc_trace_dram_base_pa;
static u32 slbc_trace_dram_size;
static u32 lvl_count[LVL_MAX + 1];

/* need to modify enum slbc_trace_level */
const char *slbc_trace_level_str[LVL_MAX + 1] = {
	"NULL",
	"NORM",
	"WARN",
	"ERR",
	"QOS",
	"FAIL",
};
EXPORT_SYMBOL_GPL(slbc_trace_level_str);
#define SLBC_TRACE_LEVEL_STR(lvl) \
	((lvl <= LVL_MAX) ? slbc_trace_level_str[(lvl)] : slbc_trace_level_str[LVL_MAX])

/* need to modify enum slbc_trace_level */
const char *slbc_trace_level_mark[LVL_MAX + 1] = {
	"",
	"",
	"->",
	"=>",
	"*",
	"",
};
EXPORT_SYMBOL_GPL(slbc_trace_level_mark);
#define SLBC_TRACE_LEVEL_MARK(lvl) \
	((lvl <= LVL_MAX) ? slbc_trace_level_mark[(lvl)] : slbc_trace_level_mark[LVL_MAX])

/* need to modify enum slbc_trace_type */
const char *slbc_trace_type_str[TYPE_MAX + 1] = {
	"NULL",
	"N",
	"B",
	"C",
	"A",
	"FAIL",
};
EXPORT_SYMBOL_GPL(slbc_trace_type_str);
#define SLBC_TRACE_TYPE_STR(type) \
	((type <= TYPE_MAX) ? slbc_trace_type_str[(type)] : slbc_trace_type_str[TYPE_MAX])

void slbc_trace_rec_write(const char *name, int line, u32 lvl, u32 type, int id,
		int ret, char *fmt, ...)
{
	static char buf[1024];
	int len = 0;
	u64 index;
	va_list va;

	if (!slbc_trace) {
		/*
		va_start(va, fmt);
		len = vsnprintf(buf, sizeof(buf), (fmt) ? fmt : "null", va);
		va_end(va);

		if (len)
			pr_info("#@# %s(%d) ret %d, %s\n", name, line, ret, buf);
		*/
		return;
	}

	if (!slbc_trace->enable)
		return;

	if (slbc_trace->rec_enable && SLBC_TRACE_REC_LVL(lvl)) {
		index = (u64)atomic64_inc_return((atomic64_t *)&slbc_trace->record_count);
		index = (index - 1) % SLBC_MAX_EVENT_COUNT;

		slbc_trace->record[index].time = ktime_get_ns()/1000;
		slbc_trace->record[index].line = line;
		slbc_trace->record[index].lvl = (lvl < LVL_MAX) ? lvl : LVL_MAX;
		slbc_trace->record[index].type = (type < TYPE_MAX) ? type : TYPE_MAX;
		slbc_trace->record[index].id = id;
		slbc_trace->record[index].ret = ret;

		strncpy(slbc_trace->record[index].name, (name) ? (char *)name : "null",
				SLBC_EVENT_NAME_SIZE);
		slbc_trace->record[index].name[SLBC_EVENT_NAME_SIZE - 1] = '\0';

		va_start(va, fmt);
		len = vsnprintf(slbc_trace->record[index].data, SLBC_EVENT_DATA_SIZE,
				(fmt) ? fmt : "null", va);
		va_end(va);

		if (len < 0)
			strncpy(slbc_trace->record[index].data, "print fail",
					SLBC_EVENT_DATA_SIZE);
	}

	if (slbc_trace->log_enable && SLBC_TRACE_LOG_LVL(lvl)) {
		va_start(va, fmt);
		len = vsnprintf(buf, sizeof(buf), (fmt) ? fmt : "null", va);
		va_end(va);

		if (len)
			pr_info("#@# %s(%d) ret %d, %s\n", name, line, ret, buf);
	}
}
EXPORT_SYMBOL_GPL(slbc_trace_rec_write);

u32 slbc_trace_dump_check(u32 idx)
{
	if (idx >= SLBC_MAX_EVENT_COUNT)
		return 0;

	if (!SLBC_TRACE_DUMP_LVL(slbc_trace->record[idx].lvl))
		return 0;

	if (slbc_trace->focus &&
		!SLBC_TRACE_FOCUS_TYPE(slbc_trace->record[idx].type))
		return 0;

	if (slbc_trace->focus && slbc_trace->focus_id &&
		!SLBC_TRACE_FOCUS_ID(slbc_trace->record[idx].id))
		return 0;

	return 1;
}

u32 slbc_trace_dump_event(struct seq_file *m, u32 idx)
{
	if (!slbc_trace_dump_check(idx))
		return 0;

	if (slbc_trace->record[idx].lvl <= LVL_MAX)
		lvl_count[slbc_trace->record[idx].lvl]++;

	SLBC_DUMP(m, "%6s %-6s %-16llu %-32s %-5d %-5s %-7x %-6d %s\n",
			SLBC_TRACE_LEVEL_MARK(slbc_trace->record[idx].lvl),
			SLBC_TRACE_LEVEL_STR(slbc_trace->record[idx].lvl),
			slbc_trace->record[idx].time,
			slbc_trace->record[idx].name,
			slbc_trace->record[idx].line,
			SLBC_TRACE_TYPE_STR(slbc_trace->record[idx].type),
			slbc_trace->record[idx].id,
			slbc_trace->record[idx].ret,
			slbc_trace->record[idx].data);

	return 1;
}

u32 slbc_trace_dump_rec(struct seq_file *m)
{
	u32 total_count = 0;
	u32 temp_count = 0;
	u32 dump_count = 0;
	u64 rec_count = (u64)atomic64_read((atomic64_t *)&slbc_trace->record_count);
	u32 i, base_index, end_index, last_index;

	SLBC_DUMP(m, "\nRECORD DATA (MAX RECORD EVENT:%d):\n", SLBC_MAX_EVENT_COUNT);
	SLBC_DUMP(m, "TOTAL RECORD COUNT: %llu\n", rec_count);
	SLBC_DUMP(m, "%-6s %-6s %-16s %-32s %-5s %-5s %-7s %-6s %s\n",
			"MARK", "LEVEL", "TIME(us)", "EVENT",
			"LINE", "TYPE", "ID(HEX)", "RET", "DATA");

	if (!rec_count) {
		SLBC_DUMP(m, "NO RECORD!\n");
		return 0;
	}

	/* setup index */
	last_index = (rec_count - 1) % SLBC_MAX_EVENT_COUNT;
	base_index = 0;
	end_index = last_index;
	if (rec_count > SLBC_MAX_EVENT_COUNT) {
		base_index = (last_index + 1) % SLBC_MAX_EVENT_COUNT;
		end_index = SLBC_MAX_EVENT_COUNT - 1;
	}

	/* count total dump num */
	for (i = base_index; i <= end_index; i++)
		total_count += slbc_trace_dump_check(i);

	if (base_index)
		for (i = 0; i <= last_index; i++)
			total_count += slbc_trace_dump_check(i);

	/* dump record */
	for (i = base_index; i <= end_index; i++) {
		temp_count += slbc_trace_dump_check(i);
		if (slbc_trace->dump_num + temp_count > total_count)
			dump_count += slbc_trace_dump_event(m, i);
	}

	if (base_index)
		for (i = 0; i <= last_index; i++) {
			temp_count += slbc_trace_dump_check(i);
			if (slbc_trace->dump_num + temp_count > total_count)
				dump_count += slbc_trace_dump_event(m, i);
		}

	if (!dump_count)
		SLBC_DUMP(m, "NO DUMP DATA!\n");

	return dump_count;
}

void slbc_trace_dump(void *ptr)
{
	struct seq_file *m = (struct seq_file *)ptr;
	u32 dump_count = 0;
	u32 i;

	SLBC_DUMP(m, "SLBC TRACE DUMP\n");

	if (!slbc_trace) {
		SLBC_DUMP(m, "SLBC TRACE INIT FAIL!\n");
		SLBC_DUMP(m, "trace_dram_base_va: 0x%llx\n", slbc_trace_dram_base_va);
		SLBC_DUMP(m, "trace_dram_base_pa: 0x%x\n", slbc_trace_dram_base_pa);
		SLBC_DUMP(m, "trace_dram_size: 0x%x\n", slbc_trace_dram_size);
		return;
	}

	if (!slbc_trace->enable) {
		SLBC_DUMP(m, "SLBC TRACE DISABLE!\n");
		return;
	}

	SLBC_DUMP(m, "------------ Enable Setting ------------\n");
	SLBC_DUMP(m, "enable: %u\n", slbc_trace->enable);
	SLBC_DUMP(m, "log_enable: %u\n", slbc_trace->log_enable);
	SLBC_DUMP(m, "rec_enable: %u\n", slbc_trace->rec_enable);
	SLBC_DUMP(m, "------------- Mask Setting -------------\n");
	SLBC_DUMP(m, "log_mask: 0x%x\n", slbc_trace->log_mask);
	SLBC_DUMP(m, "rec_mask: 0x%x\n", slbc_trace->rec_mask);
	SLBC_DUMP(m, "dump_mask: 0x%x\n", slbc_trace->dump_mask);
	SLBC_DUMP(m, "------------- Focus Setting ------------\n");
	SLBC_DUMP(m, "focus: 0x%x\n", slbc_trace->focus);
	SLBC_DUMP(m, "focus_type: 0x%x\n", slbc_trace->focus_type);
	SLBC_DUMP(m, "focus_id: 0x%llx\n", slbc_trace->focus_id);
	SLBC_DUMP(m, "-------------- Dump Setting ------------\n");
	SLBC_DUMP(m, "dump_num: %u\n", slbc_trace->dump_num);
	SLBC_DUMP(m, "-------------- Data Info ---------------\n");
	SLBC_DUMP(m, "trace_dram_base_va: 0x%llx\n", slbc_trace_dram_base_va);
	SLBC_DUMP(m, "trace_dram_base_pa: 0x%x\n", slbc_trace_dram_base_pa);
	SLBC_DUMP(m, "trace_dram_size: 0x%x\n", slbc_trace_dram_size);

	memset(lvl_count, 0, sizeof(lvl_count));

	dump_count = slbc_trace_dump_rec(m);

	if (dump_count) {
		SLBC_DUMP(m, "\n-------------- summary --------------\n");

		for (i = 1; i < LVL_MAX; i++)
			SLBC_DUMP(m, "%s: %u\n", SLBC_TRACE_LEVEL_STR(i), lvl_count[i]);

		SLBC_DUMP(m, "TOTAL: %u\n", dump_count);
		SLBC_DUMP(m, "-------------------------------------\n");
	}
}
EXPORT_SYMBOL_GPL(slbc_trace_dump);

static int slbc_reserve_memory_remap(struct platform_device *pdev)
{
	int ret = 0;

	/* Get reserved memory from dts if have */
	ret = of_property_read_u32(pdev->dev.of_node, "trace-dram-base",
								&slbc_trace_dram_base_pa);
	if (ret < 0) {
		pr_info("slbc of_property_read_u32 trace-dram-base ERR : %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "trace-dram-size",
								&slbc_trace_dram_size);
	if (ret < 0) {
		pr_info("slbc of_property_read_u32 trace-dram-size ERR : %d\n", ret);
		return ret;
	}

	if (!slbc_trace_dram_base_pa || !slbc_trace_dram_size) {
		pr_info("slbc dram size ERR, trace-dram-base: 0x%x, trace-dram-size: 0x%x\n",
				slbc_trace_dram_base_pa, slbc_trace_dram_size);
		return -1;
	}

	/* Get va by memremap */
	slbc_trace_dram_base_va = (u64)
			memremap((phys_addr_t)slbc_trace_dram_base_pa,
					 slbc_trace_dram_size, MEMREMAP_WB);
	if (!slbc_trace_dram_base_va) {
		pr_info("slbc get virtual dram size ERR, slbc_trace_dram_base_va: 0x%llx\n",
				slbc_trace_dram_base_va);
		return -1;
	}
	return 0;
}

int slbc_trace_init(void *ptr)
{
	int ret = 0;

	slbc_trace = NULL;
	ret = slbc_reserve_memory_remap((struct platform_device *)ptr);
	if (ret)
		return ret;

	slbc_trace = (struct slbc_trace *)slbc_trace_dram_base_va;
	/* init record setting */
	atomic64_set((atomic64_t *)&slbc_trace->record_count, 0);
	/* init mask setting */
	slbc_trace->log_mask = 0x1; /* mask LVL_NORM */
	slbc_trace->rec_mask = 0;
	slbc_trace->dump_mask = 0;
	/* init focus setting */
	slbc_trace->focus = 0;
	slbc_trace->focus_type = 0;
	slbc_trace->focus_id = 0;
	/* dump */
	slbc_trace->dump_num = 50;
	/* init enable setting */
	slbc_trace->log_enable = 1;
	slbc_trace->rec_enable = 1;
	slbc_trace->enable = 1;

	SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
			"sizeof(struct slbc_trace): 0x%lx bytes", sizeof(struct slbc_trace));

	return 0;
}
EXPORT_SYMBOL_GPL(slbc_trace_init);

void slbc_trace_exit(void)
{
	memset(slbc_trace, 0, slbc_trace_dram_size);
}
EXPORT_SYMBOL_GPL(slbc_trace_exit);

MODULE_DESCRIPTION("SLBC Trace Driver v0.1");
MODULE_LICENSE("GPL");

