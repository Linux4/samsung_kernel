/*
 * DSPG DBMDX SPI interface driver
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
#include <linux/spi/spi.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-vqe-regmap.h"
#include "dbmdx-spi.h"

#define DEFAULT_SPI_WRITE_CHUNK_SIZE	8
#define MAX_SPI_WRITE_CHUNK_SIZE	0x20000
#define DEFAULT_SPI_READ_CHUNK_SIZE	8
#define MAX_SPI_READ_CHUNK_SIZE		8192

static DECLARE_WAIT_QUEUE_HEAD(dbmdx_wq);

ssize_t send_spi_cmd_vqe(struct dbmdx_private *p,
	u32 command, u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
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
				"%s: cmd:0x%08X - send_spi_data failed ret:%zd\n",
				__func__, command, ret);
		return ret;
	}

	usleep_range(DBMDX_USLEEP_SPI_VQE_CMD_AFTER_SEND,
		DBMDX_USLEEP_SPI_VQE_CMD_AFTER_SEND + 1000);

	if ((command == (DBMDX_VQE_SET_POWER_STATE_CMD |
			DBMDX_VQE_SET_POWER_STATE_HIBERNATE)) ||
		(command == DBMDX_VQE_SET_SWITCH_TO_BOOT_CMD))
		return 0;

	/* we need additional sleep till system is ready */
	if ((command == (DBMDX_VQE_SET_SYSTEM_CONFIG_CMD |
			DBMDX_VQE_SET_SYSTEM_CONFIG_PRIMARY_CFG)))
		msleep(DBMDX_MSLEEP_SPI_VQE_SYS_CFG_CMD);

	/* read response */
	do {
		ret = spi_read(spi_p->client, recv, 4);
		if (ret < 0) {
#if 0
			dev_dbg(spi_p->dev, "%s: read failed; retries:%d\n",
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

		dev_warn(spi_p->dev,
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

ssize_t send_spi_cmd_va(struct dbmdx_private *p, u32 command,
				   u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
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

		usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND,
			DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND + 100);

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
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2 + 100);
	return ret;
}

#define DBMDX_VA_SPI_CMD_PADDED_SIZE 150

ssize_t send_spi_cmd_va_padded(struct dbmdx_private *p,
				u32 command,
				u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	char tmp[3];
	u8 send[DBMDX_VA_SPI_CMD_PADDED_SIZE] = {0};
	u8 recv[DBMDX_VA_SPI_CMD_PADDED_SIZE] = {0};
	int ret;
	u32 padded_cmd_w_size = spi_p->pdata->dma_min_buffer_size;
	u32 padded_cmd_r_size = 5;

	dev_dbg(spi_p->dev, "%s: Send 0x%02x\n", __func__, command);
	if (response) {

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[padded_cmd_w_size - 3] = tmp[0];
		send[padded_cmd_w_size - 2] = tmp[1];
		send[padded_cmd_w_size - 1] = 'r';

		ret = send_spi_data(p, send, padded_cmd_w_size);
		if (ret != padded_cmd_w_size)
			goto out;

		usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND,
			DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND + 100);

		ret = 0;

		/* the sleep command cannot be acked before the device
		 * goes to sleep */
		ret = read_spi_data(p, recv, padded_cmd_r_size);
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
		send[padded_cmd_w_size - 7] = tmp[0];
		send[padded_cmd_w_size - 6] = tmp[1];
		send[padded_cmd_w_size - 5] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[padded_cmd_w_size - 4] = tmp[0];
		send[padded_cmd_w_size - 3] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[padded_cmd_w_size - 2] = tmp[0];
		send[padded_cmd_w_size - 1] = tmp[1];

		ret = send_spi_data(p, send, padded_cmd_w_size);
		if (ret != padded_cmd_w_size)
			goto out;
		ret = 0;

	}
out:
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2 + 100);
	return ret;
}

