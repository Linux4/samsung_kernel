/**
 * dwc3-sprd.c - Spreadtrum DWC3 Specific Glue layer
 *
 * Copyright (c) 2015 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Miao Zhu <miao.zhu@spreadtrum.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/usb/otg.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/sprd_battery_common.h>
#include <linux/mfd/typec.h>
#include <soc/sprd/usb.h>
#ifdef CONFIG_SC_FPGA
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#endif

#include "core.h"
#include "gadget.h"
#include "io.h"

#include "debug.h"

#define DWC3_GLOBALS_REGS_BASE (sdwc->base - DWC3_GLOBALS_REGS_START)

static u64 dma_mask = DMA_BIT_MASK(64);

struct dwc3_sprd {
	struct device	*dev;
	void __iomem	*base;
	struct platform_device	*dwc3;
	int			irq;
	struct clk	*core_clk;
	struct clk	*ref_clk;
	struct clk	*susp_clk;
	struct usb_phy		*hs_phy, *ss_phy;

	struct delayed_work	hotplug_work;
	struct delayed_work	vbus_work;
	struct delayed_work	usbid_work;

	struct work_struct	restart_usb_work;
	struct work_struct	reset_usb_block_work;
	struct work_struct	resume_usb_work;

	struct notifier_block hot_plug_nb;

	bool		hibernate_en;
	bool		pwr_collapse;
	bool		pwr_collapse_por;
	enum usb_dr_mode dr_mode;
	uint32_t	ref_clk_rate, susp_clk_rate;

	int			gpio_vbus;
	int			gpio_usb_id;
	int			gpio_boost_supply;
	int			vbus_irq;

	struct wake_lock	wake_lock;
	spinlock_t 	lock;

	bool		in_p3;
	bool		in_hiber;
	bool		bus_active;
	bool		vbus_active;
	bool		block_active;
};

static BLOCKING_NOTIFIER_HEAD(usb_chain_head);

int __weak register_typec_notifier(struct notifier_block *nb)
{return 0;}
int __weak unregister_typec_notifier(struct notifier_block *nb)
{return 0;}
__weak void xhci_set_notifier(void (*notify)(struct usb_hcd *, unsigned))
{ }

int register_usb_notifier(struct notifier_block *nb)
{
	pr_info("%s %p\n", __func__, nb);

	return blocking_notifier_chain_register(&usb_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_usb_notifier);

int unregister_usb_notifier(struct notifier_block *nb)
{
	pr_info("%s %p\n", __func__, nb);

	return blocking_notifier_chain_unregister(&usb_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_usb_notifier);

static int usb_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&usb_chain_head, val, NULL)
		== NOTIFY_BAD) ? -EINVAL : 0;
}

static void dwc3_flush_all_events(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	int i;
	unsigned long flags;

	/* Disable all events on cable disconnect */
	dbg_event(0xFF, "Dis EVT", 0);
	dwc3_gadget_disable_irq(dwc);

	/* Skip remaining events on disconnect */
	spin_lock_irqsave(&dwc->lock, flags);
	for (i = 0; i < dwc->num_event_buffers; i++) {
		struct dwc3_event_buffer *evt;
		evt = dwc->ev_buffs[i];
		evt->lpos = (evt->lpos + evt->count) %
			DWC3_EVENT_BUFFERS_SIZE;
		evt->count = 0;
		evt->flags &= ~DWC3_EVENT_PENDING;
	}
	spin_unlock_irqrestore(&dwc->lock, flags);

	/*
	 * If there is a pending block reset due to erratic event,
	 * wait for it to complete
	 */
	if (dwc->err_evt_seen) {
		dbg_event(0xFF, "Flush BR", 0);
		flush_work(&sdwc->reset_usb_block_work);
		dwc->err_evt_seen = 0;
	}
}

static int dwc3_sprd_start(struct dwc3_sprd *sdwc, uint32_t mode)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	struct usb_hcd *hcd;
	uint32_t core_mode;
	int flag, ret;

	/*
	if (sdwc->in_p3 || sdwc->in_hiber) {
		queue_work(system_nrt_wq, &sdwc->resume_usb_work);
		return 0;
	}
	 */
	core_mode = mode == USB_DR_MODE_HOST ?
		DWC3_GCTL_PRTCAP_HOST : DWC3_GCTL_PRTCAP_DEVICE;
#ifdef CONFIG_SC_FPGA
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_AP_AHB_USB3_EB |
		BIT_AP_AHB_USB3_SUSPEND_EB | BIT_AP_AHB_USB3_REF_EB);
#else
	clk_prepare_enable(sdwc->core_clk);
	clk_prepare_enable(sdwc->ref_clk);

	if (sdwc->pwr_collapse_por) {
		clk_prepare_enable(sdwc->susp_clk);
		usb_phy_init(sdwc->hs_phy);
		usb_phy_init(sdwc->ss_phy);
	}
