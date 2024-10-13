// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <usb20.h>
#include <musb_io.h>
#include <mtk_musb_reg.h>
#include <musb_core.h>

#ifdef CONFIG_USB_MTK_OTG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mtk_musb.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
#include <linux/cable_type_notifier.h>
#endif
#ifdef CONFIG_MTK_USB_TYPEC
#ifdef CONFIG_TCPC_CLASS
#include <tcpm.h>
#endif
#endif
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/phy/phy.h>

#include <mtk_musb.h>

#ifdef CONFIG_MTK_MUSB_PHY
#include <usb20_phy.h>
#endif

/*HS03s code for AL5626TDEV-170 by kangkai at 20220923 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
#define OTG_CURRENT_LIMIT 1200000
#endif
/*HS03s code for AL5626TDEV-170 by kangkai at 20220923 end*/
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#ifdef CONFIG_HQ_PROJECT_HS04
#define OTG_CURRENT_LIMIT 1200000
#endif
/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/

MODULE_LICENSE("GPL v2");

#include <mt-plat/mtk_boot_common.h>
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
static struct notifier_block otg_nb;
static struct tcpc_device *otg_tcpc_dev;
static struct delayed_work register_otg_work;
static unsigned long swch_d_time, swch_h_time;

#ifdef CONFIG_MTK_USB_TYPEC
#ifdef CONFIG_TCPC_CLASS
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data);
#define TCPC_OTG_DEV_NAME "type_c_port0"
static void do_register_otg_work(struct work_struct *data)
{
#define REGISTER_OTG_WORK_DELAY 500
	static int ret;

	if (!otg_tcpc_dev)
		otg_tcpc_dev = tcpc_dev_get_by_name(TCPC_OTG_DEV_NAME);

	if (!otg_tcpc_dev) {
		DBG(0, "get type_c_port0 fail\n");
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(otg_tcpc_dev, &otg_nb,
		TCP_NOTIFY_TYPE_VBUS | TCP_NOTIFY_TYPE_USB |
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		DBG(0, "register OTG <%p> fail\n", otg_tcpc_dev);
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	DBG(0, "register OTG <%p> ok\n", otg_tcpc_dev);
}
#endif
#endif
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
static struct notifier_block otg_nb;
static struct tcpc_device *otg_tcpc_dev;
static struct delayed_work register_otg_work;
static unsigned long swch_d_time, swch_h_time;

#ifdef CONFIG_MTK_USB_TYPEC
#ifdef CONFIG_TCPC_CLASS
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data);
#define TCPC_OTG_DEV_NAME "type_c_port0"
static void do_register_otg_work(struct work_struct *data)
{
#define REGISTER_OTG_WORK_DELAY 500
	static int ret;

	if (!otg_tcpc_dev)
		otg_tcpc_dev = tcpc_dev_get_by_name(TCPC_OTG_DEV_NAME);

	if (!otg_tcpc_dev) {
		DBG(0, "get type_c_port0 fail\n");
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	otg_nb.notifier_call = otg_tcp_notifier_call;
	ret = register_tcp_dev_notifier(otg_tcpc_dev, &otg_nb,
		TCP_NOTIFY_TYPE_VBUS | TCP_NOTIFY_TYPE_USB |
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		DBG(0, "register OTG <%p> fail\n", otg_tcpc_dev);
		queue_delayed_work(mtk_musb->st_wq, &register_otg_work,
				msecs_to_jiffies(REGISTER_OTG_WORK_DELAY));
		return;
	}

	DBG(0, "register OTG <%p> ok\n", otg_tcpc_dev);
}
#endif
#endif
#endif
void mt_usb_host_connect(int delay);
void mt_usb_host_disconnect(int delay);

/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
#include <charger_class.h>
#ifdef CONFIG_HQ_PROJECT_HS03S
static struct charger_device *primary_charger;
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
static struct charger_device *primary_charger;
#endif
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
#include <mt-plat/mtk_boot_common.h>

struct device_node	*usb_node;
static int		iddig_eint_num;
static ktime_t		ktime_start, ktime_end;
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/

#ifdef CONFIG_HQ_PROJECT_OT8
static struct		regulator *reg_vbus;
#endif
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/

static struct musb_fifo_cfg fifo_cfg_host[] = {
{ .hw_ep_num = 1, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 1, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 2, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 2, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 3, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 3, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 4, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 4, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 5, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 5, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 6, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 6, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 7, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 7, .style = FIFO_RX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 8, .style = FIFO_TX,
		.maxpacket = 512, .mode = BUF_SINGLE},
{ .hw_ep_num = 8, .style = FIFO_RX,
		.maxpacket = 64,  .mode = BUF_SINGLE},
};

u32 delay_time = 15;
module_param(delay_time, int, 0644);
u32 delay_time1 = 55;
module_param(delay_time1, int, 0644);
u32 iddig_cnt;
module_param(iddig_cnt, int, 0644);

static bool vbus_on;
module_param(vbus_on, bool, 0644);
static int vbus_control;
module_param(vbus_control, int, 0644);

/*HS03s for DEVAL5625-8 by wenyaqi at 20210425 start*/
void set_usb_eye_diagram(s32 u2_vrt_ref, s32 u2_term_ref, s32 u2_hstx_srctrl,
	s32 u2_enhance)
{
	if (u2_vrt_ref != -1) {
		if (u2_vrt_ref <= VAL_MAX_WIDTH_3) {
			USBPHY_CLR32(OFFSET_RG_USB20_VRT_VREF_SEL,
				VAL_MAX_WIDTH_3 << SHFT_RG_USB20_VRT_VREF_SEL);
			USBPHY_SET32(OFFSET_RG_USB20_VRT_VREF_SEL,
				u2_vrt_ref << SHFT_RG_USB20_VRT_VREF_SEL);
		}
	}
	if (u2_term_ref != -1) {
		if (u2_term_ref <= VAL_MAX_WIDTH_3) {
			USBPHY_CLR32(OFFSET_RG_USB20_TERM_VREF_SEL,
				VAL_MAX_WIDTH_3 << SHFT_RG_USB20_TERM_VREF_SEL);
			USBPHY_SET32(OFFSET_RG_USB20_TERM_VREF_SEL,
				u2_term_ref << SHFT_RG_USB20_TERM_VREF_SEL);
		}
	}
	if (u2_hstx_srctrl != -1) {
		if (u2_hstx_srctrl <= VAL_MAX_WIDTH_3) {
			USBPHY_CLR32(0x14, VAL_MAX_WIDTH_3 << 12);
			USBPHY_SET32(0x14, u2_hstx_srctrl << 12);
		}
	}
	if (u2_enhance != -1) {
		if (u2_enhance <= VAL_MAX_WIDTH_2) {
			USBPHY_CLR32(OFFSET_RG_USB20_PHY_REV6,
				VAL_MAX_WIDTH_2 << SHFT_RG_USB20_PHY_REV6);
			USBPHY_SET32(OFFSET_RG_USB20_PHY_REV6,
					u2_enhance<<SHFT_RG_USB20_PHY_REV6);
		}
	}
	/*HS03s for DEVAL5625-8 by wangzikang at 20210615 start*/
	pr_info("%s:u2_vrt_ref=0x%x, u2_term_ref=0x%x, u2_hstx_srctrl=0x%x,u2_enhance=0x%x\n",
			__func__,u2_vrt_ref,u2_term_ref,u2_hstx_srctrl,u2_enhance);
	/*HS03s for DEVAL5625-8 by wangzikang at 20210615 end*/
}

void set_usb_phy_mode(int mode)
{
	switch (mode) {
	case PHY_DEV_ACTIVE:
	/* VBUSVALID=1, AVALID=1, BVALID=1, SESSEND=0, IDDIG=1, IDPULLUP=1 */
		USBPHY_CLR32(0x6C, (0x10<<0));
		USBPHY_SET32(0x6C, (0x2F<<0));
		USBPHY_SET32(0x6C, (0x3F<<8));
		USBPHY_CLR32(0x14, 1 << 16);
		set_usb_eye_diagram(U2_VRT_REF, U2_TERM_REF, U2_HSTX_SRCTRL, U2_ENHANCE);
		break;
	case PHY_HOST_ACTIVE:
	/* VBUSVALID=1, AVALID=1, BVALID=1, SESSEND=0, IDDIG=0, IDPULLUP=1 */
		USBPHY_CLR32(0x6c, (0x12<<0));
		USBPHY_SET32(0x6c, (0x2d<<0));
		USBPHY_SET32(0x6c, (0x3f<<8));
		USBPHY_SET32(0x14, 1 << 16);
		set_usb_eye_diagram(HOST_U2_VRT_REF, HOST_U2_TERM_REF, HOST_U2_HSTX_SRCTRL,
			HOST_U2_ENHANCE);
		break;
	case PHY_IDLE_MODE:
	/* VBUSVALID=0, AVALID=0, BVALID=0, SESSEND=1, IDDIG=0, IDPULLUP=1 */
		USBPHY_SET32(0x6c, (0x11<<0));
		USBPHY_CLR32(0x6c, (0x2e<<0));
		USBPHY_SET32(0x6c, (0x3f<<8));
		USBPHY_CLR32(0x14, 1 << 16);
		set_usb_eye_diagram(U2_VRT_REF, U2_TERM_REF, U2_HSTX_SRCTRL, U2_ENHANCE);
		break;
	default:
		set_usb_eye_diagram(U2_VRT_REF, U2_TERM_REF, U2_HSTX_SRCTRL, U2_ENHANCE);
		DBG(0, "mode error %d\n", mode);
	}
	DBG(0, "force PHY to mode %d, 0x6c=%x\n", mode, USBPHY_READ32(0x6c));
}

#ifdef CONFIG_HQ_PROJECT_HS03S
static void _set_vbus(int is_on)
{
	/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
	/*   //delete
	if (!reg_vbus) {
		DBG(0, "vbus_init\n");
		reg_vbus = regulator_get(mtk_musb->controller, "usb-otg-vbus");
		if (IS_ERR_OR_NULL(reg_vbus)) {
			DBG(0, "failed to get vbus\n");
			return;
		}
	}
*/
	if (!primary_charger) {
		DBG(0, "vbus_init<%d>\n", vbus_on);

		primary_charger = get_charger_by_name("primary_chg");
		if (!primary_charger) {
			DBG(0, "get primary charger device failed\n");
			return;
		}
	}
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	DBG(0, "op<%d>, status<%d>\n", is_on, vbus_on);
	if (is_on && !vbus_on) {
		/* update flag 1st then enable VBUS to make
		 * host mode correct used by PMIC
		 */
		vbus_on = true;
	/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
		charger_dev_enable_otg(primary_charger, true);
		/*HS03s code for AL5626TDEV-170 by kangkai at 20220923 start*/
		charger_dev_set_boost_current_limit(primary_charger, OTG_CURRENT_LIMIT);
		/*HS03s code for AL5626TDEV-170 by kangkai at 20220923 end*/
		charger_dev_enable(primary_charger, false);
/*

		if (regulator_set_voltage(reg_vbus, 5000000, 5000000))
			DBG(0, "vbus regulator set voltage failed\n");

		if (regulator_set_current_limit(reg_vbus, 1500000, 1800000))
			DBG(0, "vbus regulator set current limit failed\n");

		if (regulator_enable(reg_vbus))
			DBG(0, "vbus regulator enable failed\n");
*/
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	} else if (!is_on && vbus_on) {
		/* disable VBUS 1st then update flag
		 * to make host mode correct used by PMIC
		 */
		vbus_on = false;
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
/*
		regulator_disable(reg_vbus);
*/
		charger_dev_enable_otg(primary_charger, false);
		charger_dev_enable(primary_charger, true);
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	}
}
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
static void _set_vbus(int is_on)
{
	/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
	/*   //delete
	if (!reg_vbus) {
		DBG(0, "vbus_init\n");
		reg_vbus = regulator_get(mtk_musb->controller, "usb-otg-vbus");
		if (IS_ERR_OR_NULL(reg_vbus)) {
			DBG(0, "failed to get vbus\n");
			return;
		}
	}
*/
	if (!primary_charger) {
		DBG(0, "vbus_init<%d>\n", vbus_on);

		primary_charger = get_charger_by_name("primary_chg");
		if (!primary_charger) {
			DBG(0, "get primary charger device failed\n");
			return;
		}
	}
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	DBG(0, "op<%d>, status<%d>\n", is_on, vbus_on);
	if (is_on && !vbus_on) {
		/* update flag 1st then enable VBUS to make
		 * host mode correct used by PMIC
		 */
		vbus_on = true;
	/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
		charger_dev_enable_otg(primary_charger, true);
		charger_dev_set_boost_current_limit(primary_charger, OTG_CURRENT_LIMIT);
		charger_dev_enable(primary_charger, false);
/*

		if (regulator_set_voltage(reg_vbus, 5000000, 5000000))
			DBG(0, "vbus regulator set voltage failed\n");

		if (regulator_set_current_limit(reg_vbus, 1500000, 1800000))
			DBG(0, "vbus regulator set current limit failed\n");

		if (regulator_enable(reg_vbus))
			DBG(0, "vbus regulator enable failed\n");
*/
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	} else if (!is_on && vbus_on) {
		/* disable VBUS 1st then update flag
		 * to make host mode correct used by PMIC
		 */
		vbus_on = false;
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 start*/
/*
		regulator_disable(reg_vbus);
*/
		charger_dev_enable_otg(primary_charger, false);
		charger_dev_enable(primary_charger, true);
/* HS03s for SR-AL5625-01-515 by wangzikang at 21210610 end*/
	}
}
#endif
#ifdef CONFIG_HQ_PROJECT_OT8
static void _set_vbus(int is_on)
{
        if (!reg_vbus) {
                DBG(1, "vbus_init\n");
                reg_vbus = regulator_get(mtk_musb->controller, "usb-otg-vbus");
                if (IS_ERR_OR_NULL(reg_vbus)) {
                        DBG(0, "failed to get vbus\n");
                        return;
                }
        }

        DBG(1, "op<%d>, status<%d>\n", is_on, vbus_on);
        if (is_on && !vbus_on) {
                /* update flag 1st then enable VBUS to make
                 * host mode correct used by PMIC
                 */
                vbus_on = true;

                if (regulator_set_voltage(reg_vbus, 5000000, 5000000))
                        DBG(0, "vbus regulator set voltage failed\n");

                if (regulator_set_current_limit(reg_vbus, 1500000, 1800000))
                        DBG(0, "vbus regulator set current limit failed\n");

                if (regulator_enable(reg_vbus))
                        DBG(0, "vbus regulator enable failed\n");

        } else if (!is_on && vbus_on) {
                /* disable VBUS 1st then update flag
                 * to make host mode correct used by PMIC
                 */
                vbus_on = false;
                regulator_disable(reg_vbus);
        }
}
#endif

void set_otg_vbus(bool val) {
	if(!val)
		_set_vbus(val);
}
EXPORT_SYMBOL(set_otg_vbus);

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
void mt_otg_accessory_power(int is_on)
{
	DBG(0, "is_on<%d>, control<%d>\n", is_on, vbus_control);

	if (!vbus_control)
		return;

	if (is_on)
		_set_vbus(1);
	else
		_set_vbus(0);
}
#endif

int mt_usb_get_vbus_status(struct musb *musb)
{
	return true;
#ifdef NEVER
	int	ret = 0;

	if ((musb_readb(musb->mregs, MUSB_DEVCTL) &
		MUSB_DEVCTL_VBUS) != MUSB_DEVCTL_VBUS)
		ret = 1;
	else
		DBG(0, "VBUS error, devctl=%x, power=%d\n",
			musb_readb(musb->mregs, MUSB_DEVCTL),
			musb->power);
	pr_debug("vbus ready = %d\n", ret);
	return ret;
#endif
}

#if defined(CONFIG_USBIF_COMPLIANCE)
u32 sw_deboun_time = 1;
#else
u32 sw_deboun_time = 400;
#endif
module_param(sw_deboun_time, int, 0644);

u32 typec_control;
module_param(typec_control, int, 0644);

static bool typec_req_host;
static bool iddig_req_host;
static int host_on_delay;
static int host_off_delay;

static void do_host_work(struct work_struct *data);
static void issue_host_work(int ops, int delay, bool on_st)
{
	struct mt_usb_work *work;

	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}

	/* create and prepare worker */
	work = kzalloc(sizeof(struct mt_usb_work), GFP_ATOMIC);
	if (!work) {
		DBG(0, "work is NULL, directly return\n");
		return;
	}
	work->ops = ops;
	INIT_DELAYED_WORK(&work->dwork, do_host_work);

	/* issue connection work */
	DBG(0, "issue work, ops<%d>, delay<%d>, on_st<%d>\n",
		ops, delay, on_st);

	if (on_st)
		queue_delayed_work(mtk_musb->st_wq,
					&work->dwork, msecs_to_jiffies(delay));
	else
		schedule_delayed_work(&work->dwork,
					msecs_to_jiffies(delay));
}

void mtk_usb_host_connect(void)
{
	typec_req_host = true;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	issue_host_work(CONNECTION_OPS_CONN, host_on_delay, true);
}
EXPORT_SYMBOL(mtk_usb_host_connect);

void mt_usb_host_connect(int delay)
{
#if defined(CONFIG_CABLE_TYPE_NOTIFIER) && !defined(CONFIG_EXTCON_MTK_USB)
	host_on_delay = delay;
	cable_type_notifier_set_attached_dev(CABLE_TYPE_OTG);
#else
	typec_req_host = true;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	issue_host_work(CONNECTION_OPS_CONN, delay, true);
#endif
}
EXPORT_SYMBOL(mt_usb_host_connect);

void mtk_usb_host_disconnect(void)
{
	typec_req_host = false;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	issue_host_work(CONNECTION_OPS_DISC, host_off_delay, true);
}
EXPORT_SYMBOL(mtk_usb_host_disconnect);

void mt_usb_host_disconnect(int delay)
{
#if defined(CONFIG_CABLE_TYPE_NOTIFIER) && !defined(CONFIG_EXTCON_MTK_USB)
	host_off_delay = delay;
	cable_type_notifier_set_attached_dev(CABLE_TYPE_NONE);
#else
	typec_req_host = false;
	DBG(0, "%s\n", typec_req_host ? "connect" : "disconnect");
	issue_host_work(CONNECTION_OPS_DISC, delay, true);
#endif
}
EXPORT_SYMBOL(mt_usb_host_disconnect);

/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
#ifdef CONFIG_MTK_USB_TYPEC
#ifdef CONFIG_TCPC_CLASS
static void do_vbus_work(struct work_struct *data)
{
	struct mt_usb_work *work =
		container_of(data, struct mt_usb_work, dwork.work);
	bool vbus_on = (work->ops ==
			VBUS_OPS_ON ? true : false);

	_set_vbus(vbus_on);
	/* free kfree */
	kfree(work);
}

static void issue_vbus_work(int ops, int delay)
{
	struct mt_usb_work *work;

	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* create and prepare worker */
	work = kzalloc(sizeof(struct mt_usb_work), GFP_ATOMIC);
	if (!work) {
		DBG(0, "work is NULL, directly return\n");
		return;
	}
	work->ops = ops;
	INIT_DELAYED_WORK(&work->dwork, do_vbus_work);

	/* issue vbus work */
	DBG(0, "issue work, ops<%d>, delay<%d>\n", ops, delay);

	queue_delayed_work(mtk_musb->st_wq,
				&work->dwork, msecs_to_jiffies(delay));
}

static void mt_usb_vbus_on(int delay)
{
	DBG(0, "vbus_on\n");
	issue_vbus_work(VBUS_OPS_ON, delay);
}

static void mt_usb_vbus_off(int delay)
{
	DBG(0, "vbus_off\n");
	issue_vbus_work(VBUS_OPS_OFF, delay);
}

/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 start*/
bool sink_to_src_flag = false;
bool src_to_sink_flag = false;
/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 end*/
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		DBG(0, "source vbus = %dmv\n", noti->vbus_state.mv);
		if (noti->vbus_state.mv)
			mt_usb_vbus_on(0);
		else
			mt_usb_vbus_off(0);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		DBG(0, "TCP_NOTIFY_TYPEC_STATE, old_state=%d, new_state=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state);
		/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 start*/
		if(unlikely(noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC)) {
			sink_to_src_flag = true;
			src_to_sink_flag = false;
		} else if(unlikely(noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK)) {
			sink_to_src_flag = false;
			src_to_sink_flag = true;
		} else {
			sink_to_src_flag = false;
			src_to_sink_flag = false;
		}
		/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 end*/
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			DBG(0, "OTG Plug in\n");
			mt_usb_host_connect(0);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state ==
					TYPEC_ATTACHED_NORP_SRC) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (is_host_active(mtk_musb)) {
				DBG(0, "OTG Plug out\n");
				mt_usb_host_disconnect(0);
			} else {
				DBG(0, "USB Plug out\n");
				mt_usb_dev_disconnect();
			}
#ifdef CONFIG_MTK_UART_USB_SWITCH
		} else if ((noti->typec_state.new_state ==
					TYPEC_ATTACHED_SNK ||
				noti->typec_state.new_state ==
					TYPEC_ATTACHED_CUSTOM_SRC ||
				noti->typec_state.new_state ==
					TYPEC_ATTACHED_NORP_SRC) &&
				in_uart_mode) {
			pr_info("%s USB cable plugged-in in UART mode.Switch to USB mode.\n", __func__);
			usb_phy_switch_to_usb();
#endif
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		DBG(0, "TCP_NOTIFY_DR_SWAP, new role=%d\n",
				noti->swap_state.new_role);
		if (is_host_active(mtk_musb) &&
			noti->swap_state.new_role == PD_ROLE_UFP) {
			DBG(0, "switch role to device\n");
			mt_usb_host_disconnect(0);
			swch_d_time = jiffies;
			if (!time_after(swch_d_time, swch_h_time + HZ)) {
				pr_info("%s delay for switch\n", __func__);
				msleep(100);
			}
			mt_usb_connect();
		} else if (is_peripheral_active(mtk_musb) &&
			noti->swap_state.new_role == PD_ROLE_DFP) {
			DBG(0, "switch role to host\n");
			mt_usb_dev_disconnect();
			swch_h_time = jiffies;
			if (!time_after(swch_h_time, swch_d_time + HZ)) {
				pr_info("%s delay for switch\n", __func__);
				msleep(100);
			}
			mt_usb_host_connect(0);
		}
		break;
	}
	return NOTIFY_OK;
}
#endif
#endif
#endif

