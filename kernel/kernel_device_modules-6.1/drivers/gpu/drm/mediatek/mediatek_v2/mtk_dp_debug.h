/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DP_DEBUG_H__
#define __MTK_DP_DEBUG_H__
#include <linux/types.h>

void mtk_dp_debug_enable(bool enable);
bool mtk_dp_debug_get(void);
void mtk_dp_debug(const char *opt);
#ifdef MTK_DPINFO
int mtk_dp_debugfs_init(void);
void mtk_dp_debugfs_deinit(void);
#endif


#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
extern void mtk_dp_logger_print(const char *fmt, ...);

#define DPTXFUNC(fmt, arg...)		\
	pr_info("[DPTX][%s line:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg)

#define DPTXDBG(fmt, arg...)              \
	do {                                 \
		if (mtk_dp_debug_get()) {                 \
			pr_info("[DPTX]"pr_fmt(fmt), ##arg);     \
			mtk_dp_logger_print(fmt, ##arg);	\
		}	\
	} while (0)

#define DPTXMSG(fmt, arg...)                                  \
	do {                                 \
		pr_info("[DPTX]"pr_fmt(fmt), ##arg);	\
		mtk_dp_logger_print(fmt, ##arg);	\
	} while (0)

#define DPTXERR(fmt, arg...)                                   \
	do {                                 \
		pr_err("[DPTX][ERROR]"pr_fmt(fmt), ##arg);	\
		mtk_dp_logger_print(fmt, ##arg);	\
	} while (0)

#else //CONFIG_SEC_DISPLAYPORT_LOGGER
#define DPTXFUNC(fmt, arg...)		\
	pr_info("[DPTX][%s line:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg)

#define DPTXDBG(fmt, arg...)              \
	do {                                 \
		if (mtk_dp_debug_get())                  \
			pr_info("[DPTX]"pr_fmt(fmt), ##arg);     \
	} while (0)

#define DPTXMSG(fmt, arg...)                                  \
		pr_info("[DPTX]"pr_fmt(fmt), ##arg)

#define DPTXERR(fmt, arg...)                                   \
		pr_err("[DPTX][ERROR]"pr_fmt(fmt), ##arg)

#endif //CONFIG_SEC_DISPLAYPORT_LOGGER

#endif

