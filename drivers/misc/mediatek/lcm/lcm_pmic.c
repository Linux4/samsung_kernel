// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/kernel.h>

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;
static int regulator_inited;

int display_bias_regulator_init(void)
{
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_info("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_info("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}
EXPORT_SYMBOL(display_bias_regulator_init);

int disp_late_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	display_bias_regulator_init();

	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_info("enable regulator disp_bias_pos fail, ret = %d\n",
		ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_info("enable regulator disp_bias_neg fail, ret = %d\n",
		ret);
	retval |= ret;

	return retval;
}
EXPORT_SYMBOL(disp_late_bias_enable);

int display_bias_enable(void)
{
	int ret = 0;
	int retval = 0;
	/* hs03s code for SR-AL5625-01-395 by gaozhengwei at 2021/05/29 start */
	char *lcm_cmdline = saved_command_line;
	pr_info("lcm_name = %s\n",lcm_cmdline);
	display_bias_regulator_init();

	/* set voltage with min & max*/
	if (NULL != strstr(lcm_cmdline, "jd9365t_hdplus1600_dsi_vdo_hlt_boe_6mask")
		|| NULL != strstr(lcm_cmdline, "hx83102e_hlt_hsd_fhdplus2408")
        || NULL != strstr(lcm_cmdline, "ft8201ab_dt_qunchuang_inx_vdo_fhdplus2408")
        || NULL != strstr(lcm_cmdline, "hx83102e_copper_hlt_hsd_fhdplus2408")) {
		ret = regulator_set_voltage(disp_bias_pos, 5950000, 5950000);
		if (ret < 0)
			pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
		retval |= ret;

		ret = regulator_set_voltage(disp_bias_neg, 5950000, 5950000);
		if (ret < 0)
			pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
		retval |= ret;
	} else if (NULL != strstr(lcm_cmdline, "jd9365t_hdplus1600_dsi_vdo_hy_mdt")
		|| NULL != strstr(lcm_cmdline, "nt36523_hlt_mdt_incell_vdo")
		|| NULL != strstr(lcm_cmdline, "hx83102e_liansi_mdt_incell_vdo")) {
		ret = regulator_set_voltage(disp_bias_pos, 5900000, 5900000);
		if (ret < 0)
			pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
		retval |= ret;

		ret = regulator_set_voltage(disp_bias_neg, 5900000, 5900000);
		if (ret < 0)
			pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
		retval |= ret;
	/* hs03s code for DEVAL5625-1264 by fengzhigang at 2021/06/09 start */
	} else if (NULL != strstr(lcm_cmdline, "hx83112a_hdplus1600_dsi_vdo_ls_boe_9mask_55nm")) {
		ret = regulator_set_voltage(disp_bias_pos, 5500000, 5500000);
		if (ret < 0)
			pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
		retval |= ret;

		ret = regulator_set_voltage(disp_bias_neg, 5500000, 5500000);
		if (ret < 0)
			pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
		retval |= ret;
	/* hs03s code for DEVAL5625-1264 by fengzhigang at 2021/06/09 end */
	} else {
		ret = regulator_set_voltage(disp_bias_pos, 6000000, 6000000);
		if (ret < 0)
			pr_info("set voltage disp_bias_pos fail, ret = %d\n", ret);
		retval |= ret;

		ret = regulator_set_voltage(disp_bias_neg, 6000000, 6000000);
		if (ret < 0)
			pr_info("set voltage disp_bias_neg fail, ret = %d\n", ret);
		retval |= ret;
	}
	/* hs03s code for SR-AL5625-01-395 by gaozhengwei at 2021/05/29 end */

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_info("enable regulator disp_bias_pos fail, ret = %d\n",ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_info("enable regulator disp_bias_neg fail, ret = %d\n",ret);
	retval |= ret;

	return retval;
}
EXPORT_SYMBOL(display_bias_enable);

int display_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	display_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_info("disable regulator disp_bias_neg fail, ret = %d\n",
			ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_info("disable regulator disp_bias_pos fail, ret = %d\n",
			ret);
	retval |= ret;

	return retval;
}
EXPORT_SYMBOL(display_bias_disable);

#else
int display_bias_regulator_init(void)
{
	return 0;
}
EXPORT_SYMBOL(display_bias_regulator_init);

int display_bias_enable(void)
{
	return 0;
}
EXPORT_SYMBOL(display_bias_enable);

int disp_late_bias_enable(void)
{
	return 0;
}
EXPORT_SYMBOL(disp_late_bias_enable);

int display_bias_disable(void)
{
	return 0;
}
EXPORT_SYMBOL(display_bias_disable);
#endif

