/*
 *  drivers/media/radio/rtc6213n/radio-rtc6213n-common.c
 *
 *  Driver for Richwave RTC6213N FM Tuner
 *
 *  Copyright (c) 2013 TianTsai Chang <changtt@richwave.com.tw>
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


/*
 * History:
 * 2013-05-12   TianTsai Chang <changtt@richwave.com.tw>
 *      Version 1.0.0
 *      - First working version
 */

#include <linux/delay.h>
#include <linux/i2c.h>

/* kernel includes */
#include "radio-rtc6213n.h"

/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Spacing (kHz) */
/* 0: 200 kHz (USA, Australia) */
/* 1: 100 kHz (Europe, Japan) */
/* 2:  50 kHz */
static unsigned short space = 1;
module_param(space, ushort, 0444);
MODULE_PARM_DESC(space, "Spacing: 0=200kHz *1=100kHz* 2=50kHz");

/* Bottom of Band (MHz) */
/* 0: 87.5 - 108 MHz (USA, Europe)*/
/* 1: 76   - 108 MHz (Japan wide band) */
/* 2: 76   -  90 MHz (Japan) */
static unsigned short band = 0;
module_param(band, ushort, 0444);
MODULE_PARM_DESC(band, "Band: *0=87.5-108MHz* 1=76-108MHz 2=76-91MHz 3=65-76MHz");

/* De-emphasis */
/* 0: 75 us (USA) */
/* 1: 50 us (Europe, Australia, Japan) */
static unsigned short de = 0;
module_param(de, ushort, 0444);
MODULE_PARM_DESC(de, "De-emphasis: *0=75us* 1=50us");

/* Tune timeout */
static unsigned int tune_timeout = 3000;
module_param(tune_timeout, uint, 0644);
MODULE_PARM_DESC(tune_timeout, "Tune timeout: *3000*");

/* Seek timeout */
static unsigned int seek_timeout = 8000;
module_param(seek_timeout, uint, 0644);
MODULE_PARM_DESC(seek_timeout, "Seek timeout: *8000*");

/**************************************************************************
 * Generic Functions
 **************************************************************************/

/*
 * rtc6213n_set_chan - set the channel
 */
static int rtc6213n_set_chan(struct rtc6213n_device *radio, unsigned short chan)
{
    int retval;
    unsigned long timeout;
    bool timed_out = 0;

    dev_info(&radio->videodev->dev, "RTC6213n tuning process is starting : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx chan=0x%4.4hx\n",
        radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS], chan);

    /* start tuning */
    radio->registers[CHANNEL] &= ~CHANNEL_CSR0_CH;
    radio->registers[CHANNEL] |= CHANNEL_CSR0_TUNE | chan;
    retval = rtc6213n_set_register(radio, CHANNEL);
    if (retval < 0)
        goto done;

    /* currently I2C driver only uses interrupt way to tune */
    if (radio->stci_enabled)
    {
        INIT_COMPLETION(radio->completion);

        /* wait till tune operation has completed */
        retval = wait_for_completion_timeout(&radio->completion,
            msecs_to_jiffies(tune_timeout));
        if (!retval)
        {
            dev_info(&radio->videodev->dev, "rtc6213n_set_chan : timed_out\n");
            timeout = true;
        }
        retval = rtc6213n_get_register(radio, STATUS);
        if (retval < 0)
            goto stop;
    }
    else
    {
        /* wait till tune operation has completed */
        timeout = jiffies + msecs_to_jiffies(tune_timeout);
        do {
            retval = rtc6213n_get_register(radio, STATUS);
            if (retval < 0)
                goto stop;
            timed_out = time_after(jiffies, timeout);
        } while (((radio->registers[STATUS] & STATUS_STD) == 0)
            && (!timed_out));
        dev_info(&radio->videodev->dev, "RTC6213n tuning process is done : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
                  radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);
    }

    if ((radio->registers[STATUS] & STATUS_STD) == 0)
        dev_info(&radio->videodev->dev, "tune does not complete\n");
    if (timed_out)
        dev_info(&radio->videodev->dev,
            "tune timed out after %u ms\n", tune_timeout);

stop:
    /* stop tuning */
    radio->registers[CHANNEL] &= ~CHANNEL_CSR0_TUNE;
    retval = rtc6213n_set_register(radio, CHANNEL);
    if (retval < 0)
        goto stop;

    retval = rtc6213n_get_register(radio, STATUS);
    if (retval < 0)
        goto stop;

done:
    dev_info(&radio->videodev->dev, "rtc6213n_set_chans is done : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
        radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);
    return retval;
}


/*
 * rtc6213n_get_freq - get the frequency
 */
