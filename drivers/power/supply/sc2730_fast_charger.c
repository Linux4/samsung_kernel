// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/usb/phy.h>
#include <linux/regmap.h>
#include <linux/notifier.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/power/charger-manager.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>
#include "afc.h"

#define printc(fmt, args...) do { printk("[wt_usb][fschg][%s] "fmt, __FUNCTION__, ##args); } while (0)

#define FCHG1_TIME1				0x0
#define FCHG1_TIME2				0x4
#define FCHG1_DELAY				0x8
#define FCHG2_DET_HIGH				0xc
#define FCHG2_DET_LOW				0x10
#define FCHG2_DET_LOW_CV			0x14
#define FCHG2_DET_HIGH_CV			0x18
#define FCHG2_DET_LOW_CC			0x1c
#define FCHG2_ADJ_TIME1				0x20
#define FCHG2_ADJ_TIME2				0x24
#define FCHG2_ADJ_TIME3				0x28
#define FCHG2_ADJ_TIME4				0x2c
#define FCHG_CTRL				0x30
#define FCHG_ADJ_CTRL				0x34
#define FCHG_INT_EN				0x38
#define FCHG_INT_CLR				0x3c
#define FCHG_INT_STS				0x40
#define FCHG_INT_STS0				0x44
#define FCHG_ERR_STS				0x48

#define SC2721_MODULE_EN0		0xC08
#define SC2721_CLK_EN0			0xC10
#define SC2721_IB_CTRL			0xEA4
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define SC2730_IB_CTRL			0x1b84
#define UMP9620_MODULE_EN0		0x2008
#define UMP9620_CLK_EN0			0x2010
#define UMP9620_IB_CTRL			0x2384

#define ANA_REG_IB_TRIM_MASK			GENMASK(6, 0)
#define ANA_REG_IB_TRIM_SHIFT			2
#define ANA_REG_IB_TRIM_EM_SEL_BIT		BIT(1)
#define ANA_REG_IB_TRUM_OFFSET			0x1e

#define FAST_CHARGE_MODULE_EN0_BIT		BIT(11)
#define FAST_CHARGE_RTC_CLK_EN0_BIT		BIT(4)

#define FCHG_ENABLE_BIT				BIT(0)
#define FCHG_INT_EN_BIT				BIT(1)
#define FCHG_INT_CLR_MASK			BIT(1)
#define FCHG_TIME1_MASK				GENMASK(10, 0)
#define FCHG_TIME2_MASK				GENMASK(11, 0)
#define FCHG_DET_VOL_MASK			GENMASK(1, 0)
#define FCHG_DET_VOL_SHIFT			3
#define FCHG_DET_VOL_EXIT_SFCP			3
#define FCHG_CALI_MASK				GENMASK(15, 9)
#define FCHG_CALI_SHIFT				9

#define FCHG_ERR0_BIT				BIT(1)
#define FCHG_ERR1_BIT				BIT(2)
#define FCHG_ERR2_BIT				BIT(3)
#define FCHG_OUT_OK_BIT				BIT(0)

#define FCHG_INT_STS_DETDONE			BIT(5)

/* FCHG1_TIME1_VALUE is used for detect the time of V > VT1 */
#define FCHG1_TIME1_VALUE			0x514
/* FCHG1_TIME2_VALUE is used for detect the time of V > VT2 */
#define FCHG1_TIME2_VALUE			0x9c4

#define FCHG_VOLTAGE_5V				5000000
#define FCHG_VOLTAGE_9V				9000000
#define FCHG_VOLTAGE_12V			12000000
#define FCHG_VOLTAGE_20V			20000000

#define SC2730_FCHG_TIMEOUT			msecs_to_jiffies(5000)
#define SC2730_FAST_CHARGER_DETECT_MS		msecs_to_jiffies(1000)

#define SC2730_PD_DEFAULT_POWER_UW		10000000

#define SC2730_ENABLE_PPS			2
#define SC2730_DISABLE_PPS			1

struct sc27xx_fast_chg_data {
	u32 module_en;
	u32 clk_en;
	u32 ib_ctrl;
};

static const struct sc27xx_fast_chg_data sc2721_info = {
	.module_en = SC2721_MODULE_EN0,
	.clk_en = SC2721_CLK_EN0,
	.ib_ctrl = SC2721_IB_CTRL,
};

static const struct sc27xx_fast_chg_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.ib_ctrl = SC2730_IB_CTRL,
};

static const struct sc27xx_fast_chg_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.ib_ctrl = UMP9620_IB_CTRL,
};

