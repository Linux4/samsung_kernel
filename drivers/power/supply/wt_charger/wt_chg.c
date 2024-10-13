/*
 * wingtch charger manage driver
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pm_wakeup.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/reboot.h>
#include "wt_chg.h"
#include "wt_chg_iio.h"
//+P86801AA1,peiyuexiang.wt,modify,2023/06/19,add charger_mode
#ifdef CONFIG_QGKI_BUILD
#include <linux/tp_notifier.h>
extern uint8_t Himax_USB_detect_flag;
extern int chipone_charger_mode_status;
#endif
//-P86801AA1,peiyuexiang.wt,modify,2023/06/19,add charger_mode

bool lcd_on_state=1;
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
bool batt_hv_disable = 0;
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
/* P86801AA1-13544, gudi1@wt, add 20231017, usb if*/
struct wt_chg *g_wt_chg = NULL;

enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,
};

static const char * const power_supply_usb_type_text[] = {
   "Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
   "USB_PD", "PD_DRP", "PD_PPS", "BrickID", "USB_HVDCP",
   "USB_HVDCP3","USB_HVDCP3P5", "USB_FLOAT","USB_AFC"
};

#ifdef WT_WL_CHARGE
static const char * const power_supply_wl_type_text[] = {
   "Unknown", "BPP", "10W", "EPP"
};
#endif

static const char * const power_supply_usbc_text[] = {
   "Nothing attached",
   "Source attached (default current)",
   "Source attached (medium current)",
   "Source attached (high current)",
   "Non compliant",
   "Sink attached",
   "Powered cable w/ sink",
   "Debug Accessory",
   "Audio Adapter",
   "Powered cable w/o sink",
};

static const char * const qc_power_supply_usb_type_text[] = {
   "HVDCP", "HVDCP_3", "HVDCP_3P5","USB_FLOAT","HVDCP_3"
};

static int thermal_mitigation[] = {
	4000000, 3500000, 3000000, 2500000,
	2000000, 1500000, 1000000, 500000,
};

static struct hvdcp30_profile_t  hvdcp30_profile[] = {
	{3400, 5600},
	{3500, 5600},
	{3600, 5600},
	{3700, 5600},
	{3800, 5600},
	{3900, 5600},
	{4000, 5600},
	{4100, 5600},
	{4200, 5600},
	{4300, 5600},
	{4400, 5600},
	{4500, 5600},
	{4600, 5600},
};

//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
static const char * const thermal_engine_type_names[] = {
	[USB_THERMAL_ENGINE_BC12] = "usb-thermal-engine-bc12-table",
	[USB_THERMAL_ENGINE_FCHG] = "usb-thermal-engine-fchg-table",
	[USB_THERMAL_ENGINE_WL_BPP] = "usb-thermal-engine-wl-bpp-table",
	[USB_THERMAL_ENGINE_WL_EPP] = "usb-thermal-engine-wl-epp-table",
	[USB_THERMAL_ENGINE_UNKNOW] = "usb-thermal-engine-unknow-table",
	[BOARD_THERMAL_ENGINE_BC12] = "board-thermal-engine-bc12-table",
	[BOARD_THERMAL_ENGINE_FCHG] = "board-thermal-engine-fchg-table",
	[BOARD_THERMAL_ENGINE_WL_BPP] = "board-thermal-engine-wl-bpp-table",
	[BOARD_THERMAL_ENGINE_WL_EPP] = "board-thermal-engine-wl-epp-table",
	[BOARD_THERMAL_ENGINE_UNKNOW] = "board-thermal-engine-unknow-table",
	[CHG_THERMAL_ENGINE_BC12] = "chg-thermal-engine-bc12-table",
	[CHG_THERMAL_ENGINE_FCHG] = "chg-thermal-engine-fchg-table",
	[CHG_THERMAL_ENGINE_WL_BPP] = "chg-thermal-engine-wl-bpp-table",
	[CHG_THERMAL_ENGINE_WL_EPP] = "chg-thermal-engine-wl-epp-table",
	[THERMAL_ENGINE_MAX] = "chg-thermal-engine-unknow-table",
};
//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

//+bug825533, tankaikun@wt, add 20230216, add software control iterm
static const char * const wt_chrg_state[] = {
	[CHRG_STATE_STOP]		= "DISCHARGING",
	[CHRG_STATE_FAST]		= "FAST_CHARGING",
	[CHRG_STATE_TAPER]		= "TAPER_CHARGING",
	[CHRG_STATE_FULL]		= "FULL",
};
//-bug825533, tankaikun@wt, add 20230216, add software control iterm

//bug 769367, tankaikun@wt, add 20220902, fix charge status is full but capacity is lower than 100%
static void wtchg_battery_check_eoc_condition(struct wt_chg *chg);
static void wtchg_init_chg_parameter(struct wt_chg *chg);

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
#ifdef CONFIG_QGKI_BUILD
static void wtchg_float_charge_type_check(struct work_struct *work);
static void wtchg_set_main_chg_rerun_apsd(struct wt_chg *chg, int val);
#endif
static inline bool is_device_suspended(struct wt_chg *chg)
{
	return !chg->resume_completed;
}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
static int wtchg_hv_disable(struct wt_chg *chg, int batt_hv_disable);
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

//+bug794981, tankaikun@wt, add 20220908, fix offmode usb/sdp charge timeout
static int get_boot_mode(){
	char *bootmode_string = NULL;
	char bootmode_start[32] = " ";
	int rc;

#ifdef CONFIG_QGKI_BUILD
	bootmode_string = strstr(saved_command_line,"androidboot.mode=");
#else
	bootmode_string="androidboot.mode=unknow";
#endif /* CONFIG_QGKI_BUILD */
	if(NULL != bootmode_string){
		strncpy(bootmode_start, bootmode_string+17, 7);
		rc = strncmp(bootmode_start, "charger", 7);
		if(0 == rc){
			pr_err("OFF charger mode\n");
			return 1;
		}
	}
	return 0;
}
//-bug794981, tankaikun@wt, add 20220908, fix offmode usb/sdp charge timeout

//+bug761884, tankaikun@wt, add 20220811, GKI charger bring up
int set_jeita_lcd_on_off(bool lcdon)
{
	lcd_on_state = lcdon;
	pr_err("%s, lcd_on %d\n", __func__, lcdon);
	return 0;
}
EXPORT_SYMBOL(set_jeita_lcd_on_off);

int get_jeita_lcd_on_off(void)
{
	return lcd_on_state;
}
//-bug761884, tankaikun@wt, add 20220811, GKI charger bring up

int usb_connect = false;
int usb_load_finish = false;
void wtchg_set_usb_connect(int connect)
{
	usb_load_finish = true;
	usb_connect = connect;
}
EXPORT_SYMBOL(wtchg_set_usb_connect);

static int wtchg_get_usb_connect(struct wt_chg *chg)
{
	chg->usb_connect = usb_connect;

	return chg->usb_connect;
}

#ifdef CONFIG_QGKI_BUILD
static int wtchg_is_usb_ready(struct wt_chg *chg)
{
	chg->usb_load_finish = usb_load_finish;

	return chg->usb_load_finish;
}
#endif

extern void charger_enable_device_mode(bool enable);

static int get_iio_channel(struct wt_chg *chg, const char *propname,
                                        struct iio_channel **chan)
{
        int ret = 0;

        ret = of_property_match_string(chg->dev->of_node,
                                        "io-channel-names", propname);
        if (ret < 0)
                return 0;

        *chan = iio_channel_get(chg->dev, propname);
        if (IS_ERR(*chan)) {
                ret = PTR_ERR(*chan);
                if (ret != -EPROBE_DEFER)
                        pr_err("%s channel unavailable, %d\n", propname, ret);
                *chan = NULL;
        }

        return ret;
}

struct iio_channel **wtchg_get_ext_channels(struct device *dev,

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


static bool wtchg_is_bms_chan_valid(struct wt_chg *chg,
		enum batt_qg_exit_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
//	if (IS_ERR(chg->gq_ext_iio_chans[chan]))
//		return false;
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

	if (!chg->gq_ext_iio_chans) {
		iio_list = wtchg_get_ext_channels(chg->dev, qg_ext_iio_chan_name,
		ARRAY_SIZE(qg_ext_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->gq_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->gq_ext_iio_chans = iio_list;
	}

	return true;
}

static bool wtchg_is_main_chg_chan_valid(struct wt_chg *chg,
		enum main_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->main_chg_ext_iio_chans) {
		iio_list = wtchg_get_ext_channels(chg->dev, main_iio_chan_name,
		ARRAY_SIZE(main_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->main_chg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->main_chg_ext_iio_chans = iio_list;
	}

	return true;
}


static bool wtchg_is_afc_chg_chan_valid(struct wt_chg *chg,
		enum main_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->afc_chg_ext_iio_chans) {
		iio_list = wtchg_get_ext_channels(chg->dev, afc_chg_iio_chan_name,
		ARRAY_SIZE(afc_chg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->afc_chg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->afc_chg_ext_iio_chans = iio_list;
	}

	return true;
}


static bool wtchg_is_wl_chg_chan_valid(struct wt_chg *chg,
		enum main_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->wl_chg_ext_iio_chans) {
		iio_list = wtchg_get_ext_channels(chg->dev, wl_chg_iio_chan_name,
		ARRAY_SIZE(wl_chg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get wl channels, %d\n",
					rc);
				chg->wl_chg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->wl_chg_ext_iio_chans = iio_list;
	}

	return true;
}

static int wtchg_read_iio_prop(struct wt_chg *chg,
				enum wtchg_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	switch (type) {
	case BMS:
		if (!wtchg_is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN_CHG:
		if (!wtchg_is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	case WL:
		if (!wtchg_is_wl_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->wl_chg_ext_iio_chans[channel];
		break;
	case AFC:
		if (!wtchg_is_afc_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->wl_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

static int wtchg_write_iio_prop(struct wt_chg *chg,
				enum wtchg_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	switch (type) {
	case BMS:
		if (!wtchg_is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN_CHG:
		if (!wtchg_is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	case WL:
		if (!wtchg_is_wl_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->wl_chg_ext_iio_chans[channel];
		break;
	case AFC:
		if (!wtchg_is_afc_chg_chan_valid(chg,channel))
			return -ENODEV;
		iio_chan_list = chg->afc_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

//+ bug 761884, tankaikun@wt, add 20220711, charger bring up
static void wtchg_stay_awake(struct wtchg_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(source->source);
		pr_debug("enabled source %s\n", source->source->name);
	}
}

static void wtchg_relax(struct wtchg_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(source->source);
		pr_debug("disabled source %s\n", source->source->name);
	}
}

static bool wtchg_wake_active(struct wtchg_wakeup_source *source)
{
	return !source->disabled;
}
//- bug 761884, tankaikun@wt, add 20220711, charger bring up
//+ EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit when c_to_c two device
#define MICRO_5V	5000000
#define MICRO_9V	9000000
#define MICRO_12V	12000000
static int wtchg_set_usb_pd_allowed_voltage(struct wt_chg *chg,
					int min_allowed_uv, int max_allowed_uv)
{
	int allowed_voltage;

	if (min_allowed_uv == MICRO_5V && max_allowed_uv == MICRO_5V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V;
	} else if (min_allowed_uv <= MICRO_9V && max_allowed_uv == MICRO_9V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_9V;
	} else if (min_allowed_uv == MICRO_12V && max_allowed_uv == MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_12V;
	} else if (min_allowed_uv < MICRO_9V && max_allowed_uv <= MICRO_9V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V;
	} else if (min_allowed_uv < MICRO_9V && max_allowed_uv <= MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_9V;
	} else if (min_allowed_uv < MICRO_12V && max_allowed_uv <= MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_12V;
	} else {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V;
		pr_err( "invalid allowed voltage [%d, %d]\n",  min_allowed_uv, max_allowed_uv);
		return -EINVAL;
	}

	chg->pd_allow_vol = allowed_voltage;

	return 0;
}

static int wtchg_set_prop_pd_voltage_min(struct wt_chg *chg,
				    int val)
{
	int rc, min_uv;

	min_uv = min(val, chg->pd_max_vol);
	rc = wtchg_set_usb_pd_allowed_voltage(chg, min_uv,
					       chg->pd_max_vol);
	if (rc < 0) {
		pr_err("invalid max voltage %duV rc=%d\n",
			val, rc);
		return rc;
	}

	chg->pd_min_vol = min_uv;

	return rc;
}

static int wtchg_set_prop_pd_voltage_max(struct wt_chg *chg,
				    int val)
{
	int rc, max_uv;

	max_uv = max(val, chg->pd_min_vol);
	rc = wtchg_set_usb_pd_allowed_voltage(chg, chg->pd_min_vol,
					       max_uv);
	if (rc < 0) {
		pr_err( "invalid min voltage %duV rc=%d\n",
			val, rc);
		return rc;
	}

	chg->pd_max_vol = max_uv;
	return rc;
}
//- EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit when c_to_c two device

static int wtchg_get_usb_online(struct wt_chg *chg)
{
	if (chg->vbus_online &&
		(chg->real_type == POWER_SUPPLY_TYPE_USB
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_CDP
			)) {
		chg->usb_online = true;
	} else {
		chg->usb_online = false;
	}

	return chg->usb_online;
}

static int wtchg_get_ac_online(struct wt_chg *chg)
{
	if (chg->vbus_online &&
		(chg->real_type == POWER_SUPPLY_TYPE_USB_DCP
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_FLOAT
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_PD
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_PD_DRP
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5
                        #ifdef CONFIG_QGKI_BUILD
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_AFC
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_PE
                        #endif
			)) {
		chg->ac_online = true;
	} else {
		chg->ac_online = false;
	}

	return chg->ac_online;
}

static int wtchg_get_usb_real_type(struct wt_chg *chg)
{
	return chg->real_type;
}

static int wtchg_get_charge_bc12_type(struct wt_chg *chg)
{
	int ret;
	int val;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get bc12 type \n");
		return chg->real_type;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wtchg_read_iio_prop(chg, MAIN_CHG, MAIN_CHARGER_TYPE, &val);
	if (ret < 0) {
		chg->real_type = 0;
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->real_type = val;
	#ifdef CONFIG_QGKI_BUILD
		if (chg->real_type == POWER_SUPPLY_TYPE_USB_NONSTAND_1A
				|| chg->real_type == POWER_SUPPLY_TYPE_USB_NONSTAND_1P5A) {
			chg->real_type = POWER_SUPPLY_TYPE_USB_DCP;
		}
	#endif
	}

	if (chg->pd_active) {
		chg->real_type = POWER_SUPPLY_TYPE_USB_PD;
	}

	return chg->real_type;
}

static int wtchg_get_vbus_online(struct wt_chg *chg)
{
	int ret;
	int val;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get vbus online \n");
		return chg->vbus_online;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wtchg_read_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_VBUS_ONLINE, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
#ifdef CONFIG_QGKI_BUILD
		if (!val && chg->vbus_online && !chg->typec_cc_orientation) {
			chg->vbus_online = false;
			wtchg_set_main_chg_rerun_apsd(chg, 0);
		} else if (!val && chg->typec_cc_orientation
			&& !wtchg_read_iio_prop(chg, MAIN_CHG, MAIN_CHARGER_HZ, &ret) && ret) {
			chg->vbus_online = true;
			wtchg_init_chg_parameter(chg);
			wtchg_set_main_chg_rerun_apsd(chg, 1);
		} else
#endif
			chg->vbus_online = val;
	}

	return chg->vbus_online;
}

static int wtchg_get_wl_type(struct wt_chg *chg)
{
	int ret;
	int val;

	ret = wtchg_read_iio_prop(chg, WL, WL_CHARGE_TYPE, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
		chg->wl_type = 0;
	} else {
		chg->wl_type = val;
	}

	return chg->wl_type;
}

static int wtchg_get_wl_present(struct wt_chg *chg)
{
	int ret;
	int val;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get wl online \n");
		return chg->wl_online;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wtchg_read_iio_prop(chg, WL, WL_CHARGE_VBUS_ONLINE, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->wl_online = val;
	}

	return chg->wl_online;
}

#ifdef WT_WL_CHARGE
static int wtchg_get_wl_online(struct wt_chg *chg)
{
	int ret;
	int val;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get wl online \n");
		return chg->wl_online;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wtchg_read_iio_prop(chg, WL, WL_CHARGE_VBUS_ONLINE, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		if (chg->vbus_online && val)
			chg->wl_online = true;
		else
			chg->wl_online = false;
	}

	return chg->wl_online;
}
#endif

static void wtchg_get_batt_time(struct wt_chg *chg)
{
	struct power_supply *psy = power_supply_get_by_name("bms");
	union power_supply_propval time_val;
	int ret;

	if(!psy){
		printk("bms psy get failed\n");
		return;
	}
	ret = power_supply_get_property(psy,POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,&time_val);
	chg->batt_time = time_val.intval;
}

static void wtchg_get_batt_cycle(struct wt_chg *chg)
{
	int ret = wtchg_read_iio_prop(chg, BMS, BATT_QG_BATTERY_CYCLE, &(chg->batt_cycle));

	dev_info(chg->dev, "%s: batt_cycle = %d, ret = %d\n", __func__, chg->batt_cycle, ret);
}

static void wtchg_set_batt_cycle(struct wt_chg *chg)
{
	int ret = wtchg_write_iio_prop(chg, BMS, BATT_QG_BATTERY_CYCLE, chg->batt_cycle);
	dev_info(chg->dev, "%s: chg->cycle= %d, ret = %d\n", __func__, chg->batt_cycle, ret);
}

static void wtchg_set_main_chg_ship_mode(struct wt_chg *chg)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_SET_SHIP_MODE, true);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_main_chg_otg(struct wt_chg *chg, int enable)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_OTG_ENABLE, enable);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
		return;
	}
	chg->otg_enable = enable;
}

static void wtchg_set_main_chg_volt(struct wt_chg *chg, int volt)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGER_VOLTAGE_TERM, volt);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_main_chg_current(struct wt_chg *chg, int curr)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHAGER_CURRENT, curr);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void  wtchg_set_main_input_current(struct wt_chg *chg, int curr)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_INPUT_CURRENT_SETTLED, curr);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_main_term_current(struct wt_chg *chg, int curr)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHAGER_TERM, curr);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_main_chg_enable(struct wt_chg *chg, int enable)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGING_ENABLED, enable);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_main_input_suspend(struct wt_chg *chg, int enable)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGER_HZ, enable);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}
//+P231018-03087 gudi.wt 20231103,fix add hiz func
#if 0
void __attribute__((weak)) typec_set_input_suspend_for_usbif_cx25890h(bool enable)
{
}
void __attribute__((weak)) typec_set_input_suspend_for_usbif_upm6910(bool enable)
{
}

/*+ P86801AA1-13544, gudi1@wt, add 20231017, usb if*/
void wtchg_turn_on_hiz(void)
{

	if(g_wt_chg == NULL){
		return;
	}

	typec_set_input_suspend_for_usbif_cx25890h(1);
	typec_set_input_suspend_for_usbif_upm6910(1);
	//	ret = wtchg_write_iio_prop(g_wt_chg, MAIN_CHG, MAIN_CHARGER_HZ, 1);
	//	if (ret < 0) {
	//		dev_err(g_wt_chg->dev, "%s: fail: %d\n", __func__, ret);
	//}

}
EXPORT_SYMBOL(wtchg_turn_on_hiz);

void wtchg_turn_off_hiz(void)
{
	if(g_wt_chg == NULL){
		return;
	}

	typec_set_input_suspend_for_usbif_cx25890h(0);
	typec_set_input_suspend_for_usbif_upm6910(0);
	//ret = wtchg_write_iio_prop(g_wt_chg, MAIN_CHG, MAIN_CHARGER_HZ, 0);
	//if (ret < 0) {
	//	 dev_err(g_wt_chg->dev, "%s: fail: %d\n", __func__, ret);
	//}

}
EXPORT_SYMBOL(wtchg_turn_off_hiz);
#endif
//-P231018-03087 gudi.wt 20231103,fix add hiz func
/*- P86801AA1-13544, gudi1@wt, add 20231017, usb if*/

#ifdef CONFIG_QGKI_BUILD
static void wtchg_set_main_chg_rerun_apsd(struct wt_chg *chg, int val)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_APSD_RERUN, val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}
#endif

static void wtchg_set_main_feed_wdt(struct wt_chg *chg)
{
	int ret;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_FEED_WDT, 1);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_afc_detect_9v(struct wt_chg *chg)
{
	int ret;

	return;
	dev_err(chg->dev, "%s:ENTER \n", __func__);
	ret = wtchg_write_iio_prop(chg, AFC, AFC_DETECT, 9);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void wtchg_set_slave_chg_ship_mode(struct wt_chg *chg)
{
	//do nothing

}

static void wtchg_set_slave_chg_volt(struct wt_chg *chg, int volt)
{
	//do nothing

}

static void wtchg_set_slave_chg_current(struct wt_chg *chg, int curr)
{
	//do nothing

}

static void wtchg_set_slave_input_current(struct wt_chg *chg, int curr)
{
	//do nothing

}

static void wtchg_set_slave_chg_enable(struct wt_chg *chg, int enable)
{
	//do nothing

}

static void wtchg_set_slave_input_suspend(struct wt_chg *chg, int enable)
{
	//do nothing

}

static void wtchg_set_slave_chg_monitor(struct wt_chg *chg, int enable)
{
	//do nothing
}

static int wtchg_set_adapter_voltage(struct wt_chg *chg, int volt)
{
	int ret = 0;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_SET_DP_DM, QTI_POWER_SUPPLY_DP_DM_HVDCP3_SUPPORTED);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}

	return ret;
}

static void wtchg_set_hvdcp_voltage(struct wt_chg *chg, int volt)
{
	//do nothing

}

static int wtchg_set_wl_voltage(struct wt_chg *chg, int volt)
{
	int ret = 0;

	ret = wtchg_write_iio_prop(chg, WL, WL_CHARGE_VBUS_VOLT, volt);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}

	return ret;
}

static int wtchg_set_wl_fod(struct wt_chg *chg, int dynamic_fod)
{
	int ret = 0;

	ret = wtchg_write_iio_prop(chg, WL, WL_CHARGE_FOD_UPDATE, dynamic_fod);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}

	return ret;
}

