/*
 * Copyright (C) 2013 Samsung Electronics
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm80x.h>
#include "board-lt02-regulators.h"

static struct regulator_consumer_supply regulator_supplies_lt02_r0_0[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REGULATOR_SUPPLY("vcc_main", NULL),
	[PM800_ID_BUCK2] = REGULATOR_SUPPLY("v_buck2", NULL),
	[PM800_ID_BUCK3] = REGULATOR_SUPPLY("v_buck3", NULL),
	[PM800_ID_BUCK4] = REGULATOR_SUPPLY("v_rf_vdd", NULL),
	[PM800_ID_BUCK5] = REGULATOR_SUPPLY("v_cam_c", NULL),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REGULATOR_SUPPLY("v_ldo1", NULL),
	[PM800_ID_LDO2]  = REGULATOR_SUPPLY("v_micbias", NULL),
	[PM800_ID_LDO3]  = REGULATOR_SUPPLY("v_analog_2v8", NULL),
	[PM800_ID_LDO4]  = REGULATOR_SUPPLY("v_usim1", NULL),
	[PM800_ID_LDO5]  = REGULATOR_SUPPLY("v_usb_3v1", NULL),
	[PM800_ID_LDO6]  = REGULATOR_SUPPLY("vled_ic", NULL),				/* VLED_IC_1.9V for IRDA */
	[PM800_ID_LDO7]  = REGULATOR_SUPPLY("v_vramp_2v8"/*V_LDO7*/, NULL),

	[PM800_ID_LDO8]  = REGULATOR_SUPPLY("v_ldo8", NULL),				/* V_3M_A2.8V for Main CAM */
	[PM800_ID_LDO9]  = REGULATOR_SUPPLY("v_wib_3v3", NULL),
	[PM800_ID_LDO10] = REGULATOR_SUPPLY("v_proxy_2v85", NULL),
	[PM800_ID_LDO11] = REGULATOR_SUPPLY("v_cam_io", NULL),				/* V_CAM_IO_1.8V for CAM */
	[PM800_ID_LDO12] = REGULATOR_SUPPLY("vqmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO13] = REGULATOR_SUPPLY("vmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO14] = REGULATOR_SUPPLY("v_ldo14_3v", NULL),			/* VREG_L16_LVDS_3P3 for LVDS */
	[PM800_ID_LDO15] = REGULATOR_SUPPLY("v_proxy_led_3v3", NULL),
	[PM800_ID_LDO16] = REGULATOR_SUPPLY("v_lvds_1v2", NULL),
	[PM800_ID_LDO17] = REGULATOR_SUPPLY("v_lvds_1v8", NULL),
	[PM800_ID_LDO18] = REGULATOR_SUPPLY("v_ldo18", NULL),
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("v_gps_1v8", NULL),

	/* below 4 IDs are fake ids, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REGULATOR_SUPPLY("vcc_main_ap_active", NULL),
	[PM800_ID_BUCK1_AP_LPM] = REGULATOR_SUPPLY("vcc_main_ap_lpm", NULL),
	[PM800_ID_BUCK1_APSUB_IDLE] = REGULATOR_SUPPLY("vcc_main_apsub_idle", NULL),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REGULATOR_SUPPLY("vcc_main_apsub_sleep", NULL),
};

static struct regulator_consumer_supply regulator_supplies_lt02_r0_1[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REGULATOR_SUPPLY("vcc_main", NULL),
	[PM800_ID_BUCK2] = REGULATOR_SUPPLY("v_buck2", NULL),
	[PM800_ID_BUCK3] = REGULATOR_SUPPLY("v_buck3", NULL),
	[PM800_ID_BUCK4] = REGULATOR_SUPPLY("v_rf_vdd", NULL),
	[PM800_ID_BUCK5] = REGULATOR_SUPPLY("v_cam_c", NULL),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REGULATOR_SUPPLY("v_ldo1", NULL),
	[PM800_ID_LDO2]  = REGULATOR_SUPPLY("v_micbias", NULL),
	[PM800_ID_LDO3]  = REGULATOR_SUPPLY("v_analog_2v8", NULL),
	[PM800_ID_LDO4]  = REGULATOR_SUPPLY("v_usim1", NULL),
	[PM800_ID_LDO5]  = REGULATOR_SUPPLY("v_usb_3v1", NULL),
	[PM800_ID_LDO6]  = REGULATOR_SUPPLY("vled_ic", NULL),				/* VLED_IC_1.9V for IRDA */
	[PM800_ID_LDO7]  = REGULATOR_SUPPLY("v_vramp_2v8"/*V_LDO7*/, NULL),

	[PM800_ID_LDO8]  = REGULATOR_SUPPLY("v_ldo8", NULL),				/* V_3M_A2.8V for Main CAM */
	[PM800_ID_LDO9]  = REGULATOR_SUPPLY("v_wib_3v3", NULL),
	[PM800_ID_LDO10] = REGULATOR_SUPPLY("v_proxy_2v85", NULL),
	[PM800_ID_LDO11] = REGULATOR_SUPPLY("v_cam_io", NULL),				/* V_CAM_IO_1.8V for CAM */
	[PM800_ID_LDO12] = REGULATOR_SUPPLY("vqmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO13] = REGULATOR_SUPPLY("vmmc", "sdhci-pxav3.0"),
	[PM800_ID_LDO14] = REGULATOR_SUPPLY("v_ldo14_3v", NULL),			/* VREG_L16_LVDS_3P3 for LVDS */
	[PM800_ID_LDO15] = REGULATOR_SUPPLY("v_motor_3v", NULL),			/* V_MOTOR_3.3V */
	[PM800_ID_LDO16] = REGULATOR_SUPPLY("v_lvds_1v2", NULL),
	[PM800_ID_LDO17] = REGULATOR_SUPPLY("v_lvds_1v8", NULL),
	[PM800_ID_LDO18] = REGULATOR_SUPPLY("v_ldo18", NULL),
	[PM800_ID_LDO19] = REGULATOR_SUPPLY("v_gps_1v8", NULL),

	/* below 4 IDs are fake ids, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REGULATOR_SUPPLY("vcc_main_ap_active", NULL),
	[PM800_ID_BUCK1_AP_LPM] = REGULATOR_SUPPLY("vcc_main_ap_lpm", NULL),
	[PM800_ID_BUCK1_APSUB_IDLE] = REGULATOR_SUPPLY("vcc_main_apsub_idle", NULL),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REGULATOR_SUPPLY("vcc_main_apsub_sleep", NULL),
};

static int regulator_index[] = {
	PM800_ID_BUCK1,
	PM800_ID_BUCK2,
	PM800_ID_BUCK3,
	PM800_ID_BUCK4,
	PM800_ID_BUCK5,
	PM800_ID_LDO1,
	PM800_ID_LDO2,
	PM800_ID_LDO3,
	PM800_ID_LDO4,
	PM800_ID_LDO5,
	PM800_ID_LDO6,
	PM800_ID_LDO7,
	PM800_ID_LDO8,
	PM800_ID_LDO9,
	PM800_ID_LDO10,
	PM800_ID_LDO11,
	PM800_ID_LDO12,
	PM800_ID_LDO13,
	PM800_ID_LDO14,
	PM800_ID_LDO15,
	PM800_ID_LDO16,
	PM800_ID_LDO17,
	PM800_ID_LDO18,
	PM800_ID_LDO19,

	/* below 4 ids are fake id, they are only used in new dvc */
	PM800_ID_BUCK1_AP_ACTIVE,
	PM800_ID_BUCK1_AP_LPM,
	PM800_ID_BUCK1_APSUB_IDLE,
	PM800_ID_BUCK1_APSUB_SLEEP,
};

