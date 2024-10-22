/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_INTERCONNECT_H
#define __MTK_INTERCONNECT_H

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/kernel.h>

/* macros for converting to icc units */
#define Bps_to_icc(x)	((x) / 1000)
#define kBps_to_icc(x)	(x)
#define MBps_to_icc(x)	((x) * 1000)
#define GBps_to_icc(x)	((x) * 1000 * 1000)
#define bps_to_icc(x)	(1)
#define kbps_to_icc(x)	((x) / 8 + ((x) % 8 ? 1 : 0))
#define Mbps_to_icc(x)	((x) * 1000 / 8)
#define Gbps_to_icc(x)	((x) * 1000 * 1000 / 8)

struct icc_path;
struct device;

/* For systrace */
enum MMQOS_ICC_PROFILE_LEVEL {
	MMQOS_ICC_PROFILE_SYSTRACE = 0,
	MMQOS_ICC_PROFILE_MAX /* Always keep at the end */
};
bool mmqos_icc_systrace_enabled(void);
int icc_tracing_mark_write(char *fmt, ...);

#define TRACE_MSG_LEN	1024

#define MMQOS_ICC_TRACE_FORCE_BEGIN_TID(tid, fmt, args...) \
	icc_tracing_mark_write("B|%d|" fmt "\n", tid, ##args)

#define MMQOS_ICC_TRACE_FORCE_BEGIN(fmt, args...) \
	MMQOS_ICC_TRACE_FORCE_BEGIN_TID(current->tgid, fmt, ##args)

#define MMQOS_ICC_TRACE_FORCE_END() \
	icc_tracing_mark_write("E\n")

#define MMQOS_ICC_SYSTRACE_BEGIN(fmt, args...) do { \
	if (mmqos_icc_systrace_enabled()) { \
		MMQOS_ICC_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define MMQOS_ICC_SYSTRACE_END() do { \
	if (mmqos_icc_systrace_enabled()) { \
		MMQOS_ICC_TRACE_FORCE_END(); \
	} \
} while (0)

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_EXTENSION)

struct icc_path *mtk_icc_get(struct device *dev, const int src_id,
			 const int dst_id);
struct icc_path *of_mtk_icc_get(struct device *dev, const char *name);
void mtk_icc_put(struct icc_path *path);
int mtk_icc_set_bw(struct icc_path *path, u32 avg_bw, u32 peak_bw);
int mtk_icc_set_bw_not_update(struct icc_path *path, u32 avg_bw, u32 peak_bw);
void mtk_icc_set_tag(struct icc_path *path, u32 tag);

#else

static inline struct icc_path *mtk_icc_get(struct device *dev, const int src_id,
				       const int dst_id)
{
	return NULL;
}

static inline struct icc_path *of_mtk_icc_get(struct device *dev,
					  const char *name)
{
	return NULL;
}

static inline void mtk_icc_put(struct icc_path *path)
{
}

static inline int mtk_icc_set_bw(struct icc_path *path, u32 avg_bw, u32 peak_bw)
{
	return 0;
}

static inline int mtk_icc_set_bw_not_update(struct icc_path *path, u32 avg_bw, u32 peak_bw)
{
	return 0;
}

static inline void mtk_icc_set_tag(struct icc_path *path, u32 tag)
{
}

#endif /* CONFIG_INTERCONNECT_MTK_EXTENSION */

#endif /* __MTK_INTERCONNECT_H */
