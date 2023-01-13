#ifndef __HX83112F_A13_PANEL_I2C_H__
#define __HX83112F_A13_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data hx83112f_boe_a13_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
