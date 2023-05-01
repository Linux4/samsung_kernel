/*
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <mach/msm8930-gpio.h>
#include <linux/delay.h>
#include "board-8930.h"
#include "devices-msm8x60.h"

#ifdef CONFIG_TOUCHSCREEN_MXT224E
#include <linux/i2c/mxt224e.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MXT224S
#include <linux/i2c/mxt224s.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MXTS
#include <linux/i2c/mxts_msm8930.h>
#endif

#define MSM_8930_GSBI3_QUP_I2C_BUS_ID	3

#define GPIO_TOUCH_IRQ		11
#define GPIO_TOUCH_SDA		16
#define GPIO_TOUCH_SCL		17

#ifdef CONFIG_TOUCHSCREEN_MXT224E
static void mxt224_power_onoff(int onoff)
{
	int ret = 0;

	static struct regulator *reg_l31;
	static struct regulator *reg_lvs6;

	pr_info("[TSP] power %s\n", onoff ? "on" : "off");

	if (!reg_lvs6) {
		reg_lvs6 = regulator_get(NULL, "8917_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("%s: could not get 8917_lvs6, rc = %ld\n",
				__func__, PTR_ERR(reg_lvs6));
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(reg_lvs6);
		if (ret) {
			pr_err("%s: enable lvs6 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_info("%s: tsp 1.8V on is finished.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_lvs6))
			ret = regulator_disable(reg_lvs6);
		else
			printk(KERN_ERR
				"%s: rugulator LVS6(1.8V) is disabled\n",
					__func__);
		if (ret) {
			pr_err("%s: enable lvs6 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_info("%s: tsp 1.8V off is finished.\n", __func__);
	}

	if (!reg_l31) {
		reg_l31 = regulator_get(NULL, "8917_l31");
		if (IS_ERR(reg_l31)) {
			pr_err("%s: could not get 8917_l31, rc = %ld\n",
				__func__, PTR_ERR(reg_l31));
			return;
		}
		ret = regulator_set_voltage(reg_l31, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo31 voltage to 3.3V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(reg_l31);
		if (ret) {
			pr_err("%s: enable l31 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_info("%s: tsp 3.3V on is finished.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l31))
			ret = regulator_disable(reg_l31);
		else
			printk(KERN_ERR
				"%s: rugulator is(L31(3.3V) disabled\n",
					__func__);
		if (ret) {
			pr_err("%s: disable l31 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_info("%s: tsp 3.3V off is finished.\n", __func__);
	}
	msleep(30);

	return;
}

void mxt224_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP]mxt224_register_callback\n");
}

static void mxt224_read_ta_status(bool *ta_status)
{
#if defined(CONFIG_USB_SWITCH_FSA9485)
	if (set_cable_status == CABLE_TYPE_AC
		|| set_cable_status == CABLE_TYPE_USB
		|| set_cable_status == CABLE_TYPE_NONE)
		*ta_status = set_cable_status;
#endif	
}

/*
	Configuration for MXT224-E
*/
#define MXT224E_THRESHOLD_BATT		25
#define MXT224E_THRESHOLD_CHRG		32
#define MXT224E_CALCFG_BATT		114
#define MXT224E_CALCFG_CHRG		114
#define MXT224E_ATCHFRCCALTHR_NORMAL		40
#define MXT224E_ATCHFRCCALRATIO_NORMAL		55
#define MXT224E_ATCHCALST		4
#define MXT224E_ATCHCALTHR		20
#define MXT224_MAX_MT_FINGERS 10

static u8 t7_config_e[] = {GEN_POWERCONFIG_T7,
				48, 255, 25};

static u8 t8_config_e[] = {GEN_ACQUISITIONCONFIG_T8,
				22, 0, 5, 2, 0, 0, 4, 20,
				MXT224E_ATCHFRCCALTHR_NORMAL,
				MXT224E_ATCHFRCCALRATIO_NORMAL};

static u8 t9_config_e[] = {TOUCH_MULTITOUCHSCREEN_T9,
				139, 0, 0, 19, 11, 0, 32,
				MXT224E_THRESHOLD_BATT, 2, 1, 10, 3, 1,
				46, MXT224_MAX_MT_FINGERS, 5, 40, 10, 191, 3,
				27, 2, 10, 10, 10, 10, 143, 40, 143, 80, 18,
				15, 50, 50, 2};

