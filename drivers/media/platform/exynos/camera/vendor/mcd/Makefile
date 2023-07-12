obj-$(CONFIG_VENDER_MCD) := is-vender.o \
				is-sec-define.o \
				crc32.o \
				is-sysfs.o \
				is-device-from.o

obj-$(CONFIG_COMPANION_DCDC_USE) += is-pmic.o

obj-$(CONFIG_OIS_USE)	+= is-device-ois_common.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= is-device-af.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)	+= is-device-eeprom.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)	+= is-device-eeprom.o
obj-$(CONFIG_CAMERA_USE_MCU)	+= is-device-ois_mcu.o
obj-$(CONFIG_CAMERA_CIS_TOF_BUCK_USE) += is-isl91128.o
obj-$(CONFIG_CAMERA_USE_INTERNAL_MCU) += is-ois-mcu.o \
				is-hw-api-ois-mcu-v1_1_1.o

ccflags-y := -Idrivers/media/platform/exynos/camera
ccflags-y += -Idrivers/media/platform/exynos/camera/include
ccflags-y += -Idrivers/media/platform/exynos/camera/sensor/module_framework
ccflags-y += -Idrivers/media/platform/exynos/camera/sensor/module_framework/iris
