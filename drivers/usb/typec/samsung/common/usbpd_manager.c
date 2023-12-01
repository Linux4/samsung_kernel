/*
 *	USB PD Driver - Device Policy Manager
 */

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/of_gpio.h>

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/usb/typec/samsung/common/usbpd.h>
#include <linux/misc/samsung/muic/common/muic.h>
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/misc/samsung/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#include <linux/usb/typec/samsung/common/usbpd_ext.h>
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
#include <linux/usb/typec/samsung/s2mu106/usbpd-s2mu106.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
#include <linux/usb/typec_s2m/s2mu107/usbpd-s2mu107.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
#include <linux/usb/typec/samsung/s2mf301/usbpd-s2mf301.h>
#endif
#elif IS_ENABLED(CONFIG_PDIC_NOITIFIER)
#include <linux/usb/typec/slsi/common/usbpd.h>
#include <linux/delay.h>
#include <linux/muic/common/muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/slsi/common/s2m_pdic_notifier.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif
#endif
#include <linux/usb/typec/slsi/common/usbpd_ext.h>
#endif

/* switch device header */
#if IS_ENABLED(CONFIG_SWITCH)
#include <linux/switch.h>
#endif /* CONFIG_SWITCH */

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif

#include <linux/completion.h>

#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
extern void select_pdo(int num);
void usbpd_manager_select_pdo(int num);
extern void (*fp_select_pdo)(int num);
#if defined(CONFIG_PDIC_PD30)
extern int (*fp_sec_pd_select_pps)(int num, int ppsVol, int ppsCur);
extern int (*fp_sec_pd_get_apdo_max_power)(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int (*fp_pps_enable)(int num, int ppsVol, int ppsCur, int enable);
int (*fp_get_pps_voltage)(void);
#endif
#endif
#endif

#if IS_ENABLED(CONFIG_SWITCH)
static struct switch_dev switch_dock = {
	.name = "ccic_dock",
};
#endif

static enum power_supply_property ccic_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *ccic_supplied_to[] = {
	"battery",
};

static char DP_Pin_Assignment_Print[7][40] = {
    {"DP_Pin_Assignment_None"},
    {"DP_Pin_Assignment_A"},
    {"DP_Pin_Assignment_B"},
    {"DP_Pin_Assignment_C"},
    {"DP_Pin_Assignment_D"},
    {"DP_Pin_Assignment_E"},
    {"DP_Pin_Assignment_F"},
};

void usbpd_manager_select_pdo_cancel(struct device *dev);

#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE)
int usbpd_manager_get_side_check(void)
{
	struct usbpd_data *pd_data = pd_noti.pd_data;
	int ret = 0;

	ret = PDIC_OPS_FUNC(get_side_check, pd_data);

	return ret;
}
EXPORT_SYMBOL_GPL(usbpd_manager_get_side_check);
#endif

void usbpd_manager_select_pdo(int num)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short = 0;
#if defined(CONFIG_PDIC_PD30)
	int pps_enable = 0;
#endif
	PDIC_OPS_PARAM_FUNC(get_vbus_short_check, pd_data, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		return;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1) {
        pr_info("%s, flashmode == 1\n", __func__);
		goto exit;
    }

	if (pd_data->policy.plug_valid == 0) {
		pr_info(" %s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}

	if (pd_noti.sink_status.available_pdo_num < 1) {
		pr_info("%s: available pdo is 0\n", __func__);
		goto exit;
	}

	if (pd_noti.sink_status.power_list[num].apdo) {
		pr_info("%s, PDO(%d) is APDO\n", __func__, num);
		goto exit;
	}

#if defined(CONFIG_PDIC_PD30)
	PDIC_OPS_PARAM_FUNC(get_pps_enable, pd_data, &pps_enable);

	if (pps_enable == PPS_ENABLE) {
		pr_info(" %s : forced pps disable\n", __func__);
		PDIC_OPS_PARAM_FUNC(pps_enable, pd_data, PPS_DISABLE);
		PDIC_OPS_FUNC(force_pps_disable, pd_data);
	}
#endif
	if (pd_noti.sink_status.selected_pdo_num == num)
		usbpd_manager_plug_attach(pd_data->dev, ATTACHED_DEV_TYPE3_CHARGER_MUIC);
	else if (num > pd_noti.sink_status.available_pdo_num)
		pd_noti.sink_status.selected_pdo_num = pd_noti.sink_status.available_pdo_num;
	else if (num < 1)
		pd_noti.sink_status.selected_pdo_num = 1;
	else
		pd_noti.sink_status.selected_pdo_num = num;
	pr_info(" %s : PDO(%d) is selected to change\n", __func__, pd_noti.sink_status.selected_pdo_num);

	schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(50));
exit:
	mutex_unlock(&manager->pdo_mutex);
}

#if defined(CONFIG_PDIC_PD30)
int usbpd_manager_select_pps(int num, int ppsVol, int ppsCur)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short = 0;
	int ret = 0;
	int pps_enable = -1;

	PDIC_OPS_PARAM_FUNC(get_vbus_short_check, pd_data, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1)
		goto exit;

	if (pd_data->policy.plug_valid == 0) {
		pr_info(" %s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}
	/* [dchg] TODO: check more below option */
	if (num > pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		ret = -EINVAL;
		goto exit;
	}

	PDIC_OPS_PARAM_FUNC(get_pps_enable, pd_data, &pps_enable);

	if (pps_enable == PPS_ENABLE) {
		pr_info(" %s : forced pps disable\n", __func__);
		PDIC_OPS_PARAM_FUNC(pps_enable, pd_data, PPS_DISABLE);
		PDIC_OPS_FUNC(force_pps_disable, pd_data);
	}

	pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_noti.sink_status.pps_voltage = ppsVol;
	pd_noti.sink_status.pps_current = ppsCur;

	pr_info(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

//	schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(50));
	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);

exit:
	mutex_unlock(&manager->pdo_mutex);

	return ret;
}

int usbpd_manager_get_apdo_max_power(unsigned int *pdo_pos,
	unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	int i;
	int ret = 0;
	int max_current = 0, max_voltage = 0, max_power = 0;

	if (!pd_noti.sink_status.has_apdo) {
		pr_info("%s: pd don't have apdo\n", __func__);
		return -1;
	}

	/* First, get TA maximum power from the fixed PDO */
	for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
		if (!(pd_noti.sink_status.power_list[i].apdo)) {
			max_voltage = pd_noti.sink_status.power_list[i].max_voltage;
			max_current = pd_noti.sink_status.power_list[i].max_current;
			max_power = max_voltage*max_current;	/* uW */
			*taMaxPwr = max_power;	/* mW */
		}
	}

	if (*pdo_pos == 0) {
		/* Get the proper PDO */
		for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
			if (pd_noti.sink_status.power_list[i].apdo) {
				if (pd_noti.sink_status.power_list[i].max_voltage >= *taMaxVol) {
					*pdo_pos = i;
					*taMaxVol = pd_noti.sink_status.power_list[i].max_voltage;
					*taMaxCur = pd_noti.sink_status.power_list[i].max_current;
					break;
				}
			}
			if (*pdo_pos)
				break;
		}

		if (*pdo_pos == 0) {
			pr_info("mv (%d) and ma (%d) out of range of APDO\n",
				*taMaxVol, *taMaxCur);
			ret = -EINVAL;
		}
	} else {
		/* If we already have pdo object position, we don't need to search max current */
		ret = -ENOTSUPP;
	}

	pr_info("%s : *pdo_pos(%d), *taMaxVol(%d), *maxCur(%d), *maxPwr(%d)\n",
		__func__, *pdo_pos, *taMaxVol, *taMaxCur, *taMaxPwr);

	return ret;
}