#endif
	sdwc->bus_active = 1;

	dwc3_set_mode(dwc, core_mode);

	ret = dwc3_core_init(dwc);
	if (ret) {
		dev_err(sdwc->dev, "fail to init usb core\n");
		return ret;
	}

	dwc3_notify_event(dwc, DWC3_CONTROLLER_POST_INITIALIZATION_EVENT);

	if (mode == USB_DR_MODE_PERIPHERAL) {
		ret = usb_gadget_connect(&dwc->gadget);
		if (ret) {
			dev_err(sdwc->dev, "fail to pull up the peripheral core\n");
			return ret;
		}
	} else {
		if (!dwc->xhci) {
			dev_err(sdwc->dev, "The Host core is not initialized\n");
			return 0;
		}
		hcd = dev_get_drvdata(&dwc->xhci->dev);
		if (!hcd) {
			dev_err(sdwc->dev, "The Host core is not initialized\n");
			return 0;
		}
		do {
			flag = usb_hcd_is_primary_hcd(hcd);
			if (hcd->driver && hcd->driver->start) {
				dev_info(hcd->self.controller, "start %s hcd\n",
					flag ? "primary" : "shared");
				ret = hcd->driver->start(hcd);
				if (ret) {
					dev_err(sdwc->dev, "fail to run the host hcd 0x%p\n", hcd);
					return ret;
				}
			}
			if (!flag)
				break;
		} while ((hcd = hcd->shared_hcd) != NULL);
	}
	return 0;
}

static int dwc3_sprd_stop(struct dwc3_sprd *sdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	struct usb_hcd *hcd;

	dwc3_flush_all_events(sdwc);

	if (sdwc->dr_mode == USB_DR_MODE_PERIPHERAL)
		usb_gadget_disconnect(&dwc->gadget);
	else if (sdwc->dr_mode == USB_DR_MODE_HOST) {
		if (!dwc->xhci) {
			dev_err(sdwc->dev, "The Host core is not initialized\n");
			return 0;
		}
		hcd = dev_get_drvdata(&dwc->xhci->dev);
		if (!hcd) {
			dev_err(sdwc->dev, "The Host core is not initialized\n");
			return 0;
		}

		if (hcd->driver && hcd->driver->stop) {
			hcd->driver->stop(hcd);
			dev_info(hcd->self.controller, "stop hcd\n");
		}
	}

#ifndef CONFIG_SC_FPGA
	clk_disable_unprepare(sdwc->core_clk);
	clk_disable_unprepare(sdwc->ref_clk);

	if (sdwc->pwr_collapse) {
		clk_disable_unprepare(sdwc->susp_clk);
		usb_phy_shutdown(sdwc->hs_phy);
		usb_phy_shutdown(sdwc->ss_phy);
	}
#endif
	sdwc->bus_active = 0;
	return 0;
}

/* Detect which UE connects with, a HOST or an adapter
 * True if it's HOST.
 * False if it's adapter.
 */
static bool dwc3_connect_with_host(void)
{
	int adp_type;

#ifdef CONFIG_SC_FPGA
	return 1;
#else
	adp_type = sprdchg_charger_is_adapter();
	return (adp_type == ADP_TYPE_CDP || adp_type == ADP_TYPE_SDP) ?
		1 : 0;
#endif
}

static int dwc3_typec_hotplug(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct dwc3_sprd *sdwc = container_of(nb, struct dwc3_sprd, hot_plug_nb);
	unsigned long flags;
	int ret = 0;

	switch(action) {

	case TYPEC_STATUS_DEV_CONN:
		if (dwc3_connect_with_host()) {
			spin_lock_irqsave(&sdwc->lock, flags);
			sdwc->vbus_active = 1;
			sdwc->dr_mode = USB_DR_MODE_PERIPHERAL;
			spin_unlock_irqrestore(&sdwc->lock, flags);

			queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
		}

		usb_notifier_call_chain(USB_CABLE_PLUG_IN);
		break;

	case TYPEC_STATUS_HOST_CONN:
		spin_lock_irqsave(&sdwc->lock, flags);
		sdwc->vbus_active = 1;
		sdwc->dr_mode = USB_DR_MODE_HOST;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
		break;

	case TYPEC_STATUS_DEV_DISCONN:
		spin_lock_irqsave(&sdwc->lock, flags);
		sdwc->vbus_active = 0;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
		usb_notifier_call_chain(USB_CABLE_PLUG_OUT);
		break;

	case TYPEC_STATUS_HOST_DISCONN:
		spin_lock_irqsave(&sdwc->lock, flags);
		sdwc->vbus_active = 0;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
		break;

	default:
		/* Unknown Actions */
		dev_err(sdwc->dev, "Unknown action %ld from Type-C\n", action);
		ret= -EINVAL;
		break;
	}

	return ret;
}

