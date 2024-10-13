/*! \file sx93xx_sar.c
 * \brief  Semtech CAP Sensor Driver for SAR
 *
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#define POWER_ENABLE    1
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#define USER_TEST       1

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

#include "sx933x.h"

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */ 
#include <linux/sensors.h>
#include <linux/hardware_info.h>
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */

#define I2C_M_WR_SMTC   0 /* for i2c Write */
#define I2C_M_RD_SMTC   1 /* for i2c Read */

#define NUM_IRQ_BITS 8

#define NUM_RETRY_ON_I2C_ERR    2
#define SLEEP_BETWEEN_RETRY     100 //in ms

/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
#define SAR_STATE_NEAR                  1
#define SAR_STATE_FAR                   2
/* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
#define SAR_STATE_NONE                  -1

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

    bool enabled;
    PHASE_USAGE usage;
    PROX_STATUS state;
    struct sensors_classdev sx9338_sensors_class_cdev;
    int type;
}phase_t, *phase_p;

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

    //phases are for temperature reference correction
    int ref_phase_a;
	int ref_phase_b;
    int ref_phase_c;

    spinlock_t lock; /* Spin Lock used for nirq worker function */
    struct delayed_work worker; /* work struct for worker function */


    //Function that will be called back when corresponding IRQ is raised
    //Refer to register 0x4000
    void (*irq_handler[NUM_IRQ_BITS])(struct self_s* self);

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
    int power_state;
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch

}self_t, *Self;

Self pSx9338Data;

#define SMTC_LOG_DBG(fmt, args...)   pr_debug ("[DBG] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_INF(fmt, args...)   pr_info ("[INF] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)
#define SMTC_LOG_ERR(fmt, args...)   pr_err  ("[ERR] " "smtc.%s(%d): "fmt"\n", __func__, __LINE__, ##args)

static void smtc_log_raw_data(Self self);

//#################################################################################################
int _log_hex_data = 0;
int _log_dbg_data = 0;
u16 _reading_reg = 0xFFFF;

//The value of all other variables not configured in the table are 0.
#if defined(SX933X)
static phase_t smtc_phase_table[] =
{
	{
        .name = "grip_sensor",
		.prox1_mask = 1 << 24,
		.prox2_mask = 1 << 16,
		.prox3_mask = 1 << 8,
		.enabled = false,
		.type = 65560
	},
	{
		.name = "grip_sensor_sub",
		.prox1_mask = 1 << 25,
		.prox2_mask = 1 << 17,
		.prox3_mask = 1 << 9,
		.enabled = false,
		.type = 65636
	},
	{
		.name = "grip_sensor_wifi",
		.prox1_mask = 1 << 26,
		.prox2_mask = 1 << 18,
		.prox3_mask = 1 << 10,
		.enabled = false,
		.type = 65575
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
	{
        .name = "SMTC SAR Sensor CH0",
		.prox1_mask = 1 << 24,
		.prox2_mask = 1 << 16,
		.prox3_mask = 1 << 8,
		.prox4_mask = 1 << 0
	},
	{
		.name = "SMTC SAR Sensor CH1",
		.prox1_mask = 1 << 25,
		.prox2_mask = 1 << 17,
		.prox3_mask = 1 << 9,
		.prox4_mask = 1 << 1
	},
	{
		.name = "SMTC SAR Sensor CH2",
		.prox1_mask = 1 << 26,
		.prox2_mask = 1 << 18,
		.prox3_mask = 1 << 10,
		.prox4_mask = 1 << 2
	},
	{
		.name = "SMTC SAR Sensor CH3",
		.prox1_mask = 1 << 27,
		.prox2_mask = 1 << 19,
		.prox3_mask = 1 << 11,
		.prox4_mask = 1 << 3
	},
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
		.prox1_mask = 1 << 31,
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
static int smtc_send_command(Self self, COMMANDS cmd)
{
    int retry_times=40, state, ret;
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
            return -EBUSY;
        }
        msleep(50);
        SMTC_LOG_DBG("Chip is busy, go to retry.");
    }

    ret = smtc_i2c_write(self, REG_CMD, (u32)cmd);
    if (ret < 0){
        SMTC_LOG_ERR("Failed to send command.");
        return -EIO;
    }

    return 0;
}

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
 
        count = sprintf(buf+count, "PH%d= %d", offset);
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


