/*
 * Marvell SAR sensor driver
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

#define SAR_SENSOR_INPUT_NAME   "sarsensor"

/*********************************************************************
sarsensor(sarsensor)
range is 0~1
1-> indicate body closed
*********************************************************************/
struct sar_sensor_data {
	struct device *dev;
	struct input_dev *input_dev;
	unsigned gpio_inverted;
	int irq_gpio;
	int irq;
	struct mutex update_lock;
	struct work_struct sar_sensor_work;
};
static struct class *sar_sensor_class;
static ssize_t sar_sensor_value_read(int *status,
				     struct sar_sensor_data *sar_sensor_data);
static void sar_sensor_task(struct work_struct *work);

static void sar_sensor_report_values(struct sar_sensor_data *sar_sensor_data,
				     int value)
{
	input_report_abs(sar_sensor_data->input_dev, ABS_DISTANCE, value);
	/*printk("sar_sensor_data_report_values ...value = %d\n",value);*/
	input_sync(sar_sensor_data->input_dev);
}

static int sar_sensor_input_init(struct sar_sensor_data *sar_sensor_data)
{
	struct input_dev *input_dev;
	int err;
	input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;
	input_dev->name = SAR_SENSOR_INPUT_NAME;

	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SYN, input_dev->evbit);
	sar_sensor_data->input_dev = input_dev;
	err = input_register_device(sar_sensor_data->input_dev);
	if (err < 0) {
		input_free_device(sar_sensor_data->input_dev);
		return err;
	}
	return 0;
}

static void sar_sensor_input_delete(struct sar_sensor_data *sar_sensor_data)
{
	struct input_dev *input_dev = sar_sensor_data->input_dev;
	/*printk("entry -> sar_sensor_data_input_delete\n");*/
	input_unregister_device(input_dev);
	input_free_device(input_dev);
}

static ssize_t sar_sensor_value_read(int *status,
				     struct sar_sensor_data *sar_sensor_data)
{
	*status = gpio_get_value(sar_sensor_data->irq_gpio);
	pr_info("sarsensor gpio status:%d\n", *status);
	return 0;
}

static ssize_t sar_sensor_value_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret = -1;
	int sar_sensor_status;
	struct sar_sensor_data *sar_sensor_data = dev_get_drvdata(dev);
	/*printk("entry -> sar_sensor_value_show\n");*/
	ret = sar_sensor_value_read(&sar_sensor_status, sar_sensor_data);

	if (sar_sensor_data->gpio_inverted == 1)
		sar_sensor_status = (sar_sensor_status == 0) ? 1 : 0;
	return sprintf(buf, "%d\n", sar_sensor_status);
}

static DEVICE_ATTR(sar_sensor_data, S_IRUGO, sar_sensor_value_show, NULL);

static int create_sar_sensor_class(void)
{
	if (!sar_sensor_class) {
		sar_sensor_class = class_create(THIS_MODULE, "sarsensor");
		if (IS_ERR(sar_sensor_class))
			return PTR_ERR(sar_sensor_class);
	}
	return 0;
}

static int sar_sensor_dev_register(struct device *parent,
				   struct sar_sensor_data *sar_sensor_data)
{
	int ret = -1;

	ret = create_sar_sensor_class();

	if (ret) {
		/*printk("sar_sensor_data_sar_sensor_class:create
			class fail\n");*/
		return ret;
	}

	sar_sensor_data->dev =
	    device_create(sar_sensor_class, parent, 0, NULL, "sar_sensor");

	if (IS_ERR(sar_sensor_data->dev)) {
		/*printk("%s:sarsensor device_create fail\n", __func__);*/
		return PTR_ERR(sar_sensor_data->dev);
	}

	ret =
	    device_create_file(sar_sensor_data->dev, &dev_attr_sar_sensor_data);
	if (ret)
		return ret;

	dev_set_drvdata(sar_sensor_data->dev, sar_sensor_data);

	return 0;
}

static void sar_sensor_dev_unregister(struct sar_sensor_data *sar_sensor_data)
{
	device_remove_file(sar_sensor_data->dev, &dev_attr_sar_sensor_data);
	device_unregister(sar_sensor_data->dev);
	class_destroy(sar_sensor_class);
}

static void sar_sensor_task(struct work_struct *work)
{
	int sar_sensor_status;
	struct sar_sensor_data *sar_sensor_data =
	    container_of(work, struct sar_sensor_data, sar_sensor_work);
	mutex_lock(&sar_sensor_data->update_lock);
	sar_sensor_value_read(&sar_sensor_status, sar_sensor_data);

	if (sar_sensor_data->gpio_inverted == 1)
		sar_sensor_status = (sar_sensor_status == 0) ? 1 : 0;

	sar_sensor_report_values(sar_sensor_data, sar_sensor_status);
	mutex_unlock(&sar_sensor_data->update_lock);
}

