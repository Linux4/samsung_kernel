/* drivers/i2c/chips/epl2182.c - light and proxmity sensors driver
 * Copyright (C) 2011 ELAN Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
//#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/i2c/epl2182_pls_v2.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

/******************************************************************************
 * configuration
*******************************************************************************/
#define PS_INTERRUPT_MODE		1		// 0 is polling mode, 1 is interrupt mode
#define PLSENSOR_ADAPTIVE
#ifdef PLSENSOR_ADAPTIVE
/*ensure the initial value can enable Psensor interrupt,the ps_state of the initial value will be 0*/
static u16 P_SENSOR_LTHD = 0x7fff;   //init
static u16 P_SENSOR_HTHD = 0x0;       //init
#else
#define P_SENSOR_LTHD                  1900//120         //100
#define P_SENSOR_HTHD                  2000//170         //500
#endif

#define PS_POLLING_RATE			100
#define ALS_POLLING_RATE		1000

#define LUX_PER_COUNT			440		// 660 = 1.1*0.6*1000
#ifdef PLSENSOR_ADAPTIVE
#define DEBOUNCE 30
#define MIN_SPACING 250
#define DIVEDE 4
static bool first_do_irq = true;
static u16 ps_min = 0;
static u16 ps_max = 0;
static u16 ps_threshold = 0;
#endif
/******************************************************************************
 * configuration
*******************************************************************************/

typedef enum
{
    CMC_MODE_ALS    = 0x00,
    CMC_MODE_PS     = 0x10,
} CMC_MODE;

#define TXBYTES				2
#define RXBYTES				2

#define PS_DELAY			50
#define ALS_DELAY			55

#define PACKAGE_SIZE		2
#define I2C_RETRY_COUNT		10
#define P_INTT				1

#define PS_INTT				4
#define ALS_INTT			6	//5-8

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd);

#if PS_INTERRUPT_MODE
static void epl_sensor_irq_do_work(struct work_struct *work);
static DECLARE_WORK(epl_sensor_irq_work, epl_sensor_irq_do_work);
#endif

static void report_polling_do_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(report_polling_work, report_polling_do_work);
static void polling_do_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(polling_work, polling_do_work);

/* primitive raw data from I2C */
typedef struct _epl_raw_data
{
    u8 raw_bytes[PACKAGE_SIZE];
    u16 ps_state;
    u16 ps_int_state;
    u16 ps_ch1_raw;
    u16 als_ch1_raw;
} epl_raw_data;

struct elan_epl_data
{
    struct i2c_client *client;
    //struct input_dev *als_input_dev;
    struct input_dev *ps_input_dev;
    struct workqueue_struct *epl_wq;
    struct early_suspend early_suspend;

    int intr_pin;
    //int (*power)(int on);

    int ps_opened;
    int als_opened;

    int enable_pflag;
    int enable_lflag;
    int read_flag;
    int irq;
} ;

static DECLARE_WAIT_QUEUE_HEAD(ps_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(ls_waitqueue);


static int ps_data_changed;
static int ls_data_changed;
static struct i2c_client *this_client = NULL;

//static struct wake_lock g_ps_wlock;
struct elan_epl_data *epl_data;
static epl_raw_data	gRawData;
static int dual_count;

static const char ElanPsensorName[] = "proximity";
static const char ElanALsensorName[] = "lightsensor-level";

static int psensor_mode_suspend = 0;

#define LOG_TAG				        "[EPL2182] "
#define LOG_FUN(f)			        printk(KERN_INFO LOG_TAG"%s\n", __FUNCTION__)
#define LOG_INFO(fmt, args...)		printk(KERN_INFO LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)		printk(KERN_ERR  LOG_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

/*
//====================I2C write operation===============//
//regaddr: ELAN Register Address.
//bytecount: How many bytes to be written to register via i2c bus.
//txbyte: I2C bus transmit byte(s). Single byte(0X01) transmit only slave address.
//data: setting value.
//
// Example: If you want to write single byte to 0x1D register address, show below
//	      elan_sensor_I2C_Write(client,0x1D,0x01,0X02,0xff);
//
*/
static int elan_sensor_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount, uint8_t txbyte, uint8_t data)
{
    uint8_t buffer[2];
    int ret = 0;
    int retry,val;
    struct elan_epl_data *epld = epl_data;

    buffer[0] = (regaddr<<3) | bytecount ;
    buffer[1] = data;

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_send(client, buffer, txbyte);

        if (ret == txbyte)
        {
            break;
        }

        val = gpio_get_value(epld->intr_pin);

        LOG_INFO("INTERRUPT GPIO val = %d\n",  val);

        msleep(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        LOG_ERR(KERN_ERR "i2c write retry over %d\n", I2C_RETRY_COUNT);
        return -EINVAL;
    }

    return ret;
}

static int elan_sensor_I2C_Read(struct i2c_client *client)
{
    uint8_t buffer[RXBYTES];
    int ret = 0, i =0;
    int retry,val;
    struct elan_epl_data *epld = epl_data;

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_recv(client, buffer, RXBYTES);

        if (ret == RXBYTES)
            break;

        val = gpio_get_value(epld->intr_pin);

        LOG_INFO("INTERRUPT GPIO val = %d\n", val);

        msleep(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        LOG_ERR("i2c read retry over %d\n", I2C_RETRY_COUNT);
        return -EINVAL;
    }

    for(i=0; i<PACKAGE_SIZE; i++)
    {
        gRawData.raw_bytes[i] = buffer[i];
    }

    return ret;
}

