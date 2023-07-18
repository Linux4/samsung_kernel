WCN_BSP_PCIE_TREE := drivers/misc/sprd_wcn_n6pro/wcn_bsp_pcie/
ccflags-y += \
	-I$(WCN_BSP_PCIE_TREE)/include \
	-I$(WCN_BSP_PCIE_TREE)/boot \
	-I$(WCN_BSP_PCIE_TREE)/platform \
	-I$(WCN_BSP_PCIE_TREE)/sleep \
	-I$(WCN_BSP_PCIE_TREE)/pcie

ccflags-y += -DCONFIG_WCN_BSP_PCIE_Y

ifeq ($(TARGET_BUILD_VARIANT),user)
	ccflags-y += -DCONFIG_WCN_USER
endif

#WCN MODULE
obj-$(CONFIG_WCN_BSP_PCIE) += wcn_bsp_pcie.o
wcn_bsp_pcie-y += wcn_bus.o

#PLATFORM FILE
wcn_bsp_pcie-y += platform/wcn_parn_parser.o
wcn_bsp_pcie-y += platform/wcn_procfs.o
wcn_bsp_pcie-y += platform/wcn_txrx.o
#wcn_bsp_pcie-y += platform/rdc_debug.o
wcn_bsp_pcie-y += platform/wcn_log.o
wcn_bsp_pcie-y += platform/wcn_misc.o
wcn_bsp_pcie-y += platform/bufring.o
wcn_bsp_pcie-y += platform/wcn_op.o
wcn_bsp_pcie-y += platform/sysfs.o
wcn_bsp_pcie-y += platform/reset.o
wcn_bsp_pcie-y += platform/reset_test.o
wcn_bsp_pcie-y += platform/loopcheck.o
wcn_bsp_pcie-y += platform/wcn_boot.o
wcn_bsp_pcie-y += platform/wcn_dump.o
wcn_bsp_pcie-y += platform/pcie_boot.o
wcn_bsp_pcie-$(CONFIG_WCN_SWD_DAP) += platform/wcn_swd_dap.o
wcn_bsp_pcie-y += platform/wcn_debugfs.o
wcn_bsp_pcie-y += platform/wcn_gnss_ops.o
wcn_bsp_pcie-y += platform/wcn_gnss_dump.o

#PCIE FILE
wcn_bsp_pcie-y += pcie/pcie.o
wcn_bsp_pcie-y += pcie/edma_engine.o
wcn_bsp_pcie-y += pcie/pcie_dbg.o
wcn_bsp_pcie-y += pcie/ioctl.o
wcn_bsp_pcie-y += pcie/mchn.o
wcn_bsp_pcie-y += pcie/edma_test.o
wcn_bsp_pcie-y += pcie/pcie_attach_bus.o
wcn_bsp_pcie-y += pcie/pcie_pm.o

##GNSS Module
wcn_bsp_pcie-y += platform/gnss/gnss_common_ctl.o
wcn_bsp_pcie-y += platform/gnss/gnss_dbg.o
wcn_bsp_pcie-y += platform/gnss/gnss_pmnotify_ctl.o
