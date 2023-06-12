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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <asm/unaligned.h>
#include <linux/stat.h>
#include <linux/export.h>

#include <linux/input/ist30xx.h>
#include "ist30xx_update.h"

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif

#if IST30XX_INTERNAL_BIN
#include "ist30xx_fw.h"

const u8 *ts_fw = ist30xx_fw;
const u8 *ts_param = ist30xx_param;
u32 ts_fw_size = sizeof(ist30xx_fw);
u32 ts_param_size = sizeof(ist30xx_param);
#endif // IST30XX_INTERNAL_BIN

u32 ist30xx_fw_ver = 0;
u32 ist30xx_param_ver = 0;

extern struct ist30xx_data *ts_data;

extern void ist30xx_disable_irq(struct ist30xx_data *data);
extern void ist30xx_enable_irq(struct ist30xx_data *data);


int ist30xx_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			 int msg_num, u8 *cmd_buf)
{
	int ret = 0;
	int idx = msg_num - 1;
	int size = msgs[idx].len;
	u8 *msg_buf = NULL;
	u8 *pbuf = NULL;
	int trans_size, max_size = 0;

	if (msg_num == WRITE_CMD_MSG_LEN)
		max_size = I2C_MAX_WRITE_SIZE;
	else if (msg_num == READ_CMD_MSG_LEN)
		max_size = I2C_MAX_READ_SIZE;

	if (max_size == 0) {
		tsp_err("%s() : transaction size(%d)\n", __func__, max_size);
		return -EINVAL;
	}

	if (msg_num == WRITE_CMD_MSG_LEN) {
		msg_buf = kmalloc(max_size + IST30XX_ADDR_LEN, GFP_KERNEL);
		if (!msg_buf)
			return -ENOMEM;
		memcpy(msg_buf, cmd_buf, IST30XX_ADDR_LEN);
		pbuf = msgs[idx].buf;
	}

	while (size > 0) {
		trans_size = (size >= max_size ? max_size : size);

		msgs[idx].len = trans_size;
		if (msg_num == WRITE_CMD_MSG_LEN) {
			memcpy(&msg_buf[IST30XX_ADDR_LEN], pbuf, trans_size);
			msgs[idx].buf = msg_buf;
			msgs[idx].len += IST30XX_ADDR_LEN;
		}
		ret = i2c_transfer(adap, msgs, msg_num);
		if (ret != msg_num) {
			tsp_err("%s() : i2c_transfer failed(%d), num=%d\n",
				__func__, ret, msg_num);
			break;
		}

		if (msg_num == WRITE_CMD_MSG_LEN)
			pbuf += trans_size;
		else
			msgs[idx].buf += trans_size;

		size -= trans_size;
	}

	if (msg_num == WRITE_CMD_MSG_LEN)
		kfree(msg_buf);

	return ret;
}

int ist30xx_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	int ret, i;
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
			.len = len * IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
		},
	};

	ret = ist30xx_i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN, NULL);
	if (ret != READ_CMD_MSG_LEN)
		return -EIO;

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_be32(buf[i]);

	return 0;
}

int ist30xx_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	int i;
	int ret;
	struct i2c_msg msg;
	u8 cmd_buf[IST30XX_ADDR_LEN];
	u8 msg_buf[IST30XX_DATA_LEN * (len + 1)];

	put_unaligned_be32(cmd, cmd_buf);

	if (len > 0) {
		for (i = 0; i < len; i++)
			put_unaligned_be32(buf[i], msg_buf + (i * IST30XX_DATA_LEN));
	} else {
		/* then add dummy data(4byte) */
		put_unaligned_be32(0, msg_buf);
		len = 1;
	}

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_DATA_LEN * len;
	msg.buf = msg_buf;

	ret = ist30xx_i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN, cmd_buf);
	if (ret != WRITE_CMD_MSG_LEN)
		return -EIO;

	return 0;
}


/* ist30xx isp function */
#define CMD_ENTER_ISP_MODE          (0xE1)
#define CMD_EXIT_ISP_MODE           (0xE0)
#define CMD_ENTER_XUM_MODE          (0x41)
#define CMD_EXIT_XUM_MODE           (0x40)

#define CMD_ISP_START_READ          (0x11)
#define CMD_ISP_END_READ            (0x10)
#define CMD_ISP_ERASE_ALL_PAGE      (0x8F)
#define CMD_ISP_PROGRAM_PAGE        (0x91)

static void prepare_msg_buffer(u8 *buf, u8 cmd, u16 addr)
{
	memset(buf, 0, sizeof(buf));

	buf[0] = cmd;
	buf[1] = (u8)((addr >> 8) & 0xFF);
	buf[2] = (u8)(addr & 0xFF);
}

