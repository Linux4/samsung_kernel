#ifndef IS_VENDER_ROM_CONFIG_AAS_V21S_H
#define IS_VENDER_ROM_CONFIG_AAS_V21S_H

#include "is-eeprom-rear-gm2_v001.h"
#include "is-eeprom-front-hi1336b_v001.h"
#include "is-eeprom-uw-4ha_sr846_v001.h"
#include "is-eeprom-macro-gc02m1_v001.h"

const struct is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_gm2_cal_addr,				//[0] SENSOR_POSITION_REAR
	&front_hi1336b_cal_addr,				//[1] SENSOR_POSITION_FRONT
	NULL,						//[2] SENSOR_POSITION_REAR2
	NULL,						//[3] SENSOR_POSITION_FRONT2
	&uw_4ha_sr846_cal_addr,				//[4] SENSOR_POSITION_REAR3
	NULL,						//[5] SENSOR_POSITION_FRONT3
	&macro_gc02m1_cal_addr,				//[6] SENSOR_POSITION_REAR4
	NULL,						//[7] SENSOR_POSITION_FRONT4
	NULL,						//[8] SP_REAR_TOF
	NULL,						//[9] SP_FRONT_TOF
};

#endif /*IS_VENDER_ROM_CONFIG_AAS_V21S_H*/

