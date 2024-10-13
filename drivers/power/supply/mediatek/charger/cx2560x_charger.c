/*
 * CX2560X battery charging driver
 *
 * Copyright (C) 2022 SGM
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)    "[cx2560x]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
#include <mt-plat/upmu_common.h>
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/mtk_charger.h>
/* AL6528A code for SR-AL6528A-01-303 by wenyaqi at 2022/08/31 start */
#include <mt-plat/v1/charger_type.h>
#include "mtk_charger_intf.h"
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
#include "mtk_charger_init.h"
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */
/* A06 code for P240514-06636 by xiongxiaoliang at 20240520 start */
#include "mtk_battery_internal.h"
/* A06 code for P240514-06636 by xiongxiaoliang at 20240520 end */

#include "cx2560x_reg.h"
#include "cx2560x.h"

/* A06 code for AL7160A-108 | AL7160A-830 by jiashixian at 2024/05/25 start */
#ifdef CONFIG_CHARGER_CP_PPS
#include "charge_pump/pd_policy_manager.h"
#endif
/* A06 code for AL7160A-108 | AL7160A-830 by jiashixian at 2024/05/25 end */

/* hs14 code for AL6528A-600|AL6528A-1033 by gaozhengwei at 2022/12/09 start */
/* A06 code for AL7160A-3399 by xiongxiaoliang at 20240618 start */
#define BC12_DONE_TIMEOUT_CHECK_MAX_RETRY	17
/* A06 code for AL7160A-3399 by xiongxiaoliang at 20240618 end */
/* hs14 code for AL6528A-600|AL6528A-1033 by gaozhengwei at 2022/12/09 end */

/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
#define I2C_RETRY_CNT       3
/* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */
static int no_usb_flag = 0;
static int force_dpdm_count = 0;
static bool usb_detect_flag = false;
static bool usb_detect_done = false;

bool charge_done_flag = false;

static bool is_probe_detect_usb = false;
/* A06 code for AL7160A-119 by liufurong at 20240425 start */
static int gs_usb_detect = 0;
static bool gs_usb_detect_done = false;
/* A06 code for AL7160A-888 by zhangziyi at 20240510 start */
static bool gs_exit_irq = false;
/* A06 code for AL7160A-888 by zhangziyi at 20240510 end */
extern bool gadget_info_is_connected(void);
/* A06 code for AL7160A-119 by liufurong at 20240425 end */

struct cx2560x {
    struct device *dev;
    struct i2c_client *client;

    int part_no;

    const char *chg_dev_name;
    const char *eint_name;

    bool chg_det_enable;

    // enum charger_type chg_type;
    struct power_supply_desc psy_desc;
    int psy_usb_type;

    int status;
    u32 intr_gpio;
    int irq;

    struct mutex i2c_rw_lock;

    bool charge_enabled;    /* Register bit status */
    bool power_good;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    bool vbus_gd;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    bool vbus_stat;
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    bool bypass_chgdet_en;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    int prev_hiz_type;

    struct cx2560x_platform_data *platform_data;
    struct charger_device *chg_dev;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    struct delayed_work psy_dwork;
    struct delayed_work prob_dwork;
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    struct delayed_work charge_detect_delayed_work;
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */
    struct delayed_work charge_usb_detect_work;
    struct delayed_work charge_done_detect_work;
    struct delayed_work recharge_detect_work;
    struct delayed_work set_hiz_work;
    struct delayed_work detect_type_work;
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */
    struct power_supply *psy;
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 start*/
    int icl_ma;
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/
};
static void cx2560x_dump_regs(struct cx2560x *cx);
/* A06 code for AL7160A-119 by liufurong at 20240425 start */
static const unsigned int IPRECHG_CURRENT_STABLE[] = {
    52000,  104000, 156000, 208000, 260000, 312000, 364000, 416000,
    468000, 520000, 572000, 624000, 676000, 728000, 780000, 832000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
    60000,  120000,  180000,  240000, 300000,  360000,  420000,  480000,
    540000,  600000,  660000,  720000,  780000
};
/* A06 code for AL7160A-119 by liufurong at 20240425 end */

static const struct charger_properties cx2560x_chg_props = {
    .alias_name = "cx2560x",
};

/*A06 code for AL7160A-1630 by shixuanxuan at 20240614 start*/
static int cx2560x_set_ivl(struct charger_device *chg_dev, u32 volt);
#define CX2560_MIVR_MAX  5400000
#define CX2560_MIVR_DEFAULT  4600000
static int cx2560_enable_power_path(struct charger_device *chg_dev, bool en)
{
	u32 mivr = (en ? CX2560_MIVR_DEFAULT : CX2560_MIVR_MAX);

    pr_info("CX2560X %s: en = %d\n", __func__, en);
    return cx2560x_set_ivl(chg_dev, mivr);
}
/*A06 code for AL7160A-1630 by shixuanxuan at 20240614 end*/

/* A06 code for P240610-03562 by shixuanxuan at 20240614 start */
extern int g_pr_swap;
/* A06 code for P240610-03562 by shixuanxuan at 20240614 end */

static int __cx2560x_read_reg_without_retry(struct cx2560x *cx, u8 reg, u8 *data)
{
    s32 ret;
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_read_byte_data(cx->client, reg);

        if (ret >= 0)
            break;

        pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}


static int __cx2560x_read_reg(struct cx2560x *cx, u8 reg, u8 *data)
{
    s32 ret;
	u8 i, retry_count;
	u8 tmp_val[2];

	for(retry_count = 0; retry_count < 3; retry_count++) {
		for(i = 0; i < 2; i++) {
            ret=__cx2560x_read_reg_without_retry(cx, reg, data);
            if (ret < 0) {
				return ret;
		    } else {
				tmp_val[i] = *data;
		    }
		}
		if(tmp_val[0] == tmp_val[1]) {
			*data = tmp_val[0];
			return 0;
		}
	}
    pr_err("i2c retry read fail: can't read from reg 0x%02X\n", reg);
    return 0;
}

static int __cx2560x_write_reg_without_retry(struct cx2560x *cx, int reg, u8 val)
{
    s32 ret;
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 start */
    int i;

    for (i = 0; i < I2C_RETRY_CNT; ++i) {

        ret = i2c_smbus_write_byte_data(cx->client, reg, val);

        if (ret >= 0)
            break;

        pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
            __func__, reg, ret, i + 1, I2C_RETRY_CNT);
    }
    /* hs14 code for SR-AL6528A-01-787 by gaozhengwei at 2022/12/06 end */

    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                val, reg, ret);
        return ret;
    }
    return 0;
}

static int __cx2560x_write_reg(struct cx2560x *cx, int reg, u8 val)
{
    s32 ret;
	u8 read_val = 0;
	u8 retry_count;

	for(retry_count = 0; retry_count < 10; retry_count++) {
		ret = __cx2560x_write_reg_without_retry(cx, reg, val);
		if (ret < 0) {
			return ret;
		} else {
		    ret = __cx2560x_read_reg_without_retry(cx, reg, &read_val);
			if (ret < 0) {
				return ret;
			} else {
			    if (val == read_val) {
					break;
			    } else {
					if(retry_count == 9) {
						pr_err( "i2c retry write fail: can't read from reg 0x%02X \n", reg);
						return 0;
					}
				}
			}
	   }
	}
	return 0;
}



static int cx2560x_read_byte(struct cx2560x *cx, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&cx->i2c_rw_lock);
    ret = __cx2560x_read_reg(cx, reg, data);
    mutex_unlock(&cx->i2c_rw_lock);

    return ret;
}

static int cx2560x_read_byte_without_retry(struct cx2560x *cx, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&cx->i2c_rw_lock);
    ret = __cx2560x_read_reg_without_retry(cx, reg, data);
    mutex_unlock(&cx->i2c_rw_lock);

    return ret;
}

/* static int cx2560x_write_byte(struct cx2560x *cx, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&cx->i2c_rw_lock);
    ret = __cx2560x_write_reg(cx, reg, data);
    mutex_unlock(&cx->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
} */

static int cx2560x_write_byte_without_retry(struct cx2560x *cx, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&cx->i2c_rw_lock);
    ret = __cx2560x_write_reg_without_retry(cx, reg, data);
    mutex_unlock(&cx->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int cx2560x_update_bits(struct cx2560x *cx, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&cx->i2c_rw_lock);
    ret = __cx2560x_read_reg(cx, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

	if( (reg==0x01&&mask==0x40) || (reg==0x07&&mask==0x80) ||
		(reg==0x0B&&mask==0x80) || (reg==0x0D&&mask==0x80) ||
		(reg==0x07&&mask==0x20) ) {
          ret = __cx2560x_write_reg_without_retry(cx, reg, tmp);
	} else {
          ret = __cx2560x_write_reg(cx, reg, tmp);
	}

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&cx->i2c_rw_lock);
    return ret;
}

static int cx2560x_write_reg40(struct cx2560x *cx, bool enable)
{
	int ret,try_count=10;
	u8 read_val;

	if(enable)
	{
		while(try_count--)
		{
			ret = cx2560x_write_byte_without_retry(cx, 0x40, 0x00);
			ret = cx2560x_write_byte_without_retry(cx, 0x40, 0x50);
			ret = cx2560x_write_byte_without_retry(cx, 0x40, 0x57);
			ret = cx2560x_write_byte_without_retry(cx, 0x40, 0x44);
			ret = cx2560x_read_byte_without_retry(cx, 0x40, &read_val);
			if(0x03 == read_val)
			{
				ret=1;
				break;
			}
		}
	}
	else
	{
		ret = cx2560x_write_byte_without_retry(cx, 0x40, 0x00);
	}
	return ret;
}

