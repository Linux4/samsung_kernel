/* linux/drivers/video/s5p_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * InKi Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>

#include <plat/fb.h>
#include <plat/regs-mipidsim.h>
#include <plat/dsim.h>
#include <plat/cpu.h>
#include <plat/clock.h>

#include <mach/exynos-mipiphy.h>
#include <mach/map.h>

#include "s5p_mipi_dsi_lowlevel.h"
#include "s5p_mipi_dsi.h"

#include "./decon_display/decon_display_driver.h"
#include "./decon_display/decon_pm.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "exynos_display_handler.h"

static DEFINE_MUTEX(dsim_rd_wr_mutex);
static DECLARE_COMPLETION(dsim_wr_comp);
static DECLARE_COMPLETION(dsim_rd_comp);

#define MIPI_WR_TIMEOUT msecs_to_jiffies(35)
#define MIPI_RD_TIMEOUT msecs_to_jiffies(35)

#define FIMD_REG_BASE			S5P_PA_FIMD0
#define FIMD_MAP_SIZE			SZ_256K

static unsigned int dpll_table[15] = {
	100, 120, 170, 220, 270,
	320, 390, 450, 510, 560,
	640, 690, 770, 870, 950
};

static int s5p_mipi_clk_state(struct clk *clk)
{
	return clk->usage;
}

static int s5p_mipi_en_state(struct mipi_dsim_device *dsim)
{
	int ret = (!pm_runtime_suspended(dsim->dev) && s5p_mipi_clk_state(dsim->clock));

	return ret;
}

static int s5p_mipi_force_enable(struct mipi_dsim_device *dsim)
{
	if (pm_runtime_suspended(dsim->dev))
		pm_runtime_get_sync(dsim->dev);
	if (!s5p_mipi_clk_state(dsim->clock))
		clk_enable(dsim->clock);
	return 0;
}

static int s5p_mipi_dsi_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct mipi_dsim_device *dsim;

	dsim = container_of(self, struct mipi_dsim_device, fb_notif);

	switch (event) {
	case FB_EVENT_RESUME:
	case FB_BLANK_UNBLANK:
	default:
		break;
	}

	return 0;
}

static int s5p_mipi_dsi_register_fb(struct mipi_dsim_device *dsim)
{
	memset(&dsim->fb_notif, 0, sizeof(dsim->fb_notif));
	dsim->fb_notif.notifier_call = s5p_mipi_dsi_fb_notifier_callback;

	return fb_register_client(&dsim->fb_notif);
}

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim,
	u8 cmd, const u8 *buf, u32 len)
{
	int i;
	unsigned char tempbuf[2] = {0, };
	int remind, temp;
	unsigned int payload;
	int ret = len;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv = get_display_driver();
	disp_pm_add_refcount(dispdrv);
#endif

	if (dsim->enabled == false) {
		dev_dbg(dsim->dev, "MIPI DSIM is disabled.\n");
		return -EINVAL;
	}

	if (dsim->state != DSIM_STATE_ULPS && dsim->state != DSIM_STATE_HSCLKEN) {
		dev_dbg(dsim->dev, "MIPI DSIM is not ready.\n");
		return -EINVAL;
	}

	if (!s5p_mipi_en_state(dsim)) {
		s5p_mipi_force_enable(dsim);
		dev_warn(dsim->dev, "%s: MIPI state check!!\n", __func__);
	} else
		dev_dbg(dsim->dev, "MIPI state is valied.\n");

	mutex_lock(&dsim_rd_wr_mutex);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	disp_pm_gate_lock(dispdrv, true);
#endif

	memset(tempbuf, 0, sizeof(tempbuf));

	if (len <= 2) {
		for (i = 0; i < len; i++)
			tempbuf[i] = buf[i];
	} else {
		tempbuf[0] = len & 0xff;
		tempbuf[1] = (len >> 8) & 0xff;
	}
	switch (cmd) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		break;

	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		break;

	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		break;

	/* short and response packet types for command */
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
		/* process response func should be implemented. */
		break;

	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		break;

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
#if defined CONFIG_BACKLIGHT_POLLING_AMOLED
		int srcval, timeout = 3200; /* 32ms mipi timeout */
#endif
		INIT_COMPLETION(dsim_wr_comp);
		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_SFR_FIFO_EMPTY);
		temp = 0;
		remind = len;
		for (i = 0; i < (len/4); i++) {
			temp = i * 4;
			payload = buf[temp] |
						buf[temp + 1] << 8 |
						buf[temp + 2] << 16 |
						buf[temp + 3] << 24;
			remind -= 4;
			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
		if (remind) {
			payload = 0;
			temp = len-remind;
			for (i = 0; i < remind; i++)
				payload |= buf[temp + i] << (i * 8);

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
		/* put data into header fifo */
		s5p_mipi_dsi_wr_tx_header(dsim, cmd, tempbuf);
#if defined CONFIG_BACKLIGHT_POLLING_AMOLED
		while(1) {
			if((srcval=readl(dsim->reg_base +  S5P_DSIM_INTSRC) & 0x20000000)) {
				break;
			}
			udelay(10);
			if(--timeout <=0 ) {
				pr_err("[ERROR] MIPI DSIM write timeout: 0x%08X\n", srcval);
				ret = -ETIMEDOUT;
				goto exit;
			}
		}
		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_SFR_FIFO_EMPTY);
#else
		if (!wait_for_completion_interruptible_timeout(&dsim_wr_comp,
				MIPI_WR_TIMEOUT)) {

				dev_err(dsim->dev, "MIPI DSIM write Timeout!\n");
				ret = -ETIMEDOUT;
				goto exit;
		}
#endif
		break;
	}

	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n", cmd);

		ret = -EINVAL;
		goto exit;
	}