ssize_t read_spi_data(struct dbmdx_private *p, void *buf, size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	size_t count = spi_p->pdata->read_chunk_size;
	ssize_t i;
	int ret;

	/* We are going to read everything in on chunk */
	if (len < count) {

		ret =  spi_read(spi_p->client, buf, len);

		if (ret < 0) {
			dev_err(spi_p->dev, "%s: spi_read failed\n",
				__func__);
			i = -EIO;
			goto out;
		}

		return len;
	} else {

		u8 *d = (u8 *)buf;
		/* if stuck for more than 10s, something is wrong */
		unsigned long timeout = jiffies + msecs_to_jiffies(10000);

		for (i = 0; i < len; i += count) {
			if ((i + count) > len)
				count = len - i;

			ret =  spi_read(spi_p->client,
				spi_p->pdata->read_buf, count);
			if (ret < 0) {
				dev_err(spi_p->dev, "%s: spi_read failed\n",
					__func__);
				i = -EIO;
				goto out;
			}
			memcpy(d + i, spi_p->pdata->read_buf, count);

			if (!time_before(jiffies, timeout)) {
				dev_err(spi_p->dev,
					"%s: read data timed out after %zd bytes\n",
					__func__, i);
				i = -ETIMEDOUT;
				goto out;
			}
		}
		return len;
	}
out:
	return i;
}


ssize_t write_spi_data(struct dbmdx_private *p, const u8 *buf,
			      size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
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


ssize_t send_spi_data(struct dbmdx_private *p, const void *buf,
			      size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int ret = 0;
	const u8 *cmds = (const u8 *)buf;
	size_t to_copy = len;
	size_t max_size = (size_t)(spi_p->pdata->write_chunk_size);

	while (to_copy > 0) {
		ret = write_spi_data(p, cmds,
				min_t(size_t, to_copy, max_size));
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

int send_spi_cmd_boot(struct dbmdx_spi_private *spi_p, u32 command)
{
	struct spi_device *spi = spi_p->client;
	u8 send[3];
	int ret = 0;

	dev_info(spi_p->dev, "%s: send_spi_cmd_boot = %x\n", __func__, command);

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
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT + 1000);

	return ret;
}


static int spi_can_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_prepare_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	struct spi_device *spi = spi_p->client;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	/*
	* setup spi parameters; this makes sure that parameters we request
	* are acceptable by the spi driver
	*/

	spi->mode = SPI_MODE_0; /* clk active low */
	spi->bits_per_word = 8;
	spi->max_speed_hz = spi_p->pdata->spi_speed;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(spi_p->dev, "%s:failed %x\n", __func__, ret);
		return -EIO;
	}

	dev_info(spi_p->dev, "%s: speed %u hZ\n",
		__func__, spi_p->pdata->spi_speed);

	/* send_spi_data(p, sync_spi, sizeof(sync_spi)); */
	ret = spi_setup(spi);

	return ret;
}

static int spi_finish_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;


	struct spi_device *spi = spi_p->client;
	int ret = 0;
	/* change to normal operation spi address */
	dev_dbg(spi_p->dev, "%s\n", __func__);
	msleep(DBMDX_MSLEEP_SPI_FINISH_BOOT_1);

	/*
	* setup spi parameters; this makes sure that parameters we request
	* are acceptable by the spi driver
	*/
	spi->mode = SPI_MODE_0; /* clk active low */
	spi->bits_per_word = 8;
	/* spi->max_speed_hz = 800000; */

	ret = spi_setup(spi);

	msleep(DBMDX_MSLEEP_SPI_FINISH_BOOT_2);

	return 0;
}

static int spi_dump_state(struct dbmdx_private *p, char *buf)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int off = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	off += sprintf(buf + off, "\t===SPI Interface  Dump====\n");
	off += sprintf(buf + off, "\tSPI Write Chunk Size:\t\t%d\n",
				spi_p->pdata->write_chunk_size);
	off += sprintf(buf + off, "\tSPI Read Chunk Size:\t\t%d\n",
				spi_p->pdata->read_chunk_size);

	off += sprintf(buf + off, "\tSPI DMA Min Buffer Size:\t\t%d\n",
				spi_p->pdata->dma_min_buffer_size);

	off += snprintf(buf + off, PAGE_SIZE - off,
					"\tInterface resumed:\t%s\n",
			spi_p->interface_enabled ? "ON" : "OFF");

	return off;
}

static int spi_set_va_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}


static int spi_set_vqe_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static void spi_transport_enable(struct dbmdx_private *p, bool enable)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");

	if (enable) {

		ret = wait_event_interruptible(dbmdx_wq,
			spi_p->interface_enabled);

		if (ret)
			dev_dbg(spi_p->dev,
				"%s, waiting for interface was interrupted",
				__func__);
		else
			dev_dbg(spi_p->dev, "%s, interface is active\n",
				__func__);
	}

	if (enable) {
		p->wakeup_set(p);
		msleep(DBMDX_MSLEEP_SPI_WAKEUP);
	} else
		p->wakeup_release(p);
}