static irqreturn_t sar_sensor_isr(int irq, void *sar_sensor_value)
{
	struct sar_sensor_data *sar_sensor_data = sar_sensor_value;
	schedule_work(&sar_sensor_data->sar_sensor_work);
	return IRQ_HANDLED;
}

static int sar_sensor_probe(struct platform_device *op)
{
	int err = -1;
	struct sar_sensor_data *sar_sensor_data = NULL;
	struct device_node *np = op->dev.of_node;

	sar_sensor_data = kzalloc(sizeof(struct sar_sensor_data), GFP_KERNEL);

	if (!sar_sensor_data) {
		err = -ENOMEM;
		goto exit;
	}

	sar_sensor_data->irq_gpio =
	    of_get_named_gpio(np, "sarsensor,irq-gpios", 0);

	dev_dbg(&op->dev, "sarsenor gpio number =%d\n",
		sar_sensor_data->irq_gpio);

	if (sar_sensor_data->irq_gpio < 0) {
		dev_err(&op->dev, "of_get_named_gpio irq faild\n");
		goto kfree_exit;
	}
	if (gpio_is_valid(sar_sensor_data->irq_gpio)) {
		if (of_get_property(np, "gpio-inverted", NULL))
			sar_sensor_data->gpio_inverted = 1;
		/* configure sensor gpio */
		err = gpio_request(sar_sensor_data->irq_gpio, "grip_irq_gpio");
		if (err) {
			dev_err(&op->dev, "unable to request gpio [%d]\n",
				sar_sensor_data->irq_gpio);
			goto kfree_exit;
		}
		err = gpio_direction_input(sar_sensor_data->irq_gpio);
		if (err) {
			dev_err(&op->dev,
				"unable to set direction for gpio [%d]\n",
				sar_sensor_data->irq_gpio);
			goto gpio_exit;
		}
	} else {
		dev_err(&op->dev, "irq gpio not provided\n");
		goto kfree_exit;
	}

	mutex_init(&sar_sensor_data->update_lock);

	INIT_WORK(&sar_sensor_data->sar_sensor_work, sar_sensor_task);

	sar_sensor_data->irq = gpio_to_irq(sar_sensor_data->irq_gpio);
	err =
	    request_irq(sar_sensor_data->irq, sar_sensor_isr,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"sarsensor1", sar_sensor_data);
	if (err) {
		dev_err(&op->dev, "%s request_irq error\n", __func__);
		goto gpio_exit;
	}
	/* register input */
	err = sar_sensor_input_init(sar_sensor_data);
	if (err < 0)
		goto gpio_exit;
	err = sar_sensor_dev_register(sar_sensor_data->dev, sar_sensor_data);
	if (err < 0) {
		dev_err(&op->dev, "sar_sensor_dev_register fail\n");
		goto err1;
	}

	dev_set_drvdata(&op->dev, sar_sensor_data);

	gpio_free(sar_sensor_data->irq_gpio);
	return 0;

err1:
	sar_sensor_dev_unregister(sar_sensor_data);
	sar_sensor_input_delete(sar_sensor_data);
gpio_exit:
	gpio_free(sar_sensor_data->irq_gpio);
kfree_exit:
	kfree(sar_sensor_data);
exit:
	dev_dbg(&op->dev, "%s return err= %d", __func__, err);
	return err;

}

static int sar_sensor_remove(struct platform_device *op)
{
	struct sar_sensor_data *sar_sensor = dev_get_drvdata(&op->dev);
	sar_sensor_input_delete(sar_sensor);
	dev_set_drvdata(&op->dev, NULL);
	return 0;
}

static const struct of_device_id sar_sensor_match[] = {
	{
	 .compatible = "SARS,sarsensor",
	 },
};

static struct platform_driver sar_sensor_driver = {
	.driver = {
		   .name = "sarsensor",
		   .owner = THIS_MODULE,
		   .of_match_table = sar_sensor_match,
		   },
	.probe = sar_sensor_probe,
	.remove = sar_sensor_remove,
};

static int __init sar_sensor_init(void)
{
	int err = platform_driver_register(&sar_sensor_driver);

	if (err)
		pr_err("sarsensor init failed err= %x\n", err);

	return err;
}

static void __exit sar_sensor_exit(void)
{
	platform_driver_unregister(&sar_sensor_driver);
}

MODULE_AUTHOR("add by ZhenqingQiu");
MODULE_DESCRIPTION("sarsensor driver");
MODULE_LICENSE("GPL");
module_init(sar_sensor_init);
module_exit(sar_sensor_exit);
