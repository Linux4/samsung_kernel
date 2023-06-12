
#ifdef BUILD_LK
#include <string.h>
#include <mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
/* jongmahn.kim@lge.com */
#include <linux/string.h>
#include "mt_gpio.h"
#include <mach/gpio_const.h>
/* #include <mach/mt_pm_ldo.h> */
#endif

#include "lcm_drv.h"
/* #include <cust_gpio_usage.h> */
#if defined(BUILD_LK)
#define LCM_PRINT printf
#include <boot_mode.h>
#elif defined(BUILD_UBOOT)
#include <mach/upmu_hw.h>
#include <mt_typedefs.h>
#define LCM_PRINT printf
#else
#include <mach/upmu_hw.h>
/* #include <mt_typedefs.h> */
#define LCM_PRINT printk
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
/* pixel */
#define FRAME_WIDTH			(480)
#define FRAME_HEIGHT			(854)
/* physical dimension */
#define PHYSICAL_WIDTH        (55)
#define PHYSICAL_HIGHT         (99)


#define LCM_ID       (0x40)

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA	/* END OF REGISTERS MARKER */

/* #define USING_EXT_LDO */

/* DSV power +5V,-5v */
#ifndef GPIO_DSV_AVDD_EN
#define GPIO_DSV_AVDD_EN (GPIO41 | 0x80000000)
#define GPIO_DSV_AVDD_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVDD_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVDD_EN_M_PWM GPIO_MODE_05
#endif

#ifndef GPIO_DSV_AVEE_EN
#define GPIO_DSV_AVEE_EN (GPIO42 | 0x80000000)
#define GPIO_DSV_AVEE_EN_M_GPIO GPIO_MODE_00
#define GPIO_DSV_AVEE_EN_M_KROW GPIO_MODE_06
#define GPIO_DSV_AVEE_EN_M_PWM GPIO_MODE_05
#endif

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)								(lcm_util.set_reset_pin((v)))
#define UDELAY(n)	(lcm_util.udelay(n))
#define MDELAY(n)	(lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V3(para_tbl, size, force_update)\
lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)\
lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)\
lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)\
lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)\
lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)\
lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)\
lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static unsigned int need_set_lcm_addr = 1;


/* jongmahn.kim@lge.com */
#ifndef GPIO_LCM_RST
#define GPIO_LCM_RST         (GPIO146 | 0x80000000)
#define GPIO_LCM_RST_M_GPIO   GPIO_MODE_00
#define GPIO_LCM_RST_M_LCM_RST   GPIO_MODE_01
#endif


struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/* 150821 initcode ver.A0 E0 */
static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {
	{0x39, 0xFF, 5, {0xFF, 0x98, 0x06, 0x04, 0x01} },	/* change to page 1 */
	{0x15, 0x08, 1, {0x10} },
	{0x15, 0x21, 1, {0x01} },
	{0x15, 0x30, 1, {0x01} },
	{0x15, 0x31, 1, {0x00} },
	{0x15, 0x40, 1, {0x11} },
	{0x15, 0x41, 1, {0x22} },
	{0x15, 0x42, 1, {0x03} },
	{0x15, 0x43, 1, {0x89} },
	{0x15, 0x44, 1, {0x86} },
	{0x15, 0x50, 1, {0x70} },
	{0x15, 0x51, 1, {0x70} },
	{0x15, 0x52, 1, {0x00} },
	{0x15, 0x53, 1, {0x4A} },
	{0x15, 0x60, 1, {0x0A} },
	{0x15, 0x61, 1, {0x00} },
	{0x15, 0x62, 1, {0x0A} },
	{0x15, 0x63, 1, {0x00} },

	{0x15, 0xA0, 1, {0x00} },	/* Gamma setting */
	{0x15, 0xA1, 1, {0x0C} },
	{0x15, 0xA2, 1, {0x15} },
	{0x15, 0xA3, 1, {0x0F} },
	{0x15, 0xA4, 1, {0x09} },
	{0x15, 0xA5, 1, {0x15} },
	{0x15, 0xA6, 1, {0x0A} },
	{0x15, 0xA7, 1, {0x0B} },
	{0x15, 0xA8, 1, {0x02} },
	{0x15, 0xA9, 1, {0x06} },
	{0x15, 0xAA, 1, {0x06} },
	{0x15, 0xAB, 1, {0x07} },
	{0x15, 0xAC, 1, {0x0C} },
	{0x15, 0xAD, 1, {0x34} },
	{0x15, 0xAE, 1, {0x30} },
	{0x15, 0xAF, 1, {0x00} },

	{0x15, 0xC0, 1, {0x00} },
	{0x15, 0xC1, 1, {0x07} },
	{0x15, 0xC2, 1, {0x0F} },
	{0x15, 0xC3, 1, {0x0F} },
	{0x15, 0xC4, 1, {0x08} },
	{0x15, 0xC5, 1, {0x13} },
	{0x15, 0xC6, 1, {0x09} },
	{0x15, 0xC7, 1, {0x06} },
	{0x15, 0xC8, 1, {0x05} },
	{0x15, 0xC9, 1, {0x0A} },
	{0x15, 0xCA, 1, {0x08} },
	{0x15, 0xCB, 1, {0x03} },
	{0x15, 0xCC, 1, {0x0B} },
	{0x15, 0xCD, 1, {0x28} },
	{0x15, 0xCE, 1, {0x22} },
	{0x15, 0xCF, 1, {0x00} },

	{0x39, 0xFF, 5, {0xFF, 0x98, 0x06, 0x04, 0x06} },	/* change to page 6 */
	{0x15, 0x00, 1, {0x21} },
	{0x15, 0x01, 1, {0x0A} },
	{0x15, 0x02, 1, {0x00} },
	{0x15, 0x03, 1, {0x00} },
	{0x15, 0x04, 1, {0x01} },
	{0x15, 0x05, 1, {0x01} },
	{0x15, 0x06, 1, {0x80} },
	{0x15, 0x07, 1, {0x06} },
	{0x15, 0x08, 1, {0x01} },
	{0x15, 0x09, 1, {0x80} },
	{0x15, 0x0a, 1, {0x00} },
	{0x15, 0x0b, 1, {0x00} },
	{0x15, 0x0c, 1, {0x0A} },
	{0x15, 0x0d, 1, {0x0A} },
	{0x15, 0x0e, 1, {0x00} },
	{0x15, 0x0F, 1, {0x00} },
	{0x15, 0x10, 1, {0xF0} },
	{0x15, 0x11, 1, {0xF4} },
	{0x15, 0x12, 1, {0x04} },
	{0x15, 0x13, 1, {0x00} },
	{0x15, 0x14, 1, {0x00} },
	{0x15, 0x15, 1, {0xC0} },
	{0x15, 0x16, 1, {0x08} },
	{0x15, 0x17, 1, {0x00} },
	{0x15, 0x18, 1, {0x00} },
	{0x15, 0x19, 1, {0x00} },
	{0x15, 0x1a, 1, {0x00} },
	{0x15, 0x1b, 1, {0x00} },
	{0x15, 0x1c, 1, {0x00} },
	{0x15, 0x1d, 1, {0x00} },
	{0x15, 0x20, 1, {0x01} },
	{0x15, 0x21, 1, {0x23} },
	{0x15, 0x22, 1, {0x45} },
	{0x15, 0x23, 1, {0x67} },
	{0x15, 0x24, 1, {0x01} },
	{0x15, 0x25, 1, {0x23} },
	{0x15, 0x26, 1, {0x45} },
	{0x15, 0x27, 1, {0x67} },
	{0x15, 0x30, 1, {0x01} },
	{0x15, 0x31, 1, {0x11} },
	{0x15, 0x32, 1, {0x00} },
	{0x15, 0x33, 1, {0xEE} },
	{0x15, 0x34, 1, {0xFF} },
	{0x15, 0x35, 1, {0xBB} },
	{0x15, 0x36, 1, {0xCA} },
	{0x15, 0x37, 1, {0xDD} },
	{0x15, 0x38, 1, {0xAC} },
	{0x15, 0x39, 1, {0x76} },
	{0x15, 0x3a, 1, {0x67} },
	{0x15, 0x3b, 1, {0x22} },
	{0x15, 0x3c, 1, {0x22} },
	{0x15, 0x3d, 1, {0x22} },
	{0x15, 0x3e, 1, {0x22} },
	{0x15, 0x3F, 1, {0x22} },
	{0x15, 0x40, 1, {0x22} },
	{0x15, 0x52, 1, {0x10} },
	{0x15, 0x53, 1, {0x10} },
	{0x15, 0x58, 1, {0x97} },

	{0x39, 0xFF, 5, {0xFF, 0x98, 0x06, 0x04, 0x07} },	/* change to page 7 */
	{0x15, 0x17, 1, {0x22} },
	{0x15, 0x02, 1, {0x77} },
	{0x15, 0xE1, 1, {0x79} },

	{0x39, 0xFF, 5, {0xff, 0x98, 0x06, 0x04, 0x00} },	/* change to page 0 */
	{0x15, 0x35, 1, {0x00} },
	{0x15, 0x55, 1, {0x90} },	/* Color Enhancement Medium */
	{REGFLAG_END_OF_TABLE, 0x00, 1, {} },
};

