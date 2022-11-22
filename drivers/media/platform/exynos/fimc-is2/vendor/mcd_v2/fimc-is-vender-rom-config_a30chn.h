#ifndef FIMC_IS_VENDER_ROM_CONFIG_A30CHN_H
#define FIMC_IS_VENDER_ROM_CONFIG_A30CHN_H

/***** [ ROM VERSION HISTORY] *******************************************
 *
 * 
 * 
 * 
 *
 * 
 * 
 * 
 *
 ***********************************************************************/

#include "fimc-is-eeprom-rear-3l6-gc5035_v001.h"
#include "fimc-is-eeprom-front-3p8sp_v001.h"
#include "fimc-is-eeprom-rear3-5e9_v001.h"


const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_3l6_gc5035_cal_addr,		//[0] SENSOR_POSITION_REAR
	&front_3p8sp_cal_addr,	//[1] SENSOR_POSITION_FRONT
	NULL,							//[2] SENSOR_POSITION_REAR2
	NULL,							//[3] SENSOR_POSITION_FRONT2
	&rear3_5e9_cal_addr,				//[4] SENSOR_POSITION_REAR3
	NULL,							//[5] SENSOR_POSITION_FRONT3
};

#endif/*FIMC_IS_VENDER_ROM_CONFIG_A30CHN_H*/
