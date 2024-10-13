/* linux/arch/arm/mach-exynos/include/mach/asv-exynos5430-lite.h
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS5430 - Adoptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_EXYNOS5430_FHD_ASV_H
#define __ASM_ARCH_EXYNOS5430_FHD_ASV_H __FILE__

#define ARM_DVFS_LEVEL_NR		(24)
#define ARM_ASV_GRP_NR			(16)
#define ARM_MAX_VOLT			(1200000)
#define KFC_DVFS_LEVEL_NR		(19)
#define KFC_ASV_GRP_NR			(16)
#define KFC_MAX_VOLT			(1162500)
#define G3D_DVFS_LEVEL_NR		(7)
#define G3D_ASV_GRP_NR			(16)
#define G3D_MAX_VOLT			(1037500)
#define MIF_DVFS_LEVEL_NR		(11)
#define MIF_ASV_GRP_NR			(16)
#define MIF_MAX_VOLT			(1050000)
#define INT_DVFS_LEVEL_NR		(8)
#define INT_ASV_GRP_NR			(16)
#define INT_MAX_VOLT			(1062500)
#define ISP_DVFS_LEVEL_NR		(11)
#define ISP_ASV_GRP_NR			(16)
#define ISP_MAX_VOLT			(1000000)

/* ASV V00 */
#define arm_asv_abb_info	arm_asv_abb_info_v101
#define kfc_asv_abb_info	kfc_asv_abb_info_v101
#define g3d_asv_abb_info	g3d_asv_abb_info_v101
#define mif_asv_abb_info	mif_asv_abb_info_v101
#define int_asv_abb_info	int_asv_abb_info_v101
#define arm_asv_volt_info	arm_asv_volt_info_v101
#define kfc_asv_volt_info	kfc_asv_volt_info_v101
#define g3d_asv_volt_info	g3d_asv_volt_info_v101
#define mif_asv_volt_info	mif_asv_volt_info_v101
#define int_asv_volt_info	int_asv_volt_info_v101
#define isp_asv_volt_info	isp_asv_volt_info_v101

/* ASV V01 */
#define arm_asv_abb_info_v01	arm_asv_abb_info_v101
#define kfc_asv_abb_info_v01	kfc_asv_abb_info_v101
#define g3d_asv_abb_info_v01	g3d_asv_abb_info_v101
#define mif_asv_abb_info_v01	mif_asv_abb_info_v101
#define int_asv_abb_info_v01	int_asv_abb_info_v101
#define arm_asv_volt_info_v01	arm_asv_volt_info_v101
#define kfc_asv_volt_info_v01	kfc_asv_volt_info_v101
#define g3d_asv_volt_info_v01	g3d_asv_volt_info_v101
#define mif_asv_volt_info_v01	mif_asv_volt_info_v101
#define int_asv_volt_info_v01	int_asv_volt_info_v101
#define isp_asv_volt_info_v01	isp_asv_volt_info_v101

/* ASV V10 */
#define arm_asv_abb_info_v10	arm_asv_abb_info_v101
#define kfc_asv_abb_info_v10	kfc_asv_abb_info_v101
#define g3d_asv_abb_info_v10	g3d_asv_abb_info_v101
#define mif_asv_abb_info_v10	mif_asv_abb_info_v101
#define int_asv_abb_info_v10	int_asv_abb_info_v101
#define arm_asv_volt_info_v10	arm_asv_volt_info_v101
#define kfc_asv_volt_info_v10	kfc_asv_volt_info_v101
#define g3d_asv_volt_info_v10	g3d_asv_volt_info_v101
#define mif_asv_volt_info_v10	mif_asv_volt_info_v101
#define int_asv_volt_info_v10	int_asv_volt_info_v101
#define isp_asv_volt_info_v10	isp_asv_volt_info_v101

/* ASV V11 */
#define arm_asv_abb_info_v11	arm_asv_abb_info_v101
#define kfc_asv_abb_info_v11	kfc_asv_abb_info_v101
#define g3d_asv_abb_info_v11	g3d_asv_abb_info_v101
#define mif_asv_abb_info_v11	mif_asv_abb_info_v101
#define int_asv_abb_info_v11	int_asv_abb_info_v101
#define arm_asv_volt_info_v11	arm_asv_volt_info_v101
#define kfc_asv_volt_info_v11	kfc_asv_volt_info_v101
#define g3d_asv_volt_info_v11	g3d_asv_volt_info_v101
#define mif_asv_volt_info_v11	mif_asv_volt_info_v101
#define int_asv_volt_info_v11	int_asv_volt_info_v101
#define isp_asv_volt_info_v11	isp_asv_volt_info_v101

/* ASV V100 */
#define arm_asv_abb_info_v100	arm_asv_abb_info_v101
#define kfc_asv_abb_info_v100	kfc_asv_abb_info_v101
#define g3d_asv_abb_info_v100	g3d_asv_abb_info_v101
#define mif_asv_abb_info_v100	mif_asv_abb_info_v101
#define int_asv_abb_info_v100	int_asv_abb_info_v101
#define arm_asv_volt_info_v100	arm_asv_volt_info_v101
#define kfc_asv_volt_info_v100	kfc_asv_volt_info_v101
#define g3d_asv_volt_info_v100	g3d_asv_volt_info_v101
#define mif_asv_volt_info_v100	mif_asv_volt_info_v101
#define int_asv_volt_info_v100	int_asv_volt_info_v101
#define isp_asv_volt_info_v100	isp_asv_volt_info_v101

/* ASV_V101 */
static unsigned int arm_asv_abb_info_v101[ARM_DVFS_LEVEL_NR][ARM_ASV_GRP_NR + 1] = {
	{ 2500000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 2400000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 2300000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 2200000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 2100000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 2000000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1900000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1800000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1700000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1600000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1500000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1400000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1300000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1200000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1100000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{ 1000000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  900000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  800000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  700000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  600000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  500000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  400000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  300000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  200000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
};

static unsigned int kfc_asv_abb_info_v101[KFC_DVFS_LEVEL_NR][KFC_ASV_GRP_NR + 1] = {
	{ 2000000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1900000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1800000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1700000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1600000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1500000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1400000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1300000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1200000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1100000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{ 1000000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  900000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  800000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  700000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  600000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  500000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  400000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  300000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  200000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
};

static unsigned int g3d_asv_abb_info_v101[G3D_DVFS_LEVEL_NR][G3D_ASV_GRP_NR + 1] = {
	{  600000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  550000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  500000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  420000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  350000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  266000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
	{  160000,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8,        0x8},
};

static unsigned int mif_asv_abb_info_v101[MIF_DVFS_LEVEL_NR][MIF_ASV_GRP_NR + 1] = {
	{ 1066000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  933000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  825000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  633000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  543000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  413000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  272000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  211000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  158000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  136000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
	{  109000,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB,        0xB},
};

static unsigned int int_asv_abb_info_v101[INT_DVFS_LEVEL_NR][INT_ASV_GRP_NR + 1] = {
	{  543000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  400000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  317000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  267000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  200000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  160000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  133000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
	{  100000, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS, ABB_BYPASS},
};

/* ASV_V101 */
static unsigned int arm_asv_volt_info_v101[ARM_DVFS_LEVEL_NR][ARM_ASV_GRP_NR + 1] = {
	{ 2500000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000},
	{ 2400000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1300000},
	{ 2300000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1300000, 1287500, 1275000, 1262500, 1250000},
	{ 2200000, 1350000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1300000, 1287500, 1275000, 1262500, 1250000, 1237500, 1225000, 1212500, 1200000},
	{ 2100000, 1350000, 1325000, 1312500, 1300000, 1287500, 1275000, 1262500, 1250000, 1237500, 1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000},
	{ 2000000, 1300000, 1275000, 1262500, 1250000, 1237500, 1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000},
	{ 1900000, 1250000, 1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000, 1087500, 1087500, 1075000, 1062500},
	{ 1800000, 1200000, 1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1037500, 1025000, 1012500},
	{ 1700000, 1162500, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000, 1000000,  987500,  975000},
	{ 1600000, 1125000, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000, 1000000,  987500,  975000,  975000,  962500,  950000},
	{ 1500000, 1100000, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  975000,  962500,  950000,  950000,  937500,  925000},
	{ 1400000, 1075000, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  950000,  937500,  925000,  925000,  912500,  900000},
	{ 1300000, 1050000, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  925000,  912500,  900000,  900000,  887500,  875000},
	{ 1200000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  900000,  887500,  875000,  875000,  875000,  875000},
	{ 1100000, 1000000,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{ 1000000,  975000,  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  900000,  950000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  800000,  937500,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  700000,  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  600000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  500000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  400000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  300000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
	{  200000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  887500,  887500,  875000,  875000,  875000,  875000},
};

static unsigned int kfc_asv_volt_info_v101[KFC_DVFS_LEVEL_NR][KFC_ASV_GRP_NR + 1] = {
	{ 2000000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1300000},
	{ 1900000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1312500, 1300000, 1287500, 1275000, 1262500, 1250000},
	{ 1800000, 1350000, 1350000, 1350000, 1337500, 1325000, 1312500, 1300000, 1287500, 1275000, 1262500, 1262500, 1250000, 1237500, 1225000, 1212500, 1200000},
	{ 1700000, 1350000, 1337500, 1312500, 1287500, 1275000, 1262500, 1250000, 1237500, 1225000, 1212500, 1212500, 1200000, 1187500, 1175000, 1162500, 1150000},
	{ 1600000, 1312500, 1287500, 1262500, 1237500, 1225000, 1212500, 1200000, 1187500, 1175000, 1162500, 1162500, 1150000, 1137500, 1125000, 1112500, 1100000},
	{ 1500000, 1262500, 1237500, 1212500, 1187500, 1175000, 1162500, 1150000, 1137500, 1125000, 1112500, 1112500, 1100000, 1087500, 1075000, 1062500, 1050000},
	{ 1400000, 1212500, 1187500, 1162500, 1137500, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000},
	{ 1300000, 1162500, 1137500, 1112500, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1012500, 1000000,  987500,  975000,  962500,  950000},
	{ 1200000, 1125000, 1100000, 1075000, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  975000,  962500,  950000,  937500,  925000,  912500},
	{ 1100000, 1100000, 1075000, 1050000, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000},
	{ 1000000, 1075000, 1050000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000},
	{  900000, 1050000, 1025000, 1000000,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  800000, 1025000, 1000000,  975000,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000},
	{  700000, 1000000,  975000,  950000,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000},
	{  600000,  975000,  950000,  925000,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000},
	{  500000,  950000,  925000,  900000,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  725000},
	{  400000,  925000,  900000,  875000,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  725000,  725000,  725000},
	{  300000,  900000,  875000,  850000,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  725000,  725000,  725000,  725000,  725000},
	{  200000,  875000,  850000,  825000,  800000,  787500,  775000,  762500,  750000,  737500,  725000,  725000,  725000,  725000,  725000,  725000,  725000},
};

static unsigned int g3d_asv_volt_info_v101[G3D_DVFS_LEVEL_NR][G3D_ASV_GRP_NR + 1] = {
	{  600000, 1050000, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  950000,  937500,  937500,  925000,  925000,  912500,  900000,  887500},
	{  550000, 1012500,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  912500,  900000,  900000,  887500,  887500,  875000,  862500,  850000},
	{  500000,  987500,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  887500,  875000,  875000,  862500,  850000,  837500,  825000,  812500},
	{  420000,  950000,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  850000,  837500,  837500,  825000,  812500,  800000,  787500,  775000},
	{  350000,  925000,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  825000,  812500,  812500,  800000,  787500,  775000,  762500,  750000},
	{  266000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  800000,  787500,  787500,  775000,  762500,  750000,  750000,  750000},
	{  160000,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  787500,  775000,  775000,  762500,  750000,  750000,  750000,  750000},
};

static unsigned int mif_asv_volt_info_v101[MIF_DVFS_LEVEL_NR][MIF_ASV_GRP_NR + 1] = {
	{ 1066000, 1150000, 1125000, 1112500, 1100000, 1087500, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000},
	{  933000, 1100000, 1075000, 1062500, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000},
	{  825000, 1050000, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000},
	{  633000,  962500,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  775000},
	{  543000,  937500,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  750000},
	{  413000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  787500,  775000,  775000,  762500,  762500,  750000,  737500},
	{  272000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  737500,  725000,  725000,  725000,  725000},
	{  211000,  850000,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  737500,  725000,  725000,  725000,  725000},
	{  158000,  850000,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  737500,  725000,  725000,  725000,  725000},
	{  136000,  850000,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  737500,  725000,  725000,  725000,  725000},
	{  109000,  850000,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  737500,  725000,  725000,  725000,  725000},
};

static unsigned int int_asv_volt_info_v101[INT_DVFS_LEVEL_NR][INT_ASV_GRP_NR + 1] = {
	{  543000, 1075000, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000},
	{  400000,  987500,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500},
	{  317000,  937500,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  762500,  762500},
	{  267000,  900000,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  775000,  762500,  762500,  762500,  762500,  762500},
	{  200000,  887500,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  775000,  775000,  762500,  762500,  750000,  750000,  750000},
	{  160000,  875000,  850000,  837500,  825000,  812500,  800000,  787500,  787500,  775000,  775000,  775000,  750000,  750000,  737500,  725000,  725000},
	{  133000,  875000,  850000,  837500,  825000,  812500,  800000,  787500,  787500,  775000,  775000,  775000,  750000,  750000,  737500,  725000,  725000},
	{  100000,  875000,  850000,  837500,  825000,  812500,  800000,  787500,  787500,  775000,  775000,  775000,  750000,  750000,  737500,  725000,  725000},
};

static unsigned int isp_asv_volt_info_v101[ISP_DVFS_LEVEL_NR][ISP_ASV_GRP_NR + 1] = {
	{  777000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  666000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  555000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  466000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  455000, 1025000, 1000000,  987500,  975000,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  862500,  850000,  837500,  825000},
	{  444000,  987500,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  875000,  862500,  850000,  837500,  825000,  812500,  800000},
	{  333000,  987500,  962500,  950000,  937500,  925000,  912500,  900000,  887500,  875000,  875000,  862500,  850000,  837500,  825000,  812500,  800000},
	{  222000,  912500,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  725000,  712500},
	{  111000,  912500,  887500,  875000,  862500,  850000,  837500,  825000,  812500,  800000,  787500,  775000,  762500,  750000,  737500,  725000,  712500},
};

#endif /* EXYNOS5430_FHD_ASV_H */
