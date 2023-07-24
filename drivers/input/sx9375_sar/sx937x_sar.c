/*! \file sx937x.c
 * \brief  sx937x Driver
 *
 * Driver for the sx937x
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
//#define DEBUG
#define DRIVER_NAME "sx937x"

#define MAX_WRITE_ARRAY_SIZE 32
#define POWER_ENABLE 1
#ifdef WT_COMPILE_FACTORY_VERSION
#define SAR_IN_FRANCE 0
#else
#define SAR_IN_FRANCE 1
#endif

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
#include <linux/version.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#include <linux/device.h>
#include "sx937x.h"   /* main struct, interrupt,init,pointers */

//20221221 wnn add lib start
#include <linux/sensors.h>
#include <linux/hardware_info.h>
//20221221 wnn add lib end

#define SX937x_I2C_M_WR                 0 /* for i2c Write */
#define SX937x_I2C_M_RD                 1 /* for i2c Read */

#define IDLE      0
#define PROX_ACTIVE  1
#define BODY_ACTIVE  2

#if SAR_IN_FRANCE
#define MAX_INT_COUNT 20
static int32_t enable_data[8] = {1, 1, 1};
#endif

/* Failer Index */
#define SX937x_ID_ERROR   1
#define SX937x_NIRQ_ERROR  2
#define SX937x_CONN_ERROR  3
#define SX937x_I2C_ERROR  4

#if defined(CONFIG_SENSORS)
#define MODEL_NAME       "SX9325"
#endif

/*! \struct sx937x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx937x
{
    pbuttonInformation_t pbuttonInformation;
    psx937x_platform_data_t hw;    /* specific platform data settings */
} sx937x_t, *psx937x_t;

static int irq_gpio_num;
psx93XX_t pSx9375Data;
/*
#if SAR_IN_FRANCE
static void touchProcess(psx93XX_t this);
#endif
*/
/*! \fn static int sx937x_i2c_write_16bit(psx93XX_t this, u8 address, u8 value)
 * \brief Sends a write register to the device
 * \param this Pointer to main parent struct
 * \param address 8-bit register address
 * \param value   8-bit register value to write to address
 * \return Value from i2c_master_send
 */

static int sx937x_i2c_write_16bit(psx93XX_t this, u16 reg_addr, u32 buf)
{
    int ret =  -ENOMEM;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg;
    unsigned char w_buf[6];

    if (this && this->bus)
    {
        i2c = this->bus;
        w_buf[0] = (u8)(reg_addr>>8);
        w_buf[1] = (u8)(reg_addr);
        w_buf[2] = (u8)(buf>>24);
        w_buf[3] = (u8)(buf>>16);
        w_buf[4] = (u8)(buf>>8);
        w_buf[5] = (u8)(buf);

        msg.addr = i2c->addr;
        msg.flags = SX937x_I2C_M_WR;
        msg.len = 6; //2bytes regaddr + 4bytes data
        msg.buf = (u8 *)w_buf;

        ret = i2c_transfer(i2c->adapter, &msg, 1);
        if (ret < 0)
            pr_err("[SX933X]: %s - i2c write error %d\n", __func__, ret);

    }
    return ret;
}


/*! \fn static int sx937x_i2c_read_16bit(psx93XX_t this, u8 address, u8 *value)
* \brief Reads a register's value from the device
* \param this Pointer to main parent struct
* \param address 8-Bit address to read from
* \param value Pointer to 8-bit value to save register value to
* \return Value from i2c_smbus_read_byte_data if < 0. else 0
*/
static int sx937x_i2c_read_16bit(psx93XX_t this, u16 reg_addr, u32 *data32)
{
    int ret =  -ENOMEM;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg[2];
    u8 w_buf[2];
    u8 buf[4];

    if (this && this->bus)
    {
        i2c = this->bus;

        w_buf[0] = (u8)(reg_addr>>8);
        w_buf[1] = (u8)(reg_addr);

        msg[0].addr = i2c->addr;
        msg[0].flags = SX937x_I2C_M_WR;
        msg[0].len = 2;
        msg[0].buf = (u8 *)w_buf;

        msg[1].addr = i2c->addr;;
        msg[1].flags = SX937x_I2C_M_RD;
        msg[1].len = 4;
        msg[1].buf = (u8 *)buf;

        ret = i2c_transfer(i2c->adapter, msg, 2);
        if (ret < 0)
            pr_err("[SX937x]: %s - i2c read error %d\n", __func__, ret);

        data32[0] = ((u32)buf[0]<<24) | ((u32)buf[1]<<16) | ((u32)buf[2]<<8) | ((u32)buf[3]);

    }
    return ret;
}


/*! \fn static int read_regStat(psx93XX_t this)
 * \brief Shortcut to read what caused interrupt.
 * \details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * \param this Pointer to main parent struct
 * \return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(psx93XX_t this)
{
    u32 data = 0;
    if (this)
    {
        if (sx937x_i2c_read_16bit(this,SX937X_IRQ_SOURCE,&data) > 0)
            return (data & 0x00FF);
    }
    return 0;
}

static int sx937x_Hardware_Check(psx93XX_t this)
{
    int ret = 0;
    u32 chip_id;
    u8 loop = 0;
    this->failStatusCode = 0;

    //Check th IRQ Status
    while(this->get_nirq_low && this->get_nirq_low())
    {
        read_regStat(this);
        msleep(100);
        if(++loop >10)
        {
            ret = -1;
            this->failStatusCode = SX937x_NIRQ_ERROR;
            pr_err("SX9375 check irq fail %s\n", __func__);
            break;
        }
    }

    //Check I2C Connection
    ret = sx937x_i2c_read_16bit(this, SX937X_DEVICE_INFO, &chip_id);
    if(ret < 0)
    {
        this->failStatusCode = SX937x_I2C_ERROR;
    }

    pr_info("[SX937x]: sx937x chip_id= 0x%X failcode= 0x%x\n", chip_id, this->failStatusCode);
    //return (int)this->failStatusCode;
    return ret;
}

#if 1
static int sx9375_chipId_check(psx93XX_t this)
{
    int ret = 0;
    u32 chip_id;

    ret = sx937x_i2c_read_16bit(this, SX937X_DEVICE_INFO, &chip_id);
    if (ret < 0) {
        if (0x937527 != chip_id)
            ret = -1;
        else
            ret = 0;
    }
    pr_info("[SX937x]: sx937x chip_id= 0x%X failcode= %d\n", chip_id, ret);
    return ret;
}
#endif

static int sx937x_global_variable_init(psx93XX_t this)
{
    this->irq_disabled = 0;
    this->failStatusCode = 0;
    this->reg_in_dts = true;
    return 0;
}

/*! \brief Perform a manual offset calibration
* \param this Pointer to main parent struct
* \return Value return value from the write register
 */
