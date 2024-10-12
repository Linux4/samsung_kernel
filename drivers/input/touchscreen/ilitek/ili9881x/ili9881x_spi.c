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

struct touch_bus_info {
	struct spi_driver bus_driver;
	struct ilitek_hwif_info *hwif;
};

#ifdef CONFIG_SAMSUNG_TUI
static int stui_tsp_enter(void);
static int stui_tsp_exit(void);
#endif

struct ilitek_ts_data *ilits;

#if SPI_DMA_TRANSFER_SPLIT
#define DMA_TRANSFER_MAX_CHUNK		64   // number of chunks to be transferred.
#define DMA_TRANSFER_MAX_LEN		1024 // length of a chunk.

int ili_spi_write_then_read_split(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	int status = -1, duplex_len = 0;
	int xfercnt = 0, xferlen = 0, xferloop = 0;
	int offset = 0;
	u8 cmd = 0;
	struct spi_message message;
	struct spi_transfer *xfer;

	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

#ifdef CONFIG_SAMSUNG_TUI
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ilits->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_info(true, ilits->dev, "%s: secure touch is enabled\n", __func__);
		return -EBUSY;
	}
#endif

	xfer = kzalloc(DMA_TRANSFER_MAX_CHUNK * sizeof(struct spi_transfer), GFP_KERNEL);

	if (n_rx > SPI_RX_BUF_SIZE) {
		input_err(true, ilits->dev, "%s Rx length is greater than spi local buf, abort\n", __func__);
		status = -ENOMEM;
		goto out;
	}

	memset(ilits->spi_tx, 0x0, SPI_TX_BUF_SIZE);
	memset(ilits->spi_rx, 0x0, SPI_RX_BUF_SIZE);

	if ((n_tx > 0) && (n_rx > 0))
		cmd = SPI_READ;
	else
		cmd = SPI_WRITE;

	switch (cmd) {
	case SPI_WRITE:
		if (n_tx % DMA_TRANSFER_MAX_LEN)
			xferloop = (n_tx / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = n_tx / DMA_TRANSFER_MAX_LEN;

		xferlen = n_tx;
		memcpy(ilits->spi_tx, (u8 *)txbuf, xferlen);

		if (ilits->cs_gpio > 0)
			gpio_direction_output(ilits->cs_gpio, 0);

		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			spi_message_init(&message);

			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = ilits->spi_tx + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = n_tx - (xfercnt+1) * DMA_TRANSFER_MAX_LEN;

			status = spi_sync(spi, &message);
		}

		if (ilits->cs_gpio > 0)
			gpio_direction_output(ilits->cs_gpio, 1);

		break;
	case SPI_READ:
		if (n_tx > DMA_TRANSFER_MAX_LEN) {
			input_err(true, ilits->dev, "%s Tx length must be lower than dma length (%d).\n",
				__func__, DMA_TRANSFER_MAX_LEN);
			status = -EINVAL;
			break;
		}

		if (!atomic_read(&ilits->ice_stat))
			offset = 2;

		memcpy(ilits->spi_tx, txbuf, n_tx);
		duplex_len = n_tx + n_rx + offset;

		if (duplex_len % DMA_TRANSFER_MAX_LEN)
			xferloop = (duplex_len / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = duplex_len / DMA_TRANSFER_MAX_LEN;

		xferlen = duplex_len;

		if (ilits->cs_gpio > 0)
			gpio_direction_output(ilits->cs_gpio, 0);

		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			spi_message_init(&message);

			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = ilits->spi_tx;
			xfer[xfercnt].rx_buf = ilits->spi_rx + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = duplex_len - (xfercnt + 1) * DMA_TRANSFER_MAX_LEN;

			status = spi_sync(spi, &message);
		}

		if (ilits->cs_gpio > 0)
			gpio_direction_output(ilits->cs_gpio, 1);

		if (status == 0) {
			if (ilits->spi_rx[1] != SPI_ACK && !atomic_read(&ilits->ice_stat)) {
				status = DO_SPI_RECOVER;
				input_err(true, ilits->dev, "%s Do spi recovery: rxbuf[1] = 0x%x, ice = %d\n",
					__func__, ilits->spi_rx[1], atomic_read(&ilits->ice_stat));
				break;
			}

			memcpy((u8 *)rxbuf, ilits->spi_rx + offset + 1, n_rx);
		} else {
			input_err(true, ilits->dev, "%s spi read fail, status = %d\n", __func__, status);
		}
		break;
	default:
		input_info(true, ilits->dev, "%s Unknown command 0x%x\n", __func__, cmd);
		break;
	}

