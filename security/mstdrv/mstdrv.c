/*
 * @file mstdrv.c
 * @brief MST drv Support
 * Copyright (c) 2015, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/regulator/machine.h>
#include <linux/pinctrl/consumer.h>
#include <linux/err.h>
#include <linux/qseecom.h>
#include "mstdrv.h"

/* defines */
#define	ON				1	// On state
#define	OFF				0	// Off state
#define	TRACK1				1	// Track1 data
#define	TRACK2				2	// Track2 data
#define	BITS_PER_CHAR_TRACK1		7	// Number of bits per character for Track1 data
#define	BITS_PER_CHAR_TRACK2		5	// Number of bits per character Track2 data
#define LEADING_ZEROES			28	// Number of leading zeroes to data
#define TRAILING_ZEROES			28	// Number of trailing zeroes to data
#define LRC_CHAR_TRACK1			70	// LRC character ('&' in ISO/IEC 7811) for Track1 data
#define	PWR_EN_WAIT_TIME		11	// time (mS) to wait after MST+PER_EN signal sent (from HW team : ~10-11 mS)
#define	MST_DATA_TIME_DURATION		300	// time (uS) duration for transferring one MST data bit
#define CMD_MST_LDO_OFF			'0'	// MST LDO off
#define CMD_MST_LDO_ON			'1'	// MST LDO on
#define CMD_SEND_TRACK1_DATA		'2'	// Send track1 test data
#define CMD_SEND_TRACK2_DATA		'3'	// send track2 test data
#define CMD_HW_RELIABILITY_TEST_START	'4'	// start HW reliability test
#define CMD_HW_RELIABILITY_TEST_STOP	'5'	// stop HW reliability test
#define CMD_SECURE_MODE_MST_TEST	'6'	// Test mst from secure mode
#define ERROR_VALUE			-1	// Error value
#define TEST_RESULT_LEN			256	// result array length
#define MST_LDO_3P0			"VDD_MST"	// MST LDO regulator
#define SVC_MST_ID			0x000A0000	// need to check ID
#define MST_CREATE_CMD(x)		(SVC_MST_ID | x)	// Create MST commands

/* enum definitions */
typedef enum {
	MST_CMD_TEST        = MST_CREATE_CMD(0x00000000),
	MST_CMD_UNKNOWN     = MST_CREATE_CMD(0x7FFFFFFF)
} mst_cmd_type;

/* struct definitions */
struct qseecom_handle {
    void *dev; /* in/out */
    unsigned char *sbuf; /* in/out */
    uint32_t sbuf_len; /* in/out */
};

typedef struct mst_req_s {
	mst_cmd_type cmd_id;
	uint32_t data;
} __attribute__ ((packed)) mst_req_t;

typedef struct mst_rsp_s {
	uint32_t data;
	uint32_t status;
} __attribute__ ((packed)) mst_rsp_t;

/* extern function declarations */
extern int qseecom_start_app(struct qseecom_handle **handle, char *app_name, uint32_t size);
extern int qseecom_shutdown_app(struct qseecom_handle **handle);
extern int qseecom_send_command(struct qseecom_handle *handle, void *send_buf, uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len);

/* function declarations */
static ssize_t show_mst_drv(struct device *dev,
        struct device_attribute *attr, char *buf);
static ssize_t store_mst_drv(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static int mst_ldo_device_probe(struct platform_device *pdev);

/* global variables */
static int mst_pwr_en = 0;		// MST_PWR_EN pin
static int mst_pd = 0;			// MST_D_P pin
static int mst_md = 0;			// MST_D_N pin
static int escape_loop = 1;		// for HW reliability test
static struct class *mst_drv_class;	// mst_drv class
struct device *mst_drv_dev;		// mst_drv driver handle
static spinlock_t event_lock;		// spinlock to disable interrupts
static struct wake_lock   mst_wakelock;	// wake_lock used for HW reliability test
static DEVICE_ATTR(transmit, 0770, show_mst_drv, store_mst_drv);	// define device attribute
static bool mst_power_on = 0;		// to track current level of mst signal
static struct qseecom_handle *qhandle = NULL;

DEFINE_MUTEX(mst_mutex);

/* device driver structures */
static struct of_device_id mst_match_ldo_table[] = {
	{ .compatible = "sec-mst",},
	{},
};

static struct platform_driver sec_mst_ldo_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mstldo",
		.of_match_table = mst_match_ldo_table,
	},
	.probe = mst_ldo_device_probe,
};

