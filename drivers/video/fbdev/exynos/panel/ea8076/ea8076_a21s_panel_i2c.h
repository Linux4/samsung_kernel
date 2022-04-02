#ifndef __EA8076_A21S_PANEL_I2C_H__
#define __EA8076_A21S_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data ea8076_a21s_i2c_data = {
	.vendor = "LSI",
	.model = "S2DPS01",
	.addr_len = 0x02,
	.data_len = 0x02,
};

#endif