static void dwc3_restart_usb_work(struct work_struct *w)
{
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						restart_usb_work);
	uint32_t mode;

	dev_info(sdwc->dev, "%s\n", __func__);

	spin_lock(&sdwc->lock);
	if (!sdwc->vbus_active) {
		spin_unlock(&sdwc->lock);
		dev_err(sdwc->dev,
			"%s failed! the cable is not connected\n", __func__);
		return;
	}
	spin_unlock(&sdwc->lock);

	dbg_event(0xFF, "RestartUSB", 0);
	mode = sdwc->dr_mode;
	dwc3_sprd_stop(sdwc);

	/* Add a delay */
	usleep_range(300000,500000);

	if (dwc3_sprd_start(sdwc, mode))
		dev_err(sdwc->dev, "%s failed\n", __func__);
}

/**
 * Reset USB connection
 * This performs full hardware reset and re-initialization
 */
void dwc3_restart_usb_session(struct usb_gadget *gadget)
{
	struct dwc3 *dwc = container_of(gadget, struct dwc3, gadget);
	struct dwc3_sprd *sdwc = dev_get_drvdata(dwc->dev->parent);

	if (!sdwc)
		return;

	dev_info(sdwc->dev, "%s\n", __func__);

	queue_work(system_nrt_wq, &sdwc->restart_usb_work);
}
EXPORT_SYMBOL(dwc3_restart_usb_session);

static ssize_t restart_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dwc3_sprd	*sdwc = dev_get_drvdata(dev);
	uint32_t			restart;

	if (!sdwc)
		return -EINVAL;

	sscanf(buf, "%d", &restart);
	if (restart && sdwc->vbus_active)
		queue_work(system_nrt_wq, &sdwc->restart_usb_work);

	return size;
}
static DEVICE_ATTR(restart, S_IWUSR,
	NULL, restart_store);

static ssize_t maximum_speed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dwc3_sprd	*sdwc = dev_get_drvdata(dev);
	struct dwc3			*dwc;

	if (!sdwc)
		return -EINVAL;
	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;

	return sprintf(buf, "%s\n", usb_speed_string(dwc->maximum_speed));
}

static ssize_t maximum_speed_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dwc3_sprd	*sdwc = dev_get_drvdata(dev);
	struct dwc3			*dwc;
	uint32_t			max_speed;

	if (!sdwc)
		return -EINVAL;

	sscanf(buf, "%d", &max_speed);

	if (max_speed <= USB_SPEED_UNKNOWN ||
		max_speed > USB_SPEED_SUPER)
		return -EINVAL;

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;
	dwc->maximum_speed = max_speed;

	return size;
}
static DEVICE_ATTR(maximum_speed, S_IRUGO | S_IWUSR,
	maximum_speed_show, maximum_speed_store);

static ssize_t current_speed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dwc3_sprd	*sdwc = dev_get_drvdata(dev);
	struct dwc3			*dwc;

	if (!sdwc)
		return -EINVAL;

	dwc = platform_get_drvdata(sdwc->dwc3);
	if (!dwc)
		return -EINVAL;

	return sprintf(buf, "%s\n", usb_speed_string(dwc->gadget.speed));
}
static DEVICE_ATTR(current_speed, S_IRUGO,
	current_speed_show, NULL);


static int dwc3_sprd_suspend(struct dwc3_sprd *sdwc)
{
	return 0;
}

static int dwc3_sprd_resume(struct dwc3_sprd *sdwc)
{
	return 0;
}

static void dwc3_sprd_update_clks(struct dwc3_sprd *sdwc)
{
	uint32_t guctl, gfladj, gctl;

	guctl = readl(DWC3_GLOBALS_REGS_BASE + DWC3_GUCTL);
	gctl = readl(DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);
	gctl &= ~DWC3_GCTL_PWRDNSCALEMASK;
	guctl &= ~DWC3_GUCTL_REFCLKPER;

	/* GFLADJ register is used starting with revision 2.50a */
	if (readl(DWC3_GLOBALS_REGS_BASE + DWC3_GSNPSID)
		>= DWC3_REVISION_250A) {
		gfladj = readl(DWC3_GLOBALS_REGS_BASE + DWC3_GFLADJ);
		gfladj &= ~DWC3_GFLADJ_REFCLK_240MHZDECR_PLS1;
		gfladj &= ~DWC3_GFLADJ_REFCLK_240MHZ_DECR;
		gfladj &= ~DWC3_GFLADJ_REFCLK_LPM_SEL;
		gfladj &= ~DWC3_GFLADJ_REFCLK_FLADJ;
	}

	/* Refer to SNPS Databook Table 6-18 for calculations used */
	switch (sdwc->ref_clk_rate) {
	case 19200000:
		guctl |= 52 << __ffs(DWC3_GUCTL_REFCLKPER);
		gfladj |= 12 << __ffs(DWC3_GFLADJ_REFCLK_240MHZ_DECR);
		gfladj |= DWC3_GFLADJ_REFCLK_240MHZDECR_PLS1;
		gfladj |= DWC3_GFLADJ_REFCLK_LPM_SEL;
		gfladj |= 200 << __ffs(DWC3_GFLADJ_REFCLK_FLADJ);
		break;
	case 24000000:
		guctl |= 41 << __ffs(DWC3_GUCTL_REFCLKPER);
		gfladj |= 10 << __ffs(DWC3_GFLADJ_REFCLK_240MHZ_DECR);
		gfladj |= DWC3_GFLADJ_REFCLK_LPM_SEL;
		gfladj |= 2032 << __ffs(DWC3_GFLADJ_REFCLK_FLADJ);
		break;
	default:
		dev_warn(sdwc->dev, "Unsupported ref_clk_rate: %u\n",
				sdwc->ref_clk_rate);
		break;
	}

	switch (sdwc->susp_clk_rate) {
	case 32000:
		gctl |= DWC3_GCTL_PWRDNSCALE(2);
		break;
	case 1000000:
		gctl |= DWC3_GCTL_PWRDNSCALE(63);
		break;
	default:
		dev_warn(sdwc->dev, "Unsupported suspend_clk_rate: %u\n",
				sdwc->susp_clk_rate);
		break;
	}

	writel(guctl, DWC3_GLOBALS_REGS_BASE + DWC3_GUCTL);

	writel(gctl, DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);

	if (gfladj)
		writel(gfladj, DWC3_GLOBALS_REGS_BASE + DWC3_GFLADJ);
}