static void elan_sensor_restart_work(void)
{
    struct elan_epl_data *epld = epl_data;
    cancel_delayed_work(&polling_work);
    cancel_delayed_work(&report_polling_work);
    queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(10));
}

static int elan_sensor_psensor_enable(struct elan_epl_data *epld)
{
    int ret;
    uint8_t regdata = 0;
    struct i2c_client *client = epld->client;

    LOG_INFO("--- Proximity sensor Enable --- \n");

    //disable_irq(epld->irq);
    ret = elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);

    regdata = EPL_SENSING_8_TIME | EPL_PS_MODE | EPL_L_GAIN ;
    regdata = regdata | (PS_INTERRUPT_MODE ? EPL_C_SENSING_MODE : EPL_S_SENSING_MODE);
    ret = elan_sensor_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

    regdata = PS_INTT<<4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
    ret = elan_sensor_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

    set_psensor_intr_threshold(P_SENSOR_LTHD ,P_SENSOR_HTHD);
#ifdef PLSENSOR_ADAPTIVE
    if (true == first_do_irq) {
            msleep(PS_DELAY);
            elan_sensor_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
            elan_sensor_I2C_Read(client);
            gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);
            elan_sensor_I2C_Write(client,REG_16,R_TWO_BYTE,0x01,0x00);
            elan_sensor_I2C_Read(client);
            gRawData.ps_ch1_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
            P_SENSOR_LTHD = gRawData.ps_ch1_raw + MIN_SPACING - DEBOUNCE;
            P_SENSOR_HTHD = gRawData.ps_ch1_raw + MIN_SPACING - DEBOUNCE;
            set_psensor_intr_threshold(P_SENSOR_LTHD ,P_SENSOR_HTHD);
    }
#endif

    ret = elan_sensor_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
    ret = elan_sensor_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);

#if PS_INTERRUPT_MODE
    if(epld->enable_lflag)
    {
        msleep(PS_DELAY);
        elan_sensor_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
        elan_sensor_I2C_Read(client);
        gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);

        if(gRawData.ps_state!= gRawData.ps_int_state)
        {
            elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
        }
        else
        {
            elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
        }

    }
    else
    {
        elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
    }
    //enable_irq(epld->irq);
#endif

    if (ret != 0x02)
    {
        LOG_INFO("P-sensor i2c err\n");
    }

    return ret;
}

static int elan_sensor_lsensor_enable(struct elan_epl_data *epld)
{
    int ret;
    uint8_t regdata = 0;
    struct i2c_client *client = epld->client;

    LOG_INFO("--- ALS sensor Enable --- \n");

    //disable_irq(epld->irq);
    regdata = EPL_INT_DISABLE;
    ret = elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02, regdata);

    regdata = EPL_S_SENSING_MODE | EPL_SENSING_8_TIME | EPL_ALS_MODE | EPL_AUTO_GAIN;
    ret = elan_sensor_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

    regdata = ALS_INTT<<4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
    ret = elan_sensor_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

    ret = elan_sensor_I2C_Write(client,REG_10,W_SINGLE_BYTE,0x02, EPL_GO_MID);
    ret = elan_sensor_I2C_Write(client,REG_11,W_SINGLE_BYTE,0x02, EPL_GO_LOW);

    ret = elan_sensor_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
    ret = elan_sensor_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);

    if(ret != 0x02)
    {
        LOG_INFO(" ALS-sensor i2c err\n");
    }

    return ret;

}

/*
//====================elan_epl_ps_poll_rawdata===============//
//polling method for proximity sensor detect. Report proximity sensor raw data.
//Report "ABS_DISTANCE" event to HAL layer.
//Variable "value" 0 and 1 to represent which distance from psensor to target(human's face..etc).
//value: 0 represent near.
//value: 1 represent far.
*/
static int elan_epl_ps_poll_rawdata(void)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;
    u16 value = 0;

    elan_sensor_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_LOCK);

    elan_sensor_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
    elan_sensor_I2C_Read(client);
    gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);

    elan_sensor_I2C_Write(client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_sensor_I2C_Read(client);
    gRawData.ps_ch1_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

#ifdef PLSENSOR_ADAPTIVE
    value = gRawData.ps_ch1_raw;
    /*threshold detect process*/
    if(0 == ps_max || 0 == ps_min) {
        ps_min = value;
        ps_max = value;
        ps_threshold = ps_min + MIN_SPACING;
    }
    if(value > ps_max) {
        ps_max = value;
        ps_threshold = ((ps_max - ps_min)  / DIVEDE) + ps_min;
        if(ps_threshold - ps_min < MIN_SPACING)
              ps_threshold = ps_min + MIN_SPACING;
    } else if(value < ps_min) {
        ps_min = value;
        ps_threshold = ((ps_max - ps_min)  / DIVEDE) + ps_min;
        if(ps_threshold - ps_min < MIN_SPACING)
              ps_threshold = ps_min + MIN_SPACING;
    }
    P_SENSOR_LTHD = ps_threshold - DEBOUNCE;
    P_SENSOR_HTHD = ps_threshold + DEBOUNCE;
    set_psensor_intr_threshold(P_SENSOR_LTHD ,P_SENSOR_HTHD);
    /*threshold detect process end*/
    LOG_INFO("ps_min (%4d), ps_max (%4d), ps_threshold (%4d)\n", ps_min, ps_max, ps_threshold);
    if (true == first_do_irq)
            first_do_irq = false;
#endif
    elan_sensor_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
    LOG_INFO("### ps_ch1_raw_data  (%d), value(%d) ###\n", gRawData.ps_ch1_raw, gRawData.ps_state);
    ps_data_changed = 1;

    input_report_abs(epld->ps_input_dev, ABS_DISTANCE, gRawData.ps_state);
    input_sync(epld->ps_input_dev);
    return 0;
}

