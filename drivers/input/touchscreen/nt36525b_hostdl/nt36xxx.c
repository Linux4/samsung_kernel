/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 61426 $
 * $Date: 2020-04-29 15:04:36 +0800 (¶g¤T, 29 ?¤ë 2020) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#if defined(CONFIG_FB)
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#endif
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include "nt36xxx.h"
#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if SEC_TOUCH_CMD
int nvt_ts_sec_fn_init(struct nvt_ts_data *ts);
void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts);
int nvt_ts_mode_restore(struct nvt_ts_data *ts);
#endif

#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif

#if NVT_TOUCH_MP
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
#endif

struct nvt_ts_data *ts;

#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
extern void Boot_Update_Firmware(struct work_struct *work);
#endif

#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#else
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void nvt_ts_early_suspend(struct early_suspend *h);
static void nvt_ts_late_resume(struct early_suspend *h);
#endif

uint32_t ENG_RST_ADDR  = 0x7FFF80;
uint32_t SWRST_N8_ADDR = 0; //read from dtsi
uint32_t SPI_RD_FAST_ADDR = 0;	//read from dtsi

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

const uint16_t gesture_key_array[] = {
	KEY_WAKEUP,  //GESTURE_WORD_C
	KEY_WAKEUP,  //GESTURE_WORD_W
	KEY_WAKEUP,  //GESTURE_WORD_V
	KEY_WAKEUP,  //GESTURE_DOUBLE_CLICK
	KEY_WAKEUP,  //GESTURE_WORD_Z
	KEY_WAKEUP,  //GESTURE_WORD_M
	KEY_WAKEUP,  //GESTURE_WORD_O
	KEY_WAKEUP,  //GESTURE_WORD_e
	KEY_WAKEUP,  //GESTURE_WORD_S
	KEY_WAKEUP,  //GESTURE_SLIDE_UP
	KEY_WAKEUP,  //GESTURE_SLIDE_DOWN
	KEY_WAKEUP,  //GESTURE_SLIDE_LEFT
	KEY_WAKEUP,  //GESTURE_SLIDE_RIGHT
};

#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24

#ifdef CONFIG_MTK_SPI
const struct mt_chip_conf spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if defined(CONFIG_SPI_MT65XX)
const struct mtk_chip_config spi_ctrdata = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen irq enable/disable function.

return:
	n.a.
*******************************************************/
static void nvt_irq_enable(bool enable)
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	input_info(true, &ts->client->dev, "enable=%d, desc->depth=%d\n", enable, desc->depth);
}

/*******************************************************
Description:
	Novatek touchscreen spi read/write core function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len , NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};
	int ret;

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	memset(ts->xbuf, 0, len + DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
		case NVTREAD:
			t.tx_buf = ts->xbuf;
			t.rx_buf = ts->rbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = ts->xbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (ts->platdata->cs_gpio > 0)
		gpio_direction_output(ts->platdata->cs_gpio, 0);

	ret = spi_sync(client, &m);

	if (ts->platdata->cs_gpio > 0)
		gpio_direction_output(ts->platdata->cs_gpio, 1);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;

		if (ts->power_status == POWER_OFF_STATUS) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == 5)) {
		input_err(true, &client->dev, "read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;

		if (ts->power_status == POWER_OFF_STATUS) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == 5)) {
		input_err(true, &client->dev, "error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return CTP_SPI_WRITE(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret) {
		input_err(true, &ts->client->dev, "set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret) {
		input_err(true, &ts->client->dev, "write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen enable hw bld crc function.

return:
	N/A.
*******************************************************/
void nvt_bld_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to BLD_CRC_EN_ADDR---
	nvt_set_page(ts->mmap->BLD_CRC_EN_ADDR);

	//---read data from index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = 0xFF;
	CTP_SPI_READ(ts->client, buf, 2);

	//---write data to index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = buf[1] | (0x01 << 7);
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen clear status & enable fw crc function.

return:
	N/A.
*******************************************************/
void nvt_fw_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 2);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen set boot ready function.

return:
	N/A.
*******************************************************/
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	mdelay(5);

	if (!ts->hw_crc) {
		//---write BOOT_RDY status cmds---
		nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*******************************************************
Description:
	Novatek touchscreen eng reset cmd
    function.

return:
	n.a.
*******************************************************/
void nvt_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_write_addr(ENG_RST_ADDR, 0x5A);

	mdelay(1);	//wait tMCU_Idle2TP_REX_Hi after TP_RST
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset(void)
{
	//---software reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0x55);

	msleep(10);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0xAA);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0x69);

	mdelay(5);	//wait tBRST2FR after Bootload RST

	if (SPI_RD_FAST_ADDR) {
		/* disable SPI_RD_FAST */
		nvt_write_addr(SPI_RD_FAST_ADDR, 0x00);
	}
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			input_err(true, &ts->client->dev, "error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 10000);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get novatek project id information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_SPI_READ(ts->client, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];
	ts->fw_ver_ic[1] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_SPI_READ(ts->client, buf, 20);
	ts->fw_ver = buf[1];
	ts->platdata->x_num = buf[3];
	ts->platdata->y_num = buf[4];
	ts->platdata->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->platdata->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		input_err(true, &ts->client->dev, "FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->platdata->x_num = 18;
		ts->platdata->y_num = 32;
		ts->platdata->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->platdata->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if(retry_count < 3) {
			retry_count++;
			input_err(true, &ts->client->dev, "retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			input_err(true, &ts->client->dev, "Set default fw_ver=%d, x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->platdata->x_num, ts->platdata->y_num,
					ts->platdata->abs_x_max, ts->platdata->abs_y_max, ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	input_info(true, &ts->client->dev, "fw_ver = 0x%02X, fw_type = 0x%02X\n", ts->fw_ver, buf[14]);

	// Customized version string start
	//---Get IC name---
	ts->fw_ver_ic[0] = buf[15];
/*
	input_info(true, &ts->client->dev, "buf[0~9] : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]);
	input_info(true, &ts->client->dev, "buf[10~19] : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19]);
*/
	//---Get Novatek PID---
	nvt_read_pid();

	//---get panel id---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PANEL);
	buf[0] = EVENT_MAP_PANEL;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "CTP_SPI_READ error(%d)\n", ret);
		return ret;
	}
	ts->fw_ver_ic[2] = buf[1];

	//---get firmware version---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	buf[0] = EVENT_MAP_FWINFO;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "CTP_SPI_READ error(%d)\n", ret);
		return ret;
	}
	ts->fw_ver_ic[3] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "fw_ver_ic = %02X%02X%02X%02X\n",
		ts->fw_ver_ic[0], ts->fw_ver_ic[1], ts->fw_ver_ic[2], ts->fw_ver_ic[3]);
	// Customized version string end

	input_info(true, &ts->client->dev, "trim_ver_ic = %02X%02X%02X%02X\n",
			trim_id_table[ts->ic_idx].id[5], trim_id_table[ts->ic_idx].id[4],
			trim_id_table[ts->ic_idx].id[3], trim_id_table[ts->ic_idx].id[0]);

	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		input_err(true, &ts->client->dev, "invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		input_err(true, &ts->client->dev, "kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		input_err(true, &ts->client->dev, "kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		input_err(true, &ts->client->dev, "copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				input_err(true, &ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				input_err(true, &ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		input_err(true, &ts->client->dev, "Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
	kfree(buf);
kzalloc_failed:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		input_err(true, &ts->client->dev, "Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: Failed!\n", __func__);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: Succeeded!\n", __func__);
	}

	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);
	input_info(true, &ts->client->dev, "%s: Create /proc/%s\n", __func__, DEVICE_NAME);
	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, DEVICE_NAME);
	}
}
#endif

