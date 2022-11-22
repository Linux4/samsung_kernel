/* drivers/video/sprdfb/lcd_nt51017_mipi_lvds.c
 *
 * Support for nt51017 lvds LCD device
 *
 * Copyright (C) 2013 Spreadtrum
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
//#include <asm/arch/sprd_lcd.h>
//#include <asm/arch/sprd_i2c.h>
#include <linux/kernel.h>
//#include <linux/bug.h>
#include <linux/delay.h>
//#include <linux/platform_device.h>
#include <linux/i2c.h>
//#include <include/asm/gpio.h>
//#include "sp8830_i2c.h"
#include "../sprdfb_panel.h"

//#define printk printf

//#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

/***************** Important Macro! *****************/
/* For TI! ONLY ONE of the flowing marco can be defined! */
//#define LINES_ALL_RIGHT_TI
#define LINES_ONLY_CLK_RIGHT_TI

/* For CHIPONE! ONLY ONE of the flowing marco can be defined! */
//#ifdef CONFIG_SHARK_PAD_HW_V102  // 5735A-2 v1.0.2 & 5735C-1 v1.0.0
#if 1
#define LINES_ALL_WRONG_C1
#else  // 5735A-2 v1.0.1
#define LINES_ALL_RIGHT_C1
#endif
/***************** Important Macro! *****************/

struct bridge_ops {
	int32_t (*init)(struct panel_spec *self);
	int32_t (*suspend)(struct panel_spec *self);
};

static struct bridge_ops b_ops = {
	.init = NULL,
	.suspend = NULL,
};


#define I2CID_BRIDGE                2
#define I2C_NT50366_ADDR_WRITE      0x9E /* NT50169 100_1111 0 */
#define I2C_NT51017_ADDR_WRITE      0xD0 /* NT51017 110_1000 0 */
#define I2C_BRIDGE_ADDR_WRITE0      0x2C /* 0 010_1100 ADDR=Low */
#define I2C_BRIDGE_ADDR_WRITE1      0x2D /* 0 010_1101 ADDR=High */
#define I2C_BRIDGE_ADDR             I2C_BRIDGE_ADDR_WRITE0


#define GPIOID_LCMRSTN       103 /* should modify pinmap */
#define GPIOID_STBYB         230 /* should modify pinmap */
#define GPIOID_BRIDGE_EN     231
#define GPIOID_VDDPWR        232
#define GPIOID_ADDR          234 /* should modify pinmap */
#define GPIOID_LCDPWR        235

#define ARRAY_SIZE(array) ( sizeof(array) / sizeof(array[0]))

extern int gpio_direction_output(unsigned gpio, int value);
extern int gpio_request(unsigned gpio, const char *label);
extern void gpio_free(unsigned gpio);

