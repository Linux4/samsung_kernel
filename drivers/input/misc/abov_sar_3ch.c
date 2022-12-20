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
#define USE_SENSORS_CLASS
#define WT_ADD_SAR_HARDWARE_INFO 1

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/input/abov_sar_3ch.h> /* main struct, interrupt,init,pointers */
#if WT_ADD_SAR_HARDWARE_INFO
#include <linux/hardware_info.h>
#endif
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#define SLEEP(x)	mdelay(x)

#define C_I2C_FIFO_SIZE 8

#define I2C_MASK_FLAG	(0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;

#define IDLE 0
#define ACTIVE 1
#define S_PROX   1
#define S_BODY   2

#define ABOV_DEBUG 1
#define LOG_TAG "ABOV-->"

#if ABOV_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...)	pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG fmt, ##args)

//work mode reg : 0x07
//mode : 0x00 active
//     : 0x01 sleep
//     : 0x02 stoop

static int mEnabled;
static int programming_done;
pabovXX_t abov_sar_ptr;

#define TEMPORARY_HOLD_TIME	500
struct wakeup_source wakesrc;

/**
 * struct abov
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct abov {
    pbuttonInformation_t pbuttonInformation;
    pabov_platform_data_t hw; /* specific platform data settings */
} abov_t, *pabov_t;

static void ForcetoTouched(pabovXX_t this)
{
    pabov_t pDevice = NULL;
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;

    pDevice = this->pDevice;
    if (this && pDevice) {
        LOG_INFO("ForcetoTouched()\n");

        pCurrentButton = pDevice->pbuttonInformation->buttons;
        input_bottom = pDevice->pbuttonInformation->input_bottom;
        input_topleft = pDevice->pbuttonInformation->input_topleft;
        input_topright = pDevice->pbuttonInformation->input_topright;
        pCurrentButton->state = ACTIVE;
        if (mEnabled) {
            input_report_rel(input_bottom, REL_MISC, 1);
            input_sync(input_bottom);
             input_report_rel(input_topleft, REL_MISC, 1);
            input_sync(input_topleft);
            input_report_rel(input_topright, REL_MISC, 1);
            input_sync(input_topright);
        }
        LOG_INFO("Leaving ForcetoTouched()\n");
    }
}

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
        LOG_INFO("write_register Addr: 0x%02x Val: 0x%02x Return: %d\n",
                address, value, returnValue);
    }
    if (returnValue < 0) {
        ForcetoTouched(this);
        LOG_DBG("Write_register-ForcetoTouched()\n");
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
        LOG_INFO("read_register Addr: 0x%02x Return: 0x%02x\n",
                address, returnValue);
        if (returnValue >= 0) {
            *value = returnValue;
            return 0;
        } else {
            return returnValue;
        }
    }
    ForcetoTouched(this);
    LOG_INFO("read_register-ForcetoTouched()\n");
    return -ENOMEM;
}



/**
 * brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u8 reg_value = 0;
    pabovXX_t this = dev_get_drvdata(dev);

    LOG_INFO("Reading IRQSTAT_REG\n");
    read_register(this, ABOV_IRQSTAT_REG, &reg_value);
    return scnprintf(buf, PAGE_SIZE, "%d\n", reg_value);
}

/* brief sysfs store function for manual calibration */
static ssize_t calibration_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = dev_get_drvdata(dev);
    unsigned long val;

    if (kstrtoul(buf, 0, &val))
        return -EINVAL;
    if (val) {
        LOG_INFO("Performing manual_offset_calibration()\n");
         write_register(this, ABOV_RECALI_REG, 0x01);
    }
    return count;
}

static DEVICE_ATTR(calibration, 0644, calibration_show,calibration_store);
static struct attribute *abov_attributes[] = {
    &dev_attr_calibration.attr,
    NULL,
};
static struct attribute_group abov_attr_group = {
    .attrs = abov_attributes,
};


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
    LOG_INFO("Inside hw_init().\n");
    if(this) {
        pDevice = this->pDevice;
        pdata = pDevice->hw;
    }
    if (this && pDevice && pdata) {
        while (i < pdata->i2c_reg_num) {
            /* Write all registers/values contained in i2c_reg */
            write_register(this, pdata->pi2c_reg[i].reg,
                    pdata->pi2c_reg[i].val);
            i++;
        }
    } else {
        LOG_DBG("ERROR! platform data err\n");
        /* Force to touched if error */
        ForcetoTouched(this);
        LOG_INFO("Hardware_init-ForcetoTouched()\n");
    }
    LOG_INFO("Exiting hw_init().\n");
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
        LOG_INFO("Inside initialize().\n");
        /* prepare reset by disabling any irq handling */
        if(this->irq_disabled == 0) {
           disable_irq(this->irq);
           this->irq_disabled = 1;
        }	
        /*init register*/
        hw_init(this);
        msleep(300); /* make sure everything is running */
        /* re-enable interrupt handling */
        if(this->irq_disabled == 1) {
           enable_irq(this->irq);
           this->irq_disabled = 0;
        }		
        LOG_INFO("Exiting initialize().\n");
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
    int counter = 0;
    u8 i = 0;
    int numberOfButtons = 0;
    pabov_t pDevice = NULL;
    struct _buttonInfo *buttons = NULL;
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;	
    struct _buttonInfo *pCurrentButton  = NULL;
    struct abov_platform_data *board;

    pDevice = this->pDevice;
    board = this->board;
    if (this && pDevice) {
        LOG_INFO("Inside touchProcess().\n");
        read_register(this, ABOV_IRQSTAT_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        input_bottom = pDevice->pbuttonInformation->input_bottom;
        input_topleft = pDevice->pbuttonInformation->input_topleft;
        input_topright = pDevice->pbuttonInformation->input_topright;
        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely((buttons == NULL) || (input_bottom == NULL) || (input_topleft == NULL) || (input_topright == NULL))) {
            LOG_DBG("ERROR!! buttons or input NULL!!!\n");
            return;
        }
#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
            return;
        }