exit:
	if (dsim->enabled && (ret == -ETIMEDOUT)) {
		dev_info(dsim->dev, "0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
				readl(dsim->reg_base + S5P_DSIM_STATUS),
				readl(dsim->reg_base + S5P_DSIM_INTSRC),
				readl(dsim->reg_base + S5P_DSIM_FIFOCTRL),
				readl(dsim->reg_base + S5P_DSIM_MULTI_PKT));
		s5p_mipi_dsi_init_fifo_pointer(dsim, DSIM_INIT_SFR);
	}
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_gate_lock(dispdrv, false);
#endif
	mutex_unlock(&dsim_rd_wr_mutex);
	return ret;
}

static void s5p_mipi_dsi_rx_err_handler(struct mipi_dsim_device *dsim,
	u32 rx_fifo)
{
	/* Parse error report bit*/
	if (rx_fifo & (1 << 8))
		dev_err(dsim->dev, "SoT error!\n");
	if (rx_fifo & (1 << 9))
		dev_err(dsim->dev, "SoT sync error!\n");
	if (rx_fifo & (1 << 10))
		dev_err(dsim->dev, "EoT error!\n");
	if (rx_fifo & (1 << 11))
		dev_err(dsim->dev, "Escape mode entry command error!\n");
	if (rx_fifo & (1 << 12))
		dev_err(dsim->dev, "Low-power transmit sync error!\n");
	if (rx_fifo & (1 << 13))
		dev_err(dsim->dev, "HS receive timeout error!\n");
	if (rx_fifo & (1 << 14))
		dev_err(dsim->dev, "False control error!\n");
	/* Bit 15 is reserved*/
	if (rx_fifo & (1 << 16))
		dev_err(dsim->dev, "ECC error, single-bit(detected and corrected)!\n");
	if (rx_fifo & (1 << 17))
		dev_err(dsim->dev, "ECC error, multi-bit(detected, not corrected)!\n");
	if (rx_fifo & (1 << 18))
		dev_err(dsim->dev, "Checksum error(long packet only)!\n");
	if (rx_fifo & (1 << 19))
		dev_err(dsim->dev, "DSI data type not recognized!\n");
	if (rx_fifo & (1 << 20))
		dev_err(dsim->dev, "DSI VC ID invalid!\n");
	if (rx_fifo & (1 << 21))
		dev_err(dsim->dev, "Invalid transmission length!\n");
	/* Bit 22 is reserved */
	if (rx_fifo & (1 << 23))
		dev_err(dsim->dev, "DSI protocol violation!\n");
}

