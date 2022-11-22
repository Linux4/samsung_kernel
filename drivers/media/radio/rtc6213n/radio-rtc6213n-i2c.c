/*
 * drivers/media/radio/rtc6213n/radio-rtc6213n-i2c.c
 *
 * I2C driver for Richwave RTC6213N FM Tuner
 *
 * Copyright (c) 2013 Richwave Technology Co.Ltd
 * Author: TianTsai Chang <changtt@richwave.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* driver definitions */
#define DRIVER_AUTHOR "TianTsai Chang <changtt@richwave.com.tw>";
#define DRIVER_KERNEL_VERSION KERNEL_VERSION(1, 0, 1)
#define DRIVER_CARD "Richwave rtc6213n FM Tuner"
#define DRIVER_DESC "I2C radio driver for rtc6213n FM Tuner"
#define DRIVER_VERSION "1.0.1"

/* kernel includes */
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
//#include <linux/interrupt.h>

#include <linux/jiffies.h>

#include "radio-rtc6213n.h"


/* I2C Device ID List */
static const struct i2c_device_id rtc6213n_i2c_id[] = {
    /* Generic Entry */
    { "rtc6213n", 0 },
    /* Terminating entry */
    { }
};
MODULE_DEVICE_TABLE(i2c, rtc6213n_i2c_id);

static unsigned short space = 1;
static unsigned short band = 0;
static unsigned short de = 0;


/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Radio Nr */
static int radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");

/* RDS buffer blocks */
static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0444);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

/* RDS maximum block errors */
static unsigned short max_rds_errors = 1;
/* 0 means   0  errors requiring correction */
/* 1 means 1-2  errors requiring correction (used by original USBRadio.exe) */
/* 2 means 3-5  errors requiring correction */
/* 3 means   6+ errors or errors in checkword, correction not possible */
module_param(max_rds_errors, ushort, 0644);
MODULE_PARM_DESC(max_rds_errors, "RDS maximum block errors: *1*");



/**************************************************************************
 * I2C Definitions
 **************************************************************************/

/* Write starts with the upper byte of register 0x02 */
//#define WRITE_REG_NUM     8
//#define WRITE_INDEX(i)        (i + 0x02)
#define WRITE_REG_NUM       RADIO_REGISTER_NUM
#define WRITE_INDEX(i)      (i + 0x02)%16

/* Read starts with the upper byte of register 0x0a */
#define READ_REG_NUM        RADIO_REGISTER_NUM
#define READ_INDEX(i)       ((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)

//static struct timer_list my_timer;
//static
struct tasklet_struct my_tasklet;
/**************************************************************************
 * General Driver Functions - REGISTERs
 **************************************************************************/

/*
 * rtc6213n_get_register - read register
 */
int rtc6213n_get_register(struct rtc6213n_device *radio, int regnr)
{
    u16 buf[READ_REG_NUM];
    struct i2c_msg msgs[1] = {
        { radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
            (void *)buf },
    };

    if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
        return -EIO;

    radio->registers[regnr] = __be16_to_cpu(buf[READ_INDEX(regnr)]);

    return 0;
}


/*
 * rtc6213n_set_register - write register
 */
int rtc6213n_set_register(struct rtc6213n_device *radio, int regnr)
{
    int i;
    u16 buf[WRITE_REG_NUM];
    struct i2c_msg msgs[1] = {
        { radio->client->addr, 0, sizeof(u16) * WRITE_REG_NUM,
            (void *)buf },
    };

    for (i = 0; i < WRITE_REG_NUM; i++)
        buf[i] = __cpu_to_be16(radio->registers[WRITE_INDEX(i)]);

    if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
        return -EIO;

    return 0;
}

/*
 * rtc6213n_set_register - write register
 */
int rtc6213n_set_serial_registers(struct rtc6213n_device *radio, u16 *data, int bytes)
{
    int i;
    u16 buf[46];
    struct i2c_msg msgs[1] = {
        { radio->client->addr, 0, sizeof(u16) * bytes,
            (void *)buf },
    };

    for (i = 0; i < bytes; i++)
        buf[i] = __cpu_to_be16(data[i]);

    if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
        return -EIO;

    return 0;
}


/**************************************************************************
 * General Driver Functions - ENTIRE REGISTERS
 **************************************************************************/

/*
 * rtc6213n_get_all_registers - read entire registers
 */
