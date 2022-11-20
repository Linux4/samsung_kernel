/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_CAL_DATA_H
#define _CAM_CAL_DATA_H

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

//+bug584789, liuxingyu.wt, ADD, 2020/12/13, add hi556 gc5035c bringup
struct stCAM_CAL_DATAINFO_STRUCT{
	u32 sensorID; // Sensor ID
	u32 deviceID; // MAIN = 0x01, SUB  = 0x02, MAIN_2 = 0x04
	u32 dataLength; //Data len
	u32 sensorVendorid; // Module ID | Pos ID | Vcm ID | Len ID
	u8  vendorByte[4]; // Module ID offset, Pos ID offset, Vcm ID offset,  Len ID offset
	u8  *dataBuffer; //It's need malloc dataLength cache
};

typedef enum{
	MODULE_ITEM = 0,
	AWB_ITEM,
	SEGMENT_ITEM,
	AF_ITEM,
	LSC_ITEM,
	PDAF_ITEM,
	//+bug604664 zhouyikuan, add, 2020/12/18,hi4821q_qt eeprom bring up
	PDAF_PROC2_ITEM,
	HI4821Q_XGC,
	HI4821Q_QGC,
	HI4821Q_BPGC,
	//-bug604664 zhouyikuan, add, 2020/12/18,hi4821q_qt eeprom bring up
        //+bug604664 liudijn, add, 2020/12/25,3ts ov48b2q eeprom bring up.
        PDAF_PROC3_ITEM,//10
        OV48B2Q_PDC_ITEM,
        OV48B2Q_CT_ITEM,
        //-bug604664 liudijn, add, 2020/12/25,3ts ov48b2q eeprom bring up.
        //+bug604664 liudijn, add, 2021/03/29,1ts w1ov64b40reartxd eeprom bring up.
        W1OV64B40REARTXD_CT_ITEM,
	DUALCAM_ITEM,
	TOTAL_ITEM,
	MAX_ITEM,
}stCAM_CAL_CHECKSUM_ITEM;

struct stCAM_CAL_CHECKSUM_STRUCT{
	stCAM_CAL_CHECKSUM_ITEM item;
	u32 flagAdrees;
	u32 startAdress;
	u32 endAdress;
	u32 checksumAdress;
	u8  validFlag;
};
//-bug584789, liuxingyu.wt, ADD, 2020/12/13, add hi556 gc5035c bringup

struct CAM_CAL_SENSOR_INFO {
	u32 sensor_id;
};

struct stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	/*
	 * MAIN = 0x01,
	 * SUB  = 0x02,
	 * MAIN_2 = 0x04,
	 * SUB_2 = 0x08,
	 * MAIN_3 = 0x10,
	 */
	u32 deviceID;
	u8 *pu1Params;
};

#ifdef CONFIG_COMPAT

struct COMPAT_stCAM_CAL_INFO_STRUCT {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	u32 deviceID;
	compat_uptr_t pu1Params;
};
#endif

#endif/*_CAM_CAL_DATA_H*/
