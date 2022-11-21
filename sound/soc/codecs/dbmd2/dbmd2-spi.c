/*
 * DSPG DBMD2 SPI interface driver
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
#define DEBUG
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/firmware.h>

#include "dbmd2-interface.h"
#include "dbmd2-va-regmap.h"
#include "dbmd2-vqe-regmap.h"

#define MAX_CMD_SEND_SIZE		8
#define RETRY_COUNT			5

static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};

struct dbmd2_spi_private {
	struct device			*dev;
	struct spi_device		*client;
	struct chip_interface		chip;
	long int			speed;
};

static ssize_t read_spi_data(struct dbmd2_private *p, void *buf, size_t len)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
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

		ret =  spi_read(spi_p->client, local_buf, count);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s: spi_read failed\n",
				__func__);
			i = -EIO;
			goto out;
		}
		memcpy(d + i, local_buf, ret);

		if (!time_before(jiffies, timeout)) {
			dev_err(spi_p->dev,
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



static ssize_t write_spi_data(struct dbmd2_private *p, const u8 *buf,
			      size_t len)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	struct spi_device *spi = spi_p->client;
	int rc;

	rc = spi_write(spi, buf, len);
	if (rc != 0) {
		dev_err(spi_p->dev, "%s(): error %d writing SR\n",
				__func__, rc);
		return rc;
	} else
		return len;
}


static ssize_t send_spi_data(struct dbmd2_private *p, const void *buf,
			      size_t len)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	int ret = 0;
	const u8 *cmds = (const u8 *)buf;
	size_t to_copy = len;

	while (to_copy > 0) {
		ret = write_spi_data(p, cmds,
				min_t(size_t, to_copy, MAX_CMD_SEND_SIZE));
		if (ret < 0) {
			dev_err(spi_p->dev,
				"%s: send_spi_data failed ret=%d\n",
				__func__, ret);
			break;
		}
		to_copy -= ret;
		cmds += ret;
	}

	return len - to_copy;
}

static int send_spi_cmd_boot(struct dbmd2_spi_private *spi_p, u32 command)
{
	struct spi_device *spi = spi_p->client;
	u8 send[3];
	int ret = 0;

	dev_err(spi_p->dev, "%s: send_spi_cmd_boot = %x\n", __func__, command);

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	ret = spi_write(spi, send, 2);
	if (ret < 0) {
		dev_err(spi_p->dev, "%s: send_spi_cmd_boot ret = %d\n",
			__func__, ret);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	   processing is finished, which can take up to 10 ms */
	usleep_range(10000, 11000);

	return ret;
}

static ssize_t send_spi_cmd_vqe(struct dbmd2_private *p,
	u32 command, u16 *response)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	u8 send[4];
	u8 recv[4];
	ssize_t ret = 0;
	int retries = 10;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = send_spi_data(p, send, 4);
	if (ret < 0) {
		dev_err(spi_p->dev,
				"%s: cmd:0x%0X - send_spi_data failed ret:%zd\n",
				__func__, command, ret);
		return ret;
	}

	usleep_range(10000, 11000);

	if (command == (DBMD2_VQE_SET_POWER_STATE_CMD |
			DBMD2_VQE_SET_POWER_STATE_HIBERNATE))
		return 0;

	/* read response */
	do {
		ret = spi_read(spi_p->client, recv, 4);
		if (ret < 0) {
			dev_dbg(spi_p->dev, "%s: read failed; retries:%d\n",
				__func__, retries);
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

		dev_err(spi_p->dev,
			"%s: incorrect ack (got 0x%.2x%.2x)\n",
			__func__, recv[0], recv[1]);
		ret = -EINVAL;

		/* Wait before polling again */
		usleep_range(10000, 11000);
	} while (--retries);

	if (!retries)
		dev_err(spi_p->dev,
			"%s: cmd:0x%08X - wrong ack, giving up\n",
			__func__, command);

	return ret;
}


