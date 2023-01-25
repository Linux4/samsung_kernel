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
#include <linux/keyreset.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/mms_ts.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <mach/msm8960-gpio.h>
#include "board-8960.h"

#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3

#define GPIO_TOUCH_IRQ		11

/* touch is on i2c3 */
#define GPIO_TOUCH_SCL	17
#define GPIO_TOUCH_SDA	16
#define GPIO_LCD_TYPE	PM8921_GPIO_PM_TO_SYS(44)

/* #if !defined(CONFIG_TOUCHSCREEN_MXT224)
int touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);
#endif */
#if defined(CONFIG_SEC_PRODUCT_8960)
extern unsigned int system_rev;
#endif
static int melfas_mux_fw_flash(bool to_gpios)
{
	if (to_gpios) {
		gpio_direction_output(GPIO_TOUCH_IRQ, 0);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_direction_output(GPIO_TOUCH_SCL, 0);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_SCL, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_direction_output(GPIO_TOUCH_SDA, 0);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_SDA, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	} else {
		gpio_direction_output(GPIO_TOUCH_IRQ, 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_direction_output(GPIO_TOUCH_SCL, 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_SCL, 1,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_direction_output(GPIO_TOUCH_SDA, 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_SDA, 1,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	}

	return 0;
}

void  melfas_vdd_on(bool onoff)
{
	int ret = 0;
	/* 3.3V */
	static struct regulator *reg_l17;
	/* 1.8V */
#ifdef CONFIG_MACH_M2_VZW
	if (system_rev < BOARD_REV02) {
		if (onoff)
			gpio_direction_output(10, 1);
		else
			gpio_direction_output(10, 0);
	} else
#endif
	{
#if !defined(CONFIG_MACH_ESPRESSO_VZW) && !defined(CONFIG_MACH_ESPRESSO_ATT) \
				&& !defined(CONFIG_MACH_ESPRESSO10_VZW) \
				&& !defined(CONFIG_MACH_ESPRESSO_SPR)
		static struct regulator *reg_lvs6;

		if (!reg_lvs6) {
			reg_lvs6 = regulator_get(NULL, "8921_lvs6");
			if (IS_ERR(reg_lvs6)) {
				pr_err("could not get 8921_lvs6, rc = %ld\n",
					PTR_ERR(reg_lvs6));
				return;
			}
		}

		if (onoff) {
			ret = regulator_enable(reg_lvs6);
			if (ret) {
				pr_err("enable lvs6 failed, rc=%d\n", ret);
				return;
			}
			pr_info("tsp 1.8V on is finished.\n");
		} else {
			if (regulator_is_enabled(reg_lvs6))
				ret = regulator_disable(reg_lvs6);
			if (ret) {
				pr_err("enable lvs6 failed, rc=%d\n", ret);
				return;
			}
			pr_info("tsp 1.8V off is finished.\n");
		}
	}


	if (!reg_l17) {
		reg_l17 = regulator_get(NULL, "8921_l17");
		if (IS_ERR(reg_l17)) {
			pr_err("could not get 8921_l17, rc = %ld\n",
				PTR_ERR(reg_l17));
			return;
		}
		ret = regulator_set_voltage(reg_l17, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo17 voltage to 3.3V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(reg_l17);
		if (ret) {
			pr_err("enable l17 failed, rc=%d\n", ret);
			return;
		}
		pr_info("tsp 3.3V on is finished.\n");
	} else {
		if (regulator_is_enabled(reg_l17))
			ret = regulator_disable(reg_l17);

		if (ret) {
			pr_err("disable l17 failed, rc=%d\n", ret);
			return;
		}
		pr_info("tsp 3.3V off is finished.\n");
	}
#else
		static struct regulator *reg_l11;
		/* 2.8V */
		if (machine_is_ESPRESSO_VZW() && system_rev < BOARD_REV03) {
			if (!reg_l17) {
				reg_l17 = regulator_get(NULL, "8921_l17");
				if (IS_ERR(reg_l17)) {
					pr_err("could not get L17, rc=%ld\n",
					PTR_ERR(reg_l17));
					return;
				}
				ret = regulator_set_voltage(reg_l17,
					2800000, 2800000);
				if (ret) {
					pr_err("%s: not set L17 to 2.8V\n",
						__func__);
					return;
				}
			}

			if (onoff) {
				ret = regulator_enable(reg_l17);
				if (ret) {
					pr_err("l17 failed, rc=%d\n", ret);
					return;
				}
				pr_info("tsp 2.8V on is finished.\n");
			} else {
				if (regulator_is_enabled(reg_l17))
					ret = regulator_disable(reg_l17);

				if (ret) {
					pr_err("disable l17 failed, rc=%d\n",
					ret);
					return;
				}
				pr_info("tsp 2.8V off is finished.\n");
			}
		} else {
			if (!reg_l11) {
				reg_l11 = regulator_get(NULL, "8921_l11");
				if (IS_ERR(reg_l11)) {
					pr_err("could not get L11, rc=%ld\n",
						PTR_ERR(reg_l11));
					return;
				}
				ret = regulator_set_voltage(reg_l11,
					2800000, 2800000);
				if (ret) {
					pr_err("%s: not set L11 to 2.8V\n",
						__func__);
					return;
				}
			}

			if (onoff) {
				ret = regulator_enable(reg_l11);
				if (ret) {
					pr_err("enable l11 failed, rc=%d\n",
						ret);
					return;
				}
				pr_info("tsp 2.8V on is finished.\n");
			} else {
				if (regulator_is_enabled(reg_l11))
					ret = regulator_disable(reg_l11);

				if (ret) {
					pr_err("disable l11 failed, rc=%d\n",
						ret);
					return;
				}
				pr_info("tsp 2.8V off is finished.\n");
			}


		}
	}

#endif
}

int is_melfas_vdd_on(void)
{
	int ret;
	/* 3.3V */
	static struct regulator *reg_l17;
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
	static struct regulator *reg_l11;
	if (system_rev < BOARD_REV03) {
		if (!reg_l17) {
			reg_l17 = regulator_get(NULL, "8921_l17");
			if (IS_ERR(reg_l17)) {
				ret = PTR_ERR(reg_l17);
				pr_err("could not get 8921_l17, rc = %d\n",
					ret);
				return ret;
			}
			ret = regulator_set_voltage(reg_l17, 2800000, 2800000);
			if (ret) {
				pr_err("%s: unable to set ldo17 voltage to 3.3V\n",
					__func__);
				return ret;
			}
		}
		if (regulator_is_enabled(reg_l17))
			return 1;
		else
			return 0;

	} else {
		if (!reg_l11) {
			reg_l11 = regulator_get(NULL, "8921_l11");
			if (IS_ERR(reg_l11)) {
				ret = PTR_ERR(reg_l11);
				pr_err("could not get 8921_l11, rc = %d\n",
					ret);
				return ret;
			}
			ret = regulator_set_voltage(reg_l11,
				2800000, 2800000);
			if (ret) {
				pr_err("%s: not set L11 voltage to 3.3V\n",
					__func__);
				return ret;
			}
		}

		if (regulator_is_enabled(reg_l11))
			return 1;
		else
			return 0;
	}
#else
	if (!reg_l17) {
		reg_l17 = regulator_get(NULL, "8921_l17");
		if (IS_ERR(reg_l17)) {
			ret = PTR_ERR(reg_l17);
			pr_err("could not get 8921_l17, rc = %d\n",
				ret);
			return ret;
		}
		ret = regulator_set_voltage(reg_l17, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo17 voltage to 3.3V\n",
				__func__);
			return ret;
		}
	}
	if (regulator_is_enabled(reg_l17))
		return 1;
	else
		return 0;

#endif
}

#if defined(CONFIG_TOUCHSCREEN_MMS136) ||defined(CONFIG_TOUCHSCREEN_MMS144)
static void melfas_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP] melfas_register_callback\n");
}
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS136) || \
	defined(CONFIG_TOUCHSCREEN_MMS136_TABLET)
