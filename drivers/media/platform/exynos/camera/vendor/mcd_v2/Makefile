obj-$(CONFIG_VENDER_MCD_V2) :=	is-vender.o \
							is-vender-caminfo.o \
							is-sec-define.o \
							crc32.o \
							is-sysfs.o \
							is-device-eeprom.o \
							is-sec-util.o

obj-$(CONFIG_OIS_USE)	+= is-device-ois_common.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= is-device-af.o
obj-$(CONFIG_OIS_USE_RUMBA_S4) += is-device-ois_s4.o
obj-$(CONFIG_OIS_USE_RUMBA_SA) += is-device-ois_sa.o

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/is2/include
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/is2/sensor/module_framework
