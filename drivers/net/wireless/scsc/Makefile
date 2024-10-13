# ----------------------------------------------------------------------------
# FILE: Makefile
#
# PURPOSE:
#       Build instructions for SCSC Wlan driver.
#
# Copyright (c) 2018 Samsung Electronics Co., Ltd. All rights reserved
# ----------------------------------------------------------------------------

# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
KBUILD_CFLAGS += -Wframe-larger-than=4096

clean-files := *.o *.ko

obj-$(CONFIG_SCSC_WLAN) += scsc_wlan.o

ifeq ($(CONFIG_SCSC_WLAN_HIP5),y)
# ----------------------------------------------------------------------------
# HIP5
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip5.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip4_sampler.o
ccflags-y += -DCONFIG_SCSC_WLAN_HIP5
ccflags-y += -DCONFIG_SCSC_WLAN_RX_NAPI
ccflags-y += -DCONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
ccflags-y += -DCONFIG_SCSC_WLAN_HIP4_PROFILING
else
# ----------------------------------------------------------------------------
# HIP4
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip4.o
ifeq ($(CONFIG_SCSC_WLAN_HIP4_PROFILING),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip4_sampler.o
endif
scsc_wlan-$(CONFIG_SCSC_SMAPPER) += hip4_smapper.o
endif

scsc_wlan-$(CONFIG_SCSC_WLAN) += mbulk.o

# Upper driver
scsc_wlan-$(CONFIG_SCSC_WLAN) += dev.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cfg80211_ops.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += netif.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += rx.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += tx.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += ba.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += ba_replay.o

# SAPs
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_mlme.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_ma.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_dbg.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += sap_test.o

# ----------------------------------------------------------------------------
# Common Driver Files
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += mib.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mib_text_convert.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += debug.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += debug_frame.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += procfs.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mgt.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mlme.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += udi.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += log_clients.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += src_sink.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += fw_test.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cac.o
ifeq ($(CONFIG_SCSC_WLAN_TX_API),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += txbp.o
else
scsc_wlan-$(CONFIG_SCSC_WLAN) += scsc_wifi_fcq.o
endif
scsc_wlan-$(CONFIG_SCSC_WLAN) += ioctl.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += traffic_monitor.o
ifeq ($(CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += load_manager.o
endif
ifeq ($(CONFIG_SCSC_WLAN_HOST_DPD),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += dpd_mmap.o
endif
scsc_wlan-$(CONFIG_SCSC_WLAN) += reg_info.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += log2us.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += ini_config.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += tdls_manager.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += scsc_wlan_mmap.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += qsfs.o

ifeq ($(CONFIG_SCSC_WLAN_TRACEPOINT_DEBUG),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += slsi_tracepoint_debug.o
endif

# ----------------------------------------------------------------------------
# CPU Hotplug Monitor
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WLAN_CPUHP_MONITOR),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += slsi_cpuhp_monitor.o
endif

# ----------------------------------------------------------------------------
# Wifi Logger Files
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WIFILOGGER),m)
ccflags-y += -DCONFIG_SCSC_WIFILOGGER
endif

ifeq ($(CONFIG_SCSC_WLAN_HIP4_PROFILING),m)
ccflags-y += -DCONFIG_SCSC_WLAN_HIP4_PROFILING
endif

ifeq ($(CONFIG_SCSC_WLAN_SAE_CONFIG),m)
ccflags-y += -DCONFIG_SCSC_WLAN_SAE_CONFIG
endif

obj-$(CONFIG_SCSC_WIFILOGGER) +=  scsc_wifilogger.o
scsc_wifilogger-y += scsc_wifilogger_api.o \
                     scsc_wifilogger_internal.o \
                     scsc_wifilogger_core.o \
                     scsc_wifilogger_module.o \
                     scsc_wifilogger_ring_connectivity.o \
                     scsc_wifilogger_ring_wakelock.o \
                     scsc_wifilogger_ring_pktfate.o

scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_DEBUGFS) += scsc_wifilogger_debugfs.o
scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_TEST) += scsc_wifilogger_ring_test.o

# ----------------------------------------------------------------------------
# ACM
# ----------------------------------------------------------------------------
ifeq ($(CONFIG_SCSC_WLAN_ACM_API),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += acm_api.o
endif

# ----------------------------------------------------------------------------
# Building for Hardware
# ----------------------------------------------------------------------------
scsc_wlan-$(CONFIG_SCSC_WLAN) += hip.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += cm_if.o

ifeq ($(CONFIG_SCSC_MXLOGGER), m)
ccflags-y += -DCONFIG_SCSC_MXLOGGER
endif

ifeq ($(CONFIG_SCSC_LOG_COLLECTION), m)
ccflags-y += -DCONFIG_SCSC_LOG_COLLECTION
endif

ifeq ($(CONFIG_SCSC_WLAN_KIC_OPS),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += kic.o
endif
ccflags-$(CONFIG_SCSC_WLAN_KIC_OPS) += -DCONFIG_SCSC_WLAN_KIC_OPS

ifeq ($(CONFIG_SCSC_WLAN_GSCAN_ENABLE),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += nl80211_vendor.o
ccflags-y += -DCONFIG_SCSC_WLAN_GSCAN_ENABLE
endif
ifeq ($(CONFIG_SCSC_WIFI_NAN_ENABLE),y)
scsc_wlan-$(CONFIG_SCSC_WLAN) += nl80211_vendor_nan.o
scsc_wlan-$(CONFIG_SCSC_WLAN) += mlme_nan.o
endif

ifeq ($(CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD),y)
ccflags-y += -DCONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
endif

# ----------------------------------------------------------------------------
# Wlan configuration
# ----------------------------------------------------------------------------
ccflags-$(CONFIG_SCSC_WLAN_DEBUG)                                         += -DCONFIG_SCSC_WLAN_DEBUG
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TRACE)                                 += -DCONFIG_SCSC_WLAN_OFFLINE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE)              += -DCONFIG_SCSC_WLAN_OFFLINE_DATA_PLANE_PROFILE_TRACE
ccflags-$(CONFIG_SCSC_WLAN_OFFLINE_TX_TRACE)                              += -DCONFIG_SCSC_WLAN_OFFLINE_TX_TRACE
ccflags-$(CONFIG_SCSC_WLAN_HIP_PSCHED_AMSDU)                              += -DCONFIG_SCSC_WLAN_HIP_PSCHED_AMSDU
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI)                                       += -DCONFIG_SCSC_WLAN_RX_NAPI
ccflags-$(CONFIG_SCSC_WLAN_RX_NAPI_GRO)                                   += -DCONFIG_SCSC_WLAN_RX_NAPI_GRO
ccflags-$(CONFIG_SCSC_WLAN_HIP_SUPPORT_SCATTER_GATHER_API)                += -DCONFIG_SCSC_WLAN_HIP_SUPPORT_SCATTER_GATHER_API
ccflags-$(CONFIG_SCSC_WLAN_WES_NCHO)                                      += -DCONFIG_SCSC_WLAN_WES_NCHO
ccflags-$(CONFIG_SCSC_WLAN_MUTEX_DEBUG)                                   += -DCONFIG_SCSC_WLAN_MUTEX_DEBUG
ccflags-$(CONFIG_SCSC_WLAN_DISABLE_NAT_KA)				  += -DCONFIG_SCSC_WLAN_DISABLE_NAT_KA

ccflags-y += $(CONFIG_SAMSUNG_MAXWELL_EXTRA)
ccflags-y += -DCONFIG_SCSC_WLAN_MAX_INTERFACES=$(CONFIG_SCSC_WLAN_MAX_INTERFACES)

# Android specific build options
ccflags-$(CONFIG_SCSC_WLAN_ANDROID) += -DCONFIG_SCSC_WLAN_ANDROID

# SOC specific build options
ifeq ($(CONFIG_SOC_S5E5515),y)
ccflags-y += -DCONFIG_SCSC_WLAN_DELAYED_SCHED_SCAN
endif

ccflags-y += -Idrivers/misc/samsung/scsc
# Android Platform version
ifneq ($(ANDROID_VERSION),)
ANDROID_PLATFORM_VERSION=$(shell expr $(ANDROID_VERSION) / 10000)
ccflags-y += -DSCSC_SEP_VERSION=$(ANDROID_PLATFORM_VERSION)
$(info ANDROID_VERSION is $(ANDROID_VERSION))
else
ifneq ($(PLATFORM_VERSION),)
ANDROID_PLATFORM_VERSION := $(basename $(PLATFORM_VERSION))
ccflags-y += -DSCSC_SEP_VERSION=$(ANDROID_PLATFORM_VERSION)
$(info PLATFORM_VERSION is $(PLATFORM_VERSION))
endif
endif
$(info ANDROID_PLATFORM_VERSION is $(ANDROID_PLATFORM_VERSION))


ifneq ($(ANDROID_PLATFORM_VERSION),)
    $(info Validating config)
    ifeq ($(CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER),y)
    $(error Invalid config CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER)
    endif
    ifeq ($(CONFIG_SCSC_ENHANCED_PACKET_STATS),y)
    $(error Invalid config CONFIG_SCSC_ENHANCED_PACKET_STATS)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_LOW_LATENCY_MODE),y)
    $(error Invalid config CONFIG_SCSC_WLAN_LOW_LATENCY_MODE)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT),y)
    $(error Invalid config CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_DUAL_STATION),y)
    $(error Invalid config CONFIG_SCSC_WLAN_DUAL_STATION)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_DYNAMIC_ITO),y)
    $(error Invalid config CONFIG_SCSC_WLAN_DYNAMIC_ITO)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC),y)
    $(error Invalid config CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_EXPONENTIAL_SCHED_SCAN),y)
    $(error Invalid config CONFIG_SCSC_WLAN_EXPONENTIAL_SCHED_SCAN)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_SAR_SUPPORTED),y)
    $(error Invalid config CONFIG_SCSC_WLAN_SAR_SUPPORTED)
    endif
    ifeq ($(CONFIG_SCSC_WLAN_MAX_LINK_SPEED),y)
    $(error Invalid config CONFIG_SCSC_WLAN_MAX_LINK_SPEED)
    endif
endif

## See drivers/misc/samsung/scsc/ Makefile for an explanation of the reasons
## of the following ifeq/else
ifeq ($(CONFIG_SCSC_LOGRING), m)
ccflags-y		 						+= -DCONFIG_SCSC_PRINTK
else
ccflags-$(CONFIG_SCSC_LOGRING) 						+= -DCONFIG_SCSC_PRINTK
endif

ifeq ($(CONFIG_SCSC_PCIE), m)
ccflags-y += -DCONFIG_SCSC_PCIE
endif
