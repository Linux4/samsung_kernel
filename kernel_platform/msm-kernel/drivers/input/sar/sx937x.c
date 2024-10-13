/*! \file sx93xx_sar.c
 * \brief  Semtech CAP Sensor Driver for SAR
 *
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "sx937x.h"
/* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
#ifdef SX937X_USER_NODE
#include <linux/sensor/sensors_core.h>
#endif //SX937X_USER_NODE

/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 start */
#ifdef SX937X_FACTORY_NODE
#include "sensor_fac_node.h"
extern sar_driver_t g_facnode;
#endif  //SX937X_FACTORY_NODE
/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 end */

#ifdef SX937X_USER_NODE
#define VENDOR_NAME      "SEMTECH"
#define MODEL_NAME       "SX937X"
#endif //SX937X_USER_NODE

static bool g_Enabled = false;
/* M55 code for QN6887A-96 by zhangziyi at 20230925 end */

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
#include "sensor_user_node.h"
extern sar_driver_func_t g_usernode;
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

/* M55 code for SR-QN6887A-01-461|QN6887A-74 by lichang at 20230919 start */
/* M55 code for QN6887A-443 by zhangziyi at 20231028 start */
#ifdef SX937X_ANFR_LOGIC
const int g_irq_maxnum = 28;
const int g_cali_maxnum = 2;
static bool g_anfr_flag = true;
static int g_cali_count = 0;
static int g_irq_count = 0;
static bool g_sarpcbinfo = false;
#endif //SX937X_ANFR_LOGIC
/* M55 code for SR-QN6887A-01-461|QN6887A-74 by lichang at 20230919 end */

/*up down*/
static int gs_up_far = 0;
static int gs_up_prox1 = 0;
static int gs_up_prox2 = 0;
static int gs_down_far = 0;
static int gs_down_prox1 = 0;
static int gs_down_prox2 = 0;
/* M55 code for QN6887A-443 by zhangziyi at 20231028 end */

/* M55 code for QN6887A-4698 by lichang at 20240112 start */
typedef enum {
    SAR_WIFI = 0,
    SAR_MIMO = 1,
    SAR_DOWN = 2,
    SAR_UP = 3
}SAR_PH;

static int gs_ph  = SAR_DOWN;
typedef struct level_report_s
{
    int grip_sensor_up_far;
    int grip_sensor_up_prox1;
    int grip_sensor_up_prox2;
    int grip_sensor_down_far;
    int grip_sensor_down_prox1;
    int grip_sensor_down_prox2;
}level_report_t;
/* M55 code for QN6887A-4698 by lichang at 20240112 end */

#define I2C_M_WR_SMTC   0 /* for i2c Write */
#define I2C_M_RD_SMTC   1 /* for i2c Read */

#define NUM_IRQ_BITS 8

#define NUM_RETRY_ON_I2C_ERR    2
#define SLEEP_BETWEEN_RETRY     100 //in ms

//Refer to register 0x8000
typedef enum {
    IDLE = 0,
    PROX1 = 1,  //SX933X: PROXSTAT
    PROX2 = 2,  //SX933X: TABLESTAT
    PROX3 = 3,  //SX933X: BODYSTAT
    PROX4 = 4   //SX933X: Not used
}PROX_STATUS;

typedef enum {
    NOT_USED = 0,   //this phase is not used.
    MAIN = 1,       //its CS is connected to an antenna for detecting human proximity.
    REF = 2         //Reference phase. Used to correct the temperature drift of the correspoinding MAIN phase
}PHASE_USAGE;

//Refer to register 0x4280
typedef enum{
    CMD_ACTIVATE    = 0xF,
    CMD_COMPENSATE  = 0xE,
    CMD_SLEEP       = 0xD,
    CMD_WAKEUP      = 0xC,

}COMMANDS;

typedef struct reg_addr_val_s
{
    //Regiter is 16 bit, declare it as u32 is for parsing the dts.
    u32 addr;
    u32 val;
}reg_addr_val_t, *reg_addr_val_p;

typedef struct phase_s
{
    char *name; //Name to show in the input getevent
    struct input_dev *input;

    //Refer to register 0x8000, prox4_mask is not used by the sx933x
    u32 prox4_mask;
    u32 prox3_mask;
    u32 prox2_mask;
    u32 prox1_mask;
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
    char phase_status;
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 end */
    #ifdef SX937X_FACTORY_NODE
    int keycode_far;
    int keycode_close;
    #endif //SX937X_FACTORY_NODE
    bool enabled;
    PHASE_USAGE usage;
    PROX_STATUS state;
}phase_t, *phase_p;

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
struct dumpinfo {
    int ph_state;
    u16 ref_cap;
    u16 total_cap;
    s32 raw_data;
    s32 baseline_data;
    s32 diff_data;
};
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

typedef struct self_s
{
    struct i2c_client *client;
    int irq_id; /* irq number used */
    int gpio;
    int num_regs;   //Number of registers need to be configured. Parsed from dts
    reg_addr_val_t *regs_addr_val;

    phase_t *phases;

    /*Each bit represents whether its correspoding phase is used for(1) this runction or not(0)
    for example: main_phases=0x5=0b0101, means PHASE 2 and 0 are used as the main phase*/
    u32 main_phases;
    u32 ref_phases;

    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
    u32 fw_num;
    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

    //phases are for temperature reference correction
    int ref_phase_a;
    int ref_phase_b;
    int ref_phase_c;

    /* M55 code for QN6887A-74 by lichang at 20230919 start */
    bool irq_wake_status;
    /* M55 code for QN6887A-74 by lichang at 20230919 end */

    //Function that will be called back when corresponding IRQ is raised
    //Refer to register 0x4000
    void (*irq_handler[NUM_IRQ_BITS])(struct self_s* self);
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
    #ifdef SX937X_USER_NODE
    char channel_status;
    struct device *factory_device;
    struct device *factory_device_sub;
    struct device *factory_device_wifi;
    #endif //SX937X_USER_NODE
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 end */

}self_t, *Self;

/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 start */
#define SMTC_LOG_DBG(fmt, args...)   pr_debug ("[SENS_SAR] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_INF(fmt, args...)   pr_info ("[SENS_SAR] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_ERR(fmt, args...)   pr_err  ("[SENS_SAR] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)

static Self g_self;
/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 end */

/* M55 code for QN6887A-74 by lichang at 20230919 start */
static void smtc_log_raw_data(Self self);
static void smtc_process_touch_status(Self self);
/* M55 code for QN6887A-74 by lichang at 20230919 end */

//#################################################################################################
int _log_hex_data = 0;
int _log_dbg_data = 0;
u16 _reading_reg = 0xFFFF;

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
static bool g_onoff_enable = false;
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

//The value of all other variables not configured in the table are 0.
#if defined(SX933X)
static phase_t smtc_phase_table[] =
{
    {
        .name = "SMTC SAR Sensor CH0",
        .prox1_mask = 1 << 24,
        .prox2_mask = 1 << 16,
        .prox3_mask = 1 << 8
    },
    {
        .name = "SMTC SAR Sensor CH1",
        .prox1_mask = 1 << 25,
        .prox2_mask = 1 << 17,
        .prox3_mask = 1 << 9
    },
    {
        .name = "SMTC SAR Sensor CH2",
        .prox1_mask = 1 << 26,
        .prox2_mask = 1 << 18,
        .prox3_mask = 1 << 10
    },
    {
        .name = "SMTC SAR Sensor CH3",
        .prox1_mask = 1 << 27,
        .prox2_mask = 1 << 19,
        .prox3_mask = 1 << 11,
    },
    {
        .name = "SMTC SAR Sensor CH4",
        .prox1_mask = 1 << 28,
        .prox2_mask = 1 << 20,
        .prox3_mask = 1 << 12
    },
    {
        .name = "SMTC SAR Sensor CH5",
        .prox1_mask = 1 << 29,
        .prox2_mask = 1 << 21,
        .prox3_mask = 1 << 13
    }
};

#elif defined(SX937X)
static phase_t smtc_phase_table[] =
{
    /* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230925 start */
    {
        #ifdef SX937X_FACTORY_NODE
        .name = "grip_sensor_wifi_mimo",
        .keycode_close = KEY_SAR4_CLOSE,
        .keycode_far = KEY_SAR4_FAR,
        #else
        .name = "grip_sensor_wifi",
        #endif //SX937X_FACTORY_NODE
        .prox1_mask = 1 << 27,
        .prox2_mask = 1 << 19,
        .prox3_mask = 1 << 11,
        .prox4_mask = 1 << 3
    },
    {
        .name = "grip_sensor_wifi",
        #ifdef SX937X_FACTORY_NODE
        .keycode_close = KEY_SAR2_CLOSE,
        .keycode_far = KEY_SAR2_FAR,
        #endif //SX937X_FACTORY_NODE
        .phase_status = 0x04,
        .prox1_mask = 1 << 25,
        .prox2_mask = 1 << 17,
        .prox3_mask = 1 << 9,
        .prox4_mask = 1 << 1
    },
    {
        .name = "grip_sensor",
        #ifdef SX937X_FACTORY_NODE
        .keycode_close = KEY_SAR1_CLOSE,
        .keycode_far = KEY_SAR1_FAR,
        #endif //SX937X_FACTORY_NODE
        .phase_status = 0x01,
        .prox1_mask = 1 << 24,
        .prox2_mask = 1 << 16,
        .prox3_mask = 1 << 8,
        .prox4_mask = 1 << 0
    },
    {
        .name = "grip_sensor_sub",
        #ifdef SX937X_FACTORY_NODE
        .keycode_close = KEY_SAR3_CLOSE,
        .keycode_far = KEY_SAR3_FAR,
        #endif //SX937X_FACTORY_NODE
        .phase_status = 0x02,
        .prox1_mask = 1 << 26,
        .prox2_mask = 1 << 18,
        .prox3_mask = 1 << 10,
        .prox4_mask = 1 << 2
    },
    /* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230925 end */
    {
        .name = "SMTC SAR Sensor CH4",
        .prox1_mask = 1 << 28,
        .prox2_mask = 1 << 20,
        .prox3_mask = 1 << 12,
        .prox4_mask = 1 << 4
    },
    {
        .name = "SMTC SAR Sensor CH5",
        .prox1_mask = 1 << 29,
        .prox2_mask = 1 << 21,
        .prox3_mask = 1 << 13,
        .prox4_mask = 1 << 5
    },
    {
        .name = "SMTC SAR Sensor CH6",
        .prox1_mask = 1 << 30,
        .prox2_mask = 1 << 22,
        .prox3_mask = 1 << 14,
        .prox4_mask = 1 << 6
    },
    {
        .name = "SMTC SAR Sensor CH7",
        .prox1_mask = 0,
        .prox2_mask = 1 << 23,
        .prox3_mask = 1 << 15,
        .prox4_mask = 1 << 7
    }
};
#else
#error No chip is defined. Please define corresponding chip in the header file
#endif