#ifdef CONFIG_HQ_PROJECT_HS04
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
#ifdef CONFIG_MTK_USB_TYPEC
#ifdef CONFIG_TCPC_CLASS
static void do_vbus_work(struct work_struct *data)
{
	struct mt_usb_work *work =
		container_of(data, struct mt_usb_work, dwork.work);
	bool vbus_on = (work->ops ==
			VBUS_OPS_ON ? true : false);

	_set_vbus(vbus_on);
	/* free kfree */
	kfree(work);
}

static void issue_vbus_work(int ops, int delay)
{
	struct mt_usb_work *work;

	if (!mtk_musb) {
		DBG(0, "mtk_musb = NULL\n");
		return;
	}
	/* create and prepare worker */
	work = kzalloc(sizeof(struct mt_usb_work), GFP_ATOMIC);
	if (!work) {
		DBG(0, "work is NULL, directly return\n");
		return;
	}
	work->ops = ops;
	INIT_DELAYED_WORK(&work->dwork, do_vbus_work);

	/* issue vbus work */
	DBG(0, "issue work, ops<%d>, delay<%d>\n", ops, delay);

	queue_delayed_work(mtk_musb->st_wq,
				&work->dwork, msecs_to_jiffies(delay));
}

static void mt_usb_vbus_on(int delay)
{
	DBG(0, "vbus_on\n");
	issue_vbus_work(VBUS_OPS_ON, delay);
}