static void dwc3_sprd_notify_event(struct dwc3* dwc, unsigned event)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dwc->dev->parent);
	u32 reg;

	if (dwc->revision < DWC3_REVISION_230A)
		return;

	switch (event) {
	case DWC3_CONTROLLER_ERROR_EVENT:
		dev_info(sdwc->dev,
			"DWC3_CONTROLLER_ERROR_EVENT received, irq cnt %lu\n",
			dwc->irq_cnt);

		dwc3_gadget_disable_irq(dwc);

		/* prevent core from generating interrupts until recovery */
		reg = readl(DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);
		reg |= DWC3_GCTL_CORESOFTRESET;
		writel(reg, DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);

		/*
		 * schedule work for doing block reset for recovery from erratic
		 * error event.
		 */
		queue_work(system_nrt_wq, &sdwc->reset_usb_block_work);
		break;
	case DWC3_CONTROLLER_RESET_EVENT:
		dev_info(sdwc->dev, "DWC3_CONTROLLER_RESET_EVENT received\n");
		break;
	case DWC3_CONTROLLER_POST_RESET_EVENT:
		dev_info(sdwc->dev,
				"DWC3_CONTROLLER_POST_RESET_EVENT received\n");

		dwc3_sprd_update_clks(sdwc);
		break;
	case DWC3_CONTROLLER_POST_INITIALIZATION_EVENT:
		/*
		 * Workaround: Disable internal clock gating always, as there
		 * is a known HW bug that causes the internal RAM clock to get
		 * stuck when entering low power modes.
		 */
		reg = readl(DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);
		reg |= DWC3_GCTL_DSBLCLKGTNG;
		writel(reg, DWC3_GLOBALS_REGS_BASE + DWC3_GCTL);
		break;
	case DWC3_CONTROLLER_CONNDONE_EVENT:
		dev_info(sdwc->dev, "DWC3_CONTROLLER_CONNDONE_EVENT received\n");
		break;
	default:
		dev_info(sdwc->dev, "unknown dwc3 event\n");
		break;
	}

}

static void dwc3_sprd_host_notify_event(struct usb_hcd *hcd,
	unsigned event)
{
	struct dwc3_sprd *sdwc;

	sdwc = dev_get_drvdata(hcd->self.controller->parent->parent);

	dev_info(sdwc->dev, "%s\n", __func__);
	clk_disable_unprepare(sdwc->core_clk);
	clk_disable_unprepare(sdwc->ref_clk);

	if (sdwc->pwr_collapse) {
		clk_disable_unprepare(sdwc->susp_clk);

		usb_phy_shutdown(sdwc->hs_phy);
		usb_phy_shutdown(sdwc->ss_phy);
	}
	sdwc->bus_active = 0;
}

/*
static int dwc3_sprd_set_boost_supply(struct dwc3_sprd *sdwc, int on)
{
	int ret = 0;

	if (sdwc->gpio_boost_supply)
		gpio_set_value(sdwc->gpio_boost_supply, on);
	else {
		if (on) {
			ret = usb_phy_vbus_on(sdwc->hs_phy);
			ret |= usb_phy_vbus_on(sdwc->ss_phy);
		} else {
			ret = usb_phy_vbus_off(sdwc->hs_phy);
			ret |= usb_phy_vbus_off(sdwc->ss_phy);
		}
	}

	return ret;
}
*/