//#################################################################################################
static int smtc_i2c_write(Self self, u16 reg_addr, u32 reg_val)
{
    int ret = -ENOMEM;
    int num_retried = 0;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg;
    unsigned char w_buf[6];

    i2c = self->client;
    w_buf[0] = (u8)(reg_addr>>8);
    w_buf[1] = (u8)(reg_addr);
    w_buf[2] = (u8)(reg_val>>24);
    w_buf[3] = (u8)(reg_val>>16);
    w_buf[4] = (u8)(reg_val>>8);
    w_buf[5] = (u8)(reg_val);

    msg.addr = i2c->addr;
    msg.flags = I2C_M_WR_SMTC;
    msg.len = 6; //2 bytes regaddr + 4 bytes data
    msg.buf = (u8 *)w_buf;

    while(true)
    {
        ret = i2c_transfer(i2c->adapter, &msg, 1);
        if (ret >= 0)
            break;

        if (num_retried++ < NUM_RETRY_ON_I2C_ERR)
        {
            SMTC_LOG_ERR("i2c write reg 0x%x error %d. Goint to retry", reg_addr, ret);
            if(SLEEP_BETWEEN_RETRY != 0)
                msleep(SLEEP_BETWEEN_RETRY);
        }
        else{
            SMTC_LOG_ERR("i2c write reg 0x%x error %d after retried %d times",
                reg_addr, ret, NUM_RETRY_ON_I2C_ERR);
             break;
        }
    }

    return ret;
}

//=================================================================================================
//return number of bytes read from the i2c device or error codes
static int smtc_i2c_read(Self self, u16 reg_addr, u32 *reg_val)
{
    int ret = -ENOMEM;
    int num_retried = 0;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg[2];
    u8 w_buf[2];
    u8 buf[4];


    if (self && self->client)
    {
        i2c = self->client;
        w_buf[0] = (u8)(reg_addr>>8);
        w_buf[1] = (u8)(reg_addr);

        msg[0].addr = i2c->addr;
        msg[0].flags = I2C_M_WR_SMTC;
        msg[0].len = 2;
        msg[0].buf = (u8 *)w_buf;

        msg[1].addr = i2c->addr;;
        msg[1].flags = I2C_M_RD_SMTC;
        msg[1].len = 4;
        msg[1].buf = (u8 *)buf;

        while(true)
        {
            ret = i2c_transfer(i2c->adapter, msg, 2);
            if (ret >= 0){
                *reg_val = (u32)buf[0]<<24 | (u32)buf[1]<<16 | (u32)buf[2]<<8 | (u32)buf[3];
                break;
            }

            if (num_retried++ < NUM_RETRY_ON_I2C_ERR)
            {
                SMTC_LOG_ERR("i2c read reg 0x%x error %d. Goint to retry", reg_addr, ret);
                if(SLEEP_BETWEEN_RETRY != 0)
                    msleep(SLEEP_BETWEEN_RETRY);
            }
            else{
                SMTC_LOG_ERR("i2c read reg 0x%x error %d after retried %d times",
                    reg_addr, ret, NUM_RETRY_ON_I2C_ERR);
                 break;
            }
       }

    }
    return ret;
}

//=================================================================================================
/* M55 code for QN6887A-551 by zhangziyi at 20231102 start */
static int smtc_send_command(Self self, COMMANDS cmd)
{
    int state = 0;
    int ret = 0;
    int retry_times = 100;
    while(1)
    {
        ret = smtc_i2c_read(self, REG_CMD_STATE, &state);
        if (ret < 0){
            SMTC_LOG_ERR("Failed to send command.");
            return -EIO;
        }
        if ((state & 1) == 0){
            SMTC_LOG_DBG("Chip is free and capable to process new command.");
            break;
        }

        if (--retry_times == 0){
            SMTC_LOG_ERR("Chip keeps busy after 10 times retry.");
            break;
        }
        msleep(10);
        SMTC_LOG_DBG("Chip is busy, go to retry.");
    }

    ret = smtc_i2c_write(self, REG_CMD, (u32)cmd);
    if (ret < 0){
        SMTC_LOG_ERR("Failed to send command.");
        return -EIO;
    }

    return 0;
}
/* M55 code for QN6887A-551 by zhangziyi at 20231102 end */

//#################################################################################################
static int smtc_read_and_clear_irq(Self self, u32 *irq_src)
{
    int ret = 0;
    u32 reg_val;

    ret = smtc_i2c_read(self, REG_IRQ_SRC, &reg_val);
    if (ret > 0)
    {
        reg_val &= 0xFF;
        SMTC_LOG_INF("irq_src= 0x%X", reg_val);

        if (irq_src){
            *irq_src = reg_val;
        }
        ret = 0;
    }
    else if (ret==0){
        ret = -EIO;
    }

    return ret;
}

//=================================================================================================
static int smtc_calibrate(Self self)
{
    int ret = 0;
    SMTC_LOG_INF("Enter");

    ret = smtc_send_command(self, CMD_COMPENSATE);
    if (ret){
        SMTC_LOG_ERR("Failed to calibrate. ret=%d", ret);
    }
    return ret;
}

//=================================================================================================
static int smtc_is_irq_low(Self self)
{
    return !gpio_get_value(self->gpio);
}


//#################################################################################################
static ssize_t smtc_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int count=0, ph;
    u32 reg_val = 0;
    u16 offset;
    Self self = dev_get_drvdata(dev);

    for(ph =0; ph < NUM_PHASES; ph++)
    {
        int shift = ph*4;
        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);

        count = sprintf(buf+count, "PH%d= %d", ph, offset);
    }

    SMTC_LOG_INF("%s", buf);
    return count;
}

//-------------------------------------------------------------------------------------------------
static ssize_t smtc_calibration_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    Self self = dev_get_drvdata(dev);
    SMTC_LOG_INF("Enter");
    smtc_calibrate(self);
    return count;
}

//=================================================================================================
static ssize_t smtc_reg_write_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 reg_addr = 0, reg_val = 0;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%x,%x", &reg_addr, &reg_val) != 2)
    {
        SMTC_LOG_ERR("Invalid command format. Example: ehco '0x4280,0xE' > register_write");
        return -EINVAL;
    }

    smtc_i2c_write(self, reg_addr, reg_val);
    SMTC_LOG_INF("0x%X= 0x%X", reg_addr, reg_val);
    return count;
}
//-------------------------------------------------------------------------------------------------
static ssize_t smtc_reg_write_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf,
        "\nUsage: echo reg_addr,reg_val > register_write\n"
        "Where reg_addr/reg_val are the address/value of the register you want to write.\n"
        "Example: echo 0x4004,0x74 > register_write\n");
}

//=================================================================================================
static ssize_t smtc_reg_read_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int ret;
    u32 reg_val=0;
    u16 reg_addr = 0;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%x", &reg_addr) != 1)
    {
        SMTC_LOG_ERR(
            "echo reg_addr > register_read; cat reg_addr\n"
            "Where reg_addr is the address of the register you want to read\n"
            "Example: echo 0x4000 > register_read; cat register_read\n");
        return -EINVAL;
    }

    _reading_reg = reg_addr;
    ret = smtc_i2c_read(self, reg_addr, &reg_val);

    if (ret < 0){
        SMTC_LOG_ERR("Failed to read register 0x%X", reg_addr);
    }else{
        SMTC_LOG_INF("0x%X= 0x%X", reg_addr, reg_val);
    }

    return count;
}
//-------------------------------------------------------------------------------------------------
static ssize_t smtc_reg_read_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int count=0, ret;
    u32 reg_val = 0;
    Self self = dev_get_drvdata(dev);

    if (_reading_reg == 0xFFFF){
        count = sprintf(buf,
            "\nUsage: echo reg_addr > register_read; cat register_read\n"
            "Where reg_addr is the address of the register you want to read\n"
            "Example: echo 0x4000 > register_read; cat register_read\n");
    }else{
        ret = smtc_i2c_read(self, _reading_reg, &reg_val);

        if (ret < 0){
            count = sprintf(buf, "Failed to read reg=0x%X\n", _reading_reg);
        }else{
            count = sprintf(buf, "0x%X= 0x%X\n", _reading_reg, reg_val);
        }
    }

    return count;
}

