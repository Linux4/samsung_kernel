/* linux/arch/arm/mach-exynos/asv-exynos3470.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS3470 - ASV(Adoptive Support Voltage) driver
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
#include <mach/asv-exynos3470.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#define PRO_ID_REG		(S5P_VA_CHIPID)
#define CHIP_ID_REG		(S5P_VA_CHIPID + 0x04)
#define CHIP_OPERATION_REG	(S5P_VA_CHIPID + 0x08)
#define GROUP_FUSE_BIT		(3)
#define GROUP_FUSE_MASK		(0x1)
#define ORIGINAL_BIT		(17)
#define ORIGINAL_MASK		(0xF)
#define MODIFIED_BIT		(21)
#define MODIFIED_MASK		(0x7)
#define EXYNOS3470_IDS_OFFSET	(24)
#define EXYNOS3470_IDS_MASK	(0xFF)
#define EXYNOS3470_HPM_OFFSET	(11)
#define EXYNOS3470_HPM_MASK	(0x3F)

#define ARM_LOCK_BIT		(28)
#define INT_LOCK_BIT		(7)
#define MIF_LOCK_BIT		(24)
#define G3D_LOCK_BIT		(9)
#define ARM_LOCK_MASK		(0x7)
#define INT_LOCK_MASK		(0x3)
#define MIF_LOCK_MASK		(0x3)
#define G3D_LOCK_MASK		(0x3)

#define LOT_ID_REG		(S5P_VA_CHIPID + 0x14)
#define LOT_ID_LEN		(5)

#define POP_TYPE_SHIFT	4
#define POP_TYPE_MASK	0x3

unsigned int pop_type;

extern int arm_lock;
static int mif_lock;
unsigned int revision_id;

enum volt_offset {
        VOLT_OFFSET_0MV,
        VOLT_OFFSET_12_5MV,
        VOLT_OFFSET_50MV,
        VOLT_OFFSET_25MV,
};

unsigned int special_lot_group;
bool is_speedgroup;
bool is_special_lot;
enum volt_offset asv_volt_offset[5][1];

static int set_arm_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;

#ifdef CONFIG_ASV_MARGIN_TEST
static int __init get_arm_volt(char *str)
{
	get_option(&str, &set_arm_volt);
	return 0;
}
early_param("arm", get_arm_volt);

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
#endif

unsigned int exynos3470_add_volt_offset(unsigned int voltage, enum volt_offset offset)
{
	switch (offset) {
	case VOLT_OFFSET_0MV:
		break;
	case VOLT_OFFSET_12_5MV:
		voltage += 12500;
		break;
	case VOLT_OFFSET_50MV:
		voltage += 50000;
		break;
	case VOLT_OFFSET_25MV:
		voltage += 25000;
		break;
	}

	return voltage;
}

unsigned int exynos3470_apply_volt_offset(unsigned int voltage, enum asv_type_id target_type)
{
	if (!is_speedgroup)
		return voltage;

	voltage = exynos3470_add_volt_offset(voltage, asv_volt_offset[target_type][0]);

	return voltage;
}

unsigned int exynos_set_abb(enum asv_type_id type, unsigned int target_val)
{
	void __iomem *target_reg;
	unsigned int tmp;

	switch (type) {
	case ID_ARM:
		target_reg = EXYNOS4270_ABB_ARM;
		break;
	case ID_INT:
		target_reg = EXYNOS4270_ABB_INT;
		break;
	case ID_MIF:
		target_reg = EXYNOS4270_ABB_MIF;
		break;
	case ID_G3D:
		target_reg = EXYNOS4270_ABB_G3D;
		break;
	default:
		pr_err("Invalied ABB type\n");
		return -EINVAL;
	}

	tmp = __raw_readl(target_reg);

	if (target_val == ABB_BYPASS) {
		/* Disable PMOS */
		tmp &= ~ABB_ENABLE_PMOS_MASK;
		__raw_writel(tmp, target_reg);

		/* Enable SEL */
		tmp |= ABB_ENABLE_SET_MASK;
		__raw_writel(tmp, target_reg);

		/* Setting Bypass */
		tmp &= ~ABB_CODE_PMOS_MASK;
		__raw_writel(tmp, target_reg);
	} else {
		/* Enable SEL */
		tmp |= ABB_ENABLE_SET_MASK;
		__raw_writel(tmp, target_reg);

		/* Setting FBB or RBB */
		tmp &= ~ABB_CODE_PMOS_MASK;
		tmp |= (target_val << ABB_CODE_PMOS_OFFSET);
		__raw_writel(tmp, target_reg);

		/* Enable PMOS */
		tmp |= ABB_ENABLE_PMOS_MASK;
		__raw_writel(tmp, target_reg);
	}

	return 0;
}

