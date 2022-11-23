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

/* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 start */
#define JUDGE_BOARD_ID
/* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 end */
#ifdef JUDGE_BOARD_ID
#include <soc/qcom/smem.h>
u32 pcb_id_code = 0;
#endif

#if defined(CONFIG_SENSORS)
#define VENDOR_NAME      "ABOV"
#define MODEL_NAME       "A96T3X6"
#define MODULE_NAME      "grip_sensor"
#define SUB_MODULE_NAME  "grip_sensor_sub"
#endif

#define SLEEP(x)    mdelay(x)

#define C_I2C_FIFO_SIZE 8

#define I2C_MASK_FLAG    (0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;

#define IDLE    0
#define S_PROX  1
#define S_BODY  2

#define ABOV_DEBUG 1

#define LOG_TAG "[ABOV] "

#if ABOV_DEBUG
#define LOG_INFO(fmt, args...)  pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...)   pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG fmt, ##args)

static int mEnabled = 1;

pabovXX_t abov_sar_ptr;

extern char *sar_name;

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
    LOG_INFO("manual_offset_calibration()\n");
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

/* HS50 code for HS50EU-191 by xiongxiaoliang at 20201021 start */
static int sar_charger_notifier_callback(struct notifier_block *nb,
                                unsigned long val, void *v) {
    int ret = 0;
    struct power_supply *psy = NULL;
    union power_supply_propval prop;

    LOG_INFO("sar_charger_notifier_callback\n");
    psy= power_supply_get_by_name("usb");
    if (!psy) {
        LOG_INFO("Couldn't get usbpsy\n");
        return -EINVAL;
    }

    LOG_INFO("sar_charger_notifier_callback val = %ld\n", val);
    if (!strcmp(psy->desc->name, "usb")) {
        if (psy && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &prop);

            LOG_INFO("power_supply_get_property prop.intval = %d\n", prop.intval);

            if (ret < 0) {
                LOG_INFO("Couldn't get POWER_SUPPLY_PROP_ONLINE rc = %d\n", ret);
                return ret;
            } else {
                if(abov_sar_ptr->usb_plug_status == 2)
                    abov_sar_ptr->usb_plug_status = prop.intval;

                if(abov_sar_ptr->usb_plug_status != prop.intval) {
                    LOG_INFO("usb prop.intval = %d\n", prop.intval);
                    abov_sar_ptr->usb_plug_status = prop.intval;

                    if(abov_sar_ptr->charger_notify_wq != NULL)
                        queue_work(abov_sar_ptr->charger_notify_wq, &abov_sar_ptr->update_charger);
                }
            }
        }
    }
    return 0;
}

static void sar_update_charger(struct work_struct *work)
{
    int ret = 0;

    ret = manual_offset_calibration(abov_sar_ptr);

    if(ret != 2) {
        LOG_INFO("manual_offset_calibration failed\n");
    }
}

void sar_plat_charger_init(void)
{
    int ret = 0;

    abov_sar_ptr->usb_plug_status = 2;
    abov_sar_ptr->charger_notify_wq = create_singlethread_workqueue("sar_charger_wq");

    if (!abov_sar_ptr->charger_notify_wq) {
        LOG_INFO("allocate sar_charger_notify_wq failed\n");
        return;
    }

    INIT_WORK(&abov_sar_ptr->update_charger, sar_update_charger);

    abov_sar_ptr->notifier_charger.notifier_call = sar_charger_notifier_callback;
    ret = power_supply_reg_notifier(&abov_sar_ptr->notifier_charger);

    if (ret < 0)
        LOG_INFO("power_supply_reg_notifier failed\n");
}
/* HS50 code for HS50EU-191 by xiongxiaoliang at 20201021 start */

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
        /* re-enable interrupt handling */
        enable_irq(this->irq);
        enable_irq_wake(this->irq);
        this->irq_disabled = 0;

        /* make sure no interrupts are pending since enabling irq will only
         * work on next falling edge */
        read_regStat(this);
        LOG_INFO("Exiting initialize(). NIRQ = %d\n",
                this->get_nirq_low(this->board->irq_gpio));
#if defined(CONFIG_SENSORS)
        this->skip_data = false;