//=================================================================================================
static ssize_t smtc_raw_data_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int ph, bytes=0;
    u32 reg_val;
    u16 offset;
    s32 useful, average, diff;
    Self self = dev_get_drvdata(dev);

    for (ph=0; ph<NUM_PHASES; ph++)
    {
        int shift = ph*4;
        smtc_i2c_read(self, REG_PH0_USEFUL  + shift,    &reg_val);
        useful = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_AVERAGE + shift,    &reg_val);
        average = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_DIFF    + shift,    &reg_val);
        diff = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);

        bytes += sprintf(buf+bytes, "ph= %d useful= %-7d average= %-7d diff= %-7d offset= %d\n",
            ph, useful, average, diff, offset);

    }
    smtc_log_raw_data(self);

    return bytes;
}

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
struct dumpinfo g_dumpinfo;
u8 g_channel = 0;
const int g_bitoffset = 4;
const int g_bitshift = 10;
enum channel_info {
    CH0,
    CH1,
    CH2,
    CH3,
    CH4,
    CH5,
    CH6,
};

/* M55 code for QN6887A-4698 by lichang at 20240112 start */
static ssize_t smtc_set_dumpinfo(const char *buf, size_t count)
{
    u8 value = 0;

    if (sscanf(buf, "%d", &value) != 1) {
        SMTC_LOG_ERR("Invalid command format. Usage: ehco 1 > dumpinfo");
        return -EINVAL;
    }

    if (value > CH3) {
        SMTC_LOG_ERR("echo %d dumpinfo error.Exceeded the number of channels.", value);
        return count;
    } else if (value == CH0) {
        gs_ph = SAR_DOWN;
    } else if (value == CH1) {
        gs_ph = SAR_MIMO;
    } else if (value == CH2) {
        gs_ph = SAR_UP;
    } else if (value == CH3) {
        gs_ph = SAR_WIFI;
    }
    g_channel = value;
    SMTC_LOG_ERR("echo %d > dumpinfo", value);

    return count;
}

static ssize_t smtc_get_dumpinfo(char *buf)
{
    int shift = 0;
    u32 reg_val = 0;
    phase_p phase = NULL;
    level_report_t level_report;

    if (g_self != NULL) {
        phase = &smtc_phase_table[gs_ph];
        smtc_i2c_read(g_self, REG_PROX_STATUS, &reg_val);
        level_report.grip_sensor_up_far = !((reg_val >> 26) & 1);
        level_report.grip_sensor_up_prox1 = ((reg_val >> 26) & 1);
        level_report.grip_sensor_up_prox2 = (reg_val >> 18) & 1;
        level_report.grip_sensor_down_far = !((reg_val >> 24) & 1);
        level_report.grip_sensor_down_prox1 = (reg_val >> 24) & 1;
        level_report.grip_sensor_down_prox2 = (reg_val >> 16) & 1;

        if (gs_ph <= SAR_MIMO) {
            if (reg_val & phase->prox1_mask) {
                g_dumpinfo.ph_state = PROX1;
            } else {
                g_dumpinfo.ph_state = IDLE;
            }
        } else {
            if (gs_ph == SAR_DOWN) {
                if ((level_report.grip_sensor_up_far && level_report.grip_sensor_down_far) ||
                ((level_report.grip_sensor_up_prox1 && (!level_report.grip_sensor_up_prox2)) && level_report.grip_sensor_down_far) ||
                (level_report.grip_sensor_up_prox2 && level_report.grip_sensor_down_far) ||
                (level_report.grip_sensor_up_prox2 && (level_report.grip_sensor_down_prox1 && (!level_report.grip_sensor_down_prox2)))) {
                    g_dumpinfo.ph_state = IDLE;
                } else {
                    g_dumpinfo.ph_state = PROX1;
                }
            } else if (gs_ph == SAR_UP) {
                if (level_report.grip_sensor_up_far) {
                    g_dumpinfo.ph_state = IDLE;
                } else {
                     g_dumpinfo.ph_state = PROX1;
                }
            }
        }

        shift = g_channel * g_bitoffset;
        smtc_i2c_read(g_self, REG_PH0_USEFUL + shift, &reg_val);
        g_dumpinfo.raw_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_AVERAGE + shift, &reg_val);
        g_dumpinfo.baseline_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_DIFF + shift, &reg_val);
        g_dumpinfo.diff_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
        g_dumpinfo.total_cap = (u16)(reg_val & OFFSET_VAL_MASK);

        g_dumpinfo.total_cap = ((((g_dumpinfo.total_cap & 0x00003F10) >> 4) * 310) +
            (((g_dumpinfo.total_cap & 0x0000007F)) * 54))/1000;

        if (g_channel == CH0) {
            shift = CH4 * g_bitoffset;
            smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
            g_dumpinfo.ref_cap = (u16)(reg_val & OFFSET_VAL_MASK);
        } else if(g_channel == CH1) {
            shift = CH5 * g_bitoffset;
            smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
            g_dumpinfo.ref_cap = (u16)(reg_val & OFFSET_VAL_MASK);
        } else if(g_channel == CH3) {
            shift = CH6 * g_bitoffset;
            smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
            g_dumpinfo.ref_cap = (u16)(reg_val & OFFSET_VAL_MASK);
        } else {
            g_dumpinfo.ref_cap = 0;
            SMTC_LOG_INF("g_channel=%d", g_channel);
        }

        if (g_channel == CH0 || g_channel == CH1 || g_channel == CH3) {
            g_dumpinfo.ref_cap = ((((g_dumpinfo.total_cap & 0x00003F10) >> 4) * 310) +
                (((g_dumpinfo.ref_cap & 0x0000007F)) * 54))/1000;
        }

        return snprintf(buf, PAGE_SIZE, "%ld,%d,%d,%d,%d,%d,%d\n",
            g_self->fw_num, g_dumpinfo.ph_state, g_dumpinfo.total_cap, g_dumpinfo.ref_cap,
            g_dumpinfo.raw_data, g_dumpinfo.baseline_data, g_dumpinfo.diff_data);

        SMTC_LOG_INF("ph_state=%d", g_dumpinfo.ph_state);
    } else {
        SMTC_LOG_ERR("g_self is NULL!");
    }

    return 0;
}
/* M55 code for QN6887A-4698 by lichang at 20240112 end */

void smtc_user_cali(void)
{
    SMTC_LOG_INF("Enter");
    if (g_self != NULL) {
        smtc_calibrate(g_self);
    }
}

/* M55 code for SR-QN6887A-01-461 by lichang at 20230915 start */
#ifdef SX937X_ANFR_LOGIC
static void smtc_anfr_forcereport(Self self)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN) {
            continue;
        }

        input = phase->input;
        phase->state = PROX1;

        /* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230925 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 1);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 1);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
        /* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230925 end */
        SMTC_LOG_INF("%s force reports touch", phase->name);
    }
}
#endif //SX937X_ANFR_LOGIC
/* M55 code for SR-QN6887A-01-461 by lichang at 20230915 end */

//=================================================================================================
/*Each bit of the buf represents whether its correspoding phase is being enabled(1) or disabled(0)
for example: echo 0x05 > enable buf=0x5=0b0101, means PHASE 2 and 0 are being enabled,
and all other phases are being disabled
*/
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

//=================================================================================================
static void smtc_test_func(Self self)
{
    SMTC_LOG_INF("You can modify this function freely to test any of driver functionality.");
}

static ssize_t smtc_debug_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int cmd, arg;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%d,%d", &cmd, &arg) != 2)
    {
        SMTC_LOG_ERR("Invalid command format. Use 'cat debug' to show the usages.");
        return -EINVAL;
    }

    switch (cmd){
    case 0:
        smtc_test_func(self);
        break;
    case 1:
        _log_hex_data = arg;
        SMTC_LOG_INF("%s log hex data", arg ? "Enable" : "Disable");
        break;
    case 2:
        _log_dbg_data = arg;
        SMTC_LOG_INF("%s log debug data", arg ? "Enable" : "Disable");
        break;
   default:
        SMTC_LOG_ERR("Invalid command=%d. Use 'cat debug' to show the usages.", cmd);
    }

    return count;
}

//-------------------------------------------------------------------------------------------------
static ssize_t smtc_debug_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s",
        "Usage: echo 'cmd,arg' > debug\n"
        "cmd:\n"
        "1: Turn on(arg=1) | off(arg=0)\n"
        "   the logger that logs some of registers value(Useful, average, etc.) in hex format.\n"
        "2: Turn on(arg=1) | off(arg=0)\n"
        "   the logger that logs debug data, such as raw data, DeltaVar, etc."
    );
}

//=================================================================================================
//read and print the value of registers in the dts
static ssize_t smtc_dump_reg_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int i, bytes=0;
    u32 addr, val;
    Self self = dev_get_drvdata(dev);

    for (i=0; i<self->num_regs; i++)
    {
        addr = self->regs_addr_val[i].addr;

        if (smtc_i2c_read(self, addr, &val) > 0){
            bytes += sprintf(buf+bytes, "0x%X= 0x%X\n", addr, val);
        }else{
            bytes += sprintf(buf+bytes, "0x%X= FAILED\n", addr);
        }
    }
    return bytes;
}