static int pinctrl_configure(struct nvt_ts_data *ts, bool enable)
{
	struct pinctrl_state *state;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, enable ? "ACTIVE" : "SUSPEND");

	if (enable) {
		state = pinctrl_lookup_state(ts->platdata->pinctrl, "on_state");
		if (IS_ERR(ts->platdata->pinctrl))
			input_info(true, &ts->client->dev, "%s: could not get active pinstate\n", __func__);
	} else {
		state = pinctrl_lookup_state(ts->platdata->pinctrl, "off_state");
		if (IS_ERR(ts->platdata->pinctrl))
			input_info(true, &ts->client->dev, "%s: could not get suspend pinstate\n", __func__);
	}

	if (!IS_ERR_OR_NULL(state)) {
		ret = pinctrl_select_state(ts->platdata->pinctrl, state);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: could not control pinstate\n", __func__);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen parse device tree function.

return:
	n.a.
*******************************************************/
#ifdef CONFIG_OF
static int nvt_parse_dt(struct device *dev)
{
	struct nvt_ts_platdata *platdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	int32_t ret = 0;
	int tmp[3];
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0, dt_lcdtype;
#ifdef CONFIG_MTK_ENABLE_LCDID_CHECK_FOR_TOUCH
	uint32_t tmp_lcd_id[4] = {0};
	int i = 0;
	u32 data = 0;
#endif


	input_info(true, dev, "%s: start!\n", __func__);

#if LCD_SETTING
	if (lcdtype == 0) {
		input_err(true, dev, "%s: panel is not conneted\n", __func__);
		return -EINVAL;
	}

	lcd_id1_gpio = of_get_named_gpio(np, "novatek,lcdid1-gpio", 0);
	if (gpio_is_valid(lcd_id1_gpio))
		input_info(true, dev, "%s: lcd id1_gpio %d(%d)\n", __func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
	else {
		input_err(true, dev, "%s: Failed to get novatek,lcdid1-gpio\n", __func__);
		return -EINVAL;
	}

	lcd_id2_gpio = of_get_named_gpio(np, "novatek,lcdid2-gpio", 0);
	if (gpio_is_valid(lcd_id2_gpio))
		input_info(true, dev, "%s: lcd id2_gpio %d(%d)\n", __func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
	else {
		input_err(true, dev, "%s: Failed to get novatek,lcdid2-gpio\n", __func__);
		return -EINVAL;
	}
	platdata->lcd_id = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);

	if (of_property_count_u32_elems(np, "novatek,lcdtype") <= 0 ||
			of_property_count_strings(np, "novatek,fw_name") <= 0 ||
			of_property_count_strings(np, "novatek,fw_name_mp") <= 0 ||
			of_property_count_u32_elems(np, "novatek,lcdtype") <= platdata->lcd_id ||
			of_property_count_strings(np, "novatek,fw_name") <= platdata->lcd_id ||
			of_property_count_strings(np, "novatek,fw_name_mp") <= platdata->lcd_id) {

		input_err(true, dev, "%s: abnormal lcdtype & fw name count, lcd_id(%d)\n", __func__, platdata->lcd_id);
		return -EINVAL;
	}

	of_property_read_u32_index(np, "novatek,lcdtype", platdata->lcd_id, &dt_lcdtype);
	input_info(true, dev, "%s: lcd id(%d), lcdtype=0x%06X & dt_lcdtype=0x%06X\n",
					__func__, platdata->lcd_id, lcdtype, dt_lcdtype);
#endif

#if defined(CONFIG_MTK_ENABLE_LCDID_CHECK_FOR_TOUCH)
	if (of_property_read_u32(np, "novatek,lcd_id_count", &data) == 0)
		platdata->ssdev_lcd_id_count = data;
	pr_info("%s : lcd_id_count(0x%x)\n", __func__, data);

	ret = of_property_read_u32_array(np, "novatek,lcd_id",
						tmp_lcd_id, platdata->ssdev_lcd_id_count);
	if (ret && (ret != -EINVAL)) {
		input_err(true, dev, " %s:Fail to read lcd_id %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < (int)(platdata->ssdev_lcd_id_count); i++) {
		platdata->ssdev_lcd_id[i] = tmp_lcd_id[i];
		input_err(true, dev, "%s : lcd_id[%d] = 0x%x\n",
					__func__, i, platdata->ssdev_lcd_id[i]);
	}
#endif

	of_property_read_string_index(np, "novatek,fw_name", platdata->lcd_id, &platdata->firmware_name);
	if (platdata->firmware_name == NULL || strlen(platdata->firmware_name) == 0) {
		input_err(true, dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, platdata->firmware_name);
	}

	of_property_read_string_index(np, "novatek,fw_name_mp", platdata->lcd_id, &platdata->firmware_name_mp);
	if (platdata->firmware_name_mp == NULL || strlen(platdata->firmware_name_mp) == 0) {
		input_err(true, dev, "%s: Failed to get mp fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, platdata->firmware_name_mp);
	}

#if LCD_SETTING
	if (of_property_read_string(np, "novatek,regulator_lcd_vdd", &platdata->regulator_lcd_vdd)) {
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "novatek,regulator_lcd_reset", &platdata->regulator_lcd_reset)) {
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "novatek,regulator_lcd_bl", &platdata->regulator_lcd_bl)) {
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);
		return -EINVAL;
	}
#endif
#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	input_info(true, dev, "%s: novatek,reset-gpio=%d\n", __func__, ts->reset_gpio);
#endif
	platdata->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &platdata->irq_flags);
	if (!gpio_is_valid(platdata->irq_gpio)) {
		input_err(true, dev,"failed to get irq-gpio(%d)\n", platdata->irq_gpio);
		return -EINVAL;
	} else if (!platdata->irq_flags) {
		platdata->irq_flags = IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT;
	}
	input_info(true, dev, "%s: novatek,irq-gpio=%d\n", __func__, platdata->irq_gpio);

	platdata->cs_gpio = of_get_named_gpio(np, "novatek,cs-gpio", 0);

	if (!gpio_is_valid(platdata->cs_gpio)) {
		platdata->cs_gpio = -1;
		input_info(true, dev, "%s: gpio_cs value is not valid\n", __func__);

	} else {
		int error;
		input_info(true, dev, "%s: cs_gpio=%d\n", __func__, platdata->cs_gpio);
		error = gpio_request(platdata->cs_gpio, "novatek,cs-gpio");

		if (error < 0) {
			input_err(true, dev, "%s: request gpio_cs pin failed\n", __func__);
		} else {
			gpio_direction_output(platdata->cs_gpio, 1);
		}
	}

	ret = of_property_read_u32(np, "novatek,swrst-n8-addr", &SWRST_N8_ADDR);
	if (ret) {
		input_err(true, dev, "%s: error reading novatek,swrst-n8-addr. ret=%d\n", __func__, ret);
		return -ENXIO;
	} else {
		input_info(true, dev, "%s: SWRST_N8_ADDR=0x%06X\n", __func__, SWRST_N8_ADDR);
	}

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &SPI_RD_FAST_ADDR);
	if (ret) {
		input_info(true, dev, "%s: not support novatek,spi-rd-fast-addr\n", __func__);
		SPI_RD_FAST_ADDR = 0;
//		ret = ERR_PTR(-ENXIO);
	} else {
		input_info(true, dev, "%s: SPI_RD_FAST_ADDR=0x%06X\n", __func__, SPI_RD_FAST_ADDR);
	}

	ret = of_property_read_u32_array(np, "novatek,resolution", tmp, 2);
	if (!ret) {
		platdata->abs_x_max = tmp[0];
		platdata->abs_y_max = tmp[1];
	} else {
		platdata->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		platdata->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
	}

	ret = of_property_read_u32_array(np, "novatek,max_touch_num", tmp, 1);
	if (!ret)
		platdata->max_touch_num = tmp[0];
	else
		platdata->max_touch_num = TOUCH_MAX_FINGER_NUM;

	if (of_property_read_u32_array(np, "novatek,area-size", tmp, 3)) {
		platdata->area_indicator = 48;
		platdata->area_navigation = 96;
		platdata->area_edge = 60;
	} else {
		platdata->area_indicator = tmp[0];
		platdata->area_navigation = tmp[1];
		platdata->area_edge = tmp[2];
	}

	input_info(true, dev, "%s: zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, platdata->area_indicator, platdata->area_navigation, platdata->area_edge);

	input_info(true, dev, "%s: irq_flags: 0x%X, resolution: (%d, %d), max_touch_num: %d\n",
		__func__, platdata->irq_flags,// platdata->firmware_name,
		platdata->abs_x_max, platdata->abs_y_max,
		platdata->max_touch_num);
#if PROXIMITY_FUNCTION
	platdata->support_ear_detect = of_property_read_bool(np, "novatek,support_ear_detect_mode");
	input_info(true, dev, "%s: ED mode %s\n",
				__func__, platdata->support_ear_detect ? "ON" : "OFF");
#endif

	platdata->enable_settings_aot = of_property_read_bool(np, "novatek,enable_settings_aot");
	input_info(true, dev, "%s: AOT mode %s\n",
				__func__, platdata->enable_settings_aot ? "ON" : "OFF");

	input_info(true, dev, "%s: end!\n", __func__);

	return 0;
}
#else
static int nvt_parse_dt(struct device *dev)
{
	struct nvt_ts_platdata *platdata = dev->platform_data;

	input_err(true, dev, "no platform data specified\n");

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	platdata->irq_gpio = NVTTOUCH_INT_PIN;
	return platdata;
}
#endif