static void elan_epl_als_rawdata(void)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;
    uint32_t lux;

    elan_sensor_I2C_Write(client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_sensor_I2C_Read(client);
    gRawData.als_ch1_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    lux =  (gRawData.als_ch1_raw* LUX_PER_COUNT) / 1000 * 15 / 100;
    if(lux>20000)
        lux=20000;

    LOG_INFO("-------------------  ALS raw = %d, lux = %d\n\n",  gRawData.als_ch1_raw,  lux);
	ls_data_changed = 1;
#if 1
    input_report_abs(epld->ps_input_dev, ABS_MISC, lux);
    input_sync(epld->ps_input_dev);
#else
    input_report_abs(epld->als_input_dev, ABS_MISC, lux);
    input_sync(epld->als_input_dev);
#endif
}

/*
//====================set_psensor_intr_threshold===============//
//low_thd: The value is psensor interrupt low threshold.
//high_thd:	The value is psensor interrupt hihg threshold.
//When psensor rawdata > hihg_threshold, interrupt pin will be pulled low.
//After interrupt occur, psensor rawdata < low_threshold, interrupt pin will be pulled high.
*/
static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
    int ret = 0;
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;

    uint8_t high_msb ,high_lsb, low_msb, low_lsb;

    high_msb = (uint8_t) (high_thd >> 8);
    high_lsb = (uint8_t) (high_thd & 0x00ff);
    low_msb  = (uint8_t) (low_thd >> 8);
    low_lsb  = (uint8_t) (low_thd & 0x00ff);

    elan_sensor_I2C_Write(client,REG_2,W_SINGLE_BYTE,0x02,high_lsb);
    elan_sensor_I2C_Write(client,REG_3,W_SINGLE_BYTE,0x02,high_msb);
    elan_sensor_I2C_Write(client,REG_4,W_SINGLE_BYTE,0x02,low_lsb);
    elan_sensor_I2C_Write(client,REG_5,W_SINGLE_BYTE,0x02,low_msb);

    return ret;
}

#if PS_INTERRUPT_MODE
static void epl_sensor_irq_do_work(struct work_struct *work)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;
    int mode = 0;
    LOG_FUN();

    elan_sensor_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_LOCK);

    elan_sensor_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
    elan_sensor_I2C_Read(client);
    mode = gRawData.raw_bytes[0]&(3<<4);

    // 0x10 is ps mode
    if(mode==CMC_MODE_PS && epld->enable_pflag)
    {
        gRawData.ps_int_state= !((gRawData.raw_bytes[0]&0x04)>>2);
        elan_epl_ps_poll_rawdata();
    }
    else
    {
        LOG_INFO("interrupt in als\n");
    }
    elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
    elan_sensor_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
    enable_irq(epld->irq);
}

static irqreturn_t elan_sensor_irq_handler(int irqNo, void *handle)
{
    struct elan_epl_data *epld = (struct elan_epl_data*)handle;

    disable_irq_nosync(epld->irq);
    queue_work(epld->epl_wq, &epl_sensor_irq_work);

    return IRQ_HANDLED;
}
#endif

static void report_polling_do_work(struct work_struct *work)
{
    struct elan_epl_data *epld = epl_data;

    if(dual_count==CMC_MODE_PS)
    {
        elan_epl_ps_poll_rawdata();
    }
    else if(dual_count==CMC_MODE_ALS)
    {
        elan_epl_als_rawdata();
    }

    if(epld->enable_pflag)
    {
        elan_sensor_psensor_enable(epld);

        if(PS_INTERRUPT_MODE==0)
        {
            dual_count=CMC_MODE_PS; // ps mode
            queue_delayed_work(epld->epl_wq, &report_polling_work,msecs_to_jiffies(PS_POLLING_RATE));
        }
    }
}

