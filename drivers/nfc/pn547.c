/*
 * Copyright (C) 2010 Trusted Logic S.A.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/pn547.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>

#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#include <mach/exynos-fimc-is.h>
#include <mach/exynos-audio.h>
#include <linux/clk-provider.h>
#define CLK_USE_CAM1	2
#define CLK_USE_AUDIO	1
#endif
#endif

#define MAX_BUFFER_SIZE		512

#define NXP_KR_READ_IRQ_MODIFY

#ifdef NXP_KR_READ_IRQ_MODIFY
static bool do_reading;
static bool cancle_read;
#endif

#define NFC_DEBUG		0
#define MAX_TRY_I2C_READ	10
#define I2C_ADDR_READ_L		0x51
#define I2C_ADDR_READ_H		0x57
#define MULITPLE_ADDRESS_PN65T

struct pn547_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn547_device;
	struct wake_lock	nfc_wake_lock;
	int		ven_gpio;
	int		firm_gpio;
	int		irq_gpio;
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	int		clk_req_gpio;
	int		clk_req_irq;
	int		clk_use_check;
	struct clk *		clk;
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	struct platform_device *pdev;
#endif
	atomic_t		clk_enabled;
#endif
	atomic_t		irq_enabled;
};

static irqreturn_t pn547_dev_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
#if NFC_DEBUG
		pr_err("%s, irq_gpio = %d\n", __func__,
			gpio_get_value(pn547_dev->irq_gpio));
#endif
		return IRQ_HANDLED;
	}

#ifdef NXP_KR_READ_IRQ_MODIFY
	do_reading = true;
#endif
	/* Wake up waiting readers */
	wake_up(&pn547_dev->read_wq);

#if NFC_DEBUG
	pr_info("%s, IRQ_HANDLED\n", __func__);
#endif
	wake_lock_timeout(&pn547_dev->nfc_wake_lock, 2 * HZ);
	return IRQ_HANDLED;
}

#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
static int pn547_clk_initialize(struct pn547_dev *pn547_dev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	pn547_dev->clk = clk_get(&pn547_dev->client->dev, "sclk_isp_sensor1");
	if (IS_ERR(pn547_dev->clk)) {
            pr_err("%s : clk not found\n", __func__);
            return -EPERM;
	}
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	if (pn547_dev->clk_use_check == CLK_USE_CAM1) {
		u32 frequency;
		int ret;

		ret = fimc_is_set_parent_dt(pn547_dev->pdev, "mout_sclk_isp_sensor1", "oscclk");
		if (ret) {
			pr_err("%s, fimc_is_set_parent_dt:%d\n", __func__, ret);
			return -EPERM;
		}
		ret = fimc_is_set_rate_dt(pn547_dev->pdev, "dout_sclk_isp_sensor1_a", 24 * 1000000);
		if (ret) {
			pr_err("%s, fimc_is_set_rate_dt A:%d\n", __func__, ret);
			return -EPERM;
		}
		ret = fimc_is_set_rate_dt(pn547_dev->pdev, "dout_sclk_isp_sensor1_b", 24 * 1000000);
		if (ret) {
			pr_err("%s, fimc_is_set_rate_dt B:%d\n", __func__, ret);
			return -EPERM;
		}
		frequency = fimc_is_get_rate_dt(pn547_dev->pdev, "sclk_isp_sensor1");
		pr_info("%s(mclk : %d)\n", __func__, frequency);

		pn547_dev->clk = clk_get(&pn547_dev->client->dev, "sclk_isp_sensor1");
		if (IS_ERR(pn547_dev->clk)) {
			pr_err("%s : clk not found\n", __func__);
			return -EPERM;
		}
	}
#endif
	return 0;
}

static void pn547_clk_uninitialize(struct pn547_dev *pn547_dev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	clk_put(pn547_dev->clk);
#endif
}

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
unsigned int pn547_prepare_clk(struct pn547_dev *pn547_dev, bool enable)
{
	if (enable) {
		return clk_prepare(pn547_dev->clk);
	} else {
		clk_unprepare(pn547_dev->clk);
		return 0;
	}
}

unsigned int pn547_enable_clk(struct pn547_dev *pn547_dev, bool enable)
{
	if (enable) {
		return clk_enable(pn547_dev->clk);
	} else {
		clk_disable(pn547_dev->clk);
		return 0;
	}
}
#endif

