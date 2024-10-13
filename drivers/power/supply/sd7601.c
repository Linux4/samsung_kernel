/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": %s :"fmt,__func__

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/reboot.h>
#include <linux/power_supply.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
//#include <mt-plat/charger_type.h>
#include "sd7601.h"
//#include <mt-plat/charger_type.h>
//#include "mtk_charger_intf.h"
#include <linux/errno.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif
//#include <mtk_musb.h>
//#include <charger_class.h>
//#include <mtk_charger.h>
#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/charger_class.h>


#include <mt-plat/upmu_common.h>

#include <mt-plat/v1/mtk_charger.h>
#include <linux/hardware_info.h>
//#include "mtk_charger_intf.h"

//#define CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
enum sd7601_port_stat {
	SD7601_PORTSTAT_NO_INPUT = 0,
	SD7601_PORTSTAT_SDP,
	SD7601_PORTSTAT_CDP,
	SD7601_PORTSTAT_DCP,
	SD7601_PORTSTAT_HVDCP,
	SD7601_PORTSTAT_UNKNOWN,
	SD7601_PORTSTAT_NON_STANDARD,
	SD7601_PORTSTAT_OTG,
//	SD7601_PORTSTAT_MAX,
};

enum sd7601_usbsw_state {
	SD7601_USBSW_CHG = 0,
	SD7601_USBSW_USB,
};

struct sd7601_chip {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct i2c_client * client;
	struct mutex i2c_rw_lock;
	struct mutex adc_lock;
	struct mutex irq_lock;
	const char *chg_dev_name;
	struct delayed_work sd7601_work;
//	struct delayed_work sd7601_chg_work;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	struct mutex bc12_lock;
	struct delayed_work psy_dwork;
	atomic_t vbus_gd;
	bool attach;
	enum sd7601_port_stat port;
	enum charger_type chg_type;
	struct power_supply *psy;
	wait_queue_head_t bc12_en_req;
	atomic_t bc12_en_req_cnt;
	struct task_struct *bc12_en_kthread;
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	struct power_supply_desc psy_desc;
	struct power_supply *sd7601_psy;
	struct power_supply *otg_psy;
	int32_t	component_id;
	int32_t  intr_gpio;
	int32_t  irq;
	int32_t  ce_gpio;
	uint32_t chg_state;

	uint32_t ilimit_ma;
	uint32_t vlimit_mv;
	uint32_t cc_ma;
	uint32_t cv_mv;
	uint32_t pre_ma;
	uint32_t eoc_ma;
	uint32_t rechg_mv;
	uint32_t vsysmin_mv;
	uint32_t target_hv;
	int32_t vbat0_flag;
};

struct sd7601_irq_info{
	const char	*irq_name;
	int32_t		(*irq_func)(struct sd7601_chip *chip,uint8_t rt_stat);
	uint8_t 		irq_mask;
	uint8_t		irq_shift;
};
struct sd7601_irq_handle{
	uint8_t reg;
	uint8_t value;
	uint8_t pre_value;
	struct sd7601_irq_info irq_info[5];
};

const static char* vbus_type[] = {
	"no input",
	"sdp",
	"cdp",
	"dcp",
	"hvdcp",
	"unknown",
	"non-standard",
	"otg"
};
const static char* charge_state[] = {
	"not charging",
	"pre-charge",
	"fast charging",
	"charge termination"
};
//for SD7601AD only
const static char* adc_channel[] = {
	"vbat",
	"vsys",
	"tbat",
	"vbus",
	"ibat",
	"ibus",
};
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg begin
struct sd7601_chip *otg_chip = NULL;
bool sd7601_is_loaded = false;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg end
#define sd7601_dbg pr_err

static int32_t sd7601_read_byte(struct sd7601_chip *chip, uint8_t reg, uint8_t *data)
{
	int32_t ret = 0;
	int32_t i = 0;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> abnomal boot begin	
	usleep_range(1000,2000);
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> abnomal boot end	
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_read_byte_data(chip->client, reg);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, reg, ret, i);
	}
	mutex_unlock(&chip->i2c_rw_lock);

	if(i >= 4)
	{
		*data = 0;
		return -1;
	}
	*data = (uint8_t)ret;

	return 0;
}

static int32_t sd7601_write_byte(struct sd7601_chip *chip, uint8_t reg, uint8_t val)
{
	int32_t ret = 0;
	int32_t i = 0;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg begin	
	usleep_range(1000,2000);
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg end	
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_write_byte_data(chip->client, reg, val);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, reg, ret, i);
	}
	mutex_unlock(&chip->i2c_rw_lock);
	return (i>=4) ? -1 : 0;
}
static int32_t sd7601_update(struct sd7601_chip *chip, uint8_t RegNum, uint8_t val, uint8_t MASK,uint8_t SHIFT)
{
	uint8_t data = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,RegNum, &data);

	if(ret < 0)
		return ret;

	if((data & (MASK << SHIFT)) != (val << SHIFT))
	{
		data &= ~(MASK << SHIFT);
		data |= (val << SHIFT);
		return sd7601_write_byte(chip,RegNum, data);
	}
	return 0;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int32_t sd7601_set_en_hiz(struct sd7601_chip *chip, int32_t enable)
{
	return sd7601_update(chip,SD7601_R00,!!enable,CON0_EN_HIZ_MASK,CON0_EN_HIZ_SHIFT);
}
static int32_t sd7601_get_en_hiz(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R00,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON0_EN_HIZ_MASK << CON0_EN_HIZ_SHIFT)) >> CON0_EN_HIZ_SHIFT;	
	return !!data_reg;
}

/* +S96818AA1-6675, churui1.wt, ADD, 20230614, added pg_state judgment logic */
static int32_t sd7601_get_power_good(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0, index = 0;
	int32_t ret = 0;

	for (index = 0; index <= 30; ++index) {
		ret = sd7601_read_byte(chip, SD7601_R08, &data_reg);
		if (ret < 0) {
			pr_err("get pg_state fail(%d)\n", ret);
			return ret;
		}
		data_reg = (data_reg & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT;
		if (data_reg)
			break;

		msleep(100);
	}

	return data_reg;
}
/* -S96818AA1-6675, churui1.wt, ADD, 20230614, added pg_state judgment logic */

/* +churui1.wt, ADD, 20230519, set the hiz mode */
static int32_t sd7601_enable_hiz(struct charger_device* chg_dev , bool enable)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);

/* +S96818AA1-6675, churui1.wt, ADD, 20230614, added pg_state judgment logic */
	if (enable) {
		data_reg = sd7601_get_power_good(chip);
		if (data_reg == 0) {
			pr_err("pg_state error!!!\n");
		}
	}
/* +S96818AA1-6675, churui1.wt, ADD, 20230614, added pg_state judgment logic */

	ret = sd7601_set_en_hiz(chip, enable);
	return ret;
}
/*-churui1.wt, ADD, 20230519, set the hiz mode */

static int32_t sd7601_set_input_curr_lim(struct sd7601_chip *chip, uint32_t ilimit_ma)
{
	uint8_t  data_reg = 0;
	uint32_t input_current = 0; 

	if (ilimit_ma < INPUT_CURRT_MIN)
		input_current = INPUT_CURRT_MIN;
	else if (ilimit_ma > INPUT_CURRT_MAX)
		input_current = INPUT_CURRT_MAX;
	else
		input_current = ilimit_ma;

	data_reg = (input_current - INPUT_CURRT_MIN) / INPUT_CURRT_STEP;

	return sd7601_update(chip,SD7601_R00,data_reg,CON0_IINLIM_MASK,CON0_IINLIM_SHIFT);
}
static int32_t sd7601_get_input_curr_lim(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R00,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON0_IINLIM_MASK << CON0_IINLIM_SHIFT)) >> CON0_IINLIM_SHIFT;

	return (data_reg * INPUT_CURRT_STEP + INPUT_CURRT_MIN);
}
static int32_t sd7601_get_iterm(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R03,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON3_ITERM_MASK<< CON3_ITERM_SHIFT)) >> CON3_ITERM_SHIFT;

	return (data_reg * EOC_CURRT_STEP + EOC_CURRT_MIN);
}
static int32_t sd7601_set_iterm(struct sd7601_chip *chip, uint32_t iterm)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (iterm < EOC_CURRT_MIN)
		data_temp = EOC_CURRT_MIN;
	else if (iterm > EOC_CURRT_MAX)
		data_temp = EOC_CURRT_MAX;
	else
		data_temp = iterm;

	data_reg = (data_temp - EOC_CURRT_MIN)/ EOC_CURRT_STEP;

	return sd7601_update(chip,SD7601_R03,data_reg,CON3_ITERM_MASK,CON3_ITERM_SHIFT);
}

static int32_t sd7601_set_wd_reset(struct sd7601_chip *chip)
{
	return sd7601_update(chip,SD7601_R01,1,CON1_WD_MASK,CON1_WD_SHIFT);
}

static int32_t sd7601_set_otg_en(struct sd7601_chip *chip, int32_t enable)
{
	return sd7601_update(chip,SD7601_R01,!!enable,CON1_OTG_CONFIG_MASK,CON1_OTG_CONFIG_SHIFT);
}

static int32_t sd7601_set_charger_en(struct sd7601_chip *chip, int32_t enable)
{
	return sd7601_update(chip,SD7601_R01,!!enable,CON1_CHG_CONFIG_MASK,CON1_CHG_CONFIG_SHIFT);
}
static int32_t sd7601_get_charger_en(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R01,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON1_CHG_CONFIG_MASK << CON1_CHG_CONFIG_SHIFT)) >> CON1_CHG_CONFIG_SHIFT;

	return !!data_reg;
}
static int32_t sd7601_operate_state1(struct sd7601_chip *chip, int32_t enable)
{
	uint8_t i    = 0;
	int32_t ret  = 0;
	uint8_t data = 0;
	uint8_t check_val = 0;

	for(i=0;i<5;i++)
	{
		if(!!enable)
		{
			sd7601_write_byte(chip,0xE0,0x64);
			sd7601_write_byte(chip,0xE0,0x62);
			sd7601_write_byte(chip,0xE0,0x67);
			check_val = 0x03;
		}
		else
		{
			sd7601_write_byte(chip,0xE0,0x00);
			check_val = 0;
		}

		ret = sd7601_read_byte(chip,0xE0,&data);
		//sd7601_info("check: ret 0x%02X, data = 0x%02X\n",ret,data);
		if((0 == ret) && (check_val == data))
		{
		//	sd7601_info("%s success\n",(!!enable) ? "enter":"exit");
			return 0;
		}
		else
		{
			//sd7601_info("%s failed,retry %d\n",(!!enable) ? "enter":"exit",i);
			usleep_range(10000,20000);
		}
	}
	return -1;	
}

