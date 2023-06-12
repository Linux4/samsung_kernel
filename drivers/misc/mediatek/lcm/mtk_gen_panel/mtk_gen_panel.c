/*
 * Copyright (C) 2016 Samsung Electronics Co, Ltd.
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

#define LOG_TAG "LCM"

#ifndef BUILD_LK
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
//#include <linux/delay.h>

#include <linux/platform_data/gen-panel.h>
#include <linux/platform_data/gen-panel-bl.h>
#include <linux/platform_data/gen-panel-mdnie.h>
#include <video/mipi_display.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define set_gpio_lcd_rst(cmd) \
	lcm_util.set_gpio_lcd_rst_enp(cmd)
#define set_gpio_lcd_enp(cmd) \
	lcm_util.set_gpio_lcd_bias_enp(cmd)

/* static unsigned char lcd_id_pins_value = 0xFF; */
#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (540)
#define FRAME_HEIGHT (960)

#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD
#define REGFLAG_RESET_LOW 0xFFFE
#define REGFLAG_RESET_HIGH 0xFFFF

#define LCM_ID_S6E3FA3_CMD_W1 0x0102
#define LCM_ID_S6E3FA3_CMD_B1 0x0103
#define LCM_ID_S6E3FA3_CMD_W2 0x0304

//static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum {
	MTK_PANEL_OFF,
	MTK_PANEL_REDUCED_ON,
	MTK_PANEL_ON,
	MTK_PANEL_START,
};

static int mtk_panel_state;
static DEFINE_MUTEX(mtk_panel_lock);

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util;
struct lcd *g_lcd = NULL;

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
void mtk_panel_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void mtk_panel_get_params(LCM_PARAMS *params)
{
	pr_info("%s +\n", __func__);
	memset(params, 0, sizeof(LCM_PARAMS));
	params->type			= LCM_TYPE_DSI;
	params->width			= FRAME_WIDTH;
	params->height			= FRAME_HEIGHT;
	params->physical_width		= 64;
	params->physical_height		= 110;
	/* enable tearing-free */
	params->dbi.te_mode		= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity	= LCM_POLARITY_RISING;

	params->dsi.mode		= SYNC_PULSE_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM		= LCM_TWO_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding		= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;
	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.DSI_WMEM_CONTI	= 0x3C;
	params->dsi.DSI_RMEM_CONTI	= 0x3E;
	params->dsi.packet_size		= 256;
	/* Video mode setting */
	params->dsi.intermediat_buffer_num 	= 2;
	params->dsi.PS				= LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active	= 2;
	params->dsi.vertical_backporch		= 6;
	params->dsi.vertical_frontporch		= 8;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.horizontal_sync_active	= 28;
	params->dsi.horizontal_backporch	= 35;
	params->dsi.horizontal_frontporch	= 35;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;
	/* Bit rate calculation */
	params->dsi.PLL_CLOCK = 230;
	params->dsi.ssc_disable = 1;

	params->dsi.clk_lp_per_line_enable	= 0;
	params->dsi.cont_clock			= 1;

	/*ESD configuration */
	params->dsi.customization_esd_check_enable	= 1;
	params->dsi.esd_check_enable			= 0;
	#if 0
	params->dsi.lcm_esd_check_table[0].cmd		= 0x0A;
	params->dsi.lcm_esd_check_table[0].count	= 1;
	params->dsi.lcm_esd_check_table[0].para_list[0]	= 0x9C;
	#endif
}

void mtk_panel_init(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}

	mutex_lock(&mtk_panel_lock);
	gen_panel_set_status(lcd, GEN_PANEL_ON_REDUCED);
	mtk_panel_state = MTK_PANEL_REDUCED_ON;
	mutex_unlock(&mtk_panel_lock);
}

void mtk_panel_resume_power(void)
{
	pr_info("%s is called\n", __func__);

	set_gpio_lcd_enp(1);
	MDELAY(60/*system on 40msec + 20msec after LP11*/);
	/*
	set_gpio_lcd_rst(1);
	MDELAY(1);
	set_gpio_lcd_rst(0);
	MDELAY(10);
	*/
	set_gpio_lcd_rst(1);
	MDELAY(30);
}

void mtk_panel_resume(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}
	mtk_panel_resume_power();

	mutex_lock(&mtk_panel_lock);
	gen_panel_set_status(lcd, GEN_PANEL_ON);
	mtk_panel_state = MTK_PANEL_ON;
	mutex_unlock(&mtk_panel_lock);
}

