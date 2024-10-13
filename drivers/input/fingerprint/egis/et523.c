/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

/*
*
*  et523.spi.c
*  Date: 2016/03/16
*  Version: 0.9.0.1
*  Revise Date:  2017/7/2
*  Copyright (C) 2007-2017 Egis Technology Inc.
*
*/


#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/list.h>

//#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <soc/qcom/scm.h>
#include <linux/pm_wakeup.h>
#include <linux/platform_data/spi-s3c64xx.h>
#include <linux/fb.h>
//#include <fp_vendor.h>

//#include "../../../teei/V1.0/tz_vfs/fp_vendor.h"

#include "et523.h"
//#include "navi_input.h"

#define EDGE_TRIGGER_FALLING    0x0
#define	EDGE_TRIGGER_RISING    0x1
#define	LEVEL_TRIGGER_LOW       0x2
#define	LEVEL_TRIGGER_HIGH      0x3
#define EGIS_NAVI_INPUT 0  /* 1:open ; 0:close */
//#define Not_support_for_Sumsung_N9_Dual_sensor
#define Support_for_Sumsung_N9_Dual_sensor
struct wakeup_source wakeup_source_fp;

/*
 * FPS interrupt table
 */
//spinlock_t interrupt_lock;
struct interrupt_desc fps_ints = {0 , 0, "BUT0" , 0};
extern int finger_sysfs;
unsigned int bufsiz = 4096;

int gpio_irq;
int request_irq_done = 0;
/* int t_mode = 255; */
int g_screen_onoff = 1;

struct ioctl_cmd {
int int_mode;
int detect_period;
int detect_threshold;
};
/* To be compatible with fpc driver */
#ifndef CONFIG_SENSORS_FPC_1020
static struct FPS_data {
	unsigned int enabled;
	unsigned int state;
	struct blocking_notifier_head nhead;
} *fpsData;

struct FPS_data *FPS_init(void)
{
	struct FPS_data *mdata;
	if (!fpsData) {
		mdata = kzalloc(
				sizeof(struct FPS_data), GFP_KERNEL);
		if (mdata) {
			BLOCKING_INIT_NOTIFIER_HEAD(&mdata->nhead);
			pr_debug("%s: FPS notifier data structure init-ed\n", __func__);
		}
		fpsData = mdata;
	}
	return fpsData;
}
int FPS_register_notifier(struct notifier_block *nb,
	unsigned long stype, bool report)
{
	int error;
	struct FPS_data *mdata = fpsData;

	mdata = FPS_init();
	if (!mdata)
		return -ENODEV;
	mdata->enabled = (unsigned int)stype;
	pr_debug("%s: FPS sensor %lu notifier enabled\n", __func__, stype);

	error = blocking_notifier_chain_register(&mdata->nhead, nb);
	if (!error && report) {
		int state = mdata->state;
		/* send current FPS state on register request */
		blocking_notifier_call_chain(&mdata->nhead,
				stype, (void *)&state);
		pr_debug("%s: FPS reported state %d\n", __func__, state);
	}
	return error;
}
EXPORT_SYMBOL_GPL(FPS_register_notifier);

int FPS_unregister_notifier(struct notifier_block *nb,
		unsigned long stype)
{
	int error;
	struct FPS_data *mdata = fpsData;

	if (!mdata)
		return -ENODEV;

	error = blocking_notifier_chain_unregister(&mdata->nhead, nb);
	pr_debug("%s: FPS sensor %lu notifier unregister\n", __func__, stype);

	if (!mdata->nhead.head) {
		mdata->enabled = 0;
		pr_debug("%s: FPS sensor %lu no clients\n", __func__, stype);
	}

	return error;
}
EXPORT_SYMBOL_GPL(FPS_unregister_notifier);

