LOCAL_PATH := $(call my-dir)

QMAA_DISABLES_WFD := true
ifeq ($(TARGET_USES_QMAA),true)
ifneq ($(TARGET_USES_QMAA_OVERRIDE_WFD),true)
QMAA_DISABLES_WFD := true
endif #TARGET_USES_QMAA_OVERRIDE_WFD
endif #TARGET_USES_QMAA

WFD_DISABLE_PLATFORM_LIST := neo

#Disable WFD for selected 32-bit targets
ifeq ($(call is-board-platform,bengal),true)
ifeq ($(TARGET_BOARD_SUFFIX),_32)
WFD_DISABLE_PLATFORM_LIST += bengal
endif
endif

ifneq ($(call is-board-platform-in-list,$(WFD_DISABLE_PLATFORM_LIST)),true)
ifneq ($(TARGET_HAS_LOW_RAM), true)
ifneq ($(QMAA_DISABLES_WFD),true)
include $(call all-makefiles-under, $(LOCAL_PATH))
endif #QMAA_DISABLES_WFD
endif #TARGET_HAS_LOW_RAM
endif #WFD_DISABLE_PLATFORM_LIST
