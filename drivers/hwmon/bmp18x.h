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
 * @filename bmp18x.h
 * @date	 "Mon Jul 8 10:27:33 2013 +0800"
 * @id		 "4fd82f5"
 *
 * @brief
 * The head file of BMP18X device driver core code
 *
 * Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>
 * Copyright (c) 2011  Bosch Sensortec GmbH
 * Copyright (c) 2011  Unixphere AB
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
#ifndef _BMP18X_H
#define _BMP18X_H

#define BMP18X_NAME "BMP_pressure_sensor"

/**
 * struct bmp18x_platform_data - represents platform data for the bmp18x driver
 * @chip_id: Configurable chip id for non-default chip revisions
 * @default_oversampling: Default oversampling value to be used at startup,
 * value range is 0-3 with rising sensitivity.
 * @default_sw_oversampling: Default software oversampling value to be used
 * at startup,value range is 0(Disabled) or 1(Enabled). Only take effect
 * when default_oversampling is 3.
 * @temp_measurement_period: Temperature measurement period (milliseconds), set
 * to zero if unsure.
 * @init_hw: Callback for hw specific startup
 * @deinit_hw: Callback for hw specific shutdown
 */
struct bmp18x_platform_data {
	u8	chip_id;
	u8	default_oversampling;
	u8	default_sw_oversampling;
	u32	temp_measurement_period;
	char	avdd_name[20];
	int	(*init_hw)(int);
	int	(*deinit_hw)(int);
};

struct bmp18x_bus_ops {
	int	(*read_block)(void *client, u8 reg, int len, char *buf);
	int	(*read_byte)(void *client, u8 reg);
	int	(*write_byte)(void *client, u8 reg, u8 value);
};

struct bmp18x_data_bus {
	const struct bmp18x_bus_ops	*bops;
	void	*client;
};

int bmp18x_probe(struct device *dev, struct bmp18x_data_bus *data_bus);
int bmp18x_remove(struct device *dev);
#ifdef CONFIG_PM
int bmp18x_enable(struct device *dev);
int bmp18x_disable(struct device *dev);
#endif

#endif