static void polling_do_work(struct work_struct *work)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;

    bool isInterleaving = epld->enable_pflag==1 && epld->enable_lflag==1;
    bool isAlsOnly = epld->enable_pflag==0 && epld->enable_lflag==1;
    bool isPsOnly = epld->enable_pflag==1 && epld->enable_lflag==0;

    cancel_delayed_work(&polling_work);
    cancel_delayed_work(&report_polling_work);

    LOG_INFO("enable_pflag = %d, enable_lflag = %d\n", epld->enable_pflag, epld->enable_lflag);

    if(isAlsOnly || isInterleaving)
    {
        elan_sensor_lsensor_enable(epld);
        dual_count = CMC_MODE_ALS;

        queue_delayed_work(epld->epl_wq, &report_polling_work,msecs_to_jiffies(ALS_DELAY));
        queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(ALS_POLLING_RATE));
    }
    else if(isPsOnly)
    {
        elan_sensor_psensor_enable(epld);
        dual_count = CMC_MODE_PS;

        if(PS_INTERRUPT_MODE)
        {
            // do nothing
        }
        else
        {
            queue_delayed_work(epld->epl_wq, &report_polling_work,msecs_to_jiffies(PS_DELAY));
            queue_delayed_work(epld->epl_wq, &polling_work,msecs_to_jiffies(PS_POLLING_RATE));
        }
    }
    else
    {
        elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
        elan_sensor_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,EPL_S_SENSING_MODE);
    }
}

#if 0
static ssize_t elan_ls_operationmode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    uint16_t mode=0;
    struct elan_epl_data *epld = epl_data;
    LOG_FUN();

    sscanf(buf, "%hu",&mode);
    LOG_INFO("==>[operation mode]=%d\n", mode);

    if(mode == 0)
    {
        epld->enable_lflag = 0;
        epld->enable_pflag = 0;
    }
    else if(mode == 1)
    {
        epld->enable_lflag = 1;
        epld->enable_pflag = 0;
    }
    else if(mode == 2)
    {
        epld->enable_lflag = 0;
        epld->enable_pflag = 1;
    }
    else if(mode == 3)
    {
        epld->enable_lflag = 1;
        epld->enable_pflag = 1;
    }
    else
    {
        LOG_INFO("0: none\n1: als only\n2: ps only\n3: interleaving");
    }

    elan_sensor_restart_work();
    return count;
}

static ssize_t elan_ls_operationmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct elan_epl_data *epld = epl_data;
    long *tmp = (long*)buf;
    uint16_t mode =0;
    LOG_FUN();

    if(  epld->enable_pflag==0 &&  epld->enable_lflag==0)
    {
        mode = 0;
    }
    else if(  epld->enable_pflag==0 &&  epld->enable_lflag==1)
    {
        mode = 1;
    }
    else if(  epld->enable_pflag==1 && epld->enable_lflag==0)
    {
        mode = 2;
    }
    else if(  epld->enable_pflag==1 && epld->enable_lflag==1)
    {
        mode = 3;
    }
    tmp[0] = mode;
    return sprintf(buf, "%d \n", mode);
}

static ssize_t elan_ls_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct elan_epl_data *epld = epl_data;
    u16 ch1;

    if(!epl_data)
    {
        LOG_INFO("epl_data is null!!\n");
        return 0;
    }
    elan_sensor_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_LOCK);

    elan_sensor_I2C_Write(epld->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_sensor_I2C_Read(epld->client);
    ch1 = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
    LOG_INFO("ch1 raw_data = %d\n", ch1);

    elan_sensor_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);

    return sprintf(buf, "%d\n", ch1);
}

static DEVICE_ATTR(elan_ls_operationmode, S_IROTH|S_IWOTH, elan_ls_operationmode_show,elan_ls_operationmode_store);
static DEVICE_ATTR(elan_ls_status, S_IROTH|S_IWOTH, elan_ls_status_show,NULL);

static struct attribute *ets_attributes[] =
{
    &dev_attr_elan_ls_operationmode.attr,
    &dev_attr_elan_ls_status.attr,
    NULL,
};

static struct attribute_group ets_attr_group =
{
    .attrs = ets_attributes,
};
#endif

#if 0   //ices add
static int elan_als_open(struct inode *inode, struct file *file)
{
    struct elan_epl_data *epld = epl_data;

    LOG_FUN();

    if (epld->als_opened)
    {
        return -EBUSY;
    }
    epld->als_opened = 1;

    return 0;
}

static int elan_als_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    struct elan_epl_data *epld = epl_data;
    int buf[1];
    if(epld->read_flag ==1)
    {
        buf[0] = gRawData.als_ch1_raw;
        if(copy_to_user(buffer, &buf , sizeof(buf)))
            return 0;
        epld->read_flag = 0;
        return 12;
    }
    else
    {
        return 0;
    }
}