int usbpd_manager_pps_enable(int num, int ppsVol, int ppsCur, int enable)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short = 0;
	int ret = 0;
	int pps_enable = 0;

	if (pd_data->phy_ops.pps_enable == NULL) {
		pr_info("%s : pps_enable function is not present\n", __func__);
		return -ENOMEM;
	}

	PDIC_OPS_PARAM_FUNC(get_vbus_short_check, pd_data, &vbus_short);

	if (vbus_short) {
		pr_info("%s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1)
		goto exit;

	if (pd_data->policy.plug_valid == 0) {
		pr_info("%s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}

	PDIC_OPS_PARAM_FUNC(get_pps_enable, pd_data, &pps_enable);

	if ((pps_enable == enable) && pps_enable) {
		pr_info("%s : auto pps is already enabled\n", __func__);
		goto exit;
	}


	/* [dchg] TODO: check more below option */
	if (num > pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		ret = -EINVAL;
		goto exit;
	}

	pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_noti.sink_status.pps_voltage = ppsVol;
	pd_noti.sink_status.pps_current = ppsCur;

	pr_info("%s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	if (enable) {
//		schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(0));
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
		msleep(150);
		PDIC_OPS_PARAM_FUNC(pps_enable, pd_data, PPS_ENABLE);
	} else {
		PDIC_OPS_PARAM_FUNC(pps_enable, pd_data, PPS_DISABLE);
		msleep(100);
//		schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(0));
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
	}
exit:
	mutex_unlock(&manager->pdo_mutex);

	return ret;
}

int usbpd_manager_get_pps_voltage(void)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;

	int volt = PDIC_OPS_FUNC(get_pps_voltage, pd_data);

	pr_info("%s, volt:%d\n", __func__, volt);
	if (volt >= 0)
		return volt * USBPD_PPS_RQ_VOLT_UNIT;
	else
		return -1;
}
#endif

void pdo_ctrl_by_flash(bool mode)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
#if defined(CONFIG_PDIC_PD30)
	int pps_enable = 0;
#endif

	pr_info("%s: mode(%d)\n", __func__, mode);

	mutex_lock(&manager->pdo_mutex);
	if (mode)
		manager->flash_mode = 1;
	else
		manager->flash_mode = 0;

#if defined(CONFIG_PDIC_PD30)
	PDIC_OPS_PARAM_FUNC(get_pps_enable, pd_data, &pps_enable);

	if (pps_enable == PPS_ENABLE) {
		pr_info(" %s : forced pps disable\n", __func__);
		PDIC_OPS_PARAM_FUNC(pps_enable, pd_data, PPS_DISABLE);
		PDIC_OPS_FUNC(force_pps_disable, pd_data);
	}
#endif

	if (pd_noti.sink_status.selected_pdo_num != 0) {
		pd_noti.sink_status.selected_pdo_num = 1;
		usbpd_manager_select_pdo_cancel(pd_data->dev);
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_GET_SRC_CAP);
	}
	mutex_unlock(&manager->pdo_mutex);
}
EXPORT_SYMBOL_GPL(pdo_ctrl_by_flash);

void usbpd_manager_select_pdo_handler(struct work_struct *work)
{
	pr_info("%s: call select pdo handler\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
}

void usbpd_manager_select_pdo_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->select_pdo_handler);
}

void usbpd_manager_restart_discover_msg(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	mutex_lock(&manager->vdm_mutex);
	manager->alt_sended = 0;
	mutex_unlock(&manager->vdm_mutex);
}

void usbpd_manager_start_discover_msg_handler(struct work_struct *work)
{
	struct usbpd_manager_data *manager =
		container_of(work, struct usbpd_manager_data,
										start_discover_msg_handler.work);
	pr_info("%s: call handler\n", __func__);

	mutex_lock(&manager->vdm_mutex);
	if (manager->alt_sended == 0 && manager->vdm_en == 1) {
		usbpd_manager_inform_event(pd_noti.pusbpd,
						MANAGER_START_DISCOVER_IDENTITY);
		manager->alt_sended = 1;
	}
	mutex_unlock(&manager->vdm_mutex);
}

void usbpd_manager_start_discover_msg_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->start_discover_msg_handler);
}

void usbpd_manager_send_pr_swap(struct device *dev)
{
	pr_info("%s: call send pr swap msg\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_SEND_PR_SWAP);
}

void usbpd_manager_send_dr_swap(struct device *dev)
{
	pr_info("%s: call send pr swap msg\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_SEND_DR_SWAP);
}

void usbpd_manager_match_sink_cap(struct usbpd_data *pd_data)
{
	unsigned src_cur, volt, op_cur;
	data_obj_type *data_obj = &pd_data->source_data_obj;

	volt = pd_data->source_get_sink_obj.power_data_obj_sink.voltage;
	op_cur= pd_data->source_get_sink_obj.power_data_obj_sink.op_current;
	src_cur = pd_data->source_data_obj.power_data_obj.max_current;

	pr_info("%s, src_cur(%dmA) SinkCap volt(%dmV), opcur(%dmA)\n", __func__,
			src_cur * 10, volt * 50, op_cur * 10);

	if (volt * 50 > 5000)
		return;

	if ((src_cur <= op_cur) && (op_cur * 10 <= 1000)) {
		data_obj->power_data_obj.max_current = op_cur;
		pr_info("%s, SourceCap cur (%dmA)\n", __func__,
				data_obj->power_data_obj.max_current);
	}

	return;
}

void usbpd_manager_vbus_turn_on_ctrl(struct usbpd_data *pd_data, bool enable)
{
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0, retry_cnt = 0;

	pr_info("%s %d, enable=%d\n", __func__, __LINE__, enable);

	if (!pd_data->policy.plug_valid) {
		pr_info("%s, ignore vbus control\n", __func__);
		return;
	}

	psy_otg = power_supply_get_by_name("otg");

	if (psy_otg) {
		val.intval = enable;
		pd_data->is_otg_vboost = enable;
		ret = psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);

		return;
	}
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		if (enable == VBUS_ON) {
			for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
				psy_otg->desc->get_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
				if (val.intval == VBUS_OFF) {
					msleep(100);
					val.intval = enable;
					psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
				} else
					break;
			}
		}
		pr_info("otg accessory power = %d\n", on);
	}

}

static void init_source_cap_data(struct usbpd_manager_data *_data)
{
	msg_header_type *msg_header = &_data->pd_data->source_msg_header;
	data_obj_type *data_obj = &_data->pd_data->source_data_obj;

	msg_header->msg_type = USBPD_Source_Capabilities;
	msg_header->port_data_role = USBPD_DFP;
	msg_header->spec_revision = _data->pd_data->specification_revision;
	msg_header->port_power_role = USBPD_SOURCE;
	msg_header->num_data_objs = 1;

	data_obj->power_data_obj.max_current = 500 / 10;
	data_obj->power_data_obj.voltage = 5000 / 50;
	data_obj->power_data_obj.supply = POWER_TYPE_FIXED;
	data_obj->power_data_obj.data_role_swap = 1;
	data_obj->power_data_obj.dual_role_power = 1;
	data_obj->power_data_obj.usb_suspend_support = 1;
	data_obj->power_data_obj.usb_comm_capable = 1;
}

void usbpd_manager_remove_new_cap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
	init_source_cap_data(manager);
}

static void init_sink_cap_data(struct usbpd_manager_data *_data)
{
	msg_header_type *msg_header = &_data->pd_data->sink_msg_header;
	data_obj_type *data_obj = _data->pd_data->sink_data_obj;

	msg_header->msg_type = USBPD_Sink_Capabilities;
	msg_header->port_data_role = USBPD_UFP;
	msg_header->spec_revision = _data->pd_data->specification_revision;
	msg_header->port_power_role = USBPD_SINK;
	msg_header->num_data_objs = 2;

	data_obj->power_data_obj_sink.supply_type = POWER_TYPE_FIXED;
	data_obj->power_data_obj_sink.dual_role_power = 1;
	data_obj->power_data_obj_sink.higher_capability = 1;
	data_obj->power_data_obj_sink.externally_powered = 0;
	data_obj->power_data_obj_sink.usb_comm_capable = 1;
	data_obj->power_data_obj_sink.data_role_swap = 1;
	data_obj->power_data_obj_sink.voltage = 5000/50;
	data_obj->power_data_obj_sink.op_current = 3000/10;

	(data_obj + 1)->power_data_obj_sink.supply_type = POWER_TYPE_FIXED;
	(data_obj + 1)->power_data_obj_sink.voltage = 9000/50;
	(data_obj + 1)->power_data_obj_sink.op_current = 2000/10;
}