static int pn547_clk_prepare(struct pn547_dev *pn547_dev, bool enable)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	if (pn547_dev->clk_use_check) {
		if (enable) {
			int ret;
			ret = clk_prepare(pn547_dev->clk);
			if (ret)
				pr_err("%s, prepare clk Error !!:%d\n", __func__, ret);
		} else {
			clk_unprepare(pn547_dev->clk);
		}
	}
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	if (pn547_dev->clk_use_check == CLK_USE_CAM1) {
		if (enable) {
			int ret;
			ret = pn547_prepare_clk(pn547_dev, true);
			if (ret)
				pr_err("%s, prepare sclk_isp_sensor1 Error !!:%d\n", __func__, ret);
		} else {
			pn547_prepare_clk(pn547_dev, false);
		}
	}
#endif

	return 0;
}

static int pn547_clk_requset(struct pn547_dev *pn547_dev, bool enable)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	if (pn547_dev->clk_use_check) {
		if (enable) {
			int ret;
			ret = clk_enable(pn547_dev->clk);
			if (ret)
				pr_err("%s, enable clk Error !!:%d\n", __func__, ret);
		} else {
			clk_disable(pn547_dev->clk);
		}
	}
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	if (pn547_dev->clk_use_check == CLK_USE_CAM1) {
		if (enable) {
			int ret;
			ret = pn547_enable_clk(pn547_dev, true);
			if (ret)
				pr_err("%s, enable sclk_isp_sensor1 Error !!:%d\n", __func__, ret);
		} else {
			pn547_enable_clk(pn547_dev, false);
		}
	} else if (pn547_dev->clk_use_check == CLK_USE_AUDIO) {
		exynos5_audio_set_mclk(enable, false);
	}
#endif

	return 0;
}

static irqreturn_t pn547_dev_clk_req_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;

	/* setting clk */
	if (gpio_get_value(pn547_dev->clk_req_gpio)) {
		if (!atomic_read(&pn547_dev->clk_enabled)) {
			int ret;
			ret = pn547_clk_requset(pn547_dev, true);
			if (ret) {
				pr_err("%s, enable clk Error !!:%d\n", __func__, ret);
				return ret;
			}
			atomic_set(&pn547_dev->clk_enabled, 1);
		}
	} else {
		if (atomic_read(&pn547_dev->clk_enabled)) {
			pn547_clk_requset(pn547_dev, false);
			atomic_set(&pn547_dev->clk_enabled, 0);
		}
	}

	return IRQ_HANDLED;
}
#endif

static ssize_t pn547_dev_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret = 0;

	if (count > MAX_BUFFER_SIZE) {
		count = MAX_BUFFER_SIZE;
	} else if (count == 0) {
		dev_info(&pn547_dev->client->dev, "pn547 : read 0bytes\n");
		return ret;
	}
#if NFC_DEBUG
	dev_info(&pn547_dev->client->dev, "%s : reading %zu bytes. irq=%s\n",
		__func__, count,
		gpio_get_value(pn547_dev->irq_gpio) ? "1" : "0");
	dev_info(&pn547_dev->client->dev, "pn547 : + r\n");
#endif

	mutex_lock(&pn547_dev->read_mutex);

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
#ifdef NXP_KR_READ_IRQ_MODIFY
		do_reading = false;
#endif
		if (filp->f_flags & O_NONBLOCK) {
			dev_info(&pn547_dev->client->dev, "%s : O_NONBLOCK\n",
				 __func__);
			ret = -EAGAIN;
			goto fail;
		}
#if NFC_DEBUG
		dev_info(&pn547_dev->client->dev,
			"wait_event_interruptible : in\n");
#endif

#ifdef NXP_KR_READ_IRQ_MODIFY
		ret = wait_event_interruptible(pn547_dev->read_wq,
			do_reading);
#else
		ret = wait_event_interruptible(pn547_dev->read_wq,
			gpio_get_value(pn547_dev->irq_gpio));
#endif

#if NFC_DEBUG
		dev_info(&pn547_dev->client->dev,
			"wait_event_interruptible : out\n");
#endif

