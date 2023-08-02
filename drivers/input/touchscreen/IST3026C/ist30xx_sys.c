/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <asm/io.h>
#include <mach/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <mach/regulator.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_tracking.h"

/******************************************************************************
 * Return value of Error
 * EPERM  : 1 (Operation not permitted)
 * ENOENT : 2 (No such file or directory)
 * EIO    : 5 (I/O error)
 * ENXIO  : 6 (No such device or address)
 * EINVAL : 22 (Invalid argument)
 *****************************************************************************/

extern struct tsp_dev_info *ts_pdata;
int ist30xx_cmd_gesture(struct i2c_client *client, int value)
{
	int ret = -EIO;

	if (value > 3)
		return ret;

	ret = ist30xx_write_cmd(client, 
            IST30XX_HIB_CMD, (eHCOM_GESTURE_EN << 16) | (value & 0xFFFF));

	return ret;
}

int ist30xx_cmd_start_scan(struct ist30xx_data *data)
{
	int ret = ist30xx_write_cmd(data->client, IST30XX_HIB_CMD, 
            (eHCOM_FW_START << 16) | (IST30XX_ENABLE & 0xFFFF));

	ist30xx_tracking(TRACK_CMD_SCAN);

	data->status.noise_mode = true;

	return ret;
}

int ist30xx_cmd_calibrate(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, 
            IST30XX_HIB_CMD, (eHCOM_RUN_CAL_AUTO << 16));

	ist30xx_tracking(TRACK_CMD_CALIB);

	tsp_info("%s\n", __func__);

	return ret;
}

int ist30xx_cmd_check_calib(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, IST30XX_HIB_CMD, 
            (eHCOM_RUN_CAL_PARAM << 16) | (IST30XX_ENABLE & 0xFFFF));

	ist30xx_tracking(TRACK_CMD_CHECK_CALIB);

	tsp_info("*** Check Calibration cmd ***\n");

	return ret;
}

int ist30xx_cmd_hold(struct i2c_client *client, int enable)
{
	int ret = ist30xx_write_cmd(client, 
            IST30XX_HIB_CMD, (eHCOM_FW_HOLD << 16) | (enable & 0xFFFF));

    msleep(40);

    if (enable)
  		ist30xx_tracking(TRACK_CMD_ENTER_REG);	
    else
        ist30xx_tracking(TRACK_CMD_EXIT_REG);

	return ret;
}

int ist30xx_read_reg(struct i2c_client *client, u32 reg, u32 *buf)
{
	int ret;
    u32 le_reg = cpu_to_be32(reg);

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST30XX_ADDR_LEN,
			.buf = (u8 *)&le_reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d), cmd: %x\n", __func__, ret, reg);
		return -EIO;
	}
	*buf = cpu_to_be32(*buf);

	return 0;
}

int ist30xx_read_cmd(struct ist30xx_data *data, u32 cmd, u32 *buf)
{
	int ret;

    ret = ist30xx_cmd_hold(data->client, 1);
	if (unlikely(ret))
		return ret;
 
    ist30xx_read_reg(data->client, IST30XX_DA_ADDR(cmd), buf);

    ret = ist30xx_cmd_hold(data->client, 0);
	if (unlikely(ret)) {
        ist30xx_reset(data, false);
		return ret;
    }

	return ret;
}

int ist30xx_write_cmd(struct i2c_client *client, u32 cmd, u32 val)
{
	int ret;
    
	struct i2c_msg msg;
	u8 msg_buf[IST30XX_ADDR_LEN + IST30XX_DATA_LEN];

	put_unaligned_be32(cmd, msg_buf);
	put_unaligned_be32(val, msg_buf + IST30XX_ADDR_LEN);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_ADDR_LEN + IST30XX_DATA_LEN;
	msg.buf = msg_buf;

	ret = i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d), cmd: %x(%x)\n", __func__, ret, cmd, val);
		return -EIO;
	}

    msleep(40);

    return 0;    
}

