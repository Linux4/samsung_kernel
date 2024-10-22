################################
## Add specified modules here ##
################################
$(info MODULE_NAME $(MODULE_NAME))
$(info Segment: $(SEGMENT))

ifeq ($(SEGMENT), SP)
    # build ko by connac version
    KO_CODE_PATH := $(if $(filter /%,$(src)),,$(srctree)/)$(src)
    ifeq ($(MODULE_NAME), wlan_drv_gen4m_eap_6639)
        include $(KO_CODE_PATH)/Kbuild.eap_6639
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6985_6639)
        include $(KO_CODE_PATH)/Kbuild.6985_6639
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6989_6639)
        include $(KO_CODE_PATH)/Kbuild.6989_6639
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6989_6639_dppm)
        include $(KO_CODE_PATH)/Kbuild.6989_6639_dppm
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6989_6639_offload)
        include $(KO_CODE_PATH)/Kbuild.6989_6639_offload
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6989_6639_mp2)
        include $(KO_CODE_PATH)/Kbuild.6989_6639_mp2
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6897)
        include $(KO_CODE_PATH)/Kbuild.6897
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6893)
        include $(KO_CODE_PATH)/Kbuild.6893
    else ifeq ($(MODULE_NAME), wlan_drv_gen4m_6878)
        include $(KO_CODE_PATH)/Kbuild.6878
    endif
else
    KO_CODE_PATH := $(if $(filter /%,$(src)),,$(srctree)/)$(src)

    include $(KO_CODE_PATH)/Kbuild.main
endif