int ist30xx_isp_read_buf(struct i2c_client *client, u16 addr, u32 *buf, u16 len)
{
	int ret;
	u8 msg_buf[3]; /* Command + Dummy 16bit addr */

	struct i2c_msg msgs[READ_CMD_MSG_LEN] = {
		{
			.addr = IST30XX_FW_DEV_ID,
			.flags = I2C_M_IGNORE_NAK,
			.len = IST30XX_ISP_CMD_LEN,
			.buf = NULL,
		},
		{
			.addr = IST30XX_FW_DEV_ID,
			.flags = I2C_M_RD | I2C_M_IGNORE_NAK,
			.len = len * IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
		},
	};

	prepare_msg_buffer(msg_buf, CMD_ISP_START_READ, addr);
	msgs[0].buf = (u8 *)msg_buf;

	ret = i2c_transfer(client->adapter, &msgs[0], WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN)
		return -EIO;

	ret = i2c_transfer(client->adapter, &msgs[1], 1);
	if (ret != 1)
		return -EIO;

	prepare_msg_buffer(msg_buf, CMD_ISP_END_READ, addr);
	msgs[0].buf = (u8 *)msg_buf;
	ret = i2c_transfer(client->adapter, &msgs[0], WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN)
		return -EIO;

	return 0;
}

int ist30xx_isp_write_buf(struct i2c_client *client, u32 addr, u32 *buf, u16 len)
{
	int ret;
	struct i2c_msg msg;
	u8 msg_buf[EEPROM_PAGE_SIZE + IST30XX_ISP_CMD_LEN];

	prepare_msg_buffer(msg_buf, CMD_ISP_PROGRAM_PAGE, addr);
	memcpy((msg_buf + IST30XX_ISP_CMD_LEN), buf, (len * IST30XX_DATA_LEN));

	msg.addr = IST30XX_FW_DEV_ID;
	msg.flags = I2C_M_IGNORE_NAK;
	msg.len = (len * IST30XX_DATA_LEN) + IST30XX_ISP_CMD_LEN;
	msg.buf = msg_buf;

	ret = i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN)
		return -EIO;

	return 0;
}

int ist30xx_isp_cmd(struct i2c_client *client, const u8 cmd)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = IST30XX_FW_DEV_ID;
	msg.flags = I2C_M_IGNORE_NAK;
	msg.len = 1;
	msg.buf = (u8 *)&cmd;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		return -EIO;

	/* need 2.5msec after this cmd */
	msleep(3);

	return 0;
}


int ist30xx_isp_erase_all(struct i2c_client *client)
{
	int ret;
	struct i2c_msg msg;

	/* EEPROM pase size + Command + Dummy 16bit addr */
	u8 msg_buf[EEPROM_PAGE_SIZE + 3];

	prepare_msg_buffer(msg_buf, CMD_ISP_ERASE_ALL_PAGE, EEPROM_BASE_ADDR);

	msg.addr = IST30XX_FW_DEV_ID;
	msg.flags = I2C_M_IGNORE_NAK;
	msg.len = EEPROM_PAGE_SIZE + 3;
	msg.buf = (u8 *)msg_buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1)
		return -EIO;

	/* need 2.5msec after erasing eeprom */
	msleep(3);

	return 0;
}


int ist30xx_isp_fw_read(struct i2c_client *client, u32 *buf32)
{
	int ret = 0;
	int i, j;

	u16 addr = EEPROM_BASE_ADDR;

	ist30xx_reset();

	ret = ist30xx_isp_cmd(client, CMD_ENTER_ISP_MODE);
	if (ret) {
		tsp_err("ISP cmd fail, CMD_ENTER_ISP_MODE\n");
		return ret;
	}

	for (i = 0; i < IST30XX_EEPROM_SIZE; i += EEPROM_PAGE_SIZE) {
		ret = ist30xx_isp_read_buf(client, addr, buf32,
					   EEPROM_PAGE_SIZE / IST30XX_DATA_LEN);
		if (ret) {
			tsp_err("ISP cmd fail, CMD_ISP_START_READ\n");
			return ret;
		}

		for (j = 0; j < EEPROM_PAGE_SIZE / IST30XX_DATA_LEN; j += 4)
			printk("[ TSP ] %04x: %08x %08x %08x %08x\n",
			       i + j * IST30XX_DATA_LEN,
			       buf32[j], buf32[j + 1], buf32[j + 2], buf32[j + 3]);

		addr += EEPROM_PAGE_SIZE;
	}

	ret = ist30xx_isp_cmd(client, CMD_EXIT_ISP_MODE);
	if (ret)
		return -EIO;


	return 0;
}


int ist30xx_isp_bootloader(struct i2c_client *client, const u8 *buf)
{
	int ret = 0;
	int i;
	u16 addr = EEPROM_BASE_ADDR;
	u32 page_cnt = (ts_data->fw.index) / EEPROM_PAGE_SIZE;
	int buf_cnt = EEPROM_PAGE_SIZE / IST30XX_DATA_LEN;

	u32 *src_buf32 = (u32 *)buf;

	tsp_info("*** Bootloader update (0x%04x~0x%04x) ***\n",
		 EEPROM_BASE_ADDR, ts_data->fw.index);

	ist30xx_reset();

	ret = ist30xx_isp_cmd(client, CMD_ENTER_ISP_MODE);
	if (ret) {
		tsp_err("ISP cmd fail, CMD_ENTER_ISP_MODE\n");
		goto recv_end;
	}

	ret = ist30xx_isp_erase_all(client);
	if (ret) {
		tsp_err("ISP cmd fail, CMD_ISP_ERASE_ALL_PAGE\n");
		goto recv_end;
	}

	for (i = 0; i < page_cnt; i++) {
		ret = ist30xx_isp_write_buf(client, addr, src_buf32, buf_cnt);
		if (ret) {
			tsp_err("ISP cmd fail, CMD_ISP_PROGRAM_PAGE\n");
			goto recv_end;
		}
		msleep(2);

		addr += EEPROM_PAGE_SIZE;
		src_buf32 += buf_cnt;
	}

recv_end:
	if (!ret)
		ret = ist30xx_isp_cmd(client, CMD_EXIT_ISP_MODE);

	return ret;
}

