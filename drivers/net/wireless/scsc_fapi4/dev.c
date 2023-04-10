/****************************************************************************
 *
 * Copyright (c) 2012 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"
#include "hip.h"
#include "mgt.h"
#include "debug.h"
#include "udi.h"
#include "hip_bh.h"
#include "hydra.h"
#include "cfg80211_ops.h"
#include "netif.h"
#include "procfs.h"
#include "ba.h"
#include "nl80211_vendor.h"

#include "sap_mlme.h"
#include "sap_ma.h"
#include "sap_dbg.h"
#include "sap_test.h"

#ifdef CONFIG_SCSC_WLAN_KIC_OPS
#include "kic.h"
#endif

static char *mib_file = "wlan.hcf";
module_param(mib_file, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mib_file, "mib data filename");

static char *local_mib_file = "localmib.hcf";
module_param(local_mib_file, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(local_mib_file, "local mib data filename (Optional extra mib values)");

static char *maddr_file = "mac.txt";
module_param(maddr_file, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(maddr_file, "mac address filename");

static bool term_udi_users = true;
module_param(term_udi_users, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(term_udi_users, "Try to terminate UDI user space users (applications) connected on the cdev (0, 1)");

static int sig_wait_cfm_timeout = 3000;
module_param(sig_wait_cfm_timeout, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sig_wait_cfm_timeout, "Signal wait timeout in milliseconds (default: 3000)");

/* automatically initialised to zero upon memory allocation just as external variables are */
static bool tx_cfm_reqd;
module_param(tx_cfm_reqd, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_cfm_reqd, "Tx Confirm: enable (0, 1)");

static bool lls_disabled;
module_param(lls_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lls_disabled, "Disable LLS: to disable LLS set 1");

static bool gscan_disabled = true;
module_param(gscan_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gscan_disabled, "Disable gscan: to disable gscan set 1");