static int wtchg_set_chg_input_current(struct wt_chg *chg)
{
	int ret = 0;

	if (chg->slave_chg_enable) {
		chg->main_chg_input_curr = 1450; //chg->chg_type_input_curr /2 + 200;
		chg->slave_chg_input_curr = 650; //chg->chg_type_input_curr /2 + 200;
	} else {
		chg->main_chg_input_curr = chg->chg_type_input_curr;
		chg->slave_chg_input_curr = FCC_0_MA;
	}

	//set charge input current value
	//if (chg->chg_type_input_curr  != chg->chg_type_input_curr_pre) {
		chg->chg_type_input_curr_pre = chg->chg_type_input_curr;
		wtchg_set_main_input_current(chg, chg->main_chg_input_curr);

		if (chg->slave_chg_enable) {
			wtchg_set_slave_input_suspend(chg, false);
			wtchg_set_slave_input_current(chg, chg->slave_chg_input_curr);
			wtchg_set_slave_chg_monitor(chg, true);
		} else {
			wtchg_set_slave_input_suspend(chg, true);
			wtchg_set_slave_chg_monitor(chg, false);
		}
	//}

	return ret;
}

static int wtchg_set_chg_ibat_current(struct wt_chg *chg)
{
	int ret = 0;

	if (chg->slave_chg_enable) {
		chg->main_chg_ibatt = 2800;  //chg->chg_ibatt /2;
		chg->slave_chg_ibatt = 1200;  //chg->chg_ibatt /2;
	} else {
		chg->main_chg_ibatt = chg->chg_ibatt;
		chg->slave_chg_ibatt = FCC_0_MA;
	}

	dev_err(chg->dev,  "type_ibatt=%d, jeita_ibatt=%d, main_chg_ibatt=%d, slave_chg_ibatt=%d, chg_ibatt=%d,chg_ibatt_pre=%d\n",
		chg->chg_type_ibatt, chg->jeita_ibatt, chg->main_chg_ibatt,
		chg->slave_chg_ibatt, chg->chg_ibatt, chg->chg_ibatt_pre);

	//set charge ibat current value
	if (chg->chg_ibatt != chg->chg_ibatt_pre) {
		chg->chg_ibatt_pre = chg->chg_ibatt;
		wtchg_set_main_chg_current(chg, chg->main_chg_ibatt);
#ifdef ENABLE_HARD_TERM_CURRNET
		wtchg_set_main_term_current(chg, chg->chg_iterm);
#else
		wtchg_set_main_term_current(chg, 256);
#endif

		if (chg->slave_chg_enable) {
			wtchg_set_slave_chg_current(chg, chg->slave_chg_ibatt);
			wtchg_set_slave_chg_enable(chg, true);
		} else {
			wtchg_set_slave_chg_enable(chg, false);
		}
	}

	return ret;
}

/*
static int wtchg_get_vbus_voltage(struct wt_chg *chg)
{
	int ret;
	int val;

	ret = wtchg_read_iio_prop(chg, MAIN_CHG, MAIN_INPUT_VOLTAGE_SETTLED, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->vbus_volt = val;
	}

	return chg->vbus_volt;
}
*/

static int wtchg_get_vbus_voltage (struct wt_chg *chg)
{
	int ret, temp;

	if (!chg->main_chg_therm) {
		ret = get_iio_channel(chg, "vbus_dect", &chg->vbus_value);
		if (ret < 0) {
			pr_err("get vbus_dect channel fail: %d\n", ret);
		}
	}

	if (chg->main_chg_therm) {
		ret = iio_read_channel_processed(chg->vbus_value, &temp);
		if (ret < 0) {
			dev_err(chg->dev, "%s: read vbus_dect channel fail, ret=%d\n", __func__, ret);
		return ret;
		}
	} else {
		return -ENODATA;
	}

	chg->vbus_volt = temp / 100;
	dev_err(chg->dev," chg->vbus_volt = %d \n", chg->vbus_volt);

	return chg->vbus_volt;
}

static int wtchg_get_main_chg_temp(struct wt_chg *chg,  int *val)
{
	int ret, temp;

	if (!chg->main_chg_therm) {
		ret = get_iio_channel(chg, "chg_therm", &chg->main_chg_therm);
		if (ret < 0) {
			pr_err("get chg_therm fail: %d\n", ret);
		}
	}

	if (chg->main_chg_therm) {
		ret = iio_read_channel_processed(chg->main_chg_therm,
				&temp);
		if (ret < 0) {
			dev_err(chg->dev,
				"read main_chg_therm channel fail, ret=%d\n", ret);
			return ret;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return ret;
}

static int wtchg_get_usb_port_temp(struct wt_chg *chg,  int *val)
{
	int ret, temp;

	if (!chg->usb_port_therm) {
		ret = get_iio_channel(chg, "usb_port_therm", &chg->usb_port_therm);
		if (ret < 0) {
			pr_err("get usb_port_therm fail: %d\n", ret);
		}
	}

	if (chg->usb_port_therm) {
		ret = iio_read_channel_processed(chg->usb_port_therm,
				&temp);
		if (ret < 0) {
			dev_err(chg->dev,
				"read usb_port_therm channel fail, ret=%d\n", ret);
			return ret;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	if (chg->usb_temp_debug_flag == true) {
		*val = chg->usb_debug_temp;
	}

	return ret;
}

static int wtchg_get_board_pcb_temp(struct wt_chg *chg,  int *val)
{
	int ret, temp;

	if (!chg->board_pcb_therm) {
		ret = get_iio_channel(chg, "quiet_therm", &chg->board_pcb_therm);
		if (ret < 0) {
			pr_err("get board_pcb_therm fail: %d\n", ret);
		}
	}

	if (chg->board_pcb_therm) {
		ret = iio_read_channel_processed(chg->board_pcb_therm,
				&temp);
		if (ret < 0) {
			dev_err(chg->dev,
				"read board_pcb_therm channel fail, ret=%d\n", ret);
			return ret;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	if(chg->board_temp_debug_flag)
		*val = chg->board_debug_temp;

	return ret;
}

static int wtchg_get_batt_volt_max(struct wt_chg *chg)
{
	return chg->batt_cv_max;
}

static int wtchg_get_batt_current_max(struct wt_chg *chg)
{
	return chg->batt_fcc_max;
}

static int wtchg_get_batt_iterm_max(struct wt_chg *chg)
{
	return chg->batt_iterm;
}

static int wtchg_get_batt_status(struct wt_chg *chg)
{
	int ret;
	int val;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get batt status \n");
		return chg->chg_status;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wtchg_read_iio_prop(chg, MAIN_CHG, MAIN_CHARGER_STATUS, &val);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->chg_status = val;
	}

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	if (CHRG_STATE_FULL == chg->chg_state_step)
			chg->chg_status = POWER_SUPPLY_STATUS_FULL;
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm

	return chg->chg_status;
}

#ifdef CONFIG_QGKI_BUILD
static void wtchg_check_charging_time(struct wt_chg *chg)
{
	static struct timespec charging_begin_time;
	struct timespec time_now;

	if (chg->safety_timeout || chg->max_charging_time <= 0) {
		dev_info(chg->dev, "%s: safety_timeout %d %d, skip check\n", __func__,
			chg->safety_timeout, chg->max_charging_time);
	} else if (!chg->vbus_online || chg->chg_ibatt == 0 || chg->eoc_reported || chg->wt_discharging_state) {
		charging_begin_time = ktime_to_timespec(ktime_get_boottime());
		dev_info(chg->dev, "%s: %d %d %d %d not in charging, skip check\n", __func__,
			chg->vbus_online, chg->chg_ibatt, chg->eoc_reported, chg->wt_discharging_state);
	} else {
		time_now = ktime_to_timespec(ktime_get_boottime());

		if (time_now.tv_sec - charging_begin_time.tv_sec >= chg->max_charging_time) {
			dev_err(chg->dev, "%s: SW safety timeout: %d %d %d\n", __func__,
				charging_begin_time.tv_sec, time_now.tv_sec, chg->max_charging_time);
			chg->safety_timeout = true;
		}
	}
}
#endif

static const char * wtchg_get_batt_id_info(struct wt_chg *chg)
{
	chg->batt_id_string = "S88501_BATT";

	return chg->batt_id_string;
}

static int wtchg_get_batt_volt(struct wt_chg *chg)
{

	int volt, ret;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get batt volt \n");
		return chg->batt_volt;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
	//volt = 3800000;
	ret = wtchg_read_iio_prop(chg, BMS, BATT_QG_VOLTAGE_NOW, &volt);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

	if (volt >= BATT_VOLT_MAX) {
		volt = BATT_VOLT_MAX;
	}

	if (volt  <= BATT_VOLT_MIN && chg->vbus_online) {
		if ((chg->volt_zero_count++) >= VOLT_ZERO_MAX) {
			chg->volt_zero_count = 0;
			chg->batt_volt = volt;
		}
	} else if (volt  > BATT_VOLT_MIN) {
		chg->volt_zero_count = 0;
		chg->batt_volt = volt;
	}

	return chg->batt_volt;
}

static int wtchg_get_batt_current(struct wt_chg *chg)
{
	int cur, ret;

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get batt current \n");
		return chg->chg_current;
	}
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
	//chg->chg_current = 1000;
	ret = wtchg_read_iio_prop(chg, BMS, BATT_QG_CURRENT_NOW, &cur);

	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->chg_current = cur;
	}
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

	return chg->chg_current;
}

static int wtchg_get_batt_temp(struct wt_chg *chg)
{
	int temp, ret;

//+bug 761884, tankaikun@wt, add 20220125, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get batt_temp \n");
		return chg->batt_temp;
	}
//-bug 761884, tankaikun@wt, add 20220125, fix iic read/write return -13 when system suspend

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
	ret = wtchg_read_iio_prop(chg, BMS, BATT_QG_TEMP, &temp);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
		temp = chg->batt_temp;
	}

	//temp = DEFAULT_BATT_TEMP;
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

	if (temp <= BATT_TEMP_MIN) {
		temp = BATT_TEMP_MIN;
	} else if (temp >= BATT_TEMP_MAX) {
		temp = BATT_TEMP_MAX;
	}

	if (temp != BATT_TEMP_0) {
		chg->temp_zero_count = 0;
		chg->batt_temp = temp;
	} else {
		if ((chg->temp_zero_count++) >= TEMP_ZERO_MAX) {
			chg->temp_zero_count = 0;
			chg->batt_temp = temp;
		}
	}

	if (chg->batt_temp_debug_flag) {
		chg->batt_temp = chg->batt_debug_temp;
		dev_err(chg->dev,"lsw batt_temp_debug_flag=%d use batt_debug_temp=%d\n",
								chg->batt_temp_debug_flag, chg->batt_debug_temp);
	}

#ifdef CONFIG_WT_DISABLE_TEMP_CONTROL
		chg->batt_temp = 250;
#endif

	return chg->batt_temp;
}

static int wtchg_get_batt_capacity(struct wt_chg *chg)
{
	int ret, cap;
	int soc_change;

//+bug 761884, tankaikun@wt, add 20220125, fix iic read/write return -13 when system suspend
	if(is_device_suspended(chg)){
		pr_err(" is_device_suspended cannot get batt_capacity \n");
		return chg->batt_capacity;
	}
//-bug 761884, tankaikun@wt, add 20220125, fix iic read/write return -13 when system suspend

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
	ret = wtchg_read_iio_prop(chg, BMS, BATT_QG_CAPACITY, &cap);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	} else {
		chg->batt_capacity = cap;
	}
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

//+bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current
	if (chg->batt_capacity <= 99) {
		chg->batt_capacity_max = 99;
	} else if(chg->chg_status == POWER_SUPPLY_STATUS_FULL) {
		chg->batt_capacity_max = 100;
	}

	if (100 == chg->batt_capacity && 100 != chg->batt_capacity_max && chg->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (time_is_before_jiffies(chg->batt_full_check_timer + 2 * HZ)) {
			pr_err("update battery max capacity counter \n");
			if ((chg->chg_current <= BATTERY_FULL_TERM_CURRENT) || (chg->batt_time <= 15)){
				chg->batt_full_check_counter++;
			} else {
				chg->batt_full_check_counter--;
			}

			if (chg->batt_full_check_counter >= BATTERY_FULL_MAX_CNT_DESIGN) {
				chg->batt_full_check_counter=BATTERY_FULL_MAX_CNT_DESIGN;
				if ((chg->chg_current <= BATTERY_FULL_TERM_CURRENT) || (chg->batt_time == 0)){
					chg->batt_capacity_max = 100;
				}
			} else if(chg->batt_full_check_counter<0)
				chg->batt_full_check_counter = 0;

			chg->batt_full_check_timer = jiffies;
		}
	}

	if (chg->batt_capacity > chg->batt_capacity_max) {
		chg->batt_capacity = chg->batt_capacity_max;
	}
//-bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current

//+bug 761884, tankaikun.wt, mod, 2022/7/5, add batt_capacity_fake
	if(chg->batt_capacity_fake != -EINVAL)
	{
		chg->batt_capacity = chg->batt_capacity_fake;
	}
//-bug 761884, tankaikun.wt, mod, 2022/7/5, add batt_capacity_fake

//+bug 792983, tankaikun@wt, add 20220228, fix charge status is full but capacity is lower than 100%
	chg->batt_capacity_last = chg->batt_capacity;

	wtchg_battery_check_eoc_condition(chg);

	if(chg->batt_capacity_pre != -EINVAL) {
		if (abs(chg->batt_capacity-chg->batt_capacity_pre) > 3)
			chg->batt_capacity_pre = chg->batt_capacity;
		soc_change = min(MIN_SOC_CHANGE, abs(chg->batt_capacity - chg->batt_capacity_pre));
		if (chg->batt_capacity < chg->batt_capacity_pre && chg->batt_capacity != 0)
			chg->batt_capacity = chg->batt_capacity_pre - soc_change;
		if (chg->batt_capacity > chg->batt_capacity_pre)
			chg->batt_capacity = chg->batt_capacity_pre + soc_change;
	}
	chg->batt_capacity_pre = chg->batt_capacity;
//-bug 792983, tankaikun@wt, add 20220228, fix charge status is full but capacity is lower than 100%

	if (chg->batt_capacity == 0) {
		if (!((chg->usb_online || chg->ac_online) && (chg->batt_volt >= SHUTDOWN_BATT_VOLT))) {
			chg->shutdown_cnt++;
			if (chg->shutdown_cnt == SHUTDOWN_CNT_CHECK_VBAT) {
				chg->shutdown_cnt = 0;
				chg->shutdown_check_ok = true;
			}
		}
	}

	return chg->batt_capacity;
}

static int wtchg_get_batt_capacity_level(struct wt_chg *chg)
{
	if (chg->batt_capacity >= 100) {
		chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	} else if (chg->batt_capacity >= 80 && chg->batt_capacity < 100) {
		chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	} else if (chg->batt_capacity >= 20 && chg->batt_capacity < 80) {
		chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else if (chg->batt_capacity > 0 && chg->batt_capacity < 20) {
		chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	} else if (chg->batt_capacity == 0) {
		if ((chg->usb_online || chg->ac_online)
				&& (chg->batt_volt >= SHUTDOWN_BATT_VOLT)) {
			chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		} else {
			if (chg->shutdown_check_ok == true) {
				chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			}
		}
	} else {
		chg->batt_capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	}

	//dev_err(chg->dev,"batt_capacity_level = %d\n", chg->batt_capacity_level);

	return chg->batt_capacity_level;
}

static int wtchg_get_batt_health(struct wt_chg *chg)
{
	int ret = 0;

	chg->batt_temp = wtchg_get_batt_temp(chg);
	if (chg->batt_temp >= BATT_TEMP_MIN_THRESH &&
			chg->batt_temp < BATT_TEMP_3_THRESH) {
		chg->batt_health = POWER_SUPPLY_HEALTH_COLD;
	} else if (chg->batt_temp >= BATT_TEMP_3_THRESH &&
			chg->batt_temp < BATT_TEMP_20_THRESH) {
		chg->batt_health = POWER_SUPPLY_HEALTH_COOL;
	} else if (chg->batt_temp >= BATT_TEMP_20_THRESH &&
			chg->batt_temp < BATT_TEMP_45_THRESH) {
		chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
//+bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
	} else if (chg->batt_temp >= BATT_TEMP_45_THRESH &&
			chg->batt_temp < BATT_TEMP_52_THRESH) {
		chg->batt_health = POWER_SUPPLY_HEALTH_WARM;
	} else if (chg->batt_temp >= BATT_TEMP_52_THRESH) {
		chg->batt_health = POWER_SUPPLY_HEALTH_HOT;
//-bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
	} else {
		// out of range
		dev_err(chg->dev, "batt temp: out of range, ret=%d\n", ret);
	}

	return chg->batt_health;
}

static int wtchg_get_usb_max_current(struct wt_chg *chg)
{
	int input_curr_max = 0;

	if(chg->vbus_online){
		switch (chg->real_type) {
		case POWER_SUPPLY_TYPE_USB_PD:
		case POWER_SUPPLY_TYPE_USB_HVDCP:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
                #ifdef CONFIG_QGKI_BUILD
		case POWER_SUPPLY_TYPE_USB_AFC:
		case POWER_SUPPLY_TYPE_USB_PE:
		#endif
			input_curr_max = 1700;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
			if(chg->chg_current < 1500)
				input_curr_max = FCC_1000_MA;
			else
				input_curr_max = FCC_2000_MA;
			break;
		case POWER_SUPPLY_TYPE_USB_CDP:
		case POWER_SUPPLY_TYPE_USB:
		case POWER_SUPPLY_TYPE_USB_FLOAT:
			input_curr_max = FCC_500_MA;
			break;
		default:
			input_curr_max = FCC_500_MA;
			break;
		}
	}else{
		input_curr_max = FCC_0_MA;
	}

	chg->chg_type_input_curr_max = input_curr_max * 1000; // ua

	return chg->chg_type_input_curr_max;
}

static int wtchg_get_usb_max_voltage(struct wt_chg *chg)
{
	int input_voltage_max = 0;

	if(chg->vbus_online){
		switch (chg->real_type) {
		case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
			input_voltage_max = DEFAULT_HVDCP3_USB_VOLT_DESIGN;
			break;
		case POWER_SUPPLY_TYPE_USB_HVDCP:
			input_voltage_max = DEFAULT_HVDCP_USB_VOLT_DESIGN;
			break;
		case POWER_SUPPLY_TYPE_USB_PD:
		case POWER_SUPPLY_TYPE_USB_DCP:
		case POWER_SUPPLY_TYPE_USB_CDP:
		case POWER_SUPPLY_TYPE_USB:
		case POWER_SUPPLY_TYPE_USB_FLOAT:
			input_voltage_max = DEFAULT_USB_VOLT_DESIGN;
			break;
		default:
			input_voltage_max = DEFAULT_USB_VOLT_DESIGN;
			break;
		}
	}else{
		input_voltage_max = 0;
	}
	chg->chg_type_input_voltage_max = input_voltage_max * 1000; // uv

	return chg->chg_type_input_voltage_max;
}

static void wtchg_init_chg_parameter(struct wt_chg *chg)
{
	chg->chg_type_ibatt = FCC_0_MA;
	chg->chg_type_input_curr = FCC_0_MA;
	chg->chg_type_input_curr_pre = -1;

	chg->chg_ibatt = FCC_0_MA;
	chg->chg_ibatt_pre = -1;

	chg->jeita_ibatt = FCC_0_MA;
	chg->jeita_batt_cv = BATT_NORMAL_CV;
	chg->jeita_batt_cv_pre = FCC_0_MA;

	chg->pre_vbus = DEFAULT_HVDCP_VOLT;

	//bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current
	chg->batt_full_check_counter = 0;

	//+ bug 761884, tankaikun@wt, add 20220711, charger bring up
	set_bit(STEPPER_STATUS_DISABLE, &chg->fcc_stepper_status);
	set_bit(STEPPER_STATUS_DISABLE, &chg->icl_stepper_status);
	clear_bit(STEPPER_STATUS_COMPLETE, &chg->fcc_stepper_status);
	clear_bit(STEPPER_STATUS_COMPLETE, &chg->icl_stepper_status);

	chg->main_icl_ma = FCC_500_MA;
	chg->main_fcc_ma = FCC_500_MA;
	chg->fcc_step_ma = FCC_500_MA;
	chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_5000_MV;

	cancel_delayed_work(&chg->icl_stepper_work);
	cancel_delayed_work(&chg->fcc_stepper_work);
	if(wtchg_wake_active(&chg->wtchg_wl_wake_source)){
		wtchg_relax(&chg->wtchg_wl_wake_source);
	}
	//- bug 761884, tankaikun@wt, add 20220711, charger bring up

	if (chg->chg_init_flag == false) {
		chg->chg_init_flag = true;
		wtchg_set_slave_input_current(chg, FCC_500_MA);
		wtchg_set_slave_chg_enable(chg, false);
		wtchg_set_slave_input_suspend(chg, true);

		wtchg_set_main_chg_current(chg, FCC_500_MA);
		wtchg_set_main_input_current(chg, FCC_500_MA);
	}
}

static int wtchg_hvdcp30_request_adapter_voltage(struct wt_chg *chg, int chr_volt)
{
	int ret = 0;
	int retry_cnt = 0;
	int retry_cnt_max = 3;
	int vchr_before, vchr_after, vchr_delta;

	do {
		retry_cnt++;
		vchr_before = wtchg_get_vbus_voltage(chg);
		vchr_delta = abs(vchr_before - chr_volt);
		if(vchr_delta < 500){
			dev_err(chg->dev, "vbus have already set to chr_volt(%d:%d)\n", vchr_before, chr_volt);
			return ret;
		}
		wtchg_set_hvdcp_voltage(chg, chg->pre_vbus);
		ret = wtchg_set_adapter_voltage(chg, chr_volt);
		msleep(1000);
		vchr_after = wtchg_get_vbus_voltage(chg);
		wtchg_get_vbus_online(chg);
		vchr_delta = abs(vchr_after - chr_volt);
		if ((vchr_delta < 500) && (ret == 0)) {
			chg->pre_vbus = chr_volt;
			dev_err(chg->dev, "%s ok !: vchr = (%d, %d), vchr_target:%d\n",
						__func__, vchr_before, vchr_after, chr_volt);	
			return ret;
		}

		dev_err(chg->dev, "%s: retry_cnt = (%d, %d), vchr_before:%d, vchr_after:%d\n",
						__func__, retry_cnt, retry_cnt_max, vchr_before, vchr_after);
	} while (chg->vbus_online && (retry_cnt < retry_cnt_max));

	return ret;
}

static void wtchg_hvdcp30_start_algorithm(struct wt_chg *chg)
{
	int idx, size;
	int vbus;
	int vbat;

	vbat = wtchg_get_batt_volt(chg) /1000;
	size = ARRAY_SIZE(hvdcp30_profile);
	for (idx = 0; idx < size; idx++) {
		if (vbat > (hvdcp30_profile[idx].vbat + 100))
			continue;

		vbus = hvdcp30_profile[idx].vchr;
		wtchg_hvdcp30_request_adapter_voltage(chg, vbus);
		break;
	}

	wtchg_set_chg_input_current(chg);
	wtchg_set_chg_ibat_current(chg);

}

static int wtchg_set_hvdcp20_mode(struct wt_chg *chg, int mode)
{
	int ret = 0;

	ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_SET_DP_DM, mode);
	if (ret < 0) {
		dev_err(chg->dev, "%s: fail: %d\n", __func__, ret);
	}

	return ret;
}

