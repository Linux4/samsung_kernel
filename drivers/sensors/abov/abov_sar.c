/*
 * file abov_sar.c
 * brief abov Driver for two channel SAP using
 *
 * Driver for the ABOV
 * Copyright (c) 2015-2016 ABOV Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DRIVER_NAME "abov_sar"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>

#if defined(CONFIG_FB)
#include <linux/fb.h>
#endif

#include "abov_sar.h"

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#if defined(CONFIG_SENSORS)
#include <linux/sensor/sensors_core.h>
#endif

/*HS60 code for HS60-354 by zhuqiang at 2019/09/24 start*/
#include <soc/qcom/smem.h>
u32 pcb_id_code = 0;
/*HS60 code for HS60-354 by zhuqiang at 2019/09/24 end*/

#if defined(CONFIG_SENSORS)
#define VENDOR_NAME              "ABOV"
#define MODEL_NAME               "A96T3X6"
#define MODULE_NAME              "grip_sensor"
#endif

#define SLEEP(x)    mdelay(x)

#define C_I2C_FIFO_SIZE 8

#define I2C_MASK_FLAG    (0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;

#define IDLE    0
#define ACTIVE  1

/* HS60 add for HS60-2078 by zhuqiang at 2019/09/24 start */
#define SAR_FAR    0
#define SAR_CLOSE  1
/* HS60 add for HS60-2078 by zhuqiang at 2019/09/24 end */

#define ABOV_DEBUG 1

/* HS60 add for HS60-663 by zhuqiang at 2019/08/23 start */
#define LOG_TAG "[ABOV] "
/* HS60 add for HS60-663 by zhuqiang at 2019/08/23 end */

#if ABOV_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG fmt, ##args)
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 start*/
static int mEnabled = 1;
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/
static int programming_done;
pabovXX_t abov_sar_ptr;

/*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 start*/
extern char *sar_name;
/*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 end*/

/**
 * struct abov
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct abov {
    pbuttonInformation_t pbuttonInformation;
    pabov_platform_data_t hw; /* specific platform data settings */
} abov_t, *pabov_t;


/**
 * fn static int write_register(pabovXX_t this, u8 address, u8 value)
 * brief Sends a write register to the device
 * param this Pointer to main parent struct
 * param address 8-bit register address
 * param value   8-bit register value to write to address
 * return Value from i2c_master_send
 */
static int write_register(pabovXX_t this, u8 address, u8 value)
{
    struct i2c_client *i2c = 0;
    char buffer[2];
    int returnValue = 0;

    buffer[0] = address;
    buffer[1] = value;
    returnValue = -ENOMEM;
    if (this && this->bus) {
        i2c = this->bus;

        returnValue = i2c_master_send(i2c, buffer, 2);
        LOG_INFO("write_register Addr: 0x%02x Val: 0x%x Return: %d\n",
                address, value, returnValue);
    }
    if (returnValue < 0) {
        LOG_DBG("Write_register error!\n");
    }
    return returnValue;
}

/**
 * fn static int read_register(pabovXX_t this, u8 address, u8 *value)
 * brief Reads a register's value from the device
 * param this Pointer to main parent struct
 * param address 8-Bit address to read from
 * param value Pointer to 8-bit value to save register value to
 * return Value from i2c_smbus_read_byte_data if < 0. else 0
 */
static int read_register(pabovXX_t this, u8 address, u8 *value)
{
    struct i2c_client *i2c = 0;
    s32 returnValue = 0;

    if (this && value && this->bus) {
        i2c = this->bus;
        returnValue = i2c_smbus_read_byte_data(i2c, address);
        LOG_INFO("read_register Addr: 0x%02x Return: 0x%x\n",
                address, returnValue);
        if (returnValue >= 0) {
            *value = returnValue;
            return 0;
        } else {
            return returnValue;
        }
    }
    LOG_INFO("read_register error!\n");
    return -ENOMEM;
}


/**
 * brief Perform a manual offset calibration
 * param this Pointer to main parent struct
 * return Value return value from the write register
 */
static int manual_offset_calibration(pabovXX_t this)
{
    s32 returnValue = 0;

    returnValue = write_register(this, ABOV_RECALI_REG, 0x01);
    return returnValue;
}

/**
 * brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u8 reg_value = 0;
    pabovXX_t this = dev_get_drvdata(dev);

    LOG_INFO("Reading IRQSTAT_REG\n");
    read_register(this, ABOV_IRQSTAT_LEVEL_REG, &reg_value);
    return scnprintf(buf, PAGE_SIZE, "%d\n", reg_value);
}

/* brief sysfs store function for manual calibration */
static ssize_t manual_offset_calibration_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = dev_get_drvdata(dev);
    unsigned long val;

    if (kstrtoul(buf, 0, &val))
        return -EINVAL;
    if (val) {
        LOG_INFO("Performing manual_offset_calibration()\n");
        manual_offset_calibration(this);
    }
    return count;
}

static DEVICE_ATTR(calibrate, 0644, manual_offset_calibration_show,
        manual_offset_calibration_store);
static struct attribute *abov_attributes[] = {
    &dev_attr_calibrate.attr,
    NULL,
};
static struct attribute_group abov_attr_group = {
    .attrs = abov_attributes,
};

/**
 * fn static int read_regStat(pabovXX_t this)
 * brief Shortcut to read what caused interrupt.
 * details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * param this Pointer to main parent struct
 * return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(pabovXX_t this)
{
    u8 data = 0;

    if (this) {
        if (read_register(this, ABOV_IRQSTAT_LEVEL_REG, &data) == 0)
            return (data & 0x00FF);
    }
    return 0;
}

/**
 * brief  Initialize I2C config from platform data
 * param this Pointer to main parent struct
 *
 */
