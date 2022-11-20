/* linux/drivers/video/backlight/s6e8aa0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <plat/gpio-cfg.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>

#include <plat/dsim.h>
#include <plat/mipi_dsi.h>

#include "s6e8aa0a02_param.h"

#include "dynamic_aid_s6e8aa0a02.h"

#include "../exynos_display_handler.h"

#include <plat/regs-mipidsim.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/regs-pmu.h>


struct lcd_info *g_lcd;

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define MAX_GAMMA			300
#define LINEAR_MIN_GAMMA	30


#define DEFAULT_BRIGHTNESS		135

#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_162NT


#define LDI_ID_REG			0xD1
#define LDI_ID_LEN			3

#define AID_PARAM_SIZE		19


#define LDI_MTP_LENGTH		24
#define LDI_MTP_ADDR			0xD3

#define LDI_HBMELVSS_REG		0xD4
#define LDI_HBMELVSS_LEN		24
#define LDI_ELVSS_LEN			2

#define LDI_COORDINATE_REG		0xD1
#define LDI_COORDINATE_LEN		7

#define LDI_DATE_REG		0xD4
#define LDI_DATE_LEN		10


#define DYNAMIC_ELVSS_MIN_VALUE	0x81
#define DYNAMIC_ELVSS_MAX_VALUE	0x9F

#define ELVSS_MODE0_MIN_VOLTAGE	62
#define ELVSS_MODE1_MIN_VOLTAGE	52

#define LDI_ERRFG_REG	0xE5


struct str_elvss {
	u8 reference;
	u8 limit;
};

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

static void s6e8ax0_read_id(struct lcd_info *lcd, u8 *buf);

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;

	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;

	unsigned char			id[LDI_ID_LEN];

	unsigned char			**gamma_table;
	unsigned char			**elvss_table;
	unsigned char			elvss_value[LDI_ELVSS_LEN];
	unsigned char			current_elvss;

	struct dynamic_aid_param_t	daid;
	unsigned char			aor;
	unsigned int			connected;

	unsigned int			coordinate[2];

	struct mipi_dsim_device		*dsim;
	unsigned int			*gamma_level;
};


static const unsigned int DIM_TABLE[IBRIGHTNESS_MAX] = {
	5,6,7,8,9,10,11,12,13,14,15,16,
	17,19,20,21,22,24,25,27,29,30,
	32,34,37,39,41,44,47,50,53,56,
	60,64,68,72,77,82,87,93,98,105,
	111,119,126,134,143,152,162,172,
	183,195,207,220,234,249,265,282,
	300,400
};


static void dump_register(void __iomem *reg, u32 pa, u32 end_offset)
{
	unsigned int i, pos = 0;
	unsigned char linebuf[80] = {0,};

	memset(linebuf, 0, sizeof(linebuf));
	pos = sprintf(linebuf, "%08X| ", pa);
	hex_dump_to_buffer(reg, 16, 16, 4, linebuf + pos, sizeof(linebuf) - pos, false);
	pr_err("%s\n", linebuf);

	for (i = 0; i <= end_offset; i += 16) {
		memset(linebuf, 0, sizeof(linebuf));
		pos = sprintf(linebuf, "%08X| ", pa + i);
		hex_dump_to_buffer(reg + i, 16, 16, 4, linebuf + pos, sizeof(linebuf) - pos, false);
		pr_err("%s\n", linebuf);
	}
}

static void dsim_reg_dump(struct lcd_info *lcd)
{
	pr_err("DISP_CONFIGURATION     [0x%08x]\n", readl(EXYNOS5260_DISP_CONFIGURATION));
	pr_err("DISP_STATUS            [0x%08x]\n", readl(EXYNOS5260_DISP_STATUS));
	pr_err("DISP_OPTION            [0x%08x]\n", readl(EXYNOS5260_DISP_OPTION));
	pr_err("MIPI_DPHY_CONTORL0     [0x%08x]\n", readl(S5P_MIPI_DPHY_CONTROL(0)));
	pr_err("MIPI_DPHY_CONTORL1     [0x%08x]\n", readl(S5P_MIPI_DPHY_CONTROL(1)));

	pr_err("CLKSRC_SEL_TOP_DISP1   [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_TOP_DISP1));
	pr_err("CLKSRC_SEL_DISP0       [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_DISP0));
	pr_err("CLKSRC_SEL_DISP1       [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_DISP1));
	pr_err("CLKSRC_SEL_DISP2       [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_DISP2));
	pr_err("CLKSRC_SEL_DISP3       [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_DISP3));
	pr_err("CLKSRC_SEL_DISP4       [0x%08x]\n", readl(EXYNOS5260_CLKSRC_SEL_DISP4));

	pr_err("CLKDIV_TOP_DISP        [0x%08x]\n", readl(EXYNOS5260_CLKDIV_TOP_DISP));
	pr_err("CLKDIV_DISP            [0x%08x]\n", readl(EXYNOS5260_CLKDIV_DISP));

	pr_err("CLKGATE_ACLK_DISP      [0x%08x]\n", readl(EXYNOS5260_CLKGATE_ACLK_DISP));
	pr_err("CLKGATE_PCLK_DISP      [0x%08x]\n", readl(EXYNOS5260_CLKGATE_PCLK_DISP));
	pr_err("CLKGATE_SCLK_DISP0     [0x%08x]\n", readl(EXYNOS5260_CLKGATE_SCLK_DISP0));
	pr_err("CLKGATE_SCLK_DISP1     [0x%08x]\n", readl(EXYNOS5260_CLKGATE_SCLK_DISP1));
	pr_err("CLKGATE_IP_DISP        [0x%08x]\n", readl(EXYNOS5260_CLKGATE_IP_DISP));
	pr_err("CLKGATE_IP_DISP_BUS    [0x%08x]\n", readl(EXYNOS5260_CLKGATE_IP_DISP_BUS));

	if (lcd->dsim) {
		if (lcd->dsim->enabled)
			dump_register(lcd->dsim->reg_base, S5P_PA_DSIM1, S5P_DSIM_MULTI_PKT);
		else
			dev_dbg(&lcd->ld->dev, "%s: dsim enable state is %d\n", __func__, lcd->dsim->enabled);
	}
}


static int s6e8ax0_write(struct lcd_info *lcd, const unsigned char *seq, int len)
{
	int ret;
	int retry;
	u8 cmd;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_LONG_WRITE;
	else if (len == 2)
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_DCS_SHORT_WRITE;
	else {
		ret = -EINVAL;
		goto write_err;
	}

	retry = 5;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		dsim_reg_dump(lcd);
		goto write_err;
	}
	ret = s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq, len);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}


static int s6e8ax0_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;
read_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}


static int init_backlight_level_from_brightness(struct lcd_info *lcd)
{
	int i, j, gamma;

	lcd->gamma_level = kzalloc((MAX_BRIGHTNESS+1) * sizeof(int), GFP_KERNEL); //0~255 + HBM
	if (!lcd->gamma_level) {
		pr_err("failed to allocate gamma_level table\n");
		return -1;
	}

	/* 0~19 */
	i = 0;
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 0
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 1
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 2
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 3
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 4
	lcd->gamma_level[i++] = IBRIGHTNESS_5NT; // 5
	lcd->gamma_level[i++] = IBRIGHTNESS_6NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_7NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_8NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_9NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_10NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_11NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_12NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_13NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_14NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_15NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_16NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_17NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_17NT; //17~18 : 17NT
	lcd->gamma_level[i++] = IBRIGHTNESS_19NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_20NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_21NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_22NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_22NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_24NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_25NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_27NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_27NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_29NT;
	lcd->gamma_level[i++] = IBRIGHTNESS_29NT; //29

	/* 255~30*/
	for(i = MAX_BRIGHTNESS; i >= LINEAR_MIN_GAMMA; i--) {
		gamma = ((i - LINEAR_MIN_GAMMA) * (300 - LINEAR_MIN_GAMMA) / (255 - LINEAR_MIN_GAMMA)) + LINEAR_MIN_GAMMA;
		for (j = IBRIGHTNESS_300NT; j >= 0; j--) {
			if (DIM_TABLE[j] < gamma)
				break;
			lcd->gamma_level[i] =j;
		}
	}

	return 0;

}




