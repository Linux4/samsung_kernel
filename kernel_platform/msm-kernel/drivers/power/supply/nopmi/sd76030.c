/*
 * Copyright(c) Bigmtech, 2021. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)	"[bmt-sd76030] %s: " fmt, __func__

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
#include <asm-generic/div64.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/extcon.h>
#include <../../../extcon/extcon.h>
#include "../../../usb/typec/tcpc/inc/tcpm.h"
#include "../../../usb/typec/tcpc/inc/tcpci_core.h"
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include "sd76030.h"
#include "sd76030_iio.h"
#include <linux/iio/consumer.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>
#include "qcom-pmic-voter.h"


#define bmt_dbg(fmt, args...) pr_err("[bmt-chg] %s: "fmt, __func__, ## args)
//if use irq of sd76030,do not open this Macro,chg type will report in irq function
//#define TRIGGER_CHARGE_TYPE_DETECTION
struct sd76030_irq_info
{
	const char	*irq_name;
	int32_t	(*irq_func)(struct sd76030_chip *chip,uint8_t rt_stat);
	uint8_t 	irq_mask;
	uint8_t	irq_shift;
};
struct sd76030_irq_handle
{
	uint8_t reg;
	uint8_t value;
	uint8_t pre_value;
	struct sd76030_irq_info irq_info[5];
};
const static char* vbus_type[] =
{
	"no input",
	"sdp",
	"cdp",
	"dcp",
	"hvdcp",
	"unknown",
	"non-standard",
	"otg"
};
const static char* charge_state[] =
{
	"not charging",
	"pre-charge",
	"fast charging",
	"charge termination"
};
//for SD76030
const static char* adc_channel[] =
{
	"vbat",
	"vsys",
	"tbat",
	"vbus",
	"ibat",
	"ibus",
};

static bool vbus_on = false;
static struct sd76030_irq_handle irq_handles[];
static int32_t sd76030_process_irq(struct sd76030_chip *chip);
static int32_t	sd76030_extcon_clear_types(struct sd76030_chip *chip);
static int32_t	sd76030_extcon_notify(struct sd76030_chip *chip, uint32_t extcon_id, bool data);

static int32_t sd76030_read_byte(struct sd76030_chip *chip, uint8_t reg, uint8_t *data)
{
	int32_t ret = 0;
	int32_t i = 0;

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

static int32_t sd76030_write_byte(struct sd76030_chip *chip, int32_t reg, uint8_t val)
{
	int32_t ret = 0;
	int32_t i = 0;

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

static int32_t sd76030_update(struct sd76030_chip *chip, uint8_t RegNum, uint8_t val, uint8_t MASK,uint8_t SHIFT)
{
	uint8_t data = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,RegNum, &data);

	if(ret < 0)
		return ret;

	if((data & (MASK << SHIFT)) != (val << SHIFT))
	{
		data &= ~(MASK << SHIFT);
		data |= (val << SHIFT);
		return sd76030_write_byte(chip,RegNum, data);
	}
	return 0;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
int32_t sd76030_set_en_hiz(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R00,!!enable,CON0_EN_HIZ_MASK,CON0_EN_HIZ_SHIFT);
}
EXPORT_SYMBOL(sd76030_set_en_hiz);

int32_t sd76030_set_input_curr_lim(struct sd76030_chip *chip, uint32_t ilimit_ma)
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

	return sd76030_update(chip,SD76030_R00,data_reg,CON0_IINLIM_MASK,CON0_IINLIM_SHIFT);
}

static int32_t sd76030_get_input_curr_lim(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R00,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON0_IINLIM_MASK << CON0_IINLIM_SHIFT)) >> CON0_IINLIM_SHIFT;

	return (data_reg * INPUT_CURRT_STEP + INPUT_CURRT_MIN);
}

static int32_t sd76030_set_iterm(struct sd76030_chip *chip, uint32_t iterm)
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

	return sd76030_update(chip,SD76030_R03,data_reg,CON3_ITERM_MASK,CON3_ITERM_SHIFT);
}
static int32_t sd76030_get_iterm(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R03,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON3_ITERM_MASK<< CON3_ITERM_SHIFT)) >> CON3_ITERM_SHIFT;

	return (data_reg * EOC_CURRT_STEP + EOC_CURRT_MIN);
}

static int32_t sd76030_set_wd_reset(struct sd76030_chip *chip)
{
	return sd76030_update(chip,SD76030_R01,1,CON1_WD_MASK,CON1_WD_SHIFT);
}

static int32_t sd76030_set_otg_en(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R01,!!enable,CON1_OTG_CONFIG_MASK,CON1_OTG_CONFIG_SHIFT);
}
static int32_t sd76030_get_otg_en(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R01,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON1_OTG_CONFIG_MASK << CON1_OTG_CONFIG_SHIFT)) >> CON1_OTG_CONFIG_SHIFT;

	return !!data_reg;
}

static int32_t sd76030_set_charger_en(struct sd76030_chip *chip, int32_t enable)
{
   return sd76030_update(chip,SD76030_R01,!!enable,CON1_CHG_CONFIG_MASK,CON1_CHG_CONFIG_SHIFT);
}
static int32_t sd76030_get_charger_en(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R01,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON1_CHG_CONFIG_MASK << CON1_CHG_CONFIG_SHIFT)) >> CON1_CHG_CONFIG_SHIFT;

	return !!data_reg;
}

static int32_t sd76030_set_sys_min_volt(struct sd76030_chip *chip, uint32_t vsys_mv)
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

	return sd76030_update(chip,SD76030_R01,data_reg,CON1_SYS_V_LIMIT_MASK,CON1_SYS_V_LIMIT_SHIFT);
}

static int32_t sd76030_set_boost_ilim(struct sd76030_chip *chip, uint32_t boost_ilimit)
{
	uint8_t data_reg = 0;

	if(boost_ilimit <= 500)
		data_reg = 0x00;
	else
		data_reg = 0x01;

	return sd76030_update(chip,SD76030_R02,data_reg,CON2_BOOST_ILIM_MASK,CON2_BOOST_ILIM_SHIFT);
}
static int32_t sd76030_get_boost_ilim(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_BOOST_ILIM_MASK << CON2_BOOST_ILIM_SHIFT)) >> CON2_BOOST_ILIM_SHIFT;

	return (1 == data_reg) ? 1200 : 500;
}

int32_t sd76030_set_ichg_current(struct sd76030_chip *chip, uint32_t ichg)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;
	uint8_t  r08_data = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R08,&r08_data);
	//bmt_dbg("read reg 0x%02X ,ret = %d,data = 0x%02X\n",SD76030_R08,ret,r08_data);
	#if 0
	if(ret < 0 || (r08_data & 0x01))
		ichg = 1020;
	#endif
	if (ichg < ICHG_CURR_MIN)
		data_temp = ICHG_CURR_MIN;
	else if (ichg > ICHG_CURR_MAX)
		data_temp = ICHG_CURR_MAX;
	else
		data_temp = ichg;

	data_reg = (data_temp - ICHG_CURR_MIN)/ ICHG_CURR_STEP;

	return sd76030_update(chip,SD76030_R02,data_reg,CON2_ICHG_MASK,CON2_ICHG_SHIFT);
}

static int32_t sd76030_get_ichg_current(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_ICHG_MASK << CON2_ICHG_SHIFT)) >> CON2_ICHG_SHIFT;

	return (data_reg * ICHG_CURR_STEP + ICHG_CURR_MIN);
}

// /* CON3---------------------------------------------------- */
static int32_t sd76030_set_iprechg_current(struct sd76030_chip *chip, uint32_t iprechg_curr)
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

	return sd76030_update(chip,SD76030_R03,data_reg,CON3_IPRECHG_MASK,CON3_IPRECHG_SHIFT);
}

int32_t sd76030_set_vreg_volt(struct sd76030_chip *chip, uint32_t vreg_chg_vol)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (vreg_chg_vol < VREG_VOL_MIN)
		data_temp = VREG_VOL_MIN;
	else if (vreg_chg_vol > VREG_VOL_MAX)
		data_temp = VREG_VOL_MAX;
	else
		data_temp = vreg_chg_vol;

	data_reg = (data_temp - VREG_VOL_MIN ) / (VREG_VOL_STEP >> 1);
	sd76030_update(chip, SD76030_R0D, data_reg & 1, COND_CV_SP_MASK, COND_CV_SPP_SHIFT);
	return sd76030_update(chip, SD76030_R04, data_reg >> 1, CON4_VREG_MASK, CON4_VREG_SHIFT);
}
static int32_t sd76030_get_vreg_volt(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	uint8_t data_reg1 = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip, SD76030_R04, &data_reg);
	if (ret < 0)
		return ret;
	ret = sd76030_read_byte(chip, SD76030_R0D, &data_reg1);
	if (ret < 0)
		return ret;

	data_reg = (data_reg & (CON4_VREG_MASK << CON4_VREG_SHIFT)) >> CON4_VREG_SHIFT;
	data_reg1 = (data_reg1 >> COND_CV_SPP_SHIFT) & COND_CV_SP_MASK;

	return (data_reg * VREG_VOL_STEP + VREG_VOL_MIN) + (data_reg1 * VREG_VOL_STEP / 2);
}

static int32_t sd76030_set_rechg_volt(struct sd76030_chip *chip, uint32_t rechg_volt)
{
	uint8_t data_reg = 0;

	if(rechg_volt >= 200)
		data_reg = 0x01;
	else
		data_reg = 0x00;

	return sd76030_update(chip,SD76030_R04,data_reg,CON4_VRECHG_MASK,CON4_VRECHG_SHIFT);
}
// /* CON5---------------------------------------------------- */
static int32_t sd76030_set_en_term_chg(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R05,!!enable,CON5_EN_TERM_CHG_MASK,CON5_EN_TERM_CHG_SHIFT);
}
static int32_t sd76030_set_wd_timer(struct sd76030_chip *chip, uint32_t second)
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

	return sd76030_update(chip,SD76030_R05,data_reg,CON5_WTG_TIM_SET_MASK,CON5_WTG_TIM_SET_SHIFT);
}

static int32_t sd76030_set_en_chg_timer(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R05,!!enable,CON5_EN_TIMER_MASK,CON5_EN_TIMER_SHIFT);
}
static int32_t sd76030_get_en_chg_timer(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R05,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON5_EN_TIMER_MASK << CON5_EN_TIMER_SHIFT)) >> CON5_EN_TIMER_SHIFT;

	return !!data_reg;
}

// /* CON6---------------------------------------------------- */
static int32_t sd76030_set_boost_vlim(struct sd76030_chip *chip, uint32_t boost_vlim)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (boost_vlim < BOOST_VOL_MIN)
		data_temp = BOOST_VOL_MIN;
	else if (boost_vlim > BOOST_VOL_MAX)
		data_temp = BOOST_VOL_MAX;
	else
		data_temp = boost_vlim;

	data_reg = (data_temp - BOOST_VOL_MIN)/ BOOST_VOL_STEP;

	return sd76030_update(chip,SD76030_R06,data_reg,CON6_BOOST_VLIM_MASK,CON6_BOOST_VLIM_SHIFT);
}
static int32_t sd76030_get_boost_vlim(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R06,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON6_BOOST_VLIM_MASK<< CON6_BOOST_VLIM_SHIFT)) >> CON6_BOOST_VLIM_SHIFT;

	return (data_reg * BOOST_VOL_STEP + BOOST_VOL_MIN);
}

