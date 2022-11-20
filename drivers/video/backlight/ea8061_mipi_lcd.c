/* linux/drivers/video/backlight/ea8061_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
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
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/gpio.h>

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-mipidsim.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/regs-pmu.h>

#include "../exynos_display_handler.h"

#include "ea8061_param.h"

#include "dynamic_aid_ea8061.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		162
#define DEFAULT_GAMMA_LEVEL		IBRIGHTNESS_162NT

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)

#define LDI_ID_REG			0xD1
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xDA
#define LDI_MTP_LEN			33	/* MTP 1~32th + Dummy because VT G/R share 31th para */
#define LDI_ELVSS_REG			0xB2
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)
#define LDI_HBM_REG			0xDB
#define LDI_HBM_LEN			34	/* HBM V35 ~ V255 + HBM ELVSS for D4h */

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#define LDI_DATE_REG			0xC8
#define LDI_DATE_LEN			42

#define LDI_MPS_REG			0xD4
#define LDI_MPS_LEN			18
#define MPS_PARAM_SIZE		(LDI_MPS_LEN + 1)	/* REG + 18 para */

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			current_temperature;
	unsigned int			current_hbm;
	unsigned int			current_mps;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[2];
	unsigned char			*mps_table[HBM_STATUS_MAX][MPS_STATUS_MAX];

	struct dynamic_aid_param_t	daid;
	unsigned char			aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AID_SET)];
	unsigned int			connected;

	unsigned char			**tset_table;
	int				temperature;

	unsigned int			coordinate[2];

	struct mipi_dsim_device		*dsim;
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


static int ea8061_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret = 0;
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


static int ea8061_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;
	unsigned char wbuf[] = {0xFD, 0x00, 0x00};

	if (!lcd->connected)
		return -EINVAL;

	/* set_read_address */
	wbuf[1] = addr;
	ea8061_write(lcd, wbuf, ARRAY_SIZE(wbuf));

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
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, 0xFE, len, buf, 0);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void ea8061_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;

	ret = ea8061_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int ea8061_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_MTP_REG);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static void ea8061_read_coordinate(struct lcd_info *lcd)
{
	int ret = 0;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = ea8061_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
}

