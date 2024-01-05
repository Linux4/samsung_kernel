/* linux/arch/arm/mach-exynos/asv-exynos4415.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4415 - ASV(Adoptive Support Voltage) driver
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
#include <mach/asv-exynos4415.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos4415.h>

#include <plat/cpu.h>
#include <plat/pm.h>

#define PRO_ID_REG		(S5P_VA_CHIPID)
#define CHIP_ID_REG		(S5P_VA_CHIPID + 0x04)
#define CHIP_OPERATION_REG	(S5P_VA_PROVISION)
#define ASV_DVFSVER_OFFSET	(0)
#define ASV_DVFSVER_MASK	(0x3)
#define GROUP_FUSE_BIT		(3)
#define GROUP_FUSE_MASK		(0x1)
#define ASV_DDRSZ_OFFSET	(4)
#define ASV_DDRSZ_MASK		(0x3)
#define ORIGINAL_BIT		(17)
#define ORIGINAL_MASK		(0xF)
#define MODIFIED_BIT		(21)
#define MODIFIED_MASK		(0x7)
#define EXYNOS4415_IDS_OFFSET	(24)
#define EXYNOS4415_IDS_MASK	(0xFF)
#define EXYNOS4415_HPM_OFFSET	(11)
#define EXYNOS4415_HPM_MASK	(0x3F)

#define ARM_LOCK_BIT		(28)
#define INT_LOCK_BIT		(7)
#define MIF_LOCK_BIT		(24)
#define G3D_LOCK_BIT		(9)
#define ARM_LOCK_MASK		(0x7)
#define INT_LOCK_MASK		(0x3)
#define MIF_LOCK_MASK		(0x3)
#define G3D_LOCK_MASK		(0x3)

#define PKGOPT_OFFSET		6
#define PKGOPT_MASK		0x1

#define LOT_ID_REG		(S5P_VA_CHIPID + 0x14)
#define LOT_ID_LEN		(5)

#define DEFAULT_ASV_GROUP	(4)

/* e-fuse locking variable */
#define ARM_LOCK_700		(4)
#define ARM_LOCK_800		(5)
#define ARM_LOCK_900		(6)
#define ARM_LOCK_1000		(7)

#define ASV_VER_000                    (0x0)
#define DDRSZ_1GB                      (0x00)

static bool arm_maxlimit = false;      /* ARM Max Limit true: 1.2GHz false: 1.6GHz */
static int ddr_size = 0;                       /* 0: 1GB 1: 1.5GB 2: 2GB */

#ifdef CONFIG_ASV_MARGIN_TEST                                                                                                                                             
static int set_arm_volt = 0;
static int set_int_volt = 0;
static int set_mif_volt = 0;
static int set_g3d_volt = 0;

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
#endif

unsigned int revision_id;

enum volt_offset {
        VOLT_OFFSET_0MV,
        VOLT_OFFSET_12_5MV,
        VOLT_OFFSET_50MV,
        VOLT_OFFSET_25MV,
};

static enum volt_offset asv_volt_offset[5][1];

bool exynos4415_get_arm_maxlimit(void)
{
	return arm_maxlimit;
}

int exynos4415_get_ddrsize(void)
{
	return ddr_size;
}

static unsigned int exynos4415_add_volt_offset(unsigned int voltage, enum volt_offset offset)
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

static unsigned int exynos4415_apply_volt_offset(unsigned int voltage, enum asv_type_id target_type)
{
	voltage = exynos4415_add_volt_offset(voltage, asv_volt_offset[target_type][0]);

	return voltage;
}

unsigned int exynos_set_abb(enum asv_type_id type, unsigned int target_val)
{
	void __iomem *target_reg;
	unsigned int tmp;

	/*TODO: required updated ABB guide for stability */

	switch (type) {
	case ID_ARM:
		target_reg = EXYNOS4415_BODY_BIAS_CON3;
		break;
	case ID_INT:
		target_reg = EXYNOS4415_BODY_BIAS_CON2;
		break;
	default:
		pr_err("Invalid ABB type\n");
		return -EINVAL;
	}

	tmp = __raw_readl(target_reg);
	if (target_val != ABB_BYPASS) {	/* FBB/RBB */
		tmp |= ABB_ENABLE_SET_MASK;
		tmp |= ABB_ENABLE_PMOS_MASK;
	} else {	/* ABB_BYPASS */
		tmp &= ~ABB_ENABLE_PMOS_MASK;
		tmp &= ~ABB_ENABLE_SET_MASK;
		target_val = 0;
	}
	tmp &= ~ABB_CODE_PMOS_MASK;
	tmp |= (target_val << ABB_CODE_PMOS_OFFSET);
	__raw_writel(tmp, target_reg);

	return 0;
}