static void wtchg_hvdcp20_start_algorithm(struct wt_chg *chg)
{
	int retry_cnt = 0;
	int retry_cnt_max = 3;
	int vchr_before, vchr_after;

	wtchg_get_vbus_voltage(chg);
	wtchg_get_batt_volt(chg);
	if (chg->vbus_volt <= HVDCP20_VOLT_6V) {
		do {
			retry_cnt++;
			vchr_before = wtchg_get_vbus_voltage(chg);
			wtchg_set_hvdcp20_mode(chg, QTI_POWER_SUPPLY_DP_DM_FORCE_9V);
			msleep(500);
			vchr_after = wtchg_get_vbus_voltage(chg);
			wtchg_get_vbus_online(chg);

			dev_err(chg->dev, "hvdcp20: retry_cnt = (%d, %d), vchr_before:%d, vchr_after:%d\n",
							retry_cnt, retry_cnt_max, vchr_before, vchr_after);
		} while (chg->vbus_online && (retry_cnt < retry_cnt_max)
						&& (vchr_after < HVDCP20_VOLT_6V));
	}

	wtchg_set_chg_input_current(chg);
	wtchg_set_chg_ibat_current(chg);
}

//+bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api
static void wtchg_ato_soc_user_control_work(struct work_struct *work)
{
	struct wt_chg *chg = container_of(work,
							struct wt_chg, ato_soc_user_control_work.work);

	chg->ato_soc_user_control = false;
	pr_err("wtchg_ato_soc_user_control_work user close ato control timeout \n");

	if (wtchg_wake_active(&chg->wtchg_ato_soc_wake_source)) {
		wtchg_relax(&chg->wtchg_ato_soc_wake_source);
	}
}
//-bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api

//+bug761884 tankaikun.wt, add, 20220709. Add charger current step control
static void wtchg_get_fcc_stepper_params(struct wt_chg *chg, int main_fcc_ma){
	pr_err("wtchg_get_fcc_stepper_params fcc_ua=%d fcc_ua_now=%d fcc_step_ma=%d \n", main_fcc_ma, chg->main_fcc_ma, chg->fcc_step_ma);

	chg->main_step_fcc_dir =
			(main_fcc_ma > chg->main_fcc_ma) ?
				STEP_UP : STEP_DOWN;
	if (main_fcc_ma == chg->main_fcc_ma) {
		chg->main_step_fcc_dir = 0;
	}
	chg->main_step_fcc_count =
			abs(main_fcc_ma - chg->main_fcc_ma) /
				chg->fcc_step_ma;
	chg->main_step_fcc_residual =
			abs(main_fcc_ma - chg->main_fcc_ma) %
				chg->fcc_step_ma;

	pr_err("wtchg_get_fcc_stepper_params dir=%d fcc_count=%d fcc_residual=%d\n",
				chg->main_step_fcc_dir, chg->main_step_fcc_count, chg->main_step_fcc_residual);
}

#ifdef WT_WL_CHARGE
static void wtchg_fcc_stepper_work(struct work_struct *work)
{
	struct wt_chg *chg = container_of(work,
							struct wt_chg, fcc_stepper_work.work);
	int reschedule_ms = 0;
	int main_fcc = chg->main_fcc_ma;

	pr_err("fcc_stepper_work main_fcc=%d fcc_step_ma=%d dir=%d \n", main_fcc, chg->fcc_step_ma, chg->main_step_fcc_dir);
	if (chg->vbus_online) {
		if (chg->main_step_fcc_count) {
			main_fcc += (chg->fcc_step_ma * chg->main_step_fcc_dir);
			chg->main_step_fcc_count--;
			if (chg->main_step_fcc_dir) {
				clear_bit(STEPPER_STATUS_DOWN, &chg->fcc_stepper_status);
				set_bit(STEPPER_STATUS_UP, &chg->fcc_stepper_status);
			} else {
				clear_bit(STEPPER_STATUS_UP, &chg->fcc_stepper_status);
				set_bit(STEPPER_STATUS_DOWN, &chg->fcc_stepper_status);
			}
			reschedule_ms = DEFAULT_FCC_STEP_UPDATE_DELAY_MS;
		}else if (chg->main_step_fcc_residual) {
			main_fcc += (chg->main_step_fcc_residual * chg->main_step_fcc_dir);
			chg->main_step_fcc_residual = 0;
		}

		if (!reschedule_ms) {
			clear_bit(STEPPER_STATUS_ENABLE, &chg->fcc_stepper_status);
			clear_bit(STEPPER_STATUS_DOWN, &chg->fcc_stepper_status);
			clear_bit(STEPPER_STATUS_UP, &chg->fcc_stepper_status);
			set_bit(STEPPER_STATUS_COMPLETE, &chg->fcc_stepper_status);
		}
	} else if (!chg->vbus_online && test_bit(STEPPER_STATUS_ENABLE, &chg->fcc_stepper_status)) {
		reschedule_ms = WIRELESS_WAIT_FOR_VBUS_MS;
	} else {
		reschedule_ms = 0;
	}

	chg->main_fcc_ma = main_fcc;
	pr_err("main_step_fcc_count = %d new_main_fcc=%d \n", chg->main_step_fcc_count, main_fcc);

	// set main_chg_ibat
	chg->chg_ibatt = main_fcc;
	wtchg_set_chg_ibat_current(chg);

	if (reschedule_ms) {
		schedule_delayed_work(&chg->fcc_stepper_work, msecs_to_jiffies(reschedule_ms));
		pr_err("Rescheduling FCC_STEPPER work\n");
		return;
	}else{
		if(wtchg_wake_active(&chg->wtchg_wl_wake_source)){
			wtchg_relax(&chg->wtchg_wl_wake_source);
		}
	}

	return;
}
#endif

static void wtchg_get_icl_stepper_params(struct wt_chg *chg, int main_wl_icl_ma){
	pr_err("get_icl_stepper_params target_icl_ua = %d icl_ua_now = %d \n",main_wl_icl_ma, chg->main_icl_ma);

	chg->main_step_wl_icl_dir =
			(main_wl_icl_ma > chg->main_icl_ma) ?
				STEP_UP : STEP_DOWN;
	if (main_wl_icl_ma == chg->main_icl_ma) {
		chg->main_step_wl_icl_dir = 0;
	}
	chg->main_step_wl_icl_count =
			abs(main_wl_icl_ma - chg->main_icl_ma) /
				DEFAULT_ICL_STEP_SIZE_MA;
	chg->main_step_wl_icl_residual =
			abs(main_wl_icl_ma - chg->main_icl_ma) %
				DEFAULT_ICL_STEP_SIZE_MA;
	pr_err("get_icl_stepper_params dir=%d usb_icl_count=%d usb_icl_residual=%d\n",chg->main_step_wl_icl_dir, chg->main_step_wl_icl_count,
						chg->main_step_wl_icl_residual);
}

#ifdef WT_WL_CHARGE
static void wtchg_icl_stepper_work(struct work_struct *work)
{
	struct wt_chg *chg = container_of(work,
							struct wt_chg, icl_stepper_work.work);
	int reschedule_ms = 0;
	int main_wl_icl_ma = chg->main_icl_ma;

	if(!wtchg_wake_active(&chg->wtchg_wl_wake_source)){
		wtchg_stay_awake(&chg->wtchg_wl_wake_source);
	}

	pr_err("icl_stepper_work main_usb_icl_ua=%d \n", main_wl_icl_ma);
	if (chg->vbus_online) {
		if (chg->main_step_wl_icl_count) {
			main_wl_icl_ma += (DEFAULT_ICL_STEP_SIZE_MA * chg->main_step_fcc_dir);
			chg->main_step_wl_icl_count--;
			if (chg->main_step_fcc_dir) {
				clear_bit(STEPPER_STATUS_DOWN, &chg->icl_stepper_status);
				set_bit(STEPPER_STATUS_UP, &chg->icl_stepper_status);
			} else {
				clear_bit(STEPPER_STATUS_UP, &chg->icl_stepper_status);
				set_bit(STEPPER_STATUS_DOWN, &chg->icl_stepper_status);
			}
			reschedule_ms = DEFAULT_ICL_STEP_UPDATE_DELAY_MS;
		} else if (chg->main_step_wl_icl_residual) {
			main_wl_icl_ma += (chg->main_step_wl_icl_residual * chg->main_step_fcc_dir);
			chg->main_step_wl_icl_residual = 0;
		}
		// start to step up ibat
		if(!reschedule_ms) {
			clear_bit(STEPPER_STATUS_ENABLE, &chg->icl_stepper_status);
			clear_bit(STEPPER_STATUS_UP, &chg->icl_stepper_status);
			clear_bit(STEPPER_STATUS_DOWN, &chg->icl_stepper_status);
			set_bit(STEPPER_STATUS_COMPLETE, &chg->icl_stepper_status);
			set_bit(STEPPER_STATUS_ENABLE, &chg->fcc_stepper_status);
			schedule_delayed_work(&chg->fcc_stepper_work, msecs_to_jiffies(0));
		}
	} else if (!chg->vbus_online && test_bit(STEPPER_STATUS_ENABLE, &chg->icl_stepper_status)) {
		reschedule_ms = WIRELESS_WAIT_FOR_VBUS_MS;
	} else {
		reschedule_ms = 0;
		main_wl_icl_ma = 0;
	}

	chg->main_icl_ma = main_wl_icl_ma;
	pr_err("icl_stepper_work main_step_wl_icl_count = %d new_main_wl_icl_ua=%d \n", chg->main_step_wl_icl_count, main_wl_icl_ma);

	// set main_chg_ibus
	chg->chg_type_input_curr = chg->main_icl_ma;
	wtchg_set_chg_input_current(chg);

	if (reschedule_ms) {
		schedule_delayed_work(&chg->icl_stepper_work, msecs_to_jiffies(reschedule_ms));
		pr_err("Rescheduling WL_ICL_STEPPER work\n");
		return;
	}

	return;
}
#endif

static void wtchg_get_wireless_params(struct wt_chg *chg)
{
	int wl_type=0;

	wl_type = wtchg_get_wl_type(chg);
	switch (wl_type) {
	case WIRELESS_VBUS_NONE:
	case WIRELESS_VBUS_WL_BPP:
		chg->fcc_step_ma = FCC_500_MA;
		chg->wireless_fcc_design_max = FCC_1000_MA;
		chg->wireless_icl_design_max = FCC_1000_MA;
		chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_5000_MV;
		break;
	case WIRELESS_VBUS_WL_10W_MODE:
		chg->fcc_step_ma = FCC_500_MA;
		chg->wireless_icl_design_max = FCC_1100_MA;
		chg->wireless_fcc_design_max = FCC_1500_MA;
		chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_9000_MV;
		break;
	case WIRELESS_VBUS_WL_EPP:
		chg->fcc_step_ma = FCC_1000_MA;
		if (chg->batt_capacity == 100 && !chg->lcd_on) {
			chg->wireless_fcc_design_max = FCC_1000_MA;
			chg->wireless_icl_design_max = FCC_500_MA;
		} else {
			chg->wireless_fcc_design_max = FCC_3000_MA;
			chg->wireless_icl_design_max = FCC_1250_MA;
		}
		chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_12000_MV;
		break;
	default:
		chg->fcc_step_ma = FCC_500_MA;
		chg->wireless_fcc_design_max = FCC_1000_MA;
		chg->wireless_icl_design_max = FCC_1000_MA;
		chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_5000_MV;
		pr_err("wireless type is not supported\n");
		break;
	}
}

static void wtchg_wireless_start_algorithm(struct wt_chg *chg) {
	// step1: set adapter voltage
	wtchg_set_wl_voltage(chg, chg->wireless_adapter_volt_design);

	// step2: update wireless fod
	if (chg->batt_capacity >= DYNAMIC_UPDATE_WIRELESS_FOD_CAPACITY) {
		wtchg_set_wl_fod(chg, 1);
	} else {
		wtchg_set_wl_fod(chg, 0);
	}

	// step3: set stepper current
	pr_err("wtchg_wireless_start_algorithm main_fcc=%d chg_ibatt=%d main_icl_ma=%d input_curr=%d\n", chg->main_fcc_ma, chg->chg_ibatt, chg->main_icl_ma, chg->chg_type_input_curr);
	if (test_bit(STEPPER_STATUS_DISABLE, &chg->icl_stepper_status) ||
		(chg->main_fcc_ma != chg->chg_ibatt && test_bit(STEPPER_STATUS_COMPLETE, &chg->fcc_stepper_status)) ||
		(chg->main_icl_ma != chg->chg_type_input_curr && test_bit(STEPPER_STATUS_COMPLETE, &chg->icl_stepper_status))) {
		pr_err("wtchg_wireless_start_algorithm modify current \n");
		chg->fcc_stepper_status = 0;
		set_bit(STEPPER_STATUS_ENABLE, &chg->icl_stepper_status);
		clear_bit(STEPPER_STATUS_DISABLE, &chg->icl_stepper_status);
		clear_bit(STEPPER_STATUS_COMPLETE, &chg->icl_stepper_status);

		wtchg_get_fcc_stepper_params(chg, chg->chg_ibatt);
		wtchg_get_icl_stepper_params(chg, chg->chg_type_input_curr);
		cancel_delayed_work_sync(&chg->icl_stepper_work);
		cancel_delayed_work_sync(&chg->fcc_stepper_work);
		schedule_delayed_work(&chg->icl_stepper_work, msecs_to_jiffies(10));
	}
}
//-bug761884 tankaikun.wt, add, 20220709. Add charger current step control

//+bug761884 tankaikun.wt, add, 20220709. Add lcd state notifier
#if defined(CONFIG_FB)
static int wtchg_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;
	struct wt_chg *chg = container_of(noti,
							struct wt_chg, wtchg_fb_notifier);

	pr_err("wtchg_fb_notifier_callback screen \n");

	if (!ev_data) {
		pr_err("evdata is null");
		return 0;
	}

	pr_err("wtchg_fb_notifier_callback screen \n");

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
        if (*blank == FB_BLANK_UNBLANK) {
			pr_err("wtchg_fb_notifier_callback screen on \n");
			chg->lcd_on = 1;
        }
    }

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
        if (*blank == FB_BLANK_POWERDOWN) {
			pr_err("wtchg_fb_notifier_callback screen off \n");
			chg->lcd_on = 0;
		}
	}

	return 0;
}

static int backlight_register_fb(struct wt_chg *wt_chg)
{
	memset(&wt_chg->wtchg_fb_notifier, 0, sizeof(wt_chg->wtchg_fb_notifier));
	wt_chg->wtchg_fb_notifier.notifier_call = wtchg_fb_notifier_callback;

	return fb_register_client(&wt_chg->wtchg_fb_notifier);
}

static void backlight_unregister_fb(struct wt_chg *wt_chg)
{
	fb_unregister_client(&wt_chg->wtchg_fb_notifier);
}

#endif /* CONFIG_FB */
//-bug761884 tankaikun.wt, add, 20220709. Add lcd state notifier

//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
static int wtchg_parse_thermal_engine_table(struct wt_chg *chg,
				const char *np_name,
				u32 *thermal_engine_tab_size,
				struct wtchg_thermal_engine_table **cur_table)
{
	struct device *dev = chg->dev;
	struct device_node *np = chg->dev->of_node;
	struct wtchg_thermal_engine_table *table;
	const __be32 *list;
	int i, size;
	u32 tab_size;

	list = of_get_property(np, np_name, &size);
	if (!list || !size)
		return 0;

	tab_size = size / (sizeof(struct wtchg_thermal_engine_table) /
				       sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(dev, sizeof(struct wtchg_thermal_engine_table) *
				(tab_size + 2), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 1; i < tab_size+1; i++) {
		table[i].temp = be32_to_cpu(*list++) - 1000;
		table[i].clr_temp = be32_to_cpu(*list++) - 1000;
		table[i].lcd_on_ibat = be32_to_cpu(*list++) / 1000;
		table[i].lcd_off_ibat = be32_to_cpu(*list++) / 1000;
		dev_dbg(chg->dev,"table[%d] np_name =%s temp:clr_temp:lcd_on_ibatt:lcd_off_ibatt= %d:%d:%d:%d\n",
				i,np_name, table[i].temp, table[i].clr_temp, table[i].lcd_on_ibat, table[i].lcd_off_ibat);
	}
	*cur_table = table;
	*thermal_engine_tab_size = tab_size;

	return 0;
}

static int wtchg_init_thermal_engine_table(struct wt_chg *chg)
{
	int ret, i;

	// usb thermal
	for (i = USB_THERMAL_ENGINE_BC12; i < USB_THERMAL_ENGINE_UNKNOW; i++) {
		ret = wtchg_parse_thermal_engine_table(chg,
					thermal_engine_type_names[i],
					&chg->thermal_engine_tab_size[i],
					&chg->thermal_engine_tab_array[i]);
		if (ret)
			return ret;
	}

	// board thermal
	for (i = BOARD_THERMAL_ENGINE_BC12; i < BOARD_THERMAL_ENGINE_UNKNOW; i++) {
		ret = wtchg_parse_thermal_engine_table(chg,
				   thermal_engine_type_names[i],
				   &chg->thermal_engine_tab_size[i],
				   &chg->thermal_engine_tab_array[i]);
		if (ret)
			return ret;
	}

	// chg thermal
	for (i = CHG_THERMAL_ENGINE_BC12; i < THERMAL_ENGINE_MAX; i++) {
		ret = wtchg_parse_thermal_engine_table(chg,
				   thermal_engine_type_names[i],
				   &chg->thermal_engine_tab_size[i],
				   &chg->thermal_engine_tab_array[i]);
		if (ret)
			return ret;
	}

	// init thermal table
	chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_BC12];
	chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_BC12];
	chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_BC12];

	return 0;
}

static int wtchg_get_thermal_status(struct wt_chg *chg, int cur_temp, int *last_temp, int last_thermal_status,
					struct wtchg_thermal_engine_table *thermal_engine_tab,
					u32 thermal_engine_tab_size)
{
	int i, temp_status;
	int thermal_status = last_thermal_status;

	for (i = thermal_engine_tab_size; i >= 1; i--) {
		if ((cur_temp >= thermal_engine_tab[i].temp && i >= 1)) {
			break;
		}
	}
	temp_status = i;

	if (thermal_status <= 0) {
		thermal_status = temp_status;
		goto out;
	}

	if (*last_temp >= cur_temp) { // temperature goes down
		if (cur_temp <= thermal_engine_tab[thermal_status].clr_temp) {
			dev_err(chg->dev,"temperature goes down cur_temp %d clr_temp=%d \n",cur_temp, thermal_engine_tab[thermal_status].clr_temp);
			thermal_status = thermal_status-1;
		}
	} else { // temperature goes up
		if (thermal_status != temp_status && cur_temp <= thermal_engine_tab[thermal_status].temp) {
			goto out;
		}
		dev_err(chg->dev,"temperature goes up \n");
		thermal_status = temp_status;
	}
out:
	*last_temp = cur_temp;
	dev_err(chg->dev,"thermal status:(%d) %d, temperature:%d, thermal_engine_tab_size:%d\n",
		 thermal_status, temp_status, cur_temp, thermal_engine_tab_size);
	return thermal_status;
}