int ist30xx_isp_fw_update(struct i2c_client *client, const u8 *buf)
{
	int ret = 0;
	int i;
	u32 page_cnt = IST30XX_EEPROM_SIZE / EEPROM_PAGE_SIZE;
	u16 addr = EEPROM_BASE_ADDR;

	ret = ist30xx_isp_cmd(client, CMD_ENTER_ISP_MODE);
	if (ret) {
		tsp_err("ISP cmd fail, CMD_ENTER_ISP_MODE\n");
		return ret;
	}

	ret = ist30xx_isp_erase_all(client);
	if (ret) {
		tsp_err("ISP cmd fail, CMD_ISP_ERASE_ALL_PAGE\n");
		return ret;
	}

	for (i = 0; i < page_cnt; i++) {
		ret = ist30xx_isp_write_buf(client, addr,
					    (u32 *)buf, EEPROM_PAGE_SIZE / IST30XX_DATA_LEN);
		if (ret) {
			tsp_err("ISP cmd fail, CMD_ISP_PROGRAM_PAGE\n");
			return ret;
		}

		addr += EEPROM_PAGE_SIZE;
		buf += EEPROM_PAGE_SIZE;

		msleep(2);
	}

	ret = ist30xx_isp_cmd(client, CMD_EXIT_ISP_MODE);
	if (ret)
		return -EIO;

	return 0;
}


int ist30xx_fw_write(struct i2c_client *client, const u8 *buf)
{
	int ret;
	int len;
	u32 *buf32 = (u32 *)(buf + ts_data->fw.index);
	u32 size = ts_data->fw.size;

	if (size < 0 || size > IST30XX_EEPROM_SIZE)
		return -ENOEXEC;

	while (size > 0) {
		len = (size >= EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : size);

		ret = ist30xx_write_buf(client, CMD_ENTER_FW_UPDATE, buf32, (len >> 2));
		if (ret)
			return ret;

		buf32 += (len >> 2);
		size -= len;

		msleep(5);
	}
	return 0;
}


u32 ist30xx_parse_fw_ver(int flag, const u8 *buf)
{
	u32 ver;
	u32 *buf32 = (u32 *)buf;

	if (flag == PARSE_FLAG_FW)
		ver = (u32)buf32[(ts_data->tags.flag_addr + 4) >> 2];
	else if (flag == PARSE_FLAG_PARAM)
		ver = (u32)buf32[0];
	else
		ver = 0;

	return ver;
}


u32 ist30xx_parse_param_ver(int flag, const u8 *buf)
{
	u32 ver;
	u32 *buf32 = (u32 *)buf;

	if (flag == PARSE_FLAG_FW)
		ver = (u32)buf32[(ts_data->tags.cfg_addr + 4) >> 2];
	else if (flag == PARSE_FLAG_PARAM)
		ver = (u32)buf32[4 >> 2];
	else
		ver = 0;

	return ver & 0xFFFF;
}


int calib_ms_delay = WAIT_CALIB_CNT;
int ist30xx_calib_wait(void)
{
	int cnt = calib_ms_delay;

	ts_data->status.calib_msg = 0;
	while (cnt-- > 0) {
		msleep(100);

		if (ts_data->status.calib_msg) {
			tsp_info("Calibration status : %d, Max raw gap : %d - (%08x)\n",
				 CALIB_TO_STATUS(ts_data->status.calib_msg),
				 CALIB_TO_GAP(ts_data->status.calib_msg),
				 ts_data->status.calib_msg);

			if (CALIB_TO_GAP(ts_data->status.calib_msg) < CALIB_THRESHOLD)
				return 0;  // Calibrate success
			else
				return -EAGAIN;
		}
	}
	tsp_warn("Calibration time out\n");

	return -EPERM;
}

int ist30xx_calibrate(int wait_cnt)
{
	int ret = -ENOEXEC;

	tsp_info("*** Calibrate %ds ***\n", calib_ms_delay / 10);

	ts_data->status.update = 1;
	while (wait_cnt--) {
		ist30xx_disable_irq(ts_data);
		ret = ist30xx_cmd_run_device(ts_data->client);
		if (ret) continue;

		ret = ist30xx_cmd_calibrate(ts_data->client);
		if (ret) continue;

		ist30xx_enable_irq(ts_data);
		ret = ist30xx_calib_wait();
		if (!ret) break;
	}

	ist30xx_disable_irq(ts_data);
	ist30xx_cmd_run_device(ts_data->client);

	ts_data->status.update = 2;

	ist30xx_enable_irq(ts_data);

	return ret;
}


u32 ist30xx_make_checksum(int mode, const u8 *buf)
{
	int size = 0;
	u32 chksum = 0;
	u32 *buf32 = (u32 *)(buf + ts_data->fw.index);

	if (mode == CHKSUM_ALL)
		size = ts_data->fw.size + ts_data->tags.cfg_size +
		       ts_data->tags.sensor1_size + ts_data->tags.sensor2_size;
	else if (mode == CHKSUM_FW)
		size = ts_data->fw.size - ts_data->tags.flag_size;
	else
		return 0;

	if (size < 0 || size > IST30XX_EEPROM_SIZE)
		return 0;

	size >>= 2;
	for (; size > 0; size--)
		chksum += *buf32++;

	return chksum;
}