#ifdef CONFIG_PM
static void exynos4415_set_abb_bypass(struct asv_info *asv_inform)
{
	switch (asv_inform->asv_type) {
	case ID_ARM:
	case ID_INT:
		break;
	default:
		return;
	}

	exynos_set_abb(asv_inform->asv_type, ABB_BYPASS);
}
#endif

static unsigned int exynos4415_get_asv_group(struct asv_common *asv_comm, unsigned int id)
{
	unsigned int i, ids_level = 0, hpm_level = 0, asv_grp = 0;
	struct asv_info *target_asv_info = asv_get(id);

	if (asv_comm->efuse_status)
		return asv_comm->speed_value;

	for (i = 0; i < target_asv_info->asv_group_nr; i++) {
		if (!ids_level && asv_comm->ids_value <= refer_table_get_asv[0][i])
			ids_level = i;

		if (!hpm_level && asv_comm->hpm_value <= refer_table_get_asv[1][i])
			hpm_level = i;

		if (ids_level && hpm_level)
			break;
	}

	if (ids_level > hpm_level)
		asv_grp =  hpm_level;
	else
		asv_grp = ids_level;

	/* This is hudson limitation */
	if (asv_grp < DEFAULT_ASV_GROUP)
		asv_grp = DEFAULT_ASV_GROUP;

	return asv_grp;
}

static unsigned int exynos4415_get_asv_group_arm(struct asv_common *asv_comm)
{
	return exynos4415_get_asv_group(asv_comm, ID_ARM);
}