out:
	ipio_kfree((void **)&xfer);
	return status;
}
#else
int ili_spi_write_then_read_direct(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	int status = -1, duplex_len = 0;
	int offset = 0;
	u8 cmd;
	struct spi_message message;
	struct spi_transfer xfer;

	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

#ifdef CONFIG_SAMSUNG_TUI
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ilits->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_info(true, ilits->dev, "%s: secure touch is enabled\n", __func__);
		return -EBUSY;
	}
#endif
	if (n_rx > SPI_RX_BUF_SIZE) {
		input_err(true, ilits->dev, "%s Rx length is greater than spi local buf, abort\n", __func__);
		status = -ENOMEM;
		goto out;
	}

	spi_message_init(&message);
	memset(&xfer, 0, sizeof(xfer));

	if ((n_tx > 0) && (n_rx > 0))
		cmd = SPI_READ;
	else
		cmd = SPI_WRITE;

	switch (cmd) {
	case SPI_WRITE:
		xfer.len = n_tx;
		xfer.tx_buf = txbuf;
		spi_message_add_tail(&xfer, &message);

		if (ilits->cs_gpio > 0) {
			gpio_direction_output(ilits->cs_gpio, 0);
			status = spi_sync(spi, &message);
			gpio_direction_output(ilits->cs_gpio, 1);
		} else {
			status = spi_sync(spi, &message);
		}
		break;
	case SPI_READ:
		if (!atomic_read(&ilits->ice_stat))
			offset = 2;

		duplex_len = n_tx + n_rx + offset;
		if ((duplex_len > SPI_TX_BUF_SIZE) ||
			(duplex_len > SPI_RX_BUF_SIZE)) {
			input_err(true, ilits->dev, "%s duplex_len is over than dma buf, abort\n", __func__);
			status = -ENOMEM;
			break;
		}

		memset(ilits->spi_tx, 0x0, SPI_TX_BUF_SIZE);
		memset(ilits->spi_rx, 0x0, SPI_RX_BUF_SIZE);

		xfer.len = duplex_len;
		memcpy(ilits->spi_tx, txbuf, n_tx);
		xfer.tx_buf = ilits->spi_tx;
		xfer.rx_buf = ilits->spi_rx;

		spi_message_add_tail(&xfer, &message);
		if (ilits->cs_gpio > 0) {
			gpio_direction_output(ilits->cs_gpio, 0);
			status = spi_sync(spi, &message);
			gpio_direction_output(ilits->cs_gpio, 1);
		} else {
			status = spi_sync(spi, &message);
		}
		if (status == 0) {
			if (ilits->spi_rx[1] != SPI_ACK && !atomic_read(&ilits->ice_stat)) {
				status = DO_SPI_RECOVER;
				input_err(true, ilits->dev, "%s Do spi recovery: rxbuf[1] = 0x%x, ice = %d\n",
					__func__, ilits->spi_rx[1], atomic_read(&ilits->ice_stat));
				break;
			}

			memcpy((u8 *)rxbuf, ilits->spi_rx + offset + 1, n_rx);
		} else {
			input_err(true, ilits->dev, "%s spi read fail, status = %d\n", __func__, status);
		}
		break;
	default:
		input_info(true, ilits->dev, "%s Unknown command 0x%x\n", __func__, cmd);
		break;
	}

out:
	return status;
}
#endif