static int32_t sd7601_operate_state2(struct sd7601_chip *chip, int32_t enable)
{
	int32_t ret  = 0;
	uint8_t i    = 0;
	uint8_t data = 0;
	uint8_t val  = (!!enable) ? 0x00 : 0x08;
	for(i=0;i<5;i++)
	{
		//sd7601_info("%s write 0x%02X to 0xE3\n",(!!enable) ? "enable" : "disable",val);
		sd7601_write_byte(chip,0xE3,val);

		ret = sd7601_read_byte(chip,0xE3,&data);
		//sd7601_info("check: ret 0x%02X, 0xE3 = 0x%02X\n",ret,data);
		if((0 == ret) && (val == data))
		{
		//	sd7601_info("%s success\n",(!!enable) ? "enable":"disable");
			return 0;
		}
		else
		{
			//sd7601_info("%s failed,retry %d\n",(!!enable) ? "enable":"disable",i);
			usleep_range(10000,20000);
		}
	}
	return -1;
}
static int32_t sd7601_set_sys_min_volt(struct sd7601_chip *chip, uint32_t vsys_mv)
{
	uint8_t  data_reg = 0;
	uint32_t vsys = 0; 

	if (vsys_mv < CON1_SYS_V_LIMIT_MIN)
		vsys = CON1_SYS_V_LIMIT_MIN;
	else if (vsys_mv > CON1_SYS_V_LIMIT_MAX)
		vsys = CON1_SYS_V_LIMIT_MAX;
	else
		vsys = vsys_mv;

	data_reg = (vsys - CON1_SYS_V_LIMIT_MIN) / CON1_SYS_V_LIMIT_STEP;

	return sd7601_update(chip,SD7601_R01,data_reg,CON1_SYS_V_LIMIT_MASK,CON1_SYS_V_LIMIT_SHIFT);
}

static int32_t sd7601_set_boost_ilim(struct sd7601_chip *chip, uint32_t boost_ilimit)
{
	uint8_t data_reg = 0;
	
	if(boost_ilimit <= 500)			
		data_reg = 0x00;
	else 
		data_reg = 0x01;

	return sd7601_update(chip,SD7601_R02,data_reg,CON2_BOOST_ILIM_MASK,CON2_BOOST_ILIM_SHIFT);
}
static int32_t sd7601_get_boost_ilim(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_BOOST_ILIM_MASK << CON2_BOOST_ILIM_SHIFT)) >> CON2_BOOST_ILIM_SHIFT;

	return (1 == data_reg) ? 1200 : 500;
}
static int32_t sd7601_set_ichg_current(struct sd7601_chip *chip, uint32_t ichg)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0; 
	uint8_t  r08_data = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R08,&r08_data);
	//pr_err("read reg 0x%02X ,ret = %d,data = 0x%02d\n",SD7601_R08,ret,r08_data);
	if(ret < 0 || (r08_data & 0x01))
		ichg = 1020;

	if (ichg < ICHG_CURR_MIN)
		data_temp = ICHG_CURR_MIN;
	else if (ichg > ICHG_CURR_MAX)
		data_temp = ICHG_CURR_MAX;
	else
		data_temp = ichg;

	data_reg = (data_temp - ICHG_CURR_MIN)/ ICHG_CURR_STEP;

	return sd7601_update(chip,SD7601_R02,data_reg,CON2_ICHG_MASK,CON2_ICHG_SHIFT);
}
static int32_t sd7601_get_ichg_current(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_ICHG_MASK << CON2_ICHG_SHIFT)) >> CON2_ICHG_SHIFT;

	return (data_reg * ICHG_CURR_STEP + ICHG_CURR_MIN);
}
// /* CON3---------------------------------------------------- */

static int32_t sd7601_set_iprechg_current(struct sd7601_chip *chip, uint32_t iprechg_curr)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0; 

	if (iprechg_curr < IPRECHG_CURRT_MIN)
		data_temp = IPRECHG_CURRT_MIN;
	else if (iprechg_curr > IPRECHG_CURRT_MAX)
		data_temp = IPRECHG_CURRT_MAX;
	else
		data_temp = iprechg_curr;

	data_reg = (data_temp - IPRECHG_CURRT_MIN)/ IPRECHG_CURRT_STEP;

	return sd7601_update(chip,SD7601_R03,data_reg,CON3_IPRECHG_MASK,CON3_IPRECHG_SHIFT);
}
// /* CON4---------------------------------------------------- */
static int32_t sd7601_set_vreg_volt(struct sd7601_chip *chip, uint32_t vreg_chg_vol)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (vreg_chg_vol < VREG_VOL_MIN)
		data_temp = VREG_VOL_MIN;
	else if (vreg_chg_vol > VREG_VOL_MAX)
		data_temp = VREG_VOL_MAX;
	else
		data_temp = vreg_chg_vol;

	data_reg = (data_temp - VREG_VOL_MIN)/ VREG_VOL_STEP;

	return sd7601_update(chip,SD7601_R04,data_reg,CON4_VREG_MASK,CON4_VREG_SHIFT);
}
static int32_t sd7601_get_vreg_volt(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R04,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON4_VREG_MASK<< CON4_VREG_SHIFT)) >> CON4_VREG_SHIFT;

	return (data_reg * VREG_VOL_STEP + VREG_VOL_MIN);
}
static int32_t sd7601_set_rechg_volt(struct sd7601_chip *chip, uint32_t rechg_volt)
{
	uint8_t data_reg = 0;
	
	if(rechg_volt >= 200)			
		data_reg = 0x01;
	else
		data_reg = 0x00;

	return sd7601_update(chip,SD7601_R04,data_reg,CON4_VRECHG_MASK,CON4_VRECHG_SHIFT);
}
// /* CON5---------------------------------------------------- */
static int32_t sd7601_set_en_term_chg(struct sd7601_chip *chip, int32_t enable)
{
	return sd7601_update(chip,SD7601_R05,!!enable,CON5_EN_TERM_CHG_MASK,CON5_EN_TERM_CHG_SHIFT);
}
static int32_t sd7601_set_wd_timer(struct sd7601_chip *chip, uint32_t second)
{
	uint8_t data_reg = 0;

	if(second < 40)		//second < 40s, disable wdt
		data_reg = 0x00;
	else if(second < 80)	//40 <= second < 80, set 40s 
		data_reg = 0x01;
	else if(second < 160)//80 <= second < 160, set 80s
		data_reg = 0x02;
	else 
		data_reg = 0x03;	//second >= 160, set 160s

	return sd7601_update(chip,SD7601_R05,data_reg,CON5_WTG_TIM_SET_MASK,CON5_WTG_TIM_SET_SHIFT);
}
static int32_t sd7601_set_en_chg_timer(struct sd7601_chip *chip, int32_t enable)
{
	return sd7601_update(chip,SD7601_R05,!!enable,CON5_EN_TIMER_MASK,CON5_EN_TIMER_SHIFT);
}
static int32_t sd7601_get_en_chg_timer(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R05,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON5_EN_TIMER_MASK << CON5_EN_TIMER_SHIFT)) >> CON5_EN_TIMER_SHIFT;

	return !!data_reg;
} 
// /* CON6---------------------------------------------------- */

