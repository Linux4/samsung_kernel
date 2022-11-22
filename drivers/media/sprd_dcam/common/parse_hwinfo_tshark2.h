/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sprd_mm.h>

#ifdef CONFIG_64BIT
#include <soc/sprd/sci_glb_regs.h>
#endif

#ifndef CONFIG_SC_FPGA
#ifndef CONFIG_OF
extern unsigned long  dcam_regbase;
extern unsigned long  isp_regbase;
extern unsigned long  csi_regbase;
#define DCAM_BASE       SPRD_DEV_P2V(dcam_regbase)
#define ISP_BASE        SPRD_DEV_P2V(isp_regbase)
#define CSI2_BASE       SPRD_DEV_P2V(csi_regbase)
#else
extern unsigned long  dcam_regbase;
extern unsigned long  isp_regbase;
extern unsigned long  csi_regbase;
#define DCAM_BASE       (dcam_regbase)
#define ISP_BASE        (isp_regbase)
#define CSI2_BASE       (csi_regbase)
#endif
#else
extern unsigned long  dcam_regbase;
extern unsigned long  isp_regbase;
extern unsigned long  csi_regbase;
#define DCAM_BASE       (dcam_regbase)
#define ISP_BASE        (isp_regbase)
#define CSI2_BASE       (csi_regbase)
#endif
