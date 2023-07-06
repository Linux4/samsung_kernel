#ifndef __NT36525B_DTC_A12S_PANEL_I2C_H__
#define __NT36525B_DTC_A12S_PANEL_I2C_H__

#include "../panel_drv.h"

struct i2c_data nt36525b_dtc_a12s_i2c_data = {
	.vendor = "TI",
	.model = "LM36274",
	.addr_len = 0x01,
	.data_len = 0x01,
};

#endif
