obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)	+= is-device-eeprom.o
ifeq ($(CONFIG_CAMERA_OTPROM_SUPPORT_REAR),m)
obj-$(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)	+= is-device-otprom.o
else ifeq ($(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT),m)
obj-$(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)	+= is-device-otprom.o
endif

EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/include
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/lib
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework/cis