/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "disp_dts_gpio.h"
#include <linux/bug.h>

#include <linux/kernel.h> /* printk */

static struct pinctrl *this_pctrl; /* static pinctrl instance */

/* DTS state mapping name */
static const char *this_state_name[DTS_GPIO_STATE_MAX] = {
	"mode_te_gpio",		/* DTS_GPIO_STATE_TE_MODE_GPIO */
	"mode_te_te",		/* DTS_GPIO_STATE_TE_MODE_TE */
	"lcm_rst_out0_gpio",	/* DTS_GPIO_STATE_LCM_RST_GPIO0 */
	"lcm_rst_out1_gpio",	/* DTS_GPIO_STATE_LCM_RST_GPIO1 */
	"lcd_bias_enp0_gpio",	/* DTS_GPIO_STATE_LCD_BIAS_EN0 */
	"lcd_bias_enp1_gpio",	/* DTS_GPIO_STATE_LCD_BIAS_EN1 */
	"blic_ctl_enp0_gpio",	/* DTS_GPIO_STATE_BLIC_CTL_EN0 */
	"blic_ctl_enp1_gpio",	/* DTS_GPIO_STATE_BLIC_CTL_EN1 */
	"blic_en_enp0_gpio",	/* DTS_GPIO_STATE_BLIC_EN_EN0 */
	"blic_en_enp1_gpio",	/* DTS_GPIO_STATE_BLIC_EN_EN1 */
};

/* pinctrl implementation */
static long _set_state(const char *name)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;

	BUG_ON(!this_pctrl);

	pState = pinctrl_lookup_state(this_pctrl, name);
	if (IS_ERR(pState)) {
		pr_err("set state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(this_pctrl, pState);

exit:
	return ret; /* Good! */
}

long disp_dts_gpio_init(struct platform_device *pdev)
{
	long ret = 0;
	struct pinctrl *pctrl;

	/* retrieve */
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		dev_err(&pdev->dev, "Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	this_pctrl = pctrl;

exit:
	return ret;
}

long disp_dts_gpio_select_state(DTS_GPIO_STATE s)
{
	BUG_ON(!((unsigned int)(s) < (unsigned int)(DTS_GPIO_STATE_MAX)));
	return _set_state(this_state_name[s]);
}


long lcm_TurnOnGate(bool bOn)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;


	if (!this_pctrl)
		goto exit;

	if (bOn)
		pState = pinctrl_lookup_state(this_pctrl, "lcd_bias_enp1_gpio");
	else
		pState = pinctrl_lookup_state(this_pctrl, "lcd_bias_enp0_gpio");

	if (IS_ERR(pState)) {
		if (bOn)
			pr_err("set state '%s' failed\n", "lcd_bias_enp1_gpio");
		else
			pr_err("set state '%s' failed\n", "lcd_bias_enp0_gpio");

		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(this_pctrl, pState);

exit:
	return ret; /* Good! */
}

long lcm_TurnOnGateByName(bool bOn, char *pinName)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;


	if (!this_pctrl)
		goto exit;

	if (bOn)
		pState = pinctrl_lookup_state(this_pctrl, pinName);
	else
		pState = pinctrl_lookup_state(this_pctrl, pinName);

	if (IS_ERR(pState)) {
		if (bOn)
			pr_err("set state '%s' failed\n", pinName);
		else
			pr_err("set state '%s' failed\n", pinName);

		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(this_pctrl, pState);
exit:
	return ret; /* Good! */
}

