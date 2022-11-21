/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * File Name          : lis3dh_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Carmine Iascone (carmine.iascone@st.com)
 *		      : Matteo Dameno (matteo.dameno@st.com)
 * Version            : V.1.0.5
 * Date               : 16/08/2010
 * Description        : LIS3DH accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *
 ******************************************************************************
 Revision 1.0.0 05/11/09
 First Release
 Revision 1.0.3 22/01/2010
  Linux K&R Compliant Release;
 Revision 1.0.5 16/08/2010
 modified _get_acceleration_data function
 modified _update_odr function
 manages 2 interrupts

 ******************************************************************************/

#include	<linux/module.h>
#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>

#include	<linux/input.h>
#include	<linux/input-polldev.h>
#include	<linux/miscdevice.h>
#include	<linux/uaccess.h>
#include        <linux/slab.h>

#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include  <linux/earlysuspend.h>
#endif

#include <linux/i2c/lis3dh.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>

#include <soc/sprd/i2c-sprd.h>

#define	INTERRUPT_MANAGEMENT 1

#define	G_MAX		16000	/** Maximum polled-device-reported g value */

/*
#define	SHIFT_ADJ_2G		4
#define	SHIFT_ADJ_4G		3
#define	SHIFT_ADJ_8G		2
#define	SHIFT_ADJ_16G		1
*/
#define GSENSOR_INTERRUPT_MODE

#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/

#define	HIGH_RESOLUTION		0x08

#define	AXISDATA_REG		0x28
#define WHOAMI_LIS3DH_ACC	0x33	/*      Expctd content for WAI  */

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		0x0F	/*      WhoAmI register         */
#define	TEMP_CFG_REG		0x1F	/*      temper sens control reg */
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		0x20	/*      control reg 1           */
#define	CTRL_REG2		0x21	/*      control reg 2           */
#define	CTRL_REG3		0x22	/*      control reg 3           */
#define	CTRL_REG4		0x23	/*      control reg 4           */
#define	CTRL_REG5		0x24	/*      control reg 5           */
#define	CTRL_REG6		0x25	/*      control reg 6           */

#define	FIFO_CTRL_REG		0x2E	/*      FiFo control reg        */

#define	INT_CFG1		0x30	/*      interrupt 1 config      */
#define	INT_SRC1		0x31	/*      interrupt 1 source      */
#define	INT_THS1		0x32	/*      interrupt 1 threshold   */
#define	INT_DUR1		0x33	/*      interrupt 1 duration    */

#define	INT_CFG2		0x34	/*      interrupt 2 config      */
#define	INT_SRC2		0x35	/*      interrupt 2 source      */
#define	INT_THS2		0x36	/*      interrupt 2 threshold   */
#define	INT_DUR2		0x37	/*      interrupt 2 duration    */

#define	TT_CFG			0x38	/*      tap config              */
#define	TT_SRC			0x39	/*      tap source              */
#define	TT_THS			0x3A	/*      tap threshold           */
#define	TT_LIM			0x3B	/*      tap time limit          */
#define	TT_TLAT			0x3C	/*      tap time latency        */
#define	TT_TW			0x3D	/*      tap time window         */
/*	end CONTROL REGISTRES	*/

#define ENABLE_HIGH_RESOLUTION	1

#define LIS3DH_ACC_PM_OFF		0x00
#define LIS3DH_ACC_ENABLE_ALL_AXES	0x07

#define PMODE_MASK			0x08
#define ODR_MASK			0XF0

#define ODR1		0x10	/* 1Hz output data rate */
#define ODR10		0x20	/* 10Hz output data rate */
#define ODR25		0x30	/* 25Hz output data rate */
#define ODR50		0x40	/* 50Hz output data rate */
#define ODR100		0x50	/* 100Hz output data rate */
#define ODR200		0x60	/* 200Hz output data rate */
#define ODR400		0x70	/* 400Hz output data rate */
#define ODR1250		0x90	/* 1250Hz output data rate  : test actually 650hz*/ 

#define	IA			0x40
#define	ZH			0x20
#define	ZL			0x10
#define	YH			0x08
#define	YL			0x04
#define	XH			0x02
#define	XL			0x01
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	0x40
#define	CTRL_REG6_I2_TAPEN	0x80
/*
CTRL_REG3 (22h)  的I1_DRDY1 位  enable
CTRL_REG5 (24h)   LIR_INT1   1 为 触发中断 后 中断保持，0  为 脉冲 
CTRL_REG6 (25h)    0  为中断 高有效， 1  为中断低有效
********must read data then will interrupter,******
*/
#define	CTRL_REG3_I1_DRDY1	0x10  //DRDY1 interrupt on INT1
#define	CTRL_REG5_LIR_INT1        0x08  //latch interrupt
//#define	CTRL_REG6_HLACTIVE	0x02  //low
#define	CTRL_REG6_HLACTIVE	0x00  //high


/* */

/* TAP_SOURCE_REG BIT */
#define	DTAP			0x20
#define	STAP			0x10
#define	SIGNTAP			0x08
#define	ZTAP			0x04
#define	YTAP			0x02
#define	XTAZ			0x01

#define	FUZZ			32
#define	FLAT			32
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8
#define	RES_INT_CFG2		9
#define	RES_INT_THS2		10
#define	RES_INT_DUR2		11

#define	RES_TT_CFG		12
#define	RES_TT_THS		13
#define	RES_TT_LIM		14
#define	RES_TT_TLAT		15
#define	RES_TT_TW		16

#define	RES_TEMP_CFG_REG	17
#define	RES_REFERENCE_REG	18
#define	RES_FIFO_CTRL_REG	19

#define	RESUME_ENTRIES		20
#define DEVICE_INFO             "ST, LIS3DH"
#define DEVICE_INFO_LEN         32
/* end RESUME STATE INDICES */

//#define GSENSOR_GINT1_GPI 238
#define GSENSOR_GINT2_GPI 1

#define LOG_TAG				        "[LIS3DH] "
#define LOG_FUN( )			        printk(KERN_INFO LOG_TAG"%s\n", __FUNCTION__)
#define LOG_INFO(fmt, args...)		printk(KERN_INFO LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)		printk(KERN_ERR  LOG_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

//#define LIS3DH_DBG
#define GSENSOR_INTERRUPT_MODE
#define USE_WAIT_QUEUE 1
#if USE_WAIT_QUEUE
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int gsensor_flag = 0;
#endif

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis3dh_acc_odr_table[] = {
	{
	1, ODR1250}, {
	2, ODR400}, {
	5, ODR200}, {
	10, ODR100}, {
	20, ODR50}, {
	40, ODR25}, {
	100, ODR10}, {
	1000, ODR1},};

struct lis3dh_acc_data {
	struct i2c_client *client;
	struct lis3dh_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

//	int irq1_gpio_number;
	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef LIS3DH_DBG
struct {
		u8 diag_reg_addr_wr;
		u8 diag_reg_val_wr;
	} debug;
	u64 data_num;
#endif
};

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct lis3dh_acc_data *lis3dh_acc_misc_data;
struct i2c_client *lis3dh_i2c_client;