/* A06 code for AL7160A-1074 by zhangziyi at 20240515 start */
static int cx2560x_enable_otg(struct cx2560x *cx)
{
    int ret;
    u8 val_reg;
    u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

    cx2560x_read_byte_without_retry(cx, CX2560X_REG_09, &val_reg);
    cx2560x_update_bits(cx, CX2560X_REG_0C, REG0C_BOOST_LIM_MASK,
    REG0C_BOOST_LIM_1P2A << REG0C_BOOST_LIM_SHIFT);
    cx2560x_write_reg40(cx, true);
    cx2560x_write_byte_without_retry(cx, 0x83, 0x03);
    cx2560x_write_byte_without_retry(cx, 0x41, 0xA8);
    cx2560x_update_bits(cx, 0x84, 0x80, 0x80);
    ret =  cx2560x_update_bits(cx, CX2560X_REG_01, REG01_OTG_CONFIG_MASK, val);
    msleep(100);
    cx2560x_write_byte_without_retry(cx, 0x83, 0x01);
    msleep(10);
    cx2560x_write_byte_without_retry(cx, 0x41, 0x28);
    cx2560x_write_reg40(cx, false);
    return ret;
}
/* A06 code for AL7160A-1074 by zhangziyi at 20240515 end */

static int cx2560x_disable_otg(struct cx2560x *cx)
{
    int ret;
    u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

    ret = cx2560x_update_bits(cx, CX2560X_REG_01, REG01_OTG_CONFIG_MASK, val);
	cx2560x_write_reg40(cx, true);
    cx2560x_write_byte_without_retry(cx, 0x83, 0x00);
	cx2560x_update_bits(cx, 0x84, 0x80, 0x00);
	cx2560x_write_reg40(cx, false);
    return ret;
}

static int cx2560x_enable_charger(struct cx2560x *cx)
{
    int ret;
    u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

    ret =
        cx2560x_update_bits(cx, CX2560X_REG_01, REG01_CHG_CONFIG_MASK, val);

    return ret;
}

/* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
static int cx2560x_disable_charger(struct cx2560x *cx)
{
    int ret;
    u8 val = 0;
    val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

    ret = cx2560x_update_bits(cx, CX2560X_REG_01, REG01_CHG_CONFIG_MASK, val);
    return ret;
}
/* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

/* A06 code for AL7160A-3403 by zhangziyi at 20240619 start */
int cx2560x_set_chargecurrent(struct cx2560x *cx, int curr)
{
    u8 ichg;
    u8 reg_val;

    if (curr < 0) {
        curr = 0;
    } else if (curr > 30475/10) {
        curr = 30475/10;
    }

    if (curr <= 1170) {
        ichg = curr / 90;
    } else {
        ichg = (curr - 805) / (575 / 10) + 14;
    }

    cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
    reg_val = (reg_val & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
    if (reg_val == REG08_VBUS_TYPE_OTG) {
        pr_err("OTG mode not set ichg\n");
        return 0;
    }

    return cx2560x_update_bits(cx, CX2560X_REG_02, REG02_ICHG_MASK,
                    ichg << REG02_ICHG_SHIFT);
}
/* A06 code for AL7160A-3403 by zhangziyi at 20240619 end */

int cx2560x_set_term_current(struct cx2560x *cx, int curr)
{
    u8 reg_val;

    curr = curr*1000;
    if (curr > ITERM_CURRENT_STABLE[12])
        curr = ITERM_CURRENT_STABLE[12];

    for(reg_val = 1; reg_val < 13 && curr >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;

    return cx2560x_update_bits(cx, CX2560X_REG_03,
                   REG03_ITERM_MASK, reg_val);
}

EXPORT_SYMBOL_GPL(cx2560x_set_term_current);

int cx2560x_set_prechg_current(struct cx2560x *cx, int curr)
{
    u8 reg_val;

    curr = curr*1000;
    if (curr > IPRECHG_CURRENT_STABLE[15])
        curr = IPRECHG_CURRENT_STABLE[15];

    for(reg_val = 1; reg_val < 16 && curr >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;

    return cx2560x_update_bits(cx, CX2560X_REG_03, REG03_IPRECHG_MASK,
                    reg_val << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(cx2560x_set_prechg_current);

int cx2560x_set_chargevolt(struct cx2560x *cx, int volt)
{
    u8 val;

    if (volt < REG04_VREG_BASE)
        volt = REG04_VREG_BASE;

    val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;

    return cx2560x_update_bits(cx, CX2560X_REG_04, REG04_VREG_MASK,
                    val << REG04_VREG_SHIFT);
}

int cx2560x_set_input_volt_limit(struct cx2560x *cx, int volt)
{
    u8 val;

    if (volt < REG11_VINDPM_BASE)
        volt = REG11_VINDPM_BASE;

    val = (volt - REG11_VINDPM_BASE) / REG11_VINDPM_LSB;
    return cx2560x_update_bits(cx, CX2560X_REG_11, REG11_VINDPM_MASK,
                    val << REG11_VINDPM_SHIFT);
}

int cx2560x_set_input_current_limit(struct cx2560x *cx, int curr)
{
    u8 val;

    if (curr < REG00_IINLIM_BASE) {
        curr = REG00_IINLIM_BASE;
        cx2560_enable_power_path(cx->chg_dev, false);
    } else {
        cx2560_enable_power_path(cx->chg_dev, true);
    }
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 start*/
    if (curr == 3000) {
        curr = 500;
    } else if (curr > 1700) {
        curr = 1700;
    }

    if (g_pr_swap && curr > 500) {
        curr = 500;
    }

    cx->icl_ma = curr;
    pr_err("set icl = %d\n",curr);
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 end*/

    val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
    return cx2560x_update_bits(cx, CX2560X_REG_00, REG00_IINLIM_MASK,
                    val << REG00_IINLIM_SHIFT);
}

int cx2560x_set_watchdog_timer(struct cx2560x *cx, u8 timeout)
{
    u8 temp;

    temp = (u8) (((timeout -
                REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

    return cx2560x_update_bits(cx, CX2560X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(cx2560x_set_watchdog_timer);

int cx2560x_disable_watchdog_timer(struct cx2560x *cx)
{
    u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(cx2560x_disable_watchdog_timer);

int cx2560x_reset_watchdog_timer(struct cx2560x *cx)
{
    u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_01, REG01_WDT_RESET_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_reset_watchdog_timer);

int cx2560x_reset_chip(struct cx2560x *cx)
{
    int ret;
    u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

    ret =
        cx2560x_update_bits(cx, CX2560X_REG_0B, REG0B_REG_RESET_MASK, val);
    return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_reset_chip);

/* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
int cx2560x_enter_hiz_mode(struct cx2560x *cx)
{
    int ret = 0;

    pr_err("%s enter\n", __func__);
    schedule_delayed_work(&cx->set_hiz_work, 0);
    return ret;

}
EXPORT_SYMBOL_GPL(cx2560x_enter_hiz_mode);

int cx2560x_exit_hiz_mode(struct cx2560x *cx)
{
    u8 val = 0;

    pr_err("%s exit\n", __func__);
    val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(cx2560x_exit_hiz_mode);
/* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

static int cx2560x_enable_term(struct cx2560x *cx, bool enable)
{
    u8 val;
    int ret;

    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 start */
    if (enable) {
        val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
        pr_err("cx2560x_enable_term enable\n");
    }
    else {
        val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;
    }
    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 end */

    ret = cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TERM_MASK, val);

    return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_enable_term);

/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 start */
/* A06 code for AL7160A-1074 by zhangziyi at 20240515 start */
int cx2560x_set_boost_current(struct cx2560x *cx, int curr)
{
    u8 val;

    if (curr >= BOOSTI_1200) {
        val = REG0C_BOOST_LIM_1P2A;
    } else {
        val = REG0C_BOOST_LIM_0P5A;
    }

    return cx2560x_update_bits(cx, CX2560X_REG_0C, REG0C_BOOST_LIM_MASK,
                    val << REG0C_BOOST_LIM_SHIFT);
}
/* A06 code for AL7160A-1074 by zhangziyi at 20240515 end */

int cx2560x_set_boost_voltage(struct cx2560x *cx, int volt)
{
    u8 val;

    if (volt >= BOOSTV_5300)
        val = REG06_BOOSTV_5P3V;
    else if (volt >= BOOSTV_5150 && volt < BOOSTV_5300)
        val = REG06_BOOSTV_5P15V;
    else if (volt >= BOOSTV_4850 && volt < BOOSTV_5000)
        val = REG06_BOOSTV_4P85V;
    else
        val = REG06_BOOSTV_5V;

    return cx2560x_update_bits(cx, CX2560X_REG_06, REG06_BOOSTV_MASK,
                    val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(cx2560x_set_boost_voltage);

static int cx2560x_set_acovp_threshold(struct cx2560x *cx, int volt)
{
    u8 val;

    if (volt >= VAC_OVP_14000)
        val = REG06_OVP_14P0V;
    else if (volt >= VAC_OVP_10500 && volt < VAC_OVP_14000)
        val = REG06_OVP_10P5V;
    else if (volt >= VAC_OVP_6500 && volt < REG06_OVP_10P5V)
        val = REG06_OVP_6P5V;
    else
        val = REG06_OVP_5P5V;

    return cx2560x_update_bits(cx, CX2560X_REG_06, REG06_OVP_MASK,
                    val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(cx2560x_set_acovp_threshold);
/* hs14 code for AL6528A-341 by wenyaqi at 2022/10/20 end */

static int cx2560x_set_stat_ctrl(struct cx2560x *cx, int ctrl)
{
    u8 val;

    val = ctrl;

    return cx2560x_update_bits(cx, CX2560X_REG_00, REG00_STAT_CTRL_MASK,
                    val << REG00_STAT_CTRL_SHIFT);
}

static int cx2560x_set_int_mask(struct cx2560x *cx, int mask)
{

    return 0;
}

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
static int cx2560x_force_dpdm(struct cx2560x *cx)
{
    const u8 val = REG07_FORCE_DPDM << REG07_FORCE_DPDM_SHIFT;

    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    /*  reset adapter */
    cx2560x_update_bits(cx, CX2560X_REG_10, CX2560X_DP_DAC_MASK, CX2560X_DP_0V << CX2560X_DP_DAC_SHIFT);
    cx2560x_update_bits(cx, CX2560X_REG_10, CX2560X_DM_DAC_MASK, CX2560X_DM_0V << CX2560X_DM_DAC_SHIFT);
    msleep(100);
    cx2560x_update_bits(cx, CX2560X_REG_10, CX2560X_DP_DAC_MASK, CX2560X_DP_HIZ << CX2560X_DP_DAC_SHIFT);
    cx2560x_update_bits(cx, CX2560X_REG_10, CX2560X_DM_DAC_MASK, CX2560X_DM_HIZ << CX2560X_DM_DAC_SHIFT);

    /* A06 code for AL7160A-3074 by xiongxiaoliang at 20240614 start */
    return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_FORCE_DPDM_MASK, val);
    /* A06 code for AL7160A-3074 by xiongxiaoliang at 20240614 end */
    /* A06 code for AL7160A-119 by liufurong at 20240425 end */
}
EXPORT_SYMBOL_GPL(cx2560x_force_dpdm);
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

static int cx2560x_enable_batfet(struct cx2560x *cx)
{
    const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_enable_batfet);

static int cx2560x_disable_batfet(struct cx2560x *cx)
{
    const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DIS_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_disable_batfet);

static int cx2560x_set_batfet_delay(struct cx2560x *cx, uint8_t delay)
{
    u8 val;

    if (delay == 0)
        val = REG07_BATFET_DLY_0S;
    else
        val = REG07_BATFET_DLY_10S;

    val <<= REG07_BATFET_DLY_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DLY_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_set_batfet_delay);

static int cx2560x_enable_safety_timer(struct cx2560x *cx)
{
    const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_enable_safety_timer);

static int cx2560x_disable_safety_timer(struct cx2560x *cx)
{
    const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

    return cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TIMER_MASK,
                    val);
}
EXPORT_SYMBOL_GPL(cx2560x_disable_safety_timer);

static struct cx2560x_platform_data *cx2560x_parse_dt(struct device_node *np,
                                struct cx2560x *cx)
{
    int ret;
    struct cx2560x_platform_data *pdata;

    pdata = devm_kzalloc(cx->dev, sizeof(struct cx2560x_platform_data),
                 GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "charger_name", &cx->chg_dev_name) < 0) {
        cx->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &cx->eint_name) < 0) {
        cx->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    ret = of_get_named_gpio(np, "cx,intr_gpio", 0);
    if (ret < 0) {
        pr_err("%s no cx,intr_gpio(%d)\n", __func__, ret);
    } else {
        cx->intr_gpio = ret;
    }

    cx->chg_det_enable =
        of_property_read_bool(np, "cx2560x,charge-detect-enable");

    ret = of_property_read_u32(np, "cx2560x,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of cx2560x,usb-vlim\n");
    }

    ret = of_property_read_u32(np, "cx2560x,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of cx2560x,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "cx2560x,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of cx2560x,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "cx2560x,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of cx2560x,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "cx2560x,stat-pin-ctrl",
                    &pdata->statctrl);
    if (ret) {
        pdata->statctrl = 0;
        pr_err("Failed to read node of cx2560x,stat-pin-ctrl\n");
    }

    ret = of_property_read_u32(np, "cx2560x,precharge-current",
                    &pdata->iprechg);
    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    if (ret) {
        pdata->iprechg = 260;
        pr_err("Failed to read node of cx2560x,precharge-current\n");
    }

    ret = of_property_read_u32(np, "cx2560x,termination-current",
                    &pdata->iterm);
    if (ret) {
        pdata->iterm = 240;
        pr_err
            ("Failed to read node of cx2560x,termination-current\n");
    }
    /* A06 code for AL7160A-119 by liufurong at 20240425 end */

    ret =
        of_property_read_u32(np, "cx2560x,boost-voltage",
                 &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of cx2560x,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "cx2560x,boost-current",
                 &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of cx2560x,boost-current\n");
    }

    ret = of_property_read_u32(np, "cx2560x,vac-ovp-threshold",
                    &pdata->vac_ovp);
    if (ret) {
        /* A06 code for AL7160A-119 by liufurong at 20240425 start */
        pdata->vac_ovp = 10500;
        /* A06 code for AL7160A-119 by liufurong at 20240425 end */
        pr_err("Failed to read node of cx2560x,vac-ovp-threshold\n");
    }

    return pdata;
}

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
static int cx2560x_dpdm_detect_is_done(struct cx2560x * cx)
{

    u8 pg_stat;
    int ret;


    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &pg_stat);
    pg_stat = (pg_stat & REG08_PG_STAT_MASK) >> REG08_PG_STAT_SHIFT;
	if (pg_stat == 1)
		return true;
	else
		return false;

    return true;
}

// static int cx2560x_enable_hvdcp(struct cx2560x * cx)
// {
//     int ret;

//     ret = cx2560x_update_bits(cx, CX2560X_REG_10, 0x08, 0x08);
//     pr_err("%s cx2560x_enable_hvdcp 1111\n");
//     ret = cx2560x_update_bits(cx, CX2560X_REG_10, 0x07, 0x0 << 0); //dm set hiz
//     pr_err("%s cx2560x_enable_hvdcp 2222\n");
//     ret = cx2560x_update_bits(cx, CX2560X_REG_10, 0x70, 0x2 << 4); //dp set 0.6v
//     pr_err("%s cx2560x_enable_hvdcp 3333\n");
//     cx2560x_dump_regs(cx);
//     return ret;
// }
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */

/* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
static void cx2560x_charger_detect_work_func(struct work_struct *work)
{
    int ret = 0;
    u8 reg_val = 0;
    int vbus_stat = 0;
    int chg_type = CHARGER_UNKNOWN;
    int retry_cnt = 0;

    struct cx2560x *cx = container_of(work, struct cx2560x,
                                charge_detect_delayed_work.work);

    do {
        if (!cx2560x_dpdm_detect_is_done(cx)) {
            pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
        } else {
            pr_err("[%s] BC1.2 done\n", __func__);
            break;
        }
        mdelay(60);
    } while(retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &reg_val);

    if (ret)
        return;

    vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
    vbus_stat >>= REG08_VBUS_STAT_SHIFT;

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    pr_info("[%s] reg08: 0x%02x :vbus state %d\n", __func__, reg_val, vbus_stat);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    switch (vbus_stat) {
    case REG08_VBUS_TYPE_NONE:
        chg_type = CHARGER_UNKNOWN;
        break;
    case REG08_VBUS_TYPE_SDP:
        /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
        if (is_probe_detect_usb == false) {
            pr_info("[%s] SDP detect \n", __func__);
            if (!usb_detect_flag) {
                schedule_delayed_work(&cx->charge_usb_detect_work, 1* HZ);
            }
            if(no_usb_flag == 0) {
                pr_info("[%s] CX25601D charger type: SDP\n", __func__);
                chg_type = STANDARD_HOST;
            } else {
                pr_info("[%s] CX25601D charger type: UNKNOWN/FLOAT\n", __func__);
                chg_type = NONSTANDARD_CHARGER;
            }
        } else {
                chg_type = STANDARD_HOST;
                is_probe_detect_usb = false;
                pr_info("cx2560x_charger_probe start detect usb 20s\n");
                schedule_delayed_work(&cx->charge_usb_detect_work, 20 * HZ);
        }
        /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */
    break;
    case REG08_VBUS_TYPE_CDP:
        chg_type = CHARGING_HOST;
        break;
    case REG08_VBUS_TYPE_DCP:
        chg_type = STANDARD_CHARGER;
        break;
    case REG08_VBUS_TYPE_UNKNOWN:
    /* hs14 code for SR-AL6528A-01-252 by chengyuanhang at 2022/09/27 start */
        chg_type = NONSTANDARD_CHARGER;
    /* hs14 code for SR-AL6528A-01-252 by chengyuanhang at 2022/09/27 end */
        break;
    case REG08_VBUS_TYPE_NON_STD:
        chg_type = NONSTANDARD_CHARGER;
        break;
    default:
        chg_type = NONSTANDARD_CHARGER;
        break;
    }

    /* hs14 code for SR-AL6528A-01-306|SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    if (chg_type != STANDARD_CHARGER) {
        Charger_Detect_Release();
    } else {
        // ret = cx2560x_enable_hvdcp(cx);
        // if (ret)
        //     pr_err("Failed to en HVDCP, ret = %d\n", ret);
    }
    /* hs14 code for SR-AL6528A-01-306|SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    is_probe_detect_usb = false;
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */
    cx->psy_usb_type = chg_type;

    schedule_delayed_work(&cx->psy_dwork, 0);

    return;
}
/* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
static void cx2560x_inform_psy_dwork_handler(struct work_struct *work)
{
    int ret = 0;
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    u8 reg_val = 0;
    union power_supply_propval propval;
    struct cx2560x *cx = container_of(work, struct cx2560x,
                                psy_dwork.work);
    if (!cx->psy) {
        cx->psy = power_supply_get_by_name("charger");
        if (!cx->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            mod_delayed_work(system_wq, &cx->psy_dwork,
                    msecs_to_jiffies(2000));
            return;
        }
    }
#ifdef BEFORE_MTK_ANDROID_T // which is used before Android T
    if (cx->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
#else
    if (cx->psy_usb_type != CHARGER_UNKNOWN)
#endif
        propval.intval = 1;
    else
        propval.intval = 0;

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_0A, &reg_val);
    if (ret) {
        pr_err("cx2560x_read_byte_without_retry read fail:%d\n", ret);
        return;
    }

    if (reg_val & 0x80) {
        propval.intval = 1;
    } else {
        propval.intval = 0;
    }

    pr_err("%s ONLINE %d\n", __func__, propval.intval);
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

    ret = power_supply_set_property(cx->psy, POWER_SUPPLY_PROP_ONLINE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    propval.intval = cx->psy_usb_type;

    ret = power_supply_set_property(cx->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);

    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return;
}

static irqreturn_t cx2560x_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val;
    /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 start */
    u8 val;
    /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 end */
    bool prev_pg;
    bool prev_vbus_gd;
    struct cx2560x *cx = (struct cx2560x *)data;

    ret = cx2560x_read_byte(cx, CX2560X_REG_0A, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_vbus_gd = cx->vbus_gd;

    cx->vbus_gd = !!(reg_val & REG0A_VBUS_GD_MASK);

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &reg_val);
    if (ret)
        return IRQ_HANDLED;

    prev_pg = cx->power_good;

    cx->power_good = !!(reg_val & REG08_PG_STAT_MASK);

    /* A06 code for P240610-03562 by shixuanxuan at 20240614 start */
    pr_info("%s enter. bypass_chgdet_en = %d, g_pr_swap = %d\n",
        __func__, cx->bypass_chgdet_en, g_pr_swap);

    if ((cx->bypass_chgdet_en == true) || g_pr_swap) {
        pr_err("%s:bypass_chgdet_en=%d, skip bc12\n", __func__, cx->bypass_chgdet_en);
        return IRQ_HANDLED;
    }
    /* A06 code for P240610-03562 by shixuanxuan at 20240614 end */
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240614 start */
    cx2560x_set_input_current_limit(cx, cx->icl_ma);
    /* A06 code for AL7160A-1630 by shixuanxuan at 20240614 end */

    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */

    cx2560x_dump_regs(cx);

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_00, &reg_val);
    if (ret)
        return IRQ_HANDLED;
    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    /* A06 code for AL7160A-888 by zhangziyi at 20240510 start */
    if (reg_val & 0x80) {
        cancel_delayed_work(&cx->detect_type_work);
        schedule_delayed_work(&cx->detect_type_work, 0);
        gs_exit_irq = true;
        return IRQ_HANDLED;
    }
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

    if (gs_exit_irq) {
        pr_err("%s: usb online, return\n", __func__);
        return IRQ_HANDLED;
    }
    /* A06 code for AL7160A-888 by zhangziyi at 20240510 end */

    /* A06 code for AL7160A-119 by liufurong at 20240425 end */

    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
    if (!prev_vbus_gd && cx->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        cx->vbus_stat = true;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        pr_notice("adapter/usb inserted\n");
        /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 start */
        cx2560x_write_reg40(cx, true);
        cx2560x_read_byte_without_retry(cx, 0x81, &val);
        cx2560x_write_byte_without_retry(cx, 0x81, 0x04);
        cx2560x_write_reg40(cx, false);
        /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 end */
        /* A06 code for P240514-07707 by xiongxiaoliang at 20240524 start */
        cx2560_afc_switch_ctrl(true);
        /* A06 code for P240514-07707 by xiongxiaoliang at 20240524 end */
        no_usb_flag = 0;
		force_dpdm_count = 3;
		usb_detect_flag = false;
		usb_detect_done = false;
        Charger_Detect_Init();
		schedule_delayed_work(&cx->charge_done_detect_work, 0);
    } else if (prev_vbus_gd && !cx->vbus_gd) {
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
        cx->vbus_stat = false;
        /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */
        cx->psy_usb_type = CHARGER_UNKNOWN;
        schedule_delayed_work(&cx->psy_dwork, 0);
        pr_notice("adapter/usb removed\n");
        charge_done_flag = false;
        ret = cx2560x_update_bits(cx, CX2560X_REG_10, 0x07, 0x00); //dm set hiz
        ret = cx2560x_update_bits(cx, CX2560X_REG_10, 0x70, 0x00); //dp set hiz
        Charger_Detect_Release();
        /* A06 code for AL7160A-119 by liufurong at 20240425 start */
        /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 start*/
        cx->icl_ma = 0;
        /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/
        cancel_delayed_work(&cx->charge_done_detect_work);
        cancel_delayed_work(&cx->recharge_detect_work);
        cx2560x_set_input_volt_limit(cx, 4400);

        ret = cx2560x_update_bits(cx, CX2560X_REG_01, REG01_OTG_CONFIG_MASK,
            REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
        /* A06 code for AL7160A-119 by liufurong at 20240425 end */
        return IRQ_HANDLED;
    }
    /* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    if(gs_usb_detect_done) {
        gs_usb_detect = 0;
    }
    /* A06 code for AL7160A-119 by liufurong at 20240425 end */

    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    if (!prev_pg && cx->power_good) {
        schedule_delayed_work(&cx->charge_detect_delayed_work, 0);
        /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 start */
        cx2560x_write_reg40(cx, true);
        cx2560x_write_byte_without_retry(cx, 0x81, val);
        cx2560x_write_reg40(cx, false);
        /* A06 code for AL7160A-2110 by xiongxiaoliang at 20240531 end */
        /* A06 code for P240514-07707 by xiongxiaoliang at 20240524 start */
        cx2560_afc_switch_ctrl(false);
        /* A06 code for P240514-07707 by xiongxiaoliang at 20240524 end */
    }
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */

    return IRQ_HANDLED;

}
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

/* A06 code for AL7160A-1473 by zhangziyi at 20240528 start */
static void cx2560x_use_hiz_mode(struct cx2560x *cx)
{
    int retry_cnt = 0;
    int vbus_stat = 0;
    u8 reg_val = 0;
    u8 enable_val = 0;

    enable_val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

    if (cx->prev_hiz_type == 0) {
        do {
            if (!cx2560x_dpdm_detect_is_done(cx)) {
                pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
            } else {
                pr_err("[%s] BC1.2 done\n", __func__);
                break;
            }
            mdelay(60);
        } while (retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

        cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &reg_val);

        vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
        vbus_stat >>= REG08_VBUS_STAT_SHIFT;
        cx->prev_hiz_type = vbus_stat;
    }
    pr_err("[%s] BC1.2 done prev_hiz_type %d \n", __func__, cx->prev_hiz_type);
    cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, enable_val);

    return;
}
/* A06 code for AL7160A-1473 by zhangziyi at 20240528 end */

/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
static int cx2560x_set_shipmode(struct charger_device *chg_dev, bool en)
{
    int ret;
    u8 val;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        /* A06 code for AL7160A-1473 by zhangziyi at 20240528 start */
        cx2560x_use_hiz_mode(cx);
        /* A06 code for AL7160A-1473 by zhangziyi at 20240528 end */
        cx2560x_set_input_volt_limit(cx, 4400);
        cancel_delayed_work_sync(&cx->charge_detect_delayed_work);
        cancel_delayed_work_sync(&cx->prob_dwork);
        cancel_delayed_work(&cx->charge_done_detect_work);
        cancel_delayed_work(&cx->recharge_detect_work);
        cancel_delayed_work(&cx->charge_usb_detect_work);
        val = REG07_BATFET_DLY_10S << REG07_BATFET_DLY_SHIFT;
        ret = cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DLY_MASK, val);
        val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;
        ret = cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DIS_MASK, val);
    } else {
        val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;
        ret = cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DIS_MASK, val);
    }
    cx2560x_dump_regs(cx);
    pr_err("%s shipmode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int cx2560x_get_shipmode(struct charger_device *chg_dev)
{
    int ret;
    u8 val;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    msleep(100);
    ret = cx2560x_read_byte(cx, CX2560X_REG_07, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", CX2560X_REG_07, val);
    } else {
        pr_err("%s: get shipmode reg fail! \n",__func__);
        return ret;
    }
    ret = (val & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
    pr_err("%s:shipmode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}
static int cx2560x_disable_battfet_rst(struct cx2560x *cx)
{
    int ret;
    u8 val;

    val = REG07_BATFET_RST_DISABLE << REG07_BATFET_RST_EN_SHIFT;
    ret = cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_RST_EN_MASK, val);

    pr_err("disable BATTFET_RST_EN %s\n", !ret ? "successfully" : "failed");

    return ret;
}
/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
static int cx2560x_get_charging_status(struct charger_device *chg_dev,
    int *chg_stat)
{
    int ret;
    u8 val;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
	    if (charge_done_flag == true)
		    val = REG08_CHRG_STAT_CHGDONE;

        *chg_stat = val;
    }

    return ret;
}
/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

static int cx2560x_register_interrupt(struct cx2560x *cx)
{
    int ret = 0;

    ret = devm_gpio_request_one(cx->dev, cx->intr_gpio, GPIOF_DIR_IN,
            "cx41511_intr_gpio");
    if (ret < 0) {
        dev_info(cx->dev, "request gpio fail\n");
        return ret;
    } else {
        cx->client->irq = gpio_to_irq(cx->intr_gpio);
    }

    if (! cx->client->irq) {
        pr_info("cx->client->irq is NULL\n");//remember to config dws
        return -ENODEV;
    }

    ret = devm_request_threaded_irq(cx->dev, cx->client->irq, NULL,
                    cx2560x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "ti_irq", cx);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }

    enable_irq_wake(cx->irq);

    return 0;
}

static int cx2560x_init_device(struct cx2560x *cx)
{
    int ret;

    cx2560x_reset_chip(cx);
    cx2560x_disable_watchdog_timer(cx);
    cx2560x_write_reg40(cx, true);
    cx2560x_write_byte_without_retry(cx, 0x41, 0x28);
    cx2560x_write_byte_without_retry(cx, 0x44, 0x18);
    cx2560x_write_reg40(cx, false);

    ret = cx2560x_set_stat_ctrl(cx, cx->platform_data->statctrl);
    if (ret)
        pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

    ret = cx2560x_set_prechg_current(cx, cx->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = cx2560x_set_term_current(cx, cx->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = cx2560x_set_boost_voltage(cx, cx->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = cx2560x_set_boost_current(cx, cx->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

    ret = cx2560x_set_acovp_threshold(cx, cx->platform_data->vac_ovp);
    if (ret)
        pr_err("Failed to set acovp threshold, ret = %d\n", ret);

    cx2560x_update_bits(cx, CX2560X_REG_06, REG06_OVP_MASK,
        2 << REG06_OVP_SHIFT);

    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 start */
    cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_THERMAL_MASK,
        REG0F_THERMAL_DISABLE_TREG << REG0F_THERMAL_SHIFT);
    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 end */

    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 start*/
    cx2560x_update_bits(cx, CX2560X_REG_02, REG02_Q1_FULLON_MASK,
        REG02_Q1_FULLON_ENABLE << REG02_Q1_FULLON_SHIFT);
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 end*/

    /* A06 code for AL7160A-1473 by zhangziyi at 20240528 start */
    ret = cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DLY_MASK,
        REG07_BATFET_DLY_10S << REG07_BATFET_DLY_SHIFT);
    /* A06 code for AL7160A-1473 by zhangziyi at 20240528 end */

    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 start */
    ret= cx2560x_disable_safety_timer(cx);
    if (ret)
        pr_err("Failed to set safety_timer stop, ret = %d\n", ret);
    /* hs14 code for AL6528ADEU-1863 by wenyaqi at 2022/11/03 end */

    ret = cx2560x_set_int_mask(cx,
                    REG0A_IINDPM_INT_MASK |
                    REG0A_VINDPM_INT_MASK);
    if (ret)
        pr_err("Failed to set vindpm and iindpm int mask\n");

    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    ret = cx2560x_disable_battfet_rst(cx);
    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 start */
    cx2560x_enable_term(cx, true);
    /* A06 code for AL7160A-2223 by zhangziyi at 20240531 end */
    if (ret)
        pr_err("Failed to disable_battfet\n");
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
    return 0;
}

/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
static void cx2560x_inform_prob_dwork_handler(struct work_struct *work)
{
    struct cx2560x *cx = container_of(work, struct cx2560x,
                                prob_dwork.work);

    cx2560x_force_dpdm(cx);

    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
    msleep(500);
    /* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    pr_err("cx2560x_inform_prob_dwork_handler\n");
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */
    cx2560x_irq_handler(cx->irq, (void *) cx);
}
/* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

static int cx2560x_detect_device(struct cx2560x *cx)
{
    int ret;
    u8 data;

    ret = cx2560x_read_byte(cx, CX2560X_REG_0B, &data);
    if (!ret) {
        cx->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
    }

    return ret;
}

static void cx2560x_dump_regs(struct cx2560x *cx)
{
    int addr;
    u8 val;
    int ret;

    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    for (addr = 0x0; addr <= 0x11; addr++) {
        ret = cx2560x_read_byte_without_retry(cx, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */
}

static ssize_t
cx2560x_show_registers(struct device *dev, struct device_attribute *attr,
                char *buf)
{
    struct cx2560x *cx = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "cx2560x Reg");
    for (addr = 0x0; addr <= 0x0B; addr++) {
        ret = cx2560x_read_byte_without_retry(cx, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                        "Reg[%.2x] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t
cx2560x_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct cx2560x *cx = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x0B) {
        cx2560x_write_byte_without_retry(cx, (unsigned char) reg,
                    (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, cx2560x_show_registers,
            cx2560x_store_registers);

static struct attribute *cx2560x_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group cx2560x_attr_group = {
    .attrs = cx2560x_attributes,
};

static int cx2560x_charging(struct charger_device *chg_dev, bool enable)
{

    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;

    if (enable) {
		if (charge_done_flag == false) {
            ret = cx2560x_enable_charger(cx);
		}
    } else {
        ret = cx2560x_disable_charger(cx);
    }

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    ret = cx2560x_read_byte(cx, CX2560X_REG_01, &val);

    if (!ret)
        cx->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

    return ret;
}

/* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
static int cx2560x_plug_in(struct charger_device *chg_dev)
{

    int ret;
    pr_err("%s \n", __func__);

    ret = cx2560x_charging(chg_dev, true);

    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

static int cx2560x_plug_out(struct charger_device *chg_dev)
{
    int ret;
    u8 val = 0;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    ret = cx2560x_charging(chg_dev, false);
    pr_err("%s \n", __func__);
    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    cx2560x_read_byte(cx, CX2560X_REG_07, &val);
    if (val & REG07_BATFET_DIS_MASK) {
        pr_err("%s shipmode now\n", __func__);
    } else {
        cx2560x_exit_hiz_mode(cx);
    }

    /* A06 code for AL7160A-1074 by zhangziyi at 20240515 start */
    if (gs_exit_irq) {
        gs_exit_irq = false;
        cx2560x_irq_handler(cx->irq, (void *)cx);
        pr_err("%s hiz remove irq\n", __func__);
    }
    /* A06 code for AL7160A-1074 by zhangziyi at 20240515 end */

    /* A06 code for AL7160A-888 by zhangziyi at 20240510 start */
    gs_exit_irq = false;
    /* A06 code for AL7160A-888 by zhangziyi at 20240510 end */
    cx->prev_hiz_type = 0;

    return ret;
}
/* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
static int cx2560x_bypass_chgdet(struct charger_device *chg_dev, bool bypass_chgdet_en)
{
    int ret = 0;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    cx->bypass_chgdet_en = bypass_chgdet_en;
    dev_info(cx->dev, "%s bypass_chgdet_en = %d\n", __func__, bypass_chgdet_en);

    return ret;
}
/* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */

/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 start */
static int cx2560x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    union power_supply_propval propval;

    dev_info(cx->dev, "%s en = %d\n", __func__, en);

    if (!cx->psy) {
        cx->psy = power_supply_get_by_name("charger");
        if (!cx->psy) {
            pr_err("%s Couldn't get psy\n", __func__);
            return -ENODEV;
        }
    }

    if (en == false) {
        propval.intval = 0;
    } else {
        propval.intval = 1;
    }

    ret = power_supply_set_property(cx->psy,
                    POWER_SUPPLY_PROP_ONLINE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);

    if (en == false) {
        propval.intval = CHARGER_UNKNOWN;
    } else {
        propval.intval = cx->psy_usb_type;
    }

    ret = power_supply_set_property(cx->psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

    return ret;
}
/* hs14 code for SR-AL6528A-01-255|P221117-03133 by wenyaqi at 2022/11/24 end */

static int cx2560x_dump_register(struct charger_device *chg_dev)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    cx2560x_dump_regs(cx);

    return 0;
}

static int cx2560x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    *en = cx->charge_enabled;

    return 0;
}

static int cx2560x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &val);
    if (!ret) {
        val = val & REG08_CHRG_STAT_MASK;
        val = val >> REG08_CHRG_STAT_SHIFT;
	    if (charge_done_flag == true)
		    val = REG08_CHRG_STAT_CHGDONE;
        *done = (val == REG08_CHRG_STAT_CHGDONE);
    }

    return ret;
}

static int cx2560x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge curr = %d\n", curr);

    return cx2560x_set_chargecurrent(cx, curr / 1000);
}

