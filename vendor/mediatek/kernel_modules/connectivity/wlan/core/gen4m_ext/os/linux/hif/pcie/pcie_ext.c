/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/******************************************************************************
 *[File]             pcie_ext.c
 *[Version]          v1.0
 *[Revision Date]    2023-06-27
 *[Author]
 *[Description]
 *    The program provides PCIE HIF driver
 *[Copyright]
 *    Copyright (C) 2023 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include <linux/exynos-pci-noti.h>

#if (CFG_SUPPORT_CONNAC3X == 1)
#define TC10_PCIE_CH_NUM	1
#define PCIE_L1SS_CTRL_WIFI	(0x1 << 4)
extern int exynos_pcie_rc_poweron(int ch_num);
extern void exynos_pcie_rc_poweroff(int ch_num);
extern int exynos_pcie_pm_resume(int ch_num);
extern void exynos_pcie_pm_suspend(int ch_num);
extern int exynos_pcie_rc_chk_link_status(int ch_num);
extern int exynos_pcie_register_event(struct exynos_pcie_register_event *reg);
extern int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg);
extern int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num);
extern int exynos_pcie_l1_exit(int ch_num);

static void wlan_exynos_pci_event_cb(struct exynos_pcie_notify *notify);
static int wlan_reg_exynos_pci_event(struct mt66xx_hif_driver_data *data);
static void wlan_dereg_exynos_pci_event(struct mt66xx_hif_driver_data *data);
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if (CFG_SUPPORT_CONNAC3X == 1) && defined(CFG_EAP_TC10)
static void wlan_eap_tc10_pci_event_cb(struct exynos_pcie_notify *notify)
{
	struct pci_dev *pci_dev;

	if (!notify) {
		DBGLOG(HAL, ERROR, "NULL notify.\n");
		return;
	}

	pci_dev = notify->user;
	if (!pci_dev) {
		DBGLOG(HAL, ERROR, "NULL pci_dev.\n");
		return;
	}

	DBGLOG(HAL, INFO, "event: %d\n", notify->event);

	switch (notify->event) {
	case EXYNOS_PCIE_EVENT_CPL_TIMEOUT:
		break;
	case EXYNOS_PCIE_EVENT_LINKDOWN:
		break;
	default:
		break;
	}
}

static int wlan_reg_eap_tc10_pci_event(struct mt66xx_hif_driver_data *data)
{
	struct exynos_pcie_register_event *pci_event;
	struct mt66xx_chip_info *chip_info = data->chip_info;
	int ret = 0;

	DBGLOG(HAL, INFO, "\n");

	data->eap_tc10_pci_event = kalMemAlloc(
		sizeof(struct exynos_pcie_register_event),
		VIR_MEM_TYPE);
	if (!data->eap_tc10_pci_event) {
		ret = -ENOMEM;
		goto exit;
	}
	kalMemZero(data->eap_tc10_pci_event,
		sizeof(struct exynos_pcie_register_event));

	pci_event = data->eap_tc10_pci_event;
	pci_event->events = EXYNOS_PCIE_EVENT_LINKDOWN |
		EXYNOS_PCIE_EVENT_CPL_TIMEOUT;
	pci_event->user = chip_info->pdev;
	pci_event->mode = EXYNOS_PCIE_TRIGGER_CALLBACK;
	pci_event->callback = wlan_eap_tc10_pci_event_cb;

	ret = exynos_pcie_register_event(pci_event);

exit:
	DBGLOG(HAL, INFO, "ret: %d\n", ret);
	return ret;
}

static void wlan_dereg_eap_tc10_pci_event(struct mt66xx_hif_driver_data *data)
{
	DBGLOG(HAL, INFO, "\n");

	if (!data->eap_tc10_pci_event)
		return;

	exynos_pcie_deregister_event(data->eap_tc10_pci_event);
	kalMemFree(data->eap_tc10_pci_event, VIR_MEM_TYPE,
		sizeof(struct exynos_pcie_register_event));
}
#endif