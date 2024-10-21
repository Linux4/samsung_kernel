
LPM_MODULES_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/

MTK_LPM_DBG_VERSION = v2

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/inc/lpm_trace_event/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/common/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_DBG_VERSION)/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_DBG_VERSION)/cpc/
ccflags-y += -I$(LPM_MODULES_FOLDER)/debug/mt6989/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(LPM_MODULES_FOLDER)/include/mt6989/
ifeq ($(CONFIG_MTK_SYS_RES_DBG_SUPPORT),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/debug/v6989
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/debug/v6989/subsys
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/lpm/modules/debug/sys_res
endif

BUILD_LPM_DBG_MODUDLE_NAME=mtk-lpm-dbg-mt6989
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += ${BUILD_LPM_DBG_MODUDLE_NAME}.o

${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_logger.o
ifeq ($(CONFIG_MTK_SYS_RES_DBG_SUPPORT),m)
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_sys_res_plat.o
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_sys_res_mbrain_plat.o
endif

ifeq ($(CONFIG_MTK_ECCCI_DRIVER),m)
${BUILD_LPM_DBG_MODUDLE_NAME}-objs += lpm_ccci_dump.o
endif
