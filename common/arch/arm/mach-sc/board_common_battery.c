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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/sprd_battery_common.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/board.h>
#include <mach/irqs.h>

//this is default config file of battery, you can make a special file for a new board.
#ifndef CONFIG_OF

struct sprdbat_table_data bat_temp_table[] = {
	{1084, -250},
	{1072, -200},
	{1055, -150},
	{1033, -100},
	{1008, -50},
	{977, 0},
	{938, 50},
	{897, 100},
	{850, 150},
	{798, 200},
	{742, 250},
	{686, 300},
	{628, 350},
	{571, 400},
	{513, 450},
	{459, 500},
	{410, 550},
	{362, 600},
	{318, 650},
};

struct sprdbat_table_data bat_ocv_table[] = {
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	{4160, 100}
#else
	{4180, 100}
#endif
	,
	{4100, 95}
	,
	{4000, 80}
	,
	{3950, 70}
	,
	{3900, 60}
	,
	{3850, 50}
	,
	{3810, 40}
	,
	{3775, 30}
	,
	{3740, 20}
	,
	{3720, 15}
	,
	{3680, 5}
	,
	{3400, 0}
	,
};

#if 0				//this is a demo,Be used when .fgu_mode = 0, the table must be according to battery characteristic.
struct sprdbat_table_data scx35_ref_phone_soc_table[] = {
	{4175, 100}
	,
	{4130, 95}
	,
	{4087, 90}
	,
	{4052, 85}
	,
	{3990, 80}
	,
	{3970, 75}
	,
	{3943, 70}
	,
	{3915, 65}
	,
	{3883, 60}
	,
	{3843, 55}
	,
	{3820, 50}
	,
	{3804, 45}
	,
	{3792, 40}
	,
	{3783, 35}
	,
	{3776, 30}
	,
	{3770, 25}
	,
	{3754, 20}
	,
	{3736, 15}
	,
	{3708, 10}
	,
	{3691, 5}
	,
	{3400, 0}
	,
};

struct sprdbat_table_data scx15_ref_phone_soc_table[] = {

	{4175, 100}
	,
	{4130, 95}
	,
	{4086, 90}
	,
	{4045, 85}
	,
	{4007, 80}
	,
	{3974, 75}
	,
	{3943, 70}
	,
	{3916, 65}
	,
	{3886, 60}
	,
	{3850, 55}
	,
	{3824, 50}
	,
	{3806, 45}
	,
	{3793, 40}
	,
	{3783, 35}
	,
	{3775, 30}
	,
	{3766, 25}
	,
	{3749, 20}
	,
	{3733, 15}
	,
	{3712, 10}
	,
	{3683, 5}
	,
	{3400, 0}
	,
};

#endif

struct sprd_battery_platform_data sprdbat_pdata = {
	.chg_end_vol_h = 4225,
	.chg_end_vol_pure = 4200,
	.chg_end_vol_l = 4190,
	.chg_bat_safety_vol = 4280,
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	.rechg_vol = 4110,
#else
	.rechg_vol = 4131,
#endif
	.adp_cdp_cur = 700,
	.adp_dcp_cur = 700,
	.adp_sdp_cur = 500,
	.ovp_stop = 6500,
	.ovp_restart = 5800,
	.chg_timeout = (6 * 60 * 60),
	.chgtimeout_show_full = 0,
	.chg_rechg_timeout = (90 * 60),
	.chg_cv_timeout = (60 * 60),	//Be used in pulse charging
	.chg_eoc_level = 3,	//Be used in pulse charging
	.cccv_default = 0,
	.chg_end_cur = 80,
	.otp_high_stop = 600,	//60C
	.otp_high_restart = 550,	//55C
	.otp_low_stop = -50,	//-5C
	.otp_low_restart = 0,
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	.chg_polling_time = 30,
#else
	.chg_polling_time = 60,
#endif
	.chg_polling_time_fast = 1,
	.bat_polling_time = (15),
	.bat_polling_time_fast = (15),
	.cap_one_per_time = 30,
	.cap_valid_range_poweron = 50,

	.temp_support = 0,
	.temp_adc_ch = 3,
	.temp_adc_scale = 1,
	.temp_adc_sample_cnt = 15,
	.temp_table_mode = 1,
	.temp_tab = bat_temp_table,
	.temp_tab_size =
	    sizeof(bat_temp_table) / sizeof(struct sprdbat_table_data),

	.gpio_vchg_detect = EIC_CHARGER_DETECT,
	.gpio_cv_state = EIC_CHG_CV_STATE,
	.gpio_vchg_ovi = EIC_VCHG_OVI,
	.irq_chg_timer = IRQ_APTMR3_INT,
	.irq_fgu = IRQ_ANA_FGU_INT,
	.chg_reg_base = (ANA_REG_GLB_CHGR_CTRL0),
	.fgu_reg_base = (ANA_FPU_INT_BASE),

	.fgu_mode = 1,
	.alm_soc = 5,
	.alm_vol = 3600,
	.soft_vbat_uvlo = 3100,
	.rint = 250,
	.cnom = 1500,		//1500mAH
	.rsense_real = 215,	//21.5mOHM
	.rsense_spec = 200,
	.relax_current = 50,
	.fgu_cal_ajust = 0,
	.ocv_tab = bat_ocv_table,
	.ocv_tab_size =
	    sizeof(bat_ocv_table) / sizeof(struct sprdbat_table_data),
};

static struct resource sprd_battery_resources[] = {
	[0] = {
	       .start = EIC_CHARGER_DETECT,
	       .end = EIC_CHARGER_DETECT,
	       .name = "charger_detect",
	       .flags = IORESOURCE_IO,
	       },
	[1] = {
	       .start = EIC_CHG_CV_STATE,
	       .end = EIC_CHG_CV_STATE,
	       .name = "chg_cv_state",
	       .flags = IORESOURCE_IO,
	       },
	[2] = {
	       .start = EIC_VCHG_OVI,
	       .end = EIC_VCHG_OVI,
	       .name = "vchg_ovi",
	       .flags = IORESOURCE_IO,
	       },
#ifdef CONFIG_SHARK_PAD_HW_V102
	[3] = {
	       .start = GPIO_EXT_CHG_DONE,
	       .end = GPIO_EXT_CHG_DONE,
	       .name = "ext_charge_done",
	       .flags = IORESOURCE_IO,
	       },
	[4] = {
	       .start = GPIO_EXT_CHG_EN,
	       .end = GPIO_EXT_CHG_EN,
	       .name = "ext_chg_en",
	       .flags = IORESOURCE_IO,
	       },
	[5] = {
	       .start = GPIO_EXT_CHG_OVI,
	       .end = GPIO_EXT_CHG_OVI,
	       .name = "ext_vchg_ovi",
	       .flags = IORESOURCE_IO,
	       }
#endif
};

struct platform_device sprd_battery_device = {
	.name = "sprd-battery",
	.id = 0,
	.num_resources = ARRAY_SIZE(sprd_battery_resources),
	.resource = sprd_battery_resources,
	.dev = {
		.platform_data = &sprdbat_pdata,
		},
};
#endif
