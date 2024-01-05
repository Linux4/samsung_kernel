#
# Makefile for Secure OS control sysfs
#
obj-$(CONFIG_SECURE_OS_CONTROL) += sec_os_ctrl.o
# Choose Exynos SoC
EXYNOS_SOC := exynos3472

# MobiCore kernel driver path
GUD_ROOT_FOLDER := drivers/gud/gud-$(EXYNOS_SOC)

# Includes MobiCore kernel driver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/public
