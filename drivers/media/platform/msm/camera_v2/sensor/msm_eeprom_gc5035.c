
/*
msm_eeprom_gc5035  OTP  read .
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

#define GC5035_EEPROM_START_ADDR                            0x1f
#define GC5035_FRONT_LCE_MODULE_ID                         0x18
#define GC5035_FRONT_HOLITECH_MODULE_ID               0x10  
#define GC5035_OTP_FLAG_VALID                                    0x01
#define GC5035_MODULE_INFO_FLAG                               0x18

#define addr_map(x)                  (0xc0 + x/8)

static int gc5035_match_module_id_by_eeprom_name(struct msm_sensor_ctrl_t *s_ctrl, uint8_t module_id)
{
    int match_result = 0;

    if((strcmp(s_ctrl->sensordata->sensor_name, "gc5035_depth_lce") == 0) && module_id == GC5035_FRONT_LCE_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is gc5035_depth_lce");
    }
    else if((strcmp(s_ctrl->sensordata->sensor_name, "gc5035_depth_holitech") == 0) && module_id == GC5035_FRONT_HOLITECH_MODULE_ID)
    {
        match_result = true;
        pr_info("match sensor is gc5035_depth_holitech");
    }
    else 
    {
        match_result = false;
        pr_err("gc5035 find  camera error");
    }
    return match_result;
}


static int gc5035_insensor_read_otp_prepare(struct msm_sensor_ctrl_t *s_ctrl)
{
        int rc = 0;
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfc, 0x01, MSM_CAMERA_I2C_BYTE_DATA);		
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf4, 0x40, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf5, 0xe9, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf6, 0x14, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf8, 0x49, MSM_CAMERA_I2C_BYTE_DATA);

        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf9, 0x82, MSM_CAMERA_I2C_BYTE_DATA);		
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfa, 0x10, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfc, 0x81, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfe, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x36, 0x01, MSM_CAMERA_I2C_BYTE_DATA);

        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xd3, 0x87, MSM_CAMERA_I2C_BYTE_DATA);		
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x36, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf7, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfc, 0x8f, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfc, 0x8f, MSM_CAMERA_I2C_BYTE_DATA);

        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfc, 0x8e, MSM_CAMERA_I2C_BYTE_DATA);		
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xfe, 0x02, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x55, 0x80, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x65, 0x7e, MSM_CAMERA_I2C_BYTE_DATA);
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x66, 0x03, MSM_CAMERA_I2C_BYTE_DATA);
       
        return rc;
}

static int gc5035_insensor_read_otp(struct msm_sensor_ctrl_t *s_ctrl, uint32_t startAddr, uint32_t dataNum, uint8_t *pwData)
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
        
        s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x67, 0xc0, MSM_CAMERA_I2C_BYTE_DATA);  //Fast sleep on
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf3, 0x00, MSM_CAMERA_I2C_BYTE_DATA);  // stand by on
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xe0, (startAddr>>8) & 0xff, MSM_CAMERA_I2C_BYTE_DATA);
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x67, 0xf0, MSM_CAMERA_I2C_BYTE_DATA); 
        s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf3, 0x10, MSM_CAMERA_I2C_BYTE_DATA); 


        for ( i = 0; i < dataNum; i++)
        {
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read((s_ctrl->sensor_i2c_client), addr_map((startAddr & 0xff)), &iicRtn, 1);
            pwData[i] = (uint8_t)iicRtn;
            pr_debug("gc5035 pwData[%d] = 0x%x", i, pwData[i]);
        }
        
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0x67, 0xc0, 1); // stand by on
        rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write((s_ctrl->sensor_i2c_client), 0xf3, 0x00, 1); // display mode
        msleep(1);
        return rc;
}


int gc5035_insensor_read_otp_info_match_sensor(struct msm_sensor_ctrl_t * s_ctrl)
{
       uint8_t flaginfo = 0;
       uint8_t module_id = 0;
       uint32_t  startAddr = 0;
       int match_result = 0;
       int rc = 0;

       rc = gc5035_insensor_read_otp_prepare(s_ctrl);    //prepare
	rc = gc5035_insensor_read_otp(s_ctrl, (GC5035_EEPROM_START_ADDR << 8 |GC5035_MODULE_INFO_FLAG), 1, &flaginfo);   //get module flag
	pr_info("gc5035 module flaginfo = 0x%x", flaginfo);


        if (GC5035_OTP_FLAG_VALID == ((flaginfo >> 2) & 0x03)) {    //group 1
            startAddr = 0x20;
            pr_info("gc5035 module startAddr = 0x%x", startAddr);
        }
        else if (GC5035_OTP_FLAG_VALID == (flaginfo & 0x03)) {       //group 2
            startAddr = 0x60;
            pr_info("gc5035 module startAddr = 0x%x", startAddr);
        }
        else {
            pr_err("gc5035_depth_lce otp info is Empty/Invalid! return.");
            return rc;
        }

 

       rc = gc5035_insensor_read_otp(s_ctrl, (GC5035_EEPROM_START_ADDR << 8 |startAddr), 1, &module_id);    //get module ID
       pr_info("gc5035 module id = 0x%x", module_id);
       match_result = gc5035_match_module_id_by_eeprom_name(s_ctrl, module_id);
       return match_result;
       
}