static int32_t sd7601_get_vlimit(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;
	uint32_t vlimit = 0;

	ret = sd7601_read_byte(chip,SD7601_R10,&data_reg);
	if(ret < 0)
		return ret;

	if(data_reg & (CON10_HV_VINDPM_MASK << CON10_HV_VINDPM_SHIFT))
	{
		data_reg = (data_reg & (CON10_VLIMIT_MASK << CON10_VLIMIT_SHIFT)) >> CON10_VLIMIT_SHIFT;
		vlimit =  (data_reg * VILIMIT_VOL_STEP+ VILIMIT_VOL_MIN);
	}
	else
	{
		data_reg = 0;
		ret = sd7601_read_byte(chip,SD7601_R06,&data_reg);
		if(ret < 0)
			return ret;

		data_reg = (data_reg & (CON6_VINDPM_MASK << CON6_VINDPM_SHIFT)) >> CON6_VINDPM_SHIFT;
		vlimit = (data_reg * VINDPM_VOL_STEP+ VINDPM_VOL_MIN);
	}
	return vlimit;
}
static int32_t sd7601_set_vlimit(struct sd7601_chip *chip, uint32_t vlimit_mv)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0; 
	int32_t ret = 0;
	if (vlimit_mv < VILIMIT_VOL_MIN)
		data_temp = VILIMIT_VOL_MIN;
	else if (vlimit_mv > VILIMIT_VOL_MAX)
		data_temp = VILIMIT_VOL_MAX;
	else
		data_temp = vlimit_mv;

	data_reg = (data_temp - VILIMIT_VOL_MIN)/ VILIMIT_VOL_STEP;

	//enable HV_VINDPM
	ret = sd7601_update(chip,SD7601_R10,1,CON10_HV_VINDPM_MASK,CON10_HV_VINDPM_SHIFT);
	if(ret < 0)
		return ret;

	return sd7601_update(chip,SD7601_R10,data_reg,CON10_VLIMIT_MASK,CON10_VLIMIT_SHIFT);
}
// /* CON8---------------------------------------------------- */
static int32_t sd7601_get_charger_status(struct sd7601_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R08,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT;
	
	return data_reg;
}
static int32_t sd7601_get_adc_value(struct sd7601_chip *chip,enum sd7601_adc_channel adc)
{
	uint8_t val = 0, pg_good = 0;
	int32_t ret = 0, wait_cnt = 0, adc_value = 0, ilimit = 0;
	
	if((SD7601AD_COMPINENT_ID != chip->component_id) && (SD7601AD_COMPINENT_ID2 != chip->component_id))
		return -1;

	mutex_lock(&chip->adc_lock);
#if 1
	// one shot ac conversion
	ret = sd7601_write_byte(chip,SD7601_R11,0x00);
	if(ret < 0)
		goto error;
	//start adc conversion
	ret = sd7601_update(chip,SD7601_R11,1,CON11_START_ADC_MASK,CON11_START_ADC_SHIFT);
	if(ret < 0)
		goto error;
	do
	{
		usleep_range(10000,20000);
		wait_cnt++;
		ret = sd7601_read_byte(chip,SD7601_R11,&val);
		if(ret < 0)
			goto error;
	}while((val & (CON11_START_ADC_MASK << CON11_START_ADC_SHIFT)) && wait_cnt < 100);
//	pr_err("wait cnt %d\n",wait_cnt);
#else
	//start 1s continuous conversion
	ret = sd7601_update(chip,SD7601_R11,1,CON11_CONV_RATE_MASK,CON11_CONV_RATE_SHIFT);
	if(ret < 0)
		goto error;
#endif
	ret = sd7601_read_byte(chip,SD7601_R08,&val);
	if(ret < 0)
		goto error;
	
	pg_good = (val & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT;

	if(wait_cnt < 100)
	{
		ret = sd7601_read_byte(chip,SD7601_ADC_CHANNEL_START + adc,&val);
		if(ret < 0)
			goto error;
		switch(adc)
		{
			case vbat_channel: 	
			case vsys_channel:	
				adc_value = 2304 + val * 20; 
				break;

			case tbat_channel:
				adc_value = 21000 + 465 * val;
				break;

			case vbus_channel:
				adc_value = (0 == val) ? 0 : (2600 + val * 100);
				break;

			case ibat_channel:
				adc_value = val * 25;
				break;

			case ibus_channel:
				if(1 == pg_good)
				{
					ilimit = sd7601_get_input_curr_lim(chip);
					if(ilimit < 0)
						goto error;
					adc_value = val * ilimit / 127;
				}
				else
					adc_value = 0;
				break;

			default:
				adc_value = 0;
				break;
		}
	}
	else
		goto error;
	pr_err("pg_good : %d -- %s[0x%02X]: Regval=0x%02X, Adcval=%d\n",pg_good,adc_channel[adc],SD7601_ADC_CHANNEL_START + adc,val,adc_value);
	mutex_unlock(&chip->adc_lock);
	return adc_value;

error:
	mutex_unlock(&chip->adc_lock);
	return -1;
}
static int32_t sd7601_dump_msg(struct sd7601_chip * chip)
{
	uint8_t i = 0;
	uint8_t data[SD7601_REG_NUM] = { 0 };
	int32_t ret = 0;

	for (i = 0; i < SD7601_REG_NUM; i++) 
	{
		ret = sd7601_read_byte(chip,i, &data[i]);
		if (ret < 0) 
		{
			pr_err("i2c transfor error\n");
			return -1;
		}
	}

	pr_err("[0x00]=0x%02X, [0x01]=0x%02X, [0x02]=0x%02X, [0x03]=0x%02X, [0x04]=0x%02X, [0x05]=0x%02X, [0x06]=0x%02X, [0x07]=0x%02X\n",
			 data[0x00],    data[0x01],    data[0x02],    data[0x03],    data[0x04],    data[0x05],    data[0x06],    data[0x07]);

	pr_err("[0x08]=0x%02X, [0x09]=0x%02X, [0x0A]=0x%02X, [0x0B]=0x%02X, [0x0C]=0x%02X, [0x0D]=0x%02X, [0x0E]=0x%02X, [0x0F]=0x%02X\n",
			data[0x08],	 data[0x09],	data[0x0A],	data[0x0B],	data[0x0C],	data[0x0D],	data[0x0E],	data[0x0F]);

	pr_err("[0x10]=0x%02X\n",data[0x10]);

	//charge parameter
	pr_err("vchg:%d, ichg:%d, ilimit:%d, iterm %d, vlimit %d, sftimer %d, iboost %d\n",
		sd7601_get_vreg_volt(chip),
		sd7601_get_ichg_current(chip),
		sd7601_get_input_curr_lim(chip),
		sd7601_get_iterm(chip),
		sd7601_get_vlimit(chip),
		sd7601_get_en_chg_timer(chip),
		sd7601_get_boost_ilim(chip)
	);
	//fault and state
	pr_err("hiz:%d, chg_en:%d, chg_state:%s, vbus_type:%s, pg:%d, thm:%d, vsys_state:%d\n",
		sd7601_get_en_hiz(chip),
		sd7601_get_charger_en(chip),
		charge_state[(data[SD7601_R08] & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT],
		vbus_type[(data[SD7601_R08] & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT],
		(data[SD7601_R08] & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT,
		(data[SD7601_R08] & (CON8_THM_STAT_MASK << CON8_THM_STAT_SHIFT)) >> CON8_THM_STAT_SHIFT,
		(data[SD7601_R08] & (CON8_VSYS_STAT_MASK << CON8_VSYS_STAT_SHIFT)) >> CON8_VSYS_STAT_SHIFT
	);

	pr_err("fault:%d, vbus_gd:%d, vdpm:%d, idpm:%d, acov:%d\n",
		data[SD7601_R09],
		(data[SD7601_R0A] & (CONA_VBUS_GD_MASK << CONA_VBUS_GD_SHIFT)) >> CONA_VBUS_GD_SHIFT,
		(data[SD7601_R0A] & (CONA_VINDPM_STAT_MASK << CONA_VINDPM_STAT_SHIFT)) >> CONA_VINDPM_STAT_SHIFT,
		(data[SD7601_R0A] & (CONA_IDPM_STAT_MASK << CONA_IDPM_STAT_SHIFT)) >> CONA_IDPM_STAT_SHIFT,
		(data[SD7601_R0A] & (CONA_ACOV_STAT_MASK << CONA_ACOV_STAT_SHIFT)) >> CONA_ACOV_STAT_SHIFT
	);

	for (i = vbat_channel; i < max_channel; i++) 
		sd7601_get_adc_value(chip,i);

	return 0;
}
static int32_t sd7601_set_dpdm(struct sd7601_chip *chip,uint8_t dp_val, uint8_t dm_val)
{
	uint8_t data_reg = 0;
	data_reg  = (dp_val & CONC_DP_VOLT_MASK) << CONC_DP_VOLT_SHIFT;
	data_reg |= (dm_val & CONC_DM_VOLT_MASK) << CONC_DM_VOLT_SHIFT;
	//pr_err("data_reg 0x%02X\n",data_reg);
	return sd7601_update(chip,SD7601_R0C,data_reg,CONC_DP_DM_MASK,CONC_DP_DM_SHIFT);
}
static int32_t sd7601_set_dp_for_afc(struct sd7601_chip *chip)
{
    int32_t ret = 0;

    ret = sd7601_update(chip,SD7601_R0C,CONC_DP_DM_VOL_0P6,CONC_DP_VOLT_MASK,CONC_DP_VOLT_SHIFT);
    if (ret) {
		pr_err("sd7601_set_dp failed(%d)\n", ret);
	}

	return ret;
}
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
static int32_t sd7601_charging_set_hvdcp20(struct sd7601_chip *chip,uint32_t vbus_target)
{
	int32_t ret = 0;
	//5V,and D+ = 0.6 to wake up adapter
	ret = sd7601_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P0);
	if(ret < 0)
		return ret;
	//must wait adapter
	msleep(1250);

	if(9 == vbus_target)
		ret = sd7601_set_dpdm(chip,CONC_DP_DM_VOL_3P3,CONC_DP_DM_VOL_0P6);
	else if(12 == vbus_target)
		ret = sd7601_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P6);

 	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_chg_type_det(struct sd7601_chip *chip,uint8_t en)
{
	int32_t ret = 0;
	uint8_t val = 0;
	int32_t wait_cnt = 0;
	uint8_t chg_type = 0;

	if(0 == en)
		return 0;

	ret = sd7601_update(chip,SD7601_R07,1,FORCE_IINDET_MASK,FORCE_IINDET_SHIFT);
	if(ret < 0)
		return ret;
	do
	{
		msleep(100);
		wait_cnt++;
		ret = sd7601_read_byte(chip,SD7601_R07,&val);
		if(ret < 0)
			return ret;
		
	}while((val & (FORCE_IINDET_MASK << FORCE_IINDET_SHIFT)) && wait_cnt < 30);
	pr_err("wait cnt %d\n",wait_cnt);
	if(wait_cnt < 30)
	{
		ret = sd7601_read_byte(chip,SD7601_R08,&val);
		if(ret < 0)
			return ret;

		chg_type = (val & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;
		pr_err("chg_type %s\n",vbus_type[chg_type]);
	}
	else
		return -1;
	
	return 0;
}

static void sd7601_enable_bc12(struct sd7601_chip *chip, bool en)
{
	pr_err("en = %d\n", en);
	atomic_set(&chip->bc12_en_req_cnt, en);
	wake_up(&chip->bc12_en_req);
}

static int32_t sd7601_bc12_preprocess(struct sd7601_chip *chip)
{
	if((SD7601D_COMPINENT_ID   != chip->component_id) && 
		(SD7601AD_COMPINENT_ID  != chip->component_id) && 
		(SD7601AD_COMPINENT_ID2 != chip->component_id))
		return -ENOTSUPP;
		
	sd7601_enable_bc12(chip, true);

	return 0;
}

static void sd7601_inform_psy_dwork_handler(struct work_struct *work)
{
#if 1
	int32_t ret = 0;
	union power_supply_propval propval = {.intval = 0};
#endif

	struct sd7601_chip *chip = container_of(work, struct sd7601_chip,psy_dwork.work);

#if 1
	bool attach = false;
	enum charger_type chg_type = CHARGER_UNKNOWN;

	mutex_lock(&chip->bc12_lock);
	attach = chip->attach;
	chg_type = chip->chg_type;
	mutex_unlock(&chip->bc12_lock);

	pr_err("attach = %d, type = %d\n",attach, chg_type);

	/* Get chg type det power supply */
	if (!chip->psy)
		chip->psy = power_supply_get_by_name("charger");
	if (!chip->psy) {
		pr_err("get power supply fail\n");
		mod_delayed_work(system_wq, &chip->psy_dwork,msecs_to_jiffies(1000));
		return;
	}
	propval.intval = attach;
	ret = power_supply_set_property(chip->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		pr_err("psy online fail(%d)\n",ret);

	propval.intval = chg_type;
	ret = power_supply_set_property(chip->psy,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
	if (ret < 0)
		pr_err("psy type fail(%d)\n", ret);
#endif
	power_supply_changed(chip->sd7601_psy);
}

static void sd7601_set_usbsw_state(struct sd7601_chip *chip, int state)
{
	pr_info("usbsw = %d\n", state);

	if (state == SD7601_USBSW_CHG)
		Charger_Detect_Init();
	else
		Charger_Detect_Release();
}

static int32_t sd7601_bc12_postprocess(struct sd7601_chip *chip)
{
	int32_t ret = 0;
	bool attach = false, inform_psy = true;
	u8 port = SD7601_PORTSTAT_NO_INPUT;

	if((SD7601D_COMPINENT_ID   != chip->component_id) && 
		(SD7601AD_COMPINENT_ID  != chip->component_id) &&
		(SD7601AD_COMPINENT_ID2 != chip->component_id))
		return -ENOTSUPP;

	attach = atomic_read(&chip->vbus_gd);
	if (chip->attach == attach) 
	{
		pr_err("attach(%d) is the same\n", attach);
		inform_psy = !attach;
		goto out;
	}
	chip->attach = attach;
	pr_err("attach = %d\n", attach);

	if (!attach) 
	{
		chip->port = SD7601_PORTSTAT_NO_INPUT;
		chip->chg_type = CHARGER_UNKNOWN;
		goto out;
	}

	ret = sd7601_read_byte(chip, SD7601_R08, &port);
	if (ret < 0)
		chip->port = SD7601_PORTSTAT_NO_INPUT;
	else
		chip->port = (port & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;

	// if(SD7601_PORTSTAT_CDP == chip->port)
	// {
	// 	sd7601_set_dpdm(chip,CONC_DP_DM_VOL_3P3,CONC_DP_DM_VOL_HIZ);
	// 	msleep(10);
	// 	sd7601_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
	// }
	// if(SD7601_PORTSTAT_HVDCP == chip->port)
	// {
	// 	//plug in,force 5V
	// 	sd7601_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P0);
	// }

	pr_err("chg_type %s\n",vbus_type[chip->port]);

	switch (chip->port)
	{
		case SD7601_PORTSTAT_NO_INPUT:
			chip->chg_type = CHARGER_UNKNOWN;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD7601_PORTSTAT_SDP:
			chip->chg_type = STANDARD_HOST;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD7601_PORTSTAT_CDP:
			chip->chg_type = CHARGING_HOST;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case SD7601_PORTSTAT_DCP:
			chip->chg_type = STANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			ret = sd7601_set_dp_for_afc(chip);
			break;
		case SD7601_PORTSTAT_HVDCP:
			chip->chg_type = STANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			ret = sd7601_set_dp_for_afc(chip);
			if(chip->target_hv)
				sd7601_charging_set_hvdcp20(chip,chip->target_hv);
			break;
		case SD7601_PORTSTAT_UNKNOWN:
			chip->chg_type = STANDARD_HOST;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD7601_PORTSTAT_NON_STANDARD:
		case SD7601_PORTSTAT_OTG:
		default:
			chip->chg_type = NONSTANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			ret = sd7601_set_dp_for_afc(chip);
			break;
	}
out:
	if (inform_psy)
		schedule_delayed_work(&chip->psy_dwork, 0);
	return 0;
}
static int32_t sd7601_bc12_en_kthread(void *data)
{
	int32_t ret = 0, i = 0;
	struct sd7601_chip *chip = data;
	const int32_t max_wait_cnt = 200;

	pr_err("\n");

wait:
	wait_event(chip->bc12_en_req, atomic_read(&chip->bc12_en_req_cnt) > 0 || kthread_should_stop());

	if (atomic_read(&chip->bc12_en_req_cnt) <= 0 && kthread_should_stop()) 
	{
		pr_err( "bye bye\n");
		return 0;
	}
	atomic_dec(&chip->bc12_en_req_cnt);
	
	pm_stay_awake(chip->dev);

	if(atomic_read(&chip->vbus_gd))
	{
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) 
		{
			if (is_usb_rdy())
				break;
			pr_err("CDP block\n");
			if (!atomic_read(&chip->vbus_gd)) 
			{
				pr_err("plug out\n");
				break;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			pr_err("CDP timeout\n");
		else
			pr_err("CDP free\n");	
	}

	sd7601_set_usbsw_state(chip, (atomic_read(&chip->vbus_gd))  ? SD7601_USBSW_CHG : SD7601_USBSW_USB);
		
	ret = sd7601_chg_type_det(chip,atomic_read(&chip->vbus_gd));
	if (ret < 0)
		pr_err("fail(%d)\n",ret);
	sd7601_bc12_postprocess(chip);

	pm_relax(chip->dev);
	goto wait;

	return 0;
}

#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */


/*MTK ops*/
static int32_t sd7601_chg_plug_in(struct charger_device* chg_dev)
{
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("%s\n",chip->chg_dev_name);

	//force 5V
	//sd7601_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P0);

	return 0;
}
static int32_t sd7601_chg_plug_out(struct charger_device* chg_dev)
{
//	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;
	sd7601_dbg("%s\n",chip->chg_dev_name);

	//reset DP DM to hiz
	sd7601_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);

	return 0;
}
//extern bool ui_soc_limit_charger_flag ;
static int32_t sd7601_enable_charging(struct charger_device *chg_dev,bool en)
{
  
	int32_t ret = 0;     
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);

	if(NULL == chg_dev)
		return -1;
   // if(ui_soc_limit_charger_flag == true)
    //    return ret;
	sd7601_dbg("enable state : %d\n", en);

	ret = sd7601_set_charger_en(chip,!!en);
	
	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_is_enabled(struct charger_device* chg_dev , bool* en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == en)
		return -1;
		
	ret = sd7601_get_charger_en(chip);
	if(ret < 0) 
	{
		*en = false;
		return ret;
	}

	sd7601_dbg("ret = %d\n",ret);

	*en = (ret) ? true : false;

	return 0;
}

static int32_t sd7601_get_current(struct charger_device *chg_dev,uint32_t *ichg)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == ichg)
		return -1;

	ret = sd7601_get_ichg_current(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	*ichg = ret * 1000;

	return 0;
}

static int32_t sd7601_set_current(struct charger_device *chg_dev,uint32_t current_value)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("charge_current_value = %d\n", current_value);
	
	ret = sd7601_set_ichg_current(chip,current_value / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_get_min_charging_current(struct charger_device* chg_dev, uint32_t* val_ua)

{
	if(NULL == chg_dev || NULL == val_ua)
		return -1;

	*val_ua = 100 * 1000;
	sd7601_dbg("return min cc : %d uA\n",*val_ua);
	return 0;
}

static int32_t sd7601_set_constant_voltage(struct charger_device *chg_dev,uint32_t cv)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("cv = %d uV\n", cv);

	ret = sd7601_set_vreg_volt(chip,cv / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_get_constant_voltage(struct charger_device* chg_dev, uint32_t* val_uv)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == val_uv)
		return -1;

	ret = sd7601_get_vreg_volt(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	*val_uv = ret * 1000;

	return 0;
}

static int32_t sd7601_get_input_current(struct charger_device *chg_dev,uint32_t *aicr)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == aicr)
		return -1;

	ret = sd7601_get_input_curr_lim(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	*aicr = ret * 1000;

	return 0;
}

static int32_t sd7601_set_input_current(struct charger_device *chg_dev,uint32_t current_value)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("current_value = %d\n", current_value);

	ret = sd7601_set_input_curr_lim(chip,current_value / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_get_min_input_current(struct charger_device* chg_dev, uint32_t* val_ua)
{
	if(NULL == chg_dev || NULL == val_ua)
		return -1;

	*val_ua = 100 * 1000;
	sd7601_dbg("return min input current : %d uA\n",*val_ua);
	return 0;
}

static int32_t sd7601_get_eoc_current(struct charger_device* chg_dev, uint32_t* val_ua)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == val_ua)
		return -1;

	ret = sd7601_get_iterm(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	*val_ua = ret * 1000;

	return 0;
}
static int32_t sd7601_set_eoc_current(struct charger_device* chg_dev, uint32_t eoc_ua)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("eoc_ua = %d\n", eoc_ua);

	ret = sd7601_set_iterm(chip,eoc_ua / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_reset_watch_dog_timer(struct charger_device*chg_dev)
{
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("\n");

	sd7601_set_wd_reset(chip);	/* Kick watchdog */

	sd7601_set_wd_timer(chip,160);	/* WDT 160s */

	return 0;
}
static int32_t sd7601_do_event(struct charger_device *chg_dev, uint32_t event,uint32_t args)
{
	//struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("event = %d\n", event);
/* +S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */
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
/* -S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */

	return 0;
}

// static int sd7601_reset_ta(struct charger_device *chg_dev)
// {
// 	int32_t ret = 0;
// 	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
// 	if(NULL == chg_dev)
// 		return -1;

// 	pr_err("\n");

// 	ret = sd7601_set_vlimit(chip,4600000);
// 	if (ret < 0)
// 		return ret;

// 	ret = sd7601_set_input_curr_lim(chip,100);	/* 100mA */
// 	if (ret < 0)
// 		return ret;

// 	msleep(250);

// 	ret = sd7601_set_input_curr_lim(chip,500);	/* 100mA */
// 	if (ret < 0)
// 		return ret;

// 	return (ret < 0) ? ret : 0;
// }

static int32_t sd7601_charging_set_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
	bool increase = is_increase;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("is_increase %d\n",is_increase);
	//sd7601_set_vreg_volt(chip,4400);	/* Set CV */	
	sd7601_set_ichg_current(chip,800);/* Set charging current 800ma */
	/* Enable Charging */
	sd7601_set_en_hiz(chip,0);
	sd7601_set_charger_en(chip,1);

	if (increase == 1) 
	{
		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(485);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(50);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(200);
	} else {
		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(485);

		sd7601_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(50);

		sd7601_set_input_curr_lim(chip,500);	/* 500mA */
	}

	return 0;
}
static int32_t sd7601_get_vindpm_voltage(struct charger_device *chg_dev, uint32_t *val_uV)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == val_uV)
		return -1;

	ret = sd7601_get_vlimit(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	*val_uV = ret * 1000;

	return 0;
}

static int32_t sd7601_set_vindpm_voltage(struct charger_device *chg_dev,uint32_t vindpm)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("vindpm =%d\n", vindpm);

	ret = sd7601_set_vlimit(chip,vindpm / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_get_vindpm_state(struct charger_device *chg_dev, bool *in_loop)
{
	int32_t ret = 0;
	uint8_t data = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == in_loop)
		return -1;

	ret = sd7601_read_byte(chip,SD7601_R0A,&data);
	if (ret < 0) 
		return ret;
	
	*in_loop = (data & (CONA_VINDPM_STAT_MASK << CONA_VINDPM_STAT_SHIFT)) >> CONA_VINDPM_STAT_SHIFT;

	sd7601_dbg("in_loop =%d\n", *in_loop);

	return 0;
}
#if 0
static int32_t sd7601_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == en)
		return -1;

	ret = sd7601_get_en_hiz(chip);
	if(ret < 0)
		return ret;

	sd7601_dbg("ret = %d\n",ret);

	//en hiz = 1,ic is suspend, power path is disabled
	*en = (ret) ? false : true;

	return 0;
}
static int32_t sd7601_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	uint32_t vlimit = (en ? 4500 : VILIMIT_VOL_MAX);
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("enable %d\n",en);

	ret = sd7601_set_vlimit(chip,vlimit);

	//if want to open power path, disable en hiz
	//ret = sd7601_set_en_hiz(chip,!en);

	return (ret < 0) ? ret : 0;
}
#endif
static int32_t sd7601_get_is_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == en)
		return -1;

	ret = sd7601_get_en_chg_timer(chip);
	if(ret < 0)
		return ret;

	*en = (bool)ret;
	sd7601_dbg("en =%d\n", *en);
	return 0;
}
static int32_t sd7601_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("en = %d\n",en);

	ret =	sd7601_set_en_chg_timer(chip,!!en);

	return (ret < 0) ? ret : 0;
}

