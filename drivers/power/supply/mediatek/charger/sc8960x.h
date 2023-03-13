/*
 * @Author: Aiden Aiden-yu@southchip.com
 * @Date: 2022-08-31 11:37:23
 * @LastEditors: Aiden Aiden-yu@southchip.com
 * @LastEditTime: 2022-08-31 13:41:52
 * @FilePath: \mcu_projectd:\Download\Chrome\sc8960x (3)\sc8960x\sc8960x.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef _LINUX_SC8960X_I2C_H
#define _LINUX_SC8960X_I2C_H

#include <linux/power_supply.h>


struct sc8960x_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

enum vboost {
	BOOSTV_4700 = 4700,
	BOOSTV_4900 = 4900,
	BOOSTV_5100 = 5100,
	BOOSTV_5300 = 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5800 = 5800,
	VAC_OVP_6400 = 6400,
	VAC_OVP_11000 = 11000,
	VAC_OVP_14200 = 14200,
};


struct sc8960x_platform_data {
	struct sc8960x_charge_param usb;
	int iprechg;
	int iterm;

	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4900,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;

};

#endif