static void wtchg_thermal_engine_machine(struct wt_chg *chg)
{
	static int cur_usb_thermal_status, cur_board_thermal_status, cur_chg_thermal_status;
	static int target_usb_port_thermal_cur, target_board_pcb_thermal_cur, target_main_chg_thermal_cur;
	u32 usb_port_tab_size, board_pcb_tab_size, main_chg_tab_size;
	int thermal_ibatt=0;

//P86801,gudi,20230624,add afc charge type
	switch (chg->real_type) {
	case POWER_SUPPLY_TYPE_USB_PD:
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
        #ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_TYPE_USB_AFC:
	#endif
		usb_port_tab_size = chg->thermal_engine_tab_size[USB_THERMAL_ENGINE_FCHG];
		board_pcb_tab_size = chg->thermal_engine_tab_size[BOARD_THERMAL_ENGINE_FCHG];
		main_chg_tab_size = chg->thermal_engine_tab_size[CHG_THERMAL_ENGINE_FCHG];
		if (usb_port_tab_size)
			chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_FCHG];
		if (board_pcb_tab_size)
			chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_FCHG];
		if (main_chg_tab_size)
			chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_FCHG];
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		usb_port_tab_size = chg->thermal_engine_tab_size[USB_THERMAL_ENGINE_FCHG];
		board_pcb_tab_size = chg->thermal_engine_tab_size[BOARD_THERMAL_ENGINE_FCHG];
		main_chg_tab_size = chg->thermal_engine_tab_size[CHG_THERMAL_ENGINE_FCHG];
		if (usb_port_tab_size)
			chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_FCHG];
		if (board_pcb_tab_size)
			chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_FCHG];
		if (main_chg_tab_size)
			chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_FCHG];
		break;
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		usb_port_tab_size = chg->thermal_engine_tab_size[USB_THERMAL_ENGINE_BC12];
		board_pcb_tab_size = chg->thermal_engine_tab_size[BOARD_THERMAL_ENGINE_BC12];
		main_chg_tab_size = chg->thermal_engine_tab_size[CHG_THERMAL_ENGINE_BC12];
		if (usb_port_tab_size)
			chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_BC12];
		if (board_pcb_tab_size)
			chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_BC12];
		if (main_chg_tab_size)
			chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_BC12];
		break;
	case POWER_SUPPLY_TYPE_WIRELESS:
		usb_port_tab_size = chg->thermal_engine_tab_size[USB_THERMAL_ENGINE_WL_EPP];
		board_pcb_tab_size = chg->thermal_engine_tab_size[BOARD_THERMAL_ENGINE_WL_EPP];
		main_chg_tab_size = chg->thermal_engine_tab_size[CHG_THERMAL_ENGINE_WL_EPP];
		if (usb_port_tab_size)
			chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_WL_EPP];
		if (board_pcb_tab_size)
			chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_WL_EPP];
		if (main_chg_tab_size)
			chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_WL_EPP];
		break;
	default:
		usb_port_tab_size = chg->thermal_engine_tab_size[USB_THERMAL_ENGINE_BC12];
		board_pcb_tab_size = chg->thermal_engine_tab_size[BOARD_THERMAL_ENGINE_BC12];
		main_chg_tab_size = chg->thermal_engine_tab_size[CHG_THERMAL_ENGINE_BC12];
		if (usb_port_tab_size)
			chg->usb_thermal_engine_tab = chg->thermal_engine_tab_array[USB_THERMAL_ENGINE_BC12];
		if (board_pcb_tab_size)
			chg->board_thermal_engine_tab = chg->thermal_engine_tab_array[BOARD_THERMAL_ENGINE_BC12];
		if (main_chg_tab_size)
			chg->chg_thermal_engine_tab = chg->thermal_engine_tab_array[CHG_THERMAL_ENGINE_BC12];
		break;
	}

	// usb port thermal control
	if (!chg->disable_usb_port_therm) {
		cur_usb_thermal_status = wtchg_get_thermal_status(chg, chg->usb_port_temp, &chg->usb_port_temp_pre,
									chg->last_usb_thermal_status,
									chg->usb_thermal_engine_tab,
									usb_port_tab_size);
		if (0 >= cur_usb_thermal_status) {
			target_board_pcb_thermal_cur = FCC_3000_MA;
		} else {
			if(chg->lcd_on)
				target_usb_port_thermal_cur = chg->usb_thermal_engine_tab[cur_usb_thermal_status].lcd_on_ibat;
			else
				target_usb_port_thermal_cur = chg->usb_thermal_engine_tab[cur_usb_thermal_status].lcd_off_ibat;
		}
		chg->last_board_thermal_status = cur_usb_thermal_status;
	} else {
		target_usb_port_thermal_cur = FCC_3000_MA;
		cur_usb_thermal_status = 0;
	}

	// board pcb thermal control
	if (!chg->disable_board_pcb_therm) {
		cur_board_thermal_status = wtchg_get_thermal_status(chg, chg->board_temp, &chg->board_temp_pre,
									chg->last_board_thermal_status,
									chg->board_thermal_engine_tab,
									board_pcb_tab_size);
		if (0 >= cur_board_thermal_status) {
			target_board_pcb_thermal_cur = FCC_3000_MA;
		} else {
			if(chg->lcd_on)
				target_board_pcb_thermal_cur = chg->board_thermal_engine_tab[cur_board_thermal_status].lcd_on_ibat;
			else
				target_board_pcb_thermal_cur = chg->board_thermal_engine_tab[cur_board_thermal_status].lcd_off_ibat;
		}
		chg->last_board_thermal_status = cur_board_thermal_status;
	} else {
		target_board_pcb_thermal_cur = FCC_3000_MA;
		cur_board_thermal_status = 0;
	}

	// main chg thermal control
	if (!chg->disable_main_chg_therm) {
		cur_chg_thermal_status = wtchg_get_thermal_status(chg, chg->main_chg_temp, &chg->main_chg_temp_pre,
									chg->last_chg_thermal_status,
									chg->chg_thermal_engine_tab,
									main_chg_tab_size);
		if (0 >= cur_chg_thermal_status) {
			target_main_chg_thermal_cur = FCC_3000_MA;
		} else {
			if(chg->lcd_on)
				target_main_chg_thermal_cur = chg->chg_thermal_engine_tab[cur_chg_thermal_status].lcd_on_ibat;
			else
				target_main_chg_thermal_cur = chg->chg_thermal_engine_tab[cur_chg_thermal_status].lcd_off_ibat;
		}
		chg->last_chg_thermal_status = cur_chg_thermal_status;
	} else {
		target_main_chg_thermal_cur = FCC_3000_MA;
		cur_chg_thermal_status = 0;
	}

	thermal_ibatt = min(target_main_chg_thermal_cur, target_board_pcb_thermal_cur);
	chg->thermal_ibatt = min(thermal_ibatt, target_usb_port_thermal_cur);

	dev_err(chg->dev,  "thermal_status: cur_usb=%d cur_board=%d, cur_chg=%d; last_usb=%d, last_board=%d, last_chg=%d \n",
				cur_usb_thermal_status, cur_board_thermal_status, cur_chg_thermal_status,
				chg->last_usb_thermal_status, chg->last_board_thermal_status, chg->last_chg_thermal_status);
	dev_err(chg->dev,  "usb_port_thermal_cur=%d main_chg_thermal_cur=%d, board_pcb_thermal_cur=%d thermal_ibatt=%d \n",
				target_usb_port_thermal_cur, target_main_chg_thermal_cur, target_board_pcb_thermal_cur, chg->thermal_ibatt);
}
//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
extern bool sgm41542_if_been_used(void);
extern bool cx25890h_if_been_used(void);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static void wtchg_sw_jeita_state_machine(struct wt_chg *chg)
{
	int batt_jeita_stat = BATT_TEMP_NORMAL;
	int batt_prev_low_temp = 0;
	int batt_prev_high_temp = 0;
	int temp_low_hysteresis = 0;
	int temp_high_hysteresis = 0;
	int before_hight_jeita_cv = 0;
	int board_therm_ibatt=0;
	int chg_therm_ibatt=0;
	int jeita_ibatt=0;

	//+bug 804274, tankaikun.wt, mod, 2022/9/22, offmode charge set lcd off
	if (get_boot_mode()) {
			chg->lcd_on = 0;
	} else {
			chg->lcd_on = get_jeita_lcd_on_off();
	}
	//-bug 804274, tankaikun.wt, mod, 2022/9/22, offmode charge set lcd off

	//check battery thermal status
	chg->batt_temp = wtchg_get_batt_temp(chg);
	if (chg->batt_temp > BATT_TEMP_MIN_THRESH &&
		chg->batt_temp < BATT_TEMP_0_THRESH) {
		batt_jeita_stat = BATT_TEMP_COLD;
	}else if(chg->batt_temp >= BATT_TEMP_0_THRESH &&
		chg->batt_temp < BATT_TEMP_3_THRESH){
		batt_jeita_stat = BATT_TEMP_COOL_2;
	} else if (chg->batt_temp >= BATT_TEMP_3_THRESH &&
		chg->batt_temp < BATT_TEMP_20_THRESH) {
		batt_jeita_stat = BATT_TEMP_COOL;
	} else if (chg->batt_temp >= BATT_TEMP_20_THRESH &&
		chg->batt_temp < BATT_TEMP_45_THRESH) {
		batt_jeita_stat = BATT_TEMP_NORMAL;
//+bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
	} else if (chg->batt_temp >= BATT_TEMP_45_THRESH &&
		chg->batt_temp < BATT_TEMP_52_THRESH) {
		batt_jeita_stat = BATT_TEMP_WARM;
	} else if (chg->batt_temp >= BATT_TEMP_52_THRESH) {
//-bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
		batt_jeita_stat = BATT_TEMP_HOT;
	} else {
		// out of range
	}

	//check and set battery thermal hysteresis
	if (batt_jeita_stat != chg->batt_jeita_stat_prev) {
		//+bug795378, tankaikun@wt, add, 2022/10/11, fix cool charging cannot work well
		if (-EINVAL == chg->batt_jeita_stat_prev) {
			chg->batt_jeita_stat_prev = batt_jeita_stat;
		}
		//-bug795378, tankaikun@wt, add, 2022/10/11, fix cool charging cannot work well
		switch (chg->batt_jeita_stat_prev) {
		case BATT_TEMP_COLD:
			batt_prev_low_temp = BATT_TEMP_MIN_THRESH;
			batt_prev_high_temp = BATT_TEMP_0_THRESH;
			temp_low_hysteresis = 0;
			temp_high_hysteresis = (temp_high_hysteresis + BATT_TEMP_HYSTERESIS);
			break;
		case BATT_TEMP_COOL:
			batt_prev_low_temp = BATT_TEMP_3_THRESH;
			batt_prev_high_temp = BATT_TEMP_20_THRESH;
			temp_low_hysteresis = 0;
			temp_high_hysteresis = (temp_high_hysteresis + BATT_TEMP_HYSTERESIS);
			break;
		case BATT_TEMP_COOL_2:
			batt_prev_low_temp = BATT_TEMP_0_THRESH;
			batt_prev_high_temp = BATT_TEMP_3_THRESH;
			temp_low_hysteresis = 0;
			temp_high_hysteresis = (temp_high_hysteresis + BATT_TEMP_HYSTERESIS);
			break;
		case BATT_TEMP_NORMAL:
			batt_prev_low_temp = BATT_TEMP_20_THRESH;
			batt_prev_high_temp = BATT_TEMP_45_THRESH;
			temp_low_hysteresis = 0;
			temp_high_hysteresis = 0;
			break;
		case BATT_TEMP_WARM:
			batt_prev_low_temp = BATT_TEMP_45_THRESH;
//bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
			batt_prev_high_temp = BATT_TEMP_52_THRESH;
			temp_low_hysteresis = (temp_low_hysteresis - BATT_TEMP_HYSTERESIS);
			temp_high_hysteresis = 0;
			break;
		case BATT_TEMP_HOT:
//bug 800574, liyiying.wt, mod, 2022/9/7, mod the max charge temperature
			batt_prev_low_temp = BATT_TEMP_52_THRESH;
			batt_prev_high_temp = BATT_TEMP_MAX_THRESH;
			temp_low_hysteresis = (temp_low_hysteresis - BATT_TEMP_HYSTERESIS);
			temp_high_hysteresis = 0;
			break;
		default:
			dev_err(chg->dev, "batt_jeita_stat_prev error !\n");
		}

		dev_err(chg->dev, "batt_jeita_stat_prev=%d, batt_jeita_stat=%d\n",
								chg->batt_jeita_stat_prev, batt_jeita_stat);
		if ((chg->batt_temp > (batt_prev_low_temp + temp_low_hysteresis)) &&
			(chg->batt_temp < (batt_prev_high_temp + temp_high_hysteresis))) {
			batt_jeita_stat = chg->batt_jeita_stat_prev;
		} else {
			chg->batt_jeita_stat_prev = batt_jeita_stat;
		}
	}

	if (batt_jeita_stat == BATT_TEMP_COLD) {
		chg->jeita_ibatt = FCC_0_MA;
		chg->jeita_batt_cv = BATT_NORMAL_CV;
	} else if (batt_jeita_stat == BATT_TEMP_COOL) {
		chg->jeita_ibatt = FCC_2000_MA;
		chg->jeita_batt_cv = BATT_NORMAL_CV;
	} else if (batt_jeita_stat == BATT_TEMP_COOL_2){
		chg->jeita_ibatt = FCC_2000_MA;
		chg->jeita_batt_cv = BATT_HIGH_TEMP_CV;
	}else if (batt_jeita_stat == BATT_TEMP_NORMAL) {
		chg->jeita_batt_cv = BATT_NORMAL_CV;
#if 1
		board_therm_ibatt = FCC_3000_MA;
#else
	if (chg->wl_online) {
		if (chg->board_temp < BOARD_TEMP_40_THRESH) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_3000_MA;
			} else {
				board_therm_ibatt = FCC_3000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_40_THRESH)
					&& (chg->board_temp < BOARD_TEMP_41_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1200_MA;
			} else {
				board_therm_ibatt = FCC_1600_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_41_THRESH)
					&& (chg->board_temp < BOARD_TEMP_42_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1200_MA;
			} else {
				board_therm_ibatt = FCC_1200_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_42_THRESH)
					&& (chg->board_temp < BOARD_TEMP_43_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_800_MA;
			} else {
				board_therm_ibatt = FCC_1000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_43_THRESH)
					&& (chg->board_temp < BOARD_TEMP_44_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_500_MA;
			} else {
				board_therm_ibatt = FCC_1000_MA;
			}
		} else if (chg->board_temp >= BOARD_TEMP_44_THRESH) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_500_MA;
			} else {
				board_therm_ibatt = FCC_1000_MA;
			}
		}
	} else {
		if (chg->board_temp < BOARD_TEMP_43_THRESH) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_3000_MA;
			} else {
				board_therm_ibatt = FCC_3000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_43_THRESH)
					&& (chg->board_temp < BOARD_TEMP_44_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_3000_MA;
			} else {
				board_therm_ibatt = FCC_2500_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_44_THRESH)
					&& (chg->board_temp < BOARD_TEMP_45_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_2000_MA;
			} else {
				board_therm_ibatt = FCC_2500_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_45_THRESH)
					&& (chg->board_temp < BOARD_TEMP_46_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1500_MA;
			} else {
				board_therm_ibatt = FCC_2000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_46_THRESH)
					&& (chg->board_temp < BOARD_TEMP_47_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1500_MA;
			} else {
				board_therm_ibatt = FCC_2000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_47_THRESH)
					&& (chg->board_temp < BOARD_TEMP_48_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1500_MA;
			} else {
				board_therm_ibatt = FCC_2000_MA;
			}
		} else if ((chg->board_temp >= BOARD_TEMP_48_THRESH)
					&& (chg->board_temp < BOARD_TEMP_49_THRESH)) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1500_MA;
			} else {
				board_therm_ibatt = FCC_1500_MA;
			}
		} else if (chg->board_temp >= BOARD_TEMP_49_THRESH) {
			if (chg->lcd_on) {
				board_therm_ibatt = FCC_1200_MA;
			} else {
				board_therm_ibatt = FCC_1500_MA;
			}
		}
	}
#endif
#if 1
		chg_therm_ibatt = FCC_3000_MA;
#else
		if (chg->wl_online) {
			if (chg->main_chg_temp < CHG_IC_TEMP_48_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_3000_MA;
				} else {
					chg_therm_ibatt = FCC_3000_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_48_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_52_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_1000_MA;
				} else {
					chg_therm_ibatt = FCC_1500_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_52_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_54_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_500_MA;
				} else {
					chg_therm_ibatt = FCC_800_MA;
				}
			} else if (chg->main_chg_temp >= CHG_IC_TEMP_54_THRESH) {
				chg_therm_ibatt = FCC_500_MA;
			}
		}else {
			if (chg->main_chg_temp < CHG_IC_TEMP_50_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_3000_MA;
				} else {
					chg_therm_ibatt = FCC_3000_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_50_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_52_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_2000_MA;
				} else {
					chg_therm_ibatt = FCC_2500_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_52_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_54_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_1500_MA;
				} else {
					chg_therm_ibatt = FCC_2000_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_54_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_56_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_1000_MA;
				} else {
					chg_therm_ibatt = FCC_1500_MA;
				}
			} else if (chg->main_chg_temp >= CHG_IC_TEMP_56_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_500_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			}
		}
#endif /* DISABLE_CHG_TEMP_CONTROL */
		jeita_ibatt = min(chg_therm_ibatt, board_therm_ibatt);
		chg->jeita_ibatt = jeita_ibatt;

	} else if (batt_jeita_stat == BATT_TEMP_WARM) {
		//chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
		before_hight_jeita_cv = chg->jeita_batt_cv;
		chg->jeita_batt_cv = BATT_HIGH_TEMP_CV;
#if 1
		board_therm_ibatt = FCC_1000_MA;
#else
		if (chg->wl_online) {
			if (chg->board_temp < BOARD_TEMP_42_THRESH) {
				if (chg->lcd_on) {
					board_therm_ibatt = FCC_1000_MA;
				} else {
					board_therm_ibatt = FCC_1000_MA;
				}
			} else if ((chg->main_chg_temp >= BOARD_TEMP_42_THRESH)
						&& (chg->main_chg_temp < BOARD_TEMP_43_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_800_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			} else if ((chg->main_chg_temp >= BOARD_TEMP_43_THRESH)
						&& (chg->main_chg_temp < BOARD_TEMP_44_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_500_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			} else if (chg->board_temp >= BOARD_TEMP_44_THRESH) {
				if (chg->lcd_on) {
					board_therm_ibatt = FCC_500_MA;
				} else {
					board_therm_ibatt = FCC_1000_MA;
				}
			}
		} else {
			board_therm_ibatt = FCC_1000_MA;
		}
#endif /* DISABLE_BOARD_TEMP_CONTROL */

#if 1
		chg_therm_ibatt = FCC_1000_MA;
#else
		if (chg->wl_online) {
			if (chg->main_chg_temp < CHG_IC_TEMP_52_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_1000_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			} else if ((chg->main_chg_temp >= CHG_IC_TEMP_52_THRESH)
						&& (chg->main_chg_temp < CHG_IC_TEMP_54_THRESH)) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_500_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			} else if (chg->main_chg_temp >= CHG_IC_TEMP_54_THRESH) {
				chg_therm_ibatt = FCC_500_MA;
			}
		}else {
			if (chg->main_chg_temp < CHG_IC_TEMP_56_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_1000_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			} else if (chg->main_chg_temp >= CHG_IC_TEMP_56_THRESH) {
				if (chg->lcd_on) {
					chg_therm_ibatt = FCC_500_MA;
				} else {
					chg_therm_ibatt = FCC_1000_MA;
				}
			}
		}
#endif /* DISABLE_CHG_TEMP_CONTROL */
		jeita_ibatt = min(chg_therm_ibatt, board_therm_ibatt);
		chg->jeita_ibatt = jeita_ibatt;

		//+chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
		if (chg->vbat_over_flag == 1) {
			chg->jeita_ibatt = FCC_0_MA;
			chg->jeita_batt_cv = before_hight_jeita_cv;
		}

		if (time_is_before_jiffies(chg->last_vbat_update + 10 * HZ)) {

			chg->batt_volt = wtchg_get_batt_volt(chg);
			chg->vbat_update_time++;

			if (chg->batt_volt > ((BATT_HIGH_TEMP_CV + 30)*1000)) {
				chg->vbat_over_count++;
			}

			if (chg->vbat_update_time == 6) {
				if (chg->vbat_over_count == 6) {
					chg->jeita_ibatt = FCC_0_MA;
					chg->jeita_batt_cv = before_hight_jeita_cv;
					chg->vbat_over_flag = 1;
				} else {
					chg->vbat_over_flag = 0;
				}

				chg->vbat_update_time = 0;
				chg->vbat_over_count = 0;
			}

			chg->last_vbat_update = jiffies;
		}
		//-chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
	} else if (batt_jeita_stat == BATT_TEMP_HOT) {
		chg->jeita_ibatt = FCC_0_MA;
		chg->jeita_batt_cv = BATT_HIGH_TEMP_CV;
	}

	//+P86801AA1-3622, gudi.wt, 20230705, battery protect func

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
	if(sgm41542_if_been_used()){
		if (chg->jeita_batt_cv > BATT_HIGH_TEMP_CV) {
			wtchg_get_batt_cycle(chg);
			if (chg->batt_cycle > 999)
				chg->jeita_batt_cv = 4240;    //1000+
			else if (chg->batt_cycle > 699)
				chg->jeita_batt_cv = 4280;    //700-999
			else if (chg->batt_cycle > 399) 
				chg->jeita_batt_cv = 4310;    //400-699
			else if (chg->batt_cycle > 299)  
				chg->jeita_batt_cv = 4330;    //300-399
			else
				chg->jeita_batt_cv = 4350;    //0-299
		}
	}else if(cx25890h_if_been_used()){
		if (chg->jeita_batt_cv > BATT_HIGH_TEMP_CV) {
			wtchg_get_batt_cycle(chg);
			if (chg->batt_cycle > 999)
				chg->jeita_batt_cv = 4272;    //1000+
			else if (chg->batt_cycle > 699)
				chg->jeita_batt_cv = 4320;    //700-999
			else if (chg->batt_cycle > 399)
				chg->jeita_batt_cv = 4336;    //400-699
			else if (chg->batt_cycle > 299)
				chg->jeita_batt_cv = 4352;    //300-399
			else
				chg->jeita_batt_cv = 4368;    //0-299
		}
	}
#endif
//-P86801EA2-300 gudi.wt battery protect function


	dev_err(chg->dev, "batt_temp=%d, jeita_status=%d, board_temp=%d,lcd_on=%d, vbat_over_flag=%d, vbat_update_time=%d, vbat_over_count=%d, batt_volt=%d, batt_cycle=%d\n",
				chg->batt_temp, batt_jeita_stat, chg->board_temp, chg->lcd_on, chg->vbat_over_flag,
				chg->vbat_update_time, chg->vbat_over_count, chg->batt_volt,chg->batt_cycle);
}

#define KERNEL_POWER_OFF_VOLTAGE 3200000
/*
static void wtchg_kernel_power_off_check(struct wt_chg *chg)
{
	static int count = 0;

	wtchg_get_batt_volt(chg);
	wtchg_get_batt_capacity(chg);

	if(chg->batt_volt <= KERNEL_POWER_OFF_VOLTAGE && chg->vbus_online && chg->batt_capacity < 1){
		count ++;
		pr_err("wtchg kernel power off, batt_volt:%d;count:%d\n",chg->batt_volt, count);
	}else{
		count = 0;
	}

	if(count >= 5){
#ifdef CONFIG_QGKI_BUILD
		kernel_power_off();
#endif
		pr_err("wtchg batt_volt lower than 3.2V, kernel power off\n");
	}

}
*/

//+bug825533, tankaikun@wt, add 20230216, add software control iterm
#define HYST_STEP_UV	50000
#define	ITERM_OFFSET_UA	5000