static int cx2560x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = cx2560x_read_byte(cx, CX2560X_REG_02, &reg_val);
    if (!ret) {
        ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;

        if (ichg <= 13) {
            ichg = ichg * 90;
        } else {
            ichg = (ichg - 14) * 115 / 2 + 805;
		}

        *curr = ichg * 1000;
    }

    return ret;
}

static int cx2560x_set_iterm(struct charger_device *chg_dev, u32 uA)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("termination curr = %d\n", uA);

    return cx2560x_set_term_current(cx, uA / 1000);
}

static int cx2560x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    *curr = 90 * 1000;
    /* A06 code for AL7160A-119 by liufurong at 20240425 end */
    return 0;
}

static int cx2560x_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
    *uA = 100 * 1000;
    return 0;
}

static int cx2560x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return cx2560x_set_chargevolt(cx, volt / 1000);
}

static int cx2560x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int vchg;
    int ret;

    ret = cx2560x_read_byte(cx, CX2560X_REG_04, &reg_val);
    if (!ret) {
        vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
        vchg = (vchg * REG04_VREG_LSB) + REG04_VREG_BASE;
        *volt = vchg * 1000;
    }

    return ret;
}

static int cx2560x_get_ivl_state(struct charger_device *chg_dev, bool *in_loop)
{
    int ret = 0;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;

    ret = cx2560x_read_byte(cx, CX2560X_REG_0A, &reg_val);
    if (!ret)
        *in_loop = (ret & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

    return ret;
}

static int cx2560x_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ivl;
    int ret;

    ret = cx2560x_read_byte(cx, CX2560X_REG_11, &reg_val);
    if (!ret) {
        ivl = (reg_val & REG11_VINDPM_MASK) >> REG11_VINDPM_SHIFT;
        ivl = ivl * REG11_VINDPM_LSB + REG11_VINDPM_BASE;
        *volt = ivl * 1000;
    }

    return ret;
}