static int32_t sd7601_enable_termination(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("en = %d\n",en);

	ret =	sd7601_set_en_term_chg(chip,!!en);

	return (ret < 0) ? ret : 0;
}

int32_t sd7601_old_test_set_charger_en(bool en)
{
    sd7601_dbg("en = %d\n", en);

    if (en) 
	{
        sd7601_set_en_hiz(otg_chip,0);
        sd7601_set_charger_en(otg_chip,en);
	} 
	else 
	{
        sd7601_set_en_hiz(otg_chip,0x1);
		sd7601_set_charger_en(otg_chip,en);		
	}
    return 0;
}
static int sd7601_enable_otg(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	uint8_t  data_reg = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	union power_supply_propval propval = {.intval = 0};

	sd7601_dbg("en = %d\n", en);

/* +Bug S96818AA1-5256,zhouxiaopeng2.wt,20230620, set HIZ to exit when OTG insertion */
	if (en) {
		ret = sd7601_read_byte(chip,SD7601_R00,&data_reg);
		if (!ret && ((data_reg & (CON0_EN_HIZ_MASK << CON0_EN_HIZ_SHIFT)) >> CON0_EN_HIZ_SHIFT)) {
			ret = sd7601_set_en_hiz(chip, 0);
			if (ret < 0) {
				sd7601_dbg("exit hiz failed(%d)\n", ret);
			}
		}
	}
/* -Bug S96818AA1-5256,zhouxiaopeng2.wt,20230620, set HIZ to exit when OTG insertion */

	ret = sd7601_set_otg_en(chip,!!en);
	if (ret < 0) {
		sd7601_dbg("set otg en failed(%d)\n", ret);
		return ret;
	}

/* +S96818AA1-2041, churui1.wt, ADD, 20230606, the node obtains OTG information */
	if (!chip->otg_psy)
		chip->otg_psy = power_supply_get_by_name("otg");
	if (!chip->otg_psy) {
		sd7601_dbg("get power supply failed\n");
		return -EINVAL;
	}

	propval.intval = en;
	ret = power_supply_set_property(chip->otg_psy, POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		sd7601_dbg("psy type fail(%d)\n", ret);
		return ret;
	}
/*
	propval.intval = en;
	ret = power_supply_set_property(chip->otg_psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0) {
		sd7601_dbg("psy online fail(%d)\n", ret);
		return ret;
	}
*/
/* -S96818AA1-2041, churui1.wt, ADD, 20230606, the node obtains OTG information */

	return 0;
}
#if 0
int32_t sd7601_enable_otg(bool en)
{
	sd7601_dbg("en = %d\n", en);
	if (en) 
	{
		sd7601_set_charger_en(otg_chip,0);
		sd7601_set_otg_en(otg_chip,1);
		//sd7601_set_wd_timer(otg_chip,160);	/* WDT 160s */
	} 
	else 
	{
		sd7601_set_otg_en(otg_chip,0);
		sd7601_set_charger_en(otg_chip,1);
	}
	return true;
}
#endif
static int32_t sd7601_set_boost_current_limit(struct charger_device *chg_dev, uint32_t boot_cur)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("boot_cur = %d uA\n", boot_cur);

	ret = sd7601_set_boost_ilim(chip,boot_cur / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	sd7601_dbg("en = %d\n",en);
	//if use the irq of sd7601,chg type will report in irq function
	//if (-1 != chip->intr_gpio)
	//		return 0;

	if(SD7601D_COMPINENT_ID != chip->component_id && SD7601AD_COMPINENT_ID != chip->component_id && SD7601AD_COMPINENT_ID2 != chip->component_id)
		return -ENOTSUPP;
	
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
//#ifdef CONFIG_TCPC_CLASS
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, en);
	ret = (en ? sd7601_bc12_preprocess : sd7601_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
/* +Bug S96818AA1-5263,zhouxiaopeng2.wt,20230526, set HIZ to exit when swapping interrupts */
	sd7601_set_en_hiz(chip, 0);
/* -Bug S96818AA1-5263,zhouxiaopeng2.wt,20230526, set HIZ to exit when swapping interrupts */
	if (ret < 0)
		pr_err("en bc12 fail(%d)\n", ret);
//#endif /* CONFIG_TCPC_CLASS */
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	return (ret < 0) ? ret : 0;
}
static int32_t sd7601_get_charging_status(struct charger_device *chg_dev,bool *is_done)
{
	int32_t ret_val = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == is_done)
		return -1;

	ret_val = sd7601_get_charger_status(chip);

	if (ret_val == 0x3)
		*is_done = true;
	else
		*is_done = false;

	sd7601_dbg("is_done = %d\n", *is_done);

	return 0;
}

