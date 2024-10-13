/*! \file sx933x.c
 * \brief  sx933x Driver
 *
 * Driver for the sx933x
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
//#define DEBUG
#define DRIVER_NAME "sx933x"

#define MAX_WRITE_ARRAY_SIZE 32

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
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
#include "../sensor_user_node.h"
/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/
#if defined(CONFIG_SENSORS)
#include <linux/sensor/sensors_core.h>
#endif

#include "sx933x.h"    /* main struct, interrupt,init,pointers */
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 start*/
#include <mt-plat/v1/mtk_charger.h>

#ifdef SX933X_USB_CALI
#include <linux/power_supply.h>
#endif
#ifdef SX933X_FACTORY_NODE
#include "../sensor_fac_node.h"
#endif

#if defined(CONFIG_SENSORS)
#define VENDOR_NAME      "SEMTECH"
#define MODEL_NAME       "SX9325"
#define MODULE_NAME      "grip_sensor"
#define SUB_MODULE_NAME  "grip_sensor_sub"
#endif

#define USB_PLUG_IN         1
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 end*/
#define SX933x_I2C_M_WR                 0 /* for i2c Write */
#define SX933x_I2C_M_RD                 1 /* for i2c Read */

#define IDLE                0
#define ACTIVE              1

#define MAIN_SENSOR         1 //CS1

/* Failer Index */
#define SX933x_ID_ERROR      1
#define SX933x_NIRQ_ERROR    2
#define SX933x_CONN_ERROR    3
#define SX933x_I2C_ERROR     4

/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
static bool onoff_mEnabled = true;
struct device *onoff_dev;
/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

/*! \struct sx933x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx933x
{
    pbuttonInformation_t pbuttonInformation;
    psx933x_platform_data_t hw;        /* specific platform data settings */
} sx933x_t, *psx933x_t;
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
/*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 start*/
static void touchProcess(psx93XX_t this);
bool enable_save = false;
static int irq_gpio_num;
#ifndef HQ_FACTORY_BUILD
static int irq_sign = 0;
static int cali_sign = 0;
static int anfr_sign = 1;
#define IRQ_NUM  24
#endif
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/
/*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 end*/

#ifdef SX933X_USB_CALI
static struct workqueue_struct* sx_sar_work;
static struct work_struct sx_sar_enable_work;
static struct notifier_block sx_sar_notify;
#endif

#if defined(SX933X_USB_CALI) || defined(SX933X_FACTORY_NODE)
static psx93XX_t sar_this = NULL;
#endif

#if defined(CONFIG_SENSORS)
static bool mEnabled = 1;
#endif

/*! \fn static int sx933x_i2c_write_16bit(psx93XX_t this, u8 address, u8 value)
 * \brief Sends a write register to the device
 * \param this Pointer to main parent struct
 * \param address 8-bit register address
 * \param value   8-bit register value to write to address
 * \return Value from i2c_master_send
 */

static int sx933x_i2c_write_16bit(psx93XX_t this, u16 reg_addr, u32 buf)
{
    int ret = -ENOMEM;
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
        msg.flags = SX933x_I2C_M_WR;
        msg.len = 6; //2bytes regaddr + 4bytes data
        msg.buf = (u8 *)w_buf;

        ret = i2c_transfer(i2c->adapter, &msg, 1);
        if (ret < 0)
            pr_err("[SX933X]: %s - i2c write error %d\n", __func__, ret);

    }
    return ret;
}

/*! \fn static int sx933x_i2c_read_16bit(psx93XX_t this, u8 address, u8 *value)
* \brief Reads a register's value from the device
* \param this Pointer to main parent struct
* \param address 8-Bit address to read from
* \param value Pointer to 8-bit value to save register value to
* \return Value from i2c_smbus_read_byte_data if < 0. else 0
*/
static int sx933x_i2c_read_16bit(psx93XX_t this, u16 reg_addr, u32 *data32)
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
        msg[0].flags = SX933x_I2C_M_WR;
        msg[0].len = 2;
        msg[0].buf = (u8 *)w_buf;

        msg[1].addr = i2c->addr;;
        msg[1].flags = SX933x_I2C_M_RD;
        msg[1].len = 4;
        msg[1].buf = (u8 *)buf;

        ret = i2c_transfer(i2c->adapter, msg, 2);
        if (ret < 0)
            pr_err("[SX933x]: %s - i2c read error %d\n", __func__, ret);

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
    if (this) {
        if (sx933x_i2c_read_16bit(this,SX933X_HOSTIRQSRC_REG,&data) > 0)
            return (data & 0x00FF);
    }
    return 0;
}

static int sx933x_Hardware_Check(psx93XX_t this)
{
    int ret;
    u32 failcode;
    u8 loop = 0;
    this->failStatusCode = 0;

    //Check th IRQ Status
    while (this->get_nirq_low && this->get_nirq_low()) {
        read_regStat(this);
        msleep(100);
        if (++loop > 10) {
            this->failStatusCode = SX933x_NIRQ_ERROR;
            break;
        }
    }

    //Check I2C Connection
    ret = sx933x_i2c_read_16bit(this, SX933X_INFO_REG, &failcode);
    if (ret < 0) {
        this->failStatusCode = SX933x_I2C_ERROR;
    }

    dev_info(this->pdev, "[SX933x]: sx933x failcode = 0x%x\n",this->failStatusCode);
    return (int)this->failStatusCode;
}