static int s6e8ax0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	unsigned char SEQ_PANEL_CONDITION_SET[AID_PARAM_SIZE] = {
		0xF8,
		0x3D, 0x35, 0x00, 0x00, 0x00, 0x94, 0x00, 0x3C, 0x7D, 0x10,
		0x27, 0x08, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x04
	};
	if(force || lcd->aor != aor_table[lcd->bl]) {
		SEQ_PANEL_CONDITION_SET[AID_PARAM_SIZE-1] = aor_table[lcd->bl];
		if (lcd->bl <= IBRIGHTNESS_234NT)
			SEQ_PANEL_CONDITION_SET[1] = 0x7D;
		s6e8ax0_write(lcd, SEQ_PANEL_CONDITION_SET, AID_PARAM_SIZE);
		lcd->aor = aor_table[lcd->bl];
		dev_info(&lcd->ld->dev,"%s aor = %x\n", __func__, lcd->aor);
	}
	return 0;
}


static int s6e8ax0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit;
	unsigned char SEQ_ELVSS[ELVSS_PARAM_SIZE] = {0xB1, 0x04, };

	nit = DIM_TABLE[lcd->bl];
	elvss_level = ELVSS_STATUS_300;
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= ELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	if(elvss_level == ELVSS_STATUS_HBM) {
		SEQ_ELVSS[2] = lcd->elvss_value[1];
		ret = s6e8ax0_write(lcd, SEQ_ELVSS, ELVSS_PARAM_SIZE);
		lcd->current_elvss = SEQ_ELVSS[2];
		dev_info(&lcd->ld->dev, "HBM elvss set : %d\n", SEQ_ELVSS[2]);
	} else {
		SEQ_ELVSS[2] = lcd->elvss_value[0] + ELVSS_TABLE[elvss_level][lcd->acl_enable];
		if((force) || (lcd->current_elvss != SEQ_ELVSS[2])) {
			ret = s6e8ax0_write(lcd, SEQ_ELVSS, ELVSS_PARAM_SIZE);
			lcd->current_elvss = SEQ_ELVSS[2];
			dev_info(&lcd->ld->dev, "elvss: %d, %d, %x\n", lcd->acl_enable, lcd->current_elvss,
				SEQ_ELVSS[2]);
		}
	}

	return ret;

}