#ifndef GSENSOR_INTERRUPT_MODE 
static struct workqueue_struct *_gSensorWork_workqueue = NULL;
#endif

static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,int *xyz);
static void lis3dh_acc_report_values(struct lis3dh_acc_data *acc, int *xyz);

static int lis3dh_acc_i2c_read(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,},
		{
		 .addr = acc->client->addr,
		 .flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = len,
		 .buf = buf,},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2){
			LOG_ERR("************* exception read transfer error,tries=%d********\n",tries);
			msleep_interruptible(I2C_RETRY_DELAY);
		}
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		LOG_ERR(" exception read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_i2c_write(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = { {.addr = acc->client->addr,
				   .flags = acc->client->flags & I2C_M_TEN,
				   .len = len + 1,.buf = buf,},
	};
	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


static int lis3dh_acc_register_read(struct lis3dh_acc_data *acc, u8 * buf,
				    u8 reg_address)
{
	int err = -1;
	buf[0] = (reg_address);
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	return err;
}

static void lis3dh_read_interrupt_reg(struct lis3dh_acc_data *acc)
{      
	  u8 value = 0;
	   int err;

	err = lis3dh_acc_register_read(acc, &value, CTRL_REG3);
	LOG_INFO("REG content: =======================>\n");
	if (err >= 0)
		LOG_INFO("REG content: [CTRL_REG3:0x%x]= 0x%02x\n", CTRL_REG3, value);

	err = lis3dh_acc_register_read(acc, &value, CTRL_REG5);
	if (err >= 0)
		LOG_INFO("REG content: [CTRL_REG5:0x%x]= 0x%02x\n", CTRL_REG5, value);
	err = lis3dh_acc_register_read(acc, &value, CTRL_REG6);
	if (err >= 0)
		LOG_INFO("REG content: [CTRL_REG6:0x%x]= 0x%02x\n", CTRL_REG6, value);

	LOG_INFO("REG content:< =======================\n");

}



/*  if  CTM want to turn off vdd & vddio , we recommend to call this function to retrive register setting   */

static int lis3dh_acc_hw_init(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	pr_debug("%s: hw init start\n", LIS3DH_ACC_DEV_NAME);
	LOG_INFO("sprd-gsensor: -- %s entry  :acc-> pdata ->gpio_int1=%d-- !\n",__func__,gpio_get_value(acc-> pdata ->gpio_int1));

	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0)
		goto error_firstread;
	else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_LIS3DH_ACC) {
		err = -1;	/* choose the right coded error */
		goto error_unknown_device;
	}

	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lis3dh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto error1;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto error1;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS2);
	buf[1] = acc->resume_state[RES_INT_THS2];
	buf[2] = acc->resume_state[RES_INT_DUR2];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto error1;
	buf[0] = INT_CFG2;
	buf[1] = acc->resume_state[RES_INT_CFG2];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lis3dh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto error1;

	acc->hw_initialized = 1;

	LOG_INFO("sprd-gsensor: -- %s exit :acc-> pdata ->gpio_int1=%d-- !\n",__func__,gpio_get_value(acc-> pdata ->gpio_int1));

	pr_debug("%s: hw init done\n", LIS3DH_ACC_DEV_NAME);
	return 0;

error_firstread:
	acc->hw_working = 0;
	dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
		 "available/working?\n");
	goto error1;
error_unknown_device:
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%x,"
		" Replies: 0x%x\n", WHOAMI_LIS3DH_ACC, buf[0]);
error1:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
		buf[1], err);
	return err;
}

static void lis3dh_acc_device_power_off(struct lis3dh_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LIS3DH_ACC_PM_OFF };
	int xyz[3] = { 0 };

	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);
#ifdef GSENSOR_INTERRUPT_MODE
/*
read data:clear interrupter
*/
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
#endif
	LOG_INFO("sprd-gsensor: -- %s  acc-> pdata ->gpio_int1=%d  acc->hw_initialized=%d-- !\n",__func__,gpio_get_value(acc-> pdata ->gpio_int1),acc->hw_initialized);	

}

static int lis3dh_acc_device_power_on(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[2];


	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
				"power_on failed: %d\n", err);
			return err;
		}
	}

	LOG_INFO("sprd-gsensor: -- %s  acc->irq1_gpio_number=%d acc->hw_initialized=%d-- !\n",__func__,gpio_get_value(acc-> pdata ->gpio_int1),acc->hw_initialized);	
	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		return err;
	LOG_INFO("sprd-gsensor: -- %s  acc->resume_state[RES_CTRL_REG1]=0x%02x-- !\n",__func__, acc->resume_state[RES_CTRL_REG1]);


	if (!acc->hw_initialized) {
		err = lis3dh_acc_hw_init(acc);
		lis3dh_read_interrupt_reg(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dh_acc_device_power_off(acc);
			return err;
		}
	}
	return 0;
}

static irqreturn_t lis3dh_acc_isr1(int irq, void *dev)
{
#ifndef USE_WAIT_QUEUE
	struct lis3dh_acc_data *acc = dev;
#endif

#ifdef LIS3DH_DBG
	static int count_intr = 0;
	static s64 interrupt_last = 0;
	static s64 interrupt_cur = 0;
	static int poll_interval=0;
	ktime_t time_mono;

	time_mono = ktime_get();
	if (poll_interval !=lis3dh_acc_misc_data->pdata->poll_interval) {
		count_intr=1;
		poll_interval = lis3dh_acc_misc_data->pdata->poll_interval;
	      interrupt_last = ktime_to_ns(time_mono);
	}
	interrupt_cur = ktime_to_ns(time_mono);
	int interrupt_interval = interrupt_cur - interrupt_last;
	interrupt_last = interrupt_cur;

	if(interrupt_interval > (lis3dh_acc_misc_data->pdata->poll_interval*1000000) +(lis3dh_acc_misc_data->pdata->poll_interval*1000000) /4)
		LOG_INFO("***sprd-gsensor ISR  exception:count_intr=%d,interrupt_interval=%d,poll_interva=%d********\n",count_intr++,interrupt_interval,lis3dh_acc_misc_data->pdata->poll_interval);
       LOG_INFO("sprd-gsensor: -- %s entry : interrupt_interval=%d,poll_interval=%d,acc->irq1_gpio_number=%d-- >>>>>>>>>>>>>>>>>!\n",__func__,interrupt_interval,poll_interval,gpio_get_value(lis3dh_acc_misc_data->pdata->gpio_int1));	
#endif

	disable_irq_nosync(irq);
#if USE_WAIT_QUEUE
	gsensor_flag = 1;
	wake_up_interruptible(&waiter);
#else
	queue_work(acc->irq1_work_queue, &acc->irq1_work);
#endif
	pr_debug("%s: isr1 queued\n", LIS3DH_ACC_DEV_NAME);
	return IRQ_HANDLED;
}

