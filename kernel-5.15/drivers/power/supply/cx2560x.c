#define pr_fmt(fmt)	"[cx2560x]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/iio/consumer.h>
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

#include <linux/power_supply.h>
#include <charger_class.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/usb/ch9.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/gadget.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <../../usb/mtu3/mtu3.h>

#include "cx2560x.h"
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/hardware_info.h>
#include <linux/pm_wakeup.h>
#include "mtk_charger.h"

/* +S98901AA1,wt add macro definition: CV voltage and Iterm current value */
#define CV_VREG_VALUE           4448
#define ITERM_CURRENT_VALUE     300
/* +S98901AA1,wt add macro definition: CV voltage and Iterm current value*/

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

enum cx2560x_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

bool is_cx2560x = false;
bool is_upm6922 = false;
bool is_upm6922P = false;


bool is_2rd_primary_chg_probe_ok = false;
EXPORT_SYMBOL(is_2rd_primary_chg_probe_ok);

enum {
	PN_CX25601D,
};

enum cx2560x_part_no {
	SGM41513D = 0x01,
	UPM6910D = 0x02,
};

static int pn_data[] = {
	[PN_CX25601D] = 0x02,
};


#define BC12_DONE_TIMEOUT_CHECK_MAX_RETRY	10
static int no_usb_flag = 0;
static bool usb_detect_flag = false;
bool charge_done_flag = false;
static struct charger_device *s_chg_dev_otg;

static enum power_supply_property  cx2560x_charger_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
};

static enum power_supply_usb_type cx2560x_charger_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_DCP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_C,
    POWER_SUPPLY_USB_TYPE_PD,
    POWER_SUPPLY_USB_TYPE_PD_DRP,
    POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
};
/*
static enum power_supply_property cx2560x_usb_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property cx2560x_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};
*/

struct cx2560x {
	struct device *dev;
	struct i2c_client *client;

	enum cx2560x_part_no part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	int status;
	int irq;
	u32 intr_gpio;

#ifdef WT_COMPILE_FACTORY_VERSION
	int probe_flag;
#endif
	struct mutex i2c_rw_lock;
	struct mutex i2c_up_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool hiz_online;
	bool vbus_gd;
	bool is_hiz;
	bool otg_flag;
	int prev_hiz_type;
	int hiz_chg_stat;
	bool online;
	int current_max;
	int voltage_max;
	int vbat;
	int ibat;
	int vreg;
	bool iindpm_stat;
	bool vindpm_stat;
	int chg_done_stat;
	int chg_done_data[4][5];
	int reboot_flag;
	int reboot_float_filter;
	int bootmode;
	int boottype;
	int offset_flag;
	int offset_reset;
/* +S98901AA1,wt add macro definition: CV voltage and Iterm current value */
	bool hub_check_chg;
/* -S98901AA1,wt add macro definition: CV voltage and Iterm current value */

	struct cx2560x_state state;
	struct cx2560x_platform_data *platform_data;
	struct charger_device *chg_dev;
	struct regulator_dev *otg_rdev;
	struct phy *usb_phy;
	struct usb_phy *usb2_phy;
	struct usb_gadget *gadget;
	struct device_node *usb_node;
	struct ssusb_mtk *ssusb;
	struct device_node *boot_node;


	struct power_supply *psy;
	struct power_supply *chg_psy;
	struct power_supply *bat_psy;
	struct power_supply *usb_psy;
	struct power_supply *otg_psy;
	struct power_supply *ac_psy;
	struct power_supply_desc psy_desc;

	struct delayed_work psy_dwork;
	struct delayed_work prob_dwork;

	struct delayed_work charge_detect_delayed_work;
	struct delayed_work charge_usb_detect_work;
	struct delayed_work charge_done_detect_work;
	struct delayed_work recharge_detect_work;
	struct delayed_work set_hiz_work;
	struct delayed_work irq_work;
	struct delayed_work dump_work;
	struct delayed_work exit_hiz_work;
	struct work_struct usb_work;
	struct wakeup_source *charger_wakelock;

	struct iio_channel	**pmic_ext_iio_chans;
	int psy_usb_type;
	int chg_type;
	int chg_old_status;
/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged*/
	int recharge_val;
};

static void get_power_off_charging(struct cx2560x *cx);
int cx2560x_set_boost_current(struct cx2560x *cx, int curr);
static int cx2560x_enable_bc12(struct cx2560x *cx);
static void cx2560x_dump_regs(struct cx2560x *cx);
static int cx2560x_get_status(struct cx2560x *cx, struct cx2560x_state *state);
static int cx2560x_set_dp(struct cx2560x *cx, u8 dp_dac);
static void cx2560x_set_dpdm_hiz(struct cx2560x *cx);

//static int cx2560x_set_dm(struct cx2560x *cx, u8 dm_dac);
static int cx2560x_set_usbsw_state(struct cx2560x *cx, enum cx2560x_usbsw state);
static irqreturn_t cx2560x_irq_handler(int irq, void *data);
static int cx2560x_set_shipmode_delay(struct charger_device *chg_dev, bool en);

static int  cx2560x_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		return 1;
	//case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		//return 1;
	default:
		return 0;
	}
}


static const struct charger_properties cx2560x_chg_props = {
	.alias_name = "cx2560x",
};

static int cx2560x_read_byte_without_retry(struct cx2560x *cx, u8 reg, u8 *data)
{
	s32 ret;
	mutex_lock(&cx->i2c_rw_lock);
	ret = i2c_smbus_read_byte_data(cx->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		goto out;
	}

	*data = (u8) ret;
	ret = 0;
out:
	mutex_unlock(&cx->i2c_rw_lock);
	return ret;
}

static int cx2560x_write_byte_without_retry(struct cx2560x *cx, u8 reg, u8 val)
{
	s32 ret;
	mutex_lock(&cx->i2c_rw_lock);
	ret = i2c_smbus_write_byte_data(cx->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		goto out;
	}
	ret = 0;
out:
	mutex_unlock(&cx->i2c_rw_lock);
	return ret;
}

static int cx2560x_read_byte(struct cx2560x *cx, u8 reg, u8 *data)
{
	int ret;
	u8 i,retry_count,read_val;
	u8 tmp_val[2];

	for(retry_count = 0; retry_count<3; retry_count++)
	{
		for(i=0;i<2;i++)
		{
			ret = cx2560x_read_byte_without_retry(cx,reg,&read_val);
			if(ret)
				break;
			else
				tmp_val[i] = read_val;
		}
		if((!ret) && tmp_val[0] == tmp_val[1])
		{
			*data = tmp_val[0];
			return 0;
		}
	}
    pr_err("i2c retry read fail: can't read from reg 0x%02X\n", reg);
	return -1;
}

static int cx2560x_write_byte(struct cx2560x *cx, u8 reg, u8 data)
{
	int ret;
	u8 read_val = 0,retry_count;
	for(retry_count = 0;retry_count<10;retry_count++)
	{
		ret = cx2560x_write_byte_without_retry(cx,reg,data);
		if(!ret)
		{
			ret = cx2560x_read_byte_without_retry(cx,reg,&read_val);
			if(data == read_val)
			{
				break;
			}
			if(retry_count == 9)
			{
				pr_err("%s: fail: can't write %02x to reg=%02X, ret=%d\n",__func__,data, reg, ret);
				return -1;
			}
		}
	}
	return 0;
}