static void hw_init(pabovXX_t this)
{
    pabov_t pDevice = 0;
    pabov_platform_data_t pdata = 0;
    int i = 0;
    /* configure device */
    LOG_INFO("Going to Setup I2C Registers\n");
    pDevice = this->pDevice;
    if (!pDevice)
    {
        LOG_DBG("ERROR! pDevice is NULL\n");
        return;
    }
    pdata = pDevice->hw;
    if (this && pDevice && pdata) {
        while (i < pdata->i2c_reg_num) {
            /* Write all registers/values contained in i2c_reg */
            LOG_INFO("Going to Write Reg: 0x%02x Value: 0x%02x\n",
                    pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
            write_register(this, pdata->pi2c_reg[i].reg,
                    pdata->pi2c_reg[i].val);
            i++;
        }
    } else {
        LOG_DBG("ERROR! platform data 0x%p\n", pDevice->hw);
    }
}

/**
 * fn static int initialize(pabovXX_t this)
 * brief Performs all initialization needed to configure the device
 * param this Pointer to main parent struct
 * return Last used command's return value (negative if error)
 */
static int initialize(pabovXX_t this)
{

    if (this) {
        /* prepare reset by disabling any irq handling */
        this->irq_disabled = 1;
        disable_irq(this->irq);
        hw_init(this);
        /*HS60 code for HS60-32 by zhuqiang at 2019/7/18 start*/
        //msleep(300); /* make sure everything is running */
        /*HS60 code for HS60-32 by zhuqiang at 2019/7/18 end*/
        /* re-enable interrupt handling */
        enable_irq(this->irq);
        /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
        enable_irq_wake(this->irq);
        /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/
        this->irq_disabled = 0;

        /* make sure no interrupts are pending since enabling irq will only
         * work on next falling edge */
        read_regStat(this);
        LOG_INFO("Exiting initialize(). NIRQ = %d\n",
                this->get_nirq_low(this->board->irq_gpio));
#if defined(CONFIG_SENSORS)
        this->skip_data = false;
#endif
        programming_done = ACTIVE;
        return 0;
    }
    programming_done = IDLE;
    return -ENOMEM;
}

/**
 * brief Handle what to do when a touch occurs
 * param this Pointer to main parent struct
 */
static void touchProcess(pabovXX_t this)
{
    u8 i = 0;
    int numberOfButtons = 0;
    pabov_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *input_sar = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    struct abov_platform_data *board;

    pDevice = this->pDevice;
    board = this->board;
    if (this && pDevice) {
        LOG_INFO("Inside touchProcess()\n");
        read_register(this, ABOV_IRQSTAT_LEVEL_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        input_sar = pDevice->pbuttonInformation->input_sar;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely((buttons == NULL) || (input_sar == NULL))) {
            LOG_DBG("ERROR!! buttons or input NULL!!!\n");
            return;
        }

        pCurrentButton = &buttons[0];
        if (pCurrentButton == NULL) {
            LOG_DBG("ERR!current button index: 0 NULL!\n");
            return; /* ERRORR!!!! */
        }
#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
            return;
        }
#endif
        /*HS60 code for AR-ZQL1695-01000000070/HS60-2078 by zhuqiang at 2019/09/24 start*/
        switch (i & 0x03) {
          case SAR_CLOSE: /* < 20mm */
            LOG_INFO("sar sensor detect.\n");
            #ifdef HQ_FACTORY_BUILD
            input_report_key(input_sar, KEY_SAR_CLOSE, 1);
            input_report_key(input_sar, KEY_SAR_CLOSE, 0);
            #else
            input_report_rel(input_sar, REL_MISC, 1);
            #endif
            input_sync(input_sar);
            break;
          case SAR_FAR: /* >20mm */
            LOG_INFO("sar sensor release.\n");
            #ifdef HQ_FACTORY_BUILD
            input_report_key(input_sar, KEY_SAR_FAR, 1);
            input_report_key(input_sar, KEY_SAR_FAR, 0);
            #else
            input_report_rel(input_sar, REL_MISC, 2);
            #endif
            input_sync(input_sar);
            break;
          default:
            break;
        };
        /*HS60 code for AR-ZQL1695-01000000070/HS60-2078 by zhuqiang at 2019/09/24 end*/
        LOG_INFO("Leaving touchProcess()\n");
    }
}

static int abov_get_nirq_state(unsigned irq_gpio)
{
    if (irq_gpio) {
        return !gpio_get_value(irq_gpio);
    } else {
        LOG_INFO("abov irq_gpio is not set.");
        return -EINVAL;
    }
}

static struct _totalButtonInformation smtcButtonInformation = {
    .buttons = psmtcButtons,
    .buttonSize = ARRAY_SIZE(psmtcButtons),
};

/**
 *fn static void abov_reg_setup_init(struct i2c_client *client)
 *brief read reg val form dts
 *      reg_array_len for regs needed change num
 *      data_array_val's format <reg val ...>
 */
static void abov_reg_setup_init(struct i2c_client *client)
{
    u32 data_array_len = 0;
    u32 *data_array;
    int ret, i, j;
    struct device_node *np = client->dev.of_node;

    ret = of_property_read_u32(np, "reg_array_len", &data_array_len);
    if (ret < 0) {
        LOG_DBG("data_array_len read error");
        return;
    }
    data_array = kmalloc(data_array_len * 2 * sizeof(u32), GFP_KERNEL);
    ret = of_property_read_u32_array(np, "reg_array_val",
            data_array,
            data_array_len*2);
    if (ret < 0) {
        LOG_DBG("data_array_val read error");
        return;
    }
    for (i = 0; i < ARRAY_SIZE(abov_i2c_reg_setup); i++) {
        for (j = 0; j < data_array_len*2; j += 2) {
            if (data_array[j] == abov_i2c_reg_setup[i].reg) {
                abov_i2c_reg_setup[i].val = data_array[j+1];
                LOG_DBG("read dtsi 0x%02x:0x%02x set reg\n",
                    data_array[j], data_array[j+1]);
            }
        }
    }
    kfree(data_array);
}

/*HS60 code for HS60-354 by zhuqiang at 2019/09/24 start*/
static void pcb_abov_conf(void)
{
    u32 *pcb_addr = NULL;
    u32 pcb_size = 0;

    pcb_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &pcb_size, 0, 0);
    if (pcb_addr)
    {
        /*HS60 code for ZQL1693-269 by zhuqiang at 2020/02/27 start*/
        pcb_id_code = (*pcb_addr & 0x00000ff0) >> 4;
        /*HS60 code for ZQL1693-269 by zhuqiang at 2020/02/27 end*/
        LOG_INFO("pcb_id_code=0x%x\n", pcb_id_code);
    }
    else
    {
        LOG_INFO("reading the smem conf fail\n");
    }
}
/*HS60 code for HS60-354 by zhuqiang at 2019/09/24 end*/