static int rtc6213n_get_freq(struct rtc6213n_device *radio, unsigned int *freq)
{
    unsigned int spacing, band_bottom;
    unsigned short chan;
    int retval;

    /* Spacing (kHz) */
    switch ((radio->registers[CHANNEL] & CHANNEL_CSR0_CHSPACE) >> 10)
    {
        /* 0: 200 kHz (USA, Australia) */
        case 0:
            spacing = 0.200 * FREQ_MUL; break;
        /* 1: 100 kHz (Europe, Japan) */
        case 1:
            spacing = 0.100 * FREQ_MUL; break;
        /* 2:  50 kHz */
        default:
            spacing = 0.050 * FREQ_MUL; break;
    };

    /* Bottom of Band (MHz) */
    switch ((radio->registers[CHANNEL] & CHANNEL_CSR0_BAND) >> 12)
    {
        /* 0: 87.5 - 108 MHz (USA, Europe) */
        case 0:
            band_bottom = 87.5 * FREQ_MUL; break;
        /* 1: 76   - 108 MHz (Japan wide band) */
        default:
            band_bottom = 76   * FREQ_MUL; break;
        /* 2: 76   -  90 MHz (Japan) */
        case 2:
            band_bottom = 76   * FREQ_MUL; break;
    };

stop:
    /* read channel */
    retval = rtc6213n_get_register(radio, STATUS);
    if (retval < 0)
        goto stop;
 
    chan = radio->registers[STATUS] & STATUS_READCH;
 
    dev_info(&radio->videodev->dev, "rtc6213n_get_freq is done : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
        radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);


    /* Frequency (MHz) = Spacing (kHz) x Channel + Bottom of Band (MHz) */
    *freq = chan * spacing + band_bottom;
    dev_info(&radio->videodev->dev, "rtc6213n_get_freq is done1 : band_bottom=%d, spacing=%d, chan=%d, freq=%d\n",
        band_bottom, spacing, chan, *freq);


    return retval;
}


/*
 * rtc6213n_set_freq - set the frequency
 */
int rtc6213n_set_freq(struct rtc6213n_device *radio, unsigned int freq)
{
    unsigned int spacing, band_bottom;
    unsigned short chan;

    /* Spacing (kHz) */
    switch ((radio->registers[CHANNEL] & CHANNEL_CSR0_CHSPACE) >> 10)
    {
        /* 0: 200 kHz (USA, Australia) */
        case 0:
            spacing = 0.200 * FREQ_MUL; break;
        /* 1: 100 kHz (Europe, Japan) */
        case 1:
            spacing = 0.100 * FREQ_MUL; break;
        /* 2:  50 kHz */
        default:
            spacing = 0.050 * FREQ_MUL; break;
    };

    /* Bottom of Band (MHz) */
    switch ((radio->registers[CHANNEL] & CHANNEL_CSR0_BAND) >> 12)
    {
        /* 0: 87.5 - 108 MHz (USA, Europe) */
        case 0:
            band_bottom = 87.5 * FREQ_MUL; break;
        /* 1: 76   - 108 MHz (Japan wide band) */
        default:
            band_bottom = 76   * FREQ_MUL; break;
        /* 2: 76   -  90 MHz (Japan) */
        case 2:
            band_bottom = 76   * FREQ_MUL; break;
    };

    /* Chan = [ Freq (Mhz) - Bottom of Band (MHz) ] / Spacing (kHz) */
    chan = (freq - band_bottom) / spacing;

    return rtc6213n_set_chan(radio, chan);
}


/*
 * rtc6213n_set_seek - set seek
 */
static int rtc6213n_set_seek(struct rtc6213n_device *radio,
        unsigned int seek_wrap, unsigned int seek_up)
{
    int retval = 0;
    unsigned long timeout;
    bool timed_out = 0;

    dev_info(&radio->videodev->dev, "RTC6213n seeking process is starting: CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
        radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);

#if 0
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
#endif

    if (seek_wrap)
        radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SKMODE;
    else
        radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SKMODE;

    if (seek_up)
        radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SEEKUP;
    else
        radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEKUP;

    retval = rtc6213n_set_register(radio, SEEKCFG1);
    if (retval < 0)
        goto done;

    /* start seeking */
    radio->registers[SEEKCFG1] |= SEEKCFG1_CSR0_SEEK;

#if 0
       dev_info(&radio->videodev->dev, "RTC6213n seek1: DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
                        radio->registers[DEVICEID], radio->registers[CHIPID]);
       dev_info(&radio->videodev->dev, "RTC6213n seek2: Reg2=0x%4.4hx Reg3=0x%4.4hx\n",
                        radio->registers[MPXCFG], radio->registers[CHANNEL]);
       dev_info(&radio->videodev->dev, "RTC6213n seek3: Reg4=0x%4.4hx Reg5=0x%4.4hx\n",
                        radio->registers[SYSCFG], radio->registers[SEEKCFG1]);
       dev_info(&radio->videodev->dev, "RTC6213n seek4: Reg6=0x%4.4hx Reg7=0x%4.4hx\n",
                        radio->registers[POWERCFG], radio->registers[PADCFG]);
       dev_info(&radio->videodev->dev, "RTC6213n seek5: Reg8=0x%4.4hx Reg9=0x%4.4hx\n",
                        radio->registers[8], radio->registers[9]);
       dev_info(&radio->videodev->dev, "RTC6213n seek6: regA=0x%4.4hx RegB=0x%4.4hx\n",
                        radio->registers[10], radio->registers[11]);
#endif

    retval = rtc6213n_set_register(radio, SEEKCFG1);
    if (retval < 0)
        goto done;

    /* currently I2C driver only uses interrupt way to seek */
    if (radio->stci_enabled)
    {
        INIT_COMPLETION(radio->completion);

        /* wait till seek operation has completed */
        retval = wait_for_completion_timeout(&radio->completion,
                msecs_to_jiffies(seek_timeout));
        if (!retval)
        {
            dev_info(&radio->videodev->dev, "rtc6213n_set_seek : timed_out\n");
            timed_out = true;
        }
    }
    else
    {
        /* wait till seek operation has completed */
        timeout = jiffies + msecs_to_jiffies(seek_timeout);
        do {
            retval = rtc6213n_get_register(radio, STATUS);
            if (retval < 0)
                goto stop;
            timed_out = time_after(jiffies, timeout);
        } while (((radio->registers[STATUS] & STATUS_STD) == 0)
                && (!timed_out));
        dev_info(&radio->videodev->dev, "RTC6213n seeking process is done : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx STATUS=0x%4.4hx\n",
            radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[STATUS]);
    }

    if ((radio->registers[STATUS] & STATUS_STD) == 0)
        dev_info(&radio->videodev->dev, "seek does not complete\n");
    //if (radio->registers[STATUS] & STATUS_SF)
    //    dev_info(&radio->videodev->dev, "seek failed / band limit reached\n");
    if (timed_out)
        dev_info(&radio->videodev->dev, "seek timed out after %u ms\n", seek_timeout);

stop:
    /* stop seeking */
    radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEK;
    retval = rtc6213n_set_register(radio, SEEKCFG1);

done:
    // show invalid seek (POSIX.1) error to JNI
    // terminal the completescan loop
    if (radio->registers[STATUS] & STATUS_SF)
    {
        dev_info(&radio->videodev->dev, "seek failed / band limit reached\n");
        retval = ESPIPE;
    }
    /* try again, if timed out */
    //if ((retval == 0) && timed_out)
    else if ((retval == 0) && timed_out)
        retval = -EAGAIN;
    return retval;
}


