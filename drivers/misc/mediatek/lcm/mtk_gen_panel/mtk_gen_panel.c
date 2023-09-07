/*
 * Copyright (C) 2017 Samsung Electronics Co, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/lockdep.h>

#include "gen_panel/gen-panel.h"
#include "gen_panel/gen-panel-bl.h"
#include "gen_panel/gen-panel-mdnie.h"
#include <video/mipi_display.h>
#include "lcm_drv.h"
#include "mtk_gen_common.h"

/* ------------------------------------------------------------------------- */
/* Local Functions */
/* ------------------------------------------------------------------------- */
struct lcd *g_lcd;
static LCM_UTIL_FUNCS lcm_util = {0};

#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define dsi_set_cmdq_V11(cmdq, pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq_V11(cmdq, pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static int lcm_read_by_cmdq(void *handle, unsigned data_id, unsigned offset, unsigned cmd,
					unsigned char *buffer, unsigned char size)
{
	return lcm_util.dsi_dcs_read_cmdq_lcm_reg_v2_1(handle, data_id, offset, cmd, buffer, size);
}

static void mtk_dsi_set_withrawcmdq(void *cmdq, unsigned int *pdata,
			unsigned int queue_size, unsigned char force_update)
{
	dsi_set_cmdq_V11(cmdq, pdata, queue_size, force_update);
}

/* --------------------------------------------------------------------------*/
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------*/
static void mtk_panel_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void mtk_panel_get_params(LCM_PARAMS *params)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s +\n", __func__);
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type			= lcd->mode.panel_type;
	params->width			= lcd->mode.real_xres;
	params->height			= lcd->mode.real_yres;
	params->physical_width		= lcd->mode.width;
	params->physical_height		= lcd->mode.height;

	/* Enable tearing-free */
	params->dbi.te_mode		= lcd->mode.te_dbi_mode;
	params->dbi.te_edge_polarity	= lcd->mode.te_pol;

	params->lcm_if			= lcd->mode.lcm_if;
	params->lcm_cmd_if		= lcd->mode.lcm_if;
	params->dsi.switch_mode_enable	= lcd->mode.switch_mode_en;
	params->dsi.cont_clock		= lcd->mode.cont_clk;
	params->dsi.clk_lp_per_line_enable = lcd->mode.clk_lp_enable;
	params->dsi.LANE_NUM		= lcd->mode.number_of_lane;

	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.mode			= lcd->mode.dsi_mode;
	params->dsi.data_format.color_order	= lcd->mode.color_order;
	params->dsi.data_format.trans_seq	= lcd->mode.trans_seq;
	params->dsi.data_format.padding		= lcd->mode.padding_on;
	params->dsi.data_format.format		= lcd->mode.rgb_format;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size		= lcd->mode.packet_size;
	params->dsi.PS			= lcd->mode.ps_to_bpp;

	/* Bit rate calculation */
	params->dsi.PLL_CLOCK		= lcd->mode.dsi_freq;
	params->dsi.ssc_disable		= lcd->mode.ssc_disable;

	/*ESD configuration */
	params->dsi.customization_esd_check_enable = lcd->mode.cust_esd_enable;
	params->dsi.esd_check_enable	= lcd->mode.esd_check_enable;

	/*  Transition to high speed mode before lcd init parameter setting.*/
	/*params->dsi.WaitingTimeOnHS		= lcd->mode.WaitingTimeOnHS;*/

}

static void mtk_panel_power_set(struct lcd *lcd, int status)
{

	if (status) {
		mtk_panel_power_on(lcd);
		pr_info("%s is called =[%d]\n", __func__, status);
	}
	else {
		mtk_panel_power_off(lcd);
		pr_info("%s is called =[%d]\n", __func__, status);
	}
}

static void mtk_panel_init(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}

	mutex_lock(&mtk_panel_lock);
	gen_panel_set_status(lcd, GEN_PANEL_ON_REDUCED);
	mutex_unlock(&mtk_panel_lock);
}

static void mtk_panel_reduced_init(int is_init)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}
	if (is_init) {
		mutex_lock(&mtk_panel_lock);
		gen_panel_set_status(lcd, GEN_PANEL_ON_REDUCED);
		mutex_unlock(&mtk_panel_lock);
	}
}

static void mtk_panel_resume_power(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);

	mtk_panel_power_set(lcd, true);
}

static void mtk_panel_resume(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}

	lockdep_off();
	gen_panel_set_status(lcd, GEN_PANEL_ON);
	lockdep_on();
}

static void mtk_panel_suspend_power(void)
{
	struct lcd *lcd = g_lcd;

	mtk_panel_power_set(lcd, false);
}