/* M55 code for QN6887A-443 by zhangziyi at 20231028 start */
static void smtc_level_grip_report(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;
    bool need_sync = false;
    phase = &smtc_phase_table[ph];
    input = phase->input;

    gs_up_far = !((prox_state >> 26) & 1);
    gs_up_prox1 = ((prox_state >> 26) & 1);
    gs_up_prox2 = (prox_state >> 18) & 1;
    gs_down_far = !((prox_state >> 24) & 1);
    gs_down_prox1 = (prox_state >> 24) & 1;
    gs_down_prox2 = (prox_state >> 16) & 1;

    SMTC_LOG_INF("up_down up_far=%d up_prox1=%d up_prox2=%d down_far=%d down_prox1=%d down_prox2=%d",
                    gs_up_far, gs_up_prox1, gs_up_prox2, gs_down_far, gs_down_prox1, gs_down_prox2);

    switch (phase->state) {
        case PROX1:
            if ((gs_up_far && gs_down_far) || ((gs_up_prox1 && (!gs_up_prox2)) && gs_down_far) ||
                (gs_up_prox2 && gs_down_far) || (gs_up_prox2 && (gs_down_prox1 && (!gs_down_prox2)))) {
                phase->state = IDLE;
                SMTC_LOG_INF("%s up_down reports release", phase->name);
                if (g_onoff_enable == false) {
                    #ifdef SX937X_FACTORY_NODE
                    input_report_key(input, phase->keycode_far, 1);
                    input_report_key(input, phase->keycode_far, 0);
                    need_sync = true;
                    #else
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 2);
                        input_report_rel(input, REL_X, 2);
                        need_sync = true;
                    }
                    #endif
                }
            } else {
                SMTC_LOG_INF("%s up_down is PROX already", phase->name);
            }
            break;
        case IDLE:
            if ((gs_up_far && (gs_down_prox1 && (!gs_down_prox2))) || (gs_up_far && gs_down_prox2) ||
                ((gs_up_prox1 && (!gs_up_prox2)) && (gs_down_prox1 && (!gs_down_prox2))) ||
                ((gs_up_prox1 && (!gs_up_prox2)) && gs_down_prox2) || (gs_up_prox2 && gs_down_prox2)) {
                phase->state = PROX1;
                SMTC_LOG_INF("%s up_down reports PROX1", phase->name);
                if (g_onoff_enable == false) {
                    #ifdef SX937X_FACTORY_NODE
                    input_report_key(input, phase->keycode_close, 1);
                    input_report_key(input, phase->keycode_close, 0);
                    need_sync = true;
                    #else
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 1);
                        input_report_rel(input, REL_X, 2);
                        need_sync = true;
                    }
                    #endif
                }
            } else {
                    SMTC_LOG_INF("%s up_down is IDLE already", phase->name);
            }
            break;
        default:
            break;
    }

    if (need_sync) {
        input_sync(input);
    }
}
/* M55 code for QN6887A-443 by zhangziyi at 20231028 end */

/* M55 code for SR-QN6887A-01-462|QN6887A-4859 by zhangziyi at 20240116 start */
#ifdef SX937X_USER_NODE
static ssize_t sx937x_receiver_turnon_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int receiver_status = 0;

    if (sscanf(buf, "%x", &receiver_status) != 1) {
        SMTC_LOG_ERR("(receiver_report) The number of data are wrong\n");
        return -EINVAL;
    }
    /* M55 code for QN6887A-2145 by zhangziyi at 20231130 start */
    g_anfr_flag = false;
    /* M55 code for QN6887A-2145 by zhangziyi at 20231130 end */
    SMTC_LOG_INF("(receiver_report) anfr = %d\n", g_anfr_flag);

    return count;
}
#endif //SX937X_USER_NODE
/* M55 code for QN6887A-4859 by zhangziyi at 20240116 end */

//=================================================================================================
static DEVICE_ATTR(calibrate,           0664, smtc_calibration_show,    smtc_calibration_store);
static DEVICE_ATTR(register_write,      0664, smtc_reg_write_show,      smtc_reg_write_store);
static DEVICE_ATTR(register_read,       0664, smtc_reg_read_show,       smtc_reg_read_store);
static DEVICE_ATTR(raw_data,            0664, smtc_raw_data_show,       NULL);
static DEVICE_ATTR(debug,               0664, smtc_debug_show,          smtc_debug_store);
static DEVICE_ATTR(dump_reg,            0664, smtc_dump_reg_show,       NULL);
#ifdef SX937X_USER_NODE
static DEVICE_ATTR(receiver_turnon,     0664, NULL,                     sx937x_receiver_turnon_store);
#endif //SX937X_USER_NODE


static struct attribute *smtc_sysfs_nodes_attr[] =
{
    &dev_attr_calibrate.attr,
    &dev_attr_register_write.attr,
    &dev_attr_register_read.attr,
    &dev_attr_raw_data.attr,
    &dev_attr_debug.attr,
    &dev_attr_dump_reg.attr,
    #ifdef SX937X_USER_NODE
    &dev_attr_receiver_turnon.attr,
    #endif //SX937X_USER_NODE
    NULL,
};
/* M55 code for SR-QN6887A-01-462 by zhangziyi at 2023/10/18 end */

static struct attribute_group smtc_sysfs_nodes =
{
    .attrs = smtc_sysfs_nodes_attr,
};

/* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
#ifdef SX937X_USER_NODE
const char g_ch1_mask = 0x01;
const char g_ch2_mask = 0x02;
const char g_ch3_mask = 0x04;
const char g_all_mask = 0x07;

static void smtc_enable_report_wifi_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;
    int wifi_judge = 0;

    phase = &smtc_phase_table[ph];
    input = phase->input;
    wifi_judge = (prox_state >> 25) & 5;

    if (wifi_judge != 0) {
        phase->state = PROX1;
        SMTC_LOG_INF("%s reports PROX1, onoff:%d", phase->name, g_onoff_enable);
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 1);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
    } else {
        phase->state = IDLE;
        SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
    }
    SMTC_LOG_INF("PROX: wifi -- 1 wifi_mimo 4 IDLE : 0 | status = %d ", wifi_judge);
}

static void smtc_enable_report_grip_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;

    phase = &smtc_phase_table[ph];
    input = phase->input;

    if (prox_state & phase->prox1_mask) {
        SMTC_LOG_INF("%s reports PROX1 , onoff:%d", phase->name, g_onoff_enable);
        phase->state = PROX1;
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 1);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
    } else {
        SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
        phase->state = IDLE;
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
    }
}

static void smtc_enable_report(Self self, struct input_dev *temp_input)
{
    int ph = 0;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        if (strcmp(phase->input->name, temp_input->name) != 0) {
            continue;
        }

        input = phase->input;

        if (ph < 2) {
            smtc_enable_report_wifi_normal(self, prox_state, ph);
        } else {
            smtc_enable_report_grip_normal(self, prox_state, ph);
        }
    }
}

static void smtc_disable_report(Self self, struct input_dev *temp_input)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        if (strcmp(phase->input->name, temp_input->name) != 0) {
            continue;
        }

        input = phase->input;
        phase->state = IDLE;
        if (g_onoff_enable == false) {
            if (((self->channel_status) & (phase->phase_status)) == 0) {
                input_report_rel(input, REL_MISC, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                input_report_rel(input, REL_X, 2);
                /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                input_sync(input);
            }
        }
        SMTC_LOG_INF("%s reports release", phase->name);
    }
}

static ssize_t smtc_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    Self self = dev_get_drvdata(dev);
    int status = -1;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    SMTC_LOG_INF("dev->name:%s", temp_input_dev->name);

    if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
        if(self->channel_status & g_ch1_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
        if(self->channel_status & g_ch2_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
        if(self->channel_status & g_ch3_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    return snprintf(buf, 8, "%d\n", status);
}

static ssize_t smtc_enable_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    Self self = dev_get_drvdata(dev);
    u8 enable = 0;
    int ret = 0;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        SMTC_LOG_ERR("Invalid Argument");
        return ret;
    }

    SMTC_LOG_INF("dev->name:%s new_value = %u", temp_input_dev->name, enable);

    if (enable == 1) {
        if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
            if (!(self->channel_status & g_ch1_mask)) {
                self->channel_status |= g_ch1_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
            if (!(self->channel_status & g_ch2_mask)) {
                self->channel_status |= g_ch2_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
            if (!(self->channel_status & g_ch3_mask)) {
                self->channel_status |= g_ch3_mask;
            }
        } else {
            SMTC_LOG_ERR("input_dev error");
        }
        SMTC_LOG_INF("self->channel_status = 0x%x", self->channel_status);

        if (self->channel_status & g_all_mask) {
            g_Enabled = true;
        }

        if (self->irq_wake_status != true) {
            enable_irq_wake(self->irq_id);
            self->irq_wake_status = true;
            SMTC_LOG_INF("enable irq wake success.");
        }

        #if defined(SX937X_USER_NODE) && defined(SX937X_ANFR_LOGIC)
        if (g_anfr_flag) {
            smtc_anfr_forcereport(self);
        } else {
            smtc_enable_report(self, temp_input_dev);
        }
        #else
        {
            smtc_enable_report(self, temp_input_dev);
        }
        #endif //SX937X_USER_NODE && SX937X_ANFR_LOGIC
    } else if (enable == 0) {
        if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
            if (self->channel_status & g_ch1_mask) {
                self->channel_status &= ~g_ch1_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
            if (self->channel_status & g_ch2_mask) {
                self->channel_status &= ~g_ch2_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
            if (self->channel_status & g_ch3_mask) {
                self->channel_status &= ~g_ch3_mask;
            }
        } else {
            SMTC_LOG_ERR("input_dev error");
        }

        if (!(self->channel_status & g_all_mask)) {
            g_Enabled = false;
        }

        if (self->irq_wake_status != false) {
            disable_irq_wake(self->irq_id);
            self->irq_wake_status = false;
            SMTC_LOG_INF("disable irq wake success.");
        }

        smtc_disable_report(self, temp_input_dev);
    } else {
        SMTC_LOG_ERR("Invalid param. Usage: ehco 0/1 > enable");
    }

    return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        smtc_enable_show, smtc_enable_store);

static struct attribute *ss_smtc_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group ss_smtc_attributes_group = {
    .attrs = ss_smtc_attributes
};

static ssize_t smtc_vendor_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t smtc_name_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t smtc_grip_flush_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val = 0;
    int ret = 0;
    int ph = 0;
    phase_p phase = NULL;
    struct input_dev *input = NULL;

    ret = kstrtou8(buf, 10, &val);
    if (ret < 0) {
        SMTC_LOG_INF("Invalid value of input, input=%ld", ret);
        return -EINVAL;
    }

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN) {
            continue;
        }

        input = phase->input;

        if (!strcmp(dev->kobj.name, phase->name)) {
            /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
            if (g_onoff_enable == false) {
                input_report_rel(input, REL_MAX, val);
                input_sync(input);
            }
            /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
            SMTC_LOG_INF("ret=%u, name=%s", val, phase->name);
            return count;
        }
    }

    SMTC_LOG_INF("unknown type");

    return count;
}

static DEVICE_ATTR(name, S_IRUGO, smtc_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, smtc_vendor_show, NULL);
static DEVICE_ATTR(grip_flush, S_IWUSR | S_IWGRP, NULL, smtc_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_grip_flush,
    NULL,
};
#endif //USER_NODE
/* M55 code for QN6887A-96 by zhangziyi at 20230925 end */

