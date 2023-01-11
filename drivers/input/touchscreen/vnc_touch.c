/*
 * linux/drivers/input/touchscreen/vnc_touch.c
 *
 * touch screen driver for pxa935 - vnc
 *
 * Copyright (C) 2006, Marvell Corporation (israel.davidenko@Marvell.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>

#include <linux/uaccess.h>



#define VNC_DEBUG 1
#if VNC_DEBUG
#define dbg(format, arg...) printk(KERN_INFO"vnc_driver: " format "\n", ## arg)
#else
#define dbg(format, arg...)
#endif


enum {
 TSI_PEN_UNKNOW = 0,
 TSI_PEN_DOWN = 1,
 TSI_PEN_UP = 2
};

static u16 tem_x_pre = 0xffff;
static u16 tem_y_pre = 0xffff;


struct vnc_ts_data{
 spinlock_t ts_lock;
 struct task_struct *thread;
 int suspended;
 int pen_state;
 int use_count;
 int step;
};

 struct ts_data {
	u32 i1;
	u32 i2;
	u32 i3;
 };

static struct vnc_ts_data *vnc_td;
static struct input_dev *vnc_ts_input_dev;
static void vnc_ts_handler(u8 pen_state, u16 tem_x, u16 tem_y);


static ssize_t vnc_ts_proc_write(struct file	*file,
	const char __user	*buffer,
	size_t	count,
	loff_t	*data) {
	struct ts_data ts;

	if (count != sizeof(struct ts_data)) {
		dbg("size error %zd of %zd", count, sizeof(struct ts_data));
		return -EINVAL;
	}

	if (copy_from_user(&ts, buffer, sizeof(struct ts_data))) {
		dbg("vnc_ts_proc_write copy_from_user error");
		return -EFAULT;
	}

	vnc_ts_handler(ts.i1, ts.i2, ts.i3);
	return count;
}

int vnc_ts_read(u16 *x, u16 *y, int *pen_state)
{
	return 0;
}

static void vnc_ts_handler(u8 pen_state, u16 tem_x, u16 tem_y)
{
	static int iReportUp;

	if (pen_state) {
		dbg("pen down event x = %d,y = %d", tem_x, tem_y);

		iReportUp = 1;
		/*send measure to user here.*/
		input_report_abs(vnc_ts_input_dev, ABS_X, tem_x & 0xfff);
		input_report_abs(vnc_ts_input_dev, ABS_Y, tem_y & 0xfff);
		input_report_abs(vnc_ts_input_dev, ABS_PRESSURE, 1);
		input_report_key(vnc_ts_input_dev, BTN_TOUCH, 1);
		input_sync(vnc_ts_input_dev);

		tem_x_pre = tem_x;/* update for next points evaluate*/
		tem_y_pre = tem_y;/* update for next points evaluate*/
	} else if (iReportUp == 1) {
		iReportUp = 0;
		dbg("-------->vnc_ts_handler - pen up event");
		/* Report a pen up event */
		input_report_key(vnc_ts_input_dev, BTN_TOUCH, 0);
		input_sync(vnc_ts_input_dev);
		vnc_td->pen_state = TSI_PEN_UP;
	}

	return;
}

static int vnc_ts_open(struct input_dev *idev)
{
	unsigned long flags;

	dbg("%s: enter", __func__);

	if (vnc_td->suspended) {
		dbg("touch has been suspended!");
		return -1;
	}

	spin_lock_irqsave(&vnc_td->ts_lock, flags);

	if (vnc_td->use_count++ == 0)
		spin_unlock_irqrestore(&vnc_td->ts_lock, flags);
	else
		spin_unlock_irqrestore(&vnc_td->ts_lock, flags);

	return 0;
}

