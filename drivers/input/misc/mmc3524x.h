/*
 * Copyright (C) 2014 MEMSIC, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * Definitions for mmc3524x magnetic sensor chip.
 */
#ifndef __MMC3524X_H__
#define __MMC3524X_H__

#include <linux/ioctl.h>

#define MMC3524X_I2C_NAME		"mmc3524x"

#define MMC3524X_I2C_ADDR		0x30

#define MMC3524X_REG_CTRL		0x07
#define MMC3524X_REG_BITS		0x08
#define MMC3524X_REG_DATA		0x00
#define MMC3524X_REG_DS			0x06
#define MMC3524X_REG_PRODUCTID_0		0x10
#define MMC3524X_REG_PRODUCTID_1		0x20

#define MMC3524X_CTRL_TM			0x01
#define MMC3524X_CTRL_CM			0x02
#define MMC3524X_CTRL_50HZ		0x00
#define MMC3524X_CTRL_25HZ		0x04
#define MMC3524X_CTRL_12HZ		0x08
#define MMC3524X_CTRL_NOBOOST            0x10
#define MMC3524X_CTRL_SET  	        0x20
#define MMC3524X_CTRL_RESET              0x40
#define MMC3524X_CTRL_REFILL             0x80

#define MMC3524X_BITS_SLOW_16            0x00
#define MMC3524X_BITS_FAST_16            0x01
#define MMC3524X_BITS_14                 0x02
/* Use 'm' as magic number */
#define MMC3524X_IOM			'm'

typedef struct mmc3524x_read_reg_str
{
	unsigned char start_register;
	unsigned char length;
	unsigned char reg_contents[10];
} read_reg_str ;

/* IOCTLs for MMC3524X device */
#define MMC3524X_IOC_TM			_IO (MMC3524X_IOM, 0x00)
#define MMC3524X_IOC_SET			_IO (MMC3524X_IOM, 0x01)
#define MMC3524X_IOC_READ		_IOR(MMC3524X_IOM, 0x02, int[3])
#define MMC3524X_IOC_READXYZ		_IOR(MMC3524X_IOM, 0x03, int[3])
#define MMC3524X_IOC_RESET               _IO (MMC3524X_IOM, 0x04)
#define MMC3524X_IOC_NOBOOST             _IO (MMC3524X_IOM, 0x05)
#define MMC3524X_IOC_ID                  _IOR(MMC3524X_IOM, 0x06, short)
#define MMC3524X_IOC_DIAG                _IOR(MMC3524X_IOM, 0x14, int[1])
#define MMC3524X_IOC_READ_REG		_IOWR(MMC3524X_IOM, 0x15, unsigned char)
#define MMC3524X_IOC_WRITE_REG		_IOW(MMC3524X_IOM, 0x16, unsigned char[2])
#define MMC3524X_IOC_READ_REGS		_IOWR(MMC3524X_IOM, 0x17, unsigned char[10])
#define MMC3524X_IOC_WRITE_REGS		_IOW(MMC3524X_IOM, 0x18, unsigned char[10])

#endif /* __MMC3524X_H__ */