int s5p_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, u8 data_id,
	 u8 addr, u8 count, u8 *buf, u8 rxfifo_done)
{
	u32 rx_fifo, rx_size = 0;
	int i, j, retry;
	unsigned int temp;
	unsigned char res;

	INIT_COMPLETION(dsim_rd_comp);
	/* TODO: need to check power, clock state here. */
	/* It's checked at s5p_mipi_dsi_wr_data currently. */

	/* Set the maximum packet size returned */
	s5p_mipi_dsi_wr_data(dsim,
	    MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, &count, 1);

	 /* Read request */
	s5p_mipi_dsi_wr_data(dsim, data_id, &addr, 1);
	if (!wait_for_completion_interruptible_timeout(&dsim_rd_comp,
		MIPI_RD_TIMEOUT)) {
		dev_err(dsim->dev, "MIPI DSIM read Timeout!\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim_rd_wr_mutex);

	retry = DSIM_MAX_RX_FIFO;
check_rx_header:
	if (!retry--) {
		dev_err(dsim->dev, "dsim read fail.. can't not recovery rx data\n");
		goto read_fail;
	}

	temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
	res = (unsigned char)temp & 0x000000ff;
	/* Parse the RX packet data types */
	switch (res) {

	case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
		s5p_mipi_dsi_rx_err_handler(dsim, temp);
		goto read_fail;
	case MIPI_DSI_RX_END_OF_TRANSMISSION:
		dev_dbg(dsim->dev, "EoTp was received from LCD module.\n");
		break;
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
	case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		dev_dbg(dsim->dev, "Short Packet was received from LCD module.\n");
		for (i = 0; i < count; i++)
			buf[i] = (temp >> (8 + i * 8)) & 0xff;
		rx_size = count;
		break;
	case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
	case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
		rx_size = (temp >> 8) & 0x0000ffff;
		dev_dbg(dsim->dev, "rx fifo : %8x, response : %x, rx_size : %d\n",
				temp, res, rx_size);
		if (count != rx_size) {
			dev_err(dsim->dev, "wrong rx size ..\n");
			goto check_rx_header;
		}
		dev_dbg(dsim->dev, "Long Packet was received from LCD module.\n");
		/* Read data from RX packet payload */
		for (i = 0; i < rx_size >> 2; i++) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			for (j = 0; j < 4; j++)
				buf[(i*4)+j] = (u8)(rx_fifo >> (j * 8)) & 0xff;
		}
		if (rx_size % 4) {
			rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			for (j = 0; j < rx_size % 4; j++)
				buf[(4*i)+j] = (u8)(rx_fifo >> (j*8)) & 0xff;
		}
		break;
	default:
		dev_err(dsim->dev, "Invalid packet header\n");
		goto check_rx_header;
		break;
	}

	/* for Magna DDI Temporary patch */
	if (!rxfifo_done) {
		mutex_unlock(&dsim_rd_wr_mutex);
		return rx_size;
	}

	retry = DSIM_MAX_RX_FIFO;
check_rx_done:
	if (!retry--) {
		dev_err(dsim->dev, "can't not received rx done ..\n");
		goto read_fail;
	}
	temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
	if (temp != DSIM_RX_FIFO_READ_DONE) {
		dev_warn(dsim->dev, "Can't found RX FIFO READ DONE FLAG : %x\n", temp);
		goto check_rx_done;
	}

	mutex_unlock(&dsim_rd_wr_mutex);
	return rx_size;

read_fail:
	for (i = 0; i < DSIM_MAX_RX_FIFO; i++) {
		temp = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
		dev_err(dsim->dev, "Rx FIFO[%d] : %8x\n", i, temp);
	}
	mutex_unlock(&dsim_rd_wr_mutex);
	return 0;
}

static int s5p_mipi_dsi_pll_on(struct mipi_dsim_device *dsim, unsigned int enable)
{
	int sw_timeout;

	if (enable) {
		sw_timeout = 100;

		s5p_mipi_dsi_clear_interrupt(dsim, INTSRC_PLL_STABLE);
		s5p_mipi_dsi_enable_pll(dsim, 1);
		while (1) {
			usleep_range(1000, 1000);
			sw_timeout--;
			if (s5p_mipi_dsi_is_pll_stable(dsim))
				return 0;
			if (sw_timeout == 0) {
				pr_err("mipi pll on fail!!!\n");
				return -EINVAL;
		}
		}
	} else
		s5p_mipi_dsi_enable_pll(dsim, 0);

	return 0;
}

static unsigned long s5p_mipi_dsi_change_pll(struct mipi_dsim_device *dsim,
	unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler)
{
	unsigned long dfin_pll, dfvco, dpll_out;
	unsigned int i, freq_band = 0xf;

	dfin_pll = (FIN_HZ / pre_divider);

	if (soc_is_exynos5250() || soc_is_exynos3250()|| soc_is_exynos3470()
		|| soc_is_exynos3472()) {
		if (dfin_pll < DFIN_PLL_MIN_HZ || dfin_pll > DFIN_PLL_MAX_HZ) {
			dev_warn(dsim->dev, "fin_pll range should be 6MHz ~ 12MHz\n");
			s5p_mipi_dsi_enable_afc(dsim, 0, 0);
		} else {
			if (dfin_pll < 7 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x1);
			else if (dfin_pll < 8 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x0);
			else if (dfin_pll < 9 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x3);
			else if (dfin_pll < 10 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x2);
			else if (dfin_pll < 11 * MHZ)
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x5);
			else
				s5p_mipi_dsi_enable_afc(dsim, 1, 0x4);
		}
	}
	dfvco = dfin_pll * main_divider;
	dev_dbg(dsim->dev, "dfvco: %lu, dfin_pll: %lu, main_divider: %d\n",
				dfvco, dfin_pll, main_divider);

	if (soc_is_exynos5250() || soc_is_exynos3250() || soc_is_exynos3470()
		|| soc_is_exynos3472()) {
		if (dfvco < DFVCO_MIN_HZ || dfvco > DFVCO_MAX_HZ)
			dev_warn(dsim->dev, "fvco range should be 500MHz ~ 1000MHz\n");
	}

	dpll_out = dfvco / (1 << scaler);

	/* condition with DPHY support upto 1G */
	if (!dphy_support_upto_1GHz())
		dpll_out *= 2;

	dev_dbg(dsim->dev, "dpll_out: %lu, dfvco: %lu, scaler: %d\n",
		dpll_out, dfvco, scaler);

	if (soc_is_exynos5250() || soc_is_exynos3250() || soc_is_exynos3470()
		 || soc_is_exynos3472()) {
		for (i = 0; i < ARRAY_SIZE(dpll_table); i++) {
			if (dpll_out < dpll_table[i] * MHZ) {
				freq_band = i;
				break;
			}
		}
		dev_dbg(dsim->dev, "freq_band: %d\n", freq_band);
	}

	s5p_mipi_dsi_pll_freq(dsim, pre_divider, main_divider, scaler);

	s5p_mipi_dsi_hs_zero_ctrl(dsim, 0);
	s5p_mipi_dsi_prep_ctrl(dsim, 0);

	if (soc_is_exynos5250() || soc_is_exynos3250() || soc_is_exynos3470()
		|| soc_is_exynos3472()) {
		/* Freq Band */
		s5p_mipi_dsi_pll_freq_band(dsim, freq_band);
	}
	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PLL */
	dev_dbg(dsim->dev, "FOUT of mipi dphy pll is %luMHz\n",
		(dpll_out / MHZ));

	return dpll_out;
}