static u8 t15_config_e[] = {TOUCH_KEYARRAY_T15,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t18_config_e[] = {SPT_COMCONFIG_T18,
				0, 0};

static u8 t19_config_e[] = {SPT_GPIOPWM_T19,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t23_config_e[] = {TOUCH_PROXIMITY_T23,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t25_config_e[] = {SPT_SELFTEST_T25,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t40_config_e[] = {PROCI_GRIPSUPPRESSION_T40,
				0, 0, 0, 0, 0};

static u8 t42_config_e[] = {PROCI_TOUCHSUPPRESSION_T42,
				0, 0, 0, 0, 0, 0, 0, 0};

static u8 t46_config_e[] = {SPT_CTECONFIG_T46,
				0, 3, 24, 26, 0, 0, 1, 0};

static u8 t47_config_e[] = {PROCI_STYLUS_T47,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t48_config_e[] = {PROCG_NOISESUPPRESSION_T48,
				3, 132, MXT224E_CALCFG_BATT,
				25, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				6, 6, 0, 0, 32, 4, 48, 10,
				0, 100, 5, 0, 100, 0, 5,
				0, 0, 0, 0, 0, 0, 0, MXT224E_THRESHOLD_BATT,
				2, 15, 1, 80, MXT224_MAX_MT_FINGERS,
				5, 20, 245, 245, 20, 20, 160, 50, 200,
				80, 18, 15, 0 };

static u8 t48_config_chrg_e[] = {PROCG_NOISESUPPRESSION_T48,
				3, 132, MXT224E_CALCFG_CHRG,
				0, 0, 0, 0, 0, 10, 20, 0, 0, 0,
				6, 6, 0, 0, 64, 4, 64, 10,
				0, 8, 5, 0, 15, 0, 20,
				0, 0, 0, 0, 0, 0, 0, MXT224E_THRESHOLD_CHRG,
				2, 5, 2, 80, MXT224_MAX_MT_FINGERS,
				5, 20, 235, 235, 10, 10, 160, 50, 200,
				80, 18, 15, 0 };

static u8 end_config_e[] = {RESERVED_T255};

static const u8 *mxt224e_config[] = {
	t7_config_e,
	t8_config_e,
	t9_config_e,
	t15_config_e,
	t18_config_e,
	t19_config_e,
	t23_config_e,
	t25_config_e,
	t40_config_e,
	t42_config_e,
	t46_config_e,
	t47_config_e,
	t48_config_e,
	end_config_e,
};

static struct mxt224_platform_data mxt224_data = {
	.max_finger_touches = MXT224_MAX_MT_FINGERS,
	.gpio_read_done = GPIO_TOUCH_IRQ,
	.config_e = mxt224e_config,
	.min_x = 0,
	.max_x = 539,
	.min_y = 0,
	.max_y = 959,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 30,
	.atchcalst = MXT224E_ATCHCALST,
	.atchcalsthr = MXT224E_ATCHCALTHR,
	.tchthr_batt_e = MXT224E_THRESHOLD_BATT,
	.tchthr_charging_e = MXT224E_THRESHOLD_CHRG,
	.calcfg_batt_e = MXT224E_CALCFG_BATT,
	.calcfg_charging_e = MXT224E_CALCFG_CHRG,
	.atchfrccalthr_e = MXT224E_ATCHFRCCALTHR_NORMAL,
	.atchfrccalratio_e = MXT224E_ATCHFRCCALRATIO_NORMAL,
	.t48_config_batt_e = t48_config_e,
	.t48_config_chrg_e = t48_config_chrg_e,
	.power_onoff = mxt224_power_onoff,
	.register_cb = mxt224_register_callback,
	.read_ta_status = mxt224_read_ta_status,
	.config_fw_version = "E170S_At_0409",
};

/* I2C2 */
static struct i2c_board_info mxt224_info[] __initdata = {
	{
		I2C_BOARD_INFO(MXT224_DEV_NAME, 0x4a),
		.platform_data = &mxt224_data,
		.irq = MSM_GPIO_TO_INT(GPIO_TOUCH_IRQ),
	},
};

void __init input_touchscreen_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	mxt224_power_onoff(1);
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
				mxt224_info,
				ARRAY_SIZE(mxt224_info));

}
#endif

#ifdef CONFIG_TOUCHSCREEN_MXT224S
/* atmel mxt224s config value */
#define MXT224S_CHRGTIME_BATT		25
#define MXT224S_CHRGTIME_CHRG		60
#define MXT224S_THRESHOLD_BATT		0x1E
#define MXT224S_THRESHOLD_CHRG		0x28
#define MXT224S_CALCFG_BATT		210
#define MXT224S_CALCFG_CHRG		210

static u8 t7_config_s[] = { GEN_POWERCONFIG_T7,
	0x20, 0xFF, 0x32, 0x03
};

static u8 t8_config_s[] = { GEN_ACQUISITIONCONFIG_T8,
	0x1D, 0x00, 0x05, 0x01, 0x00, 0x00, 0x04, 0x19, 0x05, 0x87
};

static u8 t8_config_go_s[] = { GEN_ACQUISITIONCONFIG_T8,
	0x1D, 0x00, 0x05, 0x01, 0x00, 0x00, 0x04, 0x19, 0x00, 0x00
};


static u8 t9_config_s[] = { TOUCH_MULTITOUCHSCREEN_T9,
	0x83, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x70, 0x32, 0x02, 0x01,
	MXT224S_MAX_MT_FINGERS,
	0x0A, 0x01, 0x3F, 0x0A, 0x0F, 0x1E, 0x0A, 0x1F, 0x03,
	0xDF, 0x01, 0x00, 0x0a, 0x14, 0x14, 0x80, 0x00, 0xc0, 0x00,
	0x14, 0x0F, 0x00, 0x00, 0x00, 0x00
};

static u8 t15_config_s[] = { TOUCH_KEYARRAY_T15,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static u8 t18_config_s[] = { SPT_COMCONFIG_T18,
	0, 0
};

static u8 t19_config_s[] = { SPT_GPIOPWM_T19,
	0, 0, 0, 0, 0, 0
};

static u8 t23_config_s[] = { TOUCH_PROXIMITY_T23,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

static u8 t25_config_s[] = { SPT_SELFTEST_T25,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

static u8 t40_config_s[] = { PROCI_GRIPSUPPRESSION_T40,
	0, 0, 0, 0, 0
};

static u8 t42_config_s[] = { PROCI_TOUCHSUPPRESSION_T42,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static u8 t46_config_s[] = { SPT_CTECONFIG_T46,
	0x04, 0x00, 0x10, 0x1E, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01
};

static u8 t46_config_s_ta[] = { SPT_CTECONFIG_T46,
	0x04, 0x00, 0x10, 0x24, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01
};

static u8 t47_config_s[] = { PROCI_STYLUS_T47,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00
};

static u8 t55_config_s[] = { PROCI_ADAPTIVETHRESHOLD_T55,
	0, 0, 0, 0, 0, 0
};

static u8 t56_config_s[] = { PROCI_SHIELDLESS_T56,
	0x02, 0x00, 0x01, 0x35, 0x0C, 0x10, 0x0B, 0x10, 0x10, 0x0F,
	0x10, 0x0E, 0x10, 0x10, 0x10, 0x13, 0x13, 0x14, 0x14, 0x13,
	0x13, 0x13, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

static u8 t57_config_s[] = { PROCI_EXTRATOUCHSCREENDATA_T57,
	0xE3, 0x37, 0x00
};

static u8 t61_config_s[] = {SPT_TIMER_T61,
	0, 0, 0, 0, 0
};

static u8 t62_config_s[] = { PROCG_NOISESUPPRESSION_T62,
	0x03, 0x03, 0x00, 0x01, 0x08, 0x00, 0x10, 0x14, 0x02, 0x00,
	0x01, 0x00, 0x02, 0x00, 0x05, 0x00, 0x0A, 0x05, 0x05, 0x50,
	0x0C, 0x14, 0x28, 0x14, 0x64, 0x2A, 0x2A, 0x04, 0x64, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x50, MXT224S_THRESHOLD_BATT,
	0x02, 0x05, 0x01, 0x30, MXT224S_MAX_MT_FINGERS,
	0x0F, 0x0F, 0xF0, 0xF2, 0xEC, 0xF0, 0x35, 0x28, 0x00,
	0x00, 0x12, 0x0A, 0x00
};

static u8 t62_config_s_ta[] = { PROCG_NOISESUPPRESSION_T62,
	0x03, 0x03, 0x00, 0x01, 0x08, 0x00, 0x10, 0x0D, 0x02, 0x00,
	0x01, 0x02, 0x05, 0x06, 0x05, 0x00, 0x0A, 0x05, 0x05, 0x50,
	0x0C, 0x14, 0x30, 0x19, 0x64, 0x1E, 0x10, 0x04, 0x64, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x50, MXT224S_THRESHOLD_CHRG,
	0x02, 0x05, 0x01, 0x30, MXT224S_MAX_MT_FINGERS,
	0x0F, 0x0F, 0xF0, 0xF2, 0xEC, 0xF0, 0x35, 0x28, 0x00,
	0x00, 0x12, 0x0A, 0x00
	};


static u8 end_config_s[] = { RESERVED_T255 };

static const u8 *mxt224s_config[] = {
	t7_config_s,
	t8_config_s,
	t9_config_s,
	t15_config_s,
	t18_config_s,
	t19_config_s,
	t23_config_s,
	t25_config_s,
	t40_config_s,
	t42_config_s,
	t46_config_s,
	t47_config_s,
	t55_config_s,
	t56_config_s,
	t57_config_s,
	t61_config_s,
	t62_config_s,
	end_config_s
};

void mxt224_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP]mxt224_register_callback\n");
}

static void mxt224_request_gpio(void)
{
	int ret;
	pr_info("[TSP] request gpio\n");

	ret = gpio_request(GPIO_TSP_D_EN, "mxt_tsp_d_en");
	if (ret) {
		pr_err("[TSP]%s: unable to request mxt_tsp_d_en [%d]\n",
				__func__, GPIO_TSP_D_EN);
	}
	ret = gpio_request(GPIO_TSP_A_EN, "mxt_tsp_a_en");
	if (ret) {
		pr_err("[TSP]%s: unable to request mxt_tsp_a_en [%d]\n",
				__func__, GPIO_TSP_A_EN);
	}

}

static void mxt224_power_onoff(int onoff)
{
	int ret = 0;

	pr_info("[TSP] power %s\n", onoff ? "on" : "off");
#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev > BOARD_REV02) {
#else
		if (system_rev >= BOARD_REV00) {
#endif
		static struct regulator *reg_l31;
		static struct regulator *reg_lvs6;
		if (!reg_lvs6) {
			reg_lvs6 = regulator_get(NULL, "8917_lvs6");
			if (IS_ERR(reg_lvs6)) {
				pr_err("%s: could not get 8917_lvs6, rc = %ld\n",
					__func__, PTR_ERR(reg_lvs6));
				return;
			}
		}

		if (onoff) {
			ret = regulator_enable(reg_lvs6);
			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V on is finished.\n", __func__);
		} else {
			if (regulator_is_enabled(reg_lvs6))
				ret = regulator_disable(reg_lvs6);
			else
				printk(KERN_ERR
					"%s: rugulator LVS6(1.8V) is disabled\n",
						__func__);

			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V off is finished.\n", __func__);
		}


		if (!reg_l31) {
			reg_l31 = regulator_get(NULL, "8917_l31");
			if (IS_ERR(reg_l31)) {
				pr_err("%s: could not get 8917_l31, rc = %ld\n",
					__func__, PTR_ERR(reg_l31));
				return;
			}
			ret = regulator_set_voltage(reg_l31, 3300000, 3300000);
			if (ret) {
				pr_err("%s: unable to set ldo31 voltage to 3.3V\n",
					__func__);
				return;
			}
		}

		if (onoff) {
			ret = regulator_enable(reg_l31);
			if (ret) {
				pr_err("%s: enable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V on is finished.\n", __func__);
		} else {
			if (regulator_is_enabled(reg_l31))
				ret = regulator_disable(reg_l31);
			else
				printk(KERN_ERR
					"%s: rugulator is(L31(3.3V) disabled\n",
						__func__);
			if (ret) {
				pr_err("%s: disable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V off is finished.\n", __func__);
		}
	} else {

		ret = gpio_direction_output(GPIO_TSP_D_EN, onoff);
		if (ret) {
			pr_err("[TSP]%s: unable to set mxt_tsp_d_en [%d]\n",
					__func__, GPIO_TSP_D_EN);
		}

		ret = gpio_direction_output(GPIO_TSP_A_EN, onoff);
		if (ret) {
			pr_err("[TSP]%s: unable to set mxt_tsp_a_en [%d]\n",
					__func__, GPIO_TSP_A_EN);
		}

	}

	msleep(30);

	return;
}

struct mxt224s_platform_data mxt224s_data = {
	.max_finger_touches = MXT224S_MAX_MT_FINGERS,
	.gpio_read_done = GPIO_TOUCH_IRQ,
	.min_x = 0,
	.max_x = 479,
	.min_y = 0,
	.max_y = 799,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 30,
	.config = mxt224s_config,
	.config_s = mxt224s_config,
	.tchthr_batt = MXT224S_THRESHOLD_BATT,
	.tchthr_charging = MXT224S_THRESHOLD_CHRG,
	.t8_config_normal_s = t8_config_go_s,
	.t62_config_batt_s = t62_config_s,
	.t62_config_chrg_s = t62_config_s_ta,
	.t46_config_batt_s = t46_config_s,
	.t46_config_chrg_s = t46_config_s_ta,
	.request_gpio = mxt224_request_gpio,
	.power_onoff = mxt224_power_onoff,
	.config_fw_version = "I8730_AT_1226",
	.register_cb = mxt224_register_callback,
};

static struct i2c_board_info mxt224s_info[] __initdata = {
	{
		I2C_BOARD_INFO(MXT224S_DEV_NAME, 0x4a),
		.platform_data = &mxt224s_data,
		.irq = MSM_GPIO_TO_INT(GPIO_TOUCH_IRQ),
	},
};

void __init input_touchscreen_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	mxt224_request_gpio();
	mxt224_power_onoff(1);
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
				mxt224s_info,
				ARRAY_SIZE(mxt224s_info));

}
#endif

#ifdef CONFIG_TOUCHSCREEN_MXTS
void mxts_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP]mxts_register_callback\n");
}

#if defined(CONFIG_MACH_SERRANO) || defined(CONFIG_MACH_GOLDEN)
static void mxts_power_onoff(int onoff)
{
	int ret = 0;

	static struct regulator *reg_l31;
	static struct regulator *reg_lvs6;

	pr_info("[TSP] power %s\n", onoff ? "on" : "off");

	if (!reg_l31) {
		reg_l31 = regulator_get(NULL, "8917_l31");
		if (IS_ERR(reg_l31)) {
			pr_err("%s: could not get 8917_l31, rc = %ld\n",
				__func__, PTR_ERR(reg_l31));
			return;
		}
		ret = regulator_set_voltage(reg_l31, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo31 voltage to 3.3V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l31)) {
			ret = regulator_enable(reg_l31);
			if (ret) {
				pr_err("%s: enable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V[l31] on is finished.\n", __func__);
		} else
			pr_info("%s: tsp 3.3V[l31] is alerady on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l31)) {
			ret = regulator_disable(reg_l31);
			if (ret) {
				pr_err("%s: disable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V[l31] off is finished.\n", __func__);
		} else
			pr_info("%s: tsp 3.3V[l31] is already off.\n", __func__);
	}

	if (!reg_lvs6) {
		reg_lvs6 = regulator_get(NULL, "8917_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("%s: could not get 8917_lvs6, rc = %ld\n",
				__func__, PTR_ERR(reg_lvs6));
			return;
		}
	}

	if (onoff) {		
		if (!regulator_is_enabled(reg_lvs6)) {
			ret = regulator_enable(reg_lvs6);
			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V[lvs6] on is finished.\n", __func__);
		} else
			pr_info("%s: tsp 1.8V[lvs6] is already on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_lvs6)) {
			ret = regulator_disable(reg_lvs6);
			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V[lvs6] off is finished.\n", __func__);
		} else
			pr_info("%s: tsp 1.8V[lvs6] is already off.\n", __func__);
	}

	msleep(30);

	return;
}

#else
static void mxts_power_onoff(int onoff)
{
	int ret = 0;

	static struct regulator *reg_l31;
	static struct regulator *reg_lvs6;

	pr_info("[TSP] power %s\n", onoff ? "on" : "off");

	if (!reg_lvs6) {
		reg_lvs6 = regulator_get(NULL, "8917_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("%s: could not get 8917_lvs6, rc = %ld\n",
				__func__, PTR_ERR(reg_lvs6));
			return;
		}
	}

	if (onoff) {		
		if (!regulator_is_enabled(reg_lvs6)) {
			ret = regulator_enable(reg_lvs6);
			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V[lvs6] on is finished.\n", __func__);
		} else
			pr_info("%s: tsp 1.8V[lvs6] is already on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_lvs6)) {
			ret = regulator_disable(reg_lvs6);
			if (ret) {
				pr_err("%s: enable lvs6 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V[lvs6] off is finished.\n", __func__);
		} else
			pr_info("%s: tsp 1.8V[lvs6] is already off.\n", __func__);
	}

	if (!reg_l31) {
		reg_l31 = regulator_get(NULL, "8917_l31");
		if (IS_ERR(reg_l31)) {
			pr_err("%s: could not get 8917_l31, rc = %ld\n",
				__func__, PTR_ERR(reg_l31));
			return;
		}
		ret = regulator_set_voltage(reg_l31, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo31 voltage to 3.3V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l31)) {
			ret = regulator_enable(reg_l31);
			if (ret) {
				pr_err("%s: enable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V[l31] on is finished.\n", __func__);
		} else
			pr_info("%s: tsp 3.3V[l31] is alerady on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l31)) {
			ret = regulator_disable(reg_l31);
			if (ret) {
				pr_err("%s: disable l31 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V[l31] off is finished.\n", __func__);
		} else
			pr_info("%s: tsp 3.3V[l31] is already off.\n", __func__);
	}

	msleep(30);

	return;
}
#endif
static int mxts_power_on(void)
{
	mxts_power_onoff(1);

	return 0;
}

static int mxts_power_off(void)
{
	mxts_power_onoff(0);

	return 0;
}

static bool mxts_read_chg(void)
{
	return gpio_get_value(GPIO_TOUCH_IRQ);
}

#if defined(CONFIG_TOUCHSCREEN_MXT336S)
#define TOUCH_MAX_X	540
#define TOUCH_MAX_Y	960
#define NUM_XNODE 24
#define NUM_YNODE 14
#if defined(CONFIG_MACH_SERRANO_EUR_3G)
#define PROJECT_NAME	"I9190"
#elif defined(CONFIG_MACH_SERRANO_EUR_LTE)
#define PROJECT_NAME	"I9195"
#elif defined(CONFIG_MACH_SERRANO_KOR_LTE)
#define PROJECT_NAME	"E370D"
#elif defined(CONFIG_MACH_SERRANO_USC)
#define PROJECT_NAME	"R890"
#elif defined(CONFIG_MACH_SERRANO_VZW)
#define PROJECT_NAME	"I435"
#elif defined(CONFIG_MACH_SERRANO_SPR)
#define PROJECT_NAME	"L520"
#elif defined(CONFIG_MACH_SERRANO_TMO)
#define PROJECT_NAME	"M709"
#elif defined(CONFIG_MACH_SERRANO_ATT)
#define PROJECT_NAME	"I257"
#elif defined(CONFIG_MACH_CANE_EUR_3G)
#define PROJECT_NAME	"I8580"
#else
#define PROJECT_NAME	"336S"
#endif
#define CONFIG_VER		"0327"
#elif defined(CONFIG_TOUCHSCREEN_MXT224S_NEW)
#define TOUCH_MAX_X	480
#define TOUCH_MAX_Y	800
#define NUM_XNODE 19
#define NUM_YNODE 11
#if defined(CONFIG_MACH_GOLDEN)
#define PROJECT_NAME	"G730"
#else
#define PROJECT_NAME	"224S"
#endif
#define CONFIG_VER		"0327"
#endif

struct mxt_platform_data mxts_data = {
	.num_xnode = NUM_XNODE,
	.num_ynode = NUM_YNODE,
	.max_x = TOUCH_MAX_X - 1,
	.max_y = TOUCH_MAX_Y - 1,
	.irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.read_chg = mxts_read_chg,
	.power_on = mxts_power_on,
	.power_off = mxts_power_off,
	.register_cb = mxts_register_callback,
	.project_name = PROJECT_NAME,
	.config_ver = CONFIG_VER,
};

static struct i2c_board_info mxts_info[] __initdata = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, 0x4a),
		.platform_data = &mxts_data,
		.irq = MSM_GPIO_TO_INT(GPIO_TOUCH_IRQ),
	},
};

void __init input_touchscreen_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	mxts_power_on();
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
				mxts_info,
				ARRAY_SIZE(mxts_info));

}
#endif