static void dwc3_hot_plug_work(struct work_struct *w)
{
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						hotplug_work.work);
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&sdwc->lock, flags);
	if (sdwc->vbus_active) {
		if (sdwc->block_active) {
			dev_err(sdwc->dev, "USB core is already activated\n");
			spin_unlock_irqrestore(&sdwc->lock, flags);
			return;
		}

		dwc->vbus_active = 1;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		ret = dwc3_sprd_start(sdwc, sdwc->dr_mode);

		spin_lock_irqsave(&sdwc->lock, flags);
		if (ret)
			sdwc->dr_mode = USB_DR_MODE_UNKNOWN;
		else
			sdwc->block_active = 1;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		if (!ret) {
			wake_lock(&sdwc->wake_lock);
			dev_info(sdwc->dev, "DWC3 USB is running as %s\n",
				sdwc->dr_mode == USB_DR_MODE_HOST ? "HOST" : "DEVICE");
		}
	} else {
		if (!sdwc->block_active) {
			dev_err(sdwc->dev, "USB core is already deactivated\n");
			spin_unlock_irqrestore(&sdwc->lock, flags);
			return;
		}
		spin_unlock_irqrestore(&sdwc->lock, flags);

		dwc3_sprd_stop(sdwc);

		spin_lock_irqsave(&sdwc->lock, flags);
		dwc->vbus_active = 0;
		sdwc->dr_mode = USB_DR_MODE_UNKNOWN;
		sdwc->block_active = 0;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		wake_unlock(&sdwc->wake_lock);

		dev_info(sdwc->dev, "DWC3 USB is shut down\n");
	}
}

static void dwc3_resume_usb_work(struct work_struct *w)
{
/* You may notice that it's a void function. To be frank,
 * I design this function for DWC3's hibernation feature.
 * Anyway, I loaf on the job due to the feature's low priority
 * and little effect on power saving. You're welcome to
 * develop this feature.
 */
/*
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						resume_usb_work);
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	unsigned long flags;
	// TODO:
*/
}

static void dwc3_reset_usb_block_work(struct work_struct *w)
{
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						reset_usb_block_work);
	struct dwc3 *dwc = platform_get_drvdata(sdwc->dwc3);
	unsigned long flags;

	dev_info(sdwc->dev, "%s\n", __func__);

	dwc3_gadget_enable_irq(dwc);

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->err_evt_seen = 0;
	spin_unlock_irqrestore(&dwc->lock, flags);
}

static void dwc3_usbid_work(struct work_struct *w)
{
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						usbid_work.work);
	unsigned long	flags;

	spin_lock_irqsave(&sdwc->lock, flags);
	if (sdwc->vbus_active) {
		sdwc->dr_mode = USB_DR_MODE_HOST;
		spin_unlock_irqrestore(&sdwc->lock, flags);

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
	} else {
		spin_unlock_irqrestore(&sdwc->lock, flags);

		enable_irq(sdwc->vbus_irq);
		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);
	}
}

static void dwc3_vbus_work(struct work_struct *w)
{
	struct dwc3_sprd *sdwc = container_of(w, struct dwc3_sprd,
						vbus_work.work);
	unsigned long flags;

	spin_lock_irqsave(&sdwc->lock, flags);
	if (sdwc->vbus_active) {
		if (dwc3_connect_with_host()) {
				sdwc->vbus_active = 1;
				sdwc->dr_mode = USB_DR_MODE_PERIPHERAL;
				spin_unlock_irqrestore(&sdwc->lock, flags);

				queue_delayed_work(system_nrt_wq,
					&sdwc->hotplug_work, 0);
		} else {
			sdwc->vbus_active = 0;
			spin_unlock_irqrestore(&sdwc->lock, flags);
		}

		usb_notifier_call_chain(USB_CABLE_PLUG_IN);
	} else {
		spin_unlock_irqrestore(&sdwc->lock, flags);

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, 0);

		usb_notifier_call_chain(USB_CABLE_PLUG_OUT);
	}
}