static irqreturn_t lis3dh_acc_isr2(int irq, void *dev)
{
	struct lis3dh_acc_data *acc = dev;

//	disable_irq_nosync(irq);
	queue_work(acc->irq2_work_queue, &acc->irq2_work);

	pr_debug("%s: isr2 queued\n", LIS3DH_ACC_DEV_NAME);

	return IRQ_HANDLED;
}
//static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,int *xyz);
static void lis3dh_acc_irq1_work_func(struct work_struct *work)
{
	int xyz[3] = { 0 };
	int err;
	u8 value;
	struct lis3dh_acc_data *acc =
	    container_of(work, struct lis3dh_acc_data, irq1_work);

	/* TODO  add interrupt service procedure.
	   ie:lis3dh_acc_get_int1_source(acc); */
	;
	/*  */
	LOG_INFO("%s: isr1 queued=========>>>>>>>>>>>>>>>>>\n", __func__);
       LOG_INFO("sprd-gsensor: -- %s entry :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
	   
	err = lis3dh_acc_register_read(acc, &value, INT_SRC1);

        if (err < 0) {
	        LOG_INFO("%s, error read register INT_SRC1\n", __func__);
        }
	LOG_INFO("%s: IRQ1 triggered  INT_SRC1=0x%02x\n",__func__,value);

	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_acc_report_values(acc, xyz);


       LOG_INFO("sprd-gsensor: -- %s exit :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
	enable_irq(acc->irq1);
}


#if USE_WAIT_QUEUE

static void lis3dh_acc_update_data_func(void)
{
	int xyz[3] = { 0 };
	int err;
	struct lis3dh_acc_data *acc =lis3dh_acc_misc_data;

//       LOG_INFO("sprd-gsensor: -- %s entry :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
 	if (err < 0)
		LOG_ERR("get_acceleration_data failed*******************************\n");
	else
		lis3dh_acc_report_values(acc, xyz);
#ifdef LIS3DH_DBG
       LOG_INFO("sprd-gsensor: -- %s exit :acc->irq1_gpio_number=%d-->>>>>>>>>>>>>>>>> !\n\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
       LOG_INFO("\n");
   #endif
	enable_irq(acc->irq1);
}

static int gsensor_event_handler(void *unused)
{
#ifdef LIS3DH_DBG
	static int count_intr = 0;
	static s64 interrupt_last = 0;
	static s64 interrupt_read = 0;
	static s64 interrupt_cur = 0;
	static int poll_interval=0;
	ktime_t time_mono;
#endif
	struct sched_param param = { .sched_priority = 5};

	sched_setscheduler(current, SCHED_RR, &param);
        LOG_INFO("sprd-gsensor: -- %s entry :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(lis3dh_acc_misc_data->pdata->gpio_int1));	

	do {
#ifdef LIS3DH_DBG
	       LOG_INFO("sprd-gsensor: -- %s 1 : gsensor_flag=%d,acc->irq1_gpio_number=%d-- !\n",__func__,gsensor_flag,gpio_get_value(lis3dh_acc_misc_data->pdata->gpio_int1));	
#endif

		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, (0 != gsensor_flag));
//
#ifdef LIS3DH_DBG
		time_mono = ktime_get();
		if (poll_interval !=lis3dh_acc_misc_data->pdata->poll_interval) {
			count_intr=1;
			poll_interval = lis3dh_acc_misc_data->pdata->poll_interval;
		      interrupt_last = ktime_to_ns(time_mono);
		}
		interrupt_cur = ktime_to_ns(time_mono);
		int interrupt_interval = interrupt_cur - interrupt_last;
		interrupt_last = interrupt_cur;

		if(interrupt_interval > (lis3dh_acc_misc_data->pdata->poll_interval*1000000) +(lis3dh_acc_misc_data->pdata->poll_interval*1000000) /4)
			LOG_INFO("***sprd-gsensor SHED  exception:count_intr=%d,interrupt_interval=%d,poll_interva=%d********\n",count_intr++,interrupt_interval,lis3dh_acc_misc_data->pdata->poll_interval);

	       LOG_INFO("sprd-gsensor: -- %s 2: interrupt_interval=%d,poll_interval=%d,gsensor_flag=%d,acc->irq1_gpio_number=%d-- !\n",__func__,interrupt_interval,poll_interval,gsensor_flag,gpio_get_value(lis3dh_acc_misc_data->pdata->gpio_int1));	
#endif
		gsensor_flag = 0;
		set_current_state(TASK_RUNNING);
		lis3dh_acc_update_data_func();
#ifdef LIS3DH_DBG
             time_mono = ktime_get();
             interrupt_read=ktime_to_ns(time_mono)-interrupt_cur;
		if(interrupt_interval > (lis3dh_acc_misc_data->pdata->poll_interval*1000000) +(lis3dh_acc_misc_data->pdata->poll_interval*1000000) /4)
			LOG_INFO("***sprd-gsensor SHED  exception:count_intr=%d,interrupt_interval=%d,poll_interva=%d, interrupt_read=%lld********\n",count_intr++,interrupt_interval,lis3dh_acc_misc_data->pdata->poll_interval,interrupt_read);

             LOG_INFO("sprd-gsensor: -- %s 3: interrupt_read=%lld,poll_interval=%d,gsensor_flag=%d,acc->irq1_gpio_number=%d-- !\n",__func__,interrupt_read,poll_interval,gsensor_flag,gpio_get_value(lis3dh_acc_misc_data->pdata->gpio_int1));		    
#endif
	} while (!kthread_should_stop());

	return 0;
}
#endif


static void lis3dh_acc_irq2_work_func(struct work_struct *work)
{

	/*struct lis3dh_acc_data *acc =
	    container_of(work, struct lis3dh_acc_data, irq2_work);
	*/
	/* TODO  add interrupt service procedure.
	   ie:lis3dh_acc_get_tap_source(acc); */
	;
	/*  */

	LOG_INFO("%s: IRQ2 triggered\n", LIS3DH_ACC_DEV_NAME);
}

int lis3dh_acc_update_g_range(struct lis3dh_acc_data *acc, u8 new_g_range)
{
	int err;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS3DH_ACC_FS_MASK | HIGH_RESOLUTION;

	pr_debug("%s\n", __func__);

	switch (new_g_range) {
	case LIS3DH_ACC_G_2G:

		sensitivity = SENSITIVITY_2G;
		break;
	case LIS3DH_ACC_G_4G:

		sensitivity = SENSITIVITY_4G;
		break;
	case LIS3DH_ACC_G_8G:

		sensitivity = SENSITIVITY_8G;
		break;
	case LIS3DH_ACC_G_16G:

		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
			new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Set configuration register 4, which contains g range setting
		 *  NOTE: this is a straight overwrite because this driver does
		 *  not use any of the other configuration bits in this
		 *  register.  Should this become untrue, we will have to read
		 *  out the value and only change the relevant bits --XX----
		 *  (marked by X) */
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;

		pr_debug("%s sensitivity %d g-range %d\n", __func__, sensitivity,new_g_range);
	}

	return 0;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
		buf[0], buf[1], err);

	return err;
}

int lis3dh_acc_update_odr(struct lis3dh_acc_data *acc, int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching */
	for (i = ARRAY_SIZE(lis3dh_acc_odr_table) - 1; i >= 0; i--) {
		if (lis3dh_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
			break;
	}
	config[1] = lis3dh_acc_odr_table[i].mask;

	LOG_INFO("%s :ODR  poll_interval_ms=%d, config[1]=0x%02x,cutoff_ms %d",__func__,poll_interval_ms,config[1],lis3dh_acc_odr_table[i].cutoff_ms );

	config[1] |= LIS3DH_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = lis3dh_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return 0;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
		config[0], config[1], err);

	return err;
}

/* */

static int lis3dh_acc_register_write(struct lis3dh_acc_data *acc, u8 * buf,
				     u8 reg_address, u8 new_value)
{
	int err = -1;

	if (atomic_read(&acc->enabled)) {
		/* Sets configuration register at reg_address
		 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	}
	return err;
}

static int lis3dh_acc_register_update(struct lis3dh_acc_data *acc, u8 * buf,
				      u8 reg_address, u8 mask,
				      u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lis3dh_acc_register_read(acc, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[1];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lis3dh_acc_register_write(acc, buf, reg_address,
						updated_val);
	}
	return err;
}

/* */

static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,
					    int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_acc_i2c_read(acc, acc_data, 6);

	if (err < 0) {
		LOG_ERR("%s  exception ssssI2C read error %d\n", LIS3DH_ACC_I2C_NAME,
		       err);
		return err;
	}

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);

	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;

	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		  : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		  : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		  : (hw_d[acc->pdata->axis_map_z]));
#ifdef LIS3DH_DBG
	LOG_INFO("%s read x=%d, y=%d, z=%d\n",
	       LIS3DH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
#endif

	return err;
}

static void lis3dh_acc_report_values(struct lis3dh_acc_data *acc, int *xyz)
{
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
	#ifdef LIS3DH_DBG
		acc->data_num++;
	#endif
}

static int lis3dh_acc_enable(struct lis3dh_acc_data *acc)
{
	int err;

	pr_debug("%s: pr_debug PRINT  \n", __func__);

	LOG_INFO("sprd-gsensor: -- %s entry :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));	

	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = lis3dh_acc_device_power_on(acc);
		LOG_INFO("sprd-gsensor: -- %s err=%d-- !\n",__func__,err);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}

		LOG_INFO("sprd-gsensor: -- %s  acc->hw_initialized=%d  acc->irq1=%d-- !\n",__func__,acc->hw_initialized,acc->irq1);
		if (acc->hw_initialized) {
			if (acc->irq1 != 0){
				LOG_INFO("sprd-gsensor: -- %s  after init : acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));	
				enable_irq(acc->irq1);
			}
			if (acc->irq2 != 0)
				enable_irq(acc->irq2);
			LOG_INFO("%s: power on: irq enabled\n",
			       LIS3DH_ACC_DEV_NAME);
		}

		#ifdef LIS3DH_DBG
			acc->data_num = 0;
		#endif

       /* 
		schedule_delayed_work(&acc->input_work,
				      msecs_to_jiffies(acc->pdata->
						       poll_interval));
	*/
#ifndef GSENSOR_INTERRUPT_MODE
        queue_delayed_work(_gSensorWork_workqueue, &acc->input_work, msecs_to_jiffies(acc->pdata->poll_interval));
#endif
        }
	 LOG_INFO("sprd-gsensor: -- %s exit :acc->irq1_gpio_number=%d-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
        LOG_INFO("sprd-gsensor: -- %s -- success!\n",__func__);
	return 0;
}

static int lis3dh_acc_disable(struct lis3dh_acc_data *acc)
{
	LOG_INFO("sprd-gsensor: -- %s --acc->hw_initialized=%d\n",__func__,acc->hw_initialized);
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		cancel_delayed_work_sync(&acc->input_work);

		if (acc->pdata->power_off) {
			if (acc->irq1 != 0)
				disable_irq_nosync(acc->irq1);
			if (acc->irq2 != 0)
				disable_irq_nosync(acc->irq2);
			acc->pdata->power_off();
			acc->hw_initialized = 0;
		}
		if (acc->hw_initialized) {
			if (acc->irq1 != 0)
				disable_irq_nosync(acc->irq1);
			if (acc->irq2 != 0)
				disable_irq_nosync(acc->irq2);
	//only init once during probe, the sensor is ALWAYS power supply
	//		acc->hw_initialized = 0;
		}
		lis3dh_acc_device_power_off(acc);
	}
#ifdef LIS3DH_DBG
	LOG_INFO("******sprd-gsensor: -- %s --acc->data_num =%lld ,acc->pdata->poll_interval =%d****\n",__func__,acc->data_num,acc->pdata->poll_interval);
	acc->data_num = 0;
#endif
	return 0;
}

static int lis3dh_acc_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = lis3dh_acc_misc_data;

	return 0;
}

static long lis3dh_acc_misc_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	u8 buf[4];
	u8 mask;
	u8 reg_address;
	u8 bit_values;
	int err;
	int interval;
	int xyz[3] = { 0 };
	struct lis3dh_acc_data *acc = file->private_data;


       LOG_INFO("%s :ioctl cmd %d\n",__func__, _IOC_NR(cmd));
	switch (cmd) {
	case LIS3DH_ACC_IOCTL_GET_DELAY:
		interval = acc->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case LIS3DH_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 1000)
			return -EINVAL;
		interval = interval/2;
		acc->pdata->poll_interval = max(interval,acc->pdata->min_interval); // max the frequecy of g sensor
             LOG_INFO(" %s,acc->pdata->poll_interval =%d,interval=%d,acc->pdata->min_interval=%d\n", __func__,acc->pdata->poll_interval,interval,acc->pdata->min_interval );
		err = lis3dh_acc_update_odr(acc, acc->pdata->poll_interval);
		/* TODO: if update fails poll is still set */
		if (err < 0)
			return err;
		break;

	case LIS3DH_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval > 1)
			return -EINVAL;
		if (interval)
			err = lis3dh_acc_enable(acc);
		else
			err = lis3dh_acc_disable(acc);
		return err;
		break;

	case LIS3DH_ACC_IOCTL_GET_ENABLE:
		interval = atomic_read(&acc->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;
		break;

	case LIS3DH_ACC_IOCTL_SET_G_RANGE:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		bit_values = buf[0];
		err = lis3dh_acc_update_g_range(acc, bit_values);
		if (err < 0)
			return err;
		break;

#ifdef INTERRUPT_MANAGEMENT
	case LIS3DH_ACC_IOCTL_SET_CTRL_REG3:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = CTRL_REG3;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_CTRL_REG3] = ((mask & bit_values) |
						    (~mask & acc->
						     resume_state
						     [RES_CTRL_REG3]));
		break;

	case LIS3DH_ACC_IOCTL_SET_CTRL_REG6:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = CTRL_REG6;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_CTRL_REG6] = ((mask & bit_values) |
						    (~mask & acc->
						     resume_state
						     [RES_CTRL_REG6]));
		break;

	case LIS3DH_ACC_IOCTL_SET_DURATION1:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_DUR1;
		mask = 0x7F;
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_DUR1] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_DUR1]));
		break;

	case LIS3DH_ACC_IOCTL_SET_THRESHOLD1:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_THS1;
		mask = 0x7F;
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_THS1] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_THS1]));
		break;

	case LIS3DH_ACC_IOCTL_SET_CONFIG1:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = INT_CFG1;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_CFG1] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_CFG1]));
		break;

	case LIS3DH_ACC_IOCTL_GET_SOURCE1:
		err = lis3dh_acc_register_read(acc, buf, INT_SRC1);
		if (err < 0)
			return err;

		pr_debug("INT1_SRC content: %d , 0x%x\n", buf[0], buf[0]);

		if (copy_to_user(argp, buf, 1))
			return -EINVAL;
		break;

	case LIS3DH_ACC_IOCTL_SET_DURATION2:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_DUR2;
		mask = 0x7F;
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_DUR2] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_DUR2]));
		break;

	case LIS3DH_ACC_IOCTL_SET_THRESHOLD2:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_THS2;
		mask = 0x7F;
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_THS2] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_THS2]));
		break;

	case LIS3DH_ACC_IOCTL_SET_CONFIG2:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = INT_CFG2;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_CFG2] = ((mask & bit_values) |
						   (~mask & acc->
						    resume_state
						    [RES_INT_CFG2]));
		break;

	case LIS3DH_ACC_IOCTL_GET_SOURCE2:
		err = lis3dh_acc_register_read(acc, buf, INT_SRC2);
		if (err < 0)
			return err;

		pr_debug("INT2_SRC content: %d , 0x%x\n", buf[0], buf[0]);

		if (copy_to_user(argp, buf, 1))
			return -EINVAL;
		break;

	case LIS3DH_ACC_IOCTL_GET_TAP_SOURCE:
		err = lis3dh_acc_register_read(acc, buf, TT_SRC);
		if (err < 0)
			return err;
		pr_debug("TT_SRC content: %d , 0x%x\n", buf[0], buf[0]);

		if (copy_to_user(argp, buf, 1)) {
			pr_err("%s: %s error in copy_to_user \n",
			       LIS3DH_ACC_DEV_NAME, __func__);
			return -EINVAL;
		}
		break;
	case LIS3DH_ACC_IOCTL_GET_COOR_XYZ:
		err = lis3dh_acc_get_acceleration_data(acc, xyz);
		if (err < 0)
			return err;

		if (copy_to_user(argp, xyz, sizeof(xyz))) {
			pr_err(" %s %d error in copy_to_user \n",
			       __func__, __LINE__);
			return -EINVAL;
		}
		break;
	case LIS3DH_ACC_IOCTL_SET_TAP_CFG:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_CFG;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_CFG] = ((mask & bit_values) |
						 (~mask & acc->
						  resume_state[RES_TT_CFG]));
		break;

	case LIS3DH_ACC_IOCTL_SET_TAP_TLIM:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_LIM;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_LIM] = ((mask & bit_values) |
						 (~mask & acc->
						  resume_state[RES_TT_LIM]));
		break;

	case LIS3DH_ACC_IOCTL_SET_TAP_THS:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_THS;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_THS] = ((mask & bit_values) |
						 (~mask & acc->
						  resume_state[RES_TT_THS]));
		break;

	case LIS3DH_ACC_IOCTL_SET_TAP_TLAT:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_TLAT;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_TLAT] = ((mask & bit_values) |
						  (~mask & acc->
						   resume_state[RES_TT_TLAT]));
		break;

	case LIS3DH_ACC_IOCTL_SET_TAP_TW:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_TW;
		mask = buf[1];
		bit_values = buf[0];
		err = lis3dh_acc_register_update(acc, (u8 *) arg, reg_address,
						 mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_TW] = ((mask & bit_values) |
						(~mask & acc->
						 resume_state[RES_TT_TW]));
		break;