static void mtk_panel_suspend(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}

	lockdep_off();
	gen_panel_start(lcd, GEN_PANEL_OFF);
	gen_panel_set_status(lcd, GEN_PANEL_OFF);
	lockdep_on();
}

static void lcm_panel_cmd_q(int enable)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s : %d\n", __func__, enable);

	mutex_lock(&mtk_panel_lock);

	if (enable)
		lcd->disp_cmdq_en = true;
	else
		lcd->disp_cmdq_en = false;

	mutex_unlock(&mtk_panel_lock);
}

/*
static void mtk_panel_set_brightness(unsigned int intensity)
{
	struct lcd *lcd = g_lcd;

	if (unlikely(!lcd || !lcd->bd)) {
		pr_info("%s, backlight device is null\n", __func__);
		return;
	}

	mutex_lock(&mtk_panel_lock);
	lcd->bd->props.brightness = intensity;
	backlight_update_status(lcd->bd);
	mutex_unlock(&mtk_panel_lock);
}

static void mtk_panel_set_brightness_cmdq(void *handle, unsigned int intensity)
{
	struct lcd *lcd = g_lcd;

	if (unlikely((!lcd) || (!lcd->bd))) {
		pr_info("%s, backlight device is null\n", __func__);
		return;
	}

	mutex_lock(&mtk_panel_lock);
	lcd->bd->props.brightness = intensity;
	if (mtk_panel_state == MTK_PANEL_ON) {
		gen_panel_start(lcd, GEN_PANEL_ON);
		mtk_panel_state = MTK_PANEL_START;
	} else {
		backlight_update_status(lcd->bd);
	}
	mutex_unlock(&mtk_panel_lock);

	pr_info("%s, brightness %d\n", __func__, intensity);
}
*/
static void mtk_panel_update(unsigned int x, unsigned int y,
				unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

#ifdef CONFIG_MTK_CONSUMER_PARTIAL_UPDATE_SUPPORT
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	struct lcd *lcd = g_lcd;

	unsigned int y1 = *y, x1 = *x;
	unsigned int y2, x2, w, h, n;
	unsigned int ori_xres, ori_yres;

	if (lcd->mode.patial_update) {

		ori_xres = x1 + *width;
		ori_yres = y1 + *height;

		/*
		* This should be changed by ddi restriction for partial update.
		* In some cases, roi maybe empty. In this case,
		* we need to use minimum roi condition.
		*/

		/*
		*pr_info("original lcm_validate_roi (%d,%d,%d,%d)\n",
					x1, y1, *width, *height);
		*/

		/*related with column adress*/
		if (*width < (lcd->mode.patial_col_min_size))
			*width = lcd->mode.patial_col_min_size;

		if ((x1 % (lcd->mode.patial_col_multi_num)) != 0) {
			n = x1 / (lcd->mode.patial_col_multi_num);
			x1 = n * (lcd->mode.patial_col_multi_num);
		}

		x2 = x1 + *width;

		if (x2 >= (lcd->mode.real_xres))
			x2 = lcd->mode.real_xres;

		if ((x2 % (lcd->mode.patial_col_multi_num)) != 0) {
			n = x2 / (lcd->mode.patial_col_multi_num);
			x2 = (n + 1) * (lcd->mode.patial_col_multi_num);
			if (x2 >= (lcd->mode.real_xres))
				x2 = lcd->mode.real_xres;
		}
		if (x2 - x1 < (lcd->mode.patial_col_min_size))
			w = lcd->mode.patial_col_min_size;
		else
			w = x2 - x1;

		if (w + x1 > (lcd->mode.real_xres))
			x1 = (lcd->mode.real_xres) - w;

		/*related with page adress*/
		if (*height < (lcd->mode.patial_page_min_size))
			*height = lcd->mode.patial_page_min_size;

		if ((y1 % (lcd->mode.patial_page_multi_num)) != 0) {
			n = y1 / (lcd->mode.patial_page_multi_num);
			y1 = n * (lcd->mode.patial_page_multi_num);
		}

		y2 = y1 + *height;

		if (y2 >= (lcd->mode.real_yres))
			y2 = lcd->mode.real_yres;

		if ((y2 % (lcd->mode.patial_page_multi_num)) != 0) {
			n = y2 / (lcd->mode.patial_page_multi_num);
			y2 = (n + 1) * (lcd->mode.patial_page_multi_num);
			if (y2 >= (lcd->mode.real_yres))
				y2 = lcd->mode.real_yres;
		}

		if (y2 - y1 < (lcd->mode.patial_page_min_size))
			h = lcd->mode.patial_page_min_size;
		else
			h = y2 - y1;

		if (h + y1 > (lcd->mode.real_yres))
			y1 = (lcd->mode.real_yres) - h;

		if (ori_xres > x1 + w)
			w = w + lcd->mode.patial_col_multi_num;
		if (ori_yres > y1 + h)
			h = h + lcd->mode.patial_page_multi_num;

		/*pr_info("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",
		*x, *y, *width, *height, x1, y1, w, h);
		*/

		/*final partial update condition check.
		* if x or y axis need to update one of two to full area,
		* it should be full size update.
		* For example font, icon image extend on display setting menu
		*/
		if (((x1 + w) == (lcd->mode.real_xres)) ||
			((y1 + h) == (lcd->mode.real_yres))) {

			x1 = y1 = 0;
			w = lcd->mode.real_xres;
			h = lcd->mode.real_yres;
		}

		*x = x1;
		*y = y1;
		*width = w;
		*height = h;

	} else {
		*x = 0; *width = lcd->mode.real_xres;
		*y = 0; *height = lcd->mode.real_yres;
	}
}
#endif
LCM_DRIVER mtk_gen_panel_drv = {
	.name = "mtk_gen_panel",
	.set_util_funcs = mtk_panel_set_util_funcs,
	.get_params = mtk_panel_get_params,
	.init = mtk_panel_init,
	.reduced_init = mtk_panel_reduced_init,
	.suspend_power = mtk_panel_suspend_power,
	.suspend = mtk_panel_suspend,
	.resume_power = mtk_panel_resume_power,
	.resume = mtk_panel_resume,
	/*
	.set_backlight = mtk_panel_set_brightness,
	.set_backlight_cmdq = mtk_panel_set_brightness_cmdq,
	*/
	.update = mtk_panel_update,
	.dsi_set_withrawcmdq = mtk_dsi_set_withrawcmdq,
	.read_by_cmdq = lcm_read_by_cmdq,
	.cmd_q = lcm_panel_cmd_q,
#ifdef CONFIG_MTK_CONSUMER_PARTIAL_UPDATE_SUPPORT
	.validate_roi = lcm_validate_roi,
#endif
	/* TODO : parse mtk dependent information */
	/*.parse_dts = mtk_panel_parse_dts,*/
};