static void exynos4415_set_asv_info_arm(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	int arm_lock;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb  = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	arm_lock = asv_volt_offset[ID_ARM][0];

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = arm_asv_volt_info[i][0];
		switch (arm_lock) {
		case ARM_LOCK_700 :
			if (asv_inform->asv_volt[i].asv_freq < 700000)		/* < 700MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[9][target_asv_grp_nr + 1];
			else							/* > 700MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
			break;
		case ARM_LOCK_800 :
			if (asv_inform->asv_volt[i].asv_freq < 800000)		/* < 800MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[8][target_asv_grp_nr + 1];
			else							/* > 800MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
			break;
		case ARM_LOCK_900 :
			if (asv_inform->asv_volt[i].asv_freq < 900000)		/* < 900MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[7][target_asv_grp_nr + 1];
			else							/* > 900MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
			break;
		case ARM_LOCK_1000 :
			if (asv_inform->asv_volt[i].asv_freq < 1000000)		/* < 1000MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[6][target_asv_grp_nr + 1];
			else							/* > 1000MHz */
				asv_inform->asv_volt[i].asv_value = arm_asv_volt_info[i][target_asv_grp_nr + 1];
			break;
		default :
			asv_inform->asv_volt[i].asv_value = exynos4415_apply_volt_offset(
							arm_asv_volt_info[i][target_asv_grp_nr + 1], ID_ARM);
			break;
		}
#ifdef CONFIG_ASV_MARGIN_TEST
                asv_inform->asv_volt[i].asv_value += set_arm_volt;
#endif
		asv_inform->asv_abb[i].asv_freq = arm_asv_abb_info[i][0];
		asv_inform->asv_abb[i].asv_value = arm_asv_abb_info[i][target_asv_grp_nr + 1];
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

struct asv_ops exynos4415_asv_ops_arm = {
	.get_asv_group	= exynos4415_get_asv_group_arm,
	.set_asv_info	= exynos4415_set_asv_info_arm,
};

static unsigned int exynos4415_get_asv_group_int(struct asv_common *asv_comm)
{
	return exynos4415_get_asv_group(asv_comm, ID_INT);
}

static void exynos4415_set_asv_info_int(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = int_asv_volt_info[i][0];
		asv_inform->asv_volt[i].asv_value = exynos4415_apply_volt_offset(int_asv_volt_info[i][target_asv_grp_nr + 1], ID_INT);
		asv_inform->asv_abb[i].asv_freq = int_asv_abb_info[i][0];
		asv_inform->asv_abb[i].asv_value = int_asv_abb_info[i][target_asv_grp_nr + 1];
#ifdef CONFIG_ASV_MARGIN_TEST
                asv_inform->asv_volt[i].asv_value += set_int_volt;
#endif
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

static struct asv_ops exynos4415_asv_ops_int = {
	.get_asv_group	= exynos4415_get_asv_group_int,
	.set_asv_info	= exynos4415_set_asv_info_int,
};

static unsigned int exynos4415_get_asv_group_mif(struct asv_common *asv_comm)
{
	return exynos4415_get_asv_group(asv_comm, ID_MIF);
}

static void exynos4415_set_asv_info_mif(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	int mif_lock;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	mif_lock = asv_volt_offset[ID_MIF][0];

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = mif_asv_volt_info[i][0];

		if (mif_lock == 2) {
			if (i < 2)
				asv_inform->asv_volt[i].asv_value = mif_asv_volt_info[i][target_asv_grp_nr + 1];
			else
				asv_inform->asv_volt[i].asv_value =
						exynos4415_apply_volt_offset(mif_asv_volt_info[i][target_asv_grp_nr + 1], ID_MIF);
		} else {
			asv_inform->asv_volt[i].asv_value = exynos4415_apply_volt_offset(mif_asv_volt_info[i][target_asv_grp_nr + 1], ID_MIF);
		}

		asv_inform->asv_abb[i].asv_freq = mif_asv_abb_info[i][0];
		asv_inform->asv_abb[i].asv_value = mif_asv_abb_info[i][target_asv_grp_nr + 1];
#ifdef CONFIG_ASV_MARGIN_TEST
                asv_inform->asv_volt[i].asv_value += set_mif_volt;
                pr_info("%s : mif offset %d\n", __func__, set_mif_volt);
#endif
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

static struct asv_ops exynos4415_asv_ops_mif = {
	.get_asv_group	= exynos4415_get_asv_group_mif,
	.set_asv_info	= exynos4415_set_asv_info_mif,
};

static unsigned int exynos4415_get_asv_group_g3d(struct asv_common *asv_comm)
{
	return exynos4415_get_asv_group(asv_comm, ID_G3D);
}

static void exynos4415_set_asv_info_g3d(struct asv_info *asv_inform, bool show_value)
{
	unsigned int i;
	unsigned int target_asv_grp_nr = asv_inform->result_asv_grp;

	asv_inform->asv_volt = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);
	asv_inform->asv_abb = kmalloc((sizeof(struct asv_freq_table) * asv_inform->dvfs_level_nr), GFP_KERNEL);

	for (i = 0; i < asv_inform->dvfs_level_nr; i++) {
		asv_inform->asv_volt[i].asv_freq = g3d_asv_volt_info[i][0];
		asv_inform->asv_volt[i].asv_value = exynos4415_apply_volt_offset(g3d_asv_volt_info[i][target_asv_grp_nr + 1], ID_G3D);
		asv_inform->asv_abb[i].asv_freq = g3d_asv_abb_info[i][0];
		asv_inform->asv_abb[i].asv_value = g3d_asv_abb_info[i][target_asv_grp_nr + 1];
#ifdef CONFIG_ASV_MARGIN_TEST
                asv_inform->asv_volt[i].asv_value += set_g3d_volt;
#endif
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

static struct asv_ops exynos4415_asv_ops_g3d = {
	.get_asv_group	= exynos4415_get_asv_group_g3d,
	.set_asv_info	= exynos4415_set_asv_info_g3d,
};

struct asv_info exynos4415_asv_member[] = {
	{
		.asv_type	= ID_ARM,
		.name		= "VDD_ARM",
		.ops		= &exynos4415_asv_ops_arm,
		.asv_group_nr	= ASV_GRP_NR(ARM),
		.dvfs_level_nr	= DVFS_LEVEL_NR(ARM),
		.max_volt_value = MAX_VOLT(ARM),
	}, {
		.asv_type	= ID_MIF,
		.name		= "VDD_MIF",
		.ops		= &exynos4415_asv_ops_mif,
		.asv_group_nr	= ASV_GRP_NR(MIF),
		.dvfs_level_nr	= DVFS_LEVEL_NR(MIF),
		.max_volt_value = MAX_VOLT(MIF),
	}, {
		.asv_type	= ID_INT,
		.name		= "VDD_INT",
		.ops		= &exynos4415_asv_ops_int,
		.asv_group_nr	= ASV_GRP_NR(INT),
		.dvfs_level_nr	= DVFS_LEVEL_NR(INT),
		.max_volt_value = MAX_VOLT(INT),
	}, {
		.asv_type	= ID_G3D,
		.name		= "VDD_G3D",
		.ops		= &exynos4415_asv_ops_g3d,
		.asv_group_nr	= ASV_GRP_NR(G3D),
		.dvfs_level_nr	= DVFS_LEVEL_NR(G3D),
		.max_volt_value = MAX_VOLT(G3D),
	},
};

unsigned int exynos4415_regist_asv_member(void)
{
	unsigned int i;

	/* Regist asv member into list */
	for (i = 0; i < ARRAY_SIZE(exynos4415_asv_member); i++)
		add_asv_member(&exynos4415_asv_member[i]);

	return 0;
}

#ifdef CONFIG_PM
static struct sleep_save exynos4415_abb_save[] = {
	SAVE_ITEM(EXYNOS4415_BODY_BIAS_CON2),
	SAVE_ITEM(EXYNOS4415_BODY_BIAS_CON3),
};

static int exynos4415_asv_suspend(void)
{
	struct asv_info *exynos_asv_info;
	int i;

	s3c_pm_do_save(exynos4415_abb_save,
			ARRAY_SIZE(exynos4415_abb_save));

	for (i = 0; i < ARRAY_SIZE(exynos4415_asv_member); i++) {
		exynos_asv_info = &exynos4415_asv_member[i];
		exynos4415_set_abb_bypass(exynos_asv_info);
	}

	return 0;
}

static void exynos4415_asv_resume(void)
{
	s3c_pm_do_restore_core(exynos4415_abb_save,
			ARRAY_SIZE(exynos4415_abb_save));
}
#else
#define exynos4415_asv_suspend NULL
#define exynos4415_asv_resume NULL
#endif

static struct syscore_ops exynos4415_asv_syscore_ops = {
	.suspend	= exynos4415_asv_suspend,
	.resume		= exynos4415_asv_resume,
};

static void exynos4415_get_lot_id(struct asv_common *asv_info)
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


int __init exynos4415_init_asv(struct asv_common *asv_info)
{
	unsigned int chip_id = __raw_readl(CHIP_ID_REG);
	unsigned int operation = __raw_readl(CHIP_OPERATION_REG);
	unsigned int original_speed_group, modified_speed_group;
	unsigned int dvfs_ver_info;

	exynos4415_get_lot_id(asv_info);
	pr_info("EXYNOS4415 ASV INFO\nLOT ID : %s\n", asv_info->lot_name);

	/* Get DVFS version information */
	dvfs_ver_info = (((chip_id >> ASV_DVFSVER_OFFSET) & ASV_DVFSVER_MASK));
	ddr_size = (((chip_id >> ASV_DDRSZ_OFFSET) & ASV_DDRSZ_MASK));

	if (((dvfs_ver_info == ASV_VER_000) && (ddr_size == DDRSZ_1GB)))
		arm_maxlimit = true;
	else
		arm_maxlimit = false;

	if (((chip_id >> GROUP_FUSE_BIT) & GROUP_FUSE_MASK)) {
		original_speed_group = ((chip_id >> ORIGINAL_BIT) & ORIGINAL_MASK);
		modified_speed_group = ((chip_id >> MODIFIED_BIT) & MODIFIED_MASK);
		asv_info->speed_value = original_speed_group - modified_speed_group;
		if (asv_info->speed_value < 0) {
			pr_err("Incorrect speed group information \n");
			return -EINVAL;
		}
		asv_info->efuse_status = true;
		pr_info("Use Fusing Speed Group %d\n", asv_info->speed_value);
	} else {
		asv_info->ids_value = (chip_id >> EXYNOS4415_IDS_OFFSET) & EXYNOS4415_IDS_MASK;
		asv_info->hpm_value = (chip_id >> EXYNOS4415_HPM_OFFSET) & EXYNOS4415_HPM_MASK;
		pr_info("IDS : %d\nHPM : %d\n", asv_info->ids_value, asv_info->hpm_value);
	}

	pr_debug("PackageOption: %s", ((chip_id >> PKGOPT_OFFSET) & PKGOPT_MASK) ? "SCP" : "POP");

	asv_info->regist_asv_member = exynos4415_regist_asv_member;

	/* Read ID_ARM lock value and offset value */
	asv_volt_offset[ID_ARM][0] = ((operation >> ARM_LOCK_BIT) & ARM_LOCK_MASK);
	/* Read ID_INT lock value and offset value */
	asv_volt_offset[ID_INT][0] = ((chip_id >> INT_LOCK_BIT) & INT_LOCK_MASK);
	/* Read ID_MIF lock value and offset value */
	asv_volt_offset[ID_MIF][0] = ((operation >> MIF_LOCK_BIT) & MIF_LOCK_MASK);
	/* Read ID_G3D lock value and offset value */
	asv_volt_offset[ID_G3D][0] = ((chip_id >> G3D_LOCK_BIT) & G3D_LOCK_MASK);

	pr_info("voltage locking info :\narm: %d\nint: %d\nmif: %d\ng3d: %d\n",
		 asv_volt_offset[ID_ARM][0],  asv_volt_offset[ID_INT][0],
		 asv_volt_offset[ID_MIF][0],  asv_volt_offset[ID_G3D][0]);

	register_syscore_ops(&exynos4415_asv_syscore_ops);

	return 0;
}