void mtk_panel_suspend(void)
{
	struct lcd *lcd = g_lcd;

	pr_info("%s is called\n", __func__);
	if (unlikely(!lcd)) {
		pr_err("%s, lcd is null\n", __func__);
		return;
	}

	mutex_lock(&mtk_panel_lock);
	gen_panel_start(lcd, GEN_PANEL_OFF);
	gen_panel_set_status(lcd, GEN_PANEL_OFF);

	set_gpio_lcd_rst(0);
	set_gpio_lcd_enp(0);
	mtk_panel_state = MTK_PANEL_OFF;
	mutex_unlock(&mtk_panel_lock);
}

void mtk_panel_set_brightness(unsigned int intensity)
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

static void mtk_panel_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
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

void mtk_panel_set_brightness_cmdq(void *handle, unsigned int intensity)
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
	pr_info("%s, brightness %d\n", __func__, intensity);
	mutex_unlock(&mtk_panel_lock);
}

LCM_DRIVER mtk_gen_panel_drv = {
	.name = "mtk_gen_panel",
	.set_util_funcs = mtk_panel_set_util_funcs,
	.get_params = mtk_panel_get_params,
	.init = mtk_panel_init,
	.suspend = mtk_panel_suspend,
	.resume = mtk_panel_resume,
	//.resume_power = mtk_panel_resume_power,
	.set_backlight = mtk_panel_set_brightness,
	.update = mtk_panel_update,
	.set_backlight_cmdq = mtk_panel_set_brightness_cmdq,
	/* TODO : parse mtk dependent information */
	//.parse_dts = mtk_panel_parse_dts,
};

#if 1
extern void print_data(char *data, int len);
#define MAX_WRITE_BYTES_LIMIT	(0x5F)
static inline int mtk_panel_dsi_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret, retry;
	unsigned char data[512];

	if (unlikely(!lcm_util.dsi_set_cmdq)) {
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
				dsi_set_cmdq((unsigned int *)data, 1, 1); 
				print_data(data, 3);
			} else if (desc[i].data_type == 0x15) {
				data[2] = desc[i].data[0];
				data[3] = desc[i].data[1];
				dsi_set_cmdq((unsigned int *)data, 1, 1); 
				print_data(data, 4);
			} else if (desc[i].data_type ==
					MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE) {
				data[2] = desc[i].data[0];
				data[3] = 0;
				dsi_set_cmdq((unsigned int *)data, 1, 1); 
				print_data(data, 4);
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
						dsi_set_cmdq((unsigned int*)data, 1, 1);
					}
					data[0] = 0x02;
					data[1] = MIPI_DSI_DCS_LONG_WRITE;
					len = (((total_len - offset) < MAX_WRITE_BYTES_LIMIT) ?
							(total_len - offset) : MAX_WRITE_BYTES_LIMIT);
					*(unsigned short *)(&data[2]) = len + 1;
					memcpy(data + 5, desc[i].data + offset + 1, len);
					dsi_set_cmdq((unsigned int *)data, (len + 8) / 4, 1);
					offset += len;
					print_data(data, len + 5);
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
			MDELAY(desc[i].delay);
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
			if (len == 0)
				pr_warn("%s, warning - read length is 0\n", __func__);
			else 
				ret += read_reg_v2(addr, buf, len);
			len = 0;
			if (desc[i].delay)
				MDELAY(desc[i].delay);
		}
	}

	if (len) {
		pr_err("%s, invalid rx command set\n", __func__);
	}

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



	return;
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
	if (unlikely(!lcd)){
                pr_err("[LCD] Unable to allocate for lcd resource\n");
		ret = -ENOMEM;
		goto err_gen_panel_probe;

	}

	match = of_match_node(mtk_panel_dt_match, np);
	if (match) {
		ops = (struct gen_panel_ops *)match->data;
	} else {
		pr_err("[LCD] %s, no matched node\n", __func__);
		ret = -ENODEV;
		goto err_gen_panel_probe;
	}
	lcd->ops = ops;

	ret = gen_panel_probe(pdev->dev.of_node, lcd);
	if (unlikely(ret)){
		pr_err("[LCD] %s, gen_panel_probe() is failed\n", __func__);
		goto err_gen_panel_probe;
	}
	gpmode_to_mtkmode(lcd);

	g_lcd = lcd;
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
	},  
	.probe = mtk_panel_probe,
	//.remove = mtk_panel_remove,
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
#endif
