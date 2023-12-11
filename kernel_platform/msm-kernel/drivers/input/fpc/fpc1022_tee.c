/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks, controlling GPIOs such as SPI chip select, sensor reset line, sensor
 * IRQ line, MISO and MOSI lines.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node. This makes it possible for a user
 * space process to poll the input node and receive IRQ events easily. Usually
 * this node is available under /dev/input/eventX where 'X' is a number given by
 * the event system. A user space process will need to traverse all the event
 * nodes and ask for its parent's name (through EVIOCGNAME) which should match
 * the value in device tree named input-device-name.
 *
 * This driver will NOT send any SPI commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

//Reference driver for being multi-vendor compatible

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/device.h>

#ifdef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif
#include <linux/proc_fs.h>
#ifdef CONFIG_SPI_MT65XX
#include "mtk_gpio.h"
#include "mach/gpio_const.h"
#endif

#ifdef CONFIG_HQ_SYSFS_SUPPORT
#include <linux/hqsysfs.h>
#endif
#include  <linux/regulator/consumer.h>

#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000

#define FPC_IRQ_DEV_NAME "fpc_irq"
#define FPC_TTW_HOLD_TIME 2000

#define FPC102X_REG_HWID 252

#define FPC_CHIP 0x1000
#define FPC_CHIP_MASK_SENSOR_TYPE 0xff00

#define GPIO_GET(pin) __gpio_get_value(pin)	//get input pin value

#define GPIOIRQ 2
#define SAMSUNG_FP_SUPPORT   1
#define PROC_NAME  "fp_info"
static struct proc_dir_entry *proc_entry;

struct regulator *regu_buck;

#ifdef SAMSUNG_FP_SUPPORT
#define FINGERPRINT_ADM_CLASS_NAME "fingerprint"
#endif


#ifdef CONFIG_SPI_MT65XX
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
#endif

//#define FPC_DRIVER_FOR_ISEE

#ifdef FPC_DRIVER_FOR_ISEE
#include "teei_fp.h"
#include "tee_client_api.h"
#endif

static int fpc_fp_adm = 0;
static int fpc_adm_release =0;
#ifdef FPC_DRIVER_FOR_ISEE
struct TEEC_UUID uuid_ta_fpc = { 0x7778c03f, 0xc30c, 0x4dd0,
	{0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b}
};
#endif

#ifdef SAMSUNG_FP_SUPPORT
static ssize_t adm_show(struct device *dev,struct device_attribute *attr,char *buf)
{
  return snprintf(buf,3,"1\n");
}

static DEVICE_ATTR(adm, 0664, adm_show, NULL);
struct class *fingerprint_adm_class;
struct device *fingerprint_adm_dev;
#endif

struct fpc1022_data {
	struct device *dev;
	struct platform_device *pldev;
	struct spi_device *spi;
	int irq_gpio;
	int rst_gpio;
	int pwr_gpio;
	int irq_num;

	struct pinctrl *pinctrl;
	struct pinctrl_state *st_irq;
	struct pinctrl_state *st_rst_l;
	struct pinctrl_state *st_rst_h;

	struct input_dev *idev;
	char idev_name[32];
	int event_type;
	int event_code;
	struct mutex lock;
	bool prepared;
	bool wakeup_enabled;
	struct wakeup_source ttw_wl;
};

static irqreturn_t fpc1022_irq_handler(int irq, void *handle);
static struct fpc1022_data *fpc1022;