#endif /* INTERRUPT_MANAGEMENT */
	case LIS3DH_ACC_IOCTL_GET_CHIP_ID:
	    {
	        u8 devid = 0;
	        u8 devinfo[DEVICE_INFO_LEN] = {0};
	        err = lis3dh_acc_register_read(acc, &devid, WHO_AM_I);
	        if (err < 0) {
	            LOG_INFO("%s, error read register WHO_AM_I\n", __func__);
	            return -EAGAIN;
	        }
	        sprintf(devinfo, "%s, %#x", DEVICE_INFO, devid);

	        if (copy_to_user(argp, devinfo, sizeof(devinfo))) {
	            printk("%s error in copy_to_user(IOCTL_GET_CHIP_ID)\n", __func__);
	            return -EINVAL;
	        }
	    }
	   break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations lis3dh_acc_misc_fops = {
	.owner = THIS_MODULE,
	.open = lis3dh_acc_misc_open,
	.unlocked_ioctl = lis3dh_acc_misc_ioctl,
};

static struct miscdevice lis3dh_acc_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = LIS3DH_ACC_DEV_NAME,
	.fops = &lis3dh_acc_misc_fops,
};


#ifndef GSENSOR_INTERRUPT_MODE 

static void lis3dh_acc_input_work_func(struct work_struct *work)
{
	struct lis3dh_acc_data *acc;

	int xyz[3] = { 0 };
	int err;
	static int count_intr = 0;
	static s64 interrupt_last = 0;
	static s64 interrupt_cur = 0;
	ktime_t time_mono;
	time_mono = ktime_get();
	interrupt_cur = ktime_to_ns(time_mono);
	int interrupt_interval = interrupt_cur - interrupt_last;
	interrupt_last = interrupt_cur;

//	LOG_INFO("ENTRY----->%s\n",__func__);
	acc = container_of((struct delayed_work *)work,
			   struct lis3dh_acc_data, input_work);

	mutex_lock(&acc->lock);
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_acc_report_values(acc, xyz);
//	LOG_INFO(KERN_ERR "lihai: sprd-gsensor count_intr=%8d  interrupt_interval=%8d,  poll_interval=%d, err=%d \n", count_intr++, interrupt_interval, acc->pdata->poll_interval, err);
#if 0
        schedule_delayed_work(&acc->input_work,
			      msecs_to_jiffies(acc->pdata->poll_interval));
#else
        queue_delayed_work(_gSensorWork_workqueue, &acc->input_work, msecs_to_jiffies(acc->pdata->poll_interval));
#endif        
        mutex_unlock(&acc->lock);

//	LOG_INFO("EXIT----->%s\n\n",__func__);
}
#endif
	