static int elan_als_release(struct inode *inode, struct file *file)
{
    struct elan_epl_data *epld = epl_data;

    LOG_FUN();

    epld->als_opened = 0;

    return 0;
}
#if 1
static long elan_als_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int flag;
    unsigned long buf[1];
    struct elan_epl_data *epld = epl_data;

    void __user *argp = (void __user *)arg;

    LOG_INFO("als io ctrl cmd %d\n", _IOC_NR(cmd));

    switch(cmd)
    {
        case ELAN_EPL6800_IOCTL_GET_LFLAG:

            LOG_INFO("elan ambient-light IOCTL Sensor get lflag \n");
            flag = epld->enable_lflag;
            if (copy_to_user(argp, &flag, sizeof(flag)))
                return -EFAULT;

            LOG_INFO("elan ambient-light Sensor get lflag %d\n",flag);
            break;

        case ELAN_EPL6800_IOCTL_ENABLE_LFLAG:

            LOG_INFO("elan ambient-light IOCTL Sensor set lflag \n");
            if (copy_from_user(&flag, argp, 1))
                return -EFAULT;
			LOG_INFO("elan ambient-light Sensor set lflag %d\n",flag);
			if(flag) {
				flag = 1;
			}else {
				flag = 0;
			}
            //if (flag < 0 || flag > 1)
            //    return -EINVAL;

            epld->enable_lflag = flag;
            elan_sensor_restart_work();

            //LOG_INFO("elan ambient-light Sensor set lflag %d\n",flag);
            break;

        case ELAN_EPL6800_IOCTL_GETDATA:
            buf[0] = (unsigned long)gRawData.als_ch1_raw;
            if(copy_to_user(argp, &buf , 2))
                return -EFAULT;

            break;

        default:
            LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
            return -EINVAL;
    }

    return 0;

}
#endif
static unsigned int elan_als_poll(struct file *fp, poll_table * wait)
{
	if(ls_data_changed){
		//pr_info("%s, ls_data_changed 1st, ls_data = 0x%x\n", __func__, ls_data);
		ls_data_changed = 0;
		//mutex_unlock(&stk3x1x_mutex);
		return POLLIN | POLLRDNORM;
	}
	poll_wait(fp, &ls_waitqueue, wait);
	//mutex_unlock(&stk3x1x_mutex);
	return 0;
}

static struct file_operations elan_als_fops =
{
    .owner = THIS_MODULE,
    .open = elan_als_open,
    .read = elan_als_read,
    .release = elan_als_release,
    .unlocked_ioctl = elan_als_ioctl,
    .poll = elan_als_poll,
};

static struct miscdevice elan_als_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "elan_als",
    .fops = &elan_als_fops
};
#endif  //ices end

static int elan_ps_open(struct inode *inode, struct file *file)
{
    struct elan_epl_data *epld = epl_data;

    LOG_FUN();

    if (epld->ps_opened)
        return -EBUSY;

    epld->ps_opened = 1;

    return 0;
}

static int elan_ps_release(struct inode *inode, struct file *file)
{
    struct elan_epl_data *epld = epl_data;

    LOG_FUN();

    epld->ps_opened = 0;

    psensor_mode_suspend = 0;

    return 0;
}

static int epl2182_read_chip_info(struct i2c_client *client, char *buf)
{
        if(NULL == buf) {
                return -1;
        }
        if(NULL == client) {
                *buf = 0;
                return -2;
        }

        sprintf(buf, "EPL2182");
        printk("[EPL2182] epl2182_read_chip_info %s\n",buf);
        return 0;
}

static long elan_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int flag, err;
    unsigned long buf;
    char strbuf[256];
    struct elan_epl_data *epld = epl_data;
    void __user *argp = (void __user *)arg;

    LOG_INFO("ps io ctrl cmd %d\n", _IOC_NR(cmd));
    //ioctl message handle must define by android sensor library (case by case)
    switch(cmd)
    {
        case ELAN_EPL6800_IOCTL_GET_PFLAG:
            flag = epld->enable_pflag;
            if (copy_to_user(argp, &flag, sizeof(flag)))
                return -EFAULT;
            LOG_INFO("elan Proximity Sensor get pflag %d\n",flag);
            break;

        case ELAN_EPL6800_IOCTL_ENABLE_PFLAG:
            if (copy_from_user(&flag, argp, sizeof(flag)))
                return -EFAULT;
            LOG_INFO("elan Proximity Sensor set pflag %d\n",flag);
            if(flag) {
		    flag = 1;
            }else {
		    flag = 0;
            }
            epld->enable_pflag = flag;
            elan_sensor_restart_work();
            break;

        case ELAN_EPL6800_IOCTL_GET_CHIPINFO:
                err = epl2182_read_chip_info(this_client, strbuf);
                if(err < 0)
                        return -EFAULT;
                if(copy_to_user(argp, strbuf, strlen(strbuf)+1))
                        return -EFAULT;
            break;

        case ELAN_EPL6800_IOCTL_GET_LFLAG:
            flag = epld->enable_lflag;
            if (copy_to_user(argp, &flag, sizeof(flag)))
                return -EFAULT;
            LOG_INFO("elan ambient-light Sensor get lflag %d\n",flag);
            break;

        case ELAN_EPL6800_IOCTL_ENABLE_LFLAG:
            if (copy_from_user(&flag, argp, sizeof(flag)))
                return -EFAULT;
            LOG_INFO("elan ambient-light Sensor set lflag %d\n",flag);
            if(flag) {
		    flag = 1;
            }else {
		    flag = 0;
            }
            epld->enable_lflag = flag;
            elan_sensor_restart_work();
            break;

        case ELAN_EPL6800_IOCTL_GETDATA:
            buf = (unsigned long)gRawData.als_ch1_raw;
            if(copy_to_user(argp, &buf , sizeof(buf)))
                return -EFAULT;
            LOG_INFO("elan als Sensor get data (%lu)\n",buf);
            break;

        default:
            LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
            return -EINVAL;
    }

    return 0;
}