struct ist30xx_tags *ts_tags;
int ist30xx_parse_tags(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret = -1;

	ts_tags = (struct ist30xx_tags *)(&buf[size - sizeof(struct ist30xx_tags)]);

	if (!strncmp(ts_tags->magic1, IST30XX_TAG_MAGIC, sizeof(ts_tags->magic1))
	    && !strncmp(ts_tags->magic2, IST30XX_TAG_MAGIC, sizeof(ts_tags->magic2))) {
		data->fw.index = ts_tags->fw_addr;
		data->fw.size = ts_tags->flag_addr - ts_tags->fw_addr +
				ts_tags->flag_size;
		data->fw.chksum = ts_tags->chksum;
		data->tags = *ts_tags;

		ret = 0;
	}

	return ret;
}

void ist30xx_get_update_info(struct ist30xx_data *data, const u8 *buf, const u32 size)
{
	int ret;

	ret = ist30xx_parse_tags(data, buf, size);
	if (ret != TAGS_PARSE_OK) {
		tsp_warn("Cannot find tags of F/W, make a tags by 'tagts.exe'\n");
		if (size >= IST30XX_EEPROM_SIZE) { // Firmware update
			data->fw.index = IST30XX_FW_START_ADDR;
			data->fw.size = IST30XX_FW_SIZE;
			data->tags.flag_addr = IST30XX_FW_END_ADDR - IST30XX_FLAG_SIZE;
			data->tags.flag_size = IST30XX_FLAG_SIZE;
			data->fw.chksum = ist30xx_make_checksum(CHKSUM_FW, buf);
		} else {                    // Parameters update
			data->tags.cfg_size = IST30XX_CONFIG_SIZE;
			data->tags.sensor1_size = IST30XX_SENSOR1_SIZE;
			data->tags.sensor2_size = IST30XX_SENSOR2_SIZE;
		}
	}
}

#if (IST30XX_DEBUG) && (IST30XX_INTERNAL_BIN)
extern TSP_INFO ist30xx_tsp_info;
extern TKEY_INFO ist30xx_tkey_info;
int ist30xx_get_tkey_info(void)
{
	int ret = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	TKEY_INFO *tkey = &ist30xx_tkey_info;
	u8 *cfg_buf;

	ist30xx_get_update_info(ts_data, ts_fw, ts_fw_size);
	cfg_buf = (u8 *)&ts_fw[ts_data->tags.cfg_addr];

	tkey->enable = (bool)cfg_buf[0x14];
	tkey->key_num = (u8)cfg_buf[0x15];
	tkey->ch_num[0] = (u8)cfg_buf[0x16];
	tkey->ch_num[1] = (u8)cfg_buf[0x17];
	tkey->ch_num[2] = (u8)cfg_buf[0x18];
	tkey->ch_num[3] = (u8)cfg_buf[0x19];
	tkey->ch_num[4] = (u8)cfg_buf[0x1A];
	tkey->axis_chnum = (u8)cfg_buf[0x1C];

	if (tsp->dir.txch_y)
        tkey->tx_line = ((cfg_buf[0x1B]&1) ? false : true);
    else
        tkey->tx_line = ((cfg_buf[0x1B]&1) ? true : false);

	return ret;
}

#define TSP_INFO_SWAP_XY    (1 << 0)
#define TSP_INFO_FLIP_X     (1 << 1)
#define TSP_INFO_FLIP_Y     (1 << 2)
int ist30xx_get_tsp_info(void)
{
	int ret = 0;
	TSP_INFO *tsp = &ist30xx_tsp_info;
	u8 *cfg_buf, *sensor_buf;

	ist30xx_get_update_info(ts_data, ts_fw, ts_fw_size);
	cfg_buf = (u8 *)&ts_fw[ts_data->tags.cfg_addr];
	sensor_buf = (u8 *)&ts_fw[ts_data->tags.sensor1_addr];

	tsp->finger_num = (u8)cfg_buf[0x0E];

	tsp->intl.x = (u8)cfg_buf[0x10];
	tsp->intl.y = (u8)cfg_buf[0x11];
	tsp->mod.x = (u8)cfg_buf[0x12];
	tsp->mod.y = (u8)cfg_buf[0x13];

	tsp->dir.txch_y = (bool)(sensor_buf[0x78] & 7 ?  true : false);
	tsp->dir.swap_xy = (bool)(cfg_buf[0xF] & TSP_INFO_SWAP_XY ? true : false);
	tsp->dir.flip_x = (bool)(cfg_buf[0xF] & TSP_INFO_FLIP_X ? true : false);
	tsp->dir.flip_y = (bool)(cfg_buf[0xF] & TSP_INFO_FLIP_Y ? true : false);

	tsp->buf.int_len = tsp->intl.x * tsp->intl.y;
	tsp->buf.mod_len = tsp->mod.x * tsp->mod.y;
	tsp->height = (tsp->dir.swap_xy ? tsp->mod.x : tsp->mod.y);
	tsp->width = (tsp->dir.swap_xy ? tsp->mod.y : tsp->mod.x);

	return ret;
}
#endif // (IST30XX_DEBUG) && (IST30XX_INTERNAL_BIN)