static int ili_spi_mp_pre_cmd(u8 cdc)
{
	u8 pre[5] = {0};

	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

	if (!atomic_read(&ilits->mp_stat) || cdc != P5_X_SET_CDC_INIT ||
		ilits->chip->core_ver >= CORE_VER_1430)
		return 0;

#ifdef CONFIG_SAMSUNG_TUI
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	ILI_DBG("%s mp test with pre commands\n", __func__);

	pre[0] = SPI_WRITE;
	pre[1] = 0x0;// dummy byte
	pre[2] = 0x2;// Write len byte
	pre[3] = P5_X_READ_DATA_CTRL;
	pre[4] = P5_X_GET_CDC_DATA;
	if (ilits->spi_write_then_read(ilits->spi, pre, 5, NULL, 0) < 0) {
		input_err(true, ilits->dev, "%s Failed to write pre commands\n", __func__);
		return -1;
	}

	pre[0] = SPI_WRITE;
	pre[1] = 0x0;// dummy byte
	pre[2] = 0x1;// Write len byte
	pre[3] = P5_X_GET_CDC_DATA;
	if (ilits->spi_write_then_read(ilits->spi, pre, 4, NULL, 0) < 0) {
		input_err(true, ilits->dev, "%s Failed to write pre commands\n", __func__);
		return -1;
	}
	return 0;
}

static int ili_spi_pll_clk_wakeup(void)
{
	int index = 0;
	u8 wdata[32] = {0};
	u8 wakeup[9] = {0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3};
	u32 wlen = sizeof(wakeup);

	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

	wdata[0] = SPI_WRITE;
	wdata[1] = (wlen >> 8) & 0xFF;
	wdata[2] = wlen & 0xff;
	index = 3;
	wlen += index;

	ipio_memcpy(&wdata[index], wakeup, sizeof(wakeup), wlen);

	input_info(true, ilits->dev, "%s Write dummy to wake up spi pll clk\n", __func__);
	if (ilits->spi_write_then_read(ilits->spi, wdata, wlen, NULL, 0) < 0) {
		input_info(true, ilits->dev, "%s spi slave write error\n", __func__);
		return -1;
	}

	return 0;
}