#endif
        return 0;
    }
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
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;
    struct _buttonInfo *pCurrentButton    = NULL;
    struct abov_platform_data *board;

    pDevice = this->pDevice;
    board = this->board;
    if (this && pDevice) {
        LOG_INFO("Inside touchProcess()\n");
        read_register(this, ABOV_IRQSTAT_LEVEL_REG, &i);

        buttons = pDevice->pbuttonInformation->buttons;
        input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
        input_top_sar = pDevice->pbuttonInformation->input_top_sar;

        numberOfButtons = pDevice->pbuttonInformation->buttonSize;

        if (unlikely((buttons == NULL) || (input_bottom_sar == NULL) || (input_top_sar == NULL))) {
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
                if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
                    LOG_INFO("CH%d State=BODY.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 1);
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = S_BODY;
                } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
                    LOG_INFO("CH%d State=PROX.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 1);
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = S_PROX;
                } else {
                    LOG_INFO("CH%d still in IDLE State.\n", counter);
                }
                break;
            case S_PROX: /* Button is being in proximity! */
                if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
                    LOG_INFO("CH%d State=BODY.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 1);
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = S_BODY;
                } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
                    LOG_INFO("CH%d still in PROX State.\n", counter);
                } else{
                    LOG_INFO("CH%d State=IDLE.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 2);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_FAR, 1);
                        input_report_key(input_top_sar, KEY_SAR_FAR, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 2);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = IDLE;
                }
                break;
            case S_BODY: /* Button is being in 0mm! */
                if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
                    LOG_INFO("CH%d still in BODY State.\n", counter);
                } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
                    LOG_INFO("CH%d State=PROX.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_CLOSE, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 1);
                        input_report_key(input_top_sar, KEY_SAR_CLOSE, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 1);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = S_PROX;
                } else{
                    LOG_INFO("CH%d State=IDLE.\n", counter);
                    if (board->cap_channel_bottom == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 1);
                        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 0);
                        #else
                        input_report_rel(input_bottom_sar, REL_MISC, 2);
                        #endif
                        input_sync(input_bottom_sar);
                    } else if (board->cap_channel_top == counter) {
                        #ifdef HQ_FACTORY_BUILD
                        input_report_key(input_top_sar, KEY_SAR_FAR, 1);
                        input_report_key(input_top_sar, KEY_SAR_FAR, 0);
                        #else
                        input_report_rel(input_top_sar, REL_MISC, 2);
                        #endif
                        input_sync(input_top_sar);
                    }
                    pCurrentButton->state = IDLE;
                }
                break;
            default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
                break;
            };
        }
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

#ifdef JUDGE_BOARD_ID
static void pcb_abov_conf(void)
{
    u32 *pcb_addr = NULL;
    u32 pcb_size = 0;

    pcb_addr = (u32 *)smem_get_entry(SMEM_ID_VENDOR1, &pcb_size, 0, 0);
    if (pcb_addr)
    {
        pcb_id_code = (*pcb_addr & 0x00000ff0) >> 4;
        LOG_INFO("pcb_id_code=0x%x\n", pcb_id_code);
    }
    else
    {
        LOG_INFO("reading the smem conf fail\n");
    }
}
#endif

static void abov_platform_data_of_init(struct i2c_client *client,
        pabov_platform_data_t pplatData)
{
    struct device_node *np = client->dev.of_node;
    u32 cap_channel_top, cap_channel_bottom;
    int ret;

    client->irq = of_get_named_gpio(np, "gpio-irq-std", 0);
    LOG_INFO("client->irq = %d\n", client->irq);
    pplatData->irq_gpio = client->irq;
    ret = of_property_read_u32(np, "cap,use_channel_bottom", &cap_channel_bottom);
    ret = of_property_read_u32(np, "cap,use_channel_top", &cap_channel_top);
    pplatData->cap_channel_bottom = (int)cap_channel_bottom;
    pplatData->cap_channel_top = (int)cap_channel_top;
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

    /* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 start */
    pcb_abov_conf();
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 start*/
    if(!(((pcb_id_code >=0x27) && (pcb_id_code <= 0x2A)) || (pcb_id_code == 0x2F))){
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 end*/
        ret = of_property_read_string(np, "label1", &pplatData->fw_name);
    }else{
        ret = of_property_read_string(np, "label2", &pplatData->fw_name);
    }
    /* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 end */
    if (ret < 0) {
        LOG_DBG("firmware name read error!\n");
        return;
    }
    LOG_INFO("firmware name = %s.BIN\n", pplatData->fw_name);
}

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
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;

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
        #ifdef HQ_FACTORY_BUILD
        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 1);
        input_report_key(input_bottom_sar, KEY_SAR2_FAR, 0);
        input_report_key(input_top_sar, KEY_SAR_FAR, 1);
        input_report_key(input_top_sar, KEY_SAR_FAR, 0);
        #else
        input_report_rel(input_bottom_sar, REL_MISC, 2);
        input_report_rel(input_top_sar, REL_MISC, 2);
        #endif
        input_sync(input_bottom_sar);
        input_sync(input_top_sar);
        mEnabled = 0;
    } else {
        LOG_DBG("unknown enable symbol\n");
    }

    return count;
}

