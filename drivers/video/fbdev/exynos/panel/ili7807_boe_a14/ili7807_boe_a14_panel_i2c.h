#ifndef __ILI7807_A14_PANEL_I2C_H__
#define __ILI7807_A14_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ili7807_boe_a14_i2c_data = {
	.vendor = "BOE",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
