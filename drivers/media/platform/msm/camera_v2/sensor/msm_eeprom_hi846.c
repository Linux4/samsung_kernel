
/*
msm_eeprom_hi846  OTP  read .
add by  shanglei.wt

*/
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_camera_dt_util.h"
#include "msm_sensor_driver.h"
#include "msm_sd.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include<linux/kernel.h>

#define HI846_EEPROM_START_ADDR                 0x0201
#define HI846_EEPROM_READ_REG                     0x0708

#define HI846_EEPROM_OFFSET                          0x0201
#define HI846_MODULE_INFO_FLAG                    (0x0201 - HI846_EEPROM_OFFSET)
#define HI846_MODULE_INFO_CHECKSUM           (0x0212 - HI846_EEPROM_OFFSET)
#define HI846_MODULE_INFO_FIRST_BYTE         (0x0202 - HI846_EEPROM_OFFSET)
#define HI846_MODULE_INFO_LAST_BYTE          (0x0211 - HI846_EEPROM_OFFSET)
#define HI846_MODULE_GROUP_SIZE                 (0x0212 - HI846_EEPROM_OFFSET)

#define HI846_MODULE_ID_GROUP_1                 (1)
#define HI846_MODULE_ID_GROUP_2                 (HI846_MODULE_GROUP_SIZE +1)
#define HI846_MODULE_ID_GROUP_3                 (HI846_MODULE_GROUP_SIZE*2 +1)


#define HI846_FRONT     1
#define HI846_WIDE       2

#define HI846_FRONT_SHINETECH_MODULE_ID    0x17
#define HI846_FRONT_TXD_MODULE_ID               0x19  

#define HI846_WIDE_TRULY_MODULE_ID              0x06
#define HI846_WIDE_SHINETECH_MODULE_ID       0x17



static int hi846_match_module_id_by_eeprom_name(struct msm_sensor_ctrl_t *s_ctrl, uint8_t module_id)
{
    int match_result = 0;

    if((strcmp(s_ctrl->sensordata->sensor_name, "hi846_front_shinetech") == 0) && module_id == HI846_FRONT_SHINETECH_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is hi846_front_shinetech");
    }
    else if((strcmp(s_ctrl->sensordata->sensor_name, "hi846_front_txd") == 0) && module_id == HI846_FRONT_TXD_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is hi846_front_txd");
    }
    else if((strcmp(s_ctrl->sensordata->sensor_name, "hi846_wide_truly") == 0) && module_id == HI846_WIDE_TRULY_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is hi846_wide_truly");
    }
    else if((strcmp(s_ctrl->sensordata->sensor_name, "hi846_wide_shinetech") == 0) && module_id == HI846_WIDE_SHINETECH_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is hi846_wide_shinetech");
    }
    else 
    {
        match_result = false;
        pr_err("HI846 find  camera error");
    }
    return match_result;
}



static int hi846_insensor_read_otp_prepare(struct msm_sensor_ctrl_t *s_ctrl)
{
        int rc = 0;
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x0000, MSM_CAMERA_I2C_WORD_DATA);			
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x2000, 0x0000, MSM_CAMERA_I2C_WORD_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x2002, 0x00FF, MSM_CAMERA_I2C_WORD_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x2004, 0x0000, MSM_CAMERA_I2C_WORD_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x2008, 0x3FFF, MSM_CAMERA_I2C_WORD_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x23FE, 0xC056, MSM_CAMERA_I2C_WORD_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x0000, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0E04, 0x0012, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F08, 0x2F04, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F30, 0x001F, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F36, 0x001F, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F04, 0x3A00, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F32, 0x025A, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F38, 0x025A, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F2A, 0x4124, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x006A, 0x0100, MSM_CAMERA_I2C_WORD_DATA); 
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x004C, 0x0100, MSM_CAMERA_I2C_WORD_DATA); 
        return rc;
}

static int hi846_insensor_read_otp(struct msm_sensor_ctrl_t *s_ctrl, uint32_t startAddr, uint32_t dataNum, uint8_t *pwData)
{
        uint32_t i = 0;
        uint16_t  iicRtn = 0;
        int rc = 0;
        if(pwData == NULL ) {
           return -EINVAL;
        }
        if(dataNum == 0) {
           return -EINVAL;;
        }
        
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A02, 0x01, 1);  //Fast sleep on
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x00, 1);  // stand by on
        msleep(20);

        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0F02, 0x00, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x071A, 0x01, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x071B, 0x09, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0D04, 0x01, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0D00, 0x07, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x003E, 0x10, 1);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x01, 1);
        msleep(1);

        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x070A, ((startAddr>>8)&0xff), 1);// start address H
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x070B, (startAddr&0xff), 1); // start address L
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0702, 0x01, 1);   // read mode

        for ( i = 0; i < dataNum; i++)
        {
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read((s_ctrl->sensor_i2c_client), HI846_EEPROM_READ_REG, &iicRtn, 1);
            pwData[i] = (uint8_t)iicRtn;
            pr_debug("hi846 pwData[%d] = 0x%x", i, pwData[i]);
        }
        
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x00, 1); // stand by on
        msleep(20);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x003E, 0x00, 1); // display mode
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x0A00, 0x00, 1); // stand by off
        msleep(1);
        return rc;
}


int hi846_insensor_read_otp_info_match_sensor(struct msm_sensor_ctrl_t * s_ctrl)
{
       uint8_t flaginfo = 0;
       uint8_t module_id = 0;
       uint32_t  startAddr = 0;
       int match_result = 0;
       int rc = 0;

       rc = hi846_insensor_read_otp_prepare(s_ctrl);    //prepare
	rc = hi846_insensor_read_otp(s_ctrl, HI846_EEPROM_START_ADDR, 1, &flaginfo);   //get module flag
	pr_info("hi846 module flaginfo = 0x%x", flaginfo);
	if (flaginfo == 0x1)
       {
		startAddr = 0x0202;
	} else if (flaginfo == 0x13) 
	{
		startAddr = 0x0213;
	} else if (flaginfo == 0x37) 
	{
		startAddr = 0x0224;
	} else 
	{
		pr_err("module flaginfo is not match");
		return false;
	}

       rc = hi846_insensor_read_otp(s_ctrl, startAddr, 1, &module_id);    //get module ID
       pr_info("hi846 module id = 0x%x", module_id);
       match_result = hi846_match_module_id_by_eeprom_name(s_ctrl, module_id);
       return match_result;
       
}