static void abov_platform_data_of_init(struct i2c_client *client,
        pabov_platform_data_t pplatData)
{
    struct device_node *np = client->dev.of_node;
    int ret;

    /*HS60 code for HS60-32 by zhuqiang at 2019/7/17 start*/
    client->irq = of_get_named_gpio(np, "gpio-irq-std", 0);
    LOG_INFO("client->irq = %d\n", client->irq);
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/17 end*/
    pplatData->irq_gpio = client->irq;
    pplatData->get_is_nirq_low = abov_get_nirq_state;
    pplatData->init_platform_hw = NULL;
    /*  pointer to an exit function. Here in case needed in the future */
    /*
     *.exit_platform_hw = abov_exit_ts,
     */
    pplatData->exit_platform_hw = NULL;
    abov_reg_setup_init(client);
    pplatData->pi2c_reg = abov_i2c_reg_setup;
    pplatData->i2c_reg_num = ARRAY_SIZE(abov_i2c_reg_setup);

    pplatData->pbuttonInformation = &smtcButtonInformation;
    /*HS60 code for HS60-354 by zhuqiang at 2019/09/24 start*/
    pcb_abov_conf();
    /*HS60 code for ZQL1693-269 by zhuqiang at 2020/02/27 start*/
    if (pcb_id_code == 0x9)
        ret = of_property_read_string(np, "label2", &pplatData->fw_name);
    else if (pcb_id_code == 0x6)
        ret = of_property_read_string(np, "label3", &pplatData->fw_name);
    /*HS60 code for ZQL1693-269 by zhuqiang at 2020/02/27 end*/
    else
        ret = of_property_read_string(np, "label1", &pplatData->fw_name);

    if (ret < 0) {
        LOG_DBG("firmware name read error!\n");
        return;
    }
    LOG_INFO("firmware name = %s.BIN\n", pplatData->fw_name);
    /*HS60 code for HS60-354 by zhuqiang at 2019/09/24 end*/
}

/*HS60 code for HS60-204 by zhuqiang at 2019/7/25 start*/
static ssize_t capsense_enable_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t capsense_enable_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_sar = NULL;

    pDevice = this->pDevice;
    input_sar = pDevice->pbuttonInformation->input_sar;

    if (!count || (this == NULL))
        return -EINVAL;

    if (!strncmp(buf, "1", 1)) {
        LOG_DBG("enable cap sensor\n");
        enable_irq(this->irq);
        write_register(this, ABOV_CTRL_MODE_REG, 0x00);
        this->statusFunc[0](this);
        mEnabled = 1;
    } else if (!strncmp(buf, "0", 1)) {
        LOG_DBG("disable cap sensor\n");
        disable_irq(this->irq);
        write_register(this, ABOV_CTRL_MODE_REG, 0x01);
        /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 start*/
        #ifdef HQ_FACTORY_BUILD
        input_report_key(input_sar, KEY_SAR_FAR, 1);
        input_report_key(input_sar, KEY_SAR_FAR, 0);
        #else
        input_report_rel(input_sar, REL_MISC, 2);
        #endif
        /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/
        input_sync(input_sar);
        mEnabled = 0;
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return count;
}

static CLASS_ATTR(enable, 0660, capsense_enable_show, capsense_enable_store);
/*HS60 code for HS60-204 by zhuqiang at 2019/7/25 end*/

static ssize_t reg_dump_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 reg_value = 0, i;
    pabovXX_t this = abov_sar_ptr;
    char *p = buf;

    if (this->read_flag) {
        this->read_flag = 0;
        read_register(this, this->read_reg, &reg_value);
        p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n", this->read_reg, reg_value);
        return (p-buf);
    }

    for (i = 0; i < 0x26; i++) {
        read_register(this, i, &reg_value);
        p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
                i, reg_value);
    }

    for (i = 0x80; i < 0x8C; i++) {
        read_register(this, i, &reg_value);
        p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
                i, reg_value);
    }

    return (p-buf);
}

static ssize_t reg_dump_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    unsigned int val, reg, opt;
    if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
        LOG_INFO("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
        this->read_reg = *((u8 *)&reg);
        this->read_flag = 1;
    } else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
        LOG_INFO("%s,reg = 0x%02x, val = 0x%02x\n",
                __func__, *(u8 *)&reg, *(u8 *)&val);
        write_register(this, *((u8 *)&reg), *((u8 *)&val));
    }

    return count;
}

static CLASS_ATTR(reg, 0660, reg_dump_show, reg_dump_store);

static struct class capsense_class = {
    .name            = "capsense",
    .owner            = THIS_MODULE,
};

#if defined(CONFIG_SENSORS)
static ssize_t abovxx_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", mEnabled);
}

static ssize_t abovxx_enable_store (struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    u8 enable;
    int ret;
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_sar = NULL;

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    LOG_INFO("%s - new_value = %u\n", __func__, enable);

    pDevice = this->pDevice;
    input_sar = pDevice->pbuttonInformation->input_sar;

    if (enable == 1) {
        if(this->irq_disabled == 0){
            enable_irq(this->irq);
            this->irq_disabled = 1;
            LOG_DBG("abov enable_irq\n");
        }

        write_register(this, ABOV_CTRL_MODE_REG, 0x00);
        msleep(20);

        this->statusFunc[0](this);
        mEnabled = 1;
    } else if (enable == 0) {
        LOG_DBG("disable capsensor\n");

        if(this->irq_disabled == 1){
            disable_irq(this->irq);
            this->irq_disabled = 0;
            LOG_DBG("capsensor disable_irq\n");
        }

        write_register(this, ABOV_CTRL_MODE_REG, 0x01);

        input_report_rel(input_sar, REL_MISC, 2);
        input_sync(input_sar);
        mEnabled = 0;
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return 0;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        abovxx_enable_show, abovxx_enable_store);

static struct attribute *abovxx_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group abovxx_attributes_group = {
    .attrs = abovxx_attributes
};

static ssize_t abovxx_vendor_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t abovxx_name_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t abovxx_onoff_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    pabovXX_t this = abov_sar_ptr;

    return snprintf(buf, PAGE_SIZE, "%u\n", !this->skip_data);
}

