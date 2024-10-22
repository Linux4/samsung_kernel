
ifeq ($(CONFIG_MTK_FPSGO_V3),$(filter $(CONFIG_MTK_FPSGO_V3),m))
ccflags-y += -DCONFIG_MTK_FPSGO_V3
subdir-ccflags-y += -DCONFIG_MTK_FPSGO_V3
endif

MTK_PM_BUILD_FOLDER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance

subdir-ccflags-y += \
	-I$(MTK_PM_BUILD_FOLDER)/perf_ioctl/ \
	-I$(MTK_PM_BUILD_FOLDER)/include/	\
	-I$(MTK_PM_BUILD_FOLDER)/powerhal_cpu_ctrl/	\
	-I$(MTK_PM_BUILD_FOLDER)/clk_adjust/

# mtk_perf_ioctl
include $(MTK_PM_BUILD_FOLDER)/perf_ioctl/build.mk
MTK_PERF_IOCTL = $(mtk_perf_ioctl_objs:%.o=perf_ioctl/%.o)
MTK_PERF_IOCTL_SRC = $(MTK_PERF_IOCTL)
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += mtk_perf_ioctl.o
mtk_perf_ioctl-objs += $(MTK_PERF_IOCTL_SRC)

# mtk_ioctl_touch_boost
MTK_IOCTL_TOUCH_BOOST = $(mtk_ioctl_touch_boost_objs:%.o=perf_ioctl/%.o)
MTK_IOCTL_TOUCH_BOOST_SRC = $(MTK_IOCTL_TOUCH_BOOST)
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += mtk_ioctl_touch_boost.o
mtk_ioctl_touch_boost-objs += $(MTK_IOCTL_TOUCH_BOOST_SRC)

# mtk_ioctl_powerhal
MTK_IOCTL_POWERHAL = $(mtk_ioctl_powerhal_objs:%.o=perf_ioctl/%.o)
MTK_IOCTL_POWERHAL_SRC = $(MTK_IOCTL_POWERHAL)
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += mtk_ioctl_powerhal.o
mtk_ioctl_powerhal-objs += $(MTK_IOCTL_POWERHAL_SRC)

# mtk_perf_ioctl_magt
MTK_PERF_IOCTL_MAGT = $(mtk_perf_ioctl_magt_objs:%.o=perf_ioctl/%.o)
MTK_PERF_IOCTL_MAGT_SRC = $(MTK_PERF_IOCTL_MAGT)
obj-$(CONFIG_MTK_PERF_IOCTL_MAGT) += mtk_perf_ioctl_magt.o
mtk_perf_ioctl_magt-objs += $(MTK_PERF_IOCTL_MAGT_SRC)

#Generic subdir build
obj-$(CONFIG_MTK_FPSGO_V3) += fpsgo_v3/
obj-$(CONFIG_MTK_FRS) += frs/
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += powerhal_cpu_ctrl/
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += touch_boost/
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += clk_adjust/

obj-$(CONFIG_MTK_LOAD_TRACKER) += load_track/
obj-$(CONFIG_MTK_LOAD_TRACKER) += uload_ind/