#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G) //for rev00, 01
static int s6e8ax0_gamma_ctl_old(struct lcd_info *lcd)
{
	if((lcd->id[0] == 0xa2) && (lcd->id[1] == 0x60) && (lcd->id[2] == 0x90)) {
		s6e8ax0_write(lcd, SEQ_GAMMA_CONDITION_SET_D_A2_48, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET_D_A2_48));
		s6e8ax0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	}
	return 0;
}
#endif

static int s6e8ax0_gamma_ctl(struct lcd_info *lcd)
{
	s6e8ax0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	return 0;
}

static int s6e8ax0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;

	level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e8ax0_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;
#ifdef CONFIG_MACH_M2ALTE_CHN_CMCC /* set max brightness to 282nit when it is not autobrightness mode */
	if (unlikely(!lcd->auto_brightness && brightness > 235))
			brightness = 235;
#endif

	lcd->bl = lcd->gamma_level[brightness];

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == lcd->bd->props.max_brightness))
		lcd->bl = IBRIGHTNESS_HBM;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
		if((lcd->id[0] == 0xa2) && (lcd->id[1] == 0x60) && (lcd->id[2] == 0x90)) {
			s6e8ax0_gamma_ctl_old(lcd);
			lcd->current_bl = lcd->bl;
			dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", brightness, lcd->bl, DIM_TABLE[lcd->bl]);
			mutex_unlock(&lcd->bl_lock);
			return 0;
		}
