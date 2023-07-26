/*
 * Driver for the TI sgm4154x charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include "battery_id_via_adc.h"

#define SGM4154X_REG_00				0x00
#define SGM4154X_REG_01				0x01
#define SGM4154X_REG_02				0x02
#define SGM4154X_REG_03				0x03
#define SGM4154X_REG_04				0x04
#define SGM4154X_REG_05				0x05
#define SGM4154X_REG_06				0x06
#define SGM4154X_REG_07				0x07
#define SGM4154X_REG_08				0x08
#define SGM4154X_REG_09				0x09
#define SGM4154X_REG_0A				0x0a
#define SGM4154X_REG_0B				0x0b
#define SGM4154X_REG_0C				0x0c
#define SGM4154X_REG_0D				0x0d
#define SGM4154X_REG_0E				0x0e
#define SGM4154X_REG_0F				0x0f
#define SGM4154X_REG_NUM			16

/* Register 0x00 */
#define REG00_ENHIZ_MASK			BIT(7)
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_ICHG_MASK			GENMASK(6,5)
#define REG00_EN_ILIM_SHIFT			5
#define REG00_IINLIM_MASK			GENMASK(4,0)
#define REG00_IINLIM_SHIFT			0

/* Register 0x01*/
#define REG01_ENPFM_MASK			BIT(7)
#define REG01_ENPFM_SHIFT			7
#define REG01_WDT_MASK				BIT(6)
#define REG01_WDT_SHIFT				6
#define REG01_OTG_CONFIG_MASK		BIT(5) //enable otg
#define REG01_OTG_CONFIG_SHIFT		5
#define REG01_ENCHG_MASK			BIT(4) //enable battery charging
#define REG01_ENCHG_SHIFT			4
#define REG01_SYS_MIN_MASK			GENMASK(3,1)
#define REG01_SYS_MIN_SHIFT			1
#define REG01_MIN_BAT_SEL_MASK		BIT(0)
#define REG01_MIN_BAT_SEL_SHIFT		0


/* Register 0x02*/
#define REG02_BOOST_LIM_MASK			BIT(7)
#define REG02_BOOST_LIM_SHIFT			7
#define REG02_Q1_FULLON_MASK			BIT(6)
#define REG02_Q1_FULLON_SHIFT			6
#define REG02_ICHG_MASK					GENMASK(5,0)
#define REG02_ICHG_SHIFT				0

/* Register 0x03 */
#define REG03_IPRECHG_MASK				GENMASK(7,4)
#define REG03_IPRECHG_SHIFT				4
#define REG03_ITERM_MASK				GENMASK(3,0)
#define REG03_ITERM_SHIFT				0

/* Register 0x04*/
#define REG04_VREG_MASK				GENMASK(7,3)
#define REG04_VREG_SHIFT			3
#define REG04_TOPOFF_TIMER_MASK		GENMASK(2,1)
#define REG04_TOPOFF_TIMER_SHIFT	1
#define REG04_VRECHG_MASK			BIT(0)
#define REG04_VRECHG_SHIFT			0

/* Register 0x05*/
#define REG05_EN_TERM_MASK			BIT(7)
#define REG05_EN_TERM_SHIFT			7
#define REG05_ARDCEN_ITS_MASK		BIT(6)
#define REG05_ARDCEN_ITS_SHIFT		6
#define REG05_WDT_TIMER_MASK		GENMASK(5,4)
#define REG05_WDT_TIMER_SHIFT		4
#define REG05_EN_TIMER_MASK			BIT(3)
#define REG05_EN_TIMER_SHIFT		3
#define REG05_CHG_TIMER_MASK		BIT(2)
#define REG05_CHG_TIMER_SHIFT		2
#define REG05_TREG_MASK				BIT(1)
#define REG05_TREG_SHIFT			1
#define REG05_JEITA_ISET_L_MASK		BIT(0)
#define REG05_JEITA_ISET_L_SHIFT	0

/* Register 0x06*/
#define REG06_OVP_MASK				GENMASK(7,6)
#define REG06_OVP_SHIFT				6
#define REG06_BOOSTV_MASK			GENMASK(5,4)
#define REG06_BOOSTV_SHIFT			4
#define REG06_VINDPM_MASK			GENMASK(3,0)
#define REG06_VINDPM_SHIFT			0

/* Register 0x07*/
#define REG07_IINDET_EN_MASK		BIT(7)
#define REG07_IINDET_EN_SHIFT		7
#define REG07_TMR2X_EN_MASK			BIT(6)
#define REG07_TMR2X_EN_SHIFT		6
#define REG07_BATFET_DIS_MASK		BIT(5)
#define REG07_BATFET_DIS_SHIFT		5
#define REG07_JEITA_VSET_H_MASK		BIT(4)
#define REG07_JEITA_VSET_H_SHIFT	4
#define REG07_BATFET_DLY_MASK		BIT(3)
#define REG07_BATFET_DLY_SHIFT		3
#define REG07_BATFET_RST_EN_MASK	BIT(2)
#define REG07_BATFET_RST_EN_SHIFT	2
#define REG07_VDPM_BAT_TRACK_MASK	GENMASK(1,0)
#define REG07_VDPM_BAT_TRACK_SHIFT	0

/* Register 0x08*/
#define REG08_VBUS_STAT_MASK		GENMASK(7,5)
#define REG08_VBUS_STAT_SHIFT		5
#define REG08_CHRG_STAT_MASK		GENMASK(4,3)
#define REG08_CHRG_STAT_SHIFT		3
#define REG08_PG_STAT_MASK			BIT(2)
#define REG08_PG_STAT_SHIFT			2
#define REG08_THERM_STAT_MASK		BIT(1)
#define REG08_THERM_STAT_SHIFT		1
#define REG08_VSYS_STAT_MASK		BIT(0)
#define REG08_VSYS_STAT_SHIFT		0

/* Register 0x09*/
#define REG09_WDT_FAULT_MASK		BIT(7)
#define REG09_WDT_FAULT_SHIFT		7
#define REG09_BOOST_FAULT_MASK		BIT(6)
#define REG09_BOOST_FAULT_SHIFT		6
#define REG09_CHRG_FAULT_MASK		GENMASK(5,4)
#define REG09_CHRG_FAULT_SHIFT		4
#define REG09_BAT_FAULT_MASK		BIT(3)
#define REG09_BAT_FAULT_SHIFT		3
#define REG09_NTC_FAULT_MASK		GENMASK(2,0)
#define REG09_NTC_FAULT_SHIFT		0