#endif
        for (counter = 0; counter < numberOfButtons; counter++) {
            pCurrentButton = &buttons[counter];
            if (pCurrentButton == NULL) {
                LOG_DBG("ERR!current button index: %d NULL!\n",
                        counter);
                return; /* ERRORR!!!! */
            }
            switch (pCurrentButton->state) {
            case IDLE: /* Button is being in far state! */
                if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x15)) {
                    LOG_INFO("CH%d State=PROX.\n",counter);
                    if ((board->cap_channel_ch0 == counter) && (this->channel_status & 0x01)) {
                        input_report_rel(input_bottom, REL_MISC, 1);
                        input_sync(input_bottom);
                    } else if ((board->cap_channel_ch1 == counter) && (this->channel_status & 0x02)) {
                        input_report_rel(input_topright, REL_MISC, 1);
                        input_sync(input_topright);
                    } else if ((board->cap_channel_ch2 == counter) && (this->channel_status & 0x04)) {
                        input_report_rel(input_topleft, REL_MISC, 1);
                        input_sync(input_topleft);
                    }
                    pCurrentButton->state = S_PROX;
                } else {
                    LOG_INFO("CH%d still in IDLE State.\n",counter);
                }
                break;
            case S_PROX: /* Button is being in proximity! */
                if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x15)) {
                    LOG_INFO("CH%d still in PROX State.\n",counter);
                } else{
                    LOG_INFO("CH%d State=IDLE.\n",counter);
                    if ((board->cap_channel_ch0 == counter) && (this->channel_status & 0x01)) {
                        input_report_rel(input_bottom, REL_MISC, 2);
                        input_sync(input_bottom);
                    } else if ((board->cap_channel_ch1 == counter) && (this->channel_status & 0x02)) {
                        input_report_rel(input_topright, REL_MISC, 2);
                        input_sync(input_topright);
                    } else if ((board->cap_channel_ch2 == counter) && (this->channel_status & 0x04)) {
                        input_report_rel(input_topleft, REL_MISC, 2);
                        input_sync(input_topleft);
                    }
                    pCurrentButton->state = IDLE;
                }
                break;
            default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
                break;
            };
        }
        LOG_INFO("Exiting touchProcess().\n");
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

static void abov_platform_data_of_init(struct i2c_client *client,
        pabov_platform_data_t pplatData)
{
    struct device_node *np = client->dev.of_node;
    u32 cap_channel_ch0, cap_channel_ch1, cap_channel_ch2;
    int ret;
	client->irq = of_get_named_gpio_flags(np, "abov,irq-gpio",
                      0, NULL);
    //client->irq = of_get_gpio(np, 0);
    pplatData->irq_gpio = client->irq;
    ret = of_property_read_u32(np, "cap,use_channel_ch0", &cap_channel_ch0);
    ret = of_property_read_u32(np, "cap,use_channel_ch1", &cap_channel_ch1);
    ret = of_property_read_u32(np, "cap,use_channel_ch2", &cap_channel_ch2);	
    pplatData->cap_channel_ch0 = (int)cap_channel_ch0;
    pplatData->cap_channel_ch1 = (int)cap_channel_ch1;
    pplatData->cap_channel_ch2 = (int)cap_channel_ch2;
    LOG_INFO("cap_channel_ch0 = %d,cap_channel_ch1 = %d,cap_channel_ch2 = %d\n",
        cap_channel_ch0, cap_channel_ch1, cap_channel_ch2);

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

    ret = of_property_read_string(np, "label", &pplatData->fw_name);
    if (ret < 0) {
        LOG_DBG("firmware name read error!\n");
        return;
    }

    ret = of_property_read_string(np, "label_New", &pplatData->fw_name_New);
    if (ret < 0) {
        LOG_DBG("firmware name read error!\n");
        return;
    }

    LOG_INFO("firmware name: %s\n", pplatData->fw_name);
    LOG_INFO("firmware name: %s\n", pplatData->fw_name_New);
}

static ssize_t calibrate_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    return snprintf(buf, 8, "%d\n", programming_done);
}

static ssize_t calibrate_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;
    int ret = 0;

    pDevice = this->pDevice;
    input_bottom = pDevice->pbuttonInformation->input_bottom;
    input_topleft = pDevice->pbuttonInformation->input_topleft;
    input_topright = pDevice->pbuttonInformation->input_topright;

    LOG_INFO("going to calibrate cap sensor\n");
    if (!count || (this == NULL))
        return -EINVAL;

    if (!strncmp(buf, "1", 1))
        ret = write_register(this, ABOV_RECALI_REG, 0x01);
    if (ret < 0)
        LOG_DBG("calibrate cap sensor failed\n");

    input_report_rel(input_bottom, REL_MISC, 2);
    input_sync(input_bottom);
    input_report_rel(input_topleft, REL_MISC, 2);
    input_sync(input_topleft);
    input_report_rel(input_topright, REL_MISC, 2);
    input_sync(input_topright);	
    return count;
}

static CLASS_ATTR_RW(calibrate);

static ssize_t enable_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t enable_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;

    pDevice = this->pDevice;
    input_bottom = pDevice->pbuttonInformation->input_bottom;
    input_topleft = pDevice->pbuttonInformation->input_topleft;
    input_topright = pDevice->pbuttonInformation->input_topright;

    if (!count || (this == NULL))
        return -EINVAL;

    if ((!strncmp(buf, "1", 1)) && (mEnabled == 0)) {
        LOG_DBG("enable cap sensor\n");
        initialize(this);

        input_report_rel(input_bottom, REL_MISC, 1);
        input_sync(input_bottom);
        input_report_rel(input_topleft, REL_MISC, 1);
        input_sync(input_topleft);
        input_report_rel(input_topright, REL_MISC, 1);
        input_sync(input_topright);
        mEnabled = 1;
    } else if ((!strncmp(buf, "0", 1)) && (mEnabled == 1)) {
        LOG_DBG("disable cap sensor\n");

        write_register(this, ABOV_CTRL_MODE_REG, 0x02);

        input_report_rel(input_bottom, REL_MISC, 2);
        input_sync(input_bottom);
        input_report_rel(input_topleft, REL_MISC, 2);
        input_sync(input_topleft);
        input_report_rel(input_topright, REL_MISC, 2);
        input_sync(input_topright);
        mEnabled = 0;
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return count;
}

static CLASS_ATTR_RW(enable);