static void mt_usb_vbus_off(int delay)
{
	DBG(0, "vbus_off\n");
	issue_vbus_work(VBUS_OPS_OFF, delay);
}

/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 start*/
bool sink_to_src_flag = false;
bool src_to_sink_flag = false;
/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 end*/
static int otg_tcp_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		DBG(0, "source vbus = %dmv\n", noti->vbus_state.mv);
		if (noti->vbus_state.mv)
			mt_usb_vbus_on(0);
		else
			mt_usb_vbus_off(0);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		DBG(0, "TCP_NOTIFY_TYPEC_STATE, old_state=%d, new_state=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state);
		/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 start*/
		if(unlikely(noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC)) {
			sink_to_src_flag = true;
			src_to_sink_flag = false;
		} else if(unlikely(noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK)) {
			sink_to_src_flag = false;
			src_to_sink_flag = true;
		} else {
			sink_to_src_flag = false;
			src_to_sink_flag = false;
		}
		/*TabA7 Lite code for P210226-03264 by wenyaqi at 20210309 end*/
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			DBG(0, "OTG Plug in\n");
			mt_usb_host_connect(0);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state ==
					TYPEC_ATTACHED_NORP_SRC) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (is_host_active(mtk_musb)) {
				DBG(0, "OTG Plug out\n");
				mt_usb_host_disconnect(0);
			} else {
				DBG(0, "USB Plug out\n");
				mt_usb_dev_disconnect();
			}