#define update_next_step(ret)   { if (ret) goto end; }
int ist30xx_fw_update(struct i2c_client *client, const u8 *buf, int size, bool mode)
{
	int ret = 0;
	u32 ver, chksum;

	tsp_info("*** F/W update ***\n");
	tsp_info("F/W ver: %08x, addr: 0x%04x ~ 0x%04x\n", ist30xx_fw_ver,
		 ts_data->fw.index, ts_data->fw.index + ts_data->fw.size);

	ts_data->status.update = 1;

	ist30xx_disable_irq(ts_data);

	ist30xx_reset();

	if (mode) {     /* ISP Mode */
		ret = ist30xx_isp_fw_update(client, buf);
		update_next_step(ret);
	} else {        /* I2C Mode */
		ret = ist30xx_cmd_update(client, CMD_ENTER_FW_UPDATE);
		update_next_step(ret);

		ret = ist30xx_fw_write(client, buf);
		update_next_step(ret);
	}
	msleep(50);

	buf += IST30XX_EEPROM_SIZE;
	size -= IST30XX_EEPROM_SIZE;

	ret = ist30xx_cmd_run_device(client);
	update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_CHECKSUM, &chksum);
	if ((ret) || (chksum != ts_data->fw.chksum))
		goto end;

	ret = ist30xx_read_cmd(client, CMD_GET_FW_VER, &ver);
	update_next_step(ret);
	ts_data->fw.ver = ver;

	ret = ist30xx_read_cmd(client, CMD_GET_PARAM_VER, &ver);
	update_next_step(ret);
	ts_data->param_ver = ver;

	tsp_info("F/W version core: %08x, param: %08x\n",
		 ts_data->fw.ver, ts_data->param_ver);

end:
	if (ret) {
		tsp_warn("F/W update Fail!, ret=%d", ret);
	} else if (chksum != ts_data->fw.chksum) {
		tsp_warn("Error CheckSum: %08x(%08x)\n", chksum, ts_data->fw.chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(ts_data);

	ts_data->status.update = 2;

	return ret;
}


int ist30xx_param_update(struct i2c_client *client, const u8 *buf, int size)
{
	int ret;
	u32 val, ver;
	u16 len;
	u32 *param = (u32 *)buf;

	ts_data->status.update = 1;

	tsp_info("*** Parameters update ***\n");

	ist30xx_disable_irq(ts_data);

	ret = ist30xx_cmd_run_device(client);
	update_next_step(ret);

	/* Compare F/W version */
	ret = ist30xx_read_cmd(client, CMD_GET_FW_VER, &ver);
	update_next_step(ret);

	val = ist30xx_parse_fw_ver(PARSE_FLAG_PARAM, buf);
	if ((val & MASK_FW_VER) != (ver & MASK_FW_VER)) {
		tsp_warn("Fail, F/W ver check. 0x%04x -> 0x%04x\n", ver, val);
		goto end;
	}
	ist30xx_fw_ver = val;

	ret = ist30xx_cmd_update(client, CMD_ENTER_UPDATE);
	update_next_step(ret);

	/* update config */
	len = (ts_data->tags.cfg_size >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_CONFIG, param, len);
	update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	/* update sensor */
	len = ((ts_data->tags.sensor1_size + ts_data->tags.sensor2_size) >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_SENSOR, param, len);
	update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	ret = ist30xx_cmd_update(client, CMD_EXIT_UPDATE);
	update_next_step(ret);
	msleep(120);

	ret = ist30xx_cmd_run_device(client);
	update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_PARAM_VER, &val);
	update_next_step(ret);
	ts_data->param_ver = val;

	tsp_info("F/W version core: %08x, param: %08x\n",
		 ts_data->fw.ver, ts_data->param_ver);

end:
	if (size < 0)
		tsp_warn("Param update_fail, size=%d\n", size);
	else if (ret)
		tsp_warn("Param update_fail, ret=%d\n", ret);

	ist30xx_enable_irq(ts_data);

	ts_data->status.update = 2;

	return ret;
}

