// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include "cam_eeprom_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_eeprom_soc.h"
#include "cam_eeprom_core.h"
#include "cam_debug_util.h"
#include "camera_main.h"

#include "eepromWriteInitParmsBeforeReadCalData.h"
#include "w11_hi556_front_lianyi_eeprom.h"
#include "w11_hi846_rear_truly_eeprom.h"
#include "w11_c8496_rear_st_eeprom.h"
#include "w11_c5590_front_txd_eeprom.h"
#include "w11_sc520cs_front_txd_eeprom.h"
#include "w11_gc08a3_rear_cxt_eeprom.h"
#include <linux/delay.h>


#define MAX_READ_SIZE  0x7FFFF
#define EEPROM_DUMP_ENABLE 0


uint8_t WtOTPData[WT_OTP_DATA_LEN];



int eeprom_process_para_before_read(uint32_t slave_addr,struct eeprom_memory_map_init_write_params **pWriteParams,struct WingtechOtpCheckInfo **pOtpParams,uint32_t *count_read){
	int eeprom_result = 0;

	if(w11_hi556_front_lianyi_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "hi556 get otp init paras");
		*pWriteParams = &w11_hi556_front_lianyi_eeprom;
		*pOtpParams = &w11_hi556_front_lianyi_otp_checkinfo;
		*count_read = (uint32_t)W11_HI556_OTP_DATA_LEN;
	}else if(w11_hi846_rear_truly_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "hi846 get otp init paras");
		*pWriteParams = &w11_hi846_rear_truly_eeprom;
		*pOtpParams = &w11_hi846_rear_truly_otp_checkinfo;
		*count_read = (uint32_t)W11_HI846_OTP_DATA_LEN;
	}else if(w11_c8496_rear_st_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "c8496 get otp init paras");
		*pWriteParams = &w11_c8496_rear_st_eeprom;
		*pOtpParams = &w11_c8496_rear_st_otp_checkinfo;
		*count_read = (uint32_t)W11_C8496_OTP_DATA_LEN;
	}else if(w11_c5590_front_txd_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "c5590 get otp init paras");
		*pWriteParams = &w11_c5590_front_txd_eeprom;
		*pOtpParams = &w11_c5590_front_txd_otp_checkinfo;
		*count_read = (uint32_t)W11_C5590_OTP_DATA_LEN;
	}else if(w11_gc08a3_rear_cxt_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "gc08a3 get otp init paras");
		*pWriteParams = &w11_gc08a3_rear_cxt_eeprom;
		*pOtpParams = &w11_gc08a3_rear_cxt_otp_checkinfo;
		*count_read = (uint32_t)W11_GC08A3_OTP_DATA_LEN;
	}else{
		eeprom_result = -1;
		CAM_DBG(CAM_EEPROM, "it is not a valid otp");
	}

	return eeprom_result;
}

int eeprom_process_para_after_read(uint32_t slave_addr,struct eeprom_memory_map_init_write_params **pWriteParams){
	int eeprom_result = 0;

	if(w11_hi556_front_lianyi_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "hi556 get otp uninit paras");
		*pWriteParams = &w11_hi556_front_lianyi_eeprom_after_read;
	}else if(w11_hi846_rear_truly_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "hi846 get otp uninit paras");
		*pWriteParams = &w11_hi846_rear_truly_eeprom_after_read;
	}else if(w11_c8496_rear_st_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "c8496 get otp uninit paras");
		*pWriteParams = &w11_c8496_rear_st_eeprom_after_read;
	}else if(w11_c5590_front_txd_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "c5590 get otp uninit paras");
		*pWriteParams = &w11_c5590_front_txd_eeprom_after_read;
	}else if(w11_gc08a3_rear_cxt_i2c_slave_addr == slave_addr){
		CAM_DBG(CAM_EEPROM, "gc08a3 get otp uninit paras");
		*pWriteParams = &w11_gc08a3_rear_cxt_eeprom_after_read;
	}else{
		eeprom_result = -1;
		CAM_DBG(CAM_EEPROM, "it is not a valid otp");
	}

	return eeprom_result;
}