const static __u8 i2c_msg_buf_ti[][2] = {
        {0x09, 0x00},
		{0x0A, 0x03},
		{0x0B, 0x18},
		{0x0D, 0x00},
		{0x10, 0x26},
		{0x11, 0x00},
		{0x12, 0x31},
		{0x13, 0x00},
#ifdef LINES_ALL_RIGHT_TI
		{0x18, 0x18},/* all the line is right */
#elif defined LINES_ONLY_CLK_RIGHT_TI
        {0x18, 0xF8},/* only clock line is right */
#endif
		{0x19, 0x0C},/* 0x00 */
		{0x1A, 0x03},
		{0x1B, 0x00},
		{0x20, 0x00},
		{0x21, 0x03},
		{0x22, 0x00},
		{0x23, 0x00},
		{0x24, 0x00},
		{0x25, 0x00},
		{0x26, 0x00},
		{0x27, 0x00},
		{0x28, 0x20},
		{0x29, 0x00},
		{0x2A, 0x00},
		{0x2B, 0x00},
		{0x2C, 0x04},/*0x04*/
		{0x2D, 0x00},
		{0x2E, 0x00},
		{0x2F, 0x00},
		{0x30, 0x04},/*0x04*/
		{0x31, 0x00},
		{0x32, 0x00},
		{0x33, 0x00},
		{0x34, 0x4C},
		{0x35, 0x00},
		{0x36, 0x00},
		{0x37, 0x00},
		{0x38, 0x00},
		{0x39, 0x00},
		{0x3A, 0x00},
		{0x3B, 0x00},
		{0x3C, 0x00},
		{0x3D, 0x00},
		{0x3E, 0x00},
};
const static __u8 i2c_msg_buf_c1[][2] = {
        /*{0x10, 0x4E},add*/
        {0x20, 0x00},
		{0x21, 0x00},
		{0x22, 0x43},
		{0x23, 0x28},
		{0x24, 0x04},
		{0x25, 0x4C},
		{0x26, 0x00},
		{0x27, 0x12},
        {0x28, 0x04},/*0x02*/
		{0x29, 0x13},
		{0x2A, 0x01},
		{0x34, 0xF0},/*add*/
		{0x36, 0x50},/*add*/
		{0x86, 0x2B},
		{0xB5, 0xA0},
		{0x51, 0x20},
		{0x56, 0x92},
		{0x69, 0x1C},/*0x1F*/
		{0x6B, 0x22},
		{0x5C, 0xFF},
		{0xB6, 0x20},
#ifdef LINES_ALL_RIGHT_C1
        {0x13, 0x10},
#elif defined LINES_ONLY_CLK_RIGHT_C1
        {0x13, 0x1F},
#elif defined LINES_ALL_WRONG_C1
		{0x13, 0x5F},
#endif
		/*{0x10, 0x47},add*/
		/*{0x2A, 0x31},modify*/
		/*{0x2E, 0x40},add*/
		/*{0x2F, 0x40},add*/
		{0x09, 0x10},
        /*{0x7B, 0xF2},*/
        /*{0x7C, 0xF3},*/
};

static struct i2c_msg msg_w = {
	.addr = I2C_BRIDGE_ADDR,
	.flags = 0,
	.len = 2,
	.buf = NULL,
};

static struct i2c_msg msg_r[] = {
	[0] = {
		.addr = I2C_BRIDGE_ADDR,
		.flags = 0,
		.len = 1,
		.buf = NULL,
	},
	[1] = {
		.addr = I2C_BRIDGE_ADDR,
		.flags = I2C_M_RD,
		.len = 1,
		.buf = NULL,
	},
};