static int ili_spi_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool spi_irq, bool i2c_irq)
{
	int ret = 0;
	int mode = 0, index = 0;
	u8 wdata[32] = {0};
	u8 checksum = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

#ifdef CONFIG_SAMSUNG_TUI
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (wlen > 0) {
		if (!txbuf) {
			input_err(true, ilits->dev, "%s txbuf is null\n", __func__);
			return -ENOMEM;
		}

		/* 3 bytes data consist of length and header */
		if ((wlen + 4) > sizeof(wdata)) {
			input_err(true, ilits->dev, "%s WARNING! wlen(%d) > wdata(%d), using wdata length to transfer\n",
				__func__, wlen, (int)sizeof(wdata));
			return -ENOMEM;
		}
	}

	if (rlen > 0) {
		if (!rxbuf) {
			input_err(true, ilits->dev, "%s rxbuf is null\n", __func__);
			return -ENOMEM;
		}
	}

	if (rlen > 0 && !wlen)
		mode = SPI_READ;
	else
		mode = SPI_WRITE;

	if (ilits->int_pulse)
		ilits->detect_int_stat = ili_ic_check_int_pulse;
	else
		ilits->detect_int_stat = ili_ic_check_int_level;

	if (spi_irq)
		atomic_set(&ilits->cmd_int_check, ENABLE);

	switch (mode) {
	case SPI_WRITE:
#if (PLL_CLK_WAKEUP_TP_RESUME == ENABLE)
		if (ilits->pll_clk_wakeup == true) {
#else
		if ((ilits->pll_clk_wakeup == true) && (ilits->tp_suspend == true)) {
#endif
			ret = ili_spi_pll_clk_wakeup();
			if (ret < 0) {
				input_err(true, ilits->dev, "%s Wakeup pll clk error\n", __func__);
				break;
			}
		}
		if (ice) {
			wdata[0] = SPI_WRITE;
			index = 1;
		} else {
			wdata[0] = SPI_WRITE;
			wdata[1] = wlen >> 8;
			wdata[2] = wlen & 0xff;
			index = 3;
		}

		wlen += index;

		ipio_memcpy(&wdata[index], txbuf, wlen, wlen);

		/*
		 * NOTE: If TP driver is doing MP test and commanding 0xF1 to FW, we add a checksum
		 * to the last index and plus 1 with size.
		 */
		if (atomic_read(&ilits->mp_stat) && wdata[index] == P5_X_SET_CDC_INIT) {
			checksum = ili_calc_packet_checksum(&wdata[index], wlen - index);
			wdata[wlen] = checksum;
			wlen++;
			wdata[1] = (wlen - index) >> 8;
			wdata[2] = (wlen - index) & 0xff;
			ili_dump_data(wdata, 8, wlen, 0, "mp cdc cmd with checksum");
		}

		ret = ilits->spi_write_then_read(ilits->spi, wdata, wlen, txbuf, 0);
		if (ret < 0) {
			input_info(true, ilits->dev, "%s spi-wrapper write error\n", __func__);
			break;
		}

		/* Won't break if it needs to read data following with writing. */
		if (!rlen)
			break;
	case SPI_READ:
		if (!ice && spi_irq) {
			/* Check INT triggered by FW when sending cmds. */
			if (ilits->detect_int_stat(false) < 0) {
				input_err(true, ilits->dev, "%s ERROR! Check INT timeout\n", __func__);
				ret = -ETIME;
				break;
			}
		}

		ret = ili_spi_mp_pre_cmd(wdata[3]);
		if (ret < 0)
			input_err(true, ilits->dev, "%s spi-wrapper mp pre cmd error\n", __func__);

		wdata[0] = SPI_READ;

		ret = ilits->spi_write_then_read(ilits->spi, wdata, 1, rxbuf, rlen);
		if (ret < 0)
			input_err(true, ilits->dev, "%s spi-wrapper read error\n", __func__);

		break;
/*
 * DEADCODE : prevent major issue.
 *	default:
 *		input_err(true, ilits->dev, "%s Unknown spi mode (%d)\n", __func__, mode);
 *		ret = -EINVAL;
 *		break;
 */
	}

	if (spi_irq)
		atomic_set(&ilits->cmd_int_check, DISABLE);

	return ret;
}

int ili_core_spi_setup(int num)
{
	u32 freq[] = {
		TP_SPI_CLK_1M,
		TP_SPI_CLK_2M,
		TP_SPI_CLK_3M,
		TP_SPI_CLK_4M,
		TP_SPI_CLK_5M,
		TP_SPI_CLK_6M,
		TP_SPI_CLK_7M,
		TP_SPI_CLK_8M,
		TP_SPI_CLK_9M,
		TP_SPI_CLK_10M,
		TP_SPI_CLK_11M,
		TP_SPI_CLK_12M,
		TP_SPI_CLK_13M,
		TP_SPI_CLK_14M,
		TP_SPI_CLK_15M
	};

	if (num >= (sizeof(freq) / sizeof(freq[0]))) {
		input_err(true, ilits->dev, "%s Invaild clk freq, set default clk freq\n", __func__);
		num = 7;
	}

	input_info(true, ilits->dev, "%s spi clock = %d\n", __func__, freq[num]);

	ilits->spi->mode = ilits->spi_mode;
	ilits->spi->bits_per_word = 8;
	ilits->spi->max_speed_hz = freq[num];

	if (spi_setup(ilits->spi) < 0) {
		input_err(true, ilits->dev, "%s Failed to setup spi device\n", __func__);
		return -ENODEV;
	}

	input_info(true, ilits->dev, "%s name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n",
			__func__, ilits->spi->modalias, ilits->spi->master->bus_num, ilits->spi->chip_select,
			ilits->spi->mode, ilits->spi->max_speed_hz);
	return 0;
}

static int ilitek_spi_probe(struct spi_device *spi)
{
	struct touch_bus_info *info;
	int ret;

	input_info(true, &spi->dev, "%s ilitek spi probe\n", __func__);

	if (!spi) {
		input_err(true,  &spi->dev, "%s spi device is NULL\n", __func__);
		return -ENODEV;
	}

	info = container_of(to_spi_driver(spi->dev.driver), struct touch_bus_info, bus_driver);

	ilits = devm_kzalloc(&spi->dev, sizeof(struct ilitek_ts_data), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits)) {
		input_err(true,  &spi->dev, "%s Failed to allocate ts memory, %ld\n", __func__, PTR_ERR(ilits));
		return -ENOMEM;
	}

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true,  &spi->dev, "%s Full duplex not supported by master\n", __func__);
		return -EIO;
	}

	ilits->update_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->update_buf)) {
		input_err(true,  &spi->dev, "%s fw kzalloc error\n", __func__);
		return -ENOMEM;
	}

	/* Used for receiving touch data only, do not mix up with others. */
	ilits->tr_buf = kzalloc(TR_BUF_SIZE, GFP_ATOMIC);
	if (ERR_ALLOC_MEM(ilits->tr_buf)) {
		input_err(true, &spi->dev, "%s failed to allocate touch report buffer\n", __func__);
		return -ENOMEM;
	}

	ilits->spi_tx = kzalloc(SPI_TX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->spi_tx)) {
		input_err(true, &spi->dev, "%s Failed to allocate spi tx buffer\n", __func__);
		return -ENOMEM;
	}

	ilits->spi_rx = kzalloc(SPI_RX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ilits->spi_rx)) {
		input_err(true, &spi->dev, "%s Failed to allocate spi rx buffer\n", __func__);
		return -ENOMEM;
	}

	ilits->gcoord = kzalloc(sizeof(struct gesture_coordinate), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ilits->gcoord)) {
		input_err(true, &spi->dev, "%s Failed to allocate gresture coordinate buffer\n", __func__);
		return -ENOMEM;
	}

	ilits->i2c = NULL;
	ilits->spi = spi;
	ilits->dev = &spi->dev;
	ilits->hwif = info->hwif;
	ilits->phys = "SPI";
	ilits->wrapper = ili_spi_wrapper;
	ilits->detect_int_stat = ili_ic_check_int_pulse;
	ilits->int_pulse = true;
	ilits->mp_retry = false;
	ilits->tp_suspend = false;
	ilits->tp_shutdown = false;
	ilits->power_status = POWER_ON_STATUS;
	ilits->screen_off_sate = TP_RESUME;