static unsigned int elan_ps_poll(struct file *fp, poll_table * wait)
{
	if(ps_data_changed) {
		ps_data_changed = 0;
		return POLLIN | POLLRDNORM;
	}
	poll_wait(fp, &ps_waitqueue, wait);
	return 0;
}


static struct file_operations elan_ps_fops =
{
    .owner = THIS_MODULE,
    .open = elan_ps_open,
    .release = elan_ps_release,
    .unlocked_ioctl = elan_ps_ioctl,
    .poll		= elan_ps_poll,
};

static struct miscdevice elan_ps_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = EPL2182_PLS_DEVICE,
    .fops = &elan_ps_fops
};

static int initial_sensor(struct elan_epl_data *epld)
{
    struct i2c_client *client = epld->client;

    int ret = 0;

    LOG_INFO("initial_sensor enter!\n");

    ret = elan_sensor_I2C_Read(client);

    if(ret < 0)
        return -EINVAL;

    elan_sensor_I2C_Write(client,REG_0,W_SINGLE_BYTE,0x02, EPL_S_SENSING_MODE);
    elan_sensor_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
    set_psensor_intr_threshold(P_SENSOR_LTHD , P_SENSOR_HTHD);

    msleep(2);

    epld->enable_lflag = 0;
    epld->enable_pflag = 0;

    return ret;
}
#if 0   //ices add
/*----------------------------------------------------------------------------*/
static ssize_t light_enable_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct elan_epl_data *epld  = epl_data;
	printk("%s: ALS_status=%d\n", __func__, epld->enable_lflag);
	return sprintf(buf, "%d\n", epld->enable_lflag);
}

static ssize_t light_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct elan_epl_data *epld = epl_data;

    LOG_INFO("light_enable_store: enable=%s \n", buf);

	if (sysfs_streq(buf, "1"))
		epld->enable_lflag= 1;
	else if (sysfs_streq(buf, "0"))
		epld->enable_lflag= 0;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return 0;
	}

	elan_sensor_restart_work();
	return size;
}