static ssize_t abovxx_onoff_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val;
    int ret;
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_sar = NULL;

    pDevice = this->pDevice;
    input_sar = pDevice->pbuttonInformation->input_sar;
    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        this->skip_data = true;
        if (mEnabled) {
            input_report_rel(input_sar, REL_MISC, 2);
            input_sync(input_sar);
        }
    } else {
        this->skip_data = false;
    }

    LOG_INFO("%s -%u\n", __func__, val);
    return count;
}

static DEVICE_ATTR(name, S_IRUGO, abovxx_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, abovxx_vendor_show, NULL);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
        abovxx_onoff_show, abovxx_onoff_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_onoff,
    NULL,
};
#endif

static int _i2c_adapter_block_write(struct i2c_client *client, u8 *data, u8 len)
{
    u8 buffer[C_I2C_FIFO_SIZE];
    u8 left = len;
    u8 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg = {
        .addr = client->addr & I2C_MASK_FLAG,
        .flags = 0,
        .buf = buffer,
    };

    if (data == NULL || len < 1) {
        LOG_ERR("Invalid : data is null or len=%d\n", len);
        return -EINVAL;
    }

    while (left > 0) {
        retry = 0;
        if (left >= C_I2C_FIFO_SIZE) {
            msg.buf = &data[offset];
            msg.len = C_I2C_FIFO_SIZE;
            left -= C_I2C_FIFO_SIZE;
            offset += C_I2C_FIFO_SIZE;
        } else {
            msg.buf = &data[offset];
            msg.len = left;
            left = 0;
        }

        while (i2c_transfer(client->adapter, &msg, 1) != 1) {
            retry++;
            if (retry > 10) {
                LOG_ERR("OUT : fail - addr:%#x len:%d \n", client->addr, msg.len);
                return -EIO;
            }
        }
    }
    return 0;
}

static int abov_tk_check_busy(struct i2c_client *client)
{
    int ret, count = 0;
    unsigned char val = 0x00;

    do {
        ret = i2c_master_recv(client, &val, sizeof(val));
        if (val & 0x01) {
            count++;
            if (count > 1000) {
                LOG_INFO("%s: val = 0x%x\r\n", __func__, val);
                return ret;
            }
        } else {
            break;
        }
    } while (1);
    return ret;
}

static int abov_tk_fw_write(struct i2c_client *client, unsigned char *addrH,
                        unsigned char *addrL, unsigned char *val)
{
    int length = 36, ret = 0;
    unsigned char buf[40] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0x7A;
    memcpy(&buf[2], addrH, 1);
    memcpy(&buf[3], addrL, 1);
    memcpy(&buf[4], val, 32);
    ret = _i2c_adapter_block_write(client, buf, length);
    if (ret < 0) {
        LOG_ERR("Firmware write fail ...\n");
        return ret;
    }

    SLEEP(3);
    abov_tk_check_busy(client);
    return 0;
}

static int abov_tk_reset_for_bootmode(struct i2c_client *client)
{
    int ret, retry_count = 10;
    unsigned char buf[3] = {0, };

retry:
    buf[0] = 0xF0;
    buf[1] = 0xAA;
    ret = i2c_master_send(client, buf, 2);
    if (ret < 0) {
        LOG_INFO("write fail(retry:%d)\n", retry_count);
        if (retry_count-- > 0) {
            goto retry;
        }
        return -EIO;
    } else {
        LOG_INFO("success reset & boot mode\n");
        return 0;
    }
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
    int ret = 0;
    unsigned char buf[3] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0x5B;
    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        LOG_ERR("SEND : fail - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }
    LOG_INFO("SEND : succ - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
    SLEEP(5);

    ret = i2c_master_recv(client, buf, 1);
    if (ret < 0) {
        LOG_ERR("Enter fw mode fail ... Device ID:%#x\n",buf[0]);
        return -EIO;
    }

    LOG_INFO("Enter fw mode succ ... Device ID:%#x\n", buf[0]);
    return 0;
}

static int abov_tk_fw_mode_exit(struct i2c_client *client)
{
    int ret = 0;
    unsigned char buf[3] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0xE1;
    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }
    LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
    return 0;
}

static int abov_tk_flash_erase(struct i2c_client *client)
{
    int ret = 0;
    unsigned char buf[3] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0x2E;

    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }

    LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
    return 0;
}

static int abov_tk_i2c_read_checksum(struct i2c_client *client)
{
    unsigned char checksum[6] = {0, };
    unsigned char buf[7] = {0, };
    int ret;

    checksum_h = 0;
    checksum_l = 0;

    buf[0] = 0xAC;
    buf[1] = 0x9E;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = checksum_h_bin;
    buf[5] = checksum_l_bin;
    ret = i2c_master_send(client, buf, 6);

    if (ret != 6) {
        LOG_ERR("SEND : fail - addr:%#x len:%d ... ret:%d\n", client->addr, 6, ret);
        return -EIO;
    }
    SLEEP(5);

    buf[0] = 0x00;
    ret = i2c_master_send(client, buf, 1);
    if (ret != 1) {
        LOG_ERR("SEND : fail - addr:%#x data:%#x ... ret:%d\n", client->addr, buf[0], ret);
        return -EIO;
    }
    SLEEP(5);

    ret = i2c_master_recv(client, checksum, 6);
    if (ret < 0) {
        LOG_ERR("Read raw fail ... \n");
        return -EIO;
    }

    checksum_h = checksum[4];
    checksum_l = checksum[5];

    return 0;
}

