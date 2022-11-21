/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x.h"

#define UPDATE_PASS		0
#define UPDATE_FAIL		-1
#define TIMEOUT_SECTOR		500
#define TIMEOUT_PAGE		3500
#define TIMEOUT_PROGRAM		10

static struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	int delay_after_upgrade;
	bool isCRC;
	bool isboot;
	bool is80k;
	int hex_tag;
} tfd;

static struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
} fbi[FW_BLOCK_INFO_NUM];

static u8 *pfw;
static u8 *CTPM_FW;

static u32 HexToDec(char *phex, s32 len)
{
	u32 ret = 0, temp = 0, i;
	s32 shift = (len - 1) * 4;

	for (i = 0; i < len; shift -= 4, i++) {
		if ((phex[i] >= '0') && (phex[i] <= '9'))
			temp = phex[i] - '0';
		else if ((phex[i] >= 'a') && (phex[i] <= 'f'))
			temp = (phex[i] - 'a') + 10;
		else if ((phex[i] >= 'A') && (phex[i] <= 'F'))
			temp = (phex[i] - 'A') + 10;

		ret |= (temp << shift);
	}
	return ret;
}

static int CalculateCRC32(u32 start_addr, u32 len, u8 *pfw)
{
	int i = 0, j = 0;
	int crc_poly = 0x04C11DB7;
	int tmp_crc = 0xFFFFFFFF;

	for (i = start_addr; i < start_addr + len; i++) {
		tmp_crc ^= (pfw[i] << 24);

		for (j = 0; j < 8; j++) {
			if ((tmp_crc & 0x80000000) != 0)
				tmp_crc = tmp_crc << 1 ^ crc_poly;
			else
				tmp_crc = tmp_crc << 1;
		}
	}
	return tmp_crc;
}

