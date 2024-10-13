#ifndef __DUMMY_V230_PANEL_I2C_H__
#define __DUMMY_V230_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data dummy_lcd_v320_i2c_data = {
	.vendor = "INTERSIL",
	.model = "ISL98608",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