/* Register 0x0A*/
#define REG0A_VBUS_GD_MASK			BIT(7)
#define REG0A_VBUS_GD_SHIFT			7
#define REG0A_VINDPM_SATA_MASK		BIT(6)
#define REG0A_VINDPM_SATA_SHIFT		6
#define REG0A_IINDPM_SATA_MASK		BIT(5)
#define REG0A_IINDPM_SATA_SHIFT		5
#define REG0A_CV_SATA_MASK			BIT(4)
#define REG0A_CV_SATA_SHIFT			4
#define REG0A_TOPOFF_ACTIVE_MASK	BIT(3)
#define REG0A_TOPOFF_ACTIVE_SHIFT	3
#define REG0A_ACOV_SATA_MASK		BIT(2)
#define REG0A_ACOV_SATA_SHIFT		2
#define REG0A_VINDPM_INT_MASK		BIT(1)
#define REG0A_VINDPM_INT_SHIFT		1
#define REG0A_IINDPM_INT_MASK		BIT(0)
#define REG0A_IINDPM_INT_SHIFT		0

/* Register 0x0B*/
#define REG0B_REG_RST_MASK			BIT(7)
#define REG0B_REG_RST_SHIFT			7
#define REG0B_PN_ID_MASK			GENMASK(6,3)
#define REG0B_PN_ID_SHIFT			3
#define REG0B_SGMPART_MASK			BIT(2)
#define REG0B_SGMPART_SHIFT			2
#define REG0B_DEV_REV_MASK			GENMASK(1,0)
#define REG0B_DEV_REV_SHIFT			0

/* Register 0x0C*/
#define REG0C_JEITA_VSET_L_MASK		BIT(7)
#define REG0C_JEITA_VSET_L_SHIFT	7
#define REG0C_JEITA_ISET_L_EN_MASK	BIT(6)
#define REG0C_JEITA_ISET_L_EN_SHIFT	6
#define REG0C_JEITA_ISET_H_MASK		GENMASK(5,4)
#define REG0C_JEITA_ISET_H_SHIFT	4
#define REG0C_JEITA_VT2_MASK		GENMASK(3,2)
#define REG0C_JEITA_VT2_SHIFT		2
#define REG0C_JEITA_VT3_MASK		GENMASK(1,0)
#define REG0C_JEITA_VT3_SHIFT		0

/* Register 0x0D*/
#define REG0D_EN_PUMPX_MASK			BIT(7)
#define REG0D_EN_PUMPX_SHIFT		7
#define REG0D_PUMPX_UP_MASK			BIT(6)
#define REG0D_PUMPX_UP_SHIFT		6
#define REG0D_PUMPX_DN_MASK			BIT(5)
#define REG0D_PUMPX_DN_SHIFT		5
#define REG0D_DP_VSET_MASK			GENMASK(4,3)
#define REG0D_DP_VSET_SHIFT			3
#define REG0D_DM_VSET_MASK			GENMASK(2,1)
#define REG0D_DM_VSET_SHIFT			1
#define REG0D_JEITA_EN_MASK			BIT(0)
#define REG0D_JEITA_EN_SHIFT		0

/* Register 0x0E*/
#define REG0E_INPUT_DET_DONE_MASK	BIT(7)
#define REG0E_INPUT_DET_DONE_SHIFT	7
#define REG0E_RESERVED_MASK			GENMASK(6,0)
#define REG0E_RESERVED_SHIFT		0

/* Register 0x0F*/
#define REG0F_VREG_FT_MASK			GENMASK(7,6)
#define REG0F_VREG_FT_SHIFT			6
#define REG0F_RESERVED_MASK			BIT(5)
#define REG0F_RESERVED_SHIFT		5
#define REG0F_DCEN_MASK				BIT(4)
#define REG0F_DCEN_SHIFT			4
#define REG0F_STAT_SET_MASK			GENMASK(3,2)
#define REG0F_STAT_SET_SHIFT		2
#define REG0F_VINDPM_OS_MASK		GENMASK(1,0)
#define REG0F_VINDPM_OS_SHIFT		0

#define WDT_RESET				1
#define REG_RESET				1
#define OTG_DISABLE			0
#define OTG_ENABLE			1
/* charge status flags  */
#define HIZ_DISABLE			0
#define HIZ_ENABLE			1

#define SGM4154X_OTG_MASK		GENMASK(5, 4)
#define SGM4154X_OTG_EN		    BIT(5)

/* Part ID  */
#define SGM4154X_PN_MASK	    GENMASK(6, 3)
#define SGM4154X_PN_41541_ID    (BIT(6)| BIT(5))
#define SGM4154X_PN_41542_ID    (BIT(6)| BIT(5)| BIT(3))

/* WDT TIMER SET  */
#define SGM4154X_WDT_TIMER_DISABLE     0
#define SGM4154X_WDT_TIMER_40S         1
#define SGM4154X_WDT_TIMER_80S         2
#define SGM4154X_WDT_TIMER_160S        3

#define SGM4154X_WDT_RST_MASK          BIT(6)

/* SAFETY TIMER SET  */
#define SGM4154X_SAFETY_TIMER_MASK     GENMASK(3, 3)
#define SGM4154X_SAFETY_TIMER_DISABLE     0
#define SGM4154X_SAFETY_TIMER_EN       BIT(3)
#define SGM4154X_SAFETY_TIMER_5H         0
#define SGM4154X_SAFETY_TIMER_10H      BIT(2)

/* recharge voltage  */
#define SGM4154X_VRECHARGE              BIT(0)
#define SGM4154X_VRECHRG_STEP_mV		100
#define SGM4154X_VRECHRG_OFFSET_mV		100

/* charge status  */
#define SGM4154X_VSYS_STAT		BIT(0)
#define SGM4154X_THERM_STAT		BIT(1)
#define SGM4154X_PG_STAT		BIT(2)
#define SGM4154X_CHG_STAT_MASK	GENMASK(4, 3)
#define SGM4154X_PRECHRG		BIT(3)
#define SGM4154X_FAST_CHRG	    BIT(4)
#define SGM4154X_TERM_CHRG	    (BIT(3)| BIT(4))

/* charge type  */
#define SGM4154X_VBUS_STAT_MASK	GENMASK(7, 5)
#define SGM4154X_NOT_CHRGING	0
#define SGM4154X_USB_SDP		BIT(5)
#define SGM4154X_USB_CDP		BIT(6)
#define SGM4154X_USB_DCP		(BIT(5) | BIT(6))
#define SGM4154X_UNKNOWN	    (BIT(7) | BIT(5))
#define SGM4154X_NON_STANDARD	(BIT(7) | BIT(6))
#define SGM4154X_OTG_MODE	    (BIT(7) | BIT(6) | BIT(5))

/* TEMP Status  */
#define SGM4154X_TEMP_MASK	    GENMASK(2, 0)
#define SGM4154X_TEMP_NORMAL	BIT(0)
#define SGM4154X_TEMP_WARM	    BIT(1)
#define SGM4154X_TEMP_COOL	    (BIT(0) | BIT(1))
#define SGM4154X_TEMP_COLD	    (BIT(0) | BIT(3))
#define SGM4154X_TEMP_HOT	    (BIT(2) | BIT(3))

/* precharge current  */
#define SGM4154X_PRECHRG_CUR_MASK		GENMASK(7, 4)
#define SGM4154X_PRECHRG_CURRENT_STEP_uA		60000
#define SGM4154X_PRECHRG_I_MIN_uA		60000
#define SGM4154X_PRECHRG_I_MAX_uA		780000
#define SGM4154X_PRECHRG_I_DEF_uA		180000