#if SPI_DMA_TRANSFER_SPLIT
	ilits->spi_write_then_read = ili_spi_write_then_read_split;
#else
	ilits->spi_write_then_read = ili_spi_write_then_read_direct;
#endif

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;
#if AXIS_PACKET
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_INFO_LEN + P5_X_DEMO_MODE_PACKET_LEN +
				P5_X_DEMO_MODE_AXIS_LEN + P5_X_DEMO_MODE_STATE_INFO;
#endif
	ilits->tp_data_mode = AP_MODE;

	if (TDDI_RST_BIND)
		ilits->reset = TP_IC_WHOLE_RST;
	else
		ilits->reset = TP_HW_RST_ONLY;

	ilits->rst_edge_delay = 11;
	ilits->fw_upgrade_mode = UPGRADE_IRAM;
	ilits->mp_move_code = ili_move_mp_code_iram;
	ilits->gesture_move_code = ili_move_gesture_code_iram;
	ilits->esd_recover = ili_wq_esd_spi_check;
	ilits->ges_recover = ili_touch_esd_gesture_iram;
	ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ilits->gesture_demo_ctrl = DISABLE;
	ilits->wtd_ctrl = OFF;
	ilits->report = ENABLE;
	ilits->dnp = DISABLE;
	ilits->irq_tirgger_type = IRQF_TRIGGER_FALLING;
	ilits->info_from_hex = ENABLE;
	ilits->wait_int_timeout = AP_INT_TIMEOUT;
	ilits->prox_lp_scan_mode = false;
	ilits->started_prox_intensity = false;
	ilits->incell_power_state = false;
	ilits->prox_face_mode = false;
	ilits->dead_zone_enabled = true; //default true at fw
	ilits->sip_mode_enabled = false;
	ilits->high_sensitivity_mode_enabled = false;
	ilits->game_mode_enabled = false;
	ilits->clear_cover_mode_enabled = false;
	ilits->prox_lp_scan_mode_enabled = false;
	ilits->sleep_handler_mode = TP_RESUME;
	ilits->mp_test_item = MP_ITEM_NOISE_PEAK_TO_PEAK_WITH_PANEL;
	ilits->current_mpitem = "";

