/*
 * DSPG DBMD2 I2C interface driver
 *
 * Copyright (C) 2014 DSP Group
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
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/firmware.h>

#include "dbmd2-interface.h"
#include "dbmd2-va-regmap.h"
#include "dbmd2-vqe-regmap.h"
#include "dbmd2-i2c-sbl.h"

#define DRIVER_VERSION			"1.0"

#define MAX_CMD_SEND_SIZE		32
#define RETRY_COUNT			5

static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};

struct dbmd2_i2c_private {
	struct device			*dev;
	struct i2c_client		*client;
	struct chip_interface		chip;
	unsigned short			boot_addr;
	unsigned short			operation_addr;
};

static ssize_t send_i2c_cmd_vqe(struct dbmd2_private *p,
	u32 command, u16 *response)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	u8 send[4];
	u8 recv[4];
	ssize_t ret = 0;
	int retries = 10;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = i2c_master_send(i2c_p->client, send, 4);
	if (ret < 0) {
		dev_err(i2c_p->dev,
				"%s: cmd:0x%08X - i2c_master_send failed ret:%zd\n",
				__func__, command, ret);
		return ret;
	}

	usleep_range(20000, 21000);

	if ((command == (DBMD2_VQE_SET_POWER_STATE_CMD |
			DBMD2_VQE_SET_POWER_STATE_HIBERNATE)) ||
		(command == DBMD2_VQE_SET_SWITCH_TO_BOOT_CMD))
		return 0;

	/* we need additional sleep till system is ready */
	if ((command == (DBMD2_VQE_SET_SYSTEM_CONFIG_CMD |
			DBMD2_VQE_SET_SYSTEM_CONFIG_PRIMARY_CFG)))
		msleep(60);

	/* read response */
	do {
		ret = i2c_master_recv(i2c_p->client, recv, 4);
		if (ret < 0) {
#if 0
			dev_dbg(i2c_p->dev, "%s: read failed; retries:%d\n",
				__func__, retries);
#endif
			/* Wait before polling again */
			usleep_range(10000, 11000);

			continue;
		}
		/*
		 * Check that the first two bytes of the response match
		 * (the ack is in those bytes)
		 */
		if ((send[0] == recv[0]) && (send[1] == recv[1])) {
			if (response)
				*response = (recv[2] << 8) | recv[3];
			ret = 0;
			break;
		}

		dev_warn(i2c_p->dev,
			"%s: incorrect ack (got 0x%.2x%.2x)\n",
			__func__, recv[0], recv[1]);
		ret = -EINVAL;

		/* Wait before polling again */
		usleep_range(10000, 11000);
	} while (--retries);

	if (!retries)
		dev_err(i2c_p->dev,
			"%s: cmd:0x%8x - wrong ack, giving up\n",
			__func__, command);

	return ret;
}

static ssize_t send_i2c_cmd_va(struct dbmd2_private *p, u32 command,
				  u16 *response)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	u8 send[3];
	u8 recv[2];
	int ret;

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;
	send[2] = (command) & 0xff;

	if (response) {
		ret = i2c_master_send(i2c_p->client, send, 1);
		if (ret < 0) {
			dev_err(i2c_p->dev,
				"%s: i2c_master_send failed ret = %d\n",
				__func__, ret);
			return ret;
		}

		usleep_range(10000, 11000);
		ret = i2c_master_recv(i2c_p->client, recv, 2);
		if (ret < 0) {
			dev_err(i2c_p->dev, "%s: i2c_master_recv failed\n",
				__func__);
			return ret;
		}
		*response = (recv[0] << 8) | recv[1];
	} else {
		ret = i2c_master_send(i2c_p->client, send, 3);
		if (ret < 0) {
			dev_err(i2c_p->dev,
				"%s: i2c_master_send failed ret = %d\n",
				__func__, ret);
			return ret;
		}
		usleep_range(10000, 11000);
	}

	return 0;
}

