#ifndef __ILI7807_M13_PANEL_I2C_H__
#define __ILI7807_M13_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ili7807_boe_m13_i2c_data = {
	.vendor = "LSI",
	.model = "S2DPS01",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