#ifdef CONFIG_MTK_UART_USB_SWITCH
		} else if ((noti->typec_state.new_state ==
					TYPEC_ATTACHED_SNK ||
				noti->typec_state.new_state ==
					TYPEC_ATTACHED_CUSTOM_SRC ||
				noti->typec_state.new_state ==
					TYPEC_ATTACHED_NORP_SRC) &&
				in_uart_mode) {
			pr_info("%s USB cable plugged-in in UART mode.Switch to USB mode.\n", __func__);
			usb_phy_switch_to_usb();
#endif
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		DBG(0, "TCP_NOTIFY_DR_SWAP, new role=%d\n",
				noti->swap_state.new_role);
		if (is_host_active(mtk_musb) &&
			noti->swap_state.new_role == PD_ROLE_UFP) {
			DBG(0, "switch role to device\n");
			mt_usb_host_disconnect(0);
			swch_d_time = jiffies;
			if (!time_after(swch_d_time, swch_h_time + HZ)) {
				pr_info("%s delay for switch\n", __func__);
				msleep(100);
			}
			mt_usb_connect();
		} else if (is_peripheral_active(mtk_musb) &&
			noti->swap_state.new_role == PD_ROLE_DFP) {
			DBG(0, "switch role to host\n");
			mt_usb_dev_disconnect();
			swch_h_time = jiffies;
			if (!time_after(swch_h_time, swch_d_time + HZ)) {
				pr_info("%s delay for switch\n", __func__);
				msleep(100);
			}
			mt_usb_host_connect(0);
		}
		break;
	}
	return NOTIFY_OK;
}
#endif
#endif
#endif