void FPS_notify(unsigned long stype, int state)
{
	struct FPS_data *mdata = fpsData;

	pr_debug("%s: Enter", __func__);

	if (!mdata) {
		pr_err("%s: FPS notifier not initialized yet\n", __func__);
		return;
	}

	pr_debug("%s: FPS current state %d -> (0x%x)\n", __func__,
		mdata->state, state);

	if (mdata->enabled && mdata->state != state) {
		mdata->state = state;
		blocking_notifier_call_chain(&mdata->nhead,
						stype, (void *)&state);
		pr_debug("%s: FPS notification sent\n", __func__);
	} else if (!mdata->enabled) {
		pr_err("%s: !mdata->enabled", __func__);
	} else {
		pr_err("%s: mdata->state==state", __func__);
	}
}
#endif

static struct etspi_data *g_data;

DECLARE_BITMAP(minors, N_SPI_MINORS);
LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

/* ------------------------------ Interrupt -----------------------------*/
/*
 * Interrupt description
 */

#define FP_INT_DETECTION_PERIOD  10
#define FP_DETECTION_THRESHOLD	10
/* struct interrupt_desc fps_ints; */
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
/*
 *	FUNCTION NAME.
 *		interrupt_timer_routine
 *
 *	FUNCTIONAL DESCRIPTION.
 *		basic interrupt timer inital routine
 *
 *	ENTRY PARAMETERS.
 *		gpio - gpio address
 *
 *	EXIT PARAMETERS.
 *		Function Return
 */

void interrupt_timer_routine(unsigned long _data)
{
	struct interrupt_desc *bdata = (struct interrupt_desc *)_data;

	DEBUG_PRINT("FPS interrupt count = %d", bdata->int_count);
	if (bdata->int_count >= bdata->detect_threshold) {
		bdata->finger_on = 1;
		DEBUG_PRINT("FPS triggered !!!!!!!\n");
	} else {
		DEBUG_PRINT("FPS not triggered !!!!!!!\n");
	}

	bdata->int_count = 0;
	wake_up_interruptible(&interrupt_waitq);
}
void egis_irq_enable(bool irq_wake,bool enable)
{
	unsigned long nIrqFlag;
	struct irq_desc *desc;
	spin_lock_irqsave(&g_data->irq_lock, nIrqFlag);
	if (1 == irq_wake) {
		if (1 == enable && 0 == g_data->irq_enable_flag) {
			enable_irq_wake(gpio_irq);
			g_data->irq_enable_flag = 1;
		}
	} else {
		if (1 == enable && 0 == g_data->irq_enable_flag) {
			enable_irq(gpio_irq);
			g_data->irq_enable_flag = 1;
		} else if (0 == enable && 1 == g_data->irq_enable_flag) {
			disable_irq_nosync(gpio_irq);
			g_data->irq_enable_flag = 0;
		}
	}
	desc = irq_to_desc(gpio_irq);
	DEBUG_PRINT("irq_type = %d, g_data->irq_enable_flag = %d,desc->depth = %d\n",irq_wake,g_data->irq_enable_flag,desc->depth);
	spin_unlock_irqrestore(&g_data->irq_lock, nIrqFlag);
}
static irqreturn_t fp_eint_func(int irq, void *dev_id)
{
	if (!fps_ints.int_count)
		//mod_timer(&fps_ints.timer, jiffies + msecs_to_jiffies(fps_ints.detect_period));
	fps_ints.int_count++;
	/* DEBUG_PRINT_ratelimited(KERN_WARNING "-----------   zq fp fp_eint_func  ,fps_ints.int_count=%d",fps_ints.int_count);*/
	__pm_wakeup_event(&wakeup_source_fp, msecs_to_jiffies(1500));
	return IRQ_HANDLED;
}

static irqreturn_t fp_eint_func_ll(int irq , void *dev_id)
{
	DEBUG_PRINT("[egis]fp_eint_func_nospinlock\n");
	fps_ints.finger_on = 1;
	egis_irq_enable(0,0);
	wake_up_interruptible(&interrupt_waitq);
	__pm_wakeup_event(&wakeup_source_fp, msecs_to_jiffies(1500));
	return IRQ_RETVAL(IRQ_HANDLED);
}

