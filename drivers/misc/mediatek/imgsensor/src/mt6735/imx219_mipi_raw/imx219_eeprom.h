/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   catc24c16.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Header file of CAM_CAL driver
 *
 *
 * Author:
 * -------
 *   John Wei (MTK07407)
 *
 *============================================================================*/
#ifndef __IMX219_EEPROM_H
#define __IMX219_EEPROM_H

#define CAM_CAL_DEV_MAJOR_NUMBER 226

/* CAM_CAL READ/WRITE ID */
#define IMX219_EEPROM_DEVICE_ID							0xA0/*slave id of imx135*/
/*#define I2C_UNIT_SIZE                       1 //in byte*/
/*#define OTP_START_ADDR                        //0x0A04*/
/*#define OTP_SIZE                              24*/

/*LukeHu++150706=For coding style*/
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData,
u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);

#endif/* __CAM_CAL_H */