// changtt changed from static
int rtc6213n_get_all_registers(struct rtc6213n_device *radio)
{
    int i;
    u16 buf[READ_REG_NUM];
    struct i2c_msg msgs[1] = {
        { radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
            (void *)buf },
    };

    if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
        return -EIO;

    for (i = 0; i < READ_REG_NUM; i++)
        radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

    return 0;
}

/*
 * rtc6213n_get_allbanks_registers - read entire registers of each bank
 */
#if 0
int rtc6213n_get_allbanks_registers(struct rtc6213n_device *radio)
{
    int i;
    u16 buf[READ_REG_NUM];

    radio->registers[BANKCFG] = 0x4000;
    retval = rtc6213n_set_register(radio, BANKCFG);
    if (retval < 0) goto done;

    struct i2c_msg msgs[1] = {
        { radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
            (void *)buf },
    };

    if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
        return -EIO;

    for (i = 0; i < READ_REG_NUM; i++)
        radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

    return 0;
}
#endif

int rtc6213n_disconnect_check(struct rtc6213n_device *radio)
{
    return 0;
}

/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * rtc6213n_fops_open - file open
 */
int rtc6213n_fops_open(struct file *file)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    mutex_lock(&radio->lock);
    radio->users++;

    dev_info(&radio->videodev->dev, "rtc6213n_fops_open : enter user num = %d\n",  radio->users);

    if (radio->users == 1) {
        /* start radio */
        retval = rtc6213n_start(radio);
        if (retval < 0) goto done;
        dev_info(&radio->videodev->dev, "rtc6213n_fops_open : after initialization \n");
 
#ifdef CONFIG_STD_POLLING
    //tasklet_hi_schedule(&my_tasklet);
    tasklet_disable(&my_tasklet);
    dev_info(&radio->videodev->dev, "rtc6213n_fops_open tasklet state = %d, tasklet enable =%d\n",
        my_tasklet.state, my_tasklet.count);
#endif
 
    /* mpxconfig */
    radio->registers[MPXCFG] = 0x0000 |
        MPXCFG_CSR0_DIS_SMUTE |             /* Disable Softmute */
        MPXCFG_CSR0_DIS_MUTE |              /* Disable Mute */
        ((de << 12) & MPXCFG_CSR0_DEEM) | 0x0008;   /* De-emphasis & Default Volume 8 */
    retval = rtc6213n_set_register(radio, MPXCFG);
    if (retval < 0) goto done;

    /* channel */
    radio->registers[CHANNEL] =
        ((band  << 12) & CHANNEL_CSR0_BAND)  |              /* Band */
        ((space << 10) & CHANNEL_CSR0_CHSPACE) | 0x1a;    /* Space & Default channel 90.1Mhz*/
    retval = rtc6213n_set_register(radio, CHANNEL);
    if (retval < 0) goto done;

    /* seekconfig2 */
    radio->registers[SEEKCFG2] = 0x4050;  /* Seeking TH */
    retval = rtc6213n_set_register(radio, SEEKCFG2);
    if (retval < 0) goto done;

    /* enable RDS / STC interrupt */
    radio->registers[SYSCFG] |= SYSCFG_CSR0_RDSIRQEN;
    radio->registers[SYSCFG] |= SYSCFG_CSR0_STDIRQEN;
    radio->registers[SYSCFG] |= SYSCFG_CSR0_RDS_EN;
    retval = rtc6213n_set_register(radio, SYSCFG);
    if (retval < 0) goto done;

    radio->registers[PADCFG] &= ~PADCFG_CSR0_GPIO;
    radio->registers[PADCFG] |= 0x1 << 2;
    retval = rtc6213n_set_register(radio, PADCFG);
    if (retval < 0) goto done;

    /* powerconfig */
    // radio->registers[POWERCFG] = POWERCFG_RDSM;?
    radio->registers[POWERCFG] = POWERCFG_CSR0_ENABLE;  /* Enable FM */
    retval = rtc6213n_set_register(radio, POWERCFG);
    if (retval < 0) goto done;

    /* r last channel */
    //retval = rtc6213n_set_chan(radio, 0x1a);    // 90.1MHz for testing

#if 1
    dev_info(&radio->videodev->dev, "RTC6213n Tuner1: DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
                radio->registers[DEVICEID], radio->registers[CHIPID]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner2: Reg2=0x%4.4hx Reg3=0x%4.4hx\n",
                radio->registers[MPXCFG], radio->registers[CHANNEL]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner3: Reg4=0x%4.4hx Reg5=0x%4.4hx\n",
                radio->registers[SYSCFG], radio->registers[SEEKCFG1]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner4: Reg6=0x%4.4hx Reg7=0x%4.4hx\n",
                radio->registers[POWERCFG], radio->registers[PADCFG]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner5: Reg8=0x%4.4hx Reg9=0x%4.4hx\n",
                radio->registers[8], radio->registers[9]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner6: regA=0x%4.4hx RegB=0x%4.4hx\n",
                radio->registers[10], radio->registers[11]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner7: regC=0x%4.4hx RegD=0x%4.4hx\n",
                radio->registers[12], radio->registers[13]);
    dev_info(&radio->videodev->dev, "RTC6213n Tuner8: regE=0x%4.4hx RegF=0x%4.4hx\n",
                radio->registers[14], radio->registers[15]);
#endif
    }
    dev_info(&radio->videodev->dev, "rtc6213n_fops_open : Exit\n");