struct sc2730_fchg_info {
	struct device *dev;
	struct regmap *regmap;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct notifier_block pd_notify;
	struct power_supply *psy_usb;
	struct power_supply *psy_tcpm;
	struct delayed_work work;
	struct work_struct pd_change_work;
	struct mutex lock;
	struct completion completion;
	struct adapter_power_cap pd_source_cap;
	u32 state;
	u32 base;
	int input_vol;
	int pd_fixed_max_uw;
	u32 limit;
	bool detected;
	bool pd_enable;
	bool sfcp_enable;
	bool pps_enable;
	bool pps_active;
	bool support_pd_pps;
	bool support_sfcp;
	const struct sc27xx_fast_chg_data *pdata;
//+ add by guoyanjun
	bool afc_enable;
	struct afc_dev afc;
	struct delayed_work afc_work;
//- add by guoyanjun

};
//+ add afc communication function
void cycle(struct afc_dev *afc, int ui)
{
	gpio_set_value(afc->afc_data_gpio, 1);
	udelay(ui);
	gpio_set_value(afc->afc_data_gpio, 0);
	udelay(ui);
}
int afc_send_parity_bit(struct afc_dev *afc, int data)
{
	int cnt = 0, odd = 0;

	for (; data > 0; data = data >> 1) {
		if (data & 0x1)	
			cnt++;
	}

	odd = cnt % 2;
	if (!odd)
		gpio_set_value(afc->afc_data_gpio, 1);
	else
		gpio_set_value(afc->afc_data_gpio, 0);

	udelay(UI);

	return odd;
}

static void afc_send_Mping(struct afc_dev *afc)
{
	gpio_direction_output(afc->afc_data_gpio,0);
	//spin_lock_irq(&afc->afc_spin_lock);
	gpio_set_value(afc->afc_data_gpio,1);
	udelay(16*UI);
	gpio_set_value(afc->afc_data_gpio,0);
	//spin_unlock_irq(&afc->afc_spin_lock);
}
int afc_recv_Sping(struct afc_dev *afc)
{
	int i = 0, sp_cnt = 0;
    gpio_direction_input(afc->afc_data_gpio);

	while (1)
	{
		udelay(UI);
		if (gpio_get_value(afc->afc_data_gpio)) {
			break;
		}
        
		if (WAIT_SPING_COUNT < i++) {
			printc("%s: wait time out ! \n", __func__);
			goto Sping_err;
		}
	}
    
	do {
		if (SPING_MAX_UI < sp_cnt++) {
			goto Sping_err;        
        }
		udelay(UI);
	} while (gpio_get_value(afc->afc_data_gpio));
	//printc("sp_cnt = %d\n",sp_cnt);

	if (sp_cnt < SPING_MIN_UI) {
		printc("sp_cnt < %d,sping err!",SPING_MIN_UI);
		goto Sping_err;
	}
	//printc("done,sp_cnt %d \n", sp_cnt);
	return 0;

Sping_err: 
	printc("%s: Err sp_cnt %d \n", __func__, sp_cnt);
	return -1;
}
void afc_send_data(struct afc_dev *afc, int data)
{
	int i = 0;
	gpio_direction_output(afc->afc_data_gpio,0);
	//spin_lock_irq(&afc->afc_spin_lock);
	udelay(160);

	if (data & 0x80)
		cycle(afc, UI/4);
	else {
		cycle(afc, UI/4);
		gpio_set_value(afc->afc_data_gpio,1);
		udelay(UI/4);
	}

	for (i = 0x80; i > 0; i = i >> 1)
	{
		gpio_set_value(afc->afc_data_gpio, data & i);
		udelay(UI);
	}

	if (afc_send_parity_bit(afc, data)) {
		gpio_set_value(afc->afc_data_gpio, 0);
		udelay(UI/4);
		cycle(afc, UI/4);
	} else {
		udelay(UI/4);
		cycle(afc, UI/4);
	}
	//spin_unlock_irq(&afc->afc_spin_lock);
	
}
int afc_recv_data(struct afc_dev *afc)
{
	int ret = 0;
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_2;
		return -1;
	}
	//spin_lock_irq(&afc->afc_spin_lock);
	mdelay(2);//+Bug 522575,zhaosidong.wt,ADD,20191216,must delay 2 ms
	//spin_unlock_irq(&afc->afc_spin_lock);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_3;
		return -1;
	}
	return 0;
}

static int afc_communication(struct afc_dev *afc)
{
	int ret = 0;

	printc("Start\n");
	gpio_set_value(afc->afc_switch_gpio, 1);
	msleep(5);
	afc_send_Mping(afc);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		printc("Mping recv error!\n");
		afc->afc_error = SPING_ERR_1;
		goto afc_fail;
	}
	if (afc->vol == SET_9V)
		afc_send_data(afc, 0x46);
	else
		afc_send_data(afc, 0x08);
	//udelay(160);
	afc_send_Mping(afc);
	ret = afc_recv_data(afc);
	if (ret < 0)
		goto afc_fail;
	//spin_lock_irq(&afc->afc_spin_lock);
	udelay(200);
	//spin_unlock_irq(&afc->afc_spin_lock);
	afc_send_Mping(afc);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_4;
		goto afc_fail;
	}
	gpio_set_value(afc->afc_switch_gpio,0);
	printc("ack ok \n");
	afc->afc_error = 0;

	return 0;

afc_fail:
	gpio_set_value(afc->afc_switch_gpio,0);
	printc("AFC communication has been failed(%d)\n", afc->afc_error);

	return -1;
}