static int manual_offset_calibration(psx93XX_t this)
{
    int ret = 0;
    pr_info("[SX937x]: manual_offset_calibration\n");
    ret = sx937x_i2c_write_16bit(this, SX937X_COMMAND, 0xE);
    return ret;
}

/****************************************************************************/
/*! \brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u32 reg_value = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    pr_info("[SX937x]: Reading IRQSTAT_REG\n");
    sx937x_i2c_read_16bit(this, SX937X_IRQ_SOURCE, &reg_value);
    return sprintf(buf, "%d\n", reg_value);
}


static ssize_t manual_offset_calibration_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long val;
    psx93XX_t this = dev_get_drvdata(dev);

    if (kstrtoul(buf, 10, &val))                //(strict_strtoul(buf, 10, &val)) {
    {
        pr_err("[SX933X]: %s - Invalid Argument\n", __func__);
        return -EINVAL;
    }

    if (val)
        manual_offset_calibration(this);

    return count;
}

/****************************************************************************/
static ssize_t sx937x_register_write_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 reg_address = 0, val = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2)
    {
        pr_err("[SX937x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    sx937x_i2c_write_16bit(this, reg_address, val);
  pr_info("%s reg 0x%X= 0x%X\n", __func__, reg_address, val);

    return count;
}

//read registers not include the advanced one
static ssize_t sx937x_register_read_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 val=0;
    int regist = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    pr_info("Reading register\n");

    if (sscanf(buf, "%x", &regist) != 1)
    {
        pr_err("[SX937x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    sx937x_i2c_read_16bit(this, regist, &val);
    pr_info("%s reg 0x%X= 0x%X\n", __func__, regist, val);

    return count;
}

static ssize_t sx937x_enable(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 phases=0;
    u32 val;
    psx93XX_t this = dev_get_drvdata(dev);

    if (sscanf(buf, "%x", &phases) != 1)
    {
        pr_err("[SX937x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }
    pr_info("%s enable phases= 0x%X\n", __func__, phases);
    phases &= 0xFF;

    sx937x_i2c_read_16bit(this, SX937X_GENERAL_SETUP, &val);
    val &= ~0xFF;
    val |= phases;

    pr_info("%s set reg 0x8020= 0x%X\n",__func__, val);
    sx937x_i2c_write_16bit(this, SX937X_GENERAL_SETUP, val);
    manual_offset_calibration(this);

    return count;
}

static void read_dbg_raw(psx93XX_t this)
{
    int ph, state;
    u32 uData, ph_sel;
    s32 ant_use, ant_raw;
    s32 avg, diff;
    u16 off;
    s32 use_flt_dlt_var;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    int ref_ph_a, ref_ph_b, ref_ph_c;

    psx937x_t pDevice = NULL;
    psx937x_platform_data_t pdata = NULL;

    pDevice = this->pDevice;
    pdata = pDevice->hw;
    ref_ph_a = pdata->ref_phase_a;
    ref_ph_b = pdata->ref_phase_b;
    ref_ph_c = pdata->ref_phase_c;

    sx937x_i2c_read_16bit(this, SX937X_DEVICE_STATUS_A, &uData);
    pr_info("SX937X_STAT0_REG= 0x%X\n", uData);

    if(ref_ph_a != -1)
    {
        sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_a*4, &uData);
        ref_a_use = (s32)uData >> 10;
    }
    if(ref_ph_b != -1)
    {
        sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_b*4, &uData);
        ref_b_use = (s32)uData >> 10;
    }
    if(ref_ph_c != -1)
    {
        sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_c*4, &uData);
        ref_c_use = (s32)uData >> 10;
    }

    sx937x_i2c_read_16bit(this, SX937X_DEBUG_SETUP, &ph_sel);
    sx937x_i2c_read_16bit(this, SX937X_DEBUG_READBACK_2, &uData);
    ant_raw = (s32)uData>>10;
    sx937x_i2c_read_16bit(this, SX937X_DEBUG_READBACK_3, &uData);
    use_flt_dlt_var = (s32)uData>>4;

    ph = (ph_sel >> 3) & 0x7;

    sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ph*4, &uData);
    ant_use = (s32)uData>>10;

    sx937x_i2c_read_16bit(this, SX937X_AVERAGE_PH0 + ph*4, &uData);
    avg = (s32)uData>>10;
    sx937x_i2c_read_16bit(this, SX937X_DIFF_PH0 + ph*4, &uData);
    diff = (s32)uData>>10;
    sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0 + ph*4*3, &uData);
    off = (u16)(uData & 0x3FFF);
    state = psmtcButtons[ph].state;

    pr_info("SMTC_DBG PH= %d USE= %d RAW= %d PH%d_USE= %d PH%d_USE= %d PH%d_USE= %d STATE= %d AVG= %d DIFF= %d OFF= %d DLT= %d SMTC_END\n",
      ph, ant_use, ant_raw, ref_ph_a, ref_a_use, ref_ph_b, ref_b_use, ref_ph_c, ref_c_use, state, avg, diff, off, use_flt_dlt_var);
}


static void read_rawData(psx93XX_t this)
{
    u8 csx, index;
    s32 useful, average, diff;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    u32 uData;
    u32 use_hex, avg_hex, dif_hex, dlt_hex, dbg_ph;
    u16 offset;
    int state;
    psx937x_t pDevice = NULL;
    psx937x_platform_data_t pdata = NULL;
    int ref_ph_a, ref_ph_b, ref_ph_c;

    if(this)
    {
        pDevice = this->pDevice;
        pdata = pDevice->hw;
        ref_ph_a = pdata->ref_phase_a;
        ref_ph_b = pdata->ref_phase_b;
        ref_ph_c = pdata->ref_phase_c;
        pr_info("[SX937x] ref_ph_a= %d ref_ph_b= %d ref_ph_c= %d\n", ref_ph_a, ref_ph_b, ref_ph_c);

        sx937x_i2c_read_16bit(this, SX937X_DEVICE_STATUS_A, &uData);
        pr_info("SX937X_DEVICE_STATUS_A= 0x%X\n", uData);
        sx937x_i2c_read_16bit(this, SX937X_DEBUG_SETUP, &dbg_ph);
        dbg_ph = (dbg_ph >> 3) & 0x7;
        sx937x_i2c_read_16bit(this, SX937X_DEBUG_READBACK_3, &dlt_hex);

        if(ref_ph_a != -1)
        {
            sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_a*4, &uData);
            ref_a_use = (s32)uData >> 10;
        }
        if(ref_ph_b != -1)
        {
            sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_b*4, &uData);
            ref_b_use = (s32)uData >> 10;
        }
        if(ref_ph_c != -1)
        {
            sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + ref_ph_c*4, &uData);
            ref_c_use = (s32)uData >> 10;
        }

        for(csx =0; csx<8; csx++)
        {
            index = csx*4;
            sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + index, &use_hex);
            useful = (s32)use_hex>>10;
            sx937x_i2c_read_16bit(this, SX937X_AVERAGE_PH0 + index, &avg_hex);
            average = (s32)avg_hex>>10;
            sx937x_i2c_read_16bit(this, SX937X_DIFF_PH0 + index, &dif_hex);
            diff = (s32)dif_hex>>10;
            sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0 + index*3, &uData);
            offset = (u16)(uData & 0x3FFF);

            state = psmtcButtons[csx].state;
            pr_info(
            "SMTC_DAT PH= %d DIFF= %d USE= %d PH%d_USE= %d PH%d_USE= %d PH%d_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
            csx, diff, useful, ref_ph_a, ref_a_use, ref_ph_b, ref_b_use, ref_ph_c, ref_c_use, state, offset, average);

            pr_info(
            "SMTC_HEX PH= %d USE= 0x%X AVG= 0x%X DIF= 0x%X PH%d_DLT= 0x%X SMTC_END\n",
            csx, use_hex, avg_hex, dif_hex, dbg_ph, dlt_hex);
        }
        read_dbg_raw(this);
    }
}

