/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks.
 * *
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
 *
 * Copyright (c) 2018 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>



#if 1
#define FPC_ALOGF()                 do{printk(KERN_ERR "[fpc@f@%04d]-@%s(): entry!\n", __LINE__, __FUNCTION__);}while(0)
#define FPC_ALOGD(fmt, args...)     do{printk(KERN_ERR "[fpc@d@%04d]-@%s(): " fmt "\n", __LINE__, __FUNCTION__, ##args);}while(0)
#define FPC_ALOGE(fmt, args...)     do{printk(KERN_ERR "[fpc@e@%04d]-@%s(): " fmt "\n", __LINE__, __FUNCTION__, ##args);}while(0)
#else
#define APS_ALOGF()                 do {} while (0)
#define APS_ALOGD(fmt, args...)     do {} while (0)
#define APS_ALOGE(fmt, args...)     do{printk(KERN_ERR "[alsps@e@%04d]-@%s(): " fmt "\n", __LINE__, __FUNCTION__, ##args);}while(0)
#endif

#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000
#define FPC_TTW_HOLD_TIME 1000

#define FPC102X_REG_HWID              0xfc
#define FPC1520_CHIP                  0x1000 //0x10xx

#define FPC1022_CHIP_MASK_SENSOR_TYPE 0xff00
//extern bool sunwave_fp_exist ;
extern struct spi_device *spi_fingerprint;
extern int fpc_or_chipone_exist;
extern int fingerprint_adm;

static const char * const pctl_names[] = {
	"rst-low",
	"rst-high",
};

struct fpc_data {
	struct device *dev;
	struct spi_device *spidev;
	struct pinctrl *pinctrl_fpc;
	struct platform_device *pldev;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
	int irq_gpio;
	int rst_gpio;
	bool wakeup_enabled;
	bool compatible_enabled;
	struct wakeup_source ttw_wl;
};

static DEFINE_MUTEX(spidev_set_gpio_mutex);

extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);

static irqreturn_t fpc_irq_handler(int irq, void *handle);

static int select_pin_ctl(struct fpc_data *fpc, const char *name)
{
	size_t i;
	int rc;
	struct device *dev = fpc->dev;
	for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
		const char *n = pctl_names[i];
		if (!strncmp(n, name, strlen(n))) {
			mutex_lock(&spidev_set_gpio_mutex);
			rc = pinctrl_select_state(fpc->pinctrl_fpc, fpc->pinctrl_state[i]);
			mutex_unlock(&spidev_set_gpio_mutex);
			if (rc)
				dev_err(dev, "cannot select '%s'\n", name);
			else
				dev_dbg(dev, "Selected '%s'\n", name);
			goto exit;
		}
	}
	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found\n", __func__, name);
exit:
	return rc;
}

static int set_clks(struct fpc_data *fpc, bool enable)
{
	int rc = 0;

	if (enable) {
		mt_spi_enable_master_clk(fpc->spidev);
		rc = 1;
	} else {
		mt_spi_disable_master_clk(fpc->spidev);
		rc = 0;
	}

	return rc;
}

static int hw_reset(struct  fpc_data *fpc)
{
	int irq_gpio;
	//struct device *dev = fpc->dev;
	FPC_ALOGF();
	select_pin_ctl(fpc, "rst-high");
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	select_pin_ctl(fpc, "rst-low");
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	select_pin_ctl(fpc, "rst-high");
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);

	irq_gpio = gpio_get_value(fpc->irq_gpio);
	FPC_ALOGD("IRQ after reset %d\n", irq_gpio);

	FPC_ALOGD("Using GPIO#%d as IRQ.\n", fpc->irq_gpio );
	FPC_ALOGD("Using GPIO#%d as RST.\n", fpc->rst_gpio );

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct  fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset"))) {
		rc = hw_reset(fpc);
		return rc ? rc : count;
	}
	else
		return -EINVAL;
}