/*
 *	FUNCTION NAME.
 *		Interrupt_Init
 *
 *	FUNCTIONAL DESCRIPTION.
 *		button initial routine
 *
 *	ENTRY PARAMETERS.
 *		int_mode - determine trigger mode
 *			EDGE_TRIGGER_FALLING    0x0
 *			EDGE_TRIGGER_RAISING    0x1
 *			LEVEL_TRIGGER_LOW        0x2
 *			LEVEL_TRIGGER_HIGH       0x3
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Init(struct etspi_data *etspi, int int_mode, int detect_period, int detect_threshold)
{

	int err = 0;
	int status = 0;
	DEBUG_PRINT("FP --  %s mode = %d period = %d threshold = %d\n", __func__, int_mode, detect_period, detect_threshold);
	DEBUG_PRINT("FP --  %s request_irq_done = %d gpio_irq = %d  pin = %d  \n", __func__, request_irq_done, gpio_irq, etspi->irqPin);


	fps_ints.detect_period = detect_period;
	fps_ints.detect_threshold = detect_threshold;
	fps_ints.int_count = 0;
	fps_ints.finger_on = 0;

	if (request_irq_done == 0)	{
		
		/* Initial IRQ Pin*/
		status = gpio_request(etspi->irqPin, "irq-gpio");
		if (status < 0) {
			pr_err("%s gpio_request etspi_irq failed\n",
				__func__);
			goto done;
		}
		status = gpio_direction_input(etspi->irqPin);
		if (status < 0) {
			pr_err("%s gpio_direction_input IRQ failed\n",
				__func__);
			goto done;
		}
		gpio_irq = gpio_to_irq(etspi->irqPin);
		if (gpio_irq < 0) {
			DEBUG_PRINT("%s gpio_to_irq failed\n", __func__);
			status = gpio_irq;
			goto done;
		}

		DEBUG_PRINT("[Interrupt_Init] flag current: %d disable: %d enable: %d\n",
		fps_ints.drdy_irq_flag, DRDY_IRQ_DISABLE, DRDY_IRQ_ENABLE);
		/* t_mode = int_mode; */
		if (int_mode == EDGE_TRIGGER_RISING) {
			DEBUG_PRINT("%s EDGE_TRIGGER_RISING\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func, IRQ_TYPE_EDGE_RISING, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == EDGE_TRIGGER_FALLING) {
			DEBUG_PRINT("%s EDGE_TRIGGER_FALLING\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func, IRQ_TYPE_EDGE_FALLING, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == LEVEL_TRIGGER_LOW) {
			DEBUG_PRINT("%s LEVEL_TRIGGER_LOW\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_LOW, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == LEVEL_TRIGGER_HIGH) {
			DEBUG_PRINT("%s LEVEL_TRIGGER_HIGH\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_HIGH, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		}
		DEBUG_PRINT("[Interrupt_Init]:gpio_to_irq return: %d\n", gpio_irq);
		DEBUG_PRINT("[Interrupt_Init]:request_irq return: %d\n", err);
		egis_irq_enable(1,1);
		request_irq_done = 1;
	}
	egis_irq_enable(0,1);
done:
	
	return 0;
}

/*
 *	FUNCTION NAME.
 *		Interrupt_Free
 *
 *	FUNCTIONAL DESCRIPTION.
 *		free all interrupt resource
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Free(struct etspi_data *etspi)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;
	if (1 == g_data->irq_enable_flag) {
		egis_irq_enable(0,0);
		del_timer_sync(&fps_ints.timer);
	}
	return 0;
}

/*
 *	FUNCTION NAME.
 *		fps_interrupt_re d
 *
 *	FUNCTIONAL DESCRIPTION.
 *		FPS interrupt read status
 *
 *	ENTRY PARAMETERS.
 *		wait poll table structure
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

unsigned int fps_interrupt_poll(
struct file *file,
struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	/* DEBUG_PRINT("%s %d\n", __func__, fps_ints.finger_on);*/
	/* fps_ints.int_count = 0; */
	poll_wait(file, &interrupt_waitq, wait);
	if (fps_ints.finger_on) {
		mask |= POLLIN | POLLRDNORM;
		/* fps_ints.finger_on = 0; */
	}
	return mask;
}

void fps_interrupt_abort(void)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;
	wake_up_interruptible(&interrupt_waitq);
}

/*-------------------------------------------------------------------------*/

/* ---------------------------- spi operation -----------------------------*/
#if 0
static int sec_spi_prepare(struct platform_device *spi)
{
//	int speed = 0;

        struct s3c64xx_spi_driver_data *sdd = NULL;
        //struct s3c64xx_spi_csinfo *cs;
    DEBUG_PRINT("%s\n", __func__);  
	sdd = spi_master_get_devdata(spi->master);
        if (!sdd)
                return -EFAULT;

        pm_runtime_get_sync(&sdd->pdev->dev);
       // set spi clock rate
	   // spi->max_speed_hz = SPI_MAX_SPEED;
        clk_set_rate(sdd->src_clk, SPI_MAX_SPEED * 2);
//		speed = clk_get_rate(sdd->src_clk);
//		DEBUG_PRINT("spi clock %d\n", speed);
//		DEBUG_PRINT("%s return\n", __func__);
			/* Enable Clock */
		// exynos_update_ip_idle_status(sdd->idle_ip_index, 0);
		clk_prepare_enable(sdd->src_clk);
		
        return 0;
}

static int sec_spi_unprepare(struct platform_device *spi)
{
        struct s3c64xx_spi_driver_data *sdd = NULL;
	    DEBUG_PRINT("%s\n", __func__);
        sdd = spi_master_get_devdata(spi->master);
        if (!sdd)
                return -EFAULT;

        pm_runtime_put(&sdd->pdev->dev);
		DEBUG_PRINT("%s return\n", __func__);

        return 0;
}
#endif

/*-------------------------------------------------------------------------*/
#ifdef	Support_for_Sumsung_N9_Dual_sensor
int etspi_reset_request(struct etspi_data *etspi)
{
	int status = 0;
	DEBUG_PRINT("%s\n", __func__);
	if (etspi != NULL) {
		status = gpio_request(etspi->rstPin, "reset-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_Reset failed\n",
				__func__);
			goto etspi_reset_request_failed;
		}
		status = gpio_direction_output(etspi->rstPin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output Reset failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_reset_request_failed;
		}
		gpio_set_value(etspi->rstPin, 1); 
		DEBUG_PRINT("ets523: %s successful status=%d\n", __func__, status);
	}
	return status;
	etspi_reset_request_failed:
	gpio_free(etspi->rstPin);
	pr_err("%s is failed\n", __func__);
	return status;
}
#endif