#ifdef LIS3DH_OPEN_ENABLE
int lis3dh_acc_input_open(struct input_dev *input)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(input);

	return lis3dh_acc_enable(acc);
}

void lis3dh_acc_input_close(struct input_dev *dev)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(dev);

	lis3dh_acc_disable(acc);
}
#endif

static int lis3dh_acc_validate_pdata(struct lis3dh_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
					acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 || acc->pdata->axis_map_y > 2
	    || acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
			acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
	    || acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", acc->pdata->negate_x,
			acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dh_acc_input_init(struct lis3dh_acc_data *acc)
{
	int err;

#ifndef GSENSOR_INTERRUPT_MODE 
	/* Polling rx data when the interrupt is not used.*/
	if (1 /*acc->irq1 == 0 && acc->irq1 == 0 */ ) {
		INIT_DELAYED_WORK(&acc->input_work, lis3dh_acc_input_work_func);
	    _gSensorWork_workqueue = create_singlethread_workqueue("acc_singlethread");
            if (!_gSensorWork_workqueue) {
                    err = -ESRCH;
                    printk(KERN_ERR "acc: create_singlethread_workqueue\n");
                   return err;
            }
        }
#endif

	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocate failed\n");
		goto err0;
	}
#ifdef LIS3DH_ACC_OPEN_ENABLE
	acc->input_dev->open = lis3dh_acc_input_open;
	acc->input_dev->close = lis3dh_acc_input_close;
#endif

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*      next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*      next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, 0, 0);
	/*      next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*      next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);

	acc->input_dev->name = "accelerometer";

	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
			"unable to register input polled device %s\n",
			acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dh_acc_input_cleanup(struct lis3dh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis3dh_early_suspend(struct early_suspend *es);
static void lis3dh_early_resume(struct early_suspend *es);
#endif

#ifdef CONFIG_OF
static struct lis3dh_acc_platform_data *lis3dh_acc_parse_dt(struct device *dev)
{
	struct lis3dh_acc_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct lis3dh_acc_platform_data");
		return NULL;
	}

	pdata->gpio_int1 = of_get_gpio(np, 0);
	LOG_INFO(" %s:get gpio_int1  %d\n",__func__,pdata->gpio_int1);
	if(pdata->gpio_int1 < 0){
		dev_err(dev, "fail to get irq_gpio_number\n");
		LOG_INFO(" get irq1_gpio_number fail\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "poll_interval", &pdata->poll_interval);
	LOG_INFO("%s: get pdata->poll_interval %d\n",__func__,pdata->poll_interval);	
	if(ret){
		dev_err(dev, "fail to get poll_interval\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "min_interval", &pdata->min_interval);
	if(ret){
		dev_err(dev, "fail to get min_interval\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "g_range", &pdata->g_range);
	if(ret){
		dev_err(dev, "fail to get g_range\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_x", &pdata->axis_map_x);
	if(ret){
		dev_err(dev, "fail to get axis_map_x\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_y", &pdata->axis_map_y);
	if(ret){
		dev_err(dev, "fail to get axis_map_y\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_z", &pdata->axis_map_z);
	if(ret){
		dev_err(dev, "fail to get axis_map_z\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_x", &pdata->negate_x);
	if(ret){
		dev_err(dev, "fail to get negate_x\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_y", &pdata->negate_y);
	if(ret){
		dev_err(dev, "fail to get negate_y\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_z", &pdata->negate_z);
	if(ret){
		dev_err(dev, "fail to get negate_z\n");
		goto fail;
	}
	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif




#ifdef LIS3DH_DBG
static int str2int(char *p, char *end)
{
        int result = 0;
        int len = end - p + 1;
        int c;

        for(; len > 0; len--, p++) {
                if(p > end)
                        return -1;
                c = *p - '0';
                if((unsigned int)c >= 10)
                        return -1;
                result = result * 10 + c;
        }
        return result;
}

/*parse a int from the string*/
static int get_int(char *p, u8 max_len)
{
        char *start = p;
        char *end =NULL;

        if(max_len > 60) {
                LOG_ERR("CMD ERROR!the max length of the cmd should less than 60\n");
                return -1;
        }
        for(; max_len > 0; max_len--, p++) {
                if(*p >= '0' && *p <= '9') {
                        start = p;
                        break;
                }
        }
        if(0 == max_len)
                return -1;

        end = start;

        for(; max_len > 0; max_len--, p++) {
                if(*p >= '0' && *p <= '9') {
                        end = p;
                        continue;
                }
                break;
        }

        return str2int(start, end);
}

