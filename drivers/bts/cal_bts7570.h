/* arch/arm/mach-exynos/cal_bts.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BTSCAL_H__
#define __BTSCAL_H__

#include <linux/io.h>
#include <linux/debugfs.h>
/* for BTS Through SYSTEM Register */
typedef enum {
	BTS_SYSREG_CPU = 1,
	BTS_SYSREG_APM,
	BTS_SYSREG_CPGNSS,
} QOS_SYSREG_IP;

typedef void __iomem *addr_u32;

void trex_setqos(addr_u32 base, unsigned int priority);
void trex_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo);
void trex_disable(addr_u32 base);
void trex_showqos(addr_u32 base, struct seq_file *buf);
void sysreg_setqos(QOS_SYSREG_IP qos_id, addr_u32 base,
			unsigned int priority);
void sysreg_disable(QOS_SYSREG_IP qos_id, addr_u32 base);
void sysreg_showqos(QOS_SYSREG_IP qos_id, addr_u32 base, struct seq_file *buf);

void bts_setqos(addr_u32 base, unsigned int priority);
void bts_setqos_bw(addr_u32 base, unsigned int priority,
			unsigned int window, unsigned int token);
void bts_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo);
void bts_disable(addr_u32 base);
void bts_showqos(addr_u32 base, struct seq_file *buf);

#define EXYNOS7570_PA_BTS_CPU			0x104D0000

#define EXYNOS7570_PA_TREX_MFCMSCL		0x12420000
#define EXYNOS7570_PA_TREX_G3D			0x12430000
#define EXYNOS7570_PA_TREX_FSYS			0x12440000
#define EXYNOS7570_PA_TREX_ISP			0x12450000
#define EXYNOS7570_PA_TREX_DISPAUD		0x12460000
#define EXYNOS7570_PA_TREX_CELLULAR		0x12480000
#define EXYNOS7570_PA_TREX_WLBT			0x12490000

#define EXYNOS7570_PA_TREX_SLAVE_CP		0x124B0000
#define EXYNOS7570_PA_TREX_SLAVE_MM		0x124C0000

#define EXYNOS7570_PA_SYSREG_CP			0x11C80000
#define EXYNOS7570_PA_SYSREG_APM		0x11C50000
#define EXYNOS7570_PA_SYSREG_DISPAUD		0x148F0000
#define EXYNOS7570_PA_SYSREG_FSYS		0x13720000
#define EXYNOS7570_PA_SYSREG_ISP		0x144F0000
#define EXYNOS7570_PA_SYSREG_CPU		0x10450000
#define EXYNOS7570_PA_SYSREG_MFCMSCL		0x12CA0000

#define EXYNOS7570_PA_DREX			0x10400000

#define QOS_TIMEOUT_0x0				0x300
#define QOS_TIMEOUT_0x1				0x304
#define QOS_TIMEOUT_0x2				0x308
#define QOS_TIMEOUT_0x3				0x30c
#define QOS_TIMEOUT_0x4				0x310
#define QOS_TIMEOUT_0x5				0x314
#define QOS_TIMEOUT_0x6				0x318
#define QOS_TIMEOUT_0x7				0x31c
#define QOS_TIMEOUT_0x8				0x320
#define QOS_TIMEOUT_0x9				0x324
#define QOS_TIMEOUT_0xA				0x328
#define QOS_TIMEOUT_0xB				0x32C
#define QOS_TIMEOUT_0xC				0x330
#define QOS_TIMEOUT_0xD				0x334
#define QOS_TIMEOUT_0xE				0x338
#define QOS_TIMEOUT_0xF				0x33c

#endif