static void etspi_reset(struct etspi_data *etspi)
{
	DEBUG_PRINT("%s\n", __func__);
	gpio_set_value(etspi->rstPin, 0);
	msleep(30);
	gpio_set_value(etspi->rstPin, 1);
	msleep(20);
}

static ssize_t etspi_read(struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t etspi_write(struct file *filp,
	const char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}
static ssize_t etspi_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int state = (*buf == '1') ? 1 : 0;
	FPS_notify(0xbeef, state);
	DEBUG_PRINT("%s  state = %d\n", __func__, state);
	return 1;
}
static DEVICE_ATTR(etspi_enable, S_IWUSR | S_IWGRP, NULL, etspi_enable_set);
static struct attribute *attributes[] = {
	&dev_attr_etspi_enable.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

/* HS70 code for HS70-861 by chenlei at 2019/11/20 start */
static int egistec_fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;
	int retval = 0;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */)
		return 0;

	blank = *(int *)evdata->data;
	DEBUG_PRINT(" enter, blank=0x%x\n", blank);

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_screen_onoff = 1;
		DEBUG_PRINT("lcd on notify  ------\n");
		break;

	case FB_BLANK_POWERDOWN:
		g_screen_onoff = 0;
		DEBUG_PRINT("lcd off notify  ------\n");
		break;

	default:
		pr_err("other notifier, ignore\n");
		break;
	}
	return retval;
}
/* HS70 code for HS70-861 by chenlei at 2019/11/20 end */

