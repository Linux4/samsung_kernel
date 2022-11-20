/*
 * drivers/hid/ntrig_spi_low.c
 *
 * Copyright (c) 2012, N-Trig
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "ntrig_spi_low.h"
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define DRIVER_NAME "ntrig_spi"

#ifdef CONFIG_PM

static ntrig_spi_suspend_callback suspend_callback;
static ntrig_spi_resume_callback resume_callback;

void ntrig_spi_register_pwr_mgmt_callbacks(
	ntrig_spi_suspend_callback s, ntrig_spi_resume_callback r)
{
	suspend_callback = s;
	resume_callback = r;
}
EXPORT_SYMBOL_GPL(ntrig_spi_register_pwr_mgmt_callbacks);

/**
 * called when suspending the device (sleep with preserve
 * memory)
 */
static int ntrig_spi_suspend(struct spi_device *dev, pm_message_t mesg)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	if (suspend_callback)
		return suspend_callback(dev, mesg);
	return 0;
}

/**
 * called when resuming after sleep (suspend)
 */
static int ntrig_spi_resume(struct spi_device *dev)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	if (resume_callback)
		return resume_callback(dev);
	return 0;
}

#endif	/* CONFIG_PM */

/**
 * release the SPI driver
 */
static int ntrig_spi_remove(struct spi_device *spi)
{
	printk(KERN_DEBUG "in %s\n", __func__);
	sci_glb_clr(REG_AP_APB_APB_EB, BIT_SPI0_EB);
	return 0;
}

static struct spi_device_data s_spi_device_data;

struct spi_device_data *get_ntrig_spi_device_data(void)
{
	return &s_spi_device_data;
}
EXPORT_SYMBOL_GPL(get_ntrig_spi_device_data);

#ifdef CONFIG_OF
static struct ntrig_spi_platform_data *ntrig_parse_dt(struct device *dev)
{
	struct ntrig_spi_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct fts_platform_data");
		return NULL;
	}
/*
	pdata->oe_gpio = of_get_gpio(np, 0);
	if(pdata->oe_gpio < 0){
		dev_err(dev, "fail to get reset_gpio_number\n");
		goto fail;
	}
	pdata->pwr_gpio = of_get_gpio(np, 1);
	if(pdata->pwr_gpio < 0){
		dev_err(dev, "fail to get reset_gpio_number\n");
		goto fail;
	}
*/
	ret = of_property_read_u32(np, "oe-gpio", &pdata->oe_gpio);
	if(ret){
		dev_err(dev, "fail to get oe_gpio\n");
		goto fail;
	}
	pdata->oe_gpio = -1;
	ret = of_property_read_u32(np, "oe-inverted", &pdata->oe_inverted);
	if(ret){
		dev_err(dev, "fail to get oe_inverted\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "pwr-gpio", &pdata->pwr_gpio);
	if(ret){
		dev_err(dev, "fail to get pwr_gpio\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "irq-flags", &pdata->irq_flags);
	if(ret){
		dev_err(dev, "fail to get irq_flags\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "irq-gpio", &pdata->irq_gpio);
	if(ret){
		dev_err(dev, "fail to get irq_gpio\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif

static int ntrig_spi_probe(struct spi_device *spi)
{
	struct ntrig_spi_platform_data *pdata = spi->dev.platform_data;

	printk("in %s\n", __func__);

#if 0//def CONFIG_OF
	struct device_node *np = spi->dev.of_node;
	if (np && !pdata){
		pdata = ntrig_parse_dt(&spi->dev);
		if(pdata){
			spi->dev.platform_data = pdata;
		}
		else{
			return -ENOMEM;
		}
	}
#else
	pdata->oe_gpio = 142;
	pdata->oe_inverted = 0;
	pdata->pwr_gpio = 143; //otg_en
	pdata->irq_flags = 0;
	pdata->irq_gpio = 121;
	//pdata->cs_gpio = 90;

	//gpio_request(pdata->irq_gpio, "ntrig_irq_gpio");
	//gpio_direction_input(pdata->irq_gpio);
	//spi->irq = gpio_to_irq(pdata->irq_gpio); //121
	printk("in %s, irq is %d\n", __func__, spi->irq);
#endif
	s_spi_device_data.m_spi_device = spi;
	s_spi_device_data.m_platform_data = *(struct ntrig_spi_platform_data *)spi->dev.platform_data;
#ifdef CONFIG_HAS_EARLYSUSPEND
	s_spi_device_data.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	s_spi_device_data.early_suspend.suspend = ntrig_spi_suspend;
	s_spi_device_data.early_suspend.resume	= ntrig_spi_resume;
	register_early_suspend(&s_spi_device_data.early_suspend);
#endif
	sci_glb_set(REG_AP_APB_APB_EB, BIT_SPI0_EB);
	return 0;
}

static const struct of_device_id ntrig_of_match[] = {                                                      
	{ .compatible = "ntrig,ntrig_spi", },
	{ }
};

static struct spi_board_info spidev_boardinfo[] = {
    {
    .modalias = "ntrig_spi",
    .bus_num = 0,
    .chip_select = 0,
    .max_speed_hz = 8 * 1000 * 1000,
    .mode = SPI_MODE_0,                                                       
//  .platform_data = &mxd_plat_data,
    },
};


static struct spi_driver ntrig_spi_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
//		.of_match_table = ntrig_of_match,
	},
	.suspend = NULL, //ntrig_spi_suspend,
	.resume = NULL, //ntrig_spi_resume,
	.probe		= ntrig_spi_probe,
	.remove		= ntrig_spi_remove,
};

static int __init ntrig_spi_init(void)
{
	int ret = 0;
    struct spi_board_info *info = spidev_boardinfo;
//    int busnum, cs, gpio;
//	int i;

	printk("in %s\n", __func__);
//	for (i = 0; i < ARRAY_SIZE(spidev_boardinfo); i++) {
//		busnum = info[i].bus_num;
//    	cs = info[i].chip_select;
//		gpio   = spi_cs_gpio_map[busnum][cs];
 
//		info[i].controller_data = (void *)gpio;
//	}
	ret = spi_register_board_info(info, ARRAY_SIZE(spidev_boardinfo));
	if (ret != 0)
		printk(KERN_ERR "Failed to register ntrig device: %d\n",
			ret);

	ret = spi_register_driver(&ntrig_spi_driver);
	if (ret != 0)
		printk(KERN_ERR "Failed to register N-trig SPI driver: %d\n",
			ret);

	return ret;
}
module_init(ntrig_spi_init);

static void __exit ntrig_spi_exit(void)
{
	spi_unregister_driver(&ntrig_spi_driver);
}
module_exit(ntrig_spi_exit);

MODULE_ALIAS("ntrig_spi");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("N-Trig SPI driver");

