/*
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#define LOG_TAG "LCM"
#include "lcm_drv.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include "lcm_drv.h"
#include "ddp_hal.h"
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "mtk_gpio.h"
#include <mach/gpio_const.h>

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (1920)
#define LCD_VDD_1P8 (GPIO22 | 0x80000000)
#define LCD_VSP_5P5 (GPIO20 | 0x80000000)
#define LCD_VSN_5P5 (GPIO19 | 0x80000000)
#define MLCD_RSET (GPIO158 | 0x80000000)
#define TSP_RSET (GPIO18 | 0x80000000)
#define TSP_ATTN (GPIO1 | 0x80000000)
#define TSP_SDA (GPIO91 | 0x80000000)
#define TSP_SCL (GPIO94 | 0x80000000)
#define LCD_BL_EN (GPIO21 | 0x80000000)
#define ISL98611_SLAVE_ADDR_WRITE  0x29
#define I2C_WRITE (0)
#define I2C_READ (1)
#define I2C_SPEED (100)
#define I2C_WRITE_LEN (2)
#define REGFLAG_DELAY  0xFC
#define REGFLAG_UDELAY  0xFB
#define REGFLAG_END_OF_TABLE 0xFD
#define REGFLAG_RESET_LOW 0xFE
#define REGFLAG_RESET_HIGH 0xFF
#define LCM_LOGI(string, args...)  pr_debug("[KERNEL/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  pr_debug("[KERNEL/"LOG_TAG"]"string, ##args)
/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {0};


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
/*****************************************************************************
 * Define
 *****************************************************************************/
#define BLICILI98611
#ifdef BLICILI98611
#define TPS_I2C_BUSNUM  1    /* for I2C channel 1 */
#define I2C_ID_NAME "isl98611"
#define ISL98611_SLAVE_ADDR_WRITE  0x29
/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_board_info isl98611_board_info __initdata = { I2C_BOARD_INFO(I2C_ID_NAME, ISL98611_SLAVE_ADDR_WRITE) };
static struct i2c_client *isl98611_i2c_client;
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int isl98611_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int isl98611_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/
    struct isl98611_dev {
struct i2c_client *client;
};
static const struct i2c_device_id isl98611_id[] = {
{I2C_ID_NAME, 0},
{}
};
/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)) */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
/* #endif */
static struct i2c_driver isl98611_iic_driver = {
.id_table = isl98611_id,
 .probe = isl98611_probe,
.remove = isl98611_remove,
	    /* .detect  = mt6605_detect, */
	.driver = {
.owner = THIS_MODULE,
.name = "isl98611",
},
};
/*****************************************************************************
 * Function
 *****************************************************************************/
static int isl98611_write_bytes(unsigned char addr, unsigned char value)
{
int ret = 0;
struct i2c_client *client = isl98611_i2c_client;
char write_data[2] = { 0 };
 write_data[0] = addr;
 write_data[1] = value;
ret = i2c_master_send(client, write_data, 2);
if (ret < 0)
LCM_LOGI("isl98611 write data fail !!\n");
return ret;
}