/*#define DEBUG*/
#define MAX_WRITE_BYTES_LIMIT	(0x7F)
unsigned char data[512];
static inline int mtk_panel_dsi_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret, retry;

	if (unlikely(!lcm_util.dsi_set_cmdq_V11)) {
		pr_err("%s, lcm_util is not prepared\n", __func__);
		return 0;
	}
	for (i = 0; i < count; i++) {
		retry = 3;
		do {
			data[0] = (desc[i].data_type == 0x39) ? 0x02 : 0x00;
			data[1] = desc[i].data_type;
			if (desc[i].data_type == 0x05) {
				data[2] = desc[i].data[0];
				data[3] = 0;
				if (lcd->disp_cmdq_en)
					primary_display_dsi_set_withrawcmdq((unsigned int *)data, 1, 1);
				else
					dsi_set_cmdq((unsigned int *)data, 1, 1);
#ifdef DEBUG

				print_data(data, 3);
#endif
			} else if (desc[i].data_type == 0x15) {
				data[2] = desc[i].data[0];
				data[3] = desc[i].data[1];
				if (lcd->disp_cmdq_en)
					primary_display_dsi_set_withrawcmdq((unsigned int *)data, 1, 1);
				else
					dsi_set_cmdq((unsigned int *)data, 1, 1);

#ifdef DEBUG

				print_data(data, 4);
#endif
			} else if (desc[i].data_type ==
				MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE) {
				data[2] = desc[i].data[0];
				data[3] = 0;
				if (lcd->disp_cmdq_en)
					primary_display_dsi_set_withrawcmdq((unsigned int *)data, 1, 1);
				else
					dsi_set_cmdq((unsigned int *)data, 1, 1);
#ifdef DEBUG

				print_data(data, 4);
#endif
			} else if (desc[i].data_type == 0x39) {
				int len, offset = 0;
				int total_len = desc[i].length - 1;

				data[4] = desc[i].data[0];

				while (offset < total_len) {
					if (offset) {
						data[0] = 0x0;
						data[1] = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
						data[2] = 0xB0;
						data[3] = offset;
						if (lcd->disp_cmdq_en)
							primary_display_dsi_set_withrawcmdq((unsigned int *)data, 1, 1);
						else
							dsi_set_cmdq((unsigned int *)data, 1, 1);
					}
					data[0] = 0x02;
					data[1] = MIPI_DSI_DCS_LONG_WRITE;
					len = (((total_len - offset) < MAX_WRITE_BYTES_LIMIT) ?
							(total_len - offset) : MAX_WRITE_BYTES_LIMIT);
					*(unsigned short *)(&data[2]) = len + 1;
					memcpy(data + 5, desc[i].data + offset + 1, len);
					if (lcd->disp_cmdq_en)
						primary_display_dsi_set_withrawcmdq((unsigned int *)data, (len + 8) / 4, 1);
					else
						dsi_set_cmdq((unsigned int *)data, (len + 8) / 4, 1);

					offset += len;
#ifdef DEBUG

					print_data(data, len + 5);
#endif
				}
			}
			ret = 0;
		} while (retry-- && ret);

		if (unlikely(ret)) {
			pr_err("[LCD]%s, dsi write failed  ret=%d\n",
					__func__, ret);
			pr_err("[LCD]%s, idx:%d, data:0x%02X, len:%d\n",
					__func__, i, desc[i].data[0],
					desc[i].length);
			return ret;
		}
		if (desc[i].delay)
			msleep(desc[i].delay);
	}

	return count;
}