static int32_t sd76030_set_shipping_mode(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R07,!!enable,BATFET_DIS_MASK,BATFET_DIS_SHIFT);;
}
static int32_t sd76030_get_shipping_mode(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R07,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (BATFET_DIS_MASK<< BATFET_DIS_SHIFT)) >> BATFET_DIS_SHIFT;

	return !!data_reg;
}

static int32_t sd76030_set_vlimit(struct sd76030_chip *chip, uint32_t vlimit_mv)
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
	ret = sd76030_update(chip,SD76030_R10,1,CON10_HV_VINDPM_MASK,CON10_HV_VINDPM_SHIFT);
	if(ret < 0)
		return ret;

	return sd76030_update(chip,SD76030_R10,data_reg,CON10_VLIMIT_MASK,CON10_VLIMIT_SHIFT);
}
static int32_t sd76030_get_vlimit(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;
	uint32_t vlimit = 0;

	ret = sd76030_read_byte(chip,SD76030_R10,&data_reg);
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
		ret = sd76030_read_byte(chip,SD76030_R06,&data_reg);
		if(ret < 0)
			return ret;

		data_reg = (data_reg & (CON6_VINDPM_MASK << CON6_VINDPM_SHIFT)) >> CON6_VINDPM_SHIFT;
		vlimit = (data_reg * VINDPM_VOL_STEP+ VINDPM_VOL_MIN);
	}
	return vlimit;
}

#if 0
static int32_t sd76030_enable_hvdcp(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R0C,!!enable,CONC_EN_HVDCP_MASK,CONC_EN_HVDCP_SHIFT);;
}
#endif

//for dpdm regulator start
static int sd76030_request_dpdm(struct sd76030_chip *chg, bool enable)
{
	int rc = 0;

	/* fetch the DPDM regulator */
	if (!chg->dpdm_reg && of_get_property(chg->dev->of_node,
				"dpdm-supply", NULL))
	{
		chg->dpdm_reg = devm_regulator_get(chg->dev, "dpdm");
		if (IS_ERR(chg->dpdm_reg))
		{
			rc = PTR_ERR(chg->dpdm_reg);
			bmt_dbg( "%s Couldn't get dpdm regulator rc=%d\n",__func__,rc);
			chg->dpdm_reg = NULL;
			return rc;
		}
	}

	mutex_lock(&chg->dpdm_lock);
	if (enable)
	{
		if (chg->dpdm_reg && !chg->dpdm_enabled)
		{
			bmt_dbg("enabling DPDM regulator\n");
			rc = regulator_enable(chg->dpdm_reg);
			if (rc < 0)
				bmt_dbg("%s  Couldn't enable dpdm regulator rc=%d\n",__func__,rc);
			else
				chg->dpdm_enabled = true;
		}
	} else
	{
		if (chg->dpdm_reg && chg->dpdm_enabled)
		{
			bmt_dbg("disabling DPDM regulator\n");
			rc = regulator_disable(chg->dpdm_reg);
			if (rc < 0)
				bmt_dbg("%s  Couldn't disable dpdm regulator rc=%d\n",__func__,rc);
			else
				chg->dpdm_enabled = false;
		}
	}
	mutex_unlock(&chg->dpdm_lock);

	return rc;
}
//for dpdm regulator end

#ifdef SD76030_TEMPERATURE_ENABLE
static one_latitude_data_t sd76030_cell_temp_data[TEMPERATURE_DATA_NUM] = {
	{681,   115}, {766,   113}, {865,   105},
	{980,   100}, {1113,   95}, {1266,   90},
	{1451,   85}, {1668,   80}, {1924,   75},
	{2228,   70}, {2588,   65}, {3020,   60},
	{3536,   55}, {4160,   50}, {4911,   45},
	{5827,   40}, {6940,   35}, {8313,   30},
	{10000,  25}, {12090,  20}, {14690,  15},
	{17960,  10}, {22050,   5}, {27280,   0},
	{33900,  -5}, {42470, -10}, {53410, -15},
	{67770, -20},
};
static int32_t one_latitude_table(int32_t number,one_latitude_data_t *data,int32_t value)
{
	int32_t j = 0;
	int32_t res = 0;

	for (j = 0;j < number;j++)
	{
		if (data[j].x ==value)
		{
			res = data[j].y;
			return res;
		}
		if(data[j].x > value)
			break;
	}

	if(j == 0)
		res = data[j].y;
	else if(j == number)
		res = data[j-1].y;
	else
	{
		res = ((value - data[j-1].x) * (data[j].y - data[j-1].y));

		if((data[j].x - data[j -1 ].x) != 0)
			res = res / (data[j].x  - data[j-1].x );
		res += data[j-1].y;
	}
	return res;
}
// /* CON8---------------------------------------------------- */
//adc value is mul 1000; 21% is 21000 100% is 100000
//adc_val is adc value of tspct mul 1000,such as 21000 is 21%
static int32_t sd76030_tchg_convert(int32_t adc_val)
{
	int32_t tchg = 25;
	int32_t Rntc = 0;
	unsigned long long Vdown = (unsigned long long)adc_val;
	unsigned long long Vpull = 100000 - Vdown;
	unsigned long long Rdown = SD76030_R_DOWN;
	unsigned long long Rpull = SD76030_R_PULL;
	unsigned long long val1  = Vdown * Rpull * Rdown;
	unsigned long long val2  = (Vpull * Rdown) - (Vdown * Rpull);

// #ifdef CONFIG_ARM64
// 	Rntc = val1 / val2;
// #else
	do_div(val1,val2);
	Rntc = val1;
//#endif
	tchg = one_latitude_table(TEMPERATURE_DATA_NUM,sd76030_cell_temp_data,Rntc);

	//bmt_dbg("Rdown %lld, Rpull %lld,Vdown %lld,Vpull %lld,Rntc %d, tchg %d\n",Rdown,Rpull,Vdown,Vpull,Rntc,tchg);
	return tchg;
}
#else
static int32_t sd76030_tchg_convert(int32_t adc_val)
{
	return 25;
}
#endif

int32_t sd76030_get_charger_status(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R08,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT;
	return data_reg;
}