static int _abov_fw_update(struct i2c_client *client, const u8 *image, u32 size)
{
    int ret, ii = 0;
    int count;
    unsigned short address;
    unsigned char addrH, addrL;
    unsigned char data[32] = {0, };

    LOG_INFO("%s: call in\r\n", __func__);

    if (abov_tk_reset_for_bootmode(client) < 0) {
        LOG_ERR("don't reset(enter boot mode)!");
        return -EIO;
    }

    SLEEP(45);

    for (ii = 0; ii < 10; ii++) {
        if (abov_tk_fw_mode_enter(client) < 0) {
            LOG_ERR("don't enter the download mode! %d", ii);
            SLEEP(40);
            continue;
        }
        break;
    }

    if (10 <= ii) {
        return -EAGAIN;
    }

    if (abov_tk_flash_erase(client) < 0) {
        LOG_ERR("don't erase flash data!");
        return -EIO;
    }

    SLEEP(1400);

    address = 0x800;
    count = size / 32;

    for (ii = 0; ii < count; ii++) {
        /* first 32byte is header */
        addrH = (unsigned char)((address >> 8) & 0xFF);
        addrL = (unsigned char)(address & 0xFF);
        memcpy(data, &image[ii * 32], 32);
        ret = abov_tk_fw_write(client, &addrH, &addrL, data);
        if (ret < 0) {
            LOG_INFO("fw_write.. ii = 0x%x err\r\n", ii);
            return ret;
        }

        address += 0x20;
        memset(data, 0, 32);
    }

    ret = abov_tk_i2c_read_checksum(client);
    ret = abov_tk_fw_mode_exit(client);
    if ((checksum_h == checksum_h_bin) && (checksum_l == checksum_l_bin)) {
        LOG_INFO("fw update succ. checksum_h:%x=checksum_h_bin:%x && checksum_l:%x=checksum_l_bin:%x\n",
            checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
    } else {
        LOG_INFO("fw update fail. checksum_h:%x!=checksum_h_bin:%x && checksum_l:%x!=checksum_l_bin:%x\n",
            checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
        ret = -1;
    }
    /* HS60 add for HS60-663 by zhuqiang at 2019/08/23 start */
    SLEEP(110);
    /* HS60 add for HS60-663 by zhuqiang at 2019/08/23 end */

    return ret;
}

static int abov_fw_update(bool force)
{
    int update_loop;
    pabovXX_t this = abov_sar_ptr;
    struct i2c_client *client = this->bus;
    int rc;
    bool fw_upgrade = false;
    u8 fw_version = 0, fw_file_version = 0;
    u8 fw_modelno = 0, fw_file_modeno = 0;
    const struct firmware *fw = NULL;
    char fw_name[32] = {0};

    strlcpy(fw_name, this->board->fw_name, NAME_MAX);
    strlcat(fw_name, ".BIN", NAME_MAX);
    rc = request_firmware(&fw, fw_name, this->pdev);
    if (rc < 0) {
        LOG_INFO("Request firmware failed - %s (%d)\n",
                this->board->fw_name, rc);
        return rc;
    }

    if (force == false) {
        read_register(this, ABOV_VERSION_REG, &fw_version);
        read_register(this, ABOV_MODELNO_REG, &fw_modelno);
    }

    fw_file_modeno = fw->data[1];
    fw_file_version = fw->data[5];
    checksum_h_bin = fw->data[8];
    checksum_l_bin = fw->data[9];

    if ((force) || (fw_version < fw_file_version) || (fw_modelno != fw_file_modeno))
        fw_upgrade = true;
    else {
        LOG_INFO("Exiting fw upgrade...\n");
        fw_upgrade = false;
        rc = -EIO;
        goto rel_fw;
    }

    if (fw_upgrade) {
        for (update_loop = 0; update_loop < 10; update_loop++) {
            rc = _abov_fw_update(client, &fw->data[32], fw->size-32);
            if (rc < 0)
                LOG_INFO("retry : %d times!\n", update_loop);
            else {
                initialize(this);
                break;
            }
        }
        if (update_loop >= 10)
            rc = -EIO;
    }

rel_fw:
    release_firmware(fw);
    return rc;
}

static ssize_t capsense_fw_ver_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 fw_version = 0;
    pabovXX_t this = abov_sar_ptr;

    read_register(this, ABOV_VERSION_REG, &fw_version);

    return snprintf(buf, 16, "ABOV:0x%02x\n", fw_version);
}

static ssize_t capsense_update_fw_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    unsigned long val;
    int rc;

    if (count > 2)
        return -EINVAL;

    rc = kstrtoul(buf, 10, &val);
    if (rc != 0)
        return rc;

    this->irq_disabled = 1;
    disable_irq(this->irq);

    mutex_lock(&this->mutex);
    if (!this->loading_fw  && val) {
        this->loading_fw = true;
        abov_fw_update(false);
        this->loading_fw = false;
    }
    mutex_unlock(&this->mutex);

    enable_irq(this->irq);
    this->irq_disabled = 0;

    return count;
}
static CLASS_ATTR(update_fw, 0660, capsense_fw_ver_show, capsense_update_fw_store);

static ssize_t capsense_force_update_fw_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    unsigned long val;
    int rc;

    if (count > 2)
        return -EINVAL;

    rc = kstrtoul(buf, 10, &val);
    if (rc != 0)
        return rc;

    this->irq_disabled = 1;
    disable_irq(this->irq);

    mutex_lock(&this->mutex);
    if (!this->loading_fw  && val) {
        this->loading_fw = true;
        abov_fw_update(true);
        this->loading_fw = false;
    }
    mutex_unlock(&this->mutex);

    enable_irq(this->irq);
    this->irq_disabled = 0;

    return count;
}
static CLASS_ATTR(force_update_fw, 0660, capsense_fw_ver_show, capsense_force_update_fw_store);

/* HS60 add for HS60-663 by zhuqiang at 2019/08/23 start */
static void capsense_update_work(struct work_struct *work)
{
    pabovXX_t this = container_of(work, abovXX_t, fw_update_work);

    LOG_INFO("%s: start update firmware\n", __func__);

    this->irq_disabled = 1;
    disable_irq(this->irq);

    mutex_lock(&this->mutex);
    this->loading_fw = true;
    abov_fw_update(false);
    this->loading_fw = false;
    mutex_unlock(&this->mutex);

    enable_irq(this->irq);
    this->irq_disabled = 0;

    LOG_INFO("%s: update firmware end\n", __func__);
}

static void capsense_fore_update_work(struct work_struct *work)
{
    pabovXX_t this = container_of(work, abovXX_t, fw_update_work);

    LOG_INFO("%s: start force update firmware\n", __func__);

    this->irq_disabled = 1;
    disable_irq(this->irq);

    mutex_lock(&this->mutex);
    this->loading_fw = true;
    abov_fw_update(true);
    this->loading_fw = false;
    mutex_unlock(&this->mutex);

    enable_irq(this->irq);
    this->irq_disabled = 0;

    LOG_INFO("%s: force update firmware end\n", __func__);
}
/* HS60 add for HS60-663 by zhuqiang at 2019/08/23 end */


