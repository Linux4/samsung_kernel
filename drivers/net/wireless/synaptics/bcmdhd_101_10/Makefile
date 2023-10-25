#
# Copyright (C) 2022, Broadcom.
#
#      Unless you and Broadcom execute a separate written software license
# agreement governing use of this software, this software is licensed to you
# under the terms of the GNU General Public License version 2 (the "GPL"),
# available at http://www.broadcom.com/licenses/GPLv2.php, with the
# following added to such license:
#
#      As a special exception, the copyright holders of this software give you
# permission to link this software with independent modules, and to copy and
# distribute the resulting executable under terms of your choice, provided that
# you also meet, for each linked independent module, the terms and conditions of
# the license of that module.  An independent module is a module which is not
# derived from this software.  The special exception does not apply to any
# modifications of the software.
#
#
# <<Broadcom-WL-IPTag/Open:>>
#
# dhd

# Path to the module source
M ?= $(shell pwd)

ifeq ($(KERNEL_SRC),)
  LINUXDIR ?= $(KDIR)
  ifneq ($(LINUXDIR),)
    KERNEL_SRC ?= $(LINUXDIR)
  endif
  BCMDHD_ROOT=$(srctree)/$(src)
else
 KBUILD_OPTIONS += BCMDHD_ROOT=$(shell cd $(KERNEL_SRC); readlink -e $(M))
endif # KERNEL_SRC

#####################
# Basic feature
#####################

DHDCFLAGS += -DBCMUTILS_ERR_CODES -DUSE_NEW_RSPEC_DEFS
DHDCFLAGS += -Dlinux -DLINUX -DBCMDRIVER      \
        -DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE      \
        -DDHDTHREAD -DDHD_BCMEVENTS -DSHOW_EVENTS -DWLP2P      \
        -DWIFI_ACT_FRAME -DKEEP_ALIVE -DCSCAN     \
        -DPKT_FILTER_SUPPORT -DEMBEDDED_PLATFORM -DPNO_SUPPORT          \
        -DSEC_ENHANCEMENT           \

#####################
# Default settings
#####################
CONFIG_BCM43456 ?= y
CONFIG_CHIPSET ?= BCM43456
CONFIG_BCMDHD_SDIO_SDMMC ?= y
CONFIG_WPA3_SUPPORT ?= y
CONFIG_BCMDHD_DEBUG ?= y
CONFIG_BCMDHD_LOG ?= y
CONFIG_WLAN_VENDOR_SYNAPTICS=y

CONFIG_BUILD_CFG80211 ?= y
# 0:SDPCM_TXGLOM_CPY(copy)
# 1:SDPCM_TXGLOM_MDESC(ADMA)
CONFIG_BCMDHD_SDIO_TXGLOM_MANNER ?= 1
CONFIG_DHD_USE_STATIC_BUF ?= y
CONFIG_BCMDHD_PREALLOC_PKTIDMAP ?= y
CONFIG_BCMDHD_PREALLOC_MEMDUMP ?= y
CONFIG_BROADCOM_WIFI_RESERVED_MEM ?= y

# For inbuilt module, below configs will be provided via defconfig
# But for out-of-tree module, explicitly define them here and add
# them as cflags
ifeq ($(CONFIG_BCMDHD),)
CONFIG_BCMDHD=y
CONFIG_DHD_OF_SUPPORT ?= y
CONFIG_WIFI_BROADCOM_COB ?= y
CONFIG_BCMDHD_OOB_HOST_WAKE ?= y
CONFIG_BCMDHD_FW_PATH="\"bcmdhd_sta.bin\""
CONFIG_BCMDHD_NVRAM_PATH="\"nvram_net.txt\""
CONFIG_BCMDHD_CLM_PATH="\"bcmdhd_clm.blob\""
CONFIG_BCMDHD_CONFIG_PATH="\"wlan_config.ini\""
CONFIG_DHD_USE_SCHED_SCAN=y
CONFIG_BCMDHD_GET_OOB_STATE=y

DHDCFLAGS += -DCONFIG_BCMDHD=$(CONFIG_BCMDHD)
DHDCFLAGS += -DCONFIG_DHD_OF_SUPPORT=$(CONFIG_DHD_OF_SUPPORT)
DHDCFLAGS += -DCONFIG_BCMDHD_FW_PATH=$(CONFIG_BCMDHD_FW_PATH)
DHDCFLAGS += -DCONFIG_BCMDHD_NVRAM_PATH=$(CONFIG_BCMDHD_NVRAM_PATH)
DHDCFLAGS += -DCONFIG_BCMDHD_CLM_PATH=$(CONFIG_BCMDHD_CLM_PATH)
DHDCFLAGS += -DCONFIG_BROADCOM_WIFI_RESERVED_MEM=$(CONFIG_BROADCOM_WIFI_RESERVED_MEM)
DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF=$(CONFIG_DHD_USE_STATIC_BUF)
DHDCFLAGS += -DCONFIG_DHD_USE_SCHED_SCAN=$(CONFIG_DHD_USE_SCHED_SCAN)
DHDCFLAGS += -DCONFIG_BCMDHD_PREALLOC_PKTIDMAP=$(CONFIG_BCMDHD_PREALLOC_PKTIDMAP)
DHDCFLAGS += -DCONFIG_BCMDHD_PREALLOC_MEMDUMP=$(CONFIG_BCMDHD_PREALLOC_MEMDUMP)
DHDCFLAGS += -DCONFIG_BCMDHD_OOB_HOST_WAKE=$(CONFIG_BCMDHD_OOB_HOST_WAKE)
DHDCFLAGS += -DCONFIG_BCMDHD_GET_OOB_STATE=$(CONFIG_BCMDHD_GET_OOB_STATE)
DHDCFLAGS += -DCONFIG_WIFI_BROADCOM_COB=$(CONFIG_WIFI_BROADCOM_COB)
endif


#####################
# Bus Interface Type
#####################
ifneq ($(CONFIG_BCMDHD_PCIE),)
	BUS_IFACE_PCIE=y
else
	BUS_IFACE_SDIO=y
endif

#####################
# SDIO I/F
#####################
ifeq ($(BUS_IFACE_SDIO),y)
	DHDCFLAGS += -DBDC -DDHD_BCMEVENTS -DMMC_SDIO_ABORT
	DHDCFLAGS += -DBCMSDIO -DBCMLXSDMMC -DUSE_SDIOFIFO_IOVAR
	DHDCFLAGS += -U__ARM_ARCH_7A__
	# DPC priority
	DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=98
	# idle count
	DHDCFLAGS += -DDHD_USE_IDLECOUNT
	# SKB TAILPAD to avoid out of boundary memory access
	DHDCFLAGS += -DDHDENABLE_TAILPAD
	# Debug for DPC Thread watchdog bark
	DHDCFLAGS += -DDEBUG_DPC_THREAD_WATCHDOG
	# KSO Attempts count
	DHDCFLAGS += -DCUSTOM_MAX_KSO_ATTEMPTS=64