/*
* __afc_set_ta_vchr
*
* Set TA voltage, chr_volt mV
*/
static int __afc_set_ta_vchr(struct sc2730_fchg_info *pinfo, u32 chr_volt)
{
	int i = 0, ret = 0; 
	struct afc_dev *afc = &pinfo->afc;
        
 	afc->vol = chr_volt;
	for (i = 0; i < AFC_COMM_CNT ; i++) {      
		ret = afc_communication(afc);
		if (ret == 0) {
        		printc("ok, i %d \n",i);
        		break;
        	}
		msleep(100);
	}
	return ret;
}
#define FGU_NAME			"sc27xx-fgu"
static int fast_charger_get_charge_voltage(struct sc2730_fchg_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(FGU_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get BQ25890_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}
bool is_afc_charger = false;
//+ yuecong.wt.20220401 add hv_disable sys node
bool hv_disable = false;
//- yuecong.wt.20220401 add hv_disable sys node
static int afc_set_voltage(struct sc2730_fchg_info *pinfo, int chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;

	printc("volt = %d,Start\n",chr_volt);
	fast_charger_get_charge_voltage(pinfo, &vchr_before);
	do {
		ret = __afc_set_ta_vchr(pinfo, chr_volt);
		mdelay(200);
		fast_charger_get_charge_voltage(pinfo, &vchr_after);
		vchr_delta = abs(vchr_after - chr_volt);
		printc("vchr_before=%d,vchr_after=%d,vchr_delta=%d\n",vchr_before,vchr_after,vchr_delta);
		/*
		 * It is successful if VBUS difference to target is
		 * less than 1000mV.
		 */	
		if (vchr_delta < 1000000) {
				printc("afc set %d  seccessfully!!!\n",chr_volt);			
				return 0;
		}
		if (ret == 0 || sw_retry_cnt >= sw_retry_cnt_max)
			retry_cnt++;
		else
			sw_retry_cnt++;

		printc("retry cnt(%d-%d)\n",retry_cnt,sw_retry_cnt);
	} while ( pinfo->usb_phy->chg_type != UNKNOWN_TYPE &&
		 retry_cnt < retry_cnt_max);

	
	return ret;
}

static int afc_test_set_voltage(struct sc2730_fchg_info *pinfo, int chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;
	static int try_work_cnt_max = 3;
	printc("volt = %d,Start\n",chr_volt);
	fast_charger_get_charge_voltage(pinfo, &vchr_before);
	do {
		ret = __afc_set_ta_vchr(pinfo, chr_volt);
		mdelay(200);
		fast_charger_get_charge_voltage(pinfo, &vchr_after);
		vchr_delta = abs(vchr_after - chr_volt);
		printc("vchr_before=%d,vchr_after=%d,vchr_delta=%d\n",vchr_before,vchr_after,vchr_delta);
		/*
		 * It is successful if VBUS difference to target is
		 * less than 1000mV.
		 */	
		if (vchr_delta < 2000000) {
			if(!pinfo->afc.afc_error){
				printc("afc match seccessfully!!!\n");
				is_afc_charger = true;
				pinfo->afc_enable = true;
				cm_notify_event(pinfo->psy_usb, CM_EVENT_AFC_CHARGE, NULL);		
				try_work_cnt_max = 3;
				return 0;
			}else{
				printc("afc match failed!!\n");
				pinfo->afc_enable = false;
			}
		}
		if (ret == 0 || sw_retry_cnt >= sw_retry_cnt_max)
			retry_cnt++;
		else
			sw_retry_cnt++;

		printc("retry cnt(%d-%d)\n",retry_cnt,sw_retry_cnt);
	} while ( pinfo->usb_phy->chg_type != UNKNOWN_TYPE &&
		 retry_cnt < retry_cnt_max);

	if(try_work_cnt_max > 0){
		printc("afc match failed!!!trycnt=%d\n",try_work_cnt_max);
		try_work_cnt_max--;
		schedule_delayed_work(&pinfo->afc_work,1*HZ);
		return ret;
	}
	try_work_cnt_max = 3;
	return ret;
}

static void afc_check_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sc2730_fchg_info *info =
		container_of(dwork, struct sc2730_fchg_info, afc_work);
	printc("afc set valtage start\n");
	//mutex_lock(&info->lock);
	afc_test_set_voltage(info,SET_5V);
	//mutex_unlock(&info->lock);
}
static int afc_fchg_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{

	int ret;

	printc("## afc adjust vol = %d\n",input_vol);

	if (input_vol < FCHG_VOLTAGE_9V) {
		ret = afc_set_voltage(info,SET_5V);
		if(ret){
			printc("adjust 5v failed!\n");
			return ret;
		}
	} else if (input_vol < FCHG_VOLTAGE_12V) {
		ret = afc_set_voltage(info,SET_9V);
		if(ret){
			printc("adjust 9v failed!\n");
			return ret;
		}
	}

	return 0;
}

//- add afc communication function