int samsung_uvdm_ready(void)
{
	int uvdm_ready = false;
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);

	if (manager->is_samsung_accessory_enter_mode)
		uvdm_ready = true;

	pr_info("uvdm ready = %d", uvdm_ready);
#endif
	return uvdm_ready;
}

void samsung_uvdm_close(void)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);

	complete(&manager->uvdm_out_wait);
	complete(&manager->uvdm_in_wait);
#endif
}

int usbpd_manager_send_samsung_uvdm_message(void *data, const char *buf, size_t size)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	int received_data = 0;
	int data_role = 0;
	int power_role = 0;

	if ((buf == NULL)||(size < sizeof(unsigned int))) {
		pr_err("%s given data is not valid !\n", __func__);
		return -EINVAL;
	}

	sscanf(buf, "%d", &received_data);

	pd_data->phy_ops.get_data_role(pd_data, &data_role);

	if (data_role == USBPD_UFP) {
		pr_err("%s, skip, now data role is ufp\n", __func__);
		return 0;
	}

	data_role = pd_data->phy_ops.get_power_role(pd_data, &power_role);

	manager->uvdm_msg_header.msg_type = USBPD_UVDM_MSG;
	manager->uvdm_msg_header.port_data_role = USBPD_DFP;
	manager->uvdm_msg_header.port_power_role = power_role;
	manager->uvdm_data_obj[0].unstructured_vdm.vendor_id = SAMSUNG_VENDOR_ID;
	manager->uvdm_data_obj[0].unstructured_vdm.vendor_defined = SEC_UVDM_UNSTRUCTURED_VDM;
	manager->uvdm_data_obj[1].object = received_data;
	if (manager->uvdm_data_obj[1].sec_uvdm_header.data_type == SEC_UVDM_SHORT_DATA) {
		pr_info("%s - process short data!\n", __func__);
		// process short data
		// phase 1. fill message header
		manager->uvdm_msg_header.num_data_objs = 2; // VDM Header + 6 VDOs = MAX 7
		// phase 2. fill uvdm header (already filled)
		// phase 3. fill sec uvdm header
		manager->uvdm_data_obj[1].sec_uvdm_header.total_number_of_uvdm_set = 1;
	} else {
		pr_info("%s - process long data!\n", __func__);
		// process long data
		// phase 1. fill message header
		// phase 2. fill uvdm header
		// phase 3. fill sec uvdm header

	}
	usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
#endif
	return 0;
}

int samsung_uvdm_write(void *data, int size)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	uint8_t *SEC_DATA;
	uint8_t rcv_data[MAX_INPUT_DATA] = {0,};
	int need_set_cnt = 0;
	int cur_set_data = 0;
	int cur_set_num = 0;
	int remained_data_size = 0;
	int accumulated_data_size = 0;
	int received_data_index = 0;
	int time_left = 0;
	int i;

	pr_info("%s \n", __func__);
	set_msg_header(&manager->uvdm_msg_header,USBPD_Vendor_Defined,7);
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	if (size <= 1) {
		pr_info("%s - process short data!\n", __func__);
		// process short data
		// phase 1. fill message header
		manager->uvdm_msg_header.num_data_objs = 2; // VDM Header + 6 VDOs = MAX 7
		// phase 2. fill uvdm header (already filled)
		// phase 3. fill sec uvdm header
		manager->uvdm_data_obj[1].sec_uvdm_header.total_number_of_uvdm_set = 1;
	} else {
		pr_info("%s - process long data!\n", __func__);
		// process long data
		// phase 1. fill message header
		// phase 2. fill uvdm header
		// phase 3. fill sec uvdm header
		// phase 4.5.6.7 fill sec data header , data , sec data tail and so on.

		set_endian(data, rcv_data, size);
		need_set_cnt = set_uvdmset_count(size);
		manager->uvdm_first_req = true;
		manager->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (manager->uvdm_first_req)
			set_sec_uvdm_header(&manager->uvdm_data_obj[0], manager->Product_ID,
					SEC_UVDM_LONG_DATA,SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(&manager->uvdm_data_obj[0], manager->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data = get_data_size(manager->uvdm_first_req,remained_data_size);

			pr_info("%s current set data size: %d, total data size %d, current uvdm set num %d\n", __func__, cur_set_data, size, cur_set_num);

			if (manager->uvdm_first_req) {
				SEC_DATA = (uint8_t *)&manager->uvdm_data_obj[3];
				for ( i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] = rcv_data[received_data_index++];
			} else {
				SEC_DATA = (uint8_t *)&manager->uvdm_data_obj[2];
				for ( i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] = rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&manager->uvdm_data_obj[0]);

			reinit_completion(&manager->uvdm_out_wait);
			usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

			time_left = wait_for_completion_interruptible_timeout(&manager->uvdm_out_wait, msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (time_left <= 0) {
				pr_err("%s tiemout \n",__func__);
				return -ETIME;
			}
			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (manager->uvdm_first_req)
				manager->uvdm_first_req = false;
			cur_set_num++;
		}
		return size;
	}
#endif
	return 0;
}

int samsung_uvdm_read(void *data)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	struct policy_data *policy = &pd_data->policy;
	uint8_t in_data[MAX_INPUT_DATA] = {0,};

	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	msg_header_type	uvdm_msg_header;

	int cur_set_data = 0;
	int cur_set_num = 0;
	int total_set_num = 0;
	int rcv_data_size = 0;
	int total_rcv_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int cal_checksum = 0;

	pr_info("%s\n", __func__);

	manager->uvdm_dir = DIR_IN;
	manager->uvdm_first_req = true;
	uvdm_msg_header.word = policy->rx_msg_header.word;

	/* 2. Common : Fill the MSGHeader */
	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if(manager->uvdm_first_req)
		set_sec_uvdm_header(&manager->uvdm_data_obj[0], manager->Product_ID,\
				SEC_UVDM_LONG_DATA, SEC_UVDM_ININIATOR, DIR_IN, 0, 0);

	/* 5. Send data to PDIC */
	reinit_completion(&manager->uvdm_in_wait);
	usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_for_completion_interruptible_timeout(&manager->uvdm_in_wait,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (time_left <= 0) {
			pr_err("%s timeout\n", __func__);
			return -ETIME;
		}

		/* read data */
		uvdm_msg_header.word = policy->rx_msg_header.word;
		for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
			uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

		if (manager->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[1].object;
			SEC_TX_HEADER.object = uvdm_data_obj[2].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			} else {
				/* 1. check the data size received */
				size = SEC_TX_HEADER.total_size;
				cur_set_data = SEC_TX_HEADER.cur_size;
				cur_set_num = SEC_TX_HEADER.order_cur_set;
				total_set_num = SEC_RES_HEADER.total_set_num;

				manager->uvdm_first_req = false;
				/* 2. copy data to buffer */
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++) {
					in_data[rcv_data_size++] =uvdm_data_obj[3+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
				}
				total_rcv_size += cur_set_data;
				manager->uvdm_first_req = false;
			}
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++)
				in_data[rcv_data_size++] = uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
		}
		/* 3. Check Checksum */
		SEC_TX_TAILER.object =uvdm_data_obj[6].object;
		cal_checksum = get_checksum((char *)&uvdm_data_obj[0], 4, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		/* 5. Common : Fill the MSGHeader */
		set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
		/* 5.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);
		/* 5.2. Common : Fill the First SEC_VDMHeader*/

		set_sec_uvdm_rx_header(&manager->uvdm_data_obj[0], cur_set_num, cur_set_data, ack);
		reinit_completion(&manager->uvdm_in_wait);
		usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
	} while ( cur_set_num < total_set_num);

	set_endian(in_data, data, size);

	return size;