static ssize_t lis3dh_val_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
	 LOG_FUN( );
	
        return sprintf(buff, "acc->pdata->poll_interva=%d\n",lis3dh_acc_misc_data->pdata->poll_interval);
}
// adjust poll interval dynamiclly,eg echo 5>val_show ,means poll interval is 5ms
static ssize_t lis3dh_val_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t n)
{
	LOG_FUN( );
	char *buff_temp = NULL;
	int interval;
       int err;
	   
	buff_temp = kmalloc(n, GFP_KERNEL);
	if(NULL == buff_temp){
	    LOG_ERR("kmalloc err\n");
	    return n;
	}

	memcpy(buff_temp, buff, n);
	interval = get_int(buff_temp, n);

	if (interval < 0 || interval > 1000)
	return -EINVAL;

	lis3dh_acc_misc_data->pdata->poll_interval = max(interval,lis3dh_acc_misc_data->pdata->min_interval); // max the frequecy of g sensor
       LOG_INFO("yuebao %s,acc->pdata->poll_interval =%d,interval=%d,acc->pdata->min_interval=%d\n", __func__,lis3dh_acc_misc_data->pdata->poll_interval,interval,lis3dh_acc_misc_data->pdata->min_interval );
	err = lis3dh_acc_update_odr(lis3dh_acc_misc_data, lis3dh_acc_misc_data->pdata->poll_interval );
	/* TODO: if update fails poll is still set */
	//		if (err < 0)
	//			return err;
	if(buff_temp != NULL)
	    kfree(buff_temp);

	return 0;
}

/*
1  should first write ,eg echo 0x20=0x97>reg
2  then read ,eg cat reg:Addr: 0x20 Val: 0x97
*/
static ssize_t sensor_reg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
        int err;
	  u8 value = 0;
	
	 LOG_FUN( );

	err = lis3dh_acc_register_read(lis3dh_acc_misc_data, &value, lis3dh_acc_misc_data->debug.diag_reg_addr_wr);
	return sprintf(buff, "Addr: 0x%x Val: 0x%x\n",
			lis3dh_acc_misc_data->debug.diag_reg_addr_wr, value);	
}

static ssize_t sensor_reg_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t n)
{
	char buf[32];
	char *sptr, *token;
	unsigned int len = 0;
	u8 reg_addr, reg_val;
	 int err = -1;

	LOG_INFO("%s, user echo is:%s\n",__func__,buff);
	len = min(n, sizeof(buf) - 1);
	if (!memcpy(buf, buff, len)){
		LOG_INFO("%s, %s\n",__func__,buf);
		return -EFAULT;
	}

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, "=");
	LOG_INFO("%s, token=%s\n",__func__,token);

	if (!token)
		return -EINVAL;

	 LOG_INFO("%s,now  token=%s,sptr=%s\n",__func__,token,sptr);

	if (kstrtou8(token, 0, &reg_addr)){
		 LOG_INFO("%s,now  reg_addr=0x%x\n",__func__,reg_addr);
		return -EINVAL;
	}