/*
 * rtc6213n_start - switch on radio
 */
int rtc6213n_start(struct rtc6213n_device *radio)
{
    int retval;

    u16 swbk1[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000, 0x4000,
                    0x1A08, 0x0100, 0x0740, 0x0040, 0x005A, 0x02C0, 0x0000,
                    0x1440, 0x0080, 0x0840, 0x0000, 0x4002, 0x805A, 0x0D35, 0x7367, 0x0000};
    u16 swbk2[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000, 0x8000,
                    0x0000, 0x0000, 0x0333, 0x051C, 0x01EB, 0x01EB, 0x0333,
                    0xF2AB, 0x7F8A, 0x0780, 0x0000, 0x1400, 0x405A, 0x0000, 0x3200, 0x0000};
    u16 swbk4[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000, 0x2000,
                    0x050F, 0x0E85, 0x5AA6, 0xDC57, 0x8000, 0x00A3, 0x00A3,
                    0xC018, 0x7F80, 0x3C08, 0xB6CF, 0x8100, 0x0000, 0x0140, 0x4700, 0x0000};
    u16 swbk5[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000, 0x6000,
                    0x3590, 0x6311, 0x3008, 0x0019, 0x0D79, 0x7D2F, 0x8000,
                    0x02A1, 0x771F, 0x3241, 0x2635, 0x2516, 0x3614, 0x0000, 0x0000, 0x0000};
    u16 swbk7[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7000, 0xE000,
                    0x11A2, 0x0F92, 0x0000, 0x0000, 0x0000, 0x0000, 0x801D,
                    0x0000, 0x0000, 0x0072, 0x00FF, 0x001F, 0x03FF, 0x16D1, 0x13B7, 0x0000};

    dev_info(&radio->videodev->dev, "RTC6213n_start1 : DeviceID=0x%4.4hx ChipID=0x%4.4hx Addr=0x%4.4hx\n",
              radio->registers[DEVICEID], radio->registers[CHIPID], radio->client->addr);

    /* Set 0x16AA */
    radio->registers[BANKCFG] = 0x0000;
    radio->registers[DEVICEID] = 0x16AA;
    retval = rtc6213n_set_register(radio, DEVICEID);
    if (retval < 0) goto done;
    msleep(10);

    // Don't read all between writing 0x16AA and 0x96AA
    radio->registers[DEVICEID] = 0x96AA;
    retval = rtc6213n_set_register(radio, DEVICEID);
    if (retval < 0) goto done;
    msleep(50);

    /* get device and chip versions */
    /* Have to update shadow buf from all register */
    if (rtc6213n_get_all_registers(radio) < 0) 
    {
        retval = -EIO;
        goto done;
    }

    dev_info(&radio->videodev->dev, "==================== before initail process ====================\n");
    dev_info(&radio->videodev->dev, "RTC6213n_start2 : DeviceID=0x%4.4hx ChipID=0x%4.4hx Addr=0x%4.4hx\n",
              radio->registers[DEVICEID], radio->registers[CHIPID], radio->client->addr);

    // rtc6213n initial setting
    retval = rtc6213n_set_serial_registers(radio, swbk1, 23);
    if (retval < 0) goto done;

    retval = rtc6213n_set_serial_registers(radio, swbk2, 23);
    if (retval < 0) goto done;

    retval = rtc6213n_set_serial_registers(radio, swbk4, 23);
    if (retval < 0) goto done;

    retval = rtc6213n_set_serial_registers(radio, swbk5, 23);
    if (retval < 0) goto done;

    retval = rtc6213n_set_serial_registers(radio, swbk7, 23);
    if (retval < 0) goto done;
    // rtc6213n initial setting

    /* get device and chip versions */
    if (rtc6213n_get_all_registers(radio) < 0)
    {
        retval = -EIO;
        goto done;
    }

done:
    return retval;
}