/*----------------------------------------------------------------------------*/
static struct device_attribute dev_attr_light_enable =
__ATTR(enable, S_IRWXUGO,
	   light_enable_show, light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/

static int lightsensor_setup(struct elan_epl_data *epld)
{
    int err = 0;
    LOG_INFO("lightsensor_setup enter.\n");

    epld->als_input_dev = input_allocate_device();
    if (!epld->als_input_dev)
    {
        LOG_ERR( "could not allocate ls input device\n");
        return -ENOMEM;
    }
    epld->als_input_dev->name = ElanALsensorName;
    set_bit(EV_ABS, epld->als_input_dev->evbit);
    input_set_abs_params(epld->als_input_dev, ABS_MISC, 0, 9, 0, 0);

    err = input_register_device(epld->als_input_dev);
    if (err < 0)
    {
        LOG_ERR("can not register ls input device\n");
        goto err_free_ls_input_device;
    }
#if 0   //ices add
    err = misc_register(&elan_als_device);
    if (err < 0)
    {
        LOG_ERR("can not register ls misc device\n");
        goto err_unregister_ls_input_device;
    }
#endif  //ices end
    err = sysfs_create_group(&epld->als_input_dev->dev.kobj, &light_attribute_group);

	if (err) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_free_ls_input_device;
	}

    return err;

err_unregister_ls_input_device:
    input_unregister_device(epld->als_input_dev);
err_free_ls_input_device:
    input_free_device(epld->als_input_dev);
    return err;
}
#endif
/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct elan_epl_data *epld = epl_data;
	printk("%s: PS status=%d\n", __func__, epld->enable_pflag);
	LOG_INFO("epl2182_setup_psensor enter.\n");
	return sprintf(buf, "%d\n", epld->enable_pflag);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct elan_epl_data *epld = epl_data;

    LOG_INFO("proximity_enable_store: enable=%s \n", buf);
	if (sysfs_streq(buf, "1"))
		epld->enable_pflag =1;
	else if (sysfs_streq(buf, "0"))
	    epld->enable_pflag =0;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return 0;
	}

	elan_sensor_restart_work();
	return size;
}
/*----------------------------------------------------------------------------*/
static struct device_attribute dev_attr_ps_enable =
__ATTR(enable, S_IRWXUGO,
	   proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_ps_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/

static int psensor_setup(struct elan_epl_data *epld)
{
    int err = 0;
    LOG_INFO("psensor_setup enter.\n");

    epld->ps_input_dev = input_allocate_device();
    if (!epld->ps_input_dev)
    {
        LOG_ERR("could not allocate ps input device\n");
        return -ENOMEM;
    }
    epld->ps_input_dev->name = ElanPsensorName;

    set_bit(EV_ABS, epld->ps_input_dev->evbit);
    input_set_abs_params(epld->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);
#if 1
    set_bit(EV_ABS, epld->ps_input_dev->evbit);
    input_set_abs_params(epld->ps_input_dev, ABS_MISC, 0, 9, 0, 0);
#endif

    err = input_register_device(epld->ps_input_dev);
    if (err < 0)
    {
        LOG_ERR("could not register ps input device\n");
        goto err_free_ps_input_device;
    }

    err = misc_register(&elan_ps_device);
    if (err < 0)
    {
        LOG_ERR("could not register ps misc device\n");
        goto err_unregister_ps_input_device;
    }

    err = sysfs_create_group(&epld->ps_input_dev->dev.kobj, &proximity_attribute_group);
	if (err) {
		pr_err("%s: PS could not create sysfs group\n", __func__);
		goto err_free_ps_input_device;
	}

    return err;

err_unregister_ps_input_device:
    input_unregister_device(epld->ps_input_dev);
err_free_ps_input_device:
    input_free_device(epld->ps_input_dev);
    return err;
}

#if PS_INTERRUPT_MODE
static int setup_interrupt(struct elan_epl_data *epld)
{
    struct i2c_client *client = epld->client;
	struct elan_epl_platform_data *pdata = client->dev.platform_data;

    int err = 0;
    msleep(5);
#if 1
    err = gpio_request(pdata->irq_gpio_number, "Elan EPL IRQ");
    if (err)
    {
        LOG_ERR("gpio pin request fail (%d)\n", err);
        goto initial_fail;
    }
    else
    {

		gpio_direction_input(pdata->irq_gpio_number);

		/*get irq*/
		client->irq = gpio_to_irq(pdata->irq_gpio_number);
		epld->irq = client->irq;

		printk("IRQ number is %d\n", client->irq);

    }
#endif
    err = request_irq(epld->irq,elan_sensor_irq_handler, IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND ,
                      client->dev.driver->name, epld);
    if(err <0)
    {
        LOG_ERR("request irq pin %d fail for gpio\n",err);
        goto fail_free_intr_pin;
    }

    return err;

initial_fail:
fail_free_intr_pin:
    gpio_free(epld->intr_pin);
    return err;
}
#endif

#ifdef CONFIG_SUSPEND
static int elan_sensor_suspend(struct i2c_client *client, pm_message_t mesg)
{
    LOG_FUN();

    if(epl_data->enable_pflag==0 )
    {
        elan_sensor_I2C_Write(client,REG_7, W_SINGLE_BYTE, 0x02, EPL_C_P_DOWN);
        cancel_delayed_work(&polling_work);
    }
    return 0;
}

static int elan_sensor_resume(struct i2c_client *client)
{
    struct elan_epl_data *epld = epl_data;
    LOG_FUN();

    if(epld->enable_pflag | epld->enable_lflag)
    {
        elan_sensor_I2C_Write(client,REG_7, W_SINGLE_BYTE, 0x02, EPL_C_P_UP);
    }

    if(epld->enable_pflag)
    {
        elan_sensor_restart_work();
    }
    return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void elan_sensor_early_suspend(struct early_suspend *h)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;
    LOG_FUN();

    if( epld->enable_pflag==0)
    {
        elan_sensor_I2C_Write(client,REG_7, W_SINGLE_BYTE, 0x02, EPL_C_P_DOWN);
        cancel_delayed_work(&polling_work);
    }

    psensor_mode_suspend = 1;
}

static void elan_sensor_late_resume(struct early_suspend *h)
{
    struct elan_epl_data *epld = epl_data;
    struct i2c_client *client = epld->client;

    LOG_FUN();

    if(epld->enable_pflag | epld->enable_lflag)
    {
        elan_sensor_I2C_Write(client,REG_7, W_SINGLE_BYTE, 0x02, EPL_C_P_UP);
    }

    if(epld->enable_pflag || epld->enable_lflag)
    {
        elan_sensor_restart_work();
    }
    psensor_mode_suspend = 0;
}
#endif

static int elan_sensor_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int err = 0;
    int chip_id = 0;
    struct elan_epl_data *epld ;
    struct elan_epl_platform_data *pdata = client->dev.platform_data;
    //static struct platform_device *sensor_dev;
    struct device_node *np = client->dev.of_node;
    LOG_INFO("elan sensor probe enter.\n");
#ifdef CONFIG_OF
        if (np && !pdata){
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Could not allocate struct elan_epl_platform_data");
			goto exit_allocate_pdata_failed;
		}
		pdata->irq_gpio_number = of_get_gpio(np, 0);
		if(pdata->irq_gpio_number < 0){
			dev_err(&client->dev, "fail to get irq_gpio_number\n");
			kfree(pdata);
			goto exit_irq_gpio_read_fail;
		}
		client->dev.platform_data = pdata;
	}
