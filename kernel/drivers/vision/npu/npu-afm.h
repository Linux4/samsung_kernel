/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_AFM_H_
#define _NPU_AFM_H_

#ifdef CONFIG_NPU_AFM
#define DNC			(0x0)
#define GNPU1			(0x1)
#define NPU0			(0x0)
#define NPU1			(0x1)
#define NPU_AFM_ENABLE		(0x1)
#define NPU_AFM_DISABLE		(0x0)
#define	NPU_AFM_LIMIT_SET	(0x1)
#define	NPU_AFM_LIMIT_REL	(0x0)
#define	NPU_AFM_OCP_WARN	(0x1)
#define	NPU_AFM_OCP_NONE	(0x0)
#define	NPU_AFM_STATE_OPEN	(0x0)
#define	NPU_AFM_STATE_CLOSE	(0x1)
#define	HTU_DNC			(0xCAFE)
#define	HTU_GNPU1		(0xC0DE)
#define NPU_AFM_FREQ_SHIFT	(0x1000)
#define NPU_AFM_INC_FREQ	(0x1533)
#define NPU_AFM_DEC_FREQ	(0x2135)

#define NPU_AFM_TDC_THRESHOLD	(0x100)
#define NPU_AFM_TDT		(0x10)	// 32 cnt
#define NPU_AFM_RESTORE_MSEC	(0x1F4)	// 500 msec

#if IS_ENABLED(CONFIG_SOC_S5E9945)
#define NPU_AFM_FREQ_LEVEL_CNT (0x5)
#else
#define NPU_AFM_FREQ_LEVEL_CNT (0x3)
#endif

#define S2MPS_AFM_WARN_EN		(0x80)
#define S2MPS_AFM_WARN_DEFAULT_LVL	(0x00)
#define S2MPS_AFM_WARN_LVL_MASK		(0xFF)

#if IS_ENABLED(CONFIG_SOC_S5E9945)
static const u32 NPU_AFM_FREQ_LEVEL[] = {
	1300000,
	1200000,
	1066000,
	935000,
	800000,
};
#else // IS_ENABLED(CONFIG_SOC_S5E8845)
static const u32 NPU_AFM_FREQ_LEVEL[] = {
	1066000,
	800000,
	666000,
};
#endif

enum {
	NPU_AFM_MODE_NORMAL = 0,
	NPU_AFM_MODE_TDC,
};

struct npu_afm {
       struct npu_system *npu_afm_system;
       bool afm_local_onoff;
       u32 npu_afm_irp_debug;
       u32 npu_afm_tdc_threshold;
       u32 npu_afm_restore_msec;
       u32 npu_afm_tdt;
       u32 npu_afm_tdt_debug;
};

struct npu_afm_tdc {
       u32 dnc_cnt;
       u32 gnpu1_cnt;
};

void __npu_afm_work(const char *ip, int freq);
void npu_afm_control_global(struct npu_system *system, int location, int enable);
void npu_afm_clear_dnc_interrupt(void);
void npu_afm_onoff_dnc_interrupt(int enable);
void npu_afm_clear_gnpu1_interrupt(void);
void npu_afm_onoff_gnpu1_interrupt(int enable);
void npu_afm_open(struct npu_system *system, int hid);
void npu_afm_close(struct npu_system *system, int hid);
int npu_afm_probe(struct npu_device *device);
int npu_afm_release(struct npu_device *device);
#else	/* CONFIG_NPU_AFM is not defined */
#define npu_afm_open(p, id)	(0)
#define npu_afm_close(p, id)	(0)
#define npu_afm_probe(p)	(0)
#define npu_afm_release(p)	(0)
#endif

#endif	/* _NPU_AFM_H */