static int cx2560x_update_bits(struct cx2560x *cx, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;
	mutex_lock(&cx->i2c_up_lock);
	ret = cx2560x_read_byte(cx, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

/* +S98901AA1 liangjianfeng wt, modify, 20240724, modify for shipmode enter*/
	if( (reg==0x01&&mask==0x40) || (reg==0x07&&mask==0x80) ||
		(reg==0x0B&&mask==0x80) || (reg==0x0D&&mask==0x80) ||
		(reg==0x07&&mask==0x20) || (reg==0x07&&mask==0x08)) {
		ret = cx2560x_write_byte_without_retry(cx, reg, tmp);
	} else {
		ret = cx2560x_write_byte(cx, reg, tmp);
	}
/* -S98901AA1 liangjianfeng wt, modify, 20240724, modify for shipmode enter*/

	if(ret)
		pr_err("%s: fail: reg %02x data=%02X, ret=%d\n",__func__,reg, data, ret);

out:
	mutex_unlock(&cx->i2c_up_lock);
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

			cx2560x_read_byte_without_retry(cx, 0x40, &read_val);
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



static int cx2560x_enable_otg(struct cx2560x *cx)
{
	u8 tmp_val;
	int ret;
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;
	cx2560x_set_usbsw_state(cx, USBSW_USB);
	cx2560x_read_byte_without_retry(cx, CX2560X_REG_09, &tmp_val);
	//set boost current limit 1875ma
	ret =  cx2560x_update_bits(cx, 0x0C, 0x0E, 0x06<<1);
	if(ret)
		pr_err("set iboost limit 1200mA failed\n");

	ret=cx2560x_write_reg40(cx, 1);
	if(ret != 0)
		pr_err("otg write reg40 successfully; \n");

	ret = cx2560x_write_byte(cx,0x83,0x03);
	ret = cx2560x_write_byte(cx,0x41,0xA8);
	// ret = cx2560x_update_bits(cx, 0x84, 0x80, 0x80);
	ret = cx2560x_update_bits(cx, CX2560X_REG_01,REG01_OTG_CONFIG_MASK,val);
	msleep(100);
	cx2560x_write_byte(cx,0x83,0x01);
	msleep(10);
	cx2560x_write_byte(cx,0x41,0x28);
	cx2560x_write_reg40(cx, 0);

	return ret;

}

static int cx2560x_disable_otg(struct cx2560x *cx)
{
	int ret;
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;
	ret = cx2560x_update_bits(cx, CX2560X_REG_01, REG01_OTG_CONFIG_MASK,val);
	cx2560x_write_reg40(cx, 1);
	cx2560x_write_byte(cx,0x83,0x00);
	ret = cx2560x_update_bits(cx, 0x84, 0x80, 0x00);
	cx2560x_write_reg40(cx, 0);

	return ret; 

}

static int cx2560x_enable_charger(struct cx2560x *cx)
{
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;
	return cx2560x_update_bits(cx, CX2560X_REG_01, REG01_CHG_CONFIG_MASK, val);
}

static int cx2560x_disable_charger(struct cx2560x *cx)
{
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;
	return cx2560x_update_bits(cx, CX2560X_REG_01, REG01_CHG_CONFIG_MASK, val);
}



int cx2560x_set_chargecurrent(struct cx2560x *cx, int curr)
{
	u8 ichg;
	if (curr > 30475/10)
		curr = 30475/10;
	
	if(curr <= 1170)
		ichg = curr/90;
	else
		ichg = (curr-805)/(575/10)+14;

	pr_err("%s set ichg %d\n",__func__ ,curr);
	return cx2560x_update_bits(cx, CX2560X_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);
}

int cx2560x_set_term_current(struct cx2560x *cx, int curr)
{
	u8 iterm;
	pr_err("%s set iterm %d\n", __func__,curr);
	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;
	if(curr > REG03_ITERM_MAX)
		curr = REG03_ITERM_MAX;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;
	return cx2560x_update_bits(cx, CX2560X_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(cx2560x_set_term_current);

struct iio_channel ** cx2560x_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool cx2560x_is_pmic_chan_valid(struct cx2560x *cx,
		enum pmic_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!cx->pmic_ext_iio_chans) {
		iio_list = cx2560x_get_ext_channels(cx->dev, pmic_iio_chan_name,
		ARRAY_SIZE(pmic_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(cx->dev, "Failed to get channels, %d\n",
					rc);
				cx->pmic_ext_iio_chans = NULL;
			}
			return false;
		}
		cx->pmic_ext_iio_chans = iio_list;
	}

	return true;
}

int cx2560x_get_vbus_value(struct cx2560x *cx,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!cx2560x_is_pmic_chan_valid(cx, VBUS_VOLTAGE)) {
		dev_err(cx->dev,"read vbus_dect channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = cx->pmic_ext_iio_chans[VBUS_VOLTAGE];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(cx->dev,
		"read vbus_dect channel fail, ret=%d\n", ret);
		return ret;
	}
	//*val = temp / 100;
	*val = temp * 369 / 39;
	dev_err(cx->dev,"%s: vbus_volt = %d \n", __func__, *val);

	return ret;
}

static int cx2560x_get_vbus(struct charger_device *chg_dev,  u32 *vbus)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	int val;
	int ret;

	ret = cx2560x_get_vbus_value(cx, &val);
	if (ret < 0) {
		pr_err("cx2560x_get_vbus error \n");
		val = 0;
	}
	*vbus = val * 1000;
	pr_err(" cx_get_vbus :%d \n", val);

	return val;
}

enum charg_stat cx2560x_get_charging_status(struct cx2560x *cx)
{
	enum charg_stat status = CHRG_STAT_NOT_CHARGING;
	int ret = 0;
	u8 reg_val = 0;

	ret = cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
	if (ret) {
		pr_err("read CX2560X_REG_08 failed, ret:%d\n", ret);
		return ret;
	}

	status = (reg_val & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;
	if(charge_done_flag == true)
		status = CHRG_STAT_CHARGING_TERM;

	return status;
}

int cx2560x_set_prechg_current(struct cx2560x *cx, int curr)
{
	u8 iprechg;
	pr_err("%s set iprechg = %d\n", __func__,curr);
	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;
	else if(curr >REG03_IPRECHG_MAX)
		curr = REG03_IPRECHG_MAX;

	iprechg = curr/REG03_IPRECHG_LSB - 1;

	return cx2560x_update_bits(cx, CX2560X_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(cx2560x_set_prechg_current);


int cx2560x_set_chargevolt(struct cx2560x *cx, int volt)
{
	u8 reg_val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;
	else if(volt > REG04_VREG_MAX)
		volt = REG04_VREG_MAX;
	
	reg_val = (volt-REG04_VREG_BASE)/REG04_VREG_LSB;
	reg_val <<= REG04_VREG_SHIFT;

	return cx2560x_update_bits(cx, CX2560X_REG_04, REG04_VREG_MASK,reg_val);
}

/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged start*/
int cx2560x_set_rechargeth(struct cx2560x *cx, int rechargeth)
{
	u8 val;

	if (rechargeth <= 100) {
		val = REG04_VRECHG_100MV << REG04_VRECHG_SHIFT;
		cx->recharge_val = 100;
	} else {
		val = REG04_VRECHG_200MV << REG04_VRECHG_SHIFT;
		cx->recharge_val = 200;
	}

	return cx2560x_update_bits(cx, CX2560X_REG_04, REG04_VRECHG_MASK, val);
}

static int cx2560x_set_recharge(struct charger_device *chg_dev, u32 volt)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

	pr_err("recharge = %d\n", volt);
	return cx2560x_set_rechargeth(cx, volt / 1000);
}
/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged end*/

int cx2560x_set_input_volt_limit(struct cx2560x *cx, int volt)
{
	u8 val;
	if (volt < REG11_VINDPM_BASE)
		volt = REG11_VINDPM_BASE;
	else if(volt > REG11_VINDPM_MAX)
		volt = REG11_VINDPM_MAX;

	val = (volt - REG11_VINDPM_BASE) / REG11_VINDPM_LSB;
	return cx2560x_update_bits(cx, CX2560X_REG_11, REG11_VINDPM_MASK,
				   val << REG11_VINDPM_SHIFT);
}

static int cx2560x_get_input_current_limit(struct cx2560x *cx)
{
	u8 val = 0;
	int ret = 0;
	int iindpm = 0;

	ret = cx2560x_read_byte(cx, CX2560X_REG_00, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", CX2560X_REG_00, val);
	}
	val &= REG00_IINLIM_MASK;
	iindpm = (val*REG00_IINLIM_LSB)+REG00_IINLIM_BASE;
	return iindpm;
}


int cx2560x_set_input_current_limit(struct cx2560x *cx, int curr)
{
	u8 val;
	pr_err("%s: set iindpm = %d\n",__func__ ,curr);
	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;
	else if(curr > REG00_IINLIM_MAX)
		curr = REG00_IINLIM_MAX;
/* +S98901AA1; set upward value when curr not divisible by step(100) */
	if ((curr - REG00_IINLIM_BASE) % REG00_IINLIM_LSB)
		val = ((curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB) + 1;
	else
		val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
/* +S98901AA1; set upward value when curr not divisible by step(100) */
	return cx2560x_update_bits(cx, CX2560X_REG_00, REG00_IINLIM_MASK,val);
}

int cx2560x_set_watchdog_timer(struct cx2560x *cx, u8 timeout)
{
	u8 temp;
	int ret;

	temp = (u8) (((timeout -
		       REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	ret = cx2560x_update_bits(cx, CX2560X_REG_05, REG05_WDT_MASK, temp);
	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_set_watchdog_timer);

int cx2560x_disable_watchdog_timer(struct cx2560x *cx)
{
	int ret;
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;
	ret =  cx2560x_update_bits(cx, CX2560X_REG_05, REG05_WDT_MASK, val);
	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	return ret;
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
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;
	pr_err("%s\n",__func__);
	return cx2560x_update_bits(cx, CX2560X_REG_0B, REG0B_REG_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(cx2560x_reset_chip);

int cx2560x_enter_hiz_mode(struct cx2560x *cx)
{
	int ret = 0;
	pr_err("%s:enter hiz\n",__func__);
	schedule_delayed_work(&cx->set_hiz_work, 0);
	return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_enter_hiz_mode);

int cx2560x_exit_hiz_mode(struct cx2560x *cx)
{
	int ret = 0, hiz_state = 0;
	u8 regval;
	int vbus = 0;
	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	cx2560x_read_byte(cx, CX2560X_REG_00, &regval);
	hiz_state = !!(regval & REG00_ENHIZ_MASK);
	cx2560x_set_dpdm_hiz(cx);
	ret = cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, val);
	if (ret) {
		pr_err("%s:fail\n", __func__);
		return ret;
	}
	cx->is_hiz = false;
	cx->hiz_online = 0;
	cx2560x_get_vbus_value(cx, &vbus);
	if (hiz_state == 1 && vbus < 3900) {
		cx2560x_irq_handler(cx->irq, (void *)cx);
	} else if (hiz_state == 1 && vbus >= 3900) {
		cancel_delayed_work_sync(&cx->exit_hiz_work);
		schedule_delayed_work(&cx->exit_hiz_work, msecs_to_jiffies(500));
	}
	return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_exit_hiz_mode);

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
//EXPORT_SYMBOL_GPL(cx2560x_set_hiz_mode);


int cx2560x_enable_term(struct cx2560x *cx, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TERM_MASK, val);
	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(cx2560x_enable_term);

int cx2560x_set_boost_current(struct cx2560x *cx, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr >= BOOSTI_1200) 
		val = REG02_BOOST_LIM_1P2A;

	return cx2560x_update_bits(cx, CX2560X_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int cx2560x_set_boost_voltage(struct cx2560x *cx, int volt)
{
	u8 val;

	if (volt == BOOSTV_4870)
		val = REG06_BOOSTV_4P87V;
	else if (volt == BOOSTV_4998)
		val = REG06_BOOSTV_4P998V;
	else if (volt == BOOSTV_5126)
		val = REG06_BOOSTV_5P126V;
	else if (volt == BOOSTV_5254)
		val = REG06_BOOSTV_5P254V;
	else
		val = REG06_BOOSTV_5P126V;


	return cx2560x_update_bits(cx, CX2560X_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}
//EXPORT_SYMBOL_GPL(cx2560x_set_boost_voltage);

static int cx2560x_set_acovp_threshold(struct cx2560x *cx, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return cx2560x_update_bits(cx, CX2560X_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
//EXPORT_SYMBOL_GPL(cx2560x_set_acovp_threshold);



static int cx2560x_get_status(struct cx2560x *cx, struct cx2560x_state *state)
{
	int ret;
	u8 status = 0, fault = 0, ctrl = 0;

	ret = cx2560x_read_byte(cx, CX2560X_REG_08, &status);
	if (ret) {
		dev_err(cx->dev, "read cx2560x 0x08 register failed");
		return -EINVAL;
	}
/*+S98901AA1.wt,MODIFY,20240711, modify Read REG_09 on OTG */
	if(cx->otg_flag){
		ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_09, &fault);
		if (ret) {
			dev_err(cx->dev, "read cx2560x 0x09 register failed");
			return -EINVAL;
		}
	}
/*+S98901AA1.wt,MODIFY,20240628, modify Read REG_09 on OTG*/
	ret = cx2560x_read_byte(cx, CX2560X_REG_0A, &ctrl);
	if (ret) {
		dev_err(cx->dev, "read cx2560x 0x0A register failed");
		return -EINVAL;
	}

	state->pg_stat = (status & REG08_PG_STAT_MASK) >> 2;
	state->chrg_stat = (status & REG08_CHRG_STAT_MASK) >> 3;
	state->vbus_stat = (status & REG08_VBUS_STAT_MASK) >> 5;
	state->online = !!(status & REG08_PG_STAT_MASK);
	state->vbus_gd = !!(ctrl & REG0A_VBUS_GD_MASK);
	if(cx->otg_flag){
		state->boost_fault = (fault&REG09_FAULT_BOOST_MASK);
	}
	if (state->vbus_gd) {
		dev_err(cx->dev, "vbus_gd = 1");
	} else {
		dev_err(cx->dev, "vbus_gd = 0");
	}

	dev_info(cx->dev, "[08]: %#x, [09]: %#x, [0A]: %#x",status, fault, ctrl);

	return 0;
}

static int cx2560x_dpdm_detect_is_done(struct cx2560x * cx)
{

	u8 pg_stat;
	int ret;

	if(!cx->is_hiz)
	{
		ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &pg_stat);
		pg_stat = (pg_stat & REG08_PG_STAT_MASK) >> REG08_PG_STAT_SHIFT;
		if (pg_stat == 1)
			return true;
		else
			return false;
	}else{
		return true;
	}
}

static void dump_show_work(struct work_struct *work)
{
	 struct cx2560x *cx = container_of(work, struct cx2560x,
												dump_work.work);
	cx2560x_dump_regs(cx);
	schedule_delayed_work(&cx->dump_work, 3* HZ);
}

static void set_hiz_func(struct work_struct *work)
{
	int ret = 0;
	int retry_cnt = 0;
	int vbus_stat = 0;
	int vbus_val=0;
	int chg_stat = 0;
	u8 reg_val = 0;
	u8 enable_val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	struct cx2560x *cx = container_of(work, struct cx2560x,
											set_hiz_work.work);
	if(!cx->is_hiz)
	{
		cx->is_hiz = true;
		cx2560x_get_vbus_value(cx,&vbus_val);
		if(vbus_val>1000)
			cx->hiz_online = 1;
		do {
			if (!cx2560x_dpdm_detect_is_done(cx)) {
				pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
			} else {
				pr_err("[%s] BC1.2 done\n", __func__);
				break;
			}
			mdelay(60);
		} while(retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

		ret = cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
		if (ret)
			return;

		vbus_stat = (reg_val & REG08_VBUS_STAT_MASK)>>REG08_VBUS_STAT_SHIFT;
		cx->prev_hiz_type = vbus_stat;
		chg_stat =  (reg_val & REG08_CHRG_STAT_MASK)>>REG08_CHRG_STAT_SHIFT;
		cx->hiz_chg_stat = chg_stat;
		pr_err("[%s] cx->prev_hiz_type=%d ,cx->hiz_chg_stat=%d\n", __func__,cx->prev_hiz_type,cx->hiz_chg_stat);
		ret = cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, enable_val);
		if (ret){
			cx->is_hiz = false;
			cx->hiz_online = 0;
			pr_err("[%s] en hiz fail\n", __func__);
		}
	}
	
	return;
}

static void hiz_detect_type_func(struct cx2560x * cx)
{
	u8 reg_val = 0;
	int ret = 0;
	pr_err("%s \n", __func__);
	ret = cx2560x_read_byte(cx, CX2560X_REG_00, &reg_val);
	if (reg_val & 0x80) {
		pr_err("%s detect hiz mode, hiztype=%d\n", __func__,cx->prev_hiz_type);
		switch (cx->prev_hiz_type) {
			case REG08_VBUS_TYPE_NONE:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
				cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
				break;
			case REG08_VBUS_TYPE_USB_SDP:
				cx->chg_type = POWER_SUPPLY_TYPE_USB;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				cx->psy_usb_type =POWER_SUPPLY_USB_TYPE_SDP;
				break;
			case REG08_VBUS_TYPE_USB_CDP:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
				cx->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
				break;
			case REG08_VBUS_TYPE_USB_DCP:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
				cx->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				break;
			case REG08_VBUS_TYPE_UNKNOWN:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
				cx->chg_type = POWER_SUPPLY_TYPE_USB;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
			case REG08_VBUS_TYPE_NON_STANDARD:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
				cx->chg_type = POWER_SUPPLY_TYPE_USB;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
			default:
				cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
				cx->chg_type = POWER_SUPPLY_TYPE_USB;
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
		}
		cancel_delayed_work(&cx->charge_usb_detect_work);

		if (cx->chg_type != POWER_SUPPLY_TYPE_USB_DCP) {
			cx2560x_set_usbsw_state(cx, USBSW_USB);
		}
		// power_supply_changed(cx->chg_psy);
		schedule_delayed_work(&cx->psy_dwork, 0);
	}
	return;
}


static int cx2560x_get_bat_current(struct cx2560x * cx)
{
	int ibat;
	union power_supply_propval psy_prop;
	if(!cx->bat_psy)
	{
		cx->bat_psy = power_supply_get_by_name("battery");
		if(!cx->bat_psy)
		{
			pr_err("get battery power supply failed\n");
			return -1;
		}
	}

	power_supply_get_property(cx->bat_psy,POWER_SUPPLY_PROP_CURRENT_NOW,&psy_prop);
	ibat = psy_prop.intval /1000;

	return ibat;
}

static int cx2560x_get_bat_voltage(struct cx2560x * cx)
{
	int vbat;
	union power_supply_propval psy_prop;
	if(!cx->bat_psy)
	{
		cx->bat_psy = power_supply_get_by_name("battery");
		if(!cx->bat_psy)
		{
			pr_err("get battery power supply failed\n");
			return -1;
		}
	}

	power_supply_get_property(cx->bat_psy,POWER_SUPPLY_PROP_VOLTAGE_NOW,&psy_prop);
	vbat = psy_prop.intval /1000;

	return vbat;
}

static void check_hiz_remove(struct work_struct *work)
{
	struct cx2560x *cx = container_of(work, struct cx2560x,exit_hiz_work.work);
	int vbus = 0;

	cx2560x_get_vbus_value(cx, &vbus);
	cx->ibat = cx2560x_get_bat_current(cx);
	pr_err("%s ,ibat=%d,vbus=%d\n",__func__,cx->ibat,vbus);
	if (vbus < 3900 && cx->ibat < 0) {
		cx2560x_irq_handler(cx->irq, (void *)cx);
	}
}

static int cx2560x_get_vreg(struct cx2560x *cx)
{
	int ret;
	u8 data;
	ret = cx2560x_read_byte(cx, CX2560X_REG_04, &data);
	if (ret) {
		pr_err("%s read REG_04 fail\n",__func__);
		return -1;
	}
	data = (data & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
	return (data * 32 + 3856);
}

static int get_chg_state(struct cx2560x *cx)
{
	u8 data = 0;
	int ret;
	cx2560x_set_term_current(cx,120);
	ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_0A, &data);
	if(ret){
		pr_err("%s read REG_0A fail\n",__func__);
		return -1;
	}
	cx->iindpm_stat = !!(data&REG0A_IINDPM_STAT_MASK);
	cx->vindpm_stat = !!(data&REG0A_VINDPM_STAT_MASK);
	cx->ibat = cx2560x_get_bat_current(cx);
/*+S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
	cx->vbat = cx2560x_get_bat_voltage(cx);
/*-S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
	cx->vreg = cx2560x_get_vreg(cx);
	if(cx->vreg < 0)
		return -1;
	ret = cx2560x_read_byte_without_retry(cx, CX2560X_REG_08, &data);
	if (ret){
		pr_err("%s read REG_08 fail\n",__func__);
		return -1;
	}
	cx->chg_done_stat = (data & REG08_CHRG_STAT_MASK)>>REG08_CHRG_STAT_SHIFT;

	return 0;
}

/* +S98901AA1;improving charger limit current  */
static void dynamic_set_iindpm_offset(struct cx2560x *cx, bool enable)
{
    u8 current_I1 = 0, current_I2 = 0, current_I3 = 0;
    u8 data;
    int ret = 0;
    if (enable)
    {
        if (cx->offset_flag == 0)
        {
            cx->offset_flag = 1;
            cx->offset_reset = 0;
            ret += cx2560x_write_reg40(cx, true);
            ret += cx2560x_write_byte_without_retry(cx, 0x89, 0x03);
            ret += cx2560x_update_bits(cx, 0x86, 0x20, 0x20);
            ret += cx2560x_write_byte_without_retry(cx, 0x83, 0x00);
            ret += cx2560x_read_byte_without_retry(cx, 0x84, &data);
            pr_err(" %s, before: reg84 = %02x\n", __func__, data);
            current_I1 = (data & 0x60) >> 5;
            current_I2 = (data & 0x0C) >> 2;
            current_I3 = (data & 0x03);
            if (current_I2 < 3)
            {
                current_I2++;
            }
            else if (current_I2 == 3 && current_I3 > 0 && current_I1 < 3)
            {
                current_I1++;
                current_I3--;
            }
            data &= 0x90;
            ret += cx2560x_write_byte_without_retry(cx, 0x84, data | current_I1 << 5 | current_I2 << 2 | current_I3);
            ret += cx2560x_read_byte_without_retry(cx, 0x84, &data);
            pr_err("%s ,after :reg84 = %02x\n", __func__, data);
            ret += cx2560x_write_reg40(cx, false);
            if (ret < 1) {
                cx->offset_flag = 0;
                return;
            }
        }
    }
    else
    {
        if (cx->offset_reset == 0)
        {
            cx->offset_flag = 0;
            cx->offset_reset = 1;
            ret += cx2560x_write_reg40(cx, true);
            ret += cx2560x_write_byte_without_retry(cx, 0x89, 0x03);
            ret += cx2560x_read_byte_without_retry(cx, 0x84, &data);
            pr_err("%s , reg84 = %02x\n", __func__, data);
            ret += cx2560x_update_bits(cx, 0x86, 0x20, 0x20);
            ret += cx2560x_write_reg40(cx, false);
            if (ret < 1) {
                cx->offset_reset = 0;
                return;
            }
        }
    }
}

/* +S98901AA1;improving charger limit current  */

static void cx2560x_set_auto_vindpm(struct cx2560x *cx)
{
	int vbus = 0;
	cx2560x_get_vbus_value(cx,&vbus);

/* +S98901AA1,Modify iindpm offset value when vbus is 9V */
	if (vbus > 6900) {
		dynamic_set_iindpm_offset(cx,true);
	} else {
		dynamic_set_iindpm_offset(cx,false);
	}
/* +S98901AA1,Modify iindpm offset value when vbus is 9V */
}

static void init_chg_done_data(int data[4][5])
{
	int i = 0 , j = 0;

	for(i = 0; i < 4; i++)
	{
		for(j = 0; j < 5; j++)
		{
			if(i==0)
				data[i][j]=CV_VREG_VALUE;
			else if(i==1)
				data[i][j]=ITERM_CURRENT_VALUE;
			else if(i==2)
				data[i][j]=1;
			else if(i==3)
				data[i][j]=1;
		}
	}
}

static void update_chg_done_data(int data[4][5],int vbat,int ibat,int iindpm,int vindpm)
{
	u8 i,j;
	for(i=4;i>0;i--){
		for(j=0;j<4;j++){
			data[j][i]=data[j][i-1];
		}
	}
	data[0][0] = vbat;
	data[1][0] = ibat;
	data[2][0] = iindpm;
	data[3][0] = vindpm;
}

static bool is_chg_done(int data[4][5],int vreg,int iterm)
{
	u8 iindpm_stat_count = 0,vindpm_stat_count = 0;
	u8 i;
	bool is_done = true;

	for(i = 0;i < 5; i++)
	{
		if((vreg - data[0][i]) > 100 )
			is_done = false;
		if(data[1][i] > iterm)
			is_done = false;
		if(data[2][i])
			iindpm_stat_count++;
		if(data[3][i])
			vindpm_stat_count++;
	}
	if(iindpm_stat_count > 2)
		is_done = false;
	if(vindpm_stat_count > 2)
		is_done = false;
	return is_done;
}

static void charge_done_detect_work_func(struct work_struct *work)
{
	int  ret = -1;
	struct cx2560x *cx = container_of(work, struct cx2560x,
								charge_done_detect_work.work);

	cx2560x_set_auto_vindpm(cx);
	ret = get_chg_state(cx);
	if(ret)
		goto out;

	if(cx->chg_done_stat == REG08_CHRG_STAT_CHGDONE)
	{
		if(is_chg_done(cx->chg_done_data,cx->vreg,ITERM_CURRENT_VALUE))
			goto chg_done;
		else{
			cx2560x_disable_charger(cx);
			cx2560x_enable_charger(cx);
			msleep(100);
		}
	}
	pr_err("%s: ibat = %d, vbat = %d, val_vreg=%d , iindpm_flag %d vindpm_flag %d\n",
		__func__, cx->ibat, cx->vbat, cx->vreg, cx->iindpm_stat, cx->vindpm_stat);

	update_chg_done_data(cx->chg_done_data,cx->vbat,cx->ibat,cx->iindpm_stat,cx->vindpm_stat);
	if(is_chg_done(cx->chg_done_data,cx->vreg,ITERM_CURRENT_VALUE)){
		goto chg_done;
	}

out:
/*+S98901AA1.wt,MODIFY,20240627,optimize charger done work period*/
	schedule_delayed_work(&cx->charge_done_detect_work, 2 * HZ);
/*+S98901AA1.wt,MODIFY,20240627,optimize charger done work period*/
	return;
chg_done:
	pr_err("%s: chg done\n", __func__);
	cx2560x_disable_charger(cx);
	charge_done_flag = true;
	cancel_delayed_work(&cx->recharge_detect_work);
	schedule_delayed_work(&cx->recharge_detect_work, 0);
	cancel_delayed_work(&cx->charge_done_detect_work);
	return;
}


static void recharge_detect_work_func(struct work_struct *work)
{
	struct cx2560x *cx = container_of(work, struct cx2560x,
								recharge_detect_work.work);

    if(charge_done_flag)
    {
		cx->vbat = cx2560x_get_bat_voltage(cx);
		cx->vreg = cx2560x_get_vreg(cx);
		if(cx->vreg < 0)
			goto out;
		pr_err("%s: vbat = %d, val_vreg=%d\n", __func__, cx->vbat, cx->vreg);
/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged*/
        if(((cx->vreg) - (cx->vbat)) > cx->recharge_val)
        {
            pr_err("%s: entry enable_recharger\n", __func__);
            cx2560x_enable_charger(cx);
            charge_done_flag = false;
			cancel_delayed_work(&cx->charge_done_detect_work);
            schedule_delayed_work(&cx->charge_done_detect_work, 0);
            cancel_delayed_work(&cx->recharge_detect_work);
			return;
        }
    }
out:
	schedule_delayed_work(&cx->recharge_detect_work, 3 * HZ);
    return;
}

static void charger_usb_detect_work_func(struct work_struct *work)
{
	struct delayed_work *charge_usb_detect_work = NULL;
	struct cx2560x *cx = NULL;
	struct cx2560x_state state;
	int usb_device_state = 0;
	int i = 0 ;

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
	// reboot with connect usb or charging in power_off
	get_power_off_charging(cx);
	pr_err("%s,enter bootmode = %d\n",__func__,cx->bootmode);
	if(cx->reboot_float_filter || cx->bootmode == 8){
		no_usb_flag = 0;
		cx->reboot_float_filter = 0;
	}
	else
	{
		if (!IS_ERR_OR_NULL(cx->ssusb) && !IS_ERR_OR_NULL(cx->ssusb->u3d)) {
			usb_device_state = cx->ssusb->u3d->g.state;
		}
		pr_err("%s: enter,usb_device_state=%d\n", __func__,usb_device_state);

		if(usb_device_state == 0 && (cx->hub_check_chg == true)){
			do{
				if(!IS_ERR_OR_NULL(cx->ssusb))
				{
					usb_device_state = cx->ssusb->u3d->g.state;
				}
				cx2560x_set_usbsw_state(cx, USBSW_CHG);
				msleep(300);
				cx2560x_enable_bc12(cx);
				msleep(1000);
				no_usb_flag = 1;
				i++;
				if(usb_device_state != 0)
				{
					no_usb_flag = 0;
					break;
				}
				cx2560x_get_status(cx,&state);
				if(state.vbus_stat != REG08_VBUS_TYPE_USB_SDP){
					no_usb_flag = 0;
					break;
				}
			}while(i< 1 && (usb_device_state==0));
			pr_err("%s: SDP retry:%d, no_usb_flag=%d\n", __func__,i,no_usb_flag);
			pr_err("%s: exit\n", __func__);
			//if(cx->reboot_float_filter)
			//no_usb_flag = 0;
		}
	}
	schedule_delayed_work(&cx->irq_work, 0);
	return;
}

static int cx2560x_disable_batfet(struct cx2560x *cx)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(cx2560x_disable_batfet);

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
//EXPORT_SYMBOL_GPL(cx2560x_set_batfet_delay);

static int cx2560x_set_batfet_reset(struct cx2560x *cx, bool en)
{
	u8 val;

	if (en)
		val = REG07_BATFET_RST_ENABLE;
	else
		val = REG07_BATFET_RST_DISABLE;

	val <<= REG07_BATFET_RST_EN_SHIFT;
	return cx2560x_update_bits(cx, CX2560X_REG_07, REG07_BATFET_RST_EN_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(cx2560x_set_batfet_reset);

static int cx2560x_enable_safety_timer(struct cx2560x *cx)
{
	int ret;
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;
	ret =  cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TIMER_MASK,val);
	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	return ret;
}
//EXPORT_SYMBOL_GPL(cx2560x_enable_safety_timer);

static int cx2560x_disable_safety_timer(struct cx2560x *cx)
{
	int ret;
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;
	ret =  cx2560x_update_bits(cx, CX2560X_REG_05, REG05_EN_TIMER_MASK,val);
	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	return ret;
}
//EXPORT_SYMBOL_GPL(cx2560x_disable_safety_timer);



static void cx2560x_set_dpdm_hiz(struct cx2560x *cx)
{
	cx2560x_update_bits(cx, CX2560X_REG_10,
		REG10_DP_DAC_MASK | REG10_DM_DAC_MASK,
		REG10_DP_DAC_HIZ | REG10_DM_DAC_HIZ);
}

static int cx2560x_force_dpm_det(struct cx2560x *cx) {
	int ret;
	u8 reg_val;

	ret = cx2560x_read_byte(cx, CX2560X_REG_07, &reg_val);
	if (ret) {
		dev_err(cx->dev, "get cx2560x 0x07 register failed");
		return -EINVAL;
	}
	if(0 != (reg_val & 0x80)) {
		dev_err(cx->dev, "cx2560x_force_dpm_det IINDET_EN=1\n");
		return -EINVAL;
	}
	reg_val = reg_val | 0x80;
	reg_val = reg_val & 0xf3;
	ret = cx2560x_write_byte(cx, CX2560X_REG_07, reg_val);
	if (ret) {
		dev_err(cx->dev, "set cx2560x force dpm det register failed");
		return ret;
	}

	ret = cx2560x_read_byte(cx, CX2560X_REG_07, &reg_val);
	if (ret) {
		dev_err(cx->dev, "get cx2560x 0x07 register failed");
		return -EINVAL;
	}
	dev_err(cx->dev, "cx2560x_force_dpm_det get 0x07 register  reg_val=%#x", reg_val);
	return 0;
}

static int cx2560x_enable_bc12(struct cx2560x *cx)
{
	cx2560x_update_bits(cx, CX2560X_REG_10,
		REG10_DP_DAC_MASK | REG10_DM_DAC_MASK,
		REG10_DP_DAC_0V | REG10_DM_DAC_0V);    //复位适配器
	msleep(100);
	cx2560x_set_dpdm_hiz(cx);
	cx2560x_force_dpm_det(cx);
	return 0;
}



static int cx2560x_set_dp(struct cx2560x *cx, u8 dp_dac)
{
	u8 val = 0;
	if(dp_dac == REG10_DP_DAC_HIZ)
		val = REG10_DP_DAC_HIZ << REG10_DP_DAC_SHIFT;
	else if(dp_dac == REG10_DP_DAC_0V)
		val = REG10_DP_DAC_0V << REG10_DP_DAC_SHIFT;
	else if(dp_dac == REG10_DP_DAC_0P6V)
		val = REG10_DP_DAC_0P6V << REG10_DP_DAC_SHIFT;
	else if(dp_dac == REG10_DP_DAC_1P2V)
		val = REG10_DP_DAC_1P2V << REG10_DP_DAC_SHIFT;
	else if(dp_dac == REG10_DP_DAC_2V)
		val = REG10_DP_DAC_2V << REG10_DP_DAC_SHIFT;
	else if(dp_dac == REG10_DP_DAC_2P7V)
		val = REG10_DP_DAC_2P7V << REG10_DP_DAC_SHIFT;
	
	return cx2560x_update_bits(cx, CX2560X_REG_10, REG10_DP_DAC_MASK,
				   val);
}
//EXPORT_SYMBOL_GPL(cx2560x_set_dp);
/*
static int cx2560x_set_dm(struct cx2560x *cx, u8 dm_dac)
{
	u8 val = 0;
	if(dm_dac == REG10_DM_DAC_HIZ)
		val = REG10_DM_DAC_HIZ << REG10_DM_DAC_SHIFT;
	else if(dm_dac == REG10_DM_DAC_0V)
		val = REG10_DM_DAC_0V << REG10_DM_DAC_SHIFT;
	else if(dm_dac == REG10_DM_DAC_0P6V)
		val = REG10_DM_DAC_0P6V << REG10_DM_DAC_SHIFT;
	else if(dm_dac == REG10_DM_DAC_1P2V)
		val = REG10_DM_DAC_1P2V << REG10_DM_DAC_SHIFT;
	else if(dm_dac == REG10_DM_DAC_2V)
		val = REG10_DM_DAC_2V << REG10_DM_DAC_SHIFT;
	else if(dm_dac == REG10_DM_DAC_2P7V)
		val = REG10_DM_DAC_2P7V << REG10_DM_DAC_SHIFT;
	
	return cx2560x_update_bits(cx, CX2560X_REG_10, REG10_DM_DAC_MASK,
				   val);
}
*/
/*
int cx2560x_enable_hvdcp(struct cx2560x *cx, bool en)
{
	const u8 val = en << REG10_HVDCP_EN_SHIFT;

	return cx2560x_update_bits(cx, CX2560X_REG_10, REG10_HVDCP_EN_MASK,
				   val);
}*/

static int cx2560x_set_dp_for_afc(struct cx2560x *cx)
{
    int ret;
	//  cx2560x_set_usbsw_state(cx, USBSW_CHG);
	//  msleep(20);
    // ret = cx2560x_enable_hvdcp(cx, REG10_HVDCP_ENABLE);
    // if (ret) {
	// 	pr_err("cx2560x_enable_hvdcp failed(%d)\n", ret);
	// }

    ret = cx2560x_set_dp(cx, REG10_DP_DAC_0P6V);
    if (ret) {
		pr_err("cx2560x_set_dp failed(%d)\n", ret);
	}
	// msleep(50);
	return ret;
}

//EXPORT_SYMBOL_GPL(cx2560x_set_dp_for_afc);

static int cx2560x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = cx2560x_read_byte(cx, CX2560X_REG_02, &reg_val);
	reg_val &= REG02_ICHG_MASK;
	if(reg_val <= 13)
	{
		ichg = reg_val * 90;
	}
	else
	{
		ichg = (reg_val-14)*115/2+805;
	}
	*curr = ichg*1000;
	pr_err("%s get ichg=%d\n", __func__, *curr);
	return ret;
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
		dev_err(cx->dev, "%s: get iindpm=%d\n", __func__,*curr);
	}
	return ret;
}

static int cx2560x_set_usbsw_state(struct cx2560x *cx,
                enum cx2560x_usbsw usbsw)
{
    struct phy *phy;
    int ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET :
                           PHY_MODE_BC11_CLR;

    dev_err(cx->dev, "usbsw=%d\n", usbsw);
    phy = phy_get(cx->dev, "usb2-phy");
    if (IS_ERR_OR_NULL(phy)) {
        dev_err(cx->dev, "%s: failed to get usb2-phy\n",__func__);
        return -ENODEV;
    }

    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
    if (ret)
        dev_err(cx->dev, "%s: failed to set phy ext mode\n",__func__);
    phy_put(cx->dev, phy);
    return ret;
}


static struct cx2560x_platform_data *cx2560x_parse_dt(struct device_node *np,
						      struct cx2560x *cx)
{
	int ret;
	struct cx2560x_platform_data *pdata;

	pdata = devm_kzalloc(&cx->client->dev, sizeof(struct cx2560x_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &cx->chg_dev_name) < 0) {
		cx->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "cx,eint_name", &cx->eint_name) < 0) {
		cx->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	ret = of_get_named_gpio(np, "intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no cx,intr_gpio(%d)\n",
				      __func__, ret);
	} else
		cx->intr_gpio = ret;

	pr_info("%s intr_gpio = %u\n", __func__, cx->intr_gpio);

	cx->chg_det_enable = of_property_read_bool(np, "cx,cx2560x,charge-detect-enable");

	ret = of_property_read_u32(np, "cx,cx2560x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of cx,cx2560x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of cx,cx2560x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of cx,cx2560x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of cx,cx2560x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of cx,cx2560x,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of cx,cx2560x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of cx,cx2560x,termination-current\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5126;
		pr_err("Failed to read node of cx,cx2560x,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of cx,cx2560x,boost-current\n");
	}

	ret = of_property_read_u32(np, "cx,cx2560x,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 10500;
		pr_err("Failed to read node of cx,cx2560x,vac-ovp-threshold\n");
	}

	return pdata;
}


static void cx2560x_charge_irq_work(struct work_struct *work)
{
    struct cx2560x *cx = container_of(work, struct cx2560x,
								irq_work.work);

    int ret;
	u8 reg_val = 0;
	int vbus_stat = 0;
	int retry_cnt = 0;
	do{
		if (!cx2560x_dpdm_detect_is_done(cx)) {
			pr_err("[%s] DPDM detecte not done, retry_cnt:%d\n", __func__, retry_cnt);
		} else {
			pr_err("[%s] BC1.2 done\n", __func__);
			break;
		}
		mdelay(60);
	} while(retry_cnt++ < BC12_DONE_TIMEOUT_CHECK_MAX_RETRY);

	ret = cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
	if (ret)
		return;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	pr_info("[%s] reg08: 0x%02x\n", __func__, reg_val);

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		pr_info("no input detected");
		break;
	case REG08_VBUS_TYPE_USB_SDP:
		if (!usb_detect_flag) {
			pr_info("[%s] enter usb detect\n", __func__);
			schedule_delayed_work(&cx->charge_usb_detect_work, 2* HZ);
		}
		if(no_usb_flag == 0) {
			pr_info("[%s] CX25601D charger type: SDP\n", __func__);
			cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			cx->chg_type = POWER_SUPPLY_TYPE_USB;
			cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			cx2560x_set_input_current_limit(cx, 500);
			cx2560x_set_chargecurrent(cx,2000);
		} else {
			pr_info("[%s] CX25601D charger type: UNKNOWN/FLOAT\n", __func__);
			cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			cx->chg_type = POWER_SUPPLY_TYPE_USB;
			cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			cx2560x_set_input_current_limit(cx, 1000);
			cx2560x_set_chargecurrent(cx,2000);
		}
		break;
	case REG08_VBUS_TYPE_USB_CDP:
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		cx->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		cx2560x_set_input_current_limit(cx, 1500);
		cx2560x_set_chargecurrent(cx,2000);
		pr_info("CDP detected");
		break;
	case REG08_VBUS_TYPE_USB_DCP:
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		cx->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		cx2560x_set_input_current_limit(cx, 1500);
		cx2560x_set_chargecurrent(cx, 2000);
		cx2560x_set_dp_for_afc(cx);
		pr_info("DCP detected");
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		cx->chg_type = POWER_SUPPLY_TYPE_USB;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cx2560x_set_input_current_limit(cx, 1000);
		cx2560x_set_chargecurrent(cx,2000);
		pr_info("unknow detected");
/*+S98901AA1.wt,MODIFY,20240628,add once BC12 */
		cx2560x_set_usbsw_state(cx, USBSW_CHG);
		msleep(100);
		cx2560x_enable_bc12(cx);
		msleep(500);
/*+S98901AA1.wt,MODIFY,20240628,add once BC12*/
		break;
	case REG08_VBUS_TYPE_NON_STANDARD:
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		cx->chg_type = POWER_SUPPLY_TYPE_USB;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		cx2560x_set_input_current_limit(cx, 1000);
		cx2560x_set_chargecurrent(cx,2000);
		pr_info("nostandard detected");
		cx2560x_set_usbsw_state(cx, USBSW_CHG);
		msleep(100);
		cx2560x_enable_bc12(cx);
		msleep(500);
		break;
	default:
		cx2560x_set_input_current_limit(cx, 1500);
		cx2560x_set_chargecurrent(cx,2000);
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		cx->chg_type = POWER_SUPPLY_TYPE_USB;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		pr_info("default detected");
		break;
	}
	if ((cx->psy_desc.type == POWER_SUPPLY_TYPE_USB) || (cx->psy_desc.type == POWER_SUPPLY_TYPE_USB_CDP)) {
    	cx2560x_set_usbsw_state(cx, USBSW_USB);
	}
	schedule_delayed_work(&cx->psy_dwork, 0);
	return ;
}

static void cx2560x_inform_psy_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	u8 reg_val = 0;
	int vbus = 0;
	union power_supply_propval propval;
	struct cx2560x *cx = container_of(work, struct cx2560x,
								psy_dwork.work);

	if(cx->is_hiz){
		cx2560x_get_vbus_value(cx,&vbus);
		if(vbus>1000){
			propval.intval = 1;
		} else {
			propval.intval = 0;
		}
	}else{
		ret = cx2560x_read_byte(cx, CX2560X_REG_0A, &reg_val);
		if (ret) {
			return;
		}

		if (reg_val & 0x80) {
			propval.intval = 1;
		} else {
			propval.intval = 0;
		}
	}
	pr_err("%s ONLINE %d\n", __func__,propval.intval);
	if (!IS_ERR_OR_NULL(cx->chg_psy)) {
		ret = power_supply_set_property(cx->chg_psy, POWER_SUPPLY_PROP_ONLINE,
						&propval);

		if (ret < 0)
			pr_notice("inform power supply online failed:%d\n", ret);

		propval.intval = cx->psy_desc.type;

		ret = power_supply_set_property(cx->chg_psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&propval);

		if (cx->chg_psy) {
			power_supply_changed(cx->chg_psy);
			pr_err("power supply changed\n");
		}
	}
	return;
}

static irqreturn_t cx2560x_irq_handler(int irq, void *data)
{
	bool prev_vbus_gd;
	struct cx2560x_state state;
    struct cx2560x *cx = (struct cx2560x *)data;
	int otg_retry=4,vbus_stat,ret;
	u8 regdata;
	// bool boot_mode;

#ifdef WT_COMPILE_FACTORY_VERSION
	if(cx->probe_flag == 0)
		return IRQ_HANDLED;
#endif
	dev_info(cx->dev, "enter:%s",__func__);

	// get_power_off_charging(cx);

	if(!cx->reboot_flag)  // reboot with connect usb
		cx->reboot_float_filter = 1;
	else
		cx->reboot_float_filter = 0;
	// pr_err("%s cx->reboot_flag =  %d,cx->reboot_float_filter=%d\n", __func__,cx->reboot_flag,cx->reboot_float_filter);

	prev_vbus_gd = cx->state.vbus_gd;
	//cx2560x_dump_regs(cx);

	if(cx->is_hiz)
	{
		dev_info(cx->dev, "hiz irq");
		hiz_detect_type_func(cx);
		return IRQ_HANDLED;
	}
	cx2560x_get_status(cx, &state);
	if (cx->state.pg_stat == state.pg_stat && !state.boost_fault) {
		dev_info(cx->dev, "state not changed");
		return IRQ_HANDLED;
	}
	cx->state = state;
	if (state.vbus_stat == REG08_VBUS_TYPE_USB_DCP) {
		cx2560x_set_dp_for_afc(cx);
	}
	cx2560x_dump_regs(cx);
	if (!prev_vbus_gd && cx->state.vbus_gd) {
		if (cx->hub_check_chg)
			cx2560x_set_usbsw_state(cx, USBSW_CHG);
/*+S98901AA1.wt,MODIFY,20240628,add wakeup lock*/
		if(!IS_ERR_OR_NULL(cx->charger_wakelock)) {
			if(!cx->charger_wakelock->active) {
				__pm_stay_awake(cx->charger_wakelock);
			}
		}
/*+S98901AA1.wt,MODIFY,20240628,add wakeup lock*/
		pr_err("adapter/usb inserted\n");
		cx->offset_flag = 0;
		cx->offset_reset = 0;
		no_usb_flag = 0;
		usb_detect_flag = false;
		charge_done_flag = false;
		cx2560x_set_input_current_limit(cx, 100);
		cx2560x_enable_charger(cx);
		cancel_delayed_work(&cx->charge_done_detect_work);
		schedule_delayed_work(&cx->charge_done_detect_work, 0);
		schedule_delayed_work(&cx->irq_work,msecs_to_jiffies(50));
		cancel_delayed_work_sync(&cx->dump_work);
		schedule_delayed_work(&cx->dump_work, 3* HZ);//debug
	} else if (prev_vbus_gd && !cx->state.vbus_gd) {
/*+S98901AA1.wt,MODIFY,20240628,relax wakeup lock and comment out repeart reporting*/
		__pm_relax(cx->charger_wakelock);
		//schedule_delayed_work(&cx->psy_dwork, 0);
/*+S98901AA1.wt,MODIFY,20240628,relax wakeup lock and comment out repeart reporting*/
		pr_err("adapter/usb removed\n");
		cx2560x_set_usbsw_state(cx, USBSW_USB);
		cx->hiz_online = 0;
		cx->reboot_float_filter = 0;
		cx->hub_check_chg = true;
		cx2560x_set_dpdm_hiz(cx);
		cancel_delayed_work(&cx->charge_usb_detect_work);
		cancel_delayed_work_sync(&cx->charge_done_detect_work);
		cancel_delayed_work_sync(&cx->recharge_detect_work);
		cancel_delayed_work(&cx->irq_work);
		cancel_delayed_work_sync(&cx->dump_work);
		cx2560x_set_input_volt_limit(cx, 4600);
		cx2560x_set_chargecurrent(cx,500);
		cx2560x_disable_otg(cx);
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		cx->chg_old_status = POWER_SUPPLY_STATUS_UNKNOWN;
		power_supply_changed(cx->chg_psy);
		return IRQ_HANDLED;
	}

	if(state.boost_fault){
		pr_err("otg fault occur\n");
		do{
			ret = cx2560x_enable_otg(cx);
			msleep(2);
			cx2560x_read_byte_without_retry(cx,CX2560X_REG_08,&regdata);
			vbus_stat = (regdata&REG08_VBUS_STAT_MASK)>>REG08_VBUS_STAT_SHIFT;
			if(vbus_stat == REG08_VBUS_TYPE_OTG && ret>=0){
				pr_err("otg retry success\n");
/*+S98901AA1.wt,MODIFY,20240628,add otg_flag */
				cx->otg_flag = 1;
				break;
			}
		}while((otg_retry--)>0 && (vbus_stat != REG08_VBUS_TYPE_OTG));
		if(vbus_stat != REG08_VBUS_TYPE_OTG){
			pr_err("otg retry fail\n");
			cx->otg_flag = 0;
/*+S98901AA1.wt,MODIFY,20240628,add otg_flag */
		}
	}


	return IRQ_HANDLED;
}

static int cx2560x_register_interrupt(struct cx2560x *cx)
{
	int ret = 0;
	ret = devm_gpio_request_one(cx->dev, cx->intr_gpio, GPIOF_DIR_IN,
			devm_kasprintf(cx->dev, GFP_KERNEL,
			"cx2560x_intr_gpio.%s", dev_name(cx->dev)));
	if (ret < 0) {
		pr_err("%s gpio request fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	cx->client->irq = gpio_to_irq(cx->intr_gpio);
	if (cx->client->irq < 0) {
		pr_err("%s gpio2irq fail(%d)\n",
				      __func__, cx->client->irq);
		return cx->client->irq;
	}
	pr_info("%s irq = %d\n", __func__, cx->client->irq);

	ret = devm_request_threaded_irq(cx->dev, cx->client->irq, NULL,
					cx2560x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"cx_irq", cx);
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
	u8 data;

	cx2560x_disable_watchdog_timer(cx);

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

	ret =cx2560x_set_chargecurrent(cx,cx->platform_data->usb.ichg);
	if (ret)
		pr_err("Failed to set ichg, ret = %d\n", ret);
	ret =cx2560x_set_chargevolt(cx,CV_VREG_VALUE);
	if (ret)
		pr_err("Failed to set vreg, ret = %d\n", ret);
/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged*/
	ret =cx2560x_set_rechargeth(cx,100);
	if (ret)
		pr_err("Failed to set rechargeth, ret = %d\n", ret);
	ret =cx2560x_set_input_current_limit(cx,cx->platform_data->usb.ilim);
	if (ret)
		pr_err("Failed to set iindpm, ret = %d\n", ret);
	ret =cx2560x_set_input_volt_limit(cx,cx->platform_data->usb.vlim);
	if (ret)
		pr_err("Failed to set vindpm, ret = %d\n", ret);

	ret = cx2560x_set_batfet_reset(cx, 1);
	if (ret)
		pr_err("Failed to set batfet reset enable, ret = %d\n", ret);

	ret = cx2560x_disable_safety_timer(cx);
	if (ret)
		pr_err("Failed to disable safety timer function\n");

	cx2560x_enable_term(cx,true);
	cx2560x_set_term_current(cx,120);
	ret=cx2560x_write_reg40(cx, true);
	if(ret==1)
		pr_err(" write reg40 successfully; \n", __func__);

	cx2560x_write_byte(cx, 0x41, 0x28);
	cx2560x_write_byte_without_retry(cx, 0x44, 0x18);
	cx2560x_write_byte_without_retry(cx, 0x89, 0x03);
	cx2560x_write_byte_without_retry(cx, 0x83, 0x00);
	cx2560x_read_byte(cx, 0x41, &data);
	pr_err("%s;enter;0x41=%x;\n", __func__, data);
/*+S98901AA1.wt,MODIFY,20240628, shield TS cold irq */
	cx2560x_update_bits(cx, 0x86, 0x20, 0x20);
/*+S98901AA1.wt,MODIFY,20240628, shield Ts cold irq */
	ret=cx2560x_write_reg40(cx, false);
	cx2560x_set_chargecurrent(cx,500);
	cx2560x_exit_hiz_mode(cx);
	cx2560x_set_shipmode_delay(cx->chg_dev,true);
	// close Q1_FULLON
	cx2560x_update_bits(cx, 0x02, 0x40, 0x0);


	cx2560x_update_bits(cx, CX2560X_REG_0F, REG0F_TREG_MASK, 0<<REG0F_TREG_SHIFT);
	init_chg_done_data(cx->chg_done_data);
	pr_err(" cx2560x init sucess \n", __func__);
	return 0;
}

static void cx2560x_inform_prob_dwork_handler(struct work_struct *work)
{
	struct cx2560x *cx = container_of(work, struct cx2560x,
								prob_dwork.work);

	cx2560x_set_usbsw_state(cx, USBSW_CHG);
	msleep(100);
	cx2560x_enable_bc12(cx);
	msleep(600);
	cx2560x_set_usbsw_state(cx, USBSW_USB);
#ifdef WT_COMPILE_FACTORY_VERSION
	cx->probe_flag = 1;
#endif
	pr_err("%s\n", __func__);
	cx2560x_irq_handler(cx->irq, (void *) cx);
}

static int cx2560x_detect_device(struct cx2560x *cx)
{
	int ret = 0;
	u8 data = 0;

	ret = cx2560x_read_byte(cx, CX2560X_REG_0B, &data);
	if (ret) {
		pr_err("%s: read UPM6910_REG_0B fail, ret:%d\n", __func__, ret);
		return ret;
	} else {
		cx->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		cx->revision = (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
		if(cx->part_no == 0x07) {
			pr_err("%s: cx25601 found \n", __func__);
		}
	}
	return ret;
}

static void cx2560x_dump_regs(struct cx2560x *cx)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x11; addr++) {
		if(addr == CX2560X_REG_09 && (!cx->otg_flag))
			continue;
		ret = cx2560x_read_byte_without_retry(cx, addr, &val);
		if(ret)
			pr_err("%s i2c transfor error\n", __func__);
		pr_err("%s :Reg[%.2x] = 0x%.2x\n", __func__, addr, val);
	}
}


static ssize_t
cx2560x_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct cx2560x *cx = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[512];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "cx2560x Reg");
	for (addr = 0x0; addr <= 0x11; addr++) {
		ret = cx2560x_read_byte(cx, addr, &val);
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
	if (ret == 2 && reg < 0x12) {
		cx2560x_write_byte(cx, (unsigned char) reg,
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

	if (enable)
		if (charge_done_flag == false) 
			ret = cx2560x_enable_charger(cx);
		else
			pr_err("charge_done_flag is true\r\n");
	else
		ret = cx2560x_disable_charger(cx);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = cx2560x_read_byte(cx, CX2560X_REG_01, &val);

	if (!ret)
		cx->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int cx2560x_plug_in(struct charger_device *chg_dev)
{
	int ret;
	pr_err("%s\n", __func__);
	ret = cx2560x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int cx2560x_plug_out(struct charger_device *chg_dev)
{
	int ret;
	pr_err("%s\n", __func__);
	ret = cx2560x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

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

	ret = cx2560x_read_byte(cx, CX2560X_REG_08, &val);
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

static int cx2560x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 0 * 1000;

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
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
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

	return cx2560x_set_input_current_limit(cx, curr / 1000);
}

static int cx2560x_kick_wdt(struct charger_device *chg_dev)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

	return cx2560x_reset_watchdog_timer(cx);
}

extern int nu2115_set_otg_txmode(bool enable);
static int cx2560x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;

	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	union power_supply_propval propval;

	if (en) {
		cx2560x_set_usbsw_state(cx, USBSW_USB);
		cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, 0);//exit hiz
		ret = cx2560x_enable_otg(cx);
		nu2115_set_otg_txmode(1);
		cx->otg_flag = 1;
	} else {
		ret = cx2560x_disable_otg(cx);
		nu2115_set_otg_txmode(0);
		if (cx->otg_flag) {
			cx->otg_flag = 0;
		}
	}

	//cx2560x_dump_regs(cx);
	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	if (!cx->otg_psy)
		cx->otg_psy = power_supply_get_by_name("otg");
	if (!cx->otg_psy) {
		pr_err("get power supply failed\n");
		return -EINVAL;
	}

	propval.intval = en;
	ret = power_supply_set_property(cx->otg_psy, POWER_SUPPLY_PROP_TYPE, &propval);
	if (ret < 0) {
		pr_err("otg_psy type fail(%d)\n", ret);
		return ret;
	}


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

static int cx2560x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = cx2560x_set_boost_current(cx, curr / 1000);

	return ret;
}

static int cx2560x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = -1;
	int vbus = 0;
	struct cx2560x *cx = NULL;
	struct cx2560x_state state;
	union power_supply_propval propval;

	if(NULL == chg_dev){
		pr_err("chg_dev is NULL \n");
		return ret;
	}

	cx = dev_get_drvdata(&chg_dev->dev);
	if(NULL == cx){
		pr_err("cx is NULL \n");
		return ret;
	}
	cx2560x_set_usbsw_state(cx, USBSW_CHG);
	msleep(100);
	cx2560x_enable_bc12(cx);
	msleep(700);
	schedule_delayed_work(&cx->psy_dwork, 0);

	if (en == false) {
        propval.intval = 0;
    } else {
        propval.intval = 1;
    }

	cx2560x_get_status(cx,&state);

	if(cx->is_hiz){
		cx2560x_get_vbus_value(cx,&vbus);
		if(vbus>1000){
			propval.intval = 1;
		} else {
			propval.intval = 0;
		}
	}
	ret = power_supply_set_property(cx->chg_psy,
                    POWER_SUPPLY_PROP_ONLINE,
                    &propval);
	if (ret < 0)
        pr_notice("inform power supply online failed:%d\n", ret);
	
	if (en == false) {
        propval.intval = CHARGER_UNKNOWN;
    } else {
		if (state.vbus_stat == 0)
			propval.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		else if (state.vbus_stat == 1)
			propval.intval = POWER_SUPPLY_USB_TYPE_SDP;
		else if (state.vbus_stat == 2)
			propval.intval = POWER_SUPPLY_USB_TYPE_CDP;
		else if (state.vbus_stat == 3)
			propval.intval = POWER_SUPPLY_USB_TYPE_DCP;
		else if (state.vbus_stat == 5)
			propval.intval = POWER_SUPPLY_USB_TYPE_SDP;
		else if (state.vbus_stat == 6)
			propval.intval = POWER_SUPPLY_USB_TYPE_DCP;
		else
			propval.intval = POWER_SUPPLY_USB_TYPE_SDP;
    }

	ret = power_supply_set_property(cx->chg_psy,
                    POWER_SUPPLY_PROP_CHARGE_TYPE,
                    &propval);
    if (ret < 0)
        pr_notice("inform power supply charge type failed:%d\n", ret);

	return ret;
};

static int cx2560x_enable_vbus(struct regulator_dev *rdev)
{
    struct cx2560x *cx = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s ente\r\n", __func__);

	if (IS_ERR_OR_NULL(cx->chg_dev)) {
		ret = PTR_ERR(cx->chg_dev);
		return ret;
	}

	cx2560x_set_otg(cx->chg_dev, true);

    return ret;
}

static int cx2560x_disable_vbus(struct regulator_dev *rdev)
{
    struct cx2560x *cx = charger_get_data(s_chg_dev_otg);
    int ret = 0;

    pr_notice("%s enter\n", __func__);

	if (IS_ERR_OR_NULL(cx->chg_dev)) {
		ret = PTR_ERR(cx->chg_dev);
		return ret;
	}

	cx2560x_set_otg(cx->chg_dev, false);

	return ret;
}

static int cx2560x_is_enabled_vbus(struct regulator_dev *rdev)
{
    struct cx2560x *cx = charger_get_data(s_chg_dev_otg);

	u8 reg_val = 0;
	u8 val = 0;

    pr_notice("%s enter\n", __func__);

	cx2560x_read_byte(cx, CX2560X_REG_01, &reg_val);
	val = !!(reg_val & REG01_OTG_CONFIG_MASK);
	pr_notice("%s, is enable vbus :%d\n", __func__,val);

	return val ? 1 : 0;;
}

static struct regulator_ops cx2560x_vbus_ops = {
    .enable = cx2560x_enable_vbus,
    .disable = cx2560x_disable_vbus,
    .is_enabled = cx2560x_is_enabled_vbus,
};

static const struct regulator_desc cx2560x_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &cx2560x_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int cx2560x_vbus_regulator_register(struct charger_device *chg_dev)
{
    struct regulator_config config = {};
    struct cx2560x *cx = NULL;
    int ret = 0;
    //otg regulator
    cx = dev_get_drvdata(&chg_dev->dev);
    config.dev = cx->dev;
    config.driver_data = cx;
    cx->otg_rdev =
        devm_regulator_register(cx->dev, &cx2560x_otg_rdesc, &config);
    cx->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(cx->otg_rdev)) {
        ret = PTR_ERR(cx->otg_rdev);
        pr_info("%s: register otg regulator failed (%d)\n", __func__,ret);
    }
    return ret;
}

extern nu2115_shipmode_enable_adc(bool enable);
static int cx2560x_set_shipmode(struct charger_device *chg_dev, bool en)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en) {
		nu2115_shipmode_enable_adc(false);
		ret = cx2560x_disable_batfet(cx);
		if (ret < 0) {
			pr_err("set shipmode failed(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int cx2560x_set_shipmode_delay(struct charger_device *chg_dev, bool en)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);
	int ret;

	//if set R07 bit5 as 1, then set bit3 as 0
	ret = cx2560x_set_batfet_delay(cx, en);
	if (ret < 0) {
		pr_err("set shipmode delay failed(%d)\n", ret);
		return ret;
	}

	return 0;
}

/*+S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
static int cx2560x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct cx2560x *cx = dev_get_drvdata(&chg_dev->dev);

    dev_info(cx->dev, "%s event = %d\n", __func__, event);

    switch (event) {
    case EVENT_FULL:
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
/*-S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/

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
	.is_charging_done = cx2560x_is_charging_done,
	.get_min_charging_current = cx2560x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = cx2560x_set_safety_timer,
	.is_safety_timer_enabled = cx2560x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = cx2560x_set_otg,
	.set_boost_current_limit = cx2560x_set_boost_ilmt,
	.enable_discharge = NULL,

	.enable_chg_type_det = cx2560x_enable_chg_type_det,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	.get_vbus_adc = cx2560x_get_vbus,

/*+S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
    /* Event */
    .event = cx2560x_do_event,
/*-S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/

	/* ship mode */
	.set_shipmode = cx2560x_set_shipmode,
	.set_shipmode_delay = cx2560x_set_shipmode_delay,

    /* HIZ */
	.enable_hz = cx2560x_set_hiz_mode, 

	/* ADC */
	.get_tchg_adc = NULL,

/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged start*/
	/* rechg */
	.set_recharge = cx2560x_set_recharge,
/*+S98901AA1 gujiayin.wt,modify,2024/08/08 second-supplier connected PC discharge can not be recharged end*/
};



static int  cx2560x_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct cx2560x *cx= power_supply_get_drvdata(psy);

	int ret = 0;
    u8 reg_val;
	int prop_type;
	//int pg_stat;
	pr_err("%s: psp=%d \n", __func__, psp);

	switch(psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		if(cx->is_hiz)
		{
			val->intval = cx->hiz_online;
			pr_err("hiz_online =%d \n",val->intval);
		}else{
			val->intval =  cx->state.pg_stat;
			pr_err("online =%d \n",val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (cx->psy_desc.type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_err("POWER_SUPPLY_TYPE_UNKNOWN \n");
            ret = cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
            if(!ret){
                cx->state.vbus_stat = (reg_val & REG08_VBUS_STAT_MASK)>>REG08_VBUS_STAT_SHIFT;
            }
		}
		if(cx->is_hiz)
		{
			prop_type = cx->prev_hiz_type;
		}else{
			prop_type = cx->state.vbus_stat;
		}
		switch(prop_type)
		{
			case REG08_VBUS_TYPE_NONE:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
				break;
			case REG08_VBUS_TYPE_USB_SDP:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
			case REG08_VBUS_TYPE_USB_CDP:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
				break;
			case REG08_VBUS_TYPE_USB_DCP:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				break;
			case REG08_VBUS_TYPE_UNKNOWN:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
			case REG08_VBUS_TYPE_NON_STANDARD:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
			default:
				cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
				break;
		}
		pr_err("POWER_SUPPLY_PROP_TYPE=%d,state.vbus_stat=%d\n", cx->psy_desc.type, cx->state.vbus_stat);
		val->intval = cx->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		if (!cx->state.vbus_gd && !cx->online) {
			 dev_info(cx->dev, "vbus not good\n");
			 val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		} else {
			dev_info(cx->dev, "vbus is good\n");
			pr_err("POWER_SUPPLY_PROP_USB_TYPE=%d,state.vbus_stat=%d\n", cx->psy_usb_type, cx->state.vbus_stat);
			val->intval = cx->psy_usb_type;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = cx2560x_get_input_current_limit(cx);
		val->intval = (ret < 0) ? 0 : (ret * 1000);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = cx2560x_get_charging_status(cx);
		pr_err("original val=%d\n", ret);
		if(ret >= 0) {
			switch (ret) {
				case CHRG_STAT_NOT_CHARGING:
					// if (!pg_stat)
					// 	val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
					// else
						val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
					break;
				case CHRG_STAT_PRE_CHARGINT:
				case CHRG_STAT_FAST_CHARGING:
					val->intval = POWER_SUPPLY_STATUS_CHARGING;
					break;
/*+S98901AA1-10984 liangjianfeng,wt.High-temperature charger power display 84% stop charging,term is 450ma*/
				case CHRG_STAT_CHARGING_TERM:
					if (charge_done_flag)
						val->intval = POWER_SUPPLY_STATUS_FULL;
					else
						val->intval = POWER_SUPPLY_STATUS_CHARGING;
					break;
/*-S98901AA1-10984 liangjianfeng,wt.High-temperature charger power display 84% stop charging,term is 450ma*/
				default:
					val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
					break;
			}
			ret = 0;
		} else {
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		}
/*+S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
		if(cx->chg_old_status != POWER_SUPPLY_STATUS_CHARGING && val->intval == POWER_SUPPLY_STATUS_CHARGING) {
			if (cx->chg_psy) {
				power_supply_changed(cx->chg_psy);
				pr_err("%s:status=%d,%d\n", __func__, cx->chg_old_status, val->intval);
			} else {
				pr_err("%s: cx->chg_psy error!!!\n", __func__);
			}
		} else if ((cx->chg_old_status != val->intval) && (val->intval == POWER_SUPPLY_STATUS_FULL) && (charge_done_flag == true)) {
			if (cx->chg_psy) {
				power_supply_changed(cx->chg_psy);
				pr_err("%s:status=%d,%d\n", __func__, cx->chg_old_status, val->intval);
			} else {
				pr_err("%s: cx->chg_psy error!!!\n", __func__);
			}
		}
/*-S98901AA1-10226 liangjianfeng,wt.Low-temperature charger power display 97% stop charging*/
		cx->chg_old_status = val->intval;
		pr_err("charger status =%d\n", val->intval);
		break;
	default:
		ret = -ENODATA;
		break;
	}

	if (ret < 0){
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}
	return 0;
}


static int  cx2560x_charger_set_property(struct power_supply *psy,
					  enum power_supply_property psp,
					  const union power_supply_propval *val)
{
	struct cx2560x *cx = power_supply_get_drvdata(psy);
	struct cx2560x_state state;
	int ret = 0;
	pr_err("%s: psp=%d ,val->intval = %d\n", __func__, psp,val->intval);
	switch(psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = cx2560x_set_input_current_limit(cx, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		dev_info(cx->dev, "%s: online=%d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		cx->hub_check_chg = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!cx->state.vbus_gd && !cx->online) {
				dev_info(cx->dev, "vbus not good\n");
				cx->psy_usb_type =POWER_SUPPLY_USB_TYPE_UNKNOWN;
		} else {
			dev_info(cx->dev, "vbus is good\n");
			dev_info(cx->dev, "charger usb type: %d,state.vbus_stat=%d,cx->psy_usb_type=%d\n",
					val->intval, state.vbus_stat,cx->psy_usb_type);
		}
		power_supply_changed(cx->chg_psy);
		break;
	default:
		ret = -ENODATA;
		break;
	}

   return ret;
}

static const struct power_supply_desc cx2560x_charger_desc = {
  .name 		  = "primary_chg",
  .type 		  = POWER_SUPPLY_TYPE_USB,
  .properties	  =  cx2560x_charger_properties,
  .num_properties = ARRAY_SIZE(cx2560x_charger_properties),
  .get_property   =  cx2560x_charger_get_property,
  .set_property   =  cx2560x_charger_set_property,
  .property_is_writeable  =  cx2560x_charger_property_is_writeable,
  .usb_types	  =  cx2560x_charger_usb_types,
  .num_usb_types  = ARRAY_SIZE( cx2560x_charger_usb_types),
};

static char *cx2560x_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static struct power_supply *cx2560x_register_charger_psy(struct cx2560x *cx)
{
	struct power_supply_config psy_cfg = {
		.drv_data = cx,
		.of_node = cx->dev->of_node,
	};

	psy_cfg.supplied_to = cx2560x_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(cx2560x_charger_supplied_to);
	memcpy(&cx->psy_desc, &cx2560x_charger_desc, sizeof(cx->psy_desc));

	return power_supply_register(cx->dev,&cx->psy_desc,&psy_cfg);
}



static struct of_device_id cx2560x_charger_match_table[] = {
	{
	 .compatible = "cx,cx2560xd",
	 .data = &pn_data[PN_CX25601D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, cx2560x_charger_match_table);

static void get_power_off_charging(struct cx2560x *cx)
{
	struct tag_bootmode *tag = NULL;

	if (IS_ERR_OR_NULL(cx->boot_node))
		pr_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(cx->boot_node,
							"atag,boot", NULL);
		if (IS_ERR_OR_NULL(tag))
			pr_err("%s: failed to get atag,boot\n", __func__);
		else {
			pr_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			cx->bootmode = tag->bootmode;
			cx->boottype = tag->boottype;
			 pr_err("%s:bootmode = %d,boottype =%d\n", __func__,cx->bootmode,cx->boottype);

		}
	}

}

static int cx2560x_charger_remove(struct i2c_client *client);
static int cx2560x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct cx2560x *cx;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct platform_device  *usb_pdev;
	const char *compatible;
	int ret = 0;
	u8 reg_val = 0;
	char *name = NULL;

	cx = devm_kzalloc(&client->dev, sizeof(struct cx2560x), GFP_KERNEL);
	if (!cx)
		return -ENOMEM;

	client->addr = 0x6B;
	cx->dev = &client->dev;
	cx->client = client;
	cx->hub_check_chg = true;
	cx->reboot_flag = 0;
	cx->reboot_float_filter =0;
#ifdef WT_COMPILE_FACTORY_VERSION
	cx->probe_flag=0;
#endif
	i2c_set_clientdata(client, cx);

	mutex_init(&cx->i2c_rw_lock);
	mutex_init(&cx->i2c_up_lock);
	pr_err("%s:enter!\n", __func__);
	ret = cx2560x_detect_device(cx);
	if (ret) {
		pr_err("No cx2560x device found!\n");
		client->addr = 0x6B;
		cx->dev = &client->dev;
		cx->client = client;
		i2c_set_clientdata(client, cx);
		ret = cx2560x_detect_device(cx);
		if (ret) {
			pr_err("No cx2560x device found!\n");
			return -ENODEV;
		}
	}else{
		is_cx2560x = true;
		pr_err("charger ic is cx2560x\n");
	}
	match = of_match_node(cx2560x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	cx->platform_data = cx2560x_parse_dt(node, cx);
	if (!cx->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}
	cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;

	INIT_DELAYED_WORK(&cx->psy_dwork, cx2560x_inform_psy_dwork_handler);
	INIT_DELAYED_WORK(&cx->prob_dwork, cx2560x_inform_prob_dwork_handler);
	INIT_DELAYED_WORK(&cx->irq_work, cx2560x_charge_irq_work);
	INIT_DELAYED_WORK(&cx->charge_usb_detect_work, charger_usb_detect_work_func);
	INIT_DELAYED_WORK(&cx->charge_done_detect_work, charge_done_detect_work_func);
	INIT_DELAYED_WORK(&cx->recharge_detect_work, recharge_detect_work_func);
 	INIT_DELAYED_WORK(&cx->set_hiz_work, set_hiz_func);
	INIT_DELAYED_WORK(&cx->dump_work, dump_show_work);
	INIT_DELAYED_WORK(&cx->exit_hiz_work, check_hiz_remove);

        cx->chg_dev = charger_device_register(cx->chg_dev_name,
					&client->dev, cx,
					&cx2560x_chg_ops,
					&cx2560x_chg_props);

	if (IS_ERR_OR_NULL(cx->chg_dev)) {
		ret = PTR_ERR(cx->chg_dev);
		return ret;
	}


	ret = cx2560x_init_device(cx);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}
/*+S98901AA1.wt,MODIFY,20240628,add wakelock */
	name = devm_kasprintf(cx->dev,GFP_KERNEL,"%s","cx2560x suspend wakelock");
	cx->charger_wakelock = wakeup_source_register(cx->dev,name);
	if(IS_ERR_OR_NULL(cx->charger_wakelock)) {
		pr_err("Fail to register cx2560x charer wakelock\n");
	}
/*+S98901AA1.wt,MODIFY,20240628,add wakelock*/
	cx2560x_register_interrupt(cx);

	/* power supply register */
	cx->chg_psy = cx2560x_register_charger_psy(cx);
	if (IS_ERR_OR_NULL(cx->chg_psy)) {
		pr_err("Fail to register power supply dev\n");
		return  -EBUSY;
	}


	ret = sysfs_create_group(&cx->dev->kobj, &cx2560x_attr_group);
	if (ret)
		dev_err(cx->dev, "failed to register sysfs. err: %d\n", ret);

	mod_delayed_work(system_wq, &cx->prob_dwork,
					msecs_to_jiffies(2*1000));

	cx->otg_flag = 0;
	is_2rd_primary_chg_probe_ok = true;

    /* otg regulator */
	s_chg_dev_otg = cx->chg_dev;
	ret = cx2560x_vbus_regulator_register(cx->chg_dev);
	if (ret)
		dev_err(cx->dev, "failed to register cx2560x_vbus_regulator: %d\n", ret);

	ret = cx2560x_read_byte(cx, CX2560X_REG_0A, &reg_val);
	reg_val = (reg_val & REG0A_VBUS_GD_MASK) >> REG0A_VBUS_GD_SHIFT;
	if (reg_val) {
		pr_err("cx2560x_charger_probe gd = 1 \n");
		cancel_delayed_work(&cx->charge_done_detect_work);
		schedule_delayed_work(&cx->charge_done_detect_work, 0);
	}

	ret = cx2560x_read_byte(cx, CX2560X_REG_08, &reg_val);
	reg_val = (reg_val & REG08_VBUS_STAT_MASK) >> REG08_VBUS_STAT_SHIFT;
	if(reg_val == REG08_VBUS_TYPE_USB_SDP){
		pr_err("probe usb_detect \n");
		schedule_delayed_work(&cx->charge_usb_detect_work, 20* HZ);
	}

	// schedule_delayed_work(&cx->dump_work, 3* HZ);

	cx->usb_node =  of_parse_phandle(node,"usb",0);
	pr_err("usb node address: %p\n",cx->usb_node);
	if(!IS_ERR_OR_NULL(cx->usb_node)){
		if(of_property_read_string(cx->usb_node,"compatible",&compatible)==0){
			if(strcmp(compatible,"mediatek,mtu3")==0){
				pr_err("node compatible ok\n");
			}else{
				pr_err("node compatible fail\n");
			}
		}
		usb_pdev = of_find_device_by_node(cx->usb_node);
		if(IS_ERR_OR_NULL(usb_pdev)){
			pr_err("fail to find platform device\n");
		}
		of_node_put(cx->usb_node);

		cx->ssusb = platform_get_drvdata(usb_pdev);
		if (IS_ERR_OR_NULL(cx->ssusb) || IS_ERR_OR_NULL(cx->ssusb->u3d)) {
			pr_err("fail to get ssusb\n");
		}else{
			pr_err("usb gadget state = %d \n",cx->ssusb->u3d->g.state);
		}
		put_device(&usb_pdev->dev);
	}else{
		pr_err("fail to parse usb node \n");
	}
	cx->boot_node = of_parse_phandle(node, "bootmode", 0);
	get_power_off_charging(cx);

	if (cx->part_no == SGM41513D)
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SGM41513D");
	else {
		if(is_upm6922P)
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "UPM6922P");
		else if(is_upm6922)
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "UPM6922");
		else if(is_cx2560x)
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "CX25601D");
		else
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "UPM6910D");
	}
	cx->reboot_flag = 1;
	pr_err("cx2560x probe successfully, Part Num:%d, Revision:%d\n!",
	       cx->part_no, cx->revision);
	return 0;


}

static int cx2560x_charger_remove(struct i2c_client *client)
{
	struct cx2560x *cx = i2c_get_clientdata(client);
	cancel_delayed_work(&cx->charge_done_detect_work);
	cancel_delayed_work(&cx->recharge_detect_work);
	cancel_delayed_work(&cx->dump_work);
	cancel_delayed_work(&cx->irq_work);
	mutex_destroy(&cx->i2c_rw_lock);
	mutex_destroy(&cx->i2c_up_lock);
	power_supply_unregister(cx->chg_psy);
	power_supply_unregister(cx->ac_psy);
	sysfs_remove_group(&cx->dev->kobj, &cx2560x_attr_group);
	wakeup_source_destroy(cx->charger_wakelock);

	return 0;
}

/*+S96818AA1-9473 lijiawei,wt.PVT BA1 GN4 machine can not enter shipmode*/
extern bool factory_shipmode;
static void cx2560x_charger_shutdown(struct i2c_client *client)
{
	struct cx2560x *cx = i2c_get_clientdata(client);
	cx->reboot_flag = 0 ;
	cx->reboot_float_filter = 0;
	pr_err("cx2560x_charger_shutdown\n");
/* +S98901AA1-9560 liangjianfeng wt, modify, 20240727, modify for shipmode enter */
	cancel_delayed_work_sync(&cx->dump_work);
	cancel_delayed_work_sync(&cx->psy_dwork);
	cancel_delayed_work_sync(&cx->prob_dwork);
	cancel_delayed_work(&cx->irq_work);
	cancel_delayed_work_sync(&cx->charge_done_detect_work);
	cancel_delayed_work_sync(&cx->recharge_detect_work);
	cancel_delayed_work(&cx->charge_usb_detect_work);
#ifdef WT_COMPILE_FACTORY_VERSION
	if (factory_shipmode == true) {
		pr_err("cx2560x enter shipmode\n");
		cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		cx2560x_set_shipmode_delay(cx->chg_dev,false);
		msleep(15);
		cx2560x_set_shipmode(cx->chg_dev,true);
	}
#else

	if (factory_shipmode == true) {
		pr_err("cx2560x enter shipmode\n");
		cx2560x_update_bits(cx, CX2560X_REG_00, REG00_ENHIZ_MASK, REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT);
		cx2560x_set_shipmode_delay(cx->chg_dev,false);
		msleep(15);
		cx2560x_set_shipmode(cx->chg_dev,true);
	}
#endif
/* -S98901AA1-9560 liangjianfeng wt, modify, 20240727, modify for shipmode enter */

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

MODULE_DESCRIPTION("SUNCORE CX2560x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SUNCORE");