EXPORT_SYMBOL_GPL(mst_drv_dev);

/**
 * of_mst_hw_onoff - Enable/Disable MST LDO GPIO pin (or Regulator)
 * @on: on/off value
 */
static void of_mst_hw_onoff(bool on){
	struct regulator *regulator3_0;
	int ret;
	regulator3_0 = regulator_get(NULL, MST_LDO_3P0);
	if (IS_ERR(regulator3_0)) {
		printk("[MST] %s : regulator 3.0 is not available\n", __func__);
		return;
	}
	if (mst_power_on == on) {
		printk("[MST] mst-drv : mst_power_onoff : already %d\n", on);
		regulator_put(regulator3_0);
		return;
	}
	mst_power_on = on;
	if(regulator3_0 == NULL){
		printk(KERN_ERR "[MST] %s: regulator3_0 is invalid(NULL)\n", __func__);
		return ;
	}
	if(on) {
		ret = regulator_enable(regulator3_0);
		if (ret < 0) {
			printk("[MST] %s : regulator 3.0 is not enable\n", __func__);
		}
	}else{
		regulator_disable(regulator3_0);
	}
	regulator_put(regulator3_0);
}

/**
 * mst_change - change MST gpio pins based on input and current data bit
 * @mst_val: data bit to set
 * @mst_stat: current data bit
 */
int mst_change(int mst_val, int mst_stat) {
	if(mst_stat){
		gpio_set_value(mst_md, OFF);
		gpio_set_value(mst_pd, ON);
		if(mst_val) {
			udelay((MST_DATA_TIME_DURATION/2));
			gpio_set_value(mst_pd, OFF);
			gpio_set_value(mst_md, ON);
			udelay((MST_DATA_TIME_DURATION/2));
			mst_stat = ON;
		} else {
			udelay(MST_DATA_TIME_DURATION);
			mst_stat = OFF;
		}
	} else {
		gpio_set_value(mst_pd, OFF);
		gpio_set_value(mst_md, ON);
		if(mst_val) {
			udelay((MST_DATA_TIME_DURATION/2));
			gpio_set_value(mst_md, OFF);
			gpio_set_value(mst_pd, ON);
			udelay((MST_DATA_TIME_DURATION/2));
			mst_stat = OFF;
		} else {
			udelay(MST_DATA_TIME_DURATION);
			mst_stat = ON;
		}
	}
	return mst_stat;
}

/**
 * transmit_mst_data - Transmit test track data
 * @track: 1:track1, 2:track2
 */
