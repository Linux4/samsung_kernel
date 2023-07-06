#ifndef __NT36525_XCOVER5_PANEL_I2C_H__
#define __NT36525_XCOVER5_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data nt36525_xcover5_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