static int s5p_mipi_dsi_set_clock(struct mipi_dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable)
{
	unsigned int esc_div;
	unsigned long esc_clk_error_rate;
	unsigned long byte_clk;
	unsigned long escape_clk;
	unsigned long hs_clk;

	if (enable) {
		dsim->e_clk_src = byte_clk_sel;

		/* Escape mode clock and byte clock source */
		s5p_mipi_dsi_set_byte_clock_src(dsim, byte_clk_sel);

		/* DPHY, DSIM Link : D-PHY clock out */
		if (byte_clk_sel == DSIM_PLL_OUT_DIV8) {
			hs_clk = s5p_mipi_dsi_change_pll(dsim,
				dsim->dsim_config->p, dsim->dsim_config->m,
				dsim->dsim_config->s);
			if (hs_clk == 0) {
				dev_err(dsim->dev,
					"failed to get hs clock.\n");
				return -EINVAL;
			}
			if (!soc_is_exynos5250() && !soc_is_exynos3250() && !soc_is_exynos3470() && !soc_is_exynos3472()) {
#if defined(CONFIG_LCD_MIPI_S6E3FA0)
				s5p_mipi_dsi_set_b_dphyctrl(dsim, 0x0AF);
				s5p_mipi_dsi_set_timing_register0(dsim, 0x08, 0x0d);
				s5p_mipi_dsi_set_timing_register1(dsim, 0x09, 0x30,
									0x0e, 0x0a);
				s5p_mipi_dsi_set_timing_register2(dsim, 0x0c, 0x11,
									0x0d);
#else
				s5p_mipi_dsi_set_b_dphyctrl(dsim, 0x0AF);
				s5p_mipi_dsi_set_timing_register0(dsim, 0x03, 0x06);
#ifdef CONFIG_LCD_MIPI_D6EA8061
				s5p_mipi_dsi_set_timing_register1(dsim, 0x04, 0x16, 0x0A, 0x05);
#else
				s5p_mipi_dsi_set_timing_register1(dsim, 0x04, 0x15, 0x09, 0x04);
#endif
				s5p_mipi_dsi_set_timing_register2(dsim, 0x05, 0x06, 0x07);
#endif
			}
			s5p_mipi_dsi_enable_pll_bypass(dsim, 0);
			s5p_mipi_dsi_pll_on(dsim, 1);
		} else {
			dev_warn(dsim->dev, "this project only support PLL clock source for MIPI DSIM\n");
			return 0;
		}

		/* escape clock divider */
		byte_clk = hs_clk / 8;
		esc_div = byte_clk / (dsim->dsim_config->esc_clk);
		dev_dbg(dsim->dev, "esc_div: %d, byte_clk: %lu, esc_clk: %lu\n",
			esc_div, byte_clk, dsim->dsim_config->esc_clk);

		if ((byte_clk / esc_div) >= (20 * MHZ) || (byte_clk / esc_div) > dsim->dsim_config->esc_clk)
			esc_div += 1;

		escape_clk = byte_clk / esc_div;

		/* enable byte clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_ON);

		/* enable escape clock */
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 1, esc_div);

		/* escape clock on lane */
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim, (DSIM_LANE_CLOCK | dsim->data_lane), 1);

		dev_info(dsim->dev, "escape_clk: %lu, byte_clk: %lu, esc_div: %d, hs_clk: %lu\n", escape_clk, byte_clk, esc_div, hs_clk);

		if ((byte_clk / esc_div) > escape_clk) {
			esc_clk_error_rate = escape_clk / (byte_clk / esc_div);
			dev_warn(dsim->dev, "error rate is %lu over.\n", (esc_clk_error_rate / 100));
		} else if ((byte_clk / esc_div) < (escape_clk)) {
			esc_clk_error_rate = (byte_clk / esc_div) / escape_clk;
			dev_warn(dsim->dev, "error rate is %lu under.\n", (esc_clk_error_rate / 100));
		}
	} else {
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 0, 0);

		/* disable escape clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_OFF);

		if (byte_clk_sel == DSIM_PLL_OUT_DIV8)
			s5p_mipi_dsi_pll_on(dsim, 0);
	}

	return 0;
}

static void s5p_mipi_dsi_d_phy_onoff(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	exynos_dsim_phy_enable(dsim->id, enable);
}

static int s5p_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);

	dsim->state = DSIM_STATE_INIT;

	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

	s5p_mipi_dsi_sw_reset(dsim);
	s5p_mipi_dsi_dp_dn_swap(dsim, 0);

	return 0;
}

#if 0
static int s5p_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/* enable only frame done interrupt */
	s5p_mipi_dsi_set_interrupt_mask(dsim, INTMSK_FRAME_DONE, enable);

	return 0;
}
#endif