static long etspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int retval = 0;
	struct etspi_data *etspi;
	struct ioctl_cmd data;

	memset(&data, 0, sizeof(data));

	DEBUG_PRINT("%s\n", __func__);

	etspi = filp->private_data;

	switch (cmd) {
	case INT_TRIGGER_INIT:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			goto done;
		}
		finger_sysfs = 0x04;
		DEBUG_PRINT("fp_ioctl >>> fp Trigger function init\n");
		retval = Interrupt_Init(etspi, data.int_mode, data.detect_period, data.detect_threshold);
		DEBUG_PRINT("fp_ioctl trigger init = %x\n", retval);
		break;
	case FP_SENSOR_RESET:
		DEBUG_PRINT("fp_ioctl ioc->opcode == FP_SENSOR_RESET --");
		etspi_reset(etspi);
		goto done;
	case INT_TRIGGER_CLOSE:
		DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
		retval = Interrupt_Free(etspi);
		DEBUG_PRINT("fp_ioctl trigger close = %x\n", retval);
		goto done;
	case INT_TRIGGER_ABORT:
		DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
		fps_interrupt_abort();
		goto done;
#ifdef	Support_for_Sumsung_N9_Dual_sensor
	case FP_PIN_RESOURCE_REQUEST:
		DEBUG_PRINT("fp_ioctl <<< FP_RESET_REQUEST\n");
		retval = etspi_reset_request(etspi); 
		goto done;
#endif
/* HS70 code for HS70-861 by chenlei at 2019/11/20 start */
	case GET_SCREEN_ONOFF:
		DEBUG_PRINT("fp_ioctl <<< GET_SCREEN_ONOFF  \n");
		data.int_mode = g_screen_onoff;
		if (copy_to_user((int __user *)arg, &data, sizeof(data))) {
			retval = -EFAULT;
		}
		goto done;
/* HS70 code for HS70-861 by chenlei at 2019/11/20 end */
	case FP_WAKELOCK_TIMEOUT_ENABLE: //0Xb1
		DEBUG_PRINT("EGISTEC fp_ioctl <<< FP_WAKELOCK_TIMEOUT_ENABLE  \n");
		__pm_stay_awake(&wakeup_source_fp);
		goto done;
	case FP_WAKELOCK_TIMEOUT_DISABLE: //0Xb2
		DEBUG_PRINT("EGISTEC fp_ioctl <<< FP_WAKELOCK_TIMEOUT_DISABLE  \n");
		__pm_relax(&wakeup_source_fp);
		goto done;
	default:
		retval = -ENOTTY;
		break;
	}
done:
	return retval;
}

#ifdef CONFIG_COMPAT
static long etspi_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return etspi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define etspi_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int etspi_open(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;
	int			status = -ENXIO;

	DEBUG_PRINT("%s\n", __func__);

	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry)	{
		if (etspi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (etspi->buffer == NULL) {
			etspi->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buffer == NULL) {
				/* dev_dbg(&etspi->spi->dev, "open/ENOMEM\n"); */
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
		}
	} else {
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static int etspi_release(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;

	DEBUG_PRINT("%s\n", __func__);

	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
		int	dofree;

		kfree(etspi->buffer);
		etspi->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
	}
	mutex_unlock(&device_list_lock);
	return 0;

}



int etspi_platformInit(struct etspi_data *etspi)
{
	int status = 0;
	DEBUG_PRINT("%s\n", __func__);

	if (etspi != NULL) {
#ifdef	Not_support_for_Sumsung_N9_Dual_sensor
		/* Initial Reset Pin*/
		status = gpio_request(etspi->rstPin, "reset-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_Reset failed\n",
				__func__);
			goto etspi_platformInit_rst_failed;
		}
		gpio_direction_output(etspi->rstPin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output Reset failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_gpio_init_failed;
		}
		/* gpio_set_value(etspi->rstPin, 1); */
		pr_err("et523:  reset to high\n");
#endif
		/* initial power pin */
		status = gpio_request(etspi->vcc_Pin, "vcc-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset vcc_Pin failed\n",
				__func__);
			goto etspi_platformInit_gpio_init_failed;
		}
		gpio_direction_output(etspi->vcc_Pin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output vcc_Pin failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_gpio_init_failed;
		}
		gpio_set_value(etspi->vcc_Pin, 1);
		pr_err("ets523:  vcc_Pin set to high\n");
		#ifdef egis_1v8
		/* initial 18V power pin 
		status = gpio_request(etspi->vdd_18v_Pin, "18v-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset vdd_18v_Pin failed\n",
				__func__);
			goto etspi_platformInit_rst_failed;
		}
		gpio_direction_output(etspi->vdd_18v_Pin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output vdd_18v_Pin failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_rst_failed;
		}

		gpio_set_value(etspi->vdd_18v_Pin, 1);
		pr_err("ets523:  vdd_18v_Pin set to high\n");*/
		#endif

	}
	DEBUG_PRINT("ets523: %s successful status=%d\n", __func__, status);
	return status;

