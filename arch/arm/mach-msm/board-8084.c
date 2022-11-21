/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/i2c.h>
#include <linux/clk/msm-clk-provider.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

#ifdef CONFIG_PROC_AVC
#include <linux/proc_avc.h>
#endif

#ifdef CONFIG_REGULATOR_MAX77826
#include <linux/regulator/max77826.h>
#endif

#ifdef CONFIG_REGULATOR_S2MPB01
#include <linux/regulator/s2mpb01.h>
#endif

#ifdef CONFIG_LEDS_MAX77803
#include <linux/leds-max77803.h>
#endif

#ifdef CONFIG_LEDS_MAX77804K
#include <linux/leds-max77804k.h>
#endif

#ifdef CONFIG_LEDS_MAX77828
#include <linux/leds-max77828.h>
#include <linux/leds.h>
#endif

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#include <mach/apq8084-thermistor.h>
#endif

#ifdef CONFIG_LEDS_MAX77803
struct max77803_led_platform_data max77803_led_pdata = {
    .num_leds = 2,

    .leds[0].name = "leds-sec1",
    .leds[0].id = MAX77803_FLASH_LED_1,
    .leds[0].timer = MAX77803_FLASH_TIME_187P5MS,
    .leds[0].timer_mode = MAX77803_TIMER_MODE_MAX_TIMER,
    .leds[0].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
    .leds[0].brightness = 0x3D,

    .leds[1].name = "torch-sec1",
    .leds[1].id = MAX77803_TORCH_LED_1,
    .leds[1].cntrl_mode = MAX77803_LED_CTRL_BY_FLASHSTB,
    .leds[1].brightness = 0x06,
};
#endif

#ifdef CONFIG_LEDS_MAX77804K
struct max77804k_led_platform_data max77804k_led_pdata = {
    .num_leds = 2,

    .leds[0].name = "leds-sec1",
    .leds[0].id = MAX77804K_FLASH_LED_1,
    .leds[0].timer = MAX77804K_FLASH_TIME_187P5MS,
    .leds[0].timer_mode = MAX77804K_TIMER_MODE_MAX_TIMER,
    .leds[0].cntrl_mode = MAX77804K_LED_CTRL_BY_FLASHSTB,
    .leds[0].brightness = 0x3D,

    .leds[1].name = "torch-sec1",
    .leds[1].id = MAX77804K_TORCH_LED_1,
    .leds[1].cntrl_mode = MAX77804K_LED_CTRL_BY_FLASHSTB,
    .leds[1].brightness = 0x06,
};
#endif

#ifdef CONFIG_LEDS_MAX77828
struct max77828_led_platform_data max77828_led_pdata = {
    .num_leds = 2,

    .leds[0].name = "leds-sec1",
    .leds[0].default_trigger = "flash",
    .leds[0].id = MAX77828_FLASH,
    .leds[0].brightness = MAX77828_FLASH_IOUT,

    .leds[1].name = "torch-sec1",
    .leds[1].default_trigger = "torch",
    .leds[1].id = MAX77828_TORCH,
    .leds[1].brightness = MAX77828_TORCH_IOUT,
};
#endif

#ifdef CONFIG_REGULATOR_MAX77826
#define MAX77826_I2C_BUS_ID	13

#define MAX77826_VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply max77826_vreg_consumers_##_id[]

#define MAX77826_VREG_INIT(_id, _min_uV, _max_uV, _always_on) \
	static struct regulator_init_data max77826_##_id##_init_data = { \
		.constraints = { \
			.min_uV			= _min_uV, \
			.max_uV			= _max_uV, \
			.apply_uV		= 1, \
			.always_on		= _always_on, \
			.valid_modes_mask = REGULATOR_MODE_NORMAL, \
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | \
							REGULATOR_CHANGE_STATUS, \
		}, \
		.num_consumer_supplies = ARRAY_SIZE(max77826_vreg_consumers_##_id), \
		.consumer_supplies = max77826_vreg_consumers_##_id, \
	}

#define MAX77826_VREG_INIT_DATA(_id) \
	(struct regulator_init_data *)&max77826_##_id##_init_data

