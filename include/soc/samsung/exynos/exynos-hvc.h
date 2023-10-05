/*
 *  Copyright (c) 2019 Samsung Electronics.
 *
 * A header for Hypervisor Call(HVC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_HVC_H__
#define __EXYNOS_HVC_H__

#include <linux/arm-smccc.h>

/* HVC FID */
#define HVC_FID_BASE					(0xC6000000)

#define HVC_FID_GET_HARX_INFO				(HVC_FID_BASE + 0x20)

#define HVC_FID_SET_EL2_CRASH_INFO_FP_BUF		(HVC_FID_BASE + 0x40)
#define HVC_FID_SET_EL2_LV3_TABLE_BUF			(HVC_FID_BASE + 0x41)
#define HVC_FID_DELIVER_LV3_ALLOC_IRQ_NUM		(HVC_FID_BASE + 0x42)
#define HVC_FID_GET_EL2_LOG_BUF				(HVC_FID_BASE + 0x43)

#define HVC_FID_SET_S2MPU				(HVC_FID_BASE + 0x100)
#define HVC_FID_SET_S2MPU_BLOCKLIST			(HVC_FID_BASE + 0x101)
#define HVC_FID_REQUEST_FW_STAGE2_AP_COMPAT		(HVC_FID_BASE + 0x102)
#define HVC_FID_SET_ALL_S2MPU_BLOCKLIST			(HVC_FID_BASE + 0x103)
#define HVC_FID_BAN_KERNEL_RO_REGION			(HVC_FID_BASE + 0x104)
#define HVC_FID_BACKUP_RESTORE_S2MPU			(HVC_FID_BASE + 0x108)
#define HVC_FID_GET_S2MPU_PD_BITMAP			(HVC_FID_BASE + 0x109)

#define HVC_FID_VERIFY_SUBSYSTEM_FW			(HVC_FID_BASE + 0x110)
#define HVC_FID_REQUEST_FW_STAGE2_AP			(HVC_FID_BASE + 0x111)
#define HVC_FID_RELEASE_FW_STAGE2_AP			(HVC_FID_BASE + 0x112)

#define HVC_FID_CHECK_STAGE2_ENABLE			(HVC_FID_BASE + 0x120)
#define HVC_FID_CHECK_S2MPU_ENABLE			(HVC_FID_BASE + 0x121)

#define HVC_CMD_INIT_S2MPUFD				(HVC_FID_BASE + 0x601)
#define HVC_CMD_GET_S2MPUFD_FAULT_INFO			(HVC_FID_BASE + 0x602)
#define HVC_FID_CLEAR_S2MPU_INTERRUPT			(HVC_FID_BASE + 0x603)

#define HVC_DRM_TZMP2_PROT				(0x82002110)
#define HVC_DRM_TZMP2_UNPROT			(0x82002111)

#define HVC_DRM_TZMP2_MFCFW_PROT		(0x82002112)
#define HVC_DRM_TZMP2_MFCFW_UNPROT		(0x82002113)

#define HVC_PROTECTION_SET				(0x82002010)

/* Parameter for smc */
#define HVC_PROTECTION_ENABLE			(1)
#define HVC_PROTECTION_DISABLE			(0)

/* BAAW */
#define HVC_FID_SET_BAAW_WINDOW				(HVC_FID_BASE + 0x700)
#define HVC_FID_GET_BAAW_WINDOW				(HVC_FID_BASE + 0x701)

/* HVC ERROR */
#define HVC_OK						(0x0)
#define HVC_UNK						(-1)

#define HARX_ERROR_BASE					(0xE12E0000)
#define HARX_ERROR(ev)					(HARX_ERROR_BASE + (ev))

/* HVC_FID_GET_HARX_INFO */
#define HARX_INFO_MAJOR_VERSION				(1)
#define HARX_INFO_MINOR_VERSION				(0)
#define HARX_INFO_VERSION				(0xE1200000 |			\
							(HARX_INFO_MAJOR_VERSION << 8) |\
							HARX_INFO_MINOR_VERSION)

#define ERROR_INVALID_INFO_TYPE				HARX_ERROR(0x10)

#define PROT_MFC0			(0)
#define PROT_MSCL0			(1)
#define PROT_MSCL1			(2)
#define PROT_L0				(3)
#define PROT_L1				(4)
#define PROT_L2				(5)
#define PROT_L3				(6)
#define PROT_L4				(7)
#define PROT_L5				(8)
#define PROT_L6				(9)
#define PROT_L7				(10)
#define PROT_L8				(11)
#define PROT_L9				(12)
#define PROT_L10			(13)
#define PROT_L11			(14)
#define PROT_L12			(15)
#define PROT_L13			(16)
#define PROT_L14			(17)
#define PROT_L15			(18)
#define PROT_WB0			(19)
#define PROT_G3D			(20)
#define PROT_JPEG			(21)
#define PROT_G2D			(22)
#define PROT_MFC1			(23)
#define PROT_FGN			(24)

#ifndef	__ASSEMBLY__
#include <linux/types.h>

/* HVC_FID_GET_HARX_INFO */
enum harx_info_type {
	HARX_INFO_TYPE_VERSION = 0,
	HARX_INFO_TYPE_HARX_BASE,
	HARX_INFO_TYPE_HARX_SIZE,
	HARX_INFO_TYPE_RAM_LOG_BASE,
	HARX_INFO_TYPE_RAM_LOG_SIZE
};

static inline unsigned long exynos_hvc(unsigned long hvc_fid,
					unsigned long arg1,
					unsigned long arg2,
					unsigned long arg3,
					unsigned long arg4)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(hvc_fid, arg1, arg2, arg3, arg4, 0, 0, 0, &res);

	return res.a0;
}
#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_HVC_H__ */