//=================================================================================================
/*Each bit of the buf represents whether its correspoding phase is being enabled(1) or disabled(0)
for example: echo 0x05 > enable buf=0x5=0b0101, means PHASE 2 and 0 are being enabled,
and all other phases are being disabled
*/
static ssize_t smtc_enable_phases_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    u32 phases=0;
	u32 reg_val;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "0x%x", &phases) != 1)
    {
        SMTC_LOG_ERR("Invalid command format. Usage: ehco 0xXX > enable");
        return -EINVAL;
    }
	SMTC_LOG_INF("phases= 0x%X", phases);
    if (phases & ~((1<<NUM_PHASES)-1))
    {
        SMTC_LOG_ERR("Invalid phases=0x%X, max phases supported=%d", phases, NUM_PHASES);
        return -EINVAL;
    }

    smtc_i2c_read(self, REG_PHEN, &reg_val);
	reg_val &= ~0xFF;
	reg_val |= phases;
	smtc_i2c_write(self, REG_PHEN, reg_val);

#ifdef SX933X
    //sx937x will compensate the enabling phase automatically
    SMTC_LOG_INF("Calibrating...");
    smtc_calibrate(self);
#endif

    return count;
}

//-------------------------------------------------------------------------------------------------
static ssize_t smtc_enable_phases_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u32 reg_val;
    int ph=0, ret=0, bytes=0;
    Self self = dev_get_drvdata(dev);

    ret = smtc_i2c_read(self, REG_PHEN, &reg_val);
    if (ret < 0){
        bytes = sprintf(buf, "%s", "Failed to read reg=0x%X\n", REG_PHEN);
        SMTC_LOG_ERR("%s", buf);
        return bytes;
    }

    bytes = sprintf(buf, "%s", "Current enabled phases: ");
    if (reg_val & 0xFF)
    {
        for (ph=0; ph<NUM_PHASES; ph++)
        {
            if (reg_val & 1<<ph){
                bytes += sprintf(buf+bytes, "%d ", ph);
            }
        }
    }else{
        bytes += sprintf(buf+bytes, "%s", "none");
    }

    bytes += sprintf(buf+bytes, "%s", "\n");
    return bytes;
}

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

//=================================================================================================
static DEVICE_ATTR(calibrate,           0664, smtc_calibration_show,    smtc_calibration_store);
static DEVICE_ATTR(register_write,      0664, smtc_reg_write_show,      smtc_reg_write_store);
static DEVICE_ATTR(register_read,       0664, smtc_reg_read_show,       smtc_reg_read_store);
static DEVICE_ATTR(raw_data,            0664, smtc_raw_data_show,       NULL);
static DEVICE_ATTR(sxenable,            0664, smtc_enable_phases_show,  smtc_enable_phases_store);
static DEVICE_ATTR(debug,               0664, smtc_debug_show,          smtc_debug_store);
static DEVICE_ATTR(dump_reg,            0664, smtc_dump_reg_show,       NULL);


static struct attribute *smtc_sysfs_nodes_attr[] =
{
    &dev_attr_calibrate.attr,
    &dev_attr_register_write.attr,
    &dev_attr_register_read.attr,
    &dev_attr_raw_data.attr,
    &dev_attr_sxenable.attr,
    &dev_attr_debug.attr,
    &dev_attr_dump_reg.attr,

    NULL,
};
static struct attribute_group smtc_sysfs_nodes =
{
    .attrs = smtc_sysfs_nodes_attr,
};