static ssize_t read_i2c_data(struct dbmd2_private *p, void *buf, size_t len)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
#define SIZE 8
	size_t count = SIZE;
	u8 local_buf[SIZE];
	u8 *d = (u8 *)buf;
	ssize_t i;
	int ret;
	/* if stuck for more than 10s, something is wrong */
	unsigned long timeout = jiffies + msecs_to_jiffies(10000);

	for (i = 0; i < len; i += count) {
		if ((i + count) > len)
			count = len - i;

		ret = i2c_master_recv(i2c_p->client, local_buf, count);
		if (ret < 0) {
			dev_err(i2c_p->dev, "%s: i2c_master_recv failed\n",
				__func__);
			i = -EIO;
			goto out;
		}
		memcpy(d + i, local_buf, ret);

		if (!time_before(jiffies, timeout)) {
			dev_err(i2c_p->dev,
				"%s: read data timed out after %zd bytes\n",
				__func__, i);
			i = -ETIMEDOUT;
			goto out;
		}
	}
	return len;
out:
	return i;
#undef SIZE
}

static ssize_t write_i2c_data(struct dbmd2_private *p, const void *buf,
			      size_t len)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	ssize_t ret = 0;
	const u8 *i2c_cmds = (const u8 *)buf;
	size_t to_copy = len;

	while (to_copy > 0) {
		ret = i2c_master_send(i2c_p->client, i2c_cmds,
				min_t(size_t, to_copy, MAX_CMD_SEND_SIZE));
		if (ret < 0) {
			dev_err(i2c_p->dev,
				"%s: i2c_master_send failed ret=%zd\n",
				__func__, ret);
			break;
		}
		to_copy -= ret;
		i2c_cmds += ret;
	}

	return len - to_copy;
}

static int send_i2c_cmd_boot(struct dbmd2_i2c_private *i2c_p, u32 command)
{
	u8 send[3];
	int ret = 0;

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	ret = i2c_master_send(i2c_p->client, send, 2);
	if (ret < 0) {
		dev_err(i2c_p->dev, "%s: i2c_master_send failed ret = %d\n",
			__func__, ret);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	return 0;
}

static int i2c_can_boot(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static int i2c_prepare_boot(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static int i2c_boot(const struct firmware *fw, struct dbmd2_private *p,
		    const void *checksum, size_t chksum_len, int load_fw)
{
	int retry = RETRY_COUNT;
	int ret = 0;
	ssize_t send_bytes;
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	u8 rx_checksum[6];

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	/* change to boot I2C address */
	i2c_p->client->addr = i2c_p->boot_addr;

	while (retry--) {

		if (p->active_fw == DBMD2_FW_PRE_BOOT) {

			/* reset DBMD2 chip */
			p->reset_sequence(p);

			/* delay before sending commands */
			if (p->clk_get_rate(p, DBMD2_CLK_MASTER) <= 32768)
				msleep(300);
			else
				usleep_range(15000, 20000);

			if (p->set_i2c_freq_callback) {
				dev_dbg(p->dev,
					"%s: setting master bus to slow freq\n",
					__func__);
				p->set_i2c_freq_callback(
					i2c_p->client->adapter, I2C_FREQ_SLOW);
			}

			/* send SBL */
			send_bytes = write_i2c_data(p, sbl, sizeof(sbl));
			if (send_bytes != sizeof(sbl)) {
				dev_err(p->dev,
					"%s: -----------> load SBL error\n",
					 __func__);
				continue;
			}

			usleep_range(10000, 11000);

			if (p->set_i2c_freq_callback) {
				msleep(20);
				dev_dbg(p->dev,
					"%s: setting master bus to fast freq\n",
					__func__);
				p->set_i2c_freq_callback(
					i2c_p->client->adapter, I2C_FREQ_FAST);
			}

			/* send CRC clear command */
			ret = write_i2c_data(p, clr_crc, sizeof(clr_crc));
			if (ret != sizeof(clr_crc)) {
				dev_err(p->dev, "%s: failed to clear CRC\n",
					 __func__);
				continue;
			}
		} else {
			/* delay before sending commands */
			if (p->active_fw == DBMD2_FW_VQE)
				ret = send_i2c_cmd_vqe(p,
					DBMD2_VQE_SET_SWITCH_TO_BOOT_CMD,
					NULL);
			else if (p->active_fw == DBMD2_FW_VA)
				ret = send_i2c_cmd_va(p,
					DBMD2_VA_SWITCH_TO_BOOT,
					NULL);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to send 'Switch to BOOT' cmd\n",
					 __func__);
				continue;
			}
		}

		if (!load_fw)
			break;
		/* Sleep is needed here to ensure that chip is ready */
		msleep(20);

		/* send firmware */
		send_bytes = write_i2c_data(p, fw->data, fw->size - 4);
		if (send_bytes != fw->size - 4) {
			dev_err(p->dev,
				"%s: -----------> load firmware error\n",
				__func__);
			continue;
		}

		msleep(20);

		/* verify checksum */
		if (checksum) {
			ret = send_i2c_cmd_boot(i2c_p, DBMD2_READ_CHECKSUM);
			if (ret < 0) {
				dev_err(i2c_p->dev,
					"%s: could not read checksum\n",
					__func__);
				continue;
			}

			ret = i2c_master_recv(i2c_p->client, rx_checksum, 6);
			if (ret < 0) {
				dev_err(i2c_p->dev,
					"%s: could not read checksum data\n",
					__func__);
				continue;
			}

			ret = p->verify_checksum(p, checksum, &rx_checksum[2],
						 4);
			if (ret) {
				dev_err(p->dev, "%s: checksum mismatch\n",
					__func__);
				continue;
			}
		}

		dev_info(p->dev, "%s: ---------> firmware loaded\n",
			__func__);
		break;
	}

	/* no retries left, failed to boot */
	if (retry < 0) {
		dev_err(p->dev, "%s: failed to load firmware\n", __func__);
		return -1;
	}

	/* send boot command */
	ret = send_i2c_cmd_boot(i2c_p, DBMD2_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	/* wait some time */
	usleep_range(10000, 11000);

	return 0;
}

static int i2c_finish_boot(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	/* XXX */
	msleep(275);

	/* change to normal operation I2C address */
	i2c_p->client->addr = i2c_p->operation_addr;

	i2c_set_clientdata(i2c_p->client, &p->chip);

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static int i2c_dump_state(struct dbmd2_private *p, char *buf)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	int off = 0;

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	off += sprintf(buf + off, "\t===I2C Interface  Dump====\n");

	return off;
}

static int i2c_set_va_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}


static int i2c_set_vqe_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static void i2c_transport_enable(struct dbmd2_private *p, bool enable)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");

	if (enable) {
		p->wakeup_set(p);
		msleep(35); /* FIXME: why so long? check with FW guys */
	} else
		p->wakeup_release(p);
	return;
}