static int sx933x_global_variable_init(psx93XX_t this)
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
    dev_info(this->pdev, "[SX933x]: manual_offset_calibration\n");
    ret = sx933x_i2c_write_16bit(this, SX933X_CMD_REG, I2C_REGCMD_COMPEN);
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

    dev_info(this->pdev, "[SX933x]: Reading IRQSTAT_REG\n");
    sx933x_i2c_read_16bit(this,SX933X_HOSTIRQSRC_REG,&reg_value);
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
static ssize_t sx933x_register_write_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 reg_address = 0, val = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2)
    {
        pr_err("[SX933x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    sx933x_i2c_write_16bit(this, reg_address, val);
    pr_info("%s reg 0x%X= 0x%X\n", __func__, reg_address, val);

    return count;
}

//read registers not include the advanced one
static ssize_t sx933x_register_read_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 val=0;
    int regist = 0;
    psx93XX_t this = dev_get_drvdata(dev);

    dev_info(this->pdev, "Reading register\n");

    if (sscanf(buf, "%x", &regist) != 1)
    {
        pr_err("[SX933x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    sx933x_i2c_read_16bit(this, regist, &val);
    pr_info("%s reg 0x%X= 0x%X\n", __func__, regist, val);

    return count;
}

/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 start*/
#ifndef HQ_FACTORY_BUILD
static ssize_t sx933x_receiver_turnon_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 val = 0;
    int receiver_status = 0;
    int grip_sensor_sub_status = 0;
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor_sub = NULL;

    psx93XX_t this = dev_get_drvdata(dev);

    /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
    if (onoff_mEnabled == false) {
        return count;
    }
    /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

    if (this && (pDevice = this->pDevice)) {
        dev_info(this->pdev, "receiver_turnon\n");

        if (sscanf(buf, "%x", &receiver_status) != 1) {
            pr_err("[SX933x]: %s - The number of data are wrong\n", __func__);
            return -EINVAL;
        }

        grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
        if ((anfr_sign == 1) && (receiver_status == 1)) {
            anfr_sign = 0;
            sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &val);
            grip_sensor_sub_status = ((val>>26) & 0x01) == 0 ? 2 : 1;
            input_report_rel(grip_sensor_sub, REL_MISC, grip_sensor_sub_status);
            input_report_rel(grip_sensor_sub, REL_X, 2);
            input_sync(grip_sensor_sub);
            pr_info("%s anfr_sign[%d]\n", __func__, anfr_sign);
        }
    }

    pr_info("%s out\n", __func__);

    return count;
}
#endif
/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 end*/
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
static ssize_t sx933x_really_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 phases=0;
	u32 val;
    psx93XX_t this = dev_get_drvdata(dev);

    if (sscanf(buf, "%x", &phases) != 1)
    {
        pr_err("[SX933x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }
    if (phases != 0) {
        enable_save = true;
    } else {
        enable_save = false;
    }
	pr_info("[SX933x]: %s enable phases= 0x%X\n", __func__, phases);
	phases &= 0x3F;

    sx933x_i2c_read_16bit(this, 0x8020, &val);
	val &= ~0x3F;
	val |= phases;

    pr_info("[SX933x]: %s set reg 0x8020= 0x%X\n",__func__, val);
	sx933x_i2c_write_16bit(this, 0x8020, val);
	manual_offset_calibration(this);

    return count;
}
#ifndef CONFIG_SENSORS
static ssize_t sx933x_enable_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", enable_save);
}
static ssize_t sx933x_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 phases = 0;
    u32 val;
    int report1;
    int report2;
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;
    psx93XX_t this = dev_get_drvdata(dev);
    if (this && (pDevice = this->pDevice)) {
        if (sscanf(buf, "%x", &phases) != 1) {
            pr_err("[SX933x]: %s - The number of data are wrong\n",__func__);
            return -EINVAL;
        }
        if (phases != 0) {
            grip_sensor = pDevice->pbuttonInformation->grip_sensor;
            grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
            sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &val);
            pr_info("[SX933x]: %s enable phases= 0x%X\n", __func__, phases);
            if (onoff_mEnabled == true) {
#ifndef HQ_FACTORY_BUILD
                if (anfr_sign == 1) {
                    if (phases == 0x07) {
                        input_report_rel(grip_sensor, REL_MISC, 1);
                        input_report_rel(grip_sensor, REL_X, 1);
                        input_report_rel(grip_sensor_sub, REL_MISC, 1);
                        input_report_rel(grip_sensor_sub, REL_X, 1);
                        input_sync(grip_sensor);
                        input_sync(grip_sensor_sub);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign %d\n", phases, anfr_sign);
                    } else if (phases == 0x06) {
                        input_report_rel(grip_sensor_sub, REL_MISC, 1);
                        input_report_rel(grip_sensor_sub, REL_X, 1);
                        input_sync(grip_sensor_sub);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign %d\n", phases, anfr_sign);
                    } else if (phases == 0x03) {
                        input_report_rel(grip_sensor, REL_MISC, 1);
                        input_report_rel(grip_sensor, REL_X, 1);
                        input_sync(grip_sensor);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign %d\n", phases, anfr_sign);

                    }
                } else {
#endif
                    report1 = ((val>>24) & 0x01) == 0 ? 5 : 1;
                    report2 = ((val>>26) & 0x01) == 0 ? 5 : 1;
                    if (phases == 0x07) {
                        input_report_rel(grip_sensor, REL_MISC, report1);
                        input_report_rel(grip_sensor, REL_X, 2);
                        input_report_rel(grip_sensor_sub, REL_MISC, report2);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        input_sync(grip_sensor);
                        input_sync(grip_sensor_sub);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign not is 1\n", phases);
                    } else if (phases == 0x06) {
                        input_report_rel(grip_sensor_sub, REL_MISC, report2);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        input_sync(grip_sensor_sub);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign not is 1\n", phases);
                    } else if (phases == 0x03) {
                        input_report_rel(grip_sensor, REL_MISC, report1);
                        input_report_rel(grip_sensor, REL_X, 2);
                        input_sync(grip_sensor);
                        dev_info(this->pdev, "[SX933x]:phases %d anfr_sign not is 1\n", phases);
                    }
#ifndef HQ_FACTORY_BUILD
                }
#endif
            }
            enable_save = true;
        } else {
            enable_save = false;
        }
    }

    return count;
}
#endif
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/
static void read_dbg_raw(psx93XX_t this)
{
    int ph, state;
    u32 uData, ph_sel;
    s32 ant_use, ant_raw;
    s32 avg, diff;
    u16 off;
    s32 adc_min, adc_max, use_flt_dlt_var;
    s32 ref_a_use=0, ref_b_use=0;
    int ref_ph_a, ref_ph_b;

    psx933x_t pDevice = NULL;
    psx933x_platform_data_t pdata = NULL;

    pDevice = this->pDevice;
    pdata = pDevice->hw;
    ref_ph_a = pdata->ref_phase_a;
    ref_ph_b = pdata->ref_phase_b;
    dev_info(this->pdev, "[SX933x] ref_ph_a= %d ref_ph_b= %d\n", ref_ph_a, ref_ph_b);

    sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &uData);
    dev_info(this->pdev, "SX933X_STAT0_REG= 0x%X\n", uData);

    if(ref_ph_a != 0xFF)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + ref_ph_a*4, &uData);
        ref_a_use = (s32)uData >> 10;
    }
    if(ref_ph_b != 0xFF)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + ref_ph_b*4, &uData);
        ref_b_use = (s32)uData >> 10;
    }

    sx933x_i2c_read_16bit(this, SX933X_REG_DBG_PHASE_SEL, &ph_sel);

    sx933x_i2c_read_16bit(this, SX933X_REG_PROX_ADC_MIN, &uData);
    adc_min = (s32)uData>>10;
    sx933x_i2c_read_16bit(this, SX933X_REG_PROX_ADC_MAX, &uData);
    adc_max = (s32)uData>>10;
    sx933x_i2c_read_16bit(this, SX933X_REG_PROX_RAW, &uData);
    ant_raw = (s32)uData>>10;
    sx933x_i2c_read_16bit(this, SX933X_REG_DLT_VAR, &uData);
    use_flt_dlt_var = (s32)uData>>4;

    if (((ph_sel >> 3) & 0x7) == 0)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG, &uData);
        ant_use = (s32)uData>>10;
        ph = 0;
    }
    else if (((ph_sel >> 3) & 0x7) == 1)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH1_REG, &uData);
        ant_use = (s32)uData>>10;
        ph = 1;
    }
    else if (((ph_sel >> 3) & 0x7) == 2)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH2_REG, &uData);
        ant_use = (s32)uData>>10;
        ph = 2;
    }
    else if (((ph_sel >> 3) & 0x7) == 3)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH3_REG, &uData);
        ant_use = (s32)uData>>10;
        ph = 3;
    }
    else if (((ph_sel >> 3) & 0x7) == 4)
    {
        sx933x_i2c_read_16bit(this, SX933X_USEPH4_REG, &uData);
        ant_use = (s32)uData>>10;
        ph = 4;
    }
    else
    {
        dev_info(this->pdev, "read_dbg_raw(): invalid reg_val= 0x%X\n", ph_sel);
        ph = -1;
    }

    if(ph != -1)
    {
        sx933x_i2c_read_16bit(this, SX933X_AVGPH0_REG + ph*4, &uData);
        avg = (s32)uData>>10;
        sx933x_i2c_read_16bit(this, SX933X_DIFFPH0_REG + ph*4, &uData);
        diff = (s32)uData>>10;
        sx933x_i2c_read_16bit(this, SX933X_OFFSETPH0_REG + ph*4*2, &uData);
        off = (u16)(uData & 0x7FFF);
        state = psmtcButtons[ph].state;

        if(ref_ph_a != 0xFF && ref_ph_b != 0xFF)
        {
            dev_info(this->pdev,
            "SMTC_DBG PH= %d USE= %d RAW= %d PH%d_USE= %d PH%d_USE= %d STATE= %d AVG= %d DIFF= %d OFF= %d ADC_MIN= %d ADC_MAX= %d DLT= %d SMTC_END\n",
            ph,    ant_use, ant_raw, ref_ph_a, ref_a_use,  ref_ph_b, ref_b_use,    state,    avg,    diff,    off,    adc_min,   adc_max,    use_flt_dlt_var);
        }
        else if(ref_ph_a != 0xFF)
        {
            dev_info(this->pdev,
            "SMTC_DBG PH= %d USE= %d RAW= %d PH%d_USE= %d STATE= %d AVG= %d DIFF= %d OFF= %d ADC_MIN= %d ADC_MAX= %d DLT= %d SMTC_END\n",
            ph,    ant_use, ant_raw, ref_ph_a, ref_a_use,  state,    avg,    diff,    off,    adc_min,   adc_max,    use_flt_dlt_var);
        }
        else if(ref_ph_b != 0xFF)
        {
            dev_info(this->pdev,
            "SMTC_DBG PH= %d USE= %d RAW= %d PH%d_USE= %d STATE= %d AVG= %d DIFF= %d OFF= %d ADC_MIN= %d ADC_MAX= %d DLT= %d SMTC_END\n",
            ph,    ant_use, ant_raw, ref_ph_b, ref_b_use,  state,    avg,    diff,    off,    adc_min,   adc_max,    use_flt_dlt_var);
        }
        else
        {
            dev_info(this->pdev,
            "SMTC_DBG PH= %d USE= %d RAW= %d STATE= %d AVG= %d DIFF= %d OFF= %d ADC_MIN= %d ADC_MAX= %d DLT= %d SMTC_END\n",
            ph,    ant_use, ant_raw, state,    avg,    diff,    off,    adc_min,   adc_max,    use_flt_dlt_var);
        }
    }
}

