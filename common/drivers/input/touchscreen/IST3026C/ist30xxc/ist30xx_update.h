/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST30XXC_UPDATE_H__
#define __IST30XXC_UPDATE_H__

// Flash size
#define IST30XXC_FLASH_BASE_ADDR	(0x0)
#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
#define IST30XXC_FLASH_MAIN_SIZE	(0x8000)
#define IST30XXC_FLASH_INFO_SIZE	(0x400)
#define IST30XXC_FLASH_TOTAL_SIZE   (0x8400)
#else
#define IST30XXC_FLASH_MAIN_SIZE	(0xFC00)
#define IST30XXC_FLASH_INFO_SIZE	(0x0)
#define IST30XXC_FLASH_TOTAL_SIZE   (0xFC00)
#endif
#define IST30XXC_FLASH_PAGE_SIZE    (0x400)

// Flash register
#define IST30XXC_FLASH_BASE         (0x40006000)
#define IST30XXC_FLASH_MODE	        IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x00)
#define IST30XXC_FLASH_ADDR	        IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x04)
#define IST30XXC_FLASH_DIN	        IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x08)
#define IST30XXC_FLASH_DOUT	        IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x0C)
#define IST30XXC_FLASH_ISPEN	    IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x10)
#define IST30XXC_FLASH_AUTO_READ	IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x14)
#define IST30XXC_FLASH_CRC          IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x18)
#define IST30XXC_FLASH_TEST_MODE1   IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x30)
#define IST30XXC_FLASH_STATUS	    IST30XXC_DA_ADDR(IST30XXC_FLASH_BASE | 0x90)

// F/W update Info
#define IST30XXC_FW_NAME            "ist30xxc.fw"
#define IST30XXC_BIN_NAME           "ist30xxc.bin"

// Calibration
#define CAL_WAIT_TIME          	    (50)        /* unit : 100msec */
#define CALIB_TO_GAP(n)         	((n >> 16) & 0xFFF)
#define CALIB_TO_STATUS(n)      	((n >> 12) & 0xF)
#define CALIB_TO_OS_VALUE(n)    	((n >> 12) & 0xFFFF)

// Update func
#define MASK_UPDATE_INTERNAL    	(1)
#define MASK_UPDATE_FW          	(2)
#define MASK_UPDATE_SDCARD      	(3)
#define MASK_UPDATE_ERASE       	(4)

// Version flag
#define FLAG_CORE                  	(1)
#define FLAG_FW              	    (2)
#define FLAG_PROJECT              	(3)
#define FLAG_TEST               	(4)

// TSP vendor (using gpio)
#define TSP_TYPE_UNKNOWN            (0xF0)
#define TSP_TYPE_ALPS               (0xF)
#define TSP_TYPE_EELY               (0xE)
#define TSP_TYPE_TOP                (0xD)
#define TSP_TYPE_MELFAS             (0xC)
#define TSP_TYPE_ILJIN              (0xB)
#define TSP_TYPE_SYNOPEX            (0xA)
#define TSP_TYPE_SMAC               (0x9)
#define TSP_TYPE_TAEYANG            (0x8)
#define TSP_TYPE_TOVIS              (0x7)
#define TSP_TYPE_ELK                (0x6)
#define TSP_TYPE_OTHERS             (0x5)
#define TSP_TYPE_NO                 (0x10)

#define IST30XXC_PARSE_TSPTYPE(n)   ((n >> 1) & 0xF)

int ist30xxc_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);
int ist30xxc_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len);

int ist30xxc_get_update_info(struct ist30xxc_data *data, const u8 *buf,
			     const u32 size);
int ist30xxc_get_tsp_info(struct ist30xxc_data *data);
void ist30xxc_print_info(struct ist30xxc_data *data);
u32 ist30xxc_parse_ver(struct ist30xxc_data *data, int flag, const u8 *buf);

int ist30xxc_fw_update(struct ist30xxc_data *data, const u8 *buf, int size);
int ist30xxc_fw_recovery(struct ist30xxc_data *data);

int ist30xxc_auto_bin_update(struct ist30xxc_data *data);

int ist30xxc_calibrate(struct ist30xxc_data *data, int wait_cnt);

int ist30xxc_init_update_sysfs(struct ist30xxc_data *data);

#endif  // __IST30XXC_UPDATE_H__