static int cx2560x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);

    return cx2560x_set_input_volt_limit(cx, volt / 1000);

}

static int cx2560x_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);
    /* A06 code for AL7160A-108 | AL7160A-830 by jiashixian at 2024/05/25 start */
#ifdef CONFIG_CHARGER_CP_PPS
     if (g_cp_charging_lmt.cp_chg_enable) {
         curr = 200000;
         pr_err("cp open icl curr  = %d\n",curr);
     }
#endif
/* A06 code for AL7160A-108 | AL7160A-830 by jiashixian at 2024/05/25 end */

    return cx2560x_set_input_current_limit(cx, curr / 1000);
}

static int cx2560x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int icl;
    int ret;

    ret = cx2560x_read_byte(cx, CX2560X_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
        icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;

}

/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */
static int cx2560x_get_ibus(struct charger_device *chg_dev, u32 *curr)
{
    /*return 1650mA as ibus*/
    *curr = PD_INPUT_CURRENT;

    return 0;
}
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

static int cx2560x_enable_te(struct charger_device *chg_dev, bool en)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    pr_err("enable_term = %d\n", en);

    return cx2560x_enable_term(cx, en);
}

static int cx2560x_kick_wdt(struct charger_device *chg_dev)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    return cx2560x_reset_watchdog_timer(cx);
}

