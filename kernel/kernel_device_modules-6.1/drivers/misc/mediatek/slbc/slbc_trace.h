/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SLBC_TRACE_H_
#define _SLBC_TRACE_H_

#define SLBC_MAX_EVENT_COUNT 1000
#define SLBC_EVENT_NAME_SIZE 32
#define SLBC_EVENT_DATA_SIZE 128

#define SLBC_DUMP(file, fmt, args...) \
	do {\
		if (file)\
			seq_printf(file, fmt, ##args);\
		else\
			pr_info(fmt, ##args);\
	} while (0)

/* need to modify char *slbc_trace_level_str/mark */
enum slbc_trace_level {
	LVL_NULL = 0,
	LVL_NORM,
	LVL_WARN,
	LVL_ERR,
	LVL_QOS,
	LVL_MAX,
};

/* need to modify char *slbc_trace_type_str */
enum slbc_trace_type {
	TYPE_NULL = 0,
	TYPE_N,
	TYPE_B,
	TYPE_C,
	TYPE_A,
	TYPE_MAX,
};

struct slbc_event {
	char name[SLBC_EVENT_NAME_SIZE];
	int line;
	u32 lvl;
	u32 type;
	int id;
	int ret;
	u64 time;
	char data[SLBC_EVENT_DATA_SIZE];
};

struct slbc_trace {
	/* enable */
	u32 enable;
	u32 log_enable;
	u32 rec_enable;
	/* mask */
	u32 log_mask;
	u32 rec_mask;
	u32 dump_mask;
	/* focus */
	u32 focus;
	u32 focus_type;
	u64 focus_id;
	/* dump */
	u32 dump_num;
	/* record */
	u64 record_count;
	struct slbc_event record[SLBC_MAX_EVENT_COUNT];
};

extern struct slbc_trace *slbc_trace;

#define SLBC_TRACE_REC(lvl, type, id, ret, fmt, args...) \
	slbc_trace_rec_write(__func__, __LINE__, lvl, type, id, ret, fmt, ##args)

#if IS_ENABLED(CONFIG_MTK_SLBC_TRACE)
extern void slbc_trace_rec_write(const char *name, int line, u32 lvl, u32 type, int id,
	int ret, char *fmt, ...);
extern void slbc_trace_dump(void *ptr);
extern int slbc_trace_init(void *ptr);
extern void slbc_trace_exit(void);
#endif /* CONFIG_MTK_SLBC_TRACE */

#endif /* _SLBC_TRACE_H_ */