#else
	return 0;
#endif
}

void usbpd_manager_receive_samsung_uvdm_message(struct usbpd_data *pd_data)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct policy_data *policy = &pd_data->policy;
	int i = 0;
	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	struct usbpd_manager_data *manager = &pd_data->manager;
	s_uvdm_header SEC_UVDM_RES_HEADER;
	//s_uvdm_header SEC_UVDM_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;
	uvdm_msg_header.word = policy->rx_msg_header.word;


	for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
		uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

	uvdm_msg_header.word = policy->rx_msg_header.word;

	pr_info("%s dir %s \n", __func__, (manager->uvdm_dir==DIR_OUT)?"OUT":"IN");
	if (manager->uvdm_dir == DIR_OUT) {
		if (manager->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					SEC_UVDM_RX_HEADER.object = uvdm_data_obj[2].object;
					if (SEC_UVDM_RX_HEADER.result_value != RX_ACK)
						pr_err("%s Busy or Nak received.\n", __func__);
				} else
					pr_err("%s Response type is wrong.\n", __func__);
			} else {
				if ( SEC_UVDM_RES_HEADER.cmd_type == RES_ACK)
					pr_err("%s Short packet: ack received\n", __func__);
				else
					pr_err("%s Short packet: Response type is wrong\n", __func__);
			}
		/* Dir: out */
		} else {
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RX_HEADER.result_value != RX_ACK)
				pr_err("%s Busy or Nak received.\n", __func__);
		}
		complete(&manager->uvdm_out_wait);
	} else {
		if (manager->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.cmd_type != RES_ACK) {
				pr_err("%s Busy or Nak received.\n", __func__);
				return;
			}
		}

		complete(&manager->uvdm_in_wait);
	}

	return;
#else
	return;
#endif
}

void usbpd_manager_plug_attach(struct device *dev, muic_attached_dev_t new_dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	if (new_dev == ATTACHED_DEV_TYPE3_CHARGER_MUIC) {
		if (policy->send_sink_cap || (manager->ps_rdy == 1 &&
		manager->prev_available_pdo != pd_noti.sink_status.available_pdo_num)) {
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
			pd_noti.event = IFCONN_NOTIFY_EVENT_PD_SINK_CAP;
#elif defined(CONFIG_PDIC_NOTIFIER)
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
#endif
			policy->send_sink_cap = 0;
		} else
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
			pd_noti.event = IFCONN_NOTIFY_EVENT_PD_SINK;
#elif defined(CONFIG_PDIC_NOTIFIER)
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
#endif
		manager->ps_rdy = 1;
		manager->prev_available_pdo = pd_noti.sink_status.available_pdo_num;
		pd_data->phy_ops.send_pd_info(pd_data, 1);
	}
}

void usbpd_manager_plug_detach(struct device *dev, bool notify)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug detached\n", __func__);

	usbpd_policy_reset(pd_data, PLUG_DETACHED);
#if defined(CONFIG_S2M_PDIC_NOTIFIER)
	if (notify)
		s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
#endif
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	if (pd_noti.event != IFCONN_NOTIFY_EVENT_DETACH) {
#elif defined(CONFIG_PDIC_NOTIFIER)
	if (pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
#endif
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
		pd_noti.event = IFCONN_NOTIFY_EVENT_DETACH;
#elif defined(CONFIG_PDIC_NOTIFIER)
		pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
#endif
		PDIC_OPS_PARAM_FUNC(send_pd_info, pd_data, 0);
	}
}

void usbpd_manager_acc_detach(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	if ( manager->acc_type != PDIC_DOCK_DETACHED ) {
		pr_info("%s: schedule_delayed_work \n", __func__);
		if ( manager->acc_type == PDIC_DOCK_HMT )
			schedule_delayed_work(&manager->acc_detach_handler, msecs_to_jiffies(1000));
		else
			schedule_delayed_work(&manager->acc_detach_handler, msecs_to_jiffies(0));
	}
}

int usbpd_manager_command_to_policy(struct device *dev,
		usbpd_manager_command_type command)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->cmd |= command;

	usbpd_kick_policy_work(dev);

	/* TODO: check result
	if (manager->event) {
	 ...
	}
	*/
	return 0;
}

void usbpd_manager_inform_event(struct usbpd_data *pd_data,
		usbpd_manager_event_type event)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
	int ret = 0;

	manager->event = event;

	switch (event) {
	case MANAGER_DISCOVER_IDENTITY_ACKED:
		usbpd_manager_get_identity(pd_data);
		/* Ellisys : TD.PD.VDMD.E1 tVDMSenderResponse Deadline */
		/* MQP : Protocol Tests BMC-PORT-SEQ-PRSWAP */
		/* Check Modal Operation bit when RX DIS_ID ACK */
		if ((pd_data->protocol_rx.data_obj[1].id_header_vdo.USB_Vendor_ID == SAMSUNG_VENDOR_ID)
			|| (pd_data->protocol_rx.data_obj[1].id_header_vdo.modal_op_supported))
			usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_DISCOVER_SVID);
		break;
	case MANAGER_DISCOVER_SVID_ACKED:
		usbpd_manager_get_svids(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_VDM_DISCOVER_MODE);
		break;
	case MANAGER_DISCOVER_MODE_ACKED:
		ret = usbpd_manager_get_modes(pd_data);
		if (ret == USBPD_DP_SUPPORT)
			usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_VDM_ENTER_MODE);
		break;
	case MANAGER_ENTER_MODE_ACKED:
		usbpd_manager_enter_mode(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_VDM_STATUS_UPDATE);
		break;
	case MANAGER_STATUS_UPDATE_ACKED:
		usbpd_manager_get_status(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_VDM_DisplayPort_Configure);
		break;
	case MANAGER_DisplayPort_Configure_ACKED:
		usbpd_manager_get_configure(pd_data);
		break;
	case MANAGER_ATTENTION_REQUEST:
		usbpd_manager_get_attention(pd_data);
		break;
	case MANAGER_NEW_POWER_SRC:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_NEW_POWER_SRC);
		break;
	case MANAGER_GET_SRC_CAP:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_GET_SRC_CAP);
		break;
	case MANAGER_UVDM_SEND_MESSAGE:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_UVDM_SEND_MESSAGE);
		break;
	case MANAGER_UVDM_RECEIVE_MESSAGE:
		usbpd_manager_receive_samsung_uvdm_message(pd_data);
		break;
	case MANAGER_START_DISCOVER_IDENTITY:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_VDM_DISCOVER_IDENTITY);
		break;
	case MANAGER_SEND_PR_SWAP:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_PR_SWAP);
		break;
	case MANAGER_SEND_DR_SWAP:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_DR_SWAP);
		break;
	case MANAGER_CAP_MISMATCH:
		usbpd_manager_command_to_policy(pd_data->dev, MANAGER_REQ_GET_SNKCAP);
	default:
		pr_info("%s: not matched event(%d)\n", __func__, event);
	}
}

bool usbpd_manager_vdm_request_enabled(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
	/* TODO : checking cable discovering
	   if (pd_data->counter.discover_identity_counter
		   < USBPD_nDiscoverIdentityCount)

	   struct usbpd_manager_data *manager = &pd_data->manager;
	   if (manager->event != MANAGER_DISCOVER_IDENTITY_ACKED
	      || manager->event != MANAGER_DISCOVER_IDENTITY_NAKED)

	   return(1);
	*/

	manager->vdm_en = 1;
    pr_info("%s, start VDM\n", __func__);

	schedule_delayed_work(&manager->start_discover_msg_handler, msecs_to_jiffies(30));
	return true;
}

bool usbpd_manager_power_role_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->power_role_swap;
}

bool usbpd_manager_vconn_source_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->vconn_source_swap;
}

void usbpd_manager_turn_off_vconn(struct usbpd_data *pd_data)
{
	/* TODO : Turn off vconn */
}

