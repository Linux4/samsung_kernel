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
// gps for marvell
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/module.h>
// gps for marvell

#include <mach/hardware.h>
#include <linux/i2c.h>
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
#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#endif
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/i2c-gpio.h>

#include <linux/input.h>

#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/hardware.h>
#include <mach/kpd.h>

#include "devices.h"
#include <linux/input/matrix_keypad.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/i2s.h>
#include <linux/regulator/machine.h>
#ifdef CONFIG_MACH_SHARK
#include <linux/ktd253b_bl.h>
#else
#include <linux/rt4502_bl.h>
#endif
#include <linux/i2c/mms_ts.h>
#include <linux/gp2ap002a00f.h>
#include <linux/bst_sensor_common.h>

#include <sound/sprd-audio-hook.h>
#include "../../../drivers/input/touchscreen/imagis_kanas/ist30xx.h"
 // gps for marvell
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/module.h>
// gps for marvell

#include <video/sensor_drv_k.h>
#include <linux/leds/flashlight.h>

#ifdef CONFIG_SEC_THERMISTOR
#include <mach/sec_thermistor.h>
#endif
#ifdef CONFIG_NFC_PN547
#include <mach/board-sp8830ssw_nfc.h>
#endif
#if defined(CONFIG_SENSORS_ACCELOMETER_BMA25X)
#include <linux/sensor/bma255_platformdata.h>
#endif
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#define GPIO_HOME_KEY 113 /* PIN LCD_D[5] */

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif
extern void hawaii_bt_init(void);
struct class *sec_class;
EXPORT_SYMBOL(sec_class);
/*keypad define */
#define CUSTOM_KEYPAD_ROWS          (SCI_ROW0 | SCI_ROW1)
#define CUSTOM_KEYPAD_COLS          (SCI_COL0 | SCI_COL1 | SCI_COL2)
#define CUSTOM_KEYPAD_COLS_REV02          (SCI_COL0 | SCI_COL1)
#define ROWS	(2)
#define COLS	(3)
#define COLS_REV02	(2)
extern SENSOR_PROJECT_FUNC_T s_sensor_project_func;
static const unsigned int board_keymap[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_HOME),
	KEY(1, 1, 444),
	KEY(0, 2, KEY_VOLUMEDOWN),
	KEY(1, 2, KEY_VOLUMEUP),
};

static const unsigned int board_keymap_rev02[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_HOME),
};
static const struct matrix_keymap_data customize_keymap = {
	.keymap = board_keymap,
	.keymap_size = ARRAY_SIZE(board_keymap),
};
static const struct matrix_keymap_data customize_keymap_rev02 = {
	.keymap = board_keymap_rev02,
	.keymap_size = ARRAY_SIZE(board_keymap_rev02),
};
static struct sci_keypad_platform_data sci_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
//	.debounce_time = 5000,
};
static struct sci_keypad_platform_data sci_keypad_data_rev02 = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS_REV02,
	.rows_number = ROWS,
	.cols_number = COLS_REV02,
	.keymap_data = &customize_keymap_rev02,
	.support_long_key = 1,
	.repeat = 0,
//	.debounce_time = 5000,
};
#ifdef CONFIG_SEC_THERMISTOR
static struct sec_therm_adc_table temp_table_ap[] = {
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
static struct sec_therm_platform_data sec_therm_pdata = {
.adc_arr_size	= ARRAY_SIZE(temp_table_ap),
.adc_table	= temp_table_ap,
.polling_interval = 15 * 1000, /* msecs */
.no_polling = 0,
.get_siop_level=NULL,
	};
struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};
#endif
static struct platform_device rfkill_device;
static struct platform_device brcm_bluesleep_device;
static struct platform_device kb_backlight_device;
static struct platform_device gpio_button_device;
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
	&sprd_serial_device3,
	&sprd_device_rtc,
	#ifdef CONFIG_SEC_GPIO_DVS
	&secgpio_dvs_device,
	#endif
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
	&sprd_spi0_device,
	&sprd_spi1_device,
	&sprd_spi2_device,
	&sprd_keypad_device,
//	&sprd_audio_cpu_dai_vbc_device,
//	&sprd_audio_codec_sprd_codec_device,
//	&sprd_battery_device,
	&sprd_emmc_device,
	&sprd_sdio0_device,
	&sprd_sdio1_device,
//	&sprd_sdio2_device,
#ifdef CONFIG_ION
	&sprd_ion_dev,
#endif
	&sprd_vsp_device,
	&sprd_jpg_device,
	&sprd_dcam_device,
	&sprd_scale_device,
	&sprd_gsp_device,
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
	 &sprd_saudio_voip_device,
#endif
#if defined(CONFIG_BATTERY_SAMSUNG)
	&sec_device_battery,
	&fuelgauge_gpio_i2c_device,
	&rt5033_mfd_device,
#ifdef CONFIG_MFD_RT8973
	&rt8973_mfd_device_i2cadaptor,
#endif
#endif
#ifdef FANGHUA
	&sprd_thm_device,
	&sprd_thm_a_device,
#endif
	&gpio_button_device,
	&sprd_headset_device,
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
  static const sec_bat_adc_table_data_t temp_table2[] = {
	{894 , 700}, /* 70 */
	{981 , 670}, /* 67 */
	{1041 , 650}, /* 65 */
	{1080 , 630}, /* 63 */
	{1181 , 600}, /* 60 */
	{1329 , 550}, /* 55 */
	{1514 , 500}, /* 50 */
	{1675 , 460}, /* 46 */
	{1694 , 450}, /* 45 */
	{1744 , 430}, /* 43 */
	{1817 , 400}, /* 40 */
	{1972 , 350}, /* 35 */
	{2180 , 300}, /* 30 */
	{2373 , 250}, /* 25 */
	{2707 , 200}, /* 20 */
	{2882 , 150}, /* 15 */
	{3069 , 100}, /* 10 */
	{3260 , 50}, /* 5 */
	{3323 , 20}, /* 2 */
	{3372 , 0}, /* 0 */
	{3426 , -20}, /* -2 */
	{3495 , -50}, /* -5 */
	{3550 , -70}, /* -7 */
	{3690 , -150}, /* -15 */
	{3755, -200},    /* -20 */
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

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,

        .temp_check_count = 1,
        .temp_high_threshold_event = 600,
        .temp_high_recovery_event = 520,
        .temp_low_threshold_event = -50,
        .temp_low_recovery_event = 50,
        .temp_high_threshold_normal = 600,
        .temp_high_recovery_normal = 520,
        .temp_low_threshold_normal = -50,
        .temp_low_recovery_normal = 50,
        .temp_high_threshold_lpm = 600,
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
						   SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
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
    .usb_callback = NULL,
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


static struct gpio_keys_button gpio_buttons[] = {
        {
                .gpio           = GPIO_HOME_KEY,
				.ds_irqflags	= IRQF_TRIGGER_LOW,
                .code           = KEY_HOME,
                .desc           = "Home Key",
                .active_low     = 1,
                .debounce_interval = 2,
        },
};

static struct gpio_keys_platform_data gpio_button_data = {
        .buttons        = gpio_buttons,
        .nbuttons       = ARRAY_SIZE(gpio_buttons),
#ifdef CONFIG_SENSORS_HALL
		.gpio_flip_cover = 123, //PIN number of HALL IC
#endif
};

static struct platform_device gpio_button_device = {
        .name           = "gpio-keys",
        .id             = -1,
        .num_resources  = 0,
        .dev            = {
                .platform_data  = &gpio_button_data,
        }
};

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

static void __init sprd_add_otg_device(void)
{
	/*
	 * if in calibrtaion mode, we do nothing, modem will handle everything
	 */
	platform_device_register(&sprd_otg_device);
}


static int __init hw_revision_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	system_rev = n;
	return 1;
}
__setup("hw_revision=", hw_revision_setup);


#if defined(CONFIG_TOUCHSCREEN_MMS136) || defined(CONFIG_TOUCHSCREEN_MMS134)
static void touchkey_led_vdd_enable(bool on)
{
        static int ret = 0;

        if (ret == 0) {
                ret = gpio_request(GPIO_TOUCHKEY_LED_EN, "touchkey_led_en");
                if (ret) {
                        printk("%s: request gpio error\n", __func__);
                        return -EIO;
                }
                gpio_direction_output(GPIO_TOUCHKEY_LED_EN, 0);
                ret = 1;
        }
        gpio_set_value(GPIO_TOUCHKEY_LED_EN,on);
}

static void mms_ts_vdd_enable(bool on)
{
	static struct regulator *ts_vdd = NULL;

	if (ts_vdd == NULL) {
		ts_vdd = regulator_get(NULL, "vddsim2");

		if (IS_ERR(ts_vdd)) {
			pr_err("Get regulator of TSP error!\n");
			return;
		}
	}
	if (on) {
		regulator_set_voltage(ts_vdd, 3000000, 3000000);
		regulator_enable(ts_vdd);
	}
	else if (regulator_is_enabled(ts_vdd)) {
		regulator_disable(ts_vdd);
	}
}
#endif
// GPS for marvell

#ifdef CONFIG_PROC_FS

static int gps_enable_control(int flag)
{
        static struct regulator *gps_regulator = NULL;
        static int f_enabled = 0;
        printk("[GPS] LDO control : %s\n", flag ? "ON" : "OFF");

        if (flag && (!f_enabled)) {
//For 0.1 revision vddrf1 is used and for other revisions use avdd18
#ifdef CONFIG_KANAS_GPS_AVDD18
                      gps_regulator = regulator_get(NULL, "avdd18");
#else
                      gps_regulator = regulator_get(NULL, "vddrf1");
#endif
                      if (IS_ERR(gps_regulator)) {
                                   gps_regulator = NULL;
                                   return EIO;
                      } else {
                                   regulator_set_voltage(gps_regulator, 1800000, 1800000);
                                   regulator_enable(gps_regulator);
                      }
                      f_enabled = 1;
        }
        if (f_enabled && (!flag))
        {
                      if (gps_regulator) {
                                   regulator_disable(gps_regulator);
                                   regulator_put(gps_regulator);
                                   gps_regulator = NULL;
                      }
                      f_enabled = 0;
        }
        return 0;
}

/* GPS: power on/off control */
static void gps_power_on(void)
{
	unsigned int gps_rst_n,gps_on;

	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		goto out;
	}

	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);

	gps_enable_control(1);

	mdelay(10);
//	gpio_direction_output(gps_rst_n, 1);
	mdelay(10);
//	gpio_direction_output(gps_on, 1);

	pr_info("gps chip powered on\n");

	gpio_free(gps_on);
out:
	gpio_free(gps_rst_n);

	return;
}

static void gps_power_off(void)
{
	unsigned int gps_rst_n, gps_on;


	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		return;
	}


	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_debug("Request GPIO failed, gpio: %d\n", gps_rst_n);
		goto out2;
	}


	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);

	gps_enable_control(0);

	pr_info("gps chip powered off\n");

	gpio_free(gps_rst_n);
out2:
	gpio_free(gps_on);
	return;
}

static void gps_reset(int flag)
{
	unsigned int gps_rst_n;


	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gpio_direction_output(gps_rst_n, flag);
	if(flag == 1)
	{
#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate INIT position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_initgpio();
#endif
	}
	gpio_free(gps_rst_n);
	printk(KERN_INFO "gps chip reset with %s\n", flag ? "ON" : "OFF");
}

static void gps_on_off(int flag)
{
	unsigned int gps_on;


	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_on);
		return;
	}

	gpio_direction_output(gps_on, flag);
	gpio_free(gps_on);
	printk(KERN_INFO "gps chip onoff with %s\n", flag ? "ON" : "OFF");
}

#define SIRF_STATUS_LEN	16
static char sirf_status[SIRF_STATUS_LEN] = "off";

static ssize_t sirf_read_proc(struct file *filp,
		 char *buff, size_t len1, loff_t *off)
{
	char page[SIRF_STATUS_LEN]= {0};
	int len = strlen(sirf_status);

	sprintf(page, "%s\n", sirf_status);
	copy_to_user(buff,page,len + 1);
	return len + 1;
}

static ssize_t sirf_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int flag, ret;
	char buffer[7];

	if (len > 255)
		len = 255;

	memset(messages, 0, sizeof(messages));

	if (!buff || copy_from_user(messages, buff, len))
		return -EFAULT;

	if (strlen(messages) > (SIRF_STATUS_LEN - 1)) {
		pr_warning("[ERROR] messages too long! (%d) %s\n",
			strlen(messages), messages);
		return -EFAULT;
	}

	if (strncmp(messages, "off", 3) == 0) {
		strcpy(sirf_status, "off");
		gps_power_off();
	} else if (strncmp(messages, "on", 2) == 0) {
		strcpy(sirf_status, "on");
		gps_power_on();
	} else if (strncmp(messages, "reset", 5) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2){
			gps_reset(flag);
		}
	} else if (strncmp(messages, "sirfon", 6) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2){
			gps_on_off(flag);
		}
	} else
		pr_info("usage: echo {on/off} > /proc/driver/sirf\n");

	return len;
}

static const struct file_operations gps_proc_fops = {
	.owner = THIS_MODULE,
	.read = sirf_read_proc,
	.write = sirf_write_proc,
};

static void create_sirf_proc_file(void)
{
	struct proc_dir_entry *sirf_proc_file = NULL;

	/*
	 * CSR and Marvell GPS lib will both use this file
	 * "/proc/drver/gps" may be modified in future
	 */
	sirf_proc_file = proc_create("driver/sirf", 0644, NULL,&gps_proc_fops);
	if (!sirf_proc_file) {
		pr_err("sirf proc file create failed!\n");
		return;
	}
}


#endif
// GPS for marvell



static struct serial_data plat_data0 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 48000000,
};
static struct serial_data plat_data1 = {
//	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};
static struct serial_data plat_data2 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

static struct serial_data plat_data3 = {
	.wakeup_type = BT_RTS_HIGH_WHEN_SLEEP,
	.clk = 26000000,
};

#if 0
// GPS for marvell

#ifdef CONFIG_PROC_FS

static int gps_enable_control(int flag)
{
        static struct regulator *gps_regulator = NULL;
        static int f_enabled = 0;
        printk("[GPS] LDO control : %s\n", flag ? "ON" : "OFF");

        if (flag && (!f_enabled)) {
                      gps_regulator = regulator_get(NULL, "vddrf1");
                      if (IS_ERR(gps_regulator)) {
                                   gps_regulator = NULL;
                                   return EIO;
                      } else {
                                   regulator_set_voltage(gps_regulator, 1800000, 1800000);
                                   regulator_enable(gps_regulator);
                      }
                      f_enabled = 1;
        }
        if (f_enabled && (!flag))
        {
                      if (gps_regulator) {
                                   regulator_disable(gps_regulator);
                                   regulator_put(gps_regulator);
                                   gps_regulator = NULL;
                      }
                      f_enabled = 0;
        }
        return 0;
}

/* GPS: power on/off control */
static void gps_power_on(void)
{
	unsigned int gps_rst_n,gps_on;


	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		goto out;
	}

	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);

	gps_enable_control(1);

	mdelay(10);
//	gpio_direction_output(gps_rst_n, 1);
	mdelay(10);
//	gpio_direction_output(gps_on, 1);

	pr_info("gps chip powered on\n");

	gpio_free(gps_on);
out:
	gpio_free(gps_rst_n);


	return;
}

static void gps_power_off(void)
{
	unsigned int gps_rst_n, gps_on;



	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed,gpio: %d\n", gps_on);
		return;
	}


	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_debug("Request GPIO failed, gpio: %d\n", gps_rst_n);
		goto out2;
	}


	gpio_direction_output(gps_rst_n, 0);
	gpio_direction_output(gps_on, 0);

	gps_enable_control(0);

	pr_info("gps chip powered off\n");

	gpio_free(gps_rst_n);
out2:
	gpio_free(gps_on);
	return;
}

static void gps_reset(int flag)
{
	unsigned int gps_rst_n;


	gps_rst_n = GPIO_GPS_RESET;
	if (gpio_request(gps_rst_n, "gpio_gps_rst")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_rst_n);
		return;
	}

	gpio_direction_output(gps_rst_n, flag);
	if(flag == 1)
	{
#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate INIT position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_initgpio();
#endif
	}
	gpio_free(gps_rst_n);
	printk(KERN_INFO "gps chip reset with %s\n", flag ? "ON" : "OFF");
}

static void gps_on_off(int flag)
{
	unsigned int gps_on;


	gps_on = GPIO_GPS_ONOFF;
	if (gpio_request(gps_on, "gpio_gps_on")) {
		pr_err("Request GPIO failed, gpio: %d\n", gps_on);
		return;
	}

	gpio_direction_output(gps_on, flag);
	gpio_free(gps_on);
	printk(KERN_INFO "gps chip onoff with %s\n", flag ? "ON" : "OFF");
}

#define SIRF_STATUS_LEN	16
static char sirf_status[SIRF_STATUS_LEN] = "off";

static ssize_t sirf_read_proc(struct file *filp,
		 char *buff, size_t len1, loff_t *off)
{
	char page[SIRF_STATUS_LEN]= {0};
	int len = strlen(sirf_status);

	sprintf(page, "%s\n", sirf_status);
	copy_to_user(buff,page,len + 1);
	return len + 1;
}

static ssize_t sirf_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int flag, ret;
	char buffer[7];

	if (len > 255)
		len = 255;

	memset(messages, 0, sizeof(messages));

	if (!buff || copy_from_user(messages, buff, len))
		return -EFAULT;

	if (strlen(messages) > (SIRF_STATUS_LEN - 1)) {
		pr_warning("[ERROR] messages too long! (%d) %s\n",
			strlen(messages), messages);
		return -EFAULT;
	}

	if (strncmp(messages, "off", 3) == 0) {
		strcpy(sirf_status, "off");
		gps_power_off();
	} else if (strncmp(messages, "on", 2) == 0) {
		strcpy(sirf_status, "on");
		gps_power_on();
	} else if (strncmp(messages, "reset", 5) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2){
			gps_reset(flag);
		}
	} else if (strncmp(messages, "sirfon", 6) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2){
			gps_on_off(flag);
		}
	} else
		pr_info("usage: echo {on/off} > /proc/driver/sirf\n");

	return len;
}

static const struct file_operations gps_proc_fops = {
	.owner = THIS_MODULE,
	.read = sirf_read_proc,
	.write = sirf_write_proc,
};

static void create_sirf_proc_file(void)
{
	struct proc_dir_entry *sirf_proc_file = NULL;

	/*
	 * CSR and Marvell GPS lib will both use this file
	 * "/proc/drver/gps" may be modified in future
	 */
	sirf_proc_file = proc_create("driver/sirf", 0644, NULL,&gps_proc_fops);
	if (!sirf_proc_file) {
		pr_err("sirf proc file create failed!\n");
		return;
	}
}


#endif
// GPS for marvell

#endif

const u8	mms_ts_keycode[] = {KEY_RECENT, KEY_BACK};

static struct i2c_board_info i2c0_boardinfo[] = {
	{I2C_BOARD_INFO("sensor_main",0x3C),},
	{I2C_BOARD_INFO("sensor_sub",0x21),},
};

#if defined (CONFIG_TOUCHSCREEN_IST3038)
static struct tsp_platform_data ist30xx_info = {
	.gpio = GPIO_TOUCH_IRQ,
};
#endif

#if defined (CONFIG_TOUCHSCREEN_MMS136)||defined (CONFIG_TOUCHSCREEN_MMS134)
static struct mms_ts_platform_data mms_ts_info = {
	.max_x = 480,
	.max_y = 800,
	.use_touchkey = 1,
	.touchkey_keycode = mms_ts_keycode,
	.gpio_sda = 74,
	.gpio_scl = 73,
	.gpio_int = 82,
	.vdd_on = mms_ts_vdd_enable,
	.tkey_led_vdd_on = touchkey_led_vdd_enable
};
#endif

static struct i2c_board_info i2c1_boardinfo[] = {
#if defined (CONFIG_TOUCHSCREEN_MMS136)||defined (CONFIG_TOUCHSCREEN_MMS134)
	{
		I2C_BOARD_INFO("mms_ts", 0x48),
		.platform_data = &mms_ts_info
	},
#endif
#if defined (CONFIG_TOUCHSCREEN_IST3038)
	{
		I2C_BOARD_INFO("IST30XX", 0x50),
		.platform_data =&ist30xx_info,
	},
#endif
};
#if defined(CONFIG_SENSORS)

#if defined(CONFIG_SENSORS_GP2A)
/* -------------------------------------------------------------------------
 ** GP2A PROXIMITY SENSOR PLATFORM-SPECIFIC CODE AND DATA
 ** ------------------------------------------------------------------------- */
struct regulator *gp2a_prox_led_reg;
struct regulator *gp2a_prox_reg;

static int __init gp2a_setup(struct i2c_client *client);
static int  gp2a_power(bool on);

static struct gp2a_platform_data gp2a_plat_data = {
	.vout_gpio = GPIO_PROX_INT,
	.hw_setup = gp2a_setup,
	.hw_pwr = gp2a_power,
	.wakeup = true,
};

static int __init gp2a_setup(struct i2c_client *client)
{
	int err;

	gp2a_prox_led_reg = regulator_get(&client->dev, "vddrf1");
	if (IS_ERR(gp2a_prox_led_reg)) {
		pr_err("[%s] Failed to get v-prox-led-3.3v regulator for gp2a\n", __func__);
		err = PTR_ERR(gp2a_prox_led_reg);
		goto err;
	}

	regulator_set_voltage(gp2a_prox_led_reg, 2950000, 2950000);

	gp2a_prox_reg = regulator_get(&client->dev, "vddcama");
	if (IS_ERR(gp2a_prox_reg)) {
		pr_err("[%s] Failed to get v-prox-3.3v regulator for gp2a\n", __func__);
		err = PTR_ERR(gp2a_prox_reg);
		gp2a_prox_reg = NULL;
		goto err;
	}

	regulator_set_voltage(gp2a_prox_reg, 3000000, 3000000);

	gp2a_power(true);

	msleep(1); /* wait for chip power settle */

return 0;

err:
	return err;
}

static int gp2a_power(bool on)
{
	if (!IS_ERR(gp2a_prox_led_reg))	{
		if (on)
			regulator_enable(gp2a_prox_led_reg);
		else
			regulator_disable(gp2a_prox_led_reg);
	}

	if (!IS_ERR(gp2a_prox_reg)) {
		if (on)
			regulator_enable(gp2a_prox_reg);
		else
			regulator_disable(gp2a_prox_reg);
	}
	return 0;
}

#endif

#if defined(CONFIG_SENSORS_BMA2X2)
static struct bosch_sensor_specific	__initdata bss_bma2x2 = {
	.name = "bma2x2",
	.place = 7,
	.irq = GPIO_ACC_INT,
};
#endif

#if defined(CONFIG_SENSORS_BMA254)
static struct bosch_sensor_specific	__initdata bss_bma254 = {
	.name = "bma2x2",
#ifdef CONFIG_MACH_SHARK
	.place = 7,
#else
#ifdef CONFIG_BOSCH_ACCELEROMETER_POSITION
	.place = CONFIG_BOSCH_ACCELEROMETER_POSITION,
#else
	.place = 1,
#endif
#endif
	.irq = GPIO_ACC_INT,
};
#endif

#if defined(CONFIG_SENSORS_ACCELOMETER_BMA25X)
static void bma255_get_position(int *pos)
{
	*pos = 7;
}
static struct bma255_platform_data bma255_pdata = {
	.get_pos = bma255_get_position,
	.acc_int1 = GPIO_ACC_INT,
};
#endif //CONFIG_SENSORS_ACCELOMETER_BMA25X

#if defined(CONFIG_SENSORS_BMM050)
static struct bosch_sensor_specific	__initdata bss_bmm = {
	.name = "bmm050",
	.place = 7,
};
#endif

static struct i2c_board_info i2c2_boardinfo[] = {
#if defined(CONFIG_SENSORS_GP2A)
{
	/* for gp2a proximity driver */
	I2C_BOARD_INFO("gp2ap002a00f", 0x44),
	.platform_data = &gp2a_plat_data,
},
#endif

#if defined(CONFIG_SENSORS_BMA2X2)
{
	/* for bma2x2 accelerometer  driver */
	I2C_BOARD_INFO("bma2x2", 0x10),
	.platform_data = &bss_bma2x2,
},
#endif

#if defined(CONFIG_SENSORS_BMA254)
{
	/* for bma2x2 accelerometer  driver */
#ifdef CONFIG_MACH_SHARK
	I2C_BOARD_INFO("bma2x2", 0x10),
#else
	I2C_BOARD_INFO("bma2x2", 0x18),
#endif
	.platform_data = &bss_bma254,
},
#endif

#if defined(CONFIG_SENSORS_ACCELOMETER_BMA25X)
{

	I2C_BOARD_INFO("bma255-i2c", 0x18),
	.platform_data = &bma255_pdata,
},
#endif

#if defined(CONFIG_SENSORS_BMM050)
{
	/* for bmm050 magnetometer  driver */
	I2C_BOARD_INFO("bmm050", 0x12),
	.platform_data = &bss_bmm,
},
#endif
};

#endif

static int sc8810_add_i2c_devices(void)
{
#if defined(CONFIG_SENSORS)
#if defined(CONFIG_SENSORS_GP2A)
	i2c2_boardinfo[0].irq = gpio_to_irq(GPIO_PROX_INT);
#endif
	i2c_register_board_info(2, i2c2_boardinfo, ARRAY_SIZE(i2c2_boardinfo));
#endif
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
	i2c_register_board_info(0, i2c0_boardinfo, ARRAY_SIZE(i2c0_boardinfo));
#if defined(CONFIG_BATTERY_SAMSUNG)

#define GPIO_114 114
#define KANAS_FG_IRQ  GPIO_114

	gpio_request(KANAS_FG_IRQ, "RT5033_FG_ALERT");
	sec_battery_pdata.fg_irq = gpio_to_irq(KANAS_FG_IRQ);
	i2c_register_board_info(6, sec_brdinfo_fuelgauge, ARRAY_SIZE(sec_brdinfo_fuelgauge));
	i2c_register_board_info(8, rt5033_i2c_devices, ARRAY_SIZE(rt5033_i2c_devices));
#ifdef CONFIG_MFD_RT8973
	i2c_register_board_info(7, rtmuic_i2c_boardinfo, ARRAY_SIZE(rtmuic_i2c_boardinfo));
#endif
	if (system_rev >= 0x06) {
		sec_battery_pdata.temp_adc_table = temp_table2;
		sec_battery_pdata.temp_adc_table_size =
			sizeof(temp_table2)/sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = temp_table2;
		sec_battery_pdata.temp_amb_adc_table_size =
			sizeof(temp_table2)/sizeof(sec_bat_adc_table_data_t);

		sec_battery_pdata.temp_high_threshold_event = 600;
	       sec_battery_pdata.temp_high_recovery_event = 470;
		sec_battery_pdata.temp_low_threshold_event = -50;
		sec_battery_pdata.temp_low_recovery_event = 0;
	       sec_battery_pdata.temp_high_threshold_normal = 600;
	       sec_battery_pdata.temp_high_recovery_normal = 470;
	       sec_battery_pdata.temp_low_threshold_normal = -50;
	       sec_battery_pdata.temp_low_recovery_normal = 0;
	       sec_battery_pdata.temp_high_threshold_lpm = 600;
	       sec_battery_pdata.temp_high_recovery_lpm = 470;
	       sec_battery_pdata.temp_low_threshold_lpm = -50;
	       sec_battery_pdata.temp_low_recovery_lpm = 0;
	}
#endif
	return 0;
}

static int customer_ext_headphone_ctrl(int id, int on)
{
	static bool is_init = false;
	int ret;

	if (id != SPRD_AUDIO_ID_HEADPHONE) {
		return HOOK_BPY;
	}

	if (!is_init) {
		ret = gpio_request(HEADSET_AMP_GPIO, "headset amplifier");
		if (ret) {
			pr_err("%s: request gpio error\n", __func__);
			return NO_HOOK;
		}

		gpio_direction_output(HEADSET_AMP_GPIO, 0);

		is_init = true;
	}

	/* set heaset amplifier */
	if (!on) {
		gpio_set_value(HEADSET_AMP_GPIO, 0);
	}
	else {
		gpio_set_value(HEADSET_AMP_GPIO, 1);
	}

	return HOOK_OK;
}

static int sc8810_add_misc_devices(void)
{
	//static struct sprd_audio_ext_hook audio_hook = {0};
	//audio_hook.ext_headphone_ctrl = customer_ext_headphone_ctrl;

	//sprd_ext_hook_register(&audio_hook);

	return 0;
}

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

static unsigned int __sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}
#ifdef CONFIG_MACH_SHARK
static struct platform_ktd253b_backlight_data ktd253b_data = {
        .max_brightness = 255,
        .dft_brightness = 50,
        .ctrl_pin       = 214,
};
#else
static struct platform_rt4502_backlight_data rt4502_data = {
        .max_brightness = 255,
        .dft_brightness = 50,
        .ctrl_pin       = 214,
};
#endif

/*i2s0 config for BT, use pcm mode*/
static struct i2s_config i2s0_config = {
#if 0
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
#else
	.hw_port = 0,
	    .fs = 8000,
	    .slave_timeout = 0xF11,
	    .bus_type = I2S_BUS,
	    .byte_per_chan = I2S_BPCH_16,
    .mode = I2S_MASTER,
	    .lsb = I2S_MSB,
    .rtx_mode = I2S_RTX_MODE,
	    .sync_mode = I2S_LRCK,
	    .lrck_inv = I2S_L_LEFT,
    .clk_inv = I2S_CLK_R,
	    .i2s_bus_mode = I2S_COMPATIBLE,
	    .tx_watermark = 12,
	    .rx_watermark = 20,

#endif
};
/*you can change param here to config the i2s modules*/
static struct i2s_config i2s1_config = {0};
static struct i2s_config i2s2_config = {0};
static struct i2s_config i2s3_config = {0};

static void __init sc8830_init_machine(void)
{
	printk("sci get chip id = 0x%x\n",__sci_get_chip_id());

	sci_adc_init((void __iomem *)ADC_BASE);
	sci_regulator_init();
	sprd_add_otg_device();
	platform_device_add_data(&sprd_serial_device0,(const void*)&plat_data0,sizeof(plat_data0));
	platform_device_add_data(&sprd_serial_device1,(const void*)&plat_data1,sizeof(plat_data1));
	platform_device_add_data(&sprd_serial_device2,(const void*)&plat_data2,sizeof(plat_data2));
	platform_device_add_data(&sprd_serial_device3,(const void*)&plat_data3,sizeof(plat_data3));
	platform_device_add_data(&sprd_audio_i2s0_device,(const void*)&i2s0_config,sizeof(i2s0_config));
	platform_device_add_data(&sprd_audio_i2s1_device,(const void*)&i2s1_config,sizeof(i2s1_config));
	platform_device_add_data(&sprd_audio_i2s2_device,(const void*)&i2s2_config,sizeof(i2s2_config));
	platform_device_add_data(&sprd_audio_i2s3_device,(const void*)&i2s3_config,sizeof(i2s3_config));
	if(3> system_rev)
		platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data_rev02,sizeof(sci_keypad_data_rev02));
	else
		platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data,sizeof(sci_keypad_data));
#ifdef CONFIG_MACH_SHARK
	platform_device_add_data(&sprd_backlight_device,&ktd253b_data,sizeof(ktd253b_data));
#else
	platform_device_add_data(&sprd_backlight_device,&rt4502_data,sizeof(rt4502_data));
#endif
	#ifdef CONFIG_SEC_THERMISTOR
    platform_device_register(&sec_device_thermistor);
    #endif
	platform_add_devices(devices, ARRAY_SIZE(devices));
	sc8810_add_i2c_devices();
	sc8810_add_misc_devices();
	hawaii_bt_init();
#ifdef CONFIG_NFC_PN547
    pn547_i2c_device_register();
#endif
	sec_class = class_create(THIS_MODULE, "sec");
    if (IS_ERR(sec_class)) {
        pr_err("Failed to create class(sec)!\n");
		printk("Failed create class \n");
        return PTR_ERR(sec_class);
	}
// GPS for marvell
#ifdef CONFIG_PROC_FS
	/* create proc for gps GPS control */
	create_sirf_proc_file();
#endif
// GPS for marvell

}

static int _Sensor_K_SetFlash(uint32_t flash_mode)
{
	struct flashlight_device* flash_ptr = NULL;
	flash_ptr = find_flashlight_by_name("rt-flash-led");
	if(NULL == flash_ptr)
	{
		printk("_Sensor_K_SetFlash: flash_ptr is PNULL  \n");
		return -1;
	}
	//flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_FLASH);

	switch (flash_mode) {
	case 1:
flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_TORCH);
		flashlight_strobe(flash_ptr);
		break;
	case 2:        /*for torch */
		 flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_TORCH);
            flashlight_strobe(flash_ptr);
		break;
	case 0x11:
	flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_FLASH);
		flashlight_strobe(flash_ptr);
		break;
	case 0x10:     /*close flash */
	case 0x0:
		flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_OFF);
		break;
	default:
		printk("_Sensor_K_SetFlash unknow mode:flash_mode 0x%x \n", flash_mode);
		break;
	}

	printk("_Sensor_K_SetFlash: sp8830ssw flash_mode 0x%x  \n", flash_mode);

	return 0;
}
static void sprd_set_mic_bias(int uv)
{
	struct regulator *regu;
	int ret;

	regu = regulator_get(NULL, "VMICBIAS");
	ret = IS_ERR(regu);
	if (!IS_ERR(regu)) {
		regulator_set_voltage(regu, uv, uv);
		regulator_put(regu);
	} else{
		pr_err("sprd_set_mic_bias error  %d!", ret);
	}
}
static void __init sc8830_init_late(void)
{
	platform_add_devices(late_devices, ARRAY_SIZE(late_devices));
	sprd_set_mic_bias(2730000);
}

extern void __init  sci_enable_timer_early(void);
static void __init sc8830_init_early(void)
{
	/* earlier init request than irq and timer */
	__clock_init_early();
	sci_enable_timer_early();
	sci_adi_init();

	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
	s_sensor_project_func.SetFlash = _Sensor_K_SetFlash;
}
extern struct smp_operations sprd_smp_ops;

MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_early	= sc8830_init_early,
//	.handle_irq	= gic_handle_irq,
	.init_irq	= sci_init_irq,
	.init_time		= sci_timer_init,
	.init_machine	= sc8830_init_machine,
	.init_late	= sc8830_init_late,
MACHINE_END