//#################################################################################################
static void smtc_get_ph_prox_state(u32 prox_reg_val, PROX_STATUS ph_state[NUM_PHASES])
{
    int ph;
    phase_p phase;

	for (ph=0; ph<NUM_PHASES; ph++)
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

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
static ssize_t show_enable(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    size_t i = 0;
    int status = 0;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("smtc:%s: dev->name:%s\n", __func__, temp_input_dev->name);
    for (i = 0; i < ARRAY_SIZE(smtc_phase_table); i++) {
        if (!strcmp(temp_input_dev->name, smtc_phase_table[i].name)) {
            if (true == smtc_phase_table[i].enabled)
                status = 1;
            break;
        }
    }
    p += snprintf(p, PAGE_SIZE, "%d", status);
    return 1;
}

static ssize_t store_enable(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    size_t i = 0;
    u32 val = 0, pahse = 0;
    Self this = pSx9338Data;
    struct input_dev* temp_input_dev;
    bool enable = simple_strtol(buf, NULL, 10) ? 1 : 0;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("smtc:%s: dev->name:%s:%s\n", __func__, temp_input_dev->name, buf); 

    for (i = 0; i < ARRAY_SIZE(smtc_phase_table); i++) {
        if (!strcmp(temp_input_dev->name, smtc_phase_table[i].name)) {
            if (enable) {
                smtc_phase_table[i].enabled = true;
                if (i == 0)
                    pahse = (0x01 << i) | (0x01 << i+3);
                else if (i == 1)
                    pahse = (0x01 << i);
                else if (i == 2)
                    pahse = (0x01 << i) | (0x01 << i+2);
                smtc_i2c_read(this, REG_PHEN, &val);
                pr_info("smtc:before psmtcButtons value is 0X%X\n", val);
	            val &= 0xFF;
    	        val |= pahse;
    	        smtc_i2c_write(this, REG_PHEN, val);
                smtc_calibrate(this);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(smtc_phase_table[i].input, REL_MISC, SAR_STATE_FAR);
                input_report_rel(smtc_phase_table[i].input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_sync(smtc_phase_table[i].input);
                pr_info("smtc:sx9338:ph%d: enable, now psmtcButtons value is 0X%X\n", i, val);
            } else {
                smtc_phase_table[i].enabled = false;
                if (i == 0)
                    pahse = ~(0x01 << i) & ~(0x01 << i+3);
                else if (i == 1)
                    pahse = ~(0x01 << i);
                else if (i == 2)
                    pahse = ~(0x01 << i) & ~(0x01 << i+2);
                smtc_i2c_read(this, REG_PHEN, &val);
                pr_info("smtc:before psmtcButtons value is 0X%X\n", val);
	            val &= 0xFF;
    	        val &= pahse;
    	        smtc_i2c_write(this, REG_PHEN, val);
                smtc_calibrate(this);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(smtc_phase_table[i].input, REL_MISC, SAR_STATE_NONE);
                input_report_rel(smtc_phase_table[i].input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_sync(smtc_phase_table[i].input);
                pr_info("smtc:sx9338:ph%d: disable, now psmtcButtons value is 0X%X\n", i, val);
            }
        }
    }
    return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/30, wt factory app test need */
static ssize_t cap_diff_dump_show(Self self, int ch, char *buf)
{
    ssize_t len = 0;
    u32 reg_val;
    u16 offset;
    s32 diff;

    int shift = ch*4;

    smtc_i2c_read(self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT,  &reg_val);
    offset = (u16)(reg_val & OFFSET_VAL_MASK);

    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_background_cap=%d;", ch, offset);

    smtc_i2c_read(self, REG_PH0_DIFF + shift, &reg_val);
    diff = (s32)reg_val>>10;

    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d", ch, diff);

    return len;
}
static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(pSx9338Data, 0, buf);
}
static ssize_t ch1_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(pSx9338Data, 1, buf);
}
static ssize_t ch2_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(pSx9338Data, 2, buf);
}
static ssize_t ch3_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(pSx9338Data, 3, buf);
}
static ssize_t ch4_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(pSx9338Data, 4, buf);
}

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    return sprintf(buf, "%d\n", pSx9338Data->power_state);
}
static ssize_t power_enable_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;

    ret = kstrtoint(buf, 10, &pSx9338Data->power_state);
    if (0 != ret) {
        SMTC_LOG_ERR("kstrtoint failed\n");
    }
    return count;
}
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch

#if USER_TEST
static ssize_t user_test_store(struct class *class,
                               struct class_attribute *attr,
                               const char *buf, size_t count)
{
    int ret;
    int val = 0;
    Self this = pSx9338Data;

    ret = kstrtoint(buf, 10, &val);
    if (0 != ret) {
        SMTC_LOG_ERR("kstrtoint failed\n");
    }

    SMTC_LOG_INF("smtc:sx9338 user_test val = %d\n", val);
    if (val) {
        smtc_calibrate(this);
    }
    return count;
}
#endif

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch1_cap_diff_dump = __ATTR(ch1_cap_diff_dump, 0664, ch1_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0664, ch2_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch3_cap_diff_dump = __ATTR(ch3_cap_diff_dump, 0664, ch3_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch4_cap_diff_dump = __ATTR(ch4_cap_diff_dump, 0664, ch4_cap_diff_dump_show, NULL);
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if USER_TEST
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, NULL, user_test_store);
#endif

static struct attribute *sx9338_capsense_attrs[] = {
    &class_attr_ch0_cap_diff_dump.attr,
    &class_attr_ch1_cap_diff_dump.attr,
    &class_attr_ch2_cap_diff_dump.attr,
    &class_attr_ch3_cap_diff_dump.attr,
    &class_attr_ch4_cap_diff_dump.attr,
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/30, add sar power reduction control switch
#if USER_TEST
    &class_attr_user_test.attr,
#endif
    NULL,
};
ATTRIBUTE_GROUPS(sx9338_capsense);
static struct class capsense_class = {
    .name           = "capsense",
    .owner          = THIS_MODULE,
    .class_groups   = sx9338_capsense_groups,
};
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/30, wt factory app test need */

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

    if (of_property_read_u32(dNode, "Semtech5,main-phases", &self->main_phases) )
	{
		SMTC_LOG_ERR("Failed to get main-phases");
        return -EINVAL;
	}
    SMTC_LOG_DBG("main-phases= 0x%X", self->main_phases);
    printk("main-phases= 0x%X", self->main_phases);

    //.............................................................................................
    //load reference phase setup
	if (of_property_read_u32(dNode, "Semtech5,ref-phases", &self->ref_phases) )
	{
		SMTC_LOG_ERR("Failed to get ref-phases");
        return -EINVAL;
	}
    SMTC_LOG_DBG("ref-phases= 0x%X", self->ref_phases);
    printk("ref-phases= 0x%X", self->ref_phases);

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
    printk("ref_phase_a= %d ref_phase_b= %d ref_phase_c= %d",self->ref_phase_a, self->ref_phase_b, self->ref_phase_c);

    //.............................................................................................
    //load register settings
    of_property_read_u32(dNode, "Semtech5,reg-num", &self->num_regs);
    SMTC_LOG_DBG("number of registers= %d", self->num_regs);
    printk("number of registers= %d", self->num_regs);

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

        if (of_property_read_u32_array(dNode, "Semtech5,reg-init",
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
    printk("Get irq= %d from gpio= %d", self->irq_id, self->gpio);

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
static void smtc_process_touch_status(Self self)
{
    int ph;
    u32 prox_state = 0;
    bool need_sync = false;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < NUM_PHASES; ph++)
    {
        phase = &smtc_phase_table[ph];        
        if (phase->usage != MAIN)
            continue;
        if (!phase->enabled)
			continue;

        input = phase->input;
        need_sync = false;

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
        if(pSx9338Data->power_state){
            if (phase->state == IDLE)
                SMTC_LOG_INF("[power_enable]:%s already released, nothing report\n", phase->name);
            else {
                phase->state = IDLE;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, SAR_STATE_FAR);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_sync(input);
                SMTC_LOG_INF("[power_enable]:%s report released\n", phase->name);
            }
            continue;
        }
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
    	if (prox_state & phase->prox4_mask)
    	{
            if (phase->state == PROX4){
                SMTC_LOG_INF("%s is SAR_STATE_NONE already", phase->name);
            }
            else{
                SMTC_LOG_INF("%s reports SAR_STATE_NONE", phase->name);
                phase->state = PROX4;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, (int)SAR_STATE_NONE);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                need_sync = true;
            }
    	}
#if 0
        else if (prox_state & phase->prox3_mask)
    	{
            if (phase->state == PROX3){
                SMTC_LOG_INF("%s is PROX3 already", phase->name);
            }
            else{
                SMTC_LOG_INF("%s reports PROX3", phase->name);
                phase->state = PROX3;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, (int)PROX3);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                need_sync = true;
            }
    	}
        else if (prox_state & phase->prox2_mask)
    	{
            if (phase->state == PROX2){
                SMTC_LOG_INF("%s is PROX2 already", phase->name);
            }
            else{
                SMTC_LOG_INF("%s reports PROX2", phase->name);
                phase->state = PROX2;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, (int)PROX2);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                need_sync = true;
            }
    	}
#endif
        else if (prox_state & phase->prox1_mask)
    	{
            if (phase->state == PROX1){
                SMTC_LOG_INF("%s is SAR_STATE_NEAR already", phase->name);
            }
            else{
                SMTC_LOG_INF("%s reports SAR_STATE_NEAR", phase->name);
                phase->state = PROX1;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, SAR_STATE_NEAR);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                need_sync = true;
            }
    	}
        else{
            if (phase->state == IDLE){                
                SMTC_LOG_INF("%s is SAR_STATE_FAR already", phase->name);
            }
            else{
                SMTC_LOG_INF("%s reports SAR_STATE_FAR", phase->name);
                phase->state = IDLE;
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                input_report_rel(input, REL_MISC, SAR_STATE_FAR);
                input_report_rel(input, REL_X, 2);
                /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
                need_sync = true;
            }
        }

        if (need_sync){
            input_sync(input);
        }
    }
}

//#################################################################################################
static void smtc_worker_func(struct work_struct *work)
{
    int ret, irq_bit, irq_src;
    Self self = 0;
    SMTC_LOG_DBG("Enter");

    self = container_of(work, self_t, worker.work);
    ret = smtc_read_and_clear_irq(self, &irq_src);
    if (ret){
        SMTC_LOG_ERR("Failed to read irq source. ret=%d.", ret);
        return;
    }

    for (irq_bit=0; irq_bit<NUM_IRQ_BITS; irq_bit++)
    {
        if (irq_src >> irq_bit & 0x1)
        {
            if(self->irq_handler[irq_bit])
            {
                //call to smtc_process_touch_status() or smtc_log_raw_data()
                self->irq_handler[irq_bit](self);
            }else{
                SMTC_LOG_ERR("No handler to IRQ bit= %d", irq_bit);
            }
        }
    }    
}


//=================================================================================================
//call flow: smtc_irq_handler--> smtc_worker_func--> smtc_process_touch_status
static irqreturn_t smtc_irq_handler(int irq, void *pvoid)
{
    //unsigned long flags;
    Self self = (Self)pvoid;
    SMTC_LOG_INF("IRQ= %d is received", irq);

    if (likely(smtc_is_irq_low(self)))
    {
        //spin_lock_irqsave(&self->lock, flags);
        cancel_delayed_work(&self->worker);
        schedule_delayed_work(&self->worker, 0);
        //spin_unlock_irqrestore(&self->lock, flags);
    }else{
        SMTC_LOG_ERR("GPIO=%d must be low when an IRQ is received.", self->gpio);
    }
    
    return IRQ_HANDLED;
}

/* +S96818AA1-3871, liuling3.wt,ADD, 2023/05/30, distinguish sar driver for Australia */
static bool sar_get_boardid(void)
{
	char board_id[64];
	char *br_ptr;
	char *br_ptr_e;
	bool board_is_la = false;

	memset(board_id, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.board_id=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get board id */
		if (br_ptr_e != 0) {
			strncpy(board_id, br_ptr + 21,
					br_ptr_e - br_ptr - 21);
			board_id[br_ptr_e - br_ptr - 21] = '\0';
		}

		printk("smtc:sx9338  board_id = %s ", board_id);
        /* if it is LA board */
	    if ((!strncmp(board_id, "S96818CA1",strlen("S96818CA1")))) {
		     board_is_la = true;
		}
	} else
        board_is_la = false;
	return board_is_la;

}
/* -S96818AA1-3871, liuling3.wt,ADD, 2023/05/30, distinguish sar driver for Australia */

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
static int sx9338_flush_classdev(struct sensors_classdev *sensors_cdev, unsigned char flush)
{
    int ii = 0;

    for (ii = 0; ii < ARRAY_SIZE(smtc_phase_table); ii++) {
        if (sensors_cdev->type == smtc_phase_table[ii].type) {
            input_report_rel(smtc_phase_table[ii].input, REL_MAX, flush);
            input_sync(smtc_phase_table[ii].input);
            break;
        }
    }
    return 0;
}
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/

//#################################################################################################
static int smtc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{    
    int ret = 0, i;
    Self self = NULL;
    struct input_dev *input = NULL;
    struct i2c_adapter *adapter = NULL;
    SMTC_LOG_INF("Start to initialize Semtech SAR sensor %s driver.", SMTC_DRIVER_NAME);

    client->addr = 0x28;
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

    pSx9338Data = self;

    ret = sysfs_create_group(&client->dev.kobj, &smtc_sysfs_nodes);
    if (ret)
    {
        SMTC_LOG_ERR("Failed to create sysfs node.");
        goto FREE_GPIO;
    }
    
    //.............................................................................................
    //Create input nodes for each main phase
    self->phases = smtc_phase_table;
    for (i=0; i<NUM_PHASES; i++)
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

        input->name = phase->name;
        input->id.bustype = BUS_I2C;
        
        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
        __set_bit(EV_REL,input->evbit); 
        input_set_capability(input, EV_REL, REL_MISC);
        __set_bit(EV_REL, input->evbit);
        input_set_capability(input, EV_REL, REL_X);
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        input_set_capability(input, EV_REL, REL_MAX);
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */

        ret = input_register_device(input);
        if (ret)
        {
            SMTC_LOG_ERR("Failed to register input device %s. ret= %d", phase->name, ret);
            goto FREE_INPUTS;
        }
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
#if 1
        if (0 != device_create_file(&input->dev, &dev_attr_enable)) {
            SMTC_LOG_ERR("%s attribute ENABLE create fail\n", input->name);
            return -1;
        }
#endif
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */

        phase->sx9338_sensors_class_cdev.sensors_enable = NULL;
        phase->sx9338_sensors_class_cdev.sensors_poll_delay = NULL;
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
		phase->sx9338_sensors_class_cdev.sensors_flush = sx9338_flush_classdev;
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        phase->sx9338_sensors_class_cdev.sensor_name = phase->name;
        phase->sx9338_sensors_class_cdev.name = "SX9325";
        phase->sx9338_sensors_class_cdev.vendor = "semtech";
        phase->sx9338_sensors_class_cdev.version = 1;
        phase->sx9338_sensors_class_cdev.type = phase->type;
        phase->sx9338_sensors_class_cdev.max_range = "5";
        phase->sx9338_sensors_class_cdev.resolution = "5.0";
        phase->sx9338_sensors_class_cdev.sensor_power = "3";
        phase->sx9338_sensors_class_cdev.min_delay = 0;
        phase->sx9338_sensors_class_cdev.fifo_reserved_event_count = 0;
        phase->sx9338_sensors_class_cdev.fifo_max_event_count = 0;
        phase->sx9338_sensors_class_cdev.delay_msec = 100;
        phase->sx9338_sensors_class_cdev.enabled = 0; 

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
        ret = sensors_classdev_register(&input->dev, &phase->sx9338_sensors_class_cdev);

        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
        phase->sx9338_sensors_class_cdev.sensor_name = "SX9325";
        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */

        if (ret < 0) {
            SMTC_LOG_ERR("create %d cap sensor_class  file failed (%d)\n", i, ret);
            return -1;
        }
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/26, wt Adaptive sensor hal */
 
        phase->input = input;
        phase->enabled = false;
        phase->usage = MAIN;
        phase->state = IDLE;

        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
        input_report_rel(phase->input, REL_MISC, SAR_STATE_NONE);
        input_report_rel(phase->input, REL_X, 2);
        /* +S96818AA1-6209, duxin2.wt,ADD, 2023/06/09, wt Adaptive sensor new hal */
        input_sync(phase->input);
    }

    //.............................................................................................
    spin_lock_init(&self->lock);
    INIT_DELAYED_WORK(&self->worker, smtc_worker_func);
    ret = request_irq(self->irq_id, smtc_irq_handler, IRQF_TRIGGER_FALLING,
                      client->dev.driver->name, self);
    if (ret)
    {
        SMTC_LOG_ERR("Failed to request irq= %d.", self->irq_id);
        goto FREE_INPUTS;
    }
    SMTC_LOG_DBG("registered irq= %d.", self->irq_id);

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/05/29, wt Adaptive sensor hal */
    class_register(&capsense_class);
    if (ret < 0) {
        SMTC_LOG_ERR("register capsense class failed (%d)\n", &capsense_class);
        return ret;
    }
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/05/29, wt Adaptive sensor hal */

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    pSx9338Data->power_state = 0;
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch

    hardwareinfo_set_prop(HARDWARE_SAR, "sx9338_sar");

    SMTC_LOG_INF("sx9338 probe Done.");
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
    cancel_delayed_work_sync(&self->worker);

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
static int smtc_suspend(struct device *dev)
{
    //Self self = dev_get_drvdata(dev);
    //usually, SAR sensor should still be at work mode even the device is on suspend mode
    SMTC_LOG_INF("Do nothing to smtc SAR sensor.");
    return 0;
}

static int smtc_resume(struct device *dev)
{
    //Self self = dev_get_drvdata(dev);    
    return 0;
}

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
        .owner			= THIS_MODULE,
        .name			= SMTC_DRIVER_NAME,
        .of_match_table	= smtc_match_table,
        .pm				= &smtc_pm_ops,
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

