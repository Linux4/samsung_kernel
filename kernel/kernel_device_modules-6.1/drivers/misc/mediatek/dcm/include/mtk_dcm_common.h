/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DCM_COMMON_H__
#define __MTK_DCM_COMMON_H__

#include <linux/ratelimit.h>

#define DCM_OFF (0)
#define DCM_ON (1)
#define DCM_INIT (-1)

#define TAG	"[Power/dcm] "
#define dcm_pr_notice(fmt, args...)			\
	pr_notice(TAG fmt, ##args)
#define dcm_pr_info_limit(fmt, args...)			\
	pr_info_ratelimited(TAG fmt, ##args)
#define dcm_pr_info(fmt, args...)			\
	pr_info(TAG fmt, ##args)
#define dcm_pr_dbg(fmt, args...)			\
	do {						\
		if (dcm_debug)				\
			pr_info(TAG fmt, ##args);	\
	} while (0)

#define DCM_BASE_INFO(_name) \
{ \
	.name = #_name, \
	.base = &_name, \
}

/* #define reg_read(addr)	__raw_readl(IOMEM(addr)) */
#define reg_read(addr) readl((void *)addr)
/*#define reg_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))*/
#define reg_write(addr, val) \
	do { writel(val, (void *)addr); wmb(); } while (0) /* sync write */

#define REG_DUMP(addr) \
	dcm_pr_info("%-60s(0x%08lx): 0x%08x\n", #addr, addr, reg_read(addr))
#define SECURE_REG_DUMP(addr) \
	dcm_pr_info("%-60s(0x%08lx): 0x%08x\n", \
	#addr, addr, mcsi_reg_read(addr##_PHYS & 0xFFFF))

#define ALL_DCM_TYPE 0xFFFFFFFF

/*****************************************************/
typedef int (*DCM_FUNC)(int);
typedef int (*DCM_ISON_FUNC)(void);
typedef void (*DCM_FUNC_VOID_VOID)(void);
typedef void (*DCM_FUNC_VOID_UINTR)(unsigned int *);
typedef void (*DCM_FUNC_VOID_UINTR_INTR)(unsigned int *, int *);
typedef void (*DCM_PRESET_FUNC)(void);
typedef void (*DCM_FUNC_VOID_UINT)(unsigned int);

struct DCM_OPS {
	DCM_FUNC_VOID_VOID dump_regs;
	DCM_FUNC_VOID_UINTR_INTR get_init_state_and_type;
	DCM_FUNC_VOID_UINT set_debug_mode;
};

struct DCM_BASE {
	char *name;
	unsigned long *base;
};

struct DCM {
	bool force_disable;
	int default_state;
	DCM_FUNC func;
	DCM_ISON_FUNC is_on_func;
	DCM_PRESET_FUNC preset_func;
	int typeid;
	char *name;
};

#endif /* #ifndef __MTK_DCM_COMMON_H__ */