#ifdef CONFIG_SPI_MT65XX
static const struct mt_chip_conf spi_mcc = {
	.setuptime = 20,
	.holdtime = 20,
	.high_time = 50,	/* 1MHz */
	.low_time = 50,
	.cs_idletime = 5,
	.ulthgh_thrsh = 0,

	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,

	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,

	.tx_endian = SPI_LENDIAN,
	.rx_endian = SPI_LENDIAN,

	.com_mod = FIFO_TRANSFER,

	.pause = 1,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#ifdef CONFIG_SPI_MT65XX
u32 spi_speed = 1 * 1000000;
#endif

static int proc_show_ver(struct seq_file *file,void *v)
{
	seq_printf(file,"[Fingerprint]:fpc_fp  [IC]:fpc1560\n");
	return 0;
}

static int proc_open(struct inode *inode,struct file *file)
{
	printk("fpc proc_open\n");
	single_open(file,proc_show_ver,NULL);
	return 0;
}

static const struct proc_ops proc_file_fpc_ops = {
	.proc_open = proc_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};


static int fpc1022_get_irqNum(struct fpc1022_data *fpc1022)
{
	int rc = 0;
	struct device_node *node;
	struct device *dev = fpc1022->dev;
	int irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	node = of_find_compatible_node(NULL, NULL, "fpc,fingerprint");

	if (node && fpc1022->irq_gpio == 0) {
		fpc1022->irq_num = irq_of_parse_and_map(node, 0);
		fpc1022->irq_gpio = of_get_named_gpio(node, "fp-gpio-irq", 0);
		rc = gpio_request(fpc1022->irq_gpio, "fp-gpio-irq");
		if (rc) {
			dev_err(dev, "Failed to request fp-gpio-irq GPIO. rc = %d\n", rc);
			return rc;
		}

		fpc1022->pwr_gpio = of_get_named_gpio(node, "fp-gpio-pwr", 0);
		rc = gpio_request(fpc1022->pwr_gpio, "fp-gpio-pwr");
		if (rc) {
			dev_err(dev, "Failed to request fp-gpio-pwr GPIO. rc = %d\n", rc);
			return rc;
		}
		gpio_set_value(fpc1022->pwr_gpio, 0);
		usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);
		gpio_set_value(fpc1022->pwr_gpio, 1);
		usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);

		if (!fpc1022->irq_num) {
			dev_err(dev, "irq_of_parse_and_map fail!!\n");
			return -1;
		}

		rc = devm_request_threaded_irq(dev, fpc1022->irq_num,
						NULL, fpc1022_irq_handler, irqf,
						dev_name(dev), dev);

		if (rc) {
			dev_err(dev, "could not request irq %d\n", fpc1022->irq_num);
			return -1;
		}

		/* Request that the interrupt should be wakeable */
		enable_irq_wake(fpc1022->irq_num);
		//fpc1022->ttw_wl = wakeup_source_register("fpc_ttw_wl");
	}
	dev_info(dev, "%s , fpc1022->irq_num = %d, fpc1022->irq_gpio = %d\n", __func__, fpc1022->irq_num, fpc1022->irq_gpio);
	return 0;
}

