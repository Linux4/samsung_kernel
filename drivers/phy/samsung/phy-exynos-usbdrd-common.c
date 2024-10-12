#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <soc/samsung/exynos-cpupm.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#include "phy-exynos-usbdrd.h"
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
#include <sound/samsung/abox.h>
struct device *abox_dev;
#endif


u32 usb_user_scenario;
EXPORT_SYMBOL_GPL(usb_user_scenario);
int xhci_exynos_pm_state; /* xhci_exynos suspend/resume state */
EXPORT_SYMBOL_GPL(xhci_exynos_pm_state);
struct wakeup_source *main_hcd_wakelock; /* Wakelock for HS HCD */
EXPORT_SYMBOL_GPL(main_hcd_wakelock);
struct wakeup_source *shared_hcd_wakelock; /* Wakelock for SS HCD */
EXPORT_SYMBOL_GPL(shared_hcd_wakelock);
int usb_idle_ip_index;
EXPORT_SYMBOL_GPL(usb_idle_ip_index);

int otg_connection;
EXPORT_SYMBOL_GPL(otg_connection);

int is_otg_only;
EXPORT_SYMBOL_GPL(is_otg_only);

#define AUDIO_MODE_IN_CALL	2

#define OTG_NO_CONNECT		0
#define OTG_CONNECT_ONLY	1
#define OTG_DEVICE_CONNECT	2

u32 otg_is_connect(void)
{
	if (!otg_connection)
		return OTG_NO_CONNECT;
	else if (is_otg_only)
		return OTG_CONNECT_ONLY;
	else
		return OTG_DEVICE_CONNECT;
}
EXPORT_SYMBOL_GPL(otg_is_connect);

/*
 * return usb scenario info
 */
int exynos_usb_scenario_info(void)
{
	return usb_user_scenario;
}

void exynos_usb_wakelock(struct exynos_usbdrd_phy *phy_drd,
				    bool lock, u32 usb_scenario)
{
	if (phy_drd->usbphy_info.low_power_call) {
		pr_info("%s: lock = %d\n", __func__, lock);

		usb_user_scenario = usb_scenario;

		if (lock)
			__pm_stay_awake(main_hcd_wakelock);
		else
			__pm_relax(main_hcd_wakelock);
	}
}

static int exynos_usb_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_usbdrd_phy *phy_drd
		= container_of(nb, struct exynos_usbdrd_phy, pm_nb);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s suspend prepare\n", __func__);
		phy_drd->phy_usbdrd_suspended = true;
		reinit_completion(&phy_drd->resume_cmpl);
		break;
	case PM_POST_SUSPEND:
		pr_info("%s post suspend\n", __func__);
		phy_drd->phy_usbdrd_suspended = false;
		complete(&phy_drd->resume_cmpl);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

void exynos_usb_pm_noti_init(struct exynos_usbdrd_phy *phy_drd)
{
	init_completion(&phy_drd->resume_cmpl);
	phy_drd->phy_usbdrd_suspended = false;
	phy_drd->pm_nb.notifier_call = exynos_usb_pm_notifier;
	register_pm_notifier(&phy_drd->pm_nb);
}