static ssize_t reg_show(struct class *class,
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

    for (i = 0; i < 0x40; i++) {
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

static ssize_t reg_store(struct class *class,
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

static CLASS_ATTR_RW(reg);

static struct class capsense_class = {
    .name			= "capsense",
    .owner			= THIS_MODULE,
};

//+bug 492320,20191119,gaojingxuan.wt,add,use SS hal
/*********************************************
*FUNCTION
*  enable_bottom
*DESCRIPTION
*  enable func
*PARAMETERS
*  enable  [in]  1:open 0:close
*RETURN
*  0
*********************************************/
static int enable_bottom(unsigned int enable)
{
    struct abovXX * this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_bottom = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_bottom = pDevice->pbuttonInformation->input_bottom;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[0];

    if (enable == 1) {
        LOG_DBG("enable cap sensor bottom\n");

        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
            if(this->first_cali_flag == false) {
                write_register(this, ABOV_RECALI_REG, 0x01);
                msleep(500);
                this->first_cali_flag = true;
            }
#endif

            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
                LOG_DBG("abov enable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 1;
        }

        if(!(this->channel_status & 0x01)) {
            this->channel_status |= 0x01; //bit0 for ch0 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
        } else {
#endif
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x15)) {
                input_report_rel(input_bottom, REL_MISC, 1);
                input_sync(input_bottom);
                pCurrentButton->state = S_PROX;
            } else {
                input_report_rel(input_bottom, REL_MISC, 2);
                input_sync(input_bottom);
                pCurrentButton->state = IDLE;
            }
#if defined(CONFIG_SENSORS)
        }
#endif
        touchProcess(this);

        //mEnabled = 1;
    } else if (enable == 0) {
        LOG_DBG("disable cap sensor bottom\n");

        if(this->channel_status & 0x01) {
            this->channel_status &= ~0x01;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);

            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
                LOG_DBG("abov disable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 0;
        }
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

	return 0;
}
/*********************************************
*FUNCTION
*  enable_topright
*DESCRIPTION
*  enable func
*PARAMETERS
*  enable  [in]  1:open 0:close
*RETURN
*  0
*********************************************/
static int enable_topright(unsigned int enable)
{
    struct abovXX * this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_topright = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_topright = pDevice->pbuttonInformation->input_topright;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[1];

    if (enable == 1) {
        LOG_DBG("enable cap sensor topright\n");

        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
            if(this->first_cali_flag == false) {
                write_register(this, ABOV_RECALI_REG, 0x01);
                msleep(500);
                this->first_cali_flag = true;
            }
#endif

            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
                LOG_DBG("abov enable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 1;
        }

        if(!(this->channel_status & 0x02)) {
            this->channel_status |= 0x02; //bit1 for ch1 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
        } else {
#endif
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x15)) {
                input_report_rel(input_topright, REL_MISC, 1);
                input_sync(input_topright);
                pCurrentButton->state = S_PROX;
            } else {
                input_report_rel(input_topright, REL_MISC, 2);
                input_sync(input_topright);
                pCurrentButton->state = IDLE;
            }
#if defined(CONFIG_SENSORS)
        }
#endif
        touchProcess(this);

    } else if (enable == 0) {
        LOG_DBG("disable cap sensor top\n");

        if(this->channel_status & 0x02) {
            this->channel_status &= ~0x02;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);

            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
                LOG_DBG("abov disable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

            mEnabled = 0;
        }
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return 0;
}
/*********************************************
*FUNCTION
*  enable_topleft
*DESCRIPTION
*  enable func
*PARAMETERS
*  enable  [in]  1:open 0:close
*RETURN
*  0
*********************************************/
static int enable_topleft(unsigned int enable)
{
    struct abovXX * this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_topleft = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_topleft = pDevice->pbuttonInformation->input_topleft;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[2];

    if (enable == 1) {
        LOG_DBG("enable cap sensor topleft\n");

        if(this->channel_status == 0){
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
#ifdef FACTORY_VERSION
            write_register(this, ABOV_RECALI_REG, 0x01);
            msleep(500);
#else
            if(this->first_cali_flag == false) {
                write_register(this, ABOV_RECALI_REG, 0x01);
                msleep(500);
                this->first_cali_flag = true;
            }
#endif
            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
                LOG_DBG("abov enable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x04)){
            this->channel_status |= 0x04;
        }

        read_register(this, ABOV_IRQSTAT_REG, &i);

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
        } else {
#endif
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x15)) {
                input_report_rel(input_topleft, REL_MISC, 1);
                input_sync(input_topleft);
                pCurrentButton->state = S_PROX;
            } else {
                input_report_rel(input_topleft, REL_MISC, 2);
                input_sync(input_topleft);
                pCurrentButton->state = IDLE;
            }
#if defined(CONFIG_SENSORS)
        }
#endif
        touchProcess(this);
        LOG_DBG("abov enable topleft end channel_status = 0x%x\n",this->channel_status);
    } else if (enable == 0) {
        LOG_DBG("disable cap sensor topleft\n");

        if(this->channel_status & 0x04){
            this->channel_status &= ~0x04;
        }
        LOG_DBG("abov disable topleft channel_status = 0x%x\n",this->channel_status);
        if(this->channel_status == 0){
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);
            //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
                LOG_DBG("abov disable_irq\n");
            }
            //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
            mEnabled = 0;
        }
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return 0;
}

#ifdef USE_SENSORS_CLASS
static int capsensor_set_enable_bottom(struct sensors_classdev *sensors_cdev, unsigned int enable)  //ch0 for main ant
{
    return enable_bottom(enable);
}

static int capsensor_set_enable_topright(struct sensors_classdev *sensors_cdev, unsigned int enable) //ch1 for wifi ant
{
    return enable_topright(enable);
}

static int capsensor_set_enable_topleft(struct sensors_classdev *sensors_cdev, unsigned int enable) //ch2 for div ant
{
    return enable_topleft(enable);
}
#endif
//-bug 492320,20191119,gaojingxuan.wt,add,use SS hal

#if defined(CONFIG_SENSORS)
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
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;

    pDevice = this->pDevice;
    input_bottom = pDevice->pbuttonInformation->input_bottom;
    input_topleft = pDevice->pbuttonInformation->input_topleft;
    input_topright = pDevice->pbuttonInformation->input_topright;

    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        this->skip_data = true;
        if (mEnabled) {
            input_report_rel(pDevice->pbuttonInformation->input_bottom, REL_MISC, 2);
            input_sync(pDevice->pbuttonInformation->input_bottom);
            input_report_rel(pDevice->pbuttonInformation->input_topleft, REL_MISC, 2);
            input_sync(pDevice->pbuttonInformation->input_topleft);
            input_report_rel(pDevice->pbuttonInformation->input_topright, REL_MISC, 2);
            input_sync(pDevice->pbuttonInformation->input_topright);
        }
    } else {
        this->skip_data = false;
    }

    LOG_INFO("%s -%u\n", __func__, val);
    return count;
}

static int abovxx_grip_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_bottom, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_bottom);

    LOG_INFO("%s val=%u\n", __func__, val);
    return 0;
}

static int abovxx_grip_wifi_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_topright, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_topright);

    LOG_INFO("%s -%u\n", __func__, val);
    return 0;
}