ifeq ($(CONFIG_BCMDHD_OOB_HOST_WAKE),y)
	DHDCFLAGS += -DOOB_INTR_ONLY
	DHDCFLAGS += -DHW_OOB
ifeq ($(CONFIG_BCMDHD_DISABLE_WOWLAN),y)
	DHDCFLAGS += -DDISABLE_WOWLAN
endif # CONFIG_BCMDHD_DISABLE_WOWLAN
else
ifeq ($(CONFIG_BCMDHD_SDIO_SDMMC),y)
       	DHDCFLAGS += -DSDIO_ISR_THREAD
endif # CONFIG_BCMDHD_SDIO_SDMMC
endif # CONFIG_BCMDHD_OOB

	# tput enhancement
	DHDCFLAGS += -DCUSTOM_GLOM_SETTING=8 -DCUSTOM_RXCHAIN=0
	DHDCFLAGS += -DCUSTOM_SDIO_F2_BLKSIZE=256
	DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=256
	DHDCFLAGS += -DBCMSDIOH_TXGLOM
	DHDCFLAGS += -DCUSTOM_TXGLOM=$(CONFIG_BCMDHD_SDIO_TXGLOM_MANNER)
	DHDCFLAGS += -DRXFRAME_THREAD
	DHDCFLAGS += -DREPEAT_READFRAME
	DHDCFLAGS += -DCUSTOM_MAX_TXGLOM_SIZE=64
	DHDCFLAGS += -DWLFC_STATE_PREALLOC
	DHDCFLAGS += -DDHD_TXBOUND=72
	DHDCFLAGS += -DDHD_RXBOUND=256
	# Using default, 32
	DHDCFLAGS += -DDHD_SDALIGN=4
	DHDCFLAGS += -DCUSTOM_TXINRX_THRES=128

	DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
endif

#####################
# PCIE I/F
#####################
ifeq ($(BUS_IFACE_PCIE),y)
	DHDCFLAGS += -DPCIE_FULL_DONGLE -DBCMPCIE
	# Use spin_lock_bh locks
	DHDCFLAGS += -DDHD_USE_SPIN_LOCK_BH
	# Use the external WLAN_HOST_WAKE pin to wake up host
ifeq ($(CONFIG_BCMDHD_OOB_HOST_WAKE),y)
	DHDCFLAGS += -DBCMPCIE_OOB_HOST_WAKE
endif
	# Use threaded IRQ for OOB
	DHDCFLAGS += -DDHD_USE_PCIE_OOB_THREADED_IRQ
	# DPC priority
	DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=-1
	# Set D3_ACK response timeout value
	DHDCFLAGS += -DD3_ACK_RESP_TIMEOUT=4000
	# Use Coherent memory for ring
	DHDCFLAGS += -DDHD_USE_COHERENT_MEM_FOR_RING
	# Alloc Coherent memory from DMA coherent pool
	DHDCFLAGS += -DDHD_ALLOC_COHERENT_MEM_FROM_ATOMIC_POOL
ifeq ($(filter y,$(CONFIG_ARCH_KONA) $(CONFIG_ARCH_LAHAINA) \
		$(CONFIG_SOC_EXYNOS9830) $(CONFIG_SOC_EXYNOS2100) $(CONFIG_SOC_EXYNOS1000)),y)
	# Alloc Coherent memory with GFP_COMP flag
	DHDCFLAGS += -DDHD_ALLOC_COHERENT_MEM_WITH_GFP_COMP
endif
	# Enable Link down recovery
	DHDCFLAGS += -DSUPPORT_LINKDOWN_RECOVERY
	# Enable Dongle Isolation
	DHDCFLAGS += -DDONGLE_ENABLE_ISOLATION
	# Enable Firmware Coredump
	DHDCFLAGS += -DDHD_FW_COREDUMP
	DHDCFLAGS += -DDHD_DUMP_FILE_WRITE_FROM_KERNEL
        # Enable PKTID AUDIT
        DHDCFLAGS += -DDHD_PKTID_AUDIT_ENABLED
	# Enable Load Balancing support by default.
	# DHD_LB_RXP - Perform RX Packet processing in parallel
	# DHD_LB_STATS - To display the Load Blancing statistics
	DHDCFLAGS += -DDHD_LB -DDHD_LB_RXP -DDHD_LB_TXP -DDHD_LB_STATS
	DHDCFLAGS += -DWAKEUP_KSOFTIRQD_POST_NAPI_SCHEDULE
	# GRO (Generic Receive Offload) feature
	DHDCFLAGS += -DENABLE_DHD_GRO
	DHDCFLAGS += -DDHD_GRO_ENABLE_HOST_CTRL
ifeq ($(filter y,$(CONFIG_SOC_EXYNOS8895) $(CONFIG_SOC_EXYNOS9810) \
		$(CONFIG_ARCH_SDM845) $(CONFIG_SOC_EXYNOS9820) \
		$(CONFIG_SOC_EXYNOS9830) $(CONFIG_SOC_EXYNOS2100) $(CONFIG_SOC_EXYNOS1000)),y)
	# Default Tx LB Enable
	DHDCFLAGS += -DDHD_LB_TXP_DEFAULT_ENAB
endif
	DHDCFLAGS += -DDHD_RECOVER_TIMEOUT
	# Enable health check event handling
	DHDCFLAGS += -DDNGL_EVENT_SUPPORT
	DHDCFLAGS += -DHCHK_COMMON_SW_EVENT
	# PCIe interrupt affinity control interface
	DHDCFLAGS += -DSET_PCIE_IRQ_CPU_CORE
	# ROT and Scan timeout debugging due to Kernel scheduling problem
	DHDCFLAGS += -DDHD_KERNEL_SCHED_DEBUG
	# Disable watchdog thread
	DHDCFLAGS += -DCUSTOM_DHD_WATCHDOG_MS=0
	# Runtime PM
	DHDCFLAGS += -DDHD_PCIE_RUNTIMEPM
	DHDCFLAGS += -DCHECK_TRAP_ROT
	# Enable host SFH LLC insertion in Tx pkts
	DHDCFLAGS += -DHOST_SFH_LLC
 	 # HEAP ASLR
  	DHDCFLAGS += -DBCM_ASLR_HEAP
  	DHDCFLAGS += -DMAX_CNTL_TX_TIMEOUT=1
ifneq ($(filter y, $(CONFIG_ARCH_MSM) $(CONFIG_ARCH_QCOM)),)
    DHDCFLAGS += -DMSM_PCIE_LINKDOWN_RECOVERY
endif # CONFIG_ARCH_MSM || CONFIG_ARCH_QCOM
ifeq ($(CONFIG_DHD_USE_STATIC_BUF),y)
    	DHDCFLAGS += -DDHD_USE_STATIC_IOCTLBUF