etspi_platformInit_gpio_init_failed:
#ifdef egis_1v8
	gpio_free(etspi->vdd_18v_Pin);
#endif
	gpio_free(etspi->vcc_Pin);
#ifdef	Not_support_for_Sumsung_N9_Dual_sensor
etspi_platformInit_rst_failed:
	gpio_free(etspi->rstPin);
#endif

	pr_err("%s is failed\n", __func__);
	return status;
}

static int etspi_parse_dt(struct device *device,
	struct etspi_data *data)
{
	struct device_node *np = device->of_node;
	int errorno = 0;
	int gpio;
	//struct regulator *vreg;
	//int ret;

	gpio = of_get_named_gpio(np, "egistec,gpio_rst", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->rstPin = gpio;
		DEBUG_PRINT("%s: reset_Pin=%d\n", __func__, data->rstPin);
	}
	gpio = of_get_named_gpio(np, "egistec,gpio_irq", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->irqPin = gpio;
		DEBUG_PRINT("%s: irq_Pin=%d\n", __func__, data->irqPin);
	}
#if 0	
	vreg = regulator_get(&data->spi->dev,"vdd_fp");
	if (!vreg) {
		pr_err( "Unable to get vdd_fp\n");
		goto dt_exit;
	}

	if (regulator_count_voltages(vreg) > 0) {
		ret = regulator_set_voltage(vreg, 2850000,2850000);
		if (ret){
			pr_err("Unable to set voltage on vdd_fp");
			goto dt_exit;
		}
	}
	ret = regulator_enable(vreg);
	if (ret) {
		pr_err("error enabling vdd_fp %d\n",ret);
		regulator_put(vreg);
		vreg = NULL;
		goto dt_exit;
	}
	DEBUG_PRINT("Macle Set voltage on vdd_fp for egistec fingerprint");
#endif
#if 0
	gpio = of_get_named_gpio(np, "egistec,gpio_pwr_en", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->vcc_Pin = gpio;
		pr_info("%s: power pin=%d\n", __func__, data->vcc_Pin);
	}

#ifdef egis_1v8
	gpio = of_get_named_gpio(np, "egistec,gpio_ldo1p8_en", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->vdd_18v_Pin = gpio;
		pr_info("%s: 18v power pin=%d\n", __func__, data->vdd_18v_Pin);
	}
#endif
#endif
	DEBUG_PRINT("%s is successful\n", __func__);
	return errorno;
dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}

static const struct file_operations etspi_fops = {
	.owner = THIS_MODULE,
	.write = etspi_write,
	.read = etspi_read,
	.unlocked_ioctl = etspi_ioctl,
	.compat_ioctl = etspi_compat_ioctl,
	.open = etspi_open,
	.release = etspi_release,
	.llseek = no_llseek,
	.poll = fps_interrupt_poll
};

/*-------------------------------------------------------------------------*/

static struct class *etspi_class;

static int etspi_probe(struct platform_device *pdev);
static int etspi_remove(struct platform_device *pdev);