static int hw_reset(struct fpc1022_data *fpc1022)
{
	int rc = 0;
	struct device_node *node = of_find_compatible_node(NULL, NULL, "fpc,fingerprint");
	struct device *dev = fpc1022->dev;
	if (node && fpc1022->rst_gpio == 0) {
		fpc1022->rst_gpio = of_get_named_gpio(node, "fp-gpio-reset", 0);
		rc = gpio_request(fpc1022->rst_gpio, "fp-gpio-reset");
		if (rc) {
			dev_err(fpc1022->dev, "Failed to request fp-gpio-reset GPIO. rc = %d\n", rc);
			return -1;
		}
	}
	gpio_set_value(fpc1022->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);
	gpio_set_value(fpc1022->rst_gpio, 0);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);
	gpio_set_value(fpc1022->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);
	dev_info(dev, "IRQ after reset %d, irq num:%d irq gpio:%d\n", GPIO_GET(fpc1022->irq_gpio), fpc1022->irq_num, fpc1022->irq_gpio);
	/*struct device *dev = fpc1022->dev;

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_h);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_l);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	pinctrl_select_state(fpc1022->pinctrl, fpc1022->st_rst_h);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	dev_info(dev, "IRQ after reset %d\n", GPIO_GET(fpc1022->irq_gpio));*/
	

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
                            struct device_attribute *attr, const char *buf,
                            size_t count)
{
        int ret = 0;
        struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

        if (!strncmp(buf, "init", strlen("init"))) {
                if (NULL == proc_entry) proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_file_fpc_ops);
                if (NULL == proc_entry) {
                        printk("fpc_fp Couldn't create proc entry!");
                        return -ENOMEM;
                } else {
                        printk("fpc_fp Create proc entry success!");
                }
		#ifdef SAMSUNG_FP_SUPPORT
                if (fpc_fp_adm)
                {
                  fpc_fp_adm = 0;
	          fingerprint_adm_class = class_create(THIS_MODULE,FINGERPRINT_ADM_CLASS_NAME);
	          if (IS_ERR(fingerprint_adm_class))
	          {
		   printk( "fingerprint_adm_class error \n");
		   return PTR_ERR(fingerprint_adm_class);
	          }

	           fingerprint_adm_dev = device_create(fingerprint_adm_class, NULL, 0,
		                                   NULL, FINGERPRINT_ADM_CLASS_NAME);

	           if (sysfs_create_file(&(fingerprint_adm_dev->kobj), &dev_attr_adm.attr))
	           {
		    printk(" fpc fail to creat fingerprint_adm_device \n");
	            }else{
                     printk(" fpc success to creat fingerprint_adm_device \n");
                   }
                   fpc_adm_release =1;
                 }
		#endif

                ret = fpc1022_get_irqNum(fpc1022);
                if (ret) {
                        return -1;
                }
                ret = hw_reset(fpc1022);
       } else if (!strncmp(buf, "free", strlen("free"))) {
                if (NULL == proc_entry) {
                        printk("fpc fp_info is null!");
                } else {
                        remove_proc_entry(PROC_NAME,NULL);
                        printk("fpc fp_info remove!");
			#ifdef SAMSUNG_FP_SUPPORT
			if(fpc_adm_release)
			{
			device_destroy(fingerprint_adm_class,0);
			class_destroy(fingerprint_adm_class);
                        fpc_adm_release = 0;
			printk("fpc device_remove_file remove! \n");
			}
			#endif

		        disable_irq_nosync(fpc1022->irq_num);
		        devm_free_irq(dev, fpc1022->irq_num, dev);
		        gpio_set_value(fpc1022->rst_gpio, 0);
		        gpio_set_value(fpc1022->pwr_gpio, 0);
		        gpio_free(fpc1022->irq_gpio);
		        gpio_free(fpc1022->pwr_gpio);
		        gpio_free(fpc1022->rst_gpio);
		        fpc1022->rst_gpio = 0;
		        fpc1022->irq_gpio = 0;
		        proc_entry = NULL;
                }
        } else if (!strncmp(buf, "reset", strlen("reset"))) {
                 ret = hw_reset(fpc1022);
        } else {
                return -EINVAL;
        }
        return ret ? ret : count;
}

static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
* sysfs node for controlling whether the driver is allowed
* to wake up the platform on interrupt.
*/
// static ssize_t wakeup_enable_set(struct device *dev,
// 				 struct device_attribute *attr, const char *buf,
// 				 size_t count)
// {
// 	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

// 	if (!strncmp(buf, "enable", strlen("enable"))) {
// 		fpc1022->wakeup_enabled = true;
// 		smp_wmb();
// 	} else if (!strncmp(buf, "disable", strlen("disable"))) {
// 		fpc1022->wakeup_enabled = false;
// 		smp_wmb();
// 	} else
// 		return -EINVAL;

// 	return count;
// }

