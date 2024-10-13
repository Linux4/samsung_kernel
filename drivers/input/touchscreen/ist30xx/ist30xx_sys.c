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
/* Removing the error for finding mach/vreg.h
#include <mach/vreg.h>
*/
#include "ist30xx.h"
#include "ist30xx_tracking.h"
/*For regulators*/
#include <linux/regulator/machine.h>







/******************************************************************************
 * Return value of Error
 * EPERM  : 1 (Operation not permitted)
 * ENOENT : 2 (No such file or directory)
 * EIO    : 5 (I/O error)
 * ENXIO  : 6 (No such device or address)
 * EINVAL : 22 (Invalid argument)
 *****************************************************************************/

extern struct ist30xx_data *ts_data;
const u32 pos_cmd = cpu_to_be32(CMD_GET_COORD);
struct i2c_msg pos_msg[READ_CMD_MSG_LEN] = {
	{
		.flags = 0,
		.len = IST30XX_ADDR_LEN,
		.buf = (u8 *)&pos_cmd,
	},
	{ .flags = I2C_M_RD, },
};

int ist30xx_get_position(struct i2c_client *client, u32 *buf, u16 len)
{
	int ret, i;

	pos_msg[0].addr = client->addr;
	pos_msg[1].addr = client->addr;
	pos_msg[1].len = len * IST30XX_DATA_LEN,
	pos_msg[1].buf = (u8 *)buf,

	ret = i2c_transfer(client->adapter, pos_msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d)\n", __func__, ret);
		return -EIO;
	}

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_be32(buf[i]);

	return 0;
}

int ist30xx_cmd_run_device(struct i2c_client *client, bool is_reset)
{
	int ret = -EIO;

	if (is_reset == true) ist30xx_reset();
	ret = ist30xx_write_cmd(client, CMD_RUN_DEVICE, 0);

	ist30xx_tracking(TRACK_CMD_RUN_DEVICE);

	msleep(10);

	return ret;
}

int ist30xx_cmd_start_scan(struct i2c_client *client)
{
	int ret;

	ret = ist30xx_write_cmd(client, CMD_START_SCAN, 0);

	ist30xx_tracking(TRACK_CMD_SCAN);

	msleep(100);

	ts_data->status.noise_mode = true;

	return ret;
}

int ist30xx_cmd_calibrate(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CALIBRATE, 0);

	ist30xx_tracking(TRACK_CMD_CALIB);

	tsp_info("%s\n", __func__);

	msleep(100);

	return ret;
}

int ist30xx_cmd_check_calib(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CHECK_CALIB, 0);

	ist30xx_tracking(TRACK_CMD_CHECK_CALIB);

	tsp_info("*** Check Calibration cmd ***\n");

	msleep(20);

	return ret;
}

int ist30xx_cmd_update(struct i2c_client *client, int cmd)
{
	u32 val = (cmd == CMD_ENTER_FW_UPDATE ? CMD_FW_UPDATE_MAGIC : 0);
	int ret = ist30xx_write_cmd(client, cmd, val);

	ist30xx_tracking(TRACK_CMD_FWUPDATE);

	msleep(10);

	return ret;
}

int ist30xx_cmd_reg(struct i2c_client *client, int cmd)
{
	int ret = ist30xx_write_cmd(client, cmd, 0);

	if (cmd == CMD_ENTER_REG_ACCESS) {
		ist30xx_tracking(TRACK_CMD_ENTER_REG);
		msleep(100);
	} else if (cmd == CMD_EXIT_REG_ACCESS) {
		ist30xx_tracking(TRACK_CMD_EXIT_REG);
		msleep(10);
	}

	return ret;
}


int ist30xx_read_cmd(struct i2c_client *client, u32 cmd, u32 *buf)
{
	int ret,count=0;
	u32 le_reg = cpu_to_be32(cmd);

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
retry:
	ret = i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		if(count++ < 3)
			goto retry;
		tsp_err("%s: i2c failed (%d), cmd: %x\n", __func__, ret, cmd);
		return -EIO;
	}
	*buf = cpu_to_be32(*buf);

	return 0;
}


int ist30xx_write_cmd(struct i2c_client *client, u32 cmd, u32 val)
{
	int ret,count=0;
	struct i2c_msg msg;
	u8 msg_buf[IST30XX_ADDR_LEN + IST30XX_DATA_LEN];

	put_unaligned_be32(cmd, msg_buf);
	put_unaligned_be32(val, msg_buf + IST30XX_ADDR_LEN);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_ADDR_LEN + IST30XX_DATA_LEN;
	msg.buf = msg_buf;
retry:
	ret = i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN) {
		if(count++ < 3)
			goto retry;
		tsp_err("%s: i2c failed (%d), cmd: %x(%x)\n", __func__, ret, cmd, val);
		return -EIO;
	}

	return 0;
}

static void ts_power_enable(int en)
{


	static struct regulator *ts_vdd = NULL;

	if (ts_vdd == NULL) {
		ts_vdd = regulator_get(NULL, "vddsim2");

		if (IS_ERR(ts_vdd)) {
			pr_err("Get regulator of TSP error!\n");
			return;
		}
	}
	if (en) {
		regulator_set_voltage(ts_vdd, 3000000, 3000000);
		regulator_enable(ts_vdd);
	}
	else if (regulator_is_enabled(ts_vdd)) {
		regulator_disable(ts_vdd);
	}


	printk(KERN_INFO "%s %s\n", __func__, (en) ? "on" : "off");
	printk("%s: TS power change to %d.\n", __func__, en);
}

int ist30xx_power_on(void)
{
	if (ts_data->status.power != 1) {
		tsp_info("%s()\n", __func__);
		/* VDD enable */
		/* VDDIO enable */
		ist30xx_tracking(TRACK_PWR_ON);
		ts_power_enable(1);
		msleep(100);
		ts_data->status.power = 1;
		printk("[ TSP ] %s\n", __func__);
	}

	return 0;
}


int ist30xx_power_off(void)
{
	if (ts_data->status.power != 0) {
		tsp_info("%s()\n", __func__);
		/* VDDIO disable */
		/* VDD disable */
		ist30xx_tracking(TRACK_PWR_OFF);
		ts_power_enable(0);
		msleep(50);
		ts_data->status.power = 0;
		ts_data->status.noise_mode = false;
		printk("[ TSP ] %s\n", __func__);
	}

	return 0;
}


int ist30xx_reset(void)
{
	tsp_info("%s()\n", __func__);
	ist30xx_power_off();
	msleep(10);
	ist30xx_power_on();

	ts_data->status.power = 1;
	return 0;
}


int ist30xx_internal_suspend(struct ist30xx_data *data)
{
	ist30xx_power_off();
	return 0;
}


int ist30xx_internal_resume(struct ist30xx_data *data)
{
	ist30xx_power_on();
	ist30xx_cmd_run_device(data->client, false);
	return 0;
}


int ist30xx_init_system(void)
{
	int ret;

	// TODO : place additional code here.
	ret = ist30xx_power_on();
	if (ret) {
		tsp_err("%s: ist30xx_power_on failed (%d)\n", __func__, ret);
		return -EIO;
	}
#if 0
	ret = ist30xx_reset();
	if (ret) {
		tsp_err("%s: ist30xx_reset failed (%d)\n", __func__, ret);
		return -EIO;
	}
#endif
	return 0;
}