void spi_interface_resume(struct dbmdx_spi_private *spi_p)
{
	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_p->interface_enabled = 1;
	wake_up_interruptible(&dbmdx_wq);
}


void spi_interface_suspend(struct dbmdx_spi_private *spi_p)
{
	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_p->interface_enabled = 0;
}

static int spi_prepare_buffering(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

#define DBMDX_AUDIO_DEBUG_EXT 0
#define DBMDX_SAMPLE_CHUNK_VERIFICATION_FLAG	0xdbd0

static int spi_read_audio_data(struct dbmdx_private *p,
	void *buf,
	size_t samples,
	bool to_read_metadata,
	size_t *available_samples,
	size_t *data_offset)
{
	size_t bytes_to_read;
	int ret;
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	if (to_read_metadata && spi_p->pdata->dma_min_buffer_size &&
		!(p->va_flags.padded_cmds_disabled))
		ret = send_spi_cmd_va_padded(p,
			DBMDX_VA_READ_AUDIO_BUFFER | samples, NULL);
	else
		ret = send_spi_cmd_va(p,
			DBMDX_VA_READ_AUDIO_BUFFER | samples, NULL);

	if (ret) {
		dev_err(p->dev, "%s: failed to request %zu audio samples\n",
			__func__, samples);
		ret = -1;
		goto out;
	}

	*available_samples = 0;

	if (to_read_metadata)
		*data_offset = 8;
	else
		*data_offset = 2;

	bytes_to_read = samples * 8 * p->bytes_per_sample + *data_offset;

	ret = read_spi_data(p, buf, bytes_to_read);

	if (ret != bytes_to_read) {
		dev_err(p->dev,
			"%s: read audio failed, %zu bytes to read, res(%d)\n",
			__func__,
			bytes_to_read,
			ret);
		ret = -1;
		goto out;
	}

	/* Word #4 contains current number of available samples */
	if (to_read_metadata) {
		u16 verif_flag;
		*available_samples = (size_t)(((u16 *)buf)[3]);
		verif_flag = (u16)(((u16 *)buf)[2]);

#if DBMDX_AUDIO_DEBUG_EXT
		dev_err(spi_p->dev, "%s: %x:%x:%x:%x:%x:%x:%x:%x\n",
				__func__,
				((u8 *)buf)[0],
				((u8 *)buf)[1],
				((u8 *)buf)[2],
				((u8 *)buf)[3],
				((u8 *)buf)[4],
				((u8 *)buf)[5],
				((u8 *)buf)[6],
				((u8 *)buf)[7]);
#endif

		if (verif_flag != DBMDX_SAMPLE_CHUNK_VERIFICATION_FLAG) {

			*available_samples = 0;

			dev_err(p->dev,
				"%s: Flag verificaiton failed %x:%x\n",
				__func__,
				((u8 *)buf)[4],
				((u8 *)buf)[5]);

			ret = -1;
			goto out;
		}
	} else
		*available_samples = samples;

	ret = samples;

	/* FW performes SPI reset after each chunk transaction
	  Thus delay is required */
	usleep_range(DBMDX_USLEEP_SPI_AFTER_CHUNK_READ,
		DBMDX_USLEEP_SPI_AFTER_CHUNK_READ + 100);
out:
	return ret;
}

static int spi_finish_buffering(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);


	return ret;
}

static int spi_prepare_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_load_amodel(struct dbmdx_private *p,  const void *data,
			   size_t size, size_t gram_size, size_t net_size,
			   const void *checksum, size_t chksum_len,
			   enum dbmdx_load_amodel_mode load_amodel_mode)
{
	int retry = RETRY_COUNT;
	int ret;
	ssize_t send_bytes;
	size_t cur_pos;
	size_t cur_size;
	size_t model_size;
	int model_size_fw;
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	u8 rx_checksum[7];

	dev_dbg(spi_p->dev, "%s\n", __func__);

	model_size = gram_size + net_size + DBMDX_AMODEL_HEADER_SIZE*2;
	model_size_fw = (int)(model_size / 16) + 1;