/* +S96818AA1-2200, churui1.wt, ADD, 20230523, support shipmode function */
static int32_t sd7601_set_shipmode(struct charger_device *chg_dev, bool en)
{
	int32_t ret;

	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	if (en) {
		ret = sd7601_update(chip, SD7601_R07, 1, BATFET_DIS_MASK, BATFET_DIS_SHIFT);
		if (ret < 0) {
			pr_err("set shipmode failed(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int32_t sd7601_set_shipmode_delay(struct charger_device *chg_dev, bool en)
{
	int32_t ret;

	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if (NULL == chg_dev)
		return -1;

	//if set R07 bit5 as 1, then set bit3 as 0
	ret = sd7601_update(chip, SD7601_R07, en, BATFET_DLY_MASK, BATFET_DLY_SHIFT);
	if (ret < 0) {
		pr_err("set shipmode delay failed(%d)\n", ret);
		return ret;
	}

	return 0;
}
/* -S96818AA1-2200, churui1.wt, ADD, 20230523, support shipmode function */

static int32_t sd7601_dump_register(struct charger_device *chg_dev)
{
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev)
		return -1;

	return sd7601_dump_msg(chip);
}
static int32_t to_sd7601_adc(enum adc_channel chan)
{
	int32_t channel = error_channel;
	switch(chan)
	{
		case ADC_CHANNEL_VBUS	:	channel = vbus_channel;	break;
		case ADC_CHANNEL_VSYS	:	channel = vsys_channel;	break;
		case ADC_CHANNEL_VBAT	:	channel = vbat_channel;	break;
		case ADC_CHANNEL_IBUS	:	channel = ibus_channel;	break;
		case ADC_CHANNEL_IBAT	:	channel = ibat_channel;	break;
		case ADC_CHANNEL_TEMP_JC:
		case ADC_CHANNEL_USBID	:
		case ADC_CHANNEL_TS		:
		default:break;
	}
	return channel;
}
static int32_t sd7601_get_adc(struct charger_device *chg_dev, enum adc_channel chan,int32_t *min, int32_t *max)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	enum sd7601_adc_channel channel = to_sd7601_adc(chan);
	if(NULL == chg_dev || NULL == min || NULL == max)
		return -1;

	if(error_channel == channel)
		return -ENOTSUPP;

	ret = sd7601_get_adc_value(chip,channel);
	if(ret < 0)
		return ret;

	*min = *max = ret;

	sd7601_dbg("min = %d max = %d\n", *min,*max);

	return 0;
}

static int32_t sd7601_get_vbus_adc(struct charger_device *chg_dev, uint32_t *vbus)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == vbus)
		return -1;

	ret = sd7601_get_adc_value(chip,vbus_channel);
	if(ret < 0)
		return ret;
	*vbus = ret;

	sd7601_dbg("vbus = %d\n", *vbus);

	return 0;
}
static int32_t sd7601_get_ibus_adc(struct charger_device *chg_dev, uint32_t *ibus)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);
	if(NULL == chg_dev || NULL == ibus)
		return -1;
	
	ret = sd7601_get_adc_value(chip,ibus_channel);
	if(ret < 0)
		return ret;

	*ibus = ret * 1000;

	sd7601_dbg("ibus = %d\n", *ibus);

	return 0;
}
static int32_t sd7601_get_ibat_adc(struct charger_device *chg_dev, uint32_t *ibat)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd7601_get_adc_value(chip,ibat_channel);
	if(ret < 0)
		return ret;

	*ibat = ret;

	sd7601_dbg("ibat = %d\n", *ibat);

	return 0;
}

static struct charger_ops sd7601_chg_ops = {
	.suspend = NULL,
	.resume = NULL,

	/* cable plug in/out */
	.plug_in = sd7601_chg_plug_in,
	.plug_out = sd7601_chg_plug_out,

	.enable = sd7601_enable_charging,
	.is_enabled = sd7601_is_enabled,
	.enable_hz = sd7601_enable_hiz, // churui1.wt, ADD, 20230519, set the hiz mode
	.enable_chip = NULL,//sd7601_enable_powerpath,
	.is_chip_enabled = NULL,//sd7601_is_powerpath_enabled,

	.get_charging_current = sd7601_get_current,
	.set_charging_current = sd7601_set_current,
	.get_min_charging_current = sd7601_get_min_charging_current,

	.set_constant_voltage = sd7601_set_constant_voltage,
	.get_constant_voltage = sd7601_get_constant_voltage,

	.get_input_current = sd7601_get_input_current,
	.set_input_current = sd7601_set_input_current,
	.get_min_input_current = sd7601_get_min_input_current,

	.get_eoc_current = sd7601_get_eoc_current,
	.set_eoc_current = sd7601_set_eoc_current,

	.kick_wdt = sd7601_reset_watch_dog_timer,
	.event = sd7601_do_event,

	/* PE/PE+ */
	.reset_ta = NULL,//sd7601_reset_ta,
	.send_ta_current_pattern = sd7601_charging_set_ta_current_pattern,

	.send_ta20_current_pattern = NULL,
	//.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	.get_mivr = sd7601_get_vindpm_voltage,
	.set_mivr = sd7601_set_vindpm_voltage,
	.get_mivr_state = sd7601_get_vindpm_state,
	/* Power path */
	.is_powerpath_enabled = NULL,//sd7601_is_powerpath_enabled,
	.enable_powerpath = NULL,//sd7601_enable_powerpath,

	.enable_vbus_ovp = NULL,
	/* Safety timer */
	.is_safety_timer_enabled = sd7601_get_is_safetytimer_enable,
	.enable_safety_timer = sd7601_enable_safetytimer,

	.enable_termination = sd7601_enable_termination,
	/* direct charging */
	.enable_direct_charging	= NULL,
	.kick_direct_charging_wdt = NULL,
	.set_direct_charging_ibusoc = NULL,
	.set_direct_charging_vbusov = NULL,
	/* OTG */
	.enable_otg = sd7601_enable_otg,
	.enable_discharge = NULL,
	.set_boost_current_limit = sd7601_set_boost_current_limit,

	.enable_chg_type_det = sd7601_enable_chg_type_det,

	 /* ship mode */
	.set_shipmode = sd7601_set_shipmode,
	.set_shipmode_delay = sd7601_set_shipmode_delay,