endif
endif # CONFIG_BCMDHD_PCIE

##### modular driver may fail to start up if use static
ifeq ($(CONFIG_BCMDHD),m) # for modular DHD driver
  # skip static buf
  CONFIG_DHD_USE_STATIC_BUF=
endif # for modular DHD driver

### Derive Linux version
LINUXSRC := $(LINUXDIR)
### Try to fix for not local build
ifneq ($(wildcard $(LINUXSRC)/source), )
  LINUXSRC := $(LINUXDIR)/source
endif
ifneq ($(LINUXSRC),)
  VERSION = $(shell grep "^VERSION =" $(LINUXSRC)/Makefile | cut -d " " -f 3)
  PATCHLEVEL = $(shell grep "^PATCHLEVEL =" $(LINUXSRC)/Makefile | cut -d " " -f 3)
  SUBLEVEL = $(shell grep "^SUBLEVEL =" $(LINUXSRC)/Makefile | cut -d " " -f 3)
endif # LINUXSRC
BCM_KVERSIONSTRING := $(VERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL)$(if $(SUBLEVEL),.$(SUBLEVEL)))

### Compares two dotted numeric strings (e.g 2.3.16.1) for $1 >= $2
define wlan_version_ge
$(findstring TRUE,$(shell bash -c 'sort -cu -t. -k1,1nr -k2,2nr -k3,3nr -k4,4nr <(echo -e "$2\n$1") 2>&1 || echo TRUE'))
endef # wlan_version_ge

##### SDIO/PCIE Basic feature
WFLAGS := -Wall \
    -Wstrict-prototypes \
    -Werror \
    -Wno-main \
    -Wno-format-zero-length \
    -Wswitch \
    -Wno-sign-compare \
    -Wno-pointer-sign \
    -Wno-attributes \
    -Wno-unused-parameter \
    -fno-strict-aliasing \
    -Woverride-init \
    -Wtype-limits \
    -Wuninitialized \
    -Wunused-variable \
    -DSTRICT_GCC_WARNINGS

ifeq ($(call wlan_version_ge,$(GCCVER),8.0),TRUE)
WFLAGS += \
    -Werror=sizeof-pointer-memaccess \
    -Werror=stringop-overflow \
    -Werror=format
endif # ($(call wlan_version_ge,$(GCCVER),8.0),TRUE)

ifeq ($(call wlan_version_ge,$(GCCVER),5.0),TRUE)
WFLAGS_EXTRA_A := \
    -Wclobbered \
    -Wmissing-parameter-type \
    -Wold-style-declaration \
    -Wunused-but-set-variable \
    -Wmaybe-uninitialized
endif # ($(call wlan_version_ge,$(GCCVER),5.0),TRUE)

WFLAGS_EXTRA_B := \
    -Wmissing-field-initializers

# more stric syntax checking accordingly
ifneq ($(CC),clang)
  WFLAGS += ${WFLAGS_EXTRA_A}
endif # CC != clang

ifeq ($(CONFIG_ARCH_HISI),y)
  WFLAGS += ${WFLAGS_EXTRA_B}
endif # CONFIG_ARCH_HISI = y


#################
# Common feature
#################
DHDCFLAGS += ${WFLAGS}

DHDCFLAGS += -DCUSTOMER_HW4 -DCUSTOMER_HW4_DEBUG -DCUSTOMER_HW4_PRIVATE_CMD

DHDCFLAGS += -DWL_CFG80211

# DMA64 suppports on 64bit Architecture
ifeq ($(CONFIG_64BIT),y)
DHDCFLAGS := $(filter-out -DBCMDMA32,$(DHDCFLAGS))
DHDCFLAGS += -DBCMDMA64OSL
endif

# Debug
DHDCFLAGS += -DSIMPLE_MAC_PRINT
# Enable wakelock for legacy scan
DHDCFLAGS += -DDHD_USE_SCAN_WAKELOCK
# Enable wakelock debug function
DHDCFLAGS += -DDHD_TRACE_WAKE_LOCK
# Print out kernel panic point of file and line info when assertion happened
DHDCFLAGS += -DBCMASSERT_LOG
# Collect Current Assoc/Reject Status for Big Data logging
DHDCFLAGS += -DDHD_ENABLE_BIGDATA_LOGGING
# Collect roaming related information for Big Data logging
DHDCFLAGS += -DDHD_PUB_ROAM_EVT

# Enable SI utility error log
DHDCFLAGS += -DSI_ERROR_ENFORCE

# Enable SBN feature
DHDCFLAGS += -DDHD_SBN

# Enable inband device wake feature
DHDCFLAGS += -DPCIE_INB_DW

# Print 8021X
DHDCFLAGS += -DDHD_8021X_DUMP

# Print DHCP
DHDCFLAGS += -DDHD_DHCP_DUMP

# Print ICMP
DHDCFLAGS += -DDHD_ICMP_DUMP

# Print ARP
DHDCFLAGS += -DDHD_ARP_DUMP

# Print DNS
DHDCFLAGS += -DDHD_DNS_DUMP

# Enable Firmware Coredump
DHDCFLAGS += -DDHD_FW_COREDUMP

# Enable Log Dump
DHDCFLAGS += -DDHD_LOG_DUMP
DHDCFLAGS += -DDHD_PKT_LOGGING

##### optional feature
DHDCFLAGS += -DDHD_SUPPORT_VFS_CALL
DHDCFLAGS += -DWL_VIRTUAL_APSTA
DHDCFLAGS += -DPOWERUP_MAX_RETRY=0
ifeq ($(CONFIG_ARCH_HISI),y)
  DHDCFLAGS += -DBOARD_HIKEY  # get memory dump
  DHDCFLAGS += -DSHOW_LOGTRACE -DDHD_FW_COREDUMP -DDHD_COREDUMP
  ### send up the event as skb packet and some WL command like 'escanresults' may need this
  DHDCFLAGS += -DWL_EVENT_ENAB
endif # CONFIG_ARCH_HISI = y

# Disable roaming
DHDCFLAGS += -DDISABLE_BUILTIN_ROAM

DHDCFLAGS += -DSUPPORT_SET_CAC

# Debugability
ifeq ($(CONFIG_BCMDHD_LOG),y)
  CONFIG_BCMDHD_DEBUG = y