void usbpd_manager_turn_on_source(struct usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	pr_info("%s: usbpd plug attached\n", __func__);
#else
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug attached\n", __func__);

	manager->attached_dev = ATTACHED_DEV_TYPE3_ADAPTER_MUIC;
	s2m_pdic_notifier_attach_attached_dev(manager->attached_dev);
#endif
}

void usbpd_manager_turn_off_power_supply(struct usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	pr_info("%s: usbpd plug detached\n", __func__);

	/* TODO : Turn off power supply */
#else
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug detached\n", __func__);

	s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;
	/* TODO : Turn off power supply */
#endif
}

void usbpd_manager_turn_off_power_sink(struct usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif

	pr_info("%s: usbpd sink turn off\n", __func__);

	/* TODO : Turn off power sink */
	pd_noti.event = IFCONN_NOTIFY_EVENT_DETACH;
	ifconn_event_work(pdic_data, IFCONN_NOTIFY_BATTERY,
					IFCONN_NOTIFY_ID_ATTACH, IFCONN_NOTIFY_EVENT_DETACH, NULL);
#else
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd sink turn off\n", __func__);

	s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;
	/* TODO : Turn off power sink */
#endif
}

bool usbpd_manager_data_role_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->data_role_swap;
}

int usbpd_manager_register_switch_device(int mode)
{
#if IS_ENABLED(CONFIG_SWITCH)
	int ret = 0;
	if (mode) {
		ret = switch_dev_register(&switch_dock);
		if (ret < 0) {
			pr_err("%s: Failed to register dock switch(%d)\n",
			       __func__, ret);
			return -ENODEV;
		}
	} else {
		switch_dev_unregister(&switch_dock);
	}
#endif /* CONFIG_SWITCH */
	return 0;
}

static void usbpd_manager_send_dock_intent(int type)
{
	pr_info("%s: CCIC dock type(%d)\n", __func__, type);
#if IS_ENABLED(CONFIG_SWITCH)
	switch_set_state(&switch_dock, type);
#endif /* CONFIG_SWITCH */
}

void usbpd_manager_send_dock_uevent(u32 vid, u32 pid, int state)
{
	char switch_string[32];
	char pd_ids_string[32];

	pr_info("%s: CCIC dock : USBPD_IPS=%04x:%04x SWITCH_STATE=%d\n",
			__func__,
			le16_to_cpu(vid),
			le16_to_cpu(pid),
			state);


	snprintf(switch_string, 32, "SWITCH_STATE=%d", state);
	snprintf(pd_ids_string, 32, "USBPD_IDS=%04x:%04x",
			le16_to_cpu(vid),
			le16_to_cpu(pid));
}

void usbpd_manager_acc_detach_handler(struct work_struct *wk)
{
	struct usbpd_manager_data *manager =
		container_of(wk, struct usbpd_manager_data, acc_detach_handler.work);

	pr_info("%s: attached_dev : %d pdic dock type %d\n", __func__, manager->attached_dev, manager->acc_type);
	if (manager->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		if (manager->acc_type != PDIC_DOCK_DETACHED) {
			if (manager->acc_type != PDIC_DOCK_NEW)
				usbpd_manager_send_dock_intent(PDIC_DOCK_DETACHED);
			usbpd_manager_send_dock_uevent(manager->Vendor_ID, manager->Product_ID,
					PDIC_DOCK_DETACHED);
			manager->acc_type = PDIC_DOCK_DETACHED;
			manager->Vendor_ID = 0;
			manager->Product_ID = 0;
			manager->is_samsung_accessory_enter_mode = false;
		}
	}
}

void usbpd_manager_acc_handler_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	if (manager->acc_type != PDIC_DOCK_DETACHED) {
		pr_info("%s: cancel_delayed_work_sync \n", __func__);
		cancel_delayed_work_sync(&manager->acc_detach_handler);
	}
}

static int usbpd_manager_check_accessory(struct usbpd_manager_data *manager)
{
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	struct usbpd_data *pd_data = manager->pd_data;
	uint16_t vid = manager->Vendor_ID;
	uint16_t pid = manager->Product_ID;
	uint16_t acc_type = PDIC_DOCK_DETACHED;

	/* detect Gear VR */
	if (manager->acc_type == PDIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID:
			case GEARVR_PRODUCT_ID_1:
			case GEARVR_PRODUCT_ID_2:
			case GEARVR_PRODUCT_ID_3:
			case GEARVR_PRODUCT_ID_4:
			case GEARVR_PRODUCT_ID_5:
				acc_type = PDIC_DOCK_HMT;
				pr_info("%s : Samsung Gear VR connected.\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_VR_USE_COUNT);
#endif
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = PDIC_DOCK_DEX;
				pr_info("%s : Samsung DEX connected.\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case HDMI_PRODUCT_ID:
				acc_type = PDIC_DOCK_HDMI;
				pr_info("%s : Samsung HDMI connected.\n", __func__);
				break;
			case MPA2_PRODUCT_ID:
				acc_type = PDIC_DOCK_NEW;
				pr_info("%s : Samsung MPA2 connected. need to SRCCAP_CHANGE\n", __func__);
				usbpd_manager_match_sink_cap(pd_data);
				usbpd_manager_command_to_policy(pd_data->dev,
						MANAGER_REQ_SRCCAP_CHANGE);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				pr_info("%s : default device connected.\n", __func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch(pid) {
			case MPA_PRODUCT_ID:
				acc_type = PDIC_DOCK_MPA;
				pr_info("%s : Samsung MPA connected.\n", __func__);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				pr_info("%s : default device connected.\n", __func__);
				break;
			}
		} else {
			acc_type = PDIC_DOCK_NEW;
			pr_info("%s : unknown device connected.\n",	__func__);
		}
		manager->acc_type = acc_type;
	} else
		acc_type = manager->acc_type;

	if (acc_type != PDIC_DOCK_NEW)
		usbpd_manager_send_dock_intent(acc_type);

	usbpd_manager_send_dock_uevent(vid, pid, acc_type);
	return 1;
}

/* Ok : 0, NAK: -1 */
int usbpd_manager_get_identity(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->Vendor_ID = policy->rx_data_obj[1].id_header_vdo.USB_Vendor_ID;
	manager->Product_ID = policy->rx_data_obj[3].product_vdo.USB_Product_ID;
	manager->Device_Version = policy->rx_data_obj[3].product_vdo.Device_Version;

	pr_info("%s, Vendor_ID : 0x%x, Product_ID : 0x%x, Device Version : 0x%x\n",
			__func__, manager->Vendor_ID, manager->Product_ID, manager->Device_Version);

	if (usbpd_manager_check_accessory(manager))
		pr_info("%s, Samsung Accessory Connected.\n", __func__);

	return 0;
}

/* Ok : 0 (SVID_0 is DP support(0xff01)), NAK: -1 */
int usbpd_manager_get_svids(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif

	manager->SVID_0 = policy->rx_data_obj[1].vdm_svid.svid_0;
	manager->SVID_1 = policy->rx_data_obj[1].vdm_svid.svid_1;

	pr_info("%s, SVID_0 : 0x%x, SVID_1 : 0x%x\n", __func__,
				manager->SVID_0, manager->SVID_1);

	if (manager->SVID_0 == TypeC_DP_SUPPORT) {
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
		if (pdic_data->is_client == CLIENT_ON) {
			ifconn_event_work(pdic_data, IFCONN_NOTIFY_MUIC,
				IFCONN_NOTIFY_ID_ATTACH, IFCONN_NOTIFY_EVENT_DETACH, NULL);
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
			pdic_data->power_role = DUAL_ROLE_PROP_PR_NONE;
#endif
			ifconn_event_work(pdic_data, IFCONN_NOTIFY_USB,
				IFCONN_NOTIFY_ID_USB, IFCONN_NOTIFY_EVENT_DETACH, NULL);
			pdic_data->is_client = CLIENT_OFF;
		}

		if (pdic_data->is_host == HOST_OFF) {
			/* muic */
			ifconn_event_work(pdic_data, IFCONN_NOTIFY_MUIC,
				IFCONN_NOTIFY_ID_ATTACH, IFCONN_NOTIFY_EVENT_ATTACH, NULL);
			/* otg */
			pdic_data->is_host = HOST_ON;

			ifconn_event_work(pdic_data, IFCONN_NOTIFY_USB,
					IFCONN_NOTIFY_ID_USB,
					IFCONN_NOTIFY_EVENT_USB_ATTACH_DFP, NULL);
		}
#endif
		manager->dp_is_connect = 1;
		/* If you want to support USB SuperSpeed when you connect
		 * Display port dongle, You should change dp_hs_connect depend
		 * on Pin assignment.If DP use 4lane(Pin Assignment C,E,A),
		 * dp_hs_connect is 1. USB can support HS.If DP use 2lane(Pin Assigment B,D,F), dp_hs_connect is 0. USB
		 * can support SS */
		manager->dp_hs_connect = 1;

		/* sub is only used here to pass the Product_ID */
		/* template->sub1 = pd_info->Product_ID; */
		/* USBPD_SEND_DATA_NOTI_DP(DP_CONNECT,
				pd_info->Vendor_ID, &pd_info->Product_ID); */
		ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
				IFCONN_NOTIFY_ID_DP_CONNECT,
				IFCONN_NOTIFY_EVENT_ATTACH, manager);

		ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
				IFCONN_NOTIFY_ID_USB_DP, manager->dp_hs_connect, manager);
	}

	return 0;
}