#if 0				/* defined(BUILD_LK) */
static LCM_setting_table_V3 lcm_EXTC_set_enable_V3[] = {
	{0X39, 0XFF, 5, {0XFF, 0X98, 0X06, 0X04, 0X00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

/*
static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for(i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
	LCM_PRINT("[LCD] push_table\n");
}
*/

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */


static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy((void *)&lcm_util, (void *)util, sizeof(LCM_UTIL_FUNCS));
}



static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width = PHYSICAL_WIDTH;
	params->physical_height = PHYSICAL_HIGHT;

	/* enable tearing-free */
	params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;	/* SYNC_EVENT_VDO_MODE; */


	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	/* The following defined the fomat for data coming from LCD engine. */

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* Video mode setting */
	/* params->dsi.intermediat_buffer_num = 2; */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 80;
	params->dsi.horizontal_frontporch = 100;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	/* Bit rate calculation */
	/* params->dsi.pll_div1=35;
	// fref=26MHz, fvco=fref*(div1+1)       (div1=0~63, fvco=500MHZ~1GHz) */
	/* params->dsi.pll_div2=1;               // div2=0~15: fout=fvo/(2*div2) */

	/* ESD or noise interference recovery For video mode LCM only. */
	/* Send TE packet to LCM in a period of n frames and check the response. */
	/* params->dsi.lcm_int_te_monitor = 0; */
	/* params->dsi.lcm_int_te_period = 1;            // Unit : frames */

	/* Need longer FP for more opportunity to do int. TE monitor applicably. */
	/* if(params->dsi.lcm_int_te_monitor) */
	/* params->dsi.vertical_frontporch *= 2; */

	/* Monitor external TE (or named VSYNC) from LCM once per 2 sec.
	(LCM VSYNC must be wired to baseband TE pin.) */
	/* params->dsi.lcm_ext_te_monitor = 0; */
	/* Non-continuous clock */
	/* params->dsi.noncont_clock = 1; */
	/* params->dsi.noncont_clock_period = 2; // Unit : frames */

	/* DSI MIPI Spec parameters setting */
	params->dsi.HS_TRAIL = 7;
	/*params->dsi.HS_ZERO = 9;
	   params->dsi.HS_PRPR = 5;
	   params->dsi.LPX = 4;
	   params->dsi.TA_SACK = 1;
	   params->dsi.TA_GET = 20;
	   params->dsi.TA_SURE = 6;
	   params->dsi.TA_GO = 16;
	   params->dsi.CLK_TRAIL = 5;
	   params->dsi.CLK_ZERO = 18;
	   params->dsi.LPX_WAIT = 1;
	   params->dsi.CONT_DET = 0;
	   params->dsi.CLK_HS_PRPR = 4; */
	/* Bit rate calculation */
	params->dsi.PLL_CLOCK = 236;

	LCM_PRINT("[LCD] lcm_get_params\n");

}


static void init_lcm_registers(void)
{
	unsigned int data_array[16];

	dsi_set_cmdq_V3(lcm_initialization_setting_V3,
			sizeof(lcm_initialization_setting_V3) / sizeof(LCM_setting_table_V3), 1);

	data_array[0] = 0x00063902;	/* Changed to Page 0 */
	data_array[1] = 0x0698FFFF;
	data_array[2] = 0x00000004;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00110500;	/* Sleep Out */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(150);

	data_array[0] = 0x00290500;	/* Display On */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(50);

	LCM_PRINT("[LCD] init_lcm_registers\n");
}

#if 0
static void init_lcm_registers_added(void)
{
	unsigned int data_array[1];

#if defined(BUILD_LK)
	dsi_set_cmdq_V3(lcm_EXTC_set_enable_V3,
			sizeof(lcm_EXTC_set_enable_V3) / sizeof(LCM_setting_table_V3), 1);
	MDELAY(1);
#endif
	data_array[0] = 0x00290500;	/* Display On */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);
	LCM_PRINT("[LCD] init_lcm_registers_added\n");
}
#endif