#define fw_update_next_step(ret)   { if (ret) { step = 1; goto end; } }
#define param_update_next_step(ret)   { if (ret) { step = 2; goto end; } }
int ist30xx_fw_param_update(struct i2c_client *client, const u8 *buf)
{
	int ret = 0;
	int step = 0;
	u16 len = 0;
	int size = IST30XX_EEPROM_SIZE;
	u32 chksum = 0, ver = 0;
	u32 *param = (u32 *)buf;

	tsp_info("*** F/W param update ***\n");
	tsp_info("F/W ver: %08x, addr: 0x%04x ~ 0x%04x\n", ist30xx_fw_ver,
		 ts_data->fw.index, ts_data->fw.index + ts_data->fw.size);

	ts_data->status.update = 1;

	ist30xx_disable_irq(ts_data);
	ist30xx_reset();

	/* I2C F/W update */
	ret = ist30xx_cmd_update(client, CMD_ENTER_FW_UPDATE);
	fw_update_next_step(ret);

	ret = ist30xx_fw_write(client, buf);
	fw_update_next_step(ret);

	msleep(50);

	param = (u32 *)(buf + (ts_data->fw.index + ts_data->fw.size));
	size -= ts_data->fw.size;

	ret = ist30xx_cmd_run_device(client);
	fw_update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_CHECKSUM, &chksum);
	if ((ret) || (chksum != ts_data->fw.chksum))
		goto end;

	ret = ist30xx_read_cmd(client, CMD_GET_FW_VER, &ver);
	update_next_step(ret);
	ts_data->fw.ver = ver;
	tsp_info("F/W core version : %08x\n", ts_data->fw.ver);

	/* update parameters */
	ret = ist30xx_cmd_update(client, CMD_ENTER_UPDATE);
	param_update_next_step(ret);

	/* config */
	len = (ts_data->tags.cfg_size >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_CONFIG, param, len);
	param_update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	/* sensor */
	len = ((ts_data->tags.sensor1_size + ts_data->tags.sensor2_size) >> 2);
	ret = ist30xx_write_buf(client, CMD_UPDATE_SENSOR, param, len);
	param_update_next_step(ret);
	msleep(10);

	param += len;
	size -= (len << 2);

	ret = ist30xx_cmd_update(client, CMD_EXIT_UPDATE);
	param_update_next_step(ret);
	msleep(120);

	ret = ist30xx_cmd_run_device(client);
	update_next_step(ret);

	ret = ist30xx_read_cmd(client, CMD_GET_PARAM_VER, &ver);
	update_next_step(ret);
	ts_data->param_ver = ver;
	tsp_info("Param version : %08x\n", ts_data->param_ver);

end:
	if (ret) {
		if (step == 1)
			tsp_warn("Firmware update Fail!, ret=%d", ret);
		else if (step == 2)
			tsp_warn("Parameters update Fail!, ret=%d", ret);
	} else if (chksum != ts_data->fw.chksum) {
		tsp_warn("Error CheckSum: %08x(%08x)\n", chksum, ts_data->fw.chksum);
		ret = -ENOEXEC;
	}

	ist30xx_enable_irq(ts_data);
	ts_data->status.update = 2;

	return ret;
}


#if IST30XX_INTERNAL_BIN
int ist30xx_auto_fw_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_FW_UPDATE_RETRY;
	bool isp_mode = (IST30XX_ISP_MODE ? true : false);

	ist30xx_get_update_info(data, ts_fw, ts_fw_size);
	if ((data->fw.ver > 0) && (data->fw.ver < 0xFFFFFFFF)) {
		ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, ts_fw);
		if (data->fw.ver >= ist30xx_fw_ver)
			return 0;
	}

	tsp_info("New F/W detected. 0x%x -> 0x%x\n", data->fw.ver, ist30xx_fw_ver);

	mutex_lock(&ist30xx_mutex);
	while (retry--) {
		ret = ist30xx_fw_update(data->client, ts_fw, ts_fw_size,
					isp_mode);
		if (!ret)
			break;
	}
	mutex_unlock(&ist30xx_mutex);

	ist30xx_calibrate(IST30XX_FW_UPDATE_RETRY);

	return ret;
}


int ist30xx_auto_param_update(struct ist30xx_data *data)
{
	int ret = 0;
	u32 val = 0;
	int retry = IST30XX_FW_UPDATE_RETRY;

	ist30xx_get_update_info(data, ts_fw, ts_fw_size);
	if ((data->param_ver > 0) && (data->param_ver < 0xFFFFFFFF)) {
		val = ist30xx_parse_param_ver(PARSE_FLAG_PARAM, ts_param);
		if (data->param_ver >= val)
			return 0;
	}

	tsp_info("New Param detected. 0x%04x -> 0x%04x\n", data->param_ver, val);

	mutex_lock(&ist30xx_mutex);
	while (retry--) {
		ret = ist30xx_param_update(data->client, ts_param, ts_param_size);
		if (!ret) break;
	}
	mutex_unlock(&ist30xx_mutex);

	ist30xx_calibrate(IST30XX_FW_UPDATE_RETRY);

	return ret;
}


int ist30xx_check_fw(struct ist30xx_data *data, const u8 *buf)
{
	int ret;
	u32 chksum, chksum_bin;

	chksum_bin = ist30xx_make_checksum(CHKSUM_FW, buf);
	ret = ist30xx_read_cmd(data->client, CMD_GET_CHECKSUM, &chksum);
	if (ret) return ret;

	if (chksum != chksum_bin) {
		tsp_warn("Checksum compare error %08x(%08x)\n", chksum, chksum_bin);
		return -EPERM;
	}

	return 0;
}

int ist30xx_check_auto_update(struct ist30xx_data *data)
{
	ist30xx_write_cmd(data->client, CMD_RUN_DEVICE, 0);
	msleep(10);

	ist30xx_get_ver_info(data);

	ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, ts_fw);
	ist30xx_param_ver = ist30xx_parse_param_ver(PARSE_FLAG_FW, ts_fw);
	/*
	 *  data->fw.ver : FW core version of TSP IC
	 *  data->param_ver : FW version of TSP IC
	 *  ist30xx_fw_ver : FW core version of FW Binary
	 *  ist30xx_fw_ver : FW version of FW Binary
	 */

	/* If the binary version is higher than IC version, FW update operate. */
	if ((data->param_ver > 0) && (data->param_ver < 0xFFFFFFFF) &&
	    (data->fw.ver > 0) && (data->fw.ver < 0xFFFFFFFF)) {
		tsp_info("FW: %08x(%08x), Param: %04x(%04x)\n",
			 data->fw.ver, ist30xx_fw_ver, data->param_ver, ist30xx_param_ver);

		if ((data->fw.ver >= ist30xx_fw_ver) &&
		    (data->param_ver >= ist30xx_param_ver))
			return 0;
	}

	return -EAGAIN;
}