int xhci_exynos_wake_lock(int is_main_hcd, int is_lock)
{
	int idle_ip_index;
	struct wakeup_source *main_wakelock, *shared_wakelock;

	main_wakelock = main_hcd_wakelock;
	shared_wakelock = shared_hcd_wakelock;

	pr_info("%s\n", __func__);

	if (is_lock) {
		if (is_main_hcd) {
			pr_info("%s: Main HCD WAKE LOCK\n", __func__);
			__pm_stay_awake(main_wakelock);
		} else {
			pr_info("%s: Shared HCD WAKE LOCK\n", __func__);
			__pm_stay_awake(shared_wakelock);
		}

		/* Add a routine for disable IDLEIP (IP idle) */
		pr_info("IDLEIP(SICD) disable.\n");
		idle_ip_index = usb_idle_ip_index;
		pr_info("%s, usb idle ip = %d\n", __func__, idle_ip_index);
		exynos_update_ip_idle_status(idle_ip_index, 0);
	} else {
		if (is_main_hcd) {
			pr_info("%s: Main HCD WAKE UNLOCK\n", __func__);
			__pm_relax(main_wakelock);
		} else {
			pr_info("%s: Shared HCD WAKE UNLOCK\n", __func__);
			__pm_relax(shared_wakelock);
		}

		if (otg_is_connect() == 0) {
			if (!main_wakelock->active && !shared_wakelock->active) {
				pr_info("Try to IDLEIP Enable!!!\n");

				/* Add a routine for enable IDLEIP (IP idle) */
				pr_info("IDLEIP(SICD) Enable.\n");
				idle_ip_index = usb_idle_ip_index;
				pr_info("%s, usb idle ip = %d\n", __func__, idle_ip_index);
				exynos_update_ip_idle_status(idle_ip_index, 1);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(xhci_exynos_wake_lock);

/*
 * Set bypass = 1 will skip USB suspend.
 */
static int exynos_check_pm_bypass(int suspend)
{
	int user_scenario;

	user_scenario = exynos_usb_scenario_info();
	pr_info("%s: scenario = %d, suspend = %d, pm = %d\n",
		__func__, user_scenario, suspend, xhci_exynos_pm_state);

	if (suspend == false && xhci_exynos_pm_state != BUS_RESUME)
		return false;

	if (user_scenario == AUDIO_MODE_IN_CALL)
		return true;

	return false;
}

int exynos_usbdrd_phy_set_common(struct exynos_usbdrd_phy *phy_drd,
				enum phy_mode mode, int submode)
{
	int ret = 0;
	int phymode = mode;

	switch (phymode) {
	case PHY_MODE_BUS_SUSPEND:
		xhci_exynos_wake_lock(submode, 0);
		if (submode) { /* main hcd */
			pr_info("%s, pm_state = %d\n", __func__, xhci_exynos_pm_state);
			xhci_exynos_pm_state = BUS_SUSPEND;
		}
		break;
	case PHY_MODE_BUS_RESUME:
		xhci_exynos_wake_lock(submode, 1);
		if (submode) { /* main hcd */
			pr_info("%s, pm_state = %d\n", __func__, xhci_exynos_pm_state);
			xhci_exynos_pm_state = BUS_RESUME;
		}
		if (exynos_usb_scenario_info() == AUDIO_MODE_IN_CALL && submode) {
			pr_info("%s, release wakelock in call state\n", __func__);
			__pm_relax(main_hcd_wakelock);
			return 0;
		}
		break;
	case PHY_MODE_ABOX_POWER:
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
		if (submode == 1) {
			if (phy_drd->phy_usbdrd_suspended) {
				pr_info("%s: wait resume completion\n", __func__);
				wait_for_completion_timeout(&phy_drd->resume_cmpl,
							    msecs_to_jiffies(5000));
			}
			ret = abox_request_power_sync(abox_dev, 0x7007, "TFA9874");
			if (ret < 0)
				dev_err(abox_dev, "failed to abox_request_power_sync\n");

			ret = abox_wait_boot_timeout(abox_dev, nsecs_to_jiffies(100000000UL));
			if (ret < 0)
				dev_err(abox_dev, "failed to abox_wait_boot_timeout\n");
		} else {
			abox_release_power(abox_dev, 0x7007, "TFA9874");
		}
#endif
		break;
	case PHY_MODE_CALL_ENTER:
		exynos_usb_wakelock(phy_drd, 0, submode);
		break;
	case PHY_MODE_CALL_EXIT:
		exynos_usb_wakelock(phy_drd, 1, submode);
		break;
	case PHY_MODE_SUSPEND_BYPASS:
		ret = exynos_check_pm_bypass(true);
		break;
	case PHY_MODE_RESUME_BYPASS:
		ret = exynos_check_pm_bypass(false);
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_usbdrd_phy_set_common);

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
void get_exynos_abox_device(void)
{
	struct device_node *np = NULL;
	struct device_node *np_abox;
	struct platform_device *pdev_abox;

	np = of_find_compatible_node(NULL, NULL, "synopsys,dwc3");
	if (!np) {
		pr_err("%s: failed to get the dwc3 node\n", __func__);
		return;
	}

	np_abox = of_parse_phandle(np, "abox", 0);
	if(!np_abox) {
		pr_err("Failed to get abox device node\n");
		return;
	}

	pdev_abox = of_find_device_by_node(np_abox);
	if (!pdev_abox) {
		pr_err("Failed to get abox platform device\n");
		return;
	}

	abox_dev = &pdev_abox->dev;
}
EXPORT_SYMBOL_GPL(get_exynos_abox_device);
#endif
