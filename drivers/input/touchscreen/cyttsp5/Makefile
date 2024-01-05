#
# Makefile for the cypress cyttsp5 touchscreen drivers.
#

obj-$(CONFIG_CYPRESS_CYTTSP5_BUS) += cyttsp5_bus.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5)	+= cyttsp5_core.o cyttsp5_platform.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_I2C)	+= cyttsp5_i2c.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_MT_B)	+= cyttsp5_mt_b.o
cyttsp5_mt_b-y := cyttsp5_mtb.o cyttsp5_mt_common.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICE_ACCESS)	+= cyttsp5_device_access.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_LOADER)	+= cyttsp5_loader.o
obj-$(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEBUG_MODULE)	+= cyttsp5_debug.o

ifeq ($(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEBUG),y)
CFLAGS_cyttsp5_bus.o += -DDEBUG
CFLAGS_cyttsp5_core.o += -DDEBUG
CFLAGS_cyttsp5_i2c.o += -DDEBUG
CFLAGS_cyttsp5_mtb.o += -DDEBUG
CFLAGS_cyttsp5_mt_b.o += -DDEBUG
CFLAGS_cyttsp5_mt_common.o += -DDEBUG
CFLAGS_cyttsp5_device_access.o += -DDEBUG
CFLAGS_cyttsp5_loader.o += -DDEBUG
CFLAGS_cyttsp5_debug.o += -DDEBUG
endif

ifeq ($(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_VDEBUG),y)
CFLAGS_cyttsp5_bus.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_core.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_i2c.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_mtb.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_mt_b.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_mt_common.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_device_access.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_loader.o += -DVERBOSE_DEBUG
CFLAGS_cyttsp5_debug.o += -DVERBOSE_DEBUG
endif