#endif

		s6e8ax0_write(lcd, SEQ_APPLY_TESTKEY_ENABLE, ARRAY_SIZE(SEQ_APPLY_TESTKEY_ENABLE));
		s6e8ax0_gamma_ctl(lcd);
		s6e8ax0_aid_parameter_ctl(lcd, force);
		s6e8ax0_set_elvss(lcd, force);

		/* Gamma Set Update */
		s6e8ax0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

		s6e8ax0_set_acl(lcd, force);
		s6e8ax0_write(lcd, SEQ_APPLY_TESTKEY_DISABLE, ARRAY_SIZE(SEQ_APPLY_TESTKEY_DISABLE));

		lcd->current_bl = lcd->bl;
		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", brightness, lcd->bl, DIM_TABLE[lcd->bl]);
	}
	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e8ax0_ldi_init_evt0(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "%s\n", __func__);

	s6e8ax0_write(lcd, SEQ_PANEL_CONDITION_SET_A00, ARRAY_SIZE(SEQ_PANEL_CONDITION_SET_A00));
	s6e8ax0_write(lcd, SEQ_DISPLAY_CONDITION_SET_A00, ARRAY_SIZE(SEQ_DISPLAY_CONDITION_SET_A00));
	s6e8ax0_gamma_ctl_old(lcd);
	s6e8ax0_write(lcd, SEQ_ETC_SOURCE_CONTROL_A00, ARRAY_SIZE(SEQ_ETC_SOURCE_CONTROL_A00));
	s6e8ax0_write(lcd, SEQ_ETC_PENTILE_CONTROL, ARRAY_SIZE(SEQ_ETC_PENTILE_CONTROL));
	s6e8ax0_write(lcd, SEQ_ETC_NVM_SETTING_A00, ARRAY_SIZE(SEQ_ETC_NVM_SETTING_A00));
	s6e8ax0_write(lcd, SEQ_ETC_POWER_CONTROL, ARRAY_SIZE(SEQ_ETC_POWER_CONTROL));
	s6e8ax0_write(lcd, SEQ_ELVSS_CONTROL, ARRAY_SIZE(SEQ_ELVSS_CONTROL));
	return 0;

}
static int s6e8ax0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;
	dev_info(&lcd->ld->dev, "%s\n", __func__);

	s6e8ax0_write(lcd, SEQ_APPLY_TESTKEY_ENABLE, ARRAY_SIZE(SEQ_APPLY_TESTKEY_ENABLE));
	s6e8ax0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(22);

	lcd->connected = 1;
	s6e8ax0_read_id(lcd, lcd->id);
	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	/* 4.8" HD for M2 rev01*/
	if (lcd->id[0] == 0xa2 && lcd->id[1] == 0x60 && lcd->id[2] == 0x90) {
		ret = s6e8ax0_ldi_init_evt0(lcd);
		return ret;
	}
#endif

	s6e8ax0_write(lcd, SEQ_ETC_PENTILE_CONTROL, ARRAY_SIZE(SEQ_ETC_PENTILE_CONTROL));
	s6e8ax0_write(lcd, SEQ_ETC_POWER_CONTROL, ARRAY_SIZE(SEQ_ETC_POWER_CONTROL));
	s6e8ax0_write(lcd, SEQ_ACL_SET, ARRAY_SIZE(SEQ_ACL_SET));
#if 0
	//brightness control
	s6e8ax0_gamma_ctl(lcd);
	s6e8ax0_write(lcd, SEQ_PANEL_CONDITION_SET, ARRAY_SIZE(SEQ_PANEL_CONDITION_SET));
	//elvss setting
	//	s6e8ax0_set_elvss();
#else
	update_brightness(lcd, 1);
#endif
	s6e8ax0_write(lcd, SEQ_APPLY_TESTKEY_DISABLE, ARRAY_SIZE(SEQ_APPLY_TESTKEY_DISABLE));

	return ret;
}

static int s6e8ax0_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e8ax0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e8ax0_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e8ax0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	s6e8ax0_write(lcd, SEQ_STANDBY_ON, ARRAY_SIZE(SEQ_STANDBY_ON));

	return ret;
}

static int s6e8ax0_power_on(struct lcd_info *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	pd = lcd->lcd_pd;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = s6e8ax0_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	msleep(120);

	ret = s6e8ax0_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;
	//update_brightness(lcd, 1);

err:
	return ret;
}