static void read_rawData(psx93XX_t this)
{
    u8 csx, index;
    s32 useful, average, diff;
    s32 ref_a_use=0, ref_b_use=0;
    u32 uData;
    u16 offset;
    int state;
    psx933x_t pDevice = NULL;
    psx933x_platform_data_t pdata = NULL;
    int ref_ph_a, ref_ph_b;

    if(this)
    {
        pDevice = this->pDevice;
        pdata = pDevice->hw;
        ref_ph_a = pdata->ref_phase_a;
        ref_ph_b = pdata->ref_phase_b;
        dev_info(this->pdev, "[SX933x] ref_ph_a= %d ref_ph_b= %d\n", ref_ph_a, ref_ph_b);

        sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &uData);
        dev_info(this->pdev, "SX933X_STAT0_REG= 0x%X\n", uData);

        if(ref_ph_a != 0xFF)
        {
            sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + ref_ph_a*4, &uData);
            ref_a_use = (s32)uData >> 10;
        }
        if(ref_ph_b != 0xFF)
        {
            sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + ref_ph_b*4, &uData);
            ref_b_use = (s32)uData >> 10;
        }

        for(csx =0; csx<5; csx++)
        {
            index = csx*4;
            sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + index, &uData);
            useful = (s32)uData>>10;
            sx933x_i2c_read_16bit(this, SX933X_AVGPH0_REG + index, &uData);
            average = (s32)uData>>10;
            sx933x_i2c_read_16bit(this, SX933X_DIFFPH0_REG + index, &uData);
            diff = (s32)uData>>10;
            sx933x_i2c_read_16bit(this, SX933X_OFFSETPH0_REG + index*2, &uData);
            offset = (u16)(uData & 0x7FFF);

            state = psmtcButtons[csx].state;

            if(ref_ph_a != 0xFF && ref_ph_b != 0xFF)
            {
                dev_info(this->pdev,
                "SMTC_DAT PH= %d DIFF= %d USE= %d PH%d_USE= %d PH%d_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
                csx, diff, useful, ref_ph_a, ref_a_use, ref_ph_b, ref_b_use, state, offset, average);
            }
            else if(ref_ph_a != 0xFF)
            {
                dev_info(this->pdev,
                "SMTC_DAT PH= %d DIFF= %d USE= %d PH%d_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
                csx, diff, useful, ref_ph_a, ref_a_use, state, offset, average);
            }
            else if(ref_ph_b != 0xFF)
            {
                dev_info(this->pdev,
                "SMTC_DAT PH= %d DIFF= %d USE= %d PH%d_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
                csx, diff, useful, ref_ph_b, ref_b_use, state, offset, average);
            }
            else
            {
                dev_info(this->pdev,
                "SMTC_DAT PH= %d DIFF= %d USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
                csx, diff, useful, state, offset, average);
            }
        }

        read_dbg_raw(this);
    }
}

static ssize_t sx933x_raw_data_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);
    read_rawData(this);
    return 0;
}
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
static DEVICE_ATTR(manual_calibrate, 0664, manual_offset_calibration_show, manual_offset_calibration_store);
static DEVICE_ATTR(register_write, 0664, NULL, sx933x_register_write_store);
static DEVICE_ATTR(register_read, 0664, NULL, sx933x_register_read_store);
static DEVICE_ATTR(raw_data, 0664, sx933x_raw_data_show, NULL);
static DEVICE_ATTR(really_enable, 0664, NULL, sx933x_really_enable_store);
#ifndef CONFIG_SENSORS
static DEVICE_ATTR(enable, 0664, sx933x_enable_show,sx933x_enable_store);
#endif
/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 start*/
#ifndef HQ_FACTORY_BUILD
static DEVICE_ATTR(receiver_turnon, 0664, NULL, sx933x_receiver_turnon_store);
#endif
/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 end*/

static struct attribute *sx933x_attributes[] =
{
    &dev_attr_manual_calibrate.attr,
    &dev_attr_register_write.attr,
    &dev_attr_register_read.attr,
    &dev_attr_raw_data.attr,
    &dev_attr_really_enable.attr,
#ifndef CONFIG_SENSORS
    &dev_attr_enable.attr,
#endif
/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 start*/
#ifndef HQ_FACTORY_BUILD
    &dev_attr_receiver_turnon.attr,
#endif
/*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 end*/
    NULL,
};
static struct attribute_group sx933x_attr_group =
{
    .attrs = sx933x_attributes,
};
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/

#if defined(CONFIG_SENSORS)
static int enable_bottom(psx93XX_t this, unsigned int enable)
{
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u32 i = 0;

    pDevice = this->pDevice;
    grip_sensor = pDevice->pbuttonInformation->grip_sensor;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[0];

    if (enable == 1) {
        if(this->channel_status == 0) {
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x01)) {
            this->channel_status |= 0x01; //bit0 for ch0 status
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);

        if (anfr_sign == 1) {
            input_report_rel(grip_sensor, REL_MISC, 1);
            input_report_rel(grip_sensor, REL_X, 1);
            input_sync(grip_sensor);
        } else {
            sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask)) {
                input_report_rel(grip_sensor, REL_MISC, 1);
                input_report_rel(grip_sensor, REL_X, 2);
                input_sync(grip_sensor);
                pCurrentButton->state = ACTIVE;
            } else {
                input_report_rel(grip_sensor, REL_MISC, 2);
                input_report_rel(grip_sensor, REL_X, 2);
                input_sync(grip_sensor);
                pCurrentButton->state = IDLE;
            }
            touchProcess(this);
        }
    } else if (enable == 0) {
        if(this->channel_status & 0x01) {
            this->channel_status &= ~0x01;
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            mEnabled = 0;
        }
    } else {
        pr_err("unknown enable symbol\n");
    }

    return 0;
}

static int enable_top(psx93XX_t this, unsigned int enable)
{
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor_sub = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u32 i = 0;

    pDevice = this->pDevice;
    grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[1];

    if (enable == 1) {
        if(this->channel_status == 0) {
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x02)) {
            this->channel_status |= 0x02; //bit1 for ch1 status
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);

        if (anfr_sign == 1) {
            input_report_rel(grip_sensor_sub, REL_MISC, 1);
            input_report_rel(grip_sensor_sub, REL_X, 1);
            input_sync(grip_sensor_sub);
        } else {
            sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask)) {
                input_report_rel(grip_sensor_sub, REL_MISC, 1);
                input_report_rel(grip_sensor_sub, REL_X, 2);
                input_sync(grip_sensor_sub);
                pCurrentButton->state = ACTIVE;
            } else {
                input_report_rel(grip_sensor_sub, REL_MISC, 2);
                input_report_rel(grip_sensor_sub, REL_X, 2);
                input_sync(grip_sensor_sub);
                pCurrentButton->state = IDLE;
            }
            touchProcess(this);
        }
    } else if (enable == 0) {
        if(this->channel_status & 0x02) {
            this->channel_status &= ~0x02;
        }
        pr_info("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            mEnabled = 0;
        }
    } else {
        pr_err("unknown enable symbol\n");
    }

    return 0;
}

static ssize_t sx933x_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    psx93XX_t this = dev_get_drvdata(dev);
    int status = -1;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    pr_info("%s: dev->name:%s\n", __func__, temp_input_dev->name); 

    if(!this)
    {
        pr_info("%s: !this\n", __func__);    
        return snprintf(buf, 8, "0\n");
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if(this->channel_status & 0x01) {
            status = 1;    
        } else {
            status = 0;
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if(this->channel_status & 0x02) {
            status = 1;    
        } else {
            status = 0;
        }
    }

    return snprintf(buf, 8, "%d\n", status);
}