MAX77826_VREG_CONSUMERS(LDO1) = {
	REGULATOR_SUPPLY("max77826_ldo1",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO2) = {
	REGULATOR_SUPPLY("max77826_ldo2",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO3) = {
	REGULATOR_SUPPLY("max77826_ldo3",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO4) = {
	REGULATOR_SUPPLY("max77826_ldo4",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO5) = {
	REGULATOR_SUPPLY("max77826_ldo5",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO6) = {
	REGULATOR_SUPPLY("max77826_ldo6",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO7) = {
	REGULATOR_SUPPLY("max77826_ldo7",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO8) = {
	REGULATOR_SUPPLY("max77826_ldo8",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO9) = {
	REGULATOR_SUPPLY("max77826_ldo9",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO10) = {
	REGULATOR_SUPPLY("max77826_ldo10",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO11) = {
	REGULATOR_SUPPLY("max77826_ldo11",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO12) = {
	REGULATOR_SUPPLY("max77826_ldo12",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO13) = {
	REGULATOR_SUPPLY("max77826_ldo13",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO14) = {
	REGULATOR_SUPPLY("max77826_ldo14",	NULL),
};

MAX77826_VREG_CONSUMERS(LDO15) = {
	REGULATOR_SUPPLY("max77826_ldo15",	NULL),
};

MAX77826_VREG_CONSUMERS(BUCK1) = {
	REGULATOR_SUPPLY("max77826_buck1",	NULL),
};

MAX77826_VREG_CONSUMERS(BUCK2) = {
	REGULATOR_SUPPLY("max77826_buck2",	NULL),
};

MAX77826_VREG_INIT(LDO1, 1000000, 1200000, 0);
MAX77826_VREG_INIT(LDO2, 1000000, 1200000, 0);
MAX77826_VREG_INIT(LDO3, 1000000, 1200000, 0);
MAX77826_VREG_INIT(LDO4, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO5, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO6, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO7, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO8, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO9, 1800000, 1800000, 0);
MAX77826_VREG_INIT(LDO10, 2800000, 3000000, 0);
MAX77826_VREG_INIT(LDO11, 2700000, 2950000, 0);
MAX77826_VREG_INIT(LDO12, 3300000, 3300000, 0);
MAX77826_VREG_INIT(LDO13, 1800000, 3300000, 0);
MAX77826_VREG_INIT(LDO14, 1800000, 3300000, 0);
MAX77826_VREG_INIT(LDO15, 1800000, 3300000, 0);
MAX77826_VREG_INIT(BUCK1, 1225000, 1225000, 0);
MAX77826_VREG_INIT(BUCK2, 3400000, 3400000, 0);

static struct max77826_regulator_subdev max77826_regulators[] = {
	{MAX77826_LDO1, MAX77826_VREG_INIT_DATA(LDO1)},
	{MAX77826_LDO2, MAX77826_VREG_INIT_DATA(LDO2)},
	{MAX77826_LDO3, MAX77826_VREG_INIT_DATA(LDO3)},
	{MAX77826_LDO4, MAX77826_VREG_INIT_DATA(LDO4)},
	{MAX77826_LDO5, MAX77826_VREG_INIT_DATA(LDO5)},
	{MAX77826_LDO6, MAX77826_VREG_INIT_DATA(LDO6)},
	{MAX77826_LDO7, MAX77826_VREG_INIT_DATA(LDO7)},
	{MAX77826_LDO8, MAX77826_VREG_INIT_DATA(LDO8)},
	{MAX77826_LDO9, MAX77826_VREG_INIT_DATA(LDO9)},
	{MAX77826_LDO10, MAX77826_VREG_INIT_DATA(LDO10)},
	{MAX77826_LDO11, MAX77826_VREG_INIT_DATA(LDO11)},
	{MAX77826_LDO12, MAX77826_VREG_INIT_DATA(LDO12)},
	{MAX77826_LDO13, MAX77826_VREG_INIT_DATA(LDO13)},
	{MAX77826_LDO14, MAX77826_VREG_INIT_DATA(LDO14)},
	{MAX77826_LDO15, MAX77826_VREG_INIT_DATA(LDO15)},
	{MAX77826_BUCK1, MAX77826_VREG_INIT_DATA(BUCK1)},
	{MAX77826_BUCK2, MAX77826_VREG_INIT_DATA(BUCK2)},
};

static struct max77826_platform_data max77826_pmic_pdata = {
	.name = "max77826",
	.num_regulators = ARRAY_SIZE(max77826_regulators),
	.regulators = max77826_regulators,
};

static struct i2c_board_info max77826_pmic_info[] __initdata = {
	{
		I2C_BOARD_INFO("max77826", 0x60),
		.platform_data = &max77826_pmic_pdata,
	},
};
#endif /* CONFIG_REGULATOR_MAX77826 */

#ifdef CONFIG_REGULATOR_S2MPB01
#define S2MPB01_I2C_BUS_ID	14

#define S2MPB01_VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply s2mpb01_vreg_consumers_##_id[]

#define S2MPB01_VREG_INIT(_id, _min_uV, _max_uV, _always_on) \
	static struct regulator_init_data s2mpb01_##_id##_init_data = { \
		.constraints = { \
			.min_uV			= _min_uV, \
			.max_uV			= _max_uV, \
			.apply_uV		= 1, \
			.always_on		= _always_on, \
			.valid_modes_mask = REGULATOR_MODE_NORMAL, \
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | \
							REGULATOR_CHANGE_STATUS, \
		}, \
		.num_consumer_supplies = ARRAY_SIZE(s2mpb01_vreg_consumers_##_id), \
		.consumer_supplies = s2mpb01_vreg_consumers_##_id, \
	}

#define S2MPB01_VREG_INIT_DATA(_id) \
	(struct regulator_init_data *)&s2mpb01_##_id##_init_data

S2MPB01_VREG_CONSUMERS(LDO1) = {
	REGULATOR_SUPPLY("s2mpb01_ldo1",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO2) = {
	REGULATOR_SUPPLY("s2mpb01_ldo2",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO3) = {
	REGULATOR_SUPPLY("s2mpb01_ldo3",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO4) = {
	REGULATOR_SUPPLY("s2mpb01_ldo4",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO5) = {
	REGULATOR_SUPPLY("s2mpb01_ldo5",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO6) = {
	REGULATOR_SUPPLY("s2mpb01_ldo6",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO7) = {
	REGULATOR_SUPPLY("s2mpb01_ldo7",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO8) = {
	REGULATOR_SUPPLY("s2mpb01_ldo8",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO9) = {
	REGULATOR_SUPPLY("s2mpb01_ldo9",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO10) = {
	REGULATOR_SUPPLY("s2mpb01_ldo10",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO11) = {
	REGULATOR_SUPPLY("s2mpb01_ldo11",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO12) = {
	REGULATOR_SUPPLY("s2mpb01_ldo12",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO13) = {
	REGULATOR_SUPPLY("s2mpb01_ldo13",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO14) = {
	REGULATOR_SUPPLY("s2mpb01_ldo14",	NULL),
};

S2MPB01_VREG_CONSUMERS(LDO15) = {
	REGULATOR_SUPPLY("s2mpb01_ldo15",	NULL),
};

S2MPB01_VREG_CONSUMERS(BUCK1) = {
	REGULATOR_SUPPLY("s2mpb01_buck1",	NULL),
};

S2MPB01_VREG_CONSUMERS(BUCK2) = {
	REGULATOR_SUPPLY("s2mpb01_buck2",	NULL),
};

S2MPB01_VREG_INIT(LDO1, 1000000, 1200000, 0);
S2MPB01_VREG_INIT(LDO2, 1000000, 1200000, 0);
S2MPB01_VREG_INIT(LDO3, 1000000, 1200000, 0);
S2MPB01_VREG_INIT(LDO4, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO5, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO6, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO7, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO8, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO9, 1800000, 1800000, 0);
S2MPB01_VREG_INIT(LDO10, 2800000, 3000000, 0);
S2MPB01_VREG_INIT(LDO11, 2700000, 2950000, 0);
S2MPB01_VREG_INIT(LDO12, 3300000, 3300000, 0);
S2MPB01_VREG_INIT(LDO13, 1800000, 3300000, 0);
S2MPB01_VREG_INIT(LDO14, 1800000, 3300000, 0);
S2MPB01_VREG_INIT(LDO15, 1800000, 3300000, 0);
S2MPB01_VREG_INIT(BUCK1, 1225000, 1225000, 0);
S2MPB01_VREG_INIT(BUCK2, 3400000, 3400000, 0);

static struct s2mpb01_regulator_subdev s2mpb01_regulators[] = {
	{S2MPB01_LDO1, S2MPB01_VREG_INIT_DATA(LDO1)},
	{S2MPB01_LDO2, S2MPB01_VREG_INIT_DATA(LDO2)},
	{S2MPB01_LDO3, S2MPB01_VREG_INIT_DATA(LDO3)},
	{S2MPB01_LDO4, S2MPB01_VREG_INIT_DATA(LDO4)},
	{S2MPB01_LDO5, S2MPB01_VREG_INIT_DATA(LDO5)},
	{S2MPB01_LDO6, S2MPB01_VREG_INIT_DATA(LDO6)},
	{S2MPB01_LDO7, S2MPB01_VREG_INIT_DATA(LDO7)},
	{S2MPB01_LDO8, S2MPB01_VREG_INIT_DATA(LDO8)},
	{S2MPB01_LDO9, S2MPB01_VREG_INIT_DATA(LDO9)},
	{S2MPB01_LDO10, S2MPB01_VREG_INIT_DATA(LDO10)},
	{S2MPB01_LDO11, S2MPB01_VREG_INIT_DATA(LDO11)},
	{S2MPB01_LDO12, S2MPB01_VREG_INIT_DATA(LDO12)},
	{S2MPB01_LDO13, S2MPB01_VREG_INIT_DATA(LDO13)},
	{S2MPB01_LDO14, S2MPB01_VREG_INIT_DATA(LDO14)},
	{S2MPB01_LDO15, S2MPB01_VREG_INIT_DATA(LDO15)},
	{S2MPB01_BUCK1, S2MPB01_VREG_INIT_DATA(BUCK1)},
	{S2MPB01_BUCK2, S2MPB01_VREG_INIT_DATA(BUCK2)},
};

static struct s2mpb01_platform_data s2mpb01_pmic_pdata = {
	.name = "s2mpb01",
	.num_regulators = ARRAY_SIZE(s2mpb01_regulators),
	.regulators = s2mpb01_regulators,
};

static struct i2c_board_info s2mpb01_pmic_info[] __initdata = {
	{
		I2C_BOARD_INFO("s2mpb01", 0x59),
		.platform_data = &s2mpb01_pmic_pdata,
	},
};
#endif /* CONFIG_REGULATOR_S2MPB01 */

static struct of_dev_auxdata apq8084_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qca,qca1530", 0x00000000, "qca1530.1", NULL),
	OF_DEV_AUXDATA("qcom,ufshc", 0xFC594000, "msm_ufs.1", NULL),
	OF_DEV_AUXDATA("qcom,xhci-msm-hsic", 0xf9c00000, "msm_hsic_host", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC520000, "msm_pcie.1", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC528000, "msm_pcie.2", NULL),
	{}
};

void __init apq8084_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}

static void __init apq8084_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}

static struct platform_device *common_devices[] __initdata = {
#ifdef CONFIG_SEC_THERMISTOR
	&sec_device_thermistor,
#endif
};

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static void samsung_sys_class_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return;
	}
};

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init apq8084_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_pm_sleep_status_init();
	rpm_smd_regulator_driver_init();
	msm_spm_device_init();
	krait_power_init();
	if (of_board_is_rumi())
		msm_clock_init(&apq8084_rumi_clock_init_data);
	else
		msm_clock_init(&apq8084_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

static void __init apq8084_map_io(void)
{
	msm_map_8084_io();
}

void __init apq8084_init(void)
{
	struct of_dev_auxdata *adata = apq8084_auxdata_lookup;

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif

#ifdef CONFIG_PROC_AVC
	sec_avc_log_init();
#endif
	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.  apq8084_init_gpiomux needs socinfo so
	 * call socinfo_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	samsung_sys_class_init();
	apq8084_init_gpiomux();
	apq8084_add_drivers();

	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));

#ifdef CONFIG_REGULATOR_MAX77826
	i2c_register_board_info(MAX77826_I2C_BUS_ID, max77826_pmic_info,
		ARRAY_SIZE(max77826_pmic_info));
#endif

#ifdef CONFIG_REGULATOR_S2MPB01
	i2c_register_board_info(S2MPB01_I2C_BUS_ID, s2mpb01_pmic_info,
		ARRAY_SIZE(s2mpb01_pmic_info));
#endif

}

void __init apq8084_init_very_early(void)
{
	apq8084_early_memory();
}

static const char *apq8084_dt_match[] __initconst = {
	"qcom,apq8084",
	NULL
};

DT_MACHINE_START(APQ8084_DT, "Qualcomm APQ8084")
	.map_io			= apq8084_map_io,
	.init_machine		= apq8084_init,
	.dt_compat		= apq8084_dt_match,
	.reserve		= apq8084_reserve,
	.init_very_early	= apq8084_init_very_early,
	.restart		= msm_restart,
	.smp			= &msm8974_smp_ops,
MACHINE_END