static int sc2730_fchg_internal_cur_calibration(struct sc2730_fchg_info *info)
{
	struct nvmem_cell *cell;
	int calib_data, calib_current, ret;
	void *buf;
	size_t len;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	cell = nvmem_cell_get(info->dev, "fchg_cur_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	kfree(buf);

	/*
	 * In the handshake protocol behavior of sfcp, the current source
	 * of the fast charge internal module is small, we improve it
	 * by set the register ANA_REG_IB_CTRL. Now we add 30 level compensation.
	 */
	calib_current = (calib_data & FCHG_CALI_MASK) >> FCHG_CALI_SHIFT;
	calib_current += ANA_REG_IB_TRUM_OFFSET;

	ret = regmap_update_bits(info->regmap,
				 pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_MASK << ANA_REG_IB_TRIM_SHIFT,
				 (calib_current & ANA_REG_IB_TRIM_MASK) << ANA_REG_IB_TRIM_SHIFT);
	if (ret) {
		dev_err(info->dev, "failed to calibrate fast charger current.\n");
		return ret;
	}

	/*
	 * Fast charge dm current source calibration mode, enable soft calibration mode.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 0);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return 0;
}

static irqreturn_t sc2730_fchg_interrupt(int irq, void *dev_id)
{
	struct sc2730_fchg_info *info = dev_id;
	u32 int_sts, int_sts0;
	int ret;

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS, &int_sts);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_read(info->regmap, info->base + FCHG_INT_STS0, &int_sts0);
	if (ret)
		return IRQ_RETVAL(ret);

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, 0);
	if (ret) {
		dev_err(info->dev, "failed to disable fast charger irq.\n");
		return IRQ_RETVAL(ret);
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_CLR,
				 FCHG_INT_CLR_MASK, FCHG_INT_CLR_MASK);
	if (ret) {
		dev_err(info->dev, "failed to clear fast charger interrupts\n");
		return IRQ_RETVAL(ret);
	}

	if ((int_sts & FCHG_INT_STS_DETDONE) && !(int_sts0 & FCHG_OUT_OK_BIT))
		dev_warn(info->dev,
			 "met some errors, now status = 0x%x, status0 = 0x%x\n",
			 int_sts, int_sts0);

	if (info->state == POWER_SUPPLY_USB_TYPE_PD)
		dev_info(info->dev, "Already PD, don't update SFCP\n");
	else if ((int_sts & FCHG_INT_STS_DETDONE) && (int_sts0 & FCHG_OUT_OK_BIT))
		info->state = POWER_SUPPLY_USB_TYPE_SFCP_1P0;
	else
		info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	complete(&info->completion);

	return IRQ_HANDLED;
}

static void sc2730_fchg_detect_status(struct sc2730_fchg_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);

	info->limit = min;
	/*
	 * There is a confilt between charger detection and fast charger
	 * detection, and BC1.2 detection time consumption is <300ms,
	 * so we delay fast charger detection to avoid this issue.
	 */
	schedule_delayed_work(&info->work, SC2730_FAST_CHARGER_DETECT_MS);
}

static int sc2730_fchg_usb_change(struct notifier_block *nb,
				     unsigned long limit, void *data)
{
	struct sc2730_fchg_info *info =
		container_of(nb, struct sc2730_fchg_info, usb_notify);

	info->limit = limit;
	if (!info->limit) {
		cancel_delayed_work(&info->work);
		schedule_delayed_work(&info->work, 0);
	} else {
		/*
		 * There is a confilt between charger detection and fast charger
		 * detection, and BC1.2 detection time consumption is <300ms,
		 * so we delay fast charger detection to avoid this issue.
		 */
		schedule_delayed_work(&info->work, SC2730_FAST_CHARGER_DETECT_MS);
	}
	return NOTIFY_OK;
}

static u32 sc2730_fchg_get_detect_status(struct sc2730_fchg_info *info)
{
	unsigned long timeout;
	int value, ret;
	const struct sc27xx_fast_chg_data *pdata = info->pdata;

	/*
	 * In cold boot phase, system will detect fast charger status,
	 * if charger is not plugged in, it will cost another 2s
	 * to detect fast charger status, so we detect fast charger
	 * status only when DCP charger is plugged in
	 */
	if (info->usb_phy->chg_type != DCP_TYPE)
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;

	reinit_completion(&info->completion);

	if (info->input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (info->input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (info->input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	/*
	 * Due to the the current source of the fast charge internal module is small
	 * we need to dynamically calibrate it through the software during the process
	 * of identifying fast charge. After fast charge recognition is completed, we
	 * disable soft calibration compensate function, in order to prevent the dm current
	 * source from deviating in accuracy when used in other modules.
	 */
	ret = sc2730_fchg_internal_cur_calibration(info);
	if (ret) {
		dev_err(info->dev, "failed to set fast charger calibration.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT,
				 FAST_CHARGE_MODULE_EN0_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT,
				 FAST_CHARGE_RTC_CLK_EN0_BIT);
	if (ret) {
		dev_err(info->dev,
			"failed to enable fast charger clock.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME1,
				 FCHG_TIME1_MASK, FCHG1_TIME1_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time1\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG1_TIME2,
				 FCHG_TIME2_MASK, FCHG1_TIME2_VALUE);
	if (ret) {
		dev_err(info->dev, "failed to set fast charge time2\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
			FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
			(value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, FCHG_ENABLE_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, info->base + FCHG_INT_EN,
				 FCHG_INT_EN_BIT, FCHG_INT_EN_BIT);
	if (ret) {
		dev_err(info->dev, "failed to enable fast charger irq.\n");
		return ret;
	}

	timeout = wait_for_completion_timeout(&info->completion,
					      SC2730_FCHG_TIMEOUT);
	if (!timeout) {
		dev_err(info->dev, "timeout to get fast charger status\n");
		return POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	/*
	 * Fast charge dm current source calibration mode, select efuse calibration
	 * as default.
	 */
	ret = regmap_update_bits(info->regmap, pdata->ib_ctrl,
				 ANA_REG_IB_TRIM_EM_SEL_BIT,
				 ANA_REG_IB_TRIM_EM_SEL_BIT);
	if (ret) {
		dev_err(info->dev, "failed to select ib trim mode.\n");
		return ret;
	}

	return info->state;
}

static void sc2730_fchg_disable(struct sc2730_fchg_info *info)
{
	const struct sc27xx_fast_chg_data *pdata = info->pdata;
	int ret;

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_ENABLE_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger.\n");

	/*
	 * Adding delay is to make sure writing the the control register
	 * successfully firstly, then disable the module and clock.
	 */
	msleep(100);

	ret = regmap_update_bits(info->regmap, pdata->module_en,
				 FAST_CHARGE_MODULE_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable fast charger module.\n");

	ret = regmap_update_bits(info->regmap, pdata->clk_en,
				 FAST_CHARGE_RTC_CLK_EN0_BIT, 0);
	if (ret)
		dev_err(info->dev, "failed to disable charger clock.\n");
}

static int sc2730_fchg_sfcp_adjust_voltage(struct sc2730_fchg_info *info,
					   u32 input_vol)
{
	int ret, value;

	if (input_vol < FCHG_VOLTAGE_9V)
		value = 0;
	else if (input_vol < FCHG_VOLTAGE_12V)
		value = 1;
	else if (input_vol < FCHG_VOLTAGE_20V)
		value = 2;
	else
		value = 3;

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_TYPEC_TCPM
static int sc2730_get_pd_fixed_voltage_max(struct sc2730_fchg_info *info, u32 *max_vol)
{
	struct tcpm_port *port;
	int i, adptor_max_vbus = 0;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port\n");
		return -EINVAL;
	}

	tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "failed to obtain the PD power supply capacity\n");
		return -EINVAL;
	}

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if (info->pd_source_cap.type[i] == PDO_TYPE_FIXED &&
		    adptor_max_vbus < info->pd_source_cap.max_mv[i])
			adptor_max_vbus = info->pd_source_cap.max_mv[i];
	}

	*max_vol = adptor_max_vbus * 1000;

	return 0;
}


static int sc2730_get_pps_voltage_max(struct sc2730_fchg_info *info, u32 *max_vol)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_VOLTAGE_MAX,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to set online property\n");
		return ret;
	}

	*max_vol = val.intval;

	return ret;
}

static int sc2730_get_pps_current_max(struct sc2730_fchg_info *info, u32
				      *max_cur)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_CURRENT_MAX,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to set online property\n");
		return ret;
	}

	*max_cur = val.intval;

	return ret;
}

static int sc2730_fchg_pd_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	struct tcpm_port *port;
	int ret, i, index = -1;
	u32 pdo[PDO_MAX_OBJECTS];
	unsigned int snk_uw;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port\n");
		return -EINVAL;
	}

	tcpm_get_source_capabilities(port, &info->pd_source_cap);
	if (!info->pd_source_cap.nr_source_caps) {
		pdo[0] = PDO_FIXED(5000, 2000, 0);
		snk_uw = SC2730_PD_DEFAULT_POWER_UW;
		index = 0;
		goto done;
	}

	for (i = 0; i < info->pd_source_cap.nr_source_caps; i++) {
		if ((info->pd_source_cap.max_mv[i] <= input_vol / 1000) &&
		    (info->pd_source_cap.type[i] == PDO_TYPE_FIXED))
			index = i;
	}

	/*
	 * Ensure that index is within a valid range to prevent arrays
	 * from crossing bounds.
	 */
	if (index < 0 || index >= info->pd_source_cap.nr_source_caps) {
		dev_err(info->dev, "Index is invalid!!!\n");
		return -EINVAL;
	}

	snk_uw = info->pd_source_cap.max_mv[index] * info->pd_source_cap.ma[index];
	if (snk_uw > info->pd_fixed_max_uw)
		snk_uw = info->pd_fixed_max_uw;

	for (i = 0; i < index + 1; i++) {
		pdo[i] = PDO_FIXED(info->pd_source_cap.max_mv[i], info->pd_source_cap.ma[i], 0);
		if (info->pd_source_cap.max_mv[i] * info->pd_source_cap.ma[i] > snk_uw)
			pdo[i] = PDO_FIXED(info->pd_source_cap.max_mv[i],
					   snk_uw / info->pd_source_cap.max_mv[i],
					   0);
	}

done:
	ret = tcpm_update_sink_capabilities(port, pdo,
					    index + 1,
					    snk_uw / 1000);
	if (ret) {
		dev_err(info->dev, "failed to set pd, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int sc2730_fchg_pps_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	union power_supply_propval val, vol;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (!info->pps_active) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to set online property ret = %d\n", ret);
			return ret;
		}
		info->pps_active = true;
	}

	vol.intval = input_vol;
	ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_VOLTAGE_NOW, &vol);
	if (ret) {
		dev_err(info->dev, "failed to set vol property\n");
		return ret;
	}

	return 0;
}