	while (retry--) {
		if (load_amodel_mode == LOAD_AMODEL_PRIMARY) {
			ret = send_spi_cmd_va(
					p,
					DBMDX_VA_PRIMARY_AMODEL_SIZE |
					model_size_fw,
					NULL);

			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to set prim. amodel size\n",
					__func__);
				continue;
			}
		} else if (load_amodel_mode == LOAD_AMODEL_2NDARY) {
			ret = send_spi_cmd_va(
					p,
					DBMDX_VA_SECONDARY_AMODEL_SIZE |
					model_size_fw,
					NULL);

			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to set prim. amodel size\n",
					__func__);
				continue;
			}
		}

		ret = send_spi_cmd_va(
				p,
				DBMDX_VA_LOAD_NEW_ACUSTIC_MODEL |
				load_amodel_mode,
				NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set fw to receive new amodel\n",
				__func__);
			continue;
		}

		dev_info(p->dev,
			"%s: ---------> acoustic model download start\n",
			__func__);

		cur_size = DBMDX_AMODEL_HEADER_SIZE;
		cur_pos = 0;
		/* Send Gram Header */
		send_bytes = send_spi_data(p, data, cur_size);

		if (send_bytes != cur_size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* wait for FW to process the header */
		usleep_range(DBMDX_USLEEP_AMODEL_HEADER,
			DBMDX_USLEEP_AMODEL_HEADER + 1000);

		cur_pos += DBMDX_AMODEL_HEADER_SIZE;
		cur_size = gram_size;

		/* Send Gram Data */
		send_bytes = send_spi_data(p, data + cur_pos, cur_size);

		if (send_bytes != cur_size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		cur_pos += gram_size;
		cur_size = DBMDX_AMODEL_HEADER_SIZE;

		/* Send Net Header */
		send_bytes = send_spi_data(p, data + cur_pos, cur_size);
		if (send_bytes != cur_size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* wait for FW to process the header */
		usleep_range(DBMDX_USLEEP_AMODEL_HEADER,
			DBMDX_USLEEP_AMODEL_HEADER + 1000);

		cur_pos += DBMDX_AMODEL_HEADER_SIZE;
		cur_size = net_size;

		/* Send Net Data */
		send_bytes = send_spi_data(p, data + cur_pos, cur_size);
		if (send_bytes != cur_size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* verify checksum */
		if (checksum) {
			ret = send_spi_cmd_boot(spi_p, DBMDX_READ_CHECKSUM);
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

			ret = p->verify_checksum(p, checksum, &rx_checksum[3],
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
	ret = send_spi_cmd_boot(spi_p, DBMDX_FIRMWARE_BOOT);
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n", __func__);
		return -1;
	}

	/* wait some time */
	usleep_range(DBMDX_USLEEP_SPI_AFTER_LOAD_AMODEL,
		DBMDX_USLEEP_SPI_AFTER_LOAD_AMODEL + 1000);

	return 0;
}

static int spi_finish_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	/* do the same as for finishing buffering */

	return 0;
}

static u32 spi_get_read_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s SPI read chunk is %u\n",
		__func__, spi_p->pdata->read_chunk_size);

	return spi_p->pdata->read_chunk_size;
}

static u32 spi_get_write_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s SPI write chunk is %u\n",
		__func__, spi_p->pdata->write_chunk_size);

	return spi_p->pdata->write_chunk_size;
}

static int spi_set_read_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	if (size > MAX_SPI_READ_CHUNK_SIZE) {
		dev_err(spi_p->dev,
			"%s Error setting SPI read chunk. Max chunk size: %u\n",
		__func__, MAX_SPI_READ_CHUNK_SIZE);
		return -1;
	} else if ((size % 2) != 0) {
		dev_err(spi_p->dev,
			"%s Error setting SPI read chunk. Uneven size\n",
		__func__);
		return -2;
	} else if (size == 0)
		spi_p->pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;
	else
		spi_p->pdata->read_chunk_size = size;

	dev_dbg(spi_p->dev, "%s SPI read chunk was set to %u\n",
		__func__, spi_p->pdata->read_chunk_size);

	return 0;
}

static int spi_set_write_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	if (size > MAX_SPI_WRITE_CHUNK_SIZE) {
		dev_err(spi_p->dev,
			"%s Error setting SPI write chunk. Max chunk size: %u\n",
		__func__, MAX_SPI_WRITE_CHUNK_SIZE);
		return -1;
	} else if ((size % 2) != 0) {
		dev_err(spi_p->dev,
			"%s Error setting SPI write chunk. Uneven size\n",
		__func__);
		return -2;
	} else if (size == 0)
		spi_p->pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;
	else
		spi_p->pdata->write_chunk_size = size;

	dev_dbg(spi_p->dev, "%s SPI write chunk was set to %u\n",
		__func__, spi_p->pdata->write_chunk_size);

	return 0;
}