static int calc_hw_dma_crc(u32 start_addr, u32 block_size)
{
	int count = 50;
	u32 busy = 0;

	if (ilits->chip->dma_reset) {
		ILI_DBG("%s operate dma reset in reg after tp reset\n", __func__);
		if (ili_ice_mode_write(0x40040, 0x00800000, 4) < 0)
			input_err(true, ilits->dev, "%s Failed to open DMA reset\n", __func__);
		if (ili_ice_mode_write(0x40040, 0x00000000, 4) < 0)
			input_err(true, ilits->dev, "%s Failed to close DMA reset\n", __func__);
	}
	/* dma1 src1 address */
	if (ili_ice_mode_write(0x072104, start_addr, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 src1 address failed\n", __func__);
	/* dma1 src1 format */
	if (ili_ice_mode_write(0x072108, 0x80000001, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 src1 format failed\n", __func__);
	/* dma1 dest address */
	if (ili_ice_mode_write(0x072114, 0x0002725C, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 src1 format failed\n", __func__);
	/* dma1 dest format */
	if (ili_ice_mode_write(0x072118, 0x80000000, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 dest format failed\n", __func__);
	/* Block size*/
	if (ili_ice_mode_write(0x07211C, block_size, 4) < 0)
		input_err(true, ilits->dev, "%s Write block size (%d) failed\n", __func__, block_size);
	/* crc off */
	if (ili_ice_mode_write(0x041016, 0x00, 1) < 0)
		input_err(true, ilits->dev, "%s Write crc of failed\n", __func__);
	/* dma crc */
	if (ili_ice_mode_write(0x041017, 0x03, 1) < 0)
		input_err(true, ilits->dev, "%s Write dma 1 crc failed\n", __func__);
	/* crc on */
	if (ili_ice_mode_write(0x041016, 0x01, 1) < 0)
		input_err(true, ilits->dev, "%s Write crc on failed\n", __func__);
	/* Dma1 stop */
	if (ili_ice_mode_write(0x072100, 0x02000000, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 stop failed\n", __func__);
	/* clr int */
	if (ili_ice_mode_write(0x048006, 0x2, 1) < 0)
		input_err(true, ilits->dev, "%s Write clr int failed\n", __func__);
	/* Dma1 start */
	if (ili_ice_mode_write(0x072100, 0x01000000, 4) < 0)
		input_err(true, ilits->dev, "%s Write dma1 start failed\n", __func__);

	/* Polling BIT0 */
	while (count > 0) {
		mdelay(1);
		if (ili_ice_mode_read(0x048006, &busy, sizeof(u8)) < 0)
			input_err(true, ilits->dev, "%s Read busy error\n", __func__);
		ILI_DBG("%s busy = %x\n", __func__, busy);
		if ((busy & 0x02) == 2)
			break;
		count--;
	}

	if (count <= 0) {
		input_err(true, ilits->dev, "%s BIT0 is busy\n", __func__);
		return -1;
	}

	if (ili_ice_mode_read(0x04101C, &busy, sizeof(u32)) < 0) {
		input_err(true, ilits->dev, "%s Read dma crc error\n", __func__);
		return -1;
	}
	return busy;
}

static int ilitek_tddi_fw_iram_read(u8 *buf, u32 start, int len)
{
	int limit = SPI_RX_BUF_SIZE;
	int addr = 0, loop = 0, tmp_len = len, cnt = 0;
	u8 cmd[4] = {0};

	if (!buf) {
		input_err(true, ilits->dev, "%s buf is null\n", __func__);
		return -ENOMEM;
	}

	if (len % limit)
		loop = (len / limit) + 1;
	else
		loop = len / limit;

	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		if (tmp_len > limit)
			tmp_len = limit;

		cmd[0] = 0x25;
		cmd[3] = (char)((addr & 0x00FF0000) >> 16);
		cmd[2] = (char)((addr & 0x0000FF00) >> 8);
		cmd[1] = (char)((addr & 0x000000FF));

		if (ilits->wrapper(cmd, 4, NULL, 0, OFF, OFF) < 0) {
			input_err(true, ilits->dev, "%s Failed to write iram data\n", __func__);
			return -ENODEV;
		}

		if (ilits->wrapper(NULL, 0, buf + cnt * limit, tmp_len, OFF, OFF) < 0) {
			input_err(true, ilits->dev, "%s Failed to Read iram data\n", __func__);
			return -ENODEV;
		}

		tmp_len = len - cnt * limit;
		ilits->fw_update_stat = ((len - tmp_len) * 100) / len;
		input_info(true, ilits->dev, "%s Reading iram data .... %d%c", __func__, ilits->fw_update_stat, '%');
	}
	return 0;
}

int ili_fw_dump_iram_data(u32 start, u32 end, bool save)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	int i, ret = 0;
	int len, tmp = debug_en;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		ret = ili_ice_mode_ctrl(ENABLE, OFF);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Enable ice mode failed\n", __func__);
			return ret;
		}
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		input_err(true, ilits->dev, "%s len is larger than buffer, abort\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
		ilits->update_buf[i] = 0xFF;

	ret = ilitek_tddi_fw_iram_read(ilits->update_buf, start, len);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Read IRAM data failed\n", __func__);
		goto out;
	}

	if (save) {
		f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
		if (ERR_ALLOC_MEM(f)) {
			input_err(true, ilits->dev, "%s Failed to open the file at %ld.\n", __func__, PTR_ERR(f));
			ret = -ENOMEM;
			goto out;
		}

		old_fs = get_fs();
		set_fs(get_ds());
		set_fs(KERNEL_DS);
		pos = 0;
		vfs_write(f, ilits->update_buf, len, &pos);
		set_fs(old_fs);
		filp_close(f, NULL);
		input_info(true, ilits->dev, "%s Save iram data to %s\n", __func__, DUMP_IRAM_PATH);
	} else {
		debug_en = DEBUG_ALL;
		ili_dump_data(ilits->update_buf, 8, len, 0, "IRAM");
		debug_en = tmp;
	}

out:
	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			input_err(true, ilits->dev, "%s Enable ice mode failed after code reset\n", __func__);
	}

	input_info(true, ilits->dev, "%s Dump IRAM %s\n", __func__, (ret < 0) ? "FAIL" : "SUCCESS");
	return ret;
}

static int ilitek_tddi_fw_iram_program(u32 start, u8 *w_buf, u32 w_len, u32 split_len)
{
	int i = 0, j = 0, addr = 0;
	u32 end = start + w_len;
	bool fix_4_alignment = false;

	if (split_len % 4 > 0)
		input_err(true, ilits->dev,
			"%s Since split_len must be four-aligned, it must be a multiple of four", __func__);

	if (split_len != 0) {
		for (addr = start, i = 0; addr < end; addr += split_len, i += split_len) {
			if ((addr + split_len) > end) {
				split_len = end - addr;
				if (split_len % 4 != 0)
					fix_4_alignment = true;
			}

			ilits->update_buf[0] = SPI_WRITE;
			ilits->update_buf[1] = 0x25;
			ilits->update_buf[2] = (char)((addr & 0x000000FF));
			ilits->update_buf[3] = (char)((addr & 0x0000FF00) >> 8);
			ilits->update_buf[4] = (char)((addr & 0x00FF0000) >> 16);

			for (j = 0; j < split_len; j++)
				ilits->update_buf[5 + j] = w_buf[i + j];

			if (fix_4_alignment) {
				input_info(true, ilits->dev, "%s org split_len = 0x%X\n", __func__, split_len);
				input_info(true, ilits->dev, "%s idev->update_buf[5 + 0x%X] = 0x%X\n",
					__func__, split_len - 4, ilits->update_buf[5 + split_len - 4]);
				input_info(true, ilits->dev, "%s idev->update_buf[5 + 0x%X] = 0x%X\n",
					__func__, split_len - 3, ilits->update_buf[5 + split_len - 3]);
				input_info(true, ilits->dev, "%s idev->update_buf[5 + 0x%X] = 0x%X\n",
					__func__, split_len - 2, ilits->update_buf[5 + split_len - 2]);
				input_info(true, ilits->dev, "%s idev->update_buf[5 + 0x%X] = 0x%X\n",
					__func__, split_len - 1, ilits->update_buf[5 + split_len - 1]);
				for (j = 0; j < (4 - (split_len % 4)); j++) {
					ilits->update_buf[5 + j + split_len] = 0xFF;
					input_info(true, ilits->dev, "%s idev->update_buf[5 + 0x%X] = 0x%X\n",
						__func__, j + split_len, ilits->update_buf[5 + j + split_len]);
				}

				input_info(true, ilits->dev, "%s split_len %% 4 = %d\n", __func__, split_len % 4);
				split_len = split_len + (4 - (split_len % 4));
				input_info(true, ilits->dev, "%s fix split_len = 0x%X\n", __func__, split_len);
			}
			if (ilits->spi_write_then_read(ilits->spi, ilits->update_buf, split_len + 5, NULL, 0)) {
				input_err(true, ilits->dev, "%s Failed to write data via SPI in host download (%x)\n",
					__func__, split_len + 5);
				return -EIO;
			}
			ilits->fw_update_stat = (i * 100) / w_len;
		}
	} else {
		for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
			ilits->update_buf[i] = 0xFF;

		ilits->update_buf[0] = SPI_WRITE;
		ilits->update_buf[1] = 0x25;
		ilits->update_buf[2] = (char)((start & 0x000000FF));
		ilits->update_buf[3] = (char)((start & 0x0000FF00) >> 8);
		ilits->update_buf[4] = (char)((start & 0x00FF0000) >> 16);

		memcpy(&ilits->update_buf[5], w_buf, w_len);
		if (w_len % 4 != 0) {
			input_info(true, ilits->dev, "%s org w_len = %d\n", __func__, w_len);
			w_len = w_len + (4 - (w_len % 4));
			input_info(true, ilits->dev, "%s w_len = %d w_len %% 4 = %d\n", __func__, w_len, w_len % 4);
		}
		/* It must be supported by platforms that have the ability to transfer all data at once. */
		if (ilits->spi_write_then_read(ilits->spi, ilits->update_buf, w_len + 5, NULL, 0) < 0) {
			input_err(true, ilits->dev, "%s Failed to write data via SPI in host download (%x)\n",
				__func__, w_len + 5);
			return -EIO;
		}
	}
	return 0;
}

static int ilitek_tddi_fw_iram_upgrade(u8 *pfw)
{
	int i, ret = UPDATE_PASS;
	u32 mode, crc, dma, iram_crc = 0;
	u8 *fw_ptr = NULL, crc_temp[4], crc_len = 4;
	bool iram_crc_err = false;

	if (!ilits->ddi_rest_done) {
		if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE)
			ili_reset_ctrl(ilits->reset);

		ret = ili_ice_mode_ctrl(ENABLE, OFF);
		if (ret < 0)
			return -EFW_ICE_MODE;
	} else {
		/* Restore it if the wq of load_fw_ddi has been called. */
		ilits->ddi_rest_done = false;
	}

	/* Point to pfw with different addresses for getting its block data. */
	fw_ptr = pfw;
	if (ilits->actual_tp_mode == P5_X_FW_TEST_MODE) {

		mode = MP;
	} else if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		mode = GESTURE;
		crc_len = 0;
	} else {
		mode = AP;
	}

	/* Program data to iram acorrding to each block */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].mode == mode && fbi[i].len != 0) {
			ILI_DBG("%s Download %s code from hex 0x%x to IRAM 0x%x, len = 0x%x\n",
					__func__, fbi[i].name, fbi[i].start, fbi[i].mem_start, fbi[i].len);

#if SPI_DMA_TRANSFER_SPLIT
			if (ilitek_tddi_fw_iram_program(fbi[i].mem_start,
					(fw_ptr + fbi[i].start), fbi[i].len, SPI_UPGRADE_LEN) < 0)
				input_err(true, ilits->dev, "%s IRAM program failed\n", __func__);
#else
			if (ilitek_tddi_fw_iram_program(fbi[i].mem_start, (fw_ptr + fbi[i].start), fbi[i].len, 0) < 0)
				input_err(true, ilits->dev, "%s IRAM program failed\n", __func__);
#endif
			crc = CalculateCRC32(fbi[i].start, fbi[i].len - crc_len, fw_ptr);
			dma = calc_hw_dma_crc(fbi[i].mem_start, fbi[i].len - crc_len);

			if (mode != GESTURE) {
				ilitek_tddi_fw_iram_read(crc_temp, (fbi[i].mem_start + fbi[i].len - crc_len),
					sizeof(crc_temp));
				iram_crc = crc_temp[0] << 24 | crc_temp[1] << 16 | crc_temp[2] << 8 | crc_temp[3];
				if (iram_crc != dma)
					iram_crc_err = true;
			}

			input_info(true, ilits->dev, "%s %s CRC is %s hex(%x) : dma(%x) : iram(%x), calculation len is 0x%x\n",
				__func__, fbi[i].name, ((crc != dma) || (iram_crc_err)) ? "Invalid !" : "Correct !",
				crc, dma, iram_crc, fbi[i].len - crc_len);

			if ((crc != dma) || iram_crc_err) {
				input_err(true, ilits->dev, "%s CRC Error! print iram data with first 16 bytes\n",
					__func__);
				ili_fw_dump_iram_data(0x0, 0xF, false);
				return -EFW_CRC;
			}
		}
	}

	if (ilits->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
		if (ili_reset_ctrl(TP_IC_CODE_RST) < 0) {
			input_err(true, ilits->dev, "%s TP Code reset failed during iram programming\n", __func__);
			ret = -EFW_REST;
			return ret;
		}
	}

	if (ili_ice_mode_ctrl(DISABLE, OFF) < 0) {
		input_err(true, ilits->dev, "%s Disable ice mode failed after code reset\n", __func__);
		ret = -EFW_ICE_MODE;
	}

	/* Waiting for fw ready sending first cmd */
	if (!ilits->info_from_hex || (ilits->chip->core_ver < CORE_VER_1410))
		mdelay(100);

	return ret;
}

static int ilitek_fw_calc_file_crc(u8 *pfw)
{
	int i;
	u32 ex_addr, data_crc, file_crc;

	for (i = 0; i < ARRAY_SIZE(fbi); i++) {
		if (fbi[i].end == 0)
			continue;
		ex_addr = fbi[i].end;
		data_crc = CalculateCRC32(fbi[i].start, fbi[i].len - 4, pfw);
		file_crc = pfw[ex_addr - 3] << 24 | pfw[ex_addr - 2] << 16 | pfw[ex_addr - 1] << 8 | pfw[ex_addr];
		ILI_DBG("%s data crc = %x, file crc = %x\n", __func__, data_crc, file_crc);
		if (data_crc != file_crc) {
			input_err(true, ilits->dev, "%s Content of fw file is broken. (%d, %x, %x)\n",
				__func__, i, data_crc, file_crc);
			return -1;
		}
	}

	input_info(true, ilits->dev, "%s Content of fw file is correct\n", __func__);
	return 0;
}

static int ilitek_tddi_fw_update_block_info(u8 *pfw)
{
	u32 ges_area_section = 0, ges_info_addr = 0, ges_fw_start = 0, ges_fw_end = 0;
	u32 ap_end = 0, ap_len = 0;
	u32 fw_info_addr = 0, fw_mp_ver_addr = 0, fw_customer_info_addr = 0;

	if (tfd.hex_tag != BLOCK_TAG_AF) {
		input_err(true, ilits->dev, "%s HEX TAG is invalid (0x%X)\n", __func__, tfd.hex_tag);
		return -EINVAL;
	}

	fbi[AP].mem_start = (fbi[AP].fix_mem_start != INT_MAX) ? fbi[AP].fix_mem_start : 0;
	fbi[DATA].mem_start = (fbi[DATA].fix_mem_start != INT_MAX) ? fbi[DATA].fix_mem_start : DLM_START_ADDRESS;
	fbi[TUNING].mem_start = (fbi[TUNING].fix_mem_start != INT_MAX) ? fbi[TUNING].fix_mem_start :
		fbi[DATA].mem_start + fbi[DATA].len;
	fbi[MP].mem_start = (fbi[MP].fix_mem_start != INT_MAX) ? fbi[MP].fix_mem_start : 0;
	fbi[GESTURE].mem_start = (fbi[GESTURE].fix_mem_start != INT_MAX) ? fbi[GESTURE].fix_mem_start :	0;
	fbi[TAG].mem_start = (fbi[TAG].fix_mem_start != INT_MAX) ? fbi[TAG].fix_mem_start : 0;
	fbi[PARA_BACKUP].mem_start = (fbi[PARA_BACKUP].fix_mem_start != INT_MAX) ? fbi[PARA_BACKUP].fix_mem_start : 0;
	fbi[DDI].mem_start = (fbi[DDI].fix_mem_start != INT_MAX) ? fbi[DDI].fix_mem_start : 0;

	/* Parsing gesture info form AP code */
	ges_info_addr = (fbi[AP].end + 1 - 60);
	ges_area_section = (pfw[ges_info_addr + 3] << 24) + (pfw[ges_info_addr + 2] << 16) +
		(pfw[ges_info_addr + 1] << 8) + pfw[ges_info_addr];
	fbi[GESTURE].mem_start = (pfw[ges_info_addr + 7] << 24) + (pfw[ges_info_addr + 6] << 16) +
		(pfw[ges_info_addr + 5] << 8) + pfw[ges_info_addr + 4];
	ap_end = (pfw[ges_info_addr + 11] << 24) + (pfw[ges_info_addr + 10] << 16) + (pfw[ges_info_addr + 9] << 8) +
		pfw[ges_info_addr + 8];

	if (ap_end != fbi[GESTURE].mem_start)
		ap_len = ap_end - fbi[GESTURE].mem_start + 1;

	ges_fw_start = (pfw[ges_info_addr + 15] << 24) + (pfw[ges_info_addr + 14] << 16) +
		(pfw[ges_info_addr + 13] << 8) + pfw[ges_info_addr + 12];
	ges_fw_end = (pfw[ges_info_addr + 19] << 24) + (pfw[ges_info_addr + 18] << 16) +
		(pfw[ges_info_addr + 17] << 8) + pfw[ges_info_addr + 16];

	if (ges_fw_end != ges_fw_start)
		fbi[GESTURE].len = ges_fw_end - ges_fw_start;

	/* update gesture address */
	fbi[GESTURE].start = ges_fw_start;

	input_info(true, ilits->dev, "%s ==== Gesture loader info ====\n", __func__);
	input_info(true, ilits->dev, "%s gesture move to ap addr => start = 0x%x, ap_end = 0x%x, ap_len = 0x%x\n",
		__func__, fbi[GESTURE].mem_start, ap_end, ap_len);
	input_info(true, ilits->dev, "%s gesture hex addr => start = 0x%x, gesture_end = 0x%x, gesture_len = 0x%x\n",
		__func__, ges_fw_start, ges_fw_end, fbi[GESTURE].len);
	input_info(true, ilits->dev, "%s =============================\n", __func__);

	fbi[AP].name = "AP";
	fbi[DATA].name = "DATA";
	fbi[TUNING].name = "TUNING";
	fbi[MP].name = "MP";
	fbi[GESTURE].name = "GESTURE";
	fbi[TAG].name = "TAG";
	fbi[PARA_BACKUP].name = "PARA_BACKUP";
	fbi[DDI].name = "DDI";

	/* upgrade mode define */
	fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
	fbi[MP].mode = MP;
	fbi[GESTURE].mode = GESTURE;

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Copy fw info  */
	fw_info_addr = fbi[AP].end - INFO_HEX_ST_ADDR;
	input_info(true, ilits->dev, "%s Parsing hex info start addr = 0x%x\n", __func__, fw_info_addr);
	ipio_memcpy(ilits->fw_info, (pfw + fw_info_addr), sizeof(ilits->fw_info), sizeof(ilits->fw_info));

	/* copy fw mp ver */
	fw_mp_ver_addr = fbi[MP].end - INFO_MP_HEX_ADDR;
	input_info(true, ilits->dev, "%s Parsing hex mp ver addr = 0x%x\n", __func__, fw_mp_ver_addr);
	ipio_memcpy(ilits->fw_mp_ver, pfw + fw_mp_ver_addr, sizeof(ilits->fw_mp_ver), sizeof(ilits->fw_mp_ver));

	/* copy customer info ver */
	fw_customer_info_addr = fbi[AP].end - INFO_CUSTOMER_INFO_HEX_ADDR;
	input_info(true, ilits->dev, "%s Parsing hex customer info addr = 0x%x\n", __func__, fw_customer_info_addr);
	ipio_memcpy(ilits->fw_cur_info, pfw + fw_customer_info_addr, sizeof(ilits->fw_cur_info),
		sizeof(ilits->fw_cur_info));
	if (ilits->fw_customer_info[0] == 0) {
		ipio_memcpy(ilits->fw_customer_info, pfw + fw_customer_info_addr, sizeof(ilits->fw_customer_info),
			 sizeof(ilits->fw_customer_info));
	}


	/* copy fw core ver */
	ilits->chip->core_ver = (ilits->fw_info[68] << 24) | (ilits->fw_info[69] << 16) |
			(ilits->fw_info[70] << 8) | ilits->fw_info[71];
	input_info(true, ilits->dev, "%s New FW Core version = %x\n", __func__, ilits->chip->core_ver);

	/* Get hex fw vers */
	tfd.new_fw_cb = (ilits->fw_info[48] << 24) | (ilits->fw_info[49] << 16) |
			(ilits->fw_info[50] << 8) | ilits->fw_info[51];

	/* Get hex report info block*/
	ipio_memcpy(&ilits->rib, ilits->fw_info, sizeof(ilits->rib), sizeof(ilits->rib));
	input_info(true, ilits->dev,
		"%s report_info_block : nReportByPixel = %d, nIsHostDownload = %d, nIsSPIICE = %d, nIsSPISLAVE = %d\n",
		__func__, ilits->rib.nReportByPixel, ilits->rib.nIsHostDownload, ilits->rib.nIsSPIICE,
		ilits->rib.nIsSPISLAVE);
	input_info(true, ilits->dev, "%s report_info_block : nIsI2C = %d, nReserved00 = %d, nReserved01 = %x, nReserved02 = %x,  nReserved03 = %x\n",
		__func__, ilits->rib.nIsI2C, ilits->rib.nReserved00, ilits->rib.nReserved01,
		ilits->rib.nReserved02, ilits->rib.nReserved03);

	/* Calculate update address */
	input_info(true, ilits->dev, "%s New FW ver = 0x%x\n", __func__, tfd.new_fw_cb);
	input_info(true, ilits->dev, "%s star_addr = 0x%06X, end_addr = 0x%06X, Block Num = %d\n",
		__func__, tfd.start_addr, tfd.end_addr, tfd.block_number);

	return 0;
}

static int ilitek_tddi_fw_ili_convert(u8 *pfw)
{
	int i, size, blk_num = 0, blk_map = 0, num;
	int b0_addr = 0, b0_num = 0;

	if (ERR_ALLOC_MEM(ilits->md_fw_ili))
		return -ENOMEM;

	CTPM_FW = ilits->md_fw_ili;
	size = ilits->md_fw_ili_size;

	if (size < ILI_FILE_HEADER || size > MAX_HEX_FILE_SIZE) {
		input_err(true, ilits->dev, "%s size of ILI file is invalid\n", __func__);
		return -EINVAL;
	}

	/* Check if it's old version of ILI format. */
	if (CTPM_FW[22] == 0xFF && CTPM_FW[23] == 0xFF &&
		CTPM_FW[24] == 0xFF && CTPM_FW[25] == 0xFF) {
		input_err(true, ilits->dev, "%s Invaild ILI format, abort!\n", __func__);
		return -EINVAL;
	}

	blk_num = CTPM_FW[131];
	blk_map = (CTPM_FW[129] << 8) | CTPM_FW[130];
	input_info(true, ilits->dev, "%s Parsing ILI file, block num = %d, block mapping = %x\n",
		__func__, blk_num, blk_map);

	if (blk_num > (FW_BLOCK_INFO_NUM - 1) || !blk_num || !blk_map) {
		input_err(true, ilits->dev, "%s Number of block or block mapping is invalid, abort!\n", __func__);
		return -EINVAL;
	}

	memset(fbi, 0x0, sizeof(fbi));

	tfd.start_addr = 0;
	tfd.end_addr = 0;
	tfd.hex_tag = BLOCK_TAG_AF;

	/* Parsing block info */
	for (i = 0; i < FW_BLOCK_INFO_NUM - 1; i++) {
		/* B0 tag */
		b0_addr = (CTPM_FW[4 + i * 4] << 16) | (CTPM_FW[5 + i * 4] << 8) | (CTPM_FW[6 + i * 4]);
		b0_num = CTPM_FW[7 + i * 4];
		if ((b0_num != 0) && (b0_addr != 0x000000))
			fbi[b0_num].fix_mem_start = b0_addr;

		/* AF tag */
		num = i + 1;
		if (((blk_map >> i) & 0x01) == 0x01) {
			fbi[num].start =
				(CTPM_FW[132 + i * 6] << 16) | (CTPM_FW[133 + i * 6] << 8) | CTPM_FW[134 + i * 6];
			fbi[num].end =
				(CTPM_FW[135 + i * 6] << 16) | (CTPM_FW[136 + i * 6] << 8) |  CTPM_FW[137 + i * 6];

			if (fbi[num].fix_mem_start == 0)
				fbi[num].fix_mem_start = INT_MAX;

			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_DBG("%s Block[%d]: start_addr = %x, end = %x, fix_mem_start = 0x%x\n",
			 __func__, num, fbi[num].start, fbi[num].end, fbi[num].fix_mem_start);
			if (num == GESTURE)
				ilits->gesture_load_code = true;
		}
	}

	memcpy(pfw, CTPM_FW + ILI_FILE_HEADER, size - ILI_FILE_HEADER);

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.block_number = blk_num;
	tfd.end_addr = size - ILI_FILE_HEADER;
	return 0;
}

static int ilitek_tddi_fw_hex_convert(u8 *phex, int size, u8 *pfw)
{
	int block = 0;
	u32 i = 0, j = 0, k = 0, num = 0;
	u32 len = 0, addr = 0, type = 0;
	u32 start_addr = 0x0, end_addr = 0x0, ex_addr = 0;
	u32 offset;

	memset(fbi, 0x0, sizeof(fbi));

	/* Parsing HEX file */
	for (; i < size;) {
		len = HexToDec(&phex[i + 1], 2);
		addr = HexToDec(&phex[i + 3], 4);
		type = HexToDec(&phex[i + 7], 2);

		if (type == 0x04) {
			ex_addr = HexToDec(&phex[i + 9], 4);
		} else if (type == 0x02) {
			ex_addr = HexToDec(&phex[i + 9], 4);
			ex_addr = ex_addr >> 12;
		} else if (type == BLOCK_TAG_AF) {
			/* insert block info extracted from hex */
			tfd.hex_tag = type;
			if (tfd.hex_tag == BLOCK_TAG_AF)
				num = HexToDec(&phex[i + 9 + 6 + 6], 2);
			else
				num = 0xFF;

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				input_err(true, ilits->dev, "%s ERROR! block num is larger than its define (%d, %d)\n",
					__func__, num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].start = HexToDec(&phex[i + 9], 6);
			fbi[num].end = HexToDec(&phex[i + 9 + 6], 6);
			fbi[num].fix_mem_start = INT_MAX;
			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_DBG("%s Block[%d]: start_addr = %x, end = %x",
			__func__, num, fbi[num].start, fbi[num].end);

			if (num == GESTURE)
				ilits->gesture_load_code = true;

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				input_err(true, ilits->dev, "%s ERROR! block num is larger than its define (%d, %d)\n",
					__func__, num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			ILI_DBG("%s Tag 0xB0: change Block[%d] to addr = 0x%x\n",
			__func__, num, fbi[num].fix_mem_start);
		}

		addr = addr + (ex_addr << 16);

		if (phex[i + 1 + 2 + 4 + 2 + (len * 2) + 2] == 0x0D)
			offset = 2;
		else
			offset = 1;

		if (addr > MAX_HEX_FILE_SIZE) {
			input_err(true, ilits->dev, "%s Invalid hex format %d\n", __func__, addr);
			return -1;
		}

		if (type == 0x00) {
			end_addr = addr + len;
			if (addr < start_addr)
				start_addr = addr;
			/* fill data */
			for (j = 0, k = 0; j < (len * 2); j += 2, k++)
				pfw[addr + k] = HexToDec(&phex[i + 9 + j], 2);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.start_addr = start_addr;
	tfd.end_addr = end_addr;
	tfd.block_number = block;
	return 0;
}

static int ilitek_tdd_fw_hex_open(u8 op, u8 *pfw)
{
	int ret = 0, fsize = 0;
	const struct firmware *fw = NULL;
	struct file *f = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	size_t spu_fw_size;
	int nread;

	input_info(true, ilits->dev, "%s Open file method = %s, path = %s\n",
		__func__, op ? "FILP_OPEN" : "REQUEST_FIRMWARE",
		op ? ilits->md_fw_filp_path : ilits->md_fw_rq_path);

	switch (op) {
	case REQUEST_FIRMWARE:
		if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
			input_err(true, ilits->dev, "%s Request firmware failed, try again\n", __func__);
			if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
				input_err(true, ilits->dev, "%s Request firmware failed after retry\n", __func__);
				ret = -1;
				goto out;
			}
		}

		fsize = fw->size;
		input_info(true, ilits->dev, "%s fsize = %d\n", __func__, fsize);
		if (fsize <= 0) {
			input_err(true, ilits->dev, "%s The size of file is invaild\n", __func__);
			release_firmware(fw);
			ret = -1;
			goto out;
		}

		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			input_err(true, ilits->dev, "%s Failed to allocate tp_fw by vmalloc, try again\n", __func__);
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				input_err(true, ilits->dev, "%s Failed to allocate tp_fw after retry\n", __func__);
				release_firmware(fw);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* Copy fw data got from request_firmware to global */
		ipio_memcpy((u8 *)ilits->tp_fw.data, fw->data, fsize * sizeof(*fw->data), fsize);
		ilits->tp_fw.size = fsize;
		release_firmware(fw);
		break;
	case FILP_OPEN:
		f = filp_open(ilits->md_fw_filp_path, O_RDONLY, 0644);
		if (ERR_ALLOC_MEM(f)) {
			input_err(true, ilits->dev, "%s Failed to open the file, %ld\n", __func__, PTR_ERR(f));
			ret = -1;
			goto out;
		}

		fsize = f->f_inode->i_size;
		input_info(true, ilits->dev, "%s fsize = %d\n", __func__, fsize);
		if (fsize <= 0) {
			input_err(true, ilits->dev, "%s The size of file is invaild\n", __func__);
			filp_close(f, NULL);
			ret = -1;
			goto out;
		}

		if (ilits->signing) {
			/* name 3, digest 32, signature 512 */
			spu_fw_size = fsize;
			fsize -= SPU_METADATA_SIZE(TSP);
		}

		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			input_err(true, ilits->dev, "%s Failed to allocate tp_fw by vmalloc, try again\n", __func__);
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				input_err(true, ilits->dev, "%s Failed to allocate tp_fw after retry\n", __func__);
				filp_close(f, NULL);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* ready to map user's memory to obtain data by reading files */
		old_fs = get_fs();
		set_fs(get_ds());
		set_fs(KERNEL_DS);
		if (ilits->signing) {
#ifdef CONFIG_SPU_VERIFY
			unsigned char *spu_fw_data;
			size_t spu_ret = 0;

			spu_fw_data = vzalloc(spu_fw_size);
			if (!spu_fw_data) {
				input_err(true, ilits->dev, "%s: failed to alloc mem for spu\n", __func__);
				ret = -ENOMEM;
				goto signing_out;
			}
			nread = vfs_read(f, (char __user *)spu_fw_data, spu_fw_size, &f->f_pos);

			input_info(true, ilits->dev, "%s: start, file path %s, size %ld Bytes\n",
					__func__, ilits->md_fw_filp_path, spu_fw_size);

			if (nread != spu_fw_size) {
				input_err(true, ilits->dev,
					"%s: failed to read firmware file, nread %d Bytes\n",
					__func__, nread);
				ret = -EIO;
				vfree(spu_fw_data);
				goto signing_out;
			}
			spu_ret = spu_firmware_signature_verify("TSP", spu_fw_data, spu_fw_size);
			if (spu_ret != fsize) {
				input_err(true, ilits->dev, "%s: signature verify failed, %ld\n",
						__func__, spu_ret);
				ret = -EINVAL;
				vfree(spu_fw_data);
				goto signing_out;
			}

			memcpy((u8 *)ilits->tp_fw.data, spu_fw_data, fsize);
			vfree(spu_fw_data);
#endif
		} else {
			pos = 0;
			vfs_read(f, (u8 *)ilits->tp_fw.data, fsize, &pos);
		}
signing_out:
		set_fs(old_fs);
		filp_close(f, NULL);
		ilits->tp_fw.size = fsize;
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown open file method, %d\n", __func__, op);
		break;
	}

	if (ERR_ALLOC_MEM(ilits->tp_fw.data) || ilits->tp_fw.size <= 0) {
		input_err(true, ilits->dev, "%s fw data/size is invaild\n", __func__);
		ret = -1;
		goto out;
	}

	/* Convert hex and copy data from tp_fw.data to pfw */
	if (ilitek_tddi_fw_hex_convert((u8 *)ilits->tp_fw.data, ilits->tp_fw.size, pfw) < 0) {
		input_err(true, ilits->dev, "%s Convert hex file failed\n", __func__);
		ret = -1;
	}

out:
	ipio_vfree((void **)&(ilits->tp_fw.data));
	return ret;
}

int ili_fw_upgrade(int op)
{
	int i, ret = 0, retry = 3;

	if (!ilits->boot || ilits->force_fw_update || ERR_ALLOC_MEM(pfw)) {
		ilits->gesture_load_code = false;

		if (ERR_ALLOC_MEM(pfw)) {
			ipio_vfree((void **)&pfw);
			pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
			if (ERR_ALLOC_MEM(pfw)) {
				input_err(true, ilits->dev, "%s Failed to allocate pfw memory, %ld\n",
					__func__, PTR_ERR(pfw));
				ipio_vfree((void **)&pfw);
				ret = -ENOMEM;
				goto out;
			}
		}

		for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
			pfw[i] = 0xFF;

		if (ilitek_tdd_fw_hex_open(op, pfw) < 0) {
			input_err(true, ilits->dev, "%s Open hex file fail, try upgrade from ILI file\n", __func__);

			/*
			 * Users might not be aware of a broken hex file when recovering
			 * fw from ILI file. We should force them to check
			 * hex files if they attempt to update via device node.
			 */
			if (ilits->node_update) {
				input_err(true, ilits->dev, "%s Ignore update from ILI file\n", __func__);
				ipio_vfree((void **)&pfw);
				return -EFW_CONVERT_FILE;
			}

			if (ilitek_tddi_fw_ili_convert(pfw) < 0) {
				input_err(true, ilits->dev, "%s Convert ILI file error\n", __func__);
				ret = -EFW_CONVERT_FILE;
				goto out;
			}
		}

		if (ilitek_tddi_fw_update_block_info(pfw) < 0) {
			input_err(true, ilits->dev, "%s %s ilitek_tddi_fw_update_block_info fail\n", __func__);
			ret = -EFW_CONVERT_FILE;
			goto out;
		}

		if (ilits->chip->core_ver >= CORE_VER_1470 && ilits->rib.nIsHostDownload == 0) {
			input_err(true, ilits->dev, "%s hex file interface no match error\n", __func__);
			return -EFW_INTERFACE;
		}
	}

	do {
		ret = ilitek_tddi_fw_iram_upgrade(pfw);
		if (ret == UPDATE_PASS)
			break;

		input_err(true, ilits->dev, "%s Upgrade failed, do retry!\n", __func__);
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		input_err(true, ilits->dev, "%s Failed to upgrade fw %d times, erasing iram\n", __func__, retry);
		if (ili_reset_ctrl(ilits->reset) < 0)
			input_err(true, ilits->dev, "%s TP reset failed while erasing data\n", __func__);
		ilits->xch_num = 0;
		ilits->ych_num = 0;
		return ret;
	}

out:
	ili_ic_get_core_ver();
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();
	ili_ic_func_ctrl_reset();
	return ret;
}

void ili_fw_read_flash_info(void)
{
}

void ili_flash_clear_dma(void)
{
}

void ili_flash_dma_write(u32 start, u32 end, u32 len)
{
}

int ili_fw_dump_flash_data(u32 start, u32 end, bool user)
{
	return 0;
}
