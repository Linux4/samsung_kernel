#
# Makefile for the Omnivision TCM touchscreen driver.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_SPI) += omnivision_tcm_spi.o
# obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_I2C) += omnivision_tcm_i2c.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_CORE) += omnivision_tcm_core_module.o
omnivision_tcm_core_module-objs := omnivision_tcm_core.o omnivision_tcm_touch.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_ZEROFLASH) += omnivision_tcm_zeroflash.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_REFLASH) += omnivision_tcm_reflash.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_DEVICE) += omnivision_tcm_device.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_TESTING) += omnivision_tcm_testing.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_TESTING) += omnivision_tcm_csv_parser.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_RECOVERY) += omnivision_tcm_recovery.o
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_DIAGNOSTICS) += omnivision_tcm_diagnostics.o
# hs14 code for AL6528A-13 by hehaoran5 at 20220901 start
obj-$(CONFIG_TOUCHSCREEN_OMNIVISION_TCM_SEC) += omnivision_tcm_sec.o
# hs14 code for AL6528A-13 by hehaoran5 at 20220901 end