static ssize_t sx937x_raw_data_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);
    read_rawData(this);
    return 0;
}

static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show, manual_offset_calibration_store);
static DEVICE_ATTR(register_write,  0664, NULL,sx937x_register_write_store);
static DEVICE_ATTR(register_read,0664, NULL,sx937x_register_read_store);
static DEVICE_ATTR(raw_data,0664, sx937x_raw_data_show,NULL);
static DEVICE_ATTR(sxenable, 0664, NULL,sx937x_enable);

static struct attribute *sx937x_attributes[] =
{
    &dev_attr_manual_calibrate.attr,
    &dev_attr_register_write.attr,
    &dev_attr_register_read.attr,
    &dev_attr_raw_data.attr,
    &dev_attr_sxenable.attr,
    NULL,
};
static struct attribute_group sx937x_attr_group =
{
    .attrs = sx937x_attributes,
};
#if SAR_IN_FRANCE
static void enable_get_offset(struct work_struct *work) //liuling
{
    size_t i = 0;
    u32 uData;
    psx93XX_t this = pSx9375Data;

    for (i = 0; i < 3; i++) {
        if (this->sar_first_boot && enable_data[i] == 2) {
            sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0 + i*4*3, &uData);
            if (0 == i) {
                this->ch0_backgrand_cap = (u16)(uData & 0x3FFF);
                pr_info("sx937x[store_enable]:ch0_backgrand_cap:%d\n", this->ch0_backgrand_cap);
                enable_data[i] = 0;
            } else if (1 == i) {
                this->ch1_backgrand_cap = (u16)(uData & 0x3FFF);
                pr_info("sx937x[store_enable]:ch1_backgrand_cap:%d\n", this->ch1_backgrand_cap);
                enable_data[i] = 0;
            } else if (2 == i) {
                this->ch2_backgrand_cap = (u16)(uData & 0x3FFF);
                pr_info("sx937x[store_enable]:ch2_backgrand_cap:%d\n", this->ch2_backgrand_cap);
                enable_data[i] = 0;
            }
        }
    }
    pr_info("sx937x[store_enable]:ch0_backgrand_cap:%d, ch1_backgrand_cap:%d, ch2_backgrand_cap:%d\n", this->ch0_backgrand_cap, this->ch1_backgrand_cap, this->ch2_backgrand_cap);
}
#endif

static ssize_t show_enable(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    size_t i = 0;
    int status = 0;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("%s: dev->name:%s\n", __func__, temp_input_dev->name);

    for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
        if (!strcmp(temp_input_dev->name, psmtcButtons[i].name)) {
            if (true == psmtcButtons[i].enabled)
                status = 1;
            break;
        }
    }
    p += snprintf(p, PAGE_SIZE, "%d", status);
    return 0;
}

static ssize_t store_enable(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    size_t i = 0;
    struct input_dev* temp_input_dev;
    u32 val = 0, pahse = 0, uData;
    bool enable = simple_strtol(buf, NULL, 10) ? 1 : 0;
    psx93XX_t this = pSx9375Data;
    temp_input_dev = container_of(dev, struct input_dev, dev);
    pr_info("%s: dev->name:%s:%s\n", __func__, temp_input_dev->name, buf);

    for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
        if (!strcmp(temp_input_dev->name, psmtcButtons[i].name)) {
            if (enable) {
                psmtcButtons[i].enabled = true;
                pahse = (0x01 << i) | (0x01 << i+3);
                sx937x_i2c_read_16bit(this, SX937X_GENERAL_SETUP, &val);
                pr_info("before psmtcButtons value is 0X%X\n", val);
                val &= 0xFF;
                val |= pahse;
                val |= (val << 8);
                sx937x_i2c_write_16bit(this, SX937X_GENERAL_SETUP, val);
                manual_offset_calibration(this);
                input_report_rel(psmtcButtons[i].sx9375_input_dev, REL_MISC, 2);
                input_report_rel(psmtcButtons[i].sx9375_input_dev, REL_X, 2);
                input_sync(psmtcButtons[i].sx9375_input_dev);
                pr_info("sx937x:ph%d: enable, now psmtcButtons value is 0X%X\n", i, val);
#if SAR_IN_FRANCE
                if (this->sar_first_boot && enable_data[i] == 1 && i <= 2) {
                    enable_data[i] = 2;
                    schedule_delayed_work(&this->delay_get_offset, msecs_to_jiffies(1200));
                }
#endif
            } else {
                psmtcButtons[i].enabled = false;
                pahse = ~(0x01 << i) & ~(0x01 << i+3);
                sx937x_i2c_read_16bit(this, SX937X_GENERAL_SETUP, &val);
                pr_info("before psmtcButtons value is 0X%X\n", val);
                val &= 0xFF;
                val &= pahse;
                val |= (val << 8);
                sx937x_i2c_write_16bit(this, SX937X_GENERAL_SETUP, val);
                manual_offset_calibration(this);
                pr_info("sx937x:ph%d: disable, now psmtcButtons value is 0X%X\n", i, val);
            }
        }
    }
    return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);