#if ENABLE_GESTURE
	ilits->gesture = DISABLE;
	ilits->ges_sym.double_tap = DOUBLE_TAP;
	ilits->ges_sym.alphabet_line_2_top = ALPHABET_LINE_2_TOP;
	ilits->ges_sym.alphabet_line_2_bottom = ALPHABET_LINE_2_BOTTOM;
	ilits->ges_sym.alphabet_line_2_left = ALPHABET_LINE_2_LEFT;
	ilits->ges_sym.alphabet_line_2_right = ALPHABET_LINE_2_RIGHT;
	ilits->ges_sym.alphabet_m = ALPHABET_M;
	ilits->ges_sym.alphabet_w = ALPHABET_W;
	ilits->ges_sym.alphabet_c = ALPHABET_C;
	ilits->ges_sym.alphabet_E = ALPHABET_E;
	ilits->ges_sym.alphabet_V = ALPHABET_V;
	ilits->ges_sym.alphabet_O = ALPHABET_O;
	ilits->ges_sym.alphabet_S = ALPHABET_S;
	ilits->ges_sym.alphabet_Z = ALPHABET_Z;
	ilits->ges_sym.alphabet_V_down = ALPHABET_V_DOWN;
	ilits->ges_sym.alphabet_V_left = ALPHABET_V_LEFT;
	ilits->ges_sym.alphabet_V_right = ALPHABET_V_RIGHT;
	ilits->ges_sym.alphabet_two_line_2_bottom = ALPHABET_TWO_LINE_2_BOTTOM;
	ilits->ges_sym.alphabet_F = ALPHABET_F;
	ilits->ges_sym.alphabet_AT = ALPHABET_AT;
#endif

	if (ili_core_spi_setup(SPI_CLK) < 0)
		return -EINVAL;

	ret = info->hwif->plat_probe();
#ifdef CONFIG_SAMSUNG_TUI
	if (!ret)
		ret = stui_set_info(stui_tsp_enter, stui_tsp_exit, STUI_TSP_TYPE_ILITEK);
#endif
	return ret;
}
/*
static void ilitek_spi_shutdown(struct spi_device *spi)
{
	input_info(true, ilits->dev, "%s\n", __func__);
	ilits->hwif->plat_shutdown();
	return;
}

static int ilitek_spi_remove(struct spi_device *spi)
{
	input_info(true, ilits->dev, "%s\n", __func__);
	return 0;
}
*/
static struct spi_device_id tp_spi_id[] = {
	{TDDI_DEV_ID, 0},
	{},
};

int ili_interface_dev_init(struct ilitek_hwif_info *hwif)
{
	struct touch_bus_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		printk(KERN_ERR "[sec_input] %s faied to allocate spi_driver\n", __func__);
		return -ENOMEM;
	}

	if (hwif->bus_type != BUS_SPI) {
		printk(KERN_ERR "[sec_input] %s Not SPI dev\n", __func__);
		ipio_kfree((void **)&info);
		return -EINVAL;
	}

	hwif->info = info;

	info->bus_driver.driver.name = hwif->name;
	info->bus_driver.driver.owner = hwif->owner;
	info->bus_driver.driver.of_match_table = hwif->of_match_table;
	info->bus_driver.driver.pm = hwif->pm;

	info->bus_driver.probe = ilitek_spi_probe;
/* Shutdown is called from the SemInputDeviceManagerService. */
//	info->bus_driver.shutdown = ilitek_spi_shutdown;
//	info->bus_driver.remove = ilitek_spi_remove;
	info->bus_driver.id_table = tp_spi_id;

	info->hwif = hwif;
	return spi_register_driver(&info->bus_driver);
}

void ili_interface_dev_exit(struct ilitek_ts_data *ts)
{
	struct touch_bus_info *info = (struct touch_bus_info *)ilits->hwif->info;

	input_info(true, ilits->dev, "%s remove spi dev\n", __func__);
	kfree(ilits->update_buf);
	kfree(ilits->spi_tx);
	kfree(ilits->spi_rx);
//	spi_unregister_driver(&info->bus_driver);
	ipio_kfree((void **)&info);
}

#ifdef CONFIG_SAMSUNG_TUI
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

static int stui_tsp_enter(void)
{
	int ret = 0;

	if (!ilits)
		return -EINVAL;

	if (!ilits->power_status) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

	ili_irq_unregister();

	ret = stui_spi_lock(ilits->spi->master);
	if (ret) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
		ili_irq_register(ilits->irq_tirgger_type);
		return -1;
	}

	return 0;
}

static int stui_tsp_exit(void)
{
	int ret = 0;

	if (!ilits)
		return -EINVAL;

	if (!ilits->power_status) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

	ret = stui_spi_unlock(ilits->spi->master);
	if (ret)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	ili_irq_register(ilits->irq_tirgger_type);

	return ret;
}
#endif