static int nvt_regulator_init(struct nvt_ts_data *ts)
{
#if LCD_SETTING
	ts->regulator_vdd = regulator_get(NULL, ts->platdata->regulator_lcd_vdd);
	if (IS_ERR(ts->regulator_vdd)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, "vdd_ldo28");
		return PTR_ERR(ts->regulator_vdd);
	}

	ts->regulator_lcd_reset = regulator_get(NULL, ts->platdata->regulator_lcd_reset);
	if (IS_ERR(ts->regulator_lcd_reset)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, "gpio_lcd_rst");
		return PTR_ERR(ts->regulator_lcd_reset);
	}

	ts->regulator_lcd_bl_en = regulator_get(NULL, ts->platdata->regulator_lcd_bl);
	if (IS_ERR(ts->regulator_lcd_bl_en)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, "gpio_lcd_bl_en");
		return PTR_ERR(ts->regulator_lcd_bl_en);
	}
#endif
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen config and request gpio

return:
	Executive outcomes. 0---succeed. not 0---failed.
*******************************************************/
static int nvt_gpio_config(struct device *dev, struct nvt_ts_platdata *platdata)
{
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_LOW, "NVT-tp-rst");
		if (ret) {
			input_err(true, &ts->client->dev, "Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(platdata->irq_gpio)) {
		ret = gpio_request_one(platdata->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			input_err(true, &ts->client->dev, "Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen deconfig gpio

return:
	n.a.
*******************************************************/
static void nvt_gpio_deconfig(struct nvt_ts_data *ts)
{
	if (gpio_is_valid(ts->platdata->irq_gpio))
		gpio_free(ts->platdata->irq_gpio);
#if NVT_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//input_info(true, &ts->client->dev, "esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: do ESD recovery, timer = %d, retry = %d\n",
					__func__, timer, esd_retry);
		/* do esd recovery, reload fw */
		nvt_update_firmware(ts->platdata->firmware_name);
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt = 0;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
	uint32_t recovery_cnt_max = 10;
	uint8_t recovery_enable = false;
	uint8_t i = 0;

	recovery_cnt++;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
			recovery_cnt = 0;
			break;
		}
	}

	if (recovery_cnt > recovery_cnt_max){
		recovery_enable = true;
		recovery_cnt = 0;
	}

	return recovery_enable;
}
#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

void nvt_print_info(void)
{
	if (!ts)
		return;

	ts->print_info_cnt_open++;

	if (ts->print_info_cnt_open > 0xfff0)
		ts->print_info_cnt_open = 0;

	if (ts->touch_count == 0)
		ts->print_info_cnt_release++;

		input_info(true, &ts->client->dev,
				"tc:%d ver:%02d%02d%02d%02d// #%d %d\n",
				ts->touch_count, ts->fw_ver_bin[0], ts->fw_ver_bin[1],
				ts->fw_ver_bin[2], ts->fw_ver_bin[3],
				ts->print_info_cnt_open, ts->print_info_cnt_release);

}

static void nvt_print_info_work(struct work_struct *work)
{
	nvt_print_info();

	schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}


#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i + 1];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_info(true, &ts->client->dev, "i2c/spi packet checksum not match. (point_data[%d]=0x%02X, checksum=0x%02X)\n",
			length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_info(true, &ts->client->dev, "%02X %02X %02X %02X %02X %02X\n",
				buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_info(true, &ts->client->dev, "%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}
#endif /* POINT_DATA_CHECKSUM */

#define NVT_TS_LOCATION_DETECT_SIZE	6
/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/
static void location_detect(struct nvt_ts_data *ts, char *loc, int x, int y)
{
	int i;

	for (i = 0; i < NVT_TS_LOCATION_DETECT_SIZE; ++i)
		loc[i] = 0;

	if (x < ts->platdata->area_edge)
		strcat(loc, "E.");
	else if (x < (ts->platdata->abs_x_max - ts->platdata->area_edge))
		strcat(loc, "C.");
	else
		strcat(loc, "e.");

	if (y < ts->platdata->area_indicator)
		strcat(loc, "S");
	else if (y < (ts->platdata->abs_y_max - ts->platdata->area_navigation))
		strcat(loc, "C");
	else
		strcat(loc, "N");
}

static void nvt_ts_print_coord(struct nvt_ts_data *ts)
{
	int i;
	char location[NVT_TS_LOCATION_DETECT_SIZE] = { 0, };

	for (i = 0; i < TOUCH_MAX_FINGER_NUM; i++) {
		location_detect(ts, location, ts->coords[i].x, ts->coords[i].y);

		if (ts->coords[i].press && !ts->coords[i].p_press) {
			ts->touch_count++;
//			ts->all_finger_count++;

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "[P] tId:%d.%d x:%d y:%d z:%d loc:%s tc:%d type:%X\n",
				i, (ts->input_dev->mt->trkid - 1) & TRKID_MAX,
				ts->coords[i].x, ts->coords[i].y, ts->coords[i].area,
				location, ts->touch_count, ts->coords[i].status);
#else
			input_info(true, &ts->client->dev, "[P] tId:%d.%d z:%d loc:%s tc:%d type:%X\n",
				i, (ts->input_dev->mt->trkid - 1) & TRKID_MAX,
				ts->coords[i].area, location, ts->touch_count,
				ts->coords[i].status);
#endif
			ts->coords[i].p_press = ts->coords[i].press;
			ts->coords[i].p_x = ts->coords[i].x;
			ts->coords[i].p_y = ts->coords[i].y;

			ts->coords[i].p_status = 0;
			ts->coords[i].move_count = 0;

		} else if (!ts->coords[i].press && ts->coords[i].p_press) {
			ts->touch_count--;

//			if (!ts->touch_count)
//				ts->print_info_cnt_release = 0;

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "[R] tId:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d\n",
				i, location, ts->coords[i].p_x - ts->coords[i].x,
				ts->coords[i].p_y - ts->coords[i].y,
				ts->coords[i].move_count, ts->touch_count,
				ts->coords[i].x, ts->coords[i].y);
#else
			input_info(true, &ts->client->dev, "[R] tId:%d loc:%s dd:%d,%d mc:%d tc:%d\n",
				i, location, ts->coords[i].p_x - ts->coords[i].x,
				ts->coords[i].p_y - ts->coords[i].y,
				ts->coords[i].move_count, ts->touch_count);
#endif
			ts->coords[i].p_press = false;
		} else if (ts->coords[i].press) {
			if (ts->coords[i].p_status && (ts->coords[i].status != ts->coords[i].p_status)) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev, "[C] tId:%d x:%d y:%d z:%d tc:%d type:%X\n",
					i, ts->coords[i].x, ts->coords[i].y, ts->coords[i].area,
					ts->touch_count, ts->coords[i].status);
#else
				input_info(true, &ts->client->dev, "[C] tId:%d z:%d tc:%d type:%X\n",
					i, ts->coords[i].area, ts->touch_count, ts->coords[i].status);
#endif
			}

			ts->coords[i].p_status = ts->coords[i].status;
			ts->coords[i].move_count++;
		}
	}
}

