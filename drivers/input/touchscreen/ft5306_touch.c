/*
 * * Copyright (C) 2009, Notioni Corporation chenjian@notioni.com).
 * *
 * * Author: jeremy.chen
 * *
 * * This software program is licensed subject to the GNU General Public License
 * * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 * */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/suspend.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/input/mt.h>
#include <linux/pm_runtime.h>
#include <linux/i2c/ft5306_touch.h>

#include <mach/gpio.h>
#include <plat/mfp.h>

#define MAX_FINGER		(5)  /* ft5306 can only support max 5 point */

#define FT5306_LEN 31

#define POINT_PUTDOWN		(0)
#define POINT_PUTUP			(1)
#define POINT_CONTACT		(2)
#define POINT_INVALID		(3)

struct touch_finger {
	int pi;			/* point index */
	int ps;			/* point status */
	u16 px;			/* point x */
	u16 py;			/* point y */
};

struct ft5306_touch {
	struct input_dev *idev;
	struct input_dev *virtual_key;
	struct i2c_client *i2c;
	struct work_struct work;
	struct ft5306_touch_platform_data *data;
	struct touch_finger finger[MAX_FINGER];
	struct mutex lock;
	int power_on;
	int irq;
};

static struct ft5306_touch *touch;
#ifdef CONFIG_PM_RUNTIME
static u8 ft5306_mode_cmd_sleep[2] = { 0xA5, 0x03 };
#endif
static u8 ft5306_cmd[2] = { 0x0, 0x0 };

int ft5306_touch_read_reg(u8 reg, u8 *pval)
{
	int ret;
	int status;

	if (touch->i2c == NULL)
		return -1;
	ret = i2c_smbus_read_byte_data(touch->i2c, reg);
	if (ret >= 0) {
		*pval = ret;
		status = 0;
	} else {
		status = -EIO;
	}

	return status;
}

int ft5306_touch_write_reg(u8 reg, u8 val)
{
	int ret;
	int status;

	if (touch->i2c == NULL)
		return -1;
	ret = i2c_smbus_write_byte_data(touch->i2c, reg, val);
	if (ret == 0)
		status = 0;
	else
		status = -EIO;

	return status;
}

static int ft5306_touch_read(char *buf, int count)
{
	int ret;

	ret = i2c_master_recv(touch->i2c, (char *) buf, count);

	return ret;
}

static int ft5306_touch_write(char *buf, int count)
{
	int ret;

	ret = i2c_master_send(touch->i2c, buf, count);

	return ret;
}

static u8 buflog[FT5306_LEN * 5], *pbuf;
static inline int ft5306_touch_read_data(struct ft5306_touch *data)
{
	int ps, pi, i, b, ret;
	u8 buf[FT5306_LEN];
	u16 px, py;

	memset(data->finger, 0xFF, MAX_FINGER * sizeof(struct touch_finger));

	ret = ft5306_touch_read(buf, FT5306_LEN);
	if (ret < 0)
		goto out;

	pbuf = buflog;
	for (i = 0; i < FT5306_LEN; ++i)
		pbuf += sprintf(pbuf, "%02x ", buf[i]);

	dev_dbg(&data->i2c->dev, "RAW DATA: %s\n", buflog);

	for (i = 0; i < MAX_FINGER; ++i) {
		b = 3 + i * 6;
		px = ((u16) (buf[b + 0] & 0x0F) << 8) | (u16) buf[b + 1];
		py = ((u16) (buf[b + 2] & 0x0F) << 8) | (u16) buf[b + 3];
		ps = ((u16) (buf[b + 0] & 0xC0) >> 6);
		pi = ((u16) (buf[b + 2] & 0xF0) >> 4);

		data->finger[i].px = px;
		data->finger[i].py = py;
		data->finger[i].ps = ps;
		data->finger[i].pi = pi;
	}
out:
	return ret;
}

