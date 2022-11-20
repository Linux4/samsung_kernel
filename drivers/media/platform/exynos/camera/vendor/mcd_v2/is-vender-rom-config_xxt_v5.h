#ifndef IS_VENDER_ROM_CONFIG_XXT_V5_H
#define IS_VENDER_ROM_CONFIG_XXT_V5_H

#include "is-otprom-front-gc5035_v001.h"
#include "is-eeprom-rear-2p6_v001.h"

const struct is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_2p6_cal_addr,	//[0] SENSOR_POSITION_REAR
	&front_gc5035_otp_cal_addr,	//[1] SENSOR_POSITION_FRONT
	NULL,	//[2] SENSOR_POSITION_REAR2
	NULL,	//[3] SENSOR_POSITION_FRONT2
	NULL,	//[4] SENSOR_POSITION_REAR3
	NULL,	//[5] SENSOR_POSITION_FRONT3
	NULL,	//[6] SENSOR_POSITION_REAR4
	NULL,	//[7] SENSOR_POSITION_FRONT4
	NULL,	//[8] SP_REAR_TOF
	NULL,	//[9] SP_FRONT_TOF
};

#endif /*IS_VENDER_ROM_CONFIG_XXT_V5_H*/

