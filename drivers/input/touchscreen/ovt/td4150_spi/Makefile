#
# Makefile for the Synaptics TCM touchscreen driver.
#

TARGET = ovt_td4150_spi

ifneq ($(filter y m, $(CONFIG_SAMSUNG_PRODUCT_SHIP)),)
$(TARGET)-objs := ovt_tcm_spi.o ovt_tcm_sec_fn.o ovt_tcm_sec_fn_test.o ovt_tcm_core.o ovt_tcm_touch.o ovt_tcm_zeroflash.o
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_REFLASH)),)
$(TARGET)-objs += ovt_tcm_reflash.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_RECOVERY)),)
$(TARGET)-objs += ovt_tcm_recovery.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_DIAGNOSTICS)),)
$(TARGET)-objs += ovt_tcm_diagnostics.o
endif
else #CONFIG_SAMSUNG_PRODUCT_SHIP else
$(TARGET)-objs := ovt_tcm_spi.o ovt_tcm_sec_fn.o ovt_tcm_sec_fn_test.o ovt_tcm_core.o ovt_tcm_touch.o ovt_tcm_zeroflash.o
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_REFLASH)),)
$(TARGET)-objs += ovt_tcm_reflash.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_DEVICE)),)
$(TARGET)-objs += ovt_tcm_device.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_TESTING)),)
$(TARGET)-objs += ovt_tcm_testing.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_RECOVERY)),)
$(TARGET)-objs += ovt_tcm_recovery.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_OVT_TCM_DIAGNOSTICS)),)
$(TARGET)-objs += ovt_tcm_diagnostics.o
endif
endif #CONFIG_SAMSUNG_PRODUCT_SHIP end

obj-$(CONFIG_TOUCHSCREEN_OVT_TD4150_SPI) += $(TARGET).o

ccflags-y += -Wformat

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui
