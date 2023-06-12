/* arch/arm/mach-sc8825/board-logantd-battery.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
#include <mach/sci_glb_regs.h>
#include <mach/adi.h>

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/gpio_event.h>

#include <mach/globalregs.h>
#include <mach/gpio.h>
#include <mach/usb.h>
#include <mach/adc.h>
#include <mach/sci.h>
#include <mach/pinmap.h>
#include <asm/io.h>

#include "board-vivalto3g.h"

#define BATT_DETECT 43
#define SEC_STC3117_I2C_ID 4
#define SEC_STC3117_I2C_SLAVEADDR (0xE0 >> 1)
#define ADC_CHANNEL_TEMP        3
#define STBC_LOW_BATT 59

#define SEC_BATTERY_PMIC_NAME ""

#define USB_DM_GPIO 215
#define USB_DP_GPIO 216

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

/* cable state */
bool is_cable_attached;
unsigned int lpcharge;
unsigned int lp_boot_mode;
EXPORT_SYMBOL(lpcharge);

static struct s3c_adc_client *temp_adc_client;

static sec_bat_adc_table_data_t logantd_temp_table[] = {
	{1031 ,  650},	/* 65  */
	{1178 ,  600},	/* 60  */
	{1339 ,  550},	/* 55  */
	{1511 ,  500},	/* 50  */
	{1658 ,  460},/* 46  */
	{1892 ,  400},	/* 40  */
	{2080 ,  350},	/* 35  */
	{2289 ,  300},	/* 30  */
	{2484 ,  250},	/* 25  */
	{2661 ,  200},	/* 20  */
	{2825 ,  150},	/* 15  */
	{2990 ,  100},	/* 10  */
	{3118 ,  50},	/* 5  */
	{3248 ,   0},	/* 0   */
	{3364 , -50},	/* -5  */
	{3462 , -100},	/* -10  */
	{3528 , -150},	/* -15 */
	{3587 , -200},	/* -20 */
};

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