endif # CONFIG_BCMDHD_LOG
ifeq ($(CONFIG_BCMDHD_DEBUG),y)
  DHDCFLAGS += -DDHD_DEBUG
  DHDCFLAGS += -DBCMDBG
  # Dongle init fail
  DHDCFLAGS += -DDEBUG_DNGL_INIT_FAIL
  DHDCFLAGS += -DDHD_FW_COREDUMP
  DHDCFLAGS += -DBCMDBG_MEM
  DHDCFLAGS += -DDHD_RND_DEBUG
  DHDCFLAGS += -DBCMPERFSTATS
  DHDCFLAGS += -DDNGL_EVENT_SUPPORT
  DHDCFLAGS += -DREPORT_FATAL_TIMEOUTS
  DHDCFLAGS += -DDEVICE_TX_STUCK_DETECT
  DHDCFLAGS += -DTX_STATUS_LATENCY_STATS
  DHDCFLAGS += -DDBG_RING_LOG_INIT_DEFAULT
  DHDCFLAGS += -DBCMDBG_ASSERT
  DHDCFLAGS += -DDBG_PKT_MON
  DHDCFLAGS += -DRSSI_MONITOR_SUPPORT
  DHDCFLAGS += -DSUPPORT_RSSI_SUM_REPORT
  DHDCFLAGS += -DSET_SSID_FAIL_CUSTOM_RC=100
ifeq ($(CONFIG_SEC_FACTORY),y)
  DHDCFLAGS += -DDHD_DETECT_CONSECUTIVE_MFG_HANG
  DHDCFLAGS += -DBCM_DETECT_TURN_ON_FAILURE
else
  DHDCFLAGS += -DHD_USE_WARN_RATHER_THAN_BUG_ON 
endif # CONFIG_SEC_FACTORY

  ifeq ($(CONFIG_BCMDHD_PCIE),y)
    DHDCFLAGS += -DDHD_TIMESYNC
  endif # CONFIG_BCMDHD_PCIE
  ifeq ($(CONFIG_BCMDHD_SDIO),y)
    DHDCFLAGS += -DDEBUGABILITY
  endif # CONFIG_BCMDHD_SDIO
  ifeq ($(CONFIG_BUILD_CFG80211),y)
    DHDCFLAGS += -DDEBUGFS_CFG80211
  endif # CONFIG_BUILD_CFG80211
  ifeq ($(CONFIG_FIB_RULES),y)
    # HAL File dump is supported only for iptable builds(brcm_wlan_iptables_defconfig)
    DHDCFLAGS += -DDHD_FILE_DUMP_EVENT
  endif
endif # CONFIG_BCMDHD_DEBUG

ifeq ($(CONFIG_BCMDHD_LOG),y)
  # Enable Log Dump
  DHDCFLAGS += -DDHD_LOG_DUMP
  DHDCFLAGS += -DSHOW_LOGTRACE
# CONTROL_LOGTRACE=1 to print as EL
  DHDCFLAGS += -DCUSTOM_CONTROL_LOGTRACE=1
  # coredump
  DHDCFLAGS += -DDHD_FW_COREDUMP
  #  depends on DHD_LOG_DUMP
  DHDCFLAGS += -DDHD_DUMP_FILE_WRITE_FROM_KERNEL
  # Enable log print rate limit
  DHDCFLAGS += -DDHD_LOG_PRINT_RATE_LIMIT
  # Logging
  DHDCFLAGS += -DDHD_8021X_DUMP
  DHDCFLAGS += -DDHD_DHCP_DUMP
  DHDCFLAGS += -DDHD_ICMP_DUMP
  DHDCFLAGS += -DDHD_ARP_DUMP
  DHDCFLAGS += -DDHD_DNS_DUMP
  DHDCFLAGS += -DDHD_IFDEBUG
  DHDCFLAGS += -DCUSTOM_LOG_DUMP_BUFSIZE_MB=1
  DHDCFLAGS += -DDHD_DBG_DUMP
  # Print out kernel panic point of file and line info when assertion happened
  DHDCFLAGS += -DBCMASSERT_LOG
  # Enable SI utility error log
  DHDCFLAGS += -DSI_ERROR_ENFORCE
  ifeq ($(CONFIG_BCMDHD_PCIE),y)
    # Enable SSSR Dump
    DHDCFLAGS += -DDHD_SSSR_DUMP
    # Enable SMD/Minidump collection
    DHDCFLAGS += -DD2H_MINIDUMP
    # ROT and Scan timeout debugging due to Kernel scheduling problem
    DHDCFLAGS += -DDHD_KERNEL_SCHED_DEBUG
    # EDL
    DHDCFLAGS += -DEWP_EDL
    DHDCFLAGS += -DEWP_ETD_PRSRV_LOGS
    # EWP
    DHDCFLAGS += -DDHD_PCIE_REG_ACCESS
    DHDCFLAGS += -DEWP_ECNTRS_LOGGING
    DHDCFLAGS += -DEWP_RTT_LOGGING
    DHDCFLAGS += -DDHD_HMAPTEST
    DHDCFLAGS += -DEVENT_LOG_RATE_HC
    DHDCFLAGS += -DCONFIG_BCMDHD_PREALLOC_PKTIDMAP
    DHDCFLAGS += -DCONFIG_BCMDHD_PREALLOC_MEMDUMP
    DHDCFLAGS += -DCONFIG_BCMDHD_GET_OOB_STATE
  endif # CONFIG_BCMDHD_PCIE
endif # CONFIG_BCMDHD_LOG

# turn on below key features supported
DHDCFLAGS += -DDHD_MSG_LEVEL_SUPPORT
DHDCFLAGS += -DDHD_FW_LOG_SUPPORT
DHDCFLAGS += -DDHD_FW_CONS_SUPPORT

# for debug trap
DHDCFLAGS += -DWL_CFGVENDOR_SEND_HANG_EVENT

ifeq ($(CONFIG_FIB_RULES),y)
  # HAL File dump is supported only for iptable builds(brcm_wlan_iptables_defconfig)
  DHDCFLAGS := $(filter-out -DDHD_DUMP_FILE_WRITE_FROM_KERNEL ,$(DHDCFLAGS))
endif

# Enable SBN feature
DHDCFLAGS += -DDHD_SBN

# customer configuration file
DHDCFLAGS += -DREAD_CONFIG_FROM_FILE

# FBT
DHDCFLAGS += -DWLFBT

# OKC
DHDCFLAGS += -DOKC_SUPPORT