static ssize_t send_spi_cmd_va(struct dbmd2_private *p, u32 command,
				   u16 *response)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(spi_p->dev, "%s: Send 0x%02x\n", __func__, command);
	if (response) {

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'r';

		ret = send_spi_data(p, send, 3);
		if (ret != 3)
			goto out;

		ret = 0;

		/* the sleep command cannot be acked before the device
		 * goes to sleep */
		ret = read_spi_data(p, recv, 5);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s:spi_read failed =%d\n",
				__func__, ret);
			return ret;
		}
		recv[5] = 0;
		ret = kstrtou16((const char *)&recv[1], 16, response);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s failed -%d\n", __func__, ret);
			dev_err(spi_p->dev, "%s: %x:%x:%x:%x:\n",
				__func__, recv[1], recv[2],
				recv[3], recv[4]);
			return ret;
		}

		dev_dbg(spi_p->dev, "%s: Received 0x%02x\n", __func__,
			*response);

		ret = 0;
	} else {
		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		if (ret < 0)
			goto out;
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[3] = tmp[0];
		send[4] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[5] = tmp[0];
		send[6] = tmp[1];

		ret = send_spi_data(p, send, 7);
		if (ret != 7)
			goto out;
		ret = 0;

	}
out:
	return ret;
}

static int spi_can_boot(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_prepare_boot(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	struct spi_device *spi = spi_p->client;
	int ret = 0;
	u8 sync_spi[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	dev_dbg(spi_p->dev, "%s\n", __func__);

	/*
	* setup spi parameters; this makes sure that parameters we request
	* are acceptable by the spi driver
	*/

	spi->mode = SPI_MODE_0; /* clk active low */
	spi->bits_per_word = 8;
	spi->max_speed_hz = spi_p->speed;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(spi_p->dev, "%s:failed %x\n", __func__, ret);
		return -EIO;
	}

	dev_info(spi_p->dev, "%s: speed %ld hZ\n", __func__, spi_p->speed);

	send_spi_data(p, sync_spi, sizeof(sync_spi));
	ret = spi_setup(spi);

	return ret;
}

static int spi_boot(const struct firmware *fw, struct dbmd2_private *p,
		    const void *checksum, size_t chksum_len, int load_fw)
{
	int retry = RETRY_COUNT;
	int ret;
	ssize_t send_bytes;
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	u8 sync_spi[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	u8 sbl_spi[] = {
		0x5A, 0x04, 0x4c, 0x00, 0x00,
		0x03, 0x11, 0x55, 0x05, 0x00};

	u8 rx_checksum[7];

	dev_dbg(spi_p->dev, "%s\n", __func__);

	while (retry--) {

		/* reset DBMD2 chip */
		p->reset_sequence(p);

		/* delay before sending commands */
		if (p->clk_get_rate(p, DBMD2_CLK_MASTER) <= 32768)
			msleep(280);
		else
			usleep_range(15000, 20000);

		/* send sync */
		send_bytes = send_spi_data(p, sync_spi, sizeof(sync_spi));
		if (send_bytes != sizeof(sync_spi)) {
			dev_err(p->dev,
				"%s: -----------> Sync spi error\n", __func__);
			continue;
		}

		/* send SBL */
		send_bytes = send_spi_data(p, sbl_spi, sizeof(sbl_spi));
		if (send_bytes != sizeof(sbl_spi)) {
			dev_err(p->dev,
				"%s: -----------> load SBL error\n", __func__);
			continue;
		}

		if (!load_fw)
			break;

		usleep_range(10000, 11000);

		/* send CRC clear command */
		ret = send_spi_data(p, clr_crc, sizeof(clr_crc));
		if (ret != sizeof(clr_crc)) {
			dev_err(p->dev, "%s: failed to clear CRC\n",
				 __func__);
			continue;
		}

		/* send firmware */
		send_bytes = send_spi_data(p, fw->data, fw->size - 4);
		if (send_bytes != fw->size - 4) {
			dev_err(p->dev,
				"%s: -----------> load firmware error\n",
				__func__);
			continue;
		}

		msleep(50);

		/* verify checksum */
		if (checksum) {

			ret = send_spi_cmd_boot(spi_p, DBMD2_READ_CHECKSUM);
			if (ret < 0) {
				dev_err(spi_p->dev,
					"%s: could not read checksum\n",
					__func__);
				continue;
			}

			ret = spi_read(spi_p->client, (void *)rx_checksum, 7);
			if (ret < 0) {
				dev_err(spi_p->dev,
					"%s: could not read checksum data\n",
					__func__);
				continue;
			}

			ret = p->verify_checksum(
				p, checksum, (void *)(&rx_checksum[3]), 4);
			if (ret) {
				dev_err(p->dev, "%s: checksum mismatch\n",
					__func__);
				continue;
			}
		}

		dev_info(p->dev, "%s: ---------> firmware loaded\n", __func__);
		break;
	}

	/* no retries left, failed to boot */
	if (retry < 0) {
		dev_err(p->dev, "%s: failed to load firmware\n", __func__);
		return -1;
	}

	/* send boot command */
	ret = send_spi_cmd_boot(spi_p, DBMD2_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	/* wait some time */
	usleep_range(30000, 31000);


	return 0;
}

static int spi_finish_boot(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;


	struct spi_device *spi = spi_p->client;
	int ret = 0;
	/* change to normal operation spi address */
	dev_dbg(spi_p->dev, "%s\n", __func__);
	msleep(275);

	/*
	* setup spi parameters; this makes sure that parameters we request
	* are acceptable by the spi driver
	*/
	spi->mode = SPI_MODE_0; /* clk active low */
	spi->bits_per_word = 8;
	/* spi->max_speed_hz = 800000; */

	ret = spi_setup(spi);

	msleep(1000);

	return 0;
}

static int spi_dump_state(struct dbmd2_private *p, char *buf)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	int off = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	off += sprintf(buf + off, "\t===SPI Interface  Dump====\n");

	return off;
}

static int spi_set_va_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}



static int spi_set_vqe_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static void spi_transport_enable(struct dbmd2_private *p, bool enable)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");

	if (enable) {
		p->wakeup_set(p);
		msleep(35); /* FIXME: why so long? check with FW guys */
	} else
		p->wakeup_release(p);

	return;
}