static void init_lcm_registers_sleep(void)
{
	unsigned int data_array[3];

	MDELAY(10);

	data_array[0] = 0x00063902;	/* Changed to Page 0 */
	data_array[1] = 0x0698FFFF;
	data_array[2] = 0x00000004;
	dsi_set_cmdq(data_array, 3, 1);

	MDELAY(1);

	data_array[0] = 0x00280500;	/* Display Off */
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(50);

	LCM_PRINT("[LCD] init_lcm_registers_Display_off\n");
}

static void init_lcm_registers_sleep_cmd(void)
{
	unsigned int data_array[1];

	data_array[0] = 0x00100500;	/* Sleep in */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(150);

	LCM_PRINT("[LCD] init_lcm_registers_sleep_cmd\n");
}


/* vgp2 1.8v LDO enable */
/*
static void ldo_1v8io_on(void)
{
#ifdef USING_EXT_LDO
	#ifdef BUILD_UBOOT
		#error "not implemeted"
	#elif defined(BUILD_LK)
		mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_LCM_PWR2_EN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_LCM_PWR_EN, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	#else
	    LCM_PRINT("[LCD] ldo_1v8io_on\n");
		mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ONE);
	#endif
#endif
}
*/

/* vgp2 1.8v LDO disable */
/*
static void ldo_1v8io_off(void)
{
#ifdef USING_EXT_LDO
	#ifdef BUILD_UBOOT
	#error "not implemeted"
	#elif defined(BUILD_LK)
		mt_set_gpio_mode(GPIO_LCM_PWR_EN, GPIO_LCM_PWR2_EN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_LCM_PWR_EN, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(GPIO_LCM_PWR_EN, GPIO_DIR_OUT);

		mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ZERO);
	#else
		LCM_PRINT("[LCD] ldo_1v8io_off\n");
		mt_set_gpio_out(GPIO_LCM_PWR_EN, GPIO_OUT_ZERO);
	#endif
#endif
}
*/

/*
static void ldo_ext_2v8_on(void)
{
#ifdef USING_EXT_LDO
	#ifdef BUILD_UBOOT
		#error "not implemeted"
	#elif defined(BUILD_LK)
		mt_set_gpio_mode(GPIO_LCM_PWR2_EN, GPIO_LCM_PWR2_EN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_LCM_PWR2_EN, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(GPIO_LCM_PWR2_EN, GPIO_DIR_OUT);

		mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ONE);
	#else
		mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ONE);
	#endif
#else
#if 0 //alway on
	#ifdef BUILD_UBOOT
		#error "not implemeted"
	#elif defined(BUILD_LK)
		pmic_set_register_value(PMIC_RG_VIO28_EN, 1);
	#else
		hwPowerOn(MT6328_POWER_LDO_VIO28, VOL_2800, "2V8_LCD_VCI");
		MDELAY(5);
	#endif
#endif
#endif
}
*/

/*
static void ldo_ext_2v8_off(void)
{
#ifdef USING_EXT_LDO
	#ifdef BUILD_UBOOT
		#error "not implemeted"
	#elif defined(BUILD_LK)
		mt_set_gpio_mode(GPIO_LCM_PWR2_EN, GPIO_LCM_PWR2_EN_M_GPIO);
		mt_set_gpio_pull_enable(GPIO_LCM_PWR2_EN, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(GPIO_LCM_PWR2_EN, GPIO_DIR_OUT);

		mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ZERO);
	#else
		mt_set_gpio_out(GPIO_LCM_PWR2_EN, GPIO_OUT_ZERO);
	#endif
#else
#if 0
	#ifdef BUILD_UBOOT
		#error "not implemeted"
	#elif defined(BUILD_LK)
		pmic_set_register_value(PMIC_RG_VIO28_EN, 0);
	#else
		hwPowerDown(MT6328_POWER_LDO_VIO28, VOL_2800, "2V8_LCD_VCI");
		MDELAY(5);
	#endif
#endif
#endif
}
*/

/* DSV power +5V,-5v */
/*
static void ldo_p5m5_dsv_3v0_on(void)
{
#if 1
#if 0
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ONE);
#endif
#else
	mt_set_gpio_mode(GPIO_DSV_AVDD_EN, GPIO_DSV_AVDD_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVDD_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVDD_EN, GPIO_DIR_OUT);
	mt_set_gpio_mode(GPIO_DSV_AVEE_EN, GPIO_DSV_AVEE_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVEE_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVEE_EN, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO_DSV_AVDD_EN, GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO_DSV_AVEE_EN, GPIO_OUT_ONE);
#endif
}
*/