/* Ok : 0, NAK: -1 */
int usbpd_manager_get_modes(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;
	data_obj_type *pd_obj = &policy->rx_data_obj[1];

	manager->Standard_Vendor_ID = policy->rx_data_obj[0].structured_vdm.svid;

	pr_info("%s, Standard_Vendor_ID = 0x%x\n", __func__,
				manager->Standard_Vendor_ID);

	if (manager->Standard_Vendor_ID == TypeC_DP_SUPPORT &&
			manager->SVID_0 == TypeC_DP_SUPPORT) {
		if (policy->rx_msg_header.num_data_objs > 1) {
			if (((pd_obj->displayport_capabilities.port_capability == num_UFP_D_Capable)
				&& (pd_obj->displayport_capabilities.receptacle_indication == num_USB_TYPE_C_Receptacle))
				|| ((pd_obj->displayport_capabilities.port_capability == num_DFP_D_Capable)
				&& (pd_obj->displayport_capabilities.receptacle_indication == num_USB_TYPE_C_PLUG))) {

				manager->pin_assignment = pd_obj->displayport_capabilities.ufp_d_pin_assignments;
				pr_info("%s, support UFP_D %d\n", __func__, manager->pin_assignment);
			} else if (((pd_obj->displayport_capabilities.port_capability == num_DFP_D_Capable)
				&& (pd_obj->displayport_capabilities.receptacle_indication == num_USB_TYPE_C_Receptacle))
				|| ((pd_obj->displayport_capabilities.port_capability == num_UFP_D_Capable)
				&& (pd_obj->displayport_capabilities.receptacle_indication == num_USB_TYPE_C_PLUG))) {

				manager->pin_assignment = pd_obj->displayport_capabilities.dfp_d_pin_assignments;
				pr_info("%s, support DFP_D %d\n", __func__, manager->pin_assignment);
			} else if (pd_obj->displayport_capabilities.port_capability == num_DFP_D_and_UFP_D_Capable) {
				if (pd_obj->displayport_capabilities.receptacle_indication == num_USB_TYPE_C_PLUG) {

					manager->pin_assignment = pd_obj->displayport_capabilities.dfp_d_pin_assignments;
					pr_info("%s, support DFP_D %d\n", __func__, manager->pin_assignment);
				} else {
					manager->pin_assignment = pd_obj->displayport_capabilities.ufp_d_pin_assignments;
					pr_info("%s, support UFP_D %d\n", __func__, manager->pin_assignment);
				}
			} else {
				manager->pin_assignment = DP_PIN_ASSIGNMENT_NODE;
				pr_info("%s, there is not valid object %d\n", __func__, manager->pin_assignment);
			}
		}

		return USBPD_DP_SUPPORT;
	}

	return USBPD_NOT_DP;
}

int usbpd_manager_enter_mode(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;
	manager->Standard_Vendor_ID = policy->rx_data_obj[0].structured_vdm.svid;
	manager->is_samsung_accessory_enter_mode = true;
	return 0;
}

int usbpd_manager_exit_mode(struct usbpd_data *pd_data, unsigned mode)
{
	return 0;
}

int usbpd_manager_get_status(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif

	struct usbpd_manager_data *manager = &pd_data->manager;
	bool multi_func_preference = 0;
	int pin_assignment = 0;
	data_obj_type *pd_obj = &policy->rx_data_obj[1];

	if (manager->SVID_0 != TypeC_DP_SUPPORT)
		return 0;

	if (pd_obj->displayport_status.port_connected == 0) {
		pr_info("%s, port disconnected!\n", __func__);
	}

	if (manager->is_sent_pin_configuration) {
		pr_info("%s, already sent pin configuration\n", __func__);
	}

	if (pd_obj->displayport_status.port_connected &&
			!manager->is_sent_pin_configuration) {
		multi_func_preference = pd_obj->displayport_status.multi_function_preferred;

		if (multi_func_preference) {
			if (manager->pin_assignment & DP_PIN_ASSIGNMENT_D) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_D;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_B) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_B;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_F) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_F;
			} else {
				pr_info("wrong pin assignment value\n");
			}
		} else {
			if (manager->pin_assignment & DP_PIN_ASSIGNMENT_C) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_C;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_E) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_E;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_A) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_A;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_D) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_D;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_B) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_B;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_F) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_F;
			} else {
				pr_info("wrong pin assignment value\n");
			}
		}
		manager->dp_selected_pin = pin_assignment;

		manager->is_sent_pin_configuration = 1;

		pr_info("%s multi_func_preference %d  %s\n", __func__,
			multi_func_preference, DP_Pin_Assignment_Print[pin_assignment]);
	}

	if (pd_obj->displayport_status.hpd_state)
		manager->hpd = IFCONN_NOTIFY_HIGH;
	else
		manager->hpd = IFCONN_NOTIFY_LOW;

	if (pd_obj->displayport_status.irq_hpd)
		manager->hpdirq = IFCONN_NOTIFY_IRQ;

	ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
					IFCONN_NOTIFY_ID_DP_HPD, manager->hpdirq, manager);

	return 0;
}

int usbpd_manager_get_configure(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif

	if (manager->SVID_0 == TypeC_DP_SUPPORT)
		ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
			IFCONN_NOTIFY_ID_DP_LINK_CONF,
			IFCONN_NOTIFY_EVENT_ATTACH, manager);

	return 0;
}

int usbpd_manager_get_attention(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool multi_func_preference = 0;
	int pin_assignment = 0;
	data_obj_type *pd_obj = &policy->rx_data_obj[1];

	if (manager->SVID_0 != TypeC_DP_SUPPORT)
		return 0;

	if (pd_obj->displayport_status.port_connected == 0) {
		pr_info("%s, port disconnected!\n", __func__);
	}

	if (manager->is_sent_pin_configuration) {
		pr_info("%s, already sent pin configuration\n", __func__);
	}

	if (pd_obj->displayport_status.port_connected &&
			!manager->is_sent_pin_configuration) {
		multi_func_preference = pd_obj->displayport_status.multi_function_preferred;

		if (multi_func_preference) {
			if (manager->pin_assignment & DP_PIN_ASSIGNMENT_D) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_D;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_B) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_B;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_F) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_F;
			} else {
				pr_info("wrong pin assignment value\n");
			}
		} else {
			if (manager->pin_assignment & DP_PIN_ASSIGNMENT_C) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_C;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_E) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_E;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_A) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_A;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_D) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_D;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_B) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_B;
			} else if (manager->pin_assignment & DP_PIN_ASSIGNMENT_F) {
				pin_assignment = IFCONN_NOTIFY_DP_PIN_F;
			} else {
				pr_info("wrong pin assignment value\n");
			}
		}
		manager->dp_selected_pin = pin_assignment;

		manager->is_sent_pin_configuration = 1;

		pr_info("%s multi_func_preference %d  %s\n", __func__,
			multi_func_preference, DP_Pin_Assignment_Print[pin_assignment]);
	}

	if (pd_obj->displayport_status.hpd_state)
		manager->hpd = IFCONN_NOTIFY_HIGH;
	else
		manager->hpd = IFCONN_NOTIFY_LOW;

	if (pd_obj->displayport_status.irq_hpd)
		manager->hpdirq = IFCONN_NOTIFY_IRQ;

	ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
					IFCONN_NOTIFY_ID_DP_HPD, manager->hpdirq, manager);

	return 0;
}