int eeprom_memory_select_group(struct WingtechOtpCheckInfo *pOtpParams){
	int i,j;
	int eeprom_result = 0;

    //select group info .
    if (pOtpParams->groupInfo.IsAvailable) {
        for( i=0; i < CHECKNUMMAX; i++) {
            for ( j = 0; j< GROUPNUMMAX; j++ ) {
                if (WtOTPData[ pOtpParams->groupInfo.CheckItemOffset[i] ] == pOtpParams->groupInfo.GroupFlag[j]) {
                    CAM_DBG(CAM_EEPROM, "part:%d select group: %d.", i, j);
                    pOtpParams->groupInfo.SelectGroupNum[i] = j ;
                    break;
                } else {
                    CAM_DBG(CAM_EEPROM, "WtOTPData[i]:%d,GroupFlag[j]:%d",WtOTPData[ pOtpParams->groupInfo.CheckItemOffset[i]],pOtpParams->groupInfo.GroupFlag[j]);
                    continue;
                }
            }
            CAM_DBG(CAM_EEPROM, " j : %d",j);
            //can not match group ...
            if( j == GROUPNUMMAX ) {
                CAM_DBG(CAM_EEPROM, "part: %d bad otp data.", i);
                pOtpParams->groupInfo.SelectGroupNum[i] = GROUPNUMMAX; //GROUPNUMMAX is invalid group num.
            }
        }
    }
    return eeprom_result;
}

int eeprom_memory_process_otp_data(struct WingtechOtpCheckInfo *pOtpParams,uint8_t *memptr){
	int i;
	int eeprom_result = 0;
	uint32_t GroupOffset, GroupLenth, SelectGroupTemp;


	for(i=0; i<CHECKNUMMAX; i++){
		SelectGroupTemp = pOtpParams->groupInfo.SelectGroupNum[i];
		GroupLenth = pOtpParams->ItemInfo[i][SelectGroupTemp].Length;
		GroupOffset = pOtpParams->ItemInfo[i][SelectGroupTemp].Offset;
		CAM_DBG(CAM_EEPROM, "SelectGroupTemp:%d, GroupLenth :%d,GroupOffset:%d, ",SelectGroupTemp,GroupLenth,GroupOffset);
		if(1 == pOtpParams->ItemInfo[i][SelectGroupTemp].IsAvailable){
			CAM_DBG(CAM_EEPROM, " eeprom_memory_process_otp_data copy data");
			memcpy(memptr,&WtOTPData[GroupOffset],GroupLenth);
			memptr += GroupLenth;
		}

	}
	CAM_DBG(CAM_EEPROM, "eeprom_memory_process_otp_data end");

    return eeprom_result;
}


int eeprom_memory_map_read_data(uint32_t slave_addr,struct cam_eeprom_memory_map_t emap,
	struct cam_eeprom_ctrl_t *e_ctrl,uint8_t *memptr){

	int   i = 0,eeprom_result = 0;
	uint32_t  count_write,count_read;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	struct eeprom_memory_map_init_write_params *pWriteParams = NULL;
	struct WingtechOtpCheckInfo *pWtOtpCheckInfo = NULL;

	eeprom_result = eeprom_process_para_before_read(slave_addr,&pWriteParams,&pWtOtpCheckInfo,&count_read);

	if(eeprom_result < 0){
		CAM_ERR(CAM_EEPROM, "read init params failed.");
	}

	if(pWriteParams == NULL /*&& pWriteParams->memory_map_size == 0 */) {
		eeprom_result = -1;
		CAM_DBG(CAM_EEPROM, "invalid init write params , ");
	} else {
		CAM_DBG(CAM_EEPROM, " write init params , size : %d  ", pWriteParams->memory_map_size);
		for(count_write=0;count_write< pWriteParams->memory_map_size; count_write++)
		{
			i2c_reg_settings.addr_type = pWriteParams->mem_settings[count_write].addr_type;
			i2c_reg_settings.data_type = pWriteParams->mem_settings[count_write].data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = pWriteParams->mem_settings[count_write].reg_addr;
			i2c_reg_array.reg_data = pWriteParams->mem_settings[count_write].reg_data;
			i2c_reg_array.delay = pWriteParams->mem_settings[count_write].delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			eeprom_result = camera_io_dev_write(&e_ctrl->io_master_info, &i2c_reg_settings);
			if (eeprom_result) {
				CAM_ERR(CAM_EEPROM, "write init params failed eeprom_result %d", eeprom_result);
			return eeprom_result;
			}
		}
	}

	for(i = 0; i < count_read; i++) {
		eeprom_result = camera_io_dev_read_seq(&e_ctrl->io_master_info,
			 emap.mem.addr, &WtOTPData[i],
			 emap.mem.addr_type,
			 emap.mem.data_type,1);
		if (eeprom_result < 0) {
			CAM_ERR(CAM_EEPROM, "read failed eeprom_result %d",eeprom_result);
			return eeprom_result;
		}
		if (w11_c8496_rear_st_i2c_slave_addr == slave_addr ||
			w11_c5590_front_txd_i2c_slave_addr == slave_addr){
			emap.mem.addr++;
		}
	}
	eeprom_result = eeprom_memory_select_group(pWtOtpCheckInfo);
	if (eeprom_result < 0) {
		CAM_ERR(CAM_EEPROM, "failed to select group",eeprom_result);
	}

	eeprom_result = eeprom_memory_process_otp_data(pWtOtpCheckInfo,memptr);
	if (eeprom_result < 0) {
		CAM_ERR(CAM_EEPROM, "failed to process otp data",eeprom_result);
	}

	return eeprom_result;
}