static int32_t sn65dsi83_reg_read(struct panel_spec *self)
{
    __u8 reg[] = {0x09,0x0A,0x0B,0x0D,0x10,0x11,0x12,0x13,0x18,0x19,0x1A,0x1B,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E};
	__u8 val[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int32_t ret_i2c = 0;
	int32_t i = 0;
	struct i2c_adapter *adap = i2c_get_adapter(I2CID_BRIDGE);

	for (i=0; i<ARRAY_SIZE(reg); i++){
		msg_r[0].buf = &reg[i];
		msg_r[1].buf = &val[i];
		ret_i2c = i2c_transfer(adap, &msg_r, 2);
		LCD_PRINT("kernel Read All Regs! SendMsgNum=%d, Reg=0x%x, Src=0x%x,Read=0x%x,\n", ret_i2c,reg[i],i2c_msg_buf_ti[2*i+1],val[i]);
	}

    LCD_PRINT("kernel sn65dsi83_reg_read! adap_addr=0x%x, name=%s, nr=%d\n", adap, adap->name, adap->nr);

    return 0;
}

/* set pin, return 0:OK, other value:failed */
static int32_t set_en_pin(struct panel_spec *self, uint8_t value, uint8_t delays)//value 0 : set low,  value not 0 : set high
{
    static uint8_t gpio_is_requested = 0;
    
    /* request gpio */
    if (!gpio_is_requested){
		if (gpio_request(GPIOID_BRIDGE_EN, "BRIDGE_EN")){
			printk("kernel GPIO%d requeste failed!\n",GPIOID_BRIDGE_EN);
			return 1;
		}
		gpio_is_requested = 1;
	}

	if (value) {//set high
		gpio_direction_output(GPIOID_BRIDGE_EN, 0);
		msleep(delays);
		gpio_direction_output(GPIOID_BRIDGE_EN, 1);
	} else {//set low
        gpio_direction_output(GPIOID_BRIDGE_EN, 0);
		gpio_free(GPIOID_BRIDGE_EN);
		gpio_is_requested = 0;
	}
	
    return 0;
}

/* Init SN65DSI83 */
static int32_t sn65dsi83_init(struct panel_spec *self)
{
    int32_t i = 0;
	int32_t ret_i2c = 0;

	__u8 i2c_msg_buf_pll_en[]     = {0x0D, i2c_msg_buf_ti[3][1]|0x01};// PLL_EN (CSR 0x0D.0)
	__u8 i2c_msg_buf_soft_reset[] = {0x09, i2c_msg_buf_ti[0][1]|0x01};// SOFT_RESET (CSR 0x09.0)

	struct i2c_adapter *adap = i2c_get_adapter(I2CID_BRIDGE);

    mipi_set_lp_mode_t mipi_set_lp_mode = self->info.mipi->ops->mipi_set_lp_mode;/* u-boot\arch\arm\include\asm\arch\sprd_lcd.h */
	mipi_set_hs_mode_t mipi_set_hs_mode = self->info.mipi->ops->mipi_set_hs_mode;
	mipi_set_video_mode_t mipi_set_video_mode = self->info.mipi->ops->mipi_set_video_mode;
	
    /* step 1 : All DSI into LP11*/
	mipi_set_lp_mode();

	/* step 2 : Set EN*/
	if (set_en_pin(self, 1, 30)){
		printk("kernel Set EN failed!\n");
		return 0;
	}

	/* step 3 : Delay*/
	udelay(2000);
	
	/* step 4 : init CSR reg*/
	for (i=0; i<ARRAY_SIZE(i2c_msg_buf_ti); i++) {
		msg_w.buf = i2c_msg_buf_ti[i];
		ret_i2c = i2c_transfer(adap, &msg_w, 1);
	}

	/* step 5 : Start the DSI video stream */
	mipi_set_hs_mode();
	mipi_set_video_mode();
	udelay(1000);

	/* step 6 : Set the PLL_EN bit(CSR 0x0D.0) */
	msg_w.buf = i2c_msg_buf_pll_en;
	ret_i2c = i2c_transfer(adap, &msg_w, 1);

	/* step 7 : Wait for the PLL_LOCK bit to be set(CSR 0x0A.7) */
	udelay(1000);
    
	/* step 8 : Set the SOFT_RESET bit (CSR 0x09.0) */
	msg_w.buf = i2c_msg_buf_soft_reset;
	ret_i2c = i2c_transfer(adap, &msg_w, 1);

    return 0;
}

static int32_t chipone_init(struct panel_spec *self)
{
    int32_t ret_i2c = 0;
	int32_t i = 0;

	struct i2c_adapter *adap = i2c_get_adapter(I2CID_BRIDGE);

    /* Set EN pin */
	if (set_en_pin(self, 1, 10)){
		printk("kernel Set EN failed!\n");
		return 0;
	}
	udelay(1000);

    /* init chipone */
    for (i=0; i<ARRAY_SIZE(i2c_msg_buf_c1); i++) {
		msg_w.buf = i2c_msg_buf_c1[i];
		ret_i2c = i2c_transfer(adap, &msg_w, 1);
	}

    return 0;
}

/* reset sn65dsi83 */
static int32_t sn65dsi83_suspend(struct panel_spec *self)
{
	__u8 i2c_msg_buf_pll_en[] = {0x0D, i2c_msg_buf_ti[3][1] & 0xFE};// PLL_EN (CSR 0x0D.0)
	int32_t ret_i2c = 0;

	struct i2c_adapter *adap = i2c_get_adapter(I2CID_BRIDGE);
	
    LCD_PRINT("kernel sn65dsi83_suspend! adap_addr=0x%x, name=%s, nr=%d\n", adap, adap->name, adap->nr);

	/*1. Clear the PLL_EN bit to 0(CSR 0x0D.0*/
	msg_w.buf = i2c_msg_buf_pll_en;
	ret_i2c = i2c_transfer(adap, &msg_w, 1);

    /* set EN to 0 */
	if (set_en_pin(self, 0, 1)){
		printk("kernel Set EN failed!\n");
		return 0;
	}
	
	return 0;
}

static int32_t chipone_suspend(struct panel_spec *self)
{
	/* set EN to 0 */
	if (set_en_pin(self, 0, 1)){
		printk("kernel Set EN failed!\n");
		return 0;
	}
	return 0;
}

static int32_t nt51017_power(struct panel_spec *self, uint8_t power)//0:power down, 1:power up
{
	static uint8_t is_requested = 0;
	
	if(!is_requested){//should request gpio
		if (gpio_request(GPIOID_VDDPWR, "VDDPWR")){
			printk("kernel GPIO%d requeste failed!\n", GPIOID_VDDPWR);
			return 0;
		}

		if (gpio_request(GPIOID_LCDPWR, "LCDPWR")){
			printk("kernel GPIO%d requeste failed!\n", GPIOID_LCDPWR);
			return 0;
		}

		is_requested = 1;
	}

	if (is_requested){
		gpio_direction_output(GPIOID_VDDPWR, power);
		gpio_direction_output(GPIOID_LCDPWR, power);

		if (!power){//power down, should free gpio
			gpio_free(GPIOID_VDDPWR);
			gpio_free(GPIOID_LCDPWR);
			is_requested = 0;
		}
	}
	
	return 0;
}

static int32_t get_bridge_info(struct panel_spec *self)
{
	__u8 reg = 0x00;
	__u8 flag = 0xFF;
	int32_t ret_i2c = 0;

	struct i2c_adapter *adap = i2c_get_adapter(I2CID_BRIDGE);

	if (set_en_pin(self, 1, 30)){
		printk("kernel Set EN failed!\n");
		return 1;
	}

	if (gpio_request(GPIOID_ADDR, "ADDR")){
		printk("kernel GPIO%d requeste failed!\n", GPIOID_ADDR);
		return 1;
	}
	gpio_direction_output(GPIOID_ADDR, 0);// drive ADDR to low
	udelay(1000);

	msg_r[0].buf = &reg;
	msg_r[1].buf = &flag;
	ret_i2c = i2c_transfer(adap, &msg_r, 2);
	if (0 == ret_i2c){
		printk("get_bridge_info, I2C Read Failed!\n");
		return 1;
	}

	if (0x35 == flag){//TI
		b_ops.init = sn65dsi83_init;
		b_ops.suspend = sn65dsi83_suspend;
	} else if (0xC1 == flag) {//ChipOne
		b_ops.init = chipone_init;
		b_ops.suspend = chipone_suspend;
	} else {
		printk("get_bridge_info, Unknown bridge! flag=0x%x\n", flag);
		gpio_free(GPIOID_ADDR);
		return 1;
	}

	return 0;
}

static int32_t nt51017_mipi_lvds_init(struct panel_spec *self)
{
	LCD_PRINT("kernel nt51017_mipi_lvds_init\n");

    /* init */
	if (b_ops.init) {
		b_ops.init(self);
	} else {
		if (get_bridge_info(self)){//get bridge info fail
			printk("Get bridge info fail!\n");
			return 0;
		} else {
			b_ops.init(self);
		}
	}

	/* power up */
	nt51017_power(self, 1);

	LCD_PRINT("kernel nt51017_mipi_lvds_init over!\n");
	
	return 0;
}

static int32_t nt51017_mipi_lvds_reset(struct panel_spec *self)
{
	LCD_PRINT("kernel nt51017_mipi_lvds_reset\n");

	return 0;
}

static int32_t nt51017_mipi_lvds_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
    LCD_PRINT("kernel nt51017_mipi_lvds_enter_sleep! is_sleep=%d\n",is_sleep);

	if (is_sleep){//sleep in	
	    nt51017_power(self, 0);// power down 
		if (b_ops.suspend) {
			b_ops.suspend(self);
		} else {
			if (get_bridge_info(self)){
				printk("Get bridge info fail when suspend!\n");
				return 0;
			} else {
				b_ops.suspend(self);
			}
		}
	} else {//sleep out
	    nt51017_power(self, 1);// power up
        if (b_ops.init) {
			b_ops.init(self);
		} else {
			if (get_bridge_info(self)){//get bridge info fail
				printk("Get bridge info fail!\n");
				return 0;
			} else {
				b_ops.init(self);
			}
		}
	}

    return 0;
}