	/* run AICL */
	.run_aicl = NULL,
	/* reset EOC state */
	.reset_eoc_state = NULL,
	.safety_check = NULL,

	.is_charging_done = sd7601_get_charging_status,
	.set_pe20_efficiency_table = NULL,
	.dump_registers = sd7601_dump_register,
	.get_adc = sd7601_get_adc,
	.get_vbus_adc = sd7601_get_vbus_adc,
	.get_ibus_adc = sd7601_get_ibus_adc,
	.get_ibat_adc = sd7601_get_ibat_adc,
	.get_tchg_adc = NULL,
	.get_zcv = NULL,
};

static int32_t sd7601_detect_ic(struct sd7601_chip *chip)
{
	uint8_t hw_id = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R0B,&hw_id);
	if(ret < 0)
		return ret;

	pr_info("hw_id:0x%02X\n",hw_id);
	
	if(0x38 != hw_id)
		return -1;

	return 0;
}
static int32_t sd7601_get_component_id(struct sd7601_chip *chip)
{
	uint8_t chip_id = 0, version_id = 0;
	int32_t ret = 0;

	ret = sd7601_read_byte(chip,SD7601_R0E,&chip_id);
	if(ret < 0)
		return ret;

	ret = sd7601_read_byte(chip,SD7601_R0F,&version_id);
	if(ret < 0)
		return ret;

	chip->component_id = chip_id << 8 | version_id;

	pr_info("chip_id:0x%02X,version_id:0x%02X,component_id:0x%04X\n",chip_id,version_id,chip->component_id);
	
	return 0;
}

static void sd7601_hw_init(struct sd7601_chip *chip)
{
	if (SD7601AD_COMPINENT_ID2 != chip->component_id)
	{
 		sd7601_operate_state1(chip, 1);
		sd7601_operate_state2(chip, 0);
		sd7601_operate_state1(chip, 0);
	}
	sd7601_write_byte(chip,SD7601_R00,0x04);	//set ilimit 500ma as default
	sd7601_write_byte(chip,SD7601_R01,0x18);
	sd7601_write_byte(chip,SD7601_R02,0xA2);	//set ichg 2040ma as default
	sd7601_write_byte(chip,SD7601_R03,0x22);	//set pre_chg and iterm current 180ma
	sd7601_write_byte(chip,SD7601_R04,0x88);	//set cv as 4400mv
	sd7601_write_byte(chip,SD7601_R05,0x0F);	//disable iterm,disable wdt,enable charge timer,chg timer 10hrs,jeita 20% ichg
	sd7601_write_byte(chip,SD7601_R06,0xE6);	//set vindpm 4500mv as default
	sd7601_write_byte(chip,SD7601_R07,0x48);	//4C as default;48 as long press prohibited
	sd7601_write_byte(chip,SD7601_R0A,0x03);	//disable iindpm interrupt
	sd7601_write_byte(chip,SD7601_R0C,0x00);
	sd7601_write_byte(chip,SD7601_R0D,0x12);
	sd7601_write_byte(chip,SD7601_R10,0x86);	//enable hv_vindpm

	sd7601_set_en_hiz(chip,0);
	sd7601_set_vlimit(chip,chip->vlimit_mv);
	//sd7601_set_input_curr_lim(chip,chip->ilimit_ma); //keep ilimit 500ma
	sd7601_set_wd_reset(chip);
	sd7601_set_sys_min_volt(chip,chip->vsysmin_mv);
	sd7601_set_iprechg_current(chip,chip->pre_ma);
	sd7601_set_iterm(chip,chip->eoc_ma);
	sd7601_set_vreg_volt(chip,chip->cv_mv);
	sd7601_set_ichg_current(chip,chip->cc_ma);
	sd7601_set_rechg_volt(chip,chip->rechg_mv);
#ifndef WT_COMPILE_FACTORY_VERSION
	sd7601_set_en_term_chg(chip,1); //disable iterm on ATO version
#endif
	sd7601_set_wd_timer(chip,0);
	sd7601_set_en_chg_timer(chip,1);
	sd7601_set_otg_en(chip,0);
	sd7601_set_charger_en(chip,1);
}

static void sd7601_work_func(struct work_struct *work)
{
	struct sd7601_chip *chip = container_of(work, struct sd7601_chip,sd7601_work.work);
	sd7601_dump_msg(chip);
	schedule_delayed_work(&chip->sd7601_work, msecs_to_jiffies(5000));
}

static int32_t sd7601_parse_dt(struct sd7601_chip *chip,struct device *dev)
{
	int32_t ret = 0;
	struct device_node *np = dev->of_node;

	if (!np) {
		pr_err("no of node\n");
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name", &chip->chg_dev_name) < 0)
	{
		chip->chg_dev_name = "primary_chg";
		pr_err("no charger name\n");
	}

	if (of_property_read_string(np, "bigm,alias_name", &(chip->chg_props.alias_name)) < 0)
	{
		chip->chg_props.alias_name = "sd7601";
		pr_err("no alias name\n");
	}
	//irq gpio
	ret = of_get_named_gpio(np, "intr_gpio", 0);
	if (ret < 0) 
	{
		pr_err("no bigm,intr_gpio(%d)\n", ret);
		chip->intr_gpio = -1;
	} 
	else
		chip->intr_gpio = ret;
	//if config ce gpio, output low
	ret = of_get_named_gpio(np,"bigm,ce_gpio",0);
	if (ret < 0) 
	{
		pr_err("no bigm,ce_gpio(%d)\n", ret);
		chip->ce_gpio = -1;
	} 
	else
		chip->ce_gpio = ret;

	pr_err("intr_gpio = %d, ce_gpio = %d\n", chip->intr_gpio, chip->ce_gpio);

	if (-1 != chip->ce_gpio) 
	{
		ret = devm_gpio_request_one(chip->dev,chip->ce_gpio,GPIOF_DIR_OUT,devm_kasprintf(chip->dev, GFP_KERNEL,"sd7601_ce_gpio.%s", dev_name(chip->dev)));
		if (ret < 0) 
		{
			pr_err("ce gpio request fail(%d)\n", ret);
			return ret;
		}
		//enable ic
		gpio_set_value(chip->ce_gpio,0);
	}

	//ilimit
	ret = of_property_read_u32(np, "bigm,ilimit_ma",&chip->ilimit_ma);
	if(ret)
		chip->ilimit_ma = DEFAULT_ILIMIT;
	//vlimit
	ret = of_property_read_u32(np, "bigm,vlimit_mv",&chip->vlimit_mv);
	if(ret)
		chip->vlimit_mv = DEFAULT_VLIMIT;
	//cc
	ret = of_property_read_u32(np, "bigm,cc_ma",&chip->cc_ma);
	if(ret)
		chip->cc_ma = DEFAULT_CC;
	//cv
	ret = of_property_read_u32(np, "bigm,cv_mv",&chip->cv_mv);
	if(ret)
		chip->cv_mv = DEFAULT_CV;
	//pre ma
	ret = of_property_read_u32(np, "bigm,pre_ma",&chip->pre_ma);
	if(ret)
		chip->pre_ma = DEFAULT_IPRECHG;
	//eoc ma
	ret = of_property_read_u32(np, "bigm,eoc_ma",&chip->eoc_ma);
	if(ret)
		chip->eoc_ma = DEFAULT_ITERM;
	//vsys min mv
	ret = of_property_read_u32(np, "bigm,vsysmin_mv",&chip->vsysmin_mv);
	if(ret)
		chip->vsysmin_mv = DEFAULT_VSYS_MIN;
	//recharge voltage
	ret = of_property_read_u32(np, "bigm,rechg_mv",&chip->rechg_mv);
	if(ret)
		chip->rechg_mv = DEFAULT_RECHG;
	//hvdcp target voltage
	ret = of_property_read_u32(np, "bigm,target_hv",&chip->target_hv);
	if(ret)
		chip->target_hv = 0;

	pr_err("ilimit:%dma, vlimit:%dmv, cc:%dma, cv:%dmv, pre:%dma, eoc:%dma, vsysmin:%dmv, vrechg:%dmv, target_hv %dv\n",
					chip->ilimit_ma,chip->vlimit_mv,chip->cc_ma,
					chip->cv_mv,chip->pre_ma,chip->eoc_ma,chip->vsysmin_mv,
					chip->rechg_mv,chip->target_hv);
	return 0;
}
static uint8_t attr_reg = 0xFF;
static ssize_t show_registers(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	uint8_t i = 0, data = 0;
	int32_t ret  = 0;
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	if(0xFF == attr_reg)
	{
		for (i = 0; i < SD7601_REG_NUM; i++) 
		{
			data = 0;
			ret = sd7601_read_byte(chip,i, &data);
			count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
		}

		if((SD7601AD_COMPINENT_ID == chip->component_id) || (SD7601AD_COMPINENT_ID2 == chip->component_id))
		{
			//ret = sd7601_update(chip,SD7601_R11,1,CON11_CONV_RATE_MASK,CON11_CONV_RATE_SHIFT);
			ret = sd7601_write_byte(chip,SD7601_R11,0x80);//one shot
			if(ret < 0)
				count += sprintf(&buf[count], "write R11 failed\n");
			else
			{
				msleep(1000);
				for (i = SD7601_R11; i <= SD7601_R17; i++) 
				{
					data = 0;
					ret = sd7601_read_byte(chip,i, &data);
					count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
				}	
			}
		}
	}
	else
	{
		data = 0;
		ret = sd7601_read_byte(chip,attr_reg, &data);
		count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",attr_reg,data,(ret < 0) ? "failed":"successfully");
	}

	return count;
}

static ssize_t store_registers(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int32_t ret = 0;
	char *pvalue = NULL, *addr = NULL, *val = NULL;
	uint32_t reg_value = 0,reg_address = 0;
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	if(NULL == buf || (6 != size && 3 != size))
		goto msg;

	pvalue = (char *)buf;
	addr = strsep(&pvalue, " ");
	ret = kstrtou32(addr, 16,(uint32_t *)&reg_address);
	//read
	if(3 == size)
	{
		//just want read some register
		attr_reg = reg_address;
		pr_err("You want read reg 0x%02X, please cat registers!\n",(uint32_t) attr_reg);
	}
	else
	{
		//you can read register immediately��after write
		attr_reg = reg_address;

		val  = strsep(&pvalue, " ");
		ret = kstrtou32(val, 16, (uint32_t *)&reg_value);

		pr_err("write sd7601 reg 0x%02X with value 0x%02X !\n",(uint32_t) reg_address, reg_value);

		sd7601_write_byte(chip,reg_address,reg_value);
	}
	return size;
msg:
	pr_err("exsample: echo 10 22 > registers\n");
	pr_err("means   : write reg 0x10 with value 0x22!\n");
	pr_err("exsample: echo 10 > registers\n");
	pr_err("means   : cat registers will read 0x10\n");
	pr_err("exsample: echo ff > registers\n");
	pr_err("means   : cat registers will read all\n");
	return size;
}

static DEVICE_ATTR(dev_registers, 0664, show_registers,store_registers);	/* 664 */

static int32_t	sd7601_default_irq(struct sd7601_chip *chip,uint8_t data)
{
	pr_err("%d\n",data);
	return 0;
}
static int32_t	sd7601_chg_state_irq(struct sd7601_chip *chip,uint8_t data)
{
	pr_err("%d %s\n",data,charge_state[data]);
	chip->chg_state = data;
	return 0;
}
static int32_t	sd7601_pg_state_irq(struct sd7601_chip *chip,uint8_t data)
{
	int32_t ret = 0;

	if((SD7601D_COMPINENT_ID   != chip->component_id) && 
		(SD7601AD_COMPINENT_ID  != chip->component_id) &&
		(SD7601AD_COMPINENT_ID2 != chip->component_id))
		return 0;

	pr_err("pg good %d\n",data);
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
#if 0
		mutex_lock(&chip->bc12_lock);
		//plug out
		if (atomic_read(&chip->vbus_gd) && 0 == data)
			ret = sd7601_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);

		atomic_set(&chip->vbus_gd,data);

		ret = (data ? sd7601_bc12_preprocess : sd7601_bc12_postprocess)(chip);
		
		mutex_unlock(&chip->bc12_lock);
//		if(!data)
//			schedule_delayed_work(&chip->sd7601_chg_work, msecs_to_jiffies(2000));
#endif
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	return ret;
}
static int32_t	sd7601_vbus_good_irq(struct sd7601_chip *chip,uint8_t data)
{
	int32_t ret = 0;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	pr_err("data %d\n",data);
#if 0
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd,data);
//	sd7601_bc12_preprocess(chip);
	ret = (data ? sd7601_bc12_preprocess : sd7601_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
#endif
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
 	return ret;
}