//#################################################################################################
static void smtc_get_ph_prox_state(u32 prox_reg_val, PROX_STATUS ph_state[NUM_PHASES])
{
    int ph;
    phase_p phase;

    for (ph=0; ph<(sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++)
    {
        phase = &smtc_phase_table[ph];
        //The prox4_mask of sx933x is always 0
        if (prox_reg_val & phase->prox4_mask){
            ph_state[ph] = PROX4;
        }
        else if (prox_reg_val & phase->prox3_mask){
            ph_state[ph] = PROX3;
        }
        else if (prox_reg_val & phase->prox2_mask){
            ph_state[ph] = PROX2;
        }
        else if (prox_reg_val & phase->prox1_mask){
            ph_state[ph] = PROX2;
        }
        else{
            ph_state[ph] = IDLE;
        }
    }
}


//#################################################################################################
static int smtc_parse_dts(Self self)
{
    int ph;
    struct device_node *dNode = self->client->dev.of_node;
    enum of_gpio_flags flags;

    if (!dNode)
    {
        SMTC_LOG_ERR("dNode == NULL");
        return -EINVAL;
    }

    self->gpio = of_get_named_gpio_flags(dNode, "Semtech,nirq-gpio", 0, &flags);
    SMTC_LOG_DBG("irq_gpio= %d", self->gpio);

    if (self->gpio < 0)
    {
        SMTC_LOG_ERR("Failed to get irq_gpio.");
        return -EINVAL;
    }

    if (of_property_read_u32(dNode, "Semtech,main-phases", &self->main_phases) )
    {
        SMTC_LOG_ERR("Failed to get main-phases");
        return -EINVAL;
    }
    SMTC_LOG_DBG("main-phases= 0x%X", self->main_phases);

    //.............................................................................................
    //load reference phase setup
    if (of_property_read_u32(dNode, "Semtech,ref-phases", &self->ref_phases) )
    {
        SMTC_LOG_ERR("Failed to get ref-phases");
        return -EINVAL;
    }
    SMTC_LOG_DBG("ref-phases= 0x%X", self->ref_phases);
    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
    if (of_property_read_u32(dNode, "Semtech,fw-num", &self->fw_num)) {
        SMTC_LOG_ERR("Failed to get fw-num");
        return -EINVAL;
    }
    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

    self->ref_phase_a = -1;
    self->ref_phase_b = -1;
    self->ref_phase_c = -1;

    for (ph=0; ph<NUM_PHASES; ph++)
    {
        if (1<<ph & self->ref_phases){
            if (self->ref_phase_a==-1){
                self->ref_phase_a = ph;
            }
            else if (self->ref_phase_b==-1){
                self->ref_phase_b = ph;
            }
            else if (self->ref_phase_c==-1){
                self->ref_phase_c = ph;
            }
            else{
                SMTC_LOG_ERR("Max 3 reference phases are supported. ref_phases passed from dts= 0x%X",
                    self->ref_phases);
                return -EINVAL;
            }
        }
    }
    SMTC_LOG_DBG("ref_phase_a= %d ref_phase_b= %d ref_phase_c= %d",
        self->ref_phase_a, self->ref_phase_b, self->ref_phase_c);

    //.............................................................................................
    //load register settings
    of_property_read_u32(dNode, "Semtech,reg-num", &self->num_regs);
    SMTC_LOG_DBG("number of registers= %d", self->num_regs);

    if (unlikely(self->num_regs <= 0))
    {
        SMTC_LOG_ERR("Invalid reg_num= %d", self->num_regs);
        return -EINVAL;
    }
    else
    {
        // initialize platform reg data array
        self->regs_addr_val = devm_kzalloc(&self->client->dev, sizeof(reg_addr_val_t)*self->num_regs, GFP_KERNEL);
        if (unlikely(!self->regs_addr_val))
        {
            SMTC_LOG_ERR("Failed to alloc memory, num_reg= %d", self->num_regs);
            return -ENOMEM;
        }

        if (of_property_read_u32_array(dNode, "Semtech,reg-init",
                                (u32*)self->regs_addr_val,
                                sizeof(reg_addr_val_t) * self->num_regs/sizeof(u32)))
       {
            SMTC_LOG_ERR("Failed to load registers from the dts");
            return -EINVAL;
        }
    }

    return 0;
}

//=================================================================================================
static int smtc_check_hardware(Self self)
{
    int ret;
    u32 chip_id;

    ret = smtc_i2c_read(self, REG_DEV_INFO, &chip_id);
    if(ret < 0)
    {
        SMTC_LOG_ERR("Failed to read chip id. ret= %d.", ret);
        return ret;
    }

    SMTC_LOG_INF("chip_id= 0x%X", chip_id);
    return 0;
}

//=================================================================================================
static int smtc_init_irq_gpio(Self self)
{
    int ret = 0;
    SMTC_LOG_DBG("");

    if (!gpio_is_valid(self->gpio))
    {
        SMTC_LOG_ERR("Invalid irq_gpio= %d. Please check DTS stup.", self->gpio);
        return -EINVAL;
    }

    ret = gpio_request(self->gpio, "smtc_nirq_gpio");
    if (ret < 0)
    {
        SMTC_LOG_ERR("Failed to request GPIO= %d", self->gpio);
        return ret;
    }

    ret = gpio_direction_input(self->gpio);
    if (ret < 0)
    {
        SMTC_LOG_ERR("Failed to set GPIO= %d as input", self->gpio);
        gpio_free(self->gpio);
        return ret;
    }

    self->irq_id = self->client->irq = gpio_to_irq(self->gpio);
    SMTC_LOG_DBG("Get irq= %d from gpio= %d", self->irq_id, self->gpio);


    return ret;
}


//=================================================================================================
static int smtc_init_registers(Self self)
{
    int i = 0, ret=0;
    u16 reg_addr;
    u32 reg_val;

    SMTC_LOG_INF("Going to init registers passed from the DTS");
    for (i=0; i < self->num_regs; i++)
    {
        reg_addr = self->regs_addr_val[i].addr;
        reg_val  = self->regs_addr_val[i].val;
        SMTC_LOG_DBG("0x%X= 0x%X", reg_addr, reg_val);

        ret = smtc_i2c_write(self, reg_addr, reg_val);
        if (ret < 0)
        {
            SMTC_LOG_ERR("Failed to write reg=0x%x value=0x%X", reg_addr, reg_val);
            return ret;
        }
    }

    /*Chip must be activated once after reset.
    You can disable all phases in the register settings if your product need to
    put the SAR sensor into the sleep mode during startup
    */
    ret = smtc_send_command(self, CMD_ACTIVATE);
    if (ret){return ret;}

    return 0;
}

//=================================================================================================
static int smtc_wait_reset_done(Self self)
{
    int ret = 0, i;
    u32 irq_src=0;

    for (i=0; i<10; i++)
    {
        msleep(10);
        ret = smtc_i2c_read(self, REG_IRQ_SRC, &irq_src);
        if (ret > 0 && irq_src != 0)
            return 0;
    }

    SMTC_LOG_ERR("Failed to read reset IRQ");
    return -EIO;
}

//=================================================================================================
static int smtc_reset_and_init_chip(Self self)
{
    int ret = 0;
    SMTC_LOG_INF("Enter");

    ret = smtc_i2c_write(self, REG_RESET, 0xDE);
    if(ret<0){return ret;}

    ret = smtc_wait_reset_done(self);
    if(ret){return ret;}

    ret = smtc_init_registers(self);
    if(ret){return ret;}

    //make sure no interrupts are pending
    ret = smtc_read_and_clear_irq(self, NULL);
    return ret;
}

//#################################################################################################
static void smtc_log_dbg_data(Self self)
{
    u16 off;
    int ph, state;
    u32 reg_val;
    s32 avg, diff, main_use, main_raw, dlt_var;
    PROX_STATUS ph_prox_state[NUM_PHASES];

    int ref_ph_a, ref_ph_b, ref_ph_c;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    char ref_a_name[] = "na";
    char ref_b_name[] = "na";
    char ref_c_name[] = "na";

    ref_ph_a = self->ref_phase_a;
    ref_ph_b = self->ref_phase_b;
    ref_ph_c = self->ref_phase_c;

    smtc_i2c_read(self, REG_DBG_SEL, &reg_val);
    ph = reg_val>>3 & 0x7;

    smtc_i2c_read(self, REG_PROX_STATUS, &reg_val);
    SMTC_LOG_DBG("REG_PROX_STATUS=0x%X", reg_val);
    smtc_get_ph_prox_state(reg_val, ph_prox_state);
    state = ph_prox_state[ph];

    if(ref_ph_a != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_a*4, &reg_val);
        ref_a_use = (s32)reg_val >> 10;
        sprintf(ref_a_name, "%d", ref_ph_a);
    }
    if(ref_ph_b != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_b*4, &reg_val);
        ref_b_use = (s32)reg_val >> 10;
        sprintf(ref_b_name, "%d", ref_ph_b);
    }
    if(ref_ph_c != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_c*4, &reg_val);
        ref_c_use = (s32)reg_val >> 10;
        sprintf(ref_c_name, "%d", ref_ph_c);
    }

    smtc_i2c_read(self, REG_RAW_DATA, &reg_val);
    main_raw = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_DLT_VAR, &reg_val);
    dlt_var = (s32)reg_val>>4;

    smtc_i2c_read(self, REG_PH0_USEFUL + ph*4, &reg_val);
    main_use = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_AVERAGE + ph*4, &reg_val);
    avg = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_DIFF + ph*4, &reg_val);
    diff = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_OFFSET + ph*4*OFFSET_PH_REG_SHIFT, &reg_val);
    off = (u16)(reg_val & OFFSET_VAL_MASK);

    pr_info(
    "SMTC_DBG PH=%d USE=%d RAW=%d PH%s_USE=%d PH%s_USE=%d PH%s_USE=%d STATE=%d AVG=%d DIFF=%d OFF=%d DLT=%d SMTC_END\n",
    ph, main_use, main_raw, ref_a_name, ref_a_use, ref_b_name, ref_b_use, ref_c_name, ref_c_use,
    state, avg, diff, off, dlt_var);
}