static int32_t sd76030_get_adc_value(struct sd76030_chip *chip,enum sd76030_adc_channel adc)
{
	uint8_t val = 0;
	uint8_t data = 0;
	int32_t ret = 0, wait_cnt = 0, adc_value = 0, ilimit = 0;
	if(SD76030_COMPINENT_ID != chip->component_id)
		return -1;

	mutex_lock(&chip->adc_lock);
#if 1
	// one shot ac conversion
	ret = sd76030_write_byte(chip,SD76030_R11,0x00);
	if(ret < 0)
		goto error;
	//start adc conversion
	ret = sd76030_update(chip,SD76030_R11,1,CON11_START_ADC_MASK,CON11_START_ADC_SHIFT);
	if(ret < 0)
		goto error;
	do
	{
		usleep_range(10000,20000);
		wait_cnt++;
		ret = sd76030_read_byte(chip,SD76030_R11,&val);
		if(ret < 0)
			goto error;
	}while((val & (CON11_START_ADC_MASK << CON11_START_ADC_SHIFT)) && wait_cnt < 100);
//	bmt_dbg("wait cnt %d\n",wait_cnt);
#else
	//start 1s continuous conversion
	ret = sd76030_update(chip,SD76030_R11,1,CON11_CONV_RATE_MASK,CON11_CONV_RATE_SHIFT);
	if(ret < 0)
		goto error;
#endif

	if(wait_cnt < 100)
	{
		ret = sd76030_read_byte(chip,SD76030_ADC_CHANNEL_START + adc,&val);
		if(ret < 0)
			goto error;
		switch(adc)
		{
			case vbat_channel:
			case vsys_channel:
				data = val;
				data = data &~(1 << 7);
				adc_value = 2304 + data * 20;
				break;
			case tbat_channel:
				data = val;
				data = data &~(1 << 7);
				adc_value = sd76030_tchg_convert(21000 + 465 * val);
				break;
			case vbus_channel:
				data = val;
				data = data &~(1 << 7);
				adc_value = (0 == data) ? 0 : (2600 + data * 100);
				break;
			case ibat_channel:
				adc_value = val * 25;
				break;
			case ibus_channel:
				if(true == chip->attach)
				{
					ilimit = sd76030_get_input_curr_lim(chip);
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
	bmt_dbg("%s[0x%02X]: Regval=0x%02X, Adcval=%d\n",adc_channel[adc],SD76030_ADC_CHANNEL_START + adc,val,adc_value);
	mutex_unlock(&chip->adc_lock);
	return adc_value;

error:
	mutex_unlock(&chip->adc_lock);
	return -1;
}

static int32_t sd76030_dump_msg(struct sd76030_chip * chip)
{
	uint8_t i = 0;
	uint8_t data[SD76030_REG_NUM] = { 0 };
	int32_t ret = 0;
	for (i = 0; i < SD76030_REG_NUM; i++)
	{
		ret = sd76030_read_byte(chip,i, &data[i]);
		if (ret < 0)
		{
			pr_err("i2c transfor error\n");
			return -1;
		}
	}

	bmt_dbg("[0x00]=0x%02X, [0x01]=0x%02X, [0x02]=0x%02X, [0x03]=0x%02X, [0x04]=0x%02X, [0x05]=0x%02X, [0x06]=0x%02X, [0x07]=0x%02X\n",
			 data[0x00],    data[0x01],    data[0x02],    data[0x03],    data[0x04],    data[0x05],    data[0x06],    data[0x07]);

	bmt_dbg("[0x08]=0x%02X, [0x09]=0x%02X, [0x0A]=0x%02X, [0x0B]=0x%02X, [0x0C]=0x%02X, [0x0D]=0x%02X, [0x0E]=0x%02X, [0x0F]=0x%02X\n",
			data[0x08],	 data[0x09],	data[0x0A],	data[0x0B],	data[0x0C],	data[0x0D],	data[0x0E],	data[0x0F]);

	bmt_dbg("[0x10]=0x%02X\n",data[0x10]);

	//charge parameter
	bmt_dbg("vchg:%d, ichg:%d, ilimit:%d, iterm %d, vlimit %d, sftimer %d, iboost %d\n",
		sd76030_get_vreg_volt(chip),
		sd76030_get_ichg_current(chip),
		sd76030_get_input_curr_lim(chip),
		sd76030_get_iterm(chip),
		sd76030_get_vlimit(chip),
		sd76030_get_en_chg_timer(chip),
		sd76030_get_boost_ilim(chip)
	);

	//fault and state
	bmt_dbg("chg_en:%d, chg_state:%s, vbus_type:%s, pg:%d, thm:%d, vsys_state:%d\n",
		sd76030_get_charger_en(chip),
		charge_state[(data[SD76030_R08] & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT],
		vbus_type[(data[SD76030_R08] & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT],
		(data[SD76030_R08] & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT,
		(data[SD76030_R08] & (CON8_THM_STAT_MASK << CON8_THM_STAT_SHIFT)) >> CON8_THM_STAT_SHIFT,
		(data[SD76030_R08] & (CON8_VSYS_STAT_MASK << CON8_VSYS_STAT_SHIFT)) >> CON8_VSYS_STAT_SHIFT
	);

	bmt_dbg("fault:%d, vbus_gd:%d, vdpm:%d, idpm:%d, acov:%d\n",
		data[SD76030_R09],
		(data[SD76030_R0A] & (CONA_VBUS_GD_MASK << CONA_VBUS_GD_SHIFT)) >> CONA_VBUS_GD_SHIFT,
		(data[SD76030_R0A] & (CONA_VINDPM_STAT_MASK << CONA_VINDPM_STAT_SHIFT)) >> CONA_VINDPM_STAT_SHIFT,
		(data[SD76030_R0A] & (CONA_IDPM_STAT_MASK << CONA_IDPM_STAT_SHIFT)) >> CONA_IDPM_STAT_SHIFT,
		(data[SD76030_R0A] & (CONA_ACOV_STAT_MASK << CONA_ACOV_STAT_SHIFT)) >> CONA_ACOV_STAT_SHIFT
	);

	for (i = vbat_channel; i < max_channel; i++)
		sd76030_get_adc_value(chip,i);
	return 0;
}

static int32_t sd76030_set_dpdm(struct sd76030_chip *chip,uint8_t dp_val, uint8_t dm_val)
{
	uint8_t data_reg = 0;
	data_reg  = (dp_val & CONC_DP_VOLT_MASK) << CONC_DP_VOLT_SHIFT;
	data_reg |= (dm_val & CONC_DM_VOLT_MASK) << CONC_DM_VOLT_SHIFT;
	//bmt_dbg("data_reg 0x%02X\n",data_reg);
	return sd76030_update(chip,SD76030_R0C,data_reg,CONC_DP_DM_MASK,CONC_DP_DM_SHIFT);
}

static int32_t sd76030_charging_set_hvdcp20(struct sd76030_chip *chip,uint32_t vbus_target)
{
	int32_t ret = 0;
	bmt_dbg("set vbus target %d\n",vbus_target);
	switch(vbus_target)
	{
		case 5:
			sd76030_set_vlimit(chip,chip->vlimit_mv);
			ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P0);
			break;

		case 9:
			sd76030_set_vlimit(chip,8000);
			ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_3P3,CONC_DP_DM_VOL_0P6);
			break;

		case 12:
			sd76030_set_vlimit(chip,11000);
			ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P6);
			break;

		default://HIZ
			sd76030_set_vlimit(chip,chip->vlimit_mv);
			ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
			break;
	}
 	return (ret < 0) ? ret : 0;
}
// static int32_t sd76030_charging_enable_hvdcp30(struct sd76030_chip *chip, bool enable)
// {
// 	pr_err("enable %d\n",enable);
// 	//enter continuous mode DP 0.6, DM 3.3
// 	return sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_3P3);
// }

// static int32_t sd76030_charging_set_hvdcp30(struct sd76030_chip *chip, bool increase)
// {
// 	int32_t ret = 0;

// 	pr_err("increase %d\n",increase);

// 	if(increase)
// 	{
// 		//DP 3.3,DM 3.3
// 		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_3P3,CONC_DP_DM_VOL_3P3);
// 		if(ret < 0)
// 			return ret;
// 		//need test
// 		msleep(100);
// 		//DP 0.6,DM 3.3
// 		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_3P3);
// 		if(ret < 0)
// 			return ret;
// 		msleep(100);
// 	}
// 	else
// 	{
// 		//DP 0.6,DM 3.3
// 		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P6);
// 		if(ret < 0)
// 			return ret;
// 		//need test
// 		msleep(100);
// 		//DP 0.6,DM 3.3
// 		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_3P3);
// 		if(ret < 0)
// 			return ret;
// 		msleep(100);
// 	}
// 	return 0;
// }
static int32_t	sd76030_extcon_clear_types(struct sd76030_chip *chip)
{
	extcon_set_state_sync(chip->edev, EXTCON_USB, false);
	extcon_set_state_sync(chip->edev, EXTCON_USB_HOST, false);
//	extcon_set_state_sync(chip->edev, EXTCON_CHG_USB_FAST, false);
//	extcon_set_state_sync(chip->edev, EXTCON_CHG_USB_SDP, false);
	extcon_set_state_sync(chip->edev, EXTCON_NONE, false);
	return 0;
}
static int32_t	sd76030_extcon_notify(struct sd76030_chip *chip, uint32_t extcon_id, bool data)
{
	if (chip->edev)
		return extcon_set_state_sync(chip->edev, extcon_id, data);
	bmt_dbg("Notfiy(%d) Extcon %d Failed(No edev)\n", extcon_id, data);
	return -1;
}

static void sd76030_notify_extcon_props(struct sd76030_chip *chip, int id)
{
	union extcon_property_value val;

	val.intval = true;
	extcon_set_property(chip->edev, id, EXTCON_PROP_USB_SS, val);
}

static void sd76030_notify_device_mode(struct sd76030_chip *chip, bool enable)
{
	if (enable)
		sd76030_notify_extcon_props(chip, EXTCON_USB);

	extcon_set_state_sync(chip->edev, EXTCON_USB, enable);
}

static void sd76030_notify_usb_host(struct sd76030_chip *chip, bool enable)
{
	union extcon_property_value val = {.intval = 0};

	val.intval = 1;
	if (enable) {
		extcon_set_property(chip->edev, EXTCON_USB_HOST, EXTCON_PROP_USB_SS, val);
	}

	extcon_set_state_sync(chip->edev, EXTCON_USB_HOST, enable);
}

static int32_t sd76030_bc12_postprocess(struct sd76030_chip *chip)
{
	int32_t ret = 0;
	bool attach = false, inform_psy = true, usbdev_state = false;
	uint8_t port = SD76030_PORTSTAT_NO_INPUT;
	int sd76030_chg_current = CHG_SDP_CURR_MAX;
	if(SD76030_COMPINENT_ID != chip->component_id)
		return -ENOTSUPP;

	attach = atomic_read(&chip->vbus_gd);
	if (chip->attach == attach)
	{
		bmt_dbg("attach(%d) is the same\n", attach);
		inform_psy = !attach;
		goto out;
	}
	chip->attach = attach;
	bmt_dbg("attach = %d\n", attach);
	sd76030_set_vlimit(chip, DEFAULT_VLIMIT);

	if (!attach) 
	{
		chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		chip->port = SD76030_PORTSTAT_NO_INPUT;
		chip->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		chip->charge_afc = 0;
		sd76030_extcon_clear_types(chip);
		usbdev_state = false;
		vote(chip->usb_icl_votable, DCP_CHG_VOTER, false, 0);
		goto out;
	}

	ret = sd76030_read_byte(chip, SD76030_R08, &port);
	if (ret < 0)
		chip->port = SD76030_PORTSTAT_NO_INPUT;
	else
		chip->port = (port & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;

	bmt_dbg("chg_type %s\n",vbus_type[chip->port]);

	switch (chip->port)
	{
		case SD76030_PORTSTAT_NO_INPUT:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			chip->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
			sd76030_extcon_clear_types(chip);
			usbdev_state = false;
			break;
		case SD76030_PORTSTAT_SDP:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB;
			sd76030_set_input_curr_lim(chip,500);
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = true;
			break;
		case SD76030_PORTSTAT_CDP:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = true;
			break;
		case SD76030_PORTSTAT_DCP:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = false;
			break;
		case SD76030_PORTSTAT_HVDCP:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			//chip->chg_type = POWER_SUPPLY_TYPE_USB_HVDCP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = false;
			break;
		case SD76030_PORTSTAT_UNKNOWN:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			chip->chg_type = POWER_SUPPLY_TYPE_USB;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = false;
			break;
		case SD76030_PORTSTAT_NON_STANDARD:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = false;
			break;
		//case SD76030_PORTSTAT_OTG:
		default:
			chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			chip->chg_type = POWER_SUPPLY_TYPE_USB;
			sd76030_extcon_notify(chip, EXTCON_USB, true);
			usbdev_state = false;
			break;
	}
	//usb or plugout disable ir comp
	if(POWER_SUPPLY_TYPE_USB == chip->chg_type || POWER_SUPPLY_TYPE_UNKNOWN == chip->chg_type)
	{
		bmt_dbg("close ir comp\n");
		sd76030_update(chip,SD76030_R0D,0x00,0x3F,0);
	}
	else
	{
		bmt_dbg("open ir comp\n");
		sd76030_update(chip,SD76030_R0D,0x11,0x3F,0);
	}
	//afc
	if (chip->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		pr_err("unknown type\n");
	} else if (chip->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP ||
		chip->psy_usb_type == POWER_SUPPLY_USB_TYPE_CDP ||
		chip->psy_usb_type == POWER_SUPPLY_USB_TYPE_FLOAT) {
		sd76030_request_dpdm(chip,false);
		sd76030_notify_usb_host(chip, false);
		sd76030_notify_device_mode(chip, true);
		if (chip->psy_usb_type == POWER_SUPPLY_USB_TYPE_CDP) {
			sd76030_chg_current = CHG_CDP_CURR_MAX;
		} else {
			sd76030_chg_current = CHG_SDP_CURR_MAX;
		}
		sd76030_set_ichg_current(chip, sd76030_chg_current);

	} else {
		pr_err("dcp deteced\n");
		sd76030_set_ichg_current(chip, CHG_AFC_CURR_MAX);
		// sd76030_enable_hvdcp(chip, true);
		sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_HIZ);
		sd76030_set_input_curr_lim(chip, 500);
		schedule_delayed_work(&chip->hvdcp_dwork, msecs_to_jiffies(1400));
	}

out:
	sd76030_extcon_notify(chip, EXTCON_USB, usbdev_state);
	if (inform_psy)
	{
		cancel_delayed_work_sync(&chip->psy_dwork);
		schedule_delayed_work(&chip->psy_dwork, 0);
	}

	return 0;
}

static void sd76030_hvdcp_dwork(struct work_struct *work)
{
	int ret;
	struct sd76030_chip *chip = container_of(
		work, struct sd76030_chip, hvdcp_dwork.work);

	ret = afc_set_voltage_workq(AFC_QUICK_CHARGE_POWER_CODE);
	if (!ret) {
		pr_info("set afc adapter iindpm to 1700mA\n");
		sd76030_set_input_curr_lim(chip, 1700);
		chip->charge_afc = 1;
	} else {
		pr_err("afc communication failed, restore adapter iindpm to 2000mA\n");
		sd76030_set_ichg_current(chip, CHG_DCP_CURR_MAX);
		vote(chip->usb_icl_votable, DCP_CHG_VOTER, true, DCP_ICL_CURR_MAX);
		chip->charge_afc = 0;
	}
}

static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ua, const char *client)
{
	struct sd76030_chip *chip = data;
	int rc = 0;

	if (fcc_ua < 0) {
		pr_err("fcc_ua: %d < 0, ERROR!\n", fcc_ua);
		return 0;
	}

	if (fcc_ua > SD76030_MAX_FCC)
		fcc_ua = SD76030_MAX_FCC;

	rc = sd76030_set_ichg_current(chip, fcc_ua);
	if (rc < 0) {
		pr_err("failed to set charge current\n");
		return rc;
	}

	pr_info(" fcc:%d\n", fcc_ua);

	return 0;
}

static int chg_dis_vote_callback(struct votable *votable, void *data,
			int disable, const char *client)
{
	struct sd76030_chip *chip  = data;
	int rc = 0;

	if (disable)
		rc = sd76030_set_charger_en(chip,0);
	else
		rc = sd76030_set_charger_en(chip,1);

	if (rc < 0)
	{
		pr_err("failed to disable:%d\n", rc);
		return rc;
	}

	return 0;
}

static int fv_vote_callback(struct votable *votable, void *data,
			int fv_mv, const char *client)
{
	struct sd76030_chip *chip = data;
	int rc = 0;

	if (fv_mv < 0) {
		pr_err("fv_mv: %d < 0, ERROR!\n", fv_mv);
		return 0;
	}

	rc = sd76030_set_vreg_volt(chip, fv_mv);
	if (rc < 0) {
		pr_err("failed to set chargevoltage\n");
		return rc;
	}

	pr_info(" fv:%d\n", fv_mv);
	return 0;
}

static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	struct sd76030_chip *chip = data;
	int rc;

	if (icl_ma < 0)
	{
		pr_err("icl_ma: %d < 0, ERROR!\n", icl_ma);
		return 0;
	}

	if (icl_ma > SD76030_MAX_ICL)
		icl_ma = SD76030_MAX_ICL;

	rc = sd76030_set_input_curr_lim(chip, icl_ma);
	pr_err("ch_log  %s  %d\n",__func__,icl_ma);
	if (rc < 0)
	{
		pr_err("failed to set input current limit\n");
		return rc;
	}

	return 0;
}
//end
#ifdef TRIGGER_CHARGE_TYPE_DETECTION
static int32_t sd76030_chg_type_det(struct sd76030_chip *chip)
{
	int32_t ret = 0;
	uint8_t val = 0;
	int32_t wait_cnt = 0;
	uint8_t chg_type = 0;
	//if use the irq of sd76030,chg type will report in irq function
	if (-1 != chip->irq_gpio)
		return 0;

	ret = sd76030_update(chip,SD76030_R07,1,FORCE_IINDET_MASK,FORCE_IINDET_SHIFT);
	if(ret < 0)
		return ret;
	do
	{
		msleep(100);
		wait_cnt++;
		ret = sd76030_read_byte(chip,SD76030_R07,&val);
		if(ret < 0)
			return ret;
	}while((val & (FORCE_IINDET_MASK << FORCE_IINDET_SHIFT)) && wait_cnt < 30);
	bmt_dbg("wait cnt %d\n",wait_cnt);
	if(wait_cnt < 30)
	{
		ret = sd76030_read_byte(chip,SD76030_R08,&val);
		if(ret < 0)
			return ret;

		chg_type = (val & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;
		bmt_dbg("chg_type %s\n",vbus_type[chg_type]);
	}
	else
		return -1;
	return 0;
}

static int32_t sd76030_bc12_en_kthread(void *data)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = data;
	bmt_dbg("\n");

	while(1)
	{
		wait_event(chip->bc12_en_req, atomic_read(&chip->bc12_en_req_cnt) > 0 || kthread_should_stop());
		if (atomic_read(&chip->bc12_en_req_cnt) <= 0 && kthread_should_stop()) 
		{
			pr_err( "bye bye\n");
			return 0;
		}
		atomic_dec(&chip->bc12_en_req_cnt);
		pm_stay_awake(chip->dev);
		ret = sd76030_chg_type_det(chip);
		if (ret < 0)
			bmt_dbg("fail(%d)\n",ret);

		sd76030_bc12_postprocess(chip);
		pm_relax(chip->dev);
	}
	return 0;
}

#if 0
static int32_t sd76030_charge_type_detection(struct sd76030_chip *chip)
{
	if(SD76030_COMPINENT_ID != chip->component_id)
		return -ENOTSUPP;

	if (atomic_read(&chip->vbus_gd))
	{
		atomic_inc(&chip->bc12_en_req_cnt);
		wake_up(&chip->bc12_en_req);
	}
	return 0;
}
#endif
#endif

static void sd76030_inform_psy_dwork_handler(struct work_struct *work)
{
	int32_t ret = 0;
#define HVDCP20_RETRY	3
#define VBUS_ADC_RETRY	3

	union power_supply_propval propval = {.intval = 0};
	struct sd76030_chip *chip = container_of(work, struct sd76030_chip,psy_dwork.work);
	bool attach = false;
	//enum power_supply_type chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
	enum power_supply_usb_type psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	int32_t i = 0, retry = 0;
	int32_t vbus_min = chip->target_hv * 1000 - 700;
	int32_t vbus_max = chip->target_hv * 1000 + 700; //700mv offset

#if 0 //test, will set usb offline, #if 0, if do not test
	chip->attach = 0;
	chip->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
#endif
	mutex_lock(&chip->bc12_lock);
	attach = chip->attach;
	psy_usb_type = chip->psy_usb_type;
	mutex_unlock(&chip->bc12_lock);

	pr_info("attach = %d, type = %d\n",attach, psy_usb_type);

	/* Get chg type det power supply */
	if (!chip->psy)
		chip->psy = power_supply_get_by_name("bbc");
	if (!chip->psy) {
		bmt_dbg("get power supply fail\n");
		mod_delayed_work(system_wq, &chip->psy_dwork,msecs_to_jiffies(1000));
		return;
	}
	propval.intval = attach;
	ret = power_supply_set_property(chip->psy, POWER_SUPPLY_PROP_ONLINE,&propval);
	if (ret < 0)
		bmt_dbg("psy online fail(%d)\n",ret);

	propval.intval = psy_usb_type;
	ret = power_supply_set_property(chip->psy,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
	if (ret < 0)
		bmt_dbg("psy type fail(%d)\n", ret);

	if((9 == chip->target_hv || 12 == chip->target_hv) 	&& 
		//(POWER_SUPPLY_USB_TYPE_DCP == psy_usb_type) //|| POWER_SUPPLY_TYPE_USB_HVDCP == chg_type)
		(POWER_SUPPLY_USB_TYPE_DCP == psy_usb_type )
		)
	{
hvdcp_retry:
		if (!atomic_read(&chip->vbus_gd))
		{
			bmt_dbg("hvdcp retry failed,vbus_gd is 0)\n", ret);
			return;
		}
		sd76030_charging_set_hvdcp20(chip,5);
		msleep(1500);
		sd76030_charging_set_hvdcp20(chip,chip->target_hv);

		if(SD76030_COMPINENT_ID == chip->component_id)
		{
			for(i=0;i<VBUS_ADC_RETRY;i++)
			{
				if (!atomic_read(&chip->vbus_gd))
				{
					bmt_dbg("hvdcp get adc failed!\n");
					return;
				}
				ret = sd76030_get_adc_value(chip,vbus_channel);
				bmt_dbg("target_hv %d vbus_min %d vbus_max %d vbus finally %d\n",chip->target_hv,vbus_min,vbus_max,ret);
				if(ret >= vbus_min && ret <= vbus_max)
					break;
				else
					msleep(500);
			}
			if(i >= VBUS_ADC_RETRY)
			{
				retry++;
				pr_err("set hvdcp2.0 failed, retry %d\n",retry);
				if(retry <= HVDCP20_RETRY)
					goto hvdcp_retry;
			}
			//set hvdcp2.0 success
			if(retry <= HVDCP20_RETRY)
			{
				propval.intval = POWER_SUPPLY_USB_TYPE_DCP;
				chip->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
				ret = power_supply_set_property(chip->psy,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
				if (ret < 0)
					bmt_dbg("psy type fail(%d)\n", ret);
				sd76030_extcon_notify(chip, EXTCON_USB, true);
				//sd76030_extcon_notify(chip, EXTCON_CHG_USB_DCP, false);
			}
			else
			{
				sd76030_charging_set_hvdcp20(chip,5);
				// propval.intval = POWER_SUPPLY_TYPE_USB_DCP;
				// ret = power_supply_set_property(chip->psy,POWER_SUPPLY_PROP_REAL_TYPE,&propval);
				// if (ret < 0)
				// 	bmt_dbg("psy type fail(%d)\n", ret);
			}
		}
	}
}

static int32_t sd76030_detect_ic(struct sd76030_chip *chip)
{
	uint8_t hw_id = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R0B,&hw_id);
	if(ret < 0)
		return ret;

	bmt_dbg("hw_id:0x%02X\n",hw_id);
	if(0x38 != hw_id)
		return -1;

	return 0;
}

static int32_t sd76030_get_component_id(struct sd76030_chip *chip)
{
	uint8_t chip_id = 0, version_id = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R0E,&chip_id);
	if(ret < 0)
		return ret;

	ret = sd76030_read_byte(chip,SD76030_R0F,&version_id);
	if(ret < 0)
		return ret;

	chip->component_id = chip_id << 8 | version_id;

	bmt_dbg("chip_id:0x%02X,version_id:0x%02X,component_id:0x%04X\n",chip_id,version_id,chip->component_id);
	return 0;
}

static void sd76030_hw_init(struct sd76030_chip *chip)
{
	sd76030_write_byte(chip,SD76030_R00,0x04);
	sd76030_write_byte(chip,SD76030_R01,0x18);
	sd76030_write_byte(chip,SD76030_R02,0xA2);
	sd76030_write_byte(chip,SD76030_R03,0x22);
	sd76030_write_byte(chip,SD76030_R04,0x88);
	sd76030_write_byte(chip,SD76030_R05,0x9F);
	sd76030_write_byte(chip,SD76030_R06,0xE6); //ovp 14V
	sd76030_write_byte(chip,SD76030_R07,0x4C);
	sd76030_write_byte(chip,SD76030_R0A,0x00);
	sd76030_write_byte(chip,SD76030_R0C,0x00);
	sd76030_write_byte(chip,SD76030_R0D,0x11);
	sd76030_write_byte(chip,SD76030_R10,0x86);

	sd76030_set_vlimit(chip,chip->vlimit_mv);
	sd76030_set_input_curr_lim(chip,chip->ilimit_ma);
	sd76030_set_wd_reset(chip);
	sd76030_set_sys_min_volt(chip,chip->vsysmin_mv);
	sd76030_set_iprechg_current(chip,chip->pre_ma);
	sd76030_set_iterm(chip,chip->eoc_ma);
	sd76030_set_vreg_volt(chip,chip->cv_mv);
#if 0
	//modify begin  for Trickle charge
	if(sd76030_get_adc_value(chip,vbat_channel) < chip->trickle_charger_mv)
	{
		chip->is_trickle_charging = true;
		sd76030_set_ichg_current(chip,chip->pre_ma);
		bmt_dbg("kernel in trickle_charging \n");
	}
	else
	{
		chip->is_trickle_charging = false;
		sd76030_set_ichg_current(chip,chip->cc_ma);
		bmt_dbg("kernel not in trickle_charging \n");
	}
	//modify end  for Trickle charge
#endif
	sd76030_set_rechg_volt(chip,chip->rechg_mv);
	sd76030_set_en_term_chg(chip,1);
	sd76030_set_wd_timer(chip,0);
	sd76030_set_en_chg_timer(chip,1);
	sd76030_set_otg_en(chip,0);
	sd76030_set_charger_en(chip,1);
}

#ifdef SW_IR_CHECK
static int32_t sd76030_get_soc(void)
{
	int32_t ret = 0;
	union power_supply_propval value = {0,};
	static struct power_supply* bmt_psy = NULL;

	if(NULL == bmt_psy)
		bmt_psy = power_supply_get_by_name("battery");

	if(bmt_psy)
	{
		ret = power_supply_get_property(bmt_psy,POWER_SUPPLY_PROP_CAPACITY,&value);
		if(0 == ret)
			return value.intval;
	}
	return -128;//as fault
}

static int32_t sd76030_get_temp(void)
{
	int32_t ret = 0;
	union power_supply_propval value = {0,};
	static struct power_supply* bmt_psy = NULL;

	if(NULL == bmt_psy)
		bmt_psy = power_supply_get_by_name("battery");

	if(bmt_psy)
	{
		ret = power_supply_get_property(bmt_psy,POWER_SUPPLY_PROP_TEMP,&value);
		if(0 == ret)
			return value.intval / 10;
	}
	return -128; //as fault
}

static void sd76030_ir_check(struct sd76030_chip *chip)
{
	int32_t soc  = 0;
	int32_t temp = 0;
	static int32_t counter = 0;

	soc  = sd76030_get_soc();
	temp = sd76030_get_temp();
	bmt_dbg("soc now %d, temp now %d, counter %d\n",soc,temp,counter);
	if(-128 == soc || -128 == temp)
		return;

	if(temp <= 10 || temp >= 48)
		counter ++;
	else
		counter = 0;

	if(soc >= 85 || counter >= 10)//disable IR
	{
		bmt_dbg("close ir comp\n");
		counter = 0;
		sd76030_update(chip,SD76030_R0D,0x00,0x3F,0);
	}
}

static int32_t sd76030_get_vbat(void)
{
	int32_t ret = 0;
	union power_supply_propval value = {0,};
	static struct power_supply* bmt_psy = NULL;

	if(NULL == bmt_psy)
		bmt_psy = power_supply_get_by_name("battery");

	if(bmt_psy)
	{
		ret = power_supply_get_property(bmt_psy,POWER_SUPPLY_PROP_VOLTAGE_NOW,&value);
		if(0 == ret)
			return value.intval / 1000;
	}
	return -128;//as fault
}
EXPORT_SYMBOL(sd76030_get_vbat);
#endif

static void sd76030_work_func(struct work_struct *work)
{
	struct sd76030_chip *chip = container_of(work, struct sd76030_chip,sd76030_work.work);
#ifdef SW_IR_CHECK
	sd76030_ir_check(chip);
#endif
	sd76030_dump_msg(chip);
	schedule_delayed_work(&chip->sd76030_work, msecs_to_jiffies(10000));
}

static int32_t sd76030_parse_dt(struct sd76030_chip *chip,struct device *dev)
{
	int32_t ret = 0;
	struct device_node *np = dev->of_node;

	if (!np) {
		pr_err("no of node\n");
		return -ENODEV;
	}
	if (of_property_read_string(np, "charger_name",&chip->chg_dev_name) < 0)
	{
		chip->chg_dev_name = "primary_chg";
		bmt_dbg("no charger name\n");
	}
	bmt_dbg("%s\n",chip->chg_dev_name);
	//irq gpio
	ret = of_get_named_gpio(np,"bigm,irq_gpio",0);
	if (ret < 0)
	{
		bmt_dbg("no bigm,irq_gpio(%d)\n", ret);
		chip->irq_gpio = -1;
	}
	else
		chip->irq_gpio = ret;
	//if config ce gpio, output low
	ret = of_get_named_gpio(np,"bigm,ce_gpio",0);
	if (ret < 0)
	{
		bmt_dbg("no bigm,ce_gpio(%d)\n", ret);
		chip->ce_gpio = -1;
	}
	else
		chip->ce_gpio = ret;

	bmt_dbg("irq_gpio = %d, ce_gpio = %d\n", chip->irq_gpio, chip->ce_gpio);

	if (-1 != chip->ce_gpio)
	{
		ret = devm_gpio_request_one(chip->dev,chip->ce_gpio,GPIOF_DIR_OUT,devm_kasprintf(chip->dev, GFP_KERNEL,"sd76030_ce_gpio.%s", dev_name(chip->dev)));
		if (ret < 0)
		{
			bmt_dbg("ce gpio request fail(%d)\n", ret);
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
	//Trickle_charge_mv
	ret = of_property_read_u32(np, "bigm,Trickle_charge_mv",&chip->trickle_charger_mv);
	if(ret)
		chip->trickle_charger_mv = 0;

	pr_err("ilimit:%dma, vlimit:%dmv, cc:%dma, cv:%dmv, pre:%dma, eoc:%dma, vsysmin:%dmv, vrechg:%dmv, target_hv %dv ,trickle_charger %d mv\n",
					chip->ilimit_ma,chip->vlimit_mv,chip->cc_ma,
					chip->cv_mv,chip->pre_ma,chip->eoc_ma,chip->vsysmin_mv,
					chip->rechg_mv,chip->target_hv,chip->trickle_charger_mv);
	return 0;
}
static uint8_t attr_reg = 0xFF;
static ssize_t show_registers(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	uint8_t i = 0, data = 0;
	int32_t ret  = 0;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	if(0xFF == attr_reg)
	{
		for (i = 0; i < SD76030_REG_NUM; i++)
		{
			data = 0;
			ret = sd76030_read_byte(chip,i, &data);
			count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
		}

		if(SD76030_COMPINENT_ID == chip->component_id)
		{
			//ret = sd76030_update(chip,SD76030_R11,1,CON11_CONV_RATE_MASK,CON11_CONV_RATE_SHIFT);
			ret = sd76030_write_byte(chip,SD76030_R11,0x80);//one shot
			if(ret < 0)
				count += sprintf(&buf[count], "write R11 failed\n");
			else
			{
				msleep(1000);
				for (i = SD76030_R11; i <= SD76030_R17; i++)
				{
					data = 0;
					ret = sd76030_read_byte(chip,i, &data);
					count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
				}
			}
		}
	}
	else
	{
		data = 0;
		ret = sd76030_read_byte(chip,attr_reg, &data);
		count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",attr_reg,data,(ret < 0) ? "failed":"successfully");
	}
	return count;
}

static ssize_t store_registers(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int32_t ret = 0;
	char *pvalue = NULL, *addr = NULL, *val = NULL;
	uint32_t reg_value = 0,reg_address = 0;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

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
		//you can read register immediately，after write
		attr_reg = reg_address;
		val  = strsep(&pvalue, " ");
		ret = kstrtou32(val, 16, (uint32_t *)&reg_value);
		pr_err("write sd76030 reg 0x%02X with value 0x%02X !\n",(uint32_t) reg_address, reg_value);
		sd76030_write_byte(chip,reg_address,reg_value);
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

static ssize_t show_shipping_mode(struct device *dev, struct device_attribute *attr,char *buf)
{
	int32_t val = 0;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	val = sd76030_get_shipping_mode(chip);
	return sprintf(buf, "%d\n", val);
}

static ssize_t store_shipping_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t val;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	if (kstrtoint(buf, 10, &val))
		return -1;

	if(val == 1)
	{
		sd76030_set_shipping_mode(chip,1);
		bmt_dbg("OPEN SHIPPING MODE!\n");
	}
	else if(val == 0)
	{
		sd76030_set_shipping_mode(chip,0);
		bmt_dbg("CLOSE SHIPPING MODE!\n");
	}
	else
	{
		pr_err("INVALID COMMAND!\n");
		return -1;
	}

	return count;
}

static DEVICE_ATTR(registers, 0664, show_registers,store_registers);	/* 664 */
static DEVICE_ATTR(shipping_mode, 0664, show_shipping_mode,store_shipping_mode);	/* 664 */

static struct attribute *sd76030_attributes[] =
{
	&dev_attr_registers.attr,
	&dev_attr_shipping_mode.attr,
	NULL,
};

static struct attribute_group sd76030_attribute_group =
{
	.attrs = sd76030_attributes,
};

static int32_t	sd76030_default_irq(struct sd76030_chip *chip,uint8_t data)
{
	bmt_dbg("%d\n",data);
	return 0;
}
static int32_t	sd76030_chg_state_irq(struct sd76030_chip *chip,uint8_t data)
{
	bmt_dbg("%d %s\n",data,charge_state[data]);
	chip->chg_state = data;
	return 0;
}
static int32_t	sd76030_pg_state_irq(struct sd76030_chip *chip,uint8_t data)
{
	int32_t ret = 0;

	if(SD76030_COMPINENT_ID != chip->component_id)

		return 0;

	bmt_dbg("pg good %d\n",data);
	if(1 == data)
	{
		schedule_delayed_work(&chip->sd76030_work, 0);
		sd76030_request_dpdm(chip,true);
	}
	else
	{
		cancel_delayed_work_sync(&chip->sd76030_work);
		sd76030_request_dpdm(chip,false);
	}

	mutex_lock(&chip->bc12_lock);
	//plug out,reset dpdm
	if (atomic_read(&chip->vbus_gd) && 0 == data)
		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);

	atomic_set(&chip->vbus_gd,data);

	sd76030_bc12_postprocess(chip);

	mutex_unlock(&chip->bc12_lock);

	return ret;
}

#if 0
// static int32_t	sd76030_vbus_good_irq(struct sd76030_chip *chip,uint8_t data)
// {
// 	int32_t ret = 0;
// 	struct chgdev_notify *noti = &(chip->chg_dev->noti);
// 	noti->vbusov_stat = false;
// 	bmt_dbg("data %d\n",data);
// 	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
// #ifndef CONFIG_TCPC_CLASS
// 	mutex_lock(&chip->bc12_lock);
// 	atomic_set(&chip->vbus_gd,data);
// 	// if(1 == data)
// 	// 	ret = sd76030_bc12_preprocess(chip);
// 	mutex_unlock(&chip->bc12_lock);
// #endif /* CONFIG_TCPC_CLASS */
// 	return ret;
// }

static int32_t	sd76030_boost_fault_irq(struct sd76030_chip *chip,uint8_t data)
{
	int32_t ret = 0;
	uint8_t val = 0;
	uint8_t retry_times = 0;
	uint8_t check_times = 0;

	if(1 == data)
	{
		for(retry_times = 0; retry_times < 5; retry_times++)
		{
			sd76030_set_otg_en(chip,0);
			sd76030_set_otg_en(chip,1);
			//check result
			for(check_times = 0; check_times < 5; check_times++)
			{
				ret = sd76030_read_byte(chip,SD76030_R09,&val);
				if(ret < 0)
					continue;

				bmt_dbg("R09 = 0x%02X\n",val);
				if(0 == (val & (CON9_BOOST_STAT_MASK << CON9_BOOST_STAT_SHIFT)))
					break;
			}
			bmt_dbg("check_times = %d\n",check_times);
			//failed, retry
			if(check_times >= 5)
				continue;
			else//successful,break
				break;
		}
	}
	return 0;
}
#endif
static struct sd76030_irq_handle irq_handles[] = {
	{
		SD76030_R08,0,0,
		{
			{
				.irq_name	= "chg state",
				.irq_func	= sd76030_chg_state_irq,
				.irq_mask   	= CON8_CHRG_STAT_MASK,
				.irq_shift	= CON8_CHRG_STAT_SHIFT,
			},
			{
				.irq_name	= "pg state",
				.irq_func	= sd76030_pg_state_irq,
				.irq_mask   	= CON8_PG_STAT_MASK,
				.irq_shift	= CON8_PG_STAT_SHIFT,
			},
			{
				.irq_name	= "therm state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CON8_THM_STAT_MASK,
				.irq_shift	= CON8_THM_STAT_SHIFT,
			},
			{
				.irq_name	= "vsys state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CON8_VSYS_STAT_MASK,
				.irq_shift	= CON8_VSYS_STAT_SHIFT,
			},
		},
	},

	{
		SD76030_R09,0,0,
		{
			{
				.irq_name	= "wathc dog fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CON9_WATG_STAT_MASK,
				.irq_shift	= CON9_WATG_STAT_SHIFT,
			},
			//{
			//	.irq_name	= "boost fault",
			//	.irq_func	= sd76030_boost_fault_irq,
			//	.irq_mask   	= CON9_BOOST_STAT_MASK,
			//	.irq_shift	= CON9_BOOST_STAT_SHIFT,
			//},
			// {
			// 	.irq_name	= "chg fault",
			// 	.irq_func	= sd76030_default_irq,
			// 	.irq_mask   	= CON9_CHRG_FAULT_MASK,
			// 	.irq_shift	= CON9_CHRG_FAULT_SHIFT,
			// },
			//{
			//	.irq_name	= "bat fault",
			//	.irq_func	= sd76030_bat_fault_irq,
			//	.irq_mask   	= CON9_BAT_STAT_MASK,
			//	.irq_shift	= CON9_BAT_STAT_SHIFT,
			//},
			{
				.irq_name	= "ntc fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CON9_NTC_STAT_MASK,
				.irq_shift	= CON9_NTC_STAT_SHIFT,
			},
		},
	},
	{
		SD76030_R0A,0,0,
		{
			// {
			// 	.irq_name	= "vbus good",
			// 	.irq_func	= sd76030_vbus_good_irq,
			// 	.irq_mask   	= CONA_VBUS_GD_MASK,
			// 	.irq_shift	= CONA_VBUS_GD_SHIFT,
			// },
			{
				.irq_name	= "vdpm",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CONA_VINDPM_STAT_MASK,
				.irq_shift	= CONA_VINDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "idpm",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CONA_IDPM_STAT_MASK,
				.irq_shift	= CONA_IDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "topoff active",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CONA_TOPOFF_ACTIVE_MASK,
				.irq_shift	= CONA_TOPOFF_ACTIVE_SHIFT,
			},
			{
				.irq_name	= "acov state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   	= CONA_ACOV_STAT_MASK,
				.irq_shift	= CONA_ACOV_STAT_SHIFT,
			},
		},
	},
};

static int32_t sd76030_process_irq(struct sd76030_chip *chip)
{
	int32_t i, j,ret = 0;
	uint8_t pre_state=0 ,now_state=0;
	int32_t handler_count = 0;

	for (i=0; i<ARRAY_SIZE(irq_handles); i++)
	{
		for(j=0; j < 20; j++) {
			ret = sd76030_read_byte(chip,irq_handles[i].reg,&irq_handles[i].value);
			if (ret >= 0)
				break;
			else {
				dev_err(chip->dev, "Couldn't read reg 0x%02X ret=%d\n",irq_handles[i].reg,ret);
				usleep_range(3000,5000);
			}
		}
		bmt_dbg("reg 0x%02X,value now 0x%02X, value pre 0x%02X\n",irq_handles[i].reg,irq_handles[i].value,irq_handles[i].pre_value);

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
				bmt_dbg("%s: [%d -> %d]\n", irq_handles[i].irq_info[j].irq_name,pre_state,now_state);

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

static irqreturn_t sd76030_irq_handler(int32_t irq, void *data)
{
	struct sd76030_chip *chip = (struct sd76030_chip *)data;

	bmt_dbg("--------------------------\n");
	pm_stay_awake(chip->dev);
	//wait for register modify
	msleep(100);

	mutex_lock(&chip->irq_lock);
	sd76030_process_irq(chip);
	mutex_unlock(&chip->irq_lock);

	pm_relax(chip->dev);

	return IRQ_HANDLED;
}
static enum power_supply_property sd76030_props[] = {

	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
//	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
//	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
//	POWER_SUPPLY_PROP_VOLTAGE_MAX,

};
static int32_t sd76030_get_prop(struct power_supply *psy,enum power_supply_property psp,union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp)
	{
		case POWER_SUPPLY_PROP_STATUS:
			if (!atomic_read(&chip->vbus_gd)) {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
			}
			ret = sd76030_get_charger_status(chip);
			if (ret == 3)
			{
				val->intval = POWER_SUPPLY_STATUS_FULL;
			}
			else if (ret == 0)
			{
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			else
			{
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			val->intval = chip->psy_usb_type;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = chip->attach;
			/*if( chip->attach && chip->chg_type != POWER_SUPPLY_TYPE_USB)
				val->intval = 1;
			else 
				val->intval = 0;*/
			break;
/*
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			ret = sd76030_get_input_curr_lim(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
			ret = sd76030_get_ichg_current(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX:	//cv
			ret = sd76030_get_vreg_volt(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;
*/
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
static int32_t sd76030_set_prop(struct power_supply *psy,enum power_supply_property psp,const union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = power_supply_get_drvdata(psy);
	switch (psp)
	{
		case POWER_SUPPLY_PROP_ONLINE:
			chip->attach = val->intval;
			break;

		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			chip->chg_type = val->intval;
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: //ilimit
			ret = sd76030_set_input_curr_lim(chip,val->intval / 1000);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX: //cc
			ret = sd76030_set_ichg_current(chip,val->intval / 1000);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:		//cv
			ret = sd76030_set_vreg_volt(chip,val->intval / 1000);
			break;
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			if (val->intval == 0)
				ret = sd76030_set_en_term_chg(chip, 1);
			else if (val->intval == -1)
				ret = sd76030_set_en_term_chg(chip, 0);
			else
				ret = sd76030_set_iterm(chip, val->intval);
			break;
		default:
			pr_err("set prop %d is not supported\n", psp);
			ret = -EINVAL;
			break;
	}

//	power_supply_changed(psy);

	return ret;
}

static int32_t sd76030_prop_is_writeable(struct power_supply *psy,enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			return 1;
	default:
		break;
	}

	return 0;
}

static int32_t sd76030_power_init(struct sd76030_chip *chip)
{
	chip->chg_cfg.drv_data = chip;
	chip->chg_cfg.of_node  = chip->dev->of_node;

	chip->chg_desc.name = "bbc";//"usb",
	chip->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	chip->chg_desc.properties = sd76030_props,
	chip->chg_desc.num_properties = ARRAY_SIZE(sd76030_props),
	chip->chg_desc.get_property = sd76030_get_prop,
	chip->chg_desc.set_property = sd76030_set_prop,
	chip->chg_desc.property_is_writeable = sd76030_prop_is_writeable,

	chip->sd76030_psy = devm_power_supply_register(chip->dev,&chip->chg_desc,&chip->chg_cfg);

	if (IS_ERR(chip->sd76030_psy))
	{
		pr_err("Couldn't register power supply sd76030 ac port2\n");
		return PTR_ERR(chip->sd76030_psy);
	}

	return 0;
}
static int32_t sd76030_extcon_init(struct sd76030_chip *chip)
{
//	chip->edev = devm_extcon_dev_allocate(chip->dev,sd76030_extcon_cables);
//	if (IS_ERR(chip->edev)) 
//	{
//		chip->edev = NULL;
//		return -1;
//	}

//	devm_extcon_dev_register(chip->dev, chip->edev);
//	bmt_dbg("Register Extcon(%s) Success.", extcon_get_edev_name(chip->edev));

	int rc;

	pr_info("start !\n");
	if(!chip){
		pr_err("chip is NULL,Fail !\n");
		return -EINVAL;
	}

	/* extcon registration */
	chip->edev = devm_extcon_dev_allocate(chip->dev, sd76030_extcon_cables);
	if (IS_ERR(chip->edev)) {
		rc = PTR_ERR(chip->edev);
		pr_err("failed to allocate extcon device rc=%d\n", rc);
		return rc;
	}

	rc = devm_extcon_dev_register(chip->dev, chip->edev);
	if (rc < 0) {
		pr_err("failed to register edev device rc=%d\n", rc);
		return rc;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(chip->edev,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chip->edev,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(chip->edev,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(chip->edev,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		pr_err("failed to configure extcon capabilities\n");

	return rc;
}

static int32_t sd76030_register_irq(struct sd76030_chip *chip)
{
	int32_t ret = 0;

	bmt_dbg("irq gpio %d\n",chip->irq_gpio);

	if (-1 == chip->irq_gpio)
		return 0;

	ret = devm_gpio_request_one(chip->dev, chip->irq_gpio, GPIOF_DIR_IN,
										devm_kasprintf(chip->dev, GFP_KERNEL,
										"sd76030_irq_gpio.%s", dev_name(chip->dev)));
	if (ret < 0)
	{
		pr_err("gpio request fail(%d)\n", ret);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0)
	{
		pr_err("gpio2irq fail(%d)\n", chip->irq);
		return chip->irq;
	}

	bmt_dbg("irq = %d\n", chip->irq);
	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					sd76030_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING, 
					devm_kasprintf(chip->dev, GFP_KERNEL,
					"sd76030_irq.%s", dev_name(chip->dev)),
					chip);

	if (ret < 0)
	{
		pr_err("request threaded irq fail(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(chip->irq);
	return ret;
}

static int32_t sd76030_enable_otg(struct regulator_dev *rdev)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
	bmt_dbg("\n");

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_set_otg_en(chip,1);

	return (ret < 0) ? ret : 0;
}

static int32_t sd76030_disable_otg(struct regulator_dev *rdev)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
	bmt_dbg("\n");

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_set_otg_en(chip,0);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_is_otg_enabled(struct regulator_dev *rdev)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_get_otg_en(chip);
	bmt_dbg("ret=%d\n",ret);
	if(ret<0)
		ret = 0;

	return ret;
}
static int32_t sd76030_set_otg_voltage(struct regulator_dev* rdev, int32_t min_uV, int32_t max_uV, uint32_t* selector)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
	bmt_dbg("min_uV %d, max_uV %d\n",min_uV,max_uV);

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_set_boost_vlim(chip, min_uV / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_otg_voltage(struct regulator_dev* rdev)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_get_boost_vlim(chip);
	bmt_dbg("ret=%d\n",ret);
	if(ret<0)
		ret = 0;

	return ret * 1000;
}
static int32_t sd76030_set_otg_current(struct regulator_dev* rdev, int32_t min_uA, int32_t max_uA)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
	bmt_dbg("min_uA %d, max_uA %d\n",min_uA,max_uA);

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_set_boost_ilim(chip,min_uA / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_otg_current(struct regulator_dev* rdev)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;

	if(NULL == rdev)
		return -1;

	chip = (struct sd76030_chip*)rdev_get_drvdata(rdev);

	ret = sd76030_get_boost_ilim(chip);
	bmt_dbg("ret=%d\n",ret);
	if(ret<0)
		ret = 0;

	return ret * 1000;
}
static const struct regulator_ops sd76030_regulator_ops = {
	.enable 		= sd76030_enable_otg,
	.disable 		= sd76030_disable_otg,
	.is_enabled 		= sd76030_is_otg_enabled,

	.set_voltage		= sd76030_set_otg_voltage,
	.get_voltage		= sd76030_get_otg_voltage,

	.set_current_limit 	= sd76030_set_otg_current,
	.get_current_limit 	= sd76030_get_otg_current,
};
static int32_t sd76030_otg_init(void *driver_data)
{
	struct sd76030_chip *chip = driver_data;
	bmt_dbg("\n");
	if(chip)
	{
		sd76030_set_otg_en(chip,0);
		sd76030_set_boost_vlim(chip,5000);
		sd76030_set_boost_ilim(chip,1200);
		return 0;
	}
	return -1;
}
static int32_t sd76030_otg_regulator_init(struct sd76030_chip *chip)
{
	int32_t ret = 0;
	struct regulator_config cfg = {};

	chip->init_data = of_get_regulator_init_data(chip->dev, chip->dev->of_node,&chip->sd76030_otg_rdesc);
	if (!chip->init_data)
	{
		pr_err("get regulator init data failed\n");
		return -1;
	}
	bmt_dbg("regulator name %s\n",chip->init_data->constraints.name);
	if (chip->init_data->constraints.name)
	{
		chip->init_data->constraints.valid_modes_mask |= REGULATOR_CHANGE_STATUS;
		chip->init_data->regulator_init = sd76030_otg_init;
		chip->init_data->driver_data = chip;
		cfg.dev 			 = chip->dev;
		cfg.init_data 	 = chip->init_data;
		cfg.driver_data = chip;
		cfg.of_node 	 = chip->dev->of_node;

		chip->sd76030_otg_rdesc.name 			= chip->init_data->constraints.name;
		chip->sd76030_otg_rdesc.ops			= &sd76030_regulator_ops;
		chip->sd76030_otg_rdesc.owner			= THIS_MODULE;
		chip->sd76030_otg_rdesc.type			= REGULATOR_VOLTAGE;
		//chip->sd76030_otg_rdesc.fixed_uV		= 5000000;
		chip->sd76030_otg_rdesc.n_voltages	= 4;

		chip->sd76030_reg_dev = regulator_register(&chip->sd76030_otg_rdesc, &cfg);
		if (IS_ERR(chip->sd76030_reg_dev))
		{
			ret = PTR_ERR(chip->sd76030_reg_dev);
			pr_err("Can't register regulator: %d\n", ret);
		}
		else
			pr_err("Success register regulator: %d\n", ret);

	}
	return ret;
}

/*iio start */

int sd76030_chg_get_iio_channel(struct sd76030_chip *chip,
			enum sd76030_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
		default:
		pr_err("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(sd76030_chg_get_iio_channel);

static int sd76030_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sd76030_chip *chip = iio_priv(indio_dev);
	int rc = 0;
	*val1 = 0;

	if(!chip){
		pr_err("chip is NULL,Fail !\n");
		return -EINVAL;
	}

	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		*val1 = sd76030_get_charger_status(chip);
		break;
	case PSY_IIO_CHARGE_AFC:
		*val1 = chip->charge_afc;
		break;
	default:
		pr_err("Unsupported sd76030 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	bmt_dbg("read IIO channel %d, rc = %d, val1 = %d\n", chan->channel, rc, val1);
	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int sd76030_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct sd76030_chip *chip = iio_priv(indio_dev);
	int rc = 0;

	if(!chip){
		pr_err("chip is NULL,Fail !\n");
		return -EINVAL;
	}

	bmt_dbg("Write IIO channel %d, val = %d\n", chan->channel, val1);
	switch (chan->channel) {
	case PSY_IIO_CHARGE_TYPE:
		chip->chg_type = val1;

	default:
		pr_err("Unsupported sd76030 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0){
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int sd76030_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sd76030_chip *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i = 0;

	if(!chip){
		pr_err("chip is NULL,Fail !\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(sd76030_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0]){
			return i;
		}
	return -EINVAL;

}

static const struct iio_info sd76030_iio_info = {
	.read_raw	= sd76030_iio_read_raw,
	.write_raw	= sd76030_iio_write_raw,
	.of_xlate	= sd76030_iio_of_xlate,
};

static int sd76030_init_iio_psy(struct sd76030_chip *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(sd76030_iio_psy_channels);
	int rc = 0, i = 0;

	bmt_dbg("start !\n");

	if(!chip){
		pr_err("chip is NULL,Fail !\n");
		return -EINVAL;
	}

	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
						sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan) {
		pr_err("chip->iio_chan is null!\n");
		return -ENOMEM;
	}

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans) {
		pr_err("chip->int_iio_chans is null!\n");
		return -ENOMEM;
	}

	indio_dev->info = &sd76030_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "main_chg";

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sd76030_iio_psy_channels[i].channel_num;
		chan->type = sd76030_iio_psy_channels[i].type;
		chan->datasheet_name =
			sd76030_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sd76030_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sd76030_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register sd76030 IIO device, rc=%d\n", rc);

	bmt_dbg("sd76030 IIO device, rc=%d\n", rc);
	return rc;
}

static int sd76030_ext_init_iio_psy(struct sd76030_chip *chip)
{
	if (!chip){
		pr_err("sd76030_ext_init_iio_psy, chip is NULL!\n");
		return -ENOMEM;
	}

	chip->ds_ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(ds_ext_iio_chan_name),
				sizeof(*chip->ds_ext_iio_chans),
				GFP_KERNEL);
	if (!chip->ds_ext_iio_chans) {
		pr_err("chip->ds_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	chip->fg_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(fg_ext_iio_chan_name), sizeof(*chip->fg_ext_iio_chans), GFP_KERNEL);
	if (!chip->fg_ext_iio_chans) {
		pr_err("chip->fg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}

	chip->nopmi_chg_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(nopmi_chg_ext_iio_chan_name), sizeof(*chip->nopmi_chg_ext_iio_chans), GFP_KERNEL);
	if (!chip->nopmi_chg_ext_iio_chans) {
		pr_err("chip->nopmi_chg_ext_iio_chans is NULL!\n");
		return -ENOMEM;
	}
	return 0;
}
/* iio end */
/*OTG start*/
static void sd76030_set_otg(struct sd76030_chip *chip, int enable)
{
	int ret;

	if (enable)
	{
		sd76030_set_charger_en(chip,0);
		ret = sd76030_set_otg_en(chip,1);
		if (ret < 0)
		{
			pr_err("%s:Failed to enable otg-%d\n", __func__, ret);
			return;
		}
	} else{
		ret = sd76030_set_otg_en(chip,0);
		if (ret < 0){
			pr_err("%s:Failed to disable otg-%d\n", __func__, ret);
		}
		sd76030_set_charger_en(chip,1);
	}
}

static bool is_ds_chan_valid(struct sd76030_chip *chip,
		enum ds_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->ds_ext_iio_chans[chan]))
		return false;
	if (!chip->ds_ext_iio_chans[chan]) {
		chip->ds_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					ds_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->ds_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->ds_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->ds_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				ds_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_bms_chan_valid(struct sd76030_chip *chip,
		enum fg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->fg_ext_iio_chans[chan]))
		return false;
	if (!chip->fg_ext_iio_chans[chan]) {
		chip->fg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					fg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->fg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->fg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->fg_ext_iio_chans[chan] = NULL;
			pr_err("bms_chan_valid Failed to get fg_ext_iio channel %s, rc=%d\n",
				fg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_nopmi_chg_chan_valid(struct sd76030_chip *chip,
		enum nopmi_chg_ext_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->nopmi_chg_ext_iio_chans[chan]))
		return false;
	if (!chip->nopmi_chg_ext_iio_chans[chan]) {
		chip->nopmi_chg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					nopmi_chg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->nopmi_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->nopmi_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->nopmi_chg_ext_iio_chans[chan] = NULL;
			pr_err("nopmi_chg_chan_valid failed to get IIO channel %s, rc=%d\n",
				nopmi_chg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

int sd76030_set_iio_channel(struct sd76030_chip *chip,
			enum sd76030_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;
/*
	if(chg->shutdown_flag)
		return -ENODEV;
*/
	switch (type) {
	case DS28E16:
		if (!is_ds_chan_valid(chip, channel)){
			pr_err("%s: iio_type is DS28E16 \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = chip->ds_ext_iio_chans[channel];
		break;
	case BMS:
		if (!is_bms_chan_valid(chip, channel)){
			pr_err("%s: iio_type is BMS \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = chip->fg_ext_iio_chans[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chg_chan_valid(chip, channel)){
			pr_err("%s: iio_type is NOPMI \n",__func__);
			return -ENODEV;
		}
		iio_chan_list = chip->nopmi_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		pr_err("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_write_channel_raw(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

static int get_source_mode(struct tcp_notify *noti)
{
	if (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	switch (noti->typec_state.rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case TYPEC_CC_VOLT_SNK_3_0:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	case TYPEC_CC_VOLT_SNK_DFT:
		return QTI_POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		break;
	}
	return QTI_POWER_SUPPLY_TYPEC_NONE;
}

static int sd76030_set_cc_orientation(struct sd76030_chip *chip, int cc_orientation)
{
	int ret = 0;

	union power_supply_propval propval;
	if(chip->usb_psy == NULL) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (chip->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = cc_orientation;
	ret = sd76030_set_iio_channel(chip, NOPMI, NOPMI_CHG_TYPEC_CC_ORIENTATION, propval.intval); 
	if (ret < 0)
		pr_err("%s : set prop CC_ORIENTATION fail ret:%d\n", __func__, ret);
	return ret;
}

static int sd76030_set_typec_mode(struct sd76030_chip *chip, enum power_supply_typec_mode typec_mode)
{
	int ret = 0;
	union power_supply_propval propval;
	if(chip->usb_psy == NULL) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (chip->usb_psy == NULL) {
			pr_err("%s : fail to get psy usb\n", __func__);
			return -ENODEV;
		}
	}
	propval.intval = typec_mode;
	ret = sd76030_set_iio_channel(chip, NOPMI, NOPMI_CHG_TYPEC_MODE, propval.intval);
	if (ret < 0)
		pr_err("%s : set prop TYPEC_MODE fail ret:%d\n", __func__, ret);
	return ret;
}

static int sd76030_tcpc_notifier_call(struct notifier_block *pnb,
                    unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct sd76030_chip *chip;
	int vbus_stat = 0 ;
	int cc_orientation = 0;
//	u8 reg_val = 0;
	enum power_supply_typec_mode typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;

	chip = container_of(pnb, struct sd76030_chip, tcp_nb);
//	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
//	vbus_stat >>= REG08_VBUS_STAT_SHIFT;
	vbus_stat = atomic_read(&chip->vbus_gd);

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_err("%s:USB Plug in, cc_or_pol = %d, state = %d\n",__func__,
					noti->typec_state.polarity, noti->typec_state.new_state);
			//if( upm6918_get_vbus_type(upm) == UPM6918_VBUS_NONE || upm6918_get_vbus_type(upm) ==  UPM6918_VBUS_UNKNOWN
			//		|| upm->chg_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if(vbus_stat == 0 || chip->chg_type == POWER_SUPPLY_USB_TYPE_UNKNOWN){
				pr_debug("%s vbus type is none, do force dpdm now.\n",__func__);
				sd76030_hw_init(chip);
				sd76030_request_dpdm(chip,true);
			}
			typec_mode = get_source_mode(noti);
			cc_orientation = noti->typec_state.polarity;
			sd76030_set_cc_orientation(chip, cc_orientation);
			pr_debug("%s: cc_or_val = %d\n",__func__, cc_orientation);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			typec_mode = QTI_POWER_SUPPLY_TYPEC_NONE;
			pr_info("USB Plug out\n");
			sd76030_request_dpdm(chip,false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("Source_to_Sink \n");
			typec_mode = QTI_POWER_SUPPLY_TYPEC_SINK;
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			typec_mode = get_source_mode(noti);
			pr_info("Sink_to_Source \n");
		}
		if(typec_mode >= QTI_POWER_SUPPLY_TYPEC_NONE && typec_mode <= QTI_POWER_SUPPLY_TYPEC_NON_COMPLIANT)
			sd76030_set_typec_mode(chip, typec_mode);
		break;

	case TCP_NOTIFY_SOURCE_VBUS:
		pr_err("vbus_on:%d TCP_NOTIFY_SOURCE_VBUS\n", vbus_on);
		if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_0V) && (vbus_on)) {
			/* disable OTG power output */
			pr_err("otg plug out and power out !\n");
			vbus_on = false;
			sd76030_set_otg(chip, false);
			sd76030_request_dpdm(chip,false);
		} else if ((noti->vbus_state.mv == TCPC_VBUS_SOURCE_5V) && (!vbus_on)) {
			/* enable OTG power output */
			pr_err("otg plug in and power on !\n");
			vbus_on = true;
			sd76030_request_dpdm(chip,false);
			sd76030_set_otg(chip, true);
			//upm6918set_otg_current(upm, upm->cfg.charge_current_1500);
		}
		break;
	}
	return NOTIFY_OK;
}

static void sd76030_register_tcpc_notify_dwork_handler(struct work_struct *work)
{
	struct sd76030_chip *chip = container_of(work, struct sd76030_chip,
								tcpc_dwork.work);
	int ret;

    if (!chip->tcpc) {
        chip->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!chip->tcpc) {
		pr_err("get tcpc dev fail\n");
		schedule_delayed_work(&chip->tcpc_dwork, msecs_to_jiffies(2*1000));
		return;
        }
        chip->tcp_nb.notifier_call = sd76030_tcpc_notifier_call;
        ret = register_tcp_dev_notifier(chip->tcpc, &chip->tcp_nb,
                        TCP_NOTIFY_TYPE_ALL);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }
}
/*OTG end*/
static const struct of_device_id sd76030_of_match[] = {
	{.compatible = "bigmtech,sd76030"},
	{},
};
MODULE_DEVICE_TABLE(of, sd76030_of_match);

static int32_t sd76030_driver_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
//iio
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct iio_dev *indio_dev = NULL;
	bmt_dbg("start\n");
#if 0
	chip = devm_kzalloc(&client->dev, sizeof(struct sd76030_chip),GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;
	chip->client->addr = 0x6b;
	//init mutex asap,before iic operate
	mutex_init(&chip->i2c_rw_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->irq_lock);
	mutex_init(&chip->dpdm_lock);
	i2c_set_clientdata(client, chip);
#endif
//iio start
	if (!client->dev.of_node)
	{
		pr_err(" client->dev.of_node is null!\n");
		return -ENODEV;
	}
	if (client->dev.of_node)
	{
		indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct sd76030_chip));
		if (!indio_dev)
		{
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	}

	chip = iio_priv(indio_dev);
	chip->indio_dev = indio_dev;
	chip->dev = &client->dev;
	chip->client = client;
	chip->client->addr = 0x6b;
	mutex_init(&chip->i2c_rw_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->irq_lock);
	mutex_init(&chip->dpdm_lock);
	i2c_set_clientdata(client, chip);
//iio end
	ret = sd76030_detect_ic(chip);
	if(ret < 0)
	{
		pr_err("do not detect ic, exit\n");
		return -ENODEV;
	}

	match = of_match_node(sd76030_of_match, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}
	sd76030_get_component_id(chip);
	ret = sd76030_parse_dt(chip, &client->dev);
	if (ret < 0)
		pr_err("sd76030_parse_dt failed ret %d\n",ret);
	sd76030_hw_init(chip);
	INIT_DELAYED_WORK(&chip->tcpc_dwork, sd76030_register_tcpc_notify_dwork_handler);
	INIT_DELAYED_WORK(&chip->hvdcp_dwork, sd76030_hvdcp_dwork);
	INIT_DELAYED_WORK(&chip->sd76030_work, sd76030_work_func);

	if(SD76030_COMPINENT_ID   == chip->component_id)
	{
		mutex_init(&chip->bc12_lock);
		atomic_set(&chip->vbus_gd, 0);
		chip->attach = false;
		chip->port = SD76030_PORTSTAT_NO_INPUT;
		INIT_DELAYED_WORK(&chip->psy_dwork, sd76030_inform_psy_dwork_handler);

#ifdef TRIGGER_CHARGE_TYPE_DETECTION
		init_waitqueue_head(&chip->bc12_en_req);
		atomic_set(&chip->bc12_en_req_cnt, 0);
		chip->bc12_en_kthread = kthread_run(sd76030_bc12_en_kthread,chip,"sd76030_bc12");
		if (IS_ERR_OR_NULL(chip->bc12_en_kthread))
		{
			ret = PTR_ERR(chip->bc12_en_kthread);
			bmt_dbg("kthread run fail(%d)\n", ret);
		}
#endif
	}
	/* longcheer afc start*/
	chip->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_callback, chip);
	if (IS_ERR(chip->fcc_votable)) {
		pr_err("fcc_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(chip->fcc_votable);
		chip->fcc_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fcc_votable create successful !\n");

	chip->chg_dis_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					chg_dis_vote_callback, chip);
	if (IS_ERR(chip->chg_dis_votable)) {
		pr_err("chg_dis_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(chip->chg_dis_votable);
		chip->chg_dis_votable = NULL;
		goto destroy_votable;
	}
	pr_info("chg_dis_votable create successful !\n");

	chip->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback, chip);
	if (IS_ERR(chip->fv_votable)) {
		pr_err("fv_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(chip->fv_votable);
		chip->fv_votable = NULL;
		goto destroy_votable;
	}
	pr_info("fv_votable create successful !\n");

	chip->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					chip);
	if (IS_ERR(chip->usb_icl_votable)) {
		pr_err("usb_icl_votable is ERROR,goto destroy!\n");
		ret = PTR_ERR(chip->usb_icl_votable);
		chip->usb_icl_votable = NULL;
		goto destroy_votable;
	}
	pr_info("usb_icl_votable create successful !\n");

	vote(chip->usb_icl_votable, PROFILE_CHG_VOTER, true, CHG_ICL_CURR_MAX);
	vote(chip->chg_dis_votable, BMS_FC_VOTER, false, 0);
	ret = sd76030_otg_regulator_init(chip);
	if(ret < 0)
		pr_err("register otg regulator fail(%d)\n", ret);

	ret = sd76030_power_init(chip);
	if (ret < 0)
		pr_err("register power supply fail(%d)\n", ret);

	ret = sd76030_extcon_init(chip);
	if (ret < 0)
		pr_err("register extcon fail(%d)\n", ret);
	ret = sd76030_register_irq(chip);
	if (ret < 0)
		pr_err("register irq fail(%d)\n", ret);
	//process irq once when probe
	ret = sd76030_process_irq(chip);
	if (ret < 0)
		pr_err("process irq fail(%d)\n", ret);
	//iio start
	ret = sd76030_init_iio_psy(chip);
 	if (ret < 0)
		pr_err("Failed to sd76030_init_iio_psy IIO PSY, ret=%d\n", ret);
	ret = sd76030_ext_init_iio_psy(chip);
	if (ret < 0)
		pr_err("Failed to initialize sd76030_ext_init_iio_psy IIO PSY, rc=%d\n", ret);
	//iio end
	device_init_wakeup(chip->dev, true);
	schedule_delayed_work(&chip->sd76030_work, (3*HZ));
	schedule_delayed_work(&chip->tcpc_dwork, msecs_to_jiffies(2*1000));
	sd76030_dump_msg(chip);
	ret = sysfs_create_group(&(client->dev.kobj), &sd76030_attribute_group);
	if(ret)
		pr_err("failed to creat attributes\n");

	bmt_dbg("end\n");

	return 0;
destroy_votable:
	pr_err("destory votable !\n");
	destroy_votable(chip->fcc_votable);
	destroy_votable(chip->chg_dis_votable);
	destroy_votable(chip->fv_votable);
	destroy_votable(chip->usb_icl_votable);
	return ret;
}

static int32_t sd76030_suspend(struct device *dev)
{
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	bmt_dbg("\n");

	if (device_may_wakeup(dev) && chip->irq >= 0)
		enable_irq_wake(chip->irq);

	//if(chip->irq >= 0)
	//	disable_irq(chip->irq);

	return 0;
}
static int32_t sd76030_resume(struct device *dev)
{
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	bmt_dbg("\n");

	//if(chip->irq >= 0)
	//	enable_irq(chip->irq);

	if (device_may_wakeup(dev) && chip->irq >= 0)
		disable_irq_wake(chip->irq);

	return 0;
}
static int32_t sd76030_driver_remove(struct i2c_client *client)
{
	struct sd76030_chip *chip = i2c_get_clientdata(client);

	if(chip->irq >= 0)
		disable_irq(chip->irq);
	if(-1 != chip->irq_gpio)
		devm_gpio_free(chip->dev,chip->irq_gpio);

	if(SD76030_COMPINENT_ID == chip->component_id)
	{
		cancel_delayed_work_sync(&chip->psy_dwork);
		if (chip->psy)
			power_supply_put(chip->psy);

#ifdef TRIGGER_CHARGE_TYPE_DETECTION
		kthread_stop(chip->bc12_en_kthread);
#endif
		mutex_destroy(&chip->bc12_lock);
	}
	sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);

	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->i2c_rw_lock);
	sysfs_remove_group(&(chip->client->dev.kobj), &sd76030_attribute_group);

	return 0;
}
static void sd76030_driver_shutdown(struct i2c_client *client)
{
	struct sd76030_chip *chip = i2c_get_clientdata(client);
	// sd76030_set_charger_en(chip,0);
	sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
	sysfs_remove_group(&(chip->client->dev.kobj), &sd76030_attribute_group);
}
static const struct dev_pm_ops sd76030_pm_ops = {
	.resume			= sd76030_resume,
	.suspend			= sd76030_suspend,
};

static const struct i2c_device_id sd76030_i2c_id[] = {
	{"sd76030",   0},
};
static struct i2c_driver sd76030_driver = {
	.driver = {
		.name 			= "sd76030",
		.owner 		= THIS_MODULE,
		.pm			= &sd76030_pm_ops,
		.of_match_table 	= sd76030_of_match,
	},
	.id_table 	= sd76030_i2c_id,
	.probe 	= sd76030_driver_probe,
	.remove	= sd76030_driver_remove,
	.shutdown	= sd76030_driver_shutdown,
};

static int32_t __init sd76030_init(void)
{
	if (0 != i2c_add_driver(&sd76030_driver))
		bmt_dbg("failed to register sd76030 i2c driver.\n");
	else
		bmt_dbg("Success to register sd76030 i2c driver.\n");

	return 0;
}

static void __exit sd76030_exit(void)
{
	i2c_del_driver(&sd76030_driver);
}
module_init(sd76030_init);
module_exit(sd76030_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("I2C sd76030 Driver");
MODULE_AUTHOR("BMT");

