/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8080_i2c.c

	Description : i2c interface source file

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA


	History :
	----------------------------------------------------------------------
*******************************************************************************/
#include <linux/mutex.h>

#include "fci_types.h"
#include "fc8080_regs.h"
#include "fci_oal.h"

#define CHIP_ADDR       0x58

static DEFINE_MUTEX(lock);

static s32 i2c_bulkread(HANDLE handle, u8 chip, u16 addr, u8 *data, u16 length)
{
	return BBM_OK;
}

static s32 i2c_bulkwrite(HANDLE handle, u8 chip, u16 addr, u8 *data, u16 length)
{
	return BBM_OK;
}

static s32 i2c_dataread(HANDLE handle, u8 chip, u16 addr, u8 *data, u16 length)
{
	return i2c_bulkread(handle, chip, addr, data, length);
}

s32 fc8080_i2c_init(HANDLE handle, u16 param1, u16 param2)
{
	/*ts_initialize();*/

	return BBM_OK;
}

s32 fc8080_i2c_byteread(HANDLE handle, u16 addr, u8 *data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (u8) CHIP_ADDR, addr, data, 1);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_wordread(HANDLE handle, u16 addr, u16 *data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (u8) CHIP_ADDR, addr, (u8 *) data, 2);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_longread(HANDLE handle, u16 addr, u32 *data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (u8) CHIP_ADDR, addr, (u8 *) data, 4);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_bulkread(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkread(handle, (u8) CHIP_ADDR, addr, data, length);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_bytewrite(HANDLE handle, u16 addr, u8 data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (u8) CHIP_ADDR, addr, (u8 *) &data, 1);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_wordwrite(HANDLE handle, u16 addr, u16 data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (u8) CHIP_ADDR, addr, (u8 *) &data, 2);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_longwrite(HANDLE handle, u16 addr, u32 data)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (u8) CHIP_ADDR, addr, (u8 *) &data, 4);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_bulkwrite(HANDLE handle, u16 addr, u8 *data, u16 length)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_bulkwrite(handle, (u8) CHIP_ADDR, addr, (u8 *) &data, length);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_dataread(HANDLE handle, u16 addr, u8 *data, u32 length)
{
	s32 res;

	mutex_lock(&lock);
	res = i2c_dataread(handle, (u8) CHIP_ADDR, addr, (u8 *) &data, length);
	mutex_unlock(&lock);

	return res;
}

s32 fc8080_i2c_deinit(HANDLE handle)
{
	/*ts_receiver_disable();*/
	return BBM_OK;
}