#ifdef CONFIG_PM
static void exynos3470_set_abb_bypass(struct asv_info *asv_inform)
{
	switch (asv_inform->asv_type) {
	case ID_ARM:
	case ID_INT:
	case ID_MIF:
	case ID_G3D:
		break;
	default:
		return;
	}

	exynos_set_abb(asv_inform->asv_type, ABB_BYPASS);
}
#endif

static unsigned int exynos3470_get_asv_group_arm(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_ARM);

	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos3470_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int *arm_volt_info;
	unsigned int *arm_abb_info;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		if (revision_id == 2) {
			arm_volt_info = arm_asv_volt_info_rev2[i];
			arm_abb_info = arm_asv_abb_info_rev2[i];
		} else {
			arm_volt_info = arm_asv_volt_info[i];
			arm_abb_info = arm_asv_abb_info[i];
		}

		asv_inform->asv_volt[i].asv_freq = arm_volt_info[0];
		asv_inform->asv_volt[i].asv_value =
			exynos3470_apply_volt_offset(arm_volt_info[target_asv_grp_nr + 1], ID_ARM) + set_arm_volt;
		asv_inform->asv_abb[i].asv_freq = arm_abb_info[0];
		asv_inform->asv_abb[i].asv_value = arm_abb_info[target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

struct asv_ops exynos3470_asv_ops_arm = {
	.get_asv_group	= exynos3470_get_asv_group_arm,
	.set_asv_info	= exynos3470_set_asv_info_arm,
};

static unsigned int exynos3470_get_asv_group_int(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_INT);

	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos3470_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int *int_volt_info;
	unsigned int *int_abb_info;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		if (revision_id == 2) {
			int_volt_info = int_asv_volt_info_rev2[i];
			int_abb_info = int_asv_abb_info_rev2[i];
		} else {
			int_volt_info = int_asv_volt_info[i];
			int_abb_info = int_asv_abb_info[i];
		}
		asv_inform->asv_volt[i].asv_freq = int_volt_info[0];
		asv_inform->asv_volt[i].asv_value =
			exynos3470_apply_volt_offset(int_volt_info[target_asv_grp_nr + 1], ID_INT) + set_int_volt;
		asv_inform->asv_abb[i].asv_freq = int_abb_info[0];
		asv_inform->asv_abb[i].asv_value = int_abb_info[target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

struct asv_ops exynos3470_asv_ops_int = {
	.get_asv_group	= exynos3470_get_asv_group_int,
	.set_asv_info	= exynos3470_set_asv_info_int,
};

static unsigned int exynos3470_get_asv_group_mif(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_MIF);

	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos3470_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int *mif_volt_info;
	unsigned int *mif_abb_info;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		if (revision_id == 2) {
			mif_volt_info = mif_asv_volt_info_rev2[i];
			mif_abb_info = mif_asv_abb_info_rev2[i];
		} else {
			mif_volt_info = mif_asv_volt_info[i];
			mif_abb_info = mif_asv_abb_info[i];
		}
		asv_inform->asv_volt[i].asv_freq = mif_volt_info[0];
		if (mif_lock == 2) {
			if (i < 2) asv_inform->asv_volt[i].asv_value = mif_volt_info[target_asv_grp_nr + 1] + set_int_volt + set_mif_volt;
			else if (i >= 2) {
				asv_inform->asv_volt[i].asv_value =
						exynos3470_apply_volt_offset(mif_volt_info[target_asv_grp_nr + 1], ID_MIF) + set_mif_volt;
			}
		} else {
			asv_inform->asv_volt[i].asv_value = exynos3470_apply_volt_offset(mif_volt_info[target_asv_grp_nr + 1], ID_MIF) + set_mif_volt;
		}
		asv_inform->asv_abb[i].asv_freq = mif_abb_info[0];
		asv_inform->asv_abb[i].asv_value = mif_abb_info[target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

struct asv_ops exynos3470_asv_ops_mif = {
	.get_asv_group	= exynos3470_get_asv_group_mif,
	.set_asv_info	= exynos3470_set_asv_info_mif,
};

static unsigned int exynos3470_get_asv_group_g3d(struct asv_common *asv_comm)
{
	unsigned int i;
	struct asv_info *target_asv_info = asv_get(ID_G3D);

	if (is_special_lot)
		return special_lot_group;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (asv_comm->ids_value <= refer_table_get_asv[0][i])
			return i;

		if (asv_comm->hpm_value <= refer_table_get_asv[1][i])
			return i;
	}

	return 0;
}

static void exynos3470_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;
	unsigned int *g3d_volt_info;
	unsigned int *g3d_abb_info;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		if (revision_id == 2) {
			g3d_volt_info = g3d_asv_volt_info_rev2[i];
			g3d_abb_info = g3d_asv_abb_info_rev2[i];
		} else {
			g3d_volt_info = g3d_asv_volt_info[i];
			g3d_abb_info = g3d_asv_abb_info[i];
		}
		asv_inform->asv_volt[i].asv_freq = g3d_volt_info[0];
		asv_inform->asv_volt[i].asv_value = g3d_volt_info[target_asv_grp_nr + 1] + set_g3d_volt;
		asv_inform->asv_abb[i].asv_freq = g3d_abb_info[0];
		asv_inform->asv_abb[i].asv_value = g3d_abb_info[target_asv_grp_nr + 1];
	}

	if (show_value) {
		for (i = 0; i < asv_inform->dvfs_level_nr; i++)
			pr_info("%s LV%d freq : %d volt : %d abb : %d\n",
					asv_inform->name, i,
					asv_inform->asv_volt[i].asv_freq,
					asv_inform->asv_volt[i].asv_value,
					asv_inform->asv_abb[i].asv_value);
	}
}