/**
 * detect if abov exist or not
 * return 1 if chip exist else return 0
 */
static int abov_detect(struct i2c_client *client)
{
    s32 returnValue = 0, i;
    u8 address = ABOV_VENDOR_ID_REG;
    u8 value = 0xAB;

    if (client) {
        for (i = 0; i < 3; i++) {
            returnValue = i2c_smbus_read_byte_data(client, address);
            LOG_INFO("abov read_register for %d time Addr: 0x%02x Return: 0x%02x\n",
                    i, address, returnValue);
            if (returnValue >= 0) {
                if (value == returnValue) {
                    LOG_INFO("abov detect success!\n");
                    return 1;
                }
            }
        }

        for (i = 0; i < 3; i++) {
                if(abov_tk_fw_mode_enter(client) == 0) {
                    LOG_INFO("abov boot detect success!\n");
                    return 2;
                }
        }
    }
    LOG_INFO("abov detect failed!!!\n");
    return 0;
}


/**
 * fn static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * brief Probe function
 * param client pointer to i2c_client
 * param id pointer to i2c_device_id
 * return Whether probe was successful
 */
static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    pabovXX_t this = 0;
    pabov_t pDevice = 0;
    pabov_platform_data_t pplatData = 0;
    bool isForceUpdate = false;
    int ret;
    struct input_dev *input_sar = NULL;

    LOG_INFO("abov_probe start()\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
    {
        LOG_INFO("i2c_check_functionality is fail\n");
        return -EIO;
    }

    /*HS60 code for ZQL169XFAC-110 by zhuqiang at 2020/01/08 start*/
    pplatData = kzalloc(sizeof(abov_platform_data_t), GFP_KERNEL);
    /*HS60 code for ZQL169XFAC-110 by zhuqiang at 2020/01/08 end*/
    if (!pplatData) {
        LOG_DBG("platform data is required!\n");
        return -EINVAL;
    }

    pplatData->vddio = regulator_get(&client->dev, "vddio");
    if (IS_ERR(pplatData->vddio))
    {
        LOG_INFO("%s: Failed to get vddio regulator\n", __func__);
    }
    else
    {
        ret = regulator_enable(pplatData->vddio);
        if (ret) {
            regulator_put(pplatData->vddio);
            LOG_DBG("%s: Error %d enable regulator\n", __func__, ret);
        }
        LOG_INFO("vddio regulator is %s\n",
                regulator_is_enabled(pplatData->vddio) ? "on" : "off");
    }

    pplatData->vdd = regulator_get(&client->dev, "vdd");
    if (IS_ERR(pplatData->vdd)) {
        LOG_INFO("%s: Failed to get vdd regulator\n", __func__);
    }
    else
    {
        ret = regulator_enable(pplatData->vdd);
        if (ret) {
            regulator_put(pplatData->vdd);
            LOG_DBG("%s: Error %d enable regulator\n", __func__, ret);
        }
        LOG_INFO("vdd regulator is %s\n",
                regulator_is_enabled(pplatData->vdd) ? "on" : "off");
    }
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/16 start*/
    msleep(100);
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/16 end*/

    /* detect if abov exist or not */
    ret = abov_detect(client);
    /*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 start*/
    if (ret == 0) {
        ret = -ENODEV;
        goto exit_no_detect_device;
    } else if (ret == 2) {
        isForceUpdate = true;
    }

    sar_name = "abov";
    /*HS60 code for AR-ZQL1695-01000000041 by zhuqiang at 2019/7/24 end*/
    abov_platform_data_of_init(client, pplatData);
    client->dev.platform_data = pplatData;

    this = kzalloc(sizeof(abovXX_t), GFP_KERNEL); /* create memory for main struct */
    if (!this) {
        LOG_ERR("failed to alloc for main struct\n");
        ret = -ENOMEM;
        goto exit_alloc_mem;
    } else {
        LOG_INFO("\t Initialized Main Memory: 0x%p\n", this);
    }

    /* In case we need to reinitialize data
    * (e.q. if suspend reset device) */
    this->init = initialize;
    /* shortcut to read status of interrupt */
    this->refreshStatus = read_regStat;
    /* pointer to function from platform data to get pendown
    * (1->NIRQ=0, 0->NIRQ=1) */
    this->get_nirq_low = pplatData->get_is_nirq_low;
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/17 start*/
    /* save irq in case we need to reference it */
    this->irq = gpio_to_irq(client->irq);
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/17 end*/
    /* do we need to create an irq timer after interrupt ? */
    this->useIrqTimer = 0;
    this->board = pplatData;
    /* Setup function to call on corresponding reg irq source bit */
    if (MAX_NUM_STATUS_BITS >= 2) {
        this->statusFunc[0] = touchProcess; /* RELEASE_STAT */
        this->statusFunc[1] = touchProcess; /* TOUCH_STAT  */
    }

    /* setup i2c communication */
    this->bus = client;
    i2c_set_clientdata(client, this);

    /* record device struct */
    this->pdev = &client->dev;

    /* create memory for device specific struct */
    this->pDevice = pDevice = kzalloc(sizeof(abov_t), GFP_KERNEL);
    if (!pDevice) {
        LOG_ERR("failed to alloc for device memory\n");
        ret = -ENOMEM;
        goto exit_alloc_dev;
    } else {
        LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n",
                pDevice);
    }
    abov_sar_ptr = this;

    /* for accessing items in user data (e.g. calibrate) */
    ret = sysfs_create_group(&client->dev.kobj, &abov_attr_group);

    /* Check if we hava a platform initialization function to call*/
    if (pplatData->init_platform_hw)
        pplatData->init_platform_hw();

    /* Add Pointer to main platform data struct */
    pDevice->hw = pplatData;

    /* Initialize the button information initialized with keycodes */
    pDevice->pbuttonInformation = pplatData->pbuttonInformation;

    /* Create the input device */
    input_sar = input_allocate_device();
    if (!input_sar) {
        LOG_ERR("input_allocate_device failed \n");
        ret = -ENOMEM;
        goto exit_alloc_input;
    }
    /* Set all the keycodes */
    /*HS60 code for AR-ZQL1695-01000000070/HS60-2078 by zhuqiang at 2019/09/24 start*/
    #ifdef HQ_FACTORY_BUILD
    __set_bit(EV_KEY, input_sar->evbit);
    __set_bit(KEY_SAR_FAR, input_sar->keybit);
    __set_bit(KEY_SAR_CLOSE, input_sar->keybit);
    #else
    input_set_capability(input_sar, EV_REL, REL_MISC);
    #endif
    /*HS60 code for AR-ZQL1695-01000000070/HS60-2078 by zhuqiang at 2019/09/24 end*/
    /* save the input pointer and finish initialization */
    pDevice->pbuttonInformation->input_sar = input_sar;
    /* save the input pointer and finish initialization */
    input_sar->name = "grip_sensor";
    input_sar->id.bustype = BUS_I2C;
    if (input_register_device(input_sar)) {
        LOG_ERR("add abov sar sensor unsuccess\n");
        ret = -ENOMEM;
        goto exit_register_input;
    }

