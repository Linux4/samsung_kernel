/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/seq_file.h>

#define BSP_INFO_DISPLAY_FN(fn_name, name, callback)				\
static int secdbg_disp_bspinfo_##fn_name(struct notifier_block *bl, \
				     unsigned long val, void *m) \
{													\
	struct seq_file *dst = (struct seq_file *)m;	\
													\
	seq_printf(dst, "\n**** [%36s] ********\n\n", name);				\
	return callback(dst);							\
}													\
													\
static struct notifier_block fn_name##_display_notifier = {	\
	.notifier_call = secdbg_disp_bspinfo_##fn_name,	\
}

#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
extern int secdbg_bspinfo_register_info_displayer(struct notifier_block *nb);
#else
static inline int secdbg_bspinfo_register_info_displayer(struct notifier_block *nb) { }
#endif