#endif

    epld = kzalloc(sizeof(struct elan_epl_data), GFP_KERNEL);
    if (!epld) {
        err = -ENOMEM;
        LOG_ERR("kzalloc elan_epl_data failed!\n");
        goto exit_kzalloc_epld_failed;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        dev_err(&client->dev,"No supported i2c func what we need?!!\n");
        err = -ENOTSUPP;
        goto i2c_fail;
    }
    chip_id = i2c_smbus_read_byte_data(client, 0x00);
    if (chip_id < 0)
    {
        dev_err(&client->dev,"read chip id REG 0x00 failed\n");
        err = -ENODEV;
        goto exit_read_chipid_failed;
    }
    LOG_INFO("chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
    LOG_INFO("chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
    LOG_INFO("chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
    LOG_INFO("chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
    LOG_INFO("chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
    LOG_INFO("chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
    LOG_INFO("chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
    LOG_INFO("chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
    LOG_INFO("chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
    LOG_INFO("chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
    LOG_INFO("chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
    LOG_INFO("chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
    LOG_INFO("chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
    LOG_INFO("chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
    LOG_INFO("chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));

    epld->client = client;
    this_client = client;
    epld->irq = client->irq;
    i2c_set_clientdata(client, epld);

    epl_data = epld;

    epld->epl_wq = create_singlethread_workqueue(EPL2182_PLS_DEVICE);
    if (!epld->epl_wq)
    {
        LOG_ERR("can't create workqueue\n");
        err = -ENOMEM;
        goto err_create_singlethread_workqueue;
    }
#if 0   //ices add
    err = lightsensor_setup(epld);
    if (err < 0)
    {
        LOG_ERR("lightsensor_setup error!!\n");
        goto err_lightsensor_setup;
    }
#endif
    err = psensor_setup(epld);
    if (err < 0)
    {
        LOG_ERR("psensor_setup error!!\n");
        goto err_psensor_setup;
    }

    err = initial_sensor(epld);
    if (err < 0)
    {
        LOG_ERR("fail to initial sensor (%d)\n", err);
        goto err_sensor_setup;
    }

#if PS_INTERRUPT_MODE
    err = setup_interrupt(epld);
    if (err < 0)
    {
        LOG_ERR("setup error!\n");
        goto err_sensor_setup;
    }
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
    epld->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    epld->early_suspend.suspend = elan_sensor_early_suspend;
    epld->early_suspend.resume = elan_sensor_late_resume;
    register_early_suspend(&epld->early_suspend);
#endif

#if 0
    wake_lock_init(&g_ps_wlock, WAKE_LOCK_SUSPEND, "ps_wakelock");

    sensor_dev = platform_device_register_simple("elan_alsps", -1, NULL, 0);
    if (IS_ERR(sensor_dev))
    {
        printk ("sensor_dev_init: error\n");
        goto err_fail;
    }

    err = sysfs_create_group(&sensor_dev->dev.kobj, &ets_attr_group);
    if (err !=0)
    {
        dev_err(&client->dev,"%s:create sysfs group error", __func__);
        goto err_fail;
    }
#endif

    LOG_INFO("sensor probe success.\n");

    return err;

err_fail:
    //input_unregister_device(epld->als_input_dev);
    input_unregister_device(epld->ps_input_dev);
    //input_free_device(epld->als_input_dev);
    input_free_device(epld->ps_input_dev);
//err_lightsensor_setup:
err_psensor_setup:
err_sensor_setup:
    destroy_workqueue(epld->epl_wq);
    misc_deregister(&elan_ps_device);
//    misc_deregister(&elan_als_device);    //ices add
err_create_singlethread_workqueue:
exit_read_chipid_failed:
i2c_fail:
//err_platform_data_null:
    kfree(epld);
exit_kzalloc_epld_failed:
#ifdef CONFIG_OF
exit_irq_gpio_read_fail:
exit_allocate_pdata_failed:
#endif
    return err;
}

static int elan_sensor_remove(struct i2c_client *client)
{
    struct elan_epl_data *epld = i2c_get_clientdata(client);

    dev_dbg(&client->dev, "%s: enter.\n", __func__);

    unregister_early_suspend(&epld->early_suspend);
    //input_unregister_device(epld->als_input_dev);
    input_unregister_device(epld->ps_input_dev);
    //input_free_device(epld->als_input_dev);
    input_free_device(epld->ps_input_dev);
    misc_deregister(&elan_ps_device);
//    misc_deregister(&elan_als_device);    //ices add
    free_irq(epld->irq,epld);
    destroy_workqueue(epld->epl_wq);
    kfree(epld);
    return 0;
}

static const struct i2c_device_id elan_sensor_id[] =
{
    { EPL2182_PLS_DEVICE, 0 },
    { }
};

static const struct of_device_id epl2182_of_match[] = {
       { .compatible = "ELAN,epl2182_pls", },
        {}
};
MODULE_DEVICE_TABLE(of, epl2182_of_match);

static struct i2c_driver elan_sensor_driver =
{
    .probe	= elan_sensor_probe,
    .remove	= elan_sensor_remove,
    .id_table	= elan_sensor_id,
    .driver	= {
        .name = EPL2182_PLS_DEVICE,
        .owner = THIS_MODULE,
        .of_match_table = epl2182_of_match,
    },
#ifdef CONFIG_SUSPEND
    .suspend = elan_sensor_suspend,
    .resume = elan_sensor_resume,
#endif
};

static int __init elan_sensor_init(void)
{
    return i2c_add_driver(&elan_sensor_driver);
}

static void __exit  elan_sensor_exit(void)
{
    i2c_del_driver(&elan_sensor_driver);
}

module_init(elan_sensor_init);
module_exit(elan_sensor_exit);

MODULE_AUTHOR("Renato Pan <renato.pan@eminent-tek.com>");
MODULE_DESCRIPTION("ELAN epl2182 driver");
MODULE_LICENSE("GPL");