static irqreturn_t dwc3_sprd_usbid_handler(int irq, void *dev_id)
{
	struct dwc3_sprd *sdwc = (struct dwc3_sprd *)dev_id;
	int value;

	value = !!gpio_get_value(sdwc->gpio_usb_id);

	irq_set_irq_type(irq,
		value ? IRQ_TYPE_LEVEL_HIGH : IRQ_TYPE_LEVEL_LOW);

	if (0 != (sdwc->vbus_active = value))
		disable_irq(sdwc->vbus_irq);

	queue_delayed_work(system_nrt_wq, &sdwc->usbid_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t dwc3_sprd_vbus_handler(int irq, void *dev_id)
{
	struct dwc3_sprd *sdwc = (struct dwc3_sprd *)dev_id;
	int value;

	value = !!gpio_get_value(sdwc->gpio_vbus);

	irq_set_irq_type(irq,
		value ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);

	sdwc->vbus_active = value;

	queue_delayed_work(system_nrt_wq, &sdwc->vbus_work, 0);

	return IRQ_HANDLED;
}

static int dwc3_sprd_config_boost_supply(struct dwc3_sprd *sdwc)
{
	int ret;

	ret = devm_gpio_request(sdwc->dev,
			sdwc->gpio_boost_supply, "usb3 boost supply");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(sdwc->gpio_boost_supply, 1);
	if (ret < 0)
		devm_gpio_free(sdwc->dev, sdwc->gpio_boost_supply);

	return ret;
}

static int dwc3_sprd_config_usbid_int(struct dwc3_sprd *sdwc)
{
	int irq, ret;

	ret = devm_gpio_request(sdwc->dev,
			sdwc->gpio_usb_id, "usb3 usb_id detect");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(sdwc->gpio_usb_id);
	if (ret < 0)
		goto err;

	irq = gpio_to_irq(sdwc->gpio_usb_id);
	if (irq < 0)
		goto err;

	ret = irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	if (ret < 0)
		goto err;

	ret = request_irq(irq, dwc3_sprd_usbid_handler,
			IRQF_SHARED | IRQF_NO_SUSPEND,
			"usb3 usb_id dectect irq", sdwc);
	if (ret < 0)
		goto err;

	return ret;
err:
	devm_gpio_free(sdwc->dev, sdwc->gpio_usb_id);

	return ret;
}

static int dwc3_sprd_config_vbus_int(struct dwc3_sprd *sdwc)
{
	int irq, ret;

	ret = devm_gpio_request(sdwc->dev,
			sdwc->gpio_vbus, "usb3 vbus detect");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(sdwc->gpio_vbus);
	if (ret < 0)
		goto err;

	irq = gpio_to_irq(sdwc->gpio_vbus);
	if (irq < 0)
		goto err;

	ret = irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);
	if (ret < 0)
		goto err;

	ret = request_irq(irq, dwc3_sprd_vbus_handler,
			IRQF_SHARED | IRQF_NO_SUSPEND,
			"usb3 vbus dectect irq", sdwc);
	if (ret < 0)
		goto err;

	sdwc->vbus_irq = irq;

	return ret;
err:
	devm_gpio_free(sdwc->dev, sdwc->gpio_vbus);

	return ret;
}

static struct dwc3_sprd *__dwc3_sprd;

static int dwc3_sprd_probe(struct platform_device *pdev)
{
	struct device_node	*node = pdev->dev.of_node, *dwc3_node;
	struct device	*dev = &pdev->dev;
	struct dwc3_sprd	*sdwc;
	struct resource	*res;
	int	ret = 0;
	const char *cable_detect, *usb_mode;

	sdwc = devm_kzalloc(dev, sizeof(*sdwc), GFP_KERNEL);
	if (!sdwc) {
		dev_err(dev, " %s not enough memory\n", __func__);
		return -ENOMEM;
	}

	__dwc3_sprd = sdwc;

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	dev->coherent_dma_mask = dma_mask;

	platform_set_drvdata(pdev, sdwc);
	sdwc->dev = dev;

	INIT_WORK(&sdwc->restart_usb_work, dwc3_restart_usb_work);
	INIT_WORK(&sdwc->reset_usb_block_work, dwc3_reset_usb_block_work);
	INIT_WORK(&sdwc->resume_usb_work, dwc3_resume_usb_work);
	INIT_DELAYED_WORK(&sdwc->hotplug_work, dwc3_hot_plug_work);

	wake_lock_init(&sdwc->wake_lock, WAKE_LOCK_SUSPEND, "dwc3-sprd");
	spin_lock_init(&sdwc->lock);

#ifdef CONFIG_SC_FPGA
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_AP_AHB_USB3_EB |
		BIT_AP_AHB_USB3_SUSPEND_EB | BIT_AP_AHB_USB3_REF_EB);
#else
	sdwc->core_clk = of_clk_get_by_name(node, "core_clk");
	if (IS_ERR(sdwc->core_clk)) {
		dev_err(dev, "no core clk specified\n");
		sdwc->core_clk = NULL;
		goto err;
	}
	clk_prepare_enable(sdwc->core_clk);

	sdwc->ref_clk = of_clk_get_by_name(node, "ref_clk");
	if (IS_ERR(sdwc->ref_clk)) {
		dev_err(dev, "no ref clk specified\n");
		sdwc->ref_clk = NULL;
		goto disable_core_clk;
	}
	sdwc->ref_clk_rate = 24000000;
	clk_prepare_enable(sdwc->ref_clk);

	sdwc->susp_clk = of_clk_get_by_name(node, "susp_clk");
	if (IS_ERR(sdwc->susp_clk)) {
		dev_err(dev, "no suspend clk specified\n");
		sdwc->susp_clk = NULL;
		goto disable_ref_clk;
	}
	sdwc->susp_clk_rate = 32000;
	clk_prepare_enable(sdwc->susp_clk);
#endif
	sdwc->bus_active = 1;
	sdwc->hibernate_en =
		of_property_read_bool(node, "sprd,hibernation-enable");
	sdwc->pwr_collapse =
		of_property_read_bool(node,
			"sprd,power-collapse-on-cable-disconnect");
	sdwc->pwr_collapse_por =
		of_property_read_bool(node,
			"sprd,por-after-power-collapse");
	dev_info(dev, "power collapse=%d, POR=%d\n",
		sdwc->pwr_collapse, sdwc->pwr_collapse_por);

	if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		usb_mode = "PERIPHERAL";
	else if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		usb_mode = "HOST";
	else
		usb_mode = "DRD";
	dev_info(dev, "DWC3 supports working as a %s\n", usb_mode);

	dwc3_set_notifier(&dwc3_sprd_notify_event);
	if (IS_ENABLED(CONFIG_USB_XHCI_HCD))
		xhci_set_notifier(&dwc3_sprd_host_notify_event);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto disable_susp_clk;
	}
	/* We only remap global registers */
	sdwc->base = devm_ioremap_nocache(dev,
		res->start + DWC3_GLOBALS_REGS_START,
		DWC3_DEVICE_REGS_START - DWC3_GLOBALS_REGS_START);
	if (!sdwc->base) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto disable_susp_clk;
	}

	dwc3_node = of_get_next_available_child(node, NULL);
	if (!dwc3_node) {
		dev_err(dev, "failed to find dwc3 child\n");
		goto disable_susp_clk;
	}

	ret = of_platform_populate(node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to add create dwc3 core\n");
		goto disable_susp_clk;
	}

	sdwc->dwc3 = of_find_device_by_node(dwc3_node);
	of_node_put(dwc3_node);
	if (!sdwc->dwc3) {
		dev_err(dev, "failed to get dwc3 platform device\n");
		goto put_dwc3;
	}

	sdwc->hs_phy = devm_usb_get_phy_by_phandle(&sdwc->dwc3->dev,
							"usb-phy", 0);
	if (IS_ERR(sdwc->hs_phy)) {
		dev_err(dev, "unable to get usb2.0 phy device\n");
		ret = PTR_ERR(sdwc->hs_phy);
		goto put_dwc3;
	}

	sdwc->ss_phy = devm_usb_get_phy_by_phandle(&sdwc->dwc3->dev,
							"usb-phy", 1);
	if (IS_ERR(sdwc->ss_phy)) {
		dev_err(dev, "unable to get usb3.0 phy device\n");
		ret = PTR_ERR(sdwc->ss_phy);
		goto put_dwc3;
	}

	ret = of_property_read_string(node,
			"sprd,cable-detection-method", &cable_detect);
	if (ret) {
		dev_err(dev, "fail to get cable detection method\n");
		goto put_dwc3;
	}
	dev_info(dev, "DWC3 cable detection method is %s\n", cable_detect);

	/* Invalidate GPIOs */
	sdwc->gpio_boost_supply = -1;
	sdwc->gpio_usb_id = -1;
	sdwc->gpio_vbus = -1;
	sdwc->vbus_irq = -1;

	if (!strcmp(cable_detect, "typec")) {
		/* Now that Type-C is introduced with USB3.1, the time has come
		 * to turn away from the old-fashioned GPIO-way
		 * which bonds gpios with Vbus or USB_ID to detect USB hot-plug.
		 * Moreover, Type-C enhances ease of use for connecting USB devices
		 * with a focus on minimizing user confusion for plug
		 * and cable orientation.
		 */
		sdwc->hot_plug_nb.notifier_call = dwc3_typec_hotplug;
		sdwc->hot_plug_nb.priority = 0;
		register_typec_notifier(&sdwc->hot_plug_nb);
	} else if (!strcmp(cable_detect, "gpios")) {
		/* In case of no Type-C being involved,
		 * backwards compatibility is essential. GPIOs are traditional
		 * methods to detect USB hot-plug.
		 */
		ret = of_get_named_gpio(node, "sprd,vbus-gpio", 0);
		if (ret < 0) {
			dev_err(dev, "fail to get gpio-vbus\n");
			goto put_dwc3;
		}
		sdwc->gpio_vbus = ret;

		ret = dwc3_sprd_config_vbus_int(sdwc);
		if (ret) {
			dev_err(dev, "fail to config vbus-gpio\n");
			goto put_dwc3;
		}

		INIT_DELAYED_WORK(&sdwc->vbus_work, dwc3_vbus_work);

		if (IS_ENABLED(CONFIG_USB_DWC3_HOST) ||
			IS_ENABLED(CONFIG_USB_DWC3_DUAL_ROLE)) {
			ret = of_get_named_gpio(node, "sprd,usb-id-gpio", 0);
			if (ret < 0) {
				dev_err(dev, "fail to get usb-id-gpio\n");
				goto free_vbus;
			}
			sdwc->gpio_usb_id = ret;

			ret = dwc3_sprd_config_usbid_int(sdwc);
			if (ret) {
				dev_err(dev, "fail to config usb-id-gpio\n");
				goto free_vbus;
			}

			INIT_DELAYED_WORK(&sdwc->vbus_work, dwc3_usbid_work);

			ret = of_get_named_gpio(node,
					"sprd,boost-supply-gpio", 0);
			if (ret < 0) {
				sdwc->gpio_boost_supply = ret;
				dwc3_sprd_config_boost_supply(sdwc);
			}
		}
	} else if(!strcmp(cable_detect, "none")){
		/* In some case, e.g. FPGA, USB Core and PHY
		 * may be always powered on.
		 */
		if (sdwc->pwr_collapse) {
			dev_err(dev, "no cable detection methods are supported!\n");
			goto put_dwc3;
		}
		sdwc->vbus_active = 1;

		if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
			sdwc->dr_mode = USB_DR_MODE_HOST;
		else
			sdwc->dr_mode = USB_DR_MODE_PERIPHERAL;
		dev_info(dev, "DWC3 is always running as %s\n",
			sdwc->dr_mode == USB_DR_MODE_PERIPHERAL ?
				"DEVICE" : "HOST");

		queue_delayed_work(system_nrt_wq, &sdwc->hotplug_work, HZ * 2);
	}

	device_create_file(dev, &dev_attr_maximum_speed);
	device_create_file(dev, &dev_attr_current_speed);
	device_create_file(dev, &dev_attr_restart);

	/* Aiming to more power saving, we SHOULD switch off
	 * USB's clocks and power when it's not working, especially
	 * in case of some chips turning on USB PHY's power by default.
	 */