static int s5p_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim,
	struct mipi_dsim_config *dsim_config)
{
	struct fb_videomode *lcd_video = NULL;
	struct s3c_fb_pd_win *pd;
	unsigned int width = 0, height = 0;
	pd = (struct s3c_fb_pd_win *)dsim->dsim_config->lcd_panel_info;
	lcd_video = (struct fb_videomode *)&pd->win_mode;

	width = dsim->pd->dsim_lcd_config->lcd_size.width;
	height = dsim->pd->dsim_lcd_config->lcd_size.height;

	/* in case of VIDEO MODE (RGB INTERFACE) */
	if (dsim->dsim_config->e_interface == (u32) DSIM_VIDEO) {
			s5p_mipi_dsi_set_main_disp_vporch(dsim,
				dsim->pd->dsim_lcd_config->rgb_timing.cmd_allow,
				dsim->pd->dsim_lcd_config->rgb_timing.stable_vfp,
				dsim->pd->dsim_lcd_config->rgb_timing.upper_margin);
			s5p_mipi_dsi_set_main_disp_hporch(dsim,
				dsim->pd->dsim_lcd_config->rgb_timing.right_margin,
				dsim->pd->dsim_lcd_config->rgb_timing.left_margin);
			s5p_mipi_dsi_set_main_disp_sync_area(dsim,
				dsim->pd->dsim_lcd_config->rgb_timing.vsync_len,
				dsim->pd->dsim_lcd_config->rgb_timing.hsync_len);
	}

	s5p_mipi_dsi_set_main_disp_resol(dsim, height, width);
	s5p_mipi_dsi_display_config(dsim);
	return 0;
}

static int s5p_mipi_dsi_init_link(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 100;

	switch (dsim->state) {
	case DSIM_STATE_INIT:
		s5p_mipi_dsi_sw_reset(dsim);

		/* dsi configuration */
		s5p_mipi_dsi_init_config(dsim);

		/* set clock configuration */
		s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

		s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
		s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

		/* check clock and data lane state are stop state */
		while (!(s5p_mipi_dsi_is_lane_state(dsim))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"DSI Master is not stop state.\n");
				dev_err(dsim->dev,
					"Check initialization process\n");
				return -EINVAL;
			}
		}

		if (time_out != 0) {
			dev_dbg(dsim->dev,
				"DSI Master driver has been completed.\n");
			dev_dbg(dsim->dev, "DSI Master state is stop state\n");
		}

		dsim->state = DSIM_STATE_STOP;

		/* BTA sequence counters */
		s5p_mipi_dsi_set_stop_state_counter(dsim,
			dsim->dsim_config->stop_holding_cnt);
		s5p_mipi_dsi_set_bta_timeout(dsim,
			dsim->dsim_config->bta_timeout);
		s5p_mipi_dsi_set_lpdr_timeout(dsim,
			dsim->dsim_config->rx_timeout);

		return 0;
	default:
		dev_info(dsim->dev, "DSI Master is already init.\n");
		return 0;
	}

	return 0;
}

#if !defined(CONFIG_S5P_LCD_INIT)
static int s5p_mipi_dsi_setup_link(struct mipi_dsim_device *dsim)
{
	/* dsi configuration */
	s5p_mipi_dsi_init_config(dsim);
	s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
	s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

	/* set clock configuration */
	s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

	dsim->state = DSIM_STATE_STOP;

	/* BTA sequence counters */
	s5p_mipi_dsi_set_stop_state_counter(dsim,
		dsim->dsim_config->stop_holding_cnt);
	s5p_mipi_dsi_set_bta_timeout(dsim,
		dsim->dsim_config->bta_timeout);
	s5p_mipi_dsi_set_lpdr_timeout(dsim,
		dsim->dsim_config->rx_timeout);

	return 0;
}
#endif

static int s5p_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim)
{
	if (dsim->state == DSIM_STATE_STOP) {
		if (dsim->e_clk_src != DSIM_EXT_CLK_BYPASS) {
			dsim->state = DSIM_STATE_HSCLKEN;

			 /* set LCDC and CPU transfer mode to HS. */
			s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
			s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);

			s5p_mipi_dsi_enable_hs_clock(dsim, 1);

			return 0;
		} else
			dev_warn(dsim->dev,
				"clock source is external bypass.\n");
	} else
		dev_warn(dsim->dev, "DSIM is not stop state.\n");

	return 0;
}

