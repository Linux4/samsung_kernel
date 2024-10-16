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
#define NPU_AFM_ENABLE		(0x1)
#define NPU_AFM_DISABLE		(0x0)
#define	NPU_AFM_LIMIT_SET	(0x1)
#define	NPU_AFM_LIMIT_REL	(0x0)
#define	HTU_DNC			(0xCAFE)
#define	HTU_GNPU1		(0xC0DE)

#define S2MPS_AFM_WARN_EN		(0x80)
#define S2MPS_AFM_WARN_DEFAULT_LVL	(0x00)
#define S2MPS_AFM_WARN_LVL_MASK		(0xFF)

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
