ifeq ($(CONFIG_SCSC_WLAN_HIP5),y)
# ----------------------------------------------------------------------------
# HIP5
# ----------------------------------------------------------------------------
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip5.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip4_sampler.o
else
# ----------------------------------------------------------------------------
# HIP4
# ----------------------------------------------------------------------------
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip4.o
ifeq ($(CONFIG_SCSC_WLAN_HIP4_PROFILING),y)
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip4_sampler.o
endif
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip4_smapper.o
endif

obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-mbulk.o

# Upper driver
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-dev.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-cfg80211_ops.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-netif.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-rx.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-tx.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-ba.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-ba_replay.o

# SAPs
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-sap_mlme.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-sap_ma.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-sap_dbg.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-sap_test.o

# ----------------------------------------------------------------------------
# Common Driver Files
# ----------------------------------------------------------------------------
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-mib.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-debug.o
#obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-debug_frame.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-procfs.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-mgt.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-mlme.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-udi.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-log_clients.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-src_sink.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-fw_test.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-cac.o
ifeq ($(CONFIG_SCSC_WLAN_TX_API),y)
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-txbp.o
else
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifi_fcq.o
endif
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-ioctl.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-traffic_monitor.o
ifeq ($(CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER),y)
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-load_manager.o
endif
ifeq ($(CONFIG_SCSC_WLAN_HOST_DPD),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += kunit-test-dpd_mmap.o
endif
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-reg_info.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-log2us.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-ini_config.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-tdls_manager.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wlan_mmap.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-qsfs.o

ifeq ($(CONFIG_SCSC_WLAN_TRACEPOINT_DEBUG),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += slsi_tracepoint_debug.o
endif

# ----------------------------------------------------------------------------
# CPU Hotplug Monitor
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WLAN_CPUHP_MONITOR),y)
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-slsi_cpuhp_monitor.o
endif

obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_api.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_core.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_internal.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_module.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_ring_connectivity.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_ring_pktfate.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-scsc_wifilogger_ring_wakelock.o

# ----------------------------------------------------------------------------
# Building for Hardware
# ----------------------------------------------------------------------------
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-hip.o
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-cm_if.o
ifeq ($(CONFIG_SCSC_WLAN_GSCAN_ENABLE),y)
obj-$(CONFIG_SCSC_WLAN_KUNIT_TEST) += kunit-test-nl80211_vendor.o
endif