void usbpd_dp_detach(struct usbpd_data *pd_data)
{
#if IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
	struct s2mf301_usbpd_data *pdic_data = pd_data->phy_driver_data;
#endif
	struct usbpd_manager_data *manager = &pd_data->manager;

	dev_info(pd_data->dev, "%s: dp_is_connect %d\n", __func__, manager->dp_is_connect);

	ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
				IFCONN_NOTIFY_ID_USB_DP, manager->dp_hs_connect, NULL);
	ifconn_event_work(pdic_data, IFCONN_NOTIFY_MANAGER,
				IFCONN_NOTIFY_ID_DP_CONNECT, IFCONN_NOTIFY_EVENT_DETACH, NULL);

	manager->dp_is_connect = 0;
	manager->dp_hs_connect = 0;
	manager->is_sent_pin_configuration = 0;

	return;
}

/*
 * InterFace Functions
 */

static int usbpd_manager_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int usbpd_manager_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct usbpd_data *pd_data = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		PDIC_OPS_FUNC(usbpd_authentic, pd_data);
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_USBPD_RESET:
			break;
        case POWER_SUPPLY_S2M_PROP_DP_ENABLED:
            pr_info("%s, DP notifier registered\n", __func__);
            pd_data->dp_enabled = 1;
            usbpd_manager_vdm_request_enabled(pd_data);
            break;
		case POWER_SUPPLY_S2M_PROP_PCP_CLK:
			PDIC_OPS_PARAM_FUNC(set_pcp_clk, pd_data, val->intval);
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

int usbpd_manager_psy_init(struct usbpd_data *pd_data, struct device *parent)
{
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	if (pd_data == NULL || parent == NULL) {
		pr_err("%s NULL data\n", __func__);
		return -1;
	}

	pd_data->ccic_desc.name           = "usbpd-manager";
	pd_data->ccic_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	pd_data->ccic_desc.get_property   = usbpd_manager_get_property;
	pd_data->ccic_desc.set_property   = usbpd_manager_set_property;
	pd_data->ccic_desc.properties     = ccic_props;
	pd_data->ccic_desc.num_properties = ARRAY_SIZE(ccic_props);

	psy_cfg.drv_data = pd_data;
	psy_cfg.supplied_to = ccic_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(ccic_supplied_to);

	pd_data->psy_ccic = power_supply_register(parent, &pd_data->ccic_desc, &psy_cfg);
	if (IS_ERR(pd_data->psy_ccic)) {
		ret = (int)PTR_ERR(pd_data->psy_ccic);
		pr_err("%s: Failed to Register psy_ccic, ret : %d\n", __func__, ret);
	}
	return ret;
}

data_obj_type usbpd_manager_select_capability(struct usbpd_data *pd_data)
{
	/* TODO: Request from present capabilities
		indicate if other capabilities would be required */
	data_obj_type obj;
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER) || (defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER))
	int pdo_num = pd_noti.sink_status.selected_pdo_num;
#if defined(CONFIG_PDIC_PD30)
	if (pd_noti.sink_status.power_list[pdo_num].apdo) {
		pr_info("%s, pps\n", __func__);
		obj.request_data_object_pps.output_voltage = pd_noti.sink_status.pps_voltage / USBPD_PPS_RQ_VOLT_UNIT;
		obj.request_data_object_pps.op_current = pd_noti.sink_status.pps_current / USBPD_PPS_RQ_CURRENT_UNIT;
		pr_info("%s, volt(%d), cur(%d)\n", __func__, obj.request_data_object_pps.output_voltage*20, obj.request_data_object_pps.op_current*50);
		obj.request_data_object_pps.no_usb_suspend = 1;
		obj.request_data_object_pps.usb_comm_capable = 1;
		obj.request_data_object_pps.capability_mismatch = 0;
		obj.request_data_object_pps.unchunked_supported = 0;
		obj.request_data_object_pps.object_position =
								pd_noti.sink_status.selected_pdo_num;
		pd_data->policy.got_pps_apdo = 1;
		return obj;
	}
#endif
	obj.request_data_object.min_current = pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.op_current = pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.object_position = pd_noti.sink_status.selected_pdo_num;
#else
	obj.request_data_object.min_current = 10;
	obj.request_data_object.op_current = 10;
	obj.request_data_object.object_position = 1;
#endif
	obj.request_data_object.no_usb_suspend = 1;
	obj.request_data_object.usb_comm_capable = 1;
	obj.request_data_object.capability_mismatch = 0;
	obj.request_data_object.give_back = 0;

	return obj;
}

int usbpd_manager_get_selected_voltage(struct usbpd_data *pd_data, int selected_pdo)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	PDIC_SINK_STATUS * pdic_sink_status = &pd_noti.sink_status;
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	ifconn_pd_sink_status_t *pdic_sink_status = &pd_noti.sink_status;
#endif
	int available_pdo = pdic_sink_status->available_pdo_num;
	int volt = 0;

	if (selected_pdo > available_pdo) {
		pr_info("%s, selected:%d, available:%d\n", __func__, selected_pdo, available_pdo);
		return 0;
	}

#if defined(CONFIG_PDIC_PD30)
	if (pdic_sink_status->power_list[selected_pdo].apdo) {
		pr_info("%s, selected pdo is apdo(%d)\n", __func__, selected_pdo);
		return 0;
	}
#endif

	volt = pdic_sink_status->power_list[selected_pdo].max_voltage;
	pr_info("%s, select_pdo : %d, selected_voltage : %dmV\n", __func__, selected_pdo, volt);

	return volt;
}


/*
   usbpd_manager_evaluate_capability
   : Policy engine ask Device Policy Manager to evaluate option
     based on supplied capabilities
	return	>0	: request object number
		0	: no selected option
*/
int usbpd_manager_evaluate_capability(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;
	int i = 0;
	int power_type = 0;
	int max_volt = 0, min_volt = 0, max_current;
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	int available_pdo_num = 0;
	ifconn_pd_sink_status_t *pdic_sink_status = &pd_noti.sink_status;
#endif
	data_obj_type *pd_obj;

	for (i = 0; i < policy->rx_msg_header.num_data_objs; i++) {
		pd_obj = &policy->rx_data_obj[i];
		power_type = pd_obj->power_data_obj_supply_type.supply_type;
		switch (power_type) {
		case POWER_TYPE_FIXED:
			max_volt = pd_obj->power_data_obj.voltage;
			max_current = pd_obj->power_data_obj.max_current;
			dev_info(pd_data->dev, "[%d] FIXED volt(%d)mV, max current(%d)\n",
					i+1, max_volt * USBPD_VOLT_UNIT, max_current * USBPD_CURRENT_UNIT);
			available_pdo_num = i + 1;
			pdic_sink_status->power_list[i + 1].max_voltage = max_volt * USBPD_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].max_current = max_current * USBPD_CURRENT_UNIT;
			break;
		case POWER_TYPE_BATTERY:
			max_volt = pd_obj->power_data_obj_battery.max_voltage;
			dev_info(pd_data->dev, "[%d] BATTERY volt(%d)mV\n",
					i+1, max_volt * USBPD_VOLT_UNIT);
			break;
		case POWER_TYPE_VARIABLE:
			max_volt = pd_obj->power_data_obj_variable.max_voltage;
			dev_info(pd_data->dev, "[%d] VARIABLE volt(%d)mV\n",
					i+1, max_volt * USBPD_VOLT_UNIT);
			break;
		case POWER_TYPE_PPS:
#if defined(CONFIG_PDIC_PD30)
			available_pdo_num = i + 1;
#endif
			max_volt = pd_obj->power_data_obj_pps.max_voltage;
            min_volt = pd_obj->power_data_obj_pps.min_voltage;
            max_current = pd_obj->power_data_obj_pps.max_current;
			dev_info(pd_data->dev, "[%d] APDO volt(%d-%d)mV %dmA\n",
					i+1, min_volt * USBPD_PPS_VOLT_UNIT, max_volt * USBPD_PPS_VOLT_UNIT,
                    max_current * USBPD_PPS_CURRENT_UNIT);
			pdic_sink_status->power_list[i + 1].max_voltage = max_volt * USBPD_PPS_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].min_voltage = min_volt * USBPD_PPS_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].max_current = max_current * USBPD_PPS_CURRENT_UNIT;
            pdic_sink_status->power_list[i + 1].apdo = true;
			pdic_sink_status->has_apdo = true;
            break;
		default:
			dev_err(pd_data->dev, "[%d] Power Type Error\n", i+1);
			break;
		}
	}

	if (manager->flash_mode == 1)
		available_pdo_num = 1;
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	if ((available_pdo_num > 0) &&
			(pdic_sink_status->available_pdo_num != available_pdo_num)) {
		policy->send_sink_cap = 1;
		pdic_sink_status->selected_pdo_num = 1;
	}
	pdic_sink_status->available_pdo_num = available_pdo_num;
	return available_pdo_num;
