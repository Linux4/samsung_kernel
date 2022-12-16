/*
 * driver/fpga_ice40xx TSP switching CPLD driver
 *
 * Copyright (C) 2012 Samsung Electronics
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
 *
 */

#include "cyttsp5_regs.h"

static void ice40_clock_en(int onoff)
{
	static struct clk *fpga_main_src_clk;
	static struct clk *fpga_main_clk;

	pr_err("%s:%d - on : %d\n", __func__, __LINE__, onoff);

	if (!fpga_main_src_clk)
		fpga_main_src_clk = clk_get(NULL, "gp2_src_clk");
	if (IS_ERR(fpga_main_src_clk))
		pr_err("%s: unable to get fpga_main_src_clk\n", __func__);
	if (!fpga_main_clk)
		fpga_main_clk = clk_get(NULL, "gp2_clk");
	if (IS_ERR(fpga_main_clk))
		pr_err("%s: unable to get fpga_main_clk\n", __func__);

	if (onoff) {
		clk_set_rate(fpga_main_src_clk, 24000000);
		clk_prepare_enable(fpga_main_clk);
	} else {
		clk_disable_unprepare(fpga_main_clk);
		clk_put(fpga_main_src_clk);
		clk_put(fpga_main_clk);
		fpga_main_src_clk = NULL;
		fpga_main_clk = NULL;
	}
}

void fpga_enable(struct cyttsp5_core_data *cd, int enable_clk)
{
	if (enable_clk) {
		if (!cd->Is_clk_enabled && (cd->enable_counte == 0)) {
			ice40_clock_en(1);
			usleep_range(1000, 2000);
			cd->Is_clk_enabled = 1;
		}
		cd->enable_counte++;
	} else {
		if (cd->Is_clk_enabled && (cd->enable_counte == 1)) {
			cd->Is_clk_enabled = 0;
			usleep_range(2000, 2500);
			ice40_clock_en(0);
		}
		if (cd->enable_counte < 0) {
			printk(KERN_ERR "%s enable_counte ERR!= %d\n",
					__func__, cd->enable_counte);
			cd->enable_counte = 0;
		} else {
			cd->enable_counte--;
		}
	}
}


static int ice40_fpga_send_firmware_data(struct cyttsp5_core_data *cd,
		const u8 *data, int len)
{
	unsigned int i, j;
	unsigned char spibit;

	pr_info("%s %d\n",__func__,len);
	gpio_set_value_cansleep(cd->cpdata->tsp_sda, 0);

	i = 0;
	while (i < len) {
		j = 0;
		spibit = data[i];
		while (j < 8) {
			gpio_set_value_cansleep(cd->cpdata->tsp_scl, 0);

			if (spibit & 0x80)
				gpio_set_value_cansleep(cd->cpdata->tsp_sel, 1);
			else
				gpio_set_value_cansleep(cd->cpdata->tsp_sel, 0);

			j = j+1;
			gpio_set_value_cansleep(cd->cpdata->tsp_scl, 1);
			spibit = spibit<<1;
		}
		i = i+1;
	}

	gpio_set_value_cansleep(cd->cpdata->tsp_sel, 1);
	i = 0;
	while (i < 200) {
		gpio_set_value_cansleep(cd->cpdata->tsp_scl, 0);
		i = i+1;
		gpio_set_value_cansleep(cd->cpdata->tsp_scl, 1);
	}
	pr_info("%s end\n",__func__);
	return 0;
}

static int ice40_fpga_fimrware_update_start(struct cyttsp5_core_data *cd)
{
	dev_info(cd->dev, "%s\n", __func__);

	fpga_enable(cd, 1);

	gpio_tlmm_config(GPIO_CFG(cd->cpdata->tsp_sda, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(cd->cpdata->tsp_scl, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(cd->cpdata->tsp_sel, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(FPGA_CDONE_GPIO, 0, GPIO_CFG_INPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	gpio_set_value(cd->cpdata->cresetb, 1);
	usleep_range(1500, 1700);

	gpio_set_value(cd->cpdata->cresetb, 0);
	usleep_range(30, 50);

	gpio_set_value(cd->cpdata->cresetb, 1);
	usleep_range(1000, 1300);

	ice40_fpga_send_firmware_data(cd, cd->fpga_fw->data, cd->fpga_fw->size);
	usleep_range(50, 70);

	fpga_enable(cd, 0);
	return 0;
}

void ice40_fpga_firmware_update(struct cyttsp5_core_data *cd)
{
	dev_err(cd->dev, "%s[%d]\n", __func__,__LINE__);

	if(request_firmware(&cd->fpga_fw, FPGA_FW_PATH, cd->dev))
		dev_err(cd->dev, "%s: Can't open firmware file.",__func__);
	else
		ice40_fpga_fimrware_update_start(cd);

	release_firmware(cd->fpga_fw);

	msleep(10);
	dev_err(cd->dev, "%s end\n",__func__);
}