#if defined(CONFIG_SENSORS)
    ret = sysfs_create_group(&input_sar->dev.kobj, &abovxx_attributes_group);
    if (ret) {
        LOG_ERR("%s - cound not create group(%d)\n",
                __func__, ret);
        goto exit_sysfs_create;
    }

    ret = sensors_register(&pplatData->factory_device, pplatData, sensor_attrs, MODULE_NAME);
    if (ret) {
        LOG_ERR("%s - cound not register sensor(%d).\n",
                __func__, ret);
        goto exit_sensors_register;
    }
#endif

    ret = class_register(&capsense_class);
    if (ret < 0) {
        LOG_DBG("Create fsys class failed (%d)\n", ret);
        goto exit_creat_file;
    }

    /*HS60 code for HS60-204 by zhuqiang at 2019/7/25 start*/
    ret = class_create_file(&capsense_class, &class_attr_enable);
    if (ret < 0) {
        LOG_DBG("Create enable file failed (%d)\n", ret);
        goto exit_creat_file;
    }
    /*HS60 code for HS60-204 by zhuqiang at 2019/7/25 end*/

    ret = class_create_file(&capsense_class, &class_attr_reg);
    if (ret < 0) {
        LOG_DBG("Create reg file failed (%d)\n", ret);
        goto exit_creat_file;
    }

    ret = class_create_file(&capsense_class, &class_attr_update_fw);
    if (ret < 0) {
        LOG_DBG("Create update_fw file failed (%d)\n", ret);
        goto exit_creat_file;
    }

    ret = class_create_file(&capsense_class, &class_attr_force_update_fw);
    if (ret < 0) {
        LOG_DBG("Create update_fw file failed (%d)\n", ret);
        goto exit_creat_file;
    }
    abovXX_sar_init(this);
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/18 start*/
    //write_register(this, ABOV_CTRL_MODE_REG, 0x02);
    /*HS60 code for HS60-32 by zhuqiang at 2019/7/18 end*/
    /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 start*/
    //mEnabled = 0;
    /*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/7/29 end*/

    this->loading_fw = false;
    if (isForceUpdate == true) {
        INIT_WORK(&this->fw_update_work, capsense_fore_update_work);
    } else {
        INIT_WORK(&this->fw_update_work, capsense_update_work);
    }
    schedule_work(&this->fw_update_work);

    LOG_INFO("abov_probe end()\n");
    return 0;

exit_creat_file:
#if defined(CONFIG_SENSORS)
    sensors_unregister(pplatData->factory_device, sensor_attrs);
exit_sensors_register:
    sysfs_remove_group(&input_sar->dev.kobj,
            &abovxx_attributes_group);
exit_sysfs_create:
#endif
    input_unregister_device(input_sar);
exit_register_input:
    input_free_device(input_sar);
exit_alloc_input:
    kfree(pDevice);
exit_alloc_dev:
    kfree(this);
exit_alloc_mem:
exit_no_detect_device:
    /* HS60 code for HS60-354 by zhuqiang at 2019/08/27 start */
    regulator_disable(pplatData->vddio);
    regulator_put(pplatData->vddio);
    regulator_disable(pplatData->vdd);
    regulator_put(pplatData->vdd);
    LOG_ERR("free vdd/vddio regulator source!\n");
    /* HS60 code for HS60-354 by zhuqiang at 2019/08/27 end */

    kfree(pplatData);
    return ret;
}

/**
 * fn static int abov_remove(struct i2c_client *client)
 * brief Called when device is to be removed
 * param client Pointer to i2c_client struct
 * return Value from abovXX_sar_remove()
 */
static int abov_remove(struct i2c_client *client)
{
    pabov_platform_data_t pplatData = 0;
    pabov_t pDevice = 0;
    pabovXX_t this = i2c_get_clientdata(client);

    pDevice = this->pDevice;
    if (this && pDevice) {
        input_unregister_device(pDevice->pbuttonInformation->input_sar);
        sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw();
        kfree(this->pDevice);
    }
    return abovXX_sar_remove(this);
}

/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
static int abov_suspend(struct device *dev)
{
    if (mEnabled){
        pabovXX_t this = dev_get_drvdata(dev);
        abovXX_suspend(this);
        write_register(this, ABOV_CTRL_MODE_REG, 0x01);//400ms check
    }
    return 0;
}
static int abov_resume(struct device *dev)
{
    if (mEnabled){
        pabovXX_t this = dev_get_drvdata(dev);
        abovXX_resume(this);
        write_register(this, ABOV_CTRL_MODE_REG, 0x00);//30ms check
    }
    return 0;
}
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/

static const struct dev_pm_ops abov_pm_ops = {
    .suspend = abov_suspend,
    .resume = abov_resume,
};

#ifdef CONFIG_OF
static struct of_device_id abov_match_table[] = {
    { .compatible = "abov,abov_sar" },
    { },
};
#endif