static bool epno_disabled = true;
module_param(epno_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(epno_disabled, "Disable ePNO: to disable ePNO set 1.\nNote: for ePNO to work gscan should be enabled");

static bool vo_vi_block_ack_disabled;
module_param(vo_vi_block_ack_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(vo_vi_block_ack_disabled, "Disable VO VI Block Ack logic added for WMM AC Cert : 5.1.4");

static int max_scan_result_count = 300;
module_param(max_scan_result_count, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_scan_result_count, "Max scan results to be reported");

bool slsi_dev_gscan_supported(void)
{
	return !gscan_disabled;
}

bool slsi_dev_lls_supported(void)
{
	return !lls_disabled;
}

bool slsi_dev_epno_supported(void)
{
	return !epno_disabled;
}

bool slsi_dev_vo_vi_block_ack(void)
{
	return vo_vi_block_ack_disabled;
}

int slsi_dev_get_scan_result_count(void)
{
	return max_scan_result_count;
}

static int slsi_dev_inetaddr_changed(struct notifier_block *nb, unsigned long data, void *arg)
{
	struct slsi_dev     *sdev = container_of(nb, struct slsi_dev, inetaddr_notifier);
	struct in_ifaddr    *ifa = arg;
	struct net_device   *dev = ifa->ifa_dev->dev;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (!wdev)
		return NOTIFY_DONE;

	if (wdev->wiphy != sdev->wiphy)
		return NOTIFY_DONE;

	SLSI_NET_DBG2(dev, SLSI_NETDEV, "IP:%pI4\n", &ifa->ifa_address);

	if (data == NETDEV_DOWN) {
		SLSI_NET_DBG2(dev, SLSI_NETDEV, "Returning 0 for NETDEV_DOWN event\n");
		return 0;
	}
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
#ifndef SLSI_TEST_DEV
	if (SLSI_IS_VIF_INDEX_WLAN(ndev_vif) && wake_lock_active(&sdev->wlan_roam_wl)) {
		SLSI_NET_DBG2(dev, SLSI_NETDEV, "Releasing the roaming wakelock\n");
		wake_unlock(&sdev->wlan_roam_wl);
	}
#endif
	slsi_ip_address_changed(sdev, dev, ifa->ifa_address);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return 0;
}

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
static int slsi_dev_inet6addr_changed(struct notifier_block *nb, unsigned long data, void *arg)
{
	struct slsi_dev     *sdev = container_of(nb, struct slsi_dev, inet6addr_notifier);
	struct inet6_ifaddr *ifa = arg;
	struct net_device   *dev = ifa->idev->dev;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	(void)data; /* unused */

	if (!wdev)
		return NOTIFY_DONE;

	if (wdev->wiphy != sdev->wiphy)
		return NOTIFY_DONE;

	SLSI_NET_DBG3(dev, SLSI_NETDEV, "Last three bytes of IPv6 addr = %02x %02x %02x", ifa->addr.s6_addr[13],
		      ifa->addr.s6_addr[14], ifa->addr.s6_addr[15]);

	slsi_spinlock_lock(&ndev_vif->ipv6addr_lock);
	memcpy(&ndev_vif->ipv6address, &ifa->addr, sizeof(struct in6_addr));
	slsi_spinlock_unlock(&ndev_vif->ipv6addr_lock);

	return 0;
}
#endif

static void slsi_regd_init(struct slsi_dev *sdev)
{
	struct ieee80211_regdomain *slsi_world_regdom_custom = sdev->device_config.domain_info.regdomain;
	struct ieee80211_reg_rule  reg_rules[] = {
		/* Channel 1 - 11*/
		REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
		/* Channel 12 - 13 NO_IR*/
		REG_RULE(2467 - 10, 2472 + 10, 40, 0, 20, NL80211_RRF_NO_IR),
#endif
		/* Channel 36 - 48 */
		REG_RULE(5180 - 10, 5240 + 10, 80, 0, 20, 0),
		/* Channel 52 - 64 */
		REG_RULE(5260 - 10, 5320 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 100 - 140 */
		REG_RULE(5500 - 10, 5700 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 149 - 165 */
		REG_RULE(5745 - 10, 5825 + 10, 80, 0, 20, 0),
	};

	int                        i;

	SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "regulatory init\n");

	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "chip ver=Maxwell, chan supp=2.4 & 5 GHz");
	slsi_world_regdom_custom->n_reg_rules = 6;
	for (i = 0; i < slsi_world_regdom_custom->n_reg_rules; i++)
		slsi_world_regdom_custom->reg_rules[i] = reg_rules[i];

	/* Country code '00' indicates world regulatory domain */
	slsi_world_regdom_custom->alpha2[0] = '0';
	slsi_world_regdom_custom->alpha2[1] = '0';

	wiphy_apply_custom_regulatory(sdev->wiphy, slsi_world_regdom_custom);
}

struct slsi_dev *slsi_dev_attach(struct device *dev, struct scsc_mx *core, struct scsc_service_client *mx_wlan_client)
{
	struct slsi_dev *sdev;
	int i;

	SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "Add Device\n");

	sdev = slsi_cfg80211_new(dev);
	if (sdev == NULL) {
		SLSI_ERR_NODEV("No sdev\n");
		return NULL;
	}

	sdev->mlme_blocked = false;

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	SLSI_MUTEX_INIT(sdev->start_stop_mutex);
	SLSI_MUTEX_INIT(sdev->device_config_mutex);

	sdev->dev = dev;
	sdev->maxwell_core = core;
	memcpy(&sdev->mx_wlan_client, mx_wlan_client, sizeof(struct scsc_service_client));

	sdev->fail_reported = false;
	sdev->p2p_certif = false;
	sdev->mib_file_name = mib_file;
	sdev->local_mib_file_name = local_mib_file;
	sdev->maddr_file_name = maddr_file;
	sdev->device_config.qos_info = 0;
	memset(&sdev->chip_info_mib, 0xFF, sizeof(struct slsi_chip_info_mib));

#ifdef CONFIG_SCSC_WLAN_WES_NCHO
	sdev->device_config.okc_mode = 0;
	sdev->device_config.wes_mode = 0;
	sdev->device_config.roam_scan_mode = 0;
#endif

	slsi_log_clients_init(sdev);

	slsi_wakelock_init(&sdev->wlan_wl, "wlan");
	slsi_wakelock_init(&sdev->wlan_wl_to, "wlan_to");
#ifndef SLSI_TEST_DEV
	wake_lock_init(&sdev->wlan_roam_wl, WAKE_LOCK_SUSPEND, "wlan_roam_wl");
#endif

#ifdef CONFIG_SCSC_WLAN_OFFLINE_TRACE
	slsi_spinlock_create(&sdev->offline_dbg_lock);
#endif

	sdev->recovery_next_state = 0;
	init_completion(&sdev->recovery_remove_completion);
	init_completion(&sdev->recovery_stop_completion);
	init_completion(&sdev->recovery_completed);
	sdev->recovery_status = 0;

	sdev->term_udi_users         = &term_udi_users;
	sdev->sig_wait_cfm_timeout   = &sig_wait_cfm_timeout;
	sdev->tx_cfm_reqd = &tx_cfm_reqd;
	slsi_sig_send_init(&sdev->sig_wait);

	for (i = 0; i < SLSI_LLS_AC_MAX; i++)
		atomic_set(&sdev->tx_host_tag[i], ((1 << 2) | i));

	if (slsi_skb_work_init(sdev, NULL, &sdev->rx_dbg_sap, "slsi_wlan_rx_dbg_sap", slsi_rx_dbg_sap_work) != 0)
		goto err_if;

	if (slsi_netif_init(sdev) != 0) {
		SLSI_ERR(sdev, "Can not create the network interface\n");
		goto err_ctrl_wq_init;
	}

	if (slsi_hip_init(sdev, dev) != 0) {
		SLSI_ERR(sdev, "slsi_hip_init() Failed\n");
		goto err_netif_init;
	}

	if (slsi_udi_node_init(sdev, dev) != 0) {
		SLSI_ERR(sdev, "failed to init UDI\n");
		goto err_hip_init;
	}

	slsi_create_proc_dir(sdev);

	/* update regulatory domain */
	slsi_regd_init(sdev);

#ifdef CONFIG_SCSC_WLAN_GSCAN_ENABLE
	slsi_nl80211_vendor_init(sdev);
#endif

	if (slsi_cfg80211_register(sdev) != 0) {
		SLSI_ERR(sdev, "failed to register with cfg80211\n");
		goto err_udi_proc_init;
	}

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	sdev->inet6addr_notifier.notifier_call = slsi_dev_inet6addr_changed;
	if (register_inet6addr_notifier(&sdev->inet6addr_notifier) != 0) {
		SLSI_ERR(sdev, "failed to register inet6addr_notifier\n");
		goto err_cfg80211_registered;
	}
#endif

	sdev->inetaddr_notifier.notifier_call = slsi_dev_inetaddr_changed;
	if (register_inetaddr_notifier(&sdev->inetaddr_notifier) != 0) {
		SLSI_ERR(sdev, "failed to register inetaddr_notifier\n");
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
		unregister_inet6addr_notifier(&sdev->inet6addr_notifier);
#endif
		goto err_cfg80211_registered;
	}

#ifdef SLSI_TEST_DEV
	slsi_init_netdev_mac_addr(sdev);
#endif
	slsi_rx_ba_init(sdev);

	if (slsi_netif_register(sdev, sdev->netdev[SLSI_NET_INDEX_WLAN]) != 0) {
		SLSI_ERR(sdev, "failed to register with wlan netdev\n");
		goto err_inetaddr_registered;
	}
#ifdef CONFIG_SCSC_WLAN_STA_ONLY
	SLSI_ERR(sdev, "CONFIG_SCSC_WLAN_STA_ONLY: not registering p2p netdev\n");
#else
	if (slsi_netif_register(sdev, sdev->netdev[SLSI_NET_INDEX_P2P]) != 0) {
		SLSI_ERR(sdev, "failed to register with p2p netdev\n");
		goto err_wlan_registered;
	}
#endif
#ifdef CONFIG_SCSC_WLAN_KIC_OPS
	if (wifi_kic_register(sdev) < 0)
		SLSI_ERR(sdev, "failed to register Wi-Fi KIC ops\n");
#endif

	slsi_dbg_skb_device_add();
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	sdev->current_tspec_id = -1;
	sdev->tspec_error_code = -1;

	/* Driver workqueue used to queue work in different modes (STA/P2P/HS2) */
	sdev->device_wq = alloc_ordered_workqueue("slsi_wlan_wq", 0);
	if (!sdev->device_wq) {
		SLSI_ERR(sdev, "Cannot allocate workqueue\n");
		goto err_wlan_registered;
	}
	return sdev;

err_wlan_registered:
	slsi_netif_remove(sdev, sdev->netdev[SLSI_NET_INDEX_WLAN]);

err_inetaddr_registered:
	unregister_inetaddr_notifier(&sdev->inetaddr_notifier);
#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	unregister_inet6addr_notifier(&sdev->inet6addr_notifier);
#endif

err_cfg80211_registered:
	slsi_cfg80211_unregister(sdev);

err_udi_proc_init:
	slsi_remove_proc_dir(sdev);
	slsi_udi_node_deinit(sdev);

err_hip_init:
	slsi_hip_deinit(sdev);

err_netif_init:
	slsi_netif_deinit(sdev);

err_ctrl_wq_init:
	slsi_skb_work_deinit(&sdev->rx_dbg_sap);

err_if:
	slsi_wakelock_exit(&sdev->wlan_wl);
	slsi_wakelock_exit(&sdev->wlan_wl_to);

	slsi_cfg80211_free(sdev);
	return NULL;
}

void slsi_dev_detach(struct slsi_dev *sdev)
{
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "Remove Device\n");

	slsi_stop(sdev);

#ifdef CONFIG_SCSC_WLAN_KIC_OPS
	wifi_kic_unregister();
#endif
	complete_all(&sdev->sig_wait.completion);
	complete_all(&sdev->recovery_remove_completion);
	complete_all(&sdev->recovery_stop_completion);
	complete_all(&sdev->recovery_completed);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Unregister inetaddr_notifier\n");
	unregister_inetaddr_notifier(&sdev->inetaddr_notifier);

#ifndef CONFIG_SCSC_WLAN_BLOCK_IPV6
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Unregister inet6addr_notifier\n");
	unregister_inet6addr_notifier(&sdev->inet6addr_notifier);
#endif

	WARN_ON(sdev->device_wq == NULL);
	if (sdev->device_wq)
		flush_workqueue(sdev->device_wq);

#ifdef CONFIG_SCSC_WLAN_GSCAN_ENABLE
	slsi_nl80211_vendor_deinit(sdev);
#endif

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Unregister netif\n");
	slsi_netif_remove_all(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Unregister cfg80211\n");
	slsi_cfg80211_unregister(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Remove proc entries\n");
	slsi_remove_proc_dir(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "De-initialise the UDI\n");
	slsi_log_clients_terminate(sdev);
	slsi_udi_node_deinit(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "De-initialise Hip\n");
	slsi_hip_deinit(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "De-initialise netif\n");
	slsi_netif_deinit(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "De-initialise TA\n");

	slsi_regd_deinit(sdev);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Stop Work Queues\n");
	slsi_skb_work_deinit(&sdev->rx_dbg_sap);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Clean up wakelock\n");
	slsi_wakelock_exit(&sdev->wlan_wl);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Clean up wakelock\n");
	slsi_wakelock_exit(&sdev->wlan_wl_to);

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Cleanup Device Data\n");
	slsi_kfree_skb(sdev->device_config.channel_config);

#ifndef SLSI_TEST_DEV
	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Clean up wakelock\n");
	wake_lock_destroy(&sdev->wlan_roam_wl);
#endif

	SLSI_DBG2(sdev, SLSI_INIT_DEINIT, "Free cfg80211\n");
	slsi_cfg80211_free(sdev);

	slsi_dbg_skb_device_remove();
	slsi_dbg_track_skb_report();
	slsi_dbg_track_skb_reset();
}

int __init slsi_dev_load(void)
{
	SLSI_INFO_NODEV("Loading Maxwell Wi-Fi driver\n");

	slsi_dbg_track_skb_init();

	if (slsi_udi_init())
		SLSI_INFO_NODEV("Failed to init udi - continuing\n");

	if (slsi_sm_service_driver_register())
		SLSI_INFO_NODEV("slsi_sm_service_driver_register failed - continuing\n");

	/* Register SAPs */
	sap_mlme_init();
	sap_ma_init();
	sap_dbg_init();
	sap_test_init();

	SLSI_INFO_NODEV("--- Maxwell Wi-Fi driver loaded successfully ---\n");
	return 0;
}

void __exit slsi_dev_unload(void)
{
	SLSI_INFO_NODEV("Unloading Maxwell Wi-Fi driver\n");

	/* Unregister SAPs */
	sap_mlme_deinit();
	sap_ma_deinit();
	sap_dbg_deinit();
	sap_test_deinit();

	slsi_sm_service_driver_unregister();

	slsi_udi_deinit();

	SLSI_INFO_NODEV("--- Maxwell Wi-Fi driver unloaded successfully ---\n");
}

module_init(slsi_dev_load);
module_exit(slsi_dev_unload);

MODULE_DESCRIPTION("mx140 Wi-Fi Driver");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