int eeprom_memory_map_unint_para(uint32_t slave_addr,struct cam_eeprom_ctrl_t *e_ctrl){

	int                                eeprom_result = 0;
	uint32_t                           count_write;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	struct eeprom_memory_map_init_write_params *pWriteParams = NULL;

	eeprom_result = eeprom_process_para_after_read(slave_addr,&pWriteParams);
	//pWriteParams = &w11_hi556_front_lianyi_eeprom_after_read;
	if(pWriteParams == NULL ) {
		eeprom_result = -1;
		CAM_ERR(CAM_EEPROM, "invalid uninit write params , ");
	} else {
		CAM_DBG(CAM_EEPROM, "write uninit params , size : %d  ", pWriteParams->memory_map_size);
		for(count_write=0;count_write< pWriteParams->memory_map_size; count_write++)
		{
			i2c_reg_settings.addr_type = pWriteParams->mem_settings[count_write].addr_type;
			i2c_reg_settings.data_type = pWriteParams->mem_settings[count_write].data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = pWriteParams->mem_settings[count_write].reg_addr;
			i2c_reg_array.reg_data = pWriteParams->mem_settings[count_write].reg_data;
			i2c_reg_array.delay = pWriteParams->mem_settings[count_write].delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			eeprom_result = camera_io_dev_write(&e_ctrl->io_master_info, &i2c_reg_settings);
			if (eeprom_result) {
				CAM_ERR(CAM_EEPROM, "write init params failed eeprom_result %d", eeprom_result);
			return eeprom_result;
			}
		}
	}
	return eeprom_result;
}

 int OtpChecksumCalc(uint8_t *memptr1)
{
	uint32_t i = 0;
	uint32_t InfSum=0,AwbSum=0,Lscsum=0;
	for(i = w11_sc520cs_front_txd_checkinf[0].Offset; i < w11_sc520cs_front_txd_checkinf[0].Length; i++){
		InfSum += memptr1[i];
	}

	for(i = w11_sc520cs_front_txd_checkinf[1].Offset; i < w11_sc520cs_front_txd_checkinf[1].Length; i++){
		AwbSum += memptr1[i];
	}

	for(i = w11_sc520cs_front_txd_checkinf[2].Offset; i < w11_sc520cs_front_txd_checkinf[2].Length; i++){
        Lscsum += memptr1[i];
	}
	if((InfSum%255+1) == memptr1[w11_sc520cs_front_txd_checkinf[0].Length]&&(AwbSum%255+1) == memptr1[w11_sc520cs_front_txd_checkinf[1].Length]&&
		(Lscsum%255+1) == memptr1[w11_sc520cs_front_txd_checkinf[2].Length]){
		CAM_INFO(CAM_EEPROM, "sc520cs otp checksum successed");
		return SUCCESSRESULT;
	}else{
		CAM_ERR(CAM_EEPROM, "sc520cs otp checksum failed");
		return FAILEESULT;
	}
}

 int eeprom_memory_map_read_data_sc520cs(uint32_t slave_addr,struct cam_eeprom_memory_map_t emap,
	 struct cam_eeprom_ctrl_t *e_ctrl,uint8_t *memptr){
	 int   i = 0,eeprom_result = 0,j = 0;
	 uint32_t  count_write,count_read;
	 struct cam_sensor_i2c_reg_setting	i2c_reg_settings = {0};
	 struct cam_sensor_i2c_reg_array	i2c_reg_array = {0};
	 for(j = 0; j < 3; j++){
		 for(count_write=0;count_write< w11_sc520cs_front_txd_eeprom_pag1[j].memory_map_size; count_write++)
		 {
			 i2c_reg_settings.addr_type = w11_sc520cs_front_txd_eeprom_pag1[j].mem_settings[count_write].addr_type;
			 i2c_reg_settings.data_type = w11_sc520cs_front_txd_eeprom_pag1[j].mem_settings[count_write].data_type;
			 i2c_reg_settings.size = 1;
			 i2c_reg_array.reg_addr = w11_sc520cs_front_txd_eeprom_pag1[j].mem_settings[count_write].reg_addr;
			 i2c_reg_array.reg_data = w11_sc520cs_front_txd_eeprom_pag1[j].mem_settings[count_write].reg_data;
			 i2c_reg_array.delay = w11_sc520cs_front_txd_eeprom_pag1[j].mem_settings[count_write].delay;
			 i2c_reg_settings.reg_setting = &i2c_reg_array;
			 eeprom_result = camera_io_dev_write(&e_ctrl->io_master_info, &i2c_reg_settings);
			 if (eeprom_result) {
				 CAM_ERR(CAM_EEPROM, "write init params failed eeprom_result %d", eeprom_result);
				 return eeprom_result;
			 }
		 }
		 mdelay(20);
		 emap.mem.addr = Pag1OtpData.StartAddr;
		 count_read = Pag1OtpData.Length;
		 for(i = 0; i < count_read; i++) {
			 eeprom_result = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				  emap.mem.addr, &WtOTPData[i],
				  emap.mem.addr_type,
				  emap.mem.data_type,1);
			 if (eeprom_result < 0) {
				 CAM_ERR(CAM_EEPROM, "read failed eeprom_result %d",eeprom_result);
				 return eeprom_result;
			 }
			 emap.mem.addr++;
		 }
		 for(count_write=0;count_write< w11_sc520cs_front_txd_eeprom_pag2.memory_map_size; count_write++)
		 {
			 i2c_reg_settings.addr_type = w11_sc520cs_front_txd_eeprom_pag2.mem_settings[count_write].addr_type;
			 i2c_reg_settings.data_type = w11_sc520cs_front_txd_eeprom_pag2.mem_settings[count_write].data_type;
			 i2c_reg_settings.size = 1;
			 i2c_reg_array.reg_addr = w11_sc520cs_front_txd_eeprom_pag2.mem_settings[count_write].reg_addr;
			 i2c_reg_array.reg_data = w11_sc520cs_front_txd_eeprom_pag2.mem_settings[count_write].reg_data;
			 i2c_reg_array.delay = w11_sc520cs_front_txd_eeprom_pag2.mem_settings[count_write].delay;
			 i2c_reg_settings.reg_setting = &i2c_reg_array;
			 eeprom_result = camera_io_dev_write(&e_ctrl->io_master_info, &i2c_reg_settings);
			 if (eeprom_result) {
				 CAM_ERR(CAM_EEPROM, "write init params failed eeprom_result %d", eeprom_result);
				 return eeprom_result;
			 }
		 }
		 mdelay(20);
		 emap.mem.addr = Pag2OtpData.StartAddr;
		 count_read = Pag2OtpData.Length;
		 for(i = 0; i < count_read; i++) {
			 eeprom_result = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				  emap.mem.addr, &WtOTPData[Pag1OtpData.Length+i],
				  emap.mem.addr_type,
				  emap.mem.data_type,1);
			 if (eeprom_result < 0) {
				 CAM_ERR(CAM_EEPROM, "read failed eeprom_result %d",eeprom_result);
				 return eeprom_result;
			 }
			 emap.mem.addr++;
		 }
		 eeprom_result = eeprom_memory_select_group(&w11_sc520cs_front_txd_otp_checkinfo);
		 if (eeprom_result < 0) {
			 CAM_ERR(CAM_EEPROM, "failed to select group",eeprom_result);
		 }

		 eeprom_result = eeprom_memory_process_otp_data(&w11_sc520cs_front_txd_otp_checkinfo,memptr);
		 if (eeprom_result < 0) {
			 CAM_ERR(CAM_EEPROM, "failed to process otp data",eeprom_result);
		 }
		 if (0 == OtpChecksumCalc(memptr)){
			 CAM_INFO(CAM_EEPROM, "sc520cs otp read successed");
			 return eeprom_result;
		 }
		 if(j == 2){
			CAM_ERR(CAM_EEPROM, "sc520cs otp read failed");
			return FAILEESULT;
		 }
		 for(count_write=0;count_write< w11_sc520cs_front_txd_eeprom_after_read.memory_map_size; count_write++)
		 {
			 i2c_reg_settings.addr_type = w11_sc520cs_front_txd_eeprom_after_read.mem_settings[count_write].addr_type;
			 i2c_reg_settings.data_type = w11_sc520cs_front_txd_eeprom_after_read.mem_settings[count_write].data_type;
			 i2c_reg_settings.size = 1;
			 i2c_reg_array.reg_addr = w11_sc520cs_front_txd_eeprom_after_read.mem_settings[count_write].reg_addr;
			 i2c_reg_array.reg_data = w11_sc520cs_front_txd_eeprom_after_read.mem_settings[count_write].reg_data;
			 i2c_reg_array.delay = w11_sc520cs_front_txd_eeprom_after_read.mem_settings[count_write].delay;
			 i2c_reg_settings.reg_setting = &i2c_reg_array;
			 eeprom_result = camera_io_dev_write(&e_ctrl->io_master_info, &i2c_reg_settings);
			 if (eeprom_result) {
				 CAM_ERR(CAM_EEPROM, "write init params failed eeprom_result %d", eeprom_result);
				 return eeprom_result;
			 }
			 mdelay(20);
		 }
	 }
	 return eeprom_result;
 }