bool musb_is_host(void)
{
	bool host_mode = 0;

	if (typec_control)
		host_mode = typec_req_host;
	else
		host_mode = iddig_req_host;

	return host_mode;
}

/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 start*/
bool ss_musb_is_host(void)
{
	bool host_mode = 0;

	if (typec_control)
		host_mode = typec_req_host;
	else
		host_mode = iddig_req_host;

	return host_mode;
}
/*HS03s for SR-AL5625-01-267 by wenyaqi at 20210427 end*/

void musb_session_restart(struct musb *musb)
{
	void __iomem	*mbase = musb->mregs;

	musb_writeb(mbase, MUSB_DEVCTL,
				(musb_readb(mbase,
				MUSB_DEVCTL) & (~MUSB_DEVCTL_SESSION)));
#ifdef CONFIG_MTK_MUSB_PHY
	DBG(0, "[MUSB] stopped session for VBUSERROR interrupt\n");
	USBPHY_SET32(0x6c, (0x3c<<8));
	USBPHY_SET32(0x6c, (0x10<<0));
	USBPHY_CLR32(0x6c, (0x2c<<0));
	DBG(0, "[MUSB] force PHY to idle, 0x6c=%x\n", USBPHY_READ32(0x6c));
	mdelay(5);
	USBPHY_CLR32(0x6c, (0x3c<<8));
	USBPHY_CLR32(0x6c, (0x3c<<0));
	DBG(0, "[MUSB] let PHY resample VBUS, 0x6c=%x\n"
				, USBPHY_READ32(0x6c));
#endif
	musb_writeb(mbase, MUSB_DEVCTL,
				(musb_readb(mbase,
				MUSB_DEVCTL) | MUSB_DEVCTL_SESSION));
	DBG(0, "[MUSB] restart session\n");
}
EXPORT_SYMBOL(musb_session_restart);