static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc_data *fpc = dev_get_drvdata(dev);
	ssize_t ret = count;

	if (!strncmp(buf, "enable", strlen("enable"))) {
		fpc->wakeup_enabled = true;
		smp_wmb();
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		fpc->wakeup_enabled = false;
		smp_wmb();
	}
	else
		ret = -EINVAL;

	return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
			struct device_attribute *attribute,
			char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc->irq_gpio);

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
	struct fpc_data *fpc = dev_get_drvdata(device);
	dev_dbg(fpc->dev, "%s\n", __func__);

	return count;
}
static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t clk_enable_set(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	return set_clks(fpc, (*buf == '1')) ? : count;
}
static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);
/*
static ssize_t compatible_all_set(struct device *device,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct device_node *node_eint;
	
	int irqf = 0;
	int irq_num = 0;
	int rc = 0;
	size_t i;
	
	struct  fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = fpc->dev;

	 if(!strncmp(buf, "enable", strlen("enable")) && fpc->compatible_enabled != 1)
	 	{	
		fpc->pinctrl_fpc = devm_pinctrl_get(&fpc->spidev->dev);
		
		if (IS_ERR(fpc->pinctrl_fpc)) {
			rc = PTR_ERR(fpc->pinctrl_fpc);
			dev_err(fpc->dev, "Cannot find pinctrl_fpc rc = %d.\n", rc);
			goto exit;
		}

		for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
			const char *n = pctl_names[i];
			struct pinctrl_state *state = pinctrl_lookup_state(fpc->pinctrl_fpc, n);
			if (IS_ERR(state)) {
				dev_err(dev, "cannot find '%s'\n", n);
				rc = -EINVAL;
				goto exit;
			}
			dev_info(dev, "found pin control %s\n", n);
			fpc->pinctrl_state[i] = state;
		}

		node_eint = of_find_compatible_node(NULL, NULL, "mediatek,fpsensor_fp_eint");
		if (node_eint == NULL) {
			rc = -EINVAL;
			FPC_ALOGE("mediatek,fpsensor_fp_eint cannot find node_eint rc = %d.\n", rc);
			goto exit;
		}

		fpc->irq_gpio = of_get_named_gpio(node_eint, "int-gpios", 0);
		FPC_ALOGD("irq_gpio=%d", fpc->irq_gpio);

		rc = devm_gpio_request(fpc->dev, fpc->irq_gpio, "fpc,gpio_irq");
		if (rc) {
				printk( KERN_ERR "failed to request gpio %d\n",  fpc->irq_gpio);
			}
			  
		irq_num = gpio_to_irq(fpc->irq_gpio);

		fpc->wakeup_enabled = false;

		irqf = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;
		if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
			irqf |= IRQF_NO_SUSPEND;
			device_init_wakeup(dev, 1);
		}
		
		rc = devm_request_threaded_irq(dev, irq_num,
			NULL, fpc_irq_handler, irqf,
			dev_name(dev), fpc);
		if (rc) {
			FPC_ALOGE("could not request irq %d\n", irq_num);
			goto exit;
		}
		enable_irq_wake(irq_num);
		wakeup_source_init(&fpc->ttw_wl, "fpc_ttw_wl");
		(void)hw_reset(fpc);
		fpc->compatible_enabled = 1;
	 }
	 else if(!strncmp(buf, "disable", strlen("disable")) && fpc->compatible_enabled != 0)
	 {
	 	if (gpio_is_valid(fpc->irq_gpio))
		{
			devm_gpio_free(dev, fpc->irq_gpio);
			FPC_ALOGE("remove irq_gpio success\n");
		}
		if (gpio_is_valid(fpc->rst_gpio))
		{
			devm_gpio_free(dev, fpc->rst_gpio);
			FPC_ALOGE("remove rst_gpio success\n");
		}
		devm_free_irq(dev, gpio_to_irq(fpc->irq_gpio), fpc);
		fpc->compatible_enabled = 0;
	 }
	 
	exit:
	return -EINVAL;
}
static DEVICE_ATTR(compatible_all, S_IWUSR, NULL, compatible_all_set);
*/
static struct attribute *fpc_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	//&dev_attr_compatible_all.attr,
	NULL
};

static const struct attribute_group const fpc_attribute_group = {
	.attrs = fpc_attributes,
};

