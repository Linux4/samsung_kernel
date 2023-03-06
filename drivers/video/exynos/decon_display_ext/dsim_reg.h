/* linux/drivers/video/decon_display/dsim_reg.h
 *
 * Header file for Samsung MIPI-DSI lowlevel driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _DSIM_REG_H
#define _DSIM_REG_H

#include <linux/io.h>
#include <linux/delay.h>

#include "decon_mipi_dsi.h"
#include "regs-dsim.h"
#include "decon_display_driver.h"

#define DSIM_PIXEL_FORMAT_RGB24		0x7
#define DSIM_PIXEL_FORMAT_RGB18		0x6
#define DSIM_PIXEL_FORMAT_RGB18_PACKED	0x5

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	unsigned long pll_freq; /* in/out parameter: Mhz */
};

struct dsim_clks {
	unsigned long hs_clk;
	unsigned long esc_clk;
	unsigned long byte_clk;
};

static inline struct mipi_dsim_device *get_dsim_drvdata_ext(void)
{
	struct display_driver *dispdrv = get_display_driver_ext();
	return dispdrv->dsi_driver.dsim;
}

static inline int dsim_wr_data_ext(u32 id, u32 d0, u32 d1)
{
	int ret;
	struct mipi_dsim_device *dsim = get_dsim_drvdata_ext();

	ret = s5p_mipi_dsi_wr_data_ext(dsim, id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

/* register access subroutines */
static inline u32 dsim_read_ext(u32 reg_id)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata_ext();
	return readl(dsim->reg_base + reg_id);
}

static inline u32 dsim_read_ext_mask_ext(u32 reg_id, u32 mask)
{
	u32 val = dsim_read_ext(reg_id);
	val &= (~mask);
	return val;
}

static inline void dsim_write_ext(u32 reg_id, u32 val)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata_ext();
	writel(val, dsim->reg_base + reg_id);
}

static inline void dsim_write_mask_ext(u32 reg_id, u32 val, u32 mask)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata_ext();
	u32 old = dsim_read_ext(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->reg_base + reg_id);
}

#define dsim_err_ext(fmt, ...)							\
	do {									\
		dev_err(get_dsim_drvdata_ext()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define dsim_info_ext(fmt, ...)							\
	do {									\
		dev_dbg(get_dsim_drvdata_ext()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define dsim_dbg_ext(fmt, ...)							\
	do {									\
		dev_dbg(get_dsim_drvdata_ext()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

/* CAL APIs list */
void dsim_reg_init_ext(struct decon_lcd *lcd_info, u32 data_lane_cnt);
void dsim_reg_init_probe_ext(struct decon_lcd *lcd_info, u32 data_lane_cnt);
int dsim_reg_set_clocks_ext(struct dsim_clks *clks, u32 lane, u32 en);
int dsim_reg_set_lanes_ext(u32 lanes, u32 en);
int dsim_reg_set_hs_clock_ext(u32 en);
void dsim_reg_set_int_ext(u32 en);
int dsim_reg_set_ulps_ext(u32 en);

/* CAL raw functions list */
void dsim_reg_sw_reset_ext(void);
void dsim_reg_dp_dn_swap_ext(u32 en);
void dsim_reg_set_byte_clk_src_is_pll_ext(void);
void dsim_reg_pll_stable_time_ext(void);
void dsim_reg_clear_int_ext(u32 int_src);
void dsim_reg_clear_int_all_ext(void);
void dsim_reg_set_pll_ext(u32 en);
u32 dsim_reg_is_pll_stable_ext(void);
int dsim_reg_enable_pll_ext(u32 en);
void dsim_reg_set_byte_clock_ext(u32 en);
void dsim_reg_set_esc_clk_prescaler_ext(u32 en, u32 p);
void dsim_reg_set_esc_clk_on_lane_ext(u32 lane, u32 en);
u32 dsim_reg_wait_lane_stop_state_ext(void);
void dsim_reg_set_stop_state_cnt_ext(void);
void dsim_reg_set_bta_timeout_ext(void);
void dsim_reg_set_lpdr_timeout_ext(void);
void dsim_reg_set_packet_ctrl_ext(void);
void dsim_reg_set_porch_ext(struct decon_lcd *lcd);
void dsim_reg_set_config_ext(u32 mode, u32 data_lane_cnt);
void dsim_reg_set_cmd_transfer_mode_ext(u32 lp);
void dsim_reg_set_data_transfer_mode_ext(u32 lp);
void dsim_reg_enable_hs_clock_ext(u32 en);
int dsim_reg_wait_hs_clk_ready_ext(void);
void dsim_reg_set_fifo_ctrl_ext(u32 cfg);
void dsim_reg_force_dphy_stop_state_ext(u32 en);
void dsim_reg_wr_tx_header_ext(u32 id, u32 data0, u32 data1);
void dsim_reg_wr_tx_payload_ext(u32 payload);
void dsim_reg_enter_ulps_ext(u32 enter);
void dsim_reg_exit_ulps_ext(u32 exit);
int dsim_reg_wait_enter_ulps_state_ext(void);
int dsim_reg_wait_exit_ulps_state_ext(void);
void dsim_reg_set_standby_ext(u32 en);
void dsim_reg_set_clear_rx_fifo_ext(void);
void dsim_reg_set_pkt_go_enable_ext(bool en);
void dsim_reg_set_pkt_go_ready_ext(void);
void dsim_reg_set_pkt_go_cnt_ext(unsigned int count);
void dsim_reg_set_win_update_conf_ext(int w, int h, bool mic_on);

#endif /* _DSIM_REG_H */