#define POINT_DATA_LEN	108	//with minor support //65 is without minor support

/* customized gesture id */
#define DATA_PROTOCOL	30

/* function page definition */
#define FUNCPAGE_GESTURE	0x01
#if PROXIMITY_FUNCTION
#define FUNCPAGE_PROXIMITY	0x02
#endif

/* touch status definition */
#define FINGER_ENTER		0x01
#define FINGER_MOVING		0x02
#define GLOVE_TOUCH			0x06

/*******************************************************
Description:
	Novatek touchscreen wake up gesture key report function.

return:
	n.a.
*******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];

	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		input_info(true, &ts->client->dev, "gesture_id %d is invalid, func_type=%d, func_id=%d\n",
						gesture_id, func_type, func_id);
		return;
	}

	input_info(true, &ts->client->dev, "gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
		case GESTURE_WORD_C:
			input_info(true, &ts->client->dev, "Gesture : Word-C.\n");
			keycode = gesture_key_array[0];
			break;
		case GESTURE_WORD_W:
			input_info(true, &ts->client->dev, "Gesture : Word-W.\n");
			keycode = gesture_key_array[1];
			break;
		case GESTURE_WORD_V:
			input_info(true, &ts->client->dev, "Gesture : Word-V.\n");
			keycode = gesture_key_array[2];
			break;
		case GESTURE_DOUBLE_CLICK:
			input_info(true, &ts->client->dev, "Gesture : Double Click.\n");
			keycode = gesture_key_array[3];
			break;
		case GESTURE_WORD_Z:
			input_info(true, &ts->client->dev, "Gesture : Word-Z.\n");
			keycode = gesture_key_array[4];
			break;
		case GESTURE_WORD_M:
			input_info(true, &ts->client->dev, "Gesture : Word-M.\n");
			keycode = gesture_key_array[5];
			break;
		case GESTURE_WORD_O:
			input_info(true, &ts->client->dev, "Gesture : Word-O.\n");
			keycode = gesture_key_array[6];
			break;
		case GESTURE_WORD_e:
			input_info(true, &ts->client->dev, "Gesture : Word-e.\n");
			keycode = gesture_key_array[7];
			break;
		case GESTURE_WORD_S:
			input_info(true, &ts->client->dev, "Gesture : Word-S.\n");
			keycode = gesture_key_array[8];
			break;
		case GESTURE_SLIDE_UP:
			input_info(true, &ts->client->dev, "Gesture : Slide UP.\n");
			keycode = gesture_key_array[9];
			break;
		case GESTURE_SLIDE_DOWN:
			input_info(true, &ts->client->dev, "Gesture : Slide DOWN.\n");
			keycode = gesture_key_array[10];
			break;
		case GESTURE_SLIDE_LEFT:
			input_info(true, &ts->client->dev, "Gesture : Slide LEFT.\n");
			keycode = gesture_key_array[11];
			break;
		case GESTURE_SLIDE_RIGHT:
			input_info(true, &ts->client->dev, "Gesture : Slide RIGHT.\n");
			keycode = gesture_key_array[12];
			break;
		default:
			break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}

#if PROXIMITY_FUNCTION
void nvt_ts_proximity_report(uint8_t *data)
{
	struct nvt_ts_event_proximity *p_event_proximity;
	u8 status = 0;

	 p_event_proximity = (struct nvt_ts_event_proximity *)&data[1];

	/* check data page */
	if (p_event_proximity->data_page != FUNCPAGE_PROXIMITY) {
		input_info(true, &ts->client->dev, "proximity data page %d is invalid\n", p_event_proximity->data_page);
		return;
	}

	status = p_event_proximity->status;

	input_info(true, &ts->client->dev, "proximity->status = %d\n", status);

	input_info(true, &ts->client->dev, "%s hover : %d\n", __func__, status);
	ts->hover_event = status;
	input_report_abs(ts->input_dev_proximity, ABS_MT_CUSTOM, status);
	input_sync(ts->input_dev_proximity);