static int ea8061_read_hbm(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061_read(lcd, LDI_HBM_REG, buf, LDI_HBM_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_HBM_REG);
	for (i = 0; i < LDI_HBM_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int ea8061_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int ea8061_read_mps(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = ea8061_read(lcd, LDI_MPS_REG, buf, LDI_MPS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_MPS_REG);
	for (i = 0; i < LDI_MPS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int ea8061_read_date(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = ea8061_read(lcd, LDI_DATE_REG, buf, LDI_DATE_LEN);

	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0 ... 5:
		backlightlevel = IBRIGHTNESS_5NT;
		break;
	case 6:
		backlightlevel = IBRIGHTNESS_6NT;
		break;
	case 7:
		backlightlevel = IBRIGHTNESS_7NT;
		break;
	case 8:
		backlightlevel = IBRIGHTNESS_8NT;
		break;
	case 9:
		backlightlevel = IBRIGHTNESS_9NT;
		break;
	case 10:
		backlightlevel = IBRIGHTNESS_10NT;
		break;
	case 11:
		backlightlevel = IBRIGHTNESS_11NT;
		break;
	case 12:
		backlightlevel = IBRIGHTNESS_12NT;
		break;
	case 13:
		backlightlevel = IBRIGHTNESS_13NT;
		break;
	case 14:
		backlightlevel = IBRIGHTNESS_14NT;
		break;
	case 15:
		backlightlevel = IBRIGHTNESS_15NT;
		break;
	case 16:
		backlightlevel = IBRIGHTNESS_16NT;
		break;
	case 17 ... 18:
		backlightlevel = IBRIGHTNESS_17NT;
		break;
	case 19:
		backlightlevel = IBRIGHTNESS_19NT;
		break;
	case 20:
		backlightlevel = IBRIGHTNESS_20NT;
		break;
	case 21:
		backlightlevel = IBRIGHTNESS_21NT;
		break;
	case 22 ... 23:
		backlightlevel = IBRIGHTNESS_22NT;
		break;
	case 24:
		backlightlevel = IBRIGHTNESS_24NT;
		break;
	case 25 ... 26:
		backlightlevel = IBRIGHTNESS_25NT;
		break;
	case 27 ... 28:
		backlightlevel = IBRIGHTNESS_27NT;
		break;
	case 29:
		backlightlevel = IBRIGHTNESS_29NT;
		break;
	case 30 ... 31:
		backlightlevel = IBRIGHTNESS_30NT;
		break;
	case 32 ... 33:
		backlightlevel = IBRIGHTNESS_32NT;
		break;
	case 34 ... 36:
		backlightlevel = IBRIGHTNESS_34NT;
		break;
	case 37 ... 38:
		backlightlevel = IBRIGHTNESS_37NT;
		break;
	case 39 ... 40:
		backlightlevel = IBRIGHTNESS_39NT;
		break;
	case 41 ... 43:
		backlightlevel = IBRIGHTNESS_41NT;
		break;
	case 44 ... 46:
		backlightlevel = IBRIGHTNESS_44NT;
		break;
	case 47 ... 49:
		backlightlevel = IBRIGHTNESS_47NT;
		break;
	case 50 ... 52:
		backlightlevel = IBRIGHTNESS_50NT;
		break;
	case 53 ... 55:
		backlightlevel = IBRIGHTNESS_53NT;
		break;
	case 56 ... 59:
		backlightlevel = IBRIGHTNESS_56NT;
		break;
	case 60 ... 63:
		backlightlevel = IBRIGHTNESS_60NT;
		break;
	case 64 ... 67:
		backlightlevel = IBRIGHTNESS_64NT;
		break;
	case 68 ... 71:
		backlightlevel = IBRIGHTNESS_68NT;
		break;
	case 72 ... 76:
		backlightlevel = IBRIGHTNESS_72NT;
		break;
	case 77 ... 81:
		backlightlevel = IBRIGHTNESS_77NT;
		break;
	case 82 ... 86:
		backlightlevel = IBRIGHTNESS_82NT;
		break;
	case 87 ... 92:
		backlightlevel = IBRIGHTNESS_87NT;
		break;
	case 93 ... 97:
		backlightlevel = IBRIGHTNESS_93NT;
		break;
	case 98 ... 104:
		backlightlevel = IBRIGHTNESS_98NT;
		break;
	case 105:
		backlightlevel = IBRIGHTNESS_105NT;
		break;
	case 106:
		backlightlevel = IBRIGHTNESS_106NT;
		break;
	case 107:
		backlightlevel = IBRIGHTNESS_107NT;
		break;
	case 108:
		backlightlevel = IBRIGHTNESS_108NT;
		break;
	case 109:
		backlightlevel = IBRIGHTNESS_109NT;
		break;
	case 110:
		backlightlevel = IBRIGHTNESS_110NT;
		break;
	case 111 ... 118:
		backlightlevel = IBRIGHTNESS_111NT;
		break;
	case 119 ... 125:
		backlightlevel = IBRIGHTNESS_119NT;
		break;
	case 126 ... 133:
		backlightlevel = IBRIGHTNESS_126NT;
		break;
	case 134 ... 142:
		backlightlevel = IBRIGHTNESS_134NT;
		break;
	case 143 ... 149:
		backlightlevel = IBRIGHTNESS_143NT;
		break;
	case 150 ... 161:
		backlightlevel = IBRIGHTNESS_152NT;
		break;
	case 162 ... 171:
		backlightlevel = IBRIGHTNESS_162NT;
		break;
	case 172 ... 172:
		backlightlevel = IBRIGHTNESS_172NT;
		break;
	case 173 ... 174:
		backlightlevel = IBRIGHTNESS_173NT;
		break;
	case 175 ... 176:
		backlightlevel = IBRIGHTNESS_175NT;
		break;
	case 177 ... 178:
		backlightlevel = IBRIGHTNESS_177NT;
		break;
	case 179 ... 180:
		backlightlevel = IBRIGHTNESS_179NT;
		break;
	case 181 ... 182:
		backlightlevel = IBRIGHTNESS_181NT;
		break;
	case 183 ... 193:
		backlightlevel = IBRIGHTNESS_183NT;
		break;
	case 194 ... 205:
		backlightlevel = IBRIGHTNESS_195NT;
		break;
	case 206 ... 218:
		backlightlevel = IBRIGHTNESS_207NT;
		break;
	case 219 ... 229:
		backlightlevel = IBRIGHTNESS_220NT;
		break;
	case 230 ... 237:
		backlightlevel = IBRIGHTNESS_234NT;
		break;
	case 238 ... 241:
		backlightlevel = IBRIGHTNESS_249NT;
		break;
	case 242 ... 244:
		backlightlevel = IBRIGHTNESS_265NT;
		break;
	case 245 ... 247:
		backlightlevel = IBRIGHTNESS_282NT;
		break;
	case 248 ... 249:
		backlightlevel = IBRIGHTNESS_300NT;
		break;
	case 250 ... 251:
		backlightlevel = IBRIGHTNESS_316NT;
		break;
	case 252 ... 253:
		backlightlevel = IBRIGHTNESS_333NT;
		break;
	case 254 ... 255:
		backlightlevel = IBRIGHTNESS_350NT;
		break;
	default:
		backlightlevel = IBRIGHTNESS_162NT;
		break;
	}

	return backlightlevel;
}


static int ea8061_gamma_ctl(struct lcd_info *lcd)
{
	ea8061_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int ea8061_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] != lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] != lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else
		goto exit;

aid_update:
	ea8061_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	return 0;
}

static int ea8061_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level;

	level = ACL_STATUS_25P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = ea8061_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int ea8061_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0, elvss;
	u32 candela = index_brightness_table[lcd->bl];
	int smart_acl = lcd->current_acl ? 1 : 0;

	switch (candela) {
	case 0 ... 105:
		elvss_level = ELVSS_STATUS_105;
		break;
	case 106:
		elvss_level = ELVSS_STATUS_106;
		break;
	case 107:
		elvss_level = ELVSS_STATUS_107;
		break;
	case 108 ... 109:
		elvss_level = ELVSS_STATUS_109;
		break;
	case 110 ... 111:
		elvss_level = ELVSS_STATUS_111;
		break;
	case 112 ... 126:
		elvss_level = ELVSS_STATUS_126;
		break;
	case 127 ... 134:
		elvss_level = ELVSS_STATUS_134;
		break;
	case 135 ... 143:
		elvss_level = ELVSS_STATUS_143;
		break;
	case 144 ... 152:
		elvss_level = ELVSS_STATUS_152;
		break;
	case 153 ... 162:
		elvss_level = ELVSS_STATUS_162;
		break;
	case 163 ... 173:
		elvss_level = ELVSS_STATUS_173;
		break;
	case 174 ... 175:
		elvss_level = ELVSS_STATUS_175;
		break;
	case 176 ... 177:
		elvss_level = ELVSS_STATUS_177;
		break;
	case 178 ... 179:
		elvss_level = ELVSS_STATUS_179;
		break;
	case 180 ... 181:
		elvss_level = ELVSS_STATUS_181;
		break;
	case 182 ... 183:
		elvss_level = ELVSS_STATUS_183;
		break;
	case 184 ... 195:
		elvss_level = ELVSS_STATUS_195;
		break;
	case 196 ... 207:
		elvss_level = ELVSS_STATUS_207;
		break;
	case 208 ... 220:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 221 ... 234:
		elvss_level = ELVSS_STATUS_234;
		break;
	case 235 ... 249:
		elvss_level = ELVSS_STATUS_249;
		break;
	case 250 ... 265:
		elvss_level = ELVSS_STATUS_265;
		break;
	case 266 ... 282:
		elvss_level = ELVSS_STATUS_282;
		break;
	case 283 ... 300:
		elvss_level = ELVSS_STATUS_300;
		break;
	case 301 ... 316:
		elvss_level = ELVSS_STATUS_316;
		break;
	case 317 ... 333:
		elvss_level = ELVSS_STATUS_333;
		break;
	case 334 ... 350:
		elvss_level = ELVSS_STATUS_350;
		break;
	case 500:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_350;
		break;
	}

	elvss = lcd->elvss_table[smart_acl][elvss_level][1];

	if (force || lcd->elvss_table[smart_acl][lcd->current_elvss][1] != elvss || lcd->current_temperature != lcd->temperature) {
		if (lcd->temperature < 0)
			lcd->elvss_table[smart_acl][elvss_level][7] = (unsigned char)((-1) * (char)lcd->temperature) | (1<<7);
		else
			lcd->elvss_table[smart_acl][elvss_level][7] = (unsigned char)lcd->temperature;

		ret = ea8061_write(lcd, lcd->elvss_table[smart_acl][elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = elvss_level;
		lcd->current_temperature = lcd->temperature;

		dev_dbg(&lcd->ld->dev, "elvss: %d, %d, %x\n", smart_acl, lcd->current_elvss,
			lcd->elvss_table[smart_acl][lcd->current_elvss][1]);
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int ea8061_set_mps(struct lcd_info *lcd, u8 force)
{
	int ret = 0;
	int mps = (lcd->temperature > 0) ? 0 : 1;
	int hbm = LEVEL_IS_HBM(lcd->auto_brightness);

	if (force || lcd->current_hbm != hbm || lcd->current_mps != mps) {
		ret += ea8061_write(lcd, SEQ_MTP_KEY_UNLOCK, sizeof(SEQ_MTP_KEY_UNLOCK));
		ret += ea8061_write(lcd, lcd->mps_table[hbm][mps], MPS_PARAM_SIZE);
		ret += ea8061_write(lcd, SEQ_MTP_KEY_LOCK, sizeof(SEQ_MTP_KEY_LOCK));
		lcd->current_mps = mps;
		lcd->current_hbm = hbm;

		dev_info(&lcd->ld->dev, "%s: hbm:%d mps:%d\n", __func__, hbm, mps);
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

static void ea8061_set_enable_hsync(struct lcd_info *lcd)
{
	ea8061_write(lcd, SEQ_MTP_KEY_UNLOCK, ARRAY_SIZE(SEQ_MTP_KEY_UNLOCK));
	ea8061_write(lcd, SEQ_LEVEL_3_KEY_UNLOCK, ARRAY_SIZE(SEQ_LEVEL_3_KEY_UNLOCK));
	ea8061_write(lcd, SEQ_HSYNC_OUT_1, ARRAY_SIZE(SEQ_HSYNC_OUT_1));
	ea8061_write(lcd, SEQ_HSYNC_OUT_2, ARRAY_SIZE(SEQ_HSYNC_OUT_2));
	ea8061_write(lcd, SEQ_HSYNC_OUT_3, ARRAY_SIZE(SEQ_HSYNC_OUT_3));
	ea8061_write(lcd, SEQ_MTP_KEY_LOCK, ARRAY_SIZE(SEQ_MTP_KEY_LOCK));
	ea8061_write(lcd, SEQ_LEVEL_3_KEY_LOCK, ARRAY_SIZE(SEQ_LEVEL_3_KEY_LOCK));
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
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

static void init_mtp_data(struct lcd_info *lcd, u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;

	mtp_data[32] = mtp_data[31];			/* VT B */
	mtp_data[31] = (mtp_data[30] >> 4) & 0xF;		/* VT G */
	mtp_data[30] = (mtp_data[30] & 0xF);		/* VT R */

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = (256-mtp_data[j]) * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_203; i >= 0; i--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (128-(mtp_data[j]&0x7F)) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j]&0x7F;
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
	if (IS_ERR_OR_NULL(gamma)) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (IS_ERR_OR_NULL(gamma[i])) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lcd->gamma_table)) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->gamma_table[i])) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		lcd->gamma_table[i][31] = 0;	/* VT: Green / Red */
		lcd->gamma_table[i][32] = 0;	/* VT: Blue */
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

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	int i;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memcpy(lcd->aor[i], SEQ_AID_SET, AID_PARAM_SIZE);
		lcd->aor[i][3] = aor_cmd[i][0];
		lcd->aor[i][4] = aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, k, ret;

	for (k = 0; k < 2; k++) {
		lcd->elvss_table[k] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

		if (IS_ERR_OR_NULL(lcd->elvss_table[k])) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			lcd->elvss_table[k][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (IS_ERR_OR_NULL(lcd->elvss_table[k][i])) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			memcpy(lcd->elvss_table[k][i], SEQ_ELVSS_SET, ELVSS_PARAM_SIZE);

			lcd->elvss_table[k][i][1] = ELVSS_TABLE[i];
		}

	}

	/* this (elvss_table[1]) is Smart ACL ON*/
	for (i = 0; i < ELVSS_STATUS_MAX; i++)
		lcd->elvss_table[1][i][1] += 4;

	return 0;

err_alloc_elvss:
	while (i > 0) {
		kfree(lcd->elvss_table[k][i-1]);
		i--;
	}
	kfree(lcd->elvss_table[k]);
err_alloc_elvss_table:
	return ret;
}

static int init_mps_table(struct lcd_info *lcd, u8 *mps_data)
{
	int i, j, ret;

	for (j = 0; j < HBM_STATUS_MAX; j++) {
		for (i = 0; i < MPS_STATUS_MAX; i++) {
			lcd->mps_table[j][i] = kzalloc(MPS_PARAM_SIZE * sizeof(u8 *), GFP_KERNEL);

			if (IS_ERR_OR_NULL(lcd->mps_table[j][i])) {
				pr_err("failed to allocate mps table\n");
				ret = -ENOMEM;
				goto err_alloc_mps;
			}

			lcd->mps_table[j][i][0] = LDI_MPS_REG;
			memcpy(&lcd->mps_table[j][i][1], mps_data, LDI_MPS_LEN);
		}
	}

	lcd->mps_table[HBM_ON][MPS_ON][3] = 0x4C;
	lcd->mps_table[HBM_OFF][MPS_ON][3] = 0x4C;
	lcd->mps_table[HBM_ON][MPS_OFF][3] = 0x48;
	lcd->mps_table[HBM_OFF][MPS_OFF][3] = 0x48;

	return 0;

err_alloc_mps:
	while (i > 0) {
		kfree(lcd->mps_table[j][i-1]);
		i--;
	}
	if (j > 0) {
		j--;
		i = HBM_STATUS_MAX;
		goto err_alloc_mps;
	}
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd, const u8 *hbm_data)
{
	int i;

	/* CA 1~21th = DB 1~21 */
	for (i = 0; i < 21; i++)
		lcd->gamma_table[IBRIGHTNESS_500NT][i+1] = hbm_data[i];

	/* D4 18th = DB 34 */
	lcd->mps_table[HBM_ON][MPS_ON][18] = hbm_data[33];
	lcd->mps_table[HBM_ON][MPS_OFF][18] = hbm_data[33];

	lcd->elvss_table[1][ELVSS_STATUS_HBM][1] = lcd->elvss_table[0][ELVSS_STATUS_HBM][1];

	return 0;
}

static void show_lcd_table(struct lcd_info *lcd)
{
	int i, j, k;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < AID_PARAM_SIZE; j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd->elvss_table[0][i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd->elvss_table[1][i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");


	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			for (k = 0; k < MPS_PARAM_SIZE; k++)
				smtd_dbg("%02X, ", lcd->mps_table[i][j][k]);
			smtd_dbg("\n");
		}
	}
	smtd_dbg("\n");
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == lcd->bd->props.max_brightness))
		lcd->bl = IBRIGHTNESS_500NT;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		ea8061_write(lcd, SEQ_LEVEL_2_KEY_UNLOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_UNLOCK));

		ea8061_write(lcd, SEQ_GAMMA_UPDATE_OFF, ARRAY_SIZE(SEQ_GAMMA_UPDATE_OFF));

		ea8061_gamma_ctl(lcd);

		ea8061_aid_parameter_ctl(lcd, force);

		ea8061_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));

		ea8061_set_elvss(lcd, force);

		ea8061_set_mps(lcd, force);

		ea8061_set_acl(lcd, force);

		ea8061_write(lcd, SEQ_LEVEL_2_KEY_LOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_LOCK));

		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", \
			brightness, lcd->bl, index_brightness_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int ea8061_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	ea8061_write(lcd, SEQ_LEVEL_2_KEY_UNLOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_UNLOCK));
	ea8061_write(lcd, SEQ_GAMMA_UPDATE_OFF, ARRAY_SIZE(SEQ_GAMMA_UPDATE_OFF));
	ea8061_write(lcd, SEQ_PANEL_CONDITION_SET, ARRAY_SIZE(SEQ_PANEL_CONDITION_SET));
	ea8061_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	ea8061_write(lcd, SEQ_SCAN_DIRECTION, ARRAY_SIZE(SEQ_SCAN_DIRECTION));
	update_brightness(lcd, 1);
	ea8061_write(lcd, SEQ_SLEW_CONTROL, ARRAY_SIZE(SEQ_SLEW_CONTROL));
