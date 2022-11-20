/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"


#define SPRDFB_I2C_NAME "sprdfb_i2c"
#define SPRDFB_I2C_TRY_NUM (4)

static struct i2c_client *this_client = NULL;
static struct panel_spec *i2c_panel_info = NULL;

static struct i2c_device_id panel_i2c_id[] = {
	{SPRDFB_I2C_NAME, 0},
	{}
};

static unsigned short panel_i2c_default_addr_list[] =
{ I2C_CLIENT_END, I2C_CLIENT_END };


static int sprdfb_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int res = 0;
	//	struct sprdfb_device *dev = (struct sprdfb_device *)id->driver_data;;

	printk(KERN_INFO "sprdfb: [%s], addr = %d\n", __FUNCTION__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "sprdfb: [%s]: functionality check failed\n",
				__FUNCTION__);
		res = -ENODEV;
		goto out;
	}

	this_client = client;

	if (i2c_panel_info->info.rgb->bus_info.i2c->i2c_addr != (this_client->addr & (~0xFF))) {
		this_client->addr =
			(this_client->addr & (~0xFF)) |
			(i2c_panel_info->info.rgb->bus_info.i2c->i2c_addr & 0xFF);
	}
	printk(KERN_INFO "sordfb: [%s]:this_client->addr =0x%x\n", __FUNCTION__,
			this_client->addr);
	msleep(20);
	return 0;
out:
	return res;
}

static int sprdfb_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int sprdfb_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	printk("SENSOR_DRV: detect!");
	strcpy(info->type, client->name);
	return 0;
}

static struct i2c_driver sprdfb_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
	},
	.probe = sprdfb_i2c_probe,
	.remove = sprdfb_i2c_remove,
	.detect = sprdfb_i2c_detect,
};

bool sprdfb_i2c_init(struct sprdfb_device *dev)
{
	/*	panel_i2c_id[0].driver_data = dev;*/
	i2c_panel_info = dev->panel;
	panel_i2c_default_addr_list[0] = dev->panel->info.rgb->bus_info.i2c->i2c_addr;

	sprdfb_i2c_driver.driver.name = SPRDFB_I2C_NAME;
	sprdfb_i2c_driver.id_table = panel_i2c_id;
	sprdfb_i2c_driver.address_list = &panel_i2c_default_addr_list[0];

	if (i2c_add_driver(&sprdfb_i2c_driver)) {
		printk("sprdfb: [%s]  i2c_add_driver fail\n", __FUNCTION__);
		return false;
	}else{
		printk("sprdfb: [%s]  i2c_add_driver ok\n", __FUNCTION__);
	}

	return true;
}

bool sprdfb_i2c_uninit(struct sprdfb_device *dev)
{
	/*	panel_i2c_id[0].driver_data = dev;*/
	panel_i2c_default_addr_list[0] = dev->panel->info.rgb->bus_info.i2c->i2c_addr;

	sprdfb_i2c_driver.driver.name = SPRDFB_I2C_NAME;
	sprdfb_i2c_driver.id_table = panel_i2c_id;
	sprdfb_i2c_driver.address_list = &panel_i2c_default_addr_list[0];

	i2c_panel_info = NULL;

	i2c_del_driver(&sprdfb_i2c_driver);

	return true;
}

/*write i2c, reg is 8 bit, val is 8 bit*/
static int32_t sprdfb_i2c_write_8bits(uint8_t reg, uint8_t val)
{
	uint8_t buf_w[2];
	uint32_t i = 0;
	int32_t ret = -1;
	struct i2c_msg msg_w;

	if (0xFF == reg) {
		mdelay(val);
		printk("sprdfb: [%s], sleep %d\n", __FUNCTION__,  val);
		return 0;
	}

	buf_w[0] = (uint8_t) reg;
	buf_w[1] = val;
	msg_w.addr = this_client->addr;
	msg_w.flags = 0;
	msg_w.buf = buf_w;
	msg_w.len = 2;
	for (i = 0; i < SPRDFB_I2C_TRY_NUM; i++) {
		ret = i2c_transfer(this_client->adapter, &msg_w, 1);
		if (ret != 1) {
			printk("sprdfb: [%s] write panel reg fai, ret : %d, I2C w addr: 0x%x, \n",
					__FUNCTION__, ret, this_client->addr);
			ret = -1;
			msleep(20);
		}else{
			ret = 0;
			break;
		}
	}
	return ret;
}

/*read i2c, reg is 8 bit, val is 8 bit*/
static int32_t sprdfb_i2c_read_8bits(uint8_t reg, uint8_t *val)
{
	uint8_t buf_w[1];
	uint8_t buf_r;
	uint32_t i = 0;
	int32_t ret = -1;
	struct i2c_msg msg_r[2];

	buf_w[0] = reg;
	msg_r[0].addr = this_client->addr;
	msg_r[0].flags = 0;
	msg_r[0].buf = buf_w;
	msg_r[0].len = 1;
	msg_r[1].addr = this_client->addr;
	msg_r[1].flags = I2C_M_RD;
	msg_r[1].buf = &buf_r;
	msg_r[1].len = 1;
	for (i = 0; i < SPRDFB_I2C_TRY_NUM; i++) {
		ret = i2c_transfer(this_client->adapter, msg_r, 2);
		if (ret != 2) {
			printk("sprdfb: [%s]: read i2c reg fail, ret: %d, I2C r addr: 0x%x \n",
					__FUNCTION__, ret, this_client->addr);
			*val = 0xff;
			ret = -1;
			msleep(20);
		}else{
			*val = buf_r;
			ret = 0;
			break;
		}
	}
	return ret;
}