#if 0
	switch (p_event_proximity->status) {
		/* Case amount depends on the status amount */
		case 0:
			input_info(true, &ts->client->dev, "Proximity State 0.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 1:
			input_info(true, &ts->client->dev, "Proximity State 1.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 2:
			input_info(true, &ts->client->dev, "Proximity State 2.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 3:
			input_info(true, &ts->client->dev, "Proximity State 3.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 4:
			input_info(true, &ts->client->dev, "Proximity State 4.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 5:
			input_info(true, &ts->client->dev, "Proximity State 5.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		default:
			break;
	}
#endif

}
#endif

/*******************************************************
Description:
	Novatek touchscreen work function.

return:
	n.a.
*******************************************************/
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	struct nvt_ts_platdata *platdata = ts->platdata;
	struct nvt_ts_event_coord *p_event_coord;
	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_LEN + 1 + DUMMY_BYTES] = {0};
	u16 x = 0, y = 0, w = 0, p = 0;
	u8 id = 0, status = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */
	int32_t i = 0;
	int32_t finger_cnt = 0;

#if PROXIMITY_FUNCTION
		if ((ts->power_status == POWER_LPM_STATUS) || (ts->power_status == POWER_PROX_STATUS))
#else
		if (ts->power_status == POWER_LPM_STATUS)
#endif
		pm_wakeup_event(&ts->input_dev->dev, 5000);

	mutex_lock(&ts->lock);

	ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}

/*
	//--- dump SPI buf ---
	for (i = 0; i < 10; i++) {
		printk("%02X %02X %02X %02X %02X %02X  ",
			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
*/

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		input_err(true, &ts->client->dev, "Recover for fw reset, %02X\n", point_data[1]);
		nvt_update_firmware(ts->platdata->firmware_name);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
	if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
		ret = nvt_ts_point_data_checksum(point_data, POINT_DATA_CHECKSUM_LEN);
		if (ret) {
			goto XFER_ERROR;
		}
	}
#endif /* POINT_DATA_CHECKSUM */

	if (ts->power_status == POWER_LPM_STATUS && ts->aot_enable) {
		id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(id, point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}

#if PROXIMITY_FUNCTION
	// Add "ts->power_status" and "ts->ear_detect_enable" checking if necessary
	if ((uint8_t)(point_data[1] >> 3) == DATA_PROTOCOL) {
		nvt_ts_proximity_report(point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}

	if(ts->power_status == POWER_PROX_STATUS && ts->prox_power_off) {
		input_info(true, &ts->client->dev, "%s : prox_on && prox_power_off\n", __func__);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}
#endif

	for (i = 0, finger_cnt = 0; i < platdata->max_touch_num; i++) {
		p_event_coord = (struct nvt_ts_event_coord *)&point_data[1 + 6 * i];
		id = p_event_coord->id;
		if (!id || (id > platdata->max_touch_num))
			continue;

		id = id - 1;
		status = p_event_coord->status;
		if ((status == FINGER_ENTER) || (status == FINGER_MOVING) || (status == GLOVE_TOUCH)) {
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
			ts->coords[id].status = status;
			x = ts->coords[id].x = (u32)(p_event_coord->x_11_4 << 4) + (u32)(p_event_coord->x_3_0);
			y = ts->coords[id].y = (u32)(p_event_coord->y_11_4 << 4) + (u32)(p_event_coord->y_3_0);
			if ((x < 0) || (y < 0) || (x > platdata->abs_x_max) || (y > platdata->abs_y_max)) {
				input_err(true, &ts->client->dev, "%s: invalid coordinate (%d, %d)\n",
					__func__, x, y);
				continue;
			}

			w = ts->coords[id].area = (u32)(p_event_coord->area);
			if (!w)
				w = 1;

			if (i < 2) {
				p = (u32)(p_event_coord->pressure_7_0) + (u32)(point_data[i + 62] << 8);
				if (p > TOUCH_FORCE_NUM)
					p = TOUCH_FORCE_NUM;
			} else {
				p = (u32)(p_event_coord->pressure_7_0);
			}

			if (!p)
				p = 1;

			ts->coords[id].p = p;

#if MT_PROTOCOL_B
			press_id[id] = ts->coords[id].press = true;
			input_mt_slot(ts->input_dev, id);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, p);

#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
			input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

			finger_cnt++;
		}
	}

#if MT_PROTOCOL_B
	for (i = 0; i < platdata->max_touch_num; i++) {
		if (!press_id[i] && ts->coords[i].p_press) {
			ts->coords[i].press = false;

			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
	if (finger_cnt == 0) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
#endif /* MT_PROTOCOL_B */

#if TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[62] >> i) & 0x01));
		}
	} else {
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], 0);
		}
	}
#endif

	input_sync(ts->input_dev);

	nvt_ts_print_coord(ts);

XFER_ERROR:

	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}