/*
 * rtc6213n_stop - switch off radio
 */
int rtc6213n_stop(struct rtc6213n_device *radio)
{
    int retval;

    /* sysconfig */
    radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;
    retval = rtc6213n_set_register(radio, SYSCFG);
    if (retval < 0)
        goto done;

    /* powerconfig */
    radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_MUTE;
    retval = rtc6213n_set_register(radio, MPXCFG);

    /* POWERCFG_ENABLE has to automatically go low */
    radio->registers[POWERCFG] |= POWERCFG_CSR0_DISABLE;
    radio->registers[POWERCFG] &= ~POWERCFG_CSR0_ENABLE;
    retval = rtc6213n_set_register(radio, POWERCFG);

    /* Set 0x16AA */
    radio->registers[DEVICEID] = 0x16AA;
    retval = rtc6213n_set_register(radio, DEVICEID);

done:
    return retval;
}

/*
 * rtc6213n_rds_on - switch on rds reception
 */
static int rtc6213n_rds_on(struct rtc6213n_device *radio)
{
    int retval;

    /* sysconfig */
    radio->registers[SYSCFG] |= SYSCFG_CSR0_RDS_EN;
    retval = rtc6213n_set_register(radio, SYSCFG);


    if (retval < 0)
        radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;

    return retval;
}

/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * rtc6213n_fops_read - read RDS data
 */
static ssize_t rtc6213n_fops_read(struct file *file, char __user *buf,
        size_t count, loff_t *ppos)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;
    unsigned int block_count = 0;

    /* switch on rds reception */
    mutex_lock(&radio->lock);
    /* if RDS is not on, then turn on RDS */
    if ((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) == 0)
        rtc6213n_rds_on(radio);

    dev_info(&radio->videodev->dev, "rtc6213n_fops_read : RDS enable? = 0x%4.4hx\n",
              radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN);

    /* block if no new data available */
    while (radio->wr_index == radio->rd_index)
    {
        dev_info(&radio->videodev->dev, "rtc6213n_fops_read : wr_index %d = rd_index %d\n", radio->wr_index, radio->rd_index);
        if (file->f_flags & O_NONBLOCK) {
            dev_info(&radio->videodev->dev, "rtc6213n_fops_read : O_NONBLOCK = %d\n", file->f_flags & O_NONBLOCK);
            retval = -EWOULDBLOCK;
            goto done;
        }
        if (wait_event_interruptible(radio->read_queue,
            radio->wr_index != radio->rd_index) < 0) {
            dev_info(&radio->videodev->dev, "rtc6213n_fops_read : (wait_event_interruptible) wr_index %d = rd_index %d\n",
                radio->wr_index, radio->rd_index);
            retval = -EINTR;
            goto done;
        }
    }

    /* calculate block count from byte count */
    count /= 3;
    dev_info(&radio->videodev->dev, "rtc6213n_fops_read : count = %d\n", count);

    /* copy RDS block out of internal buffer and to user buffer */
    while (block_count < count)
    {
        if (radio->rd_index == radio->wr_index)
            break;
        /* always transfer rds complete blocks */
        if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
            /* retval = -EFAULT; */
            break;
        /* increment and wrap read pointer */
        radio->rd_index += 3;
        if (radio->rd_index >= radio->buf_size)
            radio->rd_index = 0;
        /* increment counters */
        block_count++;
        buf += 3;
        retval += 3;
        dev_info(&radio->videodev->dev, "rtc6213n_fops_read : block_count = %d, count = %d\n",
                block_count, count);
    }

done:
    mutex_unlock(&radio->lock);
    return retval;
}

/*
 * rtc6213n_fops_poll - poll RDS data
 */
static unsigned int rtc6213n_fops_poll(struct file *file,
        struct poll_table_struct *pts)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    /* switch on rds reception */

    mutex_lock(&radio->lock);
    if ((radio->registers[SYSCFG] & SYSCFG_CSR0_RDS_EN) == 0)
        rtc6213n_rds_on(radio);
    mutex_unlock(&radio->lock);

#ifdef CONFIG_STD_POLLING
    dev_info(&radio->videodev->dev, "rtc6213n_fops_poll tasklet state = %d, tasklet enable =%d\n",
        my_tasklet.state, my_tasklet.count);
#endif
    poll_wait(file, &radio->read_queue, pts);

#ifdef CONFIG_STD_POLLING
    tasklet_disable(&my_tasklet);
    tasklet_enable(&my_tasklet);
    //if(!my_tasklet.state)
        tasklet_hi_schedule(&my_tasklet);
#endif

    if (radio->rd_index != radio->wr_index)
        retval = POLLIN | POLLRDNORM;

#ifdef CONFIG_STD_POLLING
    dev_info(&radio->videodev->dev, "rtc6213n_fops_poll tasklet state1 = %d, tasklet enable =%d\n",
        my_tasklet.state, my_tasklet.count);
#endif
    return retval;
}