static void wtchg_battery_status_check(struct wt_chg *chg)
{
	int max_fv_uv, recharg_offset_vol_uv;
	int ibat_ua;
	int design_max_fv_uv;

	max_fv_uv = chg->jeita_batt_cv * 1000;
	design_max_fv_uv = BATT_NORMAL_CV * 1000;
	ibat_ua = chg->chg_current*1000-ITERM_OFFSET_UA;
	recharg_offset_vol_uv = chg->batt_recharge_offset_vol;

	if (CHRG_STATE_STOP == chg->chg_state_step) {
		if (chg->usb_online ||chg->ac_online || chg->wl_online) {
			chg->chg_state_step = CHRG_STATE_FAST;
			pr_err("wtchg charge state from DISCHARGE change to FAST \n");
		}
	} else if (CHRG_STATE_FAST == chg->chg_state_step) {
		if ((chg->batt_volt+HYST_STEP_UV) > max_fv_uv) {
			chg->chg_status_hyst_cnt++;
			if (chg->chg_status_hyst_cnt > 3) {
				chg->chg_state_step = CHRG_STATE_TAPER;
				chg->chg_status_hyst_cnt = 0;
				pr_err("wtchg charge state from FAST change to TAPER \n");
			} else {
				chg->chg_state_step = CHRG_STATE_FAST;
			}
		} else {
			chg->chg_status_hyst_cnt = 0;
		}
	} else if (CHRG_STATE_TAPER == chg->chg_state_step) {
		if ((chg->batt_volt+HYST_STEP_UV) < max_fv_uv) {
			chg->chg_status_hyst_cnt++;
			if (chg->chg_status_hyst_cnt > 3) {
				chg->chg_state_step = CHRG_STATE_FAST;
				chg->chg_status_hyst_cnt = 0;
				pr_err("wtchg charge state from TAPER change to FAST \n");
			} else {
				chg->chg_state_step = CHRG_STATE_TAPER;
			}
		} else {
			chg->chg_status_hyst_cnt = 0;
			chg->chg_state_step = CHRG_STATE_TAPER;
		}
		//P231120-04562 liwei19.wt 20231226, solve the problem that soc may be less than 100 when cutoff charging.
		if ((100 == chg->batt_capacity) && (ibat_ua <= chg->batt_iterm)
			&& (0 == chg->wt_discharging_state) && (0 != chg->chg_ibatt)
			&& time_is_before_jiffies(chg->chg_state_update_timer + 5*HZ)) {
			if(!wtchg_wake_active(&chg->wtchg_iterm_wake_source)){
				wtchg_stay_awake(&chg->wtchg_iterm_wake_source);
			}
			chg->chg_state_update_timer = jiffies;
			chg->chg_iterm_cnt++;
			if (chg->chg_iterm_cnt > 3) {
				chg->chg_state_step = CHRG_STATE_FULL;
				chg->chg_iterm_cnt = 0;
				chg->chg_status_hyst_cnt = 0;
				if (wtchg_wake_active(&chg->wtchg_iterm_wake_source)) {
					wtchg_relax(&chg->wtchg_iterm_wake_source);
				}
				pr_err("wtchg charge state from TAPER change to FULL \n");
			}
		} else if ((100 != chg->batt_capacity) || (ibat_ua > chg->batt_iterm)) {
				chg->chg_iterm_cnt = 0;
				if (wtchg_wake_active(&chg->wtchg_iterm_wake_source)) {
					wtchg_relax(&chg->wtchg_iterm_wake_source);
				}
		}
	}else if (CHRG_STATE_FULL == chg->chg_state_step) {
		if (chg->batt_volt < (max_fv_uv-recharg_offset_vol_uv)) {
			chg->chg_status_hyst_cnt++;
			if (chg->chg_status_hyst_cnt > 3) {
				chg->chg_status_hyst_cnt = 0;
				chg->chg_state_step = CHRG_STATE_TAPER;
				pr_err("wtchg charge state change to RECHARGE vbat \n");
			}
		} else if (chg->batt_capacity <= chg->batt_capacity_recharge && design_max_fv_uv == max_fv_uv) {
			chg->chg_status_hyst_cnt = 0;
			chg->chg_state_step = CHRG_STATE_TAPER;
			pr_err("wtchg charge state change to RECHARGE soc \n");
		} else {
			chg->chg_status_hyst_cnt = 0;
		}
	} else {
		if ((chg->batt_volt+HYST_STEP_UV) > max_fv_uv)
			chg->chg_state_step = CHRG_STATE_TAPER;
		else
			chg->chg_state_step = CHRG_STATE_FAST;
	}

	switch (chg->chg_state_step) {
		case CHRG_STATE_FAST:
		case CHRG_STATE_TAPER:
		case CHRG_STATE_STOP:
			chg->chg_status_target_fcc = chg->chg_type_ibatt;
			break;
		case CHRG_STATE_FULL:
#ifdef ENABLE_HARD_TERM_CURRNET
			chg->chg_status_target_fcc = chg->chg_type_ibatt;

#else
			chg->chg_status_target_fcc = 0;
#endif
			break;
		default:
			chg->chg_status_target_fcc = chg->chg_type_ibatt;
			break;
	}

	pr_err("chg_state_step=%s chg_status_hyst_cnt=%d chg_iterm_cnt=%d chg_status_target_fcc=%d\n",
				wt_chrg_state[chg->chg_state_step], chg->chg_status_hyst_cnt, chg->chg_iterm_cnt, chg->chg_status_target_fcc);

	return;
}
//-bug825533, tankaikun@wt, add 20230216, add software control iterm

static void wtchg_charge_strategy_machine(struct wt_chg *chg)
{
	//check and set battery thermal hysteresis
	wtchg_sw_jeita_state_machine(chg);
	wtchg_thermal_engine_machine(chg); //bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	switch (chg->real_type) {
	case POWER_SUPPLY_TYPE_USB_PD:
	//+ EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit
	//	chg->chg_type_ibatt = FCC_3000_MA;
	//	chg->chg_type_input_curr = FCC_1700_MA;
		if (chg->pd_cur_max >= PD_INPUT_CURRENT_2000_MA && chg->pd_allow_vol>=USBIN_ADAPTER_ALLOW_9V) { //9V1.7A
			chg->chg_type_ibatt = FCC_3000_MA;
			chg->chg_type_input_curr = FCC_1700_MA;
		} else if (chg->pd_cur_max >= PD_INPUT_CURRENT_1500_MA && chg->pd_allow_vol>=USBIN_ADAPTER_ALLOW_9V) { // 9V1.7A
			chg->chg_type_ibatt = FCC_3000_MA;
			chg->chg_type_input_curr = FCC_1700_MA;
		} else if (chg->pd_cur_max >= PD_INPUT_CURRENT_1000_MA && chg->pd_allow_vol>=USBIN_ADAPTER_ALLOW_9V) { // 9V1.0A
			chg->chg_type_ibatt = FCC_3000_MA;
			chg->chg_type_input_curr = FCC_1000_MA;
		} else if (chg->pd_cur_max >= PD_INPUT_CURRENT_3000_MA && chg->pd_allow_vol>=USBIN_ADAPTER_ALLOW_5V) { // 5V3A
			chg->chg_type_ibatt = FCC_3000_MA;
			chg->chg_type_input_curr = FCC_1700_MA;
		} else if (chg->pd_cur_max  >= PD_INPUT_CURRENT_2000_MA  && chg->pd_allow_vol >= USBIN_ADAPTER_ALLOW_5V) { // 5V2A
			chg->chg_type_ibatt = FCC_2000_MA;
			chg->chg_type_input_curr = FCC_1700_MA;
		} else if (chg->pd_cur_max  >= PD_INPUT_CURRENT_500_MA  && chg->pd_allow_vol >= USBIN_ADAPTER_ALLOW_5V) { // 5V0.5A
			chg->chg_type_ibatt = FCC_500_MA;
			chg->chg_type_input_curr = FCC_500_MA;
		} else if (chg->pd_cur_max >=  PD_INPUT_CURRENT_100_MA  && chg->pd_allow_vol >= USBIN_ADAPTER_ALLOW_5V) { // 5V0.1A
			chg->chg_type_ibatt = FCC_100_MA;
			chg->chg_type_input_curr = FCC_100_MA;
		} else if (chg->pd_cur_max  ==  PD_INPUT_CURRENT_0_MA) { // 5V0A
			chg->chg_type_ibatt = FCC_0_MA;
			chg->chg_type_input_curr = FCC_100_MA;
		} else {
			chg->chg_type_ibatt = FCC_500_MA;
			chg->chg_type_input_curr = FCC_500_MA;
		}
	//+ EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_TYPE_USB_AFC:
	#endif
		chg->chg_type_ibatt = FCC_3000_MA;
		chg->chg_type_input_curr = FCC_1700_MA;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
		chg->chg_type_ibatt = FCC_3000_MA;
		chg->chg_type_input_curr = FCC_1700_MA;
		break;
	#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_TYPE_USB_PE:
		chg->chg_type_ibatt = FCC_3000_MA;
		chg->chg_type_input_curr = FCC_1550_MA;
		break;
	case POWER_SUPPLY_TYPE_USB_NONSTAND_1A:
		chg->chg_type_ibatt = FCC_1000_MA;
		chg->chg_type_input_curr = FCC_1000_MA;
		break;
	case POWER_SUPPLY_TYPE_USB_NONSTAND_1P5A:
		chg->chg_type_ibatt = FCC_1500_MA;
		chg->chg_type_input_curr = FCC_1500_MA;
		break;
	#endif
	case POWER_SUPPLY_TYPE_USB_DCP:
		//bug761884, liyiying.wt, mod, 2022/8/3,mod the DCP current
		chg->chg_type_ibatt = FCC_3000_MA;
		chg->chg_type_input_curr = FCC_1550_MA;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
	#ifdef WT_COMPILE_FACTORY_VERSION
		chg->chg_type_ibatt = FCC_500_MA;
		chg->chg_type_input_curr = FCC_500_MA;
	#else
		chg->chg_type_ibatt = FCC_1500_MA;
		chg->chg_type_input_curr = FCC_1500_MA;
	#endif
		break;
	case POWER_SUPPLY_TYPE_USB:
		chg->chg_type_ibatt = FCC_500_MA;
		chg->chg_type_input_curr = FCC_500_MA;
		break;
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		chg->chg_type_ibatt = FCC_500_MA;
		chg->chg_type_input_curr = FCC_500_MA;
		break;
	case POWER_SUPPLY_TYPE_WIRELESS:
		wtchg_get_wireless_params(chg);
		chg->chg_type_ibatt = chg->wireless_fcc_design_max;
		chg->chg_type_input_curr = chg->wireless_icl_design_max;
		break;
	default:
		chg->chg_type_ibatt = FCC_500_MA;
		chg->chg_type_input_curr = FCC_500_MA;
		break;
	}

	//bug825533, tankaikun@wt, add 20230216, add software control iterm
	wtchg_battery_status_check(chg);

	//choose smaller charging current
	if (chg->chg_type_ibatt <= chg->jeita_ibatt) {
		chg->chg_ibatt = chg->chg_type_ibatt;
	} else {
		chg->chg_ibatt = chg->jeita_ibatt;
	}

#ifdef CONFIG_QGKI_BUILD
	if (chg->store_mode) {
		chg->chg_ibatt = FCC_200_MA;
	}
#endif

	//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
	if (chg->chg_ibatt > chg->thermal_ibatt)
		chg->chg_ibatt = chg->thermal_ibatt;
	//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	if (chg->chg_ibatt > chg->chg_status_target_fcc)
		chg->chg_ibatt = chg->chg_status_target_fcc;
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm

#ifdef CONFIG_QGKI_BUILD
	if (chg->safety_timeout)
		chg->chg_ibatt = 0;
#endif

	if (chg->chg_ibatt > CHG_CURRENT_BYPASS_VALUE
		&& chg->batt_capacity < SLAVE_CHG_START_SOC_MAX) {
		chg->slave_chg_enable = false;
	} else {
		chg->slave_chg_enable = false;
	}

	dev_err(chg->dev,  "jeita_batt_cv=%d, jeita_batt_cv_pre=%d, input_curr=%d, input_curr_pre=%d, chg_ibatt=%d slave_enable=%d\n",
				chg->jeita_batt_cv, chg->jeita_batt_cv_pre,
				chg->chg_type_input_curr, chg->chg_type_input_curr_pre,
				chg->chg_ibatt, chg->slave_chg_enable);

	//set charge cv value
	if (chg->jeita_batt_cv != chg->jeita_batt_cv_pre) {
		chg->jeita_batt_cv_pre = chg->jeita_batt_cv;
		wtchg_set_main_chg_volt(chg, chg->jeita_batt_cv);
		wtchg_set_slave_chg_volt(chg, chg->jeita_batt_cv);
	} 

#ifdef CONFIG_QGKI_BUILD
	if (batt_hv_disable == 0) {
#endif
	switch (chg->real_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:

		wtchg_hvdcp20_start_algorithm(chg);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3P5:
        #ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_TYPE_USB_PE:
        #endif
		wtchg_hvdcp30_start_algorithm(chg);
		break;
	case POWER_SUPPLY_TYPE_WIRELESS:
		wtchg_wireless_start_algorithm(chg);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_FLOAT:
	case POWER_SUPPLY_TYPE_USB_PD:
        #ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_TYPE_USB_AFC:
	#endif
		wtchg_set_afc_detect_9v(chg);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		break;
	default:
		break;
	}
#ifdef CONFIG_QGKI_BUILD
	}else{
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
	}
#endif
}

static void wtchg_set_charge_work(struct wt_chg *chg)
{
	cancel_delayed_work(&chg->wt_chg_work);
	schedule_delayed_work(&chg->wt_chg_work, 0);
}

static int wtchg_batt_init_config(struct wt_chg *chg)
{
	int ret = 0;

	chg->usb_online = false;
	chg->ac_online = false;
	chg->wl_online = false;
	chg->usb_load_finish = false;

	chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	chg->batt_temp = DEFAULT_BATT_TEMP;
	chg->batt_capacity = 50;
	chg->batt_volt = 3800000;
	chg->jeita_batt_cv = chg->batt_cv_max;
	chg->chg_iterm = 240;
	chg->chg_type_input_curr_max = 0;

	chg->chg_init_flag = true;
	chg->safety_timeout = false;
	chg->max_charging_time = 17 * 60 * 60;
	chg->wt_discharging_state = 0;
	chg->batt_full_capacity = 100;
	chg->lcd_on = lcd_on_state;

	chg->batt_temp_debug_flag = false;
	chg->board_temp_debug_flag = false;
	chg->usb_temp_debug_flag = false;

	chg->pre_vbus = DEFAULT_HVDCP_VOLT;
	chg->ship_mode = 0;

	// bug 761884, tankaikun@wt, add 20220711, otg state
	chg->otg_enable = 0;

	// bug 761884, tankaikun@wt, add 20220725, add fake soc
	chg->batt_capacity_fake = -EINVAL;

	// bug 761884, tankaikun@wt, add 20220711, charge current stepper
	chg->fcc_stepper_status = 0;
	chg->icl_stepper_status = 0;

	//+chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
	chg->last_vbat_update = jiffies;
	chg->vbat_update_time = 0;
	chg->vbat_over_count = 0;
	chg->vbat_over_flag = 0;
	//-chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value

	// bug 761884, tankaikun@wt, add 20220805, fix hix mode usb state update slowly
	chg->start_charging_test = true;

	//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
	chg->last_usb_thermal_status = -1;
	chg->last_board_thermal_status = -1;
	chg->last_chg_thermal_status = -1;
	//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	//bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	chg->resume_completed = true;

	//+bug761884, tankaikun@wt, add 20220711, wireless step charge 
	set_bit(STEPPER_STATUS_DISABLE, &chg->fcc_stepper_status);
	set_bit(STEPPER_STATUS_DISABLE, &chg->icl_stepper_status);
	clear_bit(STEPPER_STATUS_COMPLETE, &chg->fcc_stepper_status);
	clear_bit(STEPPER_STATUS_COMPLETE, &chg->icl_stepper_status);

	chg->main_icl_ma = FCC_500_MA;
	chg->main_fcc_ma = FCC_500_MA;
	chg->fcc_step_ma = FCC_500_MA;
	chg->wireless_adapter_volt_design = WIRELESS_ADAPER_VOLT_5000_MV;
	//-bug761884, tankaikun@wt, add 20220711, wireless step charge 

	//bug795378, tankaikun@wt, add, 2022/10/11, fix cool charging cannot work well
	chg->batt_jeita_stat_prev = -EINVAL;

	//bug761884, tankaikun@wt, add 20221101, gki charger bring up
	chg->wtchg_alarm_init = false;

	//+bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current
	chg->batt_capacity_max = 100;
	chg->batt_full_check_counter = 0;
	chg->batt_full_check_timer = 0;
	//-bug 806590, tankaikun.wt, mod, 2022/11/9, limit report 100% capacity term current

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	chg->chg_state_step = CHRG_STATE_STOP;
	chg->chg_iterm_cnt = 0;
	chg->chg_status_hyst_cnt = 0;
	chg->chg_status_target_fcc = FCC_3000_MA;
	chg->chg_state_update_timer = jiffies;
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm

	chg->shutdown_check_ok = false;
	chg->shutdown_cnt = 0;

	return ret;
}

static int wtchg_batt_set_system_temp_level(struct wt_chg *chg,
				const union power_supply_propval *val)
{
	int cur;

	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval > chg->thermal_levels)
		return -EINVAL;

	chg->system_temp_level = val->intval;
	cur = thermal_mitigation[val->intval] / 1000;

	pr_err("batt_set_prop_system_temp_levels %d, cur = %d\n", chg->system_temp_level, cur);

	return 0;
}

#define DISCHARGING_STATE_ATO			0x0001
#define DISCHARGING_STATE_MAINTAIN		0x0002
#define DISCHARGING_STATE_PROTECT		0x0004
#define DISCHARGING_STATE_STOREMODE		0x0008
#define DISCHARGING_STATE_FULLCAPACITY	0x0020
#define DISCHARGING_STATE_SLATEMODE		0x0100
#define DISCHARGING_BY_DISABLE			0x00FF
#define DISCHARGING_BY_HIZ				0xFF00
static int wtchg_ato_charge_manage(struct wt_chg *chg)
{
	int ret = 0;

#ifdef WT_COMPILE_FACTORY_VERSION
	if ((chg->wt_discharging_state & DISCHARGING_STATE_ATO) != 0 &&
			(chg->ato_soc_user_control || chg->batt_capacity <= ATO_BATT_SOC_MIN)) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_ATO;
		dev_err(chg->dev, "%s, capacity:%d %d, ato_soc_user_control:%d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_capacity, ATO_BATT_SOC_MIN, chg->ato_soc_user_control, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_DISABLE) == 0) {
			wtchg_set_main_chg_enable(chg, true);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_ATO) != 0 ||
			(!chg->ato_soc_user_control && chg->batt_capacity >= ATO_BATT_SOC_MAX)) {
		chg->wt_discharging_state |= DISCHARGING_STATE_ATO;
		chg->chg_ibatt = FCC_0_MA;
		chg->chg_type_input_curr = FCC_100_MA;
		wtchg_set_main_chg_enable(chg, false);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		dev_err(chg->dev, "%s, capacity:%d %d, stop charging\n", __func__, chg->batt_capacity, ATO_BATT_SOC_MAX);
	}
#endif

	return ret;
}

static int wtchg_battery_maintain_charge_manage(struct wt_chg *chg)
{
	int ret = 0;

	if ((chg->wt_discharging_state & DISCHARGING_STATE_MAINTAIN) != 0 &&
			(!chg->batt_maintain_mode || chg->batt_capacity <= BATT_MAINTAIN_SOC_MIN)) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_MAINTAIN;
		dev_err(chg->dev, "%s, capacity:%d %d, batt_maintain_mode:%d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_capacity, BATT_MAINTAIN_SOC_MIN, chg->batt_maintain_mode, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_DISABLE) == 0) {
			wtchg_set_main_chg_enable(chg, true);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_MAINTAIN) != 0 ||
			(chg->batt_maintain_mode && chg->batt_capacity >= BATT_MAINTAIN_SOC_MAX)) {
		chg->wt_discharging_state |= DISCHARGING_STATE_MAINTAIN;
		chg->chg_ibatt = FCC_0_MA;
		chg->chg_type_input_curr = FCC_100_MA;
		wtchg_set_main_chg_enable(chg, false);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		dev_err(chg->dev, "%s, capacity:%d %d, stop charging\n", __func__, chg->batt_capacity, BATT_MAINTAIN_SOC_MAX);
	}

	return ret;
}

static int wtchg_battery_protect_charge_manage(struct wt_chg *chg)
{
	int ret = 0;

	if ((chg->wt_discharging_state & DISCHARGING_STATE_PROTECT) != 0 &&
		(!chg->batt_protected_mode && chg->batt_capacity <= BATT_PROTECT_SOC_MIN)) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_PROTECT;
		dev_err(chg->dev, "%s, capacity:%d %d, batt_protected_mode:%d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_capacity, BATT_PROTECT_SOC_MIN, chg->batt_protected_mode, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_DISABLE) == 0) {
			wtchg_set_main_chg_enable(chg, true);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_PROTECT) != 0 ||
			(chg->batt_protected_mode && chg->batt_capacity >= BATT_PROTECT_SOC_MAX)) {
		chg->wt_discharging_state |= DISCHARGING_STATE_PROTECT;
		chg->chg_ibatt = FCC_0_MA;
		chg->chg_type_input_curr = FCC_100_MA;
		wtchg_set_main_chg_enable(chg, false);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		dev_err(chg->dev, "%s, capacity:%d %d, stop charging\n", __func__, chg->batt_capacity, BATT_PROTECT_SOC_MAX);
	}

	return ret;
}

#ifdef CONFIG_QGKI_BUILD
static int wtchg_store_mode_manage(struct wt_chg *chg)
{
	int ret = 0;

	if ((chg->wt_discharging_state & DISCHARGING_STATE_STOREMODE) != 0 &&
			(!chg->store_mode || chg->batt_capacity <= STORE_MODE_SOC_MIN)) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_STOREMODE;
		dev_err(chg->dev, "%s, capacity:%d %d, store_mode:%d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_capacity, STORE_MODE_SOC_MIN, chg->store_mode, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_DISABLE) == 0) {
			wtchg_set_main_chg_enable(chg, true);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_STOREMODE) != 0 ||
			(chg->store_mode && chg->batt_capacity >= STORE_MODE_SOC_MAX)) {
		chg->wt_discharging_state |= DISCHARGING_STATE_STOREMODE;
		chg->chg_ibatt = FCC_0_MA;
		chg->chg_type_input_curr = FCC_100_MA;
		wtchg_set_main_chg_enable(chg, false);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		dev_err(chg->dev, "%s, capacity:%d %d, stop charging\n", __func__, chg->batt_capacity, STORE_MODE_SOC_MAX);
	}

	return ret;
}

static int wtchg_battery_slate_mode_manage(struct wt_chg *chg)
{
	int ret = 0;
	/*+P231218-05362  guhan01.wt 20231223,modify for Node batt_ Slate_ When the mode write value is 3,*/
	if (chg->batt_slate_mode == SEC_SMART_SWITCH_SRC)
		return ret;
	/*-P231218-05362  guhan01.wt 20231223,modify for Node batt_ Slate_ When the mode write value is 3*/
	if ((chg->wt_discharging_state & DISCHARGING_STATE_SLATEMODE) != 0 &&
			!chg->batt_slate_mode) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_SLATEMODE;
		dev_err(chg->dev, "%s, batt_slate_mode:%d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_slate_mode, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_HIZ) == 0) {
			wtchg_set_main_input_suspend(chg, false);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_SLATEMODE) != 0 ||
			chg->batt_slate_mode) {
		chg->wt_discharging_state |= DISCHARGING_STATE_SLATEMODE;
		wtchg_set_main_input_suspend(chg, true);
		dev_err(chg->dev, "%s, stop charging\n", __func__);
	}

	return ret;
}

