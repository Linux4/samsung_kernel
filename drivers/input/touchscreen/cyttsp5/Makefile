obj-$(CONFIG_TOUCHSCREEN_ATMEL_MXT2952T)	+= mxts.o
obj-$(CONFIG_TOUCHSCREEN_ZINITIX)	+= zinitix_touch.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX)		+= Himax_Touch.o
obj-$(CONFIG_TOUCHSCREEN_MMS300)		+= mms300_ts.o

#cyttsp
#obj-$(CONFIG_TOUCHSCREEN_CYTTSP5)	+= cyttsp5_i2c__.o cyttsp5_core.o cyttsp5_mtb.o cyttsp5_mt_common.o cyttsp5_debug.o cyttsp5_device_access.o cyttsp5_loader.o cyttsp5_platform.o

#obj-m	+= cyttsp5.o
obj-y	+= cyttsp5.o
cyttsp5-y := cyttsp5_core.o cyttsp5_mt_common.o #cyttsp5_platform.o 
cyttsp5-y += cyttsp5_mtb.o
#cyttsp5-y += cyttsp5_btn.o
cyttsp5-y += cyttsp5_samsung_factory.o
obj-y += cyttsp5_platform.o
#obj-m += cyttsp5_i2c.o
#obj-m += cyttsp5_loader.o
#obj-m += cyttsp5_device_access.o
obj-y += cyttsp5_i2c.o
obj-y += cyttsp5_loader.o
obj-y += cyttsp5_device_access.o

#CYTTSP_CFLAGS = -DCONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_BUTTON
#CFLAGS_cyttsp5_core.o += $(CYTTSP_CFLAGS) -DDEBUG -DVERBOSE_DEBUG
#CFLAGS_cyttsp5_i2c.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_mtb.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_mt_common.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_btn.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_device_access.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_loader.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_platform.o += $(CYTTSP_CFLAGS) -DDEBUG
#CFLAGS_cyttsp5_samsung_factory.o += $(CYTTSP_CFLAGS) -DDEBUG