static ssize_t cap_diff_dump_show(psx93XX_t this, int ch, char *buf)
{
    u8 index = ch *4;
    u32 use_hex, ref_use_hex, /*avg_hex,*/ uData, dif_hex;
    s32 /*useful, ref_useful,*/ diff;
//  s32 average;
    u16 offset; 
    ssize_t len = 0;

    int channel_nu = ch ? ch + 1 : ch;

#if 0
    sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + index, &use_hex);
    useful = (s32)use_hex>>10;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_useful=%d;", channel_nu, useful);
    sx937x_i2c_read_16bit(this, SX937X_USEFUL_PH0 + index + USEFUL_TAB * 3, &ref_use_hex);
    ref_useful = (s32)ref_use_hex>>10;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_ref_useful=%d;", channel_nu, ref_useful);
#endif

#if 0
    sx937x_i2c_read_16bit(this, SX937X_AVERAGE_PH0 + index, &avg_hex);
    average = (s32)avg_hex>>10;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_background_cap=%d;", channel_nu, average);
#endif

    sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0 + index * 3, &uData);
    offset = (u16)(uData & 0x3FFF);
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_background_cap=%d;", channel_nu, offset);

    sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0 + index * 3 + OFFSET_TAB * 3, &uData);
    offset = (u16)(uData & 0x3FFF);
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_refer_channel_cap=%d;", channel_nu, offset);

    sx937x_i2c_read_16bit(this, SX937X_DIFF_PH0 + index, &dif_hex);
    diff = (s32)dif_hex>>10; 
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d", channel_nu, diff);

    return len;
}
static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9375Data;
    return cap_diff_dump_show(this, 0, buf); 
}
static ssize_t ch2_cap_diff_dump_show(struct class *dev,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9375Data;
    return cap_diff_dump_show(this, 1, buf); 
}
static ssize_t ch3_cap_diff_dump_show(struct class *dev,
                                      struct class_attribute *attr,
                                      char *buf)
{
    psx93XX_t this = pSx9375Data;
    return cap_diff_dump_show(this, 2, buf); 
}

#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    psx93XX_t this = pSx9375Data;
    return sprintf(buf, "%d\n", this->power_state);
}

static ssize_t power_enable_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;
    psx93XX_t this = pSx9375Data;

    ret = kstrtoint(buf, 10, &this->power_state);
    if (0 != ret) {
        pr_err("kstrtoint failed\n");
    }
    return count;
}
#endif

#if SAR_IN_FRANCE
static ssize_t user_test_show(struct class *class,
                                struct class_attribute *attr,
                                char *buf)
{
    psx93XX_t this = pSx9375Data;
    return sprintf(buf, "%d\n", this->user_test);
}

static ssize_t user_test_store(struct class *class,
                                struct class_attribute *attr,
                                const char *buf, size_t count)
{
    int ret = -1;
    psx93XX_t this = pSx9375Data;

    ret = kstrtoint(buf, 10, &this->user_test);
    if (0 != ret) {
        pr_err("kstrtoint failed\n");
    }
    pr_info("this->user_test:%d\n", this->user_test);
    if (this->user_test) {
        this->sar_first_boot = false;
        pr_info("this->sar_first_boot:%d\n", this->sar_first_boot);
        pr_info("sx9375:user_test mode1, exit force input near mode!!!\n");
        manual_offset_calibration(this);
        pr_info("sx9375 user_test mode cali ... \n");
    }
    return count;
}
#endif

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0664, ch2_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch3_cap_diff_dump = __ATTR(ch3_cap_diff_dump, 0664, ch3_cap_diff_dump_show, NULL);
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
#if SAR_IN_FRANCE
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, user_test_show, user_test_store);
#endif


static struct attribute *sx9375_capsense_attrs[] = {
    &class_attr_ch0_cap_diff_dump.attr,
    &class_attr_ch2_cap_diff_dump.attr,
    &class_attr_ch3_cap_diff_dump.attr,
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
#if SAR_IN_FRANCE
    &class_attr_user_test.attr,
#endif
    NULL,
};
ATTRIBUTE_GROUPS(sx9375_capsense);

static struct class capsense_class = {
    .name           = "capsense",
    .owner          = THIS_MODULE,
    .class_groups   = sx9375_capsense_groups,
};

/**************************************/

#if defined(CONFIG_SENSORS)
static int sx937x_grip_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    int i = 0;

    for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
        if (!strcmp(sensors_cdev->name, psmtcButtons[i].name)) {
            input_report_rel(psmtcButtons[i].sx9375_input_dev, REL_MAX, val);
            input_sync(psmtcButtons[i].sx9375_input_dev);
        }
    }

    pr_info("%s val=%u\n", __func__, val);
    return 0;
}
#endif

/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct
 */