int ist30xx_burst_read(struct i2c_client *client, u32 addr,
			u32 *buf32, u16 len, bool bit_en)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_READ_SIZE / IST30XX_DATA_LEN;
    u16 remain_len = len;
	
	if (bit_en)
		addr = IST30XX_BA_ADDR(addr);

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len) max_len = remain_len;
        
		ret = ist30xx_read_buf(client, addr, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("Burst fail, addr: %x\n", __func__, addr);
			return ret;
		}

		addr += max_len * IST30XX_DATA_LEN;
		buf32 += max_len;
        remain_len -= max_len;
	}

	return 0;
}

int ist30xx_burst_write(struct i2c_client *client, u32 addr, 
            u32 *buf32, u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_WRITE_SIZE / IST30XX_DATA_LEN;
    u16 remain_len = len;
	
	addr = IST30XX_BA_ADDR(addr);

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len) max_len = remain_len;

		ret = ist30xx_write_buf(client, addr, buf32, max_len);
		if (unlikely(ret)) {
			tsp_err("Burst fail, addr: %x\n", __func__, addr);
			return ret;
		}

		addr += max_len * IST30XX_DATA_LEN;
		buf32 += max_len;
        remain_len -= max_len;        
	}

	return 0;
}

static void ts_power_enable(struct ist30xx_data *data, int en)
{
	static struct regulator *ts_vdd = NULL;

	if (ts_vdd == NULL) {
		ts_vdd = regulator_get(NULL, "vddsim2");
		printk("TS power enable get voltage .\n");
	}
	if (en) {
		regulator_set_voltage(ts_vdd, 3000000, 3000000);
		printk("TS power enable set voltage .\n");
		regulator_enable(ts_vdd);
	}
	else if (regulator_is_enabled(ts_vdd)) {
	printk("TS regulator disable\n");
		regulator_disable(ts_vdd);
	}


	printk(KERN_INFO "%s %s\n", __func__, (en) ? "on" : "off");
	printk("%s: TS power change to %d.\n", __func__, en);
}	

int ist30xx_power_on(struct ist30xx_data *data, bool download)
{
	if (data->status.power != 1) {
		tsp_info("%s()\n", __func__);
		ist30xx_tracking(TRACK_PWR_ON);
		printk("[TSP]Tracking function\n");
		/* VDD enable */
		msleep(5);
		/* VDDIO enable */
		ts_power_enable(data, 1);
		if (download)
			msleep(8);
		else
			msleep(100);
		data->status.power = 1;
		
		printk("TSP - RETUERN POWER ON\n");
	}

	return 0;
}

int ist30xx_power_off(struct ist30xx_data *data)
{
	if (data->status.power != 0) {
		tsp_info("%s()\n", __func__);
		ist30xx_tracking(TRACK_PWR_OFF);
		/* VDDIO disable */
		msleep(5);
		/* VDD disable */
		ts_power_enable(data, 0);
		msleep(50);
		data->status.power = 0;
		data->status.noise_mode = false;
	}

	return 0;
}

int ist30xx_reset(struct ist30xx_data *data, bool download)
{
	tsp_info("%s()\n", __func__);
	ist30xx_power_off(data);
	msleep(10);
	ist30xx_power_on(data, download);

	return 0;
}

int ist30xx_internal_suspend(struct ist30xx_data *data)
{
	ist30xx_power_off(data);

    return 0;
}

int ist30xx_internal_resume(struct ist30xx_data *data)
{
	ist30xx_power_on(data, false);

	return 0;
}

int ist30xx_init_system(struct ist30xx_data *data)
{
	int ret;

	// TODO : place additional code here.
	ret = ist30xx_power_on(data, false);
	if (ret) {
		tsp_err("%s: ist30xx_init_system failed (%d)\n", __func__, ret);
		return -EIO;
	}
	
#if 0
	ret = ist30xx_reset(data, false);
	if (ret) {
		tsp_err("%s: ist30xx_reset failed (%d)\n", __func__, ret);
		return -EIO;
	}
#endif

	return 0;
}
