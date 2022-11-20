#ifndef __FT8203_T260_PANEL_I2C_H__
#define __FT8203_T260_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ft8203_boe_t260_i2c_data = {
	.vendor = "INTERSIL",
	.model = "ISL98608",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