# CFG80211
ifeq ($(CONFIG_BUILD_CFG80211),y)
  DHDCFLAGS += -DWL_CFG80211
  ifeq ($(CONFIG_CFG80211_FILS_BKPORT),y)
    DHDCFLAGS += -DWL_FILS
  endif # CONFIG_CFG80211_FILS_BKPORT

  # Android iface management
  DHDCFLAGS += -DWL_IFACE_MGMT

  # for WPA3
  ifeq ($(CONFIG_WPA3_SUPPORT),y)
    # old kernel needs back port support
    ifneq ($(call wlan_version_gt,$(BCM_KVERSIONSTRING),4.16),TRUE)
    CONFIG_CFG80211_SAE_BKPORT ?= y
    endif # ($(call wlan_version_gt,$(BCM_KVERSIONSTRING),4.16),TRUE)

    # WPA3 external authenticator
    ### for external supplicant (only kernel > 4.16 support external authenticator)
    ifeq ($(call wlan_version_gt,$(BCM_KVERSIONSTRING),4.16),TRUE)
    DHDCFLAGS += -DWL_CLIENT_SAE
    endif # ($(call wlan_version_gt,$(BCM_KVERSIONSTRING),4.16),TRUE)
    ### for dongle in-driver-supplicant
    # DHDCFLAGS += -DWL_SAE
  endif # CONFIG_WPA3_SUPPORT

  ifeq ($(CONFIG_CFG80211_SCANTYPE_BKPORT),y)
    DHDCFLAGS += -DWL_SCAN_TYPE
  endif # CONFIG_CFG80211_SCANTYPE_BKPORT
endif # CONFIG_BUILD_CFG80211

ifeq ($(CONFIG_BCMDHD_PCIE),y)
  # Enable Load Balancing support by default.
  # DHD_LB_RXP - Perform RX Packet processing in parallel
  # DHD_LB_STATS - To display the Load Blancing statistics
  DHDCFLAGS += -DDHD_LB -DDHD_LB_RXP -DDHD_LB_TXP -DDHD_LB_STATS
  # Use spin_lock_bh locks
  DHDCFLAGS += -DDHD_USE_SPIN_LOCK_BH
  # Enable CTO Recovery
  DHDCFLAGS += -DBCMPCIE_CTO_PREVENTION
  # Enable inband device wake feature
  DHDCFLAGS += -DPCIE_INB_DW

  # IRQSET in DHD
  ifeq ($(CONFIG_BCMDHD),y) # only turn on when built-in since it requires kernel non-export '__irq_set_affinity'
    DHDCFLAGS += -DDHD_LB_IRQSET
  endif # CONFIG_BCMDHD
  DHDCFLAGS += -DDHD_MSI_SUPPORT
  # Enable PKTID AUDIT
  DHDCFLAGS += -DDHD_PKTID_AUDIT_ENABLED
  DHDCFLAGS += -DIDLE_TX_FLOW_MGMT
endif # CONFIG_BCMDHD_PCIE

# CUSTOMER flags
ifneq ($(CONFIG_BCM43456),)
DHDCFLAGS += -DNUM_EVENT_LOG_SETS=8
# Random scan
DHDCFLAGS += -DSUPPORT_RANDOM_MAC_SCAN
DHDCFLAGS += -DWL_USE_RANDOMIZED_SCAN
else
# 43456 is Beamformee only
DHDCFLAGS += -DUSE_WL_TXBF
DHDCFLAGS += -DWBTEXT_BTMDELTA=0
# Disabled in FW
DHDCFLAGS += -DDISABLE_PM_BCNRX
# TODO Add later
DHDCFLAGS += -DENABLE_IPMCAST_FILTER
endif

##############################################################
# dhd_sec_feature.h
DHDCFLAGS += -include "dhd_sec_feature.h"
##############################################################

# Basic / Common Feature
DHDCFLAGS += -DDHDTCPACK_SUPPRESS
DHDCFLAGS += -DUSE_WL_FRAMEBURST
DHDCFLAGS += -DCUSTOM_FRAMEBURST_SET=1
DHDCFLAGS += -DWL_CFG80211_STA_EVENT
DHDCFLAGS += -DWL11U
DHDCFLAGS += -DMFP

#use customer country code mapping table
DHDCFLAGS += -DCUSTOM_COUNTRY_CODE

# Android Feature
DHDCFLAGS += -DOEM_ANDROID
# Android iface management
DHDCFLAGS += -DWL_IFACE_MGMT

DHDCFLAGS += -DGET_CUSTOM_MAC_ENABLE
CONFIG_DHD_SET_RANDOM_MAC_VAL ?= 0x001A11
DHDCFLAGS += -DPNO_SUPPORT
# Gscan needs PNO
DHDCFLAGS += -DGSCAN_SUPPORT
DHDCFLAGS += -DAPF
DHDCFLAGS += -DDHD_GET_VALID_CHANNELS
DHDCFLAGS += -DLINKSTAT_SUPPORT
DHDCFLAGS += -DPFN_SCANRESULT_2
DHDCFLAGS += -DWL_IFACE_COMB_NUM_CHANNELS
#FAKEAP
DHDCFLAGS += -DWL_BCNRECV
#ADDTS
DHDCFLAGS += -DWL_CAC_TS

# MAC
DHDCFLAGS += -DENABLE_ARP_SNOOP_MODE
# Phy / System
DHDCFLAGS += -DDHD_ENABLE_LPC
DHDCFLAGS += -DFCC_PWR_LIMIT_2G
DHDCFLAGS += -DSUPPORT_2G_VHT
DHDCFLAGS += -DSUPPORT_5G_1024QAM_VHT
DHDCFLAGS += -DSUPPORT_LTECX
DHDCFLAGS += -DSUPPORT_LQCM
DHDCFLAGS += -DSUPPORT_SET_CAC
DHDCFLAGS += -DSUPPORT_WL_TXPOWER
DHDCFLAGS += -DWBTEXT
DHDCFLAGS += -DWBTEXT_SCORE_V2
DHDCFLAGS += -DRRM_BCNREQ_MAX_CHAN_TIME=12
DHDCFLAGS += -DWL_LASTEVT
# Wake
DHDCFLAGS += -DCONFIG_HAS_WAKELOCK
# Enable wakelock debug function
DHDCFLAGS += -DDHD_TRACE_WAKE_LOCK
DHDCFLAGS += -DDHD_WAKE_EVENT_STATUS
DHDCFLAGS += -DDHD_WAKE_RX_STATUS
#DHDCFLAGS += -DDHD_USE_SCAN_WAKELOCK
#Latency Mode
DHDCFLAGS += -DWL_LATENCY_MODE

#Android Q
#Soft AP MAC randomization
DHDCFLAGS += -DWL_SOFTAP_RAND
#p2p MAC randomization
DHDCFLAGS += -DWL_P2P_RAND
DHDCFLAGS += -DWL_STA_INIT_RAND

DHDCFLAGS += -DDHD_WAKE_STATUS
DHDCFLAGS += -DCUSTOM_WAKE_REASON_STATS
DHDCFLAGS += -DWIFI_TURNON_USE_HALINIT

ifeq ($(CONFIG_BCMDHD_PCIE),y)
  DHDCFLAGS += -DDHD_WAKE_EVENT_STATUS
  DHDCFLAGS += -DDHD_WAKE_RX_STATUS