//=================================================================================================
static void smtc_log_raw_data(Self self)
{
    int ph, state;
    u16 offset;
    s32 useful, average, diff;
    u32 reg_val, dbg_ph;
    u32 use_hex, avg_hex, dif_hex, dlt_hex;
    PROX_STATUS ph_prox_state[NUM_PHASES];

    int ref_ph_a, ref_ph_b, ref_ph_c;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    char ref_a_name[] = "na";
    char ref_b_name[] = "na";
    char ref_c_name[] = "na";

    ref_ph_a = self->ref_phase_a;
    ref_ph_b = self->ref_phase_b;
    ref_ph_c = self->ref_phase_c;

    smtc_i2c_read(self, REG_PROX_STATUS, &reg_val);
    SMTC_LOG_DBG("prox_state= 0x%X", reg_val);
    smtc_get_ph_prox_state(reg_val, ph_prox_state);

    smtc_i2c_read(self, REG_DBG_SEL, &dbg_ph);
    dbg_ph = (dbg_ph >> 3) & 0x7;
    smtc_i2c_read(self, REG_DLT_VAR, &dlt_hex);

    if(ref_ph_a != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_a*4, &reg_val);
        ref_a_use = (s32)reg_val >> 10;
        sprintf(ref_a_name, "%d", ref_ph_a);
    }
    if(ref_ph_b != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_b*4, &reg_val);
        ref_b_use = (s32)reg_val >> 10;
        sprintf(ref_b_name, "%d", ref_ph_b);
    }
    if(ref_ph_c != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_c*4, &reg_val);
        ref_c_use = (s32)reg_val >> 10;
        sprintf(ref_c_name, "%d", ref_ph_c);
    }

    for(ph =0; ph<NUM_PHASES; ph++)
    {
        int shift = ph*4;
        smtc_i2c_read(self, REG_PH0_USEFUL  + shift,    &use_hex);
        useful = (s32)use_hex>>10;
        smtc_i2c_read(self, REG_PH0_AVERAGE + shift,    &avg_hex);
        average = (s32)avg_hex>>10;
        smtc_i2c_read(self, REG_PH0_DIFF    + shift,    &dif_hex);
        diff = (s32)dif_hex>>10;
        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);
        state = ph_prox_state[ph];

        pr_info(
        "SMTC_DAT PH= %d DIFF= %d USE= %d PH%s_USE= %d PH%s_USE= %d PH%s_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
        ph, diff, useful, ref_a_name, ref_a_use, ref_b_name, ref_b_use, ref_c_name, ref_c_use, state, offset, average);

        if (_log_hex_data){
            pr_info(
            "SMTC_HEX PH= %d USE= 0x%X AVG= 0x%X DIF= 0x%X PH%d_DLT= 0x%X SMTC_END\n",
            ph, use_hex, avg_hex, dif_hex, dbg_ph, dlt_hex);
        }
    }

    if (_log_dbg_data){
        smtc_log_dbg_data(self);
    }
}

//=================================================================================================
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
/* M55 code for SR-QN6887A-01-461 by lichang at 20230915 start */
/* M55 code for QN6887A-82 by zhangziyi at 20230921 start */
#ifdef SX937X_FACTORY_NODE
static void smtc_process_touch_status_factory(Self self)
{
    int ph;
    u32 prox_state = 0;
    bool need_sync = false;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        need_sync = false;

        /* M55 code for QN6887A-443 by zhangziyi at 20231028 start */
        if (ph != 2) {
            if (phase->state == IDLE) {
                if (prox_state & phase->prox1_mask) {
                    SMTC_LOG_INF("%s reports PROX1", phase->name);
                    phase->state = PROX1;
                    if (g_onoff_enable == false) {
                            input_report_key(input, phase->keycode_close, 1);
                            input_report_key(input, phase->keycode_close, 0);
                            need_sync = true;
                    }
                } else {
                    SMTC_LOG_INF("%s is IDLE already", phase->name);
                }
            } else {
                if (prox_state & phase->prox1_mask) {
                    SMTC_LOG_INF("%s is PROX1 already", phase->name);
                } else {
                    SMTC_LOG_INF("%s reports release", phase->name);
                    phase->state = IDLE;
                    if (g_onoff_enable == false) {
                        input_report_key(input, phase->keycode_far, 1);
                        input_report_key(input, phase->keycode_far, 0);
                        need_sync = true;
                    }
                }
            }
        } else if (ph == 2) {
            smtc_level_grip_report(self, prox_state, ph);
        }
        /* M55 code for QN6887A-443 by zhangziyi at 20231028 end */
        if (need_sync) {
            input_sync(input);
        }
    }
}
#else
/* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
static void smtc_process_touch_status_wifi_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;
    int wifi_judge = 0;

    phase = &smtc_phase_table[ph];
    input = phase->input;
    wifi_judge = (prox_state >> 25) & 5;

    if (phase->state == IDLE) {
        if (wifi_judge != 0) {
            phase->state = PROX1;
            SMTC_LOG_INF("%s report PROX1", phase->name);
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 1);
                    /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                    input_report_rel(input, REL_X, 2);
                    /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                    input_sync(input);
                }
            }
        } else {
            SMTC_LOG_INF("%s is IDLE already", phase->name);
        }
    } else {
        if (wifi_judge != 0) {
            SMTC_LOG_INF("%s is PROX1 already", phase->name);
        } else {
            phase->state = IDLE;
            SMTC_LOG_INF("%s report Release", phase->name);
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 2);
                    /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                    input_report_rel(input, REL_X, 2);
                    /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                    input_sync(input);
                }
            }
        }
    }
    SMTC_LOG_INF("PROX: wifi -- 1 wifi_mimo 4 IDLE : 0 | status = %d ", wifi_judge);
}

static void smtc_process_touch_status_grip_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;

    phase = &smtc_phase_table[ph];
    input = phase->input;

    /* M55 code for QN6887A-443 by zhangziyi at 20231028 start */
    if (ph != 2) {
        if (phase->state == IDLE) {
            if (prox_state & phase->prox1_mask) {
                SMTC_LOG_INF("%s reports PROX1", phase->name);
                phase->state = PROX1;
                if (g_onoff_enable == false) {
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 1);
                        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                        input_report_rel(input, REL_X, 2);
                        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                        input_sync(input);
                    }
                }
            } else {
                SMTC_LOG_INF("%s is IDLE already", phase->name);
            }
        } else {
            if (prox_state & phase->prox1_mask) {
                SMTC_LOG_INF("%s is PROX1 already", phase->name);
            } else {
                SMTC_LOG_INF("%s reports release", phase->name);
                phase->state = IDLE;
                if (g_onoff_enable == false) {
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 2);
                        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
                        input_report_rel(input, REL_X, 2);
                        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
                        input_sync(input);
                    }
                }
            }
        }
    } else if (ph == 2) {
        smtc_level_grip_report(self, prox_state, ph);
    }
    /* M55 code for QN6887A-443 by zhangziyi at 20231028 end */
}

static void smtc_process_touch_status_normal(Self self)
{
    int ph;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        if (ph < 2) {
            smtc_process_touch_status_wifi_normal(self, prox_state, ph);
        } else {
            smtc_process_touch_status_grip_normal(self, prox_state, ph);
        }
    }
}
#endif
/* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230921 end */
/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