/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(uint32_t chip_ver_trim_addr)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	nvt_bootloader_reset(); // NOT in retry loop

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		nvt_set_page(chip_ver_trim_addr);

		buf[0] = chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_READ(ts->client, buf, 7);
		input_info(true, &ts->client->dev, "buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				input_info(true, &ts->client->dev, "This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
				ts->ic_idx = list;
				//ts->fw_ver_ic[0] = ((buf[6] & 0x0F) << 4) | ((buf[5] & 0xF0) >> 4);
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

#if 0	// mtk
static unsigned int lcdtype;
static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	printk("%s: lcdtype: %6X\n", __func__, lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);
#endif

static void ts_remaining_var_init(void)
{
//	u8 i;

	input_info(true, &ts->client->dev, "%s : start\n", __func__);

#if PROXIMITY_FUNCTION
	ts->ear_detect_enable = 0;
	ts->prox_power_off = 0;
	ts->hover_event = 0;
#endif

#if SEC_TOUCH_CMD
	ts->sec_function = 0;
#endif

	// clear ts->coords data
	memset(ts->coords, 0, sizeof(ts->coords));
/*
	for (i = 0; i < ts->platdata->max_touch_num; i++) {
		ts->coords[i].press = false;
		ts->coords[i].p_press = false;
	}
*/
	if (!ts->client->irq) {
		ts->irq_enabled = false;
	}

	ts->touch_count = 0;
	ts->isUMS = false;
	ts->aot_enable = 0;
	ts->power_status = POWER_ON_STATUS;

	input_info(true, &ts->client->dev, "%s : end\n", __func__);
}

#ifdef CONFIG_MTK_ENABLE_LCDID_CHECK_FOR_TOUCH
static int _lcm_lcd_id_dt(void)
{
	struct device_node *root = of_find_node_by_path("/");
	int _lcd_id_gpio1, _lcd_id_gpio2, _lcd_id_gpio3;
	int _lcd_id = 0;

	pr_info("%s\n", __func__);
	if (IS_ERR_OR_NULL(root)) {
		pr_info("root dev node is NULL\n");
		return -1;
	}
	_lcd_id_gpio1 = of_get_named_gpio(root, "dtbo-lcd_id_1", 0);
	_lcd_id_gpio2 = of_get_named_gpio(root, "dtbo-lcd_id_2", 0);
	_lcd_id_gpio3 = of_get_named_gpio(root, "dtbo-lcd_id_3", 0);

	if ((_lcd_id_gpio1 < 0) || (_lcd_id_gpio2 < 0) || (_lcd_id_gpio3 < 0)) {
		pr_info("%s, _lcd_id_gpio is invalid\n", __func__);
		return -1;
	}
	/* TO DO : read gpio value */
	_lcd_id = (gpio_get_value(_lcd_id_gpio3) << 2) |
				(gpio_get_value(_lcd_id_gpio2) << 1) |
				(gpio_get_value(_lcd_id_gpio1));

	pr_info("[TOUCH][lcm_nt36525] lcd_id 0x%x\n", _lcd_id);
	return _lcd_id;
}
#endif

/*******************************************************
Description:
	Novatek touchscreen driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int32_t nvt_ts_probe(struct spi_device *client)
{
	int32_t ret = 0;
	int32_t index = 0;
	struct nvt_ts_platdata *platdata;
#ifdef CONFIG_MTK_ENABLE_LCDID_CHECK_FOR_TOUCH
	int32_t i = 0;
	int32_t lcd_id = 0;
#endif


	input_info(true, &client->dev, "%s : start\n", __func__);

	ts = kmalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		input_err(true, &client->dev, "failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->xbuf = (uint8_t *)kzalloc((NVT_TRANSFER_LEN+1+DUMMY_BYTES), GFP_KERNEL);
	if(ts->xbuf == NULL) {
		input_err(true, &client->dev, "kzalloc for xbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_xbuf;
	}

	ts->rbuf = (uint8_t *)kzalloc(NVT_READ_LEN, GFP_KERNEL);
	if(ts->rbuf == NULL) {
		input_err(true, &client->dev, "kzalloc for rbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_rbuf;
	}

	if (client->dev.of_node) {
		platdata = devm_kzalloc(&client->dev,sizeof(struct nvt_ts_platdata), GFP_KERNEL);
		if (!platdata) {
			input_err(true, &client->dev, "%s: Failed to allocate platform data\n", __func__);
			goto err_alloc_platdata_failed;
		}

		client->dev.platform_data = platdata;
		ret = nvt_parse_dt(&client->dev);
		if (ret < 0) {
			input_err(true, &client->dev, "%s: Failed to parse dt(%d)\n", __func__, ret);
			goto err_parse_dt_failed;
		}
#ifdef CONFIG_MTK_ENABLE_LCDID_CHECK_FOR_TOUCH
		lcd_id = _lcm_lcd_id_dt();
		if (lcd_id < 0) {
			pr_info("%s: failed to read lcd_id\n", __func__);
			goto err_parse_dt_failed;
		}

		for (i = 0; i < (int)(platdata->ssdev_lcd_id_count); i++) {
			if (lcd_id == platdata->ssdev_lcd_id[i])
				break;
		}

		pr_info("//// %s: i= %d\n", __func__, i);
		if (i == (int)platdata->ssdev_lcd_id_count) {
			ret = -1;
			pr_info("nt36xxx : %s : LCD HW ID doesn't match\n", __func__);
			goto err_parse_dt_failed;
		}
#endif
	}

	//---request and config GPIOs---
	ret = nvt_gpio_config(&client->dev, platdata);
	if (ret) {
		input_err(true, &client->dev, "gpio config error!\n");
		goto err_gpio_config_failed;
	}

	ts->client = client;
	ts->platdata = platdata;
	spi_set_drvdata(client, ts);

	ts->platdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(ts->platdata->pinctrl))
		input_info(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);
	else
		pinctrl_configure(ts, true);

	nvt_regulator_init(ts);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true, &ts->client->dev, "Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}
	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

	ret = spi_setup(ts->client);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#if 0 // def CONFIG_MTK_SPI
	/* old usage of MTK spi API */
	memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mt_chip_conf));
	ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

#if defined(CONFIG_SPI_MT65XX)
	/* new usage of MTK spi API */
	memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mtk_chip_config));
	ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

	input_info(true, &ts->client->dev, "mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);
/*
	//---parse dts---
	ret = nvt_parse_dt(&client->dev);
	if (ret) {
		input_err(true, &ts->client->dev, "parse dt error\n");
		goto err_spi_setup;
	}

	//---request and config GPIOs---
	ret = nvt_gpio_config(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "gpio config error!\n");
		goto err_gpio_config_failed;
	}
*/
	ts->power_status = POWER_ON_STATUS; /* STANLEY TEST */
	
	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	//---eng reset before TP_RESX high
	nvt_eng_reset();

#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// need 10ms delay after POR(power on reset)
	msleep(10);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_ADDR);
	if (ret) {
		input_info(true, &ts->client->dev, "try to check from old chip ver trim address\n");
		ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_OLD_ADDR);
		if (ret) {
			input_err(true, &ts->client->dev, "chip is not identified\n");
			ret = -EINVAL;
			goto err_chipvertrim_failed;
		}
	}

	//---allocate input device---
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		input_err(true, &ts->client->dev, "allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

#if PROXIMITY_FUNCTION
	//---allocate input proximity device---
	ts->input_dev_proximity = input_allocate_device();
	if (ts->input_dev_proximity == NULL) {
		input_err(true, &ts->client->dev, "%s: allocate input proximity device failed\n", __func__);
		ret = -ENOMEM;
		goto err_input_dev_prox_alloc_failed;
	}
#endif

	// remove below settings due to re-init @ nvt_parse_dt()
/*
	ts->platdata->max_touch_num = TOUCH_MAX_FINGER_NUM;
*/
#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#if PROXIMITY_FUNCTION
	ts->input_dev->keybit[BIT_WORD(KEY_INT_CANCEL)] = BIT_MASK(KEY_INT_CANCEL);
#endif
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->platdata->max_touch_num, 0);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //area = 255

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->platdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->platdata->abs_y_max, 0, 0);
#if MT_PROTOCOL_B
	// no need to set ABS_MT_TRACKING_ID, input_mt_init_slots() already set it
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->platdata->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if TOUCH_KEY_NUM > 0
	for (index = 0; index < ts->max_button_num; index++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[index]);
	}
#endif

	for (index = 0; index < (sizeof(gesture_key_array) / sizeof(gesture_key_array[0])); index++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[index]);
	}

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_SPI;

#if PROXIMITY_FUNCTION
	//---set input proximity device info.---
	ts->input_dev_proximity->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_SW);
	ts->input_dev_proximity->propbit[0] = BIT(INPUT_PROP_DIRECT);


	input_set_abs_params(ts->input_dev_proximity, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);

	ts->input_dev_proximity->name = "sec_touchproximity";
	ts->input_dev_proximity->phys = ts->phys;
	ts->input_dev_proximity->id.bustype = BUS_SPI;