static struct delayed_work host_plug_test_work;
int host_plug_test_enable; /* default disable */
module_param(host_plug_test_enable, int, 0644);
int host_plug_in_test_period_ms = 5000;
module_param(host_plug_in_test_period_ms, int, 0644);
int host_plug_out_test_period_ms = 5000;
module_param(host_plug_out_test_period_ms, int, 0644);
int host_test_vbus_off_time_us = 3000;
module_param(host_test_vbus_off_time_us, int, 0644);
int host_test_vbus_only = 1;
module_param(host_test_vbus_only, int, 0644);
static int host_plug_test_triggered;
void switch_int_to_device(struct musb *musb)
{
	irq_set_irq_type(iddig_eint_num, IRQF_TRIGGER_HIGH);
	enable_irq(iddig_eint_num);
	DBG(0, "%s is done\n", __func__);
}

void switch_int_to_host(struct musb *musb)
{
	irq_set_irq_type(iddig_eint_num, IRQF_TRIGGER_LOW);
	enable_irq(iddig_eint_num);
	DBG(0, "%s is done\n", __func__);
}

static void do_host_plug_test_work(struct work_struct *data)
{
	static ktime_t ktime_begin, ktime_end;
	static s64 diff_time;
	static int host_on;
	static struct wakeup_source *host_test_wakelock;
	static int wake_lock_inited;

	if (!wake_lock_inited) {
		DBG(0, "wake_lock_init\n");
		host_test_wakelock = wakeup_source_register(NULL,
					"host.test.lock");
		wake_lock_inited = 1;
	}

	host_plug_test_triggered = 1;
	/* sync global status */
	mb();
	__pm_stay_awake(host_test_wakelock);
	DBG(0, "BEGIN");
	ktime_begin = ktime_get();

	host_on  = 1;
	while (1) {
		if (!musb_is_host() && host_on) {
			DBG(0, "about to exit");
			break;
		}
		msleep(50);

		ktime_end = ktime_get();
		diff_time = ktime_to_ms(ktime_sub(ktime_end, ktime_begin));
		if (host_on && diff_time >= host_plug_in_test_period_ms) {
			host_on = 0;
			DBG(0, "OFF\n");

			ktime_begin = ktime_get();

			/* simulate plug out */
			_set_vbus(0);
			udelay(host_test_vbus_off_time_us);

			if (!host_test_vbus_only)
				issue_host_work(CONNECTION_OPS_DISC, 0, false);
		} else if (!host_on && diff_time >=
					host_plug_out_test_period_ms) {
			host_on = 1;
			DBG(0, "ON\n");

			ktime_begin = ktime_get();
			if (!host_test_vbus_only)
				issue_host_work(CONNECTION_OPS_CONN, 0, false);

			_set_vbus(1);
			msleep(100);

		}
	}

	/* wait host_work done */
	msleep(1000);
	host_plug_test_triggered = 0;
	__pm_relax(host_test_wakelock);
	DBG(0, "END\n");
}