static struct of_device_id etspi_match_table[] = {
	{ .compatible = "egistec,et523",},
	{},
};
MODULE_DEVICE_TABLE(of, etspi_match_table);

/* fp_id is used only when dual sensor need to be support  */
#if 0
static struct of_device_id fp_id_match_table[] = {
	{ .compatible = "egistec,et523",},
	{},
};
MODULE_DEVICE_TABLE(of, fp_id_match_table);
#endif



static struct platform_driver etspi_driver = {
	.driver = {
		.name		= "et523",
		.owner		= THIS_MODULE,
		.of_match_table = etspi_match_table,
	},
    .probe =    etspi_probe,
    .remove =   etspi_remove,
};
/* remark for dual sensors */
/* module_platform_driver(etspi_driver); */



static int etspi_remove(struct platform_device *pdev)
{
	DEBUG_PRINT("%s(#%d)\n", __func__, __LINE__);
	free_irq(gpio_irq, NULL);
	//del_timer_sync(&fps_ints.timer);
	wakeup_source_destroy(&wakeup_source_fp);
	request_irq_done = 0;
	/* t_mode = 255; */
	return 0;
}

static int etspi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct etspi_data *etspi;
	int status = 0;
	unsigned long minor;
	int ret;
	struct regulator *vreg;

	/* int retval; */

	DEBUG_PRINT("%s initial\n", __func__);

	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET523_MAJOR, "et523", &etspi_fops);
	if (status < 0) {
		pr_err("%s register_chrdev error.\n", __func__);
		return status;
	}

	etspi_class = class_create(THIS_MODULE, "et523");
	if (IS_ERR(etspi_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET523_MAJOR, etspi_driver.driver.name);
		return PTR_ERR(etspi_class);
	}

	/* Allocate driver data */
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (etspi == NULL) {
		pr_err("%s - Failed to kzalloc\n", __func__);
		return -ENOMEM;
	}
/* HS70 code for HS70-861 by chenlei at 2019/11/20 start */
	etspi->notifier.notifier_call = egistec_fb_notifier_callback;
	status = fb_register_client(&etspi->notifier);
	if (status){
		pr_err(" register fb failed, retval=%d\n", status);
	}
/* HS70 code for HS70-861 by chenlei at 2019/11/20 end */	
	/* device tree call */
	if (pdev->dev.of_node) {
		status = etspi_parse_dt(&pdev->dev, etspi);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto etspi_probe_parse_dt_failed;
		}
	}
	
	vreg = regulator_get(&pdev->dev,"avdd");
	if (!vreg) {
		pr_err( "Unable to get vdd_fp\n");
		goto etspi_probe_parse_dt_failed;
	}
#if 0
	if (regulator_count_voltages(vreg) > 0) {
		ret = regulator_set_voltage(vreg, 3000000,3000000);
		if (ret){
			pr_err("Unable to set voltage on vdd_fp");
			goto etspi_probe_parse_dt_failed;
		}
	}
#endif
	ret = regulator_set_load(vreg,25000);
	if (ret){
    	pr_err("set avdd load current is fail\n");
		regulator_put(vreg);
		vreg = NULL;
		goto etspi_probe_parse_dt_failed;
	}
	ret = regulator_enable(vreg);
	if (ret) {
		pr_err("error enabling vdd_fp %d\n",ret);
		regulator_put(vreg);
		vreg = NULL;
		goto etspi_probe_parse_dt_failed;
	}
	DEBUG_PRINT("Macle Set voltage on vdd_fp for egistec fingerprint");


	/* Initialize the driver data */
	etspi->spi = pdev;
	g_data = etspi;
	g_data->irq_enable_flag = 0;
	spin_lock_init(&etspi->spi_lock);
	spin_lock_init(&g_data->irq_lock);
	//spin_lock_init(&interrupt_lock);
	
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&etspi->device_entry);

	/* platform init */
	status = etspi_platformInit(etspi);
	if (status != 0) {
		pr_err("%s platformInit failed\n", __func__);
		goto etspi_probe_platformInit_failed;
	}

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