#ifndef CONFIG_SC_FPGA
	if (IS_ENABLED(CONFIG_USB_DWC3_GADGET)) {
		clk_disable_unprepare(sdwc->core_clk);
		clk_disable_unprepare(sdwc->ref_clk);

		if (sdwc->pwr_collapse) {
			clk_disable_unprepare(sdwc->susp_clk);

			usb_phy_shutdown(sdwc->hs_phy);
			usb_phy_shutdown(sdwc->ss_phy);
		}
		sdwc->bus_active = 0;
	}
#endif
	return 0;

free_vbus:
	if (sdwc->gpio_vbus > 0)
		devm_gpio_free(dev, sdwc->gpio_vbus);

put_dwc3:
	platform_device_put(sdwc->dwc3);

disable_susp_clk:
#ifndef CONFIG_SC_FPGA
	clk_disable_unprepare(sdwc->susp_clk);

disable_ref_clk:
	clk_disable_unprepare(sdwc->ref_clk);

disable_core_clk:
	clk_disable_unprepare(sdwc->core_clk);
#endif

err:
	return ret;
}

static int dwc3_sprd_remove_child(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

static int dwc3_sprd_remove(struct platform_device *pdev)
{
	struct dwc3_sprd	*sdwc = platform_get_drvdata(pdev);

	device_for_each_child(&pdev->dev, NULL, dwc3_sprd_remove_child);

	clk_disable_unprepare(sdwc->core_clk);
	clk_disable_unprepare(sdwc->ref_clk);
	clk_disable_unprepare(sdwc->susp_clk);

	usb_phy_shutdown(sdwc->hs_phy);
	usb_phy_shutdown(sdwc->ss_phy);

	unregister_typec_notifier(&sdwc->hot_plug_nb);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_sprd_pm_suspend(struct device *dev)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	int ret;

	if (sdwc->in_p3) {
		dev_err(sdwc->dev, "USB block is suspended, abort PM suspend\n");
		return -EBUSY;
	}

	ret = dwc3_sprd_suspend(sdwc);
	if (!ret)
		sdwc->in_p3 = 1;

	return ret;
}

static int dwc3_sprd_pm_resume(struct device *dev)
{
	struct dwc3_sprd *sdwc = dev_get_drvdata(dev);
	int ret;

	if (!sdwc->in_p3) {
		dev_err(sdwc->dev, "USB block is active, abort PM resume\n");
		return -EBUSY;
	}

	ret = dwc3_sprd_resume(sdwc);
	if (!ret)
		sdwc->in_p3 = 0;

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int dwc3_sprd_runtime_suspend(struct device *dev)
{
	return 0;
}

static int dwc3_sprd_runtime_resume(struct device *dev)
{
	return 0;
}

static int dwc3_sprd_runtime_idle(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops dwc3_sprd_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		dwc3_sprd_pm_suspend,
		dwc3_sprd_pm_resume)

	SET_RUNTIME_PM_OPS(
		dwc3_sprd_runtime_suspend,
		dwc3_sprd_runtime_resume,
		dwc3_sprd_runtime_idle)
};

#ifdef CONFIG_OF
static const struct of_device_id sprd_dwc3_match[] = {
	{ .compatible = "sprd,dwc-usb3" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_dwc3_match);
#endif

static struct platform_driver dwc3_sprd_driver = {
	.probe		= dwc3_sprd_probe,
	.remove		= dwc3_sprd_remove,
	.driver		= {
		.name	= "sprd-dwc3",
		.of_match_table = sprd_dwc3_match,
	},
};

module_platform_driver(dwc3_sprd_driver);

MODULE_ALIAS("platform:sprd-dwc3");
MODULE_AUTHOR("Miao Zhu <miao.zhu@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 SPRD Glue Layer");
