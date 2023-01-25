/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/msm_ssbi.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/restart.h>
#include "devices.h"
#include "board-8960.h"

struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8XXX_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8XXX_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8XXX_GPIO_DISABLE(_gpio) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

#define PM8XXX_GPIO_OUTPUT(_gpio, _val) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_INPUT(_gpio, _pull) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8XXX_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_STRENGTH(_gpio, _val, _out_strength) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			_out_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

/* Initial PM8921 GPIO configurations */
static struct pm8xxx_gpio_init pm8921_gpios[] __initdata = {
	PM8XXX_GPIO_OUTPUT_VIN(21, 0, PM_GPIO_VIN_L4),
	PM8XXX_GPIO_OUTPUT_VIN(6, 1, PM_GPIO_VIN_VPH),	 /* MHL power EN_N */
	//PM8XXX_GPIO_DISABLE(7),				 /* Disable NFC */
	PM8XXX_GPIO_INPUT(16,	    PM_GPIO_PULL_UP_30), /* SD_CARD_WP */
    /* External regulator shared by display and touchscreen on LiQUID */
	PM8XXX_GPIO_OUTPUT(17,	    0),			 /* DISP 3.3 V Boost */
	PM8XXX_GPIO_OUTPUT(18,	0),	/* TABLA SPKR_LEFT_EN=off */
	PM8XXX_GPIO_OUTPUT(19,	0),	/* TABLA SPKR_RIGHT_EN=off */
	PM8XXX_GPIO_DISABLE(22),			 /* Disable NFC */
	PM8XXX_GPIO_OUTPUT(24, 0),	 /* ISP_STANDBY */
	PM8XXX_GPIO_OUTPUT_FUNC(25, 0, PM_GPIO_FUNC_2),	 /* TN_CLK */
	PM8XXX_GPIO_INPUT(26,	    PM_GPIO_PULL_UP_30), /* SD_CARD_DET_N */
	PM8XXX_GPIO_OUTPUT(43, 1),                       /* DISP_RESET_N */
	PM8XXX_GPIO_OUTPUT(42, 0),                      /* USB 5V reg enable */
	/* TABLA CODEC RESET */
	//PM8XXX_GPIO_OUTPUT_STRENGTH(34, 0, PM_GPIO_STRENGTH_MED)
};

/* Initial PM8921 MPP configurations */
static struct pm8xxx_mpp_init pm8921_mpps[] __initdata = {
	/* External 5V regulator enable; shared by HDMI and USB_OTG switches. */
	PM8XXX_MPP_INIT(7, D_INPUT, PM8921_MPP_DIG_LEVEL_VPH, DIN_TO_INT),
	PM8XXX_MPP_INIT(PM8XXX_AMUX_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
								DOUT_CTRL_LOW),
};

void __init msm8960_pm8921_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8921_gpios); i++) {
		rc = pm8xxx_gpio_config(pm8921_gpios[i].gpio,
					&pm8921_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pm8921_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8921_mpps[i].mpp,
					&pm8921_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
}

static struct pm8xxx_adc_amux pm8xxx_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"dcin", CHANNEL_DCIN, CHAN_PATH_SCALING4, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ichg", CHANNEL_ICHG, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ibat", CHANNEL_IBAT, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"usbin", CHANNEL_USBIN, CHAN_PATH_SCALING3, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"chg_temp", CHANNEL_CHG_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pa_therm1", ADC_MPP_1_AMUX8, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"xo_therm", CHANNEL_MUXOFF, CHAN_PATH_SCALING1, AMUX_RSV0,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"pa_therm0", ADC_MPP_1_AMUX3, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"dev_mpp_7", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_SEC_BOARD_THERM},  /*main_thm */
#ifdef CONFIG_SAMSUNG_JACK
	{"earjack", ADC_MPP_1_AMUX6_SCALE_DEFAULT,
		CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
#endif

#ifdef CONFIG_SEC_THERMISTOR
	{"app_thm", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_SEC_BOARD_THERM},  /*app_thm */
#endif
};

static struct pm8xxx_adc_properties pm8xxx_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data pm8xxx_adc_pdata = {
	.adc_channel            = pm8xxx_adc_channels_data,
	.adc_num_board_channel  = ARRAY_SIZE(pm8xxx_adc_channels_data),
	.adc_prop               = &pm8xxx_adc_data,
	.adc_mpp_base		= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base		= PM8921_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(104),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base	= PM8921_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base	= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
	.rtc_write_enable       = false,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
};

/* Rotate lock key is not available so use F1 */
#define KEY_ROTATE_LOCK KEY_F1

static const unsigned int keymap_liquid[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(1, 3, KEY_ROTATE_LOCK),
	KEY(1, 4, KEY_HOME),
};

static struct matrix_keymap_data keymap_data_liquid = {
	.keymap_size    = ARRAY_SIZE(keymap_liquid),
	.keymap         = keymap_liquid,
};

static struct pm8xxx_keypad_platform_data keypad_data_liquid = {
	.input_name             = "keypad_8960_liquid",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 2,
	.num_cols               = 5,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data_liquid,
};


static const unsigned int keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(0, 2, KEY_CAMERA_FOCUS),
	KEY(0, 3, KEY_CAMERA_SNAPSHOT),
};