static void sx937x_reg_init(psx93XX_t this)
{
    psx937x_t pDevice = 0;
    psx937x_platform_data_t pdata = 0;
    int i = 0;

    /* configure device */
    pr_info("[SX937x]:Going to Setup I2C Registers\n");
    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {
        if (this->reg_in_dts == true)
        {
            i = 0;
            while ( i < pdata->i2c_reg_num)
            {
                /* Write all registers/values contained in i2c_reg */
                pr_info("[SX937x]: Going to Write Reg from dts: addr= 0x%x value= 0x%x\n",
                         pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
                sx937x_i2c_write_16bit(this, pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
                i++;
            }
        }
        else     // use static ones!!
        {
            while ( i < ARRAY_SIZE(sx937x_i2c_reg_setup))
            {
                /* Write all registers/values contained in i2c_reg */
                pr_info("[SX937x]: Going to Write Reg from dts: addr= 0x%x value= 0x%x\n",
                         sx937x_i2c_reg_setup[i].reg,sx937x_i2c_reg_setup[i].val);
                sx937x_i2c_write_16bit(this, sx937x_i2c_reg_setup[i].reg,sx937x_i2c_reg_setup[i].val);
                i++;
            }
        }
        /*******************************************************************************/
        sx937x_i2c_write_16bit(this, SX937X_COMMAND, 0xF);  //activate sensor
    }
    else
    {
        pr_err("[SX937x]: ERROR! platform data 0x%p\n",pDevice->hw);
    }

}


/*! \fn static int initialize(psx93XX_t this)
 * \brief Performs all initialization needed to configure the device
 * \param this Pointer to main parent struct
 * \return Last used command's return value (negative if error)
 */
static int initialize(psx93XX_t this)
{

    int ret;
    if (this)
    {
        pr_info("[SX937x]: SX937x income initialize\n");
        /* prepare reset by disabling any irq handling */
        this->irq_disabled = 1;
        disable_irq(this->irq);
        /* perform a reset */
        sx937x_i2c_write_16bit(this, SX937X_DEVICE_RESET, 0xDE);
        /* wait until the reset has finished by monitoring NIRQ */
        pr_info("Sent Software Reset. Waiting until device is back from reset to continue.\n");
        /* just sleep for awhile instead of using a loop with reading irq status */
        msleep(100);
        ret = sx937x_global_variable_init(this);
        sx937x_reg_init(this);
        msleep(100); /* make sure everything is running */
        manual_offset_calibration(this);

        /* re-enable interrupt handling */
        enable_irq(this->irq);

        /* make sure no interrupts are pending since enabling irq will only
        * work on next falling edge */
        read_regStat(this);
        return 0;
    }
    return -ENOMEM;
}

/*!
 * \brief Handle what to do when a touch occurs
 * \param this Pointer to main parent struct
 */
static void touchProcess(psx93XX_t this)
{
    int counter = 0;
    u32 prox_stat = 0;
    u32 touchFlag = 0;
    int numberOfButtons = 0;
    psx937x_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *input = NULL;

    struct _buttonInfo *pCurrentButton  = NULL;
#if SAR_IN_FRANCE
    u32 uData;
    u16 ch0_result = 0;
    u16 ch1_result = 0;
    u16 ch2_result = 0;
#endif

    if (this && (pDevice = this->pDevice))
    {
        sx937x_i2c_read_16bit(this, SX937X_DEVICE_STATUS_A, &prox_stat);
        pr_info("touchProcess STAT0_REG= 0x%X\n", prox_stat);

        buttons = pDevice->pbuttonInformation->buttons;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely( buttons==NULL ))
        {
            pr_err(":ERROR!! buttons NULL!!!\n");
            return;
        }

#if SAR_IN_FRANCE
        if (this->interrupt_count <= MAX_INT_COUNT) {
            this->interrupt_count++;
            pr_info("sx937x interrupt_count is %d;", this->interrupt_count);
        }

        if (this->sar_first_boot) {
            sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH0, &uData);
            ch0_result = (u16)(uData & 0x3FFF);
            sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH1, &uData);
            ch1_result = (u16)(uData & 0x3FFF);
            sx937x_i2c_read_16bit(this, SX937X_OFFSET_PH2, &uData);
            ch2_result = (u16)(uData & 0x3FFF);
        }
        pr_info("sx937x: this->sar_first_boot=%d\n",this->sar_first_boot);
        pr_info("sx937x: ch0_result: %d, ch1_result: %d, ch2_result: %d\n", ch0_result, ch1_result, ch2_result);
        pr_info("sx937x: this->sar_first_boot=%d\n",this->sar_first_boot);
#endif

        for (counter = 0; counter < numberOfButtons; counter++)
        {
            if (buttons[counter].used == false) {
                pr_info("touchProcess %s unused, ignor this\n", buttons[counter].name);
                continue;
            }
            if (buttons[counter].enabled == false) {
                pr_info("touchProcess %s disabled, ignor this\n", buttons[counter].name);
                continue;
            }
            pCurrentButton = &buttons[counter];
            if (pCurrentButton==NULL)
            {
                pr_info("ERROR!! current button at index: %d NULL!!!\n", counter);
                return; // ERRORR!!!!
            }
            //20221221 wnn modify get input dev method start
            input = pCurrentButton->sx9375_input_dev;
            //20221221 wnn modify get input dev method end
            if (input==NULL)
            {
                pr_err("ERROR!! current button input at index: %d NULL!!!\n", counter);
                return; // ERRORR!!!!
            }
#if SAR_IN_FRANCE
            if (this->sar_first_boot) {
                if ((this->interrupt_count < MAX_INT_COUNT) && ((ch0_result - this->ch0_backgrand_cap) >= -5.5) && \
                    ((ch1_result - this->ch1_backgrand_cap) >= -1.5) && ((ch2_result - this->ch2_backgrand_cap) >= -1)) {
                    input_report_rel(input, REL_MISC, 1);
                    input_report_rel(input, REL_X, 1);
                    input_sync(input);
                    pCurrentButton->state = PROX_ACTIVE;
                    pr_info("sx9375: force %s State=Near\n", pCurrentButton->name);
                    continue;
                }
                this->sar_first_boot = false;
                sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH0, 0x60200078);
                sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH1, 0x60200078);
                sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH2, 0x60200078);
                pr_info("sx9375: exit force input near mode!!!\n");
            }
#endif
            touchFlag = prox_stat & (pCurrentButton->ProxMask | pCurrentButton->BodyMask);
#if POWER_ENABLE
            if (this->power_state) {
                if (pCurrentButton->state == IDLE)
                    pr_info("%s already released.\n", pCurrentButton->name);
                else{
                    input_report_rel(input, REL_MISC, 2);
                    input_report_rel(input, REL_X, 2);
                    pCurrentButton->state = IDLE;
                    input_sync(input);
                    pr_info("%s report  released.\n", pCurrentButton->name);
                } 
            }else{
#endif
                if (touchFlag == (pCurrentButton->ProxMask | pCurrentButton->BodyMask)) {
#if SAR_IN_FRANCE
                    if (pCurrentButton->state == BODY_ACTIVE && this->anfr_export_exit == false)
#else
                    if (pCurrentButton->state == BODY_ACTIVE)
#endif
                        pr_info(" %s already BODYACTIVE\n", pCurrentButton->name);
                    else {
                        input_report_rel(input, REL_MISC, 1);
                        input_report_rel(input, REL_X, 2);
                        pCurrentButton->state = BODY_ACTIVE;
                        pr_info(" %s report 5mm BODYACTIVE\n", pCurrentButton->name);
                    }
                } else if (touchFlag == pCurrentButton->ProxMask) {
#if SAR_IN_FRANCE
                    if (pCurrentButton->state == PROX_ACTIVE && this->anfr_export_exit == false)
#else
                    if (pCurrentButton->state == PROX_ACTIVE)
#endif
                        pr_info(" %s already PROX_ACTIVE\n", pCurrentButton->name);
                    else {
                        input_report_rel(input, REL_MISC, 1);
                        input_report_rel(input, REL_X, 2);
                        pCurrentButton->state = PROX_ACTIVE;
                        pr_info(" %s report 15mm PROX_ACTIVE\n", pCurrentButton->name);
                    }
                } else if (touchFlag == 0) {
#if SAR_IN_FRANCE
                    if (pCurrentButton->state == IDLE && this->anfr_export_exit == false)
#else
                    if (pCurrentButton->state == IDLE)
#endif
                        pr_info("%s already released.\n", pCurrentButton->name);
                    else {
                        input_report_rel(input, REL_MISC, 2);                        
                        input_report_rel(input, REL_X, 2);
                        pCurrentButton->state = IDLE;
                        pr_info("%s report  released.\n", pCurrentButton->name);
                    }
                }
                input_sync(input);
#if POWER_ENABLE
            }
#endif            
        }
        pr_info("Leaving touchProcess()\n");
    }
}