static inline int mtk_panel_dsi_rx_cmd_array(const struct lcd *lcd,
				u8 *buf, const void *cmds, const int count)
{
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret = 0;
	unsigned char addr = 0, len = 0;

	if (unlikely(!lcm_util.dsi_dcs_read_lcm_reg_v2)) {
		pr_err("%s, lcm_util is not prepared\n", __func__);
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (desc[i].data_type != MIPI_DSI_DCS_READ) {
			if (desc[i].data_type ==
				MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE)
				len = desc[i].data[0];
			mtk_panel_dsi_tx_cmd_array(lcd, &desc[i], 1);
		} else {
			addr = desc[i].data[0];
			pr_info("%s, lcm_read command\n", __func__);
			if (len == 0)
				pr_warn("%s, warning - read length is 0\n",
								__func__);
			else {
				if (lcd->disp_cmdq_en)
					ret = primary_display_dsi_read_cmdq_cmd(desc[i].data_type, addr, 0, buf, len);
				else
					ret = read_reg_v2(addr, buf, len);
			}
			len = 0;
			if (desc[i].delay)
				msleep(desc[i].delay);
		}
	}

	if (len)
		pr_err("%s, invalid rx command set\n", __func__);

	return ret;
}

#if CONFIG_OF
int mtk_panel_parse_dt(const struct device_node *np)
{
	if (!np)
		return -EINVAL;

	return 0;
}
#endif

static const struct gen_panel_ops gp_dsi_ops = {
	.tx_cmds = mtk_panel_dsi_tx_cmd_array,
	.rx_cmds = mtk_panel_dsi_rx_cmd_array,
#if CONFIG_OF
	.parse_dt = mtk_panel_parse_dt,
#endif
};

static void gpmode_to_mtkmode(struct lcd *lcd)
{
	g_lcd = lcd;
}

static const struct of_device_id mtk_panel_dt_match[];

static int mtk_panel_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct lcd *lcd;
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	const struct gen_panel_ops *ops = &gp_dsi_ops;

	pr_info("[LCD]called %s\n", __func__);

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (unlikely(!lcd)) {
		pr_err("[LCD] Unable to allocate for lcd resource\n");
		ret = -ENOMEM;
		goto err_gen_panel_probe;
	}

	match = of_match_node(mtk_panel_dt_match, np);
	if (match)
		ops = (struct gen_panel_ops *)match->data;
	else {
		pr_err("[LCD] %s, no matched node\n", __func__);
		ret = -ENODEV;
		goto err_gen_panel_probe;
	}
	lcd->ops = ops;

	ret = gen_panel_probe(pdev->dev.of_node, lcd);
	if (unlikely(ret)) {
		pr_err("[LCD] %s, gen_panel_probe() is failed\n", __func__);
		goto err_gen_panel_probe;
	}
	gpmode_to_mtkmode(lcd);
	pr_info("[LCD] %s success\n", __func__);
	return ret;

err_gen_panel_probe:
	kfree(lcd);
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_panel_dt_match[] = {
	{
		.compatible = "mediatek,mtk-dsi-panel",
		.data = &gp_dsi_ops,
	},
	{},
};
#endif
static struct platform_driver mtk_panel_driver = {
	.driver = {
		.name = "mtk-dsi-panel",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mtk_panel_dt_match),
#endif
		.suppress_bind_attrs = true,
	},
	.probe = mtk_panel_probe,
	/*.remove = mtk_panel_remove,*/
	/*.shutdown = mtk_panel_shutdown,*/
};

static int mtk_panel_module_init(void)
{
	return platform_driver_register(&mtk_panel_driver);
}

static void mtk_panel_module_exit(void)
{
	platform_driver_unregister(&mtk_panel_driver);
}

module_init(mtk_panel_module_init);
module_exit(mtk_panel_module_exit);

MODULE_DESCRIPTION("MTK DSI PANEL DRIVER");
MODULE_LICENSE("GPL");