static struct matrix_keymap_data keymap_data = {
	.keymap_size    = ARRAY_SIZE(keymap),
	.keymap         = keymap,
};

static struct pm8xxx_keypad_platform_data keypad_data = {
	.input_name             = "keypad_8960",
	.input_phys_device      = "keypad_8960/input0",
	.num_rows               = 1,
	.num_cols               = 5,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data,
};

static const unsigned int keymap_sim[] = {
	KEY(0, 0, KEY_7),
	KEY(0, 1, KEY_DOWN),
	KEY(0, 2, KEY_UP),
	KEY(0, 3, KEY_RIGHT),
	KEY(0, 4, KEY_ENTER),
	KEY(0, 5, KEY_L),
	KEY(0, 6, KEY_BACK),
	KEY(0, 7, KEY_M),

	KEY(1, 0, KEY_LEFT),
	KEY(1, 1, KEY_SEND),
	KEY(1, 2, KEY_1),
	KEY(1, 3, KEY_4),
	KEY(1, 4, KEY_CLEAR),
	KEY(1, 5, KEY_MSDOS),
	KEY(1, 6, KEY_SPACE),
	KEY(1, 7, KEY_COMMA),

	KEY(2, 0, KEY_6),
	KEY(2, 1, KEY_5),
	KEY(2, 2, KEY_8),
	KEY(2, 3, KEY_3),
	KEY(2, 4, KEY_NUMERIC_STAR),
	KEY(2, 5, KEY_UP),
	KEY(2, 6, KEY_DOWN),
	KEY(2, 7, KEY_LEFTSHIFT),

	KEY(3, 0, KEY_9),
	KEY(3, 1, KEY_NUMERIC_POUND),
	KEY(3, 2, KEY_0),
	KEY(3, 3, KEY_2),
	KEY(3, 4, KEY_SLEEP),
	KEY(3, 5, KEY_F1),
	KEY(3, 6, KEY_F2),
	KEY(3, 7, KEY_F3),

	KEY(4, 0, KEY_BACK),
	KEY(4, 1, KEY_HOME),
	KEY(4, 2, KEY_MENU),
	KEY(4, 3, KEY_VOLUMEUP),
	KEY(4, 4, KEY_VOLUMEDOWN),
	KEY(4, 5, KEY_F4),
	KEY(4, 6, KEY_F5),
	KEY(4, 7, KEY_F6),

	KEY(5, 0, KEY_R),
	KEY(5, 1, KEY_T),
	KEY(5, 2, KEY_Y),
	KEY(5, 3, KEY_LEFTALT),
	KEY(5, 4, KEY_KPENTER),
	KEY(5, 5, KEY_Q),
	KEY(5, 6, KEY_W),
	KEY(5, 7, KEY_E),

	KEY(6, 0, KEY_F),
	KEY(6, 1, KEY_G),
	KEY(6, 2, KEY_H),
	KEY(6, 3, KEY_CAPSLOCK),
	KEY(6, 4, KEY_PAGEUP),
	KEY(6, 5, KEY_A),
	KEY(6, 6, KEY_S),
	KEY(6, 7, KEY_D),

	KEY(7, 0, KEY_V),
	KEY(7, 1, KEY_B),
	KEY(7, 2, KEY_N),
	KEY(7, 3, KEY_MENU),
	KEY(7, 4, KEY_PAGEDOWN),
	KEY(7, 5, KEY_Z),
	KEY(7, 6, KEY_X),
	KEY(7, 7, KEY_C),

	KEY(8, 0, KEY_P),
	KEY(8, 1, KEY_J),
	KEY(8, 2, KEY_K),
	KEY(8, 3, KEY_INSERT),
	KEY(8, 4, KEY_LINEFEED),
	KEY(8, 5, KEY_U),
	KEY(8, 6, KEY_I),
	KEY(8, 7, KEY_O),

	KEY(9, 0, KEY_4),
	KEY(9, 1, KEY_5),
	KEY(9, 2, KEY_6),
	KEY(9, 3, KEY_7),
	KEY(9, 4, KEY_8),
	KEY(9, 5, KEY_1),
	KEY(9, 6, KEY_2),
	KEY(9, 7, KEY_3),

	KEY(10, 0, KEY_F7),
	KEY(10, 1, KEY_F8),
	KEY(10, 2, KEY_F9),
	KEY(10, 3, KEY_F10),
	KEY(10, 4, KEY_FN),
	KEY(10, 5, KEY_9),
	KEY(10, 6, KEY_0),
	KEY(10, 7, KEY_DOT),

	KEY(11, 0, KEY_LEFTCTRL),
	KEY(11, 1, KEY_F11),
	KEY(11, 2, KEY_ENTER),
	KEY(11, 3, KEY_SEARCH),
	KEY(11, 4, KEY_DELETE),
	KEY(11, 5, KEY_RIGHT),
	KEY(11, 6, KEY_LEFT),
	KEY(11, 7, KEY_RIGHTSHIFT),
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(0, 2, KEY_CAMERA_SNAPSHOT),
	KEY(0, 3, KEY_CAMERA_FOCUS),
};