static CLASS_ATTR(enable, 0660, capsense_enable_show, capsense_enable_store);

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
static int enable_bottom(unsigned int enable)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_bottom_sar = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[0];

    if (enable == 1) {
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
            }
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x01)) {
            this->channel_status |= 0x01; //bit0 for ch0 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
        } else {
#endif
            read_register(this, ABOV_IRQSTAT_LEVEL_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
                input_report_rel(input_bottom_sar, REL_MISC, 1);
                input_sync(input_bottom_sar);
                pCurrentButton->state = S_PROX;
            } else {
                input_report_rel(input_bottom_sar, REL_MISC, 2);
                input_sync(input_bottom_sar);
                pCurrentButton->state = IDLE;
            }
#if defined(CONFIG_SENSORS)
        }
#endif
        touchProcess(this);
    } else if (enable == 0) {
        if(this->channel_status & 0x01) {
            this->channel_status &= ~0x01;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
            }
            mEnabled = 0;
        }
    } else {
        LOG_ERR("unknown enable symbol\n");
    }

    return 0;
}

static int enable_top(unsigned int enable)
{
    pabovXX_t this = abov_sar_ptr;
    pabov_t pDevice = NULL;
    struct input_dev *input_top_sar = NULL;
    struct _buttonInfo *buttons = NULL;
    struct _buttonInfo *pCurrentButton  = NULL;
    u8 i = 0;

    pDevice = this->pDevice;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;
    buttons = pDevice->pbuttonInformation->buttons;
    pCurrentButton = &buttons[1];

    if (enable == 1) {
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x00);
            if(this->irq_disabled == 1){
                enable_irq(this->irq);
                this->irq_disabled = 0;
            }
            mEnabled = 1;
        }

        if(!(this->channel_status & 0x02)) {
            this->channel_status |= 0x02; //bit1 for ch1 status
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);

#if defined(CONFIG_SENSORS)
        if (this->skip_data == true) {
            LOG_INFO("%s - skip grip event\n", __func__);
        } else {
#endif
            read_register(this, ABOV_IRQSTAT_LEVEL_REG, &i);
            if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
                input_report_rel(input_top_sar, REL_MISC, 1);
                input_sync(input_top_sar);
                pCurrentButton->state = S_PROX;
            } else {
                input_report_rel(input_top_sar, REL_MISC, 2);
                input_sync(input_top_sar);
                pCurrentButton->state = IDLE;
            }
#if defined(CONFIG_SENSORS)
        }
#endif
        touchProcess(this);
    } else if (enable == 0) {
        if(this->channel_status & 0x02) {
            this->channel_status &= ~0x02;
        }
        LOG_DBG("this->channel_status = 0x%x \n",this->channel_status);
        if(this->channel_status == 0) {
            write_register(this, ABOV_CTRL_MODE_REG, 0x01);
            if(this->irq_disabled == 0){
                disable_irq(this->irq);
                this->irq_disabled = 1;
            }
            mEnabled = 0;
        }
    } else {
        LOG_ERR("unknown enable symbol\n");
    }

    return 0;
}

static ssize_t abovxx_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    pabovXX_t this = abov_sar_ptr;
    int status = -1;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    LOG_INFO("%s: dev->name:%s\n", __func__, temp_input_dev->name); 

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

