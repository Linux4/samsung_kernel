#ifndef __ILI7807S_A12S_PANEL_I2C_H__
#define __ILI7807S_A12S_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ili7807s_a12s_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