static void ft5306_touch_work(struct work_struct *work)
{
	struct i2c_client *client = touch->i2c;
	struct input_dev *input_dev = touch->idev;
	int status, i, ret;
	int pi, ps;
	u16 px, py, tmp;

	if (!touch->power_on)
		return;

	dev_dbg(&client->dev, "I'm in ft5306 work\n");

	ret = ft5306_touch_read_data(touch);

	for (i = 0; i < MAX_FINGER; ++i) {

		dev_dbg(&client->dev,
			"REPP: i=%d pi=%d ps=0x%02x px=%d py=%d\n",
			i, touch->finger[i].pi, touch->finger[i].ps,
			touch->finger[i].px, touch->finger[i].py);

		ps = touch->finger[i].ps;
		if (POINT_INVALID == ps)
			continue;

		pi = touch->finger[i].pi;
		status = (POINT_PUTUP != ps);

		input_mt_slot(input_dev, pi);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, status);

		if (status) {
			px = touch->finger[i].px;
			py = touch->finger[i].py;
			if (touch->data->abs_flag == 1) {
				tmp = px;
				px = py;
				py = tmp;
			} else if (touch->data->abs_flag == 2) {
				tmp = px;
				px = py;
				py = touch->data->abs_y_max - tmp;
			} else if (touch->data->abs_flag == 3) {
				tmp = px;
				px = touch->data->abs_x_max - py;
				py = tmp;
			}
			dev_dbg(&client->dev, "x: %d y:%d\n", px, py);
			input_report_abs(input_dev, ABS_MT_POSITION_X, px);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, py);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 16);
		}
	}

	input_sync(input_dev);
}

static irqreturn_t ft5306_touch_irq_handler(int irq, void *dev_id)
{
	dev_dbg(&touch->i2c->dev, "ft5306_touch_irq_handler.\n");

	schedule_work(&touch->work);

	return IRQ_HANDLED;
}

static int index;
static ssize_t ft5306_reg_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	u8 reg_val;

	if ((index < 0) || (index > FT5306_LEN))
		return 0;

	ft5306_touch_read_reg(index, (u8 *) &reg_val);
	dev_info(dev, "register 0x%x: 0x%x\n", index, reg_val);
	return 0;
}

static ssize_t ft5306_reg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t len)
{
	int ret;
	char vol[256] = { 0 };
	u32 reg = 0, val = 0;
	int i;

	if (len > 256)
		len = 256;

	if ('w' == buff[0]) {
		memcpy(vol, buff + 2, 4);
		reg = (int) simple_strtoul(vol, NULL, 16);
		memcpy(vol, buff + 7, 4);
		val = (int) simple_strtoul(vol, NULL, 16);
		ft5306_cmd[0] = reg;
		ft5306_cmd[1] = val;
		ret = ft5306_touch_write(ft5306_cmd, 2);
		dev_info(dev, "write! reg:0x%x, val:0x%x\n", reg, val);

	} else if ('r' == buff[0]) {
		memcpy(vol, buff + 2, 4);
		reg = (int) simple_strtoul(vol, NULL, 16);
		ret = ft5306_touch_read_reg(reg, (u8 *) &val);
		dev_info(dev, "Read! reg:0x%x, val:0x%x\n", reg, val);

	} else if ('d' == buff[0]) {
		for (i = 0x00; i <= 0x3E; i++) {
			reg = i;
			ft5306_touch_read_reg(reg, (u8 *) &val);
			msleep(2);
			dev_info(dev, "Display! reg:0x%x, val:0x%x\n",
				 reg, val);
		}
	}
	return len;
}

static DEVICE_ATTR(reg_show, 0444, ft5306_reg_show, NULL);
static DEVICE_ATTR(reg_store, 0664, NULL, ft5306_reg_store);

static struct attribute *ft5306_attributes[] = {
	&dev_attr_reg_show.attr,
	&dev_attr_reg_store.attr,
	NULL
};

static const struct attribute_group ft5306_attr_group = {
	.attrs = ft5306_attributes,
};

