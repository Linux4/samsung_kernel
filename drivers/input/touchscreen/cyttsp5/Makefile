#
# Makefile for the cypress cyttsp5 touchscreen drivers.
#

obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5)	+= cyttsp5.o
cyttsp5-y := cyttsp5_core.o cyttsp5_mt_common.o
cyttsp5-y += cyttsp5_mtb.o
cyttsp5-y += cyttsp5_samsung_factory.o
cyttsp5-y += cyttsp5_loader.o
cyttsp5-y += cyttsp5_device_access.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT)	+= cyttsp5_devtree.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5) += cyttsp5_platform.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5) += cyttsp5_i2c.o

CYTTSP_CFLAGS = -DCONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_BINARY_FW_UPGRADE -DCONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_MANUAL_TTCONFIG_UPGRADE -DCONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_PLATFORM_FW_UPGRADE
CFLAGS_cyttsp5_core.o += -DDEBUG
CFLAGS_cyttsp5_i2c.o += -DDEBUG
CFLAGS_cyttsp5_mtb.o += -DDEBUG
CFLAGS_cyttsp5_mt_common.o += -DDEBUG
CFLAGS_cyttsp5_device_access.o += -DDEBUG
CFLAGS_cyttsp5_loader.o += -DDEBUG $(CYTTSP_CFLAGS)
CFLAGS_cyttsp5_platform.o += -DDEBUG $(CYTTSP_CFLAGS)
CFLAGS_cyttsp5_samsung_factory.o += -DDEBUG