endif # CONFIG_BCMDHD_PCIE
DHDCFLAGS += -DENABLE_BCN_LI_BCN_WAKEUP
# Logging
DHDCFLAGS += -DBCMASSERT_LOG
DHDCFLAGS += -DDHD_8021X_DUMP
DHDCFLAGS += -DDHD_DHCP_DUMP
#DHDCFLAGS += -DDHD_ICMP_DUMP
#DHDCFLAGS += -DDHD_ARP_DUMP
DHDCFLAGS += -DDHD_DNS_DUMP
DHDCFLAGS += -DDHD_PKTDUMP_ROAM
DHDCFLAGS += -DRSSI_MONITOR_SUPPORT
DHDCFLAGS += -DSUPPORT_RSSI_SUM_REPORT
DHDCFLAGS += -DSET_SSID_FAIL_CUSTOM_RC=100
# Packet
#DHDCFLAGS += -DBLOCK_IPV6_PACKET
#DHDCFLAGS += -DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT # NAN test failure
DHDCFLAGS += -DPASS_ALL_MCAST_PKTS
DHDCFLAGS += -DPKTPRIO_OVERRIDE

ifeq ($(CONFIG_BCMDHD_PCIE),y)
  # Kernel/Platform Related Feature
  #DHDCFLAGS += -DDHD_LB_TXP_DEFAULT_ENAB # Not needed for Brix
  #DHDCFLAGS += -DDHD_RECOVER_TIMEOUT # Not needed for Brix
  #DHDCFLAGS += -DDHD_USE_ATOMIC_PKTGET # Not needed for Brix
  #DHDCFLAGS += -DDHD_USE_COHERENT_MEM_FOR_RING # Not needed for Brix
  #DHDCFLAGS += -DDONGLE_ENABLE_ISOLATION # Not compatible with Brix platform
  #DHDCFLAGS += -DKEEP_WIFION_OPTION # Not compatible with Brix platform
endif # CONFIG_BCMDHD_PCIE

DHDCFLAGS += -DWL_SUPPORT_BACKPORTED_KPATCHES

# SCAN
DHDCFLAGS += -DCUSTOMER_SCAN_TIMEOUT_SETTING
DHDCFLAGS += -DDISABLE_PRUNED_SCAN
DHDCFLAGS += -DESCAN_BUF_OVERFLOW_MGMT
DHDCFLAGS += -DUSE_INITIAL_SHORT_DWELL_TIME
DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
DHDCFLAGS += -DCUSTOM_SCAN_PASSIVE_TIME=110
DHDCFLAGS += -DCUSTOM_FORCE_NODFS_FLAG    #SCAN time

# Suspend/Resume
DHDCFLAGS += -DENABLE_MAX_DTIM_IN_SUSPEND
DHDCFLAGS += -DSUPPORT_DEEP_SLEEP
# Misc Features
DHDCFLAGS += -DDHD_BLOB_EXISTENCE_CHECK
DHDCFLAGS += -DSUPPORT_AMPDU_MPDU_CMD
DHDCFLAGS += -DWL_RELMCAST
DHDCFLAGS += -DWL_SUPP_EVENT
DHDCFLAGS += -DFILTER_IE
DHDCFLAGS += -DCUSTOM_LONG_RETRY_LIMIT=12
# Kind of WAR
DHDCFLAGS += -DENABLE_TDLS_AUTO_MODE
DHDCFLAGS += -DEXPLICIT_DISCIF_CLEANUP
DHDCFLAGS += -DSKIP_WLFC_ON_CONCURRENT
DHDCFLAGS += -DCUSTOM_BLOCK_DEAUTH_AT_EAP_FAILURE
DHDCFLAGS += -DTDLS_MSG_ONLY_WFD
# Custom tuning value
DHDCFLAGS += -DCUSTOM_EVENT_PM_WAKE=30
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=30000
# For special PNO Event keep wake lock for 10sec
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=10
DHDCFLAGS += -DSPECIFIC_MAC_GEN_SCHEME
ifeq ($(CONFIG_DHD_SET_RANDOM_MAC_VAL),y)
  DHDCFLAGS += -DCONFIG_DHD_SET_RANDOM_MAC_VAL=$(CONFIG_DHD_SET_RANDOM_MAC_VAL)
endif # CONFIG_DHD_SET_RANDOM_MAC_VAL
DHDCFLAGS += -DCUSTOM_TDLS_IDLE_MODE_SETTING=10000
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_HIGH=-80
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_LOW=-85
DHDCFLAGS += -DCUSTOM_TCPACK_SUPP_RATIO=15
DHDCFLAGS += -DCUSTOM_TCPACK_DELAY_TIME=10
DHDCFLAGS += -DD3_ACK_RESP_TIMEOUT=4000
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000
DHDCFLAGS += -DMAX_DTIM_ALLOWED_INTERVAL=925
DHDCFLAGS += -DNUM_SCB_MAX_PROBE=3
DHDCFLAGS += -DWL_SCB_TIMEOUT=10
# need to tune according if needs
#DHDCFLAGS += -DWIFI_TURNON_DELAY=200
#DHDCFLAGS += -DWIFI_TURNOFF_DELAY=100
# AMPDU
DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=64

#Static preallocated buffers
DHDCFLAGS += -DDHD_USE_STATIC_MEMDUMP

# Export control file
DHDCFLAGS += -DDHD_EXPORT_CNTL_FILE
ifeq ($(CONFIG_BCMDHD_PCIE),y)
  DHDCFLAGS += -DDHD_USE_STATIC_CTRLBUF
  #Use coherent pool
  DHDCFLAGS += -DDHD_USE_COHERENT_MEM_FOR_RING

  # 4way handshake disconnection feature
  DHDCFLAGS += -DDHD_4WAYM4_FAIL_DISCONNECT

  # Enabling this flag will allocate the coherent memory
  # from Atomic pool. For 64 bit ARM arch the default
  # coherent memory pool size is 256K. This size has to be
  # increased to 4M if the DHD uses this feature. So if the
  # BSP cannot be modified DHD should not be allocating from
  # coherent pool.
  #DHDCFLAGS += -DDHD_ALLOC_COHERENT_MEM_FROM_ATOMIC_POOL

  ## OOB
  ifeq ($(CONFIG_BCMDHD_OOB_HOST_WAKE),y)
    DHDCFLAGS += -DBCMPCIE_OOB_HOST_WAKE
    DHDCFLAGS += -DDHD_USE_PCIE_OOB_THREADED_IRQ
  endif # CONFIG_BCMDHD_OOB_HOST_WAKE
endif # CONFIG_BCMDHD_PCIE

# DMA64 suppports on 64bit Architecture
ifeq ($(CONFIG_64BIT),y)
  DHDCFLAGS := $(filter-out -DBCMDMA32,$(DHDCFLAGS))
  DHDCFLAGS += -DBCMDMA64OSL
endif # CONFIG_64BIT

DHDCFLAGS += -DVSDB