static int cx2560x_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 start*/
    if (en) {
        ret = cx2560x_enable_otg(cx);
    } else {
        if (!g_pr_swap) {
            cx->psy_usb_type = CHARGER_UNKNOWN;
            schedule_delayed_work(&cx->psy_dwork, 0);
        }
        ret = cx2560x_disable_otg(cx);
    }
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240629 end*/
    pr_err("%s OTG %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int cx2560x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    int ret;

    if (en)
        ret = cx2560x_enable_safety_timer(cx);
    else
        ret = cx2560x_disable_safety_timer(cx);

    return ret;
}

static int cx2560x_is_safety_timer_enabled(struct charger_device *chg_dev,
                        bool *en)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = cx2560x_read_byte(cx, CX2560X_REG_05, &reg_val);

    if (!ret)
        *en = !!(reg_val & REG05_EN_TIMER_MASK);

    return ret;
}

/* hs14 code for AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 start */
static int cx2560x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    dev_info(cx->dev, "%s event = %d\n", __func__, event);

    switch (event) {
    case EVENT_EOC:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
        break;
    case EVENT_RECHARGE:
        charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
        break;
    default:
        break;
    }
    return 0;
}
/* hs14 code for AL6528A-164|AL6528ADEU-643 by gaozhengwei|wenyaqi at 2022/10/13 end */

/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
static int cx2560x_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    if (en)
        ret = cx2560x_enter_hiz_mode(cx);
    else
        ret = cx2560x_exit_hiz_mode(cx);

    pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
            !ret ? "successfully" : "failed");

    return ret;
}

static int cx2560x_get_hiz_mode(struct charger_device *chg_dev)
{
    int ret;
    struct cx2560x *bq = dev_get_drvdata(&chg_dev->dev);
    u8 val;
    ret = cx2560x_read_byte(bq, CX2560X_REG_00, &val);
    if (ret == 0){
        pr_err("Reg[%.2x] = 0x%.2x\n", CX2560X_REG_00, val);
    }

    ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
    pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

    return ret;
}
/* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

static int cx2560x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = cx2560x_set_boost_current(cx, curr / 1000);

    return ret;
}

/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
static int cx2560x_get_vbus_status(struct charger_device *chg_dev)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    return cx->vbus_stat;
}

static int cx2560x_dynamic_set_hwovp_threshold(struct charger_device *chg_dev,
                    int adapter_type)
{
    struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
    u8 val = 0;

    if (adapter_type == SC_ADAPTER_NORMAL)
        val = REG06_OVP_6P5V;
    else if (adapter_type == SC_ADAPTER_HV)
        val = REG06_OVP_10P5V;

    return cx2560x_update_bits(cx, CX2560X_REG_06, REG06_OVP_MASK,
                    val << REG06_OVP_SHIFT);
}
/* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

static struct charger_ops cx2560x_chg_ops = {
    /* Normal charging */
    .plug_in = cx2560x_plug_in,
    .plug_out = cx2560x_plug_out,
    .dump_registers = cx2560x_dump_register,
    .enable = cx2560x_charging,
    .is_enabled = cx2560x_is_charging_enable,
    .get_charging_current = cx2560x_get_ichg,
    .set_charging_current = cx2560x_set_ichg,
    .get_input_current = cx2560x_get_icl,
    .set_input_current = cx2560x_set_icl,
    .get_constant_voltage = cx2560x_get_vchg,
    .set_constant_voltage = cx2560x_set_vchg,
    .kick_wdt = cx2560x_kick_wdt,
    .set_mivr = cx2560x_set_ivl,
    .get_mivr = cx2560x_get_ivl,
    .get_mivr_state = cx2560x_get_ivl_state,
    .is_charging_done = cx2560x_is_charging_done,
    .set_eoc_current = cx2560x_set_iterm,
    .enable_termination = cx2560x_enable_te,
    .reset_eoc_state = NULL,
    .get_min_charging_current = cx2560x_get_min_ichg,
    .get_min_input_current = cx2560x_get_min_aicr,

    /* Safety timer */
    .enable_safety_timer = cx2560x_set_safety_timer,
    .is_safety_timer_enabled = cx2560x_is_safety_timer_enabled,

    /* Power path */
/*A06 code for AL7160A-1630 by shixuanxuan at 20240526 start*/
    .enable_powerpath = cx2560_enable_power_path,