static int spi_prepare_buffering(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}


static int spi_read_audio_data(struct dbmd2_private *p, size_t samples)
{
#define SIZE 8
	unsigned int count = SIZE;
	size_t i;
	size_t bytes_to_read;
	int ret;
	u8 local_buf[SIZE];
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	/* XXX check this */
	ret = send_spi_cmd_va(p,
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
		ret = read_spi_data(p, local_buf, count);
		if (ret > 0)
			kfifo_in(&p->pcm_kfifo, local_buf, count);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: read audio failed, still %zu bytes to read\n",
				__func__, bytes_to_read - i);
			ret = 0;
			goto out;
		}
	}
	ret = samples;
#undef SIZE
out:
	return ret;
}

static int spi_finish_buffering(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);


	return ret;
}

static int spi_prepare_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_load_amodel(struct dbmd2_private *p,  const void *data,
			   size_t size, const void *checksum, size_t chksum_len,
			   int load)
{
	int retry = RETRY_COUNT;
	int ret;
	ssize_t send_bytes;
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	u8 rx_checksum[6];

	dev_dbg(spi_p->dev, "%s\n", __func__);

	while (retry--) {
		if (load)
			ret = send_spi_cmd_va(
				p, DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL, NULL);
		else
			ret = send_spi_cmd_va(p,
				DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL | 0x1, NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set to recieve new amodel\n",
				__func__);
			continue;
		}

		if (!load)
			return 0;

		dev_info(p->dev,
			"%s: ---------> acoustic model download start\n",
			__func__);

		send_bytes = send_spi_data(p, data, size);
		if (send_bytes != size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* verify checksum */
		if (checksum) {
			ret = send_spi_cmd_boot(spi_p, DBMD2_READ_CHECKSUM);
			if (ret < 0) {
				dev_err(spi_p->dev,
					"%s: could not read checksum\n",
					__func__);
				continue;
			}

			ret = spi_read(spi_p->client, rx_checksum, 6);
			if (ret < 0) {
				dev_err(spi_p->dev,
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
	ret = send_spi_cmd_boot(spi_p, DBMD2_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	/* wait some time */
	usleep_range(10000, 11000);

	return 0;
}

static int spi_finish_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_spi_private *spi_p =
				(struct dbmd2_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	/* do the same as for finishing buffering */
	ret = p->va_set_speed(p, DBMD2_VA_SPEED_NORMAL);
	if (ret) {
		dev_err(p->dev, "%s: failed to send change speed command\n",
			__func__);
	}

	usleep_range(10000, 11000);
	return 0;
}

static int dbmd2_spi_probe(struct spi_device *client)
{
	struct  device_node *np;
	int     cs_gpio, ret;
	u32     spi_speed;
	struct dbmd2_spi_private *p;

	dev_info(&client->dev, "%s\n", __func__);

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

	cs_gpio = of_get_named_gpio(np, "cs-gpio", 0);
	if (cs_gpio < 0) {
		cs_gpio = -1;
		dev_info(p->dev, "%s: of node is null\n", __func__);
	}
	if (!gpio_is_valid(cs_gpio)) {
		dev_err(p->dev, "%s: sp cs gpio invalid\n", __func__);
		ret = -EINVAL;
	}

	ret = gpio_request(cs_gpio, "DBMD2 spi_cs");
	if (ret < 0)
		dev_err(p->dev, "%s: error requesting reset gpio\n", __func__);

	gpio_direction_output(cs_gpio, 1);
	dev_info(p->dev, "%s: spi cs configured\n", __func__);

	ret = of_property_read_u32(np, "spi-max-frequency", &spi_speed);
	if (ret && ret != -EINVAL)
		spi_speed = 2000000;

	dev_info(p->dev, "%s: spi speed is %u\n", __func__, spi_speed);

	p->speed = spi_speed;

	client->cs_gpio = cs_gpio;

	/* fill in chip interface functions */
	p->chip.can_boot = spi_can_boot;
	p->chip.prepare_boot = spi_prepare_boot;
	p->chip.boot = spi_boot;
	p->chip.finish_boot = spi_finish_boot;
	p->chip.dump = spi_dump_state;
	p->chip.set_va_firmware_ready = spi_set_va_firmware_ready;
	p->chip.set_vqe_firmware_ready = spi_set_vqe_firmware_ready;
	p->chip.transport_enable = spi_transport_enable;
	p->chip.read = read_spi_data;
	p->chip.write = send_spi_data;
	p->chip.send_cmd_vqe = send_spi_cmd_vqe;
	p->chip.send_cmd_va = send_spi_cmd_va;
	p->chip.prepare_buffering = spi_prepare_buffering;
	p->chip.read_audio_data = spi_read_audio_data;
	p->chip.finish_buffering = spi_finish_buffering;
	p->chip.prepare_amodel_loading = spi_prepare_amodel_loading;
	p->chip.load_amodel = spi_load_amodel;
	p->chip.finish_amodel_loading = spi_finish_amodel_loading;

	spi_set_drvdata(client,  &p->chip);

	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	ret = 0;
	goto out;

out_err_kfree:
	kfree(p);
out:
	return ret;
}

static int dbmd2_spi_remove(struct spi_device *client)
{
	struct chip_interface *ci = spi_get_drvdata(client);
	struct dbmd2_spi_private *p = (struct dbmd2_spi_private *)ci->pdata;

	kfree(p);

	spi_set_drvdata(client, NULL);

	return 0;
}



static const struct of_device_id dbmd2_spi_of_match[] = {
	{ .compatible = "dspg,dbmd2-spi", },
	{},
};
MODULE_DEVICE_TABLE(of, dbmd2_spi_of_match);

static const struct spi_device_id dbmd2_spi_id[] = {
	{ "dbmd2", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, dbmd2_spi_id);

static struct spi_driver dbmd2_spi_driver = {
	.driver = {
		.name = "dbmd2-spi",
		.bus	= &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_spi_of_match,
	},
	.probe =    dbmd2_spi_probe,
	.remove =   dbmd2_spi_remove,
	.id_table = dbmd2_spi_id,
};

static int __init dbmd2_modinit(void)
{
	return spi_register_driver(&dbmd2_spi_driver);
}

module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	spi_unregister_driver(&dbmd2_spi_driver);
}
module_exit(dbmd2_exit);

MODULE_DESCRIPTION("DSPG DBMD2 spi interface driver");
MODULE_LICENSE("GPL");