static int sc2730_fchg_pps_adjust_current(struct sc2730_fchg_info *info,
					 u32 input_current)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (!info->pps_active) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to set online property\n");
			return ret;
		}
		info->pps_active = true;
	}

	val.intval = input_current;
	ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (ret) {
		dev_err(info->dev, "failed to set current property\n");
		return ret;
	}

	return 0;
}

static int sc2730_fchg_enable_pps(struct sc2730_fchg_info *info, bool enable)
{
	union power_supply_propval val;
	int ret;

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm is NULL !!!\n");
		return -EINVAL;
	}

	if (info->pps_active && !enable) {
		val.intval = SC2730_DISABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to disbale pps, ret = %d\n", ret);
			return ret;
		}
		info->pps_active = false;
	} else if (!info->pps_active && enable) {
		val.intval = SC2730_ENABLE_PPS;
		ret = power_supply_set_property(info->psy_tcpm, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_err(info->dev, "failed to enable pps, ret = %d\n", ret);
			return ret;
		}
		info->pps_active = true;
	}

	return 0;
}

static int sc2730_fchg_pd_change(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	struct sc2730_fchg_info *info =
		container_of(nb, struct sc2730_fchg_info, pd_notify);
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "tcpm-source-psy-sc27xx-pd") != 0)
		goto out;

	if (event != PSY_EVENT_PROP_CHANGED)
		goto out;

	info->psy_tcpm = data;

	schedule_work(&info->pd_change_work);