static ssize_t sx933x_enable_store (struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    psx93XX_t this = dev_get_drvdata(dev);
    u8 enable;
    int ret;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        pr_err("%s - Invalid Argument\n", __func__);
        return ret;
    }

    pr_info("%s: dev->name:%s new_value = %u\n", __func__, temp_input_dev->name, enable);

    if(!this)
    {
        pr_info("%s: !this\n", __func__);    
        return 0;
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (enable == 1) {
            enable_bottom(this, 1);
        } else {
            enable_bottom(this, 0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (enable == 1) {
            enable_top(this, 1);
        } else {
            enable_top(this, 0);
        }
    }

    return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        sx933x_enable_show, sx933x_enable_store);

static struct attribute *ss_sx933x_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group ss_sx933x_attributes_group = {
    .attrs = ss_sx933x_attributes
};

static ssize_t sx933x_vendor_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx933x_name_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx933x_grip_flush_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val = 0;
    int ret = 0;
    psx93XX_t this = dev_get_drvdata(dev);
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;

    pDevice = this->pDevice;
    grip_sensor = pDevice->pbuttonInformation->grip_sensor;
    grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;

    ret = kstrtou8(buf, 10, &val);
    if (ret < 0) {
        pr_err("%s: Invalid value of input, input=%ld\n", __func__, ret);
        return -EINVAL;
    }

    if(!strncmp(dev->kobj.name,"grip_sensor",sizeof("grip_sensor"))) {
        input_report_rel(grip_sensor, REL_MAX, val);
        input_sync(grip_sensor);
    } else if(!strncmp(dev->kobj.name,"grip_sensor_sub",sizeof("grip_sensor_sub"))) {
        input_report_rel(grip_sensor_sub, REL_MAX, val);
        input_sync(grip_sensor_sub);
    }
    else {
        pr_err("%s: unknown type\n", __func__);
    }   

    pr_info("%s ret=%u\n", __func__, val);
    return count;
}

static DEVICE_ATTR(name, S_IRUGO, sx933x_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx933x_vendor_show, NULL);
static DEVICE_ATTR(grip_flush, 0220, NULL, sx933x_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_grip_flush,
    NULL,
};
#endif


/**************************************/

/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct
 */
static void sx933x_reg_init(psx93XX_t this)
{
    psx933x_t pDevice = 0;
    psx933x_platform_data_t pdata = 0;
    int i = 0;
    /* configure device */
    dev_info(this->pdev, "[SX933x]:Going to Setup I2C Registers\n");
    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {

        if (this->reg_in_dts == true)
        {
            i = 0;
            while ( i < pdata->i2c_reg_num)
            {
                /* Write all registers/values contained in i2c_reg */
                dev_info(this->pdev, "[SX933x]: Going to Write Reg from dts: 0x%x Value: 0x%x\n",
                         pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
                sx933x_i2c_write_16bit(this, pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
                i++;
            }
        }
        else     // use static ones!!
        {
            while ( i < ARRAY_SIZE(sx933x_i2c_reg_setup))
            {
                /* Write all registers/values contained in i2c_reg */
                dev_info(this->pdev, "[SX933x]:Going to Write Reg: 0x%x Value: 0x%x\n",
                         sx933x_i2c_reg_setup[i].reg,sx933x_i2c_reg_setup[i].val);
                sx933x_i2c_write_16bit(this, sx933x_i2c_reg_setup[i].reg, sx933x_i2c_reg_setup[i].val);
                i++;
            }
        }
        /*******************************************************************************/
        sx933x_i2c_write_16bit(this, SX933X_CMD_REG, SX933X_PHASE_CONTROL);  //enable phase control
    }
    else
    {
        dev_err(this->pdev, "[SX933x]: ERROR! platform data 0x%p\n",pDevice->hw);
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
        pr_info("[SX933x]: SX933x income initialize\n");
        /* prepare reset by disabling any irq handling */
        this->irq_disabled = 1;
        disable_irq(this->irq);
        /* perform a reset */
        sx933x_i2c_write_16bit(this, SX933X_RESET_REG, I2C_SOFTRESET_VALUE);
        /* wait until the reset has finished by monitoring NIRQ */
        dev_info(this->pdev, "Sent Software Reset. Waiting until device is back from reset to continue.\n");
        /* just sleep for awhile instead of using a loop with reading irq status */
        msleep(100);
        ret = sx933x_global_variable_init(this);
        sx933x_reg_init(this);
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
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
#ifndef HQ_FACTORY_BUILD
static void anfr_attestation(psx93XX_t this, struct input_dev *grip_sensor, struct input_dev *grip_sensor_sub)
{
    if (this == NULL || grip_sensor == NULL || grip_sensor_sub == NULL) {
        printk("[SX933x]:ANFR FAIL\n");
        return;
    }

    /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
    if (onoff_mEnabled == true) {
        input_report_rel(grip_sensor, REL_MISC, 1);
        input_report_rel(grip_sensor, REL_X, 1);
        input_report_rel(grip_sensor_sub, REL_MISC, 1);
        input_report_rel(grip_sensor_sub, REL_X, 1);
        input_sync(grip_sensor);
        input_sync(grip_sensor_sub);
        dev_info(this->pdev, "[SX933x]:irq_sign %d cali_sign %d\n", irq_sign, cali_sign);
    }
    /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/
}
#endif
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/

/*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 start*/
/*!
 * \brief Handle what to do when a touch occurs
 * \param this Pointer to main parent struct
 */
static void touchProcess(psx93XX_t this)
{
    int counter = 0;
    u32 i = 0;
    int numberOfButtons = 0;
    psx933x_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;

    struct _buttonInfo *pCurrentButton  = NULL;

    if (this && (pDevice = this->pDevice))
    {
        sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        grip_sensor = pDevice->pbuttonInformation->grip_sensor;
        grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely((buttons == NULL) || (grip_sensor == NULL) || (grip_sensor_sub == NULL))) {
            dev_err(this->pdev, "[SX933x]:ERROR!! buttons or input NULL!!!\n");
            return;
        }

        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
        if (onoff_mEnabled == false) {
            return;
        }
        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

        /*hs14 code for SR-AL6528A-01-742|DEAL6398A-1888 by duxinqi at 2022/10/14 start*/
#ifndef HQ_FACTORY_BUILD
        /*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 start*/
        if ((anfr_sign == 1) && irq_sign < IRQ_NUM && cali_sign < 3) {
        /*hs14 code for AL6528A-1092 by xiongxiaoliang at 2023/02/20 end*/
            anfr_attestation(this, grip_sensor, grip_sensor_sub);
        } else {
            anfr_sign = 0;
#endif
            for (counter = 0; counter < numberOfButtons; counter++)
            {
                pCurrentButton = &buttons[counter];
                if (pCurrentButton == NULL)
                {
                    dev_err(this->pdev,"[SX933x]:ERROR!! current button at index: %d NULL!!!\n", counter);
                    return; // ERRORR!!!!
                }
                switch (pCurrentButton->state)
                {
                case IDLE: /* Button is not being touched! */
                    if (((i & pCurrentButton->mask) == pCurrentButton->mask)) {
                        /* User pressed button */
                        dev_info(this->pdev, "[SX933x]:Button %d touched\n", counter);
                        if (0 == counter && this->channel_status & 0x01) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(grip_sensor, KEY_SAR_CLOSE, 1);
                            input_report_key(grip_sensor, KEY_SAR_CLOSE, 0);
                            #else
                            input_report_rel(grip_sensor, REL_MISC, 1);
                            input_report_rel(grip_sensor, REL_X, 2);
                            #endif
                            input_sync(grip_sensor);
                            pCurrentButton->state = ACTIVE;
                        } else if(1 == counter && this->channel_status & 0x02) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 1);
                            input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 0);
                            pCurrentButton->state = ACTIVE;
                            #else
                            if ((i & SX933X_STAT_V2_FLAG) == SX933X_STAT_V2_FLAG) {
                                input_report_rel(grip_sensor_sub, REL_MISC, 1);
                                input_report_rel(grip_sensor_sub, REL_X, 2);
                                pCurrentButton->state = ACTIVE;
                            } else {
                                if((i & SX933X_STAT_V0_FLAG) == SX933X_STAT_V0_FLAG) {
                                    input_report_rel(grip_sensor_sub, REL_MISC, 2);
                                    input_report_rel(grip_sensor_sub, REL_X, 2);
                                    pCurrentButton->state = IDLE;
                                    dev_info(this->pdev, "[SX933x]:secondary1 state:%d\n",pCurrentButton->state);
                                } else {
                                    input_report_rel(grip_sensor_sub, REL_MISC, 1);
                                    input_report_rel(grip_sensor_sub, REL_X, 2);
                                    pCurrentButton->state = ACTIVE;
                                }
                            }
                            #endif
                            input_sync(grip_sensor_sub);
                        }
                    } else {
                        dev_info(this->pdev, "[SX933x]:Button %d already released.\n",counter);
                    }
                    break;
                case ACTIVE: /* Button is being touched! */
                    if (((i & pCurrentButton->mask) != pCurrentButton->mask)) {
                        /* User released button */
                        dev_info(this->pdev, "[SX933x]:Button %d released\n",counter);
                    if (0 == counter && this->channel_status & 0x01) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(grip_sensor, KEY_SAR_FAR, 1);
                            input_report_key(grip_sensor, KEY_SAR_FAR, 0);
                            #else
                            input_report_rel(grip_sensor, REL_MISC, 2);
                            input_report_rel(grip_sensor, REL_X, 2);
                            #endif
                            input_sync(grip_sensor);

                        } else if (1 == counter && this->channel_status & 0x02) {
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 1);
                            input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 0);
                            #else
                            input_report_rel(grip_sensor_sub, REL_MISC, 2);
                            input_report_rel(grip_sensor_sub, REL_X, 2);
                            #endif
                            input_sync(grip_sensor_sub);
                        }
                        pCurrentButton->state = IDLE;
                    } else if (1 == counter && (i & SX933X_STAT_V0_FLAG) == SX933X_STAT_V0_FLAG && (i & SX933X_STAT_V2_FLAG) != SX933X_STAT_V2_FLAG) {
                            input_report_rel(grip_sensor_sub, REL_MISC, 2);
                            input_report_rel(grip_sensor_sub, REL_X, 2);
                            input_sync(grip_sensor_sub);
                            pCurrentButton->state = IDLE;
                            dev_info(this->pdev, "[SX933x]:secondary2 state:%d\n",pCurrentButton->state);
                    } else {
                        dev_info(this->pdev, "[SX933x]:Button %d still touched.\n",counter);
                    }
                    break;
                default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
                    break;
                };
            }