#else
	return 1; /* select default first obj */
#endif
}

/* return: 0: cab be met, -1: cannot be met, -2: could be met later */
int usbpd_manager_match_request(struct usbpd_data *pd_data)
{
	/* TODO: Evaluation of sink request */

	unsigned supply_type
	= pd_data->source_request_obj.power_data_obj_supply_type.supply_type;
	unsigned src_max_current,  mismatch, max_min, op, pos;

	if (supply_type == POWER_TYPE_FIXED)
		pr_info("REQUEST: FIXED\n");
	else if (supply_type == POWER_TYPE_VARIABLE)
		pr_info("REQUEST: VARIABLE\n");
	else if (supply_type == POWER_TYPE_BATTERY) {
		pr_info("REQUEST: BATTERY\n");
		goto log_battery;
	} else {
		pr_info("REQUEST: UNKNOWN Supply type.\n");
		return -1;
	}

    /* Tx Source PDO */
    src_max_current = pd_data->source_data_obj.power_data_obj.max_current;

    /* Rx Request RDO */
	mismatch = pd_data->source_request_obj.request_data_object.capability_mismatch;
	max_min = pd_data->source_request_obj.request_data_object.min_current;
	op = pd_data->source_request_obj.request_data_object.op_current;
	pos = pd_data->source_request_obj.request_data_object.object_position;

#if 0
	pr_info("%s %x\n", __func__, pd_data->source_request_obj.object);
#endif
    /*src_max_current is already *10 value ex) src_max_current 500mA */
	//pr_info("Tx SourceCap Current : %dmA\n", src_max_current*10);
	//pr_info("Rx Request Current : %dmA\n", max_min*10);

    /* Compare Pdo and Rdo */
    if ((src_max_current >= op) && (pos == 1))
		return 0;
    else
		return -1;

log_battery:
	mismatch = pd_data->source_request_obj.request_data_object_battery.capability_mismatch;
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int of_usbpd_manager_dt(struct usbpd_manager_data *_data)
{
	int ret = 0;
	struct device_node *np =
		of_find_node_by_name(NULL, "pdic-manager");

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -EINVAL;
	} else {
		ret = of_property_read_u32(np, "pdic,max_power",
				&_data->max_power);
		if (ret < 0)
			pr_err("%s error reading max_power %d\n",
					__func__, _data->max_power);

		ret = of_property_read_u32(np, "pdic,op_power",
				&_data->op_power);
		if (ret < 0)
			pr_err("%s error reading op_power %d\n",
					__func__, _data->max_power);

		ret = of_property_read_u32(np, "pdic,max_current",
				&_data->max_current);
		if (ret < 0)
			pr_err("%s error reading max_current %d\n",
					__func__, _data->max_current);

		ret = of_property_read_u32(np, "pdic,min_current",
				&_data->min_current);
		if (ret < 0)
			pr_err("%s error reading min_current %d\n",
					__func__, _data->min_current);

		_data->giveback = of_property_read_bool(np,
						     "pdic,giveback");
		_data->usb_com_capable = of_property_read_bool(np,
						     "pdic,usb_com_capable");
		_data->no_usb_suspend = of_property_read_bool(np,
						     "pdic,no_usb_suspend");

		/* source capability */
		ret = of_property_read_u32(np, "source,max_voltage",
				&_data->source_max_volt);
		ret = of_property_read_u32(np, "source,min_voltage",
				&_data->source_min_volt);
		ret = of_property_read_u32(np, "source,max_power",
				&_data->source_max_power);

		/* sink capability */
		ret = of_property_read_u32(np, "sink,capable_max_voltage",
				&_data->sink_cap_max_volt);
		if (ret < 0) {
			_data->sink_cap_max_volt = 5000;
			pr_err("%s error reading sink_cap_max_volt %d\n",
					__func__, _data->sink_cap_max_volt);
		}
	}

	return ret;
}
#endif

void usbpd_init_manager_val(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	manager->alt_sended = 0;
	manager->cmd = 0;
	manager->vdm_en = 0;
	manager->Vendor_ID = 0;
	manager->Product_ID = 0;
	manager->Device_Version = 0;
	manager->SVID_0 = 0;
	manager->SVID_1 = 0;
	manager->Standard_Vendor_ID = 0;
	manager->dp_is_connect = 0;
	manager->dp_hs_connect = 0;
	manager->is_sent_pin_configuration = 0;
	manager->pin_assignment = 0;
	manager->dp_selected_pin = 0;
	manager->hpd = 0;
	manager->hpdirq = 0;
	manager->flash_mode = 0;
	init_completion(&manager->uvdm_out_wait);
	init_completion(&manager->uvdm_in_wait);
	usbpd_manager_select_pdo_cancel(pd_data->dev);
	usbpd_manager_start_discover_msg_cancel(pd_data->dev);
}

int usbpd_init_manager(struct usbpd_data *pd_data)
{
	int ret = 0;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	if (manager == NULL) {
		pr_err("%s, usbpd manager data is error!!\n", __func__);
		return -ENOMEM;
	} else
		ret = of_usbpd_manager_dt(manager);

	INIT_DELAYED_WORK(&manager->acc_detach_handler, usbpd_manager_acc_detach_handler);
	INIT_DELAYED_WORK(&manager->select_pdo_handler, usbpd_manager_select_pdo_handler);
	INIT_DELAYED_WORK(&manager->start_discover_msg_handler, usbpd_manager_start_discover_msg_handler);

	fp_select_pdo = usbpd_manager_select_pdo;
#if defined(CONFIG_PDIC_PD30)
	fp_select_pps = usbpd_manager_select_pps;
	fp_pd_get_apdo_max_power = usbpd_manager_get_apdo_max_power;
	fp_pd_pps_enable = usbpd_manager_pps_enable;
	fp_pd_get_pps_voltage = usbpd_manager_get_pps_voltage;
#endif
	mutex_init(&manager->vdm_mutex);
	mutex_init(&manager->pdo_mutex);
	manager->pd_data = pd_data;
	manager->pd_attached = 0;
	manager->power_role_swap = true;
	manager->data_role_swap = true;
	manager->vconn_source_swap = false;
	manager->acc_type = 0;
    usbpd_init_manager_val(pd_data);

	usbpd_manager_register_switch_device(1);
	init_source_cap_data(manager);
	init_sink_cap_data(manager);

	pr_info("%s done\n", __func__);
	return ret;
}