/*write i2c, reg is 8 or 16 bit, val is 8 or 16bit*/
static int32_t sprdfb_i2c_write_16bits(uint16_t reg, bool reg_is_8bit, uint16_t val, bool val_is_8bit)
{
	uint8_t cmd[4] = { 0 };
	uint32_t i = 0;
	uint32_t cmd_num = 0;
	struct i2c_msg msg_w;
	int32_t ret = -1;

	if ((reg_is_8bit && (0xff == reg))||(!reg_is_8bit &&(0xffff == reg))) {
		mdelay(val);
		printk("sprdfb: [%s], sleep %d\n", __FUNCTION__,  val);
		return 0;
	}

	if (!reg_is_8bit) {
		cmd[cmd_num++] = (uint8_t) ((reg >> 8) & 0xff);
		cmd[cmd_num++] = (uint8_t) (reg & 0xff);
	} else {
		cmd[cmd_num++] = (uint8_t) reg;
	}

	if (!val_is_8bit) {
		cmd[cmd_num++] = (uint8_t) ((val >> 8) & 0xff);
		cmd[cmd_num++] = (uint8_t) (val & 0xff);
	} else {
		cmd[cmd_num++] = (uint8_t) val;
	}

	msg_w.addr = this_client->addr;
	msg_w.flags = 0;
	msg_w.buf = cmd;
	msg_w.len = cmd_num;

	for (i = 0; i < SPRDFB_I2C_TRY_NUM; i++) {
		ret = i2c_transfer(this_client->adapter, &msg_w, 1);
		if (ret != 1) {
			printk("srpdfb [%s]: write sensor reg fai, ret : %d, I2C w addr: 0x%x, \n",
					__FUNCTION__, ret, this_client->addr);
			ret = -1;
			msleep(20);
		} else {
			ret = 0;
			break;
		}
	}
	return ret;
}

/*read i2c, reg is 8 or 16 bit, val is 8 or 16bit*/
static int32_t sprdfb_i2c_read_16bits(uint16_t reg, bool reg_is_8bit, uint16_t *val, bool val_is_8bit)
{
	uint32_t i = 0;
	uint8_t cmd[2] = { 0 };
	uint16_t w_cmd_num = 0;
	uint16_t r_cmd_num = 0;
	uint8_t buf_r[2] = { 0 };
	int32_t ret = -1;
	struct i2c_msg msg_r[2];

	if (!reg_is_8bit) {
		cmd[w_cmd_num++] = (uint8_t) ((reg >> 8) & 0xff);
		cmd[w_cmd_num++] = (uint8_t) (reg & 0xff);
	} else {
		cmd[w_cmd_num++] = (uint8_t) reg;
	}

	if (!val_is_8bit) {
		r_cmd_num = 2;
	} else {
		r_cmd_num = 1;
	}

	msg_r[0].addr = this_client->addr;
	msg_r[0].flags = 0;
	msg_r[0].buf = cmd;
	msg_r[0].len = w_cmd_num;
	msg_r[1].addr = this_client->addr;
	msg_r[1].flags = I2C_M_RD;
	msg_r[1].buf = buf_r;
	msg_r[1].len = r_cmd_num;

	for (i = 0; i < SPRDFB_I2C_TRY_NUM; i++) {
		ret = i2c_transfer(this_client->adapter, msg_r, 2);
		if (ret != 2) {
			printk("srpdfb [%s]: read panel reg fai, ret : %d, I2C w addr: 0x%x, \n",
					__FUNCTION__,ret, this_client->addr);
			*val = 0xffff;
			ret = -1;
			msleep(20);
		} else {
			*val = (r_cmd_num == 1) ? (uint16_t) buf_r[0]
				: (uint16_t) ((buf_r[0] << 8) + buf_r[1]);
			ret = 0;
			break;
		}
	}
	return ret;
}


/*write i2c with burst mode*/
static int32_t sprdfb_i2c_write_burst(uint8_t* buf, int num)
{
	uint32_t i = 0;
	struct i2c_msg msg_w;
	int32_t ret = -1;

	for (i = 0; i < SPRDFB_I2C_TRY_NUM; i++) {
		msg_w.addr = this_client->addr;
		msg_w.flags = 0;
		msg_w.buf = buf;
		msg_w.len = num;
		ret = i2c_transfer(this_client->adapter, &msg_w, 1);
		if (ret != 1) {
			printk("srpdfb [%s]: write i2c reg fai, ret : %d, I2C w addr: 0x%x, \n",
					__FUNCTION__, ret, this_client->addr);
			ret = -1;
			msleep(20);
		} else {
			ret = 0;
			break;
		}
	}

	return ret;
}

struct ops_i2c sprdfb_i2c_ops = {
	.i2c_write_8bits = sprdfb_i2c_write_8bits,
	.i2c_read_8bits = sprdfb_i2c_read_8bits,
	.i2c_write_16bits = sprdfb_i2c_write_16bits,
	.i2c_read_16bits = sprdfb_i2c_read_16bits,
	.i2c_write_burst = sprdfb_i2c_write_burst,
};