void ISL98611_reg_init(void)
{
int ret = 0;
ret |= isl98611_write_bytes(0x04, 0x14);	/*VSP 5P5 */
ret |= isl98611_write_bytes(0x05, 0x14);	/*VSN 5P5 */
ret |= isl98611_write_bytes(0x13, 0xC7);	/*PWMI * SWIRE(GND) , Fully PWM dimming */
ret |= isl98611_write_bytes(0x14, 0x7D);	/*8bit, 20915(20.915Khz) */
ret |= isl98611_write_bytes(0x12, 0xBF /*0x3F-->20mA */);	/*LED CURRENT 25mA */
if (ret < 0)
LCM_LOGI("td4303-isl98611-cmd--i2c write error\n");
	else
LCM_LOGI("td4303-isl98611-cmd--i2c write success\n");
}
static int isl98611_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
LCM_LOGI("isl98611_iic_probe\n");
LCM_LOGI("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
 isl98611_i2c_client = client;
 return 0;
}
static int isl98611_remove(struct i2c_client *client)
{
 LCM_LOGI("isl98611_remove\n");
 isl98611_i2c_client = NULL;
 i2c_unregister_device(client);
 return 0;
}
static int __init isl98611_iic_init(void)
{
 LCM_LOGI("isl98611_iic_init\n");
 i2c_register_board_info(TPS_I2C_BUSNUM, &isl98611_board_info, 1);
 LCM_LOGI("isl98611_iic_init2\n");
 i2c_add_driver(&isl98611_iic_driver);
 LCM_LOGI("isl98611_iic_init success\n");
 return 0;
}
static void __exit isl98611_iic_exit(void)
{
LCM_LOGI("isl98611_iic_exit\n");
i2c_del_driver(&isl98611_iic_driver);
}
module_init(isl98611_iic_init);
module_exit(isl98611_iic_exit);
MODULE_AUTHOR("jone.jung");
MODULE_DESCRIPTION("samsung isl98611 I2C Driver");
#endif

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output, \
  unsigned int pullen, unsigned int pull, unsigned int smt)
{
 mt_set_gpio_mode(GPIO, GPIO_MODE_00);
 mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
 mt_set_gpio_pull_enable(GPIO, pullen);
 mt_set_gpio_pull_select(GPIO, pull);
 mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
 mt_set_gpio_smt(GPIO, smt);
}

struct LCM_setting_table {
 unsigned int cmd;
 unsigned char count;
 unsigned char para_list[64];
};
static struct LCM_setting_table lcm_suspend_setting[] = {
 {0x28, 0, {} },
 {REGFLAG_DELAY, 25, {} },
 {0x10, 0, {} },
 {REGFLAG_DELAY, 85, {} },
};
static struct LCM_setting_table init_setting[] = {
 {0x35, 1, {0x00} }, /* tear on*/
{0x36, 1, {0xC0} }, /* address mode */
 {0x44, 2, {0x03, 0xE8} }, /* tear scanline*/
{0x51, 1, {0x00} }, /* display brightness */
 {0x53, 1, {0x0C} }, /* ctrl display*/
 {0x55, 1, {0x00} }, /* CABC*/
 {0x11, 0, {} }, /* sleep out*/
 {REGFLAG_DELAY, 100, {} },
 {0x2C, 0, {} }, /* memory start*/
 {0x29, 0, {} }, /* display on*/
 {REGFLAG_DELAY, 40, {} },
};
static struct LCM_setting_table bl_level[] = {
 {0x51, 1, {0xFF} },
 {REGFLAG_END_OF_TABLE, 0x00, {} }
};
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
 unsigned int i;
 for (i = 0; i < count; i++) {
  unsigned cmd;
  cmd = table[i].cmd;
  switch (cmd) {
   case REGFLAG_DELAY:
    if (table[i].count <= 10)
     msleep(table[i].count);
    else
     mdelay(table[i].count);
    break;
   case REGFLAG_UDELAY:
    udelay(table[i].count);
    break;
   case REGFLAG_END_OF_TABLE:
    break;
   default:
    dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
  }
 }
}
/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
 memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}