static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UPS */
	{600,   600,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{450,   450,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{450,   450,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{450,   450,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{450,   450,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{550,   500,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BMS */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{500,   500,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_WPC */
	{600,   600,    120,    20 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	30,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	1800,    /* SLEEP */
};

static struct power_supply *charger_supply;
extern bool is_jig_on;
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
EXPORT_SYMBOL(current_cable_type);
//u8 attached_cable;

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
	return true;
}

/* Get LP charging mode state */

static int battery_get_lpm_state(char *str)
{
	get_option(&str, &lpcharge);
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);
	lp_boot_mode = lpcharge;

	return lpcharge;
}
__setup("lpcharge=", battery_get_lpm_state);

static bool sec_bat_is_lpm(void)
{
	return lpcharge == 1 ? true : false;
}

static bool sec_bat_check_jig_status(void)
{
	return is_jig_on;
}
#if 0
void sec_charger_cb(u8 attached)
{
	return 0;
}
#endif
static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret;

	return current_cable_type;
}

#define EIC_KEY_POWER_2                (A_EIC_START + 7)
#define EIC_VCHG_OVI            (A_EIC_START + 6)

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
		value.intval =
			POWER_SUPPLY_TYPE_BATTERY;
			psy_do_property("sec-charger", set,
				POWER_SUPPLY_PROP_ONLINE, value);
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

#define CHECK_BATTERY_INTERVAL 358

int battery_updata(void)
{
	pr_info("[%s] start\n", __func__);

	int ret_val;
	static int BatteryCheckCount = 0;

	if ((current_cable_type == POWER_SUPPLY_TYPE_USB) ||
	    (current_cable_type == POWER_SUPPLY_TYPE_MAINS)) {
		ret_val = 1;
	} else if(BatteryCheckCount > CHECK_BATTERY_INTERVAL) {
		ret_val = 1;
	} else {
		ret_val = 0;
	}

	return ret_val;
}

int Temperature_fn(void)
{
	return (25);
}
#ifdef CONFIG_FUELGAUGE_STC3117
static struct battery_data_t stc3117_battery_data[] = {
		{
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 10,      /* SOC alm level %*/
		.Alm_Vbat = 3600,   /* Vbat alm level mV*/
		.CC_cnf = 302,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 408,      /* nominal VM cnf , coming from battery characterisation*/
		.Rint = 266,			/* nominal internal impedance*/
  		.Cnom = 1500,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 75, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

        /* Elentec Co Ltd Battery pack - 80 means 8% */
        .CapDerating[6] = 200,              /* capacity derating in 0.1%, for temp = -20C */
        .CapDerating[5] = 120,              /* capacity derating in 0.1%, for temp = -10C */
        .CapDerating[4] = 50,              /* capacity derating in 0.1%, for temp = 0C */
        .CapDerating[3] = 20,              /* capacity derating in 0.1%, for temp = 10C */
        .CapDerating[2] = 0,              /* capacity derating in 0.1%, for temp = 25C */
        .CapDerating[1] = 0,              /* capacity derating in 0.1%, for temp = 40C */
        .CapDerating[0] = 0,              /* capacity derating in 0.1%, for temp = 60C */

        .OCVValue[15] = 4306,            /* OCV curve adjustment */
        .OCVValue[14] = 4190,            /* OCV curve adjustment */
        .OCVValue[13] = 4090,            /* OCV curve adjustment */
        .OCVValue[12] = 3981,            /* OCV curve adjustment */
        .OCVValue[11] = 3957,            /* OCV curve adjustment */
        .OCVValue[10] = 3917,            /* OCV curve adjustment */
        .OCVValue[9] = 3830,             /* OCV curve adjustment */
        .OCVValue[8] = 3796,             /* OCV curve adjustment */
        .OCVValue[7] = 3772,             /* OCV curve adjustment */
        .OCVValue[6] = 3762,             /* OCV curve adjustment */
        .OCVValue[5] = 3745,             /* OCV curve adjustment */
        .OCVValue[4] = 3712,             /* OCV curve adjustment */
        .OCVValue[3] = 3687,             /* OCV curve adjustment */
        .OCVValue[2] = 3679,             /* OCV curve adjustment */
        .OCVValue[1] = 3629,             /* OCV curve adjustment */
        .OCVValue[0] = 3300,             /* OCV curve adjustment */

            /*if the application temperature data is preferred than the STC3117 temperature*/
        .ExternalTemperature = Temperature_fn, /*External temperature fonction, return C*/
        .ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3117 temperature */
	}
};
#endif

#ifdef CONFIG_FUELGAUGE_SPRD2713
static struct battery_data_t sprd2713_battery_data[] = {
{
	.vmode = 0,       /* 1=Voltage mode, 0=mixed mode */
	.alm_soc = 5,     /* SOC alm level %*/
	.alm_vbat = 3470,    /* Vbat alm level mV*/
	.rint =250,		 /*battery internal impedance*/
	.cnom = 1500,        /* nominal capacity in mAh */
	.rsense_real = 202,      /* sense resistor 0.1mOhm from real environment*/
	.rsense_spec = 200,      /* sense resistor 0.1mOhm from specification*/
	.relax_current = 50, /* current for relaxation in mA (< C/20) */
	.externaltemperature = Temperature_fn,  /*External temperature fonction, return C*/
	.ocv_table = {
	{4330, 100}
	,
	{4251, 95}
	,
	{4188, 90}
	,
	{4131, 85}
	,
	{4085, 80}
	,
	{4041, 75}
	,
	{3997, 70}
	,
	{3956, 65}
	,
	{3911, 60}
	,
	{3861, 55}
	,
	{3830, 50}
	,
	{3808, 45}
	,
	{3789, 40}
	,
	{3776, 35}
	,
	{3769, 30}
	,
	{3760, 25}
	,
	{3737, 20}
	,
	{3691, 15}
	,
	{3651, 10}
	,
	{3582, 5}
	,
	{SPRDFGU_BATTERY_SHUTDOWN_VOL, 0}
	,
	}    /* OCV curve adjustment */

}
};
#endif

static void sec_bat_check_vf_callback(void)
{
	return;
}

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
        int data;
        int ret = 0;

	data = sci_adc_get_value(ADC_CHANNEL_TEMP, false);
	return data;
}
static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }

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

	.adc_check_count = 5,

	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,      /* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_NONE,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,      /* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
#ifdef CONFIG_FUELGAUGE_STC3117
	.battery_data = (void *)stc3117_battery_data,
#else
	.battery_data = (void *)sprd2713_battery_data,
#endif
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_CALLBACK |
	SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = false,
	.event_waiting_time = 60,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_WORKQUEUE,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_CHARGER,
	.check_count = 0,

	/* Battery check by ADC */
	.check_adc_max = 0,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_adc_table = logantd_temp_table,
	.temp_adc_table_size =
	sizeof(logantd_temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = logantd_temp_table,
	.temp_amb_adc_table_size =
		sizeof(logantd_temp_table)/sizeof(sec_bat_adc_table_data_t),

        .temp_check_count = 1,
        .temp_high_threshold_event = 600,
        .temp_high_recovery_event = 490,
        .temp_low_threshold_event = -50,
        .temp_low_recovery_event = 20,
        .temp_high_threshold_normal = 600,
        .temp_high_recovery_normal = 490,
        .temp_low_threshold_normal = -50,
        .temp_low_recovery_normal = 20,
        .temp_high_threshold_lpm = 600,
        .temp_high_recovery_lpm = 490,
        .temp_low_threshold_lpm = -50,
        .temp_low_recovery_lpm = 20,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 95,
	.full_condition_vcell = 4300,

	.recharge_condition_type =
                SEC_BATTERY_RECHARGE_CONDITION_VCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4300,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
        .recharging_total_time = 90 * 60,
        .charging_reset_time = 0,

	/* Fuel Gauge */
	.fuelgauge_name = "sec-fuelgauge",
#ifdef CONFIG_FUELGAUGE_STC3117
	.fg_irq = STBC_LOW_BATT,
#endif
	.fg_irq_attr = IRQF_TRIGGER_FALLING |
	IRQF_TRIGGER_RISING,
	//.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
	SEC_FUELGAUGE_CAPACITY_TYPE_RAW,
	.capacity_max = 1000,
	.capacity_min = 0,
	.capacity_max_margin = 0,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	//.chg_irq = 0,
	.chg_irq_attr = IRQF_NO_SUSPEND | IRQF_ONESHOT,

	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

static struct platform_device sec_device_charger = {
	.name = "sec-charger",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

#ifdef CONFIG_FUELGAUGE_SPRD2713
static struct platform_device sec_device_fuelgauge = {
	.name = "sec-fuelgauge",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};
#endif

#ifdef CONFIG_FUELGAUGE_STC3117
static struct i2c_board_info sec_brdinfo_stc3115[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				SEC_STC3117_I2C_SLAVEADDR),
		.platform_data = &sec_battery_pdata,
	},
};
#endif

static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_battery,
	&sec_device_charger,
#ifdef CONFIG_FUELGAUGE_SPRD2713
	&sec_device_fuelgauge,
#endif
};

void __init sprd_battery_init(void)
{
	pr_info("%s: vivalto battery init\n", __func__);
#ifdef CONFIG_FUELGAUGE_STC3117
	i2c_register_board_info(SEC_STC3117_I2C_ID, sec_brdinfo_stc3115,
				ARRAY_SIZE(sec_brdinfo_stc3115));
#endif
	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	__raw_writel((BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_IE),  SCI_ADDR(SPRD_PIN_BASE, 0x00D0));
}

#if 0 /* using muic model, disable */
static int charger_is_adapter(void)
{
	int ret = 0;
	int charger_status;

	charger_status = sci_adi_read(ANA_REG_GLB_CHGR_STATUS)
	    & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);

	switch (charger_status) {
	case BIT_CDP_INT:
		ret = 0;
		break;
	case BIT_DCP_INT:
		ret = 1;
		break;
	case BIT_SDP_INT:
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}


static int charger_plugin(int usb_cable, void *data)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("charger plug in interrupt happen[%d]\n", usb_cable);

	if (usb_cable || !charger_is_adapter()) {
		value.intval = POWER_SUPPLY_TYPE_USB;
	} else {
		value.intval = POWER_SUPPLY_TYPE_MAINS;
	}

	current_cable_type = value.intval;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	return 0;
}

static int charger_plugout(int usb_cable, void *data)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	value.intval = POWER_SUPPLY_TYPE_BATTERY;
	current_cable_type = value.intval;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

	pr_info("charger plug out interrupt happen\n");

	return 0;
}

static struct usb_hotplug_callback charger_cb = {
	.plugin = charger_plugin,
	.plugout = charger_plugout,
	.data = NULL,
};

void __init sprd_stbc_init(void)
{
	pr_info("[%s] charger callback\n", __func__);
	int ret;
	ret = usb_register_hotplug_callback(&charger_cb);
}
late_initcall(sprd_stbc_init);
#endif
