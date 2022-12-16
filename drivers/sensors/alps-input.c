#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include "sensors_core.h"
#include <linux/alps_compass_io.h>
#include <linux/input-polldev.h>

#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z

#define EVENT_TYPE_MAG_X           ABS_X
#define EVENT_TYPE_MAG_Y           ABS_Y
#define EVENT_TYPE_MAG_Z           ABS_Z

#define ALPS_POLL_INTERVAL   200    /* msecs */
#define ALPS_INPUT_FUZZ        0    /* input event threshold */
#define ALPS_INPUT_FLAT        0

#define ON		1
#define OFF		0

struct alps_data {
	struct platform_device *pdev;
	struct input_polled_dev *alps_idev;
	struct mutex alps_lock;
	struct miscdevice alps_device;
	int suspend_flag;
	int delay;
	int flgM;
	int flgA;
};

/* for I/O Control */
static long alps_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -1, tmpval;
	void __user *argp = (void __user *)arg;
	struct alps_data *data = container_of(filp->private_data,
					     struct alps_data, alps_device);

	switch (cmd) {
	case ALPSIO_SET_MAGACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			pr_err("%s : failed for ALPSIO_SET_MAGACTIVATE\n",
				__func__);
			return -EFAULT;
		}
		pr_debug("%s : ALPSIO_SET_MAGACTIVATE, flgM = %d\n", __func__,
			tmpval);
		mutex_lock(&data->alps_lock);
		data->flgM = tmpval;
		hscd_activate(1, data->flgM, data->delay);
		mutex_unlock(&data->alps_lock);
		break;
	case ALPSIO_SET_ACCACTIVATE:
		pr_debug("%s : failed for ALPSIO_SET_ACCACTIVATE\n", __func__);
		break;
	case ALPSIO_SET_DELAY:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			pr_err("%s : failed for ALPSIO_SET_DELAY\n", __func__);
			return -EFAULT;
		}

		mutex_lock(&data->alps_lock);
		if (tmpval <= 10)
			tmpval = 10;
		else if (tmpval <= 20)
			tmpval = 20;
		else if (tmpval <=  70)
			tmpval =  70;
		else
			tmpval = 200;
		data->delay = tmpval;

		if (data->flgM)
			hscd_activate(1, data->flgM, data->delay);
		mutex_unlock(&data->alps_lock);

		pr_debug("%s : ALPSIO_SET_DELAY, delay = %d\n", __func__,
			data->delay);

		break;
	case ALPSIO_ACT_SELF_TEST_A:
		pr_debug("%s : ALPSIO_ACT_SELF_TEST_A\n", __func__);
		mutex_lock(&data->alps_lock);
		ret = hscd_self_test_A();
		mutex_unlock(&data->alps_lock);

		if (copy_to_user(argp, &ret, sizeof(ret))) {
			pr_err("%s : failed for ALPSIO_ACT_SELF_TEST_A\n",
				__func__);
			return -EFAULT;
		}
		break;

	case ALPSIO_ACT_SELF_TEST_B:
		pr_debug("%s : ALPSIO_ACT_SELF_TEST_A\n", __func__);
		mutex_lock(&data->alps_lock);
		ret = hscd_self_test_B();
		mutex_unlock(&data->alps_lock);

		if (copy_to_user(argp, &ret, sizeof(ret))) {
			pr_err("%s : failed for ALPSIO_ACT_SELF_TEST_B\n",
				__func__);
			return -EFAULT;
		}
		break;

	default:
		pr_debug("%s : err, flgM = %d\n", __func__, tmpval);
		return -ENOTTY;
	}
	return 0;
}

static int alps_io_open(struct inode *inode, struct file *filp)
{
	pr_info("alps_io_open\n");
	return 0;
}

static int alps_io_release(struct inode *inode, struct file *filp)
{
	pr_info("alps_io_release\n");
	return 0;
}

static const struct file_operations alps_fops = {
	.owner   = THIS_MODULE,
	.open    = alps_io_open,
	.release = alps_io_release,
	.unlocked_ioctl = alps_ioctl,
};

static void hscd_poll(struct input_dev *idev)
{
	int xyz[3];

	if (hscd_get_magnetic_field_data(xyz) == 0) {
		input_report_abs(idev, EVENT_TYPE_MAG_X, xyz[0]);
		input_report_abs(idev, EVENT_TYPE_MAG_Y, xyz[1]);
		input_report_abs(idev, EVENT_TYPE_MAG_Z, xyz[2]);
		input_event(idev, EV_SYN, SYN_REPORT, 2);
	}
}