static int transmit_mst_data(int track)
{
	int ret_val = 1;
	int i = 0;
	int j = 0;
	int mst_stat = 0;
	unsigned long flags;
	char midstring[] =
			{69,98,21,84,82,84,81,88,16,22,22,
			81,87,16,21,88,88,22,62,104,117,97,
			110,103,79,37,110,121,97,110,103,64,
			64,64,64,64,64,64,64,64,64,64,64,62,
			81,82,81,16,81,16,81,16,16,16,16,16,
			16,82,21,25,16,81,16,16,16,16,16,16,
			19,88,19,16,16,16,16,16,16,31,70};

	char midstring2[] =
			{11,22,16,1,16,21,22,7,8,19,22,19,
			25,2,25,8,8,13,2,21,16,1,16,16,16,
			21,16,16,16,16,22,16,16,7,19,25,19,16,31,13};

	gpio_set_value(mst_pd, OFF);
	gpio_set_value(mst_md, OFF);
	gpio_set_value(mst_pwr_en, OFF);
	udelay(300);
	gpio_set_value(mst_pwr_en, ON);
	mdelay(PWR_EN_WAIT_TIME);

	if(track == TRACK1) {
		spin_lock_irqsave(&event_lock, flags);
		for(i = 0 ; i < LEADING_ZEROES ; i++){
			mst_stat = mst_change(OFF, mst_stat);
		}

		i = 0;
		do{
			for(j = 0 ; j < BITS_PER_CHAR_TRACK1 ; j++) {
				if(((midstring[i] & (1 << j)) >> j) == 1) {
							mst_stat = mst_change(ON, mst_stat);
						}
						else {
							mst_stat = mst_change(OFF, mst_stat);
						}
					}
					i++;
		}
		while ( ( i < sizeof(midstring)) && (midstring[i - 1] != LRC_CHAR_TRACK1) );

		for(i = 0 ; i < TRAILING_ZEROES ; i++){
			mst_stat = mst_change(OFF, mst_stat);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_md, OFF);
		gpio_set_value(mst_pd, OFF);
		gpio_set_value(mst_pwr_en, OFF);
	}
	else if(track == TRACK2){
		spin_lock_irqsave(&event_lock, flags);
		for(i = 0 ; i < LEADING_ZEROES ; i++){
			mst_stat = mst_change(OFF, mst_stat);
		}

		i = 0;
		do{
			for(j = 0 ; j < BITS_PER_CHAR_TRACK2 ; j++) {
				if(((midstring2[i] & (1 << j)) >> j) == 1) {
							mst_stat = mst_change(ON, mst_stat);
						}
						else {
							mst_stat = mst_change(OFF, mst_stat);
						}
					}
					i++;
		}
		while ( ( i < sizeof(midstring2)) );

		for(i = 0 ; i < TRAILING_ZEROES ; i++){
			mst_stat = mst_change(OFF, mst_stat);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_md, OFF);
		gpio_set_value(mst_pd, OFF);
		gpio_set_value(mst_pwr_en, OFF);
	}
	else{
		ret_val = 0;
	}

	return ret_val;
}

/**
 * show_mst_drv - device attribute show sysfs operation
 */
static ssize_t show_mst_drv(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    if (!dev)
        return -ENODEV;

    // todo
    return sprintf(buf, "%s\n", "MST drv data");
}

/**
 * store_mst_drv - device attribute store sysfs operation
 */