#if 0
	ea8061_write(lcd, SEQ_GAMMA_UPDATE_OFF, ARRAY_SIZE(SEQ_GAMMA_UPDATE_OFF));
	ea8061_write(lcd, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	ea8061_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	ea8061_write(lcd, SEQ_AID_SET, ARRAY_SIZE(SEQ_AID_SET));
	ea8061_write(lcd, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	ea8061_write(lcd, SEQ_ACL_SET, ARRAY_SIZE(SEQ_ACL_SET));
#endif
	ea8061_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	return ret;
}

static int ea8061_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	ea8061_set_enable_hsync(lcd);
	ea8061_write(lcd, SEQ_LEVEL_2_KEY_LOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_LOCK));
	ea8061_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int ea8061_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	ea8061_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	msleep(35);

	ea8061_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	msleep(100);

	dev_info(&lcd->ld->dev, "-%s\n", __func__);

	return ret;
}

static int ea8061_power_on(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	ret = ea8061_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	msleep(120);

	ret = ea8061_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

	update_brightness(lcd, 1);

	dev_info(&lcd->ld->dev, "-%s\n", __func__);
err:
	return ret;
}

static int ea8061_power_off(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	lcd->ldi_enable = 0;

	ret = ea8061_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "-%s\n", __func__);

	return ret;
}

