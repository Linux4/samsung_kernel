/*
	$License:
	Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	$
 */


#ifndef __MPU6050_H__
#define __MPU6050_H__

#include <linux/mpu_411.h>

struct mpu6050_config {
	unsigned int odr;		/**< output data rate 1/1000 Hz */
	unsigned int fsr;		/**< full scale range mg */
	unsigned int ths;		/**< mot/no-mot thseshold mg */
	unsigned int dur;		/**< mot/no-mot duration ms */
	unsigned int irq_type;		/**< irq type */
};

struct ext_slave_descr *mpu6050_get_slave_descr(void);
extern int mpu6050_set_odr(void *mlsl_handle,
			  struct ext_slave_platform_data *pdata,
			  struct mpu6050_config *config, long apply, long odr);

#endif