// static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
* sysfs node for sending event to make the system interactive,
* i.e. waking up
*/
static ssize_t do_wakeup_set(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	if (count > 0) {
		/* Sending power key event creates a toggling
		   effect that may be desired. It could be
		   replaced by another event such as KEY_WAKEUP. */
		input_report_key(fpc1022->idev, KEY_POWER, 1);
		input_report_key(fpc1022->idev, KEY_POWER, 0);
		input_sync(fpc1022->idev);
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(do_wakeup, S_IWUSR, NULL, do_wakeup_set);

#ifdef CONFIG_SPI_MT65XX
static ssize_t clk_enable_set(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);
	dev_dbg(fpc1022->dev, "why should we set clocks here? i refuse,%s\n",
		__func__);
	dev_dbg(fpc1022->dev, " buff is %d, %s\n", *buf, __func__);

	if (fpc1022->spi) {
		//update spi clk
		if (*buf == 49)
			mt_spi_enable_master_clk(fpc1022->spi);
		dev_dbg(fpc1022->dev, " enable spi clk %s\n", __func__);

		if (*buf == 48)
			mt_spi_disable_master_clk(fpc1022->spi);
		dev_dbg(fpc1022->dev, " disable spi clk %s\n", __func__);

		dev_dbg(fpc1022->dev, " spi clk end success%s\n", __func__);
		return 1;	//set_clks(fpc1022, (*buf == '1')) ? : count;
	} else {
		dev_err(fpc1022->dev, " spi clk NULL%s\n", __func__);
		return 0;
	}

}

static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);
#endif
/**
* sysf node to check the interrupt status of the sensor, the interrupt
* handler should perform sysf_notify to allow userland to poll the node.
*/
static ssize_t irq_get(struct device *device,
		       struct device_attribute *attribute, char *buffer)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(device);
	int irq = __gpio_get_value(fpc1022->irq_gpio);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