#ifdef ETSPI_NORMAL_MODE
/*
	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
	status = spi_setup(spi);
	if (status != 0) {
		pr_err("%s spi_setup() is failed. status : %d\n",
			__func__, status);
		return status;
	}
*/
#endif

	/*
	 * If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *fdev;
		etspi->devt = MKDEV(ET523_MAJOR, minor);
		fdev = device_create(etspi_class, &pdev->dev, etspi->devt, etspi, "esfp0");
		status = IS_ERR(fdev) ? PTR_ERR(fdev) : 0;
	} else {
		dev_dbg(&pdev->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}

#if EGIS_NAVI_INPUT
	/*
	 * William Add.
	 */
	 DEBUG_PRINT("%s : go to navi_input\n",
		__func__);
	sysfs_egis_init(etspi);
	uinput_egis_init(etspi);
#endif

	mutex_unlock(&device_list_lock);

	if (status == 0) {
		//spi_set_drvdata(pdev, etspi); //corki
		dev_set_drvdata(dev, etspi);
	} else {
		goto etspi_probe_failed;
	}
	etspi_reset(etspi);

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

	/* the timer is for ET310 */
	setup_timer(&fps_ints.timer, interrupt_timer_routine, (unsigned long)&fps_ints);
	add_timer(&fps_ints.timer);
	wakeup_source_init(&wakeup_source_fp, "et580_wakeup");
	DEBUG_PRINT("  add_timer ---- \n");
//	Interrupt_Init(etspi, LEVEL_TRIGGER_LOW, 0, 0);
	DEBUG_PRINT("%s : initialize success %d\n",
		__func__, status);

	status = sysfs_create_group(&dev->kobj, &attribute_group);
	if (status) {
		pr_err("%s could not create sysfs\n", __func__);
		goto etspi_probe_failed;
	}
	request_irq_done = 0;
	return status;

etspi_probe_failed:
	device_destroy(etspi_class, etspi->devt);
	class_destroy(etspi_class);

etspi_probe_platformInit_failed:
etspi_probe_parse_dt_failed:
	kfree(etspi);
	pr_err("%s is failed\n", __func__);
#if EGIS_NAVI_INPUT
	uinput_egis_destroy(etspi);
	sysfs_egis_destroy(etspi);
#endif

	return status;
}

static int __init et523_init(void)
{
	int status = 0;
//	DEBUG_PRINT("%s  enter\n", __func__);
#if 0
	/* fp_id is used only when dual sensor need to be support  */
	struct device_node *fp_id_np = NULL;
	int fp_id_gpio = 0;
	int fp_id_gpio_value;
	
	DEBUG_PRINT("%s  enter\n", __func__);
	fp_id_np = of_find_matching_node(fp_id_np, fp_id_match_table);
	if (fp_id_np)
	    fp_id_gpio = of_get_named_gpio(fp_id_np, "egistec,gpio_id", 0);

	if (!gpio_is_valid(fp_id_gpio)) {
		/* errorno = gpio; */
		DEBUG_PRINT("%s: device tree error \n", __func__);
		return status;
	} else {
		/* data->IDPin = gpio; */
		DEBUG_PRINT("%s: fp_id Pin=%d\n", __func__, fp_id_gpio);
	}
	gpio_direction_input(fp_id_gpio);
	fp_id_gpio_value = gpio_get_value(fp_id_gpio);
	if (fp_id_gpio_value == 1)
	{
		set_fp_ta_name(FP_SENSOR_NAME, sizeof(FP_SENSOR_NAME));
	    DEBUG_PRINT("%s:  Load et523-int driver \n", __func__);
	}
	else {
		DEBUG_PRINT("%s:  Load fpc driver \n", __func__);
		return status;
    }
#endif
	status = platform_driver_register(&etspi_driver);
	DEBUG_PRINT("%s  done\n", __func__);

	return status;
}

static void __exit et523_exit(void)
{
	platform_driver_unregister(&etspi_driver);
}

module_init(et523_init);
module_exit(et523_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for et523");
MODULE_LICENSE("GPL");