/*
 * rtc6213n_fops - file operations interface
 */
static const struct v4l2_file_operations rtc6213n_fops = {
    .owner          =   THIS_MODULE,
    .read           =   rtc6213n_fops_read,
    .poll           =   rtc6213n_fops_poll,
    .unlocked_ioctl =   video_ioctl2,
    .open           =   rtc6213n_fops_open,
    .release        =   rtc6213n_fops_release,
};

/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/
/*
 * rtc6213n_vidioc_queryctrl - enumerate control items
 */
static int rtc6213n_vidioc_queryctrl(struct file *file, void *priv,
        struct v4l2_queryctrl *qc)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = -EINVAL;

    /* abort if qc->id is below V4L2_CID_BASE */
    if (qc->id < V4L2_CID_BASE)
        goto done;

    /* search video control */
    switch (qc->id)
    {
        case V4L2_CID_AUDIO_VOLUME:
            return v4l2_ctrl_query_fill(qc, 0, 15, 1, 15);
        case V4L2_CID_AUDIO_MUTE:
            return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
    }

    /* disable unsupported base controls */
    /* to satisfy kradio and such apps */
    if ((retval == -EINVAL) && (qc->id < V4L2_CID_LASTP1)) {
        qc->flags = V4L2_CTRL_FLAG_DISABLED;
        retval = 0;
    }

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "query controls failed with %d\n", retval);
    return retval;
}


/*
 * rtc6213n_vidioc_g_ctrl - get the value of a control
 */
static int rtc6213n_vidioc_g_ctrl(struct file *file, void *priv,
        struct v4l2_control *ctrl)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    mutex_lock(&radio->lock);
 
    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    switch (ctrl->id)
    {
        case V4L2_CID_AUDIO_VOLUME:
            ctrl->value = radio->registers[MPXCFG] & MPXCFG_CSR0_VOLUME;
            break;
        case V4L2_CID_AUDIO_MUTE:
            ctrl->value = ((radio->registers[MPXCFG] & MPXCFG_CSR0_DIS_MUTE) == 0) ? 1 : 0;
            break;
        case V4L2_CID_PRIVATE_CSR0_DIS_SMUTE:
            ctrl->value = ((radio->registers[MPXCFG] & MPXCFG_CSR0_DIS_SMUTE) == 0) ? 1 : 0;
            break;
        case V4L2_CID_PRIVATE_CSR0_BAND:
            ctrl->value = ((radio->registers[CHANNEL] & CHANNEL_CSR0_BAND) >> 12);
            break;
        case V4L2_CID_PRIVATE_CSR0_SEEKRSSITH:
            ctrl->value = radio->registers[SEEKCFG1] & SEEKCFG1_CSR0_SEEKRSSITH;
            break;
        case V4L2_CID_PRIVATE_RSSI:
        #if 0
            retval = rtc6213n_get_register(radio, RSSI);
            if (retval < 0)
                goto done;
            retval = rtc6213n_get_register(radio, STATUS);
            if (retval < 0)
                goto done;
        #else
            rtc6213n_get_all_registers(radio);
        #endif
            ctrl->value = radio->registers[RSSI] & RSSI_RSSI;
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_ctrl: STATUS=0x%4.4hx RSSI=0x%4.4hx\n",
                radio->registers[STATUS], radio->registers[RSSI]);
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_ctrl: regC=0x%4.4hx RegD=0x%4.4hx\n",
                radio->registers[12], radio->registers[13]);
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_ctrl: regE=0x%4.4hx RegF=0x%4.4hx\n",
                radio->registers[14], radio->registers[15]);
            break;
        case V4L2_CID_PRIVATE_DEVICEID:
            ctrl->value = radio->registers[DEVICEID] & DEVICE_ID;
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_ctrl: DEVICEID=0x%4.4hx\n", radio->registers[DEVICEID]);
            break;
        default:
            retval = -EINVAL;
    }

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "get control failed with %d\n", retval);

    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_vidioc_s_ctrl - set the value of a control
 */