static int pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

#define MAX_VOLTAGE_MV		4200
#define CHG_TERM_MA		100
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.update_time		= 60000,
	.max_voltage		= MAX_VOLTAGE_MV,
	.min_voltage		= 3200,
	.uvd_thresh_voltage	= 4050,
	.alarm_low_mv		= 3400,
	.alarm_high_mv		= 4000,
	.resume_voltage_delta	= 60,
	.resume_charge_percent	= 99,
	.term_current		= CHG_TERM_MA,
	.cool_temp		= 10,
	.warm_temp		= 45,
	.temp_check_period	= 1,
	.max_bat_chg_current	= 1100,
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage	= 4100,
	.warm_bat_voltage	= 4100,
	.thermal_mitigation	= pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(pm8921_therm_mitigation),
	.rconn_mohm		= 18,
};

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.battery_type			= BATT_UNKNOWN,
	.r_sense_uohm			= 10000,
	.v_cutoff			= 3400,
	.max_voltage_uv			= MAX_VOLTAGE_MV * 1000,
	.rconn_mohm			= 18,
	.shutdown_soc_valid_limit	= 20,
	.adjust_soc_low_threshold	= 25,
	.chg_term_ua			= CHG_TERM_MA * 1000,
	.normal_voltage_calc_ms		= 20000,
	.low_voltage_calc_ms		= 1000,
	.alarm_low_mv			= 3400,
	.alarm_high_mv			= 4000,
	.high_ocv_correction_limit_uv	= 50,
	.low_ocv_correction_limit_uv	= 100,
	.hold_soc_est			= 3,
	.enable_fcc_learning		= 1,
	.min_fcc_learning_soc		= 20,
	.min_fcc_ocv_pc			= 30,
	.min_fcc_learning_samples	= 5,
};

#define	PM8921_LC_LED_MAX_CURRENT	2	/* I = 4mA */
#define	PM8921_LC_LED_LOW_CURRENT	1	/* I = 1mA */
#define PM8XXX_LED_PWM_PERIOD		1000

#define PM8XXX_LED_PWM_DUTY_MS_PAT1_R		1
#define PM8XXX_LED_PWM_DUTY_MS_PAT1_G		1

#define PM8XXX_LED_PWM_DUTY_MS_PAT2		1

#define PM8XXX_LED_PWM_DUTY_MS_PAT3		1

#define PM8XXX_LED_PWM_DUTY_MS_PAT4		1

#define PM8XXX_LED_PWM_DUTY_MS_PAT5_R		1
#define PM8XXX_LED_PWM_DUTY_MS_PAT5_G		1
#define PM8XXX_LED_PWM_DUTY_MS_PAT5_B		1

#define PM8XXX_LED_PWM_DUTY_MS_PAT6_G		32

#define PM8XXX_LED_PWM_DUTY_MS_PAT6_B		32

#define PM8XXX_LED_PWM_DUTY_MS_PAT8		1

/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8921_led_info[] = {
	[PM8XXX_LED_PAT1_RED] = {
		.name			= "led:red_chrg",
	},
	[PM8XXX_LED_PAT1_GREEN] = {
		.name			= "led:green_chrg",
	},
	[PM8XXX_LED_PAT2_RED] = {
		.name			= "led:red_chrg_err",
	},
	[PM8XXX_LED_PAT2_GREEN] = {
		.name			= "led:green_chrg_err",
	},
	[PM8XXX_LED_PAT3_RED] = {
		.name			= "led:red_miss_noti",
	},
	[PM8XXX_LED_PAT3_GREEN] = {
		.name			= "led:green_miss_noti",
	},
	[PM8XXX_LED_PAT4_RED] = {
		.name			= "led:red_in_lowbat",
	},
	[PM8XXX_LED_PAT4_GREEN] = {
		.name			= "led:green_in_lowbat",
	},
	[PM8XXX_LED_PAT5_RED] = {
		.name			= "led:red_full_chrg",
	},
	[PM8XXX_LED_PAT5_GREEN] = {
		.name			= "led:green_full_chrg",
	},
	[PM8XXX_LED_PAT5_BLUE] = {
		.name			= "led:blue_full_chrg",
	},
	[PM8XXX_LED_PAT6_GREEN] = {
		.name			= "led:green_pwr",
	},
	[PM8XXX_LED_PAT6_BLUE] = {
		.name			= "led:blue_pwr",
	},
	[PM8XXX_LED_PAT7_RED] = {
		.name			= "led:r",
	},
	[PM8XXX_LED_PAT7_GREEN] = {
		.name			= "led:g",
	},
	[PM8XXX_LED_PAT7_BLUE] = {
		.name			= "led:b",
	},
	[PM8XXX_LED_PAT8_RED] = {
		.name			= "led:blink_red",
	},
	[PM8XXX_LED_PAT8_GREEN] = {
		.name			= "led:blink_green",
	},
	[PM8XXX_LED_PAT8_BLUE] = {
		.name			= "led:blink_blue",
	},
	[PM8XXX_LED_KB_LED] = {
		.name = "kb:backlight",
	},
};

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};


int pm8921_led0_pat1_red_pwm_duty_pcts[] = {
	100, 100
};
int pm8921_led0_pat1_green_pwm_duty_pcts[] = {
	0, 0
};

int pm8921_led0_pat2_red_pwm_duty_pcts[] = {
	0, 100
};
int pm8921_led0_pat2_green_pwm_duty_pcts[] = {
	0, 0
};

int pm8921_led0_pat3_red_pwm_duty_pcts[] = {
	0, 0
};

int pm8921_led0_pat3_green_pwm_duty_pcts[] = {
	0, 0
};

int pm8921_led0_pat3_blue_pwm_duty_pcts[] = {
	0, 100
};