* writing to the irq node will just drop a printk message
* and return success, used for latency measurement.
*/
static ssize_t irq_ack(struct device *device,
		       struct device_attribute *attribute,
		       const char *buffer, size_t count)
{
	struct fpc1022_data *fpc1022 = dev_get_drvdata(device);
	dev_dbg(fpc1022->dev, "%s\n", __func__);
	return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static struct attribute *attributes[] = {
	&dev_attr_hw_reset.attr,
	// &dev_attr_wakeup_enable.attr,
	&dev_attr_do_wakeup.attr,
#ifdef CONFIG_SPI_MT65XX
	&dev_attr_clk_enable.attr,
	&dev_attr_fpid_get.attr,
#endif
	&dev_attr_irq.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

void irq_notify(struct work_struct *work)
{
	(void)work;
	__pm_wakeup_event(&(fpc1022->ttw_wl), FPC_TTW_HOLD_TIME);
	sysfs_notify(&fpc1022->dev->kobj, NULL, dev_attr_irq.attr.name);
}

struct work_struct irq_work;

static irqreturn_t fpc1022_irq_handler(int irq, void *handle)
{
	//struct fpc1022_data *fpc1022 = handle;
	(void)irq;
	(void)handle;
	/* Make sure 'wakeup_enabled' is updated before using it
	 ** since this is interrupt context (other thread...) */
	// printk("fpc1022_irq_handler");
	// smp_rmb();

	/* if (fpc1022->wakeup_enabled) { */
	// __pm_wakeup_event(fpc1022->ttw_wl, FPC_TTW_HOLD_TIME);
	/* } */

	// sysfs_notify(&fpc1022->dev->kobj, NULL, dev_attr_irq.attr.name);
	schedule_work(&irq_work);

	return IRQ_HANDLED;
}

static int fpc1022_platform_probe(struct platform_device *pldev)
{
	int ret = 0;
	u32 val = 0;
	const char *idev_name;
	struct device *dev = &pldev->dev;
	struct device_node *np = dev->of_node;

	dev_info(dev, "%s\n", __func__);

	if (!np) {
		dev_err(dev, "no of node found\n");
		ret = -EINVAL;
		goto err_no_of_node;
	}

	fpc1022 = devm_kzalloc(dev, sizeof(*fpc1022), GFP_KERNEL);
	if (!fpc1022) {
		dev_err(dev,
			"failed to allocate memory for struct fpc1022_data\n");
		ret = -ENOMEM;
		goto err_fpc1022_malloc;
	}
	fpc1022->rst_gpio = 0;
	fpc1022->irq_gpio = 0;
        fpc_fp_adm = 1;
	fpc1022->dev = dev;
	dev_set_drvdata(dev, fpc1022);
	fpc1022->pldev = pldev;

	ret = of_property_read_u32(np, "fpc,event-type", &val);
	fpc1022->event_type = ret < 0 ? EV_MSC : val;

	ret = of_property_read_u32(np, "fpc,event-code", &val);
	fpc1022->event_code = ret < 0 ? MSC_SCAN : val;

	fpc1022->idev = devm_input_allocate_device(dev);
	if (!fpc1022->idev) {
		dev_err(dev, "failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_malloc;
	}

	input_set_capability(fpc1022->idev, fpc1022->event_type,
			     fpc1022->event_code);

	if (!of_property_read_string(np, "input-device-name", &idev_name)) {
		fpc1022->idev->name = idev_name;
	} else {
		snprintf(fpc1022->idev_name, sizeof(fpc1022->idev_name),
			 "fpc_irq@%s", dev_name(dev));
		fpc1022->idev->name = fpc1022->idev_name;
	}

	/* Also register the key for wake up */
	set_bit(EV_KEY, fpc1022->idev->evbit);
	set_bit(EV_PWR, fpc1022->idev->evbit);
	set_bit(KEY_WAKEUP, fpc1022->idev->keybit);
	set_bit(KEY_POWER, fpc1022->idev->keybit);
	ret = input_register_device(fpc1022->idev);
	// fpc1022->wakeup_enabled = false;

	ret = sysfs_create_group(&dev->kobj, &attribute_group);
	if (ret) {
		dev_err(dev, "could not create sysfs\n");
		return -1;
	}
	INIT_WORK(&irq_work, irq_notify);
	wakeup_source_add(&(fpc1022->ttw_wl));
	dev_info(dev, "%s: ok\n", __func__);
	return ret;

err_input_malloc:
	devm_kfree(dev, fpc1022);

err_fpc1022_malloc:
err_no_of_node:

	return ret;
}

static int fpc1022_platform_remove(struct platform_device *pldev)
{
	struct device *dev = &pldev->dev;
	struct fpc1022_data *fpc1022 = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);

	sysfs_remove_group(&dev->kobj, &attribute_group);
	if (NULL == proc_entry) {
		printk("fpc fp_info is null!");
	} else {
		remove_proc_entry(PROC_NAME,NULL);
		printk("fpc fp_info remove!");
	}
        #ifdef SAMSUNG_FP_SUPPORT
        if(fpc_adm_release)
        {
        device_destroy(fingerprint_adm_class,0);
        class_destroy(fingerprint_adm_class);
        fpc_adm_release = 0;
        printk("fpc device_remove_file remove! \n");
        }
	#endif

	wakeup_source_remove(&(fpc1022->ttw_wl));
	// mutex_destroy(&fpc1022->lock);
	//wakeup_source_unregister(fpc1022->ttw_wl);

	input_unregister_device(fpc1022->idev);
	devm_kfree(dev, fpc1022);

	return 0;
}

static struct of_device_id fpc1022_of_match[] = {
	{.compatible = "fpc,fingerprint",},
	{}
};

MODULE_DEVICE_TABLE(of, fpc1022_of_match);

static struct platform_driver fpc1022_driver = {
	.driver = {
		   .name = "fpc1022_irq",
		   .owner = THIS_MODULE,
		   .of_match_table = fpc1022_of_match,
		   },
	.probe = fpc1022_platform_probe,
	.remove = fpc1022_platform_remove
};

static int __init fpc1022_init(void)
{
	printk(KERN_INFO "%s\n", __func__);
	if (0 != platform_driver_register(&fpc1022_driver)) {
		printk(KERN_INFO "%s: register platform driver fail\n",
		       __func__);
		return -EINVAL;
	}
	return 0;
}

static void __exit fpc1022_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	platform_driver_unregister(&fpc1022_driver);
}

late_initcall(fpc1022_init);
module_exit(fpc1022_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aleksej Makarov");
MODULE_AUTHOR("Henrik Tillman <henrik.tillman@fingerprints.com>");
MODULE_DESCRIPTION("fpc1022 Fingerprint sensor device driver.");