static int rtc6213n_vidioc_s_ctrl(struct file *file, void *priv,
        struct v4l2_control *ctrl)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    mutex_lock(&radio->lock);
        dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl1 : before rtc6213n_disconnect_check");

    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl2 : after rtc6213n_disconnect_check");

    switch (ctrl->id)
    {
        case V4L2_CID_AUDIO_VOLUME:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : MPXCFG=0x%4.4hx POWERCFG=0x%4.4hx\n",
                      radio->registers[MPXCFG], radio->registers[POWERCFG]);
            radio->registers[MPXCFG] &= ~MPXCFG_CSR0_VOLUME;
            radio->registers[MPXCFG] |= (ctrl->value > 15) ? 8 : ctrl->value;

            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl4 : MPXCFG=0x%4.4hx POWERCFG=0x%4.4hx\n",
                      radio->registers[MPXCFG], radio->registers[POWERCFG]);
            retval = rtc6213n_set_register(radio, MPXCFG);
            break;
        case V4L2_CID_AUDIO_MUTE:
            if (ctrl->value == 1)
                radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_MUTE;
            else
                radio->registers[MPXCFG] |= MPXCFG_CSR0_DIS_MUTE;
            retval = rtc6213n_set_register(radio, MPXCFG);
            break;
        // PRIVATE CID
        //case RTC6213N_IOC_DSMUTE:
        case V4L2_CID_PRIVATE_CSR0_DIS_SMUTE:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : before V4L2_CID_PRIVATE_CSR0_DIS_SMUTE");
            if (ctrl->value == 1)
                radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DIS_SMUTE;
            else
                radio->registers[MPXCFG] |= MPXCFG_CSR0_DIS_SMUTE;
            retval = rtc6213n_set_register(radio, MPXCFG);
            break;
        //case RTC6213N_IOC_DE_SET:
        case V4L2_CID_PRIVATE_CSR0_DEEM:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : before V4L2_CID_PRIVATE_CSR0_DEEM");
            if (ctrl->value == 1)
                radio->registers[MPXCFG] |= MPXCFG_CSR0_DEEM;
            else
                radio->registers[MPXCFG] &= ~MPXCFG_CSR0_DEEM;
            retval = rtc6213n_set_register(radio, MPXCFG);
            break;
        //case RTC6213N_IOC_BAND_SET:
        case V4L2_CID_PRIVATE_CSR0_BAND:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SEEKCFG1]);
            radio->registers[CHANNEL] &= ~CHANNEL_CSR0_BAND;
            radio->registers[CHANNEL] |= (ctrl->value << 12);
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl4 : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SEEKCFG1]);
            retval = rtc6213n_set_register(radio, CHANNEL);
            break;
        //case RTC6213N_IOC_CHAN_SPACING_SET:
        case V4L2_CID_PRIVATE_CSR0_CHSPACE:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SEEKCFG1]);
            radio->registers[CHANNEL] &= ~CHANNEL_CSR0_CHSPACE;
            radio->registers[CHANNEL] |= (ctrl->value << 10);
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl4 : CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SEEKCFG1]);
            retval = rtc6213n_set_register(radio, CHANNEL);
            break;
        case V4L2_CID_PRIVATE_CSR0_RDS_EN:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : CHANNEL=0x%4.4hx SYSCFG=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SYSCFG]);
            radio->registers[SYSCFG] &= ~SYSCFG_CSR0_RDS_EN;
            radio->registers[SYSCFG] |= (ctrl->value << 12);
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl4 : CHANNEL=0x%4.4hx SYSCFG=0x%4.4hx\n",
                      radio->registers[CHANNEL], radio->registers[SYSCFG]);
            retval = rtc6213n_set_register(radio, SYSCFG);
            break;
        case V4L2_CID_PRIVATE_CSR0_SEEKRSSITH:
            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl3 : MPXCFG=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[MPXCFG], radio->registers[SEEKCFG1]);
            radio->registers[SEEKCFG1] &= ~SEEKCFG1_CSR0_SEEKRSSITH;
            radio->registers[SEEKCFG1] |= ctrl->value;

            dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_ctrl4 : MPXCFG=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                      radio->registers[MPXCFG], radio->registers[SEEKCFG1]);
            retval = rtc6213n_set_register(radio, SEEKCFG1);
            break;
        default:
            retval = -EINVAL;
    }

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "set control failed with %d\n", retval);
    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_vidioc_g_audio - get audio attributes
 */
static int rtc6213n_vidioc_g_audio(struct file *file, void *priv,
        struct v4l2_audio *audio)
{
    /* driver constants */
    audio->index = 0;
    strcpy(audio->name, "Radio");
    audio->capability = V4L2_AUDCAP_STEREO;
    audio->mode = 0;

    return 0;
}


/*
 * rtc6213n_vidioc_g_tuner - get tuner attributes
 */
static int rtc6213n_vidioc_g_tuner(struct file *file, void *priv,
        struct v4l2_tuner *tuner)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    mutex_lock(&radio->lock);
 
    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_tuner: CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx RSSI=0x%4.4hx FREQ_MUL=0x%4.4hx\n",
              radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[RSSI], FREQ_MUL);

    if (tuner->index != 0) {
        retval = -EINVAL;
        goto done;
    }

    retval = rtc6213n_get_register(radio, RSSI);
    if (retval < 0)
        goto done;

    /* driver constants */
    strcpy(tuner->name, "FM");
    tuner->type = V4L2_TUNER_RADIO;
    tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
        V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO;

    /* range limits */
    switch ((radio->registers[CHANNEL] & CHANNEL_CSR0_BAND) >> 12)
    {
        //test = rtc6213n_start(radio);

        /* 0: 87.5 - 108 MHz (USA, Europe, default) */
        default:
            tuner->rangelow  =  87.5 * FREQ_MUL;
            tuner->rangehigh = 108   * FREQ_MUL;
            break;
        /* 1: 76   - 108 MHz (Japan wide band) */
        case 1:
            tuner->rangelow  =  76   * FREQ_MUL;
            tuner->rangehigh = 108   * FREQ_MUL;
            break;
        /* 2: 76   -  90 MHz (Japan) */
        case 2:
            tuner->rangelow  =  76   * FREQ_MUL;
            tuner->rangehigh =  90   * FREQ_MUL;
            break;
    };

    /* stereo indicator == stereo (instead of mono) */
    if ((radio->registers[STATUS] & STATUS_SI) == 0)
        tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
    else
        tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
    /* If there is a reliable method of detecting an RDS channel,
       then this code should check for that before setting this
       RDS subchannel. */
    tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

    /* mono/stereo selector */
    if ((radio->registers[MPXCFG] & MPXCFG_CSR0_MONO) == 0)
        tuner->audmode = V4L2_TUNER_MODE_STEREO;
    else
        tuner->audmode = V4L2_TUNER_MODE_MONO;

    /* min is worst, max is best; signal:0..0xffff; rssi: 0..0xff */
    /* measured in units of dbµV in 1 db increments (max at ~75 dbµV) */
    tuner->signal = (radio->registers[RSSI] & RSSI_RSSI);
 