static int s5p_mipi_dsi_set_data_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int mode)
{
	if (mode) {
		if (dsim->state != DSIM_STATE_HSCLKEN) {
			dev_err(dsim->dev, "HS Clock lane is not enabled.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
	} else {
		if (dsim->state == DSIM_STATE_INIT || dsim->state ==
			DSIM_STATE_ULPS) {
			dev_err(dsim->dev,
				"DSI Master is not STOP or HSDT state.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);
	}
	return 0;
}

#if 0
static int s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim)
{
	return _s5p_mipi_dsi_get_frame_done_status(dsim);
}

static int s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim)
{
	_s5p_mipi_dsi_clear_frame_done(dsim);

	return 0;
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
#if 0
static int s5p_mipi_dsi_set_interrupt(struct mipi_dsim_device *dsim, bool enable)
{
	unsigned int int_msk;

	int_msk = SFR_PL_FIFO_EMPTY | RX_DAT_DONE | MIPI_FRAME_DONE | ERR_RX_ECC;

	if (enable) {
		/* clear interrupt */
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 0);
	} else {
		s5p_mipi_dsi_set_interrupt_mask(dsim, int_msk, 1);
	}

	return 0;
}
#endif
#endif

extern unsigned int lpcharge;
static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	unsigned int int_src = 0;
	struct mipi_dsim_device *dsim = dev_id;

	spin_lock(&dsim->slock);

	int_src = readl(dsim->reg_base + S5P_DSIM_INTSRC);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	if (int_src & MIPI_FRAME_DONE) {
		if(dsim->pd->fb_draw_done ==0 && dsim->pd->fb_activate_vsync == 0 && dsim->cmd_transfer == 0) {
			if (dsim->pd->common_mode && !lpcharge) {
				udelay(21);
				s5p_mipi_dsi_enable_hs_clock(dsim,0); /*HS TO LP*/
			}
		}
	}
#endif

	if (int_src & SFR_PL_FIFO_EMPTY)
		complete(&dsim_wr_comp);
	if (int_src & RX_DAT_DONE)
		complete(&dsim_rd_comp);
	if (int_src & ERR_RX_ECC)
		dev_err(dsim->dev, "RX ECC Multibit error was detected!\n");
	s5p_mipi_dsi_clear_interrupt(dsim, int_src);

	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

static int s5p_mipi_dsi_enable(struct mipi_dsim_device *dsim)
{
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct lcd_platform_data *lcd_pd =
		(struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;

	dev_info(dsim->dev, "+%s\n", __func__);
	mutex_lock(&dsim->lock);

	if(dsim->enabled) {
		dev_info(dsim->dev, "-%s already enabled\n", __func__);
		mutex_unlock(&dsim->lock);
		return 0;
	}

	if (dsim->pd->clock_init) {
		if (dsim->pd->clock_init() < 0)
			dev_info(dsim->dev, "failed to setup dsi clock\n");
	}

	pm_runtime_get_sync(&pdev->dev);
#ifndef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	clk_enable(dsim->clock);
#endif

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);

	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 1);

	if (lcd_pd && lcd_pd->power_on_delay)
		usleep_range(lcd_pd->power_on_delay, lcd_pd->power_on_delay);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	if (lcd_pd && lcd_pd->reset)
		lcd_pd->reset(NULL);

	if (lcd_pd && lcd_pd->reset_delay)
		usleep_range(lcd_pd->reset_delay, lcd_pd->reset_delay);

	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	/* clear interrupt */
	s5p_mipi_dsi_clear_interrupt(dsim, 0xffffffff);

	/* enable interrupts */
#if defined CONFIG_BACKLIGHT_POLLING_AMOLED
	s5p_mipi_dsi_set_interrupt_mask(dsim, RX_DAT_DONE, 0);
#else
	s5p_mipi_dsi_set_interrupt_mask(dsim, SFR_PL_FIFO_EMPTY | RX_DAT_DONE, 0);
#endif

	dsim->enabled = true;

	usleep_range(18000, 18000);

	mutex_unlock(&dsim->lock);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

int s5p_mipi_dsi_enable_by_fimd(struct device *dsim_device)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->enabled == true)
		return 0;

	s5p_mipi_dsi_enable(dsim);
	return 0;
}

static int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim)
{
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct lcd_platform_data *lcd_pd =
		(struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;

	dev_info(dsim->dev, "+%s\n", __func__);

	mutex_lock(&dsim->lock);

	if(!dsim->enabled) {
		dev_info(dsim->dev, "- %s already disabled\n", __func__);
		mutex_unlock(&dsim->lock);
		return 0;
	}

	dsim->dsim_lcd_drv->suspend(dsim);

	dsim->enabled = false;

	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 0);

	/* disable interrupts */
	s5p_mipi_dsi_set_interrupt_mask(dsim, 0xffffffff, 1);

	/* disable HS clock */
	s5p_mipi_dsi_enable_hs_clock(dsim, 0);

	/* make CLK/DATA Lane as LP00 */
	s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 0);
	s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 0);

	s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 0);

	s5p_mipi_dsi_sw_reset(dsim);

	dsim->state = DSIM_STATE_SUSPEND;
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 0);

#ifndef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	clk_disable(dsim->clock);
#endif

	pm_runtime_put_sync(&pdev->dev);

	mutex_unlock(&dsim->lock);

	dev_info(dsim->dev, "-%s\n", __func__);

	return 0;
}

static int s5p_mipi_dsi_set_power(void *data, unsigned long power)
{
	struct mipi_dsim_device *dsim = data;

	if ((power == FB_BLANK_UNBLANK) && (!dsim->enabled))
		s5p_mipi_dsi_enable(dsim);
	else if ((power == FB_BLANK_POWERDOWN) && (dsim->enabled))
		s5p_mipi_dsi_disable(dsim);

	return 0;
}

