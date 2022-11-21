/*
 * drivers/uio/uio_mvisp.c
 *
 * Marvell ISP UIO driver
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/uio_mvisp.h>

#include <mach/hardware.h>

#define VDEC_WORKING_BUFFER_SIZE	SZ_1M
#define UIO_MVISP_VERSION		"build-0"

#define MAX_NUM_SLOT	10
#define MAX_NUM_FILEHANDLE 10

#define BIT_INT_CLEAR		(0xC)
#define BIT_BUSY_FLAG		(0x160)

struct uio_mvisp_dev {
	struct uio_info uio_info;
	void *reg_base;
	struct clk *clk;
	struct mutex mutex;	/* used to protect open/release */
	int power_status;       /* 0-off 1-on */
	int clk_status;
	int filehandle_ins_num;
	int firmware_download;
};

static int mvisp_power_on(struct uio_mvisp_dev *cdev)
{
	if (cdev->power_status == 1)
		return 0;

	cdev->power_status = 1;

	return 0;
}

static int mvisp_clk_on(struct uio_mvisp_dev *cdev)
{
	if (cdev->clk_status != 0)
		return 0;

	clk_enable(cdev->clk);
	cdev->clk_status = 1;

	return 0;
}

static int mvisp_power_off(struct uio_mvisp_dev *cdev)
{
	if (cdev->power_status == 0)
		return 0;

	cdev->power_status = 0;
	return 0;
}

static int mvisp_clk_off(struct uio_mvisp_dev *cdev)
{
	if (cdev->clk_status == 0)
		return 0;

	clk_disable(cdev->clk);
	cdev->clk_status = 0;

	return 0;
}

static int active_mvisp(struct uio_mvisp_dev *cdev, int on)
{
	int ret;

	if (on) {
		ret = mvisp_power_on(cdev);
		if (ret)
			return -1;

		ret = mvisp_clk_on(cdev);
		if (ret)
			return -1;
	} else {
		ret = mvisp_clk_off(cdev);
		if (ret)
			return -1;

		ret = mvisp_power_off(cdev);
		if (ret)
			return -1;
	}

	cdev->firmware_download = 0;

	return 0;
}

static int mvisp_open(struct uio_info *info, struct inode *inode,
			void *file_priv)
{
	struct uio_mvisp_dev *cdev;
	int ret = 0;

	pr_debug("Enter uio-mvisp_open()\n");
	cdev = (struct uio_mvisp_dev *)info->priv;
	mutex_lock(&cdev->mutex);
	if (cdev->filehandle_ins_num < MAX_NUM_FILEHANDLE)
		cdev->filehandle_ins_num++;
	else {
		printk(KERN_ERR "current mvisp cannot accept " \
				"more file handle instance!!\n");
		ret = -EACCES;
		goto out;
	}

	if (cdev->filehandle_ins_num == 1) {
		if (0 != active_mvisp(cdev, 1)) {
			cdev->filehandle_ins_num = 0;
			ret = -EACCES;
			goto out;
		}
	}

out:
	mutex_unlock(&cdev->mutex);
	pr_debug("Leave mvisp_open(), ret = %d\n", ret);
	return ret;
}

static int mvisp_release(struct uio_info *info, struct inode *inode,
			void *file_priv)
{
	struct uio_mvisp_dev *cdev;
	int ret = 0;

	pr_debug("Enter mvisp_release()\n");
	cdev = (struct uio_mvisp_dev *)info->priv;
	mutex_lock(&cdev->mutex);

	if (cdev->filehandle_ins_num > 0) {
		cdev->filehandle_ins_num--;
		if (cdev->filehandle_ins_num == 0) {
			if (0 != active_mvisp(cdev, 0))
				ret = -EFAULT;
		}
	}
	mutex_unlock(&cdev->mutex);
	pr_debug("Leave mvisp_release(), ret = %d\n", ret);
	return ret;
}
static int mvisp_uio_name(struct uio_info *info,
		struct mvisp_get_uio_name *uio_name)
{
	if (!uio_name)
		return -EINVAL;