static int i2c_prepare_buffering(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static int i2c_read_audio_data(struct dbmd2_private *p, size_t samples)
{
#define SIZE 8
	unsigned int count = SIZE;
	size_t i;
	size_t bytes_to_read;
	int ret;
	u8 local_buf[SIZE];
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	/* XXX check this */
	ret = send_i2c_cmd_va(p,
			   DBMD2_VA_READ_AUDIO_BUFFER | samples,
			   NULL);
	if (ret) {
		dev_err(p->dev, "%s: failed to request %zu audio samples\n",
			__func__, samples);
		ret = 0;
		goto out;
	}

	bytes_to_read = samples * 8 * p->bytes_per_sample;

	for (i = 0; i < bytes_to_read; i += count) {
		if ((i + count) > bytes_to_read)
			count = bytes_to_read - i;
		ret = read_i2c_data(p, local_buf, count);
		if (ret > 0)
			kfifo_in(&p->pcm_kfifo, local_buf, count);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: read audio failed, still %zu bytes to read\n",
				__func__,
				bytes_to_read - i);
			ret = 0;
			goto out;
		}
	}
	ret = samples;
#undef SIZE
out:
	return ret;
}

static int i2c_finish_buffering(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	return 0;
}

static int i2c_prepare_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);
	return 0;
}

