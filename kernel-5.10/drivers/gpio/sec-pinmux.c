/*
 * drivers/gpio/sec-pinmux.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
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

#include <linux/errno.h>
#include <linux/sec-pinmux.h>
#include "gpiolib.h"
#include "../pinctrl/mediatek/pinctrl-paris.h"

#define GET_RESULT_GPIO(a, b, c)        \
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

DEFINE_SPINLOCK(sec_gpio_lock);

extern struct mtk_pinctrl *sec_gpiochip_get_data(void);
static unsigned char *checkgpiomap_result[GDVS_PHONE_STATUS_MAX];
static struct gpio_dvs *gdvs;

static struct gpiomap_result gpiomap_result;
/*
 **************************************************************
  Pre-defined variables. Defined at probe function (DO NOT CHANGE THIS!!)
  static struct gpiomap_result gpiomap_result = {
  .init = checkgpiomap_result[PHONE_INIT],
  .sleep = checkgpiomap_result[PHONE_SLEEP]
  };
 **************************************************************
 */

static int mtk_hw_get_value_wrap(struct mtk_pinctrl *hw, unsigned int gpio,
				int field)
{
	const struct mtk_pin_desc *desc;
	int value, err;

	if (gpio > hw->soc->npins)
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	err = mtk_hw_get_value(hw, desc, field, &value);
	if (err)
		return err;

	return value;
}

#define mtk_pctrl_get_pinmux(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_MODE)

#define mtk_pctrl_get_direction(hw, gpio)		\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DIR)

#define mtk_pctrl_get_out(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DO)

#define mtk_pctrl_get_in(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DI)

static void mt_check_gpio_status(unsigned char phonestate)
{
	struct mtk_pinctrl *hw = NULL;
	struct gpiomux_setting val;
	const struct mtk_pin_desc *desc;
	unsigned int i;
	int temp_io = 0, temp_pupd = 0, temp_level = 0;
	unsigned long flags;

	spin_lock_irqsave(&sec_gpio_lock, flags);

	hw = sec_gpiochip_get_data();

	if (!hw) {
		pr_err("%s: cannot get hw data\n", __func__);
		goto get_fail;
	}

	for (i = 0; i < gdvs->count; i++) {
		temp_io = 0;
		temp_pupd = 0;
		temp_level = 0;

		desc = (const struct mtk_pin_desc *)&hw->soc->pins[i];
		val.func = mtk_pctrl_get_pinmux(hw, i);
		val.dir = mtk_pctrl_get_direction(hw, i);
		mtk_pinconf_bias_get_combo(hw, desc, &val.pull, &val.pull_en);

		/* Check Pin-Mux Configuration */
		switch (val.func) {
		case GPIOMUX_FUNC_GPIO:
			switch (val.dir) {
			case GPIOMUX_IN:
				temp_io = GDVS_IO_IN;
				temp_level = mtk_pctrl_get_in(hw, i);
				break;
			case GPIOMUX_OUT:
				temp_io = GDVS_IO_OUT;
				temp_level = mtk_pctrl_get_out(hw, i);
				break;
			default:
				temp_io = GDVS_IO_ERR;
				break;
			}
			break;
		case GPIOMUX_FUNC_1:
		case GPIOMUX_FUNC_2:
		case GPIOMUX_FUNC_3:
		case GPIOMUX_FUNC_4:
		case GPIOMUX_FUNC_5:
		case GPIOMUX_FUNC_6:
		case GPIOMUX_FUNC_7:
			temp_io = GDVS_IO_FUNC;
			break;
		default:
			temp_io = GDVS_IO_ERR;
			break;
		}

		/* Check Pull Configuration */
		switch (val.pull_en) {
		case GPIOMUX_PULL_DIS:
		case GPIOMUX_PULL_NONE:
			temp_pupd = GDVS_PUPD_NP;
			break;
		case GPIOMUX_PULL_EN:
		case GPIOMUX_PULL1:
		case GPIOMUX_PULL2:
		case GPIOMUX_PULL3:
			switch (val.pull) {
			case GPIOMUX_PULL_DOWN:
				temp_pupd = GDVS_PUPD_PD;
				break;
			case GPIOMUX_PULL_UP:
				temp_pupd = GDVS_PUPD_PU;
				break;
			default:
				temp_pupd = GDVS_PUPD_ERR;
				break;
			}
			break;
		case GPIOMUX_PULL4:
		case GPIOMUX_PULL5:
		case GPIOMUX_PULL6:
		case GPIOMUX_PULL7:
		case GPIOMUX_PULL8:
		case GPIOMUX_PULL9:
		case GPIOMUX_PULLA:
		case GPIOMUX_PULLB:
			switch (val.pull) {
			case GPIOMUX_PULL_DOWN:
				temp_pupd = GDVS_PUPD_PD;
				break;
			case GPIOMUX_PULL_UP:
				temp_pupd = GDVS_PUPD_PU;
				break;
			case GPIOMUX_RSEL_NP:
				temp_pupd = GDVS_PUPD_NP;
				break;
			default:
				temp_pupd = GDVS_PUPD_ERR;
				break;
			}
			break;
		default:
			temp_pupd = GDVS_PUPD_ERR;
		}

		checkgpiomap_result[phonestate][i] =
		(unsigned char)GET_RESULT_GPIO(temp_io, temp_pupd, temp_level);
	}
get_fail:
	spin_unlock_irqrestore(&sec_gpio_lock, flags);
}

