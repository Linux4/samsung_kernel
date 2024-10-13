PRODUCT_PACKAGES += vendor.qti.hardware.lights.service

ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),S 12))
  SOONG_CONFIG_NAMESPACES += lights
  SOONG_CONFIG_lights += lighttargets
  SOONG_CONFIG_lights_lighttargets := lightaidlV1platformtarget
endif

ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),T 13))
  $(call soong_config_set, lights, lighttargets, lightaidlV2target)
endif

ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),U 14))
  $(call soong_config_set, lights, lighttargets, lightaidlV2target)
endif