struct asv_ops exynos3470_asv_ops_g3d = {
	.get_asv_group	= exynos3470_get_asv_group_g3d,
	.set_asv_info	= exynos3470_set_asv_info_g3d,
};

struct asv_info exynos3470_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos3470_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos3470_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos3470_asv_ops_int,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos3470_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	},
};

unsigned int exynos3470_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos3470_asv_member); i++)
		add_asv_member(&exynos3470_asv_member[i]);

	return 0;
}

#ifdef CONFIG_PM
static struct sleep_save exynos3470_abb_save[] = {
	SAVE_ITEM(EXYNOS4270_ABB_ARM),
	SAVE_ITEM(EXYNOS4270_ABB_INT),
	SAVE_ITEM(EXYNOS4270_ABB_MIF),
	SAVE_ITEM(EXYNOS4270_ABB_G3D),
};

static int exynos3470_asv_suspend(void)
{
	struct asv_info *exynos_asv_info;
	int i;

	s3c_pm_do_save(exynos3470_abb_save,
			ARRAY_SIZE(exynos3470_abb_save));

	for (i = 0; i < ARRAY_SIZE(exynos3470_asv_member); i++) {
		exynos_asv_info = &exynos3470_asv_member[i];
		exynos3470_set_abb_bypass(exynos_asv_info);
	}

	return 0;
}

static void exynos3470_asv_resume(void)
{
	s3c_pm_do_restore_core(exynos3470_abb_save,
			ARRAY_SIZE(exynos3470_abb_save));
}
#else
#define exynos3470_asv_suspend NULL
#define exynos3470_asv_resume NULL
#endif