done:
    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_fops_release - file release
 */
int rtc6213n_fops_release(struct file *file)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    /* safety check */
    if (!radio)
        return -ENODEV;

    mutex_lock(&radio->lock);
    radio->users--;
    if (radio->users == 0)
    {
        /* stop radio */
        retval = rtc6213n_stop(radio);
        tasklet_kill(&my_tasklet);
        //del_timer(&my_timer);
    }
    mutex_unlock(&radio->lock);
    dev_info(&radio->videodev->dev, "rtc6213n_fops_release : Exit\n");

    return retval;
}



/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/

/*
 * rtc6213n_vidioc_querycap - query device capabilities
 */
int rtc6213n_vidioc_querycap(struct file *file, void *priv,
        struct v4l2_capability *capability)
{
    strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
    strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
    capability->version = DRIVER_KERNEL_VERSION;
    capability->capabilities = V4L2_CAP_HW_FREQ_SEEK |
        V4L2_CAP_TUNER | V4L2_CAP_RADIO;

    return 0;
}



/**************************************************************************
 * I2C Interface
 **************************************************************************/

/*
 * rtc6213n_i2c_interrupt - interrupt handler
 */
#ifdef CONFIG_STD_POLLING
static void rtc6213n_i2c_interrupt(void *dev_id)
#else
static irqreturn_t rtc6213n_i2c_interrupt(int irq, void *dev_id)
#endif
{
    struct rtc6213n_device *radio = dev_id;
    unsigned char regnr;
    unsigned char blocknum;
    unsigned short bler; /* rds block errors */
    unsigned short rds;
    unsigned char tmpbuf[3];
    int retval = 0;

#if 1
    /* check Seek/Tune Complete */
    retval = rtc6213n_get_register(radio, STATUS);
    if (retval < 0)
        goto end;
#endif

#if 0
    /* Update RDS registers */
    for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
        retval = rtc6213n_get_register(radio, STATUS + regnr);
        if (retval < 0)
            goto end;
    }
#endif

    if (radio->registers[STATUS] & STATUS_STD)
    {
        complete(&radio->completion);
        goto end;
    }
    //dev_info(&radio->videodev->dev, "rtc6213n_i2c_interrupt3 : before getting rds blocks\n");

    /* safety checks */
    if ((!((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) && (radio->registers[STATUS] & STATUS_RDS_SYNC))))
        goto end;

#if 1
    /* Update RDS registers */
    for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
        retval = rtc6213n_get_register(radio, STATUS + regnr);
        if (retval < 0)
            goto end;
    }

    //dev_info(&radio->videodev->dev, "rtc6213n_i2c_interrupt5 : before getting rds blocks\n");

    /* get rds blocks */
    if ((radio->registers[STATUS] & STATUS_RDS_RDY) == 0)
        /* No RDS group ready, better luck next time */
        goto end;
    //dev_info(&radio->videodev->dev, "rtc6213n_i2c_interrupt2 : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
    //    radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);
#endif

#ifdef CONFIG_REJECTRDS_WITHERROR
    for (blocknum = 0; blocknum < 4; blocknum++) {
        switch (blocknum) {
        default:
            bler = (radio->registers[RSSI] & RSSI_RDS_BA_ERRS) >> 14;
            break;
        case 1:
            bler = (radio->registers[RSSI] & RSSI_RDS_BB_ERRS) >> 12;
            break;
        case 2:
            bler = (radio->registers[RSSI] & RSSI_RDS_BC_ERRS) >> 10;
            break;
        case 3:
            bler = (radio->registers[RSSI] & RSSI_RDS_BD_ERRS) >> 8;
            break;
        };
        if (bler > max_rds_errors)
            goto end;
    }