/*
static void ldo_p5m5_dsv_3v0_off(void)
{
#if 1
#if 0
	mt_set_gpio_mode(GPIO_DSV_EN, GPIO_DSV_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DSV_EN, GPIO_OUT_ZERO);
#endif
#else
	mt_set_gpio_mode(GPIO_DSV_AVDD_EN, GPIO_DSV_AVDD_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVDD_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVDD_EN, GPIO_DIR_OUT);
	mt_set_gpio_mode(GPIO_DSV_AVEE_EN, GPIO_DSV_AVEE_EN_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_DSV_AVEE_EN, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_DSV_AVEE_EN, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO_DSV_AVDD_EN, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO_DSV_AVEE_EN, GPIO_OUT_ZERO);
#endif
}
*/

static void reset_lcd_module(unsigned char reset)
{
	mt_set_gpio_mode(GPIO_LCM_RST, GPIO_LCM_RST_M_GPIO);
	mt_set_gpio_pull_enable(GPIO_LCM_RST, GPIO_PULL_ENABLE);
	mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);

	if (reset)
		mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
	else
		mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
}

static void lcm_init(void)
{
	MDELAY(5);
	/*
	   / reset pin High > delay 1ms > Low > delay 10ms > High > delay 120ms
	   / LCD RESET PIN
	 */
	MDELAY(1);
	reset_lcd_module(1);
	MDELAY(1);
	reset_lcd_module(0);
	MDELAY(10);
	reset_lcd_module(1);
	MDELAY(120);

	init_lcm_registers();	/* Change to Page 1 ~ Change to Page 0 */

	/* MDELAY(20); */
	/* init_lcm_registers_added(); //Display On */
	need_set_lcm_addr = 1;

	LCM_PRINT("[LCD] lcm_init\n");
}

static void lcm_suspend(void)
{
	init_lcm_registers_sleep();	/* Display off */
	init_lcm_registers_sleep_cmd();	/* Sleep in command */

	reset_lcd_module(0);	/* reset pin low */

	LCM_PRINT("[LCD] lcm_suspend\n");
}

static void lcm_resume(void)
{
	lcm_init();
	need_set_lcm_addr = 1;

	LCM_PRINT("[LCD] lcm_resume\n");
}

/*
static void lcm_esd_recover(void)
{
	lcm_suspend();
	lcm_resume();

	LCM_PRINT("[LCD] lcm_esd_recover\n");
}
*/

/*
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	// need update at the first time
	if(need_set_lcm_addr)
	{
		data_array[0]= 0x00053902;
		data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
		data_array[2]= (x1_LSB);
		dsi_set_cmdq(data_array, 3, 1);

		data_array[0]= 0x00053902;
		data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
		data_array[2]= (y1_LSB);
		dsi_set_cmdq(data_array, 3, 1);
		need_set_lcm_addr = 0;
	}

	data_array[0]= 0x002c3909;
   dsi_set_cmdq(data_array, 1, 0);
	LCM_PRINT("[LCD] lcm_update\n");
}
*/


/*
static unsigned int lcm_compare_id(void)
{
#if 0
	unsigned int id=0;
	unsigned char buffer[2];
	unsigned int array[16];
    SET_RESET_PIN(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(10);//Must over 6 ms
	array[0]=0x00043902;
	array[1]=0x8983FFB9;// page enable
	dsi_set_cmdq(array, 2, 1);
	MDELAY(10);
	array[0] = 0x00023700;// return byte number
	dsi_set_cmdq(array, 1, 1);
	MDELAY(10);
	read_reg_v2(0xDC, buffer, 2);
	id = buffer[0];
	LCM_PRINT("%s, id = 0x%08x\n", __func__, id);
	return (LCM_ID == id)?1:0;
#else

	LCM_PRINT("[ili9806_e0_dsi_vdo_fwvga]\n");
		return 1;
#endif
}
*/

/* --------------------------------------------------------------------------- */
/* Get LCM Driver Hooks */
/* --------------------------------------------------------------------------- */
LCM_DRIVER ili9806_e0_dsi_vdo_fwvga_drv = {
	.name = "ili9806_e0_dsi_vdo_fwvga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	/* .compare_id = lcm_compare_id, */
	/* .update = lcm_update, */
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
/* .esd_recover = lcm_esd_recover, */
#endif
};