static struct syscore_ops exynos3470_asv_syscore_ops = {
	.suspend	= exynos3470_asv_suspend,
	.resume		= exynos3470_asv_resume,
};

static void exynos3470_get_lot_id(struct asv_common *asv_info)
{
	unsigned int lid_reg = 0;
	unsigned int rev_lid = 0;
	unsigned int i;
	unsigned int tmp;

	lid_reg = __raw_readl(LOT_ID_REG);

	for (i = 0; i < 32; i++) {
		tmp = (lid_reg >> i) & 0x1;
		rev_lid += tmp << (31 - i);
	}

	asv_info->lot_name[0] = 'N';
	lid_reg = (rev_lid >> 11) & 0x1FFFFF;

	for (i = 4; i >= 1; i--) {
		tmp = lid_reg % 36;
		lid_reg /= 36;
		asv_info->lot_name[i] = (tmp < 10) ? (tmp + '0') : ((tmp - 10) + 'A');
	}
}

int exynos3470_init_asv(struct asv_common *asv_info)
{
	unsigned int chip_id = __raw_readl(CHIP_ID_REG);
	unsigned int operation = __raw_readl(CHIP_OPERATION_REG);

	pop_type = (chip_id >> POP_TYPE_SHIFT) & POP_TYPE_MASK;

	revision_id = __raw_readl(PRO_ID_REG);
	revision_id = ((revision_id >> 4) & 0x7);

	printk("EXYNOS3470 chip revision : EVT%d \n", revision_id);

	special_lot_group = 0;
	is_special_lot = false;
	is_speedgroup = false;

	exynos3470_get_lot_id(asv_info);

	if (is_special_lot)
		goto set_asv_info;

	if (((chip_id >> GROUP_FUSE_BIT) & GROUP_FUSE_MASK)) {
		special_lot_group = ((chip_id >> ORIGINAL_BIT) & ORIGINAL_MASK) - ((chip_id >> MODIFIED_BIT) & MODIFIED_MASK);
		is_special_lot = true;
		is_speedgroup = true;
		pr_info("Exynos3470 ASV : Use Fusing Speed Group %d\n", special_lot_group);
	} else {
		asv_info->ids_value = (chip_id >> EXYNOS3470_IDS_OFFSET) & EXYNOS3470_IDS_MASK;
		asv_info->hpm_value = (chip_id >> EXYNOS3470_HPM_OFFSET) & EXYNOS3470_HPM_MASK;
	}

	pr_info("EXYNOS3470 ASV INFO\nLOT ID : %s\nIDS : %d\nHPM : %d\n",
				asv_info->lot_name, asv_info->ids_value, asv_info->hpm_value);

	asv_info->regist_asv_member = exynos3470_regist_asv_member;

	if (is_speedgroup) {
		asv_volt_offset[ID_ARM][0] = ((operation >> ARM_LOCK_BIT) & ARM_LOCK_MASK);
		arm_lock = asv_volt_offset[ID_ARM][0];
		asv_volt_offset[ID_INT][0] = ((chip_id >> INT_LOCK_BIT) & INT_LOCK_MASK);
		asv_volt_offset[ID_MIF][0] = ((operation >> MIF_LOCK_BIT) & MIF_LOCK_MASK);
		mif_lock = asv_volt_offset[ID_MIF][0];
		asv_volt_offset[ID_G3D][0] = ((chip_id >> G3D_LOCK_BIT) & G3D_LOCK_MASK);
	}

	/* If it is not revision 2, ignore ids & hpm in asv group 1 */
	if (samsung_rev() >= EXYNOS3470_REV_2_0)
			refer_table_get_asv = refer_table_get_asv_rev2;
	else
			refer_table_get_asv = refer_table_get_asv_rev;

set_asv_info:
	asv_info->regist_asv_member = exynos3470_regist_asv_member;

	if (samsung_rev() >= EXYNOS3470_REV_2_0)
		register_syscore_ops(&exynos3470_asv_syscore_ops);

	return 0;
}