static irqreturn_t fpc_irq_handler(int irq, void *handle)
{
	struct fpc_data *fpc = handle;
	struct device *dev = fpc->dev;
	static int current_level = 0; // We assume low level from start
	current_level = !current_level;

	if (current_level) {
		dev_dbg(dev, "Reconfigure irq to trigger in low level\n");
		irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
	} else {
		dev_dbg(dev, "Reconfigure irq to trigger in high level\n");
		irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
	}

	/* Make sure 'wakeup_enabled' is updated before using it
	** since this is interrupt context (other thread...) */
	smp_rmb();
	if (fpc->wakeup_enabled) {
		__pm_wakeup_event(&fpc->ttw_wl, FPC_TTW_HOLD_TIME);
	}

	sysfs_notify(&fpc->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}

static int spi_read_hwid(struct spi_device *spi, u8 *rx_buf)
{
	int error;
	struct spi_message msg;
	struct spi_transfer *xfer;
	u8 tmp_buf[10]={0};
	u8 addr = FPC102X_REG_HWID;
	u32 spi_speed = 1*1000000;

	xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
	if (xfer == NULL) {
		dev_err(&spi->dev, "%s, no memory for SPI transfer\n", __func__);
		return -ENOMEM;
	}

	spi_message_init(&msg);

	tmp_buf[0] = addr;
	xfer[0].tx_buf = tmp_buf;
	xfer[0].len = 1;
	xfer[0].delay_usecs = 5;

	xfer[0].speed_hz = spi_speed;
	dev_err(&spi->dev, "%s %d, now spi-clock:%d\n",
			__func__, __LINE__, xfer[0].speed_hz);

	spi_message_add_tail(&xfer[0], &msg);

	xfer[1].tx_buf = tmp_buf + 2;
	xfer[1].rx_buf = tmp_buf + 4;
	xfer[1].len = 2;
	xfer[1].delay_usecs = 5;
	xfer[1].speed_hz = spi_speed;

	spi_message_add_tail(&xfer[1], &msg);
	error = spi_sync(spi, &msg);
	if (error)
		dev_err(&spi->dev, "%s : spi_sync failed, error = %d.\n", __func__, error);

	memcpy(rx_buf, (tmp_buf + 4), 2);

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}


static int check_hwid(struct spi_device *spi)
{
	int error = 0;
	u32 time_out = 0;
	u8 tmp_buf[2] = {0};
	u16 hardware_id;

	do {
		spi_read_hwid(spi, tmp_buf);
		printk(KERN_INFO "%s, fpc1022 chip version is 0x%x, 0x%x\n", __func__, tmp_buf[0], tmp_buf[1]);

		time_out++;

		hardware_id = ((tmp_buf[0] << 8) | (tmp_buf[1]));
		pr_err("fpc hardware_id[0]= 0x%x id[1]=0x%x\n", tmp_buf[0], tmp_buf[1]);

		if(( hardware_id & FPC1022_CHIP_MASK_SENSOR_TYPE) == FPC1520_CHIP)
		{
			pr_err("fpc match hardware_id = 0x%x is true\n", hardware_id);
			error = 0;
		}
		else
		{
			pr_err("fpc match hardware_id = 0x%x is failed\n", hardware_id);
			error = -1;
		}


		if (!error) {
			printk(KERN_INFO "fpc %s, fpc1022 chip version check pass, time_out=%d\n", __func__, time_out);
			return 0;
		}
	} while(time_out < 3);

	printk(KERN_INFO "%s, fpc chip version read failed, time_out=%d\n", __func__, time_out);

	return -1;
}

static int fpc1022_platform_probe(struct platform_device *pldev)
{
	int ret = 0,rc,i;
         int irqf,irq_num;
         struct device *dev = &pldev->dev;
         struct fpc_data *fpc =NULL;
	struct device_node *node_eint;

	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

         dev_info(dev, "%s test new\n", __func__);

         fpc = devm_kzalloc(dev, sizeof(struct fpc_data), GFP_KERNEL);
         if (!fpc) {
                   dev_err(dev,
                            "failed to allocate memory for struct fpc1022_data\n");
                   ret = -ENOMEM;
                   goto exit;
         }

        fpc->dev = dev;
         dev_set_drvdata(dev, fpc);
        fpc->pldev = pldev;


	node = of_find_compatible_node(NULL, NULL, "mediatek,finger-fp");
		if (node) {
			pdev = of_find_device_by_node(node);
			if (pdev) {
				fpc->pinctrl_fpc = devm_pinctrl_get(&pdev->dev);
				if (IS_ERR(fpc->pinctrl_fpc)) {
					ret = PTR_ERR(fpc->pinctrl_fpc);
					printk(KERN_ERR "%s can't find fingerprint pinctrl\n", __func__);
					goto exit;
					}
				}
			
			for (i = 0; i < ARRAY_SIZE(fpc->pinctrl_state); i++) {
					const char *n = pctl_names[i];
					struct pinctrl_state *state = pinctrl_lookup_state(fpc->pinctrl_fpc, n);
					if (IS_ERR(state)) {
						dev_err(dev, "cannot find '%s'\n", n);
						rc = -EINVAL;
						goto exit;
					}
					dev_info(dev, "found pin control %s\n", n);
					fpc->pinctrl_state[i] = state;
				}
		}
	
	node_eint = of_find_compatible_node(NULL, NULL, "mediatek,fpsensor_fp_eint");
	if (node_eint == NULL) {
		rc = -EINVAL;
		FPC_ALOGE("mediatek,fpsensor_fp_eint cannot find node_eint rc = %d.\n", rc);
		goto exit;
	}

	fpc->irq_gpio = of_get_named_gpio(node_eint, "int-gpios", 0);
	FPC_ALOGD("irq_gpio=%d", fpc->irq_gpio);
      /*
	irq_num = irq_of_parse_and_map(node_eint, 0);
	if (!irq_num) {
		rc = -EINVAL;
		FPC_ALOGE("get irq_num error rc = %d.\n", rc);
		printk(KERN_ERR "get irq_num error rc = %d.\n",rc);
		goto exit;
	}
	*/

	rc = devm_gpio_request(fpc->dev, fpc->irq_gpio, "fpc,gpio_irq");
	if (rc) {
			printk( KERN_ERR "failed to request gpio %d\n",  fpc->irq_gpio);
		}
		  
	irq_num = gpio_to_irq(fpc->irq_gpio);
		  
	printk(KERN_ERR " [shi] fpc irq_num = %d.\n",irq_num);
    
	fpc->spidev=spi_fingerprint;

	set_clks(fpc, true);

	fpc->wakeup_enabled = false;

	(void)hw_reset(fpc);
      
	rc= check_hwid(fpc->spidev);
	if(rc == -1){
		goto exit;
	}
	fingerprint_adm = rc;
	fpc_or_chipone_exist = 0;

	irqf = IRQF_TRIGGER_HIGH | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}
	rc = devm_request_threaded_irq(dev, irq_num,
		NULL, fpc_irq_handler, irqf,
		dev_name(dev), fpc);
	if (rc) {
		FPC_ALOGE("could not request irq %d\n", irq_num);
		goto exit;
	}
	FPC_ALOGD("requested irq %d\n", irq_num);

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(irq_num);
	wakeup_source_init(&fpc->ttw_wl, "fpc_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
	if (rc) {
		printk(KERN_ERR "fpc  not creat sysfs_create_group\n");
		goto exit;
	}
	FPC_ALOGD("%s: ok\n", __func__);

	return 0;
exit:
	if (gpio_is_valid(fpc->irq_gpio))
	{
		devm_gpio_free(dev, fpc->irq_gpio);
		FPC_ALOGE("remove irq_gpio success\n");
	}
	if (gpio_is_valid(fpc->rst_gpio))
	{
		devm_gpio_free(dev, fpc->rst_gpio);
		FPC_ALOGE("remove rst_gpio success\n");
	}
//	devm_free_irq(dev, gpio_to_irq(fpc->irq_gpio), fpc);
	
	return 0;
}

static int fpc1022_platform_remove(struct platform_device *pldev)

{
	struct  fpc_data *fpc = dev_get_drvdata(&pldev->dev);

	sysfs_remove_group(&pldev->dev.kobj, &fpc_attribute_group);
	wakeup_source_trash(&fpc->ttw_wl);
	

	return 0;
}


//MODULE_DEVICE_TABLE(of, mt6797_of_match);

static struct of_device_id fpc1022_of_match[] = {
         { .compatible = "mediatek,fingerprint", },
         {}
};

MODULE_DEVICE_TABLE(of, fpc1022_of_match);

static struct platform_driver fpc1022_driver = {
        .driver = {
                  .name       = "fpc1022_irq",
                  .owner     = THIS_MODULE,
                  .of_match_table = fpc1022_of_match,
        },
         .probe      = fpc1022_platform_probe,
         .remove   = fpc1022_platform_remove
};


static int __init fpc_sensor_init(void)
{
    int status=0;
    FPC_ALOGF();
    printk(KERN_ERR "fpc koko  fpc_sensor_init \n");
/* 
   if (sunwave_fp_exist) {
        printk(KERN_ERR "%s sunwave sensor has been detected, so exit FPC sensor detect.\n",__func__);
        return -EINVAL;
    }
	*/
    if(fpc_or_chipone_exist != -1){
       printk(KERN_ERR "%s, chipone sensor has been detected.\n", __func__);
        return -EINVAL;
    }
    //workaround to solve two spi device
    if(spi_fingerprint == NULL)
        pr_notice("%s Line:%d spi device is NULL,cannot spi transfer\n",
                __func__, __LINE__);
    else {
	status = platform_driver_register(&fpc1022_driver);
	if (status !=0) {
		printk("%s, fpc_sensor_init failed.\n", __func__);
	 }
    	}

	return status;
}
late_initcall(fpc_sensor_init);

static void __exit fpc_sensor_exit(void)
{
	FPC_ALOGF();
	 platform_driver_unregister(&fpc1022_driver);
}
module_exit(fpc_sensor_exit);

MODULE_LICENSE("GPL");