int spi_common_probe(struct spi_device *client)
{
#ifdef CONFIG_OF
	struct  device_node *np;
#endif
	int ret;
	struct dbmdx_spi_private *p;
	struct dbmdx_spi_data *pdata;

	dev_dbg(&client->dev, "%s(): dbmdx\n", __func__);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->client = client;
	p->dev = &client->dev;

	p->chip.pdata = p;
#ifdef CONFIG_OF
	np = p->dev->of_node;
	if (!np) {
		dev_err(p->dev, "%s: no devicetree entry\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	pdata = kzalloc(sizeof(struct dbmdx_spi_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto out_err_kfree;
	}
#else
	pdata = dev_get_platdata(&client->dev);
	if (pdata == NULL) {
		dev_err(p->dev, "%s: dbmdx, no platform data found\n",
			__func__);
		return -ENODEV;
	}
#endif

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "spi-max-frequency",
		&(pdata->spi_speed));
	if (ret && ret != -EINVAL)
		pdata->spi_speed = 2000000;
#endif
	dev_info(p->dev, "%s: spi speed is %u\n", __func__, pdata->spi_speed);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "read-chunk-size",
		&pdata->read_chunk_size);
	if (ret != 0) {
		/*
		 * read-chunk-size not set, set it to default
		 */
		pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting spi read chunk to default val: %u bytes\n",
			__func__, pdata->read_chunk_size);
	}
#endif
	if (pdata->read_chunk_size > MAX_SPI_READ_CHUNK_SIZE)
		pdata->read_chunk_size = MAX_SPI_READ_CHUNK_SIZE;
	if (pdata->read_chunk_size == 0)
		pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting spi read chunk to %u bytes\n",
			__func__, pdata->read_chunk_size);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "write-chunk-size",
		&pdata->write_chunk_size);
	if (ret != 0) {
		/*
		 * write-chunk-size not set, set it to default
		 */
		pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting spi write chunk to default val: %u bytes\n",
			__func__, pdata->write_chunk_size);
	}
#endif
	if (pdata->write_chunk_size > MAX_SPI_WRITE_CHUNK_SIZE)
		pdata->write_chunk_size = MAX_SPI_WRITE_CHUNK_SIZE;
	if (pdata->write_chunk_size == 0)
		pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting spi write chunk to %u bytes\n",
			__func__, pdata->write_chunk_size);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "dma_min_buffer_size",
		&pdata->dma_min_buffer_size);
	if (ret != 0) {
		/*
		 * read-chunk-size not set, set it to default
		 */
		pdata->dma_min_buffer_size = 0;
		dev_info(p->dev,
			"%s: Setting Min DMA Cmd Size to default: %u bytes\n",
			__func__, pdata->dma_min_buffer_size);
	}
#endif
	if (pdata->dma_min_buffer_size > DBMDX_VA_SPI_CMD_PADDED_SIZE)
		pdata->dma_min_buffer_size = DBMDX_VA_SPI_CMD_PADDED_SIZE;
	if (pdata->dma_min_buffer_size < 7 && pdata->dma_min_buffer_size > 0)
		pdata->dma_min_buffer_size = 7;

	dev_info(p->dev, "%s: Setting Min DMA Cmd Size to default: %u bytes\n",
			__func__, pdata->dma_min_buffer_size);

	p->pdata = pdata;

	/* fill in chip interface functions */
	p->chip.can_boot = spi_can_boot;
	p->chip.prepare_boot = spi_prepare_boot;
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
	p->chip.get_write_chunk_size = spi_get_write_chunk_size;
	p->chip.get_read_chunk_size = spi_get_read_chunk_size;
	p->chip.set_write_chunk_size = spi_set_write_chunk_size;
	p->chip.set_read_chunk_size = spi_set_read_chunk_size;

	p->interface_enabled = 1;

	spi_set_drvdata(client,  &p->chip);

	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	ret = 0;
	goto out;
#ifdef CONFIG_OF
out_err_kfree:
#endif
	kfree(p);
out:
	return ret;
}

int spi_common_remove(struct spi_device *client)
{
	struct chip_interface *ci = spi_get_drvdata(client);
	struct dbmdx_spi_private *p = (struct dbmdx_spi_private *)ci->pdata;

	kfree(p);

	spi_set_drvdata(client, NULL);

	return 0;
}


