
SWPM_MODULES_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/

ifeq ($(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE), m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_lite
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
endif

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V3),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v3
endif

ifeq ($(CONFIG_MTK_QOS_FRAMEWORK),m)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos
endif

ifeq ($(CONFIG_DEVICE_MODULES_MTK_THERMAL),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/thermal/mediatek/
endif

MTK_SWPM_DBG_VERSION = v1


ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/mtk_swpm_fs/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/swpm/modules/debug/$(MTK_SWPM_DBG_VERSION)/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6897/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/debug/v6897/subsys/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(SWPM_MODULES_FOLDER)/include/v6897/

BUILD_SWPM_DBG_MODUDLE_NAME=mtk-swpm-dbg-v6897
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6897.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6897_ext.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6897_subsys.o
${BUILD_SWPM_DBG_MODUDLE_NAME}-objs += swpm_v6897_dbg_init.o

BUILD_SWPM_CPU_DBG_MODUDLE_NAME=mtk-swpm-cpu-dbg-v6897
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}-objs += subsys/swpm_cpu_v6897.o
${BUILD_SWPM_CPU_DBG_MODUDLE_NAME}-objs += subsys/swpm_cpu_v6897_dbg_init.o

BUILD_SWPM_MEM_DBG_MODUDLE_NAME=mtk-swpm-mem-dbg-v6897
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}-objs += subsys/swpm_mem_v6897.o
${BUILD_SWPM_MEM_DBG_MODUDLE_NAME}-objs += subsys/swpm_mem_v6897_dbg_init.o

BUILD_SWPM_CORE_DBG_MODUDLE_NAME=mtk-swpm-core-dbg-v6897
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}-objs += subsys/swpm_core_v6897.o
${BUILD_SWPM_CORE_DBG_MODUDLE_NAME}-objs += subsys/swpm_core_v6897_dbg_init.o

BUILD_SWPM_AUDIO_DBG_MODUDLE_NAME=mtk-swpm-audio-dbg-v6897
obj-$(CONFIG_MTK_SWPM_MODULE) += ${BUILD_SWPM_AUDIO_DBG_MODUDLE_NAME}.o
${BUILD_SWPM_AUDIO_DBG_MODUDLE_NAME}-objs += subsys/swpm_audio_v6897.o
${BUILD_SWPM_AUDIO_DBG_MODUDLE_NAME}-objs += subsys/swpm_audio_v6897_dbg_init.o