#if SAR_IN_FRANCE
void exit_anfr_sx9375(void )
{
    psx93XX_t this = pSx9375Data;

    if (this->sar_first_boot) {
        this->sar_first_boot = false;
        this->anfr_export_exit = true;
        pr_info("this->sar_first_boot:%d\n", this->sar_first_boot);
        pr_info("sx9375:exit force input near mode, and report sar once!!\n");
        touchProcess(this);
        this->anfr_export_exit = false;
    }
}
EXPORT_SYMBOL(exit_anfr_sx9375);
#endif


static int sx937x_parse_dt(struct sx937x_platform_data *pdata, struct device *dev)
{
    struct device_node *dNode = dev->of_node;
    enum of_gpio_flags flags;

    if (dNode == NULL)
        return -ENODEV;
    if ( of_property_read_u32(dNode,"Semtech,button-flag",&pdata->button_used_flag)) {
        pr_err("[SX937x]: %s - get button-flag error\n", __func__);
        return -ENODEV;
    }
	pr_info("Semtech,button-flag: %d\n", pdata->button_used_flag);
    pdata->irq_gpio= of_get_named_gpio_flags(dNode, "Semtech,nirq-gpio", 0, &flags);
    irq_gpio_num = pdata->irq_gpio;
    if (pdata->irq_gpio < 0)
    {
        pr_err("[SX937x]: %s - get irq_gpio error\n", __func__);
        return -ENODEV;
    }

    pdata->ref_phase_a = -1;
    pdata->ref_phase_b = -1;
    pdata->ref_phase_c = -1;
    if ( of_property_read_u32(dNode,"Semtech,ref-phases-a",&pdata->ref_phase_a) )
    {
        pr_err("[SX937x]: %s - get ref-phases error\n", __func__);
        return -ENODEV;
    }
    if ( of_property_read_u32(dNode,"Semtech,ref-phases-b",&pdata->ref_phase_b) )
    {
        pr_err("[SX937x]: %s - get ref-phases-b error\n", __func__);
        return -ENODEV;
    }
    if ( of_property_read_u32(dNode,"Semtech,ref-phases-c",&pdata->ref_phase_c) )
    {
        pr_err("[SX937x]: %s - get ref-phases-c error\n", __func__);
        return -ENODEV;
    }
    if (pdata->ref_phase_a == 0xFF)
        pdata->ref_phase_a = -1;
    if (pdata->ref_phase_b == 0xFF)
        pdata->ref_phase_b = -1;
    if (pdata->ref_phase_c == 0xFF)
        pdata->ref_phase_c = -1;
    pr_info("[SX937x]: %s ref_phase_a= %d ref_phase_b= %d ref_phase_c= %d\n",
    __func__, pdata->ref_phase_a, pdata->ref_phase_b, pdata->ref_phase_b);

    /***********************************************************************/
    // load in registers from device tree
    of_property_read_u32(dNode,"Semtech,reg-num",&pdata->i2c_reg_num);
    // layout is register, value, register, value....
    // if an extra item is after just ignore it. reading the array in will cause it to fail anyway
    pr_info("[sx937x]:%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
    if (pdata->i2c_reg_num > 0)
    {
        // initialize platform reg data array
        pdata->pi2c_reg = devm_kzalloc(dev,sizeof(struct smtc_reg_data)*pdata->i2c_reg_num, GFP_KERNEL);
        if (unlikely(pdata->pi2c_reg == NULL))
        {
            return -ENOMEM;
        }

        // initialize the array
        if (of_property_read_u32_array(dNode,"Semtech,reg-init",
                    (u32*)&(pdata->pi2c_reg[0]),
                    (sizeof(struct smtc_reg_data)*pdata->i2c_reg_num)/sizeof(u32)))
            return -ENOMEM;
    }
    /***********************************************************************/

    pr_info("[SX937x]: %s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
    return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx937x_init_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx937x *pDevice = NULL;
    struct sx937x_platform_data *pdata = NULL;
    int rc = 0;

    pr_info("[SX937x] : %s init_platform_hw start!",__func__);

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {
        if (gpio_is_valid(pdata->irq_gpio))
        {
            rc = gpio_request(pdata->irq_gpio, "sx937x_irq_gpio");
            if (rc < 0)
            {
                pr_err("SX937x Request gpio. Fail![%d]\n", rc);
                return rc;
            }
            rc = gpio_direction_input(pdata->irq_gpio);
            if (rc < 0)
            {
                pr_err("SX937x Set gpio direction. Fail![%d]\n", rc);
                return rc;
            }
            this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
        }
        else
        {
            pr_err("SX937x Invalid irq gpio num.(init)\n");
        }
    }
    else
    {
        pr_err("[SX937x] : %s - Do not init platform HW", __func__);
    }

    pr_err("[SX937x]: %s - sx937x_irq_debug\n",__func__);
    return rc;
}

static void sx937x_exit_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx937x *pDevice = NULL;
    struct sx937x_platform_data *pdata = NULL;

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {
        if (gpio_is_valid(pdata->irq_gpio))
        {
            gpio_free(pdata->irq_gpio);
        }
        else
        {
            pr_err("Invalid irq gpio num.(exit)\n");
        }
    }
    return;
}

static int sx937x_get_nirq_state(void)
{
    return !gpio_get_value(irq_gpio_num);
}

/*! \fn static int sx937x_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */
static int sx937x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0;

    psx93XX_t this = 0;
    psx937x_t pDevice = 0;
    psx937x_platform_data_t pplatData = 0;
    struct totalButtonInformation *pButtonInformationData = NULL;
    //struct input_dev *input = NULL;
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

    pr_info("[SX937x]:sx937x_probe() drv_ver= 01_phase_enable_node\n");

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
    {
        pr_err("[SX937x]:Check i2c functionality.Fail!\n");
        err = -EIO;
        return err;
    }

    this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
    pr_info( "[SX937x]:\t Initialized Main Memory: 0x%p\n",this);
#if 1
    this->bus = client;
    err = sx9375_chipId_check(this);
    if (err < 0) {
        pr_err("SX9375 id_check failed\n");
        goto failed_id_check;
    }
#endif

    pButtonInformationData = devm_kzalloc(&client->dev , sizeof(struct totalButtonInformation), GFP_KERNEL);
    if (!pButtonInformationData)
    {
        pr_err("[SX937x]:Failed to allocate memory(totalButtonInformation)\n");
        err = -ENOMEM;
        return err;
    }

    pButtonInformationData->buttonSize = ARRAY_SIZE(psmtcButtons);
    pButtonInformationData->buttons =  psmtcButtons;
    pplatData = devm_kzalloc(&client->dev,sizeof(struct sx937x_platform_data), GFP_KERNEL);
    if (!pplatData)
    {
        pr_err("[SX937x]:platform data is required!\n");
        return -EINVAL;
    }
    pplatData->get_is_nirq_low = sx937x_get_nirq_state;
    pplatData->pbuttonInformation = pButtonInformationData;

    client->dev.platform_data = pplatData;
    err = sx937x_parse_dt(pplatData, &client->dev);
    if (err)
    {
        pr_err("[SX937x]:could not setup pin\n");
        return ENODEV;
    }

    pplatData->init_platform_hw = sx937x_init_platform_hw;
    pr_err("[SX937x]:SX937x init_platform_hw done!\n");

    if (this)
    {
        pr_info( "[SX937x]:SX937x initialize start!!");
        /* In case we need to reinitialize data
        * (e.q. if suspend reset device) */
        this->init = initialize;
        /* shortcut to read status of interrupt */
        this->refreshStatus = read_regStat;
        /* pointer to function from platform data to get pendown
        * (1->NIRQ=0, 0->NIRQ=1) */
        this->get_nirq_low = pplatData->get_is_nirq_low;
        /* save irq in case we need to reference it */
        this->irq = client->irq;
        /* do we need to create an irq timer after interrupt ? */
        this->useIrqTimer = 0;

        /* Setup function to call on corresponding reg irq source bit */
        if (MAX_NUM_STATUS_BITS>= 8)
        {
            this->statusFunc[0] = 0; /* TXEN_STAT */
            this->statusFunc[1] = 0; /* UNUSED */
            this->statusFunc[2] = 0; /* UNUSED */
            this->statusFunc[3] = read_rawData; /* CONV_STAT */
            this->statusFunc[4] = touchProcess; /* COMP_STAT */
            this->statusFunc[5] = touchProcess; /* RELEASE_STAT */
            this->statusFunc[6] = touchProcess; /* TOUCH_STAT  */
            this->statusFunc[7] = 0; /* RESET_STAT */
        }

        /* setup i2c communication */
        this->bus = client;
        i2c_set_clientdata(client, this);

        /* record device struct */
        this->pdev = &client->dev;

        /* create memory for device specific struct */
        this->pDevice = pDevice = devm_kzalloc(&client->dev, sizeof(sx937x_t), GFP_KERNEL);
        pr_info( "[SX937x]:\t Initialized Device Specific Memory: 0x%p\n", pDevice);
        pSx9375Data = this;

        if (pDevice)
        {
            /* for accessing items in user data (e.g. calibrate) */
            err = sysfs_create_group(&client->dev.kobj, &sx937x_attr_group);
            //sysfs_create_group(client, &sx937x_attr_group);

            /* Add Pointer to main platform data struct */
            pDevice->hw = pplatData;

            /* Check if we hava a platform initialization function to call*/
            if (pplatData->init_platform_hw)
                pplatData->init_platform_hw(client);

            /* Initialize the button information initialized with keycodes */
            pDevice->pbuttonInformation = pplatData->pbuttonInformation;
            for (size_t i = 0; i < pButtonInformationData->buttonSize; i++) {
				if ((pplatData->button_used_flag >> i) & 0x01) {
					pButtonInformationData->buttons[i].state = IDLE;

					pButtonInformationData->buttons[i].sx9375_input_dev = input_allocate_device();
					if (!pButtonInformationData->buttons[i].sx9375_input_dev) {
						pr_err("input_allocate_device failed\n");
                        return -1;
                        //goto failed_allocate_input_dev;
					}
					pButtonInformationData->buttons[i].sx9375_input_dev->name = pButtonInformationData->buttons[i].name;
					input_set_capability(pButtonInformationData->buttons[i].sx9375_input_dev, EV_REL, REL_MISC);
					input_set_capability(pButtonInformationData->buttons[i].sx9375_input_dev, EV_REL, REL_MAX);
					__set_bit(EV_REL, pButtonInformationData->buttons[i].sx9375_input_dev->evbit);
					input_set_capability(pButtonInformationData->buttons[i].sx9375_input_dev, EV_REL, REL_X);

					err = input_register_device(pButtonInformationData->buttons[i].sx9375_input_dev);

					if (i <= 2) {
						if (0 != device_create_file(&pButtonInformationData->buttons[i].sx9375_input_dev->dev, &dev_attr_enable)) {
							pr_info("%s attribute ENABLE create fail\n", pButtonInformationData->buttons[i].sx9375_input_dev->name);
                            return -1;
                            //goto failed_create_enable_file;
						}
					}

					input_report_rel(pButtonInformationData->buttons[i].sx9375_input_dev, REL_MISC, 2);
					input_sync(pButtonInformationData->buttons[i].sx9375_input_dev);

					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.sensors_enable = NULL;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.sensors_poll_delay = NULL;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.sensor_name = pButtonInformationData->buttons[i].name;
#if defined(CONFIG_SENSORS)
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.name = MODEL_NAME;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.sensors_flush = sx937x_grip_flush;
#endif
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.vendor = "semtech";
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.version = 1;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.type = pButtonInformationData->buttons[i].type;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.max_range = "5";
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.resolution = "5.0";
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.sensor_power = "3";
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.min_delay = 0;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.fifo_reserved_event_count = 0;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.fifo_max_event_count = 0;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.delay_msec = 100;
					pButtonInformationData->buttons[i].sx9375_sensors_class_cdev.enabled = 0;
					pButtonInformationData->buttons[i].enabled = false;
                    pButtonInformationData->buttons[i].used = true;

					err = sensors_classdev_register(&pButtonInformationData->buttons[i].sx9375_input_dev->dev, &pButtonInformationData->buttons[i].sx9375_sensors_class_cdev);
                    if (err < 0) {
                        pr_info("create %d cap sensor_class  file failed (%d)\n", i, err);
                        return -1;
                    }
				}
            }
        }

        sx93XX_IRQ_init(this);
        /* call init function pointer (this should initialize all registers */
        if (this->init) {
            this->init(this);
        } else {
            pr_err("[SX937x]:No init function!!!!\n");
            return -ENOMEM;
        }
	} else {
        return -1;
    }

    err = sx937x_Hardware_Check(this);
    if (err < 0) {
        pr_info("check device id fail\n");
        return err;
    }
    pplatData->exit_platform_hw = sx937x_exit_platform_hw;
    
    class_register(&capsense_class);

#if POWER_ENABLE
    this->power_state = 0;
#endif
#if SAR_IN_FRANCE
    this->sar_first_boot = true;
    this->user_test  = 0;
    sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH0, 0x46200030);
    sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH1, 0x42200030);
    sx937x_i2c_write_16bit(this, SX937X_FILTER_SETUP_B_PH2, 0xC1200030);
    INIT_DELAYED_WORK(&this->delay_get_offset, enable_get_offset);
#endif
    pr_info("[SX937x]:sx937x_probe() Done\n");
    hardwareinfo_set_prop(HARDWARE_SAR, "sx9375");

    return 0;

#if 1
failed_id_check:
return -1;
#endif
}

/*! \fn static int sx937x_remove(struct i2c_client *client)
 * \brief Called when device is to be removed
 * \param client Pointer to i2c_client struct
 * \return Value from sx93XX_remove()
 */
static int sx937x_remove(struct i2c_client *client)
{
    size_t index = 0;
    psx937x_platform_data_t pplatData =0;
    psx937x_t pDevice = 0;
    struct _buttonInfo *pCurrentbutton;
    psx93XX_t this = i2c_get_clientdata(client);

    if (this && (pDevice = this->pDevice))
    {
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw(client);
        
        cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
        cancel_delayed_work_sync(&this->delay_get_offset);
        for (index = 0; index < pDevice->pbuttonInformation->buttonSize; index++) {
            pCurrentbutton = &(pDevice->pbuttonInformation->buttons[index]);
            if (pCurrentbutton->used == true) {
                sensors_classdev_unregister(&(pCurrentbutton->sx9375_sensors_class_cdev));
                input_unregister_device(pCurrentbutton->sx9375_input_dev);
            }
        }
        free_irq(this->irq, this);
        kfree(this);
        return 0;
    }
#if 0
//20221221 wnn add need modify
        input_unregister_device(pDevice->pbuttonInformation->input);

        sysfs_remove_group(&client->dev.kobj, &sx937x_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw(client);
        kfree(this->pDevice);

        cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
        /*destroy_workqueue(this->workq); */
        free_irq(this->irq, this);
        kfree(this);
        return 0;
    }
//20221221 wnn add need modify
#endif
    return -ENOMEM;
}
static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
    unsigned long flags;
    if (this)
    {
        pr_info("sx93XX_schedule_work()\n");
        spin_lock_irqsave(&this->lock,flags);
        /* Stop any pending penup queues */
        cancel_delayed_work(&this->dworker);
        //after waiting for a delay, this put the job in the kernel-global workqueue. so no need to create new thread in work queue.
        schedule_delayed_work(&this->dworker,delay);
        spin_unlock_irqrestore(&this->lock,flags);
    }
    else
        printk(KERN_ERR "sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
    psx93XX_t this = 0;
//    pr_info("sx9375 interrupt\n");
    if (pvoid)
    {
        this = (psx93XX_t)pvoid;
        if ((!this->get_nirq_low) || this->get_nirq_low())
        {
            sx93XX_schedule_work(this,0);
        }
        else
        {
            pr_err("sx93XX_irq - nirq read high\n");
        }
    }
    else
    {
        printk(KERN_ERR "sx93XX_irq, NULL pvoid\n");
    }
    return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
    psx93XX_t this = 0;
    int status = 0;
    int counter = 0;
    u8 nirqLow = 0;
    if (work)
    {
        this = container_of(work,sx93XX_t,dworker.work);

        if (!this)
        {
            printk(KERN_ERR "sx93XX_worker_func, NULL sx93XX_t\n");
            return;
        }
        if (unlikely(this->useIrqTimer))
        {
            if ((!this->get_nirq_low) || this->get_nirq_low())
            {
                nirqLow = 1;
            }
        }
        /* since we are not in an interrupt don't need to disable irq. */
        status = (int)this->refreshStatus(this);
        pr_info("sx93XX_worker_func: status is 0x%x\n", status);
#if SAR_IN_FRANCE
        if ((status>>4) & 0x01)
            this->interrupt_count--;
#endif
        counter = -1;
        //dev_dbg( "Worker - Refresh Status %d\n",status);

        while((++counter) < MAX_NUM_STATUS_BITS)   /* counter start from MSB */
        {
            if (((status>>counter) & 0x01) && (this->statusFunc[counter]))
            {
                pr_info("SX937x Function Pointer Found. Calling\n");
                this->statusFunc[counter](this);
            }
        }
        if (unlikely(this->useIrqTimer && nirqLow))
        {
            /* Early models and if RATE=0 for newer models require a penup timer */
            /* Queue up the function again for checking on penup */
            sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
        }
    }
    else
    {
        printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
    }
}

