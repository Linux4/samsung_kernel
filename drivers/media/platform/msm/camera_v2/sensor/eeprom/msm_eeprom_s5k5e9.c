/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "msm_sd.h"
#include "msm_eeprom.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include<linux/kernel.h>

#define BYTES_FOR_PAGE 20
#define START_ADDR_OF_PAGE 0x0A04
#define NUM_OF_PAGE 2
#define FIRST_PAGE 17
int s5k5e9_insensor_read_otp_info(struct msm_eeprom_ctrl_t *e_ctrl, struct msm_eeprom_memory_block_t *block)
{
    uint16_t i, j, rc = 0, complete = 0;
    uint8_t * memptr = block->mapdata;
    if(NULL == memptr){
        pr_info("%s:%d memptr NULL!", __func__, __LINE__);
        return -1;
    }
    e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0136, 0x18, 1); // 24Mhz
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0137, 0x00, 1);
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0305, 0x04, 1); // Pre_pll_div
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0306, 0x00, 1); // PLL multiplier
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0307, 0x5f, 1);
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x030D, 0x04, 1); // Second_pre_pll_div
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x030E, 0x00, 1); // Second_pll_multiplier
    e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x030F, 0x92, 1); // Second_pll_multiplier
    rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0100, 0x01, 1);
    msleep(50);

    for(i = 0; i < NUM_OF_PAGE; i++){
        rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0A02, FIRST_PAGE+i, 1);
        rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0A00, 0x01, 1);
        for(j = 0; j<10 && 1 != (complete&0x0001); j++){
            msleep(1);
            rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_read(&(e_ctrl->i2c_client), 0x0A01, &complete, 1);
            pr_info("%s:%d complete=%d rc=%d", __func__, __LINE__, complete, rc);
        }
        if(0 == complete)
            return -1;
        rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(&(e_ctrl->i2c_client), START_ADDR_OF_PAGE, memptr, BYTES_FOR_PAGE);
        rc += e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), 0x0A00, 0x00, 1);
        memptr += BYTES_FOR_PAGE;
    }
    return rc;
}