#ifndef HQ_FACTORY_BUILD
        }
#endif
        /*hs14 code for SR-AL6528A-01-742|DEAL6398A-1888 by duxinqi at 2022/10/14 end*/

        dev_info(this->pdev, "Leaving touchProcess()\n");
    }
}
/*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 end*/

static int sx933x_parse_dt(struct sx933x_platform_data *pdata, struct device *dev)
{
    struct device_node *dNode = dev->of_node;
    enum of_gpio_flags flags;
    //u8 ref_phases;
    //int ret;

    if (dNode == NULL)
        return -ENODEV;

    pdata->irq_gpio= of_get_named_gpio_flags(dNode,
                     "Semtech,nirq-gpio", 0, &flags);
    irq_gpio_num = pdata->irq_gpio;
    if (pdata->irq_gpio < 0)
    {
        pr_err("[SX933x]: %s - get irq_gpio error\n", __func__);
        return -ENODEV;
    }

    pdata->ref_phase_a = -1;
    pdata->ref_phase_b = -1;
    if (of_property_read_u32(dNode,"Semtech,ref-phases-a",&pdata->ref_phase_a)) {
        pr_err("[SX933x]: %s - get ref-phases error\n", __func__);
        return -ENODEV;
    }
    if (of_property_read_u32(dNode,"Semtech,ref-phases-b",&pdata->ref_phase_b)) {
        pr_err("[SX933x]: %s - get ref-phases-b error\n", __func__);
        return -ENODEV;
    }
    pr_info("[SX933x]: %s ref_phase_a= %d ref_phase_b= %d\n",
        __func__, pdata->ref_phase_a, pdata->ref_phase_b);

    /***********************************************************************/
    // load in registers from device tree
    of_property_read_u32(dNode,"Semtech,reg-num",&pdata->i2c_reg_num);
    // layout is register, value, register, value....
    // if an extra item is after just ignore it. reading the array in will cause it to fail anyway
    pr_info("[sx933x]:%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
    if (pdata->i2c_reg_num > 0) {
        // initialize platform reg data array
        pdata->pi2c_reg = devm_kzalloc(dev,sizeof(struct smtc_reg_data)*pdata->i2c_reg_num, GFP_KERNEL);
        if (unlikely(pdata->pi2c_reg == NULL)) {
            return -ENOMEM;
        }

        // initialize the array
        if (of_property_read_u32_array(dNode, "Semtech,reg-init", (u32*)&(pdata->pi2c_reg[0]), sizeof(struct smtc_reg_data)*pdata->i2c_reg_num/sizeof(u32)))
            return -ENOMEM;
    }
    /***********************************************************************/

    pr_info("[SX933x]: %s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
    return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx933x_init_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx933x *pDevice = NULL;
    struct sx933x_platform_data *pdata = NULL;

    int rc;

    pr_info("[SX933x] : %s init_platform_hw start!",__func__);

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
    {
        if (gpio_is_valid(pdata->irq_gpio))
        {
            rc = gpio_request(pdata->irq_gpio, "sx933x_irq_gpio");
            if (rc < 0)
            {
                dev_err(this->pdev, "SX933x Request gpio. Fail![%d]\n", rc);
                return rc;
            }
            rc = gpio_direction_input(pdata->irq_gpio);
            if (rc < 0)
            {
                dev_err(this->pdev, "SX933x Set gpio direction. Fail![%d]\n", rc);
                return rc;
            }
            this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
        }
        else
        {
            dev_err(this->pdev, "SX933x Invalid irq gpio num.(init)\n");
        }
    }
    else
    {
        pr_err("[SX933x] : %s - Do not init platform HW", __func__);
    }

    pr_err("[SX933x]: %s - sx933x_irq_debug\n",__func__);
    return rc;
}

static void sx933x_exit_platform_hw(struct i2c_client *client)
{
    psx93XX_t this = i2c_get_clientdata(client);
    struct sx933x *pDevice = NULL;
    struct sx933x_platform_data *pdata = NULL;

    if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw)) {
        if (gpio_is_valid(pdata->irq_gpio)) {
            gpio_free(pdata->irq_gpio);
        } else {
            dev_err(this->pdev, "Invalid irq gpio num.(exit)\n");
        }
    }
    return;
}

static int sx933x_get_nirq_state(void)
{
    return !gpio_get_value(irq_gpio_num);
}

#ifdef SX933X_USB_CALI
static void calibration_func_callback(struct work_struct *work)
{
    pr_err("[SX933x]: calibration_func_callback_entry\n");
    manual_offset_calibration(sar_this);
}
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 start*/
static int sx933x_sar_calibration_notifier_callback(struct notifier_block *nb,unsigned long val, void *v)
{
    if (val == USB_PLUG_IN) {
        if (sx_sar_work != NULL) {
            queue_work(sx_sar_work, &sx_sar_enable_work);
        }
    }
    return 0;
}
/*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 end*/
void sx933x_sar_usb_callback_init(void)
{
    int ret = 0;
    sx_sar_work = create_singlethread_workqueue("sx_sar_calibration_wq");

    if (!sx_sar_work) {
        pr_err("[SX933x]: create_singlethread_workqueue failed\n");
        return;
    }

    INIT_WORK(&sx_sar_enable_work, calibration_func_callback);

    sx_sar_notify.notifier_call = sx933x_sar_calibration_notifier_callback;
    /*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 start*/
    ret = register_usb_check_notifier(&sx_sar_notify);
    if (ret < 0) {
        pr_err("[SX933x]: register_usb_check_notifier:error:%d",ret);
        unregister_usb_check_notifier(&sx_sar_notify);
    }
    /*hs14 code for SR-AL6528A-01-380 by lichang at 2022/09/26 end*/
}
#endif