static int wtchg_batt_full_capacity_manage(struct wt_chg *chg)
{
	int ret = 0;

	if ((chg->wt_discharging_state & DISCHARGING_STATE_FULLCAPACITY) != 0 &&
			(chg->batt_full_capacity == 100 || chg->batt_capacity <= (chg->batt_full_capacity - 2))) {
		chg->wt_discharging_state &= ~DISCHARGING_STATE_FULLCAPACITY;
		dev_err(chg->dev, "%s, capacity:%d %d, discharging_state:%d, ready charging\n", __func__,
			chg->batt_capacity, chg->batt_full_capacity, chg->wt_discharging_state);
		if ((chg->wt_discharging_state & DISCHARGING_BY_DISABLE) == 0) {
			wtchg_set_main_chg_enable(chg, true);
			msleep(10);
		}
	} else if ((chg->wt_discharging_state & DISCHARGING_STATE_FULLCAPACITY) != 0 ||
			(chg->batt_full_capacity != 100 && chg->batt_capacity >= chg->batt_full_capacity)) {
		chg->wt_discharging_state |= DISCHARGING_STATE_FULLCAPACITY;
		chg->chg_ibatt = FCC_0_MA;
		chg->chg_type_input_curr = FCC_100_MA;
		wtchg_set_main_chg_enable(chg, false);
		wtchg_set_chg_input_current(chg);
		wtchg_set_chg_ibat_current(chg);
		dev_err(chg->dev, "%s, capacity:%d %d, stop charging\n", __func__, chg->batt_capacity, chg->batt_full_capacity);
	}

	return ret;
}
#endif

//+bug 769367, tankaikun@wt, add 20220902, fix charge status is full but capacity is lower than 100%
static void wtchg_battery_check_recharge_condition(struct wt_chg *chg)
{
	if (chg->batt_capacity_last > chg->batt_capacity_recharge)
		return;

	/* Report recharge to charger for SOC based resume of charging */
	if (chg->eoc_reported) {
		chg->eoc_reported = false;
		// recharge
		wtchg_set_main_chg_enable(chg, false);
		msleep(10);
		wtchg_set_main_chg_enable(chg, true);
		dev_err(chg->dev,"recharge batt eoc_reported = %d \n", chg->eoc_reported);
	}
}

static void wtchg_battery_check_eoc_condition(struct wt_chg *chg)
{
	if (chg->batt_capacity_last == 100 && chg->chg_status == POWER_SUPPLY_STATUS_FULL
			&& chg->eoc_reported == false) {
		dev_err(chg->dev,"Report EOC to charger\n");
		chg->eoc_reported = true;
	}

	if (chg->eoc_reported == true) {
		chg->batt_capacity = 100;
	}
}
//-bug 769367, tankaikun@wt, add 20220902, fix charge status is full but capacity is lower than 100%

//+bug 790579, tankaikun@wt, add 20220905, fix plug in charger slowly cause identify float type
#ifdef CONFIG_QGKI_BUILD

static void wtchg_float_charge_type_check(struct work_struct *work)
{
	int i;
	int float_recheck_count;
	struct wt_chg *chg = container_of(work,
							struct wt_chg, wt_float_recheck_work.work);

	//Wait for the usb service to load successfully
	if (wtchg_is_usb_ready(chg) != true) {
		float_recheck_count = SYS_BOOT_RECHECK_TIME_MAX;
		for (i = 0; i <= float_recheck_count; i++) {
			msleep(50);
			if ((!chg->usb_online) && (!chg->ac_online) && (!chg->wl_online)) {
				dev_err(chg->dev, "usb remove, skip wait for usb service to load\n");
				goto float_recheck_exit;
			}

			if (wtchg_is_usb_ready(chg) == true) {
				dev_err(chg->dev, "usb service load successfully\n");
				msleep(500);
				break;
			}
		}
	}

	if (chg->real_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		float_recheck_count = FLOAT_RECHECK_TIME_MAX;
	} else {
		float_recheck_count = USB_RECHECK_TIME_MAX;
	}

	//Wait for usb connect time out
	for (i = 0; i <= float_recheck_count; i++) {
		msleep(50);
		if ((!chg->usb_online) && (!chg->ac_online) && (!chg->wl_online)) {
			dev_err(chg->dev, "usb remove, skip usb_type recheck\n");
			goto float_recheck_exit;
		}

		if (wtchg_get_usb_connect(chg) == true) {
			dev_err(chg->dev, "usb connect, skip usb_type recheck\n");
			goto float_recheck_exit;
		}

/*+P231019-06775 dunweichao.wt 20231030,add for PD hub not recheck*/
		if(chg->real_type == POWER_SUPPLY_TYPE_USB_PD){
			dev_err(chg->dev, "PD:usb connect, skip usb_type recheck\n");
			goto float_recheck_exit;
		}
/*-P231019-06775 dunweichao.wt 20231030,add for PD hub not recheck*/


		dev_err(chg->dev, "wait for usb_connect:%d, count:%d\n", chg->usb_connect, i);
	}

	if (chg->rerun_adsp_count < RERUN_ADSP_COUNT_MAX) {
		dev_err(chg->dev, "wtchg_float_charge_type_check recheck !\n");
		chg->rerun_adsp_count++;
		if (chg->real_type == POWER_SUPPLY_TYPE_USB) {
			charger_enable_device_mode(false);
			msleep(200);
		}
		wtchg_set_main_chg_rerun_apsd(chg, 2);
	}

float_recheck_exit:
	return;
}
#endif
//-bug 790579, tankaikun@wt, add 20220905, fix plug in charger slowly cause identify float type

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
#ifdef CONFIG_QGKI_BUILD
static enum alarmtimer_restart wtchg_alarm_cb(struct alarm *alarm,
								ktime_t now)
{
	struct wt_chg *chg = container_of(alarm, struct wt_chg,
							wtchg_alarm);

	if(!wtchg_wake_active(&chg->wtchg_alarm_wake_source)){
		wtchg_stay_awake(&chg->wtchg_alarm_wake_source);
	}

	dev_err(chg->dev, "chg_main_alarm_cb wt chg alarm triggered %lld\n", ktime_to_ms(now));
	schedule_work(&chg->wtchg_alarm_work);

	return ALARMTIMER_NORESTART;
}
#endif /* CONFIG_QGKI_BUILD */

static void wtchg_alarm_work(struct work_struct *work)
{
	struct wt_chg *chg = container_of(work, struct wt_chg,
											wtchg_alarm_work);
	cancel_delayed_work_sync(&chg->wt_chg_work);
	schedule_delayed_work(&chg->wt_chg_work, 100);
}
//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

static void wt_chg_main(struct work_struct *work)
{
	struct wt_chg *chg = container_of(work,
	struct wt_chg, wt_chg_work.work);

	wtchg_set_main_feed_wdt(chg);
	wtchg_get_vbus_online(chg);
	wtchg_get_charge_bc12_type(chg);
	wtchg_get_usb_online(chg);
	wtchg_get_ac_online(chg);
	wtchg_get_usb_connect(chg);
#ifdef WT_WL_CHARGE
	if(0){
	wtchg_get_wl_online(chg);
	}
#endif
	wtchg_get_batt_status(chg);
	wtchg_get_batt_volt(chg);
	wtchg_get_batt_current(chg);
	wtchg_get_batt_temp(chg);
	wtchg_get_batt_health(chg);
	wtchg_get_vbus_voltage(chg);
	wtchg_get_batt_capacity(chg);
	//wtchg_kernel_power_off_check(chg);

#ifdef CONFIG_QGKI_BUILD
	pr_err("chg_type: %d, vbus_online: %d, vbus_volt: %d, chg_status: %d, batt_volt: %d, batt_current: %d, temp: %d, health: %d, batt_full_cnt: %d, max_cap:%d, capacity: %d batt_hv_disable:%d usb_connect:%d batt_capacity_level: %d, shutdown_check_ok: %d\n",
				chg->real_type, chg->vbus_online, chg->vbus_volt,
				chg->chg_status, chg->batt_volt, chg->chg_current,
				chg->batt_temp, chg->batt_health, chg->batt_full_check_counter,
				chg->batt_capacity_max, chg->batt_capacity, batt_hv_disable,
				chg->usb_connect, chg->batt_capacity_level, chg->shutdown_check_ok);
#else
	pr_err("chg_type: %d, vbus_online: %d, vbus_volt: %d, chg_status: %d, batt_volt: %d, batt_current: %d, temp: %d, health: %d, batt_full_cnt: %d, max_cap:%d, capacity: %d, batt_capacity_level: %d, shutdown_check_ok: %d\n",
				chg->real_type, chg->vbus_online, chg->vbus_volt,
				chg->chg_status, chg->batt_volt, chg->chg_current,
				chg->batt_temp, chg->batt_health, chg->batt_full_check_counter,
				chg->batt_capacity_max, chg->batt_capacity, chg->batt_capacity_level,
				chg->shutdown_check_ok);
#endif

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
//temp-add!!!!
//	wtchg_hv_disable(chg, batt_hv_disable);
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

//+P86801AA1,peiyuexiang.wt,modify,2023/06/19,add charger_mode
#ifdef CONFIG_QGKI_BUILD
	if (chg->vbus_online == 0) {
		chg->pre_vbus_online = chg->vbus_online;
		Himax_USB_detect_flag = 0;
		chipone_charger_mode_status = 0;
		usb_notifier_call_chain_for_tp(USB_PLUG_OUT,NULL);
		chg->chg_type_ibatt = FCC_0_MA; //unplug current set
		chg->chg_type_input_curr = FCC_100_MA; //unplug current set
//		pr_err("TP_LOG:%s:[tp charger]vbus_online: %d", __func__, chg->vbus_online);
//P231020-06549 gudi.wt,20231023,turn off hiz when plug out
		//wtchg_turn_off_hiz();
	} else if (chg->vbus_online == 1) {
		if (chg->pre_vbus_online == 0 ) {
			//wtchg_float_charge_type_check(chg);

		}
		chg->pre_vbus_online = chg->vbus_online;
		Himax_USB_detect_flag = 1;
		chipone_charger_mode_status = 1;
		usb_notifier_call_chain_for_tp(USB_PLUG_IN,NULL);
//		pr_err("TP_LOG:%s:[tp charger]vbus_online: %d", __func__, chg->vbus_online);
	}
#endif
//-P86801AA1,peiyuexiang.wt,modify,2023/06/19,add charger_mode
	if (chg->usb_online ||chg->ac_online || chg->wl_online) {
		wtchg_get_main_chg_temp(chg, &chg->main_chg_temp);
		wtchg_get_usb_port_temp(chg, &chg->usb_port_temp);
		wtchg_get_board_pcb_temp(chg, &chg->board_temp);
		wtchg_charge_strategy_machine(chg);
		wtchg_battery_maintain_charge_manage(chg);
		wtchg_battery_protect_charge_manage(chg);
#ifdef CONFIG_QGKI_BUILD
		wtchg_store_mode_manage(chg);
		wtchg_batt_full_capacity_manage(chg);
#endif
		wtchg_battery_check_recharge_condition(chg);
		wtchg_ato_charge_manage(chg);

/*+P86801AA2-1813 liwei19.wt 20231124,modify for usb not recheck when poweroff charging*/
#ifdef CONFIG_QGKI_BUILD
		if (chg->real_type == POWER_SUPPLY_TYPE_USB_FLOAT
				|| (chg->real_type == POWER_SUPPLY_TYPE_USB
				&& (get_boot_mode() == 0) && (!chg->usb_connect))) {
			schedule_delayed_work(&chg->wt_float_recheck_work, 0);
		}
#endif
/*-P86801AA2-1813 liwei19.wt 20231124,modify for usb not recheck when poweroff charging*/

		chg->chg_init_flag = false;
		if (chg->ato_soc_user_control)
			chg->interval_time = CHARGE_ONLINE_ATO_INTERVAL_MS;
		else
			chg->interval_time = CHARGE_ONLINE_INTERVAL_MS;

		if(get_boot_mode() || !((chg->real_type == POWER_SUPPLY_TYPE_USB) || (chg->real_type == POWER_SUPPLY_TYPE_USB_CDP))){
			if(!wtchg_wake_active(&chg->wtchg_off_charger_wake_source)){
				wtchg_stay_awake(&chg->wtchg_off_charger_wake_source);
			}
		}
	} else {
		wtchg_init_chg_parameter(chg);
		chg->interval_time = CHARGE_ONLINE_INTERVAL_MS;

		chg->usb_connect = false;
		chg->safety_timeout = false;
		chg->wt_discharging_state &= DISCHARGING_BY_HIZ;
		chg->rerun_adsp_count = 0;

		//+chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value
		chg->vbat_over_flag = 0;
		chg->vbat_over_count = 0;
		chg->vbat_update_time = 0;
		//-chk3593, liyiying.wt, 2022/7/21, add, judge the vbat if over the cv value

		//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
		chg->last_usb_thermal_status = -1;
		chg->last_board_thermal_status = -1;
		chg->last_chg_thermal_status = -1;
		//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

		//+bug792983, tankaikun@wt, add 20220827, Add recharge logic
		if (chg->vbus_volt < VBUS_ONLINE_MIN_VOLT)
			chg->eoc_reported = false;
		//-bug792983, tankaikun@wt, add 20220827, Add recharge logic

		//+bug825533, tankaikun@wt, add 20230216, add software control iterm
		chg->chg_iterm_cnt = 0;
		chg->chg_status_hyst_cnt = 0;
		chg->chg_status_target_fcc = FCC_3000_MA;
		chg->chg_state_step = CHRG_STATE_STOP;
		if (wtchg_wake_active(&chg->wtchg_iterm_wake_source)) {
			wtchg_relax(&chg->wtchg_iterm_wake_source);
		}
		//-bug825533, tankaikun@wt, add 20230216, add software control iterm
		if(get_boot_mode() || !((chg->real_type == POWER_SUPPLY_TYPE_USB) || (chg->real_type == POWER_SUPPLY_TYPE_USB_CDP))){
			if(wtchg_wake_active(&chg->wtchg_off_charger_wake_source)){
				wtchg_relax(&chg->wtchg_off_charger_wake_source);
			}
		}
	}

#ifdef CONFIG_QGKI_BUILD
	wtchg_battery_slate_mode_manage(chg);
	wtchg_check_charging_time(chg);
#endif
	//bug792882, tankaikun@wt, add 20220831, fix battery state update slowly
	wtchg_get_batt_status(chg);

#ifdef WT_WL_CHARGE
	pr_err("chg_temp: %d, usb_temp: %d, board_temp: %d, usb_online: %d, ac_online: %d, wl_online:%d, pd_active:%d, typec_orientation:%d discharging_state: %d\n",
			chg->main_chg_temp, chg->usb_port_temp, chg->board_temp,
			chg->usb_online, chg->ac_online, chg->wl_online, chg->pd_active,
			chg->typec_cc_orientation, chg->wt_discharging_state);
	pr_err("pd_cur_max=%d pd_max_vol=%d pd_min_vol=%d pd_allow_vol=%d \n",
				chg->pd_cur_max, chg->pd_max_vol, chg->pd_min_vol, chg->pd_allow_vol);
#else
	pr_err("chg_temp: %d, usb_temp: %d, board_temp: %d, usb_online: %d, ac_online: %d, pd_active:%d, typec_orientation:%d discharging_state: %d\n",
			chg->main_chg_temp, chg->usb_port_temp, chg->board_temp,
			chg->usb_online, chg->ac_online, chg->pd_active,
			chg->typec_cc_orientation, chg->wt_discharging_state);
	pr_err("pd_cur_max=%d pd_max_vol=%d pd_min_vol=%d pd_allow_vol=%d \n",
				chg->pd_cur_max, chg->pd_max_vol, chg->pd_min_vol, chg->pd_allow_vol);
#endif

	/* Update userspace */
	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);

	if (chg->usb_psy)
		power_supply_changed(chg->usb_psy);

	if (chg->ac_psy)
		power_supply_changed(chg->ac_psy);

	if (chg->otg_psy)
		power_supply_changed(chg->otg_psy);

	//if(chg->wl_psy)
	//	power_supply_changed(chg->wl_psy);

	schedule_delayed_work(&chg->wt_chg_work, msecs_to_jiffies(chg->interval_time));

	if (wtchg_wake_active(&chg->wtchg_alarm_wake_source)) {
		wtchg_relax(&chg->wtchg_alarm_wake_source);
	}
}

static int wtchg_batt_parse_dt(struct wt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	int ret = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node,
				"qcom,fv-max-uv", &chg->batt_cv_max);
	if (ret < 0)
		chg->batt_cv_max = 4450000;
	else
		pr_err("fv-max-uv: %d\n",chg->batt_cv_max);

	ret = of_property_read_u32(node,
				"qcom,fcc-max-ua", &chg->batt_fcc_max);
	if (ret < 0)
		chg->batt_fcc_max = 3000000;
	else
		pr_err("fcc-max-ua: %d\n",chg->batt_fcc_max);

	ret = of_property_read_u32(node,
				"qcom,batt_iterm", &chg->batt_iterm);
	if (ret < 0)
		chg->batt_iterm = 240000;
	else
		pr_err("batt_iterm: %d\n",chg->batt_iterm);

	//+bug792983, tankaikun@wt, add 20220831, fix device recharge but no charging icon
	ret = of_property_read_u32(node,
				"qcom,auto-recharge-soc", &chg->batt_capacity_recharge);
	if (ret < 0)
		chg->batt_capacity_recharge = 98;
	else
		pr_err("recharge_capacity: %d\n",chg->batt_capacity_recharge);
	//-bug792983, tankaikun@wt, add 20220831, fix device recharge but no charging icon

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	ret = of_property_read_u32(node,
				"qcom,auto-recharge-offset-uv", &chg->batt_recharge_offset_vol);
	if (ret < 0)
		chg->batt_recharge_offset_vol = 150000; // default max_fv-150mv
	else
		pr_err("recharge_offset_voltage: %d\n",chg->batt_recharge_offset_vol);
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm

	//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
	chg->disable_usb_port_therm = of_property_read_bool(node, "qcom,disable-usb-port-thermal");
	if (chg->disable_usb_port_therm < 0) {
		chg->disable_usb_port_therm = 1;
	} else {
		pr_err("disable usb port therm: %d\n", chg->disable_usb_port_therm);
	}
	chg->disable_main_chg_therm = of_property_read_bool(node, "qcom,disable-main-chg-thermal");
	if (chg->disable_main_chg_therm < 0) {
		chg->disable_main_chg_therm = 1;
	} else {
		pr_err("disable main chg therm: %d\n", chg->disable_main_chg_therm);
	}
	chg->disable_board_pcb_therm = of_property_read_bool(node, "qcom,disable-board-pcb-thermal");
	if (chg->disable_board_pcb_therm < 0) {
		chg->disable_board_pcb_therm = 1;
	} else {
		pr_err("disable board therm: %d\n", chg->disable_board_pcb_therm);
	}
#ifdef DISABLE_BOARD_TEMP_CONTROL
	chg->disable_board_pcb_therm = 1;
#endif /*DISABLE_BOARD_TEMP_CONTROL*/

#ifdef DISABLE_CHG_TEMP_CONTROL
	chg->disable_main_chg_therm = 1;
#endif /*DISABLE_CHG_TEMP_CONTROL*/

#ifdef DISABLE_USB_PORT_TEMP_CONTROL
	chg->disable_usb_port_therm = 1;
#endif /*DISABLE_USB_PORT_TEMP_CONTROL*/

#ifdef CONFIG_WT_DISABLE_TEMP_CONTROL
	chg->disable_board_pcb_therm = 1;
	chg->disable_main_chg_therm = 1;
	chg->disable_usb_port_therm = 1;
#endif
	//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	ret = get_iio_channel(chg, "chg_therm", &chg->main_chg_therm);
	if (ret < 0) {
		pr_err("get chg_therm fail: %d\n", ret);
		//return ret;
	}

	ret = get_iio_channel(chg, "usb_port_therm", &chg->usb_port_therm);
	if (ret < 0) {
		pr_err("get usb_port_therm fail: %d\n", ret);
		//return ret;
	}

	ret = get_iio_channel(chg, "quiet_therm", &chg->board_pcb_therm);
	if (ret < 0) {
		pr_err("get board_pcb_therm fail: %d\n", ret);
		//return ret;
	}

	return 0;
};

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
static int wtchg_hv_disable(struct wt_chg *chg, int batt_hv_disable)
{
	int chg_type, ret;
	int vbus_volt_before, vbus_volt_after;
	struct power_supply *psy = power_supply_get_by_name("cclogic");
	union power_supply_propval val = {.intval = 0};

	chg_type = wtchg_get_charge_bc12_type(chg);
	vbus_volt_before = wtchg_get_vbus_voltage(chg);

	if (!psy) {
		pr_err("%s: power supply get fail --\n", __func__);
		return -ENODEV;
	}

	if (batt_hv_disable == 1 && vbus_volt_before >= 7000) {
		switch (chg_type) {
			case POWER_SUPPLY_TYPE_USB_HVDCP:
			case POWER_SUPPLY_TYPE_USB_HVDCP_3:
				ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_SET_DP_DM, QTI_POWER_SUPPLY_DP_DM_FORCE_5V);
				if (ret < 0) {
					dev_err(chg->dev, "%s: fail: %d --\n", __func__, ret);
				}
				vbus_volt_after = wtchg_get_vbus_voltage(chg);
				dev_err(chg->dev, "%s: downdowndown: batt_hv_disable is %d, chg_type is %d, vbus_volt_before is %d, vbus_volt_after is %d --\n",
						__func__, batt_hv_disable, chg_type, vbus_volt_before, vbus_volt_after);
				break;
			case POWER_SUPPLY_TYPE_USB_AFC:
				ret = wtchg_write_iio_prop(chg, AFC, AFC_SET_VOL, 5000);
				if (ret < 0) {
					dev_err(chg->dev, "%s: AFC set 5V fail: %d --\n", __func__, ret);
				}
				vbus_volt_after = wtchg_get_vbus_voltage(chg);
				dev_err(chg->dev, "%s: down: batt_hv_disable is %d, chg_type is %d, vbus_volt_before is %d, vbus_volt_after is %d --\n",
						__func__, batt_hv_disable, chg_type, vbus_volt_before, vbus_volt_after);
				break;
			case POWER_SUPPLY_TYPE_USB_PD:
				val.intval = 5000;
				ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
				if (ret < 0)
					pr_notice("%s: down: power supply set online fail(%d) --\n",  __func__, ret);
				vbus_volt_after = wtchg_get_vbus_voltage(chg);
				dev_err(chg->dev, "%s: down: batt_hv_disable is %d, chg_type is %d, vbus_volt_before is %d, vbus_volt_after is %d --\n",
						__func__, batt_hv_disable, chg_type, vbus_volt_before, vbus_volt_after);
				break;
			default:
				break;
		}

	}
	else if (batt_hv_disable == 0 && vbus_volt_before < 7000){
		switch (chg_type) {
			case POWER_SUPPLY_TYPE_USB_HVDCP:
			case POWER_SUPPLY_TYPE_USB_HVDCP_3:
				ret = wtchg_write_iio_prop(chg, MAIN_CHG, MAIN_CHARGE_SET_DP_DM, QTI_POWER_SUPPLY_DP_DM_FORCE_9V);
				if (ret < 0) {
					dev_err(chg->dev, "%s: fail: %d\n --", __func__, ret);
				}
				vbus_volt_after = wtchg_get_vbus_voltage(chg);
				dev_err(chg->dev, "%s: up: batt_hv_disable is %d, chg_type is %d, vbus_volt_before is %d, vbus_volt_after is %d --\n",
						__func__, batt_hv_disable, chg_type, vbus_volt_before, vbus_volt_after);
				break;
			case POWER_SUPPLY_TYPE_USB_AFC:
				ret = wtchg_write_iio_prop(chg, AFC, AFC_SET_VOL, 9000);
				if (ret < 0) {
					dev_err(chg->dev, "%s: AFC set 9V fail: %d --\n", __func__, ret);
				}
				vbus_volt_after = wtchg_get_vbus_voltage(chg);
				dev_err(chg->dev, "%s: up: batt_hv_disable is %d, chg_type is %d, vbus_volt_before is %d, vbus_volt_after is %d --\n",
						__func__, batt_hv_disable, chg_type, vbus_volt_before, vbus_volt_after);
				break;
			case POWER_SUPPLY_TYPE_USB_PD:
				val.intval = 9000;
				ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
				if (ret < 0)
					pr_notice("%s: up: power supply set online fail(%d) --\n",  __func__, ret);
				break;
			default:
				break;
		}
	}

	return 0;
}