#ifdef CONFIG_PM_RUNTIME
static void ft5306_touch_wakeup_reset(void)
{
	struct input_dev *input_dev = touch->idev;
	int i = 0, ret = 0;

	ret = ft5306_touch_read_data(touch);

	for (i = 0; i < MAX_FINGER; ++i) {
		input_mt_slot(input_dev, i);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(input_dev);
	return;
}

static int ft5306_runtime_suspend(struct device *dev)
{
	int ret, i = 0;

sleep_retry:
	ret = ft5306_touch_write(ft5306_mode_cmd_sleep, 2);
	if (ret < 0) {
		if (i < 10) {
			msleep(5);
			i++;
			dev_dbg(&touch->i2c->dev,
			"ft5306_touch can't enter sleep, retry %d\n", i);
			goto sleep_retry;
		}
		dev_info(&touch->i2c->dev,
			"ft5306_touch can't enter sleep\n");
		return 0;
	} else
		dev_dbg(&touch->i2c->dev,
			"ft5306_touch enter sleep mode.\n");

	if (touch->data->power && touch->power_on) {
		touch->data->power(dev, 0);
		mutex_lock(&touch->lock);
		touch->power_on = 0;
		mutex_unlock(&touch->lock);
	}
	return 0;
}

static int ft5306_runtime_resume(struct device *dev)
{
	if (touch->data->power && !touch->power_on)
		touch->data->power(dev, 1);

	msleep(10);
	if (touch->data->reset)
		touch->data->reset();

	ft5306_touch_wakeup_reset();
	mutex_lock(&touch->lock);
	if (!touch->power_on)
		touch->power_on = 1;
	mutex_unlock(&touch->lock);
	return 0;
}
#endif				/* CONFIG_PM_RUNTIME */

static int __devinit
ft5306_touch_probe(struct i2c_client *client,
		   const struct i2c_device_id *id)
{
	struct input_dev *input_dev;
	int maxx, maxy;
	int ret;
	u8 reg_val;

	dev_dbg(&client->dev, "ft5306_touch.c----ft5306_touch_probe.\n");

	touch = kzalloc(sizeof(struct ft5306_touch), GFP_KERNEL);
	if (touch == NULL)
		return -ENOMEM;

	touch->data = client->dev.platform_data;
	if (touch->data == NULL) {
		dev_dbg(&client->dev, "no platform data\n");
		return -EINVAL;
	}
	touch->i2c = client;
	touch->irq = client->irq;
	mutex_init(&touch->lock);

	/* register input device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	touch->idev = input_dev;
	touch->idev->name = "ft5306-ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	maxx = touch->data->abs_x_max;
	maxy = touch->data->abs_y_max;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_dev->absbit);

	input_mt_init_slots(input_dev, MAX_FINGER);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, maxx, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, maxy, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 16, 0, 0);

	ret = input_register_device(touch->idev);
	if (ret) {
		dev_dbg(&client->dev,
			"%s: unabled to register input device, ret = %d\n",
			__func__, ret);
		goto out_rg;
	}
	pm_runtime_enable(&client->dev);
	pm_runtime_get_sync(&client->dev);
	ret = ft5306_touch_read_reg(0x00, (u8 *) &reg_val);
	pm_runtime_put_sync_suspend(&client->dev);
	if (ret < 0) {
		dev_dbg(&client->dev, "ft5306 detect fail!\n");
		touch->i2c = NULL;
		goto out_rg;
	} else {
		dev_dbg(&client->dev, "ft5306 detect ok.\n");
	}

	if (touch->data->set_virtual_key)
		ret = touch->data->set_virtual_key(input_dev);
	BUG_ON(ret != 0);

	ret = request_irq(touch->irq, ft5306_touch_irq_handler,
			  IRQF_DISABLED | IRQF_TRIGGER_FALLING,
			  "ft5306 touch", touch);
	if (ret < 0) {
		dev_info(&client->dev,
			"Request IRQ for Bigstream touch failed, return:%d\n",
			ret);
		goto out_rg;
	}
	INIT_WORK(&touch->work, ft5306_touch_work);

	ret = sysfs_create_group(&client->dev.kobj, &ft5306_attr_group);
	if (ret)
		goto out_irg;

	pm_runtime_forbid(&client->dev);
	return 0;

out_irg:
	free_irq(touch->irq, touch);
out_rg:
	pm_runtime_disable(&client->dev);
	input_free_device(touch->idev);
out:
	kfree(touch);
	return ret;

}

static int ft5306_touch_remove(struct i2c_client *client)
{
	pm_runtime_disable(&client->dev);
	free_irq(touch->irq, touch);
	sysfs_remove_group(&client->dev.kobj, &ft5306_attr_group);
	input_unregister_device(touch->idev);
	return 0;
}

static const struct dev_pm_ops ft5306_ts_pmops = {
	SET_RUNTIME_PM_OPS(ft5306_runtime_suspend,
			   ft5306_runtime_resume, NULL)
};

static const struct i2c_device_id ft5306_touch_id[] = {
	{"ft5306_touch", 0},
	{}
};

static struct i2c_driver ft5306_touch_driver = {
	.driver = {
		.name = "ft5306_touch",
		.owner = THIS_MODULE,
		.pm = &ft5306_ts_pmops,
	},
	.id_table = ft5306_touch_id,
	.probe = ft5306_touch_probe,
	.remove = ft5306_touch_remove,
};

module_i2c_driver(ft5306_touch_driver);

MODULE_DESCRIPTION("ft5306 touch Driver");
MODULE_LICENSE("GPL");