#endif

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		input_err(true, &ts->client->dev, "register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

#if PROXIMITY_FUNCTION
	//---register input proximity device---
	ret = input_register_device(ts->input_dev_proximity);
	if (ret) {
		input_err(true, &client->dev, "register input proximity device (%s) failed. ret=%d\n", ts->input_dev_proximity->name, ret);
		goto err_input_register_proximity_device_failed;
	}
#endif

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(platdata->irq_gpio);
	if (client->irq) {
		input_info(true, &ts->client->dev, "irq_flags=0x%X\n", ts->platdata->irq_flags);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_ts_work_func,
						ts->platdata->irq_flags, NVT_SPI_NAME, ts);
		if (ret != 0) {
			input_err(true, &ts->client->dev, "request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			input_err(true, &ts->client->dev, "request irq %d succeed\n", client->irq);
		}
	}

#if PROXIMITY_FUNCTION
//comment this if condition due to LPWG also need this feature
//	if (ts->platdata->support_ear_detect)
#endif
		device_init_wakeup(&ts->input_dev->dev, 1);

#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = alloc_workqueue("nvt_fwu_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!nvt_fwu_wq) {
		input_err(true, &ts->client->dev, "nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work, msecs_to_jiffies(14000));
#endif

	input_info(true, &ts->client->dev, "NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_esd_check_wq) {
		input_err(true, &ts->client->dev, "nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	INIT_DELAYED_WORK(&ts->work_print_info, nvt_print_info_work);

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		input_err(true, &ts->client->dev, "nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		input_err(true, &ts->client->dev, "nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		input_err(true, &ts->client->dev, "nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif

#if SEC_TOUCH_CMD
	ret = nvt_ts_sec_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to init for factory function\n");
		goto err_init_sec_fn;
	}
#endif

#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	ts->drm_notif.notifier_call = nvt_drm_notifier_callback;
	ret = msm_drm_register_client(&ts->drm_notif);
	if(ret) {
		input_err(true, &ts->client->dev, "register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#else
	ts->fb_notif.notifier_call = nvt_fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if(ret) {
		input_err(true, &ts->client->dev, "register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = nvt_ts_early_suspend;
	ts->early_suspend.resume = nvt_ts_late_resume;
	ret = register_early_suspend(&ts->early_suspend);
	if(ret) {
		input_err(true, &ts->client->dev, "register early suspend failed. ret=%d\n", ret);
		goto err_register_early_suspend_failed;
	}
#endif

	ts_remaining_var_init();

	nvt_irq_enable(true);
	input_err(true, &client->dev, "%s : end\n", __func__);

	return 0;

#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	if (msm_drm_unregister_client(&ts->drm_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering drm_notifier.\n");
err_register_drm_notif_failed:
#else
	if (fb_unregister_client(&ts->fb_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering fb_notifier.\n");
err_register_fb_notif_failed:
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
err_register_early_suspend_failed:
#endif
#if SEC_TOUCH_CMD
err_init_sec_fn:
	nvt_ts_sec_fn_remove(ts);
#endif
#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
err_mp_proc_init_failed:
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
err_create_nvt_fwu_wq_failed:
#endif
#if PROXIMITY_FUNCTION
//comment this if condition due to LPWG also need this feature
//	if (ts->platdata->support_ear_detect)
#endif
		device_init_wakeup(&ts->input_dev->dev, 0);
	free_irq(client->irq, ts);
err_int_request_failed:
#if PROXIMITY_FUNCTION
	input_unregister_device(ts->input_dev_proximity);
err_input_register_proximity_device_failed:
#endif
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device_failed:
#if PROXIMITY_FUNCTION
	if (ts->input_dev_proximity) {
		input_free_device(ts->input_dev_proximity);
		ts->input_dev_proximity = NULL;
	}
err_input_dev_prox_alloc_failed:
#endif
	if (ts->input_dev) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
err_chipvertrim_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
	nvt_gpio_deconfig(ts);
err_spi_setup:
err_ckeck_full_duplex:
	spi_set_drvdata(client, NULL);
#if LCD_SETTING
	regulator_put(ts->regulator_vdd);
	regulator_put(ts->regulator_lcd_reset);
	regulator_put(ts->regulator_lcd_bl_en);
#endif
err_gpio_config_failed:
err_parse_dt_failed:
	if (ts->platdata) {
		devm_kfree(&client->dev, ts->platdata);
		ts->platdata = NULL;
	}
err_alloc_platdata_failed:
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}
err_malloc_rbuf:
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
err_malloc_xbuf:
	if (ts) {
		kfree(ts);
		ts = NULL;
	}
	pr_info("%s : end - fail unload driver\n", __func__);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver release function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_remove(struct spi_device *client)
{
	input_info(true, &ts->client->dev, "Removing driver...\n");

	cancel_delayed_work_sync(&ts->work_print_info);

#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	if (msm_drm_unregister_client(&ts->drm_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

#if SEC_TOUCH_CMD
	nvt_ts_sec_fn_remove(ts);
#endif
#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if PROXIMITY_FUNCTION
//comment this if condition due to LPWG also need this feature
//	if (ts->platdata->support_ear_detect)
#endif
		device_init_wakeup(&ts->input_dev->dev, 0);

	nvt_irq_enable(false);
	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_gpio_deconfig(ts);
#if LCD_SETTING
	regulator_put(ts->regulator_vdd);
	regulator_put(ts->regulator_lcd_reset);
	regulator_put(ts->regulator_lcd_bl_en);
#endif

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}

#if PROXIMITY_FUNCTION
	if (ts->input_dev_proximity) {
		input_mt_destroy_slots(ts->input_dev_proximity);
		input_unregister_device(ts->input_dev_proximity);
		input_free_device(ts->input_dev_proximity);
		ts->input_dev_proximity = NULL;
	}
#endif

	spi_set_drvdata(client, NULL);

	if (ts->platdata) {
		devm_kfree(&client->dev, ts->platdata);
		ts->platdata = NULL;
	}
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
	if (ts) {
		kfree(ts);
		ts = NULL;
	}

	return 0;
}

static void nvt_ts_shutdown(struct spi_device *client)
{
	input_info(true, &ts->client->dev, "Shutdown driver...\n");

	cancel_delayed_work_sync(&ts->work_print_info);

	nvt_irq_enable(false);

#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
	if (msm_drm_unregister_client(&ts->drm_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts->fb_notif))
		input_err(true, &ts->client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

#if SEC_TOUCH_CMD
	nvt_ts_sec_fn_remove(ts);
#endif
#if NVT_TOUCH_MP
	nvt_mp_proc_deinit();
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq) {
		cancel_delayed_work_sync(&ts->nvt_fwu_work);
		destroy_workqueue(nvt_fwu_wq);
		nvt_fwu_wq = NULL;
	}
#endif

#if PROXIMITY_FUNCTION
//comment this if condition due to LPWG also need this feature
//	if (ts->platdata->support_ear_detect)
#endif
		device_init_wakeup(&ts->input_dev->dev, 0);
}

int nvt_ts_lcd_power_ctrl(bool on)
{
#if LCD_SETTING
	int retval;
#endif
	static bool enabled;

	if (enabled == on)
		return 0;

#if LCD_SETTING
	if (on) {
		retval = regulator_enable(ts->regulator_vdd);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_vdd: %d\n", __func__, retval);
			return retval;
		}
		retval = regulator_enable(ts->regulator_lcd_reset);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_reset: %d\n", __func__, retval);
			return retval;
		}
		retval = regulator_enable(ts->regulator_lcd_bl_en);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_bl_en: %d\n", __func__, retval);
			return retval;
		}
	} else {
		regulator_disable(ts->regulator_vdd);
		regulator_disable(ts->regulator_lcd_reset);
		regulator_disable(ts->regulator_lcd_bl_en);
	}
#endif

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen driver suspend function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_suspend(struct device *dev)
{
//	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	uint8_t buf[4] = {0};
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif

	if (ts->power_status == POWER_OFF_STATUS) {
		input_info(true, &ts->client->dev, "Touch is already in DSTB mode\n");
		return 0;
	} else if (ts->power_status == POWER_LPM_STATUS) {
		input_info(true, &ts->client->dev, "Touch is already in LPWG mode\n");
		return 0;
	}

#if NVT_TOUCH_ESD_PROTECT
	input_info(true, &ts->client->dev, "cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	input_info(true, &ts->client->dev, "%s  : start\n", __func__);

	nvt_irq_enable(false);

	mutex_lock(&ts->lock);

#if PROXIMITY_FUNCTION
	if (ts->ear_detect_enable) {
/* comment this and left it to "prox_power_off_store" & "prox_lp_scan_mode" only */
//		ts->power_status = POWER_PROX_STATUS;

		/* prox scan stop */
/*
		buf[0] = 0x7F;
		buf[1] = 0x08;
		CTP_SPI_WRITE(ts->client, buf, 2);
*/
		input_info(true, &ts->client->dev, "In proximity mode\n");
		goto skip_cmd;
	}
#endif

	if (ts->aot_enable) {
		ts->power_status = POWER_LPM_STATUS;

		/* LPWG enter */
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x13;
		CTP_SPI_WRITE(ts->client, buf, 2);

		input_info(true, &ts->client->dev, "%s: lpm mode %d\n", __func__, ts->power_status);
	} else {
		if (!IS_ERR(ts->platdata->pinctrl))
			pinctrl_configure(ts, false);
		/* DSTB enter */
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x11;
		CTP_SPI_WRITE(ts->client, buf, 2);

		ts->power_status = POWER_OFF_STATUS;
		input_info(true, &ts->client->dev, "%s: power off %d\n", __func__, ts->power_status);
	}

	mutex_unlock(&ts->lock);
#if PROXIMITY_FUNCTION
skip_cmd:
#endif
	if (ts->power_status != POWER_OFF_STATUS) {
		nvt_ts_lcd_power_ctrl(true);
		if (device_may_wakeup(&ts->client->dev))
			enable_irq_wake(ts->client->irq);
		nvt_irq_enable(true);
	}

#if PROXIMITY_FUNCTION
	if (ts->prox_power_off) {
		input_info(true, &ts->client->dev, "%s : cancel touch\n", __func__);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->input_dev);
	}
#endif

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->platdata->max_touch_num; i++) {
		ts->coords[i].press = false;
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	nvt_ts_print_coord(ts);

	msleep(50);
	input_info(true, &ts->client->dev, "%s : %s 0x%02X\n", __func__,
			ts->aot_enable ? "Enter lpwg mode" : "Enter dstb mode", ts->aot_enable);

	cancel_delayed_work(&ts->work_print_info);
	nvt_print_info();

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}
static void nvt_ts_early_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s : start\n", __func__);

	if (ts->power_status != POWER_OFF_STATUS) {
		nvt_ts_lcd_power_ctrl(false);
		msleep(20);
	}
}

/*******************************************************
Description:
	Novatek touchscreen driver resume function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_resume(struct device *dev)
{
//	struct nvt_ts_data *ts = dev_get_drvdata(dev);
//	u8 buf[3] = {0};

	if (ts->power_status == POWER_ON_STATUS) {
		input_info(true, &ts->client->dev, "%s: Touch is already resume\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s : start\n", __func__);

	mutex_lock(&ts->lock);

#if PROXIMITY_FUNCTION
	// comment below to left dehind FW update
	//ts->prox_power_off = 0;
#endif

	if (!IS_ERR(ts->platdata->pinctrl))
		pinctrl_configure(ts, true);

	if (ts->power_status != POWER_OFF_STATUS) {
		if (device_may_wakeup(&ts->client->dev))
			disable_irq_wake(ts->client->irq);
	}

	ts->power_status = POWER_ON_STATUS;

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	if (nvt_update_firmware(ts->platdata->firmware_name)) {
		input_err(true, &ts->client->dev, "download firmware failed, ignore check fw state\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}

#if PROXIMITY_FUNCTION
/* comment this cmd, due to use nvt_ts_mode_restore() later
	if (ts->ear_detect_enable) {
		input_info(true, &ts->client->dev, "%s ear_detect_enable write start : ear_detect_enable = %d\n", __func__, ts->ear_detect_enable);
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = PROXIMITY_ENTER;
		CTP_SPI_WRITE(ts->client, buf, 2);
		input_info(true, &ts->client->dev, "%s ear_detect_enable write done\n", __func__);
	}
*/
#endif
#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	mutex_unlock(&ts->lock);

#if PROXIMITY_FUNCTION
	if (ts->input_dev_proximity) {
		input_report_abs(ts->input_dev_proximity, ABS_MT_CUSTOM, 0);
		input_sync(ts->input_dev_proximity);
	}
#endif

	// uncomment below function call to restore sec cmd setting before suspend
#if SEC_TOUCH_CMD
	nvt_ts_mode_restore(ts);
#endif
#if PROXIMITY_FUNCTION
	// After resume, DDIC should be in sleep-out mode
	ts->prox_power_off = 0;
#endif
	nvt_irq_enable(true);

	cancel_delayed_work(&ts->work_print_info);
	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}


#if defined(CONFIG_FB)
#ifdef _MSM_DRM_NOTIFY_H_
static int nvt_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				input_info(true, &ts->client->dev, "event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_suspend(&ts->client->dev);
			}
		} else if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				input_info(true, &ts->client->dev, "event=%lu, *blank=%d\n", event, *blank);
				nvt_ts_resume(&ts->client->dev);
			}
		}
	}

	return 0;
}
#else
static int nvt_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct nvt_ts_data *ts =
		container_of(self, struct nvt_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			input_info(true, &ts->client->dev, "event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_suspend(&ts->client->dev);
		} else if (*blank == FB_BLANK_UNBLANK) {
			input_info(true, &ts->client->dev, "event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_early_resume(&ts->client->dev);
		}
	} else if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			input_info(true, &ts->client->dev, "event=%lu, *blank=%d\n", event, *blank);
			nvt_ts_resume(&ts->client->dev);
		}
	}

	return 0;
}
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*******************************************************
Description:
	Novatek touchscreen driver early suspend function.

return:
	n.a.
*******************************************************/
static void nvt_ts_early_suspend(struct early_suspend *h)
{
	nvt_ts_suspend(ts->client, PMSG_SUSPEND);
}

/*******************************************************
Description:
	Novatek touchscreen driver late resume function.

return:
	n.a.
*******************************************************/
static void nvt_ts_late_resume(struct early_suspend *h)
{
	nvt_ts_resume(ts->client);
}
#endif

static const struct spi_device_id nvt_ts_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id nvt_match_table[] = {
	{ .compatible = "novatek,NVT-ts-spi",},
	{ },
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
	},
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	pr_info("[sec_input] %s : start\n", __func__);

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		pr_err("[sec_input] failed to add spi driver");
		goto err_driver;
	}

	pr_info("[sec_input] %s : finished\n", __func__);

err_driver:
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit nvt_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}

//late_initcall(nvt_driver_init);
module_init(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