static int i2c_load_amodel(struct dbmd2_private *p,  const void *data,
			   size_t size, const void *checksum, size_t chksum_len,
			   int load)
{
	int retry = RETRY_COUNT;
	int ret;
	ssize_t send_bytes;
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;
	u8 rx_checksum[6];

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	while (retry--) {
		if (load)
			ret = send_i2c_cmd_va(
					p,
					DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL,
					NULL);
		else
			ret = send_i2c_cmd_va(
					p,
					DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL | 0x1,
					NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set firmware to recieve new acoustic model\n",
				__func__);
			continue;
		}

		if (!load)
			return 0;

		dev_info(p->dev,
			"%s: ---------> acoustic model download start\n",
			__func__);

		send_bytes = write_i2c_data(p, data, size);
		if (send_bytes != size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* verify checksum */
		if (checksum) {
			ret = send_i2c_cmd_boot(i2c_p, DBMD2_READ_CHECKSUM);
			if (ret < 0) {
				dev_err(i2c_p->dev,
					"%s: could not read checksum\n",
					__func__);
				continue;
			}

			ret = i2c_master_recv(i2c_p->client, rx_checksum, 6);
			if (ret < 0) {
				dev_err(i2c_p->dev,
					"%s: could not read checksum data\n",
					__func__);
				continue;
			}

			ret = p->verify_checksum(p, checksum, &rx_checksum[2],
						 4);
			if (ret) {
				dev_err(p->dev, "%s: checksum mismatch\n",
					__func__);
				continue;
			}
		}
		break;
	}

	/* no retries left, failed to load acoustic */
	if (retry < 0) {
		dev_err(p->dev, "%s: failed to load acoustic model\n",
			__func__);
		return -1;
	}

	/* send boot command */
	ret = send_i2c_cmd_boot(i2c_p, DBMD2_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	/* wait some time */
	usleep_range(10000, 11000);

	return 0;
}

static int i2c_finish_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_i2c_private *i2c_p =
				(struct dbmd2_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	u32 operation_addr;
	struct device_node *np;
	struct dbmd2_i2c_private *p;

	dev_dbg(&client->dev, "%s\n", __func__);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->client = client;
	p->dev = &client->dev;

	p->chip.pdata = p;

	np = p->dev->of_node;
	if (!np) {
		dev_err(p->dev, "%s: no devicetree entry\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	/* remember boot address */
	p->boot_addr = p->client->addr;
	ret = of_property_read_u32(np, "operational-addr", &operation_addr);
	if (ret != 0) {
		/*
		 * operational address not set, assume it is the same as the
		 * boot address
		 */
		p->operation_addr = p->boot_addr;
		dev_dbg(p->dev, "%s: setting operational addr to boot address\n",
			__func__);
	} else {
		p->operation_addr = (unsigned short)operation_addr;
		dev_dbg(p->dev, "%s: setting operational addr to 0x%2.2x\n",
			__func__, p->operation_addr);
	}

	/* fill in chip interface functions */
	p->chip.can_boot = i2c_can_boot;
	p->chip.prepare_boot = i2c_prepare_boot;
	p->chip.boot = i2c_boot;
	p->chip.finish_boot = i2c_finish_boot;
	p->chip.dump = i2c_dump_state;
	p->chip.set_va_firmware_ready = i2c_set_va_firmware_ready;
	p->chip.set_vqe_firmware_ready = i2c_set_vqe_firmware_ready;
	p->chip.transport_enable = i2c_transport_enable;
	p->chip.read = read_i2c_data;
	p->chip.write = write_i2c_data;
	p->chip.send_cmd_va = send_i2c_cmd_va;
	p->chip.send_cmd_vqe = send_i2c_cmd_vqe;
	p->chip.prepare_buffering = i2c_prepare_buffering;
	p->chip.read_audio_data = i2c_read_audio_data;
	p->chip.finish_buffering = i2c_finish_buffering;
	p->chip.prepare_amodel_loading = i2c_prepare_amodel_loading;
	p->chip.load_amodel = i2c_load_amodel;
	p->chip.finish_amodel_loading = i2c_finish_amodel_loading;

	i2c_set_clientdata(client, &p->chip);

	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	ret = 0;
	goto out;

out_err_kfree:
	kfree(p);
out:
	return ret;
}

static int dbmd2_i2c_remove(struct i2c_client *client)
{
	struct chip_interface *ci = i2c_get_clientdata(client);
	struct dbmd2_i2c_private *p = (struct dbmd2_i2c_private *)ci->pdata;

	kfree(p);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct of_device_id dbmd2_i2c_of_match[] = {
	{ .compatible = "dspg,dbmd2-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, dbmd2_i2c_of_match);

static const struct i2c_device_id dbmd2_i2c_id[] = {
	{ "dbmd2", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dbmd2_i2c_id);

static struct i2c_driver dbmd2_i2c_driver = {
	.driver = {
		.name = "dbmd2-i2c",
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_i2c_of_match,
	},
	.probe =    dbmd2_i2c_probe,
	.remove =   dbmd2_i2c_remove,
	.id_table = dbmd2_i2c_id,
};

static int __init dbmd2_modinit(void)
{
	return i2c_add_driver(&dbmd2_i2c_driver);
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	i2c_del_driver(&dbmd2_i2c_driver);
}
module_exit(dbmd2_exit);

MODULE_DESCRIPTION("DSPG DBMD2 I2C interface driver");
MODULE_LICENSE("GPL");