static int ea8061_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = ea8061_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = ea8061_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int ea8061_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, power);

	return exynos_display_notifier_call_chain(power, NULL);
}

static int ea8061_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int ea8061_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness); */

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness, brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int ea8061_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return index_brightness_table[lcd->bl];
}

static int ea8061_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops ea8061_lcd_ops = {
	.set_power = ea8061_set_power,
	.get_power = ea8061_get_power,
	.check_fb = ea8061_check_fb,
};

static const struct backlight_ops ea8061_backlight_ops = {
	.get_brightness = ea8061_get_brightness,
	.update_status = ea8061_set_brightness,
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

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "SDC_AMS549BU01\n";

	strcat(buf, temp);
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

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0);
		}
	}
	return size;
}

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

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc;

	rc = kstrtoint(buf, 10, &value);

	dev_info(dev, "%s: set %d\n", __func__, value);

	if (rc < 0)
		return rc;

	if (value > 0)
		value = 1;
	else if (value >= -19)
		value = -19;
	else
		value = -20;

	if (lcd->temperature != value) {
		mutex_lock(&lcd->bl_lock);
		lcd->temperature = value;
		mutex_unlock(&lcd->bl_lock);
		if (lcd->ldi_enable)
			update_brightness(lcd, 1);
	}

	return size;
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
		ea8061_read_date(lcd, manufacture_data);

	year = ((manufacture_data[40] & 0xF0) >> 4) + 2011;
	month = manufacture_data[40] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, manufacture_data[41]);

	return strlen(buf);
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);