#ifdef SX933X_FACTORY_NODE
//Three channels are turned on and off at the same time.（ch0 ch1 ch2）
#define SX933X_EXIT_SLEEP   0x0000000C
#define SX933X_ENTER_SLEEP  0x0000000D
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
static void enabletouchProcess(psx93XX_t this)
{
    int counter = 0;
    u32 i = 0;
    int numberOfButtons = 0;
    psx933x_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;

    struct _buttonInfo *pCurrentButton  = NULL;

    if (this && (pDevice = this->pDevice))
    {
        sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        grip_sensor = pDevice->pbuttonInformation->grip_sensor;
        grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely((buttons == NULL) || (grip_sensor == NULL) || (grip_sensor_sub == NULL))) {
            dev_err(this->pdev, "[SX933x]:ERROR!! buttons or input NULL!!!\n");
            return;
        }

        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
        if (onoff_mEnabled == false) {
            return;
        }
        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

        for (counter = 0; counter < numberOfButtons; counter++)
        {
            pCurrentButton = &buttons[counter];
            if (pCurrentButton == NULL)
            {
                dev_err(this->pdev,"[SX933x]:ERROR!! current button at index: %d NULL!!!\n", counter);
                return; // ERRORR!!!!
            }
            switch (pCurrentButton->state)
            {
            case IDLE: /* Button is not being touched! */
                if (((i & pCurrentButton->mask) == pCurrentButton->mask)) {
                    /* User pressed button */
                    dev_info(this->pdev, "[SX933x]:Button %d touched\n", counter);
                    if (0 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor, KEY_SAR_CLOSE, 1);
                        input_report_key(grip_sensor, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(grip_sensor, REL_MISC, 1);
                        input_report_rel(grip_sensor, REL_X, 2);
                        #endif
                        input_sync(grip_sensor);

                    } else if(1 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 1);
                        input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(grip_sensor_sub, REL_MISC, 1);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        #endif
                        input_sync(grip_sensor_sub);
                    }
                    pCurrentButton->state = ACTIVE;
                } else {
                   if (0 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor, KEY_SAR_FAR, 1);
                        input_report_key(grip_sensor, KEY_SAR_FAR, 0);
                        #else
                        input_report_rel(grip_sensor, REL_MISC, 5);
                        input_report_rel(grip_sensor, REL_X, 2);
                        #endif
                        input_sync(grip_sensor);

                    } else if (1 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 1);
                        input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 0);
                        #else
                        input_report_rel(grip_sensor_sub, REL_MISC, 5);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        #endif
                        input_sync(grip_sensor_sub);
                    }
                    dev_info(this->pdev, "[SX933x]:Button %d already released.\n", counter);
                }
                break;
            case ACTIVE: /* Button is being touched! */
                if (((i & pCurrentButton->mask) != pCurrentButton->mask)) {
                    /* User released button */
                    dev_info(this->pdev, "[SX933x]:Button %d released\n",counter);
                   if (0 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor, KEY_SAR_FAR, 1);
                        input_report_key(grip_sensor, KEY_SAR_FAR, 0);
                        #else
                        input_report_rel(grip_sensor, REL_MISC, 5);
                        input_report_rel(grip_sensor, REL_X, 2);
                        #endif
                        input_sync(grip_sensor);

                    } else if (1 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 1);
                        input_report_key(grip_sensor_sub, KEY_SAR2_FAR, 0);
                        #else
                        input_report_rel(grip_sensor_sub, REL_MISC, 5);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        #endif
                        input_sync(grip_sensor_sub);
                    }
                    pCurrentButton->state = IDLE;
                } else {
                    if (0 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor, KEY_SAR_CLOSE, 1);
                        input_report_key(grip_sensor, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(grip_sensor, REL_MISC, 1);
                        input_report_rel(grip_sensor, REL_X, 2);
                        #endif
                        input_sync(grip_sensor);

                    } else if(1 == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 1);
                        input_report_key(grip_sensor_sub, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(grip_sensor_sub, REL_MISC, 1);
                        input_report_rel(grip_sensor_sub, REL_X, 2);
                        #endif
                        input_sync(grip_sensor_sub);
                    }
                    dev_info(this->pdev, "[SX933x]:Button %d still touched.\n", counter);
                }
                break;
            default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
                break;
            };
        }

        dev_info(this->pdev, "Leaving enabletouchProcess()\n");
    }
}
/*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/
void factory_cali(void)
{
    pr_err("[SX933x]: factory_cali\n");
    if (sar_this != NULL) {
        manual_offset_calibration(sar_this);
    }
}

ssize_t factory_set_enable(const char *buf, size_t count)
{
    u32 val = 0;
    u32 enable = 0;

    if (sscanf(buf, "%x", &enable) != 1) {
        pr_err("[SX933x]: %s - The number of data are wrong\n",__func__);
        return -EINVAL;
    }

    if ((enable != enable_save) && (enable == 1)) {
        enable_save = true;

        if (sar_this != NULL) {
            sx933x_i2c_read_16bit(sar_this, SX933X_GNRLCTRL2_REG, &val);
            val |= 0x07;  //open phen0-2
            sx933x_i2c_write_16bit(sar_this, SX933X_GNRLCTRL2_REG, val);
            msleep(100);
            sx933x_i2c_write_16bit(sar_this, SX933X_CMD_REG, SX933X_EXIT_SLEEP);
            enabletouchProcess(sar_this);
            pr_info("[SX933x]: %s enable success.\n", __func__);
        }
    } else if ((enable != enable_save) && (enable == 0)) {
        enable_save = false;
/*
        sx933x_i2c_read_16bit(sar_this, SX933X_GNRLCTRL2_REG, &val);
        val = val & 0xfffffff8;  //close phen0-2  ~(0x07)
        sx933x_i2c_write_16bit(sar_this, SX933X_GNRLCTRL2_REG, val);
        msleep(100);
*/
        if (sar_this != NULL) {
            sx933x_i2c_write_16bit(sar_this, SX933X_CMD_REG, SX933X_ENTER_SLEEP);
            pr_info("[SX933x]: %s disable success.", __func__);
        }
    } else {
        pr_info("[SX933x]: %s already enable/disable %d", __func__, enable);
    }

    return count;
}

ssize_t factory_get_enable(char *buf)
{
    return sprintf(buf, "%d\n", enable_save);
}
#endif