static int32_t	sd7601_boost_fault_irq(struct sd7601_chip *chip,uint8_t data)
{
	int32_t ret = 0;
	uint8_t val = 0;
	uint8_t retry_times = 0;
	uint8_t check_times = 0;

	if(1 == data)
	{
		for(retry_times = 0; retry_times < 5; retry_times++)
		{
			sd7601_set_otg_en(chip,0);
			sd7601_set_otg_en(chip,1);
			//check result
			for(check_times = 0; check_times < 5; check_times++)
			{
				ret = sd7601_read_byte(chip,SD7601_R09,&val);
				if(ret < 0)
					continue;

				pr_info("R09 = 0x%02X\n",val);
				if(0 == (val & (CON9_BOOST_STAT_MASK << CON9_BOOST_STAT_SHIFT)))
					break;
			}
			pr_info("check_times = %d\n",check_times);
			//failed, retry
			if(check_times >= 5)
				continue;
			else//successful,break
				break;			
		}
	}
	return 0;
}
static struct sd7601_irq_handle irq_handles[] = {
	{
		SD7601_R08,0,0,
		{
			{
				.irq_name	= "chg state",
				.irq_func	= sd7601_chg_state_irq,
				.irq_mask   = CON8_CHRG_STAT_MASK,
				.irq_shift	= CON8_CHRG_STAT_SHIFT,
			},
			{
				.irq_name	= "pg state",
				.irq_func	= sd7601_pg_state_irq,
				.irq_mask   = CON8_PG_STAT_MASK,
				.irq_shift	= CON8_PG_STAT_SHIFT,
			},
			{
				.irq_name	= "therm state",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON8_THM_STAT_MASK,
				.irq_shift	= CON8_THM_STAT_SHIFT,
			},
			{
				.irq_name	= "vsys state",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON8_VSYS_STAT_MASK,
				.irq_shift	= CON8_VSYS_STAT_SHIFT,
			},
		},
	},

	{
		SD7601_R09,0,0,
		{
			{
				.irq_name	= "wathc dog fault",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON9_WATG_STAT_MASK,
				.irq_shift	= CON9_WATG_STAT_SHIFT,
			},
			{
				.irq_name	= "boost fault",
				.irq_func	= sd7601_boost_fault_irq,
				.irq_mask   = CON9_BOOST_STAT_MASK,
				.irq_shift	= CON9_BOOST_STAT_SHIFT,
			},
			{
				.irq_name	= "chg fault",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON9_CHRG_FAULT_MASK,
				.irq_shift	= CON9_CHRG_FAULT_SHIFT,
			},
			{
				.irq_name	= "bat fault",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON9_BAT_STAT_MASK,
				.irq_shift	= CON9_BAT_STAT_SHIFT,
			},
			{
				.irq_name	= "ntc fault",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CON9_NTC_STAT_MASK,
				.irq_shift	= CON9_NTC_STAT_SHIFT,
			},
		},
	},
	{
		SD7601_R0A,0,0,
		{
			 {
			 	.irq_name	= "vbus good",
			 	.irq_func	= sd7601_vbus_good_irq,
			 	.irq_mask   = CONA_VBUS_GD_MASK,
			 	.irq_shift	= CONA_VBUS_GD_SHIFT,
			},
			{
				.irq_name	= "vdpm",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CONA_VINDPM_STAT_MASK,
				.irq_shift	= CONA_VINDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "idpm",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CONA_IDPM_STAT_MASK,
				.irq_shift	= CONA_IDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "topoff active",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CONA_TOPOFF_ACTIVE_MASK,
				.irq_shift	= CONA_TOPOFF_ACTIVE_SHIFT,
			},
			{
				.irq_name	= "acov state",
				.irq_func	= sd7601_default_irq,
				.irq_mask   = CONA_ACOV_STAT_MASK,
				.irq_shift	= CONA_ACOV_STAT_SHIFT,
			},
		},
	},
};

static int32_t sd7601_process_irq(struct sd7601_chip *chip)
{
	int32_t i, j,ret = 0;
	uint8_t pre_state=0 ,now_state=0;
	int32_t handler_count = 0;

	if (-1 == chip->intr_gpio)
		return 0;

	for (i=0; i<ARRAY_SIZE(irq_handles); i++) 
	{
		ret = sd7601_read_byte(chip,irq_handles[i].reg,&irq_handles[i].value);
		pr_err("reg 0x%02X,value now 0x%02X, value pre 0x%02X\n",irq_handles[i].reg,irq_handles[i].value,irq_handles[i].pre_value);

		if (ret < 0) 
		{
			dev_err(chip->dev, "Couldn't read reg 0x%02X ret=%d\n",irq_handles[i].reg,ret);
			continue;
		}

		for (j=0; j<ARRAY_SIZE(irq_handles[i].irq_info); j++) 
		{
			now_state = irq_handles[i].value & (irq_handles[i].irq_info[j].irq_mask << irq_handles[i].irq_info[j].irq_shift);
			pre_state = irq_handles[i].pre_value & (irq_handles[i].irq_info[j].irq_mask << irq_handles[i].irq_info[j].irq_shift); 

			now_state >>= irq_handles[i].irq_info[j].irq_shift;
			pre_state >>= irq_handles[i].irq_info[j].irq_shift;

			if(now_state != pre_state)
			{
				pr_info("%s change %d -> %d\n", irq_handles[i].irq_info[j].irq_name,pre_state,now_state);

				if (irq_handles[i].irq_info[j].irq_func)
				{
					handler_count++;
					ret = irq_handles[i].irq_info[j].irq_func(chip, now_state);
					if (ret < 0)
						dev_err(chip->dev,"Couldn't handle %d irq for reg 0x%02X ret = %d\n",j, irq_handles[i].reg, ret);
				}	
			}
		}

		irq_handles[i].pre_value = irq_handles[i].value;
	}

	return handler_count;
}

static irqreturn_t sd7601_irq_handler(int32_t irq, void *data)
{
	struct sd7601_chip *chip = (struct sd7601_chip *)data;

	pr_err("--------------------------\n");

	//wait for register modify
	msleep(50);

	pm_stay_awake(chip->dev);

	mutex_lock(&chip->irq_lock);
	sd7601_process_irq(chip);
	mutex_unlock(&chip->irq_lock);

	pm_relax(chip->dev);

	return IRQ_HANDLED;
}

