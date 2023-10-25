#ifndef __FT8720_M13_PANEL_I2C_H__
#define __FT8720_M13_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ft8720_dtc_m13_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