/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
ssize_t sx933x_set_onoff(const char *buf, size_t count)
{
    psx93XX_t this = dev_get_drvdata(onoff_dev);
    psx933x_t pDevice = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;
    u32 val;
    int report1;
    int report2;

    if (this && (pDevice = this->pDevice)){
        grip_sensor = pDevice->pbuttonInformation->grip_sensor;
        grip_sensor_sub = pDevice->pbuttonInformation->grip_sensor_sub;
        if (!strncmp(buf, "1", 1)) {
            sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &val);
            report1 = ((val>>24) & 0x01) == 0 ? 5 : 1;
            report2 = ((val>>26) & 0x01) == 0 ? 5 : 1;
            input_report_rel(grip_sensor, REL_MISC, report1);
            input_report_rel(grip_sensor, REL_X, 2);
            input_report_rel(grip_sensor_sub, REL_MISC, report2);
            input_report_rel(grip_sensor_sub, REL_X, 2);
            input_sync(grip_sensor);
            input_sync(grip_sensor_sub);
            onoff_mEnabled = true;
            dev_info(this->pdev, "onoff Function set of on\n");
        } else if (!strncmp(buf, "0", 1)) {
            onoff_mEnabled = false;
            if (grip_sensor && grip_sensor_sub) {
                input_report_rel(grip_sensor, REL_MISC, 5);
                input_report_rel(grip_sensor, REL_X, 2);
                input_report_rel(grip_sensor_sub, REL_MISC, 5);
                input_report_rel(grip_sensor_sub, REL_X, 2);
                input_sync(grip_sensor);
                input_sync(grip_sensor_sub);
                dev_info(this->pdev, "onoff report down\n");
            } else {
                dev_err(this->pdev, "onoff input device has null pointer!");
            }
            dev_info(this->pdev, "onoff Function set of off\n");
        } else {
            dev_err(this->pdev, "onoff instruction error!");
        }

    } else {
        dev_err(this->pdev, "onoff Function has null pointer!");
    }
    return count;

}
ssize_t sx933x_get_onoff(char *buf)
{
    return snprintf(buf, 8, "%d\n", onoff_mEnabled);
}
/*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

/* hs14 code for AL6528A-104 by duxinqi at 2022/09/23 start */
int machine_detcet()
{
    char *command_line = saved_command_line;
    pr_info("[SX933x]: command_line = %s\n", command_line);
    if (NULL != strstr(command_line, "androidboot.pcbainfo=EU")) {
        pr_info("[SX933x]: %s machine is EU.", __func__);
        return 1;
    } else {
        pr_info("[SX933x]: %s machine not is EU.", __func__);
        return 0;
    }
}
/* hs14 code for AL6528A-104 by duxinqi at 2022/09/23 end */
/*! \fn static int sx933x_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */
static int sx933x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i = 0;
    int err = 0;
    /* hs14 code for AL6528A-104 by duxinqi at 2022/09/23 start */
    int ret = 0;

    psx93XX_t this = 0;
    psx933x_t pDevice = 0;
    psx933x_platform_data_t pplatData = 0;
    struct totalButtonInformation *pButtonInformationData = NULL;
    struct input_dev *grip_sensor = NULL;
    struct input_dev *grip_sensor_sub = NULL;
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

    ret = machine_detcet();
    if (ret == 0) {
        dev_err(&client->dev, "[SX933x]:machine not is EU!\n");
        return -1;
    }
    /* hs14 code for AL6528A-104 by duxinqi at 2022/09/23 end */

    dev_info(&client->dev, "[SX933x]:sx933x_probe() drv_ver= 01_phase_enable_node\n");

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
    {
        dev_err(&client->dev, "[SX933x]:Check i2c functionality.Fail!\n");
        err = -EIO;
        return err;
    }

    this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
    dev_info(&client->dev, "[SX933x]:\t Initialized Main Memory: 0x%p\n",this);

    pButtonInformationData = devm_kzalloc(&client->dev , sizeof(struct totalButtonInformation), GFP_KERNEL);
    if (!pButtonInformationData)
    {
        dev_err(&client->dev, "[SX933x]:Failed to allocate memory(totalButtonInformation)\n");
        err = -ENOMEM;
        return err;
    }

    pButtonInformationData->buttonSize = ARRAY_SIZE(psmtcButtons);
    pButtonInformationData->buttons =  psmtcButtons;
    pplatData = devm_kzalloc(&client->dev,sizeof(struct sx933x_platform_data), GFP_KERNEL);
    if (!pplatData)
    {
        dev_err(&client->dev, "[SX933x]:platform data is required!\n");
        return -EINVAL;
    }
    pplatData->get_is_nirq_low = sx933x_get_nirq_state;
    pplatData->pbuttonInformation = pButtonInformationData;

    client->dev.platform_data = pplatData;
    err = sx933x_parse_dt(pplatData, &client->dev);
    if (err)
    {
        dev_err(&client->dev, "[SX933x]:could not setup pin\n");
        return ENODEV;
    }

    pplatData->init_platform_hw = sx933x_init_platform_hw;
    dev_err(&client->dev, "[SX933x]:SX933x init_platform_hw done!\n");

    if (this)
    {
        dev_info(&client->dev, "[SX933x]:SX933x initialize start!!");
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
        /*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 start*/
        if (MAX_NUM_STATUS_BITS>= 8)
        {
            this->statusFunc[0] = 0; /* TXEN_STAT */
            this->statusFunc[1] = 0; /* UNUSED */
            this->statusFunc[2] = touchProcess; /* UNUSED */
            this->statusFunc[3] = read_rawData; /* CONV_STAT */
            this->statusFunc[4] = touchProcess; /* COMP_STAT */
            this->statusFunc[5] = touchProcess; /* RELEASE_STAT */
            this->statusFunc[6] = touchProcess; /* TOUCH_STAT  */
            this->statusFunc[7] = 0; /* RESET_STAT */
        }
        /*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 end*/

        /* setup i2c communication */
        this->bus = client;
        i2c_set_clientdata(client, this);

        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 start*/
        onoff_dev = &client->dev;
        sar_func.set_onoff = sx933x_set_onoff;
        sar_func.get_onoff = sx933x_get_onoff;
        /*HS14_U code for SR-AL6528E-01-37 by xiayujie at 2023/07/28 end*/

        /* record device struct */
        this->pdev = &client->dev;

        /* create memory for device specific struct */
        this->pDevice = pDevice = devm_kzalloc(&client->dev, sizeof(sx933x_t), GFP_KERNEL);
        dev_info(&client->dev, "[SX933x]:\t Initialized Device Specific Memory: 0x%p\n", pDevice);

        if (pDevice)
        {
            /* for accessing items in user data (e.g. calibrate) */
            err = sysfs_create_group(&client->dev.kobj, &sx933x_attr_group);
            //sysfs_create_group(client, &sx933x_attr_group);

            /* Add Pointer to main platform data struct */
            pDevice->hw = pplatData;

            /* Check if we hava a platform initialization function to call*/
            if (pplatData->init_platform_hw)
                pplatData->init_platform_hw(client);

            /* Initialize the button information initialized with keycodes */
            pDevice->pbuttonInformation = pplatData->pbuttonInformation;
            /* Create the input device */
            grip_sensor = input_allocate_device();
            if (!grip_sensor) {
                return -ENOMEM;
            }
            /*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 start*/
            /* Set all the keycodes */
            #ifdef HQ_FACTORY_BUILD
            __set_bit(EV_KEY, grip_sensor->evbit);
            __set_bit(KEY_SAR_FAR, grip_sensor->keybit);
            __set_bit(KEY_SAR_CLOSE, grip_sensor->keybit);
            #else
            __set_bit(EV_REL, grip_sensor->evbit);
            __set_bit(REL_MISC, grip_sensor->relbit);
            __set_bit(REL_X, grip_sensor->relbit);
            input_set_capability(grip_sensor, EV_REL, REL_MAX);
            #endif

            /* Create the input device */
            grip_sensor_sub = input_allocate_device();
            if (!grip_sensor_sub)
            {
                return -ENOMEM;
            }
            /* Set all the keycodes */
            #ifdef HQ_FACTORY_BUILD
            __set_bit(EV_KEY, grip_sensor_sub->evbit);
            __set_bit(KEY_SAR2_FAR, grip_sensor_sub->keybit);
            __set_bit(KEY_SAR2_CLOSE, grip_sensor_sub->keybit);
            #else
            __set_bit(EV_REL, grip_sensor_sub->evbit);
            __set_bit(REL_MISC, grip_sensor_sub->relbit);
            __set_bit(REL_X, grip_sensor_sub->relbit);
            input_set_capability(grip_sensor_sub, EV_REL, REL_MAX);
            #endif
            /*hs14 code for SR-AL6528A-01-742 by duxinqi at 2022/10/12 end*/

#if 1
            for (i = 0; i < pButtonInformationData->buttonSize; i++)
            {
                pButtonInformationData->buttons[i].state = IDLE;
            }
#endif
            /* save the input pointer and finish initialization */
            pButtonInformationData->grip_sensor = grip_sensor;
            grip_sensor->name = "grip_sensor";
            grip_sensor->id.bustype = BUS_I2C;
            if (input_register_device(grip_sensor)) {
                return -ENOMEM;
            }

            pButtonInformationData->grip_sensor_sub = grip_sensor_sub;
            grip_sensor_sub->name = "grip_sensor_sub";
            grip_sensor_sub->id.bustype = BUS_I2C;
            if (input_register_device(grip_sensor_sub)) {
                return -ENOMEM;
            }
        }

#if defined(CONFIG_SENSORS)
        input_set_drvdata(grip_sensor, this);
        input_set_drvdata(grip_sensor_sub, this);
        this->channel_status = 0;
        err = sensors_register(&pplatData->factory_device, this, sensor_attrs, MODULE_NAME);
        if (err) {
            dev_err(&client->dev, "%s - cound not register sensor(%d).\n", __func__, err);
            return -ENOMEM;
        }

        err = sensors_register(&pplatData->factory_device_sub, this, sensor_attrs, SUB_MODULE_NAME);
        if (err) {
            dev_err(&client->dev, "%s - cound not register wifi sensor(%d).\n", __func__, err);
            return -ENOMEM;
        }

        err = sysfs_create_group(&grip_sensor->dev.kobj, &ss_sx933x_attributes_group);
        if (err) {
            dev_err(&client->dev, "cound not create input main group\n");
            return -ENOMEM;
        }

        err = sysfs_create_group(&grip_sensor_sub->dev.kobj, &ss_sx933x_attributes_group);
        if (err) {
            dev_err(&client->dev, "cound not create input wifi group\n");
            return -ENOMEM;
        }
#endif

        sx93XX_IRQ_init(this);
        /* call init function pointer (this should initialize all registers */
        if (this->init) {
            this->init(this);
        } else {
            dev_err(this->pdev,"[SX933x]:No init function!!!!\n");
            return -ENOMEM;
        }
    } else {
        return -1;
    }