static ssize_t store_mst_drv(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char test_result[TEST_RESULT_LEN]={0,};
	int track;
	int qsee_ret = 0;
	char app_name[MAX_APP_NAME_SIZE];
	mst_req_t *kreq = NULL;
	mst_rsp_t *krsp = NULL;
	int req_len = 0, rsp_len = 0;

	sscanf(buf, "%20s\n", test_result);

	switch(test_result[0]){

		case CMD_MST_LDO_OFF:
			of_mst_hw_onoff(OFF);
			break;

		case CMD_MST_LDO_ON:
			of_mst_hw_onoff(ON);
			break;

		case CMD_SEND_TRACK1_DATA:
			of_mst_hw_onoff(ON);
			track = TRACK1;
			printk("[MST] MST send track1 data, track %d\n", track);
			if (transmit_mst_data(track)) {
				printk("[MST] track1 data is sent\n");
			}
			of_mst_hw_onoff(OFF);
			break;

		case CMD_SEND_TRACK2_DATA:
			of_mst_hw_onoff(ON);
			track = TRACK2;
			printk("[MST] MST send track2 data, track : %d\n", track);
			if (transmit_mst_data(track)) {
				printk("[MST] track2 data is sent\n");
			}
			of_mst_hw_onoff(OFF);
			break;

		case CMD_HW_RELIABILITY_TEST_START:
			if(escape_loop){
				wake_lock_init(&mst_wakelock, WAKE_LOCK_SUSPEND, "mst_wakelock");
				wake_lock(&mst_wakelock);
			}
			escape_loop = 0;
			while( 1 ) {
				if(escape_loop == 1)
					break;
				of_mst_hw_onoff(ON);
				track = TRACK2;
				mdelay(10);
				printk("[MST] MST send track2 data, track : %d\n", track);
				if (transmit_mst_data(track)) {
					printk("[MST] track2 data is sent\n");
				}
				of_mst_hw_onoff(OFF);
				mdelay(1000);
			}
			break;

		case CMD_HW_RELIABILITY_TEST_STOP:
			if(!escape_loop)
				wake_lock_destroy(&mst_wakelock);
			escape_loop = 1;
			printk("[MST] MST escape_loop value = 1\n");
			break;

		case CMD_SECURE_MODE_MST_TEST:
			mutex_lock(&mst_mutex);
			snprintf(app_name, MAX_APP_NAME_SIZE, "%s", "mst");
			if ( NULL == qhandle ) {
				/* start the mst tzapp only when it is not loaded. */
				qsee_ret = qseecom_start_app(&qhandle, app_name, 1024);
			}

			if ( NULL == qhandle ) {
				/* qhandle is still NULL. It seems we couldn't start mst tzapp. */
				printk("[MST] cannot get tzapp handle from kernel.\n");
				mutex_unlock(&mst_mutex);
				break; /* leave the function now. */
			}

			kreq = (struct mst_req_s *)qhandle->sbuf;
			kreq->cmd_id = MST_CMD_TEST;
			req_len = sizeof(mst_req_t);
			krsp =(struct mst_rsp_s *)(qhandle->sbuf + req_len);
			rsp_len = sizeof(mst_rsp_t);

			printk("[MST] cmd_id = %x, req_len = %d, rsp_len = %d\n", kreq->cmd_id, req_len, rsp_len);

			of_mst_hw_onoff(ON);
			gpio_set_value(mst_pwr_en, OFF);
			udelay(300);
			gpio_set_value(mst_pwr_en, ON);
			qsee_ret = qseecom_send_command(qhandle, kreq, req_len, krsp, rsp_len);
			gpio_set_value(mst_pwr_en, OFF);
			of_mst_hw_onoff(OFF);

			if (qsee_ret) {
				printk("[MST] failed to send cmd to qseecom; qsee_ret = %d.\n", qsee_ret);
				printk("[MST] shutting down the tzapp.\n");
				qsee_ret = qseecom_shutdown_app(&qhandle);
				if ( qsee_ret ) {
					printk("[MST] failed to shut down the tzapp.\n");
				}else{
					qhandle = NULL;
				}
			}

			if (krsp->status == 0) {
				printk("[MST] generate sample track data from TZ -- successful.\n");
			} else {
				printk("[MST] generate sample track data from TZ -- failed. %d\n", krsp->status);
			}
			mutex_unlock(&mst_mutex);
			break;

		default:
			printk(KERN_ERR "[MST] MST invalid value : %s\n", test_result);
			break;
	}
	return count;
}


/**
 * sec_mst_gpio_init - Initialize GPIO pins used by driver
 * @dev: driver handle
 */