static uint32_t nt51017_mipi_lvds_after_suspend(struct panel_spec *self)
{
    nt51017_power(self, 0);// power down
   
    if (b_ops.suspend) {
		b_ops.suspend(self);
	}else {
		if (get_bridge_info(self)){
			printk("Get bridge info fail when suspend!\n");
			return 0;
		} else {
			b_ops.suspend(self);
		}
	}
    
    LCD_PRINT("kernel nt51017_mipi_lvds_after_suspend\n");

	return 0;
}

static uint32_t nt51017_mipi_lvds_before_resume(struct panel_spec *self)
{
    LCD_PRINT("kernel nt51017_mipi_lvds_before_resume\n");

	return 0;
}

static uint32_t nt51017_mipi_lvds_readid(struct panel_spec *self)
{
    LCD_PRINT("kernel nt51017_mipi_lvds_readid\n");

	return 0xC749;/*51017*/
}

static int32_t nt51017_mipi_lvds_check_esd(struct panel_spec *self)
{
    LCD_PRINT("kernel nt51017_mipi_lvds_check_esd\n");
	
    return 1;
}

static struct panel_operations lcd_nt51017_mipi_lvds_operations = {
	.panel_init = nt51017_mipi_lvds_init,
    .panel_reset = NULL,//nt51017_mipi_lvds_reset,
	.panel_enter_sleep = nt51017_mipi_lvds_enter_sleep,
	.panel_after_suspend = nt51017_mipi_lvds_after_suspend,
	.panel_before_resume = NULL,
	.panel_readid = nt51017_mipi_lvds_readid,
	.panel_esd_check = NULL,
};

