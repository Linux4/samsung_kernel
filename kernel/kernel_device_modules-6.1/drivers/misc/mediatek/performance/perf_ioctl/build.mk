
# Add cflags as bellow
#mtk_perf_ioctl_cflags += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/kernel/
mtk_perf_ioctl_objs += perf_ioctl.o
mtk_ioctl_touch_boost_objs += ioctl_touch_boost.o
mtk_perf_ioctl_magt_objs += perf_ioctl_magt.o
mtk_ioctl_powerhal_objs += ioctl_powerhal.o