int s5p_mipi_dsi_ulps_enable(struct mipi_dsim_device *dsim,
	unsigned int mode)
{
	int ret = 0;
	unsigned int time_out = 1000;

	if (mode == false) {
		if (dsim->state == DSIM_STATE_STOP)
			return ret;

		/* Exit ULPS clock and data lane */
		s5p_mipi_dsi_enable_ulps_exit_clk_data(dsim, 1);

		/* Check ULPS Exit request for data lane */
		while (!(s5p_mipi_dsi_is_ulps_lane_state(dsim, 0))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"%s: DSI Master is not stop state.\n", __func__);
				return -EBUSY;
			}
			usleep_range(10, 10);
		}

		/* Clear ULPS enter & exit state */
		s5p_mipi_dsi_enable_ulps_exit_clk_data(dsim, 0);

               dsim->state = DSIM_STATE_STOP;
	} else {
               if (dsim->state == DSIM_STATE_ULPS)
                       return ret;

		/* Disable TxRequestHsClk */
		s5p_mipi_dsi_enable_hs_clock(dsim, 0);

		/* Enable ULPS clock and data lane */
		s5p_mipi_dsi_enable_ulps_clk_data(dsim, 1);

		/* Check ULPS request for data lane */
		while (!(s5p_mipi_dsi_is_ulps_lane_state(dsim, 1))) {
			time_out--;
			if (time_out == 0) {
				dev_err(dsim->dev,
					"%s: DSI Master is not ULPS state.\n", __func__);

				/* Enable ULPS clock and data lane */
				s5p_mipi_dsi_enable_ulps_clk_data(dsim, 1);

				/* Disable TxRequestHsClk */
				s5p_mipi_dsi_enable_hs_clock(dsim, 0);
				return -EBUSY;
			}
			usleep_range(10, 10);
		}

		/* Clear ULPS enter & exit state */
		s5p_mipi_dsi_enable_ulps_clk_data(dsim, 0);

		dsim->state = DSIM_STATE_ULPS;
	}

	return ret;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
int s5p_mipi_dsi_hibernation_power_on(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	struct platform_device *pdev = to_platform_device(dsim->dev);

	if (dsim->enabled == true)
		return 0;

	pm_runtime_get_sync(&pdev->dev);

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 1);
	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PHY PLL */
	s5p_mipi_dsi_pll_on(dsim, 1);

	/* Exit ULPS mode clk & data */
	s5p_mipi_dsi_ulps_enable(dsim, false);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);
	dsim->enabled = true;

	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	//s5p_mipi_dsi_set_interrupt(dsim, true);

	return 0;
}

int s5p_mipi_dsi_hibernation_power_off(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	struct platform_device *pdev = to_platform_device(dsim->dev);
	if (dsim->enabled == false)
		return 0;

	//s5p_mipi_dsi_set_interrupt(dsim, false);

	/* Enter ULPS mode clk & data */
	s5p_mipi_dsi_ulps_enable(dsim, true);

	/* DSIM STOP SEQUENCE */
	/* Set main stand-by off */
	s5p_mipi_dsi_enable_main_standby(dsim, 0);

	/* CLK and LANE disable */
	s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 0);
	s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 0);

	/* escape clock on lane */
	s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);

	/* Disable byte clock */
	s5p_mipi_dsi_enable_byte_clock(dsim, 0);

	/* Disable PHY PLL */
	s5p_mipi_dsi_pll_on(dsim, 0);

	/* S/W reset */
	s5p_mipi_dsi_sw_reset(dsim);

	/* PPI signal disable + D-PHY reset */
	s5p_mipi_dsi_d_phy_onoff(dsim, 0);

	dsim->enabled = false;

	pm_runtime_put_sync(&pdev->dev);

	return 0;
}
#endif

void s5p_mipi_dsi_hs_ctrl_by_fimd(struct device *dsim_device, unsigned int enable)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	s5p_mipi_dsi_enable_hs_clock(dsim, enable);

}

int s5p_mipi_dsi_disable_by_fimd(struct device *dsim_device)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->enabled == false)
		return 0;

	s5p_mipi_dsi_disable(dsim);
	return 0;
}

int s5p_mipi_dsi_displayon_by_fimd(struct device *dsim_device)
{
	struct platform_device *pdev = to_platform_device(dsim_device);
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	dsim->dsim_lcd_drv->displayon(dsim);

	return 0;
}

int create_mipi_dsi_controller(struct platform_device *pdev)
{
	int ret = 0;
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);
	struct display_driver *dispdrv;

	dispdrv = get_display_driver();

	dispdrv->dsi_driver.dsim = dsim;

	return ret;
}

