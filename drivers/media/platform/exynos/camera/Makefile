fimc-is-objs	:= is-core.o \
		is-spi.o \
		is-debug.o \
		is-mem.o \
		is-pipe.o \
		is-framemgr.o \
		is-groupmgr.o \
		is-votfmgr.o \
		is-resourcemgr.o \
		is-devicemgr.o \
		is-video.o \
		is-video-sensor.o \
		is-video-3aa.o \
		is-video-3aac.o \
		is-video-3aap.o \
		is-video-3aaf.o \
		is-video-3aag.o \
		is-video-isp.o \
		is-video-ispc.o \
		is-video-ispp.o \
		is-video-ispt.o \
		is-video-ispg.o \
		is-video-ispv.o \
		is-video-ispw.o \
		is-video-mec.o \
		is-video-orbc.o \
		is-video-mcs.o \
		is-video-mcsp.o \
		is-video-vra.o \
		is-video-clh.o \
		is-video-clhc.o \
		is-video-ssvc0.o \
		is-video-ssvc1.o \
		is-video-ssvc2.o \
		is-video-ssvc3.o \
		is-video-pafrdma.o \
		is-subdev-ctrl.o \
		is-device-ischain.o \
		is-time.o \
		is-dvfs.o \
		is-dt.o \
		is-binary.o \
		is-i2c-config.o \
		is-work.o

fimc-is-objs += is-interface-wrap.o

ifeq ($(CONFIG_USE_SENSOR_GROUP),y)
fimc-is-objs += is-device-sensor_v2.o
else
fimc-is-objs += is-device-sensor.o
endif

obj-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER4) += is-device-csi_v4.o

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += vendor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/

subdir-ccflags-y := -Idrivers/media/platform/exynos/camera
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/include
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/interface
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/hardware
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/hardware/api
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/vendor
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/sensor/csi
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/sensor/module_framework
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera/vendor/mcd/preprocessor
subdir-ccflags-$(CONFIG_PABLO_V7_0_0) += -Idrivers/media/platform/exynos/camera/ischain/is-v7_0_0
subdir-ccflags-$(CONFIG_PABLO_V8_0_0) += -Idrivers/media/platform/exynos/camera/ischain/is-v8_0_0
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -Idrivers/media/platform/exynos/camera/ischain/is-v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -Idrivers/media/platform/exynos/camera/ischain/is-v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -Idrivers/media/platform/exynos/camera/ischain/is-v8_20_0

subdir-ccflags-$(CONFIG_VENDER_DEFAULT) += -Idrivers/media/platform/exynos/camera/vendor/default
ifeq ($(CONFIG_VENDER_DEFAULT),y)
subdir-ccflags-$(CONFIG_PABLO_V7_0_0) += -Idrivers/media/platform/exynos/camera/vendor/default/v7_0_0
subdir-ccflags-$(CONFIG_PABLO_V8_0_0) += -Idrivers/media/platform/exynos/camera/vendor/default/v8_0_0
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -Idrivers/media/platform/exynos/camera/vendor/default/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -Idrivers/media/platform/exynos/camera/vendor/default/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -Idrivers/media/platform/exynos/camera/vendor/default/v8_20_0
endif

subdir-ccflags-$(CONFIG_VENDER_MCD) += -Idrivers/media/platform/exynos/camera/vendor/mcd
ifeq ($(CONFIG_VENDER_MCD),y)
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd/v8_20_0
endif

subdir-ccflags-$(CONFIG_VENDER_MCD_V2) += -Idrivers/media/platform/exynos/camera/vendor/mcd_v2
ifeq ($(CONFIG_VENDER_MCD_V2),y)
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd_v2/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd_v2/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -Idrivers/media/platform/exynos/camera/vendor/mcd_v2/v8_20_0
endif
subdir-ccflags-y += -Idrivers/media/platform/exynos/camera-pp