#define REG_INIT_R0_0(_name, _min, _max, _always, _boot)	\
{								\
	.constraints = {					\
		.name		= __stringify(_name),		\
		.min_uV		= _min,				\
		.max_uV		= _max,				\
		.always_on	= _always,			\
		.boot_on	= _boot,			\
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE	\
				| REGULATOR_CHANGE_STATUS,	\
	},							\
	.num_consumer_supplies	= 1,				\
	.consumer_supplies	= &regulator_supplies_lt02_r0_0[PM800_ID_##_name], \
	.driver_data = &regulator_index[PM800_ID_##_name],	\
}

#define REG_INIT_R0_1(_name, _min, _max, _always, _boot)	\
{								\
	.constraints = {					\
		.name		= __stringify(_name),		\
		.min_uV		= _min,				\
		.max_uV		= _max,				\
		.always_on	= _always,			\
		.boot_on	= _boot,			\
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE	\
				| REGULATOR_CHANGE_STATUS,	\
	},							\
	.num_consumer_supplies	= 1,				\
	.consumer_supplies	= &regulator_supplies_lt02_r0_1[PM800_ID_##_name], \
	.driver_data = &regulator_index[PM800_ID_##_name],	\
}

struct regulator_init_data pm800_regulator_data_lt02_r0_0[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REG_INIT_R0_0(BUCK1,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK2] = REG_INIT_R0_0(BUCK2,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK3] = REG_INIT_R0_0(BUCK3,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK4] = REG_INIT_R0_0(BUCK4,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK5] = REG_INIT_R0_0(BUCK5,  600000, 3950000, 0, 0),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REG_INIT_R0_0(LDO1,   600000, 1500000, 0, 0),
	[PM800_ID_LDO2]  = REG_INIT_R0_0(LDO2,  1700000, 2800000, 0, 1),
	[PM800_ID_LDO3]  = REG_INIT_R0_0(LDO3,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO4]  = REG_INIT_R0_0(LDO4,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO5]  = REG_INIT_R0_0(LDO5,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO6]  = REG_INIT_R0_0(LDO6,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO7]  = REG_INIT_R0_0(LDO7,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO8]  = REG_INIT_R0_0(LDO8,  1800000, 3300000, 0, 0),
	[PM800_ID_LDO9]  = REG_INIT_R0_0(LDO9,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO10] = REG_INIT_R0_0(LDO10, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO11] = REG_INIT_R0_0(LDO11, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO12] = REG_INIT_R0_0(LDO12, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO13] = REG_INIT_R0_0(LDO13, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO14] = REG_INIT_R0_0(LDO14, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO15] = REG_INIT_R0_0(LDO15, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO16] = REG_INIT_R0_0(LDO16, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO17] = REG_INIT_R0_0(LDO17, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO18] = REG_INIT_R0_0(LDO18, 1700000, 3300000, 1, 1),
	[PM800_ID_LDO19] = REG_INIT_R0_0(LDO19, 1700000, 3300000, 1, 1),

	/* below 4 items are fake, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REG_INIT_R0_0(BUCK1_AP_ACTIVE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_AP_LPM] = REG_INIT_R0_0(BUCK1_AP_LPM,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_IDLE] = REG_INIT_R0_0(BUCK1_APSUB_IDLE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REG_INIT_R0_0(BUCK1_APSUB_SLEEP,  1000, 10000, 1, 1),
};

struct regulator_init_data pm800_regulator_data_lt02_r0_1[] = {
	/* BUCK power supplies: BUCK[1..5] */
	[PM800_ID_BUCK1] = REG_INIT_R0_1(BUCK1,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK2] = REG_INIT_R0_1(BUCK2,  600000, 3950000, 1, 1),
	[PM800_ID_BUCK3] = REG_INIT_R0_1(BUCK3,  600000, 3950000, 1, 1),
#ifdef CONFIG_WIFIONLY_BOARD
	[PM800_ID_BUCK4] = REG_INIT_R0_1(BUCK4,  600000, 3950000, 0, 0),
#else
	[PM800_ID_BUCK4] = REG_INIT_R0_1(BUCK4,  600000, 3950000, 1, 1),
#endif
	[PM800_ID_BUCK5] = REG_INIT_R0_1(BUCK5,  600000, 3950000, 0, 0),
	/* LDO power supplies: LDO[1..19] */
	[PM800_ID_LDO1]  = REG_INIT_R0_1(LDO1,   600000, 1500000, 0, 0),
	[PM800_ID_LDO2]  = REG_INIT_R0_1(LDO2,  1700000, 2800000, 0, 1),
	[PM800_ID_LDO3]  = REG_INIT_R0_1(LDO3,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO4]  = REG_INIT_R0_1(LDO4,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO5]  = REG_INIT_R0_1(LDO5,  1200000, 3300000, 1, 1),
	[PM800_ID_LDO6]  = REG_INIT_R0_1(LDO6,  1200000, 3300000, 0, 0),
#ifdef CONFIG_WIFIONLY_BOARD
	[PM800_ID_LDO7]  = REG_INIT_R0_1(LDO7,  1200000, 3300000, 0, 0),
#else
	[PM800_ID_LDO7]  = REG_INIT_R0_1(LDO7,  1200000, 3300000, 1, 1),
#endif
	[PM800_ID_LDO8]  = REG_INIT_R0_1(LDO8,  1800000, 3300000, 0, 0),
	[PM800_ID_LDO9]  = REG_INIT_R0_1(LDO9,  1200000, 3300000, 0, 0),
	[PM800_ID_LDO10] = REG_INIT_R0_1(LDO10, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO11] = REG_INIT_R0_1(LDO11, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO12] = REG_INIT_R0_1(LDO12, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO13] = REG_INIT_R0_1(LDO13, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO14] = REG_INIT_R0_1(LDO14, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO15] = REG_INIT_R0_1(LDO15, 1200000, 3300000, 0, 0),
	[PM800_ID_LDO16] = REG_INIT_R0_1(LDO16, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO17] = REG_INIT_R0_1(LDO17, 1200000, 3300000, 0, 1),
	[PM800_ID_LDO18] = REG_INIT_R0_1(LDO18, 1700000, 3300000, 1, 1),
	[PM800_ID_LDO19] = REG_INIT_R0_1(LDO19, 1700000, 3300000, 1, 1),

	/* below 4 items are fake, they are only used in new dvc */
	[PM800_ID_BUCK1_AP_ACTIVE] = REG_INIT_R0_1(BUCK1_AP_ACTIVE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_AP_LPM] = REG_INIT_R0_1(BUCK1_AP_LPM,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_IDLE] = REG_INIT_R0_1(BUCK1_APSUB_IDLE,  1000, 10000, 1, 1),
	[PM800_ID_BUCK1_APSUB_SLEEP] = REG_INIT_R0_1(BUCK1_APSUB_SLEEP,  1000, 10000, 1, 1),
};

unsigned int pm800_regulators_num = ARRAY_SIZE(regulator_index);