out:
	return NOTIFY_OK;
}

static void sc2730_fchg_pd_change_work(struct work_struct *data)
{
	struct sc2730_fchg_info *info =
		container_of(data, struct sc2730_fchg_info, pd_change_work);
	union power_supply_propval val;
	struct tcpm_port *port;
	int pd_type, ret;

	mutex_lock(&info->lock);

	if (!info->psy_tcpm) {
		dev_err(info->dev, "psy_tcpm NULL !!!\n");
		goto out;
	}

	port = power_supply_get_drvdata(info->psy_tcpm);
	if (!port) {
		dev_err(info->dev, "failed to get tcpm port!\n");
		goto out;
	}

	ret = power_supply_get_property(info->psy_tcpm,
					POWER_SUPPLY_PROP_USB_TYPE,
					&val);
	if (ret) {
		dev_err(info->dev, "failed to get pd type\n");
		goto out;
	}

	pd_type = val.intval;
	if (pd_type == POWER_SUPPLY_USB_TYPE_PD ||
	    (!info->support_pd_pps && pd_type == POWER_SUPPLY_USB_TYPE_PD_PPS)) {
		info->pd_enable = true;
		info->pps_enable = false;
		info->pps_active = false;
		info->state = POWER_SUPPLY_USB_TYPE_PD;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	} else if (pd_type == POWER_SUPPLY_USB_TYPE_PD_PPS) {
		info->pps_enable = true;
		info->pd_enable = false;
		info->state = POWER_SUPPLY_USB_TYPE_PD_PPS;
		mutex_unlock(&info->lock);
		cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
		goto out1;
	} else if (pd_type == POWER_SUPPLY_USB_TYPE_C) {
		if (info->pd_enable)
			sc2730_fchg_pd_adjust_voltage(info, FCHG_VOLTAGE_5V);

		info->pd_enable = false;
		info->pps_enable = false;
		info->pps_active = false;
		if (info->state != POWER_SUPPLY_USB_TYPE_SFCP_1P0)
			info->state = POWER_SUPPLY_USB_TYPE_C;
	}

out:
	mutex_unlock(&info->lock);

out1:
	dev_info(info->dev, "pd type = %d\n", pd_type);
}
#else
static int sc2730_get_pd_fixed_voltage_max(struct sc2730_fchg_info *info, u32
				      *max_vol)
{
	return 0;
}

static int sc2730_get_pps_voltage_max(struct sc2730_fchg_info *info, u32
				      *max_vol)
{
	return 0;
}

static int sc2730_get_pps_current_max(struct sc2730_fchg_info *info, u32
				      *max_cur)
{
	return 0;
}

static int sc2730_fchg_pd_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	return 0;
}

static int sc2730_fchg_pps_adjust_voltage(struct sc2730_fchg_info *info,
					 u32 input_vol)
{
	return 0;
}

static int sc2730_fchg_pps_adjust_current(struct sc2730_fchg_info *info,
					 u32 input_current)
{
	return 0;
}