int ist30xx_auto_bin_update(struct ist30xx_data *data)
{
	int ret = 0;
	int retry = IST30XX_FW_UPDATE_RETRY;
	bool isp_mode = (IST30XX_ISP_MODE ? true : false);

	ist30xx_get_update_info(data, ts_fw, ts_fw_size);
	ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, ts_fw);
	ist30xx_param_ver = ist30xx_parse_param_ver(PARSE_FLAG_FW, ts_fw);

	tsp_info("bin fw: %x, param: %x\n", ist30xx_fw_ver, ist30xx_param_ver);

	mutex_lock(&ist30xx_mutex);
	ret = ist30xx_check_auto_update(data);
	mutex_unlock(&ist30xx_mutex);
	if (ret >= 0)
		return ret;

update_bin:   // TSP is not ready / FW update
	tsp_info("Update version FW: %x -> %x, Param: %x -> %x\n",
		 data->fw.ver, ist30xx_fw_ver, data->param_ver, ist30xx_param_ver);

	mutex_lock(&ist30xx_mutex);
	while (retry--) {
		if (isp_mode)
			ret = ist30xx_fw_update(data->client, ts_fw, ts_fw_size, true);
		else
			ret = ist30xx_fw_param_update(data->client, ts_fw);
		if (!ret)
			break;
	}
	mutex_unlock(&ist30xx_mutex);

	if (ret)
		return ret;

	if (retry > 0 && ist30xx_check_fw(data, ts_fw))
		goto update_bin;

	ist30xx_calibrate(IST30XX_FW_UPDATE_RETRY);

	return ret;
}
#endif // IST30XX_INTERNAL_BIN


/* sysfs: /sys/class/touch/firmware/boot */
ssize_t ist30xx_boot_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XX_FW_NAME, &ts_data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XX_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
	} else {
#if IST30XX_INTERNAL_BIN
		fw = (u8 *)ts_fw;
		fw_size = ts_fw_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	if (mode == 3) {
		tsp_info("EEPROM all erase test\n");
		ist30xx_isp_cmd(ts_data->client, CMD_ENTER_ISP_MODE);
		ist30xx_isp_erase_all(ts_data->client);
		ist30xx_isp_cmd(ts_data->client, CMD_EXIT_ISP_MODE);
	} else {
		ist30xx_get_update_info(ts_data, fw, fw_size);
		ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, fw);

		ist30xx_disable_irq(ts_data);

		mutex_lock(&ist30xx_mutex);
		ist30xx_isp_bootloader(ts_data->client, fw);
		mutex_unlock(&ist30xx_mutex);

		if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW)
			release_firmware(request_fw);
	}

	return size;
}

ssize_t ist30xx_fw_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	u32 buf32[EEPROM_PAGE_SIZE / IST30XX_DATA_LEN];

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(ts_data);

	ist30xx_isp_fw_read(ts_data->client, buf32);

	ist30xx_enable_irq(ts_data);
	mutex_unlock(&ist30xx_mutex);

	return 0;
}

/* sysfs: /sys/class/touch/firmware/firmware */
ssize_t ist30xx_fw_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	int ret;
	bool isp_mode = false;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XX_FW_NAME, &ts_data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XX_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
		isp_mode = (mode == MASK_UPDATE_ISP ? true : false);
	} else {
#if IST30XX_INTERNAL_BIN
		fw = (u8 *)ts_fw;
		fw_size = ts_fw_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(ts_data, fw, fw_size);
	ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, fw);

	mutex_lock(&ist30xx_mutex);
	ist30xx_fw_update(ts_data->client, fw, fw_size, isp_mode);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_calibrate(1);

	ist30xx_init_touch_driver(ts_data);

	if (mode == MASK_UPDATE_ISP || mode == MASK_UPDATE_FW)
		release_firmware(request_fw);

	return size;
}

ssize_t ist30xx_fw_status(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int count;

	switch (ts_data->status.update) {
	case 1:
		count = sprintf(buf, "Downloading\n");
		break;
	case 2:
		count = sprintf(buf, "Update success, ver %x -> %x, status : %d, gap : %d\n",
				ts_data->fw.pre_ver, ts_data->fw.ver,
				CALIB_TO_STATUS(ts_data->status.calib_msg),
				CALIB_TO_GAP(ts_data->status.calib_msg));
		break;
	default:
		count = sprintf(buf, "Pass\n");
	}

	return count;
}