/* Kill the touchscreen thread and stop the touch digitiser. */
static void vnc_ts_close(struct input_dev *idev)
{
	unsigned long flags;

	dbg("%s: enter with use count = %d", __func__, vnc_td->use_count);

	spin_lock_irqsave(&vnc_td->ts_lock, flags);

	if (--vnc_td->use_count == 0)
		spin_unlock_irqrestore(&vnc_td->ts_lock, flags);
	else
		spin_unlock_irqrestore(&vnc_td->ts_lock, flags);
}

static const struct file_operations vnc_ts_proc_fops = {
	.write	=	vnc_ts_proc_write,
};

static int vnc_ts_probe(struct platform_device *pdev)
{
	int iResX = 0;
	int iResY = 640;

	int ret;
	struct proc_dir_entry *vnc_ts_proc_entry;

	dbg("------->vnc_ts_probe - start");

	vnc_td = kzalloc(sizeof(struct vnc_ts_data), GFP_KERNEL);
	if (!vnc_td) {
		ret = -ENOMEM;
		goto vnc_ts_out;
	}

	spin_lock_init(&vnc_td->ts_lock);
	vnc_td->pen_state = TSI_PEN_UP;
	vnc_td->suspended = 0;
	vnc_td->use_count = 0;

	/* register input device */
	vnc_ts_input_dev = input_allocate_device();
	if (vnc_ts_input_dev == NULL)
		return -ENOMEM;

	if (registered_fb[0]) {
		iResX = registered_fb[0]->var.xres;
		iResY = registered_fb[0]->var.yres;
	} else {
		iResX = 640;
		iResY = 480;
	}
	dbg("taken from fb iResX=%d iResY=%d", iResX, iResY);

	vnc_ts_input_dev->name = "vnc-ts";
	vnc_ts_input_dev->phys = "vnc-ts/input0";
	vnc_ts_input_dev->dev.parent = &pdev->dev;

	vnc_ts_input_dev->open = vnc_ts_open;
	vnc_ts_input_dev->close = vnc_ts_close;

	__set_bit(EV_ABS, vnc_ts_input_dev->evbit);
	__set_bit(ABS_X, vnc_ts_input_dev->absbit);
	__set_bit(ABS_Y, vnc_ts_input_dev->absbit);
	__set_bit(ABS_PRESSURE, vnc_ts_input_dev->absbit);

	__set_bit(EV_SYN, vnc_ts_input_dev->evbit);
	__set_bit(EV_KEY, vnc_ts_input_dev->evbit);
	__set_bit(BTN_TOUCH, vnc_ts_input_dev->keybit);

	input_set_abs_params(vnc_ts_input_dev, ABS_X, 0, iResX, 0, 0);
	input_set_abs_params(vnc_ts_input_dev, ABS_Y, 0, iResY, 0, 0);
	input_set_abs_params(vnc_ts_input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(vnc_ts_input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);

	ret = input_register_device(vnc_ts_input_dev);
	if (ret) {
		dbg("------->vnc_ts_probe - fail to register device");
		return ret;
	}

	vnc_ts_proc_entry = proc_create("driver/vnc_ts", 0, NULL, &vnc_ts_proc_fops);

	return 0;

vnc_ts_out:
	kfree(vnc_td);

	return ret;
}

static int vnc_ts_remove(struct platform_device *pdev)
{
	input_unregister_device(vnc_ts_input_dev);
	return 0;
}

static const struct of_device_id vnc_ts_of_match[] = {
	{ .compatible = "vnc-ts", },
	{ },
};
MODULE_DEVICE_TABLE(of, vnc_ts_of_match);

static struct platform_driver vnc_ts_driver = {
	.driver = {
		.name = "vnc-ts",
		.owner = THIS_MODULE,
		.of_match_table = vnc_ts_of_match,
	},
	.probe = vnc_ts_probe,
	.remove = vnc_ts_remove,
};
module_platform_driver(vnc_ts_driver);

MODULE_AUTHOR("Israel, Davidenko <israel.davidenko@marvell.com>");
MODULE_DESCRIPTION("vnc touch screen driver");
MODULE_LICENSE("GPL");