static struct timing_rgb lcd_nt51017_mipi_lvds_timing = {
	.hfp = 110,  /* unit: pixel */
	.hbp = 76,
	.hsync = 4,//4
	.vfp = 18, /*unit: line*/
	.vbp = 19,
	.vsync = 4,
};

static struct info_mipi lcd_nt51017_mipi_lvds_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 4,
	.phy_feq = 512*1000,/*494*/
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_nt51017_mipi_lvds_timing,
	.ops = NULL,
};

struct panel_spec lcd_nt51017_mipi_lvds_spec = {
	//.cap = PANEL_CAP_NOT_TEAR_SYNC,
	.width = 768,
	.height = 1024,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,//bug 273509
	.info = {
		.mipi = &lcd_nt51017_mipi_lvds_info
	},
	.ops = &lcd_nt51017_mipi_lvds_operations,
};

struct panel_cfg lcd_nt51017_mipi_lvds = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0xC749,
	.lcd_name = "lcd_nt51017_mipi_lvds",
	.panel = &lcd_nt51017_mipi_lvds_spec,
};

static int __init lcd_nt51017_mipi_lvds_init(void)
{
	return sprdfb_panel_register(&lcd_nt51017_mipi_lvds);
}

subsys_initcall(lcd_nt51017_mipi_lvds_init);


