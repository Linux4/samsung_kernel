# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/hxchipset_n6_q/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/i2c/$(MTK_PLATFORM)/
ccflags-y += -D__MTK_SPI__

# Makefile for the Himax touchscreen drivers.

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
ccflags-y += -D__HIMAX_MODULIZE__
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	himax_mmi-objs += himax_debug.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	himax_mmi-objs += himax_inspection.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INCELL)),)

	himax_mmi-objs += himax_ic_incell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_ONCELL)),)
	himax_mmi-objs += himax_ic_oncell_core.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xG)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX852xG.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX852xG.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xH)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX852xH.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX852xH.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83102.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83102.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83103.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83103.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83106)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83106.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83106.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83111.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83111.o
  endif
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)),)
  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-m += himax_ic_HX83112.o
  endif
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	obj-y += himax_ic_HX83112.o
  endif
endif

#ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83113)),)
#  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-m += himax_ic_HX83113.o
#  endif
#  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-y += himax_ic_HX83113.o
#  endif
#endif

#ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)),)
#  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-m += himax_ic_HX83191.o
#  endif
#  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-y += himax_ic_HX83191.o
#  endif
#endif

#ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX89999)),)
#  ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-m += himax_ic_HX89999.o
#  endif
#  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
#	obj-y += himax_ic_HX89999.o
#  endif
#endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
	obj-m += himax_mmi.o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
	obj-y += himax_mmi.o
endif