#define ID_PIN_WORK_RECHECK_TIME 30	/* 30 ms */
#define ID_PIN_WORK_BLOCK_TIMEOUT 30000 /* 30000 ms */
static void do_host_work(struct work_struct *data)
{
	u8 devctl = 0;
	unsigned long flags;
	static int inited, timeout; /* default to 0 */
	static s64 diff_time;
	bool host_on;
	int usb_clk_state = NO_CHANGE;
	struct mt_usb_work *work =
		container_of(data, struct mt_usb_work, dwork.work);
#ifdef CONFIG_PHY_MTK_TPHY
	struct mt_usb_glue *glue = mtk_musb->glue;
#endif
	/*
	 * kernel_init_done should be set in
	 * early-init stage through init.$platform.usb.rc
	 */
	while (!inited && !kernel_init_done &&
		   !mtk_musb->is_ready && !timeout) {
		ktime_end = ktime_get();
		diff_time = ktime_to_ms(ktime_sub(ktime_end, ktime_start));

		DBG_LIMIT(3,
			"init_done:%d, is_ready:%d, inited:%d, TO:%d, diff:%lld",
			kernel_init_done,
			mtk_musb->is_ready,
			inited,
			timeout,
			diff_time);

		if (diff_time > ID_PIN_WORK_BLOCK_TIMEOUT) {
			DBG(0, "diff_time:%lld\n", diff_time);
			timeout = 1;
		}
		msleep(ID_PIN_WORK_RECHECK_TIME);
	}

	if (!inited) {
		DBG(0, "PASS,init_done:%d,is_ready:%d,inited:%d, TO:%d\n",
				kernel_init_done,  mtk_musb->is_ready,
				inited, timeout);
		inited = 1;
	}

	/* always prepare clock and check if need to unprepater later */
	/* clk_prepare_cnt +1 here */
	usb_prepare_clock(true);

	down(&mtk_musb->musb_lock);

	host_on = (work->ops ==
			CONNECTION_OPS_CONN ? true : false);

	DBG(0, "work start, is_host=%d, host_on=%d\n",
		mtk_musb->is_host, host_on);

	if (host_on && !mtk_musb->is_host) {
		/* switch to HOST state before turn on VBUS */
		MUSB_HST_MODE(mtk_musb);

		/* to make sure all event clear */
		msleep(32);
#ifdef CONFIG_MTK_UAC_POWER_SAVING
		if (!usb_on_sram) {
			int ret;

			ret = gpd_switch_to_sram(mtk_musb->controller);
			DBG(0, "gpd_switch_to_sram, ret<%d>\n", ret);
			if (ret == 0)
				usb_on_sram = 1;
		}
#endif
		/* setup fifo for host mode */
		ep_config_from_table_for_host(mtk_musb);

		__pm_stay_awake(mtk_musb->usb_lock);

		/* this make PHY operation workable */
		musb_platform_enable(mtk_musb);

		/* for no VBUS sensing IP*/

		/* wait VBUS ready */
		msleep(100);
		/* clear session*/
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs,
				MUSB_DEVCTL, (devctl&(~MUSB_DEVCTL_SESSION)));
		set_usb_phy_mode(PHY_IDLE_MODE);
#ifdef CONFIG_PHY_MTK_TPHY
		phy_set_mode(glue->phy, PHY_MODE_INVALID);
#endif
		/* wait */
		mdelay(5);
		/* restart session */
		devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
		musb_writeb(mtk_musb->mregs,
				MUSB_DEVCTL, (devctl | MUSB_DEVCTL_SESSION));
		set_usb_phy_mode(PHY_HOST_ACTIVE);
#ifdef CONFIG_PHY_MTK_TPHY
		phy_set_mode(glue->phy, PHY_MODE_USB_HOST);
#endif
		musb_start(mtk_musb);

		if (!typec_control && !host_plug_test_triggered)
			switch_int_to_device(mtk_musb);

		if (host_plug_test_enable && !host_plug_test_triggered)
			queue_delayed_work(mtk_musb->st_wq,
						&host_plug_test_work, 0);
		usb_clk_state = OFF_TO_ON;
	}  else if (!host_on && mtk_musb->is_host) {
		/* switch from host -> device */
		/* for device no disconnect interrupt */
		spin_lock_irqsave(&mtk_musb->lock, flags);
		if (mtk_musb->is_active) {
			DBG(0, "for not receiving disconnect interrupt\n");
			usb_hcd_resume_root_hub(musb_to_hcd(mtk_musb));
			musb_root_disconnect(mtk_musb);
		}
		spin_unlock_irqrestore(&mtk_musb->lock, flags);

		DBG(1, "devctl is %x\n",
				musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		if (mtk_musb->usb_lock->active)
			__pm_relax(mtk_musb->usb_lock);

		/* for no VBUS sensing IP */
		set_usb_phy_mode(PHY_IDLE_MODE);
#ifdef CONFIG_PHY_MTK_TPHY
		/* for no VBUS sensing IP */
		phy_set_mode(glue->phy, PHY_MODE_INVALID);
#endif
		musb_stop(mtk_musb);

		if (!typec_control && !host_plug_test_triggered)
			switch_int_to_host(mtk_musb);

#ifdef CONFIG_MTK_UAC_POWER_SAVING
		if (usb_on_sram) {
			gpd_switch_to_dram(mtk_musb->controller);
			usb_on_sram = 0;
		}
#endif
		/* to make sure all event clear */
		msleep(32);

		mtk_musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		/* switch to DEV state after turn off VBUS */
		MUSB_DEV_MODE(mtk_musb);

		usb_clk_state = ON_TO_OFF;
	}
	DBG(0, "work end, is_host=%d\n", mtk_musb->is_host);
	up(&mtk_musb->musb_lock);

	if (usb_clk_state == ON_TO_OFF) {
		/* clock on -> of: clk_prepare_cnt -2 */
		usb_prepare_clock(false);
		usb_prepare_clock(false);
	} else if (usb_clk_state == NO_CHANGE) {
		/* clock no change : clk_prepare_cnt -1 */
		usb_prepare_clock(false);
	}
	/* free mt_usb_work */
	kfree(work);
}

static irqreturn_t mt_usb_ext_iddig_int(int irq, void *dev_id)
{
	iddig_cnt++;

	iddig_req_host = !iddig_req_host;
	DBG(0, "id pin assert, %s\n", iddig_req_host ?
			"connect" : "disconnect");

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
	disable_irq_nosync(iddig_eint_num);
	if (iddig_req_host)
		cable_type_notifier_set_attached_dev(CABLE_TYPE_OTG);
	else
		cable_type_notifier_set_attached_dev(CABLE_TYPE_NONE);
#else
	if (iddig_req_host)
		mt_usb_host_connect(0);
	else
		mt_usb_host_disconnect(0);
	disable_irq_nosync(iddig_eint_num);
#endif
	return IRQ_HANDLED;
}

static const struct of_device_id otg_iddig_of_match[] = {
	{.compatible = "mediatek,usb_iddig_bi_eint"},
	{},
};

static int otg_iddig_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	iddig_eint_num = irq_of_parse_and_map(node, 0);
	DBG(0, "iddig_eint_num<%d>\n", iddig_eint_num);
	if (iddig_eint_num < 0)
		return -ENODEV;

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
	ret = request_threaded_irq(iddig_eint_num, NULL, mt_usb_ext_iddig_int,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT, "USB_IDDIG", NULL);
#else
	ret = request_irq(iddig_eint_num, mt_usb_ext_iddig_int,
					IRQF_TRIGGER_LOW, "USB_IDDIG", NULL);
#endif
	if (ret) {
		DBG(0,
			"request EINT <%d> fail, ret<%d>\n",
			iddig_eint_num, ret);
		return ret;
	}

	return 0;
}

static struct platform_driver otg_iddig_driver = {
	.probe = otg_iddig_probe,
	/* .remove = otg_iddig_remove, */
	/* .shutdown = otg_iddig_shutdown, */
	.driver = {
		.name = "otg_iddig",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(otg_iddig_of_match),
	},
};