#ifdef NXP_KR_READ_IRQ_MODIFY
		if (cancle_read == true) {
			cancle_read = false;
			ret = -1;
			goto fail;
		}
#endif

		if (ret)
			goto fail;
	}

	/* Read data */
	ret = i2c_master_recv(pn547_dev->client, tmp, count);

	mutex_unlock(&pn547_dev->read_mutex);

	if (ret < 0) {
		dev_err(&pn547_dev->client->dev,
			"%s: i2c_master_recv returned %d\n",
				__func__, ret);
		return ret;
	}
	if (ret > count) {
		dev_err(&pn547_dev->client->dev,
			"%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		dev_err(&pn547_dev->client->dev,
			"%s : failed to copy to user space\n",
			__func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn547_dev->read_mutex);
	return ret;
}

static ssize_t pn547_dev_write(struct file *filp, const char __user *buf,
			       size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret = 0, retry = 2;
#if NFC_DEBUG
	int i = 0;
#endif

	pn547_dev = filp->private_data;

#if NFC_DEBUG
	dev_info(&pn547_dev->client->dev, "pn547 : + w\n");
	for (i = 0; i < count; i++)
		dev_info(&pn547_dev->client->dev, "buf[%d] = 0x%x\n",
			i, buf[i]);
#endif

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		dev_err(&pn547_dev->client->dev,
			"%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}
#if NFC_DEBUG
	dev_info(&pn547_dev->client->dev, "%s : writing %zu bytes.\n", __func__,
		count);
#endif
	/* Write data */
	do {
		retry--;
		ret = i2c_master_send(pn547_dev->client, tmp, count);
		if (ret == count)
			break;
		usleep_range(6000, 10000); /* Retry, chip was in standby */
#if NFC_DEBUG
		dev_info(&pn547_dev->client->dev, "retry = %d\n", retry);
#endif
	} while (retry);

#if NFC_DEBUG
	dev_info(&pn547_dev->client->dev, "pn547 : - w\n");
#endif

	if (ret != count) {
		dev_err(&pn547_dev->client->dev,
			"%s : i2c_master_send returned %d, %d\n",
			__func__, ret, retry);
		ret = -EIO;
	}

	return ret;
}

static int pn547_dev_open(struct inode *inode, struct file *filp)
{
	struct pn547_dev *pn547_dev = container_of(filp->private_data,
						   struct pn547_dev,
						   pn547_device);

	filp->private_data = pn547_dev;

	dev_info(&pn547_dev->client->dev, "%s : %d,%d\n", __func__,
		imajor(inode), iminor(inode));

	return 0;
}

static long pn547_dev_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct pn547_dev *pn547_dev = filp->private_data;

	switch (cmd) {
	case PN547_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			gpio_set_value(pn547_dev->ven_gpio, 1);
			gpio_set_value(pn547_dev->firm_gpio, 1);
			usleep_range(4900, 5000);
			gpio_set_value(pn547_dev->ven_gpio, 0);
			usleep_range(4900, 5000);
			gpio_set_value(pn547_dev->ven_gpio, 1);
			usleep_range(4900, 5000);
			if (atomic_read(&pn547_dev->irq_enabled) == 0) {
				atomic_set(&pn547_dev->irq_enabled, 1);
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
				pn547_clk_prepare(pn547_dev, true);
				enable_irq(pn547_dev->clk_req_irq);
#endif
				enable_irq(pn547_dev->client->irq);
				enable_irq_wake(pn547_dev->client->irq);
			}
			dev_info(&pn547_dev->client->dev,
				 "%s power on with firmware, irq=%d\n",
				 __func__,
				 atomic_read(&pn547_dev->irq_enabled));
		} else if (arg == 1) {
			/* power on */
			gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value(pn547_dev->ven_gpio, 1);
			usleep_range(4900, 5000);
			if (atomic_read(&pn547_dev->irq_enabled) == 0) {
				atomic_set(&pn547_dev->irq_enabled, 1);
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
				pn547_clk_prepare(pn547_dev, true);
				enable_irq(pn547_dev->clk_req_irq);
#endif
				enable_irq(pn547_dev->client->irq);
				enable_irq_wake(pn547_dev->client->irq);
			}
#if defined(CONFIG_SOC_EXYNOS5433) //////////////////////////
{
	void __iomem *reg2;
	unsigned int val2;
	reg2 = ioremap(0x10030B00, SZ_4);
	if(reg2 == NULL)
		pr_info("%s : before reg2 = NULL\n", __func__);
	else
	{
		val2 = readl(reg2);
		pr_info("%s : before reg2-1 = 0x%08X\n", __func__, val2);
		val2 = val2 | 0x00000040;
		writel(val2,reg2);
		pr_info("%s : before reg2-2 = 0x%08X\n", __func__, readl(reg2));
	}
}
#endif////////////////////////
			dev_info(&pn547_dev->client->dev,
				"%s power on, irq=%d\n", __func__,
				atomic_read(&pn547_dev->irq_enabled));
		} else if (arg == 0) {
			/* power off */
			if (atomic_read(&pn547_dev->irq_enabled) == 1) {
				disable_irq_wake(pn547_dev->client->irq);
				disable_irq_nosync(pn547_dev->client->irq);
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
				disable_irq_nosync(pn547_dev->clk_req_irq);
				if (atomic_read(&pn547_dev->clk_enabled)) {
					pn547_clk_requset(pn547_dev, false);
					atomic_set(&pn547_dev->clk_enabled, 0);
				}
				pn547_clk_prepare(pn547_dev, false);
#endif
				atomic_set(&pn547_dev->irq_enabled, 0);
			}
			dev_info(&pn547_dev->client->dev, "%s power off, irq=%d\n",
				 __func__,
				 atomic_read(&pn547_dev->irq_enabled));
			gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value(pn547_dev->ven_gpio, 0);
			usleep_range(4900, 5000);
#ifdef NXP_KR_READ_IRQ_MODIFY
		} else if (arg == 3) {
			pr_info("%s Read Cancle\n", __func__);
			cancle_read = true;
			do_reading = true;
			wake_up(&pn547_dev->read_wq);
#endif
		} else {
			dev_err(&pn547_dev->client->dev, "%s bad arg %lu\n",
				__func__, arg);
			return -EINVAL;
		}
		break;
	default:
		dev_err(&pn547_dev->client->dev, "%s bad ioctl %u\n", __func__,
			cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn547_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn547_dev_read,
	.write	= pn547_dev_write,
	.open	= pn547_dev_open,
	.unlocked_ioctl  = pn547_dev_ioctl,
};

#ifdef CONFIG_OF
static int pn547_parse_dt(struct device *dev,
	struct pn547_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	int ret;
#endif
	pdata->irq_gpio = of_get_named_gpio(np, "pn547-irq",0);
	pdata->ven_gpio = of_get_named_gpio(np, "pn547-en",0);
	pdata->firm_gpio = of_get_named_gpio(np, "pn547-firmware",0);

#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	ret = of_property_read_u32(np, "pn547-clk-use", &pdata->clk_use_check);
	if (ret) {
		pr_err("%s get clk check error (%d)\n", __func__, ret);
		return ret;
	}

	if (pdata->clk_use_check)
		pdata->clk_req_gpio = of_get_named_gpio(np, "pn547-clk-req",0);
	else
		pdata->clk_req_gpio = -1;

	pr_info("%s: irq : %d, ven : %d, firm : %d, clk_req : %d\n",
		__func__, pdata->irq_gpio, pdata->ven_gpio, pdata->firm_gpio,
	        pdata->clk_req_gpio);
#else
	pr_info("%s: irq : %d, ven : %d, firm :%d\n",
		__func__, pdata->irq_gpio, pdata->ven_gpio, pdata->firm_gpio);
#endif

	return 0;
}
#else
static int pn547_parse_dt(struct device *dev,
	struct pn547_i2c_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int pn547_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	int ret,err;
	struct pn547_i2c_platform_data *platform_data;
	struct pn547_dev *pn547_dev;
#if defined (MULITPLE_ADDRESS_PN65T)
	int addr;
	char tmp[4] = {0x20, 0x00, 0x01, 0x01};
	int addrcnt;
#endif

	pr_info("%s started...\n", __func__);

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(struct pn547_i2c_platform_data), GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		err = pn547_parse_dt(&client->dev, platform_data);
		if (err)
			return err;
	} else {
		platform_data = client->dev.platform_data;
	}

	if (platform_data == NULL) {
		dev_err(&client->dev, "%s : nfc probe fail\n", __func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s : need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	ret = gpio_request(platform_data->irq_gpio, "nfc_int");
	if (ret)
		return -ENODEV;
	ret = gpio_request(platform_data->ven_gpio, "nfc_ven");
	if (ret)
		goto err_ven;
	ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
	if (ret)
		goto err_firm;
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	if (platform_data->clk_use_check) {
		ret = gpio_request(platform_data->clk_req_gpio, "nfc_clk_req");
		if (ret)
			goto err_clk_req;
	}
#endif
	pn547_dev = kzalloc(sizeof(*pn547_dev), GFP_KERNEL);
	if (pn547_dev == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	pn547_dev->pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (pn547_dev->pdev == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit2;
	} else {
		pn547_dev->pdev->dev = client->dev;
	}
#endif
#endif

	pn547_dev->irq_gpio = platform_data->irq_gpio;
	pn547_dev->ven_gpio = platform_data->ven_gpio;
	pn547_dev->firm_gpio = platform_data->firm_gpio;
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	pn547_dev->clk_use_check = platform_data->clk_use_check;
	pn547_dev->clk_req_gpio = platform_data->clk_req_gpio;
#endif
	pn547_dev->client = client;

	/* init mutex and queues */
	init_waitqueue_head(&pn547_dev->read_wq);
	mutex_init(&pn547_dev->read_mutex);

	pn547_dev->pn547_device.minor = MISC_DYNAMIC_MINOR;
	pn547_dev->pn547_device.name = "pn547";
	pn547_dev->pn547_device.fops = &pn547_dev_fops;

	ret = misc_register(&pn547_dev->pn547_device);
	if (ret) {
		dev_err(&client->dev, "%s : misc_register failed. ret = %d\n",
			__FILE__, ret);
		goto err_misc_register;
	}

	i2c_set_clientdata(client, pn547_dev);

	wake_lock_init(&pn547_dev->nfc_wake_lock,
		WAKE_LOCK_SUSPEND, "nfc_wake_lock");

	gpio_direction_output(pn547_dev->ven_gpio, 0);
	gpio_direction_output(pn547_dev->firm_gpio, 0);
	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	ret = gpio_direction_input(pn547_dev->irq_gpio);
	if (ret) {
		dev_err(&client->dev, "%s : gpio_direction_input failed. ret = %d\n",
			__FILE__, ret);
		goto err_request_irq_failed;
	}

	client->irq = gpio_to_irq(pn547_dev->irq_gpio);
	dev_info(&pn547_dev->client->dev, "%s : requesting IRQ %d\n", __func__,
		 client->irq);

	ret = request_irq(client->irq, pn547_dev_irq_handler,
			  IRQF_TRIGGER_RISING | IRQF_ONESHOT, "pn547", pn547_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed. ret = %d\n", ret);
		goto err_request_irq_failed;
	}
	disable_irq_nosync(pn547_dev->client->irq);

#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	if (pn547_dev->clk_use_check) {
		ret = pn547_clk_initialize(pn547_dev);
		if (ret) {
			dev_err(&client->dev, "%s : pn547_clk_initialize failed %d\n",
				__FILE__, ret);
			goto err_clk_get;
		}

		ret = gpio_direction_input(pn547_dev->clk_req_gpio);
		if (ret) {
			dev_err(&client->dev, "%s : gpio_direction_input failed. ret = %d\n",
				__FILE__, ret);
			goto err_request_clk_req_failed;
		}

		pn547_dev->clk_req_irq = gpio_to_irq(pn547_dev->clk_req_gpio);
		dev_info(&pn547_dev->client->dev, "%s : requesting clk_req irq %d\n", __func__,
			 pn547_dev->clk_req_irq);

		ret = request_irq(pn547_dev->clk_req_irq, pn547_dev_clk_req_irq_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |IRQF_ONESHOT,
				  "pn547_clk_req", pn547_dev);
		if (ret) {
			dev_err(&client->dev, "clk_req request_irq failed. ret = %d\n", ret);
			goto err_request_clk_req_failed;
		}
		disable_irq_nosync(pn547_dev->clk_req_irq);
	}
#endif
	atomic_set(&pn547_dev->irq_enabled, 0);

#if defined (MULITPLE_ADDRESS_PN65T)
	gpio_set_value(pn547_dev->ven_gpio, 1);
	gpio_set_value(pn547_dev->firm_gpio, 1); /* add firmware pin */
	usleep_range(4900, 5000);
	gpio_set_value(pn547_dev->ven_gpio, 0);
	usleep_range(4900, 5000);
	gpio_set_value(pn547_dev->ven_gpio, 1);
	usleep_range(4900, 5000);

	for (addr = 0x2B; addr > 0x27; addr--) {
		client->addr = addr;
		addrcnt = 2;

		do {
			ret = i2c_master_send(client, tmp, 4);
			if (ret > 0) {
				pr_info("%s : i2c addr=0x%X\n",
					__func__, client->addr);
				break;
			}
		} while (addrcnt--);

		if (ret > 0)
			break;
	}

	if (ret <= 0)
		client->addr = 0x2B;

	gpio_set_value(pn547_dev->ven_gpio, 0);
	gpio_set_value(pn547_dev->firm_gpio, 0);

	if (ret < 0) {
		pr_err("%s : fail to get i2c addr\n", __func__);
		goto err_search_requset_failed;
	}
#endif
	pr_info("%s finished...\n",__func__);
	return 0;

#if defined (MULITPLE_ADDRESS_PN65T)
err_search_requset_failed:
#endif
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
err_request_clk_req_failed:
	if (pn547_dev->clk_use_check)
		pn547_clk_uninitialize(pn547_dev);
err_clk_get:
	free_irq(client->irq, pn547_dev);
#endif
err_request_irq_failed:
	wake_lock_destroy(&pn547_dev->nfc_wake_lock);
	misc_deregister(&pn547_dev->pn547_device);
err_misc_register:
	mutex_destroy(&pn547_dev->read_mutex);
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	kfree(pn547_dev->pdev);
err_exit2:
#endif
#endif
	kfree(pn547_dev);
err_exit:
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	if (platform_data->clk_use_check)
		gpio_free(platform_data->clk_req_gpio);
err_clk_req:
#endif
	gpio_free(platform_data->firm_gpio);
err_firm:
	gpio_free(platform_data->ven_gpio);
err_ven:
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int pn547_remove(struct i2c_client *client)
{
	struct pn547_dev *pn547_dev;

	pn547_dev = i2c_get_clientdata(client);
	wake_lock_destroy(&pn547_dev->nfc_wake_lock);
#ifdef CONFIG_NFC_PN547_CLOCK_REQUEST
	if (pn547_dev->clk_use_check) {
		free_irq(pn547_dev->clk_req_irq, pn547_dev);
		pn547_clk_uninitialize(pn547_dev);
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
		kfree(pn547_dev->pdev);
#endif
		gpio_free(pn547_dev->clk_req_gpio);
	}
#endif
	free_irq(client->irq, pn547_dev);
	misc_deregister(&pn547_dev->pn547_device);
	mutex_destroy(&pn547_dev->read_mutex);
	gpio_free(pn547_dev->irq_gpio);
	gpio_free(pn547_dev->ven_gpio);
	gpio_free(pn547_dev->firm_gpio);
	kfree(pn547_dev);

	return 0;
}

static const struct i2c_device_id pn547_id[] = {
	{ "pn547", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, pn547_id);
#ifdef CONFIG_OF
static struct of_device_id nfc_match_table[] = {
	{ .compatible = "pn547",},
	{},
};
#else
#define nfc_match_table NULL
#endif

static struct i2c_driver pn547_driver = {
	.id_table	= pn547_id,
	.probe		= pn547_probe,
	.remove		= pn547_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn547",
		.of_match_table = nfc_match_table,
	},
};

/*
 * module load/unload record keeping
 */

static int __init pn547_dev_init(void)
{
	pr_info("%s, Loading pn547 driver\n", __func__);
	return i2c_add_driver(&pn547_driver);
}
module_init(pn547_dev_init);

static void __exit pn547_dev_exit(void)
{
	pr_info("%s, Unloading pn547 driver\n", __func__);
	i2c_del_driver(&pn547_driver);
}
module_exit(pn547_dev_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("NFC pn547 driver");
MODULE_LICENSE("GPL");