# TDLS enable
DHDCFLAGS += -DWLTDLS -DWLTDLS_AUTO_ENABLE
# For TDLS tear down inactive time 40 sec
DHDCFLAGS += -DCUSTOM_TDLS_IDLE_MODE_SETTING=10000
# for TDLS RSSI HIGH for establishing TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_HIGH=-80
# for TDLS RSSI HIGH for tearing down TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_LOW=-85

# Set PM 2 always regardless suspend/resume
DHDCFLAGS += -DSUPPORT_PM2_ONLY
# remove this if allow DHD driver manage PM automatically
DHDCFLAGS += -DDISABLE_AUTO_CONTROL_PM

DHDCFLAGS += -DMIRACAST_AMPDU_SIZE=8
#Vendor Extension
ifeq ($(call wlan_version_ge,$(BCM_KVERSIONSTRING),3.14.0),TRUE)
  DHDCFLAGS += -DWL_VENDOR_EXT_SUPPORT
  ifeq ($(CONFIG_BUILD_CFG80211),y)
  DHDCFLAGS += -DBCM_PRIV_CMD_SUPPORT
  DHDCFLAGS += -DWL_PRIV_CMD_OVER_NL80211
  endif # CONFIG_BUILD_CFG80211
endif # wlan_version_ge

#RSSI Monitor
DHDCFLAGS += -DRSSI_MONITOR_SUPPORT
#NDOffload
DHDCFLAGS += -DNDO_CONFIG_SUPPORT
DHDCFLAGS += -DIPV6_NDO_SUPPORT

#Debugability
#DHDCFLAGS += -DDHD_DEBUG_WAKE_LOCK
#DHDCFLAGS += -DDBG_PKT_MON
#DHDCFLAGS += -DDNGL_EVENT_SUPPORT
DHDCFLAGS += -DDEBUGABILITY
DHDCFLAGS += -DDHD_DEBUGABILITY_EVENT_RING

# Early suspend
DHDCFLAGS += -DDHD_USE_EARLYSUSPEND

# For Scan result patch
DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DDUAL_ESCAN_RESULT_BUFFER

# CSI
DHDCFLAGS += -DCSI_SUPPORT

# NAN
DHDCFLAGS += -DWL_NAN -DWL_NAN_DISC_CACHE -DWL_NANP2P

# SAR Tx power scenario
DHDCFLAGS += -DWL_SAR_TX_POWER

# OWE
DHDCFLAGS += -DWL_OWE

# Silent roam
DHDCFLAGS += -DCONFIG_SILENT_ROAM

# WPA3 external authenticator
DHDCFLAGS += -DWL_CLIENT_SAE

# Disable HE on P2P based on peer support
DHDCFLAGS += -DWL_DISABLE_HE_P2P

# Scheduled scan (PNO)
# DHDCFLAGS += -DWL_SCHED_SCAN

#static if
# DHDCFLAGS += -DWL_STATIC_IF
# DHDCFLAGS += -DWL_STATIC_IFNAME_PREFIX="\"swlan0\""

# For Static Buffer
ifeq ($(CONFIG_DHD_USE_STATIC_BUF),y)
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
  DHDCFLAGS += -DENHANCED_STATIC_BUF
  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
endif # CONFIG_DHD_USE_STATIC_BUF

# Ioctl timeout 5000ms
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# Prevent rx thread monopolize
DHDCFLAGS += -DWAIT_DEQUEUE

# idle count
DHDCFLAGS += -DDHD_USE_IDLECOUNT

# SKB TAILPAD to avoid out of boundary memory access
DHDCFLAGS += -DDHDENABLE_TAILPAD

# Disable FRAMEBURST on VSDB
DHDCFLAGS += -DDISABLE_FRAMEBURST_VSDB

# WPS
DHDCFLAGS += -DWL_WPS_SYNC -DBCMCRYPTO_COMPONENT

# TCP SYNC flood attack block
DHDCFLAGS += -DDHDTCPSYNC_FLOOD_BLK -DTCP_SYNC_FLOOD_LIMIT=5000

# Path name to store the FW Debug symbol files
DHDCFLAGS += -DPLATFORM_PATH="\"/data/vendor/conn/\""

# Control PM related value for MFG process(Read from file)
DHDCFLAGS += -DDHD_PM_CONTROL_FROM_FILE

##########################
# driver type
# m: module type driver
# y: built-in type driver
##########################
DRIVER_TYPE ?= $(CONFIG_BCMDHD)

##### Chip dependent feature
DHDCFLAGS += -D$(CONFIG_CHIPSET) -D$(CONFIG_CHIPSET)_CHIP
DHDCFLAGS += -DWL11AC

# Expand TCP tx queue
DHDCFLAGS += -DDHD_TCP_PACING_SHIFT

# prioritize 802.1x packet
DHDCFLAGS += -DEAPOL_PKT_PRIO

ifeq ($(CONFIG_BCMDHD),y) # for builtin DHD driver
  DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD
  DHDCFLAGS += -DWAKEUP_KSOFTIRQD_POST_NAPI_SCHEDULE
  DHDCFLAGS += -DUSE_LATE_INITCALL_SYNC
  # Use kernel strlcpy() implementation instead of one, defined in bcmstdlib_s.c
  DHDCFLAGS += -DBCM_USE_PLATFORM_STRLCPY
else # for module DHD driver
  DHDCFLAGS += -DBCMDHD_MODULAR
  DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD
endif # for builtin DHD driver

DHDOFILES := dhd_pno.o dhd_common.o dhd_ip.o dhd_custom_cis.o dhd_custom_gpio.o dhd_custom_sec.o \
    dhd_linux.o dhd_linux_sched.o dhd_linux_wq.o aiutils.o \
    bcmevent.o bcmutils.o bcmwifi_channels.o hndpmu.o linux_osl.o linux_pkt.o \
    sbutils.o siutils.o  dhd_linux_exportfs.o wl_roam.o \
    wldev_common.o wl_linux_mon.o dhd_linux_platdev.o \
    dhd_linux_pktdump.o wl_cfg_btcoex.o hnd_pktq.o \
    hnd_pktpool.o bcmxtlv.o bcm_app_utils.o dhd_debug.o \
    dhd_debug_linux.o dhd_mschdbg.o bcmbloom.o dhd_dbg_ring.o bcmstdlib_s.o frag.o \
    wl_cfgnan.o wl_cfgscan.o

DHDOFILES += $(BCMINTERNAL_DHDOFILES)

ifeq ($(CONFIG_BUILD_CFG80211),y)
  DHDOFILES += dhd_cfg80211.o wl_cfg80211.o wl_cfgvendor.o wl_cfgp2p.o wl_android.o
endif # CONFIG_BUILD_CFG80211

# extra Source files
DHDOFILES += wl_cfgvif.o wl_cfg_btcoex.o