static int iddig_int_init(void)
{
	int	ret = 0;

	ret = platform_driver_register(&otg_iddig_driver);
	if (ret)
		DBG(0, "ret:%d\n", ret);

	return 0;
}

void mt_usb_otg_init(struct musb *musb)
{
	/* test */
	INIT_DELAYED_WORK(&host_plug_test_work, do_host_plug_test_work);
	ktime_start = ktime_get();

	/* CONNECTION MANAGEMENT*/
#ifdef CONFIG_MTK_USB_TYPEC
	DBG(0, "host controlled by TYPEC\n");
	typec_control = 1;
#ifdef CONFIG_TCPC_CLASS
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#ifdef CONFIG_HQ_PROJECT_HS03S
	INIT_DELAYED_WORK(&register_otg_work, do_register_otg_work);
	queue_delayed_work(mtk_musb->st_wq, &register_otg_work, 0);
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	INIT_DELAYED_WORK(&register_otg_work, do_register_otg_work);
	queue_delayed_work(mtk_musb->st_wq, &register_otg_work, 0);
#endif
	vbus_control = 0;
#endif /* CONFIG_TCPC_CLASS */
#else
	DBG(0, "host controlled by IDDIG\n");
	iddig_int_init();
#ifdef CONFIG_HQ_PROJECT_HS03S
	vbus_control = 1;
#endif
#ifdef CONFIG_HQ_PROJECT_HS04
	vbus_control = 1;
#endif
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
#endif /* CONFIG_MTK_USB_TYPEC */

	/* EP table */
	musb->fifo_cfg_host = fifo_cfg_host;
	musb->fifo_cfg_host_size = ARRAY_SIZE(fifo_cfg_host);

}
EXPORT_SYMBOL(mt_usb_otg_init);

void mt_usb_otg_exit(struct musb *musb)
{
	DBG(0, "OTG disable vbus\n");
}
EXPORT_SYMBOL(mt_usb_otg_exit);

enum {
	DO_IT = 0,
	REVERT,
};

#ifdef CONFIG_MTK_MUSB_PHY
static void bypass_disc_circuit(int act)
{
	u32 val;

	usb_prepare_enable_clock(true);

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	/* 0x18, 13-12 RG_USB20_HSRX_MMODE_SELE, dft:00 */
	if (act == DO_IT) {
		USBPHY_CLR32(0x18, (0x10<<8));
		USBPHY_SET32(0x18, (0x20<<8));
	} else {
		USBPHY_CLR32(0x18, (0x10<<8));
		USBPHY_CLR32(0x18, (0x20<<8));
	}
	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	usb_prepare_enable_clock(false);
}

static void disc_threshold_to_max(int act)
{
	u32 val;

	usb_prepare_enable_clock(true);

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	/* 0x18, 7-4 RG_USB20_DISCTH, dft:1000 */
	if (act == DO_IT) {
		USBPHY_SET32(0x18, (0xf0<<0));
	} else {
		USBPHY_CLR32(0x18, (0x70<<0));
		USBPHY_SET32(0x18, (0x80<<0));
	}

	val = USBPHY_READ32(0x18);
	DBG(0, "val<0x%x>\n", val);

	usb_prepare_enable_clock(false);
}
#endif

static int option;
static int set_option(const char *val, const struct kernel_param *kp)
{
	int local_option;
	int rv;

	/* update module parameter */
	rv = param_set_int(val, kp);
	if (rv)
		return rv;

	/* update local_option */
	rv = kstrtoint(val, 10, &local_option);
	if (rv != 0)
		return rv;

	DBG(0, "option:%d, local_option:%d\n", option, local_option);

	switch (local_option) {
	case 0:
		DBG(0, "case %d\n", local_option);
		iddig_int_init();
		break;
	case 1:
		DBG(0, "case %d\n", local_option);
		mt_usb_host_connect(0);
		break;
	case 2:
		DBG(0, "case %d\n", local_option);
		mt_usb_host_disconnect(0);
		break;
	case 3:
		DBG(0, "case %d\n", local_option);
		mt_usb_host_connect(3000);
		break;
	case 4:
		DBG(0, "case %d\n", local_option);
		mt_usb_host_disconnect(3000);
		break;
#ifdef CONFIG_MTK_MUSB_PHY
	case 5:
		DBG(0, "case %d\n", local_option);
		disc_threshold_to_max(DO_IT);
		break;
	case 6:
		DBG(0, "case %d\n", local_option);
		disc_threshold_to_max(REVERT);
		break;
	case 7:
		DBG(0, "case %d\n", local_option);
		bypass_disc_circuit(DO_IT);
		break;
	case 8:
		DBG(0, "case %d\n", local_option);
		bypass_disc_circuit(REVERT);
		break;
#endif
	case 9:
		DBG(0, "case %d\n", local_option);
		_set_vbus(1);
		break;
	case 10:
		DBG(0, "case %d\n", local_option);
		_set_vbus(0);
		break;
	default:
		break;
	}
	return 0;
}
static struct kernel_param_ops option_param_ops = {
	.set = set_option,
	.get = param_get_int,
};
module_param_cb(option, &option_param_ops, &option, 0644);
#else
#include "musb_core.h"
/* for not define CONFIG_USB_MTK_OTG */
void mt_usb_otg_init(struct musb *musb) {}
EXPORT_SYMBOL(mt_usb_otg_init);

void mt_usb_otg_exit(struct musb *musb) {}
EXPORT_SYMBOL(mt_usb_otg_exit);

void mt_usb_set_vbus(struct musb *musb, int is_on) {}
int mt_usb_get_vbus_status(struct musb *musb) {return 1; }
void switch_int_to_device(struct musb *musb) {}
void switch_int_to_host(struct musb *musb) {}

void musb_session_restart(struct musb *musb) {}
EXPORT_SYMBOL(musb_session_restart);
#endif
