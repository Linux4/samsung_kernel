WCN_BSP_SDIO_TREE   := drivers/misc/sprd_wcn_n6pro/wcn_bsp_sdio/
ccflags-y += -I$(WCN_BSP_SDIO_TREE)/sipc/ \
	-I$(WCN_BSP_SDIO_TREE)/include \
	-I$(WCN_BSP_SDIO_TREE)/boot \
	-I$(WCN_BSP_SDIO_TREE)/platform \
	-I$(WCN_BSP_SDIO_TREE)/platform/rf \
	-I$(WCN_BSP_SDIO_TREE)/sleep

ccflags-y += -DCONFIG_WCN_SLP_ADD
ccflags-y += -DCONFIG_SDIOHAL

ifeq ($(TARGET_BUILD_VARIANT),user)
	ccflags-y += -DCONFIG_WCN_USER
endif

#WCN BSP MODULE
obj-$(CONFIG_WCN_BSP_SDIO) += wcn_bsp_sdio.o
wcn_bsp_sdio-y += wcn_bus.o

#PLATFORM FILE
wcn_bsp_sdio-y += platform/wcn_parn_parser.o
wcn_bsp_sdio-y += platform/wcn_procfs.o
wcn_bsp_sdio-y += platform/wcn_txrx.o
#CONFIG_WCN_RDCDBG is not used in AndriodR
#wcn_bsp_sdio-y += platform/rdc_debug.o
wcn_bsp_sdio-y += platform/wcn_log.o
wcn_bsp_sdio-y += platform/wcn_misc.o
wcn_bsp_sdio-y += platform/bufring.o
wcn_bsp_sdio-y += platform/wcn_op.o
wcn_bsp_sdio-y += platform/sysfs.o
wcn_bsp_sdio-y += platform/reset.o
wcn_bsp_sdio-y += platform/reset_test.o
wcn_bsp_sdio-y += platform/loopcheck.o
wcn_bsp_sdio-y += platform/wcn_boot.o
wcn_bsp_sdio-y += platform/wcn_dump.o
wcn_bsp_sdio-y += platform/wcn_gnss_ops.o
wcn_bsp_sdio-y += platform/wcn_gnss_dump.o
#CONFIG_WCN_SWD_DAP is not used in AndriodR
wcn_bsp_sdio-$(CONFIG_WCN_SWD_DAP) += platform/wcn_swd_dap.o
wcn_bsp_sdio-y += platform/mem_pd_mgr.o
wcn_bsp_sdio-y += platform/wcn_debugfs.o
wcn_bsp_sdio-y += sleep/sdio_int.o
wcn_bsp_sdio-y += sleep/slp_mgr.o
wcn_bsp_sdio-y += sleep/slp_sdio.o
wcn_bsp_sdio-y += sleep/slp_test.o

wcn_bsp_sdio-y += sdio/sdiohal_main.o
wcn_bsp_sdio-y += sdio/sdiohal_common.o
wcn_bsp_sdio-y += sdio/sdiohal_tx.o
wcn_bsp_sdio-y += sdio/sdiohal_rx.o
wcn_bsp_sdio-y += sdio/sdiohal_if.o
wcn_bsp_sdio-y += sdio/sdiohal_ctl.o

##GNSS Module
wcn_bsp_sdio-y += platform/gnss/gnss_common_ctl.o
wcn_bsp_sdio-y += platform/gnss/gnss_dbg.o
wcn_bsp_sdio-y += platform/gnss/gnss_pmnotify_ctl.o