static int32_t sd7601_register_irq(struct sd7601_chip *chip)
{
	int32_t ret = 0;

	pr_err("irq gpio %d\n",chip->intr_gpio);

	if (-1 == chip->intr_gpio)
		return 0;

	ret = devm_gpio_request_one(chip->dev, chip->intr_gpio, GPIOF_DIR_IN,
										devm_kasprintf(chip->dev, GFP_KERNEL,
										"sd7601_intr_gpio.%s", dev_name(chip->dev)));
	if (ret < 0) 
	{
		pr_err("gpio request fail(%d)\n", ret);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->intr_gpio);
	if (chip->irq < 0) 
	{
		pr_err("gpio2irq fail(%d)\n", chip->irq);
		return chip->irq;
	}

	pr_err("irq = %d\n", chip->irq);

	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					sd7601_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING, 
					devm_kasprintf(chip->dev, GFP_KERNEL,
					"sd7601_irq.%s", dev_name(chip->dev)),
					chip);

	if (ret < 0) 
	{
		pr_err("request threaded irq fail(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(chip->irq);
	return ret;
}

static enum power_supply_property sd7601_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, //cv
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, //vbus current
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,	//cc
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
};
static int32_t sd7601_get_prop(struct power_supply *psy,enum power_supply_property psp,union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) 
	{
		case POWER_SUPPLY_PROP_PRESENT:
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = atomic_read(&chip->vbus_gd);
			break;
		case POWER_SUPPLY_PROP_TYPE:
			val->intval = psy->desc->type;
			break;
		case POWER_SUPPLY_PROP_STATUS:
			if (!atomic_read(&chip->vbus_gd)) {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
			}
			ret = sd7601_get_charger_status(chip);
			if (ret == 3)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else if (ret == 0)
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			ret = sd7601_get_charger_status(chip);
			if(0 == ret)
				val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			else if(1 == ret)
				val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			else if(2 == ret || 3 == ret)
				val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			else // ret = -1
				val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:	//cv
			ret = sd7601_get_vreg_volt(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			ret = sd7601_get_input_curr_lim(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			ret = sd7601_get_ichg_current(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:	//vbus adc
			ret = sd7601_get_adc_value(chip,vbus_channel);
			val->intval = (ret < 0) ? 0 : ret;
			break;

		case POWER_SUPPLY_PROP_USB_TYPE:
			//ret =sd7601_get_charger_type(chip);
			//val->intval = ret;//chip->chg_type;
			val->intval = chip->chg_type;
			break;

		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = "sd7601";
			break;

		case POWER_SUPPLY_PROP_MANUFACTURER:
			val->strval = "Big Moment Technology";
			break;
		case POWER_SUPPLY_PROP_ENERGY_EMPTY:
			val->intval = chip->vbat0_flag;
			break;
		default:
			pr_err("get prop %d is not supported in usb\n", psp);
			ret = -EINVAL;
			break;
	}

	if (ret < 0) 
	{
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}
	return 0;
}
static int32_t sd7601_set_prop(struct power_supply *psy,enum power_supply_property psp,const union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = power_supply_get_drvdata(psy);
	
	switch (psp) 
	{
		case POWER_SUPPLY_PROP_ONLINE:
			chip->attach = val->intval;
			break;
		case POWER_SUPPLY_PROP_USB_TYPE:
			chip->chg_type = val->intval;
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:		//cv
			ret = sd7601_set_vreg_volt(chip,val->intval / 1000);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: //ilimit
			ret = sd7601_set_input_curr_lim(chip,val->intval / 1000);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX: //cc
			ret = sd7601_set_ichg_current(chip,val->intval / 1000);
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:	//vbus in uv
			ret = sd7601_charging_set_hvdcp20(chip,val->intval / 1000000);
			break;
		case POWER_SUPPLY_PROP_ENERGY_EMPTY:
			chip->vbat0_flag = val->intval;
			break;
		default:
			pr_err("set prop %d is not supported\n", psp);
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int32_t sd7601_prop_is_writeable(struct power_supply *psy,enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_USB_TYPE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		break;
	}

	return 0;
}

static char *sd7601_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};
static enum power_supply_usb_type sd7601_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};
static const struct power_supply_desc sd7601_psy_desc = {
	.name = "n28-charger",//sd7601d
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sd7601_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(sd7601_chg_psy_usb_types),

	.properties = sd7601_props,
	.num_properties = ARRAY_SIZE(sd7601_props),
	.get_property = sd7601_get_prop,
	.set_property = sd7601_set_prop,
	.property_is_writeable = sd7601_prop_is_writeable,
};

static int32_t sd7601_power_init(struct sd7601_chip *chip)
{
	struct power_supply_config cfg = {};

	cfg.drv_data = chip;
	cfg.of_node  = chip->dev->of_node;
	cfg.supplied_to = sd7601_supplied_to;
	cfg.num_supplicants = ARRAY_SIZE(sd7601_supplied_to);
	pr_info("\n");
	memcpy(&chip->psy_desc, &sd7601_psy_desc, sizeof(chip->psy_desc));

	chip->sd7601_psy = devm_power_supply_register(chip->dev,&chip->psy_desc,&cfg);

	if (IS_ERR(chip->sd7601_psy)) 
	{
		pr_err("Couldn't register power supply\n");
		return PTR_ERR(chip->sd7601_psy);
	}

	return 0;
}
#if 0
static void sd7601_chg_work_func(struct work_struct *work)
{
	int32_t en =0;
	int32_t ret =0;

	struct sd7601_chip *chip = container_of(work, struct sd7601_chip,sd7601_chg_work.work);

	en = atomic_read(&chip->vbus_gd);
	pr_err("start en:%d\n",en);

	if(!en)
	{
		mutex_lock(&chip->bc12_lock);
		atomic_set(&chip->vbus_gd, en);
		ret = (en ? sd7601_bc12_preprocess : sd7601_bc12_postprocess)(chip);
		mutex_unlock(&chip->bc12_lock);	
		cancel_delayed_work_sync(&chip->sd7601_chg_work);
	}
}
#endif
static int32_t sd7601_driver_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct sd7601_chip *chip = NULL;
    
	pr_err("start\n");

	chip = devm_kzalloc(&client->dev, sizeof(struct sd7601_chip),GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg begin		
    otg_chip = devm_kzalloc(&client->dev, sizeof(struct sd7601_chip),GFP_KERNEL);
	if (!otg_chip)
		return -ENOMEM;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg end    

	chip->client = client;
	chip->dev = &client->dev;
	//init mutex asap,before iic operate
	mutex_init(&chip->i2c_rw_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->irq_lock);
	i2c_set_clientdata(client, chip);

	ret = sd7601_detect_ic(chip);
	if(ret < 0)
	{
		pr_err("do not detect ic, exit\n");
		return -ENODEV;
	}
	sd7601_get_component_id(chip);

	ret = sd7601_parse_dt(chip, &client->dev);
	if (ret < 0)
		pr_err("sd7601_parse_dt failed ret %d\n",ret);

	sd7601_hw_init(chip);

	ret = sd7601_power_init(chip);
	if (ret < 0) 
		pr_err("register power supply fail(%d)\n", ret);
	
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	if((SD7601D_COMPINENT_ID   == chip->component_id)  || 
		(SD7601AD_COMPINENT_ID  == chip->component_id)  ||
		(SD7601AD_COMPINENT_ID2 == chip->component_id))
	{
		pr_err("prepare for bc1.2\n");
		mutex_init(&chip->bc12_lock);
		INIT_DELAYED_WORK(&chip->psy_dwork, sd7601_inform_psy_dwork_handler);
		atomic_set(&chip->vbus_gd, 0);
		chip->attach = false;
		chip->port = SD7601_PORTSTAT_NO_INPUT;
		chip->chg_type = CHARGER_UNKNOWN;
		atomic_set(&chip->bc12_en_req_cnt, 0);
		init_waitqueue_head(&chip->bc12_en_req);
		chip->bc12_en_kthread = kthread_run(sd7601_bc12_en_kthread,chip,"sd7601_bc12");
		if (IS_ERR_OR_NULL(chip->bc12_en_kthread)) 
		{
			ret = PTR_ERR(chip->bc12_en_kthread);
			pr_err("kthread run fail(%d)\n", ret);
		}
	}
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	/* Register charger device */
	chip->chg_dev = charger_device_register(chip->chg_dev_name,&client->dev,chip,&sd7601_chg_ops,&chip->chg_props);

	if (IS_ERR_OR_NULL(chip->chg_dev)) 
	{
		ret = PTR_ERR(chip->chg_dev);
		pr_err("register chg dev fail(%d)\n", ret);
	}

	ret = sd7601_register_irq(chip);
	if (ret < 0) 
		pr_err("register irq fail(%d)\n", ret);
	//process irq once when probe
	ret = sd7601_process_irq(chip);
	if (ret < 0) 
		pr_err("process irq fail(%d)\n", ret);

	device_init_wakeup(chip->dev, true);

	sd7601_dump_register(chip->chg_dev);

	ret = device_create_file(&client->dev,&dev_attr_dev_registers);
	if (ret < 0) 
		pr_err("create file fail(%d)\n",ret);

	INIT_DELAYED_WORK(&chip->sd7601_work, sd7601_work_func);
	schedule_delayed_work(&chip->sd7601_work, (1*HZ));
//Antaiui <AI_BSP_CTP> <hehl> <2021-12-15> mmi charger ic begin
       #ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
       {
               #include <linux/ai_device_check.h>
               struct ai_device_info ai_chg_hw_info;
               ai_chg_hw_info.ai_dev_type = AI_DEVICE_TYPE_CHARGER;
               strcpy(ai_chg_hw_info.name, "SD7601");
               ai_set_device_info(ai_chg_hw_info);
       }
       #endif
//	   INIT_DELAYED_WORK(&chip->sd7601_chg_work, sd7601_chg_work_func);
//Antaiui <AI_BSP_CTP> <hehl> <2021-12-15> mmi charger ic en
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg begin
    otg_chip = chip;
    sd7601_is_loaded = true;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-21> modify otg end	
	pr_err("end\n"); 
	hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SD7601D");
	return 0;

}

static int32_t sd7601_suspend(struct device *dev)
{
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	pr_err("\n");
	
	if (device_may_wakeup(dev) && chip->irq >= 0)
		enable_irq_wake(chip->irq);

	if(chip->irq >= 0)
		disable_irq(chip->irq);

	return 0;
}
static int32_t sd7601_resume(struct device *dev)
{
	struct sd7601_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	pr_err("\n");

	if(chip->irq >= 0)
		enable_irq(chip->irq);

	if (device_may_wakeup(dev) && chip->irq >= 0)
		disable_irq_wake(chip->irq);

	return 0;
}
static int32_t sd7601_driver_remove(struct i2c_client *client)
{
	struct sd7601_chip *chip = i2c_get_clientdata(client);

	if(chip->irq >= 0)
		disable_irq(chip->irq);
	if(-1 != chip->intr_gpio)
		devm_gpio_free(chip->dev,chip->intr_gpio);

	charger_device_unregister(chip->chg_dev);

#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	if((SD7601D_COMPINENT_ID == chip->component_id) || (SD7601AD_COMPINENT_ID == chip->component_id) || (SD7601AD_COMPINENT_ID2 == chip->component_id))
	{
		cancel_delayed_work_sync(&chip->psy_dwork);
		if (chip->psy)
			power_supply_put(chip->psy);

		kthread_stop(chip->bc12_en_kthread);
		mutex_destroy(&chip->bc12_lock);
	}
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->i2c_rw_lock);
	return 0;
}
static void sd7601_driver_shutdown(struct i2c_client *client)
{
}
static const struct dev_pm_ops sd7601_pm_ops = {
	.resume			= sd7601_resume,
	.suspend		= sd7601_suspend,
};
static const struct of_device_id sd7601_of_match[] = {
	{.compatible = "bigmtech,sd7601"},
	{.compatible = "bigmtech,sd7601d"},
	{.compatible = "bigmtech,sd7601ad"},
	{},
};
static const struct i2c_device_id sd7601_i2c_id[] = { 
	{"sd7601",   0}, 
	{"sd7601d",  1},
	{"sd7601ad", 2},
};
static struct i2c_driver sd7601_driver = {
	.driver = {
		.name 			 = "sd7601",
		.owner 			 = THIS_MODULE,
		.pm				 = &sd7601_pm_ops,
		.of_match_table = sd7601_of_match,
	},
	.id_table 	= sd7601_i2c_id,
	.probe 		= sd7601_driver_probe,
	.remove		= sd7601_driver_remove,
	.shutdown	= sd7601_driver_shutdown,
};

static int32_t __init sd7601_init(void)
{
	int32_t ret = 0;
	ret = i2c_add_driver(&sd7601_driver);

	if (0 != ret) 
		pr_err("failed to register sd7601 i2c driver.\n");
	else 
		pr_err("Success to register sd7601 i2c driver.\n");
	
	return ret;
}

static void __exit sd7601_exit(void)
{
	i2c_del_driver(&sd7601_driver);
}
module_init(sd7601_init);
module_exit(sd7601_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sd7601 Driver");
MODULE_AUTHOR("marvin.xin <marvin.xin@bigmtech.com>");