#define USE_TOUCHKEY 1
static u8 touchkey_keycode[] = {KEY_MENU, KEY_HOMEPAGE, KEY_BACK};
static u8 touchkey_keycode_4key[] = {KEY_BACK, KEY_HOMEPAGE, KEY_F3, KEY_MENU};
#endif

static struct mms_ts_platform_data mms_ts_pdata = {
#if defined(CONFIG_TOUCHSCREEN_MMS136) || \
	defined(CONFIG_TOUCHSCREEN_MMS136_TABLET)
#if defined(CONFIG_MACH_ESPRESSO_VZW) || defined(CONFIG_MACH_ESPRESSO_ATT) \
				|| defined(CONFIG_MACH_ESPRESSO10_VZW) \
				|| defined(CONFIG_MACH_ESPRESSO_SPR)
	.max_x		= 1024,
	.max_y		= 600,
	.config_fw_version = "I705_MELFAS_0313",
#else
	.max_x		= 480,
	.max_y		= 800,
	.register_cb = melfas_register_callback,
#endif
	.use_touchkey = USE_TOUCHKEY,
	.touchkey_keycode = touchkey_keycode,
#elif defined(CONFIG_TOUCHSCREEN_MMS144)
	.max_x		= 720,
	.max_y		= 1280,
	.gpio_lcd_type = GPIO_LCD_TYPE,
	.config_fw_version = "I747_Me_0924",
	.register_cb = melfas_register_callback,
#endif
	.mux_fw_flash	= melfas_mux_fw_flash,
	.vdd_on		= melfas_vdd_on,
	.is_vdd_on	= is_melfas_vdd_on,
	.gpio_resetb	= GPIO_TOUCH_IRQ,
	.gpio_scl	= GPIO_TOUCH_SCL,
	.gpio_sda	= GPIO_TOUCH_SDA,
	.check_module_type = false,
};