static int sec_mst_gpio_init(struct device *dev)
{
	int ret = 0;
	struct pinctrl *mst_pd_pinctrl;

	mst_pwr_en = of_get_named_gpio(dev->of_node, "sec-mst,mst-pwr-gpio", OFF);
	mst_pd = of_get_named_gpio(dev->of_node, "sec-mst,mst-pd-gpio", OFF);
	mst_md = of_get_named_gpio(dev->of_node, "sec-mst,mst-md-gpio", OFF);

	/* check if gpio pin is inited */
	if (mst_pwr_en < 0) {
		printk(KERN_ERR "[MST] %s : Cannot create the gpio\n", __func__);
		return 1;
	}
	if (mst_pd < 0) {
		printk(KERN_ERR "[MST] %s : Cannot create the gpio\n", __func__);
		return 1;
	}
	if (mst_md < 0) {
		printk(KERN_ERR "[MST] %s : Cannot create the gpio\n", __func__);
		return 1;
	}

	/* gpio request */
	ret = gpio_request(mst_pwr_en, "sec-mst,mst-pwr-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get en gpio : %d, %d\n", ret, mst_pwr_en);
	}
	ret = gpio_request(mst_pd, "sec-mst,mst-pd-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get pd gpio : %d, %d\n", ret, mst_pd);
	}
	ret = gpio_request(mst_md, "sec-mst,mst-md-gpio");
	if (ret) {
		printk(KERN_ERR "[MST] failed to get md gpio : %d, %d\n", ret, mst_md);
	}

	/* set gpio direction */
	if (!(ret < 0)  && (mst_pwr_en > 0)) {
		gpio_direction_output(mst_pwr_en, OFF);
		printk(KERN_ERR "[MST] %s : Send Output\n", __func__);
	}
	if (!(ret < 0)  && (mst_pd > 0)) {
		gpio_direction_output(mst_pd, OFF);
		printk(KERN_ERR "[MST] %s : Send Output\n", __func__);
	}
	if (!(ret < 0)  && (mst_md > 0)) {
		gpio_direction_output(mst_md, OFF);
		printk(KERN_ERR "[MST] %s : Send Output\n", __func__);
	}

	/* pinctrl settings */
	mst_pd_pinctrl = devm_pinctrl_get_select(dev, "mst_active");
	if(IS_ERR(mst_pd_pinctrl)) {
                if (PTR_ERR(mst_pd_pinctrl) == -EPROBE_DEFER)
					return -EPROBE_DEFER;
					pr_debug("Target does not use pinctrl\n");
					mst_pd_pinctrl = NULL;
        }

	return 0;
}

/**
 * mst_ldo_device_probe - Driver probe function
 */
static int mst_ldo_device_probe(struct platform_device *pdev)
{
	int retval = 0;

	printk("[MST] %s init start\n", __func__);

	spin_lock_init(&event_lock);

	if (sec_mst_gpio_init(&pdev->dev)) {
		retval = ERROR_VALUE;
		printk(KERN_ERR "[MST] %s: driver initialization failed, retval : %d\n", __FILE__, retval);
		goto done;
	}

	mst_drv_class = class_create(THIS_MODULE, "mstldo");
	if (IS_ERR(mst_drv_class)) {
		retval = PTR_ERR(mst_drv_class);
		printk(KERN_ERR "[MST] %s: driver initialization failed, retval : %d\n", __FILE__, retval);
		goto done;
	}

	mst_drv_dev = device_create(mst_drv_class,
			NULL /* parent */, 0 /* dev_t */, NULL /* drvdata */,
			MST_DRV_DEV);
	if (IS_ERR(mst_drv_dev)) {
		retval = PTR_ERR(mst_drv_dev);
		kfree(mst_drv_dev);
		device_destroy(mst_drv_class, 0);
		printk(KERN_ERR "[MST] %s: driver initialization failed, retval : %d\n", __FILE__, retval);
		goto done;
	}

	/* register this mst device with the driver core */
	retval = device_create_file(mst_drv_dev, &dev_attr_transmit);
	if (retval)
	{
		kfree(mst_drv_dev);
		device_destroy(mst_drv_class, 0);
		printk(KERN_ERR "[MST] %s: driver initialization failed, retval : %d\n", __FILE__, retval);
		goto done;
	}

	printk(KERN_DEBUG "[MST] MST drv driver (%s) is initialized.\n", MST_DRV_DEV);

done:
	return retval;
}

/**
 * mst_drv_init - Driver init function
 */
static int __init mst_drv_init(void)
{
	int ret=0;
	printk(KERN_ERR "[MST] %s\n", __func__);
	ret = platform_driver_register(&sec_mst_ldo_driver);
	printk(KERN_ERR "[MST] MST_LDO_DRV]]] init , ret val : %d\n",ret);
	return ret;
}

/**
 * mst_drv_exit - Driver exit function
 */
static void __exit mst_drv_exit (void)
{
    class_destroy(mst_drv_class);
    printk(KERN_ALERT "[MST] %s\n", __func__);
}

MODULE_AUTHOR("JASON KANG, j_seok.kang@samsung.com");
MODULE_DESCRIPTION("MST drv driver");
MODULE_VERSION("0.1");

module_init(mst_drv_init);
module_exit(mst_drv_exit);
