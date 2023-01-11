/*!
 * @section LICENSE
 *
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *------------------------------------------------------------------------------
 * Disclaimer
 *
 * Common: Bosch Sensortec products are developed for the consumer goods
 * industry. They may only be used within the parameters of the respective valid
 * product data sheet.  Bosch Sensortec products are provided with the express
 * understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive
 * systems or any system or device that may lead to bodily harm or property
 * damage if the system or device malfunctions. In addition, Bosch Sensortec
 * products are not fit for use in products which interact with motor vehicle
 * systems.  The resale and/or use of products are at the purchaser's own risk
 * and his own responsibility. The examination of fitness for the intended use
 * is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims,
 * including any claims for incidental, or consequential damages, arising from
 * any product use not covered by the parameters of the respective valid product
 * data sheet or not approved by Bosch Sensortec and reimburse Bosch Sensortec
 * for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety and inform Bosch Sensortec without
 * delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary
 * from the valid technical specifications of the product series. They are
 * therefore not intended or fit for resale to third parties or for use in end
 * products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series.
 * Bosch Sensortec assumes no liability for the use of engineering samples. By
 * accepting the engineering samples, the Purchaser agrees to indemnify Bosch
 * Sensortec from all claims arising from the use of engineering samples.
 *
 * Special: This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. The Software and Information is subject to the following terms and
 * conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 *
 * @filename bmp18x-i2c.c
 * @date	 "Mon Jul 8 10:27:33 2013 +0800"
 * @id		 "4fd82f5"
 *
 * @brief
 * This file implements moudle function, which add
 * the driver to I2C core.
 *
 * Copyright (c) 2011  Bosch Sensortec GmbH
 * Copyright (c) 2011  Unixphere
 *
 * Based on:
 * BMP085 driver, bmp085.c
 * Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

#include "bmp18x.h"

static int bmp18x_i2c_read_block(void *client, u8 reg, int len, char *buf)
{
	return i2c_smbus_read_i2c_block_data(client, reg, len, buf);
}

static int bmp18x_i2c_read_byte(void *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int bmp18x_i2c_write_byte(void *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static const struct bmp18x_bus_ops bmp18x_i2c_bus_ops = {
	.read_block	= bmp18x_i2c_read_block,
	.read_byte	= bmp18x_i2c_read_byte,
	.write_byte	= bmp18x_i2c_write_byte
};

#ifdef CONFIG_OF
static int bmp18x_i2c_probe_dt(struct i2c_client *client)
{
	struct bmp18x_platform_data *platform_data;

	platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
	if (platform_data == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	client->dev.platform_data = platform_data;

	return 0;
}
#endif

static int bmp18x_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *id)
{
	int retval = 0;
	struct bmp18x_data_bus data_bus = {
		.bops = &bmp18x_i2c_bus_ops,
		.client = client
	};
#ifdef CONFIG_OF
	retval = bmp18x_i2c_probe_dt(client);
	if (retval == -ENOMEM) {
		dev_err(&client->dev,
		"%s: Failed to alloc mem for bmp18x_platform_data\n", __func__);
		return retval;
	}
#endif
	return bmp18x_probe(&client->dev, &data_bus);
}

static void bmp18x_i2c_shutdown(struct i2c_client *client)
{
	bmp18x_disable(&client->dev);
}

static int bmp18x_i2c_remove(struct i2c_client *client)
{
	return bmp18x_remove(&client->dev);
}

#ifdef CONFIG_PM
static int bmp18x_i2c_suspend(struct device *dev)
{
	return bmp18x_disable(dev);
}

static int bmp18x_i2c_resume(struct device *dev)
{
	return bmp18x_enable(dev);
}

static const struct dev_pm_ops bmp18x_i2c_pm_ops = {
	.suspend	= bmp18x_i2c_suspend,
	.resume		= bmp18x_i2c_resume
};
#endif

static const struct i2c_device_id bmp18x_id[] = {
	{ BMP18X_NAME, 0 },
	{ }
};

static struct of_device_id bmp18x_dt_ids[] = {
	{ .compatible = "bosch,bmp18x", },
	{}
}
;

static struct i2c_driver bmp18x_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= BMP18X_NAME,
#ifdef CONFIG_PM
		.pm	= &bmp18x_i2c_pm_ops,
#endif
		.of_match_table = of_match_ptr(bmp18x_dt_ids),
	},
	.id_table	= bmp18x_id,
	.probe		= bmp18x_i2c_probe,
	.shutdown	= bmp18x_i2c_shutdown,
	.remove		= bmp18x_i2c_remove
};

static int __init bmp18x_i2c_init(void)
{
	return i2c_add_driver(&bmp18x_i2c_driver);
}

static void __exit bmp18x_i2c_exit(void)
{
	i2c_del_driver(&bmp18x_i2c_driver);
}


MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("BMP18X I2C bus driver");
MODULE_LICENSE("GPLv2");

module_init(bmp18x_i2c_init);
module_exit(bmp18x_i2c_exit);