static int s6e8ax0_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);
	lcd->ldi_enable = 0;

	ret = s6e8ax0_ldi_disable(lcd);

	msleep(135);

	return ret;
}

static int s6e8ax0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s, power = %d, lcd->power = %d\n", __func__, power, lcd->power);

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e8ax0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e8ax0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e8ax0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e8ax0_power(lcd, power);
}

static int s6e8ax0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e8ax0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6e8ax0_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(lcd->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int s6e8ax0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static struct lcd_ops s6e8ax0_lcd_ops = {
	.set_power = s6e8ax0_set_power,
	.get_power = s6e8ax0_get_power,
	.check_fb  = s6e8ax0_check_fb,
};

static const struct backlight_ops s6e8ax0_backlight_ops  = {
	.get_brightness = s6e8ax0_get_brightness,
	.update_status = s6e8ax0_set_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[15];
	struct lcd_info *lcd = dev_get_drvdata(dev);
	sprintf(temp, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}
#if 0
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->elvss_table[i][j]);
		printk("\n");
	}
#endif
	return strlen(buf);
}
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);

static int s6e8ax0_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;
	//init_lcd(dsim);

	s6e8ax0_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}
static int s6e8ax0_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e8ax0_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e8ax0_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

static void s6e8ax0_read_coordinate(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e8ax0_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[3] << 8 | buf[4];	/* X */
	lcd->coordinate[1] = buf[5] << 8 | buf[6];	/* Y */
}
static int s6e8ax0_read_date(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	ret = s6e8ax0_read(lcd, LDI_DATE_REG, buf, LDI_DATE_LEN);

	return ret;
}


static void s6e8ax0_read_elvsshbm(struct lcd_info *lcd, u8 *elvsshbm_data)
{
	int ret = 0, i =0;

	ret = s6e8ax0_read(lcd, LDI_HBMELVSS_REG, elvsshbm_data, LDI_HBMELVSS_LEN);
	if (!ret) {
		dev_info(&lcd->ld->dev, "%s: elvss read fail\n", __func__);
	}
	for(i = 0; i < LDI_HBMELVSS_LEN; i++)
		smtd_dbg("elvsshbm_data[%d] = %d\n", i, elvsshbm_data[i]);

}

static void s6e8ax0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;

	ret = s6e8ax0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int s6e8ax0_read_mtp(struct lcd_info *lcd, u8 *mtp_data)
{
	int ret = 0;
	int i = 0;

	ret = s6e8ax0_read(lcd, LDI_MTP_ADDR, mtp_data, LDI_MTP_LENGTH);

	for(i = 0; i<LDI_MTP_LENGTH; i++)
		smtd_dbg("mtp_data[%d] = %d\n", i, mtp_data[i]);

	return ret;
}