static void lcm_get_params(LCM_PARAMS *params)
{
 memset(params, 0, sizeof(LCM_PARAMS));
 params->type = LCM_TYPE_DSI;
 params->width = FRAME_WIDTH;
 params->height = FRAME_HEIGHT;
 params->physical_width = 71;
 params->physical_height = 126;
 params->dsi.mode = CMD_MODE;
 params->lcm_if = LCM_INTERFACE_DSI0;
 params->lcm_cmd_if = LCM_INTERFACE_DSI0;
 params->dsi.switch_mode_enable = 0;
 /* DSI */
 /* Command mode setting */
 params->dsi.LANE_NUM = LCM_FOUR_LANE;
 /* The following defined the fomat for data coming from LCD engine. */
 params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
 params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
 params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
 params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
 /* Highly depends on LCD driver capability. */
 params->dsi.packet_size = 256;
 /* video mode timing */
 params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
 params->dsi.ssc_disable = 1;
 params->dsi.PLL_CLOCK = 440;
 /*params->dsi.clk_lp_per_line_enable = 0;*/
}
static void lcm_init_power(void)
{
 LCM_LOGI("lcm_init_power\n");
}
static void lcm_suspend_power(void)
{
 LCM_LOGI("lcm_init_power\n");
}
static void lcm_resume_power(void)
{
 LCM_LOGI("lcm_init_power\n");
}
static void lcm_init(void)
{
 lcm_set_gpio_output(LCD_VDD_1P8, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 mdelay(1);
 lcm_set_gpio_output(LCD_VSP_5P5, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 mdelay(5); /*ISL98611 VP/N interal boosting output delay(3.5msec) after ENP/N set and waiting delay(1.0msec) until  VSN setting*/
 lcm_set_gpio_output(LCD_VSN_5P5, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 msleep(15); /*ISL98611 VP/N interal boosting output delay(3.5msec) after ENP/N set and wating delay(10msec) until lcd reset high setting*/
 lcm_set_gpio_output(TSP_RSET, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 lcm_set_gpio_output(MLCD_RSET, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 msleep(150);
 push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
 ISL98611_reg_init(); /*BLIC initialization*/
 lcm_set_gpio_output(LCD_BL_EN, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
}
static void lcm_suspend(void)
{
 lcm_set_gpio_output(LCD_BL_EN, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
 lcm_set_gpio_output(TSP_RSET, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 lcm_set_gpio_output(MLCD_RSET, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 mdelay(5);
 lcm_set_gpio_output(LCD_VSN_5P5, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 mdelay(1);
 lcm_set_gpio_output(LCD_VSP_5P5, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
 mdelay(1);
// lcm_set_gpio_output(LCD_VDD_1P8, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
}
static void lcm_resume(void)
{
 lcm_init();
}
static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
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
static unsigned int lcm_compare_id(void)
{
 unsigned int array[4];
 unsigned char buffer[3];
 LCM_LOGI(" %s, LK\n", __func__);
#if 0
 /*lcd reset pin */
 lcd_reset(1);
 MDELAY(10);
 lcd_reset(0);
 MDELAY(5);
 lcd_reset(1);
 MDELAY(30);
 array[0] = 0x00033700; /* read id return two byte,version and id */
 dsi_set_cmdq(array, 1, 1);
 read_reg_v2(0xDA, buffer, 1);
 array[0] = 0x00033700; /* read id return two byte,version and id */
 dsi_set_cmdq(array, 1, 1);
 read_reg_v2(0xDB, buffer + 1, 1);
 array[0] = 0x00033700; /* read id return two byte,version and id */
 dsi_set_cmdq(array, 1, 1);
 read_reg_v2(0xDC, buffer + 2, 1);
 if ((buffer[0] == 0x28) && (buffer[1] == 0x61) && (buffer[2] == 0x08)) {
  printf("s6d78a0x02 lcd id is [0x286108]!\n");
  return 0x286108;
 } else
  printf("s6d78a0x02 lcd id[0x0] read fail!\n");
#endif
 return 1; //TEMP
}
static void lcm_setbacklight(void *handle, unsigned int level)
{
 LCM_LOGI("%s,TD4303 backlight: level = %d\n", __func__, level);
 bl_level[0].para_list[0] = level;
 push_table(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}
LCM_DRIVER td4303r004a_fhd_dsi_cmd_jdi_a425a = {
 .name = "td4303r004a_fhd_dsi_cmd_jdi_a425a",
 .set_util_funcs = lcm_set_util_funcs,
 .get_params = lcm_get_params,
 .init = lcm_init,
 .suspend = lcm_suspend,
 .resume = lcm_resume,
 .compare_id = lcm_compare_id,
 .init_power = lcm_init_power,
 .resume_power = lcm_resume_power,
 .suspend_power = lcm_suspend_power,
 .set_backlight_cmdq = lcm_setbacklight,
 .update = lcm_update,
};