/* termination current  */
#define SGM4154X_TERMCHRG_CURRENT_STEP_uA	60
#define SGM4154X_TERMCHRG_I_MIN_uA		60
#define SGM4154X_TERMCHRG_I_MAX_uA		960
#define SGM4154X_TERMCHRG_I_DEF_uA		180

/* charge current  */
#define SGM4154X_ICHRG_CURRENT_STEP		60
#define SGM4154X_ICHRG_I_MIN			60
#define SGM4154X_ICHRG_I_MAX			3780
#define SGM4154X_ICHRG_I_DEF			2040

/* charge voltage  */
#define SGM4154X_VREG_V_MAX_uV	    4624
#define SGM4154X_VREG_V_MIN_uV	    3856
#define SGM4154X_VREG_V_DEF_uV	    4208
#define SGM4154X_VREG_V_STEP_uV	    32

/* VREG Fine Tuning  */
#define SGM4154X_VREG_FT_MASK	     GENMASK(7, 6)
#define SGM4154X_VREG_FT_UP_8mV	     BIT(6)
#define SGM4154X_VREG_FT_DN_8mV	     BIT(7)
#define SGM4154X_VREG_FT_DN_16mV	 (BIT(7) | BIT(6))

/* iindpm current  */
#define SGM4154X_IINDPM_I_MIN	100
#define SGM4154X_IINDPM_I_MAX	3800
#define SGM4154X_IINDPM_STEP	    100
#define SGM4154X_IINDPM_DEF	    2400

/* vindpm voltage  */
#define SGM4154X_VINDPM_V_MIN_uV    3900000
#define SGM4154X_VINDPM_V_MAX_uV    12000000
#define SGM4154X_VINDPM_STEP_uV     100000
#define SGM4154X_VINDPM_DEF_uV	    3600000
#define SGM4154X_VINDPM_OS_MASK     GENMASK(1, 0)

/* DP DM SEL  */
#define SGM4154X_DP_VSEL_MASK       GENMASK(4, 3)
#define SGM4154X_DM_VSEL_MASK       GENMASK(2, 1)

/* PUMPX SET  */
#define SGM4154X_EN_PUMPX           BIT(7)
#define SGM4154X_PUMPX_UP           BIT(6)
#define SGM4154X_PUMPX_DN           BIT(5)

/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
#define SGM4154X_REG_CHARGE_DONE_MASK		GENMASK(4, 3)
#define SGM4154X_REG_CHARGE_DONE_SHIFT		3
#define SGM4154X_CHARGE_DONE                               0x3

#define SGM4154X_REG_RECHG_MASK      GENMASK(0, 0)
#define SGM4154X_REG_RECHG_SHIFT      0
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
/* PN_ID 0x1101-4353 for SGM41542 */
#define REG0B_PN_ID						13
/* Other Realted Definition*/
#define SGM4154X_BATTERY_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			BIT(0)

#define SGM4154X_WDT_VALID_MS			50

#define SGM4154X_OTG_ALARM_TIMER_MS		15000
#define SGM4154X_OTG_VALID_MS			500
#define SGM4154X_OTG_RETRY_TIMES			10

#define SGM4154X_DISABLE_PIN_MASK		BIT(0)
#define SGM4154X_DISABLE_PIN_MASK_2730		BIT(0)
#define SGM4154X_DISABLE_PIN_MASK_2721		BIT(15)
#define SGM4154X_DISABLE_PIN_MASK_2720		BIT(0)

#define SGM4154X_FAST_CHG_VOL_MAX		10500000
#define SGM4154X_NORMAL_CHG_VOL_MAX		6500000

#define SGM4154X_WAKE_UP_MS			2000
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
#define SGM4154X_CURRENT_WORK_MS		msecs_to_jiffies(100)
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end*/
/* Tab A7 Lite T618 code for AX6189DEV-78 by shixuanxuan at 20220106 start */
#define RETRYCOUNT				3
/* Tab A7 Lite T618 code for AX6189DEV-78 by shixuanxuan at 20220106 end */

struct sgm4154x_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm4154x_dump_reg;
	struct device_attribute attr_sgm4154x_lookup_reg;
	struct device_attribute attr_sgm4154x_sel_reg_id;
	struct device_attribute attr_sgm4154x_reg_val;
	struct attribute *attrs[5];

	struct sgm4154x_charger_info *info;
};

struct sgm4154x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	struct notifier_block pd_swap_notify;
	struct notifier_block extcon_nb;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	struct delayed_work cur_work;
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	bool need_disable_Q1;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 start */
	u32 actual_limit_voltage;
	/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 end */
	bool otg_enable;
	struct alarm otg_timer;
	struct sgm4154x_charger_sysfs *sysfs;
	int reg_id;
	bool disable_power_path;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	bool is_sink;
	bool use_typec_extcon;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
};

struct sgm4154x_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm4154x_charger_reg_tab reg_tab[SGM4154X_REG_NUM + 1] = {
	{0, SGM4154X_REG_00, "Setting Input Limit Current reg"},
	{1, SGM4154X_REG_01, "Setting Vindpm_OS reg"},
	{2, SGM4154X_REG_02, "Related Function Enable reg"},
	{3, SGM4154X_REG_03, "Related Function Config reg"},
	{4, SGM4154X_REG_04, "Setting Charge Limit Current reg"},
	{5, SGM4154X_REG_05, "Setting Terminal Current reg"},
	{6, SGM4154X_REG_06, "Setting Charge Limit Voltage reg"},
	{7, SGM4154X_REG_07, "Related Function Config reg"},
	{8, SGM4154X_REG_08, "IR Compensation Resistor Setting reg"},
	{9, SGM4154X_REG_09, "Related Function Config reg"},
	{10, SGM4154X_REG_0A, "Boost Mode Related Setting reg"},
	{11, SGM4154X_REG_0B, "Status reg"},
	{12, SGM4154X_REG_0C, "Fault reg"},
	{13, SGM4154X_REG_0D, "Setting Vindpm reg"},
	{14, SGM4154X_REG_0E, "ADC Conversion of Battery Voltage reg"},
	{15, SGM4154X_REG_0F, "ADDC Conversion of System Voltage reg"},
	{16, 0, "null"},
};

static int sgm4154x_charger_set_limit_current(struct sgm4154x_charger_info *info,
					     u32 limit_cur);

