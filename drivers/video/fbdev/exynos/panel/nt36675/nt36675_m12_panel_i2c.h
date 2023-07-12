#ifndef __NT36675_M12_PANEL_I2C_H__
#define __NT36675_M12_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data nt36675_m12_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