//	if (!ath6kl_dbg_is_diag_reg_valid(reg_addr))
//		return -EINVAL;

	if (kstrtou8(sptr, 0, &reg_val)){
		 LOG_INFO("%s,now reg_val=0x%x\n",__func__,reg_val);
		return -EINVAL;
	}
	 LOG_INFO("%s,now  reg_addr=0x%x,reg_val=0x%x\n",__func__,reg_addr,reg_val);

	lis3dh_acc_misc_data->debug.diag_reg_addr_wr = reg_addr;
	lis3dh_acc_misc_data->debug.diag_reg_val_wr = reg_val;

	buf[0] = reg_addr;
	buf[1] = reg_val;
		
	err = lis3dh_acc_i2c_write(lis3dh_acc_misc_data, buf, 1);
	if (err < 0){
		LOG_ERR("%s: %d\n",__func__,err);
		return -EIO;
	}
	//resume_state[index]   index  is not   reg addr,  so ONLY CAN config CTRL_REG1~CTRL_REG6 
	lis3dh_acc_misc_data->resume_state[reg_addr-0x20] = reg_val;
	 LOG_INFO("%s,now n=%d\n",__func__,n);
	return n;
}
static struct kobject *lis3dh_kobj;
static struct kobj_attribute lis3dh_val_attr =
        __ATTR(show_val, 0644, lis3dh_val_show, lis3dh_val_store);
static struct kobj_attribute sensor_reg_attr =
        __ATTR(reg, 0644, sensor_reg_show, sensor_reg_store);


static struct attribute *sensor_sysfs_attrs[] = {
	&lis3dh_val_attr.attr,
	&sensor_reg_attr.attr,
		
	NULL
};

static struct attribute_group sensor_attribute_group = {
	.attrs = sensor_sysfs_attrs,
};


static int lis3dh_sysfs_init(struct lis3dh_acc_data *acc)
{
        int ret = -1;

        lis3dh_kobj = kobject_create_and_add("lis3dh", &(acc->input_dev->dev.kobj));
        if (lis3dh_kobj == NULL) {
                ret = -ENOMEM;
                LOG_ERR("register sysfs failed. ret = %d\n", ret);
                return ret;
        }
	 ret = sysfs_create_group(lis3dh_kobj, &sensor_attribute_group);
        if (ret) {
                LOG_ERR("create sysfs failed. ret = %d\n", ret);
                return ret;
        }
        return ret;
}

#endif

static int lis3dh_acc_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{

	struct lis3dh_acc_data *acc;
	struct lis3dh_acc_platform_data *pdata = client->dev.platform_data;

	int err = -1;
	int tempvalue;
	LOG_INFO("sprd-gsensor: -- %s -- start !\n",__func__);
	pr_debug("%s: probe start.\n", LIS3DH_ACC_DEV_NAME);


#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;
	if (np && !pdata){
		pdata = lis3dh_acc_parse_dt(&client->dev);
		if(pdata){
			client->dev.platform_data = pdata;
		}
		if(!pdata){
			err = -ENOMEM;
			goto exit_alloc_platform_data_failed;
		}
	}
#endif
	/*
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE |
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "client not smb-i2c capable:2\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "client not smb-i2c capable:3\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}
	/*
	 * OK. From now, we presume we have a valid client. We now create the
	 * client structure, even though we cannot fill it completely yet.
	 */

	acc = kzalloc(sizeof(struct lis3dh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for module data: "
			"%d\n", err);
		goto exit_alloc_data_failed;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	lis3dh_i2c_client = client;
	i2c_set_clientdata(client, acc);

	LOG_INFO("gpio pin request  (%d)\n", pdata->gpio_int1);
       err=gpio_request(pdata->gpio_int1, "GSENSOR_INT1");
    
	if (err) {
	        LOG_ERR("gpio pin request fail (%d)\n", err);
	        goto err_free_irq1;
	}  else  {
			gpio_direction_input(pdata->gpio_int1);
			LOG_INFO("sprd-gsensor: -- %s  acc->irq1_gpio_number  status is [%d]-- !\n",__func__,gpio_get_value(pdata->gpio_int1));
			/*get irq*/
			acc->irq1  = gpio_to_irq(pdata->gpio_int1);
			LOG_INFO("IRQ1 number is %d\n", acc->irq1 );
	}
	if (acc->irq1 != 0) {
		pr_debug("%s request irq1\n", __func__);
		err =
		    request_irq(acc->irq1, lis3dh_acc_isr1, IRQF_TRIGGER_HIGH| IRQF_NO_SUSPEND ,
				"lis3dh_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			goto err_mutexunlockfreedata;
		}
		disable_irq_nosync(acc->irq1);

		INIT_WORK(&acc->irq1_work, lis3dh_acc_irq1_work_func);
		acc->irq1_work_queue =
		    create_singlethread_workqueue("lis3dh_acc_wq1");
		if (!acc->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev, "cannot create work queue1: %d\n",
				err);
			goto err_free_irq1;
		}
	}


//	gpio_request(GSENSOR_GINT2_GPI, "GSENSOR_INT2");
	acc->irq2 = 0; /* gpio_to_irq(GSENSOR_GINT2_GPI); */
	if (acc->irq2 != 0) {
		err =
		    request_irq(acc->irq2, lis3dh_acc_isr2, IRQF_TRIGGER_RISING,
				"lis3dh_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}
		disable_irq_nosync(acc->irq2);

/*		 Create workqueue for IRQ.*/

		INIT_WORK(&acc->irq2_work, lis3dh_acc_irq2_work_func);
		acc->irq2_work_queue =
		    create_singlethread_workqueue("lis3dh_acc_wq2");
		if (!acc->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev, "cannot create work queue2: %d\n",
				err);
			goto err_free_irq2;
		}
	}

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto err_destoyworkqueue2;
	}

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	err = lis3dh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

#ifdef GSENSOR_INTERRUPT_MODE
memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = LIS3DH_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = CTRL_REG3_I1_DRDY1;//0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = CTRL_REG5_LIR_INT1;//0x00;
	acc->resume_state[RES_CTRL_REG6] = CTRL_REG6_HLACTIVE;//0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;
	acc->resume_state[RES_INT_CFG2] = 0x00;
	acc->resume_state[RES_INT_THS2] = 0x00;
	acc->resume_state[RES_INT_DUR2] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

#endif
	sprd_i2c_ctl_chg_clk(client->adapter->nr, 400000);




	err = lis3dh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err2;
	}

#if 1
	if (i2c_smbus_read_byte(client) < 0) {
		pr_err("i2c_smbus_read_byte error!!\n");
		goto err_destoyworkqueue2;
	} else {
		pr_debug("%s Device detected!\n", LIS3DH_ACC_DEV_NAME);
	}
