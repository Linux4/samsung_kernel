# auto-detect subdirs
ifeq ($(CONFIG_ARCH_SDM845), y)
include $(srctree)/techpack/audio/config/sdm845auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
ifeq ($(CONFIG_SEC_GENZQLTE_PROJECT), y)
include $(srctree)/techpack/audio/config/sdm710auto_genz.conf
else ifeq ($(CONFIG_SEC_A9SQLTE_PROJECT), y)
include $(srctree)/techpack/audio/config/sdm710auto_a9s.conf
else ifeq ($(CONFIG_SEC_A8SQLTE_PROJECT), y)
include $(srctree)/techpack/audio/config/sdm710auto_a8s.conf
else ifeq ($(CONFIG_SEC_GTS4LV_PROJECT), y)
include $(srctree)/techpack/audio/config/sdm710auto_gts4lv.conf
else ifeq ($(CONFIG_SEC_GTACTIVEXL_PROJECT), y)
include $(srctree)/techpack/audio/config/sdm710auto_gtactivexl.conf
else
include $(srctree)/techpack/audio/config/sdm710auto_static.conf
endif
export
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
include $(srctree)/techpack/audio/config/sdxpoorwillsauto.conf
export
endif

# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi \

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include

ifeq ($(CONFIG_ARCH_SDM845), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm845autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDM670), y)
ifeq ($(CONFIG_SEC_GENZQLTE_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf_genz.h
else ifeq ($(CONFIG_SEC_A9SQLTE_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf_a9s.h
else ifeq ($(CONFIG_SEC_A8SQLTE_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf_a8s.h
else ifeq ($(CONFIG_SEC_GTS4LV_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf_gts4lv.h
else ifeq ($(CONFIG_SEC_GTACTIVEXL_PROJECT), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf_gtactivexl.h
else
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdm710autoconf.h
endif
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdxpoorwillsautoconf.h
endif

obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