done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "get tuner failed with %d\n", retval);

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_tuner: CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx RSSI=0x%4.4hx FREQ_MUL=0x%4.4hx\n",
              radio->registers[CHANNEL], radio->registers[SEEKCFG1], radio->registers[RSSI], FREQ_MUL);

    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_vidioc_s_tuner - set tuner attributes
 */
static int rtc6213n_vidioc_s_tuner(struct file *file, void *priv,
        struct v4l2_tuner *tuner)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;
    mutex_lock(&radio->lock);

    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    if (tuner->index != 0)
        goto done;

    // changtt added
    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_tuner1: DeviceID=0x%4.4hx ChipID=0x%4.4hx MPXCFG=0x%4.4hx\n",
              radio->registers[DEVICEID], radio->registers[CHIPID], radio->registers[MPXCFG]);
    //test = rtc6213n_start(radio);

    /* mono/stereo selector */
    switch (tuner->audmode)
    {
        case V4L2_TUNER_MODE_MONO:
            radio->registers[MPXCFG] |= MPXCFG_CSR0_MONO;  /* force mono */
            break;
        case V4L2_TUNER_MODE_STEREO:
            radio->registers[MPXCFG] &= ~MPXCFG_CSR0_MONO; /* try stereo */
            break;
        default:
            goto done;
    }

    retval = rtc6213n_set_register(radio, MPXCFG);

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

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_tuner2: DeviceID=0x%4.4hx ChipID=0x%4.4hx MPXCFG=0x%4.4hx\n",
              radio->registers[DEVICEID], radio->registers[CHIPID], radio->registers[MPXCFG]);
    retval = rtc6213n_get_register(radio, MPXCFG);
    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_tuner3: DeviceID=0x%4.4hx ChipID=0x%4.4hx MPXCFG=0x%4.4hx\n",
              radio->registers[DEVICEID], radio->registers[CHIPID], radio->registers[MPXCFG]);

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "set tuner failed with %d\n", retval);
    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_vidioc_g_frequency - get tuner or modulator radio frequency
 */
static int rtc6213n_vidioc_g_frequency(struct file *file, void *priv,
        struct v4l2_frequency *freq)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

    /* safety checks */
    mutex_lock(&radio->lock);
 
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    if (freq->tuner != 0) {
        retval = -EINVAL;
        goto done;
    }

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_frequency1 : *freq=%d\n",
                        freq->frequency);
    freq->type = V4L2_TUNER_RADIO;
    retval = rtc6213n_get_freq(radio, &freq->frequency);
    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_g_frequency2 : *freq=%d, retval=%d\n",
                        freq->frequency, retval);

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "get frequency failed with %d\n", retval);
    mutex_unlock(&radio->lock);
    return retval;
}


/*
 * rtc6213n_vidioc_s_frequency - set tuner or modulator radio frequency
 */
static int rtc6213n_vidioc_s_frequency(struct file *file, void *priv,
        struct v4l2_frequency *freq)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;

#ifdef CONFIG_STD_POLLING
    tasklet_disable(&my_tasklet);
#endif

    mutex_lock(&radio->lock);
 
    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    if (freq->tuner != 0) {
        retval = -EINVAL;
        goto done;
    }

    dev_warn(&radio->videodev->dev,
            "rtc6213n_vidioc_s_frequency freq = %d\n", freq->frequency);

    retval = rtc6213n_set_freq(radio, freq->frequency);

done:
    if (retval < 0)
        dev_warn(&radio->videodev->dev,
            "set frequency failed with %d\n", retval);
    mutex_unlock(&radio->lock);
#ifdef CONFIG_STD_POLLING
    tasklet_disable(&my_tasklet);
    tasklet_enable(&my_tasklet);
#endif
    return retval;
}


/*
 * rtc6213n_vidioc_s_hw_freq_seek - set hardware frequency seek
 */
static int rtc6213n_vidioc_s_hw_freq_seek(struct file *file, void *priv,
        struct v4l2_hw_freq_seek *seek)
{
    struct rtc6213n_device *radio = video_drvdata(file);
    int retval = 0;
//    unsigned int freqvalue;

#ifdef CONFIG_STD_POLLING
    tasklet_disable(&my_tasklet);
#endif