#endif

    for (blocknum = 0; blocknum < 4; blocknum++) {
        switch (blocknum) {
        default:
            bler = (radio->registers[RSSI] & RSSI_RDS_BA_ERRS) >> 14;
            rds = radio->registers[BA_DATA];
            break;
        case 1:
            bler = (radio->registers[RSSI] & RSSI_RDS_BB_ERRS) >> 12;
            rds = radio->registers[BB_DATA];
            break;
        case 2:
            bler = (radio->registers[RSSI] & RSSI_RDS_BC_ERRS) >> 10;
            rds = radio->registers[BC_DATA];
            break;
        case 3:
            bler = (radio->registers[RSSI] & RSSI_RDS_BD_ERRS) >> 8;
            rds = radio->registers[BD_DATA];
            break;
        };

        /* Fill the V4L2 RDS buffer */
        put_unaligned_le16(rds, &tmpbuf);
        tmpbuf[2] = blocknum;       /* offset name */
#ifndef CONFIG_REJECTRDS_WITHERROR
        tmpbuf[2] |= blocknum << 3; /* received offset */

        tmpbuf[2] |= bler << 6;
        //if (bler > max_rds_errors)
        //    tmpbuf[2] |= 0x80;  /* uncorrectable errors */
        //else if (bler > 0)
        //    tmpbuf[2] |= 0x40;  /* corrected error(s) */
#endif

        /* copy RDS block to internal buffer */
        memcpy(&radio->buffer[radio->wr_index], &tmpbuf, 3);
        radio->wr_index += 3;

        /* wrap write pointer */
        if (radio->wr_index >= radio->buf_size)
            radio->wr_index = 0;

        /* check for overflow */
        if (radio->wr_index == radio->rd_index) {
            /* increment and wrap read pointer */
            radio->rd_index += 3;
            if (radio->rd_index >= radio->buf_size)
                radio->rd_index = 0;
        }
    }

    if (radio->wr_index != radio->rd_index)
    {
        wake_up_interruptible(&radio->read_queue);
        //dev_info(&radio->videodev->dev, "rtc6213n_i2c_interrupt7 : radio->wr_index %d = radio->rd_index %d\n", radio->wr_index, radio->rd_index);
    }

end:
    //dev_info(&radio->videodev->dev, "rtc6213n_i2c_interrupt : radio->wr_index %d = radio->rd_index %d\n", radio->wr_index, radio->rd_index);

#ifdef CONFIG_STD_POLLING
#else
    return IRQ_HANDLED;
#endif
}

/*
 * rtc6213n_i2c_probe - probe for the device
 */
static int __devinit rtc6213n_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct rtc6213n_device *radio;
    int retval = 0;

    /* private data allocation and initialization */
    radio = kzalloc(sizeof(struct rtc6213n_device), GFP_KERNEL);
    if (!radio) {
        retval = -ENOMEM;
        goto err_initial;
    }

    radio->users = 0;
    radio->client = client;
    mutex_init(&radio->lock);

    /* video device allocation and initialization */
    radio->videodev = video_device_alloc();
    if (!radio->videodev) {
        retval = -ENOMEM;
        goto err_radio;
    }
    memcpy(radio->videodev, &rtc6213n_viddev_template,
            sizeof(rtc6213n_viddev_template));
    video_set_drvdata(radio->videodev, radio);

#if 0
    /* power up : need 110ms */
    radio->registers[POWERCFG] = POWERCFG_CSR0_ENABLE;
    if (rtc6213n_set_register(radio, POWERCFG) < 0) {
        retval = -EIO;
        goto err_video;
    }
    msleep(110);
#endif

    /* get device and chip versions */
    if (rtc6213n_get_all_registers(radio) < 0) {
        retval = -EIO;
        goto err_video;
    }

#if 0 // for testing
    retval = rtc6213n_start(radio);
    /* set initial frequency */
    rtc6213n_set_freq(radio, 90.1 * FREQ_MUL); /* available in all regions */
#endif

    dev_info(&client->dev, "rtc6213n_i2c_probe DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
            radio->registers[DEVICEID], radio->registers[CHIPID]);

    /* rds buffer allocation */
    radio->buf_size = rds_buf * 3;
    radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
    if (!radio->buffer) {
        retval = -EIO;
        goto err_video;
    }

    /* rds buffer configuration */
    radio->wr_index = 0;
    radio->rd_index = 0;
    init_waitqueue_head(&radio->read_queue);

    /* mark Seek/Tune Complete Interrupt enabled */