static int sc2730_fchg_enable_pps(struct sc2730_fchg_info *info, bool enable)
{
	return 0;
}
static int sc2730_fchg_pd_change(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	return NOTIFY_OK;
}

static void sc2730_fchg_pd_change_work(struct work_struct *data)
{

}
#endif

static int sc2730_fchg_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sc2730_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->state;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 0;
		if (info->pd_enable)
			sc2730_get_pd_fixed_voltage_max(info, &val->intval);
		else if (info->pps_enable)
			sc2730_get_pps_voltage_max(info, &val->intval);
		else if (info->sfcp_enable || info->afc_enable)//add afc VOLTAGE_MAX by guoyanjun
			val->intval = FCHG_VOLTAGE_9V;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->pps_enable)
			sc2730_get_pps_current_max(info, &val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2730_fchg_usb_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct sc2730_fchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == CM_PPS_CHARGE_DISABLE_CMD) {
			if (sc2730_fchg_enable_pps(info, false)) {
				ret = -EINVAL;
				dev_err(info->dev, "failed to disable pps\n");
			}
			break;
		} else if (val->intval == CM_PPS_CHARGE_ENABLE_CMD) {
			if (sc2730_fchg_enable_pps(info, true)) {
				ret = -EINVAL;
				dev_err(info->dev, "failed to enable pps\n");
			}
			break;
		}

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->pd_enable) {
			if (sc2730_fchg_enable_pps(info, false))
				dev_err(info->dev, "failed to disable pps\n");

			ret = sc2730_fchg_pd_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust pd vol\n");
		} else if (info->pps_enable) {
			ret = sc2730_fchg_pps_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust pd vol\n");
		} else if (info->sfcp_enable) {
			ret = sc2730_fchg_sfcp_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust sfcp vol\n");
		//+ add afc adjust voltage func by guoyanjun
		} else if (info->afc_enable) {
			printc("afc adjust voltage\n");
			ret = afc_fchg_adjust_voltage(info, val->intval);
			if (ret)
				dev_err(info->dev, "failed to adjust afc vol\n");
		} 
		//- add afc adjust voltage func by guoyanjun
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		sc2730_fchg_pps_adjust_current(info, val->intval);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2730_fchg_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sc2730_fchg_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_SFCP_1P0,
	POWER_SUPPLY_USB_TYPE_SFCP_2P0,
};

static enum power_supply_property sc2730_fchg_usb_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc sc2730_fchg_desc = {
	.name			= "sc2730_fast_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sc2730_fchg_usb_props,
	.num_properties		= ARRAY_SIZE(sc2730_fchg_usb_props),
	.get_property		= sc2730_fchg_usb_get_property,
	.set_property		= sc2730_fchg_usb_set_property,
	.property_is_writeable	= sc2730_fchg_property_is_writeable,
	.usb_types              = sc2730_fchg_usb_types,
	.num_usb_types          = ARRAY_SIZE(sc2730_fchg_usb_types),
};

static void sc2730_fchg_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sc2730_fchg_info *info =
		container_of(dwork, struct sc2730_fchg_info, work);

	mutex_lock(&info->lock);
	if (!info->limit) {
		if (!info->pps_enable || info->state != POWER_SUPPLY_USB_TYPE_PD_PPS)
			info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;

		info->detected = false;
		info->sfcp_enable = false;
		sc2730_fchg_disable(info);
	} else if (!info->detected) {
		info->detected = true;
		if (info->pd_enable || info->pps_enable || !info->support_sfcp) {
			sc2730_fchg_disable(info);
		} else if (sc2730_fchg_get_detect_status(info) ==
		    POWER_SUPPLY_USB_TYPE_SFCP_1P0) {
			/*
			 * Must release info->lock before send fast charge event
			 * to charger manager, otherwise it will cause deadlock.
			 */
			info->sfcp_enable = true;
			mutex_unlock(&info->lock);
			cm_notify_event(info->psy_usb, CM_EVENT_FAST_CHARGE, NULL);
			dev_info(info->dev, "pd_enable = %d, sfcp_enable = %d\n",
				 info->pd_enable, info->sfcp_enable);
			return;
		} else {
			sc2730_fchg_disable(info);
		}
		//+ add afc check when type is dcp and not other fast charge. by guoyanjun
		if (info->usb_phy->chg_type == DCP_TYPE && !(info->pd_enable || info->pps_enable)){
			printc("is not PD fschg,then try check afc fschg.\n");
			//+ yuecong.wt.20220401 add hv_disable sys node
			if(hv_disable)
				printc("hv charging is disabled!\n");
			else
				schedule_delayed_work(&info->afc_work,0*HZ);
			//- yuecong.wt.20220401 add hv_disable sys node
		}
		//- add afc check when type is dcp and not other fast charge. by guoyanjun
	}

	mutex_unlock(&info->lock);

	printc("pd_enable = %d, pps_enable = %d, sfcp_enable = %d\n",
		 info->pd_enable, info->pps_enable, info->sfcp_enable);
}

