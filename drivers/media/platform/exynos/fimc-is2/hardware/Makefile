fimc-is-hardware-objs := fimc-is-hw-control.o \
		fimc-is-hw-3aa.o \
		fimc-is-hw-isp.o \
		fimc-is-hw-mcscaler.o \
		fimc-is-hw-fd.o \
		fimc-is-hw-dm.o \
		api/fimc-is-hw-api-common.o \
		api/fimc-is-hw-api-mcscaler.o \
		api/fimc-is-hw-api-fd.o \
		api/fimc-is-api-common.o \
		api/fimc-is-api-isp.o

obj-$(CONFIG_USE_DIRECT_IS_CONTROL) += fimc-is-hardware.o
obj-$(CONFIG_USE_HOST_FD_LIBRARY) += api/fimc-is-api-fd.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/hardware/api
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/hardware
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/vendor