int pm8921_led0_pat4_red_pwm_duty_pcts[] = {
	0, 100
};
int pm8921_led0_pat4_green_pwm_duty_pcts[] = {
	0, 0
};

int pm8921_led0_pat5_red_pwm_duty_pcts[] = {
	0, 0
};
int pm8921_led0_pat5_green_pwm_duty_pcts[] = {
	100, 100
};
int pm8921_led0_pat5_blue_pwm_duty_pcts[] = {
	0, 0
};


static int pm8921_led0_pat6_green_pwm_duty_pcts[] = {
	8, 10 , 11, 13, 15, 17, 18, 19, 20, 22, 24, 26, 28, 31, 33, 34, 37, 39,
	41, 43, 44, 46, 48, 49, 51,
};
static int pm8921_led0_pat6_blue_pwm_duty_pcts[] = {
	79, 80, 80, 81, 82, 83, 84, 85, 85, 86, 87, 88, 89, 90, 91, 92, 92, 93,
	94, 95, 96, 97, 98, 99, 100,
};

int pm8921_led0_pat8_red_pwm_duty_pcts[] = {
	0, 100
};
int pm8921_led0_pat8_green_pwm_duty_pcts[] = {
	0, 100
};
int pm8921_led0_pat8_blue_pwm_duty_pcts[] = {
	0, 100
};


static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat5_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat5_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat5_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT5_R,
	.start_idx = 0,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat5_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat5_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat5_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT5_G,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat5_red_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat5_blue_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat5_blue_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat5_blue_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT5_B,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat5_red_pwm_duty_pcts) +
			ARRAY_SIZE(pm8921_led0_pat5_green_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat4_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat4_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat4_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT4,
	.start_idx = 0,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat4_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat4_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat4_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT4,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat4_red_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat3_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat3_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat3_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT3,
	.start_idx = 0,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat3_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat3_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat3_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT3,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat3_red_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat3_blue_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat3_blue_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat3_blue_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT3,
	.start_idx =  ARRAY_SIZE(pm8921_led0_pat3_red_pwm_duty_pcts) +
			ARRAY_SIZE(pm8921_led0_pat3_green_pwm_duty_pcts),
};


static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat2_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat2_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat2_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT2,
	.start_idx = 0,
};
static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat2_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat2_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat2_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT2,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat2_red_pwm_duty_pcts),
};
static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat1_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat1_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat1_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT1_R,
	.start_idx = 0,
};
static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat1_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat1_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat1_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT1_G,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat1_red_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat6_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat6_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat6_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT6_G,
	.start_idx = 0,
};
static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat6_blue_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat6_blue_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat6_blue_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT6_B,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat6_green_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat8_red_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat8_red_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat8_red_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT8,
	.start_idx = 0,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat8_green_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat8_green_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat8_green_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT8,
	.start_idx = ARRAY_SIZE(pm8921_led0_pat8_red_pwm_duty_pcts),
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_pat8_blue_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pat8_blue_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pat8_blue_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS_PAT8,
	.start_idx =  ARRAY_SIZE(pm8921_led0_pat8_red_pwm_duty_pcts) +
			ARRAY_SIZE(pm8921_led0_pat8_green_pwm_duty_pcts),
};




static struct pm8xxx_led_config pm8921_led_configs[] = {
	/*pattern 1 Charging*/
	[PM8XXX_LED_PAT1_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat1_red_duty_cycles,
	},
	[PM8XXX_LED_PAT1_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat1_green_duty_cycles
	},
	/*pattern 2 Charging Error*/

	[PM8XXX_LED_PAT2_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat2_red_duty_cycles
	},
	[PM8XXX_LED_PAT2_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat2_green_duty_cycles
	},
	/*pattern 3 Missed Noti*/