static int sgm4154x_read(struct sgm4154x_charger_info *info, u8 reg, u8 *data)
{
	int ret;
	/* Tab A7 Lite T618 code for AX6189DEV-78 by shixuanxuan at 20220106 start */
	int re_try = RETRYCOUNT;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	while ((ret < 0) && (re_try != 0)) {
		ret = i2c_smbus_read_byte_data(info->client, reg);
		re_try--;
	}
	/* Tab A7 Lite T618 code for AX6189DEV-78 by shixuanxuan at 20220106 end */
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sgm4154x_write(struct sgm4154x_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sgm4154x_update_bits(struct sgm4154x_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm4154x_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm4154x_write(info, reg, v);
}

static void sgm4154x_charger_dump_register(struct sgm4154x_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SGM4154X_REG_NUM; i++) {
		ret = sgm4154x_read(info, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_err(info->dev, "%s: %s", __func__, buf);
}

static bool sgm4154x_charger_is_bat_present(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SGM4154X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (!ret && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return present;
}

static int sgm4154x_charger_is_fgu_present(struct sgm4154x_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SGM4154X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sgm4154x_charger_get_pn_value(struct sgm4154x_charger_info *info)
 {
 	u8 reg_val;
 	int ret;
 
 	ret = sgm4154x_read(info, SGM4154X_REG_0B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "sgm4154x Failed to read PN value register, ret = %d\n", ret);
		return ret;
	}

	reg_val &= REG0B_PN_ID_MASK;
	reg_val >>= REG0B_PN_ID_SHIFT;
	if (reg_val != REG0B_PN_ID) {
		dev_err(info->dev, "The sgm4154x PN value is 0x%x \n", reg_val);
		return -EINVAL;
	}

	return 0;

}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
static int sgm4154x_charger_set_recharge(struct sgm4154x_charger_info *info)
{
	int ret = 0;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_04,
				  SGM4154X_REG_RECHG_MASK,
				  0x1 << SGM4154X_REG_RECHG_SHIFT);

	return ret;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
static int sgm4154x_charger_set_vindpm(struct sgm4154x_charger_info *info, u32 vol)
{
	u8 reg_val,ops_val;
	int ret;
	
	if (vol < 3900) {
		reg_val = 0x0;
		ops_val = 0;
	} else if (vol >= 3900 && vol < 5900) {
		reg_val = (vol - 3900) / 100;
		ops_val = 0;
	} else if (vol >=5900 && vol < 7500) {
		reg_val = (vol - 5900) / 100;
		ops_val = 1;
	} else if (vol >=7500 && vol <10500) {
		reg_val = (vol - 7500) / 100;
		ops_val = 2;
	} else {
		reg_val = (vol - 10500) / 100;
		ops_val = 3;
	}
	
	ret = sgm4154x_update_bits(info, SGM4154X_REG_0F,
				   REG0F_VINDPM_OS_MASK, ops_val);
	if (ret) {
		dev_err(info->dev, "set vindpm_os failed\n");
 		return ret;
	}
	ret = sgm4154x_update_bits(info, SGM4154X_REG_06,
				   REG06_VINDPM_MASK, reg_val);
	if (ret) {
		dev_err(info->dev, "set vindpm_value failed\n");
		return ret;
	}
	return 0;
}

static int sgm4154x_charger_set_termina_vol(struct sgm4154x_charger_info *info, u32 vol)
{
	int reg_val;
	/* Tab A8 code for P211125-06231 by zhaichao at 20211126 start */
	int ret;
	int retry_cnt = 3;
	/* Tab A8 code for P211125-06231 by zhaichao at 20211126 end */

	if (vol < SGM4154X_VREG_V_MIN_uV)
		vol = SGM4154X_VREG_V_MIN_uV;
	else if (vol > SGM4154X_VREG_V_MAX_uV)
		vol = SGM4154X_VREG_V_MAX_uV;


	reg_val = (vol-SGM4154X_VREG_V_MIN_uV) / SGM4154X_VREG_V_STEP_uV;

	/* Tab A8 code for P211125-06231 by zhaichao at 20211126 start */
	do {
		ret = sgm4154x_update_bits(info, SGM4154X_REG_04, REG04_VREG_MASK,
				reg_val << REG04_VREG_SHIFT);
		retry_cnt--;
		usleep_range(30, 40);
	} while ((retry_cnt != 0) && (ret != 0));
	/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 start */
	if (ret != 0) {
		dev_err(info->dev, "sgm4154x set terminal voltage failed\n");
	} else {
		info->actual_limit_voltage = (reg_val * SGM4154X_VREG_V_STEP_uV) + SGM4154X_VREG_V_MIN_uV;
		dev_err(info->dev, "sgm4154x set terminal voltage success, the value is %d\n" ,info->actual_limit_voltage);
	}
	/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 end */
	return ret;
	/* Tab A8 code for P211125-06231 by zhaichao at 20211126 end */
}

static int sgm4154x_charger_set_termina_cur(struct sgm4154x_charger_info *info, u32 cur)
{
	int reg_val;

	if (cur < SGM4154X_TERMCHRG_I_MIN_uA)
		cur = SGM4154X_TERMCHRG_I_MIN_uA;
	else if (cur > SGM4154X_TERMCHRG_I_MAX_uA)
		cur = SGM4154X_TERMCHRG_I_MAX_uA;

	reg_val = cur / SGM4154X_TERMCHRG_CURRENT_STEP_uA;

	return sgm4154x_update_bits(info, SGM4154X_REG_03, REG03_ITERM_MASK, reg_val);
}

/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
static int sgm4154x_charger_en_chg_timer(struct sgm4154x_charger_info *info, bool val)
{
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (val) {
		ret = sgm4154x_update_bits(info, SGM4154X_REG_05,
				  REG05_EN_TIMER_MASK,
				  0x1 << REG05_EN_TIMER_SHIFT);
		pr_info("sgm4154x EN_TIMER is enabled\n");
	} else {
		ret = sgm4154x_update_bits(info, SGM4154X_REG_05,
				  REG05_EN_TIMER_MASK,
				  0x0 << REG05_EN_TIMER_SHIFT);
		pr_info("sgm4154x EN_TIMER is disabled\n");
	}

	return ret;
}
/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */

static int sgm4154x_charger_hw_init(struct sgm4154x_charger_info *info)
{
	struct power_supply_battery_info bat_info;
	int ret;

	int bat_id = 0;

	bat_id = battery_get_bat_id();
	ret = power_supply_get_battery_info(info->psy_usb, &bat_info, bat_id);

	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;
/* Tab A8 code for AX6300DEV-3907 by qiaodan at 20220104 start */
#ifdef HQ_D85_BUILD
		info->vol_max_mv = 4000;
#else
		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
#endif
/* Tab A8 code for AX6300DEV-3907 by qiaodan at 20220104 end */
		info->termination_cur = bat_info.charge_term_current_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = sgm4154x_update_bits(info, SGM4154X_REG_0B, REG0B_REG_RST_MASK,
					  REG_RESET << REG0B_REG_RST_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset sgm4154x failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_vindpm(info, info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sgm4154x vindpm vol failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set sgm4154x terminal vol failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set sgm4154x terminal cur failed\n");
			return ret;
		}
		/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
		ret = sgm4154x_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret) {
			dev_err(info->dev, "set sgm4154x limit current failed\n");
			return ret;
		}

		sgm4154x_charger_set_recharge(info);
		if (ret) {
			dev_err(info->dev, "set sgm4154x recharge failed\n");
			return ret;
		}
		/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
		/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
		ret = sgm4154x_charger_en_chg_timer(info, false);
		/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	}

	return ret;
}

static int sgm4154x_enter_hiz_mode(struct sgm4154x_charger_info *info)
{
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_00,
				  REG00_ENHIZ_MASK, HIZ_ENABLE << REG00_ENHIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "enter HIZ mode failed\n");

	return ret;
}

static int sgm4154x_exit_hiz_mode(struct sgm4154x_charger_info *info)
{
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_00,
				  REG00_ENHIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "exit HIZ mode failed\n");

	return ret;
}

static int sgm4154x_get_hiz_mode(struct sgm4154x_charger_info *info,u32 *value)
{
	u8 buf;
	int ret;

	ret = sgm4154x_read(info, SGM4154X_REG_00, &buf);
	*value = (buf & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return ret;
}

static int sgm4154x_charger_get_charge_voltage(struct sgm4154x_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SGM4154X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SGM4154X_BATTERY_NAME\n");
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

static int sgm4154x_charger_start_charge(struct sgm4154x_charger_info *info)
{
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_00,
				  REG00_ENHIZ_MASK, HIZ_DISABLE << REG00_ENHIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm4154x_update_bits(info, SGM4154X_REG_05, REG05_WDT_TIMER_MASK,
				  SGM4154X_WDT_TIMER_40S << REG05_WDT_TIMER_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm4154x watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable sgm4154x charge failed\n");
			return ret;
		}

	ret = sgm4154x_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sgm4154x_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sgm4154x terminal cur failed\n");

	return ret;
}

static void sgm4154x_charger_stop_charge(struct sgm4154x_charger_info *info)
{
	int ret;
	bool present = sgm4154x_charger_is_bat_present(info);

	if (!present || info->need_disable_Q1) {
		ret = sgm4154x_update_bits(info, SGM4154X_REG_00, REG00_ENHIZ_MASK,
					  HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable sgm4154x charge failed\n");

	ret = sgm4154x_update_bits(info, SGM4154X_REG_05, REG05_WDT_TIMER_MASK,
				  SGM4154X_WDT_TIMER_DISABLE << REG05_WDT_TIMER_SHIFT);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm4154x watchdog\n");

}

static int sgm4154x_charger_set_current(struct sgm4154x_charger_info *info,
				       u32 cur)
{
	int ret;
	int reg_val;

	/* Tab A8 code for AX6300DEV-761 by zhaichao at 20210924 start */
	cur = cur / 1000;
	/* Tab A8 code for AX6300DEV-761 by zhaichao at 20210924 end */
	if (cur < SGM4154X_ICHRG_I_MIN)
		cur = SGM4154X_ICHRG_I_MIN;
	else if ( cur > SGM4154X_ICHRG_I_MAX)
		cur = SGM4154X_ICHRG_I_MAX;

	reg_val = cur / SGM4154X_ICHRG_CURRENT_STEP;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_02, REG02_ICHG_MASK, reg_val);
	
	return ret;
}

static int sgm4154x_charger_get_current(struct sgm4154x_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154X_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	*cur = reg_val * SGM4154X_ICHRG_CURRENT_STEP * 1000;

	return 0;
}

static int sgm4154x_charger_set_limit_current(struct sgm4154x_charger_info *info,
					     u32 iindpm)
{
	int ret;
	int reg_val;
	info->last_limit_current = iindpm;
	iindpm = iindpm / 1000;
	if (iindpm < SGM4154X_IINDPM_I_MIN) {
		reg_val = 0x00;
		iindpm = SGM4154X_IINDPM_I_MIN;
	} else if (iindpm >= SGM4154X_IINDPM_I_MAX) {
		reg_val = 0x1F;
		iindpm = SGM4154X_IINDPM_I_MAX;
	} else if (iindpm > SGM4154X_IINDPM_I_MIN && iindpm <= 3100) {//default
		reg_val = (iindpm - SGM4154X_IINDPM_I_MIN) / SGM4154X_IINDPM_STEP;
		iindpm = reg_val * SGM4154X_IINDPM_STEP + SGM4154X_IINDPM_I_MIN;
	} else if (iindpm > 3100 && iindpm < SGM4154X_IINDPM_I_MAX) {
		reg_val = 0x1E;
		iindpm = 3100;
	}
	ret = sgm4154x_update_bits(info, SGM4154X_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set SGM4154X limit cur failed\n");
	info->actual_limit_current = iindpm * 1000;
	return ret;
}
/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 start */
static u32 sgm4154x_charger_get_limit_voltage(struct sgm4154x_charger_info *info,
					     u32 *limit_vol)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154X_REG_04, &reg_val);
	if (ret < 0) {
		return ret;
	}

	reg_val &= REG04_VREG_MASK;
	*limit_vol = ((reg_val >> REG04_VREG_SHIFT) * SGM4154X_VREG_V_STEP_uV) + SGM4154X_VREG_V_MIN_uV;

	if (*limit_vol < SGM4154X_VREG_V_MIN_uV) {
		*limit_vol = SGM4154X_VREG_V_MIN_uV;
	} else if (*limit_vol >= SGM4154X_VREG_V_MAX_uV) {
		*limit_vol = SGM4154X_VREG_V_MAX_uV;
	}

	dev_err(info->dev, "limit voltage is %d, actual_limt is %d\n", *limit_vol, info->actual_limit_voltage);

	return 0;
}
/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 end */
static u32 sgm4154x_charger_get_limit_current(struct sgm4154x_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sgm4154x_read(info, SGM4154X_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	*limit_cur = reg_val * SGM4154X_IINDPM_STEP + SGM4154X_IINDPM_I_MIN;
	if (*limit_cur >= SGM4154X_IINDPM_I_MAX)
		*limit_cur = SGM4154X_IINDPM_I_MAX * 1000;
	else
		*limit_cur = *limit_cur * 1000;

	return 0;
}

static inline int sgm4154x_charger_get_health(struct sgm4154x_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int sgm4154x_charger_get_online(struct sgm4154x_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}
/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 start */
static int sgm4154x_charger_feed_watchdog(struct sgm4154x_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;
	u32 limit_voltage = 4200;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_01, REG01_WDT_MASK,
				  WDT_RESET << REG01_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset SGM4154X failed\n");
		return ret;
	}

	ret = sgm4154x_charger_get_limit_voltage(info, &limit_voltage);
	if (ret) {
		dev_err(info->dev, "get limit voltage failed\n");
		return ret;
	}

	if (info->actual_limit_voltage != limit_voltage) {
		ret = sgm4154x_charger_set_termina_vol(info, info->actual_limit_voltage);
		if (ret) {
			dev_err(info->dev, "set terminal voltage failed\n");
			return ret;
		}

		ret = sgm4154x_charger_set_recharge(info);
		if (ret) {
			dev_err(info->dev, "set sgm4154x recharge failed\n");
			return ret;
		}
	}

	ret = sgm4154x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = sgm4154x_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}
/* Tab A8 code for AX6300DEV-3825 by qiaodan at 20211220 end */
static int sgm4154x_charger_set_fchg_current(struct sgm4154x_charger_info *info,
					    u32 val)
{
	int ret, limit_cur, cur;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		limit_cur = info->cur.fchg_limit;
		cur = info->cur.fchg_cur;
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		limit_cur = info->cur.dcp_limit;
		cur = info->cur.dcp_cur;
	} else {
		return 0;
	}

	ret = sgm4154x_charger_set_limit_current(info, limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = sgm4154x_charger_set_current(info, cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static inline int sgm4154x_charger_get_status(struct sgm4154x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
static int sgm4154x_charger_get_charge_done(struct sgm4154x_charger_info *info,
	union power_supply_propval *val)
{
	int ret = 0;
	u8 reg_val = 0;

	if (!info || !val) {
		dev_err(info->dev, "[%s]line=%d: info or val is NULL\n", __FUNCTION__, __LINE__);
		return ret;
	}

	ret = sgm4154x_read(info, SGM4154X_REG_08, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get charge_done, ret = %d\n", ret);
		return ret;
	}

	reg_val &= SGM4154X_REG_CHARGE_DONE_MASK;
	reg_val >>= SGM4154X_REG_CHARGE_DONE_SHIFT;
	val->intval = (reg_val == SGM4154X_CHARGE_DONE);

	return 0;
}
/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
static int sgm4154x_charger_set_status(struct sgm4154x_charger_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = sgm4154x_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = sgm4154x_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = sgm4154x_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 9V charge voltage\n");
			return ret;
		}
		if (input_vol > SGM4154X_FAST_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	} else if (val == false) {
		ret = sgm4154x_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > SGM4154X_NORMAL_CHG_VOL_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sgm4154x_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sgm4154x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sgm4154x_charger_work(struct work_struct *data)
{
	struct sgm4154x_charger_info *info =
		container_of(data, struct sgm4154x_charger_info, work);
	bool present;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	int retry_cnt = 12;

	present = sgm4154x_charger_is_bat_present(info);

	if (info->use_typec_extcon && info->limit) {
		/* if use typec extcon notify charger,
		 * wait for BC1.2 detect charger type.
		*/
		while (retry_cnt > 0) {
			if (info->usb_phy->chg_type != UNKNOWN_TYPE) {
				break;
			}
			retry_cnt--;
			msleep(50);
		}
		dev_info(info->dev, "retry_cnt = %d\n", retry_cnt);
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t sgm4154x_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_reg_val);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	ret = sgm4154x_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get SGM4154X_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get SGM4154X_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "SGM4154X_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm4154x_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_reg_val);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sgm4154x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm4154x_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm4154x_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_sel_reg_id);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sgm4154x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm4154x_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM4154X_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm4154x_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sgm4154x_sysfs->name, id);
	return count;
}

static ssize_t sgm4154x_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_sel_reg_id);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sgm4154x_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_lookup_reg);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM4154X_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sgm4154x_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs =
		container_of(attr, struct sgm4154x_charger_sysfs,
			     attr_sgm4154x_dump_reg);
	struct sgm4154x_charger_info *info = sgm4154x_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm4154x_sysfs->info is null\n", __func__);

	sgm4154x_charger_dump_register(info);

	return sprintf(buf, "%s\n", sgm4154x_sysfs->name);
}

static int sgm4154x_register_sysfs(struct sgm4154x_charger_info *info)
{
	struct sgm4154x_charger_sysfs *sgm4154x_sysfs;
	int ret;

	sgm4154x_sysfs = devm_kzalloc(info->dev, sizeof(*sgm4154x_sysfs), GFP_KERNEL);
	if (!sgm4154x_sysfs)
		return -ENOMEM;

	info->sysfs = sgm4154x_sysfs;
	sgm4154x_sysfs->name = "sgm4154x_sysfs";
	sgm4154x_sysfs->info = info;
	sgm4154x_sysfs->attrs[0] = &sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr;
	sgm4154x_sysfs->attrs[1] = &sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr;
	sgm4154x_sysfs->attrs[2] = &sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr;
	sgm4154x_sysfs->attrs[3] = &sgm4154x_sysfs->attr_sgm4154x_reg_val.attr;
	sgm4154x_sysfs->attrs[4] = NULL;
	sgm4154x_sysfs->attr_g.name = "debug";
	sgm4154x_sysfs->attr_g.attrs = sgm4154x_sysfs->attrs;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr);
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr.name = "sgm4154x_dump_reg";
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.attr.mode = 0444;
	sgm4154x_sysfs->attr_sgm4154x_dump_reg.show = sgm4154x_dump_reg_show;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr);
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr.name = "sgm4154x_lookup_reg";
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.attr.mode = 0444;
	sgm4154x_sysfs->attr_sgm4154x_lookup_reg.show = sgm4154x_reg_table_show;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr);
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr.name = "sgm4154x_sel_reg_id";
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.attr.mode = 0644;
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.show = sgm4154x_reg_id_show;
	sgm4154x_sysfs->attr_sgm4154x_sel_reg_id.store = sgm4154x_reg_id_store;

	sysfs_attr_init(&sgm4154x_sysfs->attr_sgm4154x_reg_val.attr);
	sgm4154x_sysfs->attr_sgm4154x_reg_val.attr.name = "sgm4154x_reg_val";
	sgm4154x_sysfs->attr_sgm4154x_reg_val.attr.mode = 0644;
	sgm4154x_sysfs->attr_sgm4154x_reg_val.show = sgm4154x_reg_val_show;
	sgm4154x_sysfs->attr_sgm4154x_reg_val.store = sgm4154x_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sgm4154x_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
static void sgm4154x_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sgm4154x_charger_info *info =
	 container_of(dwork, struct sgm4154x_charger_info, cur_work);
	u32 limit_cur = 0;
	int ret = 0;

	ret = sgm4154x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return;
	}

	if (info->actual_limit_current == limit_cur)
		return;

	ret = sgm4154x_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return;
	}
}
/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
static int sgm4154x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sgm4154x_charger_info *info =
		container_of(nb, struct sgm4154x_charger_info, usb_notify);

	info->limit = limit;

	pm_wakeup_event(info->dev, SGM4154X_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static int sgm4154x_charger_extcon_event(struct notifier_block *nb,
				  unsigned long event, void *param)
{
	struct sgm4154x_charger_info *info =
		container_of(nb, struct sgm4154x_charger_info, extcon_nb);
	int state = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	state = extcon_get_state(info->edev, EXTCON_SINK);
	if (state < 0) {
		return NOTIFY_OK;
	}

	if (info->is_sink == state) {
		return NOTIFY_OK;
	}

	info->is_sink = state;

	if (info->is_sink) {
		info->limit = 500;
	} else {
		info->limit = 0;
	}

	pm_wakeup_event(info->dev, SGM4154X_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
static int sgm4154x_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sgm4154x_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	if (!info)
		return -ENOMEM;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = sgm4154x_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = sgm4154x_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sgm4154x_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

/* Tab A7 Lite T618 code for SR-AX6189A-01-279 by shixuanxuan at 20211231 start */
#if !defined(HQ_FACTORY_BUILD)
		case FLOAT_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_FLOAT;
			break;
#endif
/* Tab A7 Lite T618 code for SR-AX6189A-01-279 by shixuanxuan at 20211231 end */

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get sgm4154x charge status failed\n");
			goto out;
		}

		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		ret = sgm4154x_get_hiz_mode(info, &enabled);
		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_DUMP_CHARGER_IC:
		sgm4154x_charger_dump_register(info);
		break;
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
	case POWER_SUPPLY_PROP_CHARGE_DONE:
		sgm4154x_charger_get_charge_done(info, val);
		break;
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4154x_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sgm4154x_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info)
		return -ENOMEM;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm4154x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm4154x_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
		schedule_delayed_work(&info->cur_work, SGM4154X_CURRENT_WORK_MS*5);
		/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm4154x_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = sgm4154x_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sgm4154x_charger_set_termina_vol(info, val->intval / 1000);
/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 start */
#ifdef HQ_D85_BUILD
		if (ret < 0) {
			dev_err(info->dev, "failed to set terminate voltage\n");
		} else {
			ret = sgm4154x_charger_set_vindpm(info, 4300);
			if (ret < 0) {
				dev_err(info->dev, "failed to set vindpm\n");
			}
		}
#else
		if (ret < 0) {
			dev_err(info->dev, "failed to set terminate voltage\n");
		}
#endif
/* Tab A8 code for AX6300DEV-1961 by qiaodan at 20211028 end */
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			ret = sgm4154x_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sgm4154x_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		if(val->intval) {
			sgm4154x_exit_hiz_mode(info);
		} else {
			sgm4154x_enter_hiz_mode(info);
		}
		break;
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
		if(val->intval)
			ret = sgm4154x_charger_en_chg_timer(info, true);
		else
			ret = sgm4154x_charger_en_chg_timer(info, false);
		break;
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4154x_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 start */
	case POWER_SUPPLY_PROP_EN_CHG_TIMER:
	/* Tab A8 code for AX6300DEV-147 by shixuanxuan at 20210906 end */
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_POWER_PATH_ENABLED:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sgm4154x_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
/* Tab A7 Lite T618 code for SR-AX6189A-01-279 by shixuanxuan at 20211231 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_USB_TYPE_FLOAT,
#endif
/* Tab A7 Lite T618 code for SR-AX6189A-01-279 by shixuanxuan at 20211231 end */
};