static int abovxx_grip_sub_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    pDevice = this->pDevice;

    input_report_rel(pDevice->pbuttonInformation->input_topleft, REL_MAX, val);
    input_sync(pDevice->pbuttonInformation->input_topleft);

    LOG_INFO("%s -%u\n", __func__, val);
    return 0;
}

static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
        abovxx_onoff_show, abovxx_onoff_store);
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
    unsigned char buf[37] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0x7A;
    memcpy(&buf[2], addrH, 1);
    memcpy(&buf[3], addrL, 1);
    memcpy(&buf[4], val, 32);
    ret = _i2c_adapter_block_write(client, buf, length);
    if (ret < 0) {
        LOG_ERR("32 bytes write fail ...\n");
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

    buf[0] = 0xF0;
    buf[1] = 0xAA;

    for (retry_count = 0; retry_count < 10; retry_count++) {
         ret = i2c_master_send(client, buf, 2);
         if (ret < 0)
             LOG_INFO("reset to boot mode retry : %d times!\n", retry_count);
         else 
             break;
    }
    if (retry_count >= 10) {
        LOG_ERR("reset to boot mode fail\n");
        return -EIO;
    }
    
    LOG_INFO("reset to boot mode succ\n");
    return ret;
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
    int ret = 0;
    unsigned char buf[3] = {0, };

    buf[0] = 0xAC;
    buf[1] = 0x5B;
    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        LOG_ERR("Send CMD fail ... addr:%#x CMD:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }
    LOG_INFO("Send CMD succ ... addr:%#x CMD:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
    SLEEP(5);

    ret = i2c_master_recv(client, buf, 1);
    if ((buf[0] != 0x31) && (buf[0] != 0x38) && (buf[0] != 0x39) && (buf[0] != 0x3A) && (buf[0] != 0x3B) && (buf[0] != 0x3C)) {		
        LOG_ERR("Enter fw downloader mode fail ... device id:%#x\n", buf[0]);
        return -EIO;
    }
    LOG_INFO("Enter fw downloader mode succ ... device id:%#x\n", buf[0]);
    
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
        LOG_ERR("Send CMD fail ... addr:%#x CMD:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }
    LOG_INFO("Send CMD succ ... addr:%#x CMD:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

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
        LOG_ERR("Send CMD fail ... addr:%#x CMD:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
        return -EIO;
    }
    LOG_INFO("Send CMD succ ... addr:%#x CMD:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

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
        LOG_ERR("Send CMD fail ... addr:%#x CMD:%#x %#x %#x %#x %#x %#x ... ret:%d\n",
            client->addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ret);
        return -EIO;
    }
    LOG_INFO("Send CMD succ ... addr:%#x CMD:%#x %#x %#x %#x %#x %#x ... ret:%d\n",
        client->addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ret);
    
    SLEEP(5);

    buf[0] = 0x00;
    ret = i2c_master_send(client, buf, 1);
    if (ret != 1) {
        LOG_ERR("Send CMD fail ... addr:%#x CMD:%#x ... ret:%d\n", client->addr, buf[0], ret);
        return -EIO;
    }
    LOG_INFO("Send CMD fail ... addr:%#x CMD:%#x ... ret:%d\n", client->addr, buf[0], ret);
    
    SLEEP(5);

    ret = i2c_master_recv(client, checksum, 6);
    if (ret < 0) {
        LOG_ERR("Read checksum fail ... addr:%#x checksum:%#x %#x %#x %#x %#x %#x ... ret:%d\n",
            client->addr, checksum[0], checksum[1], checksum[2], checksum[3], checksum[4], checksum[5], ret);
        return -EIO;
    }
    LOG_INFO("Read checksum fail ... addr:%#x checksum:%#x %#x %#x %#x %#x %#x ... ret:%d\n",
        client->addr, checksum[0], checksum[1], checksum[2], checksum[3], checksum[4], checksum[5], ret);

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
            LOG_INFO("fw write fail ... fail at %d(32 bytes)\n", ii);
            return ret;
        }
        
        address += 0x20;
        memset(data, 0, 32);
    }

    ret = abov_tk_i2c_read_checksum(client);
    ret = abov_tk_fw_mode_exit(client);
    if ((checksum_h == checksum_h_bin) && (checksum_l == checksum_l_bin)) {
        LOG_INFO("fw update succ ... checksum_h(%#x)=checksum_h_bin(%#x) && checksum_l(%#x)=checksum_l_bin(%#x)\n",
            checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
    } else {
        LOG_INFO("fw update fail ... checksum_h(%#x)!=checksum_h_bin(%#x) && checksum_l(%#x)!=checksum_l_bin(%#x)\n",
            checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
        ret = -1;
    }
    SLEEP(100);

    return ret;
}

extern char *saved_command_line;

static int abov_fw_update(bool force)
{
    int update_loop;
    pabovXX_t this = abov_sar_ptr;
    struct i2c_client *client = this->bus;
    int rc = 0;
    bool fw_upgrade = false;
    u8 fw_version = 0, fw_file_version = 0;
    u8 fw_modelno = 0, fw_file_modeno = 0;
    const struct firmware *fw = NULL;
    char fw_name[32] = {0};

    if (strstr(saved_command_line,"ili7806s_hd_plus_vdo_txd_na")) {
        strlcpy(fw_name, this->board->fw_name_New, NAME_MAX);
    }
    else {
        strlcpy(fw_name, this->board->fw_name, NAME_MAX);
    }
    printk("fw_name is %s", fw_name);
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
    LOG_INFO("fw info:version inside ic=%#x,modelno inside ic=%#x,version inside file=%#x,modelno inside file=%#x\n",
        fw_version, fw_modelno, fw_file_version, fw_file_modeno);
    
    if ((force) || (fw_version < fw_file_version) || (fw_modelno != fw_file_modeno)) {
        fw_upgrade = true;
    } else {
        LOG_INFO("Exiting fw upgrade...\n");
        fw_upgrade = false;
        goto rel_fw;
    }

    if (fw_upgrade) {
        for (update_loop = 0; update_loop < 10; update_loop++) {
            rc = _abov_fw_update(client, &fw->data[32], fw->size-32);
            if (rc < 0)
                LOG_INFO("retry : %d times!\n", update_loop);
            else {
                initialize(this);
                write_register(this, ABOV_CTRL_MODE_REG, 0x02);
                break;
            }
        }
        
        if (update_loop >= 10) {
            LOG_ERR("fw update fail,update_loop=%d\n", update_loop);
            rc = -EIO;
        }		
    }
    
rel_fw:
    release_firmware(fw);
    return rc;
}

static ssize_t update_fw_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 fw_version = 0;
    pabovXX_t this = abov_sar_ptr;

    read_register(this, ABOV_VERSION_REG, &fw_version);

    return snprintf(buf, 16, "ABOV:0x%x\n", fw_version);
}

static ssize_t update_fw_store(struct class *class,
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

    if(this->irq_disabled == 0) {
       disable_irq(this->irq);
       this->irq_disabled = 1;
    }	

    mutex_lock(&this->mutex);
    if (!this->loading_fw  && val) {
        this->loading_fw = true;
        abov_fw_update(false);
        this->loading_fw = false;
    }
    mutex_unlock(&this->mutex);

    if(this->irq_disabled == 1) {
       enable_irq(this->irq);
       this->irq_disabled = 0;
    }		
    
    return count;
}
static CLASS_ATTR_RW(update_fw);

static ssize_t force_update_fw_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 fw_version = 0;
    pabovXX_t this = abov_sar_ptr;

    read_register(this, ABOV_VERSION_REG, &fw_version);

    return snprintf(buf, 16, "ABOV:0x%x\n", fw_version);
}

static ssize_t force_update_fw_store(struct class *class,
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

    if(this->irq_disabled == 0) {
       disable_irq(this->irq);
       this->irq_disabled = 1;
    }		

    mutex_lock(&this->mutex);
    if (!this->loading_fw  && val) {
        this->loading_fw = true;
        abov_fw_update(true);
        this->loading_fw = false;
    }
    mutex_unlock(&this->mutex);

    if(this->irq_disabled == 1) {
       enable_irq(this->irq);
       this->irq_disabled = 0;
    }		

    return count;
}
static CLASS_ATTR_RW(force_update_fw);
//Mtr 2856,20191029,gaojingxuan.wt,add sar-sensor factory test node,begin
static ssize_t reg_ch0_show(struct class *class,
                struct class_attribute *attr,
                char *buf)
{
    u8 reg_value = 0;
    u8 msb_ch0, lsb_ch0 = 0;
    u16 reg_result = 0;
    pabovXX_t this = abov_sar_ptr;
    char *p = buf;

    //CH0 background cap
    read_register(this, 0x18, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x19, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH0_background_cap=%d;", reg_result);

    //CH0 refer channel cap
    read_register(this, 0x1e, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x1f, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH0_refer_channel_cap=%d;", reg_result);

    //CH0 diff
    read_register(this, 0x34, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x35, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH0_diff=%d", reg_result);

    return (p-buf);
}

static ssize_t reg_ch0_store(struct class *class,
        struct class_attribute *attr,
					  const char *buf, size_t count)
{
    return count;
}

static CLASS_ATTR_RW(reg_ch0);

static ssize_t reg_ch1_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 reg_value = 0;
    u8 msb_ch0, lsb_ch0 = 0;
    u16 reg_result = 0;
    pabovXX_t this = abov_sar_ptr;
    char *p = buf;

    //CH1 background cap
    read_register(this, 0x1a, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x1b, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH1_background_cap=%d;", reg_result);

    //CH1 refer channel cap
    read_register(this, 0x20, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x21, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH1_refer_channel_cap=%d;", reg_result);

    //CH1 diff
    read_register(this, 0x36, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x37, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH1_diff=%d", reg_result);

    return (p-buf);
}

static ssize_t reg_ch1_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
    return count;
}

static CLASS_ATTR_RW(reg_ch1);

static ssize_t reg_ch2_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    u8 reg_value = 0;
    u8 msb_ch0, lsb_ch0 = 0;
    u16 reg_result = 0;
    pabovXX_t this = abov_sar_ptr;
    char *p = buf;

    //CH2 background cap
    read_register(this, 0x1c, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x1d, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH2_background_cap=%d;", reg_result);

    //CH2 diff
    read_register(this, 0x38, &reg_value);
    msb_ch0 = reg_value;
    read_register(this, 0x39, &reg_value);
    lsb_ch0 = reg_value;
    reg_result = (msb_ch0 << 8) | lsb_ch0;
    p += snprintf(p, PAGE_SIZE, "CH2_diff=%d", reg_result);

    return (p-buf);
}

static ssize_t reg_ch2_store(struct class *class,
        struct class_attribute *attr,
					  const char *buf, size_t count)
{
    return count;
}

static CLASS_ATTR_RW(reg_ch2);
//Mtr 2856,20191029,gaojingxuan.wt,add sar-sensor factory test node,end


//+bug 492320,20191119,gaojingxuan.wt,add,use SS hal
/*********************************************
*FUNCTION
*  show_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  p-buf
*********************************************/
static ssize_t show_enable(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
    pabovXX_t this = abov_sar_ptr;
    int status;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s\n", __func__,temp_input_dev->name); 

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if(this->channel_status & 0x01)
        {
            status = 1;    
        }else{
            status = 0;
        }
    }
    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if(this->channel_status & 0x02)
        {
            status = 1;    
        }else{
            status = 0;
        }
    }
    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if(this->channel_status & 0x04)
        {
            status = 1;    
        }else{
            status = 0;
        }
    }

    p += snprintf(p, PAGE_SIZE, "%d", status);

	return (p-buf);
}
/*********************************************
*FUNCTION
*  store_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  count
*********************************************/
static ssize_t store_enable(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s:%s\n", __func__,temp_input_dev->name,buf); 

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor:enable\n");
            enable_bottom(1);
        }else{
            LOG_INFO("name:grip_sensor:disable\n");
            enable_bottom(0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_wifi:enable\n");
            enable_topright(1);
        }else{
            LOG_INFO("name:grip_sensor_wifi:disable\n");
            enable_topright(0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_sub:enable\n");
            enable_topleft(1);
        }else{
            LOG_INFO("name:grip_sensor_sub:disable\n");
            enable_topleft(0);
        }
    }

	return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
//-bug 492320,20191119,gaojingxuan.wt,add,use SS hal

static void capsense_update_work(struct work_struct *work)
{
    pabovXX_t this = container_of(work, abovXX_t, fw_update_work.worker);

    LOG_INFO("%s: start update firmware\n", __func__);	
    mutex_lock(&this->mutex);
    this->loading_fw = true;
    abov_fw_update(this->fw_update_work.force_update);
    this->loading_fw = false;
    mutex_unlock(&this->mutex);	

    LOG_INFO("%s: update firmware end\n", __func__);
}

static void capsense_fore_update_work(struct work_struct *work)
{
	pabovXX_t this = container_of(work, abovXX_t, fw_update_work.worker);

	LOG_INFO("%s: start force update firmware\n", __func__);
	mutex_lock(&this->mutex);
	this->loading_fw = true;
	abov_fw_update(true);
	this->loading_fw = false;
	mutex_unlock(&this->mutex);
	LOG_INFO("%s: force update firmware end\n", __func__);
}

/**
 * detect if abov exist or not
 * return 0 if fw exist 
 * return 1 if bootloader exist
 * return 2 if chip no exist
 */
static int abov_detect(struct i2c_client *client)
{
    s32 returnValue = 0, i;
    u8 address = ABOV_VENDOR_ID_REG;
    u8 value = 0xAB;

    if (client) {
        for (i = 0; i < 10; i++) {
            returnValue = i2c_smbus_read_byte_data(client, address);
            if (value == returnValue) {
                LOG_INFO("abov chip id = 0x%x  detect success!\n",returnValue);
                return NORMAL_MODE;
            }
            SLEEP(10);
        }
        for (i = 0; i < 3; i++) {
            if (abov_tk_fw_mode_enter(client) == 0) {
                LOG_INFO("abov boot detect success!\n");
                return BOOTLOADER_MODE;
            }
        }
    }
    LOG_INFO("abov detect failed!!!\n");
    return UNKONOW_MODE;
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
    int ret;
    bool isForceUpdate = false;
    struct input_dev *input_bottom = NULL;
    struct input_dev *input_topleft = NULL;
    struct input_dev *input_topright = NULL;	
#ifdef SAR_USB_CHANGE
    struct power_supply *psy = NULL;
#endif
#if WT_ADD_SAR_HARDWARE_INFO
    u8 fw_version = 0;
    char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
#endif

    LOG_INFO("Inside abov_probe()\n");
    printk("Inside abov_probe()\n");
    pplatData = kzalloc(sizeof(struct abov_platform_data), GFP_KERNEL);
    if (!pplatData) {
        LOG_DBG("platform data is required!\n");
        return -EINVAL;
    }
#if 0
    pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
    if (IS_ERR(pplatData->cap_vdd)) {
        if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
            ret = PTR_ERR(pplatData->cap_vdd);
            goto err_vdd_defer;
        }
        LOG_INFO("%s: Failed to get regulator\n", __func__);
    } else {
        ret = regulator_enable(pplatData->cap_vdd);

        if (ret) {
            regulator_put(pplatData->cap_vdd);
            LOG_DBG("%s: Error %d enable regulator\n",
                    __func__, ret);
            goto err_vdd_defer;
        }
        pplatData->cap_vdd_en = true;
        LOG_INFO("cap_vdd regulator is %s\n",
                regulator_is_enabled(pplatData->cap_vdd) ?
                "on" : "off");
    }

    pplatData->cap_svdd = regulator_get(&client->dev, "cap_svdd");
    if (!IS_ERR(pplatData->cap_svdd)) {
        ret = regulator_enable(pplatData->cap_svdd);
        if (ret) {
            regulator_put(pplatData->cap_svdd);
            LOG_INFO("Failed to enable cap_svdd\n");
            goto err_svdd_error;
        }
        pplatData->cap_svdd_en = true;
        LOG_INFO("cap_svdd regulator is %s\n",
                regulator_is_enabled(pplatData->cap_svdd) ?
                "on" : "off");
    } else {
        ret = PTR_ERR(pplatData->cap_vdd);
        if (ret == -EPROBE_DEFER)
            goto err_svdd_error;
    }
#endif
    /* detect if abov exist or not */
    ret = abov_detect(client);
    if (ret == UNKONOW_MODE) {
        ret = -ENODEV;
        goto err_vdd_defer;
    } else if (ret == BOOTLOADER_MODE) {
        LOG_INFO("abov enter boot mode\n");
        isForceUpdate = true;
    }
    abov_platform_data_of_init(client, pplatData);
    client->dev.platform_data = pplatData;

    if (!i2c_check_functionality(client->adapter,
                I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        ret = -EIO;
        goto err_vdd_defer;
    }

    this = kzalloc(sizeof(abovXX_t), GFP_KERNEL);
    
    if (this) {
        this->sar_pinctl = devm_pinctrl_get(&client->dev);
        if (IS_ERR_OR_NULL(this->sar_pinctl)) {
            ret = PTR_ERR(this->sar_pinctl);
            LOG_INFO("abov Target does not use pinctrl %d\n", ret);
        }

        this->pinctrl_state_default
            = pinctrl_lookup_state(this->sar_pinctl, "abov_int_default");
        if (IS_ERR_OR_NULL(this->pinctrl_state_default)) {
            ret = PTR_ERR(this->pinctrl_state_default);
            LOG_INFO("abov Can not lookup %s pinstate %d\n","abov_int_default", ret);
        }
        wakeup_source_add(&wakesrc);
  //      wakeup_source_init(&wakesrc, "abov");
        /* In case we need to reinitialize data
         * (e.q. if suspend reset device) */
        this->init = initialize;
        /* pointer to function from platform data to get pendown
         * (1->NIRQ=0, 0->NIRQ=1) */
        this->get_nirq_low = pplatData->get_is_nirq_low;
        /* save irq in case we need to reference it */
        this->irq = gpio_to_irq(client->irq);
        /* do we need to create an irq timer after interrupt ? */
        this->useIrqTimer = 0;
        this->board = pplatData;
        /* Setup function to call on corresponding reg irq source bit */
        this->statusFunc = touchProcess; /* RELEASE_DETECT_STAT */
        /* setup i2c communication */
        this->bus = client;
        i2c_set_clientdata(client, this);

        /* record device struct */
        this->pdev = &client->dev;

        /* create memory for device specific struct */
        this->pDevice = pDevice = kzalloc(sizeof(abov_t), GFP_KERNEL);

        if (pDevice) {
            LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n",
                pDevice);
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
            input_bottom = input_allocate_device(); //ch0-->main->bottom
            if (!input_bottom) {
                ret = -ENOMEM;
                goto err_ch0_alloc;
            }

            /* Set all the keycodes */
            input_set_capability(input_bottom, EV_REL, REL_MISC);
            input_set_capability(input_bottom, EV_REL, REL_MAX);
            /* save the input pointer and finish initialization */
            pDevice->pbuttonInformation->input_bottom = input_bottom;
            input_bottom->name = "grip_sensor";
            if (input_register_device(input_bottom)) {
                LOG_INFO("add ch0 cap touch unsuccess\n");
                input_free_device(input_bottom);
                ret = -ENOMEM;
                goto err_ch0_register;
            }
            /* Create the input device */
            input_topleft = input_allocate_device(); //ch1-->wifi->top
            if (!input_topleft) {
                ret = -ENOMEM;
                goto err_ch1_alloc;
            }
            /* Set all the keycodes */
            input_set_capability(input_topleft, EV_REL, REL_MISC);
            input_set_capability(input_topleft, EV_REL, REL_MAX);
            /* save the input pointer and finish initialization */
            pDevice->pbuttonInformation->input_topleft = input_topleft;
            /* save the input pointer and finish initialization */
            input_topleft->name = "grip_sensor_sub";
            if (input_register_device(input_topleft)) {
                LOG_INFO("add ch1 cap touch unsuccess\n");
                input_free_device(input_topleft);
                ret = -ENOMEM;
                goto err_ch1_register;
            }
            /* Create the input device */
            input_topright = input_allocate_device(); //ch2-->div->div
            if (!input_topright) {
                ret = -ENOMEM;
                goto err_ch2_alloc;
            }
            /* Set all the keycodes */
            input_set_capability(input_topright, EV_REL, REL_MISC);
            input_set_capability(input_topright, EV_REL, REL_MAX);
            /* save the input pointer and finish initialization */
            pDevice->pbuttonInformation->input_topright = input_topright;
            /* save the input pointer and finish initialization */
            input_topright->name = "grip_sensor_wifi";
            if (input_register_device(input_topright)) {
                LOG_INFO("add ch2 cap touch unsuccess\n");
                input_free_device(input_topright);
                ret = -ENOMEM;
                goto err_ch2_register;
            }
            input_report_rel(input_topleft, REL_MISC, 2);
            input_sync(input_topleft);
            input_report_rel(input_topright, REL_MISC, 2);
            input_sync(input_topright);
            input_report_rel(input_bottom, REL_MISC, 2);
            input_sync(input_bottom);
        } else {
            ret = -ENOMEM;
            goto err_pDevice;
        }

        ret = class_register(&capsense_class);
        if (ret < 0) {
            LOG_DBG("Create fsys class failed (%d)\n", ret);
            goto err_class_register;
        }

        ret = class_create_file(&capsense_class, &class_attr_calibrate);
        if (ret < 0) {
            LOG_DBG("Create reset file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_enable);
        if (ret < 0) {
            LOG_DBG("Create enable file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_reg);
        if (ret < 0) {
            LOG_DBG("Create reg file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_update_fw);
        if (ret < 0) {
            LOG_DBG("Create update_fw file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_force_update_fw);
        if (ret < 0) {
            LOG_DBG("Create force_update_fw file failed (%d)\n", ret);
            goto err_class_creat;
        }
        //Mtr 2856,20191029,gaojingxuan.wt,add sar-sensor factory test node,begin
        ret = class_create_file(&capsense_class, &class_attr_reg_ch0);
        if (ret < 0) {
            LOG_DBG("Create reg_ch0 file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_reg_ch1);
        if (ret < 0) {
            LOG_DBG("Create reg_ch1 file failed (%d)\n", ret);
            goto err_class_creat;
        }

        ret = class_create_file(&capsense_class, &class_attr_reg_ch2);
        if (ret < 0) {
            LOG_DBG("Create reg_ch2 file failed (%d)\n", ret);
            goto err_class_creat;
        }
        //Mtr 2856,20191029,gaojingxuan.wt,add sar-sensor factory test node,end
#ifdef USE_SENSORS_CLASS
        sensors_capsensor_bottom_cdev.sensors_enable = capsensor_set_enable_bottom;
        sensors_capsensor_bottom_cdev.sensors_poll_delay = NULL;
        sensors_capsensor_bottom_cdev.sensors_flush = abovxx_grip_flush;
        ret = sensors_classdev_register(&input_bottom->dev, &sensors_capsensor_bottom_cdev);
        if (ret < 0)
            LOG_DBG("create top ch0 sensor_class  file failed (%d)\n", ret);
        sensors_capsensor_topleft_cdev.sensors_enable = capsensor_set_enable_topleft;
        sensors_capsensor_topleft_cdev.sensors_poll_delay = NULL;
        sensors_capsensor_topleft_cdev.sensors_flush = abovxx_grip_sub_flush;
        ret = sensors_classdev_register(&input_topleft->dev, &sensors_capsensor_topleft_cdev);
        if (ret < 0)
            LOG_DBG("create chi cap sensor_class  file failed (%d)\n", ret);
        sensors_capsensor_topright_cdev.sensors_enable = capsensor_set_enable_topright;
        sensors_capsensor_topright_cdev.sensors_poll_delay = NULL;
        sensors_capsensor_topright_cdev.sensors_flush = abovxx_grip_wifi_flush;
        ret = sensors_classdev_register(&input_topright->dev, &sensors_capsensor_topright_cdev);
        if (ret < 0)
            LOG_DBG("create ch2 cap sensor_class file failed (%d)\n", ret);
#endif
        //+bug 492320,20191119,gaojingxuan.wt,add,use SS hal
        device_create_file(&input_bottom->dev, &dev_attr_enable);
        device_create_file(&input_topleft->dev, &dev_attr_enable);
        device_create_file(&input_topright->dev, &dev_attr_enable);
        //-bug 492320,20191119,gaojingxuan.wt,add,use SS hal
#if defined(CONFIG_SENSORS)
        device_create_file(sensors_capsensor_bottom_cdev.dev, &dev_attr_onoff);
#endif
        abovXX_sar_init(this);

        write_register(this, ABOV_CTRL_MODE_REG, 0x02);

        this->channel_status = 0;

#ifdef SAR_USB_CHANGE
		INIT_WORK(&this->ps_notify_work, ps_notify_callback_work);
		this->ps_notif.notifier_call = ps_notify_callback;
		ret = power_supply_reg_notifier(&this->ps_notif);
		if (ret) {
			LOG_DBG(
				"Unable to register ps_notifier: %d\n", ret);
			goto free_ps_notifier;
		}

		psy = power_supply_get_by_name("usb");
		if (psy) {
			ret = ps_get_state(psy, &this->ps_is_present);
			if (ret) {
				LOG_DBG(
					"psy get property failed rc=%d\n",
					ret);
				goto free_ps_notifier;
			}
		}
#endif

#if WT_ADD_SAR_HARDWARE_INFO
        read_register(this, ABOV_VERSION_REG, &fw_version);
        snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"A96T346DFP,FW:0x%x",fw_version);
		hardwareinfo_set_prop(HARDWARE_SAR,firmware_ver);
#endif

		this->loading_fw = false;
		if (isForceUpdate == true) {	  
		    INIT_WORK(&this->fw_update_work.worker, capsense_fore_update_work);
		} else {
            INIT_WORK(&this->fw_update_work.worker, capsense_update_work);
		}
        schedule_work(&this->fw_update_work.worker);

        LOG_INFO("Exiting abov_probe()\n");
        return 0;
    }
    ret =  -ENOMEM;
    goto err_vdd_defer;

#ifdef SAR_USB_CHANGE
free_ps_notifier:
    power_supply_unreg_notifier(&this->ps_notif);
#endif
err_class_creat:
    class_unregister(&capsense_class);
err_class_register:
    input_unregister_device(input_topright);
err_ch2_register:
err_ch2_alloc:
    input_unregister_device(input_topleft);
err_ch1_register:
err_ch1_alloc:
    input_unregister_device(input_bottom);
err_ch0_register:
err_ch0_alloc:
    kfree(this->pDevice);
err_pDevice:
    kfree(this);
#if 0
err_this_device:
    LOG_DBG("%s device this defer.\n", __func__);
    regulator_disable(pplatData->cap_svdd);
    regulator_put(pplatData->cap_svdd);

err_svdd_error:
    LOG_DBG("%s svdd defer.\n", __func__);
    regulator_disable(pplatData->cap_vdd);
    regulator_put(pplatData->cap_vdd);
#endif
err_vdd_defer:
    LOG_DBG("%s free pplatData.\n", __func__);
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
#ifdef USE_SENSORS_CLASS
		sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
		sensors_classdev_unregister(&sensors_capsensor_topleft_cdev);
		sensors_classdev_unregister(&sensors_capsensor_topright_cdev);
#endif
        input_unregister_device(pDevice->pbuttonInformation->input_bottom);
        input_unregister_device(pDevice->pbuttonInformation->input_topleft);
        input_unregister_device(pDevice->pbuttonInformation->input_topright);

        if (this->board->cap_svdd_en) {
            regulator_disable(this->board->cap_svdd);
            regulator_put(this->board->cap_svdd);
        }

        if (this->board->cap_vdd_en) {
            regulator_disable(this->board->cap_vdd);
            regulator_put(this->board->cap_vdd);
        }
        sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw();
        kfree(this->pDevice);
    }
    return abovXX_sar_remove(this);
}

/***** Kernel Suspend *****/
static int abov_suspend(struct device *dev)
{
  struct i2c_client *client = to_i2c_client(dev);
  struct abovXX * this = i2c_get_clientdata(client);

  LOG_INFO("%s start.\n", __func__);

  if(this->irq_disabled == 0){
      enable_irq_wake(this->irq);
  }
  if(mEnabled == 1) {
      write_register(this, ABOV_CTRL_MODE_REG, 0x01);
  } else {
      write_register(this, ABOV_CTRL_MODE_REG, 0x02);
  }


  LOG_INFO("%s end.\n", __func__);

  return 0;
}

/***** Kernel Resume *****/
static int abov_resume(struct device *dev)
{
   struct i2c_client *client = to_i2c_client(dev);
   struct abovXX * this = i2c_get_clientdata(client);

   LOG_INFO("%s start.\n", __func__);

   if(this->irq_disabled == 0){
	 disable_irq_wake(this->irq);
   }
  if(mEnabled == 1) {
      write_register(this, ABOV_CTRL_MODE_REG, 0x00);
  } else {
      write_register(this, ABOV_CTRL_MODE_REG, 0x02);
  }

   LOG_INFO("%s end.\n", __func__);

   return 0;
}
#if 0
static int abov_suspend(struct i2c_client *client, pm_message_t mesg)
{
    pabovXX_t this = i2c_get_clientdata(client);
    LOG_INFO("ABOV suspend !\n");
    if (mEnabled == 1) {
        write_register(this, ABOV_CTRL_MODE_REG, 0x01);
    }	
    return 0;
}

static int abov_resume(struct i2c_client *client)
{
    pabovXX_t this = i2c_get_clientdata(client);
    LOG_INFO("ABOV resume !\n");
    if (mEnabled == 1) {
        write_register(this, ABOV_CTRL_MODE_REG, 0x00);
    }
    return 0;
}
#endif  
static SIMPLE_DEV_PM_OPS(abov_pm_ops, abov_suspend, abov_resume);


#ifdef CONFIG_OF
static const struct of_device_id abov_match_tbl[] = {
    { .compatible = "abov,abov_sar" },
    { },
};
MODULE_DEVICE_TABLE(of, abov_match_tbl);
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
        .pm     = &abov_pm_ops,		 
    },
    .id_table = abov_idtable,
    .probe	  = abov_probe,
    .remove	  = abov_remove,
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

static void abovXX_process_interrupt(pabovXX_t this, u8 nirqlow)
{

    if (!this) {
        pr_err("abovXX_process_interrupt, NULL abovXX_t\n");
        return;
    }
    /* since we are not in an interrupt don't need to disable irq. */
    this->statusFunc(this);
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

    printk("abov abovXX_worker_func");
    if (work) {
        this = container_of(work, abovXX_t, dworker.work);
        if (!this) {
            LOG_DBG("abovXX_worker_func, NULL abovXX_t\n");
            return;
        }
        if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio)) {
            /* only run if nirq is high */
            abovXX_process_interrupt(this, 0);
        }
    } else {
        LOG_INFO("abovXX_worker_func, NULL work_struct\n");
    }
}

#if 0
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
    __pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
    return IRQ_HANDLED;
}
#endif
static irqreturn_t abovXX_interrupt_handler(int irq, void *data)
{
    pabovXX_t this = 0;
    this = data;

    LOG_INFO("abovXX_irq_handler\n");

    schedule_delayed_work(&this->dworker, 0);

    __pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
    return IRQ_HANDLED;
}

int abovXX_sar_init(pabovXX_t this)
{
    int err = 0;

    if (this && this->pDevice) {

        /* initialize worker function */
        INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

        /* initialize mutex */
        mutex_init(&this->mutex);
        /* initailize interrupt reporting */
#if 0
        err = request_threaded_irq(this->irq, NULL, abovXX_interrupt_thread,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT, this->pdev->driver->name,
                this);
#endif
        err = request_irq(this->irq, abovXX_interrupt_handler,
                IRQF_TRIGGER_FALLING | IRQF_ONESHOT, this->pdev->driver->name,
                this);
        disable_irq(this->irq);
        this->irq_disabled = 1;
        if (err) {
            LOG_DBG("irq %d busy?\n", this->irq);
            return err;
        }
        LOG_DBG("registered with threaded irq (%d)\n", this->irq);
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

MODULE_AUTHOR("ABOV Corp.");
MODULE_DESCRIPTION("ABOV Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