#endif
	/* read chip id */

	tempvalue = i2c_smbus_read_word_data(client, WHO_AM_I);

	if ((tempvalue & 0x00FF) == WHOAMI_LIS3DH_ACC) {
		pr_debug("%s I2C driver registered!\n",
		       LIS3DH_ACC_DEV_NAME);
	} else {
		acc->client = NULL;
		pr_debug("I2C driver not registered!"
		       " Device unknown 0x%x\n", tempvalue);
		goto err_destoyworkqueue2;
	}
	
	i2c_set_clientdata(client, acc);

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err2;
		}
	}
#ifndef GSENSOR_INTERRUPT_MODE
	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = LIS3DH_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = CTRL_REG3_I1_DRDY1;//0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x00;//CTRL_REG5_LIR_INT1;//0x04;
	acc->resume_state[RES_CTRL_REG6] = CTRL_REG6_HLACTIVE;//0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;
	acc->resume_state[RES_INT_CFG2] = 0x00;
	acc->resume_state[RES_INT_THS2] = 0x00;
	acc->resume_state[RES_INT_DUR2] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

#endif
	atomic_set(&acc->enabled, 1);

	err = lis3dh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto err_power_off;
	}

	err = lis3dh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_power_off;
	}

	err = lis3dh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}
	lis3dh_acc_misc_data = acc;

	err = misc_register(&lis3dh_acc_misc_device);
	if (err < 0) {
		dev_err(&client->dev,
			"misc LIS3DH_ACC_DEV_NAME register failed\n");
		goto err_input_cleanup;
	}

	lis3dh_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);
/*
#ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.suspend = lis3dh_early_suspend;
	acc->early_suspend.resume = lis3dh_early_resume;
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&acc->early_suspend);
#endif
*/
#ifdef LIS3DH_DBG
        err = lis3dh_sysfs_init(acc);
        if(err) {
                LOG_ERR("lis3dh_sysfs_init failed!\n");
                goto exit_lis3dh_sysfs_init_failed;
        }
#endif
#if USE_WAIT_QUEUE
	thread = kthread_run(gsensor_event_handler, 0, "gsensor-wait");
	if (IS_ERR(thread))
	{
		err = PTR_ERR(thread);
		LOG_ERR("failed to create kernel thread: %d\n", err);
	}
#endif
	mutex_unlock(&acc->lock);
	dev_info(&client->dev, "###%s###\n", __func__);
       LOG_INFO("sprd-gsensor: -- %s exit :acc->irq1_gpio_number status is [%d]-- !\n",__func__,gpio_get_value(acc->pdata->gpio_int1));
	LOG_INFO("sprd-gsensor: -- %s -- success !\n",__func__);
	return 0;

#ifdef LIS3DH_DBG
exit_lis3dh_sysfs_init_failed:
#endif
	
err_input_cleanup:
	lis3dh_acc_input_cleanup(acc);
err_power_off:
	lis3dh_acc_device_power_off(acc);
err2:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_destoyworkqueue2:
	if (acc->irq2_work_queue)
		destroy_workqueue(acc->irq2_work_queue);
err_free_irq2:
	if (acc->irq2) {
		free_irq(acc->irq2, acc);
		gpio_free(GSENSOR_GINT2_GPI);
	}
err_destoyworkqueue1:
	if (acc->irq1_work_queue)
		destroy_workqueue(acc->irq1_work_queue);
err_free_irq1:
	if (acc->irq1) {
		free_irq(acc->irq1, acc);
		gpio_free(acc->pdata->gpio_int1);
	}
err_mutexunlockfreedata:
	i2c_set_clientdata(client, NULL);
	mutex_unlock(&acc->lock);
	kfree(acc);
	lis3dh_acc_misc_data = NULL;
exit_alloc_data_failed:
exit_check_functionality_failed:
	pr_err("%s: Driver Init failed\n", LIS3DH_ACC_DEV_NAME);
exit_alloc_platform_data_failed:
	return err;
}

static int  lis3dh_acc_remove(struct i2c_client *client)
{

	/* TODO: revisit ordering here once _probe order is finalized */
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
	if (acc != NULL) {
		if (acc->irq1) {
			free_irq(acc->irq1, acc);
			gpio_free(acc->pdata->gpio_int1);
		}
		if (acc->irq2) {
			free_irq(acc->irq2, acc);
			gpio_free(GSENSOR_GINT2_GPI);
		}

		if (acc->irq1_work_queue)
			destroy_workqueue(acc->irq1_work_queue);
		if (acc->irq2_work_queue)
			destroy_workqueue(acc->irq2_work_queue);
		misc_deregister(&lis3dh_acc_misc_device);
		lis3dh_acc_input_cleanup(acc);
		lis3dh_acc_device_power_off(acc);
		if (acc->pdata->exit)
			acc->pdata->exit();
		kfree(acc->pdata);
		kfree(acc);
	}

	return 0;
}

static int lis3dh_acc_resume(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	LOG_INFO("sprd-gsensor: -- %s -- !\n",__func__);
	dev_dbg(&client->dev, "###%s###\n", __func__);
	if (acc != NULL && acc->on_before_suspend)
		return lis3dh_acc_enable(acc);

	return 0;
}

static int lis3dh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	LOG_INFO("sprd-gsensor: -- %s -- !\n",__func__);
	dev_dbg(&client->dev, "###%s###\n", __func__);
	if (acc != NULL) {
		acc->on_before_suspend = atomic_read(&acc->enabled);
		return lis3dh_acc_disable(acc);
	}
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void lis3dh_early_suspend(struct early_suspend *es)
{
	lis3dh_acc_suspend(lis3dh_i2c_client, (pm_message_t) {
			   .event = 0});
}

static void lis3dh_early_resume(struct early_suspend *es)
{
	lis3dh_acc_resume(lis3dh_i2c_client);
}

#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct i2c_device_id lis3dh_acc_id[]
= { {LIS3DH_ACC_DEV_NAME, 0}, {}, };

MODULE_DEVICE_TABLE(i2c, lis3dh_acc_id);

static const struct of_device_id lis3dh_acc_of_match[] = {
       { .compatible = "ST,lis3dh_acc", },
       { }
};
MODULE_DEVICE_TABLE(of, lis3dh_acc_of_match);
static struct i2c_driver lis3dh_acc_driver = {
	.driver = {
		   .name = LIS3DH_ACC_I2C_NAME,
		   .of_match_table = lis3dh_acc_of_match,
		   },
	.probe = lis3dh_acc_probe,
	.remove = lis3dh_acc_remove,
	.resume = lis3dh_acc_resume,
	.suspend = lis3dh_acc_suspend,
	.id_table = lis3dh_acc_id,
};


static int __init lis3dh_acc_init(void)
{
	pr_debug("%s accelerometer driver: init\n", LIS3DH_ACC_I2C_NAME);

	return i2c_add_driver(&lis3dh_acc_driver);
}

static void __exit lis3dh_acc_exit(void)
{
	pr_debug("%s accelerometer driver exit\n", LIS3DH_ACC_DEV_NAME);

	i2c_del_driver(&lis3dh_acc_driver);
	return;
}

module_init(lis3dh_acc_init);
module_exit(lis3dh_acc_exit);

MODULE_DESCRIPTION("lis3dh accelerometer misc driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");