static void smtc_process_touch_status(Self self)
{
    #ifdef SX937X_ANFR_LOGIC
    if (g_anfr_flag) {
        smtc_anfr_forcereport(self);
    } else {
        smtc_process_touch_status_normal(self);
    }
    #else
        /* M55 code for QN6887A-82 by zhangziyi at 20230921 start */
        smtc_process_touch_status_factory(self);
        /* M55 code for QN6887A-82 by zhangziyi at 20230921 end */
    #endif //SX937X_ANFR_LOGIC
}
//#################################################################################################
/* M55 code for QN6887A-363 by zhangziyi at 20231026 start */
static void smtc_process_irq(Self self)
{
    int ret, irq_bit, irq_src;
    SMTC_LOG_DBG("Enter");

    ret = smtc_read_and_clear_irq(self, &irq_src);
    if (ret) {
        SMTC_LOG_ERR("Failed to read irq source. ret=%d.", ret);
        return;
    }

    #ifdef SX937X_ANFR_LOGIC
    if (g_cali_count < g_cali_maxnum && ((irq_src >> 4) & 0x01)) {
        g_cali_count++;
        SMTC_LOG_INF("g_cali_count:%d", g_cali_count);
    }
    #endif //SX937X_ANFR_LOGIC

    for (irq_bit = 0; irq_bit < NUM_IRQ_BITS; irq_bit++) {
        if (irq_src >> irq_bit & 0x1) {
            if (self->irq_handler[irq_bit]) {
                //call to smtc_process_touch_status() or smtc_log_raw_data()
                self->irq_handler[irq_bit](self);
            } else {
                SMTC_LOG_ERR("No handler to IRQ bit= %d", irq_bit);
            }
            SMTC_LOG_ERR("IRQ bit=%d, irq_src=%d", irq_bit, irq_src);
        }
    }
}
/* M55 code for QN6887A-363 by zhangziyi at 20231026 end */

//=================================================================================================
//call flow: smtc_irq_handler--> smtc_worker_func--> smtc_process_touch_status
static irqreturn_t smtc_irq_handler(int irq, void *pvoid)
{
    //unsigned long flags;
    Self self = (Self)pvoid;
    SMTC_LOG_INF("IRQ= %d is received", irq);

    #ifdef SX937X_ANFR_LOGIC
    if ((g_anfr_flag == true) && (g_irq_count < g_irq_maxnum)) {
        g_irq_count++;
    }

    if ((g_irq_count >= g_irq_maxnum) || (g_cali_count >= g_cali_maxnum)) {
        g_anfr_flag = false;
    }
    SMTC_LOG_INF("irq_count:%d, cali_count:%d, anfr_flag:%d", g_irq_count, g_cali_count, g_anfr_flag);
    #endif //SX937X_ANFR_LOGIC

    /* M55 code for QN6887A-363 by zhangziyi at 20231026 start */
    if (likely(smtc_is_irq_low(self))) {
        smtc_process_irq(self);
    } else {
        SMTC_LOG_ERR("GPIO=%d must be low when an IRQ is received.", self->gpio);
    }
    /* M55 code for QN6887A-363 by zhangziyi at 20231026 end */

    return IRQ_HANDLED;
}
/* M55 code for SR-QN6887A-01-461 by lichang at 20230915 end */

/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 start */
#ifdef SX937X_FACTORY_NODE
static bool g_facEnable = false;
void factory_cali(void)
{
    SMTC_LOG_INF("Enter");
    if (g_self != NULL) {
        smtc_calibrate(g_self);
    }
}

ssize_t factory_set_enable(const char *buf, size_t count)
{
    u32 enable = 0;

    if (sscanf(buf, "%x", &enable) != 1) {
        SMTC_LOG_INF("The number of param are wrong\n");
        return -EINVAL;
    }

    if (enable == 1) {
        g_facEnable = true;
        SMTC_LOG_INF("enable success.");
    } else if (enable == 0) {
        g_facEnable = false;
        SMTC_LOG_INF("disable success.");
    } else {
        SMTC_LOG_ERR("Invalid param. Usage: ehco 0/1 > enable");
    }

    return count;
}

ssize_t factory_get_enable(char *buf)
{
    return sprintf(buf, "%d\n", g_facEnable);
}
#endif //SX937X_FACTORY_NODE
/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 end */

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
/* M55 code for QN6887A-82 by zhangziyi at 20230921 start */
#ifdef SX937X_FACTORY_NODE
static void smtc_onoff_disablereport_facory(Self self)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 0; ph < sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0]); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        phase->state = IDLE;
        if (g_onoff_enable == false) {
            input_report_key(input, phase->keycode_far, 1);
            input_report_key(input, phase->keycode_far, 0);
            input_sync(input);
        }
        SMTC_LOG_INF("%s reports release", phase->name);
    }
}

static void smtc_onoff_enablereport_factory(Self self)
{
    int ph;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;

        if (prox_state & phase->prox1_mask) {
            SMTC_LOG_INF("%s reports PROX1, onoff = %d", phase->name, g_onoff_enable);
            phase->state = PROX1;
            if (g_onoff_enable == false) {
                input_report_key(input, phase->keycode_close, 1);
                input_report_key(input, phase->keycode_close, 0);
                input_sync(input);
                SMTC_LOG_INF("%s g_onoff_enable = %d", phase->name, g_onoff_enable);
            }
        } else {
            SMTC_LOG_INF("%s reports Release, onoff = %d", phase->name, g_onoff_enable);
            phase->state = IDLE;
            if (g_onoff_enable == false) {
                input_report_key(input, phase->keycode_far, 1);
                input_report_key(input, phase->keycode_far, 0);
                input_sync(input);
            }
        }
    }
}
#else
/* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
static void smtc_onoff_disablereport(Self self)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 1; ph < sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0]); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        phase->state = IDLE;
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
        SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
    }
}

static void smtc_onoff_enablereport_wifi_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;
    int wifi_judge = 0;

    phase = &smtc_phase_table[ph];
    input = phase->input;
    wifi_judge = (prox_state >> 25) & 5;

    if (wifi_judge != 0) {
        phase->state = PROX1;
        SMTC_LOG_INF("%s reports PROX1, onoff:%d", phase->name, g_onoff_enable);
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 1);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
    } else {
        phase->state = IDLE;
        SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
    }
    SMTC_LOG_INF("PROX: wifi -- 1 wifi_mimo 4 IDLE : 0 | status = %d ", wifi_judge);
}

static void smtc_onoff_enablereport_grip_normal(Self self, u32 prox_state, int ph)
{
    phase_p phase;
    struct input_dev *input;

    phase = &smtc_phase_table[ph];
    input = phase->input;

    if (prox_state & phase->prox1_mask) {
        SMTC_LOG_INF("%s reports PROX1 , onoff:%d", phase->name, g_onoff_enable);
        phase->state = PROX1;
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 1);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
    } else {
        SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
        phase->state = IDLE;
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
    }
}

static void smtc_onoff_enablereport(Self self)
{
    int ph = 0;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 1; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;

        if (ph < 2) {
            smtc_onoff_enablereport_wifi_normal(self, prox_state, ph);
        } else {
            smtc_onoff_enablereport_grip_normal(self, prox_state, ph);
        }
    }
}
#endif //SX937X_FACTORY_NODE

// on:1 off:0
ssize_t smtc_set_onoff(const char *buf, size_t count)
{
    if (g_self != NULL) {
        if (!strncmp(buf, "0", 1)) {
            #ifdef SX937X_FACTORY_NODE
            smtc_onoff_disablereport_facory(g_self);
            #else
            smtc_onoff_disablereport(g_self);
            #endif
            g_onoff_enable = true;
            SMTC_LOG_INF("onoff Function : off");
        } else if (!strncmp(buf, "1", 1)) {
            g_onoff_enable = false;
            #ifdef SX937X_FACTORY_NODE
            smtc_onoff_enablereport_factory(g_self);
            #else
            smtc_onoff_enablereport(g_self);
            #endif
            SMTC_LOG_INF("onoff Function set : on");
        } else {
            SMTC_LOG_ERR("invalid param!");
        }
    } else {
        SMTC_LOG_ERR("g_self if NULL!");
    }
    return count;
}
/* M55 code for QN6887A-82|QN6887A-96 by zhangziyi at 20230921 end */

ssize_t smtc_get_onoff(char *buf)
{
    return snprintf(buf, 8, "%d\n", g_onoff_enable);
}

/* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */
//#################################################################################################
/* M55 code for QN6887A-74 by lichang at 20230919 start */
#ifdef SX937X_ANFR_LOGIC
static bool smtc_pcbainfo_judge(void)
{
    const char *pcba_info = NULL;
    struct device_node *chosen = NULL;

    chosen = of_find_node_by_name(NULL, "chosen");
    if (NULL == chosen) {
        SMTC_LOG_ERR("failed to find node.\n");
        return false;
    }
    of_property_read_string(chosen, "bootargs", &pcba_info);
    SMTC_LOG_ERR("pcba_info = %s .\n", pcba_info);
    if (strstr(pcba_info, "pcbainfo=EUR")) {
        SMTC_LOG_ERR("pcba_info is EUR.\n");
        return true;
    } else {
        SMTC_LOG_ERR("pcba_info is not EUR.\n");
        return false;
    }
}
#endif //SX937X_ANFR_LOGIC
/* M55 code for QN6887A-74 by lichang at 20230919 end */