static int ea8061_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;
	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};
	u8 hbm_data[LDI_HBM_LEN] = {0,};
	u8 mps_data[LDI_MPS_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &ea8061_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &ea8061_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 1;
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;
	lcd->temperature = 1;
	lcd->current_temperature = 1;
	lcd->current_mps = 0;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	ea8061_write(lcd, SEQ_LEVEL_2_KEY_UNLOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_UNLOCK));
	ea8061_read_id(lcd, lcd->id);
	ea8061_read_mtp(lcd, mtp_data);
	ea8061_read_elvss(lcd, elvss_data);
	ea8061_read_coordinate(lcd);
	ea8061_read_hbm(lcd, hbm_data);
	ea8061_read_mps(lcd, mps_data);
	ea8061_write(lcd, SEQ_LEVEL_2_KEY_LOCK, ARRAY_SIZE(SEQ_LEVEL_2_KEY_LOCK));

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_mps_table(lcd, mps_data);
	ret += init_hbm_parameter(lcd, hbm_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	update_brightness(lcd, 1);

	show_lcd_table(lcd);

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", __FILE__);

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


static int ea8061_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	ea8061_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int ea8061_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	ea8061_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int ea8061_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver d6ea8061_mipi_lcd_driver = {
	.probe		= ea8061_probe,
	.displayon	= ea8061_displayon,
	.suspend	= ea8061_suspend,
	.resume		= ea8061_resume,
};

static int ea8061_init(void)
{
	return 0;
}

static void ea8061_exit(void)
{
	return;
}

module_init(ea8061_init);
module_exit(ea8061_exit);


MODULE_DESCRIPTION("MIPI-DSI S6E8FA0 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");