static void alps_poll(struct input_polled_dev *dev)
{
	struct alps_data *data = dev->private;

	if (!data->suspend_flag) {
		mutex_lock(&data->alps_lock);
		data->alps_idev->poll_interval = data->delay;

		if (data->flgM)
			hscd_poll(data->alps_idev->input);
		mutex_unlock(&data->alps_lock);
	}
}

#ifdef CONFIG_OF
static struct of_device_id alps_match_table[] = {
	{.compatible = "alps-input",},
	{},
};
#else
#define magnetic_match_table NULL
#endif

static int alps_probe(struct platform_device *dev)
{
	int ret;
	struct alps_data *data;
	struct input_dev *idev;

	pr_info("alps: alps_probe\n");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s, failed to alloc memory for module data\n",
			__func__);
		ret = -ENOMEM;
		goto exit;
	}
	mutex_init(&data->alps_lock);

	data->pdev = platform_device_register_simple("alps", -1, NULL, 0);
	if (IS_ERR(data->pdev)) {
		ret = PTR_ERR(data->pdev);
		goto out_driver;
	}

	data->alps_idev = input_allocate_polled_device();
	if (data->alps_idev == NULL) {
		ret = -ENOMEM;
		goto out_device;
	}

	data->alps_idev->poll = alps_poll;
	data->alps_idev->poll_interval = ALPS_POLL_INTERVAL;
	data->alps_idev->private = data;

	/* initialize the input class */
	idev = data->alps_idev->input;
	idev->name = "magnetic_sensor";
	idev->phys = "alps/input0";
	idev->id.bustype = BUS_HOST;
	idev->dev.parent = &data->pdev->dev;
	idev->evbit[0] = BIT_MASK(EV_ABS);

	/* 12-bit output (BMA254) with +/- 2g range */
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_X,
			2048, 2047, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Y,
			2048, 2047, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Z,
			2048, 2047, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);

	/* 15-bit output (HSCDTD008A) */
	input_set_abs_params(idev, EVENT_TYPE_MAG_X,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAG_Y,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAG_Z,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);

	ret = input_register_polled_device(data->alps_idev);
	if (ret)
		goto out_idev;

	data->alps_device.minor = MISC_DYNAMIC_MINOR;
	data->alps_device.name = "alps_io";
	data->alps_device.fops = &alps_fops;
	ret = misc_register(&data->alps_device);
	if (ret)
		goto exit_misc_device_register_failed;

	ret = sensors_create_symlink(&idev->dev.kobj, idev->name);
	if (ret < 0) {
		pr_err("failed to create symlink\n");
		return ret;
	}
	mutex_lock(&data->alps_lock);
	data->flgA = 0;
	data->flgM = 0;
	data->delay = 200;
	data->suspend_flag = OFF;
	mutex_unlock(&data->alps_lock);
	platform_set_drvdata(dev, data);
	pr_info("%s: success.\n", __func__);
	return 0;

exit_misc_device_register_failed:
	input_unregister_polled_device(data->alps_idev);
out_idev:
	input_free_polled_device(data->alps_idev);
out_device:
	platform_device_unregister(data->pdev);
out_driver:
	mutex_destroy(&data->alps_lock);
	kfree(data);
exit:
	pr_err("%s: failed!\n", __func__);
	return ret;
}

static int alps_remove(struct platform_device *dev)
{
	struct alps_data *data = platform_get_drvdata(dev);
	misc_deregister(&data->alps_device);
	input_unregister_polled_device(data->alps_idev);
	input_free_polled_device(data->alps_idev);
	platform_device_unregister(data->pdev);
	mutex_destroy(&data->alps_lock);

	kfree(data);

	return 0;
}

static int alps_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct alps_data *data = platform_get_drvdata(pdev);
	mutex_lock(&data->alps_lock);
	data->suspend_flag = ON;
	mutex_unlock(&data->alps_lock);
	return 0;
}

static int alps_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct alps_data *data = platform_get_drvdata(pdev);
	mutex_lock(&data->alps_lock);
	data->suspend_flag = OFF;
	mutex_unlock(&data->alps_lock);
	return 0;
}

static const struct dev_pm_ops alps_pm_ops = {
	.suspend = alps_suspend,
	.resume = alps_resume
};

static struct platform_driver alps_driver = {
	.driver	= {
		.name = "alps-input",
		.owner = THIS_MODULE,
		.of_match_table = alps_match_table,
	},
	.probe = alps_probe,
	.remove = alps_remove,
};

static int __init alps_init(void)
{
	return platform_driver_register(&alps_driver);
}

static void __exit alps_exit(void)
{
	platform_driver_unregister(&alps_driver);
}

module_init(alps_init);
module_exit(alps_exit);

MODULE_DESCRIPTION("Alps Input Device");
MODULE_AUTHOR("ALPS");
MODULE_LICENSE("GPL v2");