#if defined(SX933X_USB_CALI) || defined(SX933X_FACTORY_NODE)
    sar_this = this;
#endif
#ifdef SX933X_USB_CALI
    sx933x_sar_usb_callback_init();
#endif
    err = sx933x_Hardware_Check(this);
    if (err > 0) {
        dev_err(&client->dev,"[SX933x]:sx933x_Hardware_Check fail!\n");
        return err;
    } else {
#ifdef SX933X_FACTORY_NODE
        sar_drv.sar_name_drv = "sx933x";
        sar_drv.get_cali = factory_cali;
        sar_drv.set_enable = factory_set_enable;
        sar_drv.get_enable = factory_get_enable;
        dev_err(&client->dev,"[SX933x]:sx933x factory node success.\n");
#endif
    }

    pplatData->exit_platform_hw = sx933x_exit_platform_hw;

    dev_info(&client->dev, "[SX933x]:sx933x_probe() Done\n");

    return 0;
}

/*! \fn static int sx933x_remove(struct i2c_client *client)
 * \brief Called when device is to be removed
 * \param client Pointer to i2c_client struct
 * \return Value from sx93XX_remove()
 */
//static int __devexit sx933x_remove(struct i2c_client *client)
static int sx933x_remove(struct i2c_client *client)
{
    psx933x_platform_data_t pplatData =0;
    psx933x_t pDevice = 0;
    psx93XX_t this = i2c_get_clientdata(client);
    if (this && (pDevice = this->pDevice)) {
        input_unregister_device(pDevice->pbuttonInformation->grip_sensor);
        input_unregister_device(pDevice->pbuttonInformation->grip_sensor_sub);

        sysfs_remove_group(&client->dev.kobj, &sx933x_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw(client);
        kfree(this->pDevice);
    }
    return sx93XX_remove(this);
}
#if 1//def CONFIG_PM
/*====================================================*/
/***** Kernel Suspend *****/
static int sx933x_suspend(struct device *dev)
{
    if (enable_save) {
        psx93XX_t this = dev_get_drvdata(dev);
        irq_set_irq_wake(this->irq, 1);
        //sx93XX_suspend(this);
        pr_err("[SX933X]: %s out\n", __func__);
    }
    return 0;
}
/***** Kernel Resume *****/
static int sx933x_resume(struct device *dev)
{
    if (enable_save) {
        psx93XX_t this = dev_get_drvdata(dev);
        irq_set_irq_wake(this->irq, 0);
        //sx93XX_resume(this);
        pr_err("[SX933X]: %s out\n", __func__);
    }
    return 0;
}
/*====================================================*/
#else
#define sx933x_suspend       NULL
#define sx933x_resume        NULL
#endif /* CONFIG_PM */

static struct i2c_device_id sx933x_idtable[] =
{
    { DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sx933x_idtable);
#ifdef CONFIG_OF
static struct of_device_id sx933x_match_table[] =
{
    { .compatible = "Semtech,sx933x",},
    { },
};
#else
#define sx933x_match_table NULL
#endif
static const struct dev_pm_ops sx933x_pm_ops =
{
    .suspend = sx933x_suspend,
    .resume  = sx933x_resume,
};
static struct i2c_driver sx933x_driver =
{
    .driver = {
        .owner             = THIS_MODULE,
        .name              = DRIVER_NAME,
        .of_match_table    = sx933x_match_table,
        .pm                = &sx933x_pm_ops,
    },
    .id_table        = sx933x_idtable,
    .probe           = sx933x_probe,
    .remove          = sx933x_remove,
};
static int __init sx933x_I2C_init(void)
{
    return i2c_add_driver(&sx933x_driver);
}
static void __exit sx933x_I2C_exit(void)
{
    i2c_del_driver(&sx933x_driver);
}

module_init(sx933x_I2C_init);
module_exit(sx933x_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX933x Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
    unsigned long flags;
    if (this) {
        dev_info(this->pdev, "sx93XX_schedule_work()\n");
        spin_lock_irqsave(&this->lock, flags);
        /* Stop any pending penup queues */
        cancel_delayed_work(&this->dworker);
        //after waiting for a delay, this put the job in the kernel-global workqueue. so no need to create new thread in work queue.
        schedule_delayed_work(&this->dworker, delay);
        spin_unlock_irqrestore(&this->lock, flags);
    } else {
        printk(KERN_ERR "sx93XX_schedule_work, NULL psx93XX_t\n");
    }
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
    psx93XX_t this = 0;
    /*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 start*/
    /*hs14 code for AL6528A-194 by duxinqi at 2022/09/29 start*/
#ifndef HQ_FACTORY_BUILD
    if (irq_sign < IRQ_NUM) {
        irq_sign++;
    }
    printk("sx93XX:irq_sign already is (%d)\n", irq_sign);
#endif
    /*hs14 code for AL6528A-194 by duxinqi at 2022/09/29 end*/
    /*hs14 code for AL6528A-1091 by Wentao at 2023/2/17 end*/
    if (pvoid) {
        this = (psx93XX_t)pvoid;
        if ((!this->get_nirq_low) || this->get_nirq_low()) {
            sx93XX_schedule_work(this, 0);
        } else {
            dev_err(this->pdev, "sx93XX_irq - nirq read high\n");
        }
    } else {
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
    if (work) {
        this = container_of(work,sx93XX_t,dworker.work);

        if (!this) {
            printk(KERN_ERR "sx93XX_worker_func, NULL sx93XX_t\n");
            return;
        }

        if (unlikely(this->useIrqTimer)) {
            if ((!this->get_nirq_low) || this->get_nirq_low()) {
                nirqLow = 1;
            }
        }
        /* since we are not in an interrupt don't need to disable irq. */
        status = this->refreshStatus(this);
        counter = -1;
        /*hs14 code for AL6528A-194|DEAL6398A-1888 by duxinqi at 2022/10/14 start*/
        dev_err(this->pdev, "Worker1 - Refresh Status %d\n",status);
#ifndef HQ_FACTORY_BUILD
        if (cali_sign < 3 && ((status>>4) & 0x01)) {
            cali_sign++;
            dev_info(this->pdev, "[SX933x]:cali_sign %d\n", cali_sign);

        }
        dev_err(this->pdev, "Worker2 - Refresh Status %d\n",status);
#endif
        /*hs14 code for AL6528A-194|DEAL6398A-1888 by duxinqi at 2022/10/14 end*/

        while((++counter) < MAX_NUM_STATUS_BITS)   /* counter start from MSB */
        {
            if (((status>>counter) & 0x01) && (this->statusFunc[counter]))
            {
                dev_info(this->pdev, "SX933x Function Pointer Found. Calling statusFunc[%d].\n", counter);
                this->statusFunc[counter](this);
            }
        }
        if (unlikely(this->useIrqTimer && nirqLow))
        {
            /* Early models and if RATE=0 for newer models require a penup timer */
            /* Queue up the function again for checking on penup */
            sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
        }
    } else {
        printk(KERN_ERR "sx93XX_worker_func, NULL work_struct\n");
    }
}

int sx93XX_remove(psx93XX_t this)
{
    if (this) {
        cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
        /*destroy_workqueue(this->workq); */
        free_irq(this->irq, this);
        kfree(this);
        return 0;
    }
    return -ENOMEM;
}
void sx93XX_suspend(psx93XX_t this)
{
    if (this) {
        disable_irq(this->irq);
    }
}
void sx93XX_resume(psx93XX_t this)
{
    if (this) {
        enable_irq(this->irq);
    }
}

int sx93XX_IRQ_init(psx93XX_t this)
{
    int err = 0;
    if (this && this->pDevice) {
        /* initialize spin lock */
        spin_lock_init(&this->lock);
        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);
        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
                          this->pdev->driver->name, this);
        if (err) {
            dev_err(this->pdev, "irq %d busy?\n", this->irq);
            return err;
        }
        dev_info(this->pdev, "registered with irq (%d)\n", this->irq);
    }
    return -ENOMEM;
}
