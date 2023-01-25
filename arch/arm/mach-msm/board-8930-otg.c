/*
 * arch/arm/mach-msm/board-8930-otg.c
 *
 * Copyright (c) 2012, Samsung Electronics
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

#ifdef CONFIG_USB_HOST_NOTIFY
static int msm_otg_pmic_gpio_config(int gpio, int direction,
		int pullup, char *gpio_id, int req_sel)
{
	struct pm_gpio param;
	int out_strength = 0;
	int ret = 0;

	pr_info("msm_otg: pmic_gpio: %s gpio %d, dir %d, pull %d, request %d\n",
			gpio_id, gpio, direction, pullup, req_sel);

	if (direction == PM_GPIO_DIR_IN)
		out_strength = PM_GPIO_STRENGTH_NO;
	else
		out_strength = PM_GPIO_STRENGTH_HIGH;

	param.direction = direction;
	param.output_buffer = PM_GPIO_OUT_BUF_CMOS;
	param.output_value = 0;
	param.pull = pullup;
	param.vin_sel = PM_GPIO_VIN_S4;
	param.out_strength = out_strength;
	param.function = PM_GPIO_FUNC_NORMAL;
	param.inv_int_pol = 0;
	param.disable_pin = 0;

	ret = pm8xxx_gpio_config(gpio, &param);
	if (ret < 0) {
		pr_err("msm_otg: failed to configure vbus_in gpio\n");
		return ret;
	}

	if (req_sel) {
		ret = gpio_request(gpio, gpio_id);
		if (ret < 0)
			pr_err("msm_otg: failed to request vbus_in gpio\n");
	}

	return ret;
}

static void __init msm_otg_power_init(int otg_test, int ovp)
{
	/* ovp_ctrl is fixed */
	int ovp_ctrl = PM8917_GPIO_PM_TO_SYS(15);
	int ret = 0;

	pr_info("msm_otg: otg_test %d\n", otg_test);

	if (ovp) {
		ret = msm_otg_pmic_gpio_config(ovp_ctrl, PM_GPIO_DIR_OUT,
				PM_GPIO_PULL_NO, "otg_ovp_ctrl", 1);

		if (ret)
			pr_err("msm_otg: OTG_OVP_CTRL config failed. %d\n", ret);
		else
			msm_otg_pdata.ovp_ctrl_gpio = ovp_ctrl;
	}

	ret = gpio_request(otg_test, "otg_test");
	if (ret) {
		pr_err("msm_otg: OTG_TEST gpio_request failed. %d\n", ret);
		return;
	}

	ret = gpio_tlmm_config(GPIO_CFG(otg_test, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
	if (ret) {
		pr_err("msm_otg: OTG_TEST gpio_tlmm_config failed. %d\n", ret);
		gpio_free(otg_test);
		return;
	}

	msm_otg_pdata.otg_test_gpio = otg_test;
}
#endif