/* sysfs: /sys/class/touch/firmware/fwparam */
ssize_t ist30xx_fwparam_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret;
	int fw_size = 0;
	u8 *fw = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;

	unsigned long mode = simple_strtoul(buf, &tmp, 10);

	if (mode == MASK_UPDATE_FW) {
		ret = request_firmware(&request_fw, IST30XX_FW_NAME, &ts_data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XX_FW_NAME);
			return size;
		}
		fw = (u8 *)request_fw->data;
		fw_size = (u32)request_fw->size;
	} else {
#if IST30XX_INTERNAL_BIN
		fw = (u8 *)ts_fw;
		fw_size = ts_fw_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(ts_data, fw, fw_size);
	ist30xx_fw_ver = ist30xx_parse_fw_ver(PARSE_FLAG_FW, fw);

	mutex_lock(&ist30xx_mutex);
	ist30xx_fw_param_update(ts_data->client, fw);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_calibrate(1);

	ist30xx_init_touch_driver(ts_data);

	if (mode == MASK_UPDATE_FW)
		release_firmware(request_fw);

	return size;
}


/* sysfs: /sys/class/touch/firmware/viersion */
ssize_t ist30xx_fw_version(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int count;

	count = sprintf(buf, "IST30XX id: 0x%x, f/w ver: 0x%x, param ver: 0x%x\n",
			ts_data->chip_id, ts_data->fw.ver, ts_data->param_ver);

#if IST30XX_INTERNAL_BIN
	{
		char msg[128];

		ist30xx_get_update_info(ts_data, ts_fw, ts_fw_size);

		count += snprintf(msg, sizeof(msg),
				  " Header - f/w ver: 0x%x, param ver: 0x%x\r\n",
				  ist30xx_parse_fw_ver(PARSE_FLAG_FW, ts_fw),
				  ist30xx_parse_param_ver(PARSE_FLAG_FW, ts_fw));
		strncat(buf, msg, sizeof(msg));
	}
#endif

	return count;
}


/* sysfs: /sys/class/touch/parameters/param */
ssize_t ist30xx_param_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int ret = 0;
	u32 mode;
	int param_size = 0;
	u8 *param = NULL;
	char *tmp;
	const struct firmware *request_fw = NULL;

	mode = simple_strtoul(buf, &tmp, 10);
	if (mode == MASK_UPDATE_BIN) {
		ret = request_firmware(&request_fw, IST30XX_PARAM_NAME, &ts_data->client->dev);
		if (ret) {
			tsp_warn("File not found, %s\n", IST30XX_PARAM_NAME);
			return size;
		}
		param = (u8 *)request_fw->data;
		param_size = (u32)request_fw->size;
	} else {
#if IST30XX_INTERNAL_BIN
		param = (u8 *)ts_param;
		param_size = (u32)ts_param_size;
#else
		return size;
#endif          // IST30XX_INTERNAL_BIN
	}

	ist30xx_get_update_info(ts_data, param, param_size);

	mutex_lock(&ist30xx_mutex);
	ist30xx_param_update(ts_data->client, param, param_size);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_calibrate(1);

	ist30xx_init_touch_driver(ts_data);

	if (mode == MASK_UPDATE_BIN)
		release_firmware(request_fw);

	return size;
}

/* sysfs  */
static DEVICE_ATTR(firmware, S_IRWXUGO, ist30xx_fw_status, ist30xx_fw_store);
static DEVICE_ATTR(fwparam, S_IRWXUGO, NULL, ist30xx_fwparam_store);
static DEVICE_ATTR(boot, S_IRWXUGO, ist30xx_fw_show, ist30xx_boot_store);
static DEVICE_ATTR(version, S_IRWXUGO, ist30xx_fw_version, NULL);
static DEVICE_ATTR(param, S_IRWXUGO, NULL, ist30xx_param_store);

struct class *ist30xx_class;
struct device *ist30xx_fw_dev;
struct device *ist30xx_param_dev;


int ist30xx_init_update_sysfs(void)
{
	/* /sys/class/touch */
	ist30xx_class = class_create(THIS_MODULE, "touch");

	/* /sys/class/touch/firmware */
	ist30xx_fw_dev = device_create(ist30xx_class, NULL, 0, NULL, "firmware");

	/* /sys/class/touch/firmwware/firmware */
	if (device_create_file(ist30xx_fw_dev, &dev_attr_firmware) < 0)
		tsp_err("Failed to create device file(%s)!\n",
			dev_attr_firmware.attr.name);

	/* /sys/class/touch/firmwware/fwparam */
	if (device_create_file(ist30xx_fw_dev, &dev_attr_fwparam) < 0)
		tsp_err("Failed to create device file(%s)!\n",
			dev_attr_fwparam.attr.name);

	/* /sys/class/touch/firmwware/boot */
	if (device_create_file(ist30xx_fw_dev, &dev_attr_boot) < 0)
		tsp_err("Failed to create device file(%s)!\n",
			dev_attr_boot.attr.name);

	/* /sys/class/touch/firmware/version */
	if (device_create_file(ist30xx_fw_dev, &dev_attr_version) < 0)
		tsp_err("Failed to create device file(%s)!\n",
			dev_attr_version.attr.name);

	/* /sys/class/touch/parameters */
	ist30xx_param_dev = device_create(ist30xx_class, NULL, 0, NULL, "parameters");

	/* /sys/class/touch/parameters/param */
	if (device_create_file(ist30xx_param_dev, &dev_attr_param) < 0)
		tsp_err("Failed to create device file(%s)!\n",
			dev_attr_param.attr.name);

	ts_data->status.update = 0;
	ts_data->status.calib = 0;

	return 0;
}