/*A06 code for AL7160A-1630 by shixuanxuan at 20240526 end*/
    .is_powerpath_enabled = NULL,

    /* OTG */
    .enable_otg = cx2560x_set_otg,
    .set_boost_current_limit = cx2560x_set_boost_ilmt,
    .enable_discharge = NULL,

    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = NULL,
    .enable_cable_drop_comp = NULL,

    /* ADC */
    .get_tchg_adc = NULL,
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 start */

    .get_ibus_adc = cx2560x_get_ibus,
    /* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/15 end */

    /* Event */
    .event = cx2560x_do_event,

    /* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 start */
    .set_hiz_mode = cx2560x_set_hiz_mode,
    .get_hiz_mode = cx2560x_get_hiz_mode,
    /* hs14 code for SR-AL6528A-01-299 by gaozhengwei at 2022/09/02 end */

    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
    .get_ship_mode = cx2560x_get_shipmode,
    .set_ship_mode = cx2560x_set_shipmode,
    /* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/

    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
    .get_chr_status = cx2560x_get_charging_status,
    /* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/

    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 start */
    .get_vbus_status = cx2560x_get_vbus_status,
    .dynamic_set_hwovp_threshold = cx2560x_dynamic_set_hwovp_threshold,
    /* hs14 code for AL6528ADEU-580 by gaozhengwei at 2022/10/09 end */

    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 start */
    /* charger type detection */
    .enable_chg_type_det = cx2560x_enable_chg_type_det,
    /* hs14 code for SR-AL6528A-01-255 by wenyaqi at 2022/10/26 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    .bypass_chgdet = cx2560x_bypass_chgdet,
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
};

static struct of_device_id cx2560x_charger_match_table[] = {
    {
     .compatible = "cx2560x_charger",
     },
    {},
};
MODULE_DEVICE_TABLE(of, cx2560x_charger_match_table);

/* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
static void set_hiz_func(struct work_struct *work)
{
    struct delayed_work *set_hiz_work = NULL;
    struct cx2560x *cx = NULL;
    int ret = 0;
    int retry_cnt = 0;
    int vbus_stat = 0;
    u8 reg_val = 0;
    u8 enable_val = 0;

    set_hiz_work = container_of(work, struct delayed_work, work);
    if (set_hiz_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return ;
    }

    cx = container_of(set_hiz_work, struct cx2560x, set_hiz_work);
    if (cx == NULL) {
        pr_err("Cann't get cx25890_device\n");
        return ;
    }

    enable_val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

    /* A06 code for AL7160A-1473 by zhangziyi at 20240528 start */
    if (cx->prev_hiz_type == 0) {
        do {
            if (!cx2560x_dpdm_detect_is_done(cx)) {
                pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
            } else {
                pr_err("[%s] BC1.2 done\n", __func__);
                break;
            }
            mdelay(60);
        } while(retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

        ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &reg_val);
        if (ret) {
            return;
        }

        vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
        vbus_stat >>= REG08_VBUS_STAT_SHIFT;
        /* A06 code for AL7160A-3399 by xiongxiaoliang at 20240618 start */
        if (!vbus_stat) {
            if (battery_get_vbus() > 3000) {
                cx->prev_hiz_type = REG08_VBUS_TYPE_SDP;
            } else {
                return;
            }
            pr_err("[%s] force sdp, vbus:%d\n", __func__, battery_get_vbus());
        } else {
            cx->prev_hiz_type = vbus_stat;
        }
        /* A06 code for AL7160A-3399 by xiongxiaoliang at 20240618 end */
    }
    /* A06 code for AL7160A-1473 by zhangziyi at 20240528 end */

    pr_err("[%s] set hiz:%d\n", __func__, cx->prev_hiz_type);

    ret = cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, enable_val);

    return;
}
/* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

/* A06 code for AL7160A-2223 by zhangziyi at 20240531 start */
static void charge_done_detect_work_func(struct work_struct *work)
{
    int ibat = 0;
    int vbat = 0;
    int vbus = 0;
    int ret = -1;
    int val_term = 0;
    int val_vreg = 0;
    unsigned char val;
    unsigned char iindpm_flag = 0;
    unsigned char vindpm_flag = 0;
    unsigned char regulation_status = 0;
    unsigned int vindpm_value;
    static u8 count = 0;
    static u8 iindpm_flag_count = 0;
    static u8 vindpm_flag_count = 0;
    static u8 regulation_status_count = 0;
    static u8 done_count = 10;
    u8 data = 0;
    u8 I1 = 0;
    u8 I2 = 0;
    u8 I3 = 0;
    struct delayed_work *charge_done_detect_work = NULL;
    struct cx2560x *cx = NULL;

    charge_done_detect_work = container_of(work, struct delayed_work, work);
    if (charge_done_detect_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        goto out;
    }
    cx = container_of(charge_done_detect_work, struct cx2560x, charge_done_detect_work);
    if (cx == NULL) {
        pr_err("Cann't get cx25890_device\n");
        goto out;
    }

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &val);
    if (ret) {
        goto out;
    }

    cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_THERMAL_MASK,
        REG0F_THERMAL_DISABLE_TREG << REG0F_THERMAL_SHIFT);

    val = (val & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;
    if (done_count > 0) {
        if (val == REG08_CHRG_STAT_CHGDONE && charge_done_flag == false) {
            cx2560x_disable_charger(cx);
            cx2560x_enable_charger(cx);
            done_count--;
        }
    } else {
        cx2560x_disable_charger(cx);
        count = 0;
        iindpm_flag_count = 0;
        vindpm_flag_count = 0;
        regulation_status_count = 0;
        done_count = 10;
        charge_done_flag = true;
        schedule_delayed_work(&cx->recharge_detect_work, 0);
        cancel_delayed_work(&cx->charge_done_detect_work);
        pr_err("%s: Charge Termination Done \n", __func__);
        return;
    }

    ibat = battery_get_bat_current();
    vbat = battery_get_bat_voltage();
    vbus = battery_get_vbus();

    if (vbus > 8000) {
        cx2560x_set_input_volt_limit(cx, 8400);
        cx2560x_update_bits(cx, CX2560X_REG_06, REG06_OVP_MASK,
            2 << REG06_OVP_SHIFT);
        cx2560x_write_reg40(cx, true);
        cx2560x_write_byte_without_retry(cx, 0x89, 0x3);
        cx2560x_write_byte_without_retry(cx, 0x41, 0x28);
        cx2560x_write_byte_without_retry(cx, 0x44, 0x18);
        cx2560x_write_byte_without_retry(cx, 0x83, 0x00);
        cx2560x_read_byte_without_retry(cx, 0x84, &data);
        I1 = (data & 0x60) >> 5;
        I2 = (data & 0x0C) >> 2;
        I3 = data & 0x03;
        if (I2 < 3) {
            I2++;
        } else if (I2 == 3 && I3 > 0 && I1 < 3) {
            I1++;
            I3--;
        }
        data &= 0x90;
        cx2560x_write_byte_without_retry(cx, 0x84, data | I1<<5 | I2<<2 | I3);
        cx2560x_read_byte_without_retry(cx, 0x84, &data);
        cx2560x_write_reg40(cx, false);

    } else {
        if (vbat < 4200) {
            cx2560x_set_input_volt_limit(cx, 4400);
        } else {
            vindpm_value = (vbat + 250);
            cx2560x_set_input_volt_limit(cx, vindpm_value);
        }
        cx2560x_write_reg40(cx, true);
        cx2560x_write_byte_without_retry(cx, 0x89, 0x3);
        cx2560x_write_reg40(cx, false);
    }

    cx2560x_update_bits(cx, CX2560X_REG_03, REG03_ITERM_MASK, 0x01);

    ret = cx2560x_read_byte(cx, CX2560X_REG_03, &val);
    if (ret) {
        goto out;
    }

    val &= REG03_ITERM_MASK;
    val_term = (val * 60) + 60;

    ret = cx2560x_read_byte(cx, CX2560X_REG_04, &val);
    if (ret) {
        goto out;
    }

    val = (val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
    val_vreg = val * 32 + 3856;

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_0A, &val);
    if (ret) {
        goto out;
    }

    iindpm_flag = (val & REG0A_IINDPM_STAT_MASK) >> REG0A_IINDPM_STAT_SHIFT;
    vindpm_flag = (val & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &val);
    if (ret) {
        goto out;
    }

    val = (val & REG08_THERM_STAT_MASK) >> REG08_THERM_STAT_SHIFT;
    regulation_status = val;

    pr_err("%s: ibat = %d, vbat = %d, val_vreg=%d val_term=%d iindpm_flag %d vindpm_flag %d regulation_status %d  reg84=%02x \n",
        __func__, ibat/10, vbat, val_vreg, val_term, iindpm_flag, vindpm_flag, regulation_status, data);
    if (val_vreg == 4240 && vbat >= val_vreg) {
            pr_err("%s: entry disable_charger val_vreg > 4240 \n", __func__);
            cx2560x_disable_charger(cx);
            count = 0;
            charge_done_flag = true;
            iindpm_flag_count = 0;
            vindpm_flag_count = 0;
            regulation_status_count = 0;
            schedule_delayed_work(&cx->recharge_detect_work, 0);
            cancel_delayed_work(&cx->charge_done_detect_work);
            return;
    } else {
        if ((val_vreg - 100) < vbat  && ibat/10 < 250) {
            if (iindpm_flag) {
                iindpm_flag_count++;
            }

            if (vindpm_flag) {
                vindpm_flag_count++;
            }

            if (regulation_status) {
                regulation_status_count++;
            }

            count++;
            if (count >= 4 && iindpm_flag_count < 3 && vindpm_flag_count < 3 && regulation_status_count < 3) {
                pr_err("%s: entry disable_charger\n", __func__);
                cx2560x_disable_charger(cx);
                count = 0;
                charge_done_flag = true;
                iindpm_flag_count = 0;
                vindpm_flag_count = 0;
                regulation_status_count = 0;
                schedule_delayed_work(&cx->recharge_detect_work, 0);
                cancel_delayed_work(&cx->charge_done_detect_work);
                return;
            } else if (count >= 4) {
                count = 0;
                iindpm_flag_count = 0;
                vindpm_flag_count = 0;
                regulation_status_count = 0;
            }
        } else {
            count = 0;
            iindpm_flag_count = 0;
            vindpm_flag_count = 0;
            regulation_status_count = 0;
        }
    }
out:
    if ((val_vreg - 100) < vbat  && ibat/10 < 600) {
        schedule_delayed_work(&cx->charge_done_detect_work, 2 * HZ);
    } else {
        schedule_delayed_work(&cx->charge_done_detect_work, 5 * HZ);
    }

    return;
}