static ssize_t abovxx_enable_store (struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    u8 enable;
    int ret;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    LOG_INFO("%s: dev->name:%s new_value = %u\n", __func__, temp_input_dev->name, enable); 

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (enable == 1) {
            enable_bottom(1);
        } else {
            enable_bottom(0);
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (enable == 1) {
            enable_top(1);
        } else {
            enable_top(0);
        }
    }

    return count;
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
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    pDevice = this->pDevice;
    input_bottom_sar = pDevice->pbuttonInformation->input_bottom_sar;
    input_top_sar = pDevice->pbuttonInformation->input_top_sar;
    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        LOG_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        this->skip_data = true;
        if (mEnabled) {
            input_report_rel(input_bottom_sar, REL_MISC, 2);
            input_report_rel(input_top_sar, REL_MISC, 2);
            input_sync(input_bottom_sar);
            input_sync(input_top_sar);
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

static struct device_attribute *sub_sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
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
    SLEEP(110);

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
    /* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 start */
    LOG_INFO("abov_fw_update. checksum_h_bin:%x && checksum_l_bin:%x\n", checksum_h_bin, checksum_l_bin);
    /* HS50 code for HS50NA-47 by xiongxiaoliang at 20201009 end */

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
    struct input_dev *input_bottom_sar = NULL;
    struct input_dev *input_top_sar = NULL;

    LOG_INFO("abov_probe start()\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
    {
        LOG_INFO("i2c_check_functionality is fail\n");
        return -EIO;
    }

#ifdef JUDGE_BOARD_ID
    pcb_abov_conf();
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 start*/
    if (!(((pcb_id_code >=0x26) && (pcb_id_code <= 0x2A)) || (pcb_id_code == 0x2E) || (pcb_id_code == 0x2F)))
    /*HS50 code for HS50NA-330 by xiongxiaoliang at 20201203 end*/
    {
        LOG_ERR("Isn't EU and NA\n");
        return -ENODEV;
    }
#endif

    pplatData = kzalloc(sizeof(abov_platform_data_t), GFP_KERNEL);
    if (!pplatData) {
        LOG_DBG("platform data is required!\n");
        return -EINVAL;
    }

    pplatData->vddio = regulator_get(&client->dev, "vddio");
    if (IS_ERR(pplatData->vddio))
    {
        LOG_INFO("%s: Failed to get vddio regulator\n", __func__);
        ret = -EIO;
        goto exit_vddio_err;
    }
    else
    {
        ret = regulator_enable(pplatData->vddio);
        if (ret) {
            regulator_put(pplatData->vddio);
            LOG_DBG("%s: Error %d enable regulator\n", __func__, ret);
            ret = -EIO;
            goto exit_vddio_err;
        }
        LOG_INFO("vddio regulator is %s\n",
                regulator_is_enabled(pplatData->vddio) ? "on" : "off");
    }

    pplatData->vdd = regulator_get(&client->dev, "vdd");
    if (IS_ERR(pplatData->vdd)) {
        LOG_INFO("%s: Failed to get vdd regulator\n", __func__);
        ret = -EIO;
        goto exit_vdd_err;
    }
    else
    {
        ret = regulator_enable(pplatData->vdd);
        if (ret) {
            regulator_put(pplatData->vdd);
            LOG_DBG("%s: Error %d enable regulator\n", __func__, ret);
            ret = -EIO;
            goto exit_vdd_err;
        }
        LOG_INFO("vdd regulator is %s\n",
                regulator_is_enabled(pplatData->vdd) ? "on" : "off");
    }

    msleep(100);

    /* detect if abov exist or not */
    ret = abov_detect(client);
    if (ret == 0) {
        ret = -ENODEV;
        goto exit_no_detect_device;
    } else if (ret == 2) {
        isForceUpdate = true;
    }
    sar_name = "abov";

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

    /* save irq in case we need to reference it */
    this->irq = gpio_to_irq(client->irq);

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
        LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n", pDevice);
    }

    abov_sar_ptr = this;

    /* HS50 code for HS50EU-191 by xiongxiaoliang at 20201021 start */
    sar_plat_charger_init();
    /* HS50 code for HS50EU-191 by xiongxiaoliang at 20201021 end */

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
    input_bottom_sar = input_allocate_device();
    if (!input_bottom_sar) {
        LOG_ERR("input_allocate_device bottom failed \n");
        ret = -ENOMEM;
        goto exit_alloc_input_bottom;
    }
    /* Set all the keycodes */

    #ifdef HQ_FACTORY_BUILD
    __set_bit(EV_KEY, input_bottom_sar->evbit);
    __set_bit(KEY_SAR2_FAR, input_bottom_sar->keybit);
    __set_bit(KEY_SAR2_CLOSE, input_bottom_sar->keybit);
    #else
    input_set_capability(input_bottom_sar, EV_REL, REL_MISC);
    #endif

    /* save the input pointer and finish initialization */
    pDevice->pbuttonInformation->input_bottom_sar = input_bottom_sar;
    /* save the input pointer and finish initialization */
    input_bottom_sar->name = "grip_sensor";
    input_bottom_sar->id.bustype = BUS_I2C;
    if (input_register_device(input_bottom_sar)) {
        LOG_ERR("add abov sar sensor bottom unsuccess\n");
        input_free_device(input_bottom_sar);
        ret = -ENOMEM;
        goto exit_register_input_bottom;
    }

    /* Create the input device */
    input_top_sar = input_allocate_device();
    if (!input_top_sar) {
        LOG_ERR("input_allocate_device top failed \n");
        ret = -ENOMEM;
        goto exit_alloc_input_top;
    }
    /* Set all the keycodes */
    #ifdef HQ_FACTORY_BUILD
    __set_bit(EV_KEY, input_top_sar->evbit);
    __set_bit(KEY_SAR_FAR, input_top_sar->keybit);
    __set_bit(KEY_SAR_CLOSE, input_top_sar->keybit);
    #else
    input_set_capability(input_top_sar, EV_REL, REL_MISC);
    #endif
    /* save the input pointer and finish initialization */
    pDevice->pbuttonInformation->input_top_sar = input_top_sar;
    /* save the input pointer and finish initialization */
    input_top_sar->name = "grip_sensor_sub";
    input_top_sar->id.bustype = BUS_I2C;
    if (input_register_device(input_top_sar)) {
        LOG_ERR("add abov sar sensor top unsuccess\n");
        input_free_device(input_top_sar);
        ret = -ENOMEM;
        goto exit_register_input_top;
    }

#if defined(CONFIG_SENSORS)
    this->channel_status = 0;
    ret = sensors_register(&pplatData->factory_device, pplatData, sensor_attrs, MODULE_NAME);
    if (ret) {
        LOG_ERR("%s - cound not register sensor(%d).\n", __func__, ret);
        goto exit_sensors_register;
    }

    ret = sensors_register(&pplatData->factory_device_sub, pplatData, sub_sensor_attrs, SUB_MODULE_NAME);
    if (ret) {
        LOG_ERR("%s - cound not register sensor(%d).\n", __func__, ret);
        goto exit_sensors_register_sub;
    }

    ret = sysfs_create_group(&input_bottom_sar->dev.kobj, &abovxx_attributes_group);
    if (ret) {
        LOG_ERR("cound not create input bottom group\n");
        goto exit_input_create;
    }

    ret = sysfs_create_group(&input_top_sar->dev.kobj, &abovxx_attributes_group);
    if (ret) {
        LOG_ERR("cound not create input top group\n");
        goto exit_input_create;
    }
#endif

    ret = class_register(&capsense_class);
    if (ret < 0) {
        LOG_DBG("Create fsys class failed (%d)\n", ret);
        goto exit_creat_file;
    }

    ret = class_create_file(&capsense_class, &class_attr_enable);
    if (ret < 0) {
        LOG_DBG("Create enable file failed (%d)\n", ret);
        goto exit_creat_file;
    }

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
    sysfs_remove_group(&input_bottom_sar->dev.kobj,&abovxx_attributes_group);
    sysfs_remove_group(&input_top_sar->dev.kobj,&abovxx_attributes_group);
exit_input_create:
    sensors_unregister(pplatData->factory_device_sub, sub_sensor_attrs);
exit_sensors_register_sub:
    sensors_unregister(pplatData->factory_device, sensor_attrs);
exit_sensors_register:
#endif
    input_unregister_device(input_top_sar);
exit_register_input_top:
exit_alloc_input_top:
    input_unregister_device(input_bottom_sar);
exit_register_input_bottom:
exit_alloc_input_bottom:
    kfree(pDevice);
exit_alloc_dev:
    kfree(this);
exit_alloc_mem:
exit_no_detect_device:
    regulator_disable(pplatData->vdd);
    regulator_put(pplatData->vdd);
    LOG_ERR("free vdd regulator source!\n");
exit_vdd_err:
    regulator_disable(pplatData->vddio);
    regulator_put(pplatData->vddio);
    LOG_ERR("free vddio regulator source!\n");
exit_vddio_err:
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
        input_unregister_device(pDevice->pbuttonInformation->input_bottom_sar);
        input_unregister_device(pDevice->pbuttonInformation->input_top_sar);
        sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
        pplatData = client->dev.platform_data;
        if (pplatData && pplatData->exit_platform_hw)
            pplatData->exit_platform_hw();
        kfree(this->pDevice);
    }
    return abovXX_sar_remove(this);
}

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
