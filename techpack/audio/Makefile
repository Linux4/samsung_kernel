# auto-detect subdirs
ifneq ($(CONFIG_ARCH_QTI_VM), y)
ifeq ($(CONFIG_QTI_QUIN_GVM), y)
include $(srctree)/techpack/audio/config/gvmauto.conf
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
include $(srctree)/techpack/audio/config/sdxpoorwillsauto.conf
export
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
include $(srctree)/techpack/audio/config/sm8150auto.conf
export
endif
ifeq ($(CONFIG_ARCH_KONA), y)
include $(srctree)/techpack/audio/config/konaauto.conf
endif
ifeq ($(CONFIG_ARCH_LAHAINA), y)
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a52sxq))
include $(srctree)/techpack/audio/config/lahaina_a52s.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),m52xq))
include $(srctree)/techpack/audio/config/lahaina_m52.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a73xq))
include $(srctree)/techpack/audio/config/lahaina_a73.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7fewifi))
include $(srctree)/techpack/audio/config/lahaina_gts7fe.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),xcoverpro2))
include $(srctree)/techpack/audio/config/lahaina_xcoverpro2.conf
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gtact4pro gtact4prowifi))
include $(srctree)/techpack/audio/config/lahaina_gtact4pro.conf
else
include $(srctree)/techpack/audio/config/lahainaauto.conf
endif
export
endif
ifeq ($(CONFIG_ARCH_HOLI), y)
include $(srctree)/techpack/audio/config/holiauto.conf
endif
ifeq ($(CONFIG_ARCH_SA8155), y)
include $(srctree)/techpack/audio/config/sa8155auto.conf
endif
ifeq ($(CONFIG_ARCH_SA6155), y)
include $(srctree)/techpack/audio/config/sa6155auto.conf
endif
ifeq ($(CONFIG_ARCH_SM6150), y)
include $(srctree)/techpack/audio/config/sm6150auto.conf
endif
endif
# Use USERINCLUDE when you must reference the UAPI directories only.
USERINCLUDE     += \
                -I$(srctree)/techpack/audio/include/uapi/audio

# Use LINUXINCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
LINUXINCLUDE    += \
                -I$(srctree)/techpack/audio/include/uapi \
                -I$(srctree)/techpack/audio/include/uapi/audio \
                -I$(srctree)/techpack/audio/include/asoc \
                -I$(srctree)/techpack/audio/include

ifeq ($(CONFIG_QTI_QUIN_GVM), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/gvmautoconf.h
endif
ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sdxpoorwillsautoconf.h
endif
ifeq ($(CONFIG_ARCH_SM8150), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifeq ($(CONFIG_ARCH_SDMSHRIKE), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm8150autoconf.h
endif
ifeq ($(CONFIG_ARCH_KONA), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/konaautoconf.h
endif
ifeq ($(CONFIG_ARCH_LAHAINA), y)
ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a52sxq))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_a52s.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),m52xq))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_m52.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),a73xq))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_a73.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gts7fewifi))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_gts7fe.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),xcoverpro2))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_xcoverpro2.h
else ifeq ($(PROJECT_NAME),$(filter $(PROJECT_NAME),gtact4pro gtact4prowifi))
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahaina_gtact4pro.h
else
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/lahainaautoconf.h
endif
endif
ifeq ($(CONFIG_ARCH_HOLI), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/holiautoconf.h
endif
ifeq ($(CONFIG_ARCH_SA8155), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sa8155autoconf.h
endif
ifeq ($(CONFIG_ARCH_SA6155), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sa6155autoconf.h
endif
ifeq ($(CONFIG_ARCH_SM6150), y)
LINUXINCLUDE    += \
                -include $(srctree)/techpack/audio/config/sm6150autoconf.h
endif
obj-y += soc/
obj-y += dsp/
obj-y += ipc/
obj-y += asoc/
