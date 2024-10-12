# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Unisoc wcn platform configuration
#

#common obj
#GNSS
WCN_MARLIN_COMMON_FILES += gnss/gnss_common_ctl.o \
			   gnss/gnss_pmnotify_ctl.o \
			   gnss/gnss_dbg.o \
			   gnss/gnss_dump.o

#WCN buf log loopcheck ...
WCN_MARLIN_COMMON_FILES += bufring.o \
			   wcn_txrx.o \
			   wcn_misc.o \
			   wcn_parn_parser.o \
			   wcn_log.o \
			   wcn_procfs.o \
			   wcn_ca_trusty.o \
			   loopcheck.o \
			   reset.o \
			   reset_test.o \
			   rdc_debug.o \
			   sysfs.o

#common obj end

ifeq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -DCONFIG_WCN_USER
endif

obj-$(CONFIG_WCN_BOOT) := marlin.o
marlin-$(CONFIG_WCN_PCIE) += pcie_boot.o
marlin-$(CONFIG_WCN_SWD) += wcn_swd_dap.o

#marlin3:sdio pcie
MARLINOFILES += wcn_boot.o \
		wcn_dump.o \
		wcn_op.o

marlin-$(CONFIG_SDIOHAL) += mem_pd_mgr.o
marlin-$(CONFIG_WCN_UTILS) += wcn_debugfs.o

marlin-objs += $(WCN_MARLIN_COMMON_FILES)
marlin-objs += $(MARLINOFILES)

#WCN_PLATFORM
obj-$(CONFIG_WCN_PLATFORM) := wcn_platform.o

wcn_platform-c-objs += $(WCN_MARLIN_COMMON_FILES)

ifeq ($(CONFIG_WCN_INTEG),y)
wcn_platform-c-objs += rf/rf.o
wcn_platform-c-objs +=  wcn_dump_integrate.o
endif

wcn_platform-objs += $(wcn_platform-c-objs)