static void recharge_detect_work_func(struct work_struct *work)
{
    int vbat;
    int val_vreg;
    unsigned char val;
    int ret = -1;

    struct delayed_work *recharge_detect_work = NULL;
    struct cx2560x *cx = NULL;

    recharge_detect_work = container_of(work, struct delayed_work, work);
    if (recharge_detect_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        goto out;
    }
    cx = container_of(recharge_detect_work, struct cx2560x, recharge_detect_work);
    if (cx == NULL) {
        pr_err("Cann't get cx25890_device\n");
        goto out;
    }

    vbat = battery_get_bat_voltage();
    ret = cx2560x_read_byte(cx, CX2560X_REG_04, &val);
    if (ret) {
        goto out;
    }

    val = (val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
    val_vreg = val * 32 + 3856;

    pr_err("%s: vbat = %d, val_vreg=%d\n", __func__, vbat, val_vreg);
    if (charge_done_flag) {
        if (val_vreg - 100 > vbat) {
            pr_err("%s: entry enable_charger\n", __func__);
            cx2560x_enable_charger(cx);
            charge_done_flag = false;
            schedule_delayed_work(&cx->charge_done_detect_work, 0);
            cancel_delayed_work(&cx->recharge_detect_work);
            return;
        }
        else {
            schedule_delayed_work(&cx->recharge_detect_work, 5 * HZ);
            return;
        }
    }

out:
    schedule_delayed_work(&cx->recharge_detect_work, 5 * HZ);
    return;
}
/* A06 code for AL7160A-2223 by zhangziyi at 20240531 end */

/* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
static void charger_usb_detect_work_func(struct work_struct *work)
{
	struct delayed_work *charge_usb_detect_work = NULL;
	struct cx2560x *cx = NULL;
	int i = 0;
	/* A06 code for P240514-06636 by xiongxiaoliang at 20240520 start */
	static bool float_first_check = true;
	int float_check_num = 3;

	charge_usb_detect_work = container_of(work, struct delayed_work, work);
	if (charge_usb_detect_work == NULL) {
		pr_err("Cann't get charge_usb_detect_work\n");
		return ;
	}
	cx = container_of(charge_usb_detect_work, struct cx2560x, charge_usb_detect_work);
	if (cx == NULL) {
		pr_err("Cann't get cx2560x_device\n");
		return;
	}
	usb_detect_flag = true;

	if (float_first_check == true) {
		float_first_check = false;
		float_check_num = 30;
	}
	pr_err("%s: enter float_check_num[%d]\n", __func__, float_check_num);

	if ((!is_kernel_power_off_charging()) && (gadget_info_is_connected() == 0)) {
		do {
			if (gadget_info_is_connected() != 0) {
				no_usb_flag = 0;
				break;
			}
			msleep(1000);
			no_usb_flag = 1;
			i++;
		} while (i < float_check_num);
			pr_err("%s: SDP retry:%d\n", __func__, i);
	} else {
		no_usb_flag = 0;
	}
	/* A06 code for P240514-06636 by xiongxiaoliang at 20240520 end */
	pr_err("%s: exit\n", __func__);

	usb_detect_done = true;

	schedule_delayed_work(&cx->charge_detect_delayed_work, 0);
	return;
}

static void hiz_detect_type_func(struct work_struct *work)
{
    struct delayed_work *detect_type_work = NULL;
    struct cx2560x *cx = NULL;
    u8 reg_val = 0;
    int ret = 0;
    int chg_type = CHARGER_UNKNOWN;

    detect_type_work = container_of(work, struct delayed_work, work);
    if (detect_type_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return;
    }

    cx = container_of(detect_type_work, struct cx2560x, detect_type_work);
    if (cx == NULL) {
        pr_err("Cann't get cx25890_device\n");
        return;
    }

    pr_err("%s \n", __func__);
    cx2560x_dump_regs(cx);
    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_00, &reg_val);
    if (reg_val & 0x80) {
        pr_err("%s detect hiz mode\n", __func__);

        switch (cx->prev_hiz_type) {
            case REG08_VBUS_TYPE_NONE:
                chg_type = CHARGER_UNKNOWN;
                break;
            case REG08_VBUS_TYPE_SDP:
                chg_type = STANDARD_HOST;
                break;
            case REG08_VBUS_TYPE_CDP:
                chg_type = CHARGING_HOST;
                break;
            case REG08_VBUS_TYPE_DCP:
                chg_type = STANDARD_CHARGER;
                break;
            case REG08_VBUS_TYPE_UNKNOWN:
                chg_type = NONSTANDARD_CHARGER;
                break;
            case REG08_VBUS_TYPE_NON_STD:
                chg_type = NONSTANDARD_CHARGER;
                break;
            default:
                chg_type = NONSTANDARD_CHARGER;
                break;
        }
        cancel_delayed_work(&cx->charge_usb_detect_work);

        if (chg_type != STANDARD_CHARGER) {
            Charger_Detect_Release();
        }

        cx->psy_usb_type = chg_type;
        schedule_delayed_work(&cx->psy_dwork, 0);
    }

    return;
}
/* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

static int cx2560x_charger_remove(struct i2c_client *client);
static int cx2560x_charger_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct cx2560x *cx;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;

    int ret = 0;
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    u8 reg_val = 0;
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

    pr_info("%s: start\n", __func__);

    cx = devm_kzalloc(&client->dev, sizeof(struct cx2560x), GFP_KERNEL);
    if (!cx)
        return -ENOMEM;

    client->addr = 0x6B;
    cx->dev = &client->dev;
    cx->client = client;

    i2c_set_clientdata(client, cx);

    mutex_init(&cx->i2c_rw_lock);

    ret = cx2560x_detect_device(cx);
    if (ret) {
        pr_err("No cx2560x device found!\n");
        return -ENODEV;
    }

    match = of_match_node(cx2560x_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    if (cx->part_no == 0x07)
    {
        pr_info("CX2560X part number match success \n");
    } else {
        pr_info("CX2560X part number match fail \n");
        cx2560x_charger_remove(client);
        return -EINVAL;
    }

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    INIT_DELAYED_WORK(&cx->psy_dwork, cx2560x_inform_psy_dwork_handler);
    INIT_DELAYED_WORK(&cx->prob_dwork, cx2560x_inform_prob_dwork_handler);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    INIT_DELAYED_WORK(&cx->charge_detect_delayed_work, cx2560x_charger_detect_work_func);
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 start */
    cx->bypass_chgdet_en = false;
    /* hs14 code for P221116-03489 by wenyaqi at 2022/11/23 end */
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 start*/
    cx->icl_ma = 0;
    /*A06 code for AL7160A-1630 by shixuanxuan at 20240615 end*/
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    INIT_DELAYED_WORK(&cx->charge_usb_detect_work, charger_usb_detect_work_func);
    INIT_DELAYED_WORK(&cx->charge_done_detect_work, charge_done_detect_work_func);
    INIT_DELAYED_WORK(&cx->recharge_detect_work, recharge_detect_work_func);
    INIT_DELAYED_WORK(&cx->set_hiz_work, set_hiz_func);
    INIT_DELAYED_WORK(&cx->detect_type_work, hiz_detect_type_func);
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

    cx->platform_data = cx2560x_parse_dt(node, cx);

    if (!cx->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = cx2560x_init_device(cx);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    cx2560x_register_interrupt(cx);


    pr_err("%s cx->chg_dev_name = %s" ,__func__ ,cx->chg_dev_name);
    cx->chg_dev = charger_device_register(cx->chg_dev_name,
                            &client->dev, cx,
                            &cx2560x_chg_ops,
                            &cx2560x_chg_props);
    if (IS_ERR_OR_NULL(cx->chg_dev)) {
        /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
        cancel_delayed_work_sync(&cx->prob_dwork);
        cancel_delayed_work_sync(&cx->psy_dwork);
        /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */
        ret = PTR_ERR(cx->chg_dev);
        return ret;
    }

    ret = sysfs_create_group(&cx->dev->kobj, &cx2560x_attr_group);
    if (ret)
        dev_err(cx->dev, "failed to register sysfs. err: %d\n", ret);

    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    Charger_Detect_Init();

    mod_delayed_work(system_wq, &cx->prob_dwork,
                    msecs_to_jiffies(2*1000));
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    pr_err("cx2560x probe successfully, Part Num:%d\n!",
            cx->part_no);

    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
    chg_info = CX2560X;
    /* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 end */
    /* A06 code for AL7160A-119 by liufurong at 20240425 start */
    gs_usb_detect = 1;
    /* A06 code for AL7160A-119 by liufurong at 20240425 end */
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 start */
    ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_0A, &reg_val);

    reg_val = (reg_val & REG0A_VBUS_GD_MASK) >> REG0A_VBUS_GD_SHIFT;

    if (reg_val) {
        pr_err("cx2560x_charger_probe gd = 1 \n!");
        schedule_delayed_work(&cx->charge_done_detect_work, 0);
        is_probe_detect_usb = true;
        schedule_delayed_work(&cx->charge_detect_delayed_work, 3 * HZ);
    } else {
        is_probe_detect_usb = false;
    }
    /* A06 code for AL7160A-608 by zhangziyi at 20240507 end */

    return ret;
/* AL6528A code for SR-AL6528A-01-303 by wenyaqi at 2022/08/31 end */
}

static int cx2560x_charger_remove(struct i2c_client *client)
{
    struct cx2560x *cx = i2c_get_clientdata(client);

    mutex_destroy(&cx->i2c_rw_lock);

    sysfs_remove_group(&cx->dev->kobj, &cx2560x_attr_group);

    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    cancel_delayed_work_sync(&cx->charge_detect_delayed_work);
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 start */
    cancel_delayed_work_sync(&cx->prob_dwork);
    cancel_delayed_work_sync(&cx->psy_dwork);
    /* hs14 code for SR-AL6528A-01-306 by gaozhengwei at 2022/09/06 end */

    return 0;
}

static void cx2560x_charger_shutdown(struct i2c_client *client)
{
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 start */
    struct cx2560x *cx = i2c_get_clientdata(client);

    mutex_destroy(&cx->i2c_rw_lock);

    sysfs_remove_group(&cx->dev->kobj, &cx2560x_attr_group);

    cancel_delayed_work_sync(&cx->charge_detect_delayed_work);
    cancel_delayed_work_sync(&cx->prob_dwork);
    cancel_delayed_work_sync(&cx->psy_dwork);
	cx2560x_set_input_volt_limit(cx, 4400);
    /* hs14 code for AL6528A-1033 by gaozhengwei at 2022/12/09 end */
}

static struct i2c_driver cx2560x_charger_driver = {
    .driver = {
            .name = "cx2560x-charger",
            .owner = THIS_MODULE,
            .of_match_table = cx2560x_charger_match_table,
            },

    .probe = cx2560x_charger_probe,
    .remove = cx2560x_charger_remove,
    .shutdown = cx2560x_charger_shutdown,

};

module_i2c_driver(cx2560x_charger_driver);

MODULE_DESCRIPTION("CX2560X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SGM");