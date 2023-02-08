/* linux/arch/arm/mach-exynos/asv-exynos5260.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS5260 - ASV(Adoptive Support Voltage) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos5260.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <linux/regulator/consumer.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#define CHIP_ID2_REG			(S5P_VA_CHIPID + 0x04)
#define EXYNOS5260_IDS_OFFSET		(16)
#define EXYNOS5260_IDS_MASK		(0xFF)

#define FUSED_SPEED_GRP_REG		(S5P_VA_CHIPID2)
#define EXYNOS5260_FUSED_GRP_OFFSET	(24)
#define EXYNOS5260_FUSED_GRP_MASK	(0x1)

#define CHIP_ID4_REG			(S5P_VA_CHIPID2 + 0x04)
#define EXYNOS5260_TMCB_OFFSET		(0)
#define EXYNOS5260_TMCB_MASK		(0x3F)

#define CPU_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x08)
#define EXYNOS5260_EGL_GRP1_OFFSET	(0)
#define EXYNOS5260_EGL_GRP2_OFFSET	(4)
#define EXYNOS5260_EGL_GRP3_OFFSET	(8)
#define EXYNOS5260_EGL_VOL_LOCK_OFFSET	(12)
#define EXYNOS5260_EGL_GRP_MASK		(0xF)
#define EXYNOS5260_EGL_VOL_LOCK_MASK	(0xF)
#define EXYNOS5260_KFC_GRP1_OFFSET	(16)
#define EXYNOS5260_KFC_GRP2_OFFSET	(20)
#define EXYNOS5260_KFC_GRP3_OFFSET	(24)
#define EXYNOS5260_KFC_VOL_LOCK_OFFSET	(28)
#define EXYNOS5260_KFC_GRP_MASK		(0xF)
#define EXYNOS5260_KFC_VOL_LOCK_MASK	(0xF)

#define G3D_MIF_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x0C)
#define EXYNOS5260_G3D_GRP1_OFFSET	(0)
#define EXYNOS5260_G3D_GRP2_OFFSET	(4)
#define EXYNOS5260_G3D_VOL_LOCK_OFFSET	(12)
#define EXYNOS5260_G3D_GRP_MASK		(0xF)
#define EXYNOS5260_G3D_VOL_LOCK_MASK	(0xF)
#define EXYNOS5260_MIF_GRP1_OFFSET	(16)
#define EXYNOS5260_MIF_GRP2_OFFSET	(20)
#define EXYNOS5260_MIF_VOL_LOCK_OFFSET	(28)
#define EXYNOS5260_MIF_GRP_MASK		(0xF)
#define EXYNOS5260_MIF_VOL_LOCK_MASK	(0xF)

#define INT_DISP_SPEED_GRP_REG		(S5P_VA_CHIPID2 + 0x10)
#define EXYNOS5260_INT_GRP1_OFFSET	(0)
#define EXYNOS5260_INT_GRP2_OFFSET	(4)
#define EXYNOS5260_INT_VOL_LOCK_OFFSET	(12)
#define EXYNOS5260_INT_GRP_MASK		(0xF)
#define EXYNOS5260_INT_VOL_LOCK_MASK	(0xF)
#define EXYNOS5260_DISP_GRP1_OFFSET	(16)
#define EXYNOS5260_DISP_VOL_LOCK_OFFSET	(28)
#define EXYNOS5260_DISP_GRP_MASK	(0xF)
#define EXYNOS5260_DISP_VOL_LOCK_MASK	(0xF)

#define EXYNOS5260_GRP_MAX_NR		(3)

#define EXYNOS5260_GRP_L1		(0)
#define EXYNOS5260_GRP_L2		(1)
#define EXYNOS5260_GRP_L3		(2)

#define EXYNOS5260_VOL_NO_LOCK		(0x0)
#define EXYNOS5260_VOL_875_LOCK		(0x1)
#define EXYNOS5260_VOL_900_LOCK		(0x2)
#define EXYNOS5260_VOL_925_LOCK		(0x3)
#define EXYNOS5260_VOL_950_LOCK		(0x4)

#define EGL_GRP_L1_FREQ			1300000
#define EGL_GRP_L2_FREQ			800000
#define KFC_GRP_L1_FREQ			1100000
#define KFC_GRP_L2_FREQ			700000
#define G3D_GRP_L1_FREQ			450000
#define MIF_GRP_L1_FREQ			413000
#define INT_GRP_L1_FREQ			333000

#define VOL_875000			875000
#define VOL_900000			900000
#define VOL_925000			925000
#define VOL_950000			950000

struct asv_reference {
	unsigned int ids_value;
	unsigned int hpm_value;
	bool is_speedgroup;
};
static struct asv_reference asv_ref_info = {0, 0, false};

struct asv_fused_info {
	unsigned int speed_grp[EXYNOS5260_GRP_MAX_NR];
	unsigned int voltage_lock;
};

static struct asv_fused_info egl_fused_info;
static struct asv_fused_info kfc_fused_info;
static struct asv_fused_info g3d_fused_info;
static struct asv_fused_info mif_fused_info;
static struct asv_fused_info int_fused_info;
static struct asv_fused_info disp_fused_info;

#ifdef CONFIG_ASV_MARGIN_TEST
static int set_arm_volt = 0;
static int set_kfc_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;
static int set_disp_volt = 0;

static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

static int __init get_kfc_volt(char *str)
{
	get_option(&str, &set_kfc_volt);
	return 0;
}
early_param("kfc", get_kfc_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_disp_volt(char *str)
{
	get_option(&str, &set_disp_volt);
	return 0;
}
early_param("disp", get_disp_volt);
#endif

static unsigned int exynos5260_lock_voltage(unsigned int volt_lock)
{
	unsigned int lock_voltage;

	if (volt_lock == EXYNOS5260_VOL_875_LOCK)
		lock_voltage = VOL_875000;
	else if (volt_lock == EXYNOS5260_VOL_900_LOCK)
		lock_voltage = VOL_900000;
	else if (volt_lock == EXYNOS5260_VOL_925_LOCK)
		lock_voltage = VOL_925000;
	else if (volt_lock == EXYNOS5260_VOL_950_LOCK)
		lock_voltage = VOL_950000;
	else
		lock_voltage = 0;

	return lock_voltage;
}

#ifdef CONFIG_PM
static void exynos5260_set_abb_bypass(struct asv_info *asv_inform)
{
	void __iomem *target_reg;
	unsigned int target_value;

	target_value = ABB_BYPASS;

	switch (asv_inform->asv_type) {
	case ID_ARM:
		target_reg = EXYNOS5260_BIAS_CON0;
		break;
	case ID_KFC:
		target_reg = EXYNOS5260_BIAS_CON1;
		break;
	case ID_INT:
		target_reg = EXYNOS5260_BIAS_CON4;
		break;
	case ID_MIF:
		target_reg = EXYNOS5260_BIAS_CON2;
		break;
	case ID_G3D:
		target_reg = EXYNOS5260_BIAS_CON3;
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}
#endif

static void exynos5260_set_abb(struct asv_info *asv_inform)
{
	void __iomem *target_reg;
	unsigned int target_value;

	target_value = asv_inform->abb_info->target_abb;

	switch (asv_inform->asv_type) {
	case ID_ARM:
		target_reg = EXYNOS5260_BIAS_CON0;
		break;
	case ID_KFC:
		target_reg = EXYNOS5260_BIAS_CON1;
		break;
	case ID_INT:
		target_reg = EXYNOS5260_BIAS_CON4;
		break;
	case ID_MIF:
		target_reg = EXYNOS5260_BIAS_CON2;
		break;
	case ID_G3D:
		target_reg = EXYNOS5260_BIAS_CON3;
		break;
	default:
		return;
	}

	set_abb(target_reg, target_value);
}

static struct abb_common exynos5260_abb_common = {
	.set_target_abb = exynos5260_set_abb,
};

static unsigned int exynos5260_get_asv_group_arm(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ARM);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return egl_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L1_FREQ)
				target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5260_GRP_L1];
			else if (asv_inform->asv_volt[i].asv_freq >= EGL_GRP_L2_FREQ)
				target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5260_GRP_L2];
			else
				target_asv_grp_nr = egl_fused_info.speed_grp[EXYNOS5260_GRP_L3];

			lock_voltage = exynos5260_lock_voltage(egl_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= arm_asv_volt_info[i][target_asv_grp_nr + 1])
				arm_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			arm_asv_volt_info[i][target_asv_grp_nr + 1] + set_arm_volt;
#else
		asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		asv_inform->asv_abb[i].asv_freq = arm_asv_volt_info[i][0];
		asv_inform->asv_abb[i].asv_value = arm_asv_abb_info[i][target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_arm = {
	.get_asv_group	= exynos5260_get_asv_group_arm,
	.set_asv_info	= exynos5260_set_asv_info_arm,
};

static unsigned int exynos5260_get_asv_group_kfc(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_KFC);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return kfc_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_kfc(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = kfc_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L1_FREQ)
				target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5260_GRP_L1];
			else if (asv_inform->asv_volt[i].asv_freq >= KFC_GRP_L2_FREQ)
				target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5260_GRP_L2];
			else
				target_asv_grp_nr = kfc_fused_info.speed_grp[EXYNOS5260_GRP_L3];

			lock_voltage = exynos5260_lock_voltage(kfc_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= kfc_asv_volt_info[i][target_asv_grp_nr + 1])
				kfc_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			kfc_asv_volt_info[i][target_asv_grp_nr + 1] + set_kfc_volt;
#else
		asv_inform->asv_volt[i].asv_value = kfc_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		asv_inform->asv_abb[i].asv_freq = kfc_asv_volt_info[i][0];
		asv_inform->asv_abb[i].asv_value = kfc_asv_abb_info[i][target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_kfc = {
	.get_asv_group	= exynos5260_get_asv_group_kfc,
	.set_asv_info	= exynos5260_set_asv_info_kfc,
};

static unsigned int exynos5260_get_asv_group_int(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_INT);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return int_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			if (asv_inform->asv_volt[i].asv_freq >= INT_GRP_L1_FREQ)
				target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5260_GRP_L1];
			else
				target_asv_grp_nr = int_fused_info.speed_grp[EXYNOS5260_GRP_L2];

			lock_voltage = exynos5260_lock_voltage(int_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= int_asv_volt_info[i][target_asv_grp_nr + 1])
				int_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			int_asv_volt_info[i][target_asv_grp_nr + 1] + set_int_volt;
#else
		asv_inform->asv_volt[i].asv_value = int_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		asv_inform->asv_abb[i].asv_freq = int_asv_volt_info[i][0];
		asv_inform->asv_abb[i].asv_value = int_asv_abb_info[i][target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_int = {
	.get_asv_group	= exynos5260_get_asv_group_int,
	.set_asv_info	= exynos5260_set_asv_info_int,
};

static unsigned int exynos5260_get_asv_group_mif(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_MIF);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return mif_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			if (asv_inform->asv_volt[i].asv_freq >= MIF_GRP_L1_FREQ)
				target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5260_GRP_L1];
			else
				target_asv_grp_nr = mif_fused_info.speed_grp[EXYNOS5260_GRP_L2];

			lock_voltage = exynos5260_lock_voltage(mif_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= mif_asv_volt_info[i][target_asv_grp_nr + 1])
				mif_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			mif_asv_volt_info[i][target_asv_grp_nr + 1] + set_mif_volt;
#else
		asv_inform->asv_volt[i].asv_value = mif_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		asv_inform->asv_abb[i].asv_freq = mif_asv_volt_info[i][0];
		asv_inform->asv_abb[i].asv_value = mif_asv_abb_info[i][target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_mif = {
	.get_asv_group	= exynos5260_get_asv_group_mif,
	.set_asv_info	= exynos5260_set_asv_info_mif,
};

static unsigned int exynos5260_get_asv_group_g3d(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_G3D);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return g3d_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			if (asv_inform->asv_volt[i].asv_freq >= G3D_GRP_L1_FREQ)
				target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5260_GRP_L1];
			else
				target_asv_grp_nr = g3d_fused_info.speed_grp[EXYNOS5260_GRP_L2];

			lock_voltage = exynos5260_lock_voltage(g3d_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= g3d_asv_volt_info[i][target_asv_grp_nr + 1])
				g3d_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			g3d_asv_volt_info[i][target_asv_grp_nr + 1] + set_g3d_volt;
#else
		asv_inform->asv_volt[i].asv_value = g3d_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
		asv_inform->asv_abb[i].asv_freq = g3d_asv_volt_info[i][0];
		asv_inform->asv_abb[i].asv_value = g3d_asv_abb_info[i][target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_g3d = {
	.get_asv_group	= exynos5260_get_asv_group_g3d,
	.set_asv_info	= exynos5260_set_asv_info_g3d,
};

static unsigned int exynos5260_get_asv_group_disp(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_DISP);
	unsigned int ids_idx = 0, hpm_idx = 0;

	if (asv_ref_info.is_speedgroup)
		return disp_fused_info.speed_grp[EXYNOS5260_GRP_L1];

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[0][i] &&
			asv_comm->ids_value <= refer_table_get_asv[0][i]) {
			ids_idx = i;
			break;
		}
	}

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (refer_use_table_get_asv[1][i] &&
			asv_comm->hpm_value <= refer_table_get_asv[1][i]) {
			hpm_idx = i;
			break;
		}
	}

	return ((ids_idx >= hpm_idx) ? hpm_idx : ids_idx);
}

static void exynos5260_set_asv_info_disp(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int lock_voltage;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_volt) {
		pr_err("%s: Memory allocation failed for asv voltage\n", __func__);
		return;
	}

	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	if (!asv_inform->asv_abb) {
		pr_err("%s: Memory allocation failed for asv abb\n", __func__);
		kfree(asv_inform->asv_volt);
		return;
	}

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = disp_asv_volt_info[i][0];

		if (asv_ref_info.is_speedgroup) {
			lock_voltage = exynos5260_lock_voltage(disp_fused_info.voltage_lock);
			if (lock_voltage &&
				lock_voltage >= disp_asv_volt_info[i][target_asv_grp_nr + 1])
				disp_asv_volt_info[i][target_asv_grp_nr + 1] = lock_voltage;
		}

#ifdef CONFIG_ASV_MARGIN_TEST
		asv_inform->asv_volt[i].asv_value =
			disp_asv_volt_info[i][target_asv_grp_nr + 1] + set_disp_volt;
#else
		asv_inform->asv_volt[i].asv_value = disp_asv_volt_info[i][target_asv_grp_nr + 1];
#endif
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
			pr_info("%s LV%d freq : %d volt : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value);
			pr_info("%s LV%d freq : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_abb[i].asv_freq,
					asv_inform->asv_abb[i].asv_value);
		}
	}
}

static struct asv_ops exynos5260_asv_ops_disp = {
	.get_asv_group	= exynos5260_get_asv_group_disp,
	.set_asv_info	= exynos5260_set_asv_info_disp,
};

struct asv_info exynos5260_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos5260_asv_ops_arm,
		.abb_info	= &exynos5260_abb_common,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_KFC,
		.name		= "VDD_KFC",
		.ops		= &exynos5260_asv_ops_kfc,
		.abb_info	= &exynos5260_abb_common,
		.asv_group_nr	= ASV_GRP_NR(KFC),
		.dvfs_level_nr	= DVFS_LEVEL_NR(KFC),
		.max_volt_value = MAX_VOLT(KFC),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos5260_asv_ops_int,
		.abb_info	= &exynos5260_abb_common,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos5260_asv_ops_mif,
		.abb_info	= &exynos5260_abb_common,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos5260_asv_ops_g3d,
		.abb_info	= &exynos5260_abb_common,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	}, {
		.asv_type	= ID_DISP,
		.name		= "VDD_DISP",
		.ops		= &exynos5260_asv_ops_disp,
		.asv_group_nr	= ASV_GRP_NR(DISP),
		.dvfs_level_nr	= DVFS_LEVEL_NR(DISP),
		.max_volt_value = MAX_VOLT(DISP),
	},
};

unsigned int exynos5260_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos5260_asv_member); i++)
		add_asv_member(&exynos5260_asv_member[i]);

	return 0;
}

#ifdef CONFIG_PM
static struct sleep_save exynos5260_abb_save[] = {
	SAVE_ITEM(EXYNOS5260_BIAS_CON0),
	SAVE_ITEM(EXYNOS5260_BIAS_CON1),
	SAVE_ITEM(EXYNOS5260_BIAS_CON2),
	SAVE_ITEM(EXYNOS5260_BIAS_CON3),
	SAVE_ITEM(EXYNOS5260_BIAS_CON4),
};

static int exynos5260_asv_suspend(void)
{
	struct asv_info *exynos_asv_info;
	int i;

	s3c_pm_do_save(exynos5260_abb_save,
			ARRAY_SIZE(exynos5260_abb_save));

	for (i = 0; i < ARRAY_SIZE(exynos5260_asv_member); i++) {
		exynos_asv_info = &exynos5260_asv_member[i];
		exynos5260_set_abb_bypass(exynos_asv_info);
	}

	return 0;
}

static void exynos5260_asv_resume(void)
{
	s3c_pm_do_restore_core(exynos5260_abb_save,
			ARRAY_SIZE(exynos5260_abb_save));
}
#else
#define exynos5260_asv_suspend NULL
#define exynos5260_asv_resume NULL
#endif

static struct syscore_ops exynos5260_asv_syscore_ops = {
	.suspend	= exynos5260_asv_suspend,
	.resume		= exynos5260_asv_resume,
};

int exynos5260_init_asv(struct asv_common *asv_info)
{
	struct clk *clk_abb;
	unsigned int chip_id2_value;
	unsigned int chip_id4_value;
	unsigned int cpu_speed_grp_reg;
	unsigned int g3d_mif_speed_grp_reg;
	unsigned int int_disp_speed_grp_reg;
	unsigned int fused_group_reg;
	unsigned int fused_group;

	if (samsung_rev() == EXYNOS5260_REV_0) {
		pr_err("EXYNOS5260 ASV : cannot support Rev0\n");
		return -EINVAL;
	}

	/* enable abb clock */
	clk_abb = clk_get(NULL, "abb");
	if (IS_ERR(clk_abb)) {
		pr_err("EXYNOS5260 ASV : cannot find abb clock!\n");
		return -EINVAL;
	}
	clk_enable(clk_abb);

	chip_id2_value = __raw_readl(CHIP_ID2_REG);

	fused_group_reg = __raw_readl(FUSED_SPEED_GRP_REG);
	fused_group = (fused_group_reg >> EXYNOS5260_FUSED_GRP_OFFSET) & EXYNOS5260_FUSED_GRP_MASK;

	if (fused_group) {
		asv_ref_info.is_speedgroup = true;

		/* EGL/KFC Speed Group */
		cpu_speed_grp_reg = __raw_readl(CPU_SPEED_GRP_REG);
		egl_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(cpu_speed_grp_reg >> EXYNOS5260_EGL_GRP1_OFFSET) & EXYNOS5260_EGL_GRP_MASK;
		egl_fused_info.speed_grp[EXYNOS5260_GRP_L2] =
			(cpu_speed_grp_reg >> EXYNOS5260_EGL_GRP2_OFFSET) & EXYNOS5260_EGL_GRP_MASK;
		egl_fused_info.speed_grp[EXYNOS5260_GRP_L3] =
			(cpu_speed_grp_reg >> EXYNOS5260_EGL_GRP3_OFFSET) & EXYNOS5260_EGL_GRP_MASK;
		egl_fused_info.voltage_lock =
			(cpu_speed_grp_reg >> EXYNOS5260_EGL_VOL_LOCK_OFFSET) & EXYNOS5260_EGL_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : EGL Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
			egl_fused_info.speed_grp[EXYNOS5260_GRP_L1], egl_fused_info.speed_grp[EXYNOS5260_GRP_L2],
			egl_fused_info.speed_grp[EXYNOS5260_GRP_L3], egl_fused_info.voltage_lock);

		kfc_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(cpu_speed_grp_reg >> EXYNOS5260_KFC_GRP1_OFFSET) & EXYNOS5260_KFC_GRP_MASK;
		kfc_fused_info.speed_grp[EXYNOS5260_GRP_L2] =
			(cpu_speed_grp_reg >> EXYNOS5260_KFC_GRP2_OFFSET) & EXYNOS5260_KFC_GRP_MASK;
		kfc_fused_info.speed_grp[EXYNOS5260_GRP_L3] =
			(cpu_speed_grp_reg >> EXYNOS5260_KFC_GRP3_OFFSET) & EXYNOS5260_KFC_GRP_MASK;
		kfc_fused_info.voltage_lock =
			(cpu_speed_grp_reg >> EXYNOS5260_KFC_VOL_LOCK_OFFSET) & EXYNOS5260_KFC_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : KFC Speed Grp : L1(%d), L2(%d), L3(%d) : volt_lock(%d)\n",
			kfc_fused_info.speed_grp[EXYNOS5260_GRP_L1], kfc_fused_info.speed_grp[EXYNOS5260_GRP_L2],
			kfc_fused_info.speed_grp[EXYNOS5260_GRP_L3], kfc_fused_info.voltage_lock);

		/* G3D/MIF Speed Group */
		g3d_mif_speed_grp_reg = __raw_readl(G3D_MIF_SPEED_GRP_REG);
		g3d_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_G3D_GRP1_OFFSET) & EXYNOS5260_G3D_GRP_MASK;
		g3d_fused_info.speed_grp[EXYNOS5260_GRP_L2] =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_G3D_GRP2_OFFSET) & EXYNOS5260_G3D_GRP_MASK;
		g3d_fused_info.voltage_lock =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_G3D_VOL_LOCK_OFFSET) & EXYNOS5260_G3D_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : G3D Speed Grp : L1(%d), L2(%d) : volt_lock(%d)\n",
			g3d_fused_info.speed_grp[EXYNOS5260_GRP_L1], g3d_fused_info.speed_grp[EXYNOS5260_GRP_L2],
			g3d_fused_info.voltage_lock);

		mif_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_MIF_GRP1_OFFSET) & EXYNOS5260_MIF_GRP_MASK;
		mif_fused_info.speed_grp[EXYNOS5260_GRP_L2] =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_MIF_GRP2_OFFSET) & EXYNOS5260_MIF_GRP_MASK;
		mif_fused_info.voltage_lock =
			(g3d_mif_speed_grp_reg >> EXYNOS5260_MIF_VOL_LOCK_OFFSET) & EXYNOS5260_MIF_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : MIF Speed Grp : L1(%d), L2(%d) : volt_lock(%d)\n",
			mif_fused_info.speed_grp[EXYNOS5260_GRP_L1], mif_fused_info.speed_grp[EXYNOS5260_GRP_L2],
			mif_fused_info.voltage_lock);

		/* INT/DISP Speed Group */
		int_disp_speed_grp_reg = __raw_readl(INT_DISP_SPEED_GRP_REG);
		int_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(int_disp_speed_grp_reg >> EXYNOS5260_INT_GRP1_OFFSET) & EXYNOS5260_INT_GRP_MASK;
		int_fused_info.speed_grp[EXYNOS5260_GRP_L2] =
			(int_disp_speed_grp_reg >> EXYNOS5260_INT_GRP2_OFFSET) & EXYNOS5260_INT_GRP_MASK;
		int_fused_info.voltage_lock =
			(int_disp_speed_grp_reg >> EXYNOS5260_INT_VOL_LOCK_OFFSET) & EXYNOS5260_INT_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : INT Speed Grp : L1(%d), L2(%d) : volt_lock(%d)\n",
			int_fused_info.speed_grp[EXYNOS5260_GRP_L1], int_fused_info.speed_grp[EXYNOS5260_GRP_L2],
			int_fused_info.voltage_lock);

		disp_fused_info.speed_grp[EXYNOS5260_GRP_L1] =
			(int_disp_speed_grp_reg >> EXYNOS5260_DISP_GRP1_OFFSET) & EXYNOS5260_DISP_GRP_MASK;
		disp_fused_info.voltage_lock =
			(int_disp_speed_grp_reg >> EXYNOS5260_DISP_VOL_LOCK_OFFSET) & EXYNOS5260_DISP_VOL_LOCK_MASK;

		pr_info("EXYNOS5260 ASV : DISP Speed Grp : L1(%d) : volt_lock(%d)\n",
			disp_fused_info.speed_grp[EXYNOS5260_GRP_L1], disp_fused_info.voltage_lock);
	} else {
		chip_id4_value = __raw_readl(CHIP_ID4_REG);
		asv_info->hpm_value = (chip_id4_value >> EXYNOS5260_TMCB_OFFSET) & EXYNOS5260_TMCB_MASK;
		asv_info->ids_value = (chip_id2_value >> EXYNOS5260_IDS_OFFSET) & EXYNOS5260_IDS_MASK;
		asv_ref_info.hpm_value = asv_info->hpm_value;
		asv_ref_info.ids_value = asv_info->ids_value;

		if (!asv_info->hpm_value)
			pr_err("Exynos5260 ASV : invalid IDS value\n");

		pr_info("EXYNOS5260 ASV : IDS : %d HPM : %d\n", asv_info->ids_value, asv_info->hpm_value);
	}

	asv_info->regist_asv_member = exynos5260_regist_asv_member;

	register_syscore_ops(&exynos5260_asv_syscore_ops);

	return 0;
}