    mutex_lock(&radio->lock);
    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_hw_freq_seek2 : MPXCFG=0x%4.4hx CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                        radio->registers[MPXCFG], radio->registers[CHANNEL], radio->registers[SEEKCFG1]);

    /* safety checks */
    retval = rtc6213n_disconnect_check(radio);
    if (retval)
        goto done;

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_hw_freq_seek : MPXCFG=0x%4.4hx CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                        radio->registers[MPXCFG], radio->registers[CHANNEL], radio->registers[SEEKCFG1]);

    if (seek->tuner != 0) {
        retval = -EINVAL;
        goto done;
    }

    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_hw_freq_seek : MPXCFG=0x%4.4hx CHANNEL=0x%4.4hx SEEKCFG1=0x%4.4hx\n",
                        radio->registers[MPXCFG], radio->registers[CHANNEL], radio->registers[SEEKCFG1]);

    retval = rtc6213n_set_seek(radio, seek->wrap_around, seek->seek_upward);
#if 0
    retval = rtc6213n_get_freq(radio, &freqvalue);
    seek->reserved[0] = freqvalue;   // changtt
#else

    seek->reserved[0]=radio->registers[STATUS] & STATUS_READCH;
#endif
    dev_info(&radio->videodev->dev, "rtc6213n_vidioc_s_hw_freq_seek : seek->reserved[0]=%d\n",
                        seek->reserved[0]);

done:
    if (retval < 0)
        dev_info(&radio->videodev->dev,
            "set hardware frequency seek failed with %d\n", retval);
    mutex_unlock(&radio->lock);

#ifdef CONFIG_STD_POLLING
    tasklet_disable(&my_tasklet);
    tasklet_enable(&my_tasklet);
#endif
    return retval;
}


/*
 * rtc6213n_ioctl_ops - video device ioctl operations
 */
static const struct v4l2_ioctl_ops rtc6213n_ioctl_ops = {
    .vidioc_querycap            =   rtc6213n_vidioc_querycap,
    .vidioc_queryctrl           =   rtc6213n_vidioc_queryctrl,
    .vidioc_g_ctrl              =   rtc6213n_vidioc_g_ctrl,
    .vidioc_s_ctrl              =   rtc6213n_vidioc_s_ctrl,
    .vidioc_g_audio             =   rtc6213n_vidioc_g_audio,
    .vidioc_g_tuner             =   rtc6213n_vidioc_g_tuner,
    .vidioc_s_tuner             =   rtc6213n_vidioc_s_tuner,
    .vidioc_g_frequency         =   rtc6213n_vidioc_g_frequency,
    .vidioc_s_frequency         =   rtc6213n_vidioc_s_frequency,
    .vidioc_s_hw_freq_seek      =   rtc6213n_vidioc_s_hw_freq_seek,
//    .V4L2_CID_PRIVATE_DEVICEID      =   .V4L2_CID_PRIVATE_DEVICEID,
//    .RTC6213N_IOC_DSMUTE            =   .RTC6213N_IOC_DSMUTE,
//    .RTC6213N_IOC_MUTE              =   .RTC6213N_IOC_MUTE,
//    .RTC6213N_IOC_MONO_SET          =   .RTC6213N_IOC_MONO_SET,
//    .RTC6213N_IOC_DE_SET            =   .RTC6213N_IOC_DE_SET,
//    .RTC6213N_IOC_VOLUME_GET        =   .RTC6213N_IOC_VOLUME_GET,
//    .RTC6213N_IOC_VOLUME_SET        =   .RTC6213N_IOC_VOLUME_SET,
//    .RTC6213N_IOC_CHAN_SELECT       =   .RTC6213N_IOC_CHAN_SELECT,
//    .RTC6213N_IOC_BAND_SET          =   .RTC6213N_IOC_BAND_SET,
//    .RTC6213N_IOC_CHAN_SPACING_SET  =   .RTC6213N_IOC_CHAN_SPACING_SET,
//    .RTC6213N_IOC_RDS_ENABLE        =   .RTC6213N_IOC_RDS_ENABLE,
//    .RTC6213N_IOC_RESET_RDS_DATA    =   .RTC6213N_IOC_RESET_RDS_DATA,
//    .RTC6213N_IOC_RSSI_SEEK_TH_SET  =   .RTC6213N_IOC_RSSI_SEEK_TH_SET,
//    .RTC6213N_IOC_SEEK_FULL         =   .RTC6213N_IOC_SEEK_FULL,
//    .RTC6213N_IOC_SEEK_UP           =   .RTC6213N_IOC_SEEK_UP,
//    .RTC6213N_IOC_SEEK_DOWN         =   .RTC6213N_IOC_SEEK_DOWN,
//    .RTC6213N_IOC_SEEK_CANCEL       =   .RTC6213N_IOC_SEEK_CANCEL,
//    .RTC6213N_IOC_POWERUP           =   .RTC6213N_IOC_POWERUP,
//    .RTC6213N_IOC_POWERDOWN         =   .RTC6213N_IOC_POWERDOWN,
//    .RTC6213N_IOC_RDS_DATA_GET      =   .RTC6213N_IOC_RDS_DATA_GET,
//    .RTC6213N_IOC_CHAN_GET          =   .RTC6213N_IOC_CHAN_GET,
//    .RTC6213N_IOC_STATUS_RSSI_GET   =   .RTC6213N_IOC_STATUS_RSSI_GET,
};


/*
 * rtc6213n_viddev_template - video device interface
 */
struct video_device rtc6213n_viddev_template = {
    .fops           =   &rtc6213n_fops,
    .name           =   DRIVER_NAME,
    .release        =   video_device_release,
    .ioctl_ops      =   &rtc6213n_ioctl_ops,
};