void sx93XX_suspend(psx93XX_t this)
{
    if (this)
        disable_irq(this->irq);
}
void sx93XX_resume(psx93XX_t this)
{
    if (this)
    {
        enable_irq(this->irq);
    }
}

int sx93XX_IRQ_init(psx93XX_t this)
{
    int err = 0;
    if (this && this->pDevice)
    {
        /* initialize spin lock */
        spin_lock_init(&this->lock);
        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);
        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
                          this->pdev->driver->name, this);
        if (err)
        {
            pr_err("irq %d busy?\n", this->irq);
            return err;
        }
        pr_info("registered with irq (%d)\n", this->irq);
    }
    return -ENOMEM;
}

#if 1//def CONFIG_PM
/*====================================================*/
/***** Kernel Suspend *****/
static int sx937x_suspend(struct device *dev)
{
    psx93XX_t this = dev_get_drvdata(dev);
    sx93XX_suspend(this);
    return 0;
}
/***** Kernel Resume *****/
static int sx937x_resume(struct device *dev)
{
    psx93XX_t this = dev_get_drvdata(dev);
    sx93XX_resume(this);
    return 0;
}
/*====================================================*/
#else
#define sx937x_suspend    NULL
#define sx937x_resume    NULL
#endif /* CONFIG_PM */

static struct i2c_device_id sx937x_idtable[] =
{
    { DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sx937x_idtable);

static struct of_device_id sx937x_match_table[] =
{
    { .compatible = "Semtech,sx937x",},
    { },
};

static const struct dev_pm_ops sx937x_pm_ops =
{
    .suspend = sx937x_suspend,
    .resume = sx937x_resume,
};
static struct i2c_driver sx937x_driver =
{
    .driver = {
        .owner      = THIS_MODULE,
        .name      = DRIVER_NAME,
        .of_match_table  = sx937x_match_table,
        .pm        = &sx937x_pm_ops,
    },

    .id_table   = sx937x_idtable,
    .probe      = sx937x_probe,
    .remove     = sx937x_remove,
};

static int __init sx937x_I2C_init(void)
{
    printk("sx9375 init\n");
    return i2c_add_driver(&sx937x_driver);
}
static void __exit sx937x_I2C_exit(void)
{
    i2c_del_driver(&sx937x_driver);
}

rootfs_initcall(sx937x_I2C_init);
module_exit(sx937x_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX937x Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