static void check_gpio_status(unsigned char phonestate)
{
	pr_info("[secgpio_dvs][%s] state : %s\n", __func__,
			(phonestate == PHONE_INIT) ? "init" : "sleep");

	mt_check_gpio_status(phonestate);
}

static struct gpio_dvs mt_gpio_dvs = {
	.result = &gpiomap_result,
	.check_gpio_status = check_gpio_status,
	/* This value will be updated */
	.count = 0,
};

struct platform_device secgpio_dvs_device = {
	.name   = "secgpio_dvs",
	.id             = -1,
	/*
	 ***************************************************************
	   Designate appropriate variable pointer
	   in accordance with the specification of each BB vendor.
	 ***************************************************************
	 */
	.dev.platform_data = &mt_gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] = {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
			secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}

static int sec_pinmux_gpiomap_alloc(void)
{
	checkgpiomap_result[PHONE_INIT] = kcalloc(mt_gpio_dvs.count,
										sizeof(unsigned char), GFP_KERNEL);
	if (!checkgpiomap_result[PHONE_INIT])
		return -ENOMEM;

	checkgpiomap_result[PHONE_SLEEP] = kcalloc(mt_gpio_dvs.count,
										sizeof(unsigned char), GFP_KERNEL);
	if (!checkgpiomap_result[PHONE_SLEEP])
		return -ENOMEM;

	gpiomap_result.init = checkgpiomap_result[PHONE_INIT];
	gpiomap_result.sleep = checkgpiomap_result[PHONE_SLEEP];

	return 0;
}

static int sec_pinmux_parse_dt(struct device_node *np)
{
	int ret;
	u32 val;

	if (!np) {
		pr_info("%s: no device tree\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "gpio_count", &val);
	if (ret)
		return -EINVAL;
	mt_gpio_dvs.count = (int)val;
	pr_info("%s: set gpio_count: %d\n", __func__, mt_gpio_dvs.count);

	return ret;
}

int sec_pinmux_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("%s: start\n", __func__);

	ret = secgpio_dvs_device_init();

	ret = sec_pinmux_parse_dt(pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s: fail to parsing device tree\n", __func__);
		goto probe_fail;
	}

	ret = sec_pinmux_gpiomap_alloc();
	if (ret < 0) {
		pr_err("%s: fail to allocation memory\n", __func__);
		goto probe_fail;
	}

	gdvs = &mt_gpio_dvs;

probe_fail:
	return ret;
}

static int sec_pinmux_remove(struct platform_device *pdev)
{
	int ret = 0;

	kfree(checkgpiomap_result[PHONE_INIT]);
	kfree(checkgpiomap_result[PHONE_INIT]);

	return ret;
}

static const struct of_device_id sec_pinmux_match_table[] = {
	{ .compatible = "sec_pinmux" },
	{}
};

static struct platform_driver sec_pinmux_driver = {
	.driver = {
		.name = "sec_pinmux",
		.of_match_table = sec_pinmux_match_table,
	},
	.probe = sec_pinmux_probe,
	.remove = sec_pinmux_remove,
};

static int __init sec_pinmux_init(void)
{
	return platform_driver_register(&sec_pinmux_driver);
}

static void __exit sec_pinmux_exit(void)
{
	platform_driver_unregister(&sec_pinmux_driver);
}

MODULE_DESCRIPTION("Samsung pinmux driver for GPIO DVS");
MODULE_LICENSE("GPL");

late_initcall(sec_pinmux_init);
module_exit(sec_pinmux_exit);