#ifdef CONFIG_STD_POLLING
    radio->stci_enabled = false;
#else
    radio->stci_enabled = true;
#endif
    init_completion(&radio->completion);

#ifdef CONFIG_STD_POLLING
    /* setup your timer to call my_timer_callback */
    tasklet_init(&my_tasklet, rtc6213n_i2c_interrupt, (unsigned long)radio);
    dev_info(&client->dev, "rtc6213n_i2c_probe tasklet state = %d, tasklet enable =%d\n",
        my_tasklet.state, my_tasklet.count);

    tasklet_disable(&my_tasklet);
    //tasklet_enable(&my_tasklet);
    tasklet_schedule(&my_tasklet);

    dev_info(&client->dev, "rtc6213n_i2c_probe tasklet state = %d, tasklet enable =%d\n",
        my_tasklet.state, my_tasklet.count);

    /* setup timer interval to 80 msecs */
    //mod_timer(&my_timer, jiffies + msecs_to_jiffies(80));
#else
    retval = request_threaded_irq(client->irq, NULL, rtc6213n_i2c_interrupt,
            IRQF_TRIGGER_FALLING, DRIVER_NAME, radio);
    if (retval) {
        dev_err(&client->dev, "Failed to register interrupt\n");
        goto err_rds;
    }
#endif

    /* register video device */
    retval = video_register_device(radio->videodev, VFL_TYPE_RADIO, radio_nr);
    if (retval) {
        dev_warn(&client->dev, "Could not register video device\n");
        goto err_all;
    }
    i2c_set_clientdata(client, radio);

    return 0;
err_all:
    free_irq(client->irq, radio);
err_rds:
    kfree(radio->buffer);
err_video:
    video_device_release(radio->videodev);
err_radio:
    kfree(radio);
err_initial:
    return retval;
}


/*
 * rtc6213n_i2c_remove - remove the device
 */
static __devexit int rtc6213n_i2c_remove(struct i2c_client *client)
{
    struct rtc6213n_device *radio = i2c_get_clientdata(client);

    free_irq(client->irq, radio);
    video_unregister_device(radio->videodev);
    kfree(radio);

    return 0;
}


#ifdef CONFIG_PM
/*
 * rtc6213n_i2c_suspend - suspend the device
 */
static int rtc6213n_i2c_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct rtc6213n_device *radio = i2c_get_clientdata(client);

    /* power down */
    radio->registers[POWERCFG] |= POWERCFG_CSR0_DISABLE;
    if (rtc6213n_set_register(radio, POWERCFG) < 0)
        return -EIO;

    return 0;
}


/*
 * rtc6213n_i2c_resume - resume the device
 */
static int rtc6213n_i2c_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct rtc6213n_device *radio = i2c_get_clientdata(client);

    /* power up : need 110ms */
    radio->registers[POWERCFG] |= POWERCFG_CSR0_ENABLE;
    if (rtc6213n_set_register(radio, POWERCFG) < 0)
        return -EIO;
    msleep(110);

    return 0;
}

static SIMPLE_DEV_PM_OPS(rtc6213n_i2c_pm, rtc6213n_i2c_suspend, rtc6213n_i2c_resume);
#endif


/*
 * rtc6213n_i2c_driver - i2c driver interface
 */
static struct i2c_driver rtc6213n_i2c_driver = {
    .driver = {
        .name       = "rtc6213n",
        .owner      = THIS_MODULE,
#ifdef CONFIG_PM
        .pm         = &rtc6213n_i2c_pm,
#endif
    },
    .probe          = rtc6213n_i2c_probe,
    .remove         = __devexit_p(rtc6213n_i2c_remove),
    .id_table       = rtc6213n_i2c_id,
};



/**************************************************************************
 * Module Interface
 **************************************************************************/

/*
 * rtc6213n_i2c_init - module init
 */
static int __init rtc6213n_i2c_init(void)
{
    printk(KERN_INFO DRIVER_DESC ", Version " DRIVER_VERSION "\n");
    return i2c_add_driver(&rtc6213n_i2c_driver);
}


/*
 * rtc6213n_i2c_exit - module exit
 */
static void __exit rtc6213n_i2c_exit(void)
{
    i2c_del_driver(&rtc6213n_i2c_driver);
}


module_init(rtc6213n_i2c_init);
module_exit(rtc6213n_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