ifeq ($(CONFIG_BCMDHD_DEBUG),y)
#  DHDOFILES += dhd_macdbg.o
  ifeq ($(CONFIG_BCMDHD_PCIE),y)
    DHDOFILES += dhd_timesync.o
  endif # CONFIG_BCMDHD_PCIE
endif # CONFIG_BCMDHD_DEBUG

ifneq ($(filter -DWBRC, $(DHDCFLAGS)),)
	DHDOFILES += wb_regon_coordinator.o
endif

# RTT feature
ifneq ($(filter -DRTT_SUPPORT, $(DHDCFLAGS)),)
DHDOFILES += dhd_rtt.o
endif


ifneq ($(filter -DDHD_PKT_LOGGING,$(DHDCFLAGS)),)
	DHDOFILES += dhd_pktlog.o
ifneq ($(filter -DDHD_COMPACT_PKT_LOG,$(DHDCFLAGS)),)
DHDOFILES += bcm_l2_filter.o
endif
endif

ifneq ($(filter -DDHD_EWPR_VER2, $(DHDCFLAGS)),)
DHDOFILES += dhd_bitpack.o
endif

ifneq ($(filter -DBIGDATA_SOFTAP,$(DHDCFLAGS)),)
	DHDOFILES += wl_bigdata.o
endif

ifneq ($(filter -DWL_BAM,$(DHDCFLAGS)),)
DHDOFILES += wl_bam.o
endif

ifneq ($(filter -DDHD_EVENT_LOG_FILTER, $(DHDCFLAGS)),)
DHDOFILES += dhd_event_log_filter.o
endif # DHD_EVENT_LOG_FILTER

ifneq ($(filter -DDHD_LB, $(DHDCFLAGS)),)
DHDOFILES += dhd_linux_lb.o
endif # DHD_LB

ifneq ($(filter -DDHD_STATUS_LOGGING,$(DHDCFLAGS)),)
DHDOFILES += dhd_statlog.o
endif

ifneq ($(filter -DREAD_CONFIG_FROM_FILE, $(DHDCFLAGS)),)
  DHDOFILES += dhd_config.o
endif # READ_CONFIG_FROM_FILE

ifneq ($(filter -DDHD_WET, $(DHDCFLAGS)),)
	DHDOFILES += dhd_wet.o
endif

ifeq ($(CONFIG_ARCH_HISI),y)
  DHDCFLAGS += -DBOARD_MODULAR_INIT
  # Hikey sched is not so optimized and hence need a higher timeout
  DHDCFLAGS += -DWAIT_FOR_DISCONNECT_MAX=20
  DHDOFILES += dhd_custom_hikey.o
else ifneq ($(filter y, $(CONFIG_ARCH_MSM) $(CONFIG_ARCH_QCOM)),)
  DHDCFLAGS += -DBOARD_MODULAR_INIT
  DHDOFILES += dhd_custom_msm.o
  ifeq ($(CONFIG_ARCH_SDM660),y)
    DHDCFLAGS += -DCONFIG_ARCH_SDM660
  endif # CONFIG_ARCH_SDM660
else ifeq ($(CONFIG_ARCH_EXYNOS),y)
  DHDCFLAGS += -DBOARD_MODULAR_INIT
  DHDOFILES += dhd_custom_exynos.o
else
  # nothing to do here
endif # CONFIG_ARCH_HISI

ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
  DHDOFILES += dhd_custom_memprealloc.o
endif # CONFIG_BROADCOM_WIFI_RESERVED_MEM

ifeq ($(CONFIG_BCMDHD_SDIO),y)
  DHDOFILES += bcmsdh.o bcmsdh_linux.o
  ifeq ($(CONFIG_BCMDHD_SDIO_SDMMC),y)
  DHDOFILES += bcmsdh_sdmmc.o bcmsdh_sdmmc_linux.o
  else ifeq ($(CONFIG_BCMDHD_SDIO_SDSTD),y)
  DHDOFILES += bcmsdstd.o bcmsdstd_linux.o
  endif # CONFIG_BCMDHD_SDIO_SDMMC
  DHDOFILES += dhd_cdc.o dhd_wlfc.o dhd_sdio.o
endif # CONFIG_BCMDHD_SDIO

ifeq ($(CONFIG_BCMDHD_PCIE),y)
  DHDOFILES += dhd_pcie.o dhd_pcie_linux.o dhd_msgbuf.o dhd_flowring.o
  DHDOFILES += pcie_core.o
endif # CONFIG_BCMDHD_PCIE

##### Finalize
EXTRA_CFLAGS += $(DHDCFLAGS) -g
EXTRA_CFLAGS += -DSRCBASE=\"$(BCMDHD_ROOT)\" -DDHD_COMPILED=\"$(BCMDHD_ROOT)\"
EXTRA_CFLAGS += -I$(BCMDHD_ROOT)/include/ -I$(BCMDHD_ROOT)/
KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(CURDIR) -Wno-date-time
EXTRA_LDFLAGS += $(DHDLDFLAGS) -g
# remove static for modular
ifeq ($(CONFIG_BCMDHD),m) # for modular DHD driver
  EXTRA_CFLAGS := $(filter-out -DCONFIG_DHD_USE_STATIC_BUF,$(EXTRA_CFLAGS))
  KBUILD_CFLAGS := $(filter-out -DCONFIG_DHD_USE_STATIC_BUF,$(KBUILD_CFLAGS))
  EXTRA_LDFLAGS := $(filter-out -DCONFIG_DHD_USE_STATIC_BUF,$(EXTRA_LDFLAGS))
endif # modular DHD driver

dhd-objs := $(DHDOFILES)
obj-$(DRIVER_TYPE)   += dhd.o

ccflags-y := $(KBUILD_CFLAGS)
ccflags-y += $(EXTRA_CFLAGS)

ifeq ($(call wlan_version_ge,$(BCM_KVERSIONSTRING),5.3.0),TRUE)
build_command=$(MAKE) --no-print-directory -C ${KERNEL_SRC} M=$(CURDIR) $(KBUILD_OPTIONS)
else # ($(call wlan_version_ge,$(BCM_KVERSIONSTRING),5.3.0),TRUE)
build_command=$(MAKE) --no-print-directory -C ${KERNEL_SRC} SUBDIRS=$(CURDIR) $(KBUILD_OPTIONS)
endif # ($(call wlan_version_ge,$(BCM_KVERSIONSTRING),5.3.0),TRUE)

all:
	@echo ARCH='$(ARCH)'
	@echo CROSS_COMPILE='$(CROSS_COMPILE)'
	@echo LINUXDIR='$(LINUXDIR)'
	@echo ${build_command} modules
	@${build_command} modules

install modules_install:
	@${build_command} modules_install

clean:
	@rm -rf *.o *.ko *.ko.* *.mod *.mod.c *.dis* *~ .*.cmd *.o.cmd .*.o.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin *.o.ur-safe .*.o.d *.o.ur-detected *.map
	@echo clean done!
