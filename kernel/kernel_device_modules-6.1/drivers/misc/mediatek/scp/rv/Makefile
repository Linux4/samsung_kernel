obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_helper.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_excep.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_wrapper_ipi.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_irq.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_logger.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_l1c.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_scpctl.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_chre_manager.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_feature_table.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_awake.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_dvfs.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_hwvoter_dbg.o

scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_low_pwr_dbg.o
scp-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp_mbrain_dbg.o

scp-$(CONFIG_MTK_TINYSYS_SAP_SUPPORT) += sap.o

ccflags-y += -D DEBUG_DO -fno-pic -mcmodel=large
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/aee/aed/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