static void init_hbm_parameter(struct lcd_info *lcd, u8 *elvsshbm_data)
{
	int i, j, c, v;

	/* relocate HBM data */
	j = 2;
	//V1 RGB
	for(c = 0; c < CI_MAX; c++)
		lcd->gamma_table[IBRIGHTNESS_HBM][j++] =0x0;
	//V15 RGB
	for(c = 0; c < CI_MAX; c++)
		lcd->gamma_table[IBRIGHTNESS_HBM][j++] =0x80;
	//V35~V171 RGB
	for(v = 2, i = 2; v < IV_MAX-1; v++, i++) {
		for(c = 0; c < CI_MAX; c++)
			lcd->gamma_table[IBRIGHTNESS_HBM][j++] = elvsshbm_data[i+(8*c)];
	}
	//V255 RGB
	for(c = 0; c < CI_MAX; c++) {
		lcd->gamma_table[IBRIGHTNESS_HBM][j++] = elvsshbm_data[i+(8*c)];
		lcd->gamma_table[IBRIGHTNESS_HBM][j++] = elvsshbm_data[i+1+(8*c)];
	}

	//set elvss value
	lcd->elvss_value[0] = elvsshbm_data[0]; //to set elvss
	lcd->elvss_value[1] = elvsshbm_data[1]; // to set hbm elvss


}
static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX-1; //except hbm
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;

	lcd->daid.volt_table_v35 = volt_table_v35;
	lcd->daid.volt_table_v15 = volt_table_v15;
}
static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;


	for (c = 0, j = 0; c < CI_MAX ; c++, j++) {
		if (mtp_data[(IV_MAX-1)*(c+1)+j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[(IV_MAX-1)*(c+1)+j] | ~0xff;
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[(IV_MAX-1)*(c+1)+j];
	}

	for (i = 0; i < IV_MAX - 1; i++) {
		for (c=0; c<CI_MAX ; c++, j++) {
			if (mtp_data[(IV_MAX+1)*c+i] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[(IV_MAX+1)*c+i] & 0x7F) | ~0x7f;
			else
				mtp[CI_MAX*i+c] = mtp_data[(IV_MAX+1)*c+i];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%d] = %d\n", j, mtp[j]);

	for (i = 0, j = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%04d ", mtp[j]);
		smtd_dbg("\n");
	}
}

static int init_gamma_table(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xFA;
		lcd->gamma_table[i][1] = 0x01;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	pgamma = &gamma[0][0];

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX-1; i++) {
		pgamma = &gamma[i][0];
		j = 2;

		/* for V15, 35, 59, 87, 171, 255*/
		for(v = 0; v < IV_MAX-1; v++) {
			for(c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}
		/* for V255 */
		for (c = 0; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}


static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d, %d\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, manufacture_data[LDI_DATE_LEN] = {0,};

	if (lcd->ldi_enable)
		s6e8ax0_read_date(lcd, manufacture_data);

	year = ((manufacture_data[8] & 0xF0) >> 4) + 2011;
	month = manufacture_data[8] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, manufacture_data[9]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];

	sprintf(temp, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);
	return strlen(buf);
}


static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);


static int s6e8ax0_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;
	u8 mtp_data[LDI_MTP_LENGTH] = {0,};
	u8 elvsshbm_data[LDI_HBMELVSS_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e8ax0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e8ax0_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = IBRIGHTNESS_162NT;
	lcd->current_bl = lcd->bl;

	lcd->acl_enable = 0;
	lcd->siop_enable = 0;
	lcd->current_acl = 0;
	lcd->current_elvss = 0;

	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 1;
	lcd->connected = 1;
	lcd->auto_brightness = 0;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e8ax0_read_id(lcd, lcd->id);
	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	ret = s6e8ax0_read_mtp(lcd, mtp_data);
	if (!ret) {
		printk(KERN_ERR "[LCD:ERROR] : %s read mtp failed\n", __func__);
		/*return -EPERM;*/
	}

	ret = init_backlight_level_from_brightness(lcd);
	if(ret < 0)
		dev_info(&lcd->ld->dev, "gamma level generation is failed\n");

	init_dynamic_aid(lcd);
	ret = init_gamma_table(lcd, mtp_data);

	s6e8ax0_read_elvsshbm(lcd, elvsshbm_data);
	init_hbm_parameter(lcd, elvsshbm_data);
	dev_info(&lcd->ld->dev, "elvss : %d, %d\n", lcd->elvss_value[0], lcd->elvss_value[1]);
	s6e8ax0_read_coordinate(lcd);

//	ret += init_aid_dimming_table(lcd);
//	show_lcd_table(lcd);

	update_brightness(lcd, 1);

	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}



struct mipi_dsim_lcd_driver s6e8aa0a02_mipi_lcd_driver = {
	.probe		= s6e8ax0_probe,
	.displayon	= s6e8ax0_displayon,
	.suspend	= s6e8ax0_suspend,
	.resume		= s6e8ax0_resume,
};


static int s6e8ax0_init(void)
{
	return 0; //s5p_dsim_register_lcd_driver(&s6e8ax0_mipi_driver);
}

static void s6e8ax0_exit(void)
{
	return;
}
module_init(s6e8ax0_init);
module_exit(s6e8ax0_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E8AA0A02:AMS480GYXX (720x1280) Panel Driver");
MODULE_LICENSE("GPL");