static struct i2c_board_info __initdata mms_i2c3_boardinfo_final[] = {
	{
		I2C_BOARD_INFO("mms_ts", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.platform_data = &mms_ts_pdata,
		.irq = MSM_GPIO_TO_INT(GPIO_TOUCH_IRQ),
	},
};

#ifdef CONFIG_MACH_M2_VZW
static void  mms144_init(void)
{
	if (system_rev < BOARD_REV02) {
		int ret;

		ret = gpio_request(10, "tsp_ldo_en");
		if (ret != 0) {
			pr_err("tsp ldo request failed, ret=%d", ret);
			return;
		}
	}
}
#endif

#ifdef CONFIG_MACH_JASPER
static void mms136_tkey_init(void)
{
	if (system_rev >= BOARD_REV04)
		mms_ts_pdata.touchkey_keycode = touchkey_keycode_4key;
}
#endif

void __init mms_tsp_input_init(void)
{
	int ret;

#ifdef CONFIG_MACH_M2_VZW
	mms144_init();
#endif
#ifdef CONFIG_MACH_JASPER
	mms136_tkey_init();
#endif
	ret = gpio_request(GPIO_TOUCH_IRQ, "tsp_int");
	if (ret != 0) {
		pr_err("tsp int request failed, ret=%d", ret);
		goto err_int_gpio_request;
	}
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	ret = gpio_request(GPIO_TOUCH_SCL, "tsp_scl");
	if (ret != 0) {
		pr_err("tsp scl request failed, ret=%d", ret);
		goto err_scl_gpio_request;
	}
	ret = gpio_request(GPIO_TOUCH_SDA, "tsp_sda");
	if (ret != 0) {
		pr_err("tsp sda request failed, ret=%d", ret);
		goto err_sda_gpio_request;
	}
	melfas_vdd_on(1);

	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
				mms_i2c3_boardinfo_final,
				ARRAY_SIZE(mms_i2c3_boardinfo_final));

err_sda_gpio_request:
	gpio_free(GPIO_TOUCH_SCL);
err_scl_gpio_request:
	gpio_free(GPIO_TOUCH_IRQ);
err_int_gpio_request:
	;
}