static struct i2c_device_id abov_idtable[] = {
    { DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, abov_idtable);

static struct i2c_driver abov_driver = {
    .driver = {
        .owner  = THIS_MODULE,
        .name   = DRIVER_NAME,
        .of_match_table    = abov_match_table,
        .pm     = &abov_pm_ops,
    },
    .id_table = abov_idtable,
    .probe      = abov_probe,
    .remove      = abov_remove,
};
static int __init abov_init(void)
{
    return i2c_add_driver(&abov_driver);
}
static void __exit abov_exit(void)
{
    i2c_del_driver(&abov_driver);
}

module_init(abov_init);
module_exit(abov_exit);

MODULE_AUTHOR("ABOV Corp.");
MODULE_DESCRIPTION("ABOV Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#ifdef USE_THREADED_IRQ
static void abovXX_process_interrupt(pabovXX_t this, u8 nirqlow)
{
    int status = 0;

    if (!this) {
        pr_err("abovXX_worker_func, NULL abovXX_t\n");
        return;
    }
    /* since we are not in an interrupt don't need to disable irq. */
    status = this->refreshStatus(this);
    LOG_INFO("Worker - Refresh Status %d\n", status);
    this->statusFunc[0](this);
    if (unlikely(this->useIrqTimer && nirqlow)) {
        /* In case we need to send a timer for example on a touchscreen
         * checking penup, perform this here
         */
        cancel_delayed_work(&this->dworker);
        schedule_delayed_work(&this->dworker, msecs_to_jiffies(this->irqTimeout));
        LOG_INFO("Schedule Irq timer");
    }
}


static void abovXX_worker_func(struct work_struct *work)
{
    pabovXX_t this = 0;

    if (work) {
        this = container_of(work, abovXX_t, dworker.work);
        if (!this) {
            LOG_DBG("abovXX_worker_func, NULL abovXX_t\n");
            return;
        }
        if ((!this->get_nirq_low) || (!this->get_nirq_low(this->board->irq_gpio))) {
            /* only run if nirq is high */
            abovXX_process_interrupt(this, 0);
        }
    } else {
        LOG_INFO("abovXX_worker_func, NULL work_struct\n");
    }
}
static irqreturn_t abovXX_interrupt_thread(int irq, void *data)
{
    pabovXX_t this = 0;
    this = data;

    mutex_lock(&this->mutex);
    LOG_INFO("abovXX_irq\n");
    if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
        abovXX_process_interrupt(this, 1);
    else
        LOG_DBG("abovXX_irq - nirq read high\n");
    mutex_unlock(&this->mutex);
    return IRQ_HANDLED;
}
#else
static void abovXX_schedule_work(pabovXX_t this, unsigned long delay)
{
    unsigned long flags;

    if (this) {
        LOG_INFO("abovXX_schedule_work()\n");
        spin_lock_irqsave(&this->lock, flags);
        /* Stop any pending penup queues */
        cancel_delayed_work(&this->dworker);
        /*
         *after waiting for a delay, this put the job in the kernel-global
         workqueue. so no need to create new thread in work queue.
         */
        schedule_delayed_work(&this->dworker, delay);
        spin_unlock_irqrestore(&this->lock, flags);
    } else
        LOG_DBG("abovXX_schedule_work, NULL pabovXX_t\n");
}

static irqreturn_t abovXX_irq(int irq, void *pvoid)
{
    pabovXX_t this = 0;
    if (pvoid) {
        this = (pabovXX_t)pvoid;
        LOG_INFO("abovXX_irq\n");
        if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio)) {
            LOG_INFO("abovXX_irq - Schedule Work\n");
            abovXX_schedule_work(this, 0);
        } else
            LOG_INFO("abovXX_irq - nirq read high\n");
    } else
        LOG_INFO("abovXX_irq, NULL pvoid\n");
    return IRQ_HANDLED;
}

static void abovXX_worker_func(struct work_struct *work)
{
    pabovXX_t this = 0;
    int status = 0;
    u8 nirqLow = 0;

    if (work) {
        this = container_of(work, abovXX_t, dworker.work);

        if (!this) {
            LOG_INFO("abovXX_worker_func, NULL abovXX_t\n");
            return;
        }
        if (unlikely(this->useIrqTimer)) {
            if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
                nirqLow = 1;
        }
        /* since we are not in an interrupt don't need to disable irq. */
        status = this->refreshStatus(this);
        LOG_INFO("Worker - Refresh Status %d\n", status);
        this->statusFunc[0](this);
        if (unlikely(this->useIrqTimer && nirqLow)) {
            /* Early models and if RATE=0 for newer models require a penup timer */
            /* Queue up the function again for checking on penup */
            abovXX_schedule_work(this, msecs_to_jiffies(this->irqTimeout));
        }
    } else {
        LOG_INFO("abovXX_worker_func, NULL work_struct\n");
    }
}
#endif

/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 start*/
void abovXX_suspend(pabovXX_t this)
{
    if (0) {
        LOG_INFO("ABOV suspend: disable irq!\n");
        disable_irq(this->irq);
        write_register(this, ABOV_CTRL_MODE_REG, 0x01);
    }
}
void abovXX_resume(pabovXX_t this)
{
    if (0) {
        LOG_INFO("ABOV resume: enable irq!\n");
        enable_irq(this->irq);
        write_register(this, ABOV_CTRL_MODE_REG, 0x00);
        this->statusFunc[0](this);
    }
}
/*HS60 code for SR-ZQL1695-01-37 by zhuqiang at 2019/08/16 end*/

int abovXX_sar_init(pabovXX_t this)
{
    int err = 0;

    if (this && this->pDevice) {
#ifdef USE_THREADED_IRQ

        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

        /* initialize mutex */
        mutex_init(&this->mutex);
        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_threaded_irq(this->irq, NULL, abovXX_interrupt_thread,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT, this->pdev->driver->name,
                this);
#else
        /* initialize spin lock */
        spin_lock_init(&this->lock);

        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

        /* initailize interrupt reporting */
        this->irq_disabled = 0;
        err = request_irq(this->irq, abovXX_irq, IRQF_TRIGGER_FALLING,
                this->pdev->driver->name, this);
#endif
        if (err) {
            LOG_DBG("irq %d busy?\n", this->irq);
            return err;
        }
#ifdef USE_THREADED_IRQ
        LOG_DBG("registered with threaded irq (%d)\n", this->irq);
#else
        LOG_DBG("registered with irq (%d)\n", this->irq);
#endif
        disable_irq(this->irq);
        /* call init function pointer (this should initialize all registers */
        if (this->init)
            return this->init(this);
        LOG_DBG("No init function!!!!\n");
    }
    return -ENOMEM;
}

int abovXX_sar_remove(pabovXX_t this)
{
    if (this) {
        cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
        free_irq(this->irq, this);
        kfree(this);
        return 0;
    }
    return -ENOMEM;
}