static enum power_supply_property sgm4154x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
	POWER_SUPPLY_PROP_POWER_PATH_ENABLED,
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 start */
	POWER_SUPPLY_PROP_CHARGE_DONE,
	/* Tab A8 code for AX6300DEV-44 by qiaodan at 20210816 end */
};

static const struct power_supply_desc sgm4154x_charger_desc = {
	.name			= "sgm4154x_charger",
	/* Tab A8 code for P211115-05893 by wenyaqi at 20211116 start */
	// .type			= POWER_SUPPLY_TYPE_USB,
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	/* Tab A8 code for P211115-05893 by wenyaqi at 20211116 end */
	.properties		= sgm4154x_usb_props,
	.num_properties		= ARRAY_SIZE(sgm4154x_usb_props),
	.get_property		= sgm4154x_charger_usb_get_property,
	.set_property		= sgm4154x_charger_usb_set_property,
	.property_is_writeable	= sgm4154x_charger_property_is_writeable,
	.usb_types		= sgm4154x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sgm4154x_charger_usb_types),
};
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static void sgm4154x_charger_detect_status(struct sgm4154x_charger_info *info)
{
	int state = 0;

	if (info->use_typec_extcon) {
		state = extcon_get_state(info->edev, EXTCON_SINK);
		if (state < 0) {
			return;
		}
		if (state == 0) {
			return;
		}
		info->is_sink = state;

		if (info->is_sink) {
			info->limit = 500;
		}

		schedule_work(&info->work);
	} else {
		unsigned int min, max;

		/*
		* If the USB charger status has been USB_CHARGER_PRESENT before
		* registering the notifier, we should start to charge with getting
		* the charge current.
		*/
		if (info->usb_phy->chg_state != USB_CHARGER_PRESENT) {
			return;
		}

		usb_phy_get_charger_current(info->usb_phy, &min, &max);
		info->limit = min;

		/*
		* slave no need to start charge when vbus change.
		* due to charging in shut down will check each psy
		* whether online or not, so let info->limit = min.
		*/
		schedule_work(&info->work);
	}
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
static void sgm4154x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4154x_charger_info *info = container_of(dwork,
							 struct sgm4154x_charger_info,
							 wdt_work);
	int ret;

	ret = sgm4154x_update_bits(info, SGM4154X_REG_01, REG01_WDT_MASK,
				  WDT_RESET << REG01_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm4154x failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sgm4154x_charger_check_otg_valid(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sgm4154x_read(info, SGM4154X_REG_01, &value);
	if (ret) {
		dev_err(info->dev, "get sgm4154x charger otg valid status failed\n");
		return status;
	}

	if (value & REG01_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1= 0x%x\n", value);

	return status;
}

static bool sgm4154x_charger_check_otg_fault(struct sgm4154x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = sgm4154x_read(info, SGM4154X_REG_09, &value);
	if (ret) {
		dev_err(info->dev, "get SGM4154X charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG09_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_09 = 0x%x\n",
			value);

	return status;
}

static void sgm4154x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm4154x_charger_info *info = container_of(dwork,
			struct sgm4154x_charger_info, otg_work);
	bool otg_valid = sgm4154x_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm4154x_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm4154x_update_bits(info, SGM4154X_REG_01,
						  REG01_OTG_CONFIG_MASK,
						  OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sgm4154x charger otg failed\n");
		}

		otg_valid = sgm4154x_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM4154X_OTG_RETRY_TIMES);

	if (retry >= SGM4154X_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm4154x_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
		if (ret) {
			dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
			goto out ;
		}
	}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	ret = sgm4154x_update_bits(info, SGM4154X_REG_01, REG01_OTG_CONFIG_MASK,
				  OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);

	if (ret) {
		dev_err(info->dev, "enable sgm4154x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		goto out ;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM4154X_WDT_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM4154X_OTG_VALID_MS));
out:
	mutex_unlock(&info->lock);

	return 0;
}

static int sgm4154x_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
	int ret=0;

	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

 	info->otg_enable = false;
 	cancel_delayed_work_sync(&info->wdt_work);
 	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm4154x_update_bits(info, SGM4154X_REG_01,
 				  REG01_OTG_CONFIG_MASK, OTG_DISABLE);
 	if (ret) {
		dev_err(info->dev, "disable sgm4154x otg failed\n");
		goto out;
 	}

 	/* Enable charger detection function to identify the charger type */
	/* Tab A7 Lite T618 code for AX6300DEV-109|AX6189DEV-126 by shixuanxuan at 20220113 start */
	if (!info->use_typec_extcon) {
		ret =  regmap_update_bits(info->pmic, info->charger_detect,
					BIT_DP_DM_BC_ENB, 0);
		if (ret) {
			dev_err(info->dev, "enable BC1.2 failed\n");
		}
	}
	/* Tab A7 Lite T618 code for AX6300DEV-109|AX6189DEV-126 by shixuanxuan at 20220113 end */

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sgm4154x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm4154x_charger_info *info = rdev_get_drvdata(dev);
 	int ret;
 	u8 val;
 
	dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	mutex_lock(&info->lock);
	ret = sgm4154x_read(info, SGM4154X_REG_01, &val);
 	if (ret) {
		dev_err(info->dev, "failed to get sgm4154x otg status\n");
		mutex_unlock(&info->lock);
 		return ret;
 	}

 	val &= REG01_OTG_CONFIG_MASK;

	dev_info(info->dev, "%s:line%d val = %d\n", __func__, __LINE__, val);

	mutex_unlock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */
	return val;
}

static const struct regulator_ops sgm4154x_charger_vbus_ops = {
	.enable = sgm4154x_charger_enable_otg,
	.disable = sgm4154x_charger_disable_otg,
	.is_enabled = sgm4154x_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm4154x_charger_vbus_desc = {
	.name = "sgm4154x_otg_vbus",
	.of_match = "sgm4154x_otg_vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm4154x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm4154x_charger_register_vbus_regulator(struct sgm4154x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm4154x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int sgm4154x_charger_register_vbus_regulator(struct sgm4154x_charger_info *info)
{
	return 0;
}
#endif

static int sgm4154x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm4154x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	/* Tab A8 code for AX6300DEV-3409 by qiaodan at 20211124 start */
	int retry_cnt = 3;
	/* Tab A8 code for AX6300DEV-3409 by qiaodan at 20211124 end */
	if (!adapter) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->dev = dev;

	ret = sgm4154x_charger_get_pn_value(info);
	if (ret) {
		dev_err(dev, "failed to get sgm pn value \n");
		return ret;
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	info->use_typec_extcon =
		device_property_read_bool(dev, "use-typec-extcon");
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	INIT_WORK(&info->work, sgm4154x_charger_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	INIT_DELAYED_WORK(&info->cur_work, sgm4154x_current_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = sgm4154x_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	if (info->use_typec_extcon) {
		info->extcon_nb.notifier_call = sgm4154x_charger_extcon_event;
		ret = devm_extcon_register_notifier_all(dev,
							info->edev,
							&info->extcon_nb);
		if (ret) {
			dev_err(dev, "Can't register extcon\n");
			return ret;
		}
 	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = SGM4154X_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = SGM4154X_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = SGM4154X_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sgm4154x_charger_desc,
						   &charger_cfg);

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	mutex_init(&info->lock);
	mutex_lock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}
	/* Tab A8 code for AX6300DEV-3409 by qiaodan at 20211124 start */
	do {
		ret = sgm4154x_charger_hw_init(info);
		if (ret) {
			dev_err(dev, "try to sgm4154x_charger_hw_init fail:%d ,ret = %d\n" ,retry_cnt ,ret);
		}
		retry_cnt--;
	} while ((retry_cnt != 0) && (ret != 0));
	/* Tab A8 code for AX6300DEV-3409 by qiaodan at 20211124 end */
	if (ret) {
		dev_err(dev, "failed to sgm4154x_charger_hw_init\n");
		goto err_mutex_lock;
	}

	sgm4154x_charger_stop_charge(info);

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
	ret = sgm4154x_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto err_psy_usb;
	}
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */

	device_init_wakeup(info->dev, true);
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	if (!info->use_typec_extcon) {
		info->usb_notify.notifier_call = sgm4154x_charger_usb_change;
		ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
		if (ret) {
			dev_err(dev, "failed to register notifier:%d\n", ret);
			goto err_psy_usb;
		}
 	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */

	ret = sgm4154x_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	sgm4154x_charger_detect_status(info);

	INIT_DELAYED_WORK(&info->otg_work, sgm4154x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,sgm4154x_charger_feed_watchdog_work);

	pr_info("[%s]line=%d: probe success\n", __FUNCTION__, __LINE__);

	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 start */
        mutex_unlock(&info->lock);
	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_unlock(&info->lock);
	/* Tab A8 code for AX6300DEV-157 by zhangyanlong at 20210910 end */
	mutex_destroy(&info->lock);
	return ret;
}

static void sgm4154x_charger_shutdown(struct i2c_client *client)
{
	struct sgm4154x_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	cancel_delayed_work_sync(&info->cur_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm4154x_update_bits(info, SGM4154X_REG_01,
					  REG01_OTG_CONFIG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sgm4154x otg failed ret = %d\n", ret);
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
		/* Enable charger detection function to identify the charger type */
		if (!info->use_typec_extcon) {
			ret = regmap_update_bits(info->pmic, info->charger_detect,
						BIT_DP_DM_BC_ENB, 0);
			if (ret) {
				dev_err(info->dev,
					"enable charger detection function failed ret = %d\n", ret);
			}
		}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	}
}

static int sgm4154x_charger_remove(struct i2c_client *client)
{
	struct sgm4154x_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sgm4154x_charger_suspend(struct device *dev)
{
	struct sgm4154x_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SGM4154X_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	cancel_delayed_work_sync(&info->cur_work);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	/* feed watchdog first before suspend */
	ret = sgm4154x_update_bits(info, SGM4154X_REG_01, REG01_WDT_MASK,
				  WDT_RESET << REG01_WDT_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sgm4154x failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int sgm4154x_charger_resume(struct device *dev)
{
	struct sgm4154x_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = sgm4154x_update_bits(info, SGM4154X_REG_01, REG01_WDT_MASK,
				  WDT_RESET << REG01_WDT_SHIFT);
	if (ret)
		dev_warn(info->dev, "reset sgm4154x failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 start */
	schedule_delayed_work(&info->cur_work, 0);
	/* Tab A8 code for AX6300DEV-103 by qiaodan at 20210827 end */

	return 0;
}
#endif

static const struct dev_pm_ops sgm4154x_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm4154x_charger_suspend,
				sgm4154x_charger_resume)
};

static const struct i2c_device_id sgm4154x_i2c_id[] = {
	{"sgm4154x_chg", 0},
	{}
};

static const struct of_device_id sgm4154x_charger_of_match[] = {
	{ .compatible = "sgm,sgm4154x_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm4154x_charger_of_match);

static struct i2c_driver sgm4154x_charger_driver = {
 	.driver = {
 		.name = "sgm4154x_chg",
		.of_match_table = sgm4154x_charger_of_match,
		.pm = &sgm4154x_charger_pm_ops,
 	},
	.probe = sgm4154x_charger_probe,
	.shutdown = sgm4154x_charger_shutdown,
	.remove = sgm4154x_charger_remove,
	.id_table = sgm4154x_i2c_id,
 };

module_i2c_driver(sgm4154x_charger_driver);

MODULE_AUTHOR("Qiaodan <Qiaodan5@huaqin.com>");
MODULE_DESCRIPTION("SGM4154X Charger Driver");
MODULE_LICENSE("GPL");

