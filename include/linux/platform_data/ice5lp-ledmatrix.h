/*
 * Copyright (C) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _ICE5LP_LEDMATRIX_PLATFORM_DATA_H_
#define _ICE5LP_LEDMATRIX_PLATFORM_DATA_H_

struct ice5lp_ledmatrix_platform_data {
	int creset_0;
	int creset_1;	
	int fpga_reset;
	int spi_si_sda;
	int spi_clk_scl;
	int ledmatrix_en;
#if defined(CONFIG_OF)
	u32 creset_b_flag;
	u32 cdone_flag;
	u32 irda_irq_flag;
	u32 spi_si_sda_flag;
	u32 spi_clk_scl_flag;
	u32 spi_en_rstn_flag;
	u32 ir_en_flag;
#endif /* CONFIG_OF */
};

#endif /* _ICE5LP_LEDMATRIX_PLATFORM_DATA_H_ */