#define SEC_BAT_CURRENT_EVENT_NONE   0x00000
#define SEC_BAT_CURRENT_EVENT_AFC   0x00001  // fast charging
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE  0x00002
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING  0x00010
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING 0x00020
#define SEC_BAT_CURRENT_EVENT_USB_100MA   0x00040
#define SEC_BAT_CURRENT_EVENT_SLATE   0x00800
#define SEC_BAT_CURRENT_EVENT_HV_DISABLE	0x010000
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

static enum power_supply_property wtchg_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	//PLM230714-05698,23/07/25,gudi.wt,no iocn in power off charging
#ifdef CONFIG_QGKI_BUILD
	POWER_SUPPLY_PROP_ONLINE,
#endif //CONFIG_QGKI_BUILD
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	POWER_SUPPLY_PROP_NEW_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	POWER_SUPPLY_PROP_BATT_CURRENT_EVENT,
	POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW,
	POWER_SUPPLY_PROP_BATT_FULL_CAPACITY,
	POWER_SUPPLY_PROP_BATT_MISC_EVENT,
	POWER_SUPPLY_PROP_HV_DISABLE,
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
};

static int wtchg_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chg->chg_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chg->batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = true;
		break;
	//PLM230714-05698,23/07/25,gudi.wt,no iocn in power off charging
#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_ONLINE:
		if (chg->real_type == POWER_SUPPLY_TYPE_UNKNOWN)
			val->intval = 1;
		else if (chg->real_type == POWER_SUPPLY_TYPE_USB
			|| chg->real_type == POWER_SUPPLY_TYPE_USB_CDP)
			val->intval = 4;
		else if (chg->real_type == POWER_SUPPLY_TYPE_USB_FLOAT)
			val->intval = 0;
		else
			val->intval = 3;
		break;
#endif //CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = 3000*1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chg->batt_capacity;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = wtchg_get_batt_capacity_level(chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chg->batt_volt;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = wtchg_get_batt_volt_max(chg);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW:
#endif
		wtchg_get_batt_current(chg);
		val->intval = chg->chg_current * 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = wtchg_get_batt_current_max(chg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = wtchg_get_batt_iterm_max(chg);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chg->batt_temp;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = wtchg_get_batt_id_info(chg);
		break;
	//+bug 798451, liyiying.wt, add, 2022/8/30, VtsHalHealthV2_0TargetTest--PerInstance/HealthHidlTest#getHealthInfo/0_default fail
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = chg->batt_capacity * 5000 * 10;
		break;
	//-bug 798451, liyiying.wt, add, 2022/8/30, VtsHalHealthV2_0TargetTest--PerInstance/HealthHidlTest#getHealthInfo/0_default fail
	//+bug 804857, taohuayi.wt, add, 2022/9/22, CtsStatsdAtomHostTestCases--android.cts.statsdatom.statsd.HostAtomTests#testFullBatteryCapacity
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 5000*1000;
		break;
	//-bug 804857, taohuayi.wt, add, 2022/9/22, CtsStatsdAtomHostTestCases--android.cts.statsdatom.statsd.HostAtomTests#testFullBatteryCapacity
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_NEW_CHARGE_TYPE:
		val->strval = (chg->main_chg_input_curr > 1000) ? "Fast" : "Slow";
		break;
	case POWER_SUPPLY_PROP_HV_CHARGER_STATUS:
		if (batt_hv_disable)
			val->intval = 0;
		else if (chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP ||
			chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3 ||
			chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5 ||
			chg->real_type == POWER_SUPPLY_TYPE_USB_AFC ||
			chg->real_type == POWER_SUPPLY_TYPE_USB_PE ||
			chg->real_type == POWER_SUPPLY_TYPE_USB_PD)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		val->intval = chg->batt_slate_mode;
		break;
	case POWER_SUPPLY_PROP_BATT_CURRENT_EVENT:
		val->intval = SEC_BAT_CURRENT_EVENT_NONE;
		wtchg_get_vbus_online(chg);
		if (chg->vbus_online) {
			if (chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP ||
				chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3 ||
				chg->real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3P5 ||
				chg->real_type == POWER_SUPPLY_TYPE_USB_AFC ||
				chg->real_type == POWER_SUPPLY_TYPE_USB_PE ||
				chg->real_type == POWER_SUPPLY_TYPE_USB_PD)
				val->intval |= SEC_BAT_CURRENT_EVENT_AFC;
			if (chg->chg_status == POWER_SUPPLY_STATUS_DISCHARGING)
				val->intval |= SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE;
			if (chg->batt_temp < 100)
				val->intval |= SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING;
			if (chg->batt_temp > 450)
				val->intval |= SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING;
			if (chg->main_chg_input_curr == 100)
				val->intval |= SEC_BAT_CURRENT_EVENT_USB_100MA;
			if (chg->batt_slate_mode)
				val->intval |= SEC_BAT_CURRENT_EVENT_SLATE;
			if (batt_hv_disable)
				val->intval |= SEC_BAT_CURRENT_EVENT_HV_DISABLE;
		}
		break;
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		val->intval = chg->batt_full_capacity;
		break;
	case POWER_SUPPLY_PROP_BATT_MISC_EVENT:
		val->intval = 0;
		if (chg->real_type == POWER_SUPPLY_TYPE_USB_FLOAT)
			val->intval |= 4;
		if ((chg->wt_discharging_state & DISCHARGING_STATE_FULLCAPACITY) != 0)
			val->intval |= 0x01000000;
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		val->intval = batt_hv_disable;
		break;
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		wtchg_get_batt_time(chg);
		val->intval = chg->batt_time;
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}

	if (ret < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}

	return 0;
}

static int wtchg_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int ret = 0;
	struct wt_chg *chg = power_supply_get_drvdata(psy);
#ifdef CONFIG_QGKI_BUILD
	int pval = val->intval;
#endif

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = wtchg_batt_set_system_temp_level(chg, val);
		break;
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		chg->batt_slate_mode = val->intval;
		wtchg_set_charge_work(chg);
		break;
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		chg->batt_full_capacity = val->intval;
		wtchg_set_charge_work(chg);
		break;
	case POWER_SUPPLY_PROP_HV_DISABLE:
		if (pval == 1)
			batt_hv_disable = true;
		else if (pval == 0)
			batt_hv_disable = false;
		pr_err("%s HV wired charging mode is %d, batt_hv_disable is %d --\n", __func__, pval, batt_hv_disable);
		/*
		This part should be designed to invoke voltage regulation functions-liyiying
		//charger_manager_enable_high_voltage_charging(gm.pbat_consumer, !batt_hv_disable);
		*/
		wtchg_hv_disable(chg, batt_hv_disable);
		break;
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int wtchg_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
	case POWER_SUPPLY_PROP_HV_DISABLE:
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = wtchg_batt_props,
	.num_properties = ARRAY_SIZE(wtchg_batt_props),
	.get_property = wtchg_batt_get_prop,
	.set_property = wtchg_batt_set_prop,
	.property_is_writeable = wtchg_batt_prop_is_writeable,
};
#ifdef CONFIG_QGKI_BUILD
static ssize_t show_store_mode(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);

	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->store_mode);
}

static ssize_t store_store_mode(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int val;

	if (kstrtoint(buf, 10, &val) == 0){
		chg->store_mode = val;
		wtchg_set_charge_work(chg);
	}else if (sscanf(buf, "%10d\n", &val) == 1){
		chg->store_mode = val;
		wtchg_set_charge_work(chg);
	}

	return size;
}

static DEVICE_ATTR(store_mode, 0664, show_store_mode,store_store_mode);

//+P86801AA1-13521 gudi.wt,20231009, add Battery EU ECO design
static ssize_t show_batt_temp(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);

	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->batt_temp);
}

static ssize_t store_batt_temp(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int val;

	if (kstrtoint(buf, 10, &val) == 0){
		chg->batt_temp = val;
		wtchg_set_charge_work(chg);
	}else if (sscanf(buf, "%10d\n", &val) == 1){
		chg->batt_temp = val;
		wtchg_set_charge_work(chg);
	}

	return size;
}

static DEVICE_ATTR(batt_temp, 0664, show_batt_temp,store_batt_temp);


static ssize_t show_battery_cycle(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);

	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->batt_cycle);
}

static ssize_t store_battery_cycle(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int val;

	if (kstrtoint(buf, 10, &val) == 0){
		chg->batt_cycle = val;
		wtchg_set_charge_work(chg);
	}else if (sscanf(buf, "%10d\n", &val) == 1){
		chg->batt_cycle = val;
		wtchg_set_charge_work(chg);
	}

	return size;
}

static DEVICE_ATTR(battery_cycle, 0664, show_battery_cycle,store_battery_cycle);

char str_batt_type[64] = {0};
static ssize_t show_batt_type(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", str_batt_type);
}
static ssize_t store_batt_type(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int i = 0;
	printk("[%s]\n", __func__);
	if (buf != NULL && size != 0) {
		pr_err("[%s] buf is %s\n", __func__, buf);
		memset(str_batt_type, 0, 64);
		for (i = 0; i < size; ++i) {
			str_batt_type[i] = buf[i];
		}
		str_batt_type[i+1] = '\0';
		pr_err("str_batt_type:%s\n", str_batt_type);
	}

	return size;
}
static DEVICE_ATTR(batt_type, 0664, show_batt_type, store_batt_type);
//-P86801AA1-13521 gudi.wt,20231009, add Battery EU ECO design

#ifdef CONFIG_AMERICA_VERSION
static ssize_t show_charge_type(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct wt_chg *chg = power_supply_get_drvdata(psy);

	return sprintf(buf, "%s\n", (chg->main_chg_input_curr > 1000) ? "Fast" : "Trickle");
}

static DEVICE_ATTR(charge_type, 0664, show_charge_type, NULL);
#endif
#endif
static int wtchg_init_batt_psy(struct wt_chg *chg)
{
	struct power_supply_config batt_cfg = {};
	int ret = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return ret;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}
#ifdef CONFIG_QGKI_BUILD
	ret = device_create_file(&(chg->batt_psy->dev), &dev_attr_store_mode);
//+P86801AA1-13521 gudi.wt,20231009, add Battery EU ECO design
	ret = device_create_file(&(chg->batt_psy->dev), &dev_attr_batt_temp);
	ret = device_create_file(&(chg->batt_psy->dev), &dev_attr_battery_cycle);
	ret = device_create_file(&(chg->batt_psy->dev), &dev_attr_batt_type);
//-P86801AA1-13521 gudi.wt,20231009, add Battery EU ECO design
#ifdef CONFIG_AMERICA_VERSION
	ret = device_create_file(&(chg->batt_psy->dev), &dev_attr_charge_type);
#endif
#endif
	return ret;
}

/************************
 * AC PSY REGISTRATION *
 ************************/
static enum power_supply_property wtchg_ac_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static int wtchg_ac_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wtchg_get_ac_online(chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = 0;
		break;
	default:
		pr_err("get prop %d is not supported in ac\n", psp);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}

	return 0;
}

static int wtchg_ac_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	default:
		pr_err("Set prop %d is not supported in usb psy\n",
				psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wt_ac_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc ac_psy_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = wtchg_ac_props,
	.num_properties = ARRAY_SIZE(wtchg_ac_props),
	.get_property = wtchg_ac_get_prop,
	.set_property = wtchg_ac_set_prop,
	.property_is_writeable = wt_ac_prop_is_writeable,
};

static int wtchg_init_ac_psy(struct wt_chg *chg)
{
	struct power_supply_config ac_cfg = {};

	ac_cfg.drv_data = chg;
	ac_cfg.of_node = chg->dev->of_node;
	chg->ac_psy = devm_power_supply_register(chg->dev,
						  &ac_psy_desc,
						  &ac_cfg);
	if (IS_ERR(chg->ac_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->ac_psy);
	}

	return 0;
}


/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property wtchg_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_POWER_NOW,
};

static int wtchg_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		wtchg_get_vbus_online(chg);
		val->intval = wtchg_get_usb_online(chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = wtchg_get_usb_max_voltage(chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = wtchg_get_usb_max_voltage(chg);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chg->vbus_volt*1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = wtchg_get_usb_max_current(chg);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		val->intval = 0;
		break;
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}

	return 0;
}

static int wtchg_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	//struct wt_chg *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	default:
		pr_err("Set prop %d is not supported in usb psy\n",
				psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wtchg_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = wtchg_usb_props,
	.num_properties = ARRAY_SIZE(wtchg_usb_props),
	.get_property = wtchg_usb_get_prop,
	.set_property = wtchg_usb_set_prop,
	.property_is_writeable = wtchg_usb_prop_is_writeable,
};

static int wtchg_init_usb_psy(struct wt_chg *chg)
{
	struct power_supply_config usb_cfg = {};

	usb_cfg.drv_data = chg;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

/************************
 * OTG PSY REGISTRATION *
 ************************/
static enum power_supply_property wtchg_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};

static int wtchg_otg_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chg->otg_enable;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = chg->otg_enable;
		break;
	default:
		pr_err("get prop %d is not supported in otg\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wtchg_otg_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		wtchg_set_main_chg_otg(chg, val->intval);
		pr_info("%s: OTG %s %d\n", __func__, val->intval > 0 ? "ON" : "OFF", chg->otg_enable);
		break;
	default:
		pr_err("Set prop %d is not supported in otg psy\n",
				psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wtchg_otg_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc otg_psy_desc = {
	.name = "otg",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = wtchg_otg_props,
	.num_properties = ARRAY_SIZE(wtchg_otg_props),
	.get_property = wtchg_otg_get_prop,
	.set_property = wtchg_otg_set_prop,
	.property_is_writeable = wtchg_otg_prop_is_writeable,
};

static int wtchg_init_otg_psy(struct wt_chg *chg)
{
	struct power_supply_config otg_cfg = {};

	otg_cfg.drv_data = chg;
	otg_cfg.of_node = chg->dev->of_node;
	chg->otg_psy = devm_power_supply_register(chg->dev,
						  &otg_psy_desc,
						  &otg_cfg);
	if (IS_ERR(chg->otg_psy)) {
		pr_err("Couldn't register otg power supply\n");
		return PTR_ERR(chg->otg_psy);
	}

	return 0;
}

//+ bug 761884, tankaikun@wt, add 20220720, charger bring up
/************************
 * WL PSY REGISTRATION *
 ************************/
static enum power_supply_property wtchg_wl_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int wtchg_wl_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = wtchg_get_wl_present(chg);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = wtchg_get_wl_present(chg);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_WIRELESS;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chg->vbus_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chg->chg_current * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		break;
	default:
		pr_err("get prop %d is not supported in wireless\n", psp);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(chg->dev, "Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}

	return 0;
}

static int wtchg_wl_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct wt_chg *chg = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		dev_dbg(chg->dev,
				"set input current: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		dev_dbg(chg->dev,
				"set term current: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		dev_dbg(chg->dev,
				"set charge voltage: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		dev_dbg(chg->dev,
				"set charge current: %d\n", val->intval);
		break;
	default:
		dev_err(chg->dev, "set prop: %d is not supported\n", psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wtchg_wl_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc wtchg_wl_psy_desc = {
	.name = "wireless_chg",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = wtchg_wl_props,
	.num_properties = ARRAY_SIZE(wtchg_wl_props),
	.get_property = wtchg_wl_get_prop,
	.set_property = wtchg_wl_set_prop,
	.property_is_writeable = wtchg_wl_prop_is_writeable,
};

#ifdef WT_WL_CHARGE
static int wtchg_init_wl_psy(struct wt_chg *chg)
{
	struct power_supply_config wl_cfg = {};

	wl_cfg.drv_data = chg;
	wl_cfg.of_node = chg->dev->of_node;
	chg->wl_psy = devm_power_supply_register(chg->dev,
						  &wtchg_wl_psy_desc,
						  &wl_cfg);
	if (IS_ERR(chg->wl_psy)) {
		pr_err("Couldn't register wl chg power supply\n");
		return PTR_ERR(chg->wl_psy);
	}
	return 0;
}
#endif

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

#ifdef WT_WL_CHARGE
static const char *get_wireless_type_name(u32 usb_type)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(power_supply_wl_type_text); i++) {
		if (i == usb_type)
			return power_supply_wl_type_text[i];
	}

	return "Unknown";
}
#endif

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(c, struct wt_chg,
						battery_class);
	int val;

	val = wtchg_get_usb_real_type(chg);
	val -= 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(usb_real_type);

#ifdef WT_WL_CHARGE
static ssize_t wireless_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(c, struct wt_chg,
						battery_class);
	int val;

  chg->real_type = wtchg_get_wl_type(chg);
  val = chg->real_type;

  return scnprintf(buf, PAGE_SIZE, "%s\n", get_wireless_type_name(val));
}

static CLASS_ATTR_RO(wireless_real_type);
#endif

static ssize_t vbus_voltage_show(struct class *c,
				  struct class_attribute *attr, char *buf)
{
  struct wt_chg *chg = container_of(c, struct wt_chg,
					  battery_class);
  int val;

  val = wtchg_get_vbus_voltage(chg);

  return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbus_voltage);

static ssize_t battery_name_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	// todo:get batt id by iio and show the right battery name
	return scnprintf(buf, PAGE_SIZE, "S88501_SWD_4V45_5000mAh\n");
}
static CLASS_ATTR_RO(battery_name);

static ssize_t typec_cc_orientation_show(struct class *c,
			  struct class_attribute *attr, char *buf)
{

  struct wt_chg *chg = container_of(c, struct wt_chg,
					  battery_class);

  return scnprintf(buf, PAGE_SIZE, "%d\n",chg->typec_cc_orientation);

}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t stopcharging_test_show(struct class *c,
			  struct class_attribute *attr, char *buf)
{
  struct wt_chg *chg = container_of(c, struct wt_chg,
					  battery_class);

  chg->start_charging_test = false;
  wtchg_set_main_chg_enable(chg, chg->start_charging_test);
  wtchg_set_main_input_suspend(chg, !chg->start_charging_test);
  dev_err(chg->dev, "set: stopcharging_test = %d\n", chg->start_charging_test);

  power_supply_changed(chg->usb_psy);
  return scnprintf(buf, PAGE_SIZE, "%d\n", chg->start_charging_test);
}
static CLASS_ATTR_RO(stopcharging_test);

static ssize_t startcharging_test_show(struct class *c,
			  struct class_attribute *attr, char *buf)
{

  struct wt_chg *chg = container_of(c, struct wt_chg,
					  battery_class);

  chg->start_charging_test = true;
  wtchg_set_main_chg_enable(chg, chg->start_charging_test);
  wtchg_set_main_input_suspend(chg, !chg->start_charging_test);
  dev_err(chg->dev, "set: startcharging_test = %d\n", chg->start_charging_test);

  power_supply_changed(chg->usb_psy);
  return scnprintf(buf, PAGE_SIZE, "%d\n", chg->start_charging_test);
}
static CLASS_ATTR_RO(startcharging_test);

static struct attribute *battery_class_attrs[] = {
  &class_attr_usb_real_type.attr,
#ifdef WT_WL_CHARGE
  &class_attr_wireless_real_type.attr,
#endif
  &class_attr_vbus_voltage.attr,
  &class_attr_battery_name.attr,
  &class_attr_typec_cc_orientation.attr,
  &class_attr_stopcharging_test.attr,
  &class_attr_startcharging_test.attr,
  NULL,
};
ATTRIBUTE_GROUPS(battery_class);

#define BATT_TEMP_LIMIT_H	700
#define BATT_TEMP_LIMIT_L	(-200)
static ssize_t show_batt_temp_test(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "real: batt_debug_temp = %d\n", chg->batt_debug_temp);
}

static ssize_t store_batt_temp_test(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int temp = 250;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &temp) == 0) {
		if ((temp >= BATT_TEMP_LIMIT_L) && (temp <= BATT_TEMP_LIMIT_H)) {
			chg->batt_temp_debug_flag = true;
			chg->batt_debug_temp = temp;
		}
	}

	return size;
}
static DEVICE_ATTR(batt_temp_test, 0664, show_batt_temp_test, store_batt_temp_test);

#define BOARD_TEMP_LIMIT_H	700
#define BOARD_TEMP_LIMIT_L	(-200)
static ssize_t show_board_temp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "board_debug_temp = %d\n", chg->board_debug_temp);
}

