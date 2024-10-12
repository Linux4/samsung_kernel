# Makefile for the Himax touchscreen drivers.

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

ifneq ($(filter y, $(CONFIG_DRM)),)
	ccflags-y += -DHX_CONFIG_DRM
endif

ifneq ($(filter y, $(CONFIG_PM)),)
	ccflags-y += -DHX_CONFIG_PM
endif

# obj-y += himax_modular_table.o

  ccflags-y += -D__HIMAX_HX83112F_MOD__
	himax_mmi-objs := himax_ic_HX83112f.o
	himax_mmi-objs += himax_inspection.o
	himax_mmi-objs += himax_debug.o
	himax_mmi-objs += himax_ic_core.o
	himax_mmi-objs += himax_common.o
	himax_mmi-objs += himax_platform.o
  obj-y += himax_mmi.o

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	himax_mmi-objs += himax_debug.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	himax_mmi-objs += himax_inspection.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108A)),)
  ccflags-y += -D__HIMAX_HX83108A_MOD__
	himax_mmi-objs += himax_ic_HX83108a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102J)),)
  ccflags-y += -D__HIMAX_HX83102J_MOD__
	himax_mmi-objs += himax_ic_HX83102j.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112F)),)
  ccflags-y += -D__HIMAX_HX83112F_MOD__
	himax_mmi-objs += himax_ic_HX83112f.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121A)),)
  ccflags-y += -D__HIMAX_HX83121A_MOD__
	himax_mmi-objs += himax_ic_HX83121a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83122A)),)
  ccflags-y += -D__HIMAX_HX83122A_MOD__
	himax_mmi-objs += himax_ic_HX83122a.o
endif

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX85200A)),)
  ccflags-y += -D__HIMAX_HX85200A_MOD__
	himax_mmi-objs += himax_ic_HX85200a.o
endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
  himax_mmi-objs += himax_ic_core.o
  himax_mmi-objs += himax_common.o
  himax_mmi-objs += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
    ccflags-y += -DHX_FIRMWARE_HEADER
    himax_mmi-objs += himax_firmware.o
  endif
  obj-m += himax_mmi.o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  himax_mmi-objs += himax_ic_core.o
  himax_mmi-objs += himax_common.o
  himax_mmi-objs += himax_platform.o
  ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
	ccflags-y += -DHX_FIRMWARE_HEADER
	himax_mmi-objs += himax_firmware.o
  endif
  obj-y += himax_mmi.o
endif

