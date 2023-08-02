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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>
#ifdef CONFIG_OF
#include <linux/of_platform.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#endif
#include <mach/hardware.h>
#include <linux/i2c.h>
#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
#include <linux/i2c/lis3dh.h>
#endif
#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
#include <linux/i2c/ltr_558als.h>
#endif
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/serial_sprd.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/pinmap.h>
#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
#include <linux/mpu.h>
#endif
#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
#include <linux/akm8975.h>
#endif
#include <linux/irq.h>
#include <linux/input/matrix_keypad.h>

#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/hardware.h>
#include <mach/kpd.h>

#include "devices.h"

#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
#include <linux/i2c/focaltech.h>
#endif

#if(defined(CONFIG_TOUCHSCREEN_IST30XX))
#include "../../../drivers/input/touchscreen/ist30xx/ist30xx.h"
#endif

#include <mach/i2s.h>

#include <linux/i2c-gpio.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

/*keypad define */
#define CUSTOM_KEYPAD_ROWS          (SCI_ROW0 | SCI_ROW1)
#define CUSTOM_KEYPAD_COLS          (SCI_COL0 | SCI_COL1)
#define ROWS	(2)
#define COLS	(2)
#ifndef CONFIG_OF
static const unsigned int board_keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(1, 0, KEY_VOLUMEDOWN),
	KEY(0, 1, KEY_HOME),
};

static const struct matrix_keymap_data customize_keymap = {
	.keymap = board_keymap,
	.keymap_size = ARRAY_SIZE(board_keymap),
};

static struct sci_keypad_platform_data sci_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
	.debounce_time = 5000,
};

static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;
#if defined(CONFIG_BATTERY_SAMSUNG)
static struct platform_device sec_device_battery;
static struct platform_device fuelgauge_gpio_i2c_device;
static struct platform_device rt5033_mfd_device;
#ifdef CONFIG_MFD_RT8973
static struct platform_device rt8973_mfd_device_i2cadaptor;
#endif
#endif

static struct platform_device *devices[] __initdata = {
	&sprd_serial_device0,
	&sprd_serial_device1,
	&sprd_serial_device2,
	&sprd_device_rtc,
	&sprd_eic_gpio_device,
	&sprd_nand_device,
	&sprd_lcd_device0,
#ifdef CONFIG_PSTORE_RAM
	&sprd_ramoops_device,
#endif
        &sprd_backlight_device,
	&sprd_i2c_device0,
	&sprd_i2c_device1,
	&sprd_i2c_device2,
	&sprd_i2c_device3,
	&sprd_pin_switch_device,
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_spi2_device,
	&sprd_keypad_device,
	&sprd_thm_device,
	&sprd_battery_device,
	&sprd_emmc_device,
	&sprd_sdio0_device,
	&sprd_sdio1_device,
	/* &sprd_sdio2_device, */
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
	&sprd_vsp_device,
	&sprd_jpg_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_gsp_device,
#ifdef CONFIG_SPRD_IOMMU
	&sprd_iommu_gsp_device,
	&sprd_iommu_mm_device,
#endif
	&sprd_rotation_device,
	&sprd_sensor_device,
	&sprd_isp_device,
	&sprd_dma_copy_device,
#if 0
	&sprd_ahb_bm0_device,
	&sprd_ahb_bm1_device,
	&sprd_ahb_bm2_device,
	&sprd_ahb_bm3_device,
	&sprd_ahb_bm4_device,
	&sprd_axi_bm0_device,
	&sprd_axi_bm1_device,
	&sprd_axi_bm2_device,
#endif
#ifdef CONFIG_BT_BCM4330
	&rfkill_device,
	&brcm_bluesleep_device,
#endif
	&kb_backlight_device,
	&sprd_a7_pmu_device,
#ifdef CONFIG_SIPC_TD
	&sprd_cproc_td_device,
	&sprd_spipe_td_device,
	&sprd_slog_td_device,
	&sprd_stty_td_device,
	&sprd_seth0_td_device,
	&sprd_seth1_td_device,
	&sprd_seth2_td_device,
	&sprd_saudio_td_device,
#ifdef CONFIG_SIPC_SPOOL
	&sprd_spool_td_device,
#endif
#endif
#ifdef CONFIG_SIPC_WCDMA
	&sprd_cproc_wcdma_device,
	&sprd_spipe_wcdma_device,
	&sprd_slog_wcdma_device,
	&sprd_stty_wcdma_device,
	&sprd_seth0_wcdma_device,
	&sprd_seth1_wcdma_device,
	&sprd_seth2_wcdma_device,
	&sprd_saudio_wcdma_device,
#ifdef CONFIG_SIPC_SPOOL
	&sprd_spool_wcdma_device,
#endif
#endif
#ifdef CONFIG_SIPC_WCN	
	&sprd_cproc_wcn_device,
	&sprd_spipe_wcn_device,
	&sprd_slog_wcn_device,
	&sprd_sttybt_td_device,
#endif
	&sprd_saudio_voip_device,
	&sprd_headset_device,
// for samsung board ---------------------------------
#if defined(CONFIG_BATTERY_SAMSUNG)
	&sec_device_battery,
	&fuelgauge_gpio_i2c_device,
	&rt5033_mfd_device,
#ifdef CONFIG_MFD_RT8973
	&rt8973_mfd_device_i2cadaptor,
#endif
#endif
};

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#if defined(CONFIG_FUELGAUGE_RT5033)
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#include <linux/mfd/rt5033.h>
#endif
#ifdef CONFIG_FLED_RT5033
#include <linux/leds/rt5033_fled.h>
#endif

/* ---- battery ---- */
#define SEC_BATTERY_PMIC_NAME "rt5xx"

unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

unsigned int lpm_charge;
EXPORT_SYMBOL(lpm_charge);

static sec_bat_adc_region_t cable_adc_value_table[] = {
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_UNKNOWN */
	{ 0,    500 },  /* POWER_SUPPLY_TYPE_BATTERY */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_UPS */
	{ 1000, 1500 }, /* POWER_SUPPLY_TYPE_MAINS */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_USB */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_OTG */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_DOCK */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_MISC */
};

/* assume that system's average current consumption is 150mA */
/* We will suggest use 150mA as EOC setting */
/* AICR of RT5033 is 100, 500, 700, 900, 1000, 1500, 2000, we suggest to use 500mA for USB */
sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UPS */
	{700,  1500,   	150,    1800},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,  1000,    150,    1800},     /* POWER_SUPPLY_TYPE_USB */
	{500,  1000,    150,    1800},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,  1000,    150,    1800},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,  1000,    150,    1800},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{500,  1000,    150,   	1800},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{550,   500,    150,    1800},     /* POWER_SUPPLY_TYPE_WPC */
	{700,  1500,   	150,    1800},     /* POWER_SUPPLY_TYPE_UARTOFF */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	30,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	3600,    /* SLEEP */
};

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	return true;
}

static bool sec_chg_gpio_init(void)
{
	int ret;

	ret = gpio_request(GPIO_IF_PMIC_IRQ, "charger-irq");
	if(ret)
		return false;

	gpio_direction_input(GPIO_IF_PMIC_IRQ);

	return true;
}

/* Get LP charging mode state */

static int __init mode_get(char *str)
{
	if (strcmp(str, "charger") == 0)
		lpm_charge = 1;

	return 1;
}

__setup("androidboot.mode=", mode_get);

static int battery_get_lpm_state(char *str)
{
	get_option(&str, &lpcharge);
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("lpcharge=", battery_get_lpm_state);

static bool sec_bat_is_lpm(void)
{
	return lpcharge == 1 ? true : false;
}

static bool sec_bat_check_jig_status(void) { return 0; }

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
EXPORT_SYMBOL(current_cable_type);

static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	ta_nconnected = gpio_get_value(GPIO_IF_PMIC_IRQ);

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	if (ta_nconnected)
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	else
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

	return current_cable_type;
}

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	if (POWER_SUPPLY_TYPE_BATTERY < current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WPC) {
			value.intval =
				POWER_SUPPLY_TYPE_WPC;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
#if 0
		else {
			value.intval =
				POWER_SUPPLY_TYPE_BATTERY;
			psy_do_property("sec-charger", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
#endif
	}
}

static bool sec_bat_check_cable_result_callback(int cable_type)
{
	bool ret = true;
	current_cable_type = cable_type;

	switch (cable_type) {
		case POWER_SUPPLY_TYPE_USB:
			pr_info("%s set vbus applied\n",
					__func__);
			break;
		case POWER_SUPPLY_TYPE_BATTERY:
			pr_info("%s set vbus cut\n",
					__func__);
			break;
		case POWER_SUPPLY_TYPE_MAINS:
			break;
		default:
			pr_err("%s cable type (%d)\n",
					__func__, cable_type);
			ret = false;
			break;
	}
	/* omap4_tab3_tsp_ta_detect(cable_type); */

	return ret;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) { return true; }
static bool sec_bat_check_result_callback(void) { return true; }

/* Kanas battery parameter version 20140425 */

static gain_table_prop rt5033_battery_param1[] =				/// <0oC

{
	{3300, 5, 10, 190, 110},
	{4300, 5, 10, 190, 110},
};


static gain_table_prop rt5033_battery_param2[] =				/// 0oC ~ 50oC
{
	{3300, 41, 44, 52, 37},
	{4300, 41, 44, 52, 37},
};


static gain_table_prop rt5033_battery_param3[] =				/// >50oC
{
	{3300, 42, 57, 80, 60},
	{4300, 42, 57, 80, 60},
};


static offset_table_prop rt5033_poweroff_offset[] =							/// power off soc offset
{
	{0,   -12},																										/// /// SOC(unit:0.1%), Comp. (unit:0.1%)
	{15,  -5},
	{19,  -9},
	{20,  -10},
	{100,  0},
	{1000, 0},
};

#if ENABLE_CHG_OFFSET
static offset_table_prop rt5033_chg_offset[] =							/// charging soc offset
{
	{0,    0},						/// /// SOC(unit:0.1%), Comp. (unit:0.1%)
	{100,  0},
	{130,  0},
	{170,  0},
	{200,  0},
	{1000, 0},
};
#endif

static struct battery_data_t rt5033_comp_data[] =
{
	{
		.param1 = rt5033_battery_param1,
		.param1_size = sizeof(rt5033_battery_param1)/sizeof(gain_table_prop),
		.param2 = rt5033_battery_param2,
		.param2_size = sizeof(rt5033_battery_param2)/sizeof(gain_table_prop),
		.param3 = rt5033_battery_param3,
		.param3_size = sizeof(rt5033_battery_param3)/sizeof(gain_table_prop),
		.offset_poweroff = rt5033_poweroff_offset,
		.offset_poweroff_size = sizeof(rt5033_poweroff_offset)/sizeof(offset_table_prop),
#if ENABLE_CHG_OFFSET
		.offset_charging = rt5033_chg_offset,
		.offset_charging_size = sizeof(rt5033_chg_offset)/sizeof(offset_table_prop),
#endif
	},
};

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) { return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) { return true; }

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) { return true; }

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel) //{ channel = channel; return 0; } */
{
	int data = 0;
	switch(channel)
	{
		case SEC_BAT_ADC_CHANNEL_TEMP:
			data = sci_adc_get_value(ADC_CHANNEL_1,0);
			if(data==4095){
				printk("%s:Only One Thermistor",__func__);
				data = sci_adc_get_value(ADC_CHANNEL_0,0);
			}
			break;
		case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
			data = sci_adc_get_value(ADC_CHANNEL_1,0);
			if(data==4095)
				data = sci_adc_get_value(ADC_CHANNEL_0,0);
			break;
		default:
			break;
	}
	return data;
}

static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }
#if defined(CONFIG_MACH_KANAS)
static const sec_bat_adc_table_data_t temp_table[] = {
	{894 , 700}, /* 70 */
	{981 , 670}, /* 67 */
	{1041 , 650}, /* 65 */
	{1080 , 630}, /* 63 */
	{1142 , 600}, /* 60 */
	{1292 , 550}, /* 55 */
	{1477 , 500}, /* 50 */
	{1657 , 450}, /* 45 */
	{1707 , 430}, /* 43 */
	{1780 , 400}, /* 40 */
	{1972 , 350}, /* 35 */
	{2180 , 300}, /* 30 */
	{2373 , 250}, /* 25 */
	{2563 , 200}, /* 20 */
	{2842 , 150}, /* 15 */
	{3122 , 100}, /* 10 */
	{3292 , 50}, /* 5 */
	{3383 , 20}, /* 2 */
	{3424 , 0}, /* 0 */
	{3466 , -20}, /* -2 */
	{3543 , -50}, /* -5 */
	{3593 , -70}, /* -7 */
	{3782 , -150}, /* -15 */
	{3887, -200},    /* -20 */
};
#else
static const sec_bat_adc_table_data_t temp_table[] = {
	{ 159,   800 },
	{ 192,   750 },
	{ 231,   700 },
	{ 267,   650 },
	{ 321,   600 },
	{ 387,   550 },
	{ 454,   500 },
	{ 532,   450 },
	{ 625,   400 },
	{ 715,   350 },
	{ 823,   300 },
	{ 934,   250 },
	{ 1033,  200 },
	{ 1146,  150 },
	{ 1259,  100 },
	{ 1381,  50 },
	{ 1489,  0 },
	{ 1582,  -50 },
	{ 1654,  -100 },
	{ 1726,  -150 },
};
#endif
sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	//	.get_cable_from_extended_cable_type =
	//	        sec_bat_get_cable_from_extended_cable_type,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
	},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 10,

	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,      /* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,      /* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.battery_data = (void *)rt5033_comp_data,
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type = SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = //SEC_BATTERY_CABLE_SOURCE_CALLBACK,
	SEC_BATTERY_CABLE_SOURCE_EXTERNAL,
	//	SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	.event_check = false,
	.event_waiting_time = 60,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_WORKQUEUE,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_FUELGAUGE,
	.check_count = 0,

	/* Battery check by ADC */
	.check_adc_max = 600,
	.check_adc_min = 50,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_CALLBACK,
	.temp_adc_table = temp_table,
	.temp_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,

	.temp_check_count = 1,
	.temp_high_threshold_event = 650,
	.temp_high_recovery_event = 520,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 50,
	.temp_high_threshold_normal = 650,
	.temp_high_recovery_normal = 520,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 50,
	.temp_high_threshold_lpm = 650,
	.temp_high_recovery_lpm = 520,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 50,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 100,
	.full_condition_vcell = 4150,
	.full_condition_ocv = 4300,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4300,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq_attr = IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE,
	.capacity_max = 1000,
	.capacity_min = 0,
	.capacity_max_margin = 30,

	/* Charger */
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	.chg_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

static struct i2c_gpio_platform_data fuelgauge_gpio_i2c_pdata = {
	.sda_pin = GPIO_FUELGAUGE_SDA,
	.scl_pin = GPIO_FUELGAUGE_SCL,
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device fuelgauge_gpio_i2c_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &fuelgauge_gpio_i2c_pdata,
	},
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				RT5033_SLAVE_ADDRESS),
		.platform_data = &sec_battery_pdata,
	}
};

/* ---- mfd ---- */
#ifdef CONFIG_REGULATOR_RT5033
static struct regulator_consumer_supply rt5033_safe_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_safe_ldo",NULL),
};
static struct regulator_consumer_supply rt5033_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_ldo",NULL),
};
static struct regulator_consumer_supply rt5033_buck_consumers[] = {
	REGULATOR_SUPPLY("rt5033_buck",NULL),
};

static struct regulator_init_data rt5033_safe_ldo_data = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 4950000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
		.boot_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_safe_ldo_consumers),
	.consumer_supplies = rt5033_safe_ldo_consumers,
};
static struct regulator_init_data rt5033_ldo_data = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 3000000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_ldo_consumers),
	.consumer_supplies = rt5033_ldo_consumers,
};
static struct regulator_init_data rt5033_buck_data = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 1200000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_buck_consumers),
	.consumer_supplies = rt5033_buck_consumers,
};

const static struct rt5033_regulator_platform_data rv_pdata = {
	.regulator = {
		[RT5033_ID_LDO_SAFE] = &rt5033_safe_ldo_data,
		[RT5033_ID_LDO1] = &rt5033_ldo_data,
		[RT5033_ID_DCDC1] = &rt5033_buck_data,
	},
};
#endif

#ifdef CONFIG_RT5033_SADDR
#define RT5033FG_SLAVE_ADDR_MSB (0x40)
#else
#define RT5033FG_SLAVE_ADDR_MSB (0x00)
#endif

#define RT5033_SLAVE_ADDR   (0x34|RT5033FG_SLAVE_ADDR_MSB)

/* add regulator data */

/* add fled platform data */
#ifdef CONFIG_FLED_RT5033
static rt5033_fled_platform_data_t fled_pdata = {
	.fled1_en = 1,
	.fled2_en = 1,
	.fled_mid_track_alive = 0,
	.fled_mid_auto_track_en = 0,
	.fled_timeout_current_level = RT5033_TIMEOUT_LEVEL(50),
	.fled_strobe_current = RT5033_STROBE_CURRENT(500),
	.fled_strobe_timeout = RT5033_STROBE_TIMEOUT(544),
	.fled_torch_current = RT5033_TORCH_CURRENT(38),
	.fled_lv_protection = RT5033_LV_PROTECTION(3200),
	.fled_mid_level = RT5033_MID_REGULATION_LEVEL(4400),
};
#endif

rt5033_charger_platform_data_t charger_pdata = {
	.charging_current_table = charging_current_table,
	.chg_float_voltage = 4200,
};

/* Define mfd driver platform data*/
// error: variable 'sc88xx_rt5033_info' has initializer but incomplete type
struct rt5033_mfd_platform_data sc88xx_rt5033_info = {
	.irq_gpio = GPIO_IF_PMIC_IRQ,
	.irq_base = __NR_IRQS,
#ifdef CONFIG_CHARGER_RT5033
	.charger_data = &sec_battery_pdata,
#endif

#ifdef CONFIG_FLED_RT5033
	.fled_platform_data = &fled_pdata,
#endif

#ifdef CONFIG_REGULATOR_RT5033
	.regulator_platform_data = &rv_pdata,
#endif
};

static struct i2c_gpio_platform_data rt5033_i2c_data = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
	.udelay  = 3,
	.timeout = 100,
};

static struct platform_device rt5033_mfd_device = {
	.name   = "i2c-gpio",
	.id     = 8,
	.dev	= {
		.platform_data = &rt5033_i2c_data,
	}
};

static struct i2c_board_info __initdata rt5033_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rt5033-mfd", RT5033_SLAVE_ADDR),
		.platform_data	= &sc88xx_rt5033_info,
	}
};

#ifdef CONFIG_MFD_RT8973
static struct i2c_gpio_platform_data rt8973_i2cadaptor_data = {
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
	.udelay  = 10,
	.timeout = 0,
};

static struct platform_device rt8973_mfd_device_i2cadaptor = {
	.name   = "i2c-gpio",
	.id     = 7,
	.dev	= {
		.platform_data = &rt8973_i2cadaptor_data,
	}
};

extern void dwc_udc_startup(void);
extern void dwc_udc_shutdown(void);
static void usb_cable_detect_callback(uint8_t attached)
{
	if (attached)
		dwc_udc_startup();
	else
		dwc_udc_shutdown();
}

#include <linux/mfd/rt8973.h>
static struct rt8973_platform_data rt8973_pdata = {
	.irq_gpio = GPIO_MUIC_IRQ,
	.cable_chg_callback = NULL,
	.ocp_callback = NULL,
	.otp_callback = NULL,
	.ovp_callback = NULL,
	.usb_callback = usb_cable_detect_callback,
	.uart_callback = NULL,
	.otg_callback = NULL,
	.jig_callback = NULL,
};

static struct i2c_board_info rtmuic_i2c_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("rt8973", 0x28 >> 1),
		.platform_data = &rt8973_pdata,
	},
};
#endif /*CONFIG_MFD_RT8973*/

int epmic_event_handler(int level);
u8 attached_cable;

void sec_charger_cb(u8 cable_type)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);
	attached_cable = cable_type;

	switch (cable_type) {
		case MUIC_RT8973_CABLE_TYPE_NONE:
		case MUIC_RT8973_CABLE_TYPE_UNKNOWN:
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case MUIC_RT8973_CABLE_TYPE_USB:
		case MUIC_RT8973_CABLE_TYPE_CDP:
		case MUIC_RT8973_CABLE_TYPE_L_USB:
			current_cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
		case MUIC_RT8973_CABLE_TYPE_ATT_TA:
			current_cable_type = POWER_SUPPLY_TYPE_MAINS;
			break;
		case MUIC_RT8973_CABLE_TYPE_OTG:
			goto skip;
		case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
			break;
		case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
			/*
			   if (!gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8))) {
			   pr_info("%s cable type POWER_SUPPLY_TYPE_UARTOFF\n", __func__);
			   current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
			   }
			   else {
			   current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
			   }*/
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

			break;
		case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
		case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
			current_cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		case MUIC_RT8973_CABLE_TYPE_0x1A:
		case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
			current_cable_type = POWER_SUPPLY_TYPE_MAINS;
			break;
		case MUIC_RT8973_CABLE_TYPE_0x15:
			current_cable_type = POWER_SUPPLY_TYPE_MISC;
			break;
		default:
			pr_err("%s: invalid type for charger:%d\n",
					__func__, cable_type);
			current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
			goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

	if (current_cable_type == POWER_SUPPLY_TYPE_USB) {
		epmic_event_handler(1);
	}
skip:
	return;
}
EXPORT_SYMBOL(sec_charger_cb);
#endif

static struct platform_device *late_devices[] __initdata = {
	/* 1. CODECS */
	&sprd_audio_sprd_codec_v3_device,
	&sprd_audio_null_codec_device,

	/* 2. CPU DAIS */
	&sprd_audio_vbc_r2p0_device,
	&sprd_audio_vaudio_device,
	&sprd_audio_i2s0_device,
	&sprd_audio_i2s1_device,
	&sprd_audio_i2s2_device,
	&sprd_audio_i2s3_device,

	/* 3. PLATFORM */
	&sprd_audio_platform_pcm_device,

	/* 4. MACHINE */
	&sprd_audio_vbc_r2p0_sprd_codec_v3_device,
	&sprd_audio_i2s_null_codec_device,

};

/* BT suspend/resume */
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= GPIO_BT2AP_WAKE,
		.end	= GPIO_BT2AP_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= GPIO_AP2BT_WAKE,
		.end	= GPIO_AP2BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static struct resource rfkill_resources[] = {
	{
		.name   = "bt_power",
		.start  = GPIO_BT_POWER,
		.end    = GPIO_BT_POWER,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "bt_reset",
		.start  = GPIO_BT_RESET,
		.end    = GPIO_BT_RESET,
		.flags  = IORESOURCE_IO,
	},
};
static struct platform_device rfkill_device = {
	.name = "rfkill",
	.id = -1,
	.num_resources	= ARRAY_SIZE(rfkill_resources),
	.resource	= rfkill_resources,
};

/* keypad backlight */
static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
};
#endif /* CONFIG_OF */

static int calibration_mode = false;
static int __init calibration_start(char *str)
{
	int calibration_device =0;
	int mode=0,freq=0,device=0;
	if(str){
		pr_info("modem calibartion:%s\n", str);
		sscanf(str, "%d,%d,%d", &mode,&freq,&device);
	}
	if(device & 0x80){
		calibration_device = device & 0xf0;
		calibration_mode = true;
		pr_info("calibration device = 0x%x\n",calibration_device);
	}
	return 1;
}
__setup("calibration=", calibration_start);

int in_calibration(void)
{
	return (int)(calibration_mode == true);
}

EXPORT_SYMBOL(in_calibration);
#ifndef CONFIG_OF
static void __init sprd_add_otg_device(void)
{
	/*
	 * if in calibrtaion mode, we do nothing, modem will handle everything
	 */
	platform_device_register(&sprd_otg_device);
}

static struct serial_data plat_data0 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
static struct ft5x0x_ts_platform_data ft5x0x_ts_info = { 
	.irq_gpio_number    = GPIO_TOUCH_IRQ,
	.reset_gpio_number  = GPIO_TOUCH_RESET,
	.vdd_name           = "vdd28",
	.virtualkeys = {
			45,990,80,65,
			280,1020,80,65,
			405,990,80,65
	         },
	 .TP_MAX_X = 720,
	 .TP_MAX_Y = 1280,
};
#endif

#if defined (CONFIG_TOUCHSCREEN_IST30XX)
static struct tsp_platform_data ist30xx_info = {
	.gpio = GPIO_TOUCH_IRQ,
};
#endif

#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
static struct ltr558_pls_platform_data ltr558_pls_info = {
	.irq_gpio_number	= GPIO_PROX_INT,
};
#endif

#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
static struct lis3dh_acc_platform_data lis3dh_plat_data = {
	.poll_interval = 10,
	.min_interval = 10,
	.g_range = LIS3DH_ACC_G_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 1,
	.negate_z = 0
};
#endif

#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
struct akm8975_platform_data akm8975_platform_d = {
	.mag_low_x = -20480,
	.mag_high_x = 20479,
	.mag_low_y = -20480,
	.mag_high_y = 20479,
	.mag_low_z = -20480,
	.mag_high_z = 20479,
};
#endif

#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
static struct mpu_platform_data mpu9150_platform_data = {
	.int_config = 0x00,
	.level_shifter = 0,
	.orientation = { -1, 0, 0,
					  0, +1, 0,
					  0, 0, -1 },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_COMPASS,
	.sec_slave_id = COMPASS_ID_AK8963,
	.secondary_i2c_addr = 0x0C,
	.secondary_orientation = { 0, -1, 0,
					-1, 0, 0,
					0, 0, -1 },
	.key = {0xec, 0x06, 0x17, 0xdf, 0x77, 0xfc, 0xe6, 0xac,
			0x7b, 0x6f, 0x12, 0x8a, 0x1d, 0x63, 0x67, 0x37},
};
#endif

static struct i2c_board_info i2c2_boardinfo[] = {
#if(defined(CONFIG_INPUT_LIS3DH_I2C)||defined(CONFIG_INPUT_LIS3DH_I2C_MODULE))
	{ I2C_BOARD_INFO(LIS3DH_ACC_I2C_NAME, LIS3DH_ACC_I2C_ADDR),
	  .platform_data = &lis3dh_plat_data,
	},
#endif
#if(defined(CONFIG_INV_MPU_IIO)||defined(CONFIG_INV_MPU_IIO_MODULE))
	{ I2C_BOARD_INFO("mpu9150", 0x68),
	  .irq = GPIO_GYRO_INT1,
	  .platform_data = &mpu9150_platform_data,
	},
#endif
#if(defined(CONFIG_INPUT_LTR558_I2C)||defined(CONFIG_INPUT_LTR558_I2C_MODULE))
	{ I2C_BOARD_INFO(LTR558_I2C_NAME,  LTR558_I2C_ADDR),
	  .platform_data = &ltr558_pls_info,
	},
#endif
#if(defined(CONFIG_SENSORS_AK8975)||defined(CONFIG_SENSORS_AK8975_MODULE))
	{ I2C_BOARD_INFO(AKM8975_I2C_NAME, AKM8975_I2C_ADDR),
	  .platform_data = &akm8975_platform_d,
	},
#endif
};

static struct i2c_board_info i2c0_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

static struct i2c_board_info i2c1_boardinfo[] = {
#if(defined(CONFIG_TOUCHSCREEN_FOCALTECH)||defined(CONFIG_TOUCHSCREEN_FOCALTECH_MODULE))
	{
		I2C_BOARD_INFO(FOCALTECH_TS_NAME, FOCALTECH_TS_ADDR),
		.platform_data = &ft5x0x_ts_info,
	},
#endif
#if defined (CONFIG_TOUCHSCREEN_IST30XX)
	{
		I2C_BOARD_INFO(IST30XX_DEV_NAME, 0x50),
		.platform_data =&ist30xx_info,
	},
#endif
};

static int sc8810_add_i2c_devices(void)
{
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
#if defined(CONFIG_BATTERY_SAMSUNG)
	gpio_request(GPIO_FUELGAUGE_ALERT, "RT5033_FG_ALERT");
	sec_battery_pdata.fg_irq = gpio_to_irq(GPIO_FUELGAUGE_ALERT);
	i2c_register_board_info(6, sec_brdinfo_fuelgauge, ARRAY_SIZE(sec_brdinfo_fuelgauge));
	i2c_register_board_info(8, rt5033_i2c_devices, ARRAY_SIZE(rt5033_i2c_devices));
#ifdef CONFIG_MFD_RT8973
	i2c_register_board_info(7, rtmuic_i2c_boardinfo, ARRAY_SIZE(rtmuic_i2c_boardinfo));
#endif
#endif
	return 0;
}

static void sprd_spi_init(void) { }

static int sc8810_add_misc_devices(void)
{
	return 0;
}
#endif
int __init __clock_init_early(void)
{
	pr_info("ahb ctl0 %08x, ctl2 %08x glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
		sci_glb_raw_read(REG_AP_AHB_AHB_EB),
		sci_glb_raw_read(REG_AP_AHB_AHB_RST),
		sci_glb_raw_read(REG_AON_APB_APB_EB0),
		sci_glb_raw_read(REG_AON_APB_APB_EB1),
		sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));

	sci_glb_clr(REG_AP_AHB_AHB_EB,
		BIT_BUSMON2_EB		|
		BIT_BUSMON1_EB		|
		BIT_BUSMON0_EB		|
		//BIT_SPINLOCK_EB		|
		BIT_GPS_EB		|
		//BIT_EMMC_EB		|
		//BIT_SDIO2_EB		|
		//BIT_SDIO1_EB		|
		//BIT_SDIO0_EB		|
		BIT_DRM_EB		|
		BIT_NFC_EB		|
		//BIT_DMA_EB		|
		//BIT_USB_EB		|
		//BIT_GSP_EB		|
		//BIT_DISPC1_EB		|
		//BIT_DISPC0_EB		|
		//BIT_DSI_EB		|
		0);
	sci_glb_clr(REG_AP_APB_APB_EB,
		BIT_INTC3_EB		|
		BIT_INTC2_EB		|
		BIT_INTC1_EB		|
		BIT_IIS1_EB		|
		BIT_UART2_EB		|
		BIT_UART0_EB		|
		BIT_SPI1_EB		|
		BIT_SPI0_EB		|
		BIT_IIS0_EB		|
		BIT_I2C0_EB		|
		BIT_SPI2_EB		|
		BIT_UART3_EB		|
		0);
	sci_glb_clr(REG_AON_APB_APB_RTC_EB,
		BIT_KPD_RTC_EB		|
		BIT_KPD_EB		|
		BIT_EFUSE_EB		|
		0);

	sci_glb_clr(REG_AON_APB_APB_EB0,
		BIT_AUDIF_EB			|
		BIT_VBC_EB			|
		BIT_PWM3_EB			|
		BIT_PWM1_EB			|
		0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
		BIT_AUX1_EB			|
		BIT_AUX0_EB			|
		0);

	printk("sc clock module early init ok\n");
	return 0;
}

static inline int	__sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}
#ifndef CONFIG_OF
/*i2s0 config for BT, use pcm mode*/
static struct i2s_config i2s0_config = {
	.fs = 8000,
	.slave_timeout = 0xF11,
	.bus_type = PCM_BUS,
	.byte_per_chan = I2S_BPCH_16,
	.mode = I2S_MASTER,
	.lsb = I2S_LSB,
	.rtx_mode = I2S_RTX_MODE,
	.sync_mode = I2S_LRCK,
	.lrck_inv = I2S_L_LEFT,	/*NOTE: MUST I2S_L_LEFT in pcm mode*/
	.clk_inv = I2S_CLK_N,
	.pcm_bus_mode = I2S_SHORT_FRAME,
	.pcm_slot = 0x1,
	.pcm_cycle = 1,
	.tx_watermark = 8,
	.rx_watermark = 24,
};
/*you can change param here to config the i2s modules*/
static struct i2s_config i2s1_config = {0};
static struct i2s_config i2s2_config = {0};
static struct i2s_config i2s3_config = {0};
#else
const struct of_device_id of_sprd_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "sprd,adi-bus", },
	{}
};
#endif
#ifdef CONFIG_OF
static const struct of_dev_auxdata of_sprd_default_bus_lookup[] = {
	 { .compatible = "sprd,sdhci-shark",  .name = "sprd-sdhci.0", .phys_addr = SPRD_SDIO0_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sprd-sdhci.1", .phys_addr = SPRD_SDIO1_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sprd-sdhci.2", .phys_addr = SPRD_SDIO2_BASE  },
	 { .compatible = "sprd,sdhci-shark",  .name = "sprd-sdhci.3", .phys_addr = SPRD_EMMC_BASE  },
	 { .compatible = "sprd,sprd_backlight",  .name = "sprd_backlight" },
	 {}
};
#endif

static void __init sc8830_init_machine(void)
{
	printk("sci get chip id = 0x%x\n",__sci_get_chip_id());

	sci_adc_init((void __iomem *)ADC_BASE);
	sci_regulator_init();
#ifndef CONFIG_OF
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data,sizeof(sci_keypad_data));
	platform_device_add_data(&sprd_audio_i2s0_device,(const void*)&i2s0_config,sizeof(i2s0_config));
	platform_device_add_data(&sprd_audio_i2s1_device,(const void*)&i2s1_config,sizeof(i2s1_config));
	platform_device_add_data(&sprd_audio_i2s2_device,(const void*)&i2s2_config,sizeof(i2s2_config));
	platform_device_add_data(&sprd_audio_i2s3_device,(const void*)&i2s3_config,sizeof(i2s3_config));
	platform_add_devices(devices, ARRAY_SIZE(devices));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	sprd_spi_init();
#else
	of_platform_populate(NULL, of_sprd_default_bus_match_table, of_sprd_default_bus_lookup, NULL);
#endif

    sec_class = class_create(THIS_MODULE, "sec");
    if (IS_ERR(sec_class)) {
        pr_err("Failed to create class(sec)!\n");
		printk("Failed create class \n");
        return PTR_ERR(sec_class);
    }
}

#ifdef CONFIG_OF
const struct of_device_id of_sprd_late_bus_match_table[] = {
	{ .compatible = "sprd,sound", },
	{}
};
#endif

static void __init sc8830_init_late(void)
{
#ifdef CONFIG_OF
	of_platform_populate(of_find_node_by_path("/sprd-audio-devices"),
				of_sprd_late_bus_match_table, NULL, NULL);
#else
	platform_add_devices(late_devices, ARRAY_SIZE(late_devices));
#endif
}

extern void __init  sci_enable_timer_early(void);
static void __init sc8830_init_early(void)
{
	/* earlier init request than irq and timer */
	__clock_init_early();
#ifndef CONFIG_OF
	sci_enable_timer_early();
#endif
	sci_adi_init();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}
#ifdef CONFIG_OF
static void __init sc8830_pmu_init(void)
{
	__raw_writel(__raw_readl(REG_PMU_APB_PD_MM_TOP_CFG)
		     & ~(BIT_PD_MM_TOP_FORCE_SHUTDOWN),
		     REG_PMU_APB_PD_MM_TOP_CFG);

	__raw_writel(__raw_readl(REG_PMU_APB_PD_GPU_TOP_CFG)
		     & ~(BIT_PD_GPU_TOP_FORCE_SHUTDOWN),
		     REG_PMU_APB_PD_GPU_TOP_CFG);

	__raw_writel(__raw_readl(REG_AON_APB_APB_EB0) | BIT_MM_EB |
		     BIT_GPU_EB, REG_AON_APB_APB_EB0);

	__raw_writel(__raw_readl(REG_MM_AHB_AHB_EB) | BIT_MM_CKG_EB,
		     REG_MM_AHB_AHB_EB);

	__raw_writel(__raw_readl(REG_MM_AHB_GEN_CKG_CFG)
		     | BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN,
		     REG_MM_AHB_GEN_CKG_CFG);

	__raw_writel(__raw_readl(REG_MM_CLK_MM_AHB_CFG) | 0x3,
		     REG_MM_CLK_MM_AHB_CFG);
}

static void sprd_init_time(void)
{
	if(of_have_populated_dt()){
		sc8830_pmu_init();
		of_clk_init(NULL);
		clocksource_of_init();
	}else{
		sci_clock_init();
		sci_enable_timer_early();
		sci_timer_init();
	}
}
static const char *sprd_boards_compat[] __initdata = {
	"sprd,sp8835eb",
	NULL,
};
#endif
extern struct smp_operations sprd_smp_ops;

MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_early	= sc8830_init_early,
	.init_irq	= sci_init_irq,
#ifdef CONFIG_OF
	.init_time		= sprd_init_time,
#else
	.init_time		= sci_timer_init,
#endif
	.init_machine	= sc8830_init_machine,
	.init_late	= sc8830_init_late,
#ifdef CONFIG_OF
	.dt_compat = sprd_boards_compat,
#endif
MACHINE_END