	[PM8XXX_LED_PAT3_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat3_red_duty_cycles
	},
	[PM8XXX_LED_PAT3_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat3_green_duty_cycles
	},
	[PM8XXX_LED_PAT3_BLUE] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat3_blue_duty_cycles
	},
	/*pattern 4 Low Batt*/
	[PM8XXX_LED_PAT4_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat4_red_duty_cycles
	},
	[PM8XXX_LED_PAT4_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat4_green_duty_cycles
	},
	/*pattern 5 Fully Charged*/

	[PM8XXX_LED_PAT5_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat5_red_duty_cycles
	},
	[PM8XXX_LED_PAT5_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat5_green_duty_cycles
	},
	[PM8XXX_LED_PAT5_BLUE] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat5_blue_duty_cycles
	},

	/*pattern 6 Powering */
	[PM8XXX_LED_PAT6_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat6_green_duty_cycles
	},
	[PM8XXX_LED_PAT6_BLUE] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat6_blue_duty_cycles
	},
	/*pattern 7*/
	[PM8XXX_LED_PAT7_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},
	[PM8XXX_LED_PAT7_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},
	[PM8XXX_LED_PAT7_BLUE] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},

	/*pattern 8*/
	[PM8XXX_LED_PAT8_RED] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat8_red_duty_cycles,
	},
	[PM8XXX_LED_PAT8_GREEN] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat8_green_duty_cycles,
	},
	[PM8XXX_LED_PAT8_BLUE] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_pat8_blue_duty_cycles,
	},

	[PM8XXX_LED_KB_LED] = {
		.id = PM8XXX_ID_LED_KB_LIGHT,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = 30,
		.pwm_channel = 7,
	},

};

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
		.led_core = &pm8921_led_core_pdata,
		.configs = pm8921_led_configs,
		.num_configs = ARRAY_SIZE(pm8921_led_configs),
		.led_power_on = NULL,
};

static struct pm8xxx_ccadc_platform_data pm8xxx_ccadc_pdata = {
	.r_sense_uohm		= 10000,
	.calib_delay_ms		= 600000,
};

/**
 * PM8XXX_PWM_DTEST_CHANNEL_NONE shall be used when no LPG
 * channel should be in DTEST mode.
 */

#define PM8XXX_PWM_DTEST_CHANNEL_NONE   (-1)

static struct pm8xxx_pwm_platform_data pm8xxx_pwm_pdata = {
	.dtest_channel	= PM8XXX_PWM_DTEST_CHANNEL_NONE,
};

static struct pm8921_platform_data pm8921_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata              = &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.keypad_pdata		= &keypad_data,
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm_pm8921_regulator_pdata,
	.charger_pdata		= &pm8921_chg_pdata,
	.bms_pdata		= &pm8921_bms_pdata,
	.adc_pdata		= &pm8xxx_adc_pdata,
	.leds_pdata		= &pm8xxx_leds_pdata,
	.ccadc_pdata		= &pm8xxx_ccadc_pdata,
	.pwm_pdata		= &pm8xxx_pwm_pdata,
};

static struct msm_ssbi_platform_data msm8960_ssbi_pm8921_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8921-core",
		.platform_data		= &pm8921_platform_data,
	},
};

void __init msm8960_init_pmic(void)
{
	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;
	msm8960_device_ssbi_pmic.dev.platform_data =
				&msm8960_ssbi_pm8921_pdata;
	pm8921_platform_data.num_regulators = msm_pm8921_regulator_pdata_len;

	if (machine_is_msm8960_liquid()) {
		pm8921_platform_data.keypad_pdata = &keypad_data_liquid;
		pm8921_platform_data.bms_pdata->battery_type = BATT_DESAY;
	} else if (machine_is_msm8960_mtp()) {
		pm8921_platform_data.bms_pdata->battery_type = BATT_PALLADIUM;
	} else if (machine_is_msm8960_cdp()) {
		pm8921_chg_pdata.has_dc_supply = true;
	}

	if (machine_is_msm8960_fluid())
		pm8921_bms_pdata.rconn_mohm = 20;

	if (!machine_is_msm8960_fluid() && !machine_is_msm8960_liquid()
			&& !machine_is_msm8960_mtp())
		pm8921_chg_pdata.battery_less_hardware = 1;
}