static int smtc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0, i;
    Self self = NULL;
    struct input_dev *input = NULL;
    struct i2c_adapter *adapter = NULL;
    SMTC_LOG_INF("Start to initialize Semtech SAR sensor %s driver.", SMTC_DRIVER_NAME);

    /* M55 code for QN6887A-74 by lichang at 20230919 start */
    #ifdef SX937X_ANFR_LOGIC
    g_sarpcbinfo = smtc_pcbainfo_judge();
    g_anfr_flag = g_sarpcbinfo;
    SMTC_LOG_ERR("pcbainfo anfr_flag = %d\n.", g_anfr_flag);
    #endif //SX937X_ANFR_LOGIC
    /* M55 code for QN6887A-74 by lichang at 20230919 end */

    adapter = to_i2c_adapter(client->dev.parent);
    ret = i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA);
    if (!ret)
    {
        SMTC_LOG_ERR("Failed to check i2c functionality.");
        return ret;
    }

    //kernel will automatically free the memory allocated by the devm_kzalloc
    //when probe is failed and driver is removed.
    self = devm_kzalloc(&client->dev, sizeof(self_t), GFP_KERNEL);
    if (!self)
    {
        SMTC_LOG_ERR("Failed to create self. memory size=%d bytes.", sizeof(self_t));
        return -ENOMEM;
    }

    client->dev.platform_data = self;
    i2c_set_clientdata(client, self);
    self->client = client;
    SMTC_LOG_DBG("self=0x%p platform_data=0x%p", self, client->dev.platform_data);

    ret = smtc_parse_dts(self);
    if (ret) {return ret;}

    ret = smtc_init_irq_gpio(self);
    if (ret) {return ret;}

    ret = smtc_check_hardware(self);
    if (ret) {goto FREE_GPIO;}

    ret = smtc_reset_and_init_chip(self);
    if (ret) {goto FREE_GPIO;}


    //refer to register 0x4000
    self->irq_handler[0] = 0; /* UNUSED */
    self->irq_handler[1] = 0; /* UNUSED */
    self->irq_handler[2] = 0; /* UNUSED */
    self->irq_handler[3] = smtc_log_raw_data;       //CONVEDONEIRQ
    self->irq_handler[4] = smtc_process_touch_status; //COMPDONEIRQ
    self->irq_handler[5] = smtc_process_touch_status; //FARANYIRQ
    self->irq_handler[6] = smtc_process_touch_status; //CLOSEANYIRQ
    self->irq_handler[7] = 0; /* RESET_STAT */
#ifdef SX933X
    self->irq_handler[2] = smtc_process_touch_status; //PROX2(table) and PROX3(body) irq
#endif


    ret = sysfs_create_group(&client->dev.kobj, &smtc_sysfs_nodes);
    if (ret)
    {
        SMTC_LOG_ERR("Failed to create sysfs node.");
        goto FREE_GPIO;
    }

    //.............................................................................................
    //Create input nodes for each main phase
    self->phases = smtc_phase_table;
    for (i=0; i<(sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); i++)
    {
        phase_p phase = &smtc_phase_table[i];
        SMTC_LOG_DBG("name=%s input=0x%p enabled=%d usage=%d state=%d",
            phase->name, phase->input, phase->enabled, phase->usage, phase->usage);

        //main phases were specified in the dts
        if ((self->main_phases & 1<<i) == 0)
        {
            if (self->ref_phases & 1<<i){
                phase->usage = REF;
            }
            continue;
        }

        input = input_allocate_device();
        if (!input)
        {
            SMTC_LOG_ERR("Failed to create input device %s.", phase->name);
            ret = -ENOMEM;
            goto FREE_INPUTS;
        }
        #ifdef SX937X_FACTORY_NODE
        input_set_capability(input, EV_KEY, phase->keycode_far);
        input_set_capability(input, EV_KEY, phase->keycode_close);
        #else
        input_set_capability(input, EV_REL, REL_MISC);
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 start */
        input_set_capability(input, EV_REL, REL_X);
        input_set_capability(input, EV_REL, REL_MAX);
        /* M55 code for SR-QN6887A-01-465 by zhangziyi at 20230926 end */
        #endif //SX937X_FACTORY_NODE
        input->name = phase->name;
        input->id.bustype = BUS_I2C;

        ret = input_register_device(input);
        if (ret)
        {
            SMTC_LOG_ERR("Failed to register input device %s. ret= %d", phase->name, ret);
            goto FREE_INPUTS;
        }

        phase->input = input;
        phase->enabled = false;
        phase->usage = MAIN;
        phase->state = IDLE;

        /* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
        #ifdef SX937X_USER_NODE
        if (i == 0) {
            continue;
        }
        input_set_drvdata(phase->input, self);
        self->channel_status = 0;

        if (!strcmp(phase->name, "grip_sensor")) {
            ret = sensors_register(&self->factory_device, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register main sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input main group");
                return -ENOMEM;
            }
        }

        if (!strcmp(phase->name, "grip_sensor_sub")) {
            ret = sensors_register(&self->factory_device_sub, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register sub sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input sub group");
                return -ENOMEM;
            }
        }

        if (!strcmp(phase->name, "grip_sensor_wifi")) {
            ret = sensors_register(&self->factory_device_wifi, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register wifi sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input wifi group");
                return -ENOMEM;
            }
        }
        #endif // SX937X_USER_NODE
        /* M55 code for QN6887A-96 by zhangziyi at 20230925 end */
    }

    //.............................................................................................
    /* M55 code for QN6887A-363|QN6887A-551 by zhangziyi at 20231102 start */
    ret = request_threaded_irq(self->irq_id, NULL, smtc_irq_handler,
                                IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                                client->dev.driver->name, self);
    /* M55 code for QN6887A-363|QN6887A-551 by zhangziyi at 20231102 end */
    if (ret)
    {
        SMTC_LOG_ERR("Failed to request irq= %d.", self->irq_id);
        goto FREE_INPUTS;
    }

    /* M55 code for SR-QN6887A-01-458 by lichang at 20230911 start */
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
    self->irq_wake_status = false;
    /* M55 code for QN6887A-96 by zhangziyi at 20230925 end */
    g_self = self;

    #ifdef SX937X_FACTORY_NODE
    g_facnode.sar_name_drv = "sx937x";
    g_facnode.get_cali = factory_cali;
    g_facnode.set_enable = factory_set_enable;
    g_facnode.get_enable = factory_get_enable;
    SMTC_LOG_INF("factory node create done.");
    #endif //SX937X_FACTORY_NODE

    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 start */
    g_usernode.set_onoff = smtc_set_onoff;
    g_usernode.get_onoff = smtc_get_onoff;
    g_usernode.set_dumpinfo = smtc_set_dumpinfo;
    g_usernode.get_dumpinfo = smtc_get_dumpinfo;
    g_usernode.get_cali = smtc_user_cali;
    SMTC_LOG_INF("user node create done.");
    /* M55 code for SR-QN6887A-01-460|SR-QN6887A-01-466 by lichang at 20230920 end */

    SMTC_LOG_DBG("registered irq= %d.", self->irq_id);

    SMTC_LOG_INF("Done.");
    return 0;

    //.............................................................................................
FREE_INPUTS:
    for (i=0; i<NUM_PHASES; i++)
    {
        if (!self->phases[i].input){
            input_unregister_device(self->phases[i].input);
        }
    }

    sysfs_remove_group(&client->dev.kobj, &smtc_sysfs_nodes);

FREE_GPIO:
    gpio_free(self->gpio);

    return ret;
}

//#################################################################################################
static int smtc_remove(struct i2c_client *client)
{
    int ret, i;
    Self self = i2c_get_clientdata(client);
    SMTC_LOG_INF("Remove driver.");

    //enter pause mode to save power
    ret = smtc_send_command(self, CMD_SLEEP);
    if (ret){
        SMTC_LOG_ERR("Failed to enter pause mode. ret= %d", ret);
    }
    smtc_read_and_clear_irq(self, NULL);
    free_irq(self->irq_id, self);

    for (i=0; i<NUM_PHASES; i++)
    {
        if (!self->phases[i].input){
            input_unregister_device(self->phases[i].input);
        }
    }
    sysfs_remove_group(&client->dev.kobj, &smtc_sysfs_nodes);
    gpio_free(self->gpio);

    return 0;
}

//=================================================================================================
/* M55 code for QN6887A-96 by zhangziyi at 20230925 start */
static int smtc_suspend(struct device *dev)
{
    //Self self = dev_get_drvdata(dev);
    //usually, SAR sensor should still be at work mode even the device is on suspend mode
    Self self = dev_get_drvdata(dev);

    if (g_Enabled == false) {
        smtc_read_and_clear_irq(self, NULL);
        smtc_send_command(self, CMD_SLEEP);
        SMTC_LOG_INF("disable smtc_suspend Do CMD_SLEEP.");
    } else {
        SMTC_LOG_INF("enable smtc_suspend Do nothing.");
    }

    return 0;
}

/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 start */
static int smtc_resume(struct device *dev)
{
    //Self self = dev_get_drvdata(dev);
    Self self = dev_get_drvdata(dev);

    if (g_Enabled == false) {
        smtc_send_command(self, CMD_WAKEUP);
        SMTC_LOG_INF("disable smtc_resume Do CMD_WAKEUP.");
    } else {
        SMTC_LOG_INF("enable smtc_resume Do nothing.");
    }

    return 0;

}
/* M55 code for QN6887A-96 by zhangziyi at 20230925 end */
/* M55 code for SR-QN6887A-01-458 by lichang at 20230911 end */

//#################################################################################################
static struct i2c_device_id smtc_idtable[] =
{
    { SMTC_DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, smtc_idtable);

static struct of_device_id smtc_match_table[] =
{
    { .compatible = SMTC_COMPATIBLE_NAME,},
    { },
};

static const struct dev_pm_ops smtc_pm_ops =
{
    .suspend = smtc_suspend,
    .resume = smtc_resume,
};
static struct i2c_driver smtc_driver =
{
    .driver = {
        .owner            = THIS_MODULE,
        .name            = SMTC_DRIVER_NAME,
        .of_match_table    = smtc_match_table,
        .pm                = &smtc_pm_ops,
    },

    .id_table   = smtc_idtable,
    .probe      = smtc_probe,
    .remove     = smtc_remove,
};

static int __init smtc_i2c_init(void)
{
    return i2c_add_driver(&smtc_driver);
}
static void __exit smtc_i2c_exit(void)
{
    i2c_del_driver(&smtc_driver);
}

module_init(smtc_i2c_init);
module_exit(smtc_i2c_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX93xx Capacitive SAR and Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2");