static int s5p_mipi_dsi_probe(struct platform_device *pdev)
{
	int i;
	unsigned int rx_fifo;
	struct resource *res;
	struct mipi_dsim_device *dsim = NULL;
	struct mipi_dsim_config *dsim_config;
	struct s5p_platform_mipi_dsim *dsim_pd;
	struct lcd_platform_data *lcd_pd = NULL;
	int ret = -EPERM;

	if (!dsim)
		dsim = kzalloc(sizeof(struct mipi_dsim_device),
			GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -ENOMEM;
	}

	dsim->pd = to_dsim_plat(&pdev->dev);
	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;

	spin_lock_init(&dsim->slock);

	ret = s5p_mipi_dsi_register_fb(dsim);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fb notifier chain\n");
		kfree(dsim);
		return -EFAULT;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	/* get s5p_platform_mipi_dsim. */
	dsim_pd = (struct s5p_platform_mipi_dsim *)dsim->pd;
	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	dsim->dsim_config = dsim_config;

	dsim->clock = clk_get(&pdev->dev, dsim->pd->clk_name);
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source, %s\n", dsim->pd->clk_name);
		goto err_clock_get;
	}
	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -EINVAL;
		goto err_platform_get;
	}
	res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	dsim->res = res;
	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/* clear interrupt */
	s5p_mipi_dsi_clear_interrupt(dsim, 0xffffffff);

	dsim->irq = platform_get_irq(pdev, 0);
	if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
				IRQF_DISABLED, "mipi-dsi", dsim)) {
		dev_err(&pdev->dev, "request_irq failed.\n");
		ret = -EINVAL;
		goto err_irq;
	}

	dsim->dsim_lcd_drv = dsim->dsim_config->dsim_ddi_pd;

	platform_set_drvdata(pdev, dsim);

	if (dsim->pd->dsim_lcd_config->mipi_ddi_pd)
		lcd_pd = (struct lcd_platform_data *)dsim->pd->dsim_lcd_config->mipi_ddi_pd;
	else
		dev_err(&pdev->dev, "skip to register lcd platform data\n");

	/* Clear RX FIFO to prevent obnormal read opeation */
	for (i = 0; i < DSIM_MAX_RX_FIFO; i++)
		rx_fifo = readl(dsim->reg_base + S5P_DSIM_RXFIFO);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);

	if (lcd_pd && lcd_pd->power_on)
		lcd_pd->power_on(NULL, 1);

#if defined(CONFIG_S5P_LCD_INIT)
	if (lcd_pd && lcd_pd->power_on_delay)
		usleep_range(lcd_pd->power_on_delay, lcd_pd->power_on_delay);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	if (lcd_pd && lcd_pd->reset)
		lcd_pd->reset(NULL);

	if (lcd_pd && lcd_pd->reset_delay)
		usleep_range(lcd_pd->reset_delay, lcd_pd->reset_delay);
#else
	s5p_mipi_dsi_setup_link(dsim);
#endif
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);
	s5p_mipi_dsi_set_hs_enable(dsim);

	/* enable interrupts */
#if defined CONFIG_BACKLIGHT_POLLING_AMOLED
	s5p_mipi_dsi_set_interrupt_mask(dsim, RX_DAT_DONE, 0);
#else
	s5p_mipi_dsi_set_interrupt_mask(dsim, SFR_PL_FIFO_EMPTY | RX_DAT_DONE, 0);
#endif

	mutex_init(&dsim_rd_wr_mutex);
	mutex_init(&dsim->lock);

	dsim->enabled = true;
	dsim->dsim_lcd_drv->probe(dsim);
#if defined(CONFIG_S5P_LCD_INIT)
	dsim->dsim_lcd_drv->displayon(dsim);
#endif
	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_config->e_interface == DSIM_COMMAND) ? "CPU" : "RGB");

	register_display_handler(s5p_mipi_dsi_set_power, SUSPEND_LEVEL_DSIM, dsim);

	/* should be call to use HDM */
	dsi_display_driver_init(pdev);

	return 0;

err_irq:
	release_resource(dsim->res);
	kfree(dsim->res);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
err_platform_get:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);

err_clock_get:
	kfree(dsim);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return ret;

}

static int __devexit s5p_mipi_dsi_remove(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	free_irq(dsim->irq, dsim);

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	release_resource(dsim->res);
	kfree(dsim->res);

	kfree(dsim);

	return 0;
}

static void s5p_mipi_dsi_shutdown(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	struct display_driver *dispdrv = get_display_driver();
	disp_pm_add_refcount(dispdrv);
#endif

	dev_info(dsim->dev, "+%s\n", __func__);

	if (dsim->enabled)
		s5p_mipi_dsi_disable(dsim);

	dev_info(dsim->dev, "-%s\n", __func__);
}

static struct platform_driver s5p_mipi_dsi_driver = {
	.probe = s5p_mipi_dsi_probe,
	.remove = __devexit_p(s5p_mipi_dsi_remove),
	.shutdown = s5p_mipi_dsi_shutdown,
	.driver = {
		   .name = "s5p-mipi-dsim",
		   .owner = THIS_MODULE,
	},
};

static int s5p_mipi_dsi_register(void)
{
	platform_driver_register(&s5p_mipi_dsi_driver);

	return 0;
}

static void s5p_mipi_dsi_unregister(void)
{
	platform_driver_unregister(&s5p_mipi_dsi_driver);
}
#if defined(CONFIG_FB_EXYNOS_FIMD_MC) || defined(CONFIG_FB_EXYNOS_FIMD_MC_WB)
late_initcall(s5p_mipi_dsi_register);
#else
module_init(s5p_mipi_dsi_register);
#endif
module_exit(s5p_mipi_dsi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPL");