static int sc2730_fchg_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sc2730_fchg_info *info;
	struct power_supply_config charger_cfg = { };
	int irq, ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;
	info->state = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	info->pdata = of_device_get_match_data(info->dev);
	if (!info->pdata) {
		dev_err(info->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&info->work, sc2730_fchg_work);
	INIT_WORK(&info->pd_change_work, sc2730_fchg_pd_change_work);
	init_completion(&info->completion);

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &info->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get register address\n");
		return -ENODEV;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,input-voltage-microvolt",
				       &info->input_vol);
	if (ret) {
		dev_err(&pdev->dev, "failed to get fast charger voltage.\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,pd-fixed-max-microwatt",
				       &info->pd_fixed_max_uw);
	if (ret) {
		dev_info(&pdev->dev, "failed to get pd fixed max uw.\n");
		/* If this parameter is not defined in DTS, the default power is 10W */
		info->pd_fixed_max_uw = SC2730_PD_DEFAULT_POWER_UW;
	}

	info->support_pd_pps = device_property_read_bool(&pdev->dev,
							 "sprd,support-pd-pps");

	info->support_sfcp = device_property_read_bool(&pdev->dev,
						       "sprd,support-sfcp");

	info->pps_active = false;
	platform_set_drvdata(pdev, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(&pdev->dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->usb_notify.notifier_call = sc2730_fchg_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(&pdev->dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource specified\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return irq;
	}
	ret = devm_request_threaded_irq(info->dev, irq, NULL,
					sc2730_fchg_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq.\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}

	info->pd_notify.notifier_call = sc2730_fchg_pd_change;
	ret = power_supply_reg_notifier(&info->pd_notify);
	if (ret) {
		dev_err(info->dev, "failed to register pd notifier:%d\n", ret);
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;

	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc2730_fchg_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		power_supply_unreg_notifier(&info->pd_notify);
		return PTR_ERR(info->psy_usb);
	}

	sc2730_fchg_detect_status(info);
	//+ add afc gpio ctrl by guoyanjun
	info->afc.afc_error = 0;
	info->afc.afc_used = 0;
	info->afc.afc_switch_gpio = of_get_named_gpio_flags(np, "afc_switch_gpio", 0, &info->afc.afc_switch_flag);
		printc("afc_switch_gpio=%d\n", info->afc.afc_switch_gpio);
		if (gpio_is_valid(info->afc.afc_switch_gpio)) {
			ret = gpio_request_one(info->afc.afc_switch_gpio, GPIOF_OUT_INIT_LOW, "afc-switch");
			if (ret) {
				printc("Failed to request afc-switch GPIO\n");
			}
		}
		info->afc.afc_data_gpio = of_get_named_gpio_flags(np, "afc_data_gpio", 0, &info->afc.afc_data_flag);
		printc("afc_data_gpio=%d\n", info->afc.afc_data_gpio);
		if (gpio_is_valid(info->afc.afc_data_gpio)) {
			ret = gpio_request_one(info->afc.afc_data_gpio, GPIOF_OUT_INIT_LOW, "afc-data");
			if (ret) {
				printc("Failed to request afc-data GPIO\n");
			}
		}
		spin_lock_init(&info->afc.afc_spin_lock);
		INIT_DELAYED_WORK(&info->afc_work, afc_check_work);
	//- add afc gpio ctrl by guoyanjun
	return 0;
}

static int sc2730_fchg_remove(struct platform_device *pdev)
{
	struct sc2730_fchg_info *info = platform_get_drvdata(pdev);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static void sc2730_fchg_shutdown(struct platform_device *pdev)
{
	struct sc2730_fchg_info *info = platform_get_drvdata(pdev);
	int ret;
	u32 value = FCHG_DET_VOL_EXIT_SFCP;

	/*
	 * SFCP will handsharke failed from charging in shut down
	 * to charging in power up, because SFCP is not exit before
	 * shut down. Set bit3:4 to 2b'11 to exit SFCP.
	 */

	ret = regmap_update_bits(info->regmap, info->base + FCHG_CTRL,
				 FCHG_DET_VOL_MASK << FCHG_DET_VOL_SHIFT,
				 (value & FCHG_DET_VOL_MASK) << FCHG_DET_VOL_SHIFT);
	if (ret)
		dev_err(info->dev,
			"failed to set fast charger detect voltage.\n");
}

static const struct of_device_id sc2730_fchg_of_match[] = {
	{ .compatible = "sprd,sc2730-fast-charger", .data = &sc2730_info },
	{ .compatible = "sprd,ump9620-fast-chg", .data = &ump9620_info },
	{ .compatible = "sprd,sc2721-fast-charger", .data = &sc2721_info },
	{ }
};

static struct platform_driver sc2730_fchg_driver = {
	.driver = {
		.name = "sc2730-fast-charger",
		.of_match_table = sc2730_fchg_of_match,
	},
	.probe = sc2730_fchg_probe,
	.remove = sc2730_fchg_remove,
	.shutdown = sc2730_fchg_shutdown,
};

module_platform_driver(sc2730_fchg_driver);

MODULE_DESCRIPTION("Spreadtrum SC2730 Fast Charger Driver");
MODULE_LICENSE("GPL v2");