	strcpy(uio_name->driver, "uio-mvisp");
	return 0;
}
static int mvisp_ioctl(struct uio_info *info, unsigned int cmd,
			unsigned long arg, void *file_priv)
{
	int ret;
	struct uio_mvisp_dev *cdev = info->priv;

	switch (cmd) {
	case MVISP_UIO_NAME:
		mutex_lock(&cdev->mutex);
		ret = mvisp_uio_name(info,
				(struct mvisp_get_uio_name *)arg);
		mutex_unlock(&cdev->mutex);
		break;
	case MVISP_POWER_ON:
		mutex_lock(&cdev->mutex);
		ret = mvisp_power_on(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case MVISP_POWER_OFF:
		mutex_lock(&cdev->mutex);
		ret = mvisp_power_off(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case MVISP_CLK_ON:
		mutex_lock(&cdev->mutex);
		ret = mvisp_clk_on(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case MVISP_CLK_OFF:
		mutex_lock(&cdev->mutex);
		ret = mvisp_clk_off(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static int uio_mvisp_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct uio_mvisp_dev *cdev;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resources given\n");
		return -ENODEV;
	}

	cdev = devm_kzalloc(&pdev->dev, sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
		dev_err(&pdev->dev, "uio_mvisp_dev: out of memory\n");
		return -ENOMEM;
	}

	cdev->clk = clk_get(&pdev->dev, "ISP-CLK");
	if (IS_ERR(cdev->clk)) {
		dev_err(&pdev->dev, "cannot get mvisp clock\n");
		ret = PTR_ERR(cdev->clk);
		goto err_clk;
	}

	cdev->reg_base = (void *)ioremap(res->start, res->end - res->start + 1);
	if (!cdev->reg_base) {
		dev_err(&pdev->dev, "can't remap register area\n");
		ret = -ENOMEM;
		goto err_reg_base;
	}

	platform_set_drvdata(pdev, cdev);

	cdev->uio_info.name = UIO_MVISP_NAME;
	cdev->uio_info.version = UIO_MVISP_VERSION;
	cdev->uio_info.mem[0].internal_addr = (void __iomem *)cdev->reg_base;
	cdev->uio_info.mem[0].addr = res->start;
	cdev->uio_info.mem[0].memtype = UIO_MEM_PHYS;
	cdev->uio_info.mem[0].size = res->end - res->start + 1;

	cdev->uio_info.priv = cdev;

	cdev->uio_info.open = mvisp_open;
	cdev->uio_info.release = mvisp_release;
	cdev->uio_info.ioctl = mvisp_ioctl;
	cdev->uio_info.mmap = NULL;

	mutex_init(&(cdev->mutex));
	cdev->filehandle_ins_num = 0;

	ret = uio_register_device(&pdev->dev, &cdev->uio_info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register uio device\n");
		goto err_uio_register;
	}

	return 0;

err_uio_register:
	free_pages((unsigned long)cdev->uio_info.mem[1].internal_addr,
			get_order(VDEC_WORKING_BUFFER_SIZE));
err_reg_base:
	clk_put(cdev->clk);
err_clk:
	devm_kfree(&pdev->dev, cdev);

	return ret;
}

static int mvisp_remove(struct platform_device *pdev)
{
	struct uio_mvisp_dev *cdev = platform_get_drvdata(pdev);

	uio_unregister_device(&cdev->uio_info);
	free_pages((unsigned long)cdev->uio_info.mem[1].internal_addr,
			get_order(VDEC_WORKING_BUFFER_SIZE));
	iounmap(cdev->uio_info.mem[0].internal_addr);

	clk_put(cdev->clk);

	kfree(cdev);

	return 0;
}

static void mvisp_shutdown(struct platform_device *dev)
{
}

static struct platform_driver uio_mvisp_driver = {
	.probe = uio_mvisp_probe,
	.remove = mvisp_remove,
	.shutdown = mvisp_shutdown,
	.driver = {
		.name = UIO_MVISP_NAME,
		.owner = THIS_MODULE,
	},
};


module_platform_driver(uio_mvisp_driver);

MODULE_AUTHOR("Owen zhang(xinzha@marvell.com)");
MODULE_DESCRIPTION("UIO driver for Marvell ISP");
MODULE_LICENSE("GPL v0");
MODULE_ALIAS("platform:" UIO_MVISP_NAME);
