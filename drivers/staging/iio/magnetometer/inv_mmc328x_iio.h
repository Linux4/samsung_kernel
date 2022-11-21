/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

/**
 *  @addtogroup DRIVERS
 *  @brief      Hardware drivers.
 *
 *  @{
 *      @file  inv_mmc328x_iio.h
 *      @brief Struct definitions for the Invensense implementation
 *              of mmc328x driver.
 */

#ifndef _INV_GYRO_H_
#define _INV_GYRO_H_
#endif
#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "iio.h"
#include "buffer.h"
#include "trigger.h"

/**
 *  struct inv_mmc328x_state_s - Driver state variables.
 *  @i2c:		i2c client handle.
 *  @sl_handle:         Handle to I2C port.
 */
struct inv_mmc328x_state_s {
	struct mpu_platform_data plat_data;
	struct i2c_client *i2c;
	struct iio_trigger *trig;
	struct delayed_work work;
	short compass_data[3];
	int delay;
	s8 enable;
	void *sl_handle;
};

/* scan element definition */
enum inv_mpu_scan {
	INV_MMC328X_SCAN_MAGN_X,
	INV_MMC328X_SCAN_MAGN_Y,
	INV_MMC328X_SCAN_MAGN_Z,
	INV_MMC328X_SCAN_TIMESTAMP,
};

#define MMC328X_MAX_DELAY                   1000
#define MMC328X_MIN_DELAY                   10
#define MEMSIC_SCALE                       (5461 * (1<<15))

/* MMC328X register address */
#define MMC328X_REG_CTRL		0x07
#define MMC328X_REG_DATA		0x00
#define MMC328X_REG_DS			0x06

/* MMC328X control bit */
#define MMC328X_CTRL_TM		0x01
#define MMC328X_CTRL_RM		0x20
#define MMC328X_CTRL_RRM	0x40

int inv_mmc328x_configure_ring(struct iio_dev *indio_dev);
void inv_mmc328x_unconfigure_ring(struct iio_dev *indio_dev);
int inv_mmc328x_probe_trigger(struct iio_dev *indio_dev);
void inv_mmc328x_remove_trigger(struct iio_dev *indio_dev);
void set_mmc328x_enable(struct iio_dev *indio_dev, bool enable);
int mmc328x_read_raw_data(struct inv_mmc328x_state_s *st,
				short dat[3]);
void inv_read_mmc328x_fifo(struct iio_dev *indio_dev);