static ssize_t store_board_temp_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int temp = 250;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &temp) == 0) {
		if ((temp >= BOARD_TEMP_LIMIT_L) && (temp <= BOARD_TEMP_LIMIT_H)) {
			chg->board_temp_debug_flag = true;
			chg->board_debug_temp = temp;
		}
	}

	return size;
}
static DEVICE_ATTR(board_temp_test, 0664, show_board_temp_test, store_board_temp_test);

#define MAIN_CHG_TEMP_LIMIT_H	700
#define MAIN_CHG_TEMP_LIMIT_L	(-200)
static ssize_t show_main_chg_temp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "main_chg_temp = %d\n", chg->main_chg_temp);
}
static ssize_t store_main_chg_temp_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int temp;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &temp) == 0) {
		if ((temp >= MAIN_CHG_TEMP_LIMIT_L) && (temp <= MAIN_CHG_TEMP_LIMIT_H)) {
			chg->main_chg_temp_debug_flag = true;
			chg->main_chg_temp = temp;
		}
	}

	return size;
}
static DEVICE_ATTR(main_chg_temp_test, 0664, show_main_chg_temp_test, store_main_chg_temp_test);

#define USB_TEMP_LIMIT_H	700
#define USB_TEMP_LIMIT_L	(-200)
static ssize_t show_usb_temp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "usb_debug_temp = %d\n", chg->usb_debug_temp);
}

static ssize_t store_usb_temp_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int temp;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &temp) == 0) {
		if ((temp >= USB_TEMP_LIMIT_L) && (temp <= USB_TEMP_LIMIT_H)) {
			chg->usb_temp_debug_flag = true;
			chg->usb_debug_temp = temp;
		}
	}

	return size;
}
static DEVICE_ATTR(usb_temp_test, 0664, show_usb_temp_test, store_usb_temp_test);

static ssize_t show_battery_maintain(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "batt_maintain_mode = %d\n", chg->batt_maintain_mode);
}

static ssize_t store_battery_maintain(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int maintain;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &maintain) == 0) {
		if (maintain == 0) {
			chg->batt_maintain_mode = false;
		} else if  (maintain == 1) {
			chg->batt_maintain_mode = true;
		}
		wtchg_set_charge_work(chg);
	}

	return size;
}
static DEVICE_ATTR(battery_maintain, 0664, show_battery_maintain, store_battery_maintain);

static ssize_t show_battery_protected(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "batt_protected_mode = %d\n", chg->batt_protected_mode);
}

static ssize_t store_battery_protected(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int protected;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &protected) == 0) {
		if (protected == 0) {
			chg->batt_protected_mode = false;
		} else if  (protected == 1) {
			chg->batt_protected_mode = true;
		}
		wtchg_set_charge_work(chg);
	}

	return size;
}
static DEVICE_ATTR(battery_protected, 0664, show_battery_protected, store_battery_protected);

static ssize_t show_batt_cycle(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "batt_cycle = %d\n", chg->batt_cycle);
}

static ssize_t store_batt_cycle(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int cycle;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &cycle) == 0) {
		chg->batt_cycle = cycle;
	}
	wtchg_set_batt_cycle(chg);
	wtchg_get_batt_cycle(chg);

	return size;
}
static DEVICE_ATTR(batt_cycle, 0664, show_batt_cycle, store_batt_cycle);

static ssize_t show_fake_soc(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "show_fake_soc = %d\n", chg->batt_capacity_fake);
}

static ssize_t store_fake_soc(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int soc;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &soc) == 0) {
		chg->batt_capacity_fake = soc;
	}

	return size;
}
static DEVICE_ATTR(fake_soc, 0664, show_fake_soc, store_fake_soc);

static ssize_t show_ato_soc_user_control(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	return sprintf(buf, "%d\n", chg->ato_soc_user_control);
}

static ssize_t store_ato_soc_user_control(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int user_control;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &user_control) == 0) {
		if(chg->ato_soc_user_control == user_control)
			return size;

		chg->ato_soc_user_control = !!user_control;

		if (chg->ato_soc_user_control) {
			if(!wtchg_wake_active(&chg->wtchg_ato_soc_wake_source)){
				wtchg_stay_awake(&chg->wtchg_ato_soc_wake_source);
			}
			cancel_delayed_work_sync(&chg->ato_soc_user_control_work);
			schedule_delayed_work(&chg->ato_soc_user_control_work, msecs_to_jiffies(60000)); // 1mins
		} else {
			cancel_delayed_work_sync(&chg->ato_soc_user_control_work);
			if (wtchg_wake_active(&chg->wtchg_ato_soc_wake_source)) {
				wtchg_relax(&chg->wtchg_ato_soc_wake_source);
			}
		}

		wtchg_set_charge_work(chg);
	}

	return size;
}
static DEVICE_ATTR(ato_soc_user_control, 0664, show_ato_soc_user_control, store_ato_soc_user_control);

static ssize_t show_set_ship_mode(struct device *dev,
											struct device_attribute *attr, char *buf)
{
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	//P86801,gudi.wt,20230624,modify return func
	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->ship_mode);
	//return sprintf(buf, "%d\n", chg->ato_soc_user_control);
}

static ssize_t store_set_ship_mode(struct device *dev,
											struct device_attribute *attr,
											const char *buf, size_t size)
{
	int ship_mode;
	struct wt_chg *chg = container_of(dev, struct wt_chg, batt_device);

	if (kstrtoint(buf, 10, &ship_mode) == 0) {
		chg->ship_mode = !!ship_mode;
		//wtchg_set_slave_chg_ship_mode(chg);
		//wtchg_set_main_chg_ship_mode(chg);
	}

	return size;
}
static DEVICE_ATTR(set_ship_mode, 0664, show_set_ship_mode, store_set_ship_mode);

static struct attribute *battery_attributes[] = {
	&dev_attr_batt_temp_test.attr,
	&dev_attr_board_temp_test.attr,
	&dev_attr_main_chg_temp_test.attr,
	&dev_attr_usb_temp_test.attr,
	&dev_attr_battery_maintain.attr,
	&dev_attr_battery_protected.attr,
	&dev_attr_batt_cycle.attr,
	&dev_attr_fake_soc.attr,
	&dev_attr_ato_soc_user_control.attr,
	&dev_attr_set_ship_mode.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
  .attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
  &battery_attr_group,
  NULL,
};

static int wt_init_dev_class(struct wt_chg *chg)
{
  int rc = -EINVAL;
  if(!chg)
	  return rc;

  chg->battery_class.name = "qcom-battery";
  chg->battery_class.class_groups = battery_class_groups;
  rc = class_register(&chg->battery_class);
  if (rc < 0) {
	  pr_err("Failed to create battery_class rc=%d\n", rc);
  }

  chg->batt_device.class = &chg->battery_class;
  dev_set_name(&chg->batt_device, "odm_battery");
  chg->batt_device.parent = chg->dev;
  chg->batt_device.groups = battery_attr_groups;
  rc = device_register(&chg->batt_device);
  if (rc < 0) {
	  pr_err("Failed to create battery_class rc=%d\n", rc);
  }

  return rc;
}
//- bug 761884, tankaikun@wt, add 20220720, charger bring up

static int wtchg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct wt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_WTCHG_UPDATE_WORK:
		wtchg_set_charge_work(chg);
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		chg->typec_cc_orientation = val1;
		break;
	case PSY_IIO_OTG_ENABLE:
		wtchg_set_main_chg_otg(chg, val1);
		break;
	//+ EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit
	case PSY_IIO_PD_VOLTAGE_MIN:
		//pr_err("%s:rt_pd_manager min_vol\n", __func__);
		wtchg_set_prop_pd_voltage_min(chg, val1);
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		//pr_err("%s:rt_pd_manager max_vol\n", __func__);
		wtchg_set_prop_pd_voltage_max(chg, val1);
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		//pr_err("%s:rt_pd_manager current_max\n", __func__);
		chg->pd_cur_max = val1;
		break;
	//- EXTBP230807-04129, xiejiaming@wt, 20230816 add, configure pd 5V/0.5A limit
	case PSY_IIO_PD_ACTIVE:
		chg->pd_active = val1;
		if (chg->pd_active)
			chg->real_type = POWER_SUPPLY_TYPE_USB_PD;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
	case PSY_IIO_PD_IN_HARD_RESET:
	case PSY_IIO_USB_REAL_TYPE:
	case PSY_IIO_APDO_MAX_VOLT:
	case PSY_IIO_APDO_MAX_CURR:
	case PSY_IIO_TYPEC_MODE:
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int wtchg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct wt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;
	switch (chan->channel) {
	case PSY_IIO_USB_REAL_TYPE:
		wtchg_get_charge_bc12_type(chg);
		*val1 = wtchg_get_usb_real_type(chg);
		break;
	case PSY_IIO_OTG_ENABLE:
		*val1 = chg->otg_enable;
		break;
	case PSY_IIO_VBUS_VAL_NOW:
		*val1 = wtchg_get_vbus_voltage(chg);
	case PSY_IIO_WTCHG_UPDATE_WORK:
	case PSY_IIO_PD_ACTIVE:
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
	case PSY_IIO_PD_IN_HARD_RESET:
	case PSY_IIO_PD_CURRENT_MAX:
	case PSY_IIO_PD_VOLTAGE_MIN:
	case PSY_IIO_PD_VOLTAGE_MAX:
	case PSY_IIO_APDO_MAX_VOLT:
	case PSY_IIO_APDO_MAX_CURR:
	case PSY_IIO_TYPEC_MODE:
		break;
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	case PSY_IIO_HV_DISABLE_DETECT:
		pr_err("%s enter PSY_IIO_HV_DISABLE_DETECT --\n", __func__);

		*val1 = wtchg_hv_disable(chg, batt_hv_disable);
		break;
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	default:
		pr_debug("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int wtchg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct wt_chg *chg = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chg->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(wtchg_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info wtchg_iio_info = {
	.read_raw	= wtchg_iio_read_raw,
	.write_raw	= wtchg_iio_write_raw,
	.of_xlate	= wtchg_iio_of_xlate,
};

static int wtchg_init_iio_psy(struct wt_chg *chg)
{
	struct iio_dev *indio_dev = chg->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(wtchg_iio_psy_channels);
	int rc, i;

	pr_err("battery_init_iio_psy start\n");
	chg->iio_chan = devm_kcalloc(chg->dev, num_iio_channels,
				sizeof(*chg->iio_chan), GFP_KERNEL);
	if (!chg->iio_chan)
		return -ENOMEM;

	chg->int_iio_chans = devm_kcalloc(chg->dev,
				num_iio_channels,
				sizeof(*chg->int_iio_chans),
				GFP_KERNEL);
	if (!chg->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &wtchg_iio_info;
	indio_dev->dev.parent = chg->dev;
	indio_dev->dev.of_node = chg->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chg->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "wt_chg";

	for (i = 0; i < num_iio_channels; i++) {
		chg->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chg->iio_chan[i];
		chg->int_iio_chans[i].channel = chan;

		chan->address = i;
		chan->channel = wtchg_iio_psy_channels[i].channel_num;
		chan->type = wtchg_iio_psy_channels[i].type;
		chan->datasheet_name =
			wtchg_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			wtchg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			wtchg_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chg->dev, indio_dev);
	if (rc)
		pr_err("Failed to register battery IIO device, rc=%d\n", rc);

	pr_err("battery IIO device, rc=%d\n", rc);
	return rc;
}

static int wtchg_init_ext_iio_psy(struct wt_chg *chg)
{
	struct iio_channel **iio_list;

	if (!chg)
		return -ENOMEM;

	iio_list = wtchg_get_ext_channels(chg->dev, main_iio_chan_name,
		ARRAY_SIZE(main_iio_chan_name));
	if (!IS_ERR(iio_list))
		chg->main_chg_ext_iio_chans = iio_list;
	else {
		pr_err(" main_chg_ext_iio_chans error \n");
		chg->main_chg_ext_iio_chans = NULL;
		return -ENOMEM;
	}

	iio_list = wtchg_get_ext_channels(chg->dev, qg_ext_iio_chan_name,
		ARRAY_SIZE(qg_ext_iio_chan_name));
	if (!IS_ERR(iio_list))
		chg->gq_ext_iio_chans = iio_list;
	else {
		pr_err(" gq_ext_iio_chans error \n");
		chg->gq_ext_iio_chans = NULL;
		return -ENOMEM;
	}

	iio_list = wtchg_get_ext_channels(chg->dev, afc_chg_iio_chan_name,
		ARRAY_SIZE(afc_chg_iio_chan_name));
	if (!IS_ERR(iio_list))
		chg->afc_chg_ext_iio_chans = iio_list;
	else {
		pr_err(" afc_chg_ext_iio_chans error \n");
		chg->afc_chg_ext_iio_chans = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int wt_chg_probe(struct platform_device *pdev)
{
	struct wt_chg *wt_chg = NULL;
	struct iio_dev *indio_dev;
	int ret;

	pr_err("wt_chg probe start\n");
	if (!pdev->dev.of_node)
		return -ENODEV;

	pr_err("wt_chg probe start 1\n");
	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*wt_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}

	wt_chg = iio_priv(indio_dev);
	wt_chg->indio_dev = indio_dev;
	wt_chg->dev = &pdev->dev;
	wt_chg->pdev = pdev;
	platform_set_drvdata(pdev, wt_chg);
	wt_chg->pre_vbus_online = 0;

	pr_err("wt_chg probe start 2\n");
	ret = wtchg_init_iio_psy(wt_chg);
	if (ret < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", ret);
	}

	ret = wtchg_init_ext_iio_psy(wt_chg);
	if (ret < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", ret);
	}

	ret = wtchg_batt_parse_dt(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

#ifdef WT_WL_CHARGE
	//+bug761884, tankaikun@wt, add 20220711,Add wireless step charge
	wt_chg->wtchg_wl_wake_source.source = wakeup_source_register(NULL,
							"wtchg_wl_wake");
	wt_chg->wtchg_wl_wake_source.disabled = 1;
	//-bug761884, tankaikun@wt, add 20220711,Add wireless step charge
#endif

	//+bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api
	wt_chg->wtchg_ato_soc_wake_source.source = wakeup_source_register(NULL,
							"wtchg_ato_user_wake");
	wt_chg->wtchg_ato_soc_wake_source.disabled = 1;
	//-bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api

	//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	wt_chg->wtchg_alarm_wake_source.source = wakeup_source_register(NULL,
							"wtchg_alarm_wake");
	wt_chg->wtchg_alarm_wake_source.disabled = 1;
	//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	//+bug825533, tankaikun@wt, add 20230216, add software control iterm
	wt_chg->wtchg_iterm_wake_source.source = wakeup_source_register(NULL,
							"wtchg_iterm_wake");
	wt_chg->wtchg_iterm_wake_source.disabled = 1;
	//-bug825533, tankaikun@wt, add 20230216, add software control iterm
	wt_chg->wtchg_off_charger_wake_source.source = wakeup_source_register(NULL,
							"wtchg_off_charger_wake");
	wt_chg->wtchg_off_charger_wake_source.disabled = 1;

	//+bug761884, tankaikun@wt, add 20220730, add backlight notify
#if defined(CONFIG_FB)
	ret = backlight_register_fb(wt_chg);
	if (ret)
		pr_err("backlight_register_fb failed rc=%d\n", ret);
#endif /* CONFIG_FB */
	//-bug761884, tankaikun@wt, add 20220730, add backlight notify

	wtchg_batt_init_config(wt_chg);
	ret = wtchg_init_batt_psy(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", ret);
		goto cleanup;
	}

	ret = wtchg_init_ac_psy(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize ac psy rc=%d\n", ret);
		goto cleanup;
	}
	
	ret = wtchg_init_usb_psy(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", ret);
		goto cleanup;
	}

	ret = wtchg_init_otg_psy(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize otg psy rc=%d\n", ret);
		goto cleanup;
	}

#ifdef WT_WL_CHARGE
	ret = wtchg_init_wl_psy(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize wl psy rc=%d\n", ret);
		goto cleanup;
	}
#endif
	//+bug761884, tankaikun@wt, add 20220811, thermal step charge strategy
	ret = wtchg_init_thermal_engine_table(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't thermal engine table rc=%d\n", ret);
	}
	//-bug761884, tankaikun@wt, add 20220811, thermal step charge strategy

	//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	mutex_init(&wt_chg->resume_complete_lock);

#ifdef CONFIG_QGKI_BUILD
	if (alarmtimer_get_rtcdev()) {
		alarm_init(&wt_chg->wtchg_alarm, ALARM_BOOTTIME, wtchg_alarm_cb);
	} else {
		pr_err("wt_chg_probe Couldn't get rtc device wt_charger probe fail \n");
		ret = -ENODEV;
		goto cleanup;
	}
#endif /*CONFIG_QGKI_BUILD*/
	//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	ret = wt_init_dev_class(wt_chg);
	if (ret < 0) {
		pr_err("Couldn't initialize dev class rc=%d\n", ret);
		goto cleanup;
	}

/* P86801AA1-13544, gudi1@wt, add 20231017, usb if*/
	g_wt_chg = wt_chg;

	INIT_WORK(&wt_chg->wtchg_alarm_work, wtchg_alarm_work);
	INIT_DELAYED_WORK(&wt_chg->wt_chg_work, wt_chg_main);
#ifdef CONFIG_QGKI_BUILD
	INIT_DELAYED_WORK(&wt_chg->wt_float_recheck_work, wtchg_float_charge_type_check);
#endif /*CONFIG_QGKI_BUILD*/
#ifdef WT_WL_CHARGE
	//+bug761884, tankaikun@wt, add 20220711, Add wireless step charge
	INIT_DELAYED_WORK(&wt_chg->fcc_stepper_work, wtchg_fcc_stepper_work);
	INIT_DELAYED_WORK(&wt_chg->icl_stepper_work, wtchg_icl_stepper_work);
	//-bug761884, tankaikun@wt, add 20220711, Add wireless step charge
#endif
	//+bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api
	INIT_DELAYED_WORK(&wt_chg->ato_soc_user_control_work, wtchg_ato_soc_user_control_work);
	//-bug790138 tankaikun.wt, add, 20220809. Add ato soc user control api

	schedule_delayed_work(&wt_chg->wt_chg_work, 1000);

	pr_err("wt_chg probe succeed !\n");
	return 0;

cleanup:
	pr_err("wt_chg probe fail\n");
	//+ bug 761884, tankaikun@wt, add 20220730, add backlight notify
#if defined(CONFIG_FB)
	backlight_unregister_fb(wt_chg);
#endif /* CONFIG_FB */
	//- bug 761884, tankaikun@wt, add 20220730, add backlight notify

	//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
	wakeup_source_unregister(wt_chg->wtchg_alarm_wake_source.source);
#ifdef WT_WL_CHARGE
	wakeup_source_unregister(wt_chg->wtchg_wl_wake_source.source);
#endif
	wakeup_source_unregister(wt_chg->wtchg_ato_soc_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_iterm_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_off_charger_wake_source.source);

	mutex_destroy(&wt_chg->resume_complete_lock);
	//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	return ret;
}

static int wt_chg_remove(struct platform_device *pdev)
{
	struct wt_chg *wt_chg = platform_get_drvdata(pdev);

	//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
#ifdef CONFIG_QGKI_BUILD
	alarm_cancel(&wt_chg->wtchg_alarm);
#endif /* CONFIG_QGKI_BUILD */

	wakeup_source_unregister(wt_chg->wtchg_wl_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_ato_soc_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_alarm_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_iterm_wake_source.source);
	wakeup_source_unregister(wt_chg->wtchg_off_charger_wake_source.source);
	mutex_destroy(&wt_chg->resume_complete_lock);
	//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

	kfree(wt_chg);
	return 0;
}

static void wt_chg_shutdown(struct platform_device *pdev)
{
	struct wt_chg *wt_chg = platform_get_drvdata(pdev);

	if(wt_chg->ship_mode){
		wtchg_set_slave_chg_ship_mode(wt_chg);
		wtchg_set_main_chg_ship_mode(wt_chg);
        }
}

//+bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend
static int wt_chg_resume(struct device *dev)
{
	struct wt_chg *wt_chg = dev_get_drvdata(dev);

	dev_err(wt_chg->dev,"wt_chg_resume \n");

	mutex_lock(&wt_chg->resume_complete_lock);
	wt_chg->resume_completed = true;
	if (wt_chg->wtchg_alarm_status) {
		wt_chg->wtchg_alarm_status = false;
#ifdef CONFIG_QGKI_BUILD
		alarm_cancel(&wt_chg->wtchg_alarm);
#endif /* CONFIG_QGKI_BUILD */
	}
	mutex_unlock(&wt_chg->resume_complete_lock);
	schedule_delayed_work(&wt_chg->wt_chg_work, msecs_to_jiffies(30));
	return 0;
}

static int wt_chg_suspend(struct device *dev)
{
	struct wt_chg *wt_chg = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = WT_CHARGER_ALARM_TIME;

	dev_err(wt_chg->dev,"wt_chg_suspend \n");

	mutex_lock(&wt_chg->resume_complete_lock);
	wt_chg->resume_completed = false;

	if (wt_chg->vbus_online && !wt_chg->wtchg_alarm_status) {
		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC, (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
#ifdef CONFIG_QGKI_BUILD
		alarm_start(&wt_chg->wtchg_alarm, ktime_add(now, add));
#endif /*CONFIG_QGKI_BUILD*/
		wt_chg->wtchg_alarm_status = true;
	}
	mutex_unlock(&wt_chg->resume_complete_lock);
	cancel_delayed_work(&wt_chg->wt_chg_work);
	return 0;
}

static const struct dev_pm_ops wt_chg_pm_ops = {
	.resume		= wt_chg_resume,
	.suspend	= wt_chg_suspend,
};
//-bug761884, tankaikun@wt, add 20220831, fix iic read/write return -13 when system suspend

static const struct of_device_id wt_chg_dt_match[] = {
	{.compatible = "qcom,wt_chg"},
	{},
};

static struct platform_driver wt_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wt_chg",
		.of_match_table = wt_chg_dt_match,
		.pm		= &wt_chg_pm_ops,
	},
	.probe = wt_chg_probe,
	.remove = wt_chg_remove,
	.shutdown = wt_chg_shutdown,
};

static int __init wt_chg_init(void)
{
    platform_driver_register(&wt_chg_driver);
	pr_err("wt_chg init end\n");
    return 0;
}

static void __exit wt_chg_exit(void)
{
	pr_err("wt_chg exit\n");
	platform_driver_unregister(&wt_chg_driver);
}

module_init(wt_chg_init);
module_exit(wt_chg_exit);

MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL v2